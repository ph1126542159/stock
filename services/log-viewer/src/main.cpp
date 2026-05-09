#include "LogViewerController.h"

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
    QCoreApplication::setApplicationName("stok-log-viewer");
    QCoreApplication::setOrganizationName("stok");

    QCommandLineParser parser;
    parser.setApplicationDescription("stok log viewer");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << "c" << "config", "Path to the log-viewer configuration file.", "file"));
    parser.process(app);

    const QString defaultConfigPath = QDir(QCoreApplication::applicationDirPath()).filePath("log-viewer.properties");
    const std::string configPath = parser.isSet("config") ? parser.value("config").toStdString() : defaultConfigPath.toStdString();
    auto configuration = stok::services::common::loadServiceConfiguration(configPath);
    const auto identity = stok::services::common::readServiceIdentity(*configuration, "stok.log-viewer");

    stok::services::common::ServiceTelemetry telemetry(identity, configuration, configPath);
    telemetry.install();

    auto logSettings = stok::services::common::readDdsSettings(*configuration);
    logSettings.topicName = configuration->getString("dds.logTopic", "stok.ui.logs");
    auto hostedViewSettings = stok::services::common::readDdsSettings(*configuration);
    hostedViewSettings.topicName = configuration->getString("dds.hostedViewTopic", "stok.ui.hosted-views");

    const QString menuId = QString::fromStdString(configuration->getString("ui.menu.id", "log-viewer"));
    QString menuTitle = QString::fromStdString(configuration->getString("ui.menu.title", ""));
    QString menuDescription = QString::fromStdString(configuration->getString("ui.menu.description", ""));
    const QString menuGroup = QString::fromStdString(configuration->getString("ui.menu.group", "main"));
    if (menuTitle.isEmpty()) menuTitle = QStringLiteral(u"日志");
    if (menuDescription.isEmpty()) menuDescription = QStringLiteral(u"查看各子进程的实时日志");

    stok::services::common::LocalizationClient localization;
    localization.define(QStringLiteral("window.title"),
        QStringLiteral("Logs"), QStringLiteral(u"日志"));
    localization.define(QStringLiteral("lv.placeholder.waitingForProcess"),
        QStringLiteral("Waiting for log process"), QStringLiteral(u"等待日志进程"));
    {
        auto localeSettings = stok::services::common::readDdsSettings(*configuration);
        localeSettings.topicName = configuration->getString("dds.localeTopic", "stok.ui.locale");
        localization.start(localeSettings, identity.name + "-locale-subscriber");
    }

    const QString mainProcessLogPath = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(configPath, configuration->getString("mainProcessLog.path", "")));
    LogViewerController controller(logSettings, mainProcessLogPath);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("localizationController", &localization);
    engine.rootContext()->setContextProperty("logViewerController", &controller);
    engine.loadFromModule("StokLogViewer", "Main");
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
        bridge.start(QStringLiteral("log-viewer-hosted-view"));
    }
    controller.start(QStringLiteral("log-viewer-subscriber"));
    return app.exec();
}
