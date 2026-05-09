#include "TradeAlertsController.h"

#include "stok/services/common/LocalizationClient.h"

#include <QDateTime>
#include <QVariantMap>

namespace {

constexpr bool kDataQualityGatePassed = false;
constexpr bool kRiskGatePassed = false;

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

} // namespace

TradeAlertsController::TradeAlertsController(
    stok::services::common::LocalizationClient* localization, QObject* parent):
    QObject(parent),
    localization_(localization)
{
    if (localization_)
    {
        define(localization_, QStringLiteral("ta.status.fmt"),
               QStringLiteral("Alert rules online  %1"), QStringLiteral(u"提醒规则在线  %1"));
        define(localization_, QStringLiteral("ta.alert.acknowledged"), QStringLiteral("Acknowledged"), QStringLiteral(u"已确认"));
        define(localization_, QStringLiteral("ta.alert.pending"), QStringLiteral("Pending"), QStringLiteral(u"待处理"));

        define(localization_, QStringLiteral("ta.action.followup"), QStringLiteral("Follow up"), QStringLiteral(u"跟进"));
        define(localization_, QStringLiteral("ta.action.add"), QStringLiteral("Add"), QStringLiteral(u"加仓"));
        define(localization_, QStringLiteral("ta.action.rotate"), QStringLiteral("Rotate"), QStringLiteral(u"转仓"));

        define(localization_, QStringLiteral("ta.card.today.label"), QStringLiteral("Must process today"), QStringLiteral(u"今日必须处理"));
        define(localization_, QStringLiteral("ta.card.today.note"), QStringLiteral("Stop loss, earnings, position, valuation drift"), QStringLiteral(u"止损、财报、仓位、估值偏离"));
        define(localization_, QStringLiteral("ta.card.gate.label"), QStringLiteral("Add gate"), QStringLiteral(u"加仓门禁"));
        define(localization_, QStringLiteral("ta.card.gate.blocked"), QStringLiteral("Data quality / risk gates failed; strong actions blocked"), QStringLiteral(u"数据质量/风控门禁未通过，强动作阻塞"));
        define(localization_, QStringLiteral("ta.card.gate.open"), QStringLiteral("Pulled back to buy line and risk score passing"), QStringLiteral(u"回撤到买入线且风险分达标"));
        define(localization_, QStringLiteral("ta.card.cut.label"), QStringLiteral("Reduce / rotate"), QStringLiteral(u"需减仓/转仓"));
        define(localization_, QStringLiteral("ta.card.cut.note"), QStringLiteral("High valuation, high correlation or drawdown trigger"), QStringLiteral(u"高估值、高相关或回撤触发"));
        define(localization_, QStringLiteral("ta.card.review.label"), QStringLiteral("Review completion"), QStringLiteral(u"复盘完成率"));
        define(localization_, QStringLiteral("ta.card.review.note"), QStringLiteral("Trade record over the last 30 days"), QStringLiteral(u"过去 30 天交易记录"));

        define(localization_, QStringLiteral("ta.aapl.delta.blocked"), QStringLiteral("0% / wait for data and risk to clear"), QStringLiteral(u"0% / 等待数据与风控达标"));
        define(localization_, QStringLiteral("ta.aapl.delta.add"), QStringLiteral("+2%-3%"), QStringLiteral(u"+2%-3%"));
        define(localization_, QStringLiteral("ta.aapl.gate.note"), QStringLiteral("Gate blocks strong actions"), QStringLiteral(u"门禁阻塞强动作"));
        define(localization_, QStringLiteral("ta.aapl.detail.blocked"), QStringLiteral("Data-quality / risk gates block strong actions; only follow up until coverage is complete and risk score passes."), QStringLiteral(u"数据质量/风控门禁阻塞强动作，补齐缺口并确认风险分达标前只跟进。"));
        define(localization_, QStringLiteral("ta.aapl.thesis"), QStringLiteral("Services revenue, cash flow and buybacks support EPS; wait for margin of safety."), QStringLiteral(u"服务收入、现金流和回购支撑 EPS，等待安全边际。"));
        define(localization_, QStringLiteral("ta.aapl.buy.blocked"), QStringLiteral("Pause adds; wait for data-quality / risk gates"), QStringLiteral(u"暂停加仓，等待数据质量/风控门禁恢复"));
        define(localization_, QStringLiteral("ta.aapl.buy"), QStringLiteral("Drawdown 8% or PE percentile below 45%"), QStringLiteral(u"回撤 8% 或 PE 分位低于 45%"));
        define(localization_, QStringLiteral("ta.aapl.sell"), QStringLiteral("DCF premium > 20% or risk score < 55"), QStringLiteral(u"DCF 溢价超过 20% 或风险分低于 55"));
        define(localization_, QStringLiteral("ta.aapl.invalid"), QStringLiteral("Services revenue growth below 5% for two consecutive quarters"), QStringLiteral(u"服务收入增速连续两季低于 5%"));

        define(localization_, QStringLiteral("ta.nvda.delta"), QStringLiteral("0% / no chasing"), QStringLiteral(u"0% / 不追高"));
        define(localization_, QStringLiteral("ta.nvda.trigger"), QStringLiteral("Drawdown 12%"), QStringLiteral(u"回撤 12%"));
        define(localization_, QStringLiteral("ta.nvda.thesis"), QStringLiteral("Strong AI demand but valuation depends on growth conviction."), QStringLiteral(u"AI 需求强，但估值依赖高增长兑现。"));
        define(localization_, QStringLiteral("ta.nvda.buy"), QStringLiteral("Stable gross margin and 12% drawdown"), QStringLiteral(u"毛利率稳定且回撤 12%"));
        define(localization_, QStringLiteral("ta.nvda.sell"), QStringLiteral("Revenue guidance cut or position over limit"), QStringLiteral(u"营收指引下修或仓位超限"));
        define(localization_, QStringLiteral("ta.nvda.invalid"), QStringLiteral("Datacenter growth slows materially"), QStringLiteral(u"数据中心增速显著放缓"));

        define(localization_, QStringLiteral("ta.tsla.delta"), QStringLiteral("-30% rotate to low vol"), QStringLiteral(u"-30% 转低波动"));
        define(localization_, QStringLiteral("ta.tsla.trigger"), QStringLiteral("Break below 155"), QStringLiteral(u"跌破 155"));
        define(localization_, QStringLiteral("ta.tsla.thesis"), QStringLiteral("Watch FSD and storage; auto gross margin is the key variable."), QStringLiteral(u"观察 FSD 与储能进展，整车毛利是核心变量。"));
        define(localization_, QStringLiteral("ta.tsla.buy"), QStringLiteral("Margin recovery and valuation in low percentile"), QStringLiteral(u"毛利修复且估值进入低分位"));
        define(localization_, QStringLiteral("ta.tsla.sell"), QStringLiteral("Deliveries miss and inventory builds"), QStringLiteral(u"交付低于预期且库存升高"));
        define(localization_, QStringLiteral("ta.tsla.invalid"), QStringLiteral("Price war keeps compressing ROIC"), QStringLiteral(u"价格战持续压缩 ROIC"));

        define(localization_, QStringLiteral("ta.alert.type.price"), QStringLiteral("Price"), QStringLiteral(u"价格"));
        define(localization_, QStringLiteral("ta.alert.type.valuation"), QStringLiteral("Valuation"), QStringLiteral(u"估值"));
        define(localization_, QStringLiteral("ta.alert.type.earnings"), QStringLiteral("Earnings"), QStringLiteral(u"财报"));
        define(localization_, QStringLiteral("ta.alert.type.news"), QStringLiteral("News"), QStringLiteral(u"新闻"));
        define(localization_, QStringLiteral("ta.alert.type.flow"), QStringLiteral("Flow"), QStringLiteral(u"资金流"));
        define(localization_, QStringLiteral("ta.alert.aapl.detail"), QStringLiteral("Approaching planned buy band; wait for volume confirmation."), QStringLiteral(u"接近计划买入区间上沿，等待成交量二次确认。"));
        define(localization_, QStringLiteral("ta.alert.spy.instruction"), QStringLiteral("Pause adds"), QStringLiteral(u"暂停加仓"));
        define(localization_, QStringLiteral("ta.alert.spy.detail"), QStringLiteral("PE percentile rose to 74%; index add signal paused."), QStringLiteral(u"PE 分位升至 74%，指数加仓信号暂停。"));
        define(localization_, QStringLiteral("ta.alert.nvda.instruction"), QStringLiteral("Reduce vol"), QStringLiteral(u"降波动"));
        define(localization_, QStringLiteral("ta.alert.nvda.detail"), QStringLiteral("Earnings T-5; raising volatility monitoring frequency."), QStringLiteral(u"财报窗口 T-5，自动提高波动监控频率。"));
        define(localization_, QStringLiteral("ta.alert.tsla.instruction"), QStringLiteral("Re-check regulatory news"), QStringLiteral(u"复核监管新闻"));
        define(localization_, QStringLiteral("ta.alert.tsla.detail"), QStringLiteral("Regulatory news heating up; triggering research-file refresh."), QStringLiteral(u"监管新闻热度上升，触发研究档案更新。"));
        define(localization_, QStringLiteral("ta.alert.qqq.detail"), QStringLiteral("ETF net inflow expanded for 3 days; wait for valuation percentile to drop."), QStringLiteral(u"ETF 净流入连续 3 日放大，等待估值分位回落。"));

        define(localization_, QStringLiteral("ta.review.aapl.title"), QStringLiteral("AAPL: didn't chase"), QStringLiteral(u"AAPL 未追高"));
        define(localization_, QStringLiteral("ta.review.aapl.detail"), QStringLiteral("Plan untriggered; discipline maintained."), QStringLiteral(u"原计划未触发，纪律执行良好。"));
        define(localization_, QStringLiteral("ta.review.cash.title"), QStringLiteral("Cash buffer kept"), QStringLiteral(u"组合现金缓冲"));
        define(localization_, QStringLiteral("ta.review.cash.detail"), QStringLiteral("Held 12% cash through risk-on warming."), QStringLiteral(u"风险升温阶段保留 12% 现金。"));
        define(localization_, QStringLiteral("ta.review.nvda.title"), QStringLiteral("De-risked into NVDA earnings"), QStringLiteral(u"NVDA 财报前降波动"));
        define(localization_, QStringLiteral("ta.review.nvda.detail"), QStringLiteral("Cut high-correlation holdings to avoid same-direction drawdown."), QStringLiteral(u"减少高相关持仓，避免同向回撤。"));

        // QML-side strings
        define(localization_, QStringLiteral("action.refresh"), QStringLiteral("Refresh"), QStringLiteral(u"刷新"));
        define(localization_, QStringLiteral("ta.section.plans"), QStringLiteral("Trade plan"), QStringLiteral(u"交易计划"));
        define(localization_, QStringLiteral("ta.section.plans.subtitle"), QStringLiteral("Buy / sell rules, stop-loss / take-profit, invalidation"), QStringLiteral(u"买卖条件、止损止盈、失效条件"));
        define(localization_, QStringLiteral("ta.col.symbol"), QStringLiteral("Symbol"), QStringLiteral(u"代码"));
        define(localization_, QStringLiteral("ta.col.action"), QStringLiteral("Action"), QStringLiteral(u"动作"));
        define(localization_, QStringLiteral("ta.col.delta"), QStringLiteral("Position delta"), QStringLiteral(u"仓位变化"));
        define(localization_, QStringLiteral("ta.col.trigger"), QStringLiteral("Trigger"), QStringLiteral(u"触发条件"));
        define(localization_, QStringLiteral("ta.col.stoploss"), QStringLiteral("Stop loss"), QStringLiteral(u"止损"));
        define(localization_, QStringLiteral("ta.col.takeprofit"), QStringLiteral("Take profit"), QStringLiteral(u"止盈"));
        define(localization_, QStringLiteral("ta.col.thesis"), QStringLiteral("Thesis"), QStringLiteral(u"逻辑"));
        define(localization_, QStringLiteral("ta.col.buy"), QStringLiteral("Buy condition"), QStringLiteral(u"买入条件"));
        define(localization_, QStringLiteral("ta.col.sell"), QStringLiteral("Sell condition"), QStringLiteral(u"卖出条件"));
        define(localization_, QStringLiteral("ta.col.invalid"), QStringLiteral("Invalidation"), QStringLiteral(u"失效条件"));
        define(localization_, QStringLiteral("ta.section.alerts"), QStringLiteral("Live alerts"), QStringLiteral(u"实时提醒"));
        define(localization_, QStringLiteral("ta.section.review"), QStringLiteral("Review log"), QStringLiteral(u"复盘记录"));
        define(localization_, QStringLiteral("ta.action.acknowledge"), QStringLiteral("Acknowledge"), QStringLiteral(u"确认"));

        connect(localization_, &stok::services::common::LocalizationClient::languageChanged,
                this, [this]() { rebuild(); emit dataChanged(); });
    }

    rebuild();
}

