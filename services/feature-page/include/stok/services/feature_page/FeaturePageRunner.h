#pragma once

class QObject;

namespace stok::services::common {
class LocalizationClient;
}

namespace stok::services::feature_page {

using FeatureControllerFactory = QObject* (*)(QObject* parent, stok::services::common::LocalizationClient* localization);

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
};

int run(int argc, char* argv[], const FeaturePageSpec& spec);

} // namespace stok::services::feature_page
