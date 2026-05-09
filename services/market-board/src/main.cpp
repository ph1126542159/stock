#include "MarketBoardController.h"

#include "QCommandLineOption"
#include "QCommandLineParser"
#include "QDir"
#include "QGuiApplication"
#include "QQmlApplicationEngine"
#include "QQmlContext"
#include "QWindow"
#include "stok/services/common/HostedViewPublisher.h"
#include "stok/services/common/LocalizationClient.h"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TelemetryBootstrap.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("stok-market-board");
    QCoreApplication::setOrganizationName("stok");

    QCommandLineParser parser;
    parser.setApplicationDescription("stok market board");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << "c" << "config", "Path to the market config file.", "file"));
    parser.process(app);

    const QString defaultConfigPath = QDir(QCoreApplication::applicationDirPath()).filePath("market-board.properties");
    const std::string configPath = parser.isSet("config") ? parser.value("config").toStdString() : defaultConfigPath.toStdString();
    auto configuration = stok::services::common::loadServiceConfiguration(configPath);
    const auto identity = stok::services::common::readServiceIdentity(*configuration, "stok.market-board");

    stok::services::common::ServiceTelemetry telemetry(identity, configuration, configPath);
    telemetry.install();
    MarketBoardController controller(configuration, QString::fromStdString(configPath), telemetry);

    auto hostedViewSettings = stok::services::common::readDdsSettings(*configuration);
    hostedViewSettings.topicName = configuration->getString("dds.hostedViewTopic", "stok.ui.hosted-views");
    const QString menuId = QString::fromStdString(configuration->getString("ui.menu.id", "market-board"));
    QString menuTitle = QString::fromStdString(configuration->getString("ui.menu.title", ""));
    QString menuDescription = QString::fromStdString(configuration->getString("ui.menu.description", ""));
    const QString menuGroup = QString::fromStdString(configuration->getString("ui.menu.group", "main"));
    if (menuTitle.isEmpty()) menuTitle = QStringLiteral(u"\u884c\u60c5\u699c");
    if (menuDescription.isEmpty()) menuDescription = QStringLiteral(u"\u884c\u60c5\u5206\u6790\u3001\u673a\u6784\u699c\u5355\u3001\u57fa\u91d1/\u80a1\u7968\u673a\u4f1a\u548c\u5b9e\u65f6\u8bc4\u5206");

    stok::services::common::LocalizationClient localization;
    localization.define(QStringLiteral("window.title"),
        QStringLiteral("Market"), QStringLiteral(u"\u884c\u60c5\u699c"));

    // Tab bar
    localization.define(QStringLiteral("mb.tab.institutions"),
        QStringLiteral("Institutions"), QStringLiteral(u"\u56fd\u5185\u673a\u6784\u699c"));
    localization.define(QStringLiteral("mb.tab.usWatch"),
        QStringLiteral("US watch"), QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf"));
    localization.define(QStringLiteral("mb.tab.indices"),
        QStringLiteral("Indices"), QStringLiteral(u"\u5927\u76d8\u884c\u60c5"));
    localization.define(QStringLiteral("mb.tab.flow"),
        QStringLiteral("Capital flow"), QStringLiteral(u"\u8d44\u91d1\u6d41\u5411"));
    localization.define(QStringLiteral("mb.tab.rotation"),
        QStringLiteral("Rotation"), QStringLiteral(u"\u884c\u4e1a\u8f6e\u52a8"));
    localization.define(QStringLiteral("mb.tab.valuation"),
        QStringLiteral("Valuation"), QStringLiteral(u"\u4f30\u503c\u6e29\u5ea6"));
    localization.define(QStringLiteral("mb.tab.earnings"),
        QStringLiteral("Earnings"), QStringLiteral(u"\u8d22\u62a5\u65e5\u5386"));
    localization.define(QStringLiteral("mb.tab.fundLookthrough"),
        QStringLiteral("Fund look-through"), QStringLiteral(u"\u57fa\u91d1\u7a7f\u900f"));
    localization.define(QStringLiteral("mb.tab.riskAlerts"),
        QStringLiteral("Risk alerts"), QStringLiteral(u"\u98ce\u9669\u9884\u8b66"));
    localization.define(QStringLiteral("mb.tab.diagnostics"),
        QStringLiteral("Diagnostics"), QStringLiteral(u"\u7ec4\u5408\u8bca\u65ad"));
    localization.define(QStringLiteral("mb.tab.tradePlans"),
        QStringLiteral("Trade plans"), QStringLiteral(u"\u4ea4\u6613\u8ba1\u5212"));

    // Section headers
    localization.define(QStringLiteral("mb.section.institutions"),
        QStringLiteral("Domestic institutional rankings"), QStringLiteral(u"\u56fd\u5185\u673a\u6784\u699c"));
    localization.define(QStringLiteral("mb.section.institutions.subtitle"),
        QStringLiteral("Double-click a row to open the value rankings"),
        QStringLiteral(u"\u53cc\u51fb\u4efb\u610f\u4e00\u884c\u8fdb\u5165\u4ef7\u503c\u6295\u8d44\u699c"));
    localization.define(QStringLiteral("mb.action.update.1h"),
        QStringLiteral("Updates hourly"), QStringLiteral(u"1 \u5c0f\u65f6\u66f4\u65b0\u4e00\u6b21"));
    localization.define(QStringLiteral("mb.section.usWatch"),
        QStringLiteral("US watchlist & indices"), QStringLiteral(u"\u7f8e\u80a1\u4e0e\u6307\u6570\u81ea\u9009"));
    localization.define(QStringLiteral("mb.section.usWatch.subtitle"),
        QStringLiteral("Default includes Nasdaq, S&P, Dow Jones, Apple \u2014 manually addable"),
        QStringLiteral(u"\u9ed8\u8ba4\u5305\u542b\u7eb3\u6307\u3001\u6807\u666e\u3001\u9053\u743c\u65af\u3001Apple \u7b49\uff0c\u652f\u6301\u624b\u52a8\u65b0\u589e"));
    localization.define(QStringLiteral("mb.action.realtime"),
        QStringLiteral("Live network quotes / change"),
        QStringLiteral(u"\u771f\u5b9e\u7f51\u7edc\u884c\u60c5 / \u6da8\u8dcc\u5e45"));
    localization.define(QStringLiteral("mb.section.flow"),
        QStringLiteral("Capital flow"), QStringLiteral(u"\u8d44\u91d1\u6d41\u5411\u9875"));
    localization.define(QStringLiteral("mb.section.flow.subtitle"),
        QStringLiteral("Northbound flow, smart-money inflow, sector heat; \"pending source\" shown if no real feed."),
        QStringLiteral(u"\u5317\u5411\u8d44\u91d1\u3001\u4e3b\u529b\u51c0\u6d41\u5165\u3001\u884c\u4e1a\u8d44\u91d1\u70ed\u5ea6\uff1b\u6ca1\u6709\u771f\u5b9e\u8d44\u91d1\u6e90\u65f6\u660e\u786e\u663e\u793a\u5f85\u63a5\u5165\u3002"));
    localization.define(QStringLiteral("mb.section.rotation"),
        QStringLiteral("Sector / theme rotation"), QStringLiteral(u"\u884c\u4e1a/\u4e3b\u9898\u8f6e\u52a8\u9875"));
    localization.define(QStringLiteral("mb.section.rotation.subtitle"),
        QStringLiteral("Derives strength, persistence, crowding and rotation from real broad-market quotes."),
        QStringLiteral(u"\u6309\u771f\u5b9e\u5bbd\u57fa\u884c\u60c5\u63a8\u5bfc\u5f3a\u5f31\u65b9\u5411\u3001\u8fde\u7eed\u6027\u3001\u62e5\u6324\u5ea6\u548c\u8f6e\u52a8\u52a8\u4f5c\u3002"));
    localization.define(QStringLiteral("mb.section.valuation"),
        QStringLiteral("Valuation temperature"), QStringLiteral(u"\u4f30\u503c\u6e29\u5ea6\u9875"));
    localization.define(QStringLiteral("mb.section.valuation.subtitle"),
        QStringLiteral("PE/PB percentile, equity-bond yield, broad-market range; no fakery when source absent."),
        QStringLiteral(u"PE/PB \u5206\u4f4d\u3001\u80a1\u503a\u6027\u4ef7\u6bd4\u3001\u5bbd\u57fa\u4f30\u503c\u533a\u95f4\uff1b\u4f30\u503c\u6e90\u672a\u63a5\u5165\u65f6\u4e0d\u4f2a\u9020\u3002"));
    localization.define(QStringLiteral("mb.section.earnings"),
        QStringLiteral("Earnings calendar"), QStringLiteral(u"\u8d22\u62a5\u65e5\u5386\u9875"));
    localization.define(QStringLiteral("mb.section.earnings.subtitle"),
        QStringLiteral("Pre-announcements, earnings dates, beat/miss tags and portfolio impact."),
        QStringLiteral(u"\u4e1a\u7ee9\u9884\u544a\u3001\u8d22\u62a5\u53d1\u5e03\u65e5\u671f\u3001\u8d85\u9884\u671f/\u4f4e\u9884\u671f\u6807\u8bb0\u548c\u6301\u4ed3\u4e8b\u4ef6\u5f71\u54cd\u3002"));
    localization.define(QStringLiteral("mb.section.fund"),
        QStringLiteral("Fund look-through"), QStringLiteral(u"\u57fa\u91d1\u6301\u4ed3\u7a7f\u900f\u9875"));
    localization.define(QStringLiteral("mb.section.fund.subtitle"),
        QStringLiteral("Fund top sectors, top holdings, concentration, quarterly changes."),
        QStringLiteral(u"\u57fa\u91d1\u91cd\u4ed3\u884c\u4e1a\u3001\u91cd\u4ed3\u80a1\u7968\u3001\u6301\u4ed3\u96c6\u4e2d\u5ea6\u3001\u5b63\u5ea6\u53d8\u5316\u3002"));
    localization.define(QStringLiteral("mb.section.risk"),
        QStringLiteral("Risk alerts"), QStringLiteral(u"\u98ce\u9669\u9884\u8b66\u9875"));
    localization.define(QStringLiteral("mb.section.risk.subtitle"),
        QStringLiteral("High valuation, high drawdown, liquidity drop, trend break and other risk signals."),
        QStringLiteral(u"\u9ad8\u4f30\u503c\u3001\u9ad8\u56de\u64a4\u3001\u6d41\u52a8\u6027\u4e0b\u964d\u3001\u8d8b\u52bf\u7834\u4f4d\u7b49\u98ce\u9669\u4fe1\u53f7\u3002"));
    localization.define(QStringLiteral("mb.section.diag"),
        QStringLiteral("Portfolio diagnostics"), QStringLiteral(u"\u7ec4\u5408\u8bca\u65ad\u9875"));
    localization.define(QStringLiteral("mb.section.diag.subtitle"),
        QStringLiteral("Current holding correlation, sector exposure, risk-reward."),
        QStringLiteral(u"\u5f53\u524d\u6301\u4ed3\u76f8\u5173\u6027\u3001\u884c\u4e1a\u66b4\u9732\u3001\u98ce\u9669\u6536\u76ca\u6bd4\u3002"));
    localization.define(QStringLiteral("mb.section.plans"),
        QStringLiteral("Trade plans"), QStringLiteral(u"\u4ea4\u6613\u8ba1\u5212\u9875"));
    localization.define(QStringLiteral("mb.section.plans.subtitle"),
        QStringLiteral("Sell, follow up, add, rotate, exit recommendations and triggers."),
        QStringLiteral(u"\u5356\u51fa\u3001\u8ddf\u8fdb\u3001\u52a0\u4ed3\u3001\u8f6c\u4ed3\u3001\u6e05\u4ed3\u52a8\u4f5c\u5efa\u8bae\u53ca\u89e6\u53d1\u6761\u4ef6\u3002"));
    localization.define(QStringLiteral("mb.section.topFunds"),
        QStringLiteral("Top 10 value-investment funds"),
        QStringLiteral(u"\u524d 10 \u540d\u6700\u5177\u4ef7\u503c\u6295\u8d44\u7684\u57fa\u91d1"));
    localization.define(QStringLiteral("mb.section.topStocks"),
        QStringLiteral("Top 5 value-investment stocks"),
        QStringLiteral(u"\u524d 5 \u540d\u6700\u5177\u4ef7\u503c\u6295\u8d44\u7684\u80a1\u7968"));
    localization.define(QStringLiteral("mb.action.tapForDetail"),
        QStringLiteral("Tap for detail"), QStringLiteral(u"\u5355\u51fb\u5207\u6362\u53f3\u4fa7\u8be6\u60c5"));
    localization.define(QStringLiteral("mb.action.update.5m"),
        QStringLiteral("Updates every 5 minutes"), QStringLiteral(u"5 \u5206\u949f\u66f4\u65b0\u4e00\u6b21"));
    localization.define(QStringLiteral("mb.label.valueScore"),
        QStringLiteral("Value score"), QStringLiteral(u"\u4ef7\u503c\u5206"));
    localization.define(QStringLiteral("mb.label.navOrPrice"),
        QStringLiteral("Latest NAV / price"), QStringLiteral(u"\u6700\u65b0\u51c0\u503c / \u80a1\u4ef7"));
    localization.define(QStringLiteral("mb.label.return1y"),
        QStringLiteral("1-year return"), QStringLiteral(u"\u8fd1 1 \u5e74\u6536\u76ca"));
    localization.define(QStringLiteral("mb.section.trend1y"),
        QStringLiteral("1-year trend"), QStringLiteral(u"\u8fd1 1 \u5e74\u8d70\u52bf\u66f2\u7ebf"));
    localization.define(QStringLiteral("mb.section.trend1y.subtitle"),
        QStringLiteral("24 samples; one-year trend overview"),
        QStringLiteral(u"24 \u4e2a\u91c7\u6837\u70b9\uff0c\u5c55\u793a\u8fd1\u4e00\u5e74\u8d8b\u52bf"));
    localization.define(QStringLiteral("mb.section.live1h"),
        QStringLiteral("Live 1-hour intraday curve"),
        QStringLiteral(u"\u6700\u8fd1 1 \u5c0f\u65f6\u5b9e\u65f6\u8dcc\u5e45\u66f2\u7ebf"));
    localization.define(QStringLiteral("mb.section.live1h.subtitle"),
        QStringLiteral("1 sample/sec; rolling 1 hour"),
        QStringLiteral(u"1 \u79d2 1 \u4e2a\u91c7\u6837\u70b9\uff0c\u6301\u7eed\u66f4\u65b0\u6700\u8fd1 1 \u5c0f\u65f6"));

    // Inline labels
    localization.define(QStringLiteral("mb.label.score"),
        QStringLiteral("Score"), QStringLiteral(u"\u8bc4\u5206"));
    localization.define(QStringLiteral("mb.label.liveSync"),
        QStringLiteral("Live sync"), QStringLiteral(u"\u5b9e\u65f6\u540c\u6b65"));
    localization.define(QStringLiteral("mb.action.add"),
        QStringLiteral("Add"), QStringLiteral(u"\u65b0\u589e"));
    localization.define(QStringLiteral("mb.label.indices"),
        QStringLiteral("Indices"), QStringLiteral(u"\u5927\u76d8\u884c\u60c5"));
    localization.define(QStringLiteral("mb.label.realQuotes"),
        QStringLiteral("Real quotes"), QStringLiteral(u"\u771f\u5b9e\u884c\u60c5"));
    localization.define(QStringLiteral("mb.label.usWatch"),
        QStringLiteral("US watchlist"), QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf"));
    localization.define(QStringLiteral("mb.action.backToList"),
        QStringLiteral("Back to list"), QStringLiteral(u"\u8fd4\u56de\u699c\u5355"));
    localization.define(QStringLiteral("mb.label.cache"),
        QStringLiteral("Local cache"), QStringLiteral(u"\u672c\u5730\u7f13\u5b58"));
    localization.define(QStringLiteral("mb.label.detailCard"),
        QStringLiteral("Detail card"), QStringLiteral(u"\u914d\u7f6e\u8be6\u7ec6\u5361"));
    localization.define(QStringLiteral("mb.label.fundCard"),
        QStringLiteral("Fund holdings card"), QStringLiteral(u"\u57fa\u91d1\u6301\u4ed3\u5361"));
    localization.define(QStringLiteral("mb.label.topHoldings"),
        QStringLiteral("Top holdings"), QStringLiteral(u"\u91cd\u4ed3\u80a1\u7968\u5217\u8868"));
    localization.define(QStringLiteral("mb.col.name"),
        QStringLiteral("Name"), QStringLiteral(u"\u540d\u79f0"));
    localization.define(QStringLiteral("mb.col.change"),
        QStringLiteral("Change"), QStringLiteral(u"\u6da8\u8dcc\u5e45"));
    localization.define(QStringLiteral("mb.col.weight"),
        QStringLiteral("Weight"), QStringLiteral(u"\u5360\u6bd4"));
    localization.define(QStringLiteral("mb.col.qoqDelta"),
        QStringLiteral("QoQ \u0394"), QStringLiteral(u"\u8f83\u4e0a\u5b63"));
    localization.define(QStringLiteral("mb.section.valueAnalysis"),
        QStringLiteral("Value-investment analysis"), QStringLiteral(u"\u6295\u8d44\u4ef7\u503c\u5206\u6790"));
    localization.define(QStringLiteral("mb.section.forecast6m"),
        QStringLiteral("6-month forecast"), QStringLiteral(u"\u672a\u6765 6 \u4e2a\u6708\u9884\u6d4b\u5206\u6790"));
    localization.define(QStringLiteral("mb.help.intro"),
        QStringLiteral("Tab 1 = institutions; Tab 2 = US watch. Double-click an institution row to open value rankings."),
        QStringLiteral(u"\u7b2c\u4e00\u9875\u4e3a\u56fd\u5185\u673a\u6784\u699c\uff0c\u7b2c\u4e8c\u9875\u4e3a\u7f8e\u80a1\u89c2\u5bdf\uff1b\u53cc\u51fb\u56fd\u5185\u673a\u6784\u699c\u4efb\u610f\u4e00\u884c\u8fdb\u5165\u4ef7\u503c\u6295\u8d44\u699c\u3002"));
    localization.define(QStringLiteral("mb.help.disclaimer"),
        QStringLiteral("Reference only: combines change, breadth, momentum, volume and support/resistance \u2014 not a trade commitment."),
        QStringLiteral(u"\u53c2\u8003\uff1a\u7ed3\u5408\u6da8\u8dcc\u5e45\u3001\u5e02\u573a\u5bbd\u5ea6\u3001\u52a8\u91cf\u3001\u91cf\u80fd\u4e0e\u652f\u6491\u538b\u529b\u4f30\u7b97\uff0c\u4e0d\u4f5c\u4e3a\u4ea4\u6613\u627f\u8bfa\u3002"));
    localization.define(QStringLiteral("mb.empty.fund"),
        QStringLiteral("No real quarterly fund-holdings data yet; once the source is wired up the table will show name, change, weight and QoQ delta."),
        QStringLiteral(u"\u6682\u65e0\u771f\u5b9e\u57fa\u91d1\u5b63\u62a5\u6301\u4ed3\u6570\u636e\uff1b\u63a5\u5165\u5b63\u62a5\u6e90\u540e\u5c55\u793a\u540d\u79f0\u3001\u6da8\u8dcc\u5e45\u3001\u6301\u4ed3\u5360\u6bd4\u3001\u8f83\u4e0a\u5b63\u5ea6\u53d8\u5316\u3002"));

    // Controller-emitted strings (used through trCn() reverse lookup in QML)
    auto cnDef = [&](const QString& key, const QString& en, const QString& zh) {
        localization.define(key, en, zh);
    };

    // Index / equity short names used as US watchlist labels
    cnDef("mb.cn.ndx", "Nasdaq 100", QStringLiteral(u"\u7eb3\u6307"));
    cnDef("mb.cn.nasdaq", "Nasdaq", QStringLiteral(u"\u7eb3\u65af\u8fbe\u514b"));
    cnDef("mb.cn.spx", "S&P 500", QStringLiteral(u"\u6807\u666e500"));
    cnDef("mb.cn.sp", "S&P", QStringLiteral(u"\u6807\u666e"));
    cnDef("mb.cn.dji", "Dow Jones", QStringLiteral(u"\u9053\u743c\u65af"));
    cnDef("mb.cn.djia", "Dow Industrials", QStringLiteral(u"\u9053\u743c\u65af\u5de5\u4e1a"));
    cnDef("mb.cn.aapl", "Apple", QStringLiteral(u"\u82f9\u679c"));
    cnDef("mb.cn.msft", "Microsoft", QStringLiteral(u"\u5fae\u8f6f"));
    cnDef("mb.cn.nvda", "NVIDIA", QStringLiteral(u"\u82f1\u4f1f\u8fbe"));
    cnDef("mb.cn.amzn", "Amazon", QStringLiteral(u"\u4e9a\u9a6c\u900a"));
    cnDef("mb.cn.meta", "Meta", QStringLiteral(u"\u8138\u4e66"));
    cnDef("mb.cn.metaverse", "Metaverse", QStringLiteral(u"\u5143\u5b87\u5b99"));
    cnDef("mb.cn.googl", "Google", QStringLiteral(u"\u8c37\u6b4c"));
    cnDef("mb.cn.tsla", "Tesla", QStringLiteral(u"\u7279\u65af\u62c9"));

    // Status / message strings
    cnDef("mb.cn.status.institutions",   "Institution rankings (built-in sample)", QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff08\u5185\u7f6e\u793a\u4f8b\uff09"));
    cnDef("mb.cn.status.value",          "Value-investment rankings (built-in sample)", QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff08\u5185\u7f6e\u793a\u4f8b\uff09"));
    cnDef("mb.cn.status.institutions.update", "Institution rankings: updating", QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff1a\u6b63\u5728\u66f4\u65b0"));
    cnDef("mb.cn.status.institutions.empty",  "Institution rankings: response empty, keeping existing data", QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff1a\u8fd4\u56de\u4e3a\u7a7a\uff0c\u4fdd\u7559\u73b0\u6709\u6570\u636e"));
    cnDef("mb.cn.status.institutions.fail",   "Institution rankings update failed: %1", QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\u66f4\u65b0\u5931\u8d25\uff1a%1"));
    cnDef("mb.cn.status.value.update",        "Value rankings: updating", QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u6b63\u5728\u66f4\u65b0"));
    cnDef("mb.cn.status.value.empty",         "Value rankings: response empty, keeping existing data", QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u8fd4\u56de\u4e3a\u7a7a\uff0c\u4fdd\u7559\u73b0\u6709\u6570\u636e"));
    cnDef("mb.cn.status.value.fail",          "Value rankings update failed: %1", QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\u66f4\u65b0\u5931\u8d25\uff1a%1"));
    cnDef("mb.cn.us.input.placeholder", "US watch: enter symbol or name", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u8bf7\u8f93\u5165\u80a1\u7968\u4ee3\u53f7\u6216\u540d\u79f0"));
    cnDef("mb.cn.us.input.exists.fmt",  "US watch: %1 already in watchlist", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a%1 \u5df2\u5728\u81ea\u9009\u5217\u8868"));
    cnDef("mb.cn.us.input.added.fmt",   "US watch: added %1", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u5df2\u65b0\u589e %1"));
    cnDef("mb.cn.us.empty.list", "US watch: no symbols selected", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u6682\u65e0\u81ea\u9009\u6807\u7684"));
    cnDef("mb.cn.us.empty.codes","US watch: no real quotes available", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u6682\u65e0\u53ef\u67e5\u8be2\u7684\u771f\u5b9e\u884c\u60c5\u4ee3\u7801"));
    cnDef("mb.cn.us.fetch.fail.fmt", "US watch: live-quote fetch failed %1", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u771f\u5b9e\u884c\u60c5\u83b7\u53d6\u5931\u8d25 %1"));
    cnDef("mb.cn.us.fetch.ok.fmt",   "US watch: live quotes %1 entries, %2", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u771f\u5b9e\u884c\u60c5 %1 \u6761\uff0c%2"));
    cnDef("mb.cn.us.fetch.empty",    "US watch: live-quote response empty", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u771f\u5b9e\u884c\u60c5\u8fd4\u56de\u4e3a\u7a7a"));
    cnDef("mb.cn.us.waiting", "US watch: waiting for symbols", QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u7b49\u5f85\u81ea\u9009\u6807\u7684"));
    cnDef("mb.cn.free.notconfigured", "Free data source: local provider not configured", QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u672a\u914d\u7f6e\u672c\u5730\u63d0\u4f9b\u5668"));
    cnDef("mb.cn.free.refreshing",    "Free data source: refreshing", QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u6b63\u5728\u5237\u65b0"));
    cnDef("mb.cn.free.startfail.fmt", "Free data source: start failed %1", QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u542f\u52a8\u5931\u8d25 %1"));
    cnDef("mb.cn.free.refreshfail.fmt", "Free data source: refresh failed %1", QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u5237\u65b0\u5931\u8d25 %1"));
    cnDef("mb.cn.free.parsefail.fmt",   "Free data source: parse failed %1", QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u89e3\u6790\u5931\u8d25 %1"));
    cnDef("mb.cn.free.refreshed.fmt",   "Free data source: refreshed sector flow %1 / disclosures %2%3", QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u5df2\u5237\u65b0 \u884c\u4e1a\u8d44\u91d1 %1 \u6761 / \u516c\u544a %2 \u6761%3"));
    cnDef("mb.cn.free.partialFail",     " / partial sources failed", QStringLiteral(u" / \u90e8\u5206\u6e90\u5931\u8d25"));

    // Engine / analysis errors
    cnDef("mb.cn.ai.unavailable",   "Analysis engine unavailable", QStringLiteral(u"\u5206\u6790\u5f15\u64ce\u6682\u4e0d\u53ef\u7528"));
    cnDef("mb.cn.ai.writeTaskFail", "Failed to write analysis task", QStringLiteral(u"\u5199\u5165\u5206\u6790\u4efb\u52a1\u5931\u8d25"));
    cnDef("mb.cn.ai.writeSchemaFail","Failed to write analysis constraint", QStringLiteral(u"\u5199\u5165\u5206\u6790\u7ea6\u675f\u5931\u8d25"));
    cnDef("mb.cn.ai.timeout",       "Analysis timed out", QStringLiteral(u"\u5206\u6790\u8d85\u65f6"));
    cnDef("mb.cn.ai.noResult",      "No analysis result", QStringLiteral(u"\u672a\u751f\u6210\u5206\u6790\u7ed3\u679c"));
    cnDef("mb.cn.ai.parseFail",     "Failed to parse analysis result", QStringLiteral(u"\u5206\u6790\u7ed3\u679c\u89e3\u6790\u5931\u8d25"));
    cnDef("mb.cn.ai.startFail.fmt", "Could not start analysis task: %1", QStringLiteral(u"\u65e0\u6cd5\u542f\u52a8\u5206\u6790\u4efb\u52a1\uff1a%1"));
    cnDef("mb.cn.ai.failed",        "Analysis failed", QStringLiteral(u"\u5206\u6790\u5931\u8d25"));

    // Column headers
    cnDef("mb.cn.col.rank", "Rank", QStringLiteral(u"\u6392\u540d"));
    cnDef("mb.cn.col.institution", "Institution", QStringLiteral(u"\u673a\u6784"));
    cnDef("mb.cn.col.entries", "Common entries", QStringLiteral(u"\u5e38\u7528\u5165\u53e3"));
    cnDef("mb.cn.col.advantage", "Core advantage", QStringLiteral(u"\u6838\u5fc3\u4f18\u52bf"));
    cnDef("mb.cn.col.audience", "Audience", QStringLiteral(u"\u9002\u5408\u4eba\u7fa4"));
    cnDef("mb.cn.col.score", "Composite score", QStringLiteral(u"\u7efc\u5408\u5206"));
    cnDef("mb.cn.col.name", "Name", QStringLiteral(u"\u540d\u79f0"));
    cnDef("mb.cn.col.code", "Symbol", QStringLiteral(u"\u4ee3\u7801"));
    cnDef("mb.cn.col.value", "Value score", QStringLiteral(u"\u4ef7\u503c\u5206"));
    cnDef("mb.cn.col.last", "Last", QStringLiteral(u"\u6700\u65b0\u4ef7"));
    cnDef("mb.cn.col.return1y", "1Y return", QStringLiteral(u"\u8fd11\u5e74"));
    cnDef("mb.cn.col.issuer", "Issuer / category", QStringLiteral(u"\u53d1\u884c / \u8d5b\u9053"));
    cnDef("mb.cn.col.market", "Market", QStringLiteral(u"\u5e02\u573a"));
    cnDef("mb.cn.col.change", "Change", QStringLiteral(u"\u6da8\u8dcc\u5e45"));
    cnDef("mb.cn.col.curve", "Live curve", QStringLiteral(u"\u5b9e\u65f6\u66f2\u7ebf"));
    cnDef("mb.cn.col.platform", "Platform / institution", QStringLiteral(u"\u5e73\u53f0 / \u673a\u6784"));
    cnDef("mb.cn.col.product", "Main product", QStringLiteral(u"\u4e3b\u6253\u54c1\u7c7b"));
    cnDef("mb.cn.col.risk", "Risk level", QStringLiteral(u"\u98ce\u9669\u7b49\u7ea7"));
    cnDef("mb.cn.col.liquidity", "Liquidity", QStringLiteral(u"\u6d41\u52a8\u6027"));
    cnDef("mb.cn.col.note", "Note", QStringLiteral(u"\u5907\u6ce8"));

    // Risk levels / liquidity / generic
    cnDef("mb.cn.risk.low", "Low", QStringLiteral(u"\u4f4e"));
    cnDef("mb.cn.risk.lowMid", "Low to mid", QStringLiteral(u"\u4f4e\u5230\u4e2d"));
    cnDef("mb.cn.risk.mid", "Mid", QStringLiteral(u"\u4e2d"));
    cnDef("mb.cn.risk.midHigh", "Mid to high", QStringLiteral(u"\u4e2d\u5230\u9ad8"));
    cnDef("mb.cn.risk.high", "High", QStringLiteral(u"\u9ad8"));

    // Card section labels
    cnDef("mb.cn.card.fundType", "Fund type", QStringLiteral(u"\u57fa\u91d1\u7c7b\u578b"));
    cnDef("mb.cn.card.manager", "Manager", QStringLiteral(u"\u7ba1\u7406\u4eba"));
    cnDef("mb.cn.card.nav", "Latest NAV", QStringLiteral(u"\u6700\u65b0\u51c0\u503c"));
    cnDef("mb.cn.card.return1y", "1-year return", QStringLiteral(u"\u8fd11\u5e74\u6536\u76ca"));
    cnDef("mb.cn.card.value", "Value score", QStringLiteral(u"\u4ef7\u503c\u5206"));
    cnDef("mb.cn.card.holdings", "Holdings data", QStringLiteral(u"\u6301\u4ed3\u6570\u636e"));
    cnDef("mb.cn.card.holdings.pending", "Pending quarterly source", QStringLiteral(u"\u5f85\u63a5\u5165\u5b63\u62a5\u6e90"));
    cnDef("mb.cn.card.equityWeight", "Equity weight", QStringLiteral(u"\u80a1\u7968\u4ed3\u4f4d"));
    cnDef("mb.cn.card.holdings.empty", "No real quarterly data", QStringLiteral(u"\u6682\u65e0\u771f\u5b9e\u5b63\u62a5\u6570\u636e"));
    cnDef("mb.cn.card.top10", "Top 10 holdings", QStringLiteral(u"\u524d\u5341\u6301\u4ed3"));
    cnDef("mb.cn.card.top10.waiting", "Awaiting fund-holdings API", QStringLiteral(u"\u7b49\u5f85\u57fa\u91d1\u6301\u4ed3\u63a5\u53e3"));
    cnDef("mb.cn.card.turnover", "Turnover", QStringLiteral(u"\u6362\u624b\u72b6\u6001"));
    cnDef("mb.cn.card.turnover.tbd", "To compute", QStringLiteral(u"\u5f85\u8ba1\u7b97"));
    cnDef("mb.cn.card.style", "Style exposure", QStringLiteral(u"\u98ce\u683c\u66b4\u9732"));

    // Tab/section labels
    cnDef("mb.cn.tab.institutions", "Institution rankings", QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c"));
    cnDef("mb.cn.tab.value", "Value rankings", QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c"));
    {
        auto localeSettings = stok::services::common::readDdsSettings(*configuration);
        localeSettings.topicName = configuration->getString("dds.localeTopic", "stok.ui.locale");
        localization.start(localeSettings, identity.name + "-locale-subscriber");
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("localizationController", &localization);
    engine.rootContext()->setContextProperty("marketBoardController", &controller);
    engine.rootContext()->setContextProperty("institutionRankingModel", controller.institutionModel());
    engine.rootContext()->setContextProperty("usMarketModel", controller.usMarketModel());
    engine.rootContext()->setContextProperty("fundRankingModel", controller.fundModel());
    engine.rootContext()->setContextProperty("stockRankingModel", controller.stockModel());
    engine.loadFromModule("StokMarketBoard", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }

    auto* window = qobject_cast<QWindow*>(engine.rootObjects().first());
    stok::services::common::HostedViewPublisher bridge(hostedViewSettings, menuId, menuTitle, menuDescription, menuGroup, telemetry.client());
    if (window)
    {
        window->winId();
        bridge.setHostWindow(window);
        bridge.start(QStringLiteral("market-board-hosted-view"));
    }

    controller.setRealtimeActive(true);
    controller.start();
    return app.exec();
}