QString TradeAlertsController::status() const { return status_; }
QVariantList TradeAlertsController::scoreCards() const { return scoreCards_; }
QVariantList TradeAlertsController::planRows() const { return planRows_; }
QVariantList TradeAlertsController::alertRows() const { return alertRows_; }
QVariantList TradeAlertsController::reviewRows() const { return reviewRows_; }

void TradeAlertsController::refresh()
{
    rebuild();
    emit dataChanged();
}

void TradeAlertsController::acknowledgeAlert(int index)
{
    if (index < 0 || index >= alertRows_.size())
    {
        return;
    }

    QVariantMap item = alertRows_.at(index).toMap();
    item["status"] = locTr(localization_.data(), QStringLiteral("ta.alert.acknowledged"));
    item["tone"] = QStringLiteral("green");
    alertRows_[index] = item;
    emit dataChanged();
}

void TradeAlertsController::rebuild()
{
    auto* loc = localization_.data();
    status_ = locTr(loc, QStringLiteral("ta.status.fmt")).arg(QDateTime::currentDateTime().toString("HH:mm:ss"));
    const bool gateBlocked = !kDataQualityGatePassed || !kRiskGatePassed;

    const QString followUp = locTr(loc, QStringLiteral("ta.action.followup"));
    const QString add = locTr(loc, QStringLiteral("ta.action.add"));
    const QString rotate = locTr(loc, QStringLiteral("ta.action.rotate"));
    const QString pending = locTr(loc, QStringLiteral("ta.alert.pending"));

    scoreCards_ = {
        card(locTr(loc, QStringLiteral("ta.card.today.label")), "4",
             locTr(loc, QStringLiteral("ta.card.today.note")), "amber"),
        card(locTr(loc, QStringLiteral("ta.card.gate.label")), gateBlocked ? "0" : "2",
             gateBlocked ? locTr(loc, QStringLiteral("ta.card.gate.blocked"))
                         : locTr(loc, QStringLiteral("ta.card.gate.open")),
             gateBlocked ? "amber" : "green"),
        card(locTr(loc, QStringLiteral("ta.card.cut.label")), "3",
             locTr(loc, QStringLiteral("ta.card.cut.note")), "red"),
        card(locTr(loc, QStringLiteral("ta.card.review.label")), "86%",
             locTr(loc, QStringLiteral("ta.card.review.note")), "green")
    };

    planRows_ = {
        QVariantMap{
            {"symbol", "AAPL"},
            {"action", gateBlocked ? followUp : add},
            {"priority", QStringLiteral("P1")},
            {"positionDelta", gateBlocked ? locTr(loc, QStringLiteral("ta.aapl.delta.blocked"))
                                          : locTr(loc, QStringLiteral("ta.aapl.delta.add"))},
            {"triggerPrice", QStringLiteral("<= 158")},
            {"stopLoss", QStringLiteral("-6.5%")},
            {"takeProfit", QStringLiteral("+18%")},
            {"riskInstruction", gateBlocked ? locTr(loc, QStringLiteral("ta.aapl.gate.note")) : QString()},
            {"detail", gateBlocked ? locTr(loc, QStringLiteral("ta.aapl.detail.blocked")) : QString()},
            {"thesis", locTr(loc, QStringLiteral("ta.aapl.thesis"))},
            {"buy", gateBlocked ? locTr(loc, QStringLiteral("ta.aapl.buy.blocked"))
                                : locTr(loc, QStringLiteral("ta.aapl.buy"))},
            {"sell", locTr(loc, QStringLiteral("ta.aapl.sell"))},
            {"invalid", locTr(loc, QStringLiteral("ta.aapl.invalid"))},
            {"tone", gateBlocked ? "amber" : "green"}
        },
        QVariantMap{
            {"symbol", "NVDA"}, {"action", followUp}, {"priority", QStringLiteral("P2")},
            {"positionDelta", locTr(loc, QStringLiteral("ta.nvda.delta"))},
            {"triggerPrice", locTr(loc, QStringLiteral("ta.nvda.trigger"))},
            {"stopLoss", "-7.5%"}, {"takeProfit", "+22%"},
            {"thesis", locTr(loc, QStringLiteral("ta.nvda.thesis"))},
            {"buy", locTr(loc, QStringLiteral("ta.nvda.buy"))},
            {"sell", locTr(loc, QStringLiteral("ta.nvda.sell"))},
            {"invalid", locTr(loc, QStringLiteral("ta.nvda.invalid"))}, {"tone", "amber"}
        },
        QVariantMap{
            {"symbol", "TSLA"}, {"action", rotate}, {"priority", QStringLiteral("P1")},
            {"positionDelta", locTr(loc, QStringLiteral("ta.tsla.delta"))},
            {"triggerPrice", locTr(loc, QStringLiteral("ta.tsla.trigger"))},
            {"stopLoss", "-5.0%"}, {"takeProfit", "+12%"},
            {"thesis", locTr(loc, QStringLiteral("ta.tsla.thesis"))},
            {"buy", locTr(loc, QStringLiteral("ta.tsla.buy"))},
            {"sell", locTr(loc, QStringLiteral("ta.tsla.sell"))},
            {"invalid", locTr(loc, QStringLiteral("ta.tsla.invalid"))}, {"tone", "amber"}
        }
    };

    alertRows_ = {
        QVariantMap{{"type", locTr(loc, QStringLiteral("ta.alert.type.price"))}, {"target", "AAPL"},
                    {"action", followUp},
                    {"detail", locTr(loc, QStringLiteral("ta.alert.aapl.detail"))},
                    {"status", pending}, {"tone", "green"}},
        QVariantMap{{"type", locTr(loc, QStringLiteral("ta.alert.type.valuation"))}, {"target", "SPY"},
                    {"action", followUp},
                    {"riskInstruction", locTr(loc, QStringLiteral("ta.alert.spy.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("ta.alert.spy.detail"))},
                    {"status", pending}, {"tone", "amber"}},
        QVariantMap{{"type", locTr(loc, QStringLiteral("ta.alert.type.earnings"))}, {"target", "NVDA"},
                    {"action", followUp},
                    {"riskInstruction", locTr(loc, QStringLiteral("ta.alert.nvda.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("ta.alert.nvda.detail"))},
                    {"status", pending}, {"tone", "blue"}},
        QVariantMap{{"type", locTr(loc, QStringLiteral("ta.alert.type.news"))}, {"target", "TSLA"},
                    {"action", rotate},
                    {"riskInstruction", locTr(loc, QStringLiteral("ta.alert.tsla.instruction"))},
                    {"detail", locTr(loc, QStringLiteral("ta.alert.tsla.detail"))},
                    {"status", pending}, {"tone", "amber"}},
        QVariantMap{{"type", locTr(loc, QStringLiteral("ta.alert.type.flow"))}, {"target", "QQQ"},
                    {"action", followUp},
                    {"detail", locTr(loc, QStringLiteral("ta.alert.qqq.detail"))},
                    {"status", pending}, {"tone", "green"}}
    };

    reviewRows_ = {
        QVariantMap{{"date", "2026-04-22"},
                    {"title", locTr(loc, QStringLiteral("ta.review.aapl.title"))},
                    {"detail", locTr(loc, QStringLiteral("ta.review.aapl.detail"))}, {"tone", "green"}},
        QVariantMap{{"date", "2026-04-20"},
                    {"title", locTr(loc, QStringLiteral("ta.review.cash.title"))},
                    {"detail", locTr(loc, QStringLiteral("ta.review.cash.detail"))}, {"tone", "amber"}},
        QVariantMap{{"date", "2026-04-18"},
                    {"title", locTr(loc, QStringLiteral("ta.review.nvda.title"))},
                    {"detail", locTr(loc, QStringLiteral("ta.review.nvda.detail"))}, {"tone", "blue"}}
    };
}

QObject* createTradeAlertsController(QObject* parent, stok::services::common::LocalizationClient* localization)
{
    return new TradeAlertsController(localization, parent);
}
