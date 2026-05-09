#include "stok/services/feature_page/FeaturePageRunner.h"
#include "RiskBacktestController.h"

int main(int argc, char* argv[])
{
    const stok::services::feature_page::FeaturePageSpec spec{
        "stok-risk-backtest",
        "stok risk and backtest service",
        "risk-backtest.properties",
        "stok.risk-backtest",
        "risk-backtest",
        "Risk Backtest",
        "Exposure, drawdown, correlation, model portfolio, and backtest",
        "StokRiskBacktest",
        createRiskBacktestController
    };
    return stok::services::feature_page::run(argc, argv, spec);
}
