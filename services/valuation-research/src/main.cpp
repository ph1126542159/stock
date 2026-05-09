#include "stok/services/feature_page/FeaturePageRunner.h"
#include "ValuationResearchController.h"

int main(int argc, char* argv[])
{
    stok::services::feature_page::FeaturePageSpec spec{
        "stok-valuation-research",
        "stok valuation research service",
        "valuation-research.properties",
        "stok.valuation-research",
        "valuation-research",
        "Valuation Research",
        "DCF, PE/PB percentile, FCF quality, and research files",
        "StokValuationResearch",
        nullptr,
        createValuationResearchController
    };
    return stok::services::feature_page::run(argc, argv, spec);
}
