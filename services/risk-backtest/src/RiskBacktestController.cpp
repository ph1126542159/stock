#include "RiskBacktestController.h"

#include "stok/services/common/LocalizationClient.h"

#include <QDateTime>
#include <QVariantMap>

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

QVariantMap exposure(
    stok::services::common::LocalizationClient* loc,
    const QString& name,
    double value,
    double limit,
    const QString& group,
    const QString& action,
    const QString& riskInstruction)
{
    const double usage = limit <= 0.0 ? 0.0 : value / limit;
    const double suggestedDelta = usage >= 0.9 ? -(value - limit * 0.82)
        : (usage >= 0.75 ? -(value - limit * 0.88) : 0.0);
    return {
        {"name", name},
        {"value", value},
        {"limit", limit},
        {"usage", usage},
        {"group", group},
        {"action", action},
        {"riskInstruction", riskInstruction},
        {"suggestedDelta", suggestedDelta < -0.1 ? QStringLiteral("%1%").arg(suggestedDelta, 0, 'f', 1)
                                                 : locTr(loc, QStringLiteral("rb.delta.hold"))},
        {"varContributionPct", value * 0.04},
        {"drawdownContributionPct", value * 0.11},
        {"tone", usage >= 0.9 ? "red" : (usage >= 0.75 ? "amber" : "green")}
    };
}

} // namespace

