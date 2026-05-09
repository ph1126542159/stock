#include "PortfolioController.h"

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
    QCoreApplication::setApplicationName("stok-portfolio-board");
    QCoreApplication::setOrganizationName("stok");

    QCommandLineParser parser;
    parser.setApplicationDescription("stok portfolio board");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << "c" << "config", "Path to the portfolio config file.", "file"));
    parser.process(app);

    const QString defaultConfigPath = QDir(QCoreApplication::applicationDirPath()).filePath("portfolio-board.properties");
    const std::string configPath = parser.isSet("config") ? parser.value("config").toStdString() : defaultConfigPath.toStdString();
    auto configuration = stok::services::common::loadServiceConfiguration(configPath);
    const auto identity = stok::services::common::readServiceIdentity(*configuration, "stok.portfolio-board");

    stok::services::common::ServiceTelemetry telemetry(identity, configuration, configPath);
    telemetry.install();
    PortfolioController controller(configuration, QString::fromStdString(configPath), telemetry);

    auto hostedViewSettings = stok::services::common::readDdsSettings(*configuration);
    hostedViewSettings.topicName = configuration->getString("dds.hostedViewTopic", "stok.ui.hosted-views");
    const QString menuId = QString::fromStdString(configuration->getString("ui.menu.id", "portfolio-board"));
    QString menuTitle = QString::fromStdString(configuration->getString("ui.menu.title", ""));
    QString menuDescription = QString::fromStdString(configuration->getString("ui.menu.description", ""));
    const QString menuGroup = QString::fromStdString(configuration->getString("ui.menu.group", "main"));
    if (menuTitle.isEmpty()) menuTitle = QStringLiteral(u"\u6301\u4ed3");
    if (menuDescription.isEmpty()) menuDescription = QStringLiteral(u"\u6301\u4ed3\u7ba1\u7406\u3001AI \u5206\u6790\u3001\u5b9e\u65f6\u66f2\u7ebf\u548c\u4ea4\u6613\u52a8\u4f5c\u5efa\u8bae");

    stok::services::common::LocalizationClient localization;
    localization.define(QStringLiteral("window.title"),
        QStringLiteral("Portfolio"), QStringLiteral(u"\u6301\u4ed3"));
    localization.define(QStringLiteral("pb.empty"),
        QStringLiteral("No data"), QStringLiteral(u"\u6682\u65e0\u6570\u636e"));
    localization.define(QStringLiteral("pb.action.add"),
        QStringLiteral("Add"), QStringLiteral(u"\u52a0\u4ed3"));
    localization.define(QStringLiteral("pb.action.sell"),
        QStringLiteral("Sell"), QStringLiteral(u"\u5356\u51fa"));
    localization.define(QStringLiteral("pb.action.exit"),
        QStringLiteral("Exit"), QStringLiteral(u"\u6e05\u4ed3"));
    localization.define(QStringLiteral("pb.action.rotate"),
        QStringLiteral("Rotate"), QStringLiteral(u"\u8f6c\u4ed3"));
    localization.define(QStringLiteral("pb.action.new"),
        QStringLiteral("Add holding"), QStringLiteral(u"\u65b0\u589e"));
    localization.define(QStringLiteral("pb.label.recommendation"),
        QStringLiteral("Recommendation"), QStringLiteral(u"\u64cd\u4f5c\u5efa\u8bae"));
    localization.define(QStringLiteral("pb.label.tech"),
        QStringLiteral("Tech score"), QStringLiteral(u"\u6280\u672f\u5206"));
    localization.define(QStringLiteral("pb.label.fundamental"),
        QStringLiteral("Fundamental"), QStringLiteral(u"\u57fa\u672c\u9762"));
    localization.define(QStringLiteral("pb.label.risk"),
        QStringLiteral("Risk score"), QStringLiteral(u"\u98ce\u9669\u5206"));
    localization.define(QStringLiteral("pb.label.drawdown"),
        QStringLiteral("Max drawdown"), QStringLiteral(u"\u6700\u5927\u56de\u64a4"));
    localization.define(QStringLiteral("pb.label.volatility"),
        QStringLiteral("Volatility"), QStringLiteral(u"\u6ce2\u52a8\u7387"));
    localization.define(QStringLiteral("pb.label.ai"),
        QStringLiteral("AI score"), QStringLiteral(u"AI \u5206"));
    localization.define(QStringLiteral("pb.label.aiScore"),
        QStringLiteral("AI rating"), QStringLiteral(u"AI \u8bc4\u5206"));
    localization.define(QStringLiteral("pb.label.symbol"),
        QStringLiteral("Symbol"), QStringLiteral(u"\u5f53\u524d\u6807\u7684"));
    localization.define(QStringLiteral("pb.label.last"),
        QStringLiteral("Last"), QStringLiteral(u"\u6700\u65b0\u4ef7"));
    localization.define(QStringLiteral("pb.label.recent1h"),
        QStringLiteral("Last 1h"), QStringLiteral(u"\u8fd1 1 \u5c0f\u65f6"));
    localization.define(QStringLiteral("pb.section.live"),
        QStringLiteral("Live portfolio"), QStringLiteral(u"\u5b9e\u65f6\u7ec4\u5408"));
    localization.define(QStringLiteral("pb.section.add"),
        QStringLiteral("Add holding"), QStringLiteral(u"\u65b0\u589e\u6301\u4ed3"));
    localization.define(QStringLiteral("pb.section.detail"),
        QStringLiteral("Holding detail"), QStringLiteral(u"\u6301\u4ed3\u8be6\u60c5"));
    localization.define(QStringLiteral("pb.section.live1hCurve"),
        QStringLiteral("Live 1h curve"), QStringLiteral(u"\u8fd1 1 \u5c0f\u65f6\u5b9e\u65f6\u66f2\u7ebf"));
    localization.define(QStringLiteral("pb.section.industryNext1m"),
        QStringLiteral("Industry trend next 1 month"), QStringLiteral(u"\u672a\u6765 1 \u4e2a\u6708\u884c\u4e1a\u8d8b\u52bf"));
    localization.define(QStringLiteral("pb.section.analysis"),
        QStringLiteral("Holding analysis"), QStringLiteral(u"\u6301\u80a1\u5206\u6790"));
    localization.define(QStringLiteral("pb.section.industry"),
        QStringLiteral("Industry view"), QStringLiteral(u"\u884c\u4e1a\u5224\u65ad"));
    localization.define(QStringLiteral("pb.section.fundamentals"),
        QStringLiteral("Earnings signal"), QStringLiteral(u"\u8d22\u62a5\u4fe1\u53f7"));
    localization.define(QStringLiteral("pb.section.market"),
        QStringLiteral("Market signal"), QStringLiteral(u"\u5e02\u573a\u4fe1\u53f7"));
    localization.define(QStringLiteral("pb.section.forecast"),
        QStringLiteral("Forecast & risk"), QStringLiteral(u"\u9884\u6d4b\u4e0e\u98ce\u63a7"));
    localization.define(QStringLiteral("pb.field.symbol"),
        QStringLiteral("Symbol"), QStringLiteral(u"\u4ee3\u7801"));
    localization.define(QStringLiteral("pb.placeholder.select"),
        QStringLiteral("Select a holding on the left"), QStringLiteral(u"\u8bf7\u9009\u62e9\u5de6\u4fa7\u6301\u4ed3"));
    localization.define(QStringLiteral("pb.placeholder.waiting"),
        QStringLiteral("Awaiting analysis"), QStringLiteral(u"\u7b49\u5f85\u5206\u6790"));
    localization.define(QStringLiteral("pb.placeholder.analysis"),
        QStringLiteral("Analysis output appears here once generated."), QStringLiteral(u"\u5206\u6790\u751f\u6210\u540e\u4f1a\u663e\u793a\u5728\u8fd9\u91cc\u3002"));
    localization.define(QStringLiteral("pb.placeholder.industry"),
        QStringLiteral("Awaiting industry trend update."), QStringLiteral(u"\u7b49\u5f85\u884c\u4e1a\u8d8b\u52bf\u66f4\u65b0\u3002"));
    localization.define(QStringLiteral("pb.prefix.suggest"),
        QStringLiteral("Suggestion: "), QStringLiteral(u"\u5efa\u8bae\uff1a"));

    // Controller-emitted strings. The QML wraps controller properties in
    // localizationController.trCn(...) which uses the Chinese phrase as a
    // reverse-lookup key into the language table.
    auto cnDef = [&](const QString& key, const QString& en, const QString& zh) {
        localization.define(key, en, zh);
    };
    cnDef("pb.cn.fund", "Fund", QStringLiteral(u"\u57fa\u91d1"));
    cnDef("pb.cn.stock", "Stock", QStringLiteral(u"\u80a1\u7968"));
    cnDef("pb.cn.us", "US stocks", QStringLiteral(u"\u7f8e\u80a1"));
    cnDef("pb.cn.gateBlocked", "Data-quality mock gate not passing", QStringLiteral(u"\u6570\u636e\u8d28\u91cf\u6a21\u62df\u95e8\u7981\u672a\u901a\u8fc7"));
    cnDef("pb.cn.riskBelow", "Risk score %1 below %2", QStringLiteral(u"\u98ce\u63a7\u5206 %1 \u4f4e\u4e8e %2"));
    cnDef("pb.cn.gateReason",
        "Gate reason: %1. Until data quality / risk pass, original add/sell/exit strong actions degrade to follow-up or rotate; no new exposure.",
        QStringLiteral(u"\u95e8\u7981\u539f\u56e0\uff1a%1\u3002\u6570\u636e\u8d28\u91cf/\u98ce\u63a7\u8fbe\u6807\u524d\uff0c\u539f\u6709\u52a0\u4ed3/\u5356\u51fa/\u6e05\u4ed3\u7b49\u5f3a\u52a8\u4f5c\u964d\u7ea7\u4e3a\u8ddf\u8fdb\u6216\u8f6c\u4ed3\uff0c\u6682\u4e0d\u589e\u52a0\u66b4\u9732\u3002"));
    cnDef("pb.cn.plan.add",
        "Add in tranches after pullback or volume confirmation; cap any single name at 12%-15%.",
        QStringLiteral(u"\u56de\u8c03\u6216\u91cf\u80fd\u786e\u8ba4\u540e\u5206\u6279\u52a0\u4ed3\uff0c\u5355\u6807\u7684\u4ed3\u4f4d\u4e0d\u8d85\u8fc7 12%-15%\u3002"));
    cnDef("pb.cn.plan.exit",
        "Risk score down to %1; protect principal first; cut to 0%-2% watch position in tranches.",
        QStringLiteral(u"\u98ce\u9669\u5206\u964d\u81f3 %1\uff0c\u5148\u4fdd\u62a4\u672c\u91d1\uff0c\u5206\u6279\u964d\u4e3a 0%-2% \u89c2\u5bdf\u4ed3\u3002"));
    cnDef("pb.cn.plan.sell",
        "Tech score %1 weak; cut 30%-50% first; reassess once trend repairs.",
        QStringLiteral(u"\u6280\u672f\u5206 %1 \u504f\u5f31\uff0c\u5148\u51cf\u4ed3 30%-50%\uff0c\u7b49\u8d8b\u52bf\u4fee\u590d\u518d\u8bc4\u4f30\u3002"));
    cnDef("pb.cn.plan.rotate",
        "Reward/risk not attractive; rotate to lower-valuation, lower-correlation or higher-win-rate names.",
        QStringLiteral(u"\u6536\u76ca/\u98ce\u9669\u6bd4\u4e0d\u5360\u4f18\uff0c\u8f6c\u5411\u4f4e\u4f30\u503c\u3001\u4f4e\u76f8\u5173\u6216\u66f4\u9ad8\u80dc\u7387\u6807\u7684\u3002"));
    cnDef("pb.cn.plan.follow",
        "Keep tracking earnings, industry trend and price strength; hold current position.",
        QStringLiteral(u"\u7ee7\u7eed\u8ddf\u8e2a\u8d22\u62a5\u3001\u884c\u4e1a\u8d8b\u52bf\u548c\u4ef7\u683c\u5f3a\u5f31\uff0c\u4fdd\u6301\u73b0\u6709\u4ed3\u4f4d\u3002"));
    cnDef("pb.cn.stop",
        "Hard stop %1%; if break with weakening earnings/industry, trigger exit review.",
        QStringLiteral(u"\u786c\u6b62\u635f %1%\uff1b\u82e5\u8dcc\u7834\u540c\u65f6\u8d22\u62a5/\u884c\u4e1a\u8d8b\u52bf\u8f6c\u5f31\uff0c\u89e6\u53d1\u6e05\u4ed3\u590d\u6838\u3002"));
    cnDef("pb.cn.takeProfit",
        "Take profit in tranches at %1%; if valuation premium expands or risk score drops below 55, cut to lock in.",
        QStringLiteral(u"\u5206\u6279\u6b62\u76c8 %1%\uff1b\u82e5\u4f30\u503c\u6ea2\u4ef7\u6269\u5927\u6216\u98ce\u9669\u5206\u8dcc\u7834 55\uff0c\u964d\u4ed3\u9501\u5b9a\u6536\u76ca\u3002"));
    cnDef("pb.cn.fund.strong",
        "Strong earnings quality; watch cash flow, gross margin and next-quarter guidance to confirm support for valuation.",
        QStringLiteral(u"\u8d22\u62a5\u8d28\u91cf\u8f83\u5f3a\uff0c\u5173\u6ce8\u73b0\u91d1\u6d41\u3001\u6bdb\u5229\u7387\u548c\u4e0b\u671f\u6307\u5f15\u662f\u5426\u7ee7\u7eed\u652f\u6491\u4f30\u503c\u3002"));
    cnDef("pb.cn.fund.weak",
        "Earnings signal not strong enough; need to verify profit growth, cash flow and debt pressure.",
        QStringLiteral(u"\u8d22\u62a5\u4fe1\u53f7\u5c1a\u4e0d\u5145\u5206\uff0c\u9700\u8865\u5145\u5229\u6da6\u589e\u901f\u3001\u73b0\u91d1\u6d41\u548c\u8d1f\u503a\u538b\u529b\u9a8c\u8bc1\u3002"));
    cnDef("pb.cn.market.strong",
        "Short-term trend favorable; use volume and industry strength as secondary confirmation.",
        QStringLiteral(u"\u77ed\u7ebf\u8d8b\u52bf\u76f8\u5bf9\u5360\u4f18\uff0c\u53ef\u7528\u6210\u4ea4\u91cf\u548c\u884c\u4e1a\u5f3a\u5f31\u505a\u4e8c\u6b21\u786e\u8ba4\u3002"));
    cnDef("pb.cn.market.weak",
        "Short-term momentum weak; avoid chasing; wait for stop, volume or return above key MA.",
        QStringLiteral(u"\u77ed\u7ebf\u52a8\u80fd\u504f\u5f31\uff0c\u907f\u514d\u8ffd\u9ad8\uff0c\u7b49\u5f85\u6b62\u8dcc\u3001\u653e\u91cf\u6216\u56de\u5230\u5173\u952e\u5747\u7ebf\u4e0a\u65b9\u3002"));
    cnDef("pb.cn.forecast.steady",
        "Forecast steady; baseline scenario continues to hold; stress scenario risks are drawdown and correlation.",
        QStringLiteral(u"\u60c5\u666f\u9884\u6d4b\u504f\u7a33\uff0c\u57fa\u51c6\u60c5\u666f\u7ee7\u7eed\u6301\u6709\uff1b\u538b\u529b\u60c5\u666f\u4ee5\u56de\u64a4\u548c\u76f8\u5173\u6027\u4e3a\u4e3b\u8981\u98ce\u9669\u3002"));
    cnDef("pb.cn.forecast.stress",
        "Stress scenario weighting up; suggest reducing position or rotating to low-vol, low-correlation names.",
        QStringLiteral(u"\u538b\u529b\u60c5\u666f\u6743\u91cd\u5347\u9ad8\uff0c\u5efa\u8bae\u964d\u4ed3\u6216\u8f6c\u5165\u4f4e\u6ce2\u52a8\u3001\u4f4e\u76f8\u5173\u6807\u7684\u3002"));
    cnDef("pb.cn.input.placeholder",
        "Enter fund or stock name / symbol",
        QStringLiteral(u"\u8bf7\u8f93\u5165\u57fa\u91d1\u6216\u80a1\u7968\u540d\u79f0/\u4ee3\u7801"));
    cnDef("pb.cn.input.unparsable", "Cannot parse input", QStringLiteral(u"\u65e0\u6cd5\u89e3\u6790\u8f93\u5165"));
    cnDef("pb.cn.alreadyHeld", "%1 already in holdings", QStringLiteral(u"%1 \u5df2\u5728\u6301\u4ed3\u5217\u8868"));
    cnDef("pb.cn.waitingHoldings", "Waiting for holdings data", QStringLiteral(u"\u7b49\u5f85\u6301\u4ed3\u6570\u636e"));
    cnDef("pb.cn.waitingAi", "Awaiting AI analysis", QStringLiteral(u"\u7b49\u5f85 AI \u5206\u6790"));
    cnDef("pb.cn.preparing.analysis", "Preparing holding analysis", QStringLiteral(u"\u6b63\u5728\u51c6\u5907\u6301\u4ed3\u5206\u6790"));
    cnDef("pb.cn.preparing.industry", "Preparing next-month industry trend", QStringLiteral(u"\u6b63\u5728\u51c6\u5907\u4e0b\u4e00\u4e2a\u6708\u884c\u4e1a\u8d70\u52bf"));
    cnDef("pb.cn.added.fmt", "Added %1; generating AI analysis", QStringLiteral(u"\u5df2\u65b0\u589e %1\uff0c\u6b63\u5728\u751f\u6210 AI \u5206\u6790"));
    cnDef("pb.cn.loaded.fmt", "Loaded %1 instruments; quotes and risk metrics updating", QStringLiteral(u"\u5df2\u8f7d\u5165 %1 \u4e2a\u6295\u8d44\u6807\u7684\uff0c\u884c\u60c5\u4e0e\u98ce\u9669\u6307\u6807\u6b63\u5728\u66f4\u65b0"));
    cnDef("pb.cn.proGroup", "Professional portfolio", QStringLiteral(u"\u4e13\u4e1a\u6295\u8d44\u7ec4\u5408"));
    cnDef("pb.cn.aapl.suggest", "Pullback, accumulate in tranches", QStringLiteral(u"\u56de\u8c03\u5206\u6279\u5438\u7eb3"));
    cnDef("pb.cn.aapl.analysis",
        "Stable services revenue, strong cash flow quality; watch hardware refresh cycle and AI on-device rollout.",
        QStringLiteral(u"\u670d\u52a1\u6536\u5165\u7a33\u5b9a\uff0c\u73b0\u91d1\u6d41\u8d28\u91cf\u5f3a\uff1b\u9700\u8ddf\u8e2a\u786c\u4ef6\u6362\u673a\u5468\u671f\u4e0e AI \u7aef\u4fa7\u843d\u5730\u901f\u5ea6\u3002"));
    cnDef("pb.cn.aapl.outlook",
        "Consumer-electronics leader has solid earnings resilience; further valuation upside needs a new growth narrative.",
        QStringLiteral(u"\u6d88\u8d39\u7535\u5b50\u9f99\u5934\u76c8\u5229\u97e7\u6027\u8f83\u597d\uff0c\u4f30\u503c\u4e0a\u884c\u9700\u65b0\u589e\u957f\u53d9\u4e8b\u9a71\u52a8\u3002"));
    cnDef("pb.cn.nvda", "NVIDIA", QStringLiteral(u"\u82f1\u4f1f\u8fbe"));
    cnDef("pb.cn.nvda.suggest", "Hold; cap single-name limit", QStringLiteral(u"\u6301\u6709\uff0c\u63a7\u5236\u5355\u7968\u4e0a\u9650"));
    cnDef("pb.cn.nvda.analysis",
        "AI compute demand still strong; gross margin and inventory turn are the core watch items.",
        QStringLiteral(u"AI \u7b97\u529b\u9700\u6c42\u4ecd\u5f3a\uff0c\u6bdb\u5229\u7387\u548c\u5e93\u5b58\u5468\u8f6c\u662f\u6838\u5fc3\u89c2\u5bdf\u70b9\u3002"));
    cnDef("pb.cn.nvda.outlook",
        "Semi cycle hot, but watch for drawdown after consensus alignment.",
        QStringLiteral(u"\u534a\u5bfc\u4f53\u666f\u6c14\u5ea6\u9ad8\uff0c\u4f46\u9700\u9632\u6b62\u76c8\u5229\u9884\u671f\u8fc7\u5ea6\u4e00\u81f4\u540e\u7684\u56de\u64a4\u3002"));
    cnDef("pb.cn.tsla", "Tesla", QStringLiteral(u"\u7279\u65af\u62c9"));
    cnDef("pb.cn.tsla.suggest", "Wait for margin recovery signal", QStringLiteral(u"\u7b49\u5f85\u6bdb\u5229\u4fee\u590d\u4fe1\u53f7"));
    cnDef("pb.cn.tsla.analysis",
        "Delivery and per-vehicle margin under pressure; robotics and FSD are long-dated options.",
        QStringLiteral(u"\u4ea4\u4ed8\u548c\u5355\u8f66\u76c8\u5229\u627f\u538b\uff0c\u673a\u5668\u4eba\u4e0e\u81ea\u52a8\u9a7e\u9a76\u662f\u957f\u671f\u671f\u6743\u3002"));
    cnDef("pb.cn.tsla.outlook",
        "EV price war not over; near term better suited to risk-controlled positioning.",
        QStringLiteral(u"\u65b0\u80fd\u6e90\u8f66\u4ef7\u683c\u7ade\u4e89\u672a\u5b8c\u5168\u7ed3\u675f\uff0c\u77ed\u671f\u66f4\u9002\u5408\u7528\u98ce\u63a7\u4ed3\u4f4d\u53c2\u4e0e\u3002"));
    cnDef("pb.cn.qqq", "Nasdaq-100 ETF", QStringLiteral(u"\u7eb3\u6307 100 ETF"));
    cnDef("pb.cn.qqq.suggest", "Core-satellite position", QStringLiteral(u"\u6838\u5fc3\u536b\u661f\u4ed3\u4f4d"));
    cnDef("pb.cn.qqq.analysis",
        "Tech-growth exposure is diversified; suitable to carry the portfolio's US Beta.",
        QStringLiteral(u"\u79d1\u6280\u6210\u957f\u66b4\u9732\u5206\u6563\uff0c\u9002\u5408\u627f\u63a5\u7ec4\u5408\u7684\u7f8e\u80a1 Beta\u3002"));
    cnDef("pb.cn.qqq.outlook",
        "Mega-cap tech remains the earnings-quality main thread; rate expectations magnify volatility.",
        QStringLiteral(u"\u5927\u578b\u79d1\u6280\u4ecd\u662f\u76c8\u5229\u8d28\u91cf\u4e3b\u7ebf\uff0c\u5229\u7387\u9884\u671f\u53d8\u5316\u4f1a\u653e\u5927\u6ce2\u52a8\u3002"));
    cnDef("pb.cn.csi300", "CSI 300 ETF", QStringLiteral(u"\u6caa\u6df1 300 ETF"));
    cnDef("pb.cn.csi300.suggest", "Build long position in tranches", QStringLiteral(u"\u5de6\u4fa7\u5206\u6279\u5e03\u5c40"));
    cnDef("pb.cn.csi300.analysis",
        "Valuation in low historical band; dividend yield and ROE stability still need ongoing verification.",
        QStringLiteral(u"\u4f30\u503c\u5904\u4e8e\u5386\u53f2\u8f83\u4f4e\u533a\u95f4\uff0c\u5206\u7ea2\u7387\u548c ROE \u7a33\u5b9a\u6027\u9700\u6301\u7eed\u9a8c\u8bc1\u3002"));
    cnDef("pb.cn.csi300.outlook",
        "Broad index works as low-valuation core; bounce strength depends on earnings expectation repair.",
        QStringLiteral(u"\u5bbd\u57fa\u9002\u5408\u4f5c\u4e3a\u4f4e\u4f30\u503c\u5e95\u4ed3\uff0c\u53cd\u5f39\u5f3a\u5ea6\u53d6\u51b3\u4e8e\u76c8\u5229\u9884\u671f\u4fee\u590d\u3002"));
    cnDef("pb.cn.analyzing.fmt", "Analyzing %1", QStringLiteral(u"\u6b63\u5728\u5206\u6790 %1"));
    cnDef("pb.cn.analyzed.fmt", "%1 AI analysis updated", QStringLiteral(u"%1 \u7684 AI \u5206\u6790\u5df2\u66f4\u65b0"));
    cnDef("pb.cn.aiFailed.fmt", "AI analysis failed: %1", QStringLiteral(u"AI \u5206\u6790\u5931\u8d25\uff1a%1"));
    cnDef("pb.cn.aiUnavailable", "Analysis engine unavailable", QStringLiteral(u"\u5206\u6790\u5f15\u64ce\u6682\u4e0d\u53ef\u7528"));
    cnDef("pb.cn.aiCantWriteTask", "Could not write Codex task file", QStringLiteral(u"\u65e0\u6cd5\u5199\u5165 Codex \u4efb\u52a1\u6587\u4ef6"));
    cnDef("pb.cn.aiTimeout", "Analysis timeout", QStringLiteral(u"\u5206\u6790\u8d85\u65f6"));
    cnDef("pb.cn.aiNoResult", "No analysis result", QStringLiteral(u"\u672a\u751f\u6210\u5206\u6790\u7ed3\u679c"));
    cnDef("pb.cn.aiParseFail", "Failed to parse analysis result", QStringLiteral(u"\u5206\u6790\u7ed3\u679c\u89e3\u6790\u5931\u8d25"));
    cnDef("pb.cn.aiFailed", "Analysis failed", QStringLiteral(u"\u5206\u6790\u5931\u8d25"));
    cnDef("pb.cn.demoIns.ant", "Ant Wealth", QStringLiteral(u"\u8682\u8681\u8d22\u5bcc"));
    cnDef("pb.cn.demoIns.wechat", "WeChat Wealth", QStringLiteral(u"\u5fae\u4fe1\u7406\u8d22\u901a"));
    cnDef("pb.cn.demoIns.cmb", "China Merchants Bank", QStringLiteral(u"\u62db\u5546\u94f6\u884c"));
    cnDef("pb.cn.demoIns.tiantian", "Tiantian Funds", QStringLiteral(u"\u5929\u5929\u57fa\u91d1"));
    cnDef("pb.cn.demoIns.jd", "JD Finance", QStringLiteral(u"\u4eac\u4e1c\u91d1\u878d"));
    {
        auto localeSettings = stok::services::common::readDdsSettings(*configuration);
        localeSettings.topicName = configuration->getString("dds.localeTopic", "stok.ui.locale");
        localization.start(localeSettings, identity.name + "-locale-subscriber");
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("localizationController", &localization);
    engine.rootContext()->setContextProperty("portfolioController", &controller);
    engine.loadFromModule("StokPortfolioBoard", "Main");
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
        bridge.start(QStringLiteral("portfolio-board-hosted-view"));
    }

    controller.setRealtimeActive(true);
    controller.start();
    return app.exec();
}
