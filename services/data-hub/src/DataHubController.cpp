#include "DataHubController.h"

#include "stok/services/common/LocalizationClient.h"

#include <QDateTime>
#include <QVariantMap>

namespace {

QVariantMap card(const QString& label, const QString& value, const QString& note, const QString& tone)
{
    return {{"label", label}, {"value", value}, {"note", note}, {"tone", tone}};
}

QVariantMap source(
    const QString& name,
    const QString& type,
    const QString& latency,
    const QString& status,
    const QString& tone,
    const QString& qualityScore,
    const QString& coveragePct,
    const QString& freshness)
{
    return {
        {"name", name},
        {"type", type},
        {"latency", latency},
        {"status", status},
        {"tone", tone},
        {"qualityScore", qualityScore},
        {"coveragePct", coveragePct},
        {"freshness", freshness}
    };
}

void define(stok::services::common::LocalizationClient* loc, const QString& key,
            const QString& en, const QString& zh)
{
    if (loc) loc->define(key, en, zh);
}

QString locTr(stok::services::common::LocalizationClient* loc, const QString& key)
{
    return loc ? loc->tr(key) : key;
}

} // namespace

DataHubController::DataHubController(stok::services::common::LocalizationClient* localization,
                                     QObject* parent):
    QObject(parent),
    localization_(localization)
{
    if (localization_)
    {
        // Score cards
        define(localization_, QStringLiteral("dh.card.online.label"), QStringLiteral("Online sources"), QStringLiteral(u"在线数据源"));
        define(localization_, QStringLiteral("dh.card.online.note"), QStringLiteral("Auto-fail over to backup when primary is down"), QStringLiteral(u"主源异常自动切备用源"));
        define(localization_, QStringLiteral("dh.card.coverage.label"), QStringLiteral("Tradable coverage"), QStringLiteral(u"可交易覆盖"));
        define(localization_, QStringLiteral("dh.card.coverage.note"), QStringLiteral("Quotes, financials and valuation fields complete"), QStringLiteral(u"行情、财报、估值三类字段齐全"));
        define(localization_, QStringLiteral("dh.card.fresh.label"), QStringLiteral("Filing freshness"), QStringLiteral(u"财报新鲜度"));
        define(localization_, QStringLiteral("dh.card.fresh.note"), QStringLiteral("Disclosures, dividends and buybacks sync incrementally"), QStringLiteral(u"公告、分红、回购事件增量同步"));
        define(localization_, QStringLiteral("dh.card.issues.label"), QStringLiteral("Open issues"), QStringLiteral(u"异常待处理"));
        define(localization_, QStringLiteral("dh.card.issues.note"), QStringLiteral("Price jumps, gaps and split-adjust diffs in the repair queue"), QStringLiteral(u"价格跳变、缺口、复权差异进入修复队列"));

        // Sources
        define(localization_, QStringLiteral("dh.src.us.name"), QStringLiteral("US quotes"), QStringLiteral(u"美股行情"));
        define(localization_, QStringLiteral("dh.src.us.status"), QStringLiteral("Live"), QStringLiteral(u"实时可用"));
        define(localization_, QStringLiteral("dh.src.us.fresh"), QStringLiteral("under 1s"), QStringLiteral(u"1s 内"));
        define(localization_, QStringLiteral("dh.src.cn.name"), QStringLiteral("A-share quotes"), QStringLiteral(u"A 股行情"));
        define(localization_, QStringLiteral("dh.src.cn.type"), QStringLiteral("SSE/SZSE quotes / indices"), QStringLiteral(u"沪深行情 / 指数"));
        define(localization_, QStringLiteral("dh.src.cn.status"), QStringLiteral("Live"), QStringLiteral(u"实时可用"));
        define(localization_, QStringLiteral("dh.src.cn.fresh"), QStringLiteral("under 2s"), QStringLiteral(u"2s 内"));
        define(localization_, QStringLiteral("dh.src.fund.name"), QStringLiteral("Fund NAV"), QStringLiteral(u"基金净值"));
        define(localization_, QStringLiteral("dh.src.fund.status"), QStringLiteral("End-of-day"), QStringLiteral(u"日终更新"));
        define(localization_, QStringLiteral("dh.src.filings.name"), QStringLiteral("Filings"), QStringLiteral(u"财报源"));
        define(localization_, QStringLiteral("dh.src.filings.type"), QStringLiteral("SEC / HKEX / Cninfo"), QStringLiteral(u"SEC / HKEX / 巨潮"));
        define(localization_, QStringLiteral("dh.src.filings.status"), QStringLiteral("Incremental sync"), QStringLiteral(u"增量同步"));
        define(localization_, QStringLiteral("dh.src.fx.name"), QStringLiteral("FX"), QStringLiteral(u"汇率源"));
        define(localization_, QStringLiteral("dh.src.fx.status"), QStringLiteral("Slight delay"), QStringLiteral(u"轻微延迟"));
        define(localization_, QStringLiteral("dh.src.fx.fresh"), QStringLiteral("under 5s"), QStringLiteral(u"5s 内"));
        define(localization_, QStringLiteral("dh.src.news.name"), QStringLiteral("News & sentiment"), QStringLiteral(u"新闻舆情"));
        define(localization_, QStringLiteral("dh.src.news.type"), QStringLiteral("Disclosures / news / sentiment"), QStringLiteral(u"公告 / 新闻 / 舆情"));
        define(localization_, QStringLiteral("dh.src.news.status"), QStringLiteral("Keyword filtering"), QStringLiteral(u"关键词过滤"));
        define(localization_, QStringLiteral("dh.src.news.fresh"), QStringLiteral("under 2m"), QStringLiteral(u"2m 内"));

        // Latency rows
        define(localization_, QStringLiteral("dh.lat.nasdaq.detail"), QStringLiteral("AAPL, TSLA, NVDA stable; p95 latency 1.2s."), QStringLiteral(u"AAPL, TSLA, NVDA 行情稳定，p95 延迟 1.2s。"));
        define(localization_, QStringLiteral("dh.lat.nyse.detail"), QStringLiteral("SPY, DIA index ETFs nominal; volume fields validated."), QStringLiteral(u"SPY, DIA 指数 ETF 正常，成交量字段已通过校验。"));
        define(localization_, QStringLiteral("dh.lat.fund.name"), QStringLiteral("Fund NAV"), QStringLiteral(u"基金净值"));
        define(localization_, QStringLiteral("dh.lat.fund.detail"), QStringLiteral("Latest NAV synced to portfolio module; dividends and splits awaiting review."), QStringLiteral(u"最新净值已同步到持仓模块，分红与拆分进入复核。"));
        define(localization_, QStringLiteral("dh.lat.fx.name"), QStringLiteral("FX"), QStringLiteral(u"汇率"));
        define(localization_, QStringLiteral("dh.lat.fx.detail"), QStringLiteral("USD/CNY latency elevated; cross-market valuation falling back to backup source."), QStringLiteral(u"USD/CNY 延迟略高，跨市场估值已采用备用源兜底。"));
        define(localization_, QStringLiteral("dh.action.primary"), QStringLiteral("Stay primary"), QStringLiteral(u"维持主源"));
        define(localization_, QStringLiteral("dh.action.endofday"), QStringLiteral("End-of-day refresh"), QStringLiteral(u"日终刷新"));
        define(localization_, QStringLiteral("dh.action.fallback"), QStringLiteral("Switch to backup"), QStringLiteral(u"切备用"));

        // Governance rows
        define(localization_, QStringLiteral("dh.gov.fields.title"), QStringLiteral("Field validation"), QStringLiteral(u"字段校验"));
        define(localization_, QStringLiteral("dh.gov.fields.detail"), QStringLiteral("Price, currency, split factor and filing convention pass consistency checks."), QStringLiteral(u"价格、币种、复权因子、财报口径通过一致性检查。"));
        define(localization_, QStringLiteral("dh.gov.fields.tag"), QStringLiteral("Pass"), QStringLiteral(u"通过"));
        define(localization_, QStringLiteral("dh.gov.fields.action"), QStringLiteral("Ready for modeling"), QStringLiteral(u"可入模"));
        define(localization_, QStringLiteral("dh.gov.gaps.title"), QStringLiteral("Gap repair"), QStringLiteral(u"缺口修复"));
        define(localization_, QStringLiteral("dh.gov.gaps.detail"), QStringLiteral("Two intraday US bars missing; queued for backfill and excluded from high-frequency signals."), QStringLiteral(u"美股历史分时缺 2 个点，已进入补采队列并禁止用于高频信号。"));
        define(localization_, QStringLiteral("dh.gov.gaps.tag"), QStringLiteral("In progress"), QStringLiteral(u"处理中"));
        define(localization_, QStringLiteral("dh.gov.gaps.action"), QStringLiteral("Backfill"), QStringLiteral(u"补采"));
        define(localization_, QStringLiteral("dh.gov.failover.title"), QStringLiteral("Source failover"), QStringLiteral(u"源切换"));
        define(localization_, QStringLiteral("dh.gov.failover.detail"), QStringLiteral("Auto-switch to backup on primary failure; record source, latency and version."), QStringLiteral(u"主源异常时自动切备用源，并记录源、延迟和版本。"));
        define(localization_, QStringLiteral("dh.gov.failover.tag"), QStringLiteral("Enabled"), QStringLiteral(u"启用"));
        define(localization_, QStringLiteral("dh.gov.failover.action"), QStringLiteral("Degraded protection"), QStringLiteral(u"降级保护"));
        define(localization_, QStringLiteral("dh.gov.audit.title"), QStringLiteral("Audit log"), QStringLiteral(u"审计日志"));
        define(localization_, QStringLiteral("dh.gov.audit.detail"), QStringLiteral("Every update keeps source, latency and version for replay of failed suggestions."), QStringLiteral(u"所有数据更新保留来源、延迟、版本，便于复盘错误建议。"));
        define(localization_, QStringLiteral("dh.gov.audit.tag"), QStringLiteral("Complete"), QStringLiteral(u"完整"));
        define(localization_, QStringLiteral("dh.gov.audit.action"), QStringLiteral("Traceable"), QStringLiteral(u"可追溯"));

        // Status string
        define(localization_, QStringLiteral("dh.status.ok.fmt"),
               QStringLiteral("Data sources nominal  %1"),
               QStringLiteral(u"数据源监控正常  %1"));

        // Section headers (also referenced from QML)
        define(localization_, QStringLiteral("source.quality.prefix"), QStringLiteral("Quality"), QStringLiteral(u"质量"));
        define(localization_, QStringLiteral("source.coverage.prefix"), QStringLiteral("Coverage"), QStringLiteral(u"覆盖"));
        define(localization_, QStringLiteral("action.refresh"), QStringLiteral("Refresh"), QStringLiteral(u"刷新"));
        define(localization_, QStringLiteral("datahub.section.sources"), QStringLiteral("Source management"), QStringLiteral(u"数据源管理"));
        define(localization_, QStringLiteral("datahub.section.sources.subtitle"), QStringLiteral("Quotes / Filings / FX / Fund NAV"), QStringLiteral(u"行情源 / 财报源 / 汇率 / 基金净值"));
        define(localization_, QStringLiteral("datahub.section.latency"), QStringLiteral("Latency monitor"), QStringLiteral(u"延迟状态监控"));
        define(localization_, QStringLiteral("datahub.section.governance"), QStringLiteral("Data governance"), QStringLiteral(u"数据治理"));

        connect(localization_, &stok::services::common::LocalizationClient::languageChanged,
                this, [this]() { rebuild(); emit dataChanged(); });
    }

    rebuild();
}