RiskBacktestController::RiskBacktestController(
    stok::services::common::LocalizationClient* localization, QObject* parent):
    QObject(parent),
    localization_(localization)
{
    if (localization_)
    {
        define(localization_, QStringLiteral("rb.delta.hold"), QStringLiteral("Hold"), QStringLiteral(u"维持"));
        define(localization_, QStringLiteral("rb.action.followup"), QStringLiteral("Follow up"), QStringLiteral(u"跟进"));
        define(localization_, QStringLiteral("rb.action.sell"), QStringLiteral("Sell"), QStringLiteral(u"卖出"));
        define(localization_, QStringLiteral("rb.action.rotate"), QStringLiteral("Rotate"), QStringLiteral(u"转仓"));

        define(localization_, QStringLiteral("rb.status"),
               QStringLiteral("Risk fail: max drawdown / stress loss exceed -5%, must de-risk  %1"),
               QStringLiteral(u"风控不达标：最大回撤/压力损失超过 -5%，需降风险  %1"));

        define(localization_, QStringLiteral("rb.exp.tech.name"), QStringLiteral("Tech sector"), QStringLiteral(u"科技行业"));
        define(localization_, QStringLiteral("rb.group.sector"), QStringLiteral("Sector"), QStringLiteral(u"行业"));
        define(localization_, QStringLiteral("rb.exp.tech.note"), QStringLiteral("Near limit; no same-direction add."), QStringLiteral(u"接近上限，禁止同向加仓。"));
        define(localization_, QStringLiteral("rb.exp.us.name"), QStringLiteral("US market"), QStringLiteral(u"美国市场"));
        define(localization_, QStringLiteral("rb.group.country"), QStringLiteral("Country"), QStringLiteral(u"国家"));
        define(localization_, QStringLiteral("rb.exp.us.note"), QStringLiteral("Must de-risk; cut over-budget exposure first."), QStringLiteral(u"需降风险，先压降超限风险预算。"));
        define(localization_, QStringLiteral("rb.exp.usd.name"), QStringLiteral("USD assets"), QStringLiteral(u"美元资产"));
        define(localization_, QStringLiteral("rb.group.currency"), QStringLiteral("Currency"), QStringLiteral(u"币种"));
        define(localization_, QStringLiteral("rb.exp.usd.note"), QStringLiteral("Hedge FX exposure; not a trading action."), QStringLiteral(u"对冲汇率敞口，不作为投资动作。"));
        define(localization_, QStringLiteral("rb.exp.aapl.name"), QStringLiteral("AAPL single name"), QStringLiteral(u"AAPL 单票"));
        define(localization_, QStringLiteral("rb.group.single"), QStringLiteral("Single name"), QStringLiteral(u"单票"));
        define(localization_, QStringLiteral("rb.exp.aapl.note"), QStringLiteral("Cap adds; wait for drawdown contribution to fall."), QStringLiteral(u"限加仓，等待回撤贡献下降。"));
        define(localization_, QStringLiteral("rb.exp.nvda.name"), QStringLiteral("NVDA single name"), QStringLiteral(u"NVDA 单票"));
        define(localization_, QStringLiteral("rb.exp.nvda.note"), QStringLiteral("Reduce vol; do not size up into earnings."), QStringLiteral(u"降波动，财报窗口不放大仓位。"));
        define(localization_, QStringLiteral("rb.exp.cash.name"), QStringLiteral("Cash"), QStringLiteral(u"现金仓位"));
        define(localization_, QStringLiteral("rb.group.buffer"), QStringLiteral("Buffer"), QStringLiteral(u"缓冲"));
        define(localization_, QStringLiteral("rb.exp.cash.note"), QStringLiteral("Keep cash buffer."), QStringLiteral(u"保留现金缓冲。"));

        define(localization_, QStringLiteral("rb.card.dd.label"), QStringLiteral("Max drawdown"), QStringLiteral(u"最大回撤"));
        define(localization_, QStringLiteral("rb.card.dd.note"), QStringLiteral("Fail: below -5% target; blocks adds; must de-risk"), QStringLiteral(u"不达标：低于 -5% 目标，阻塞加仓，需降风险"));
        define(localization_, QStringLiteral("rb.card.var.label"), QStringLiteral("Portfolio VaR"), QStringLiteral(u"组合 VaR"));
        define(localization_, QStringLiteral("rb.card.var.note"), QStringLiteral("95% one-day confidence interval"), QStringLiteral(u"95% 单日置信区间"));
        define(localization_, QStringLiteral("rb.card.stress.label"), QStringLiteral("Stress loss"), QStringLiteral(u"压力损失"));
        define(localization_, QStringLiteral("rb.card.stress.note"), QStringLiteral("Fail: stress loss exceeds -5%; must de-risk"), QStringLiteral(u"不达标：压力损失超过 -5%，需降风险"));
        define(localization_, QStringLiteral("rb.card.block.label"), QStringLiteral("Block reasons"), QStringLiteral(u"阻塞原因"));
        define(localization_, QStringLiteral("rb.card.block.value"), QStringLiteral("2 items"), QStringLiteral(u"2 项"));
        define(localization_, QStringLiteral("rb.card.block.note"), QStringLiteral("Both max drawdown and stress loss miss the -5% risk target"), QStringLiteral(u"最大回撤与压力损失均未达 -5% 风控目标"));

        define(localization_, QStringLiteral("rb.corr.tech.instruction"), QStringLiteral("Block same-direction adds"), QStringLiteral(u"禁止同向加仓"));
        define(localization_, QStringLiteral("rb.corr.tech.detail"), QStringLiteral("Mega-cap tech moves together; avoid stacking adds."), QStringLiteral(u"大型科技同向性强，避免重复加仓。"));
        define(localization_, QStringLiteral("rb.corr.nvda.instruction"), QStringLiteral("Reduce leverage into earnings"), QStringLiteral(u"财报前降杠杆"));
        define(localization_, QStringLiteral("rb.corr.nvda.detail"), QStringLiteral("Index and single name share factors; cut shared drawdown."), QStringLiteral(u"指数和个股因子重叠，降低同向回撤。"));
        define(localization_, QStringLiteral("rb.corr.spy.instruction"), QStringLiteral("Rotate to low-correlation names"), QStringLiteral(u"转向低相关标的"));
        define(localization_, QStringLiteral("rb.corr.spy.detail"), QStringLiteral("Core-index exposures highly overlap; they fall together."), QStringLiteral(u"核心指数暴露高度重叠，回撤时会一起下行。"));
        define(localization_, QStringLiteral("rb.corr.cash.pair"), QStringLiteral("Cash / Portfolio"), QStringLiteral(u"现金 / 组合"));
        define(localization_, QStringLiteral("rb.corr.cash.instruction"), QStringLiteral("Keep buffer"), QStringLiteral(u"保留缓冲"));
        define(localization_, QStringLiteral("rb.corr.cash.detail"), QStringLiteral("Cash buffers drawdowns and funds dip-buys."), QStringLiteral(u"现金可作为回撤缓冲和二次买入弹药。"));

        define(localization_, QStringLiteral("rb.bt.valuation.name"), QStringLiteral("Valuation-percentile strategy"), QStringLiteral(u"估值分位策略"));
        define(localization_, QStringLiteral("rb.bt.valuation.instruction"), QStringLiteral("Blocked: drawdown above -5% target"), QStringLiteral(u"阻塞原因：回撤未达 -5% 目标"));
        define(localization_, QStringLiteral("rb.bt.valuation.advice"), QStringLiteral("Fail; must de-risk; bring drawdown under -5% before monthly rebalance."), QStringLiteral(u"不达标，需降风险；月度再平衡前先把回撤压到 -5% 以内。"));
        define(localization_, QStringLiteral("rb.bt.quality.name"), QStringLiteral("Quality-growth strategy"), QStringLiteral(u"质量成长策略"));
        define(localization_, QStringLiteral("rb.bt.quality.instruction"), QStringLiteral("Pause adds"), QStringLiteral(u"暂停加仓"));
        define(localization_, QStringLiteral("rb.bt.quality.advice"), QStringLiteral("Fail; cut high-vol exposure; pause adds when -5% drawdown trigger fires."), QStringLiteral(u"不达标，降低高波动仓位；触发 -5% 回撤阈值时暂停加仓。"));
        define(localization_, QStringLiteral("rb.bt.dividend.name"), QStringLiteral("Defensive dividend strategy"), QStringLiteral(u"股息防御策略"));
        define(localization_, QStringLiteral("rb.bt.dividend.instruction"), QStringLiteral("Hedge drawdown"), QStringLiteral(u"对冲回撤"));
        define(localization_, QStringLiteral("rb.bt.dividend.advice"), QStringLiteral("Fail; use as defensive replacement; aim for portfolio drawdown inside -5%."), QStringLiteral(u"不达标，用于回撤期防御替换，组合目标回撤需向 -5% 内收敛。"));

        // QML-side strings
        define(localization_, QStringLiteral("action.refresh"), QStringLiteral("Refresh"), QStringLiteral(u"刷新"));
        define(localization_, QStringLiteral("rb.section.exposure"), QStringLiteral("Exposure & risk budget"), QStringLiteral(u"敞口与风险预算"));
        define(localization_, QStringLiteral("rb.section.exposure.subtitle"), QStringLiteral("Sector / Country / Currency / Single name / Buffer"), QStringLiteral(u"行业 / 国家 / 币种 / 单票 / 缓冲"));
        define(localization_, QStringLiteral("rb.col.target"), QStringLiteral("Target"), QStringLiteral(u"目标"));
        define(localization_, QStringLiteral("rb.col.usage"), QStringLiteral("Usage"), QStringLiteral(u"占用"));
        define(localization_, QStringLiteral("rb.col.action"), QStringLiteral("Action"), QStringLiteral(u"动作"));
        define(localization_, QStringLiteral("rb.col.delta"), QStringLiteral("Suggested delta"), QStringLiteral(u"建议调整"));
        define(localization_, QStringLiteral("rb.col.var"), QStringLiteral("VaR contribution"), QStringLiteral(u"VaR 贡献"));
        define(localization_, QStringLiteral("rb.col.dd"), QStringLiteral("DD contribution"), QStringLiteral(u"回撤贡献"));
        define(localization_, QStringLiteral("rb.section.correlation"), QStringLiteral("Correlation"), QStringLiteral(u"相关性"));
        define(localization_, QStringLiteral("rb.section.backtest"), QStringLiteral("Backtest"), QStringLiteral(u"回测"));
        define(localization_, QStringLiteral("rb.bt.col.win"), QStringLiteral("Win rate"), QStringLiteral(u"胜率"));
        define(localization_, QStringLiteral("rb.bt.col.dd"), QStringLiteral("Max DD"), QStringLiteral(u"最大回撤"));
        define(localization_, QStringLiteral("rb.bt.col.sharpe"), QStringLiteral("Sharpe"), QStringLiteral(u"Sharpe"));

        connect(localization_, &stok::services::common::LocalizationClient::languageChanged,
                this, [this]() { rebuild(); emit dataChanged(); });
    }

    rebuild();
}

