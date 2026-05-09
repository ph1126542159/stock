#pragma once

#include "Poco/AutoPtr.h"
#include "Poco/Channel.h"
#include "Poco/Logger.h"
#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "Poco/OpenTelemetry/TelemetryService.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TextMessageBus.h"
#include <memory>
#include <string>

namespace stok::services::common {

class ServiceTelemetry
{
public:
    ServiceTelemetry(
        ServiceIdentity identity,
        Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
        std::string configPath);
    ~ServiceTelemetry();

    void install();
    void uninstall();

    const ServiceIdentity& identity() const;
    Poco::Logger& logger() const;
    Poco::OpenTelemetry::TelemetryService::Ptr service() const;
    Poco::OpenTelemetry::TelemetryClient client() const;

    void recordStartup(
        const std::string& mode,
        const Poco::OpenTelemetry::TelemetryAttributes& extraAttributes = {}) const;

private:
    ServiceIdentity identity_;
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration_;
    std::string configPath_;
    Poco::OpenTelemetry::TelemetryService::Ptr telemetryService_;
    mutable Poco::Logger* logger_ = nullptr;
    Poco::AutoPtr<Poco::Channel> originalRootChannel_;
    Poco::AutoPtr<Poco::Channel> originalServiceChannel_;
    Poco::AutoPtr<Poco::Channel> installedChannel_;
    std::unique_ptr<TextMessagePublisher> logPublisher_;
    Poco::AutoPtr<Poco::Channel> ddsLogChannel_;
    bool installed_ = false;
};

} // namespace stok::services::common
