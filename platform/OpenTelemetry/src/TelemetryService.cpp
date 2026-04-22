#include "Poco/OpenTelemetry/TelemetryService.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Mutex.h"
#include "Poco/ScopedLock.h"
#include "Poco/Timestamp.h"
#include "Poco/UUIDGenerator.h"

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/trace/trace_id.h"

#if defined(MYIOT_OPENTELEMETRY_HAS_OTLP_HTTP)
#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_options.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace resource_sdk = opentelemetry::sdk::resource;
namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;

using OTelAttributePair = std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>;

std::string formatTimestamp(const Poco::Timestamp& timestamp)
{
    return Poco::DateTimeFormatter::format(
        timestamp,
        Poco::DateTimeFormat::ISO8601_FRAC_FORMAT);
}

std::string formatNow()
{
    return formatTimestamp(Poco::Timestamp());
}

void upsertAttribute(
    Poco::OpenTelemetry::TelemetryAttributes& attributes,
    const std::string& key,
    const std::string& value)
{
    for (auto& attribute: attributes)
    {
        if (attribute.key == key)
        {
            attribute.value = value;
            return;
        }
    }

    attributes.push_back({key, value});
}

template <typename T>
void trimStore(std::deque<T>& values, std::size_t limit)
{
    while (values.size() > limit)
    {
        values.pop_back();
    }
}

std::string compactUuid()
{
    std::string value = Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    return value;
}

std::string traceIdToString(const trace_api::TraceId& traceId)
{
    std::array<char, trace_api::TraceId::kSize * 2> buffer{};
    traceId.ToLowerBase16(opentelemetry::nostd::span<char, trace_api::TraceId::kSize * 2>(buffer));
    return std::string(buffer.data(), buffer.size());
}

opentelemetry::nostd::string_view toView(const std::string& value)
{
    return opentelemetry::nostd::string_view(value.data(), value.size());
}

opentelemetry::common::SystemTimestamp toSystemTimestamp(const Poco::Timestamp& timestamp)
{
    return opentelemetry::common::SystemTimestamp(
        std::chrono::microseconds(timestamp.epochMicroseconds()));
}

logs_api::Severity toSeverity(const std::string& level)
{
    if (level == "trace") return logs_api::Severity::kTrace;
    if (level == "debug") return logs_api::Severity::kDebug;
    if (level == "warning" || level == "warn") return logs_api::Severity::kWarn;
    if (level == "error") return logs_api::Severity::kError;
    if (level == "fatal" || level == "critical") return logs_api::Severity::kFatal;
    return logs_api::Severity::kInfo;
}

bool isSuccessStatus(const std::string& status)
{
    return status.empty() || status == "ok" || status == "success";
}

