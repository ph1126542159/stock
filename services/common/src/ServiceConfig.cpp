#include "stok/services/common/ServiceConfig.h"

#include "Poco/Environment.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/StringTokenizer.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/Util/PropertyFileConfiguration.h"

namespace stok::services::common {

Poco::AutoPtr<Poco::Util::AbstractConfiguration> loadServiceConfiguration(const std::string& configPath)
{
    if (!configPath.empty() && Poco::File(configPath).exists())
    {
        return new Poco::Util::PropertyFileConfiguration(configPath);
    }

    return new Poco::Util::MapConfiguration;
}

ServiceIdentity readServiceIdentity(
    const Poco::Util::AbstractConfiguration& configuration,
    const std::string& fallbackName)
{
    ServiceIdentity identity;
    identity.name = configuration.getString("service.name", fallbackName);
    identity.version = configuration.getString("service.version", "0.1.0");
    identity.instanceId = configuration.getString(
        "service.instanceId",
        identity.name + "-" + Poco::UUIDGenerator::defaultGenerator().createOne().toString());
    return identity;
}

DdsSettings readDdsSettings(const Poco::Util::AbstractConfiguration& configuration)
{
    DdsSettings settings;
    settings.domainId = static_cast<std::uint32_t>(configuration.getInt("dds.domainId", 42));
    settings.topicName = configuration.getString("dds.topic", "stok.market.quotes");
    settings.segmentSize = static_cast<std::uint32_t>(
        configuration.getInt("dds.shm.segmentSize", 4 * 1024 * 1024));
    settings.portQueueCapacity = static_cast<std::uint32_t>(
        configuration.getInt("dds.shm.portQueueCapacity", 256));
    return settings;
}

Poco::OpenTelemetry::TelemetryConfiguration readTelemetryConfiguration(
    const Poco::Util::AbstractConfiguration& configuration,
    const ServiceIdentity& identity)
{
    Poco::OpenTelemetry::TelemetryConfiguration telemetry;
    telemetry.serviceName = identity.name;
    telemetry.serviceVersion = identity.version;
    telemetry.exportEnabled = configuration.getBool("telemetry.export.enabled", false);
    telemetry.exportTraces = configuration.getBool("telemetry.export.traces", true);
    telemetry.exportLogs = configuration.getBool("telemetry.export.logs", true);
    telemetry.exportMetrics = configuration.getBool("telemetry.export.metrics", true);
    telemetry.otlpEndpoint = configuration.getString("telemetry.export.otlp.endpoint", "");
    telemetry.otlpTracesPath = configuration.getString("telemetry.export.otlp.tracesPath", "/v1/traces");
    telemetry.otlpLogsPath = configuration.getString("telemetry.export.otlp.logsPath", "/v1/logs");
    telemetry.otlpMetricsPath = configuration.getString("telemetry.export.otlp.metricsPath", "/v1/metrics");
    telemetry.otlpHeaders = configuration.getString("telemetry.export.otlp.headers", "");
    telemetry.otlpInsecureSkipVerify = configuration.getBool("telemetry.export.otlp.insecureSkipVerify", false);
    telemetry.otlpConsoleDebug = configuration.getBool("telemetry.export.otlp.consoleDebug", false);
    telemetry.exportTimeoutMs = static_cast<std::size_t>(
        configuration.getInt("telemetry.export.otlp.timeout", 1000));
    telemetry.exportScheduleDelayMs = static_cast<std::size_t>(
        configuration.getInt("telemetry.export.otlp.scheduleDelay", 1000));
    telemetry.metricExportIntervalMs = static_cast<std::size_t>(
        configuration.getInt("telemetry.export.metrics.interval", 3000));

    telemetry.resourceAttributes.push_back({"service.instance.id", identity.instanceId});
    telemetry.resourceAttributes.push_back({"host.name", Poco::Environment::nodeName()});
    telemetry.resourceAttributes.push_back({"deployment.environment", configuration.getString("service.environment", "dev")});
    return telemetry;
}

std::string resolveConfigRelativePath(const std::string& baseConfigPath, const std::string& configuredPath)
{
    if (configuredPath.empty())
    {
        return configuredPath;
    }

    Poco::Path configured(configuredPath);
    if (configured.isAbsolute())
    {
        return configured.toString();
    }

    Poco::Path base(baseConfigPath);
    base.makeParent();
    base.append(configured);
    base.makeAbsolute();
    return base.toString();
}

std::vector<std::string> splitList(const std::string& value, char delimiter)
{
    std::vector<std::string> tokens;
    Poco::StringTokenizer tokenizer(
        value,
        std::string(1, delimiter),
        Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);

    tokens.reserve(tokenizer.count());
    for (const auto& token : tokenizer)
    {
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace stok::services::common
