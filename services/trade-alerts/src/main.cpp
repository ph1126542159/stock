#include "stok/services/feature_page/FeaturePageRunner.h"
#include "TradeAlertsController.h"

int main(int argc, char* argv[])
{
    const stok::services::feature_page::FeaturePageSpec spec{
        "stok-trade-alerts",
        "stok trade plan and alerts service",
        "trade-alerts.properties",
        "stok.trade-alerts",
        "trade-alerts",
        "Trade Alerts",
        "Trading plan, stop rules, invalidation, and realtime alerts",
        "StokTradeAlerts",
        createTradeAlertsController
    };
    return stok::services::feature_page::run(argc, argv, spec);
}