std::string trimCopy(const std::string& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
    {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(start, end - start);
}

std::string joinUrl(const std::string& endpoint, const std::string& path)
{
    if (endpoint.empty()) return path;
    if (path.empty()) return endpoint;

    if (endpoint.back() == '/' && path.front() == '/')
    {
        return endpoint.substr(0, endpoint.size() - 1) + path;
    }
    if (endpoint.back() != '/' && path.front() != '/')
    {
        return endpoint + "/" + path;
    }
    return endpoint + path;
}

std::vector<std::string>& activityStack()
{
    static thread_local std::vector<std::string> stack;
    return stack;
}

class OTelAttributeBuffer
{
public:
    void add(std::string key, std::string value)
    {
        _owned.push_back(std::make_pair(std::move(key), std::move(value)));
        _dirty = true;
    }

    void addLiteral(const char* key, std::string value)
    {
        _owned.push_back(std::make_pair(std::string(key), std::move(value)));
        _dirty = true;
    }

    void addAll(const Poco::OpenTelemetry::TelemetryAttributes& attributes)
    {
        for (const auto& attribute: attributes)
        {
            add(attribute.key, attribute.value);
        }
    }

    const std::vector<OTelAttributePair>& pairs()
    {
        if (_dirty)
        {
            _pairs.clear();
            _pairs.reserve(_owned.size());
            for (const auto& item: _owned)
            {
                _pairs.emplace_back(
                    opentelemetry::nostd::string_view(item.first.data(), item.first.size()),
                    opentelemetry::nostd::string_view(item.second.data(), item.second.size()));
            }
            _dirty = false;
        }
        return _pairs;
    }

    opentelemetry::common::KeyValueIterableView<std::vector<OTelAttributePair>> view()
    {
        return opentelemetry::common::KeyValueIterableView<std::vector<OTelAttributePair>>(pairs());
    }

    bool empty() const
    {
        return _owned.empty();
    }

private:
    std::vector<std::pair<std::string, std::string>> _owned;
    std::vector<OTelAttributePair> _pairs;
    bool _dirty = false;
};

struct LogStorage
{
    Poco::Timestamp timestamp;
    Poco::OpenTelemetry::TelemetryLogEntry entry;
};

struct MetricStorage
{
    Poco::Timestamp timestamp;
    Poco::OpenTelemetry::TelemetryMetricSample sample;
};

struct CompletedActivity
{
    Poco::Timestamp endedAt;
    Poco::OpenTelemetry::TelemetryTraceEntry entry;
};

struct ActiveActivity
{
    Poco::Timestamp startedAt;
    std::chrono::steady_clock::time_point steadyStartedAt;
    Poco::OpenTelemetry::TelemetryTraceEntry entry;
    opentelemetry::nostd::shared_ptr<trace_api::Span> span;
};

class TelemetryServiceImpl: public Poco::OpenTelemetry::TelemetryService
{
public:
    explicit TelemetryServiceImpl(const Poco::OpenTelemetry::TelemetryConfiguration& configuration):
        _configuration(configuration)
    {
        if (_configuration.exportEnabled && _configuration.otlpEndpoint.empty())
        {
            publishLog(
                "warning",
                "telemetry",
                "Telemetry export is enabled, but telemetry.export.otlp.endpoint is empty. Falling back to local in-memory telemetry only.",
                {});
            return;
        }

#if defined(MYIOT_OPENTELEMETRY_HAS_OTLP_HTTP)
        if (shouldExportOtlp())
        {
            initializeOtlp();
        }
#else
        if (_configuration.exportEnabled)
        {
            publishLog(
                "warning",
                "telemetry",
                "Telemetry export was requested, but this build does not include OTLP HTTP exporter support.",
                {});
        }
#endif
    }

    std::string beginActivity(
        const std::string& name,
        const std::string& category,
        const std::string& input,
        const Poco::OpenTelemetry::TelemetryAttributes& attributes) override
    {
        const Poco::Timestamp startedAt;
        const auto steadyStartedAt = std::chrono::steady_clock::now();
        const std::string parentActivityId = currentActivityId();

        Poco::OpenTelemetry::TelemetryTraceEntry entry;
        entry.name = name;
        entry.category = category;
        entry.status = "running";
        entry.input = input;
        entry.startedAt = formatTimestamp(startedAt);
        entry.parentActivityId = parentActivityId;
        entry.traceId = compactUuid();
        entry.activityId = compactUuid();
        entry.attributes = attributes;
        if (!category.empty())
        {
            upsertAttribute(entry.attributes, "category", category);
        }

        opentelemetry::nostd::shared_ptr<trace_api::Span> span;
        std::string parentTraceId;
        opentelemetry::nostd::shared_ptr<trace_api::Span> parentSpan;

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            if (!parentActivityId.empty())
            {
                const auto parentIt = _activeActivities.find(parentActivityId);
                if (parentIt != _activeActivities.end())
                {
                    parentTraceId = parentIt->second.entry.traceId;
                    parentSpan = parentIt->second.span;
                }
            }
        }

        if (!parentTraceId.empty())
        {
            entry.traceId = parentTraceId;
        }

        if (_tracer && _configuration.exportTraces)
        {
            try
            {
                trace_api::StartSpanOptions options;
                options.start_system_time = toSystemTimestamp(startedAt);
                options.start_steady_time = opentelemetry::common::SteadyTimestamp(steadyStartedAt);
                if (parentSpan && parentSpan->GetContext().IsValid())
                {
                    options.parent = parentSpan->GetContext();
                }

                span = _tracer->StartSpan(toView(name), options);
                if (span && span->GetContext().IsValid())
                {
                    entry.traceId = traceIdToString(span->GetContext().trace_id());
                    span->SetAttribute("activity.id", toView(entry.activityId));
                    if (!category.empty()) span->SetAttribute("activity.category", toView(category));
                    if (!input.empty()) span->SetAttribute("activity.input", toView(input));
                    for (const auto& attribute: entry.attributes)
                    {
                        span->SetAttribute(toView(attribute.key), toView(attribute.value));
                    }
                }
            }
            catch (...)
            {
            }
        }

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            _activeActivities[entry.activityId] = ActiveActivity{
                startedAt,
                steadyStartedAt,
                entry,
                span};
        }

        activityStack().push_back(entry.activityId);
        return entry.activityId;
    }

    void addActivityStep(
        const std::string& activityId,
        const std::string& name,
        const std::string& detail,
        const std::string& status,
        const Poco::OpenTelemetry::TelemetryAttributes& attributes) override
    {
        opentelemetry::nostd::shared_ptr<trace_api::Span> span;

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            const auto it = _activeActivities.find(activityId);
            if (it == _activeActivities.end()) return;

            Poco::OpenTelemetry::TelemetryTraceStep step;
            step.timestamp = formatNow();
            step.name = name;
            step.detail = detail;
            step.status = status.empty() ? "ok" : status;
            step.attributes = attributes;
            it->second.entry.steps.push_back(step);

            if (it->second.entry.steps.size() > _configuration.maxStepsPerTrace)
            {
                it->second.entry.steps.erase(it->second.entry.steps.begin());
            }

            span = it->second.span;
        }

        if (span && span->IsRecording())
        {
            try
            {
                OTelAttributeBuffer otelAttributes;
                if (!detail.empty()) otelAttributes.addLiteral("detail", detail);
                if (!status.empty()) otelAttributes.addLiteral("status", status);
                otelAttributes.addAll(attributes);
                span->AddEvent(toView(name), otelAttributes.view());
            }
            catch (...)
            {
            }
        }
    }

    void setActivityInput(const std::string& activityId, const std::string& input) override
    {
        opentelemetry::nostd::shared_ptr<trace_api::Span> span;

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            const auto it = _activeActivities.find(activityId);
            if (it == _activeActivities.end()) return;

            it->second.entry.input = input;
            span = it->second.span;
        }

        if (span && span->IsRecording())
        {
            try
            {
                span->SetAttribute("activity.input", toView(input));
            }
            catch (...)
            {
            }
        }
    }

    void setActivityOutput(const std::string& activityId, const std::string& output) override
    {
        opentelemetry::nostd::shared_ptr<trace_api::Span> span;

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            const auto it = _activeActivities.find(activityId);
            if (it == _activeActivities.end()) return;

            it->second.entry.output = output;
            span = it->second.span;
        }

        if (span && span->IsRecording())
        {
            try
            {
                span->SetAttribute("activity.output", toView(output));
            }
            catch (...)
            {
            }
        }
    }

    void setActivityAttribute(const std::string& activityId, const std::string& key, const std::string& value) override
    {
        opentelemetry::nostd::shared_ptr<trace_api::Span> span;

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            const auto it = _activeActivities.find(activityId);
            if (it == _activeActivities.end()) return;

            upsertAttribute(it->second.entry.attributes, key, value);
            span = it->second.span;
        }

        if (span && span->IsRecording())
        {
            try
            {
                span->SetAttribute(toView(key), toView(value));
            }
            catch (...)
            {
            }
        }
    }

    void finishActivity(
        const std::string& activityId,
        const std::string& status,
        const std::string& output,
        const std::string& error) override
    {
        const Poco::Timestamp endedAt;
        ActiveActivity activity;
        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            const auto it = _activeActivities.find(activityId);
            if (it == _activeActivities.end()) return;

            activity = it->second;
            _activeActivities.erase(it);
        }

        activity.entry.status = status.empty() ? "ok" : status;
        if (!output.empty()) activity.entry.output = output;
        if (!error.empty()) activity.entry.error = error;
        activity.entry.endedAt = formatTimestamp(endedAt);
        activity.entry.durationMs = static_cast<long long>((endedAt - activity.startedAt) / 1000);

        if (activity.span && activity.span->IsRecording())
        {
            try
            {
                if (!activity.entry.output.empty())
                {
                    activity.span->SetAttribute("activity.output", toView(activity.entry.output));
                }
                if (!activity.entry.error.empty())
                {
                    activity.span->SetAttribute("activity.error", toView(activity.entry.error));
                }

                activity.span->SetStatus(
                    isSuccessStatus(activity.entry.status) ? trace_api::StatusCode::kOk : trace_api::StatusCode::kError,
                    activity.entry.error.empty() ? toView(activity.entry.status) : toView(activity.entry.error));

                trace_api::EndSpanOptions endOptions;
                endOptions.end_steady_time = opentelemetry::common::SteadyTimestamp(std::chrono::steady_clock::now());
                activity.span->End(endOptions);
            }
            catch (...)
            {
            }
        }

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            _completedActivities.push_front(CompletedActivity{endedAt, activity.entry});
            trimStore(_completedActivities, _configuration.maxTraces);
        }

        auto& stack = activityStack();
        const auto stackIt = std::find(stack.begin(), stack.end(), activityId);
        if (stackIt != stack.end())
        {
            stack.erase(stackIt);
        }

        recordMetric(
            "myiot.activity.duration_ms",
            static_cast<double>(activity.entry.durationMs),
            "ms",
            "Business activity duration",
            {
                {"activity.name", activity.entry.name},
                {"activity.category", activity.entry.category},
                {"activity.status", activity.entry.status}
            });
    }

    void publishLog(
        const std::string& level,
        const std::string& source,
        const std::string& message,
        const Poco::OpenTelemetry::TelemetryAttributes& attributes) override
    {
        Poco::OpenTelemetry::TelemetryLogEntry entry;
        entry.timestamp = formatNow();
        entry.level = level;
        entry.source = source;
        entry.message = message;
        entry.attributes = attributes;

        const std::string activityId = currentActivityId();
        std::string traceId;
        opentelemetry::nostd::shared_ptr<trace_api::Span> activitySpan;

        if (!activityId.empty())
        {
            upsertAttribute(entry.attributes, "activity.id", activityId);

            Poco::FastMutex::ScopedLock lock(_mutex);
            const auto it = _activeActivities.find(activityId);
            if (it != _activeActivities.end())
            {
                traceId = it->second.entry.traceId;
                activitySpan = it->second.span;
                upsertAttribute(entry.attributes, "trace.id", traceId);
            }
        }

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            _logs.push_front(LogStorage{Poco::Timestamp(), entry});
            trimStore(_logs, _configuration.maxLogs);
        }

        if (_logger && _configuration.exportLogs)
        {
            try
            {
                OTelAttributeBuffer otelAttributes;
                otelAttributes.addLiteral("log.source", source);
                if (!activityId.empty()) otelAttributes.addLiteral("activity.id", activityId);
                if (!traceId.empty()) otelAttributes.addLiteral("trace.id", traceId);
                otelAttributes.addAll(attributes);
                const trace_api::SpanContext spanContext =
                    (activitySpan && activitySpan->GetContext().IsValid())
                        ? activitySpan->GetContext()
                        : trace_api::SpanContext::GetInvalid();
                _logger->EmitLogRecord(
                    toSeverity(level),
                    toView(message),
                    spanContext,
                    std::chrono::system_clock::now(),
                    otelAttributes.view());
            }
            catch (...)
            {
            }
        }
    }

    void recordMetric(
        const std::string& name,
        double value,
        const std::string& unit,
        const std::string& description,
        const Poco::OpenTelemetry::TelemetryAttributes& attributes) override
    {
        Poco::OpenTelemetry::TelemetryMetricSample sample;
        sample.timestamp = formatNow();
        sample.name = name;
        sample.description = description;
        sample.unit = unit;
        sample.value = value;
        sample.attributes = attributes;

        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            _metrics.push_front(MetricStorage{Poco::Timestamp(), sample});
            trimStore(_metrics, _configuration.maxMetrics);
        }

        if (_meter && _configuration.exportMetrics)
        {
            bool shouldFlush = false;
            try
            {
                OTelAttributeBuffer otelAttributes;
                otelAttributes.addAll(attributes);

                metrics_api::Histogram<double>* pHistogram = nullptr;
                {
                    Poco::FastMutex::ScopedLock lock(_instrumentMutex);
                    auto& histogram = _histograms[name];
                    if (!histogram)
                    {
                        histogram = _meter->CreateDoubleHistogram(
                            toView(name),
                            toView(description),
                            toView(unit));
                    }
                    pHistogram = histogram.get();
                }

                if (pHistogram)
                {
                    pHistogram->Record(value, otelAttributes.view(), opentelemetry::context::Context{});
                }

                if (_meterProvider)
                {
                    const auto now = std::chrono::steady_clock::now();
                    if (_lastMetricFlush.time_since_epoch().count() == 0 ||
                        now - _lastMetricFlush >= std::chrono::seconds(1))
                    {
                        _lastMetricFlush = now;
                        shouldFlush = true;
                    }
                }
            }
            catch (...)
            {
            }

            if (shouldFlush)
            {
                try
                {
                    _meterProvider->ForceFlush();
                }
                catch (...)
                {
                }
            }
        }
    }

    std::string currentActivityId() const override
    {
        const auto& stack = activityStack();
        return stack.empty() ? std::string() : stack.back();
    }

    Poco::OpenTelemetry::TelemetrySnapshot snapshot(
        const Poco::OpenTelemetry::TelemetrySnapshotOptions& options) const override
    {
        Poco::OpenTelemetry::TelemetrySnapshot snapshot;
        snapshot.generatedAt = formatNow();

        Poco::FastMutex::ScopedLock lock(_mutex);
        snapshot.activeTraceCount = _activeActivities.size();

        for (std::size_t index = 0; index < options.logLimit && index < _logs.size(); ++index)
        {
            snapshot.logs.push_back(_logs[index].entry);
        }

        std::vector<ActiveActivity> activeValues;
        activeValues.reserve(_activeActivities.size());
        for (const auto& item: _activeActivities)
        {
            activeValues.push_back(item.second);
        }
        std::sort(activeValues.begin(), activeValues.end(), [](const ActiveActivity& left, const ActiveActivity& right) {
            return left.startedAt > right.startedAt;
        });

        for (const auto& item: activeValues)
        {
            if (snapshot.traces.size() >= options.traceLimit) break;
            snapshot.traces.push_back(item.entry);
        }

        for (std::size_t index = 0;
             snapshot.traces.size() < options.traceLimit && index < _completedActivities.size();
             ++index)
        {
            snapshot.traces.push_back(_completedActivities[index].entry);
        }

        for (std::size_t index = 0; index < options.metricLimit && index < _metrics.size(); ++index)
        {
            snapshot.metrics.push_back(_metrics[index].sample);
        }

        return snapshot;
    }

