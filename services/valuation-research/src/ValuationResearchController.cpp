#include "ValuationResearchController.h"

#include "stok/services/common/LocalizationClient.h"

#include <QDateTime>
#include <QVariantMap>
#include <algorithm>

namespace {

QVariantMap card(const QString& label, const QString& value, const QString& note, const QString& tone)
{
    return {{"label", label}, {"value", value}, {"note", note}, {"tone", tone}};
}

QString locTr(stok::services::common::LocalizationClient* loc, const QString& key)
{
    return loc ? loc->tr(key) : key;
}

void define(stok::services::common::LocalizationClient* loc, const QString& key,
            const QString& en, const QString& zh)
{
    if (loc) loc->define(key, en, zh);
}

QVariantMap row(
    stok::services::common::LocalizationClient* loc,
    const QString& symbol,
    const QString& name,
    double price,
    double fairValue,
    double pePercentile,
    double pbPercentile,
    double dividendYield,
    double roe,
    double roic,
    double fcfQuality)
{
    const double margin = fairValue <= 0.0 ? 0.0 : (fairValue - price) / fairValue * 100.0;
    QString rating = locTr(loc, QStringLiteral("vr.rating.watch"));
    QString tone = QStringLiteral("amber");
    if (margin >= 15.0 && fcfQuality >= 80.0)
    {
        rating = locTr(loc, QStringLiteral("vr.rating.undervalued"));
        tone = QStringLiteral("green");
    }
    else if (margin < -8.0 || pePercentile > 75.0)
    {
        rating = locTr(loc, QStringLiteral("vr.rating.expensive"));
        tone = QStringLiteral("red");
    }

    QString action = locTr(loc, QStringLiteral("vr.action.followup"));
    if (margin >= 15.0 && fcfQuality >= 80.0 && pePercentile <= 55.0)
    {
        action = locTr(loc, QStringLiteral("vr.action.add"));
    }
    else if (margin < -12.0 || pePercentile >= 80.0)
    {
        action = locTr(loc, QStringLiteral("vr.action.sell"));
    }
    else if (margin < -5.0 || fcfQuality < 68.0)
    {
        action = locTr(loc, QStringLiteral("vr.action.rotate"));
    }

    return {
        {"symbol", symbol},
        {"name", name},
        {"price", price},
        {"fairValue", fairValue},
        {"buyBelow", fairValue * 0.85},
        {"sellAbove", fairValue * 1.18},
        {"margin", margin},
        {"pePercentile", pePercentile},
        {"pbPercentile", pbPercentile},
        {"dividendYield", dividendYield},
        {"roe", roe},
        {"roic", roic},
        {"fcfQuality", fcfQuality},
        {"action", action},
        {"confidence", std::min(95.0, std::max(45.0, fcfQuality * 0.55 + std::max(0.0, margin) * 1.2))},
        {"riskFlag", margin < 0.0 ? locTr(loc, QStringLiteral("vr.risk.premium"))
                                  : locTr(loc, QStringLiteral("vr.risk.margin"))},
        {"rating", rating},
        {"tone", tone}
    };
}

} // namespace