QString RiskBacktestController::status() const { return status_; }
QVariantList RiskBacktestController::scoreCards() const { return scoreCards_; }
QVariantList RiskBacktestController::exposureRows() const { return exposureRows_; }
QVariantList RiskBacktestController::correlationRows() const { return correlationRows_; }
QVariantList RiskBacktestController::backtestRows() const { return backtestRows_; }

void RiskBacktestController::refresh()
{
    rebuild();
    emit dataChanged();
}

void RiskBacktestController::rebuild()
{
    auto* loc = localization_.data();
    status_ = locTr(loc, QStringLiteral("rb.status")).arg(QDateTime::currentDateTime().toString("HH:mm:ss"));

    const QString followUp = locTr(loc, QStringLiteral("rb.action.followup"));
    const QString sell = locTr(loc, QStringLiteral("rb.action.sell"));
    const QString rotate = locTr(loc, QStringLiteral("rb.action.rotate"));

    exposureRows_ = {
        exposure(loc, locTr(loc, QStringLiteral("rb.exp.tech.name")), 42.0, 50.0,
                 locTr(loc, QStringLiteral("rb.group.sector")), followUp,
                 locTr(loc, QStringLiteral("rb.exp.tech.note"))),
        exposure(loc, locTr(loc, QStringLiteral("rb.exp.us.name")), 64.0, 70.0,
                 locTr(loc, QStringLiteral("rb.group.country")), sell,
                 locTr(loc, QStringLiteral("rb.exp.us.note"))),
        exposure(loc, locTr(loc, QStringLiteral("rb.exp.usd.name")), 68.0, 75.0,
                 locTr(loc, QStringLiteral("rb.group.currency")), followUp,
                 locTr(loc, QStringLiteral("rb.exp.usd.note"))),
        exposure(loc, locTr(loc, QStringLiteral("rb.exp.aapl.name")), 13.4, 15.0,
                 locTr(loc, QStringLiteral("rb.group.single")), followUp,
                 locTr(loc, QStringLiteral("rb.exp.aapl.note"))),
        exposure(loc, locTr(loc, QStringLiteral("rb.exp.nvda.name")), 10.8, 12.0,
                 locTr(loc, QStringLiteral("rb.group.single")), followUp,
                 locTr(loc, QStringLiteral("rb.exp.nvda.note"))),
        exposure(loc, locTr(loc, QStringLiteral("rb.exp.cash.name")), 12.0, 25.0,
                 locTr(loc, QStringLiteral("rb.group.buffer")), followUp,
                 locTr(loc, QStringLiteral("rb.exp.cash.note")))
    };

    scoreCards_ = {
        card(locTr(loc, QStringLiteral("rb.card.dd.label")), "-8.7%",
             locTr(loc, QStringLiteral("rb.card.dd.note")), "red"),
        card(locTr(loc, QStringLiteral("rb.card.var.label")), "2.6%",
             locTr(loc, QStringLiteral("rb.card.var.note")), "blue"),
        card(locTr(loc, QStringLiteral("rb.card.stress.label")), "-6.4%",
             locTr(loc, QStringLiteral("rb.card.stress.note")), "red"),
        card(locTr(loc, QStringLiteral("rb.card.block.label")),
             locTr(loc, QStringLiteral("rb.card.block.value")),
             locTr(loc, QStringLiteral("rb.card.block.note")), "red")
    };

    correlationRows_ = {
        QVariantMap{{"pair", "AAPL / MSFT"}, {"value", 0.82},
                    {"action", followUp},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.corr.tech.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("rb.corr.tech.detail"))}, {"tone", "amber"}},
        QVariantMap{{"pair", "NVDA / QQQ"}, {"value", 0.76},
                    {"action", sell},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.corr.nvda.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("rb.corr.nvda.detail"))}, {"tone", "amber"}},
        QVariantMap{{"pair", "SPY / QQQ"}, {"value", 0.91},
                    {"action", rotate},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.corr.spy.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("rb.corr.spy.detail"))}, {"tone", "red"}},
        QVariantMap{{"pair", locTr(loc, QStringLiteral("rb.corr.cash.pair"))}, {"value", -0.08},
                    {"action", followUp},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.corr.cash.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("rb.corr.cash.detail"))}, {"tone", "green"}}
    };

    backtestRows_ = {
        QVariantMap{{"name", locTr(loc, QStringLiteral("rb.bt.valuation.name"))},
                    {"winRate", "61.8%"}, {"maxDrawdown", "-11.2%"}, {"sharpe", "1.42"},
                    {"action", followUp},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.bt.valuation.instruction"))},
                    {"advice", locTr(loc, QStringLiteral("rb.bt.valuation.advice"))}},
        QVariantMap{{"name", locTr(loc, QStringLiteral("rb.bt.quality.name"))},
                    {"winRate", "58.4%"}, {"maxDrawdown", "-15.6%"}, {"sharpe", "1.18"},
                    {"action", sell},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.bt.quality.instruction"))},
                    {"advice", locTr(loc, QStringLiteral("rb.bt.quality.advice"))}},
        QVariantMap{{"name", locTr(loc, QStringLiteral("rb.bt.dividend.name"))},
                    {"winRate", "55.1%"}, {"maxDrawdown", "-6.9%"}, {"sharpe", "1.06"},
                    {"action", rotate},
                    {"riskInstruction", locTr(loc, QStringLiteral("rb.bt.dividend.instruction"))},
                    {"advice", locTr(loc, QStringLiteral("rb.bt.dividend.advice"))}}
    };
}

QObject* createRiskBacktestController(QObject* parent, stok::services::common::LocalizationClient* localization)
{
    return new RiskBacktestController(localization, parent);
}