protected:
    ~TelemetryServiceImpl() override
    {
        shutdownOtlp();
    }

private:
    bool shouldExportOtlp() const
    {
        return _configuration.exportEnabled && !_configuration.otlpEndpoint.empty();
    }

#if defined(MYIOT_OPENTELEMETRY_HAS_OTLP_HTTP)
    void initializeOtlp()
    {
        const std::size_t exportTimeoutMs = std::max<std::size_t>(_configuration.exportTimeoutMs, 100);
        const std::size_t exportScheduleDelayMs = std::max<std::size_t>(_configuration.exportScheduleDelayMs, 100);
        const std::size_t metricExportIntervalMs =
            (_configuration.metricExportIntervalMs > exportTimeoutMs)
                ? _configuration.metricExportIntervalMs
                : exportTimeoutMs + 1000;

        Poco::OpenTelemetry::TelemetryAttributes resourceAttributes = _configuration.resourceAttributes;
        upsertAttribute(resourceAttributes, "service.name", _configuration.serviceName);
        upsertAttribute(resourceAttributes, "service.version", _configuration.serviceVersion);
        upsertAttribute(resourceAttributes, "service.namespace", "myiot");

        OTelAttributeBuffer resourceBuffer;
        resourceBuffer.addAll(resourceAttributes);
        const resource_sdk::Resource resource = resource_sdk::Resource::Create(resourceBuffer.view());

        if (_configuration.exportTraces)
        {
            auto exporterOptions = opentelemetry::exporter::otlp::OtlpHttpExporterOptions();
            exporterOptions.url = joinUrl(_configuration.otlpEndpoint, _configuration.otlpTracesPath);
            exporterOptions.timeout = std::chrono::milliseconds(exportTimeoutMs);
            exporterOptions.http_headers = parseHeaders();
            exporterOptions.ssl_insecure_skip_verify = _configuration.otlpInsecureSkipVerify;
            exporterOptions.console_debug = _configuration.otlpConsoleDebug;

            auto exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(exporterOptions);
            trace_sdk::BatchSpanProcessorOptions processorOptions;
            processorOptions.schedule_delay_millis = std::chrono::milliseconds(exportScheduleDelayMs);
            auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), processorOptions);
            _tracerProvider = std::shared_ptr<trace_sdk::TracerProvider>(
                trace_sdk::TracerProviderFactory::Create(std::move(processor), resource));
            _tracer = _tracerProvider->GetTracer("myiot.telemetry", _configuration.serviceVersion);
        }

        if (_configuration.exportLogs)
        {
            auto exporterOptions = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions();
            exporterOptions.url = joinUrl(_configuration.otlpEndpoint, _configuration.otlpLogsPath);
            exporterOptions.timeout = std::chrono::milliseconds(exportTimeoutMs);
            exporterOptions.http_headers = parseHeaders();
            exporterOptions.ssl_insecure_skip_verify = _configuration.otlpInsecureSkipVerify;
            exporterOptions.console_debug = _configuration.otlpConsoleDebug;

            auto exporter = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(exporterOptions);
            logs_sdk::BatchLogRecordProcessorOptions processorOptions;
            processorOptions.schedule_delay_millis = std::chrono::milliseconds(exportScheduleDelayMs);
            auto processor = logs_sdk::BatchLogRecordProcessorFactory::Create(std::move(exporter), processorOptions);
            _loggerProvider = std::shared_ptr<logs_sdk::LoggerProvider>(
                logs_sdk::LoggerProviderFactory::Create(std::move(processor), resource));
            _logger = _loggerProvider->GetLogger("myiot.telemetry", _configuration.serviceVersion);
        }

        if (_configuration.exportMetrics)
        {
            auto exporterOptions = opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions();
            exporterOptions.url = joinUrl(_configuration.otlpEndpoint, _configuration.otlpMetricsPath);
            exporterOptions.timeout = std::chrono::milliseconds(exportTimeoutMs);
            exporterOptions.http_headers = parseHeaders();
            exporterOptions.ssl_insecure_skip_verify = _configuration.otlpInsecureSkipVerify;
            exporterOptions.console_debug = _configuration.otlpConsoleDebug;

            auto exporter = opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(exporterOptions);
            metrics_sdk::PeriodicExportingMetricReaderOptions readerOptions;
            readerOptions.export_interval_millis = std::chrono::milliseconds(metricExportIntervalMs);
            readerOptions.export_timeout_millis = std::chrono::milliseconds(exportTimeoutMs);

            auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
                std::move(exporter),
                readerOptions);

            auto context = metrics_sdk::MeterContextFactory::Create(
                std::make_unique<metrics_sdk::ViewRegistry>(),
                resource);
            context->AddMetricReader(std::move(reader));
            _meterProvider = std::shared_ptr<metrics_sdk::MeterProvider>(
                metrics_sdk::MeterProviderFactory::Create(std::move(context)));
            _meter = _meterProvider->GetMeter("myiot.telemetry", _configuration.serviceVersion);
        }

        publishLog(
            "information",
            "telemetry",
            "OTLP HTTP telemetry export initialized.",
            {
                {"telemetry.otlp.endpoint", _configuration.otlpEndpoint}
            });

        if (_configuration.exportMetrics && metricExportIntervalMs != _configuration.metricExportIntervalMs)
        {
            publishLog(
                "warning",
                "telemetry",
                "Adjusted telemetry.export.metrics.interval so it is greater than telemetry.export.otlp.timeout.",
                {
                    {"telemetry.export.otlp.timeout", std::to_string(exportTimeoutMs)},
                    {"telemetry.export.metrics.interval", std::to_string(metricExportIntervalMs)}
                });
        }
    }

    opentelemetry::exporter::otlp::OtlpHeaders parseHeaders() const
    {
        opentelemetry::exporter::otlp::OtlpHeaders headers;
        std::size_t start = 0;
        while (start <= _configuration.otlpHeaders.size())
        {
            const std::size_t end = _configuration.otlpHeaders.find_first_of(";\r\n", start);
            const std::string token = trimCopy(_configuration.otlpHeaders.substr(
                start,
                end == std::string::npos ? std::string::npos : end - start));
            if (!token.empty())
            {
                const std::size_t delimiter = token.find('=');
                if (delimiter != std::string::npos)
                {
                    const std::string key = trimCopy(token.substr(0, delimiter));
                    const std::string value = trimCopy(token.substr(delimiter + 1));
                    if (!key.empty())
                    {
                        headers.emplace(key, value);
                    }
                }
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return headers;
    }
#endif

    void shutdownOtlp() noexcept
    {
        try
        {
            if (_loggerProvider)
            {
                _loggerProvider->ForceFlush();
                _loggerProvider->Shutdown();
            }
        }
        catch (...)
        {
        }

        try
        {
            if (_meterProvider)
            {
                _meterProvider->ForceFlush();
                _meterProvider->Shutdown();
            }
        }
        catch (...)
        {
        }

        try
        {
            if (_tracerProvider)
            {
                _tracerProvider->ForceFlush();
                _tracerProvider->Shutdown();
            }
        }
        catch (...)
        {
        }

        _logger = nullptr;
        _meter = nullptr;
        _tracer = nullptr;
        _loggerProvider.reset();
        _meterProvider.reset();
        _tracerProvider.reset();
    }

    Poco::OpenTelemetry::TelemetryConfiguration _configuration;
    mutable Poco::FastMutex _mutex;
    mutable Poco::FastMutex _instrumentMutex;
    std::deque<LogStorage> _logs;
    std::deque<MetricStorage> _metrics;
    std::deque<CompletedActivity> _completedActivities;
    std::map<std::string, ActiveActivity> _activeActivities;
    std::map<std::string, opentelemetry::nostd::unique_ptr<metrics_api::Histogram<double>>> _histograms;
    std::shared_ptr<trace_sdk::TracerProvider> _tracerProvider;
    opentelemetry::nostd::shared_ptr<trace_api::Tracer> _tracer;
    std::shared_ptr<logs_sdk::LoggerProvider> _loggerProvider;
    opentelemetry::nostd::shared_ptr<logs_api::Logger> _logger;
    std::shared_ptr<metrics_sdk::MeterProvider> _meterProvider;
    opentelemetry::nostd::shared_ptr<metrics_api::Meter> _meter;
    std::chrono::steady_clock::time_point _lastMetricFlush{};
};

} // namespace

namespace Poco {
namespace OpenTelemetry {

const std::string TelemetryService::SERVICE_NAME("io.myiot.telemetry");

const std::type_info& TelemetryService::type() const
{
    return typeid(TelemetryService);
}

bool TelemetryService::isA(const std::type_info& otherType) const
{
    const std::string name(typeid(TelemetryService).name());
    return name == otherType.name() || Poco::OSP::Service::isA(otherType);
}

TelemetryService::~TelemetryService() = default;

TelemetryService::Ptr createTelemetryService(const TelemetryConfiguration& configuration)
{
    return new TelemetryServiceImpl(configuration);
}

} } // namespace Poco::OpenTelemetry
