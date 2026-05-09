#include "ConfigCenterController.h"

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
    QCoreApplication::setApplicationName("stok-config-center");
    QCoreApplication::setOrganizationName("stok");

    QCommandLineParser parser;
    parser.setApplicationDescription("stok config center");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << "c" << "config", "Path to the config file.", "file"));
    parser.process(app);

    const QString defaultConfigPath = QDir(QCoreApplication::applicationDirPath()).filePath("config-center.properties");
    const std::string configPath = parser.isSet("config") ? parser.value("config").toStdString() : defaultConfigPath.toStdString();
    auto configuration = stok::services::common::loadServiceConfiguration(configPath);
    const auto identity = stok::services::common::readServiceIdentity(*configuration, "stok.config-center");

    stok::services::common::ServiceTelemetry telemetry(identity, configuration, configPath);
    telemetry.install();

    stok::services::common::LocalizationClient localization;
    localization.define(QStringLiteral("window.title"),
        QStringLiteral("Config"),
        QStringLiteral(u"配置中心"));
    localization.define(QStringLiteral("window.description"),
        QStringLiteral("Global parameters, process keep-alive, page and strategy settings"),
        QStringLiteral(u"全局参数、进程保活、页面与策略配置"));
    localization.define(QStringLiteral("action.refresh"),
        QStringLiteral("Refresh"), QStringLiteral(u"刷新"));
    localization.define(QStringLiteral("action.send"),
        QStringLiteral("Send"), QStringLiteral(u"下发"));
    localization.define(QStringLiteral("cc.section.global"),
        QStringLiteral("Global parameters"), QStringLiteral(u"全局参数"));
    localization.define(QStringLiteral("cc.section.process"),
        QStringLiteral("Process keep-alive"), QStringLiteral(u"进程保活"));
    localization.define(QStringLiteral("cc.section.pages"),
        QStringLiteral("Per-page settings"), QStringLiteral(u"页面配置"));
    localization.define(QStringLiteral("cc.section.publish"),
        QStringLiteral("Send config"), QStringLiteral(u"下发配置"));
    localization.define(QStringLiteral("cc.field.target"),
        QStringLiteral("Target"), QStringLiteral(u"目标"));
    localization.define(QStringLiteral("cc.field.key"),
        QStringLiteral("Key"), QStringLiteral(u"属性"));
    localization.define(QStringLiteral("cc.field.value"),
        QStringLiteral("Value"), QStringLiteral(u"值"));
    localization.define(QStringLiteral("cc.label.apply"),
        QStringLiteral("Apply"), QStringLiteral(u"应用"));
    localization.define(QStringLiteral("cc.label.intro"),
        QStringLiteral("Expand the config tree per service; changes sync to the target page immediately."),
        QStringLiteral(u"按服务展开配置树，修改数值后立即同步到目标页面。"));
    localization.define(QStringLiteral("cc.section.process"),
        QStringLiteral("Process keep-alive"), QStringLiteral(u"进程保活"));
    localization.define(QStringLiteral("cc.section.process.subtitle"),
        QStringLiteral("macchina.exe child process heartbeat; immediate restart by default"),
        QStringLiteral(u"macchina.exe 托管子进程心跳；除主界面外默认立即拉起"));
    localization.define(QStringLiteral("cc.field.keepalive.enabled"),
        QStringLiteral("Global keep-alive switch"), QStringLiteral(u"全局保活开关"));
    localization.define(QStringLiteral("cc.field.keepalive.delayMs"),
        QStringLiteral("Global restart delay (ms)"), QStringLiteral(u"全局重启延迟毫秒"));
    localization.define(QStringLiteral("cc.field.keepalive.skipShell"),
        QStringLiteral("Don't auto-restart shell"), QStringLiteral(u"主界面不自动拉起"));
    localization.define(QStringLiteral("cc.section.market"),
        QStringLiteral("Market"), QStringLiteral(u"行情榜"));
    localization.define(QStringLiteral("cc.section.market.subtitle"),
        QStringLiteral("Update intervals for institutions, US watch and value boards"),
        QStringLiteral(u"机构榜、美股观察、价值投资榜的更新时间"));
    localization.define(QStringLiteral("cc.field.market.institutions"),
        QStringLiteral("Institution-board refresh"), QStringLiteral(u"机构榜刷新间隔"));
    localization.define(QStringLiteral("cc.field.market.value"),
        QStringLiteral("Value-board refresh"), QStringLiteral(u"价值投资榜刷新间隔"));
    localization.define(QStringLiteral("cc.field.market.us"),
        QStringLiteral("US-watch refresh"), QStringLiteral(u"美股观察刷新间隔"));
    localization.define(QStringLiteral("cc.section.portfolio"),
        QStringLiteral("Portfolio"), QStringLiteral(u"持仓"));
    localization.define(QStringLiteral("cc.section.portfolio.subtitle"),
        QStringLiteral("Live price and AI-analysis update intervals"),
        QStringLiteral(u"实时价格与 AI 分析更新间隔"));
    localization.define(QStringLiteral("cc.field.portfolio.curves"),
        QStringLiteral("Portfolio live-curve refresh"), QStringLiteral(u"持仓实时曲线刷新间隔"));
    localization.define(QStringLiteral("cc.field.portfolio.ai"),
        QStringLiteral("Portfolio AI analysis refresh"), QStringLiteral(u"持仓 AI 分析刷新间隔"));
    {
        auto localeSettings = stok::services::common::readDdsSettings(*configuration);
        localeSettings.topicName = configuration->getString("dds.localeTopic", "stok.ui.locale");
        localization.start(localeSettings, identity.name + "-locale-subscriber");
    }

    auto busSettings = stok::services::common::readDdsSettings(*configuration);
    busSettings.topicName = configuration->getString("dds.configTopic", "stok.ui.config");
    ConfigCenterController controller(busSettings, &localization);

    auto hostedViewSettings = stok::services::common::readDdsSettings(*configuration);
    hostedViewSettings.topicName = configuration->getString("dds.hostedViewTopic", "stok.ui.hosted-views");
    const QString menuId = QString::fromStdString(configuration->getString("ui.menu.id", "config-center"));
    QString menuTitle = QString::fromStdString(configuration->getString("ui.menu.title", ""));
    QString menuDescription = QString::fromStdString(configuration->getString("ui.menu.description", ""));
    const QString menuGroup = QString::fromStdString(configuration->getString("ui.menu.group", "main"));
    if (menuTitle.isEmpty()) menuTitle = QStringLiteral(u"配置中心");
    if (menuDescription.isEmpty()) menuDescription = QStringLiteral(u"全局参数、进程保活、页面与策略配置");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("localizationController", &localization);
    engine.rootContext()->setContextProperty("configController", &controller);
    engine.loadFromModule("StokConfigCenter", "Main");
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
        bridge.start(QStringLiteral("config-center-hosted-view"));
    }
    controller.start(QStringLiteral("config-center-publisher"));
    return app.exec();
}