QString DataHubController::status() const { return status_; }
QVariantList DataHubController::scoreCards() const { return scoreCards_; }
QVariantList DataHubController::sourceRows() const { return sourceRows_; }
QVariantList DataHubController::latencyRows() const { return latencyRows_; }
QVariantList DataHubController::governanceRows() const { return governanceRows_; }

void DataHubController::refresh()
{
    rebuild();
    emit dataChanged();
}

void DataHubController::rebuild()
{
    auto* loc = localization_.data();
    const QString now = QDateTime::currentDateTime().toString("HH:mm:ss");
    status_ = locTr(loc, QStringLiteral("dh.status.ok.fmt")).arg(now);

    scoreCards_ = {
        card(locTr(loc, QStringLiteral("dh.card.online.label")), "9 / 10",
             locTr(loc, QStringLiteral("dh.card.online.note")), "green"),
        card(locTr(loc, QStringLiteral("dh.card.coverage.label")), "96.8%",
             locTr(loc, QStringLiteral("dh.card.coverage.note")), "green"),
        card(locTr(loc, QStringLiteral("dh.card.fresh.label")), "T+12m",
             locTr(loc, QStringLiteral("dh.card.fresh.note")), "blue"),
        card(locTr(loc, QStringLiteral("dh.card.issues.label")), "3",
             locTr(loc, QStringLiteral("dh.card.issues.note")), "amber")
    };

    sourceRows_ = {
        source(locTr(loc, QStringLiteral("dh.src.us.name")), "NASDAQ / NYSE quotes", "0.8s",
               locTr(loc, QStringLiteral("dh.src.us.status")), "green", "99.4", "98.1%",
               locTr(loc, QStringLiteral("dh.src.us.fresh"))),
        source(locTr(loc, QStringLiteral("dh.src.cn.name")),
               locTr(loc, QStringLiteral("dh.src.cn.type")), "1.6s",
               locTr(loc, QStringLiteral("dh.src.cn.status")), "green", "98.7", "95.6%",
               locTr(loc, QStringLiteral("dh.src.cn.fresh"))),
        source(locTr(loc, QStringLiteral("dh.src.fund.name")), "NAV / dividend / holdings", "T+1",
               locTr(loc, QStringLiteral("dh.src.fund.status")), "blue", "97.2", "93.4%", "T+1"),
        source(locTr(loc, QStringLiteral("dh.src.filings.name")),
               locTr(loc, QStringLiteral("dh.src.filings.type")), "12m",
               locTr(loc, QStringLiteral("dh.src.filings.status")), "blue", "96.1", "91.2%", "T+12m"),
        source(locTr(loc, QStringLiteral("dh.src.fx.name")), "USD/CNY, HKD/CNY", "4.2s",
               locTr(loc, QStringLiteral("dh.src.fx.status")), "amber", "94.8", "100%",
               locTr(loc, QStringLiteral("dh.src.fx.fresh"))),
        source(locTr(loc, QStringLiteral("dh.src.news.name")),
               locTr(loc, QStringLiteral("dh.src.news.type")), "2m",
               locTr(loc, QStringLiteral("dh.src.news.status")), "green", "92.6", "88.5%",
               locTr(loc, QStringLiteral("dh.src.news.fresh")))
    };

    latencyRows_ = {
        QVariantMap{{"name", "NASDAQ"}, {"detail", locTr(loc, QStringLiteral("dh.lat.nasdaq.detail"))},
                    {"latency", "0.8s"}, {"dataAction", locTr(loc, QStringLiteral("dh.action.primary"))}, {"tone", "green"}},
        QVariantMap{{"name", "NYSE"}, {"detail", locTr(loc, QStringLiteral("dh.lat.nyse.detail"))},
                    {"latency", "1.1s"}, {"dataAction", locTr(loc, QStringLiteral("dh.action.primary"))}, {"tone", "green"}},
        QVariantMap{{"name", locTr(loc, QStringLiteral("dh.lat.fund.name"))},
                    {"detail", locTr(loc, QStringLiteral("dh.lat.fund.detail"))},
                    {"latency", "T+1"}, {"dataAction", locTr(loc, QStringLiteral("dh.action.endofday"))}, {"tone", "blue"}},
        QVariantMap{{"name", locTr(loc, QStringLiteral("dh.lat.fx.name"))},
                    {"detail", locTr(loc, QStringLiteral("dh.lat.fx.detail"))},
                    {"latency", "4.2s"}, {"dataAction", locTr(loc, QStringLiteral("dh.action.fallback"))}, {"tone", "amber"}}
    };

    governanceRows_ = {
        QVariantMap{{"title", locTr(loc, QStringLiteral("dh.gov.fields.title"))},
                    {"detail", locTr(loc, QStringLiteral("dh.gov.fields.detail"))},
                    {"tag", locTr(loc, QStringLiteral("dh.gov.fields.tag"))},
                    {"dataAction", locTr(loc, QStringLiteral("dh.gov.fields.action"))}, {"tone", "green"}},
        QVariantMap{{"title", locTr(loc, QStringLiteral("dh.gov.gaps.title"))},
                    {"detail", locTr(loc, QStringLiteral("dh.gov.gaps.detail"))},
                    {"tag", locTr(loc, QStringLiteral("dh.gov.gaps.tag"))},
                    {"dataAction", locTr(loc, QStringLiteral("dh.gov.gaps.action"))}, {"tone", "amber"}},
        QVariantMap{{"title", locTr(loc, QStringLiteral("dh.gov.failover.title"))},
                    {"detail", locTr(loc, QStringLiteral("dh.gov.failover.detail"))},
                    {"tag", locTr(loc, QStringLiteral("dh.gov.failover.tag"))},
                    {"dataAction", locTr(loc, QStringLiteral("dh.gov.failover.action"))}, {"tone", "green"}},
        QVariantMap{{"title", locTr(loc, QStringLiteral("dh.gov.audit.title"))},
                    {"detail", locTr(loc, QStringLiteral("dh.gov.audit.detail"))},
                    {"tag", locTr(loc, QStringLiteral("dh.gov.audit.tag"))},
                    {"dataAction", locTr(loc, QStringLiteral("dh.gov.audit.action"))}, {"tone", "blue"}}
    };
}

QObject* createDataHubController(QObject* parent, stok::services::common::LocalizationClient* localization)
{
    return new DataHubController(localization, parent);
}
