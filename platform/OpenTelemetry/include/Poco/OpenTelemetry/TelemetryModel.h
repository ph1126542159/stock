#ifndef OpenTelemetry_TelemetryModel_INCLUDED
#define OpenTelemetry_TelemetryModel_INCLUDED

#include "Poco/OpenTelemetry/OpenTelemetry.h"
#include <cstddef>
#include <string>
#include <vector>

namespace Poco {
namespace OpenTelemetry {

struct OpenTelemetry_API TelemetryAttribute
{
    std::string key;
    std::string value;
};

using TelemetryAttributes = std::vector<TelemetryAttribute>;

struct OpenTelemetry_API TelemetryLogEntry
{
    std::string timestamp;
    std::string level;
    std::string source;
    std::string message;
    TelemetryAttributes attributes;
};

struct OpenTelemetry_API TelemetryTraceStep
{
    std::string timestamp;
    std::string name;
    std::string detail;
    std::string status;
    TelemetryAttributes attributes;
};

struct OpenTelemetry_API TelemetryTraceEntry
{
    std::string activityId;
    std::string traceId;
    std::string parentActivityId;
    std::string name;
    std::string category;
    std::string status;
    std::string input;
    std::string output;
    std::string error;
    std::string startedAt;
    std::string endedAt;
    long long durationMs = 0;
    TelemetryAttributes attributes;
    std::vector<TelemetryTraceStep> steps;
};

struct OpenTelemetry_API TelemetryMetricSample
{
    std::string timestamp;
    std::string name;
    std::string description;
    std::string unit;
    double value = 0.0;
    TelemetryAttributes attributes;
};

struct OpenTelemetry_API TelemetrySnapshotOptions
{
    std::size_t logLimit = 200;
    std::size_t traceLimit = 60;
    std::size_t metricLimit = 60;
};

struct OpenTelemetry_API TelemetrySnapshot
{
    std::string generatedAt;
    std::size_t activeTraceCount = 0;
    std::vector<TelemetryLogEntry> logs;
    std::vector<TelemetryTraceEntry> traces;
    std::vector<TelemetryMetricSample> metrics;
};

struct OpenTelemetry_API TelemetryConfiguration
{
    std::size_t maxLogs = 4000;
    std::size_t maxTraces = 800;
    std::size_t maxMetrics = 800;
    std::size_t maxStepsPerTrace = 80;
    std::string serviceName = "myiot";
    std::string serviceVersion = "0.1.0";
    bool exportEnabled = false;
    bool exportTraces = true;
    bool exportLogs = true;
    bool exportMetrics = true;
    std::string otlpEndpoint;
    std::string otlpTracesPath = "/v1/traces";
    std::string otlpLogsPath = "/v1/logs";
    std::string otlpMetricsPath = "/v1/metrics";
    std::string otlpHeaders;
    bool otlpInsecureSkipVerify = false;
    bool otlpConsoleDebug = false;
    std::size_t exportTimeoutMs = 5000;
    std::size_t exportScheduleDelayMs = 1000;
    std::size_t metricExportIntervalMs = 2000;
    TelemetryAttributes resourceAttributes;
};

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryModel_INCLUDED
