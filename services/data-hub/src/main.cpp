#include "stok/services/feature_page/FeaturePageRunner.h"
#include "DataHubController.h"

int main(int argc, char* argv[])
{
    const stok::services::feature_page::FeaturePageSpec spec{
        "stok-data-hub",
        "stok data source management service",
        "data-hub.properties",
        "stok.data-hub",
        "data-hub",
        "Data Hub",
        "Quote, filing, FX, fund NAV, and latency monitoring",
        "StokDataHub",
        createDataHubController
    };
    return stok::services::feature_page::run(argc, argv, spec);
}
