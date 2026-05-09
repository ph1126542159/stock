#pragma once

#include "Poco/AutoPtr.h"
#include "Poco/Util/AbstractConfiguration.h"

#include <string>

class QObject;

namespace stok::services::common {
class LocalizationClient;
class ServiceTelemetry;
}

namespace stok::services::feature_page {

using FeatureControllerFactory = QObject* (*)(QObject* parent, stok::services::common::LocalizationClient* localization);

struct FeatureControllerContext
{
    QObject* parent = nullptr;
    stok::services::common::LocalizationClient* localization = nullptr;
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration;
    std::string configPath;
    stok::services::common::ServiceTelemetry* telemetry = nullptr;
};

// Extended factory that receives the loaded service configuration plus the
// telemetry handle so controllers can read storage paths, run external
// providers, and emit log lines through the same channel as the rest of
// the service. New controllers should prefer this entry point; the legacy
// FeatureControllerFactory remains for placeholder pages that don't need
// any of the above.
using FeatureControllerExtFactory = QObject* (*)(const FeatureControllerContext&);

struct FeaturePageSpec
{
    const char* applicationName;
    const char* description;
    const char* defaultConfigFile;
    const char* defaultServiceName;
    const char* defaultMenuId;
    const char* fallbackTitle;
    const char* fallbackDescription;
    const char* qmlModuleUri;
    FeatureControllerFactory controllerFactory = nullptr;
    FeatureControllerExtFactory controllerFactoryExt = nullptr;
};

int run(int argc, char* argv[], const FeaturePageSpec& spec);

} // namespace stok::services::feature_page
