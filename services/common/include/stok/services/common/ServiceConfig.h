#pragma once

#include "Poco/AutoPtr.h"
#include "Poco/OpenTelemetry/TelemetryModel.h"
#include "Poco/Util/AbstractConfiguration.h"
#include <cstdint>
#include <string>
#include <vector>

namespace stok::services::common {

struct ServiceIdentity
{
    std::string name;
    std::string version;
    std::string instanceId;
};

struct DdsSettings
{
    std::uint32_t domainId = 42;
    std::string topicName = "stok.market.quotes";
    std::uint32_t segmentSize = 4 * 1024 * 1024;
    std::uint32_t portQueueCapacity = 256;
};

Poco::AutoPtr<Poco::Util::AbstractConfiguration> loadServiceConfiguration(const std::string& configPath);

ServiceIdentity readServiceIdentity(
    const Poco::Util::AbstractConfiguration& configuration,
    const std::string& fallbackName);

DdsSettings readDdsSettings(const Poco::Util::AbstractConfiguration& configuration);

Poco::OpenTelemetry::TelemetryConfiguration readTelemetryConfiguration(
    const Poco::Util::AbstractConfiguration& configuration,
    const ServiceIdentity& identity);

std::string resolveConfigRelativePath(const std::string& baseConfigPath, const std::string& configuredPath);

std::vector<std::string> splitList(const std::string& value, char delimiter = ',');

} // namespace stok::services::common