ValuationResearchController::ValuationResearchController(
    stok::services::common::LocalizationClient* localization, QObject* parent):
    QObject(parent),
    localization_(localization)
{
    if (localization_)
    {
        define(localization_, QStringLiteral("vr.rating.watch"), QStringLiteral("Watch"), QStringLiteral(u"观察"));
        define(localization_, QStringLiteral("vr.rating.undervalued"), QStringLiteral("Undervalued"), QStringLiteral(u"低估"));
        define(localization_, QStringLiteral("vr.rating.expensive"), QStringLiteral("Expensive"), QStringLiteral(u"偏贵"));
        define(localization_, QStringLiteral("vr.action.followup"), QStringLiteral("Follow up"), QStringLiteral(u"跟进"));
        define(localization_, QStringLiteral("vr.action.add"), QStringLiteral("Add"), QStringLiteral(u"加仓"));
        define(localization_, QStringLiteral("vr.action.sell"), QStringLiteral("Sell"), QStringLiteral(u"卖出"));
        define(localization_, QStringLiteral("vr.action.rotate"), QStringLiteral("Rotate"), QStringLiteral(u"转仓"));
        define(localization_, QStringLiteral("vr.risk.premium"), QStringLiteral("Premium"), QStringLiteral(u"估值溢价"));
        define(localization_, QStringLiteral("vr.risk.margin"), QStringLiteral("Margin of safety"), QStringLiteral(u"安全边际"));

        define(localization_, QStringLiteral("vr.status.fmt"),
               QStringLiteral("Valuation models updated  %1"),
               QStringLiteral(u"估值模型已更新  %1"));

        define(localization_, QStringLiteral("vr.name.spy"), QStringLiteral("S&P 500 ETF"), QStringLiteral(u"标普500 ETF"));
        define(localization_, QStringLiteral("vr.name.qqq"), QStringLiteral("Nasdaq-100 ETF"), QStringLiteral(u"纳指100 ETF"));

        define(localization_, QStringLiteral("vr.card.margin.label"), QStringLiteral("Average margin of safety"), QStringLiteral(u"安全边际均值"));
        define(localization_, QStringLiteral("vr.card.margin.note"), QStringLiteral("DCF vs current price"), QStringLiteral(u"DCF 与当前价差"));
        define(localization_, QStringLiteral("vr.card.attractive.label"), QStringLiteral("Add-eligible names"), QStringLiteral(u"可加仓标的"));
        define(localization_, QStringLiteral("vr.card.attractive.note"), QStringLiteral("Margin, cash flow and valuation percentile all clear"), QStringLiteral(u"安全边际、现金流与估值分位同时达标"));
        define(localization_, QStringLiteral("vr.card.fcf.label"), QStringLiteral("FCF quality"), QStringLiteral(u"FCF 质量"));
        define(localization_, QStringLiteral("vr.card.fcf.note"), QStringLiteral("Cash flow coverage and stability"), QStringLiteral(u"现金流覆盖利润和稳定性"));
        define(localization_, QStringLiteral("vr.card.rotate.label"), QStringLiteral("Rotate / sell"), QStringLiteral(u"需转仓/卖出"));
        define(localization_, QStringLiteral("vr.card.rotate.note"), QStringLiteral("Overvalued or weak fundamentals"), QStringLiteral(u"高估值或基本面质量不足"));

        define(localization_, QStringLiteral("vr.research.fundamentals.title"), QStringLiteral("Earnings summary"), QStringLiteral(u"财报摘要"));
        define(localization_, QStringLiteral("vr.research.fundamentals.detail"), QStringLiteral("AAPL services revenue resilient; buybacks support EPS; track next-quarter guidance."), QStringLiteral(u"AAPL 服务收入韧性强，回购继续支撑 EPS，需跟踪下季指引。"));
        define(localization_, QStringLiteral("vr.research.fundamentals.evidence"), QStringLiteral("Cash flow+"), QStringLiteral(u"现金流+"));
        define(localization_, QStringLiteral("vr.research.ai.title"), QStringLiteral("AI notes"), QStringLiteral(u"AI 纪要"));
        define(localization_, QStringLiteral("vr.research.ai.detail"), QStringLiteral("Track NVDA datacenter growth, gross margin and capex."), QStringLiteral(u"NVDA 需要跟踪数据中心增速、毛利率和资本开支。"));
        define(localization_, QStringLiteral("vr.research.ai.tag"), QStringLiteral("3 items"), QStringLiteral(u"3 条"));
        define(localization_, QStringLiteral("vr.research.ai.evidence"), QStringLiteral("Growth conviction"), QStringLiteral(u"增长兑现"));
        define(localization_, QStringLiteral("vr.research.assumptions.title"), QStringLiteral("Valuation assumptions"), QStringLiteral(u"估值假设"));
        define(localization_, QStringLiteral("vr.research.assumptions.detail"), QStringLiteral("Discount rate 8.6%, terminal growth 2.5%, downside scenario priced in."), QStringLiteral(u"折现率 8.6%，永续增长 2.5%，逆周期压力情景已纳入。"));
        define(localization_, QStringLiteral("vr.research.assumptions.evidence"), QStringLiteral("Scenario model"), QStringLiteral(u"情景模型"));
        define(localization_, QStringLiteral("vr.research.history.title"), QStringLiteral("Past decisions"), QStringLiteral(u"历史决策"));
        define(localization_, QStringLiteral("vr.research.history.detail"), QStringLiteral("Wait for >15% margin of safety; build positions in tranches; never chase into earnings."), QStringLiteral(u"等待安全边际大于 15% 后分批建仓，避免财报前追高。"));
        define(localization_, QStringLiteral("vr.research.history.tag"), QStringLiteral("Review"), QStringLiteral(u"复盘"));
        define(localization_, QStringLiteral("vr.research.history.evidence"), QStringLiteral("Discipline"), QStringLiteral(u"纪律"));

        define(localization_, QStringLiteral("vr.thesis.business.title"), QStringLiteral("Business quality"), QStringLiteral(u"商业质量"));
        define(localization_, QStringLiteral("vr.thesis.business.detail"), QStringLiteral("Moat, pricing power, return on capital and cash conversion."), QStringLiteral(u"护城河、定价权、资本回报率和现金流转化率。"));
        define(localization_, QStringLiteral("vr.thesis.business.evidence"), QStringLiteral("Required to add"), QStringLiteral(u"通过后才可加仓"));
        define(localization_, QStringLiteral("vr.thesis.valuation.title"), QStringLiteral("Valuation discipline"), QStringLiteral(u"估值纪律"));
        define(localization_, QStringLiteral("vr.thesis.valuation.detail"), QStringLiteral("DCF, PE/PB percentile and dividend yield jointly confirm buy/sell triggers."), QStringLiteral(u"DCF、PE/PB 分位和股息率共同确认买入/卖出线。"));
        define(localization_, QStringLiteral("vr.thesis.valuation.evidence"), QStringLiteral("Act only below buy line"), QStringLiteral(u"低于买入线才行动"));
        define(localization_, QStringLiteral("vr.thesis.invalidation.title"), QStringLiteral("Invalidation"), QStringLiteral(u"失效条件"));
        define(localization_, QStringLiteral("vr.thesis.invalidation.detail"), QStringLiteral("Falling ROIC, deteriorating FCF, or repeated guidance cuts."), QStringLiteral(u"ROIC 下滑、FCF 恶化或管理层指引连续下修。"));
        define(localization_, QStringLiteral("vr.thesis.invalidation.evidence"), QStringLiteral("Triggers sell/rotate"), QStringLiteral(u"触发卖出/转仓"));

        // QML-side section labels and column headers
        define(localization_, QStringLiteral("action.refresh"), QStringLiteral("Refresh"), QStringLiteral(u"刷新"));
        define(localization_, QStringLiteral("vr.section.matrix"), QStringLiteral("Valuation matrix"), QStringLiteral(u"估值模型矩阵"));
        define(localization_, QStringLiteral("vr.section.matrix.subtitle"), QStringLiteral("DCF / PE-PB percentile / ROE-ROIC / FCF"), QStringLiteral(u"DCF / PE-PB 分位 / ROE-ROIC / FCF"));
        define(localization_, QStringLiteral("vr.col.symbol"), QStringLiteral("Symbol"), QStringLiteral(u"代码"));
        define(localization_, QStringLiteral("vr.col.name"), QStringLiteral("Name"), QStringLiteral(u"名称"));
        define(localization_, QStringLiteral("vr.col.price"), QStringLiteral("Price"), QStringLiteral(u"现价"));
        define(localization_, QStringLiteral("vr.col.fair"), QStringLiteral("Fair value"), QStringLiteral(u"合理价"));
        define(localization_, QStringLiteral("vr.col.margin"), QStringLiteral("Margin"), QStringLiteral(u"安全边际"));
        define(localization_, QStringLiteral("vr.col.bands"), QStringLiteral("Buy / Sell"), QStringLiteral(u"买/卖线"));
        define(localization_, QStringLiteral("vr.col.action"), QStringLiteral("Action"), QStringLiteral(u"动作"));
        define(localization_, QStringLiteral("vr.section.research"), QStringLiteral("Research files"), QStringLiteral(u"研究档案"));
        define(localization_, QStringLiteral("vr.section.thesis"), QStringLiteral("Investment thesis"), QStringLiteral(u"投资 Thesis 流程"));
        define(localization_, QStringLiteral("vr.band.buy"), QStringLiteral("Buy"), QStringLiteral(u"买"));
        define(localization_, QStringLiteral("vr.band.sell"), QStringLiteral("Sell"), QStringLiteral(u"卖"));

        connect(localization_, &stok::services::common::LocalizationClient::languageChanged,
                this, [this]() { rebuild(); emit dataChanged(); });
    }

    rebuild();
}

