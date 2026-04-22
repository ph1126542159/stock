#include "stok/services/common/TelemetryBootstrap.h"

#include "Poco/ConsoleChannel.h"
#include "Poco/Environment.h"
#include "Poco/File.h"
#include "Poco/FileChannel.h"
#include "Poco/FormattingChannel.h"
#include "Poco/OpenTelemetry/TelemetryLoggingChannel.h"
#include "Poco/Path.h"
#include "Poco/PatternFormatter.h"
#include "Poco/Process.h"
#include "Poco/SplitterChannel.h"

namespace stok::services::common {

ServiceTelemetry::ServiceTelemetry(
    ServiceIdentity identity,
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
    std::string configPath):
    identity_(std::move(identity)),
    configuration_(std::move(configuration)),
    configPath_(std::move(configPath))
{
}

ServiceTelemetry::~ServiceTelemetry()
{
    uninstall();
}

void ServiceTelemetry::install()
{
    if (installed_)
    {
        return;
    }

    auto formatter = Poco::AutoPtr<Poco::PatternFormatter>(
        new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%p] %s: %t"));
    auto console = Poco::AutoPtr<Poco::FormattingChannel>(
        new Poco::FormattingChannel(formatter, new Poco::ConsoleChannel));
    auto splitter = Poco::AutoPtr<Poco::SplitterChannel>(new Poco::SplitterChannel);
    splitter->addChannel(console);

    const std::string logPath = resolveConfigRelativePath(
        configPath_,
        configuration_->getString("logging.file.path", ""));
    if (!logPath.empty())
    {
        Poco::Path parent(logPath);
        parent.makeParent();
        Poco::File(parent).createDirectories();

        auto fileChannel = Poco::AutoPtr<Poco::FileChannel>(new Poco::FileChannel);
        fileChannel->setProperty("path", logPath);
        fileChannel->setProperty("rotation", "daily");
        fileChannel->setProperty("archive", "number");
        fileChannel->setProperty("purgeCount", "10");
        auto fileFormatter = Poco::AutoPtr<Poco::PatternFormatter>(
            new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%p] %s: %t"));
        splitter->addChannel(new Poco::FormattingChannel(fileFormatter, fileChannel));
    }

    const auto telemetryConfiguration = readTelemetryConfiguration(*configuration_, identity_);
    telemetryService_ = Poco::OpenTelemetry::createTelemetryService(telemetryConfiguration);
    if (telemetryService_)
    {
        splitter->addChannel(new Poco::OpenTelemetry::TelemetryLoggingChannel(telemetryService_));
    }

    logger_ = &Poco::Logger::get(identity_.name);
    originalRootChannel_ = Poco::Logger::root().getChannel();
    originalServiceChannel_ = logger_->getChannel();
    installedChannel_ = splitter;

    Poco::Logger::setChannel("", installedChannel_);
    Poco::Logger::root().setChannel(installedChannel_);
    logger_->setChannel(installedChannel_);
    installed_ = true;

    logger_->information("Telemetry bootstrap installed for %s.", identity_.name);
}

void ServiceTelemetry::uninstall()
{
    if (!installed_)
    {
        return;
    }

    Poco::Logger::setChannel("", originalRootChannel_);
    Poco::Logger::root().setChannel(originalRootChannel_);
    if (logger_)
    {
        logger_->setChannel(originalServiceChannel_ ? originalServiceChannel_ : originalRootChannel_);
    }

    telemetryService_ = nullptr;
    installedChannel_ = nullptr;
    originalServiceChannel_ = nullptr;
    originalRootChannel_ = nullptr;
    installed_ = false;
}

const ServiceIdentity& ServiceTelemetry::identity() const
{
    return identity_;
}

Poco::Logger& ServiceTelemetry::logger() const
{
    return *logger_;
}

Poco::OpenTelemetry::TelemetryService::Ptr ServiceTelemetry::service() const
{
    return telemetryService_;
}

Poco::OpenTelemetry::TelemetryClient ServiceTelemetry::client() const
{
    return Poco::OpenTelemetry::TelemetryClient(telemetryService_);
}

void ServiceTelemetry::recordStartup(
    const std::string& mode,
    const Poco::OpenTelemetry::TelemetryAttributes& extraAttributes) const
{
    Poco::OpenTelemetry::TelemetryAttributes attributes =
    {
        {"service.name", identity_.name},
        {"service.version", identity_.version},
        {"service.instance.id", identity_.instanceId},
        {"runtime.mode", mode},
        {"host.name", Poco::Environment::nodeName()}
    };

    for (const auto& attribute : extraAttributes)
    {
        attributes.push_back(attribute);
    }

    auto telemetryClient = client();
    auto activity = telemetryClient.beginActivity("service.startup", "lifecycle", mode, attributes);
    activity.step("process.initialized", identity_.instanceId);
    telemetryClient.metric(
        "stok.process.pid",
        static_cast<double>(Poco::Process::id()),
        "count",
        "Current process identifier",
        {{"service.name", identity_.name}});
    telemetryClient.metric(
        "stok.process.cpu_count",
        static_cast<double>(Poco::Environment::processorCount()),
        "count",
        "Detected CPU core count",
        {{"service.name", identity_.name}});
    activity.success("ready");
}

} // namespace stok::services::common