QString ValuationResearchController::status() const { return status_; }
QVariantList ValuationResearchController::scoreCards() const { return scoreCards_; }
QVariantList ValuationResearchController::valuationRows() const { return valuationRows_; }
QVariantList ValuationResearchController::researchRows() const { return researchRows_; }
QVariantList ValuationResearchController::thesisRows() const { return thesisRows_; }

void ValuationResearchController::refresh()
{
    rebuild();
    emit dataChanged();
}

void ValuationResearchController::rebuild()
{
    auto* loc = localization_.data();
    status_ = locTr(loc, QStringLiteral("vr.status.fmt")).arg(QDateTime::currentDateTime().toString("HH:mm:ss"));

    valuationRows_ = {
        row(loc, "AAPL", QStringLiteral("Apple"), 169.3, 196.0, 44.0, 51.0, 0.6, 147.0, 58.0, 91.0),
        row(loc, "MSFT", QStringLiteral("Microsoft"), 412.8, 438.0, 63.0, 67.0, 0.8, 36.0, 29.0, 88.0),
        row(loc, "NVDA", QStringLiteral("NVIDIA"), 887.0, 812.0, 82.0, 78.0, 0.0, 69.0, 47.0, 83.0),
        row(loc, "TSLA", QStringLiteral("Tesla"), 172.6, 158.0, 71.0, 58.0, 0.0, 15.0, 9.0, 62.0),
        row(loc, "SPY", locTr(loc, QStringLiteral("vr.name.spy")), 511.4, 497.0, 74.0, 69.0, 1.4, 18.0, 14.0, 76.0),
        row(loc, "QQQ", locTr(loc, QStringLiteral("vr.name.qqq")), 439.1, 416.0, 79.0, 72.0, 0.6, 25.0, 19.0, 81.0)
    };

    double totalMargin = 0.0;
    double totalFcf = 0.0;
    int attractive = 0;
    for (const QVariant& item : valuationRows_)
    {
        const QVariantMap map = item.toMap();
        totalMargin += map.value("margin").toDouble();
        totalFcf += map.value("fcfQuality").toDouble();
        if (map.value("tone").toString() == QStringLiteral("green"))
        {
            ++attractive;
        }
    }

    const double rowCount = static_cast<double>(std::max<qsizetype>(1, valuationRows_.size()));
    const double averageMargin = totalMargin / rowCount;
    const double averageFcf = totalFcf / rowCount;
    scoreCards_ = {
        card(locTr(loc, QStringLiteral("vr.card.margin.label")),
             QStringLiteral("%1%").arg(averageMargin, 0, 'f', 1),
             locTr(loc, QStringLiteral("vr.card.margin.note")), averageMargin >= 0.0 ? "green" : "red"),
        card(locTr(loc, QStringLiteral("vr.card.attractive.label")),
             QStringLiteral("%1").arg(attractive),
             locTr(loc, QStringLiteral("vr.card.attractive.note")), attractive > 0 ? "green" : "amber"),
        card(locTr(loc, QStringLiteral("vr.card.fcf.label")),
             QStringLiteral("%1").arg(averageFcf, 0, 'f', 0),
             locTr(loc, QStringLiteral("vr.card.fcf.note")), "blue"),
        card(locTr(loc, QStringLiteral("vr.card.rotate.label")),
             QStringLiteral("3"),
             locTr(loc, QStringLiteral("vr.card.rotate.note")), "amber")
    };

    researchRows_ = {
        QVariantMap{{"title", locTr(loc, QStringLiteral("vr.research.fundamentals.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.research.fundamentals.detail"))},
                    {"tag", "Q2"},
                    {"evidence", locTr(loc, QStringLiteral("vr.research.fundamentals.evidence"))}, {"tone", "blue"}},
        QVariantMap{{"title", locTr(loc, QStringLiteral("vr.research.ai.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.research.ai.detail"))},
                    {"tag", locTr(loc, QStringLiteral("vr.research.ai.tag"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.research.ai.evidence"))}, {"tone", "amber"}},
        QVariantMap{{"title", locTr(loc, QStringLiteral("vr.research.assumptions.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.research.assumptions.detail"))},
                    {"tag", "DCF"},
                    {"evidence", locTr(loc, QStringLiteral("vr.research.assumptions.evidence"))}, {"tone", "green"}},
        QVariantMap{{"title", locTr(loc, QStringLiteral("vr.research.history.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.research.history.detail"))},
                    {"tag", locTr(loc, QStringLiteral("vr.research.history.tag"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.research.history.evidence"))}, {"tone", "blue"}}
    };

    thesisRows_ = {
        QVariantMap{{"step", "1"}, {"title", locTr(loc, QStringLiteral("vr.thesis.business.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.thesis.business.detail"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.thesis.business.evidence"))}},
        QVariantMap{{"step", "2"}, {"title", locTr(loc, QStringLiteral("vr.thesis.valuation.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.thesis.valuation.detail"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.thesis.valuation.evidence"))}},
        QVariantMap{{"step", "3"}, {"title", locTr(loc, QStringLiteral("vr.thesis.invalidation.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.thesis.invalidation.detail"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.thesis.invalidation.evidence"))}}
    };
}

QObject* createValuationResearchController(QObject* parent, stok::services::common::LocalizationClient* localization)
{
    return new ValuationResearchController(localization, parent);
}
