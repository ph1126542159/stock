#include "DesktopShellController.h"
#include "LocalizationController.h"

#include "QCommandLineOption"
#include "QCommandLineParser"
#include "QDir"
#include "QGuiApplication"
#include "QQmlApplicationEngine"
#include "QQmlContext"
#include "QQuickWindow"
#include "QTimer"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TelemetryBootstrap.h"

int main(int argc, char* argv[])
{
    // The shell embeds foreign HWNDs as WS_CHILD windows of its own. The
    // default D3D11/RHI scene graph paints over the entire client area
    // every frame and ignores WS_CLIPCHILDREN, which hides the embedded
    // child. The software scene graph honors WS_CLIPCHILDREN, so the child
    // becomes visible inside the contentFrame region.
    QQuickWindow::setSceneGraphBackend(QStringLiteral("software"));

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("stok");
    QCoreApplication::setOrganizationName("stok");

    QCommandLineParser parser;
    parser.setApplicationDescription("stok desktop shell");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        QStringList() << "c" << "config",
        "Path to the desktop shell configuration file.",
        "file"));
    parser.process(app);

    const QString defaultConfigPath =
        QDir(QCoreApplication::applicationDirPath()).filePath("desktop-shell.properties");
    const std::string configPath = parser.isSet("config")
        ? parser.value("config").toStdString()
        : defaultConfigPath.toStdString();

    auto configuration = stok::services::common::loadServiceConfiguration(configPath);
    const auto identity = stok::services::common::readServiceIdentity(
        *configuration,
        "stok.desktop-shell");
    const auto ddsSettings = stok::services::common::readDdsSettings(*configuration);

    stok::services::common::ServiceTelemetry telemetry(identity, configuration, configPath);
    telemetry.install();
    telemetry.recordStartup("qml-shell", {{"dds.topic", ddsSettings.topicName}});

    LocalizationController localization;
    {
        auto localeSettings = stok::services::common::readDdsSettings(*configuration);
        localeSettings.topicName = configuration->getString("dds.localeTopic", "stok.ui.locale");
        localization.startBroadcast(localeSettings, identity.name + "-locale-publisher");
    }

    DesktopShellController controller(
        identity,
        ddsSettings,
        QString::fromStdString(configPath),
        configuration,
        telemetry,
        localization);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("shellController", &controller);
    engine.rootContext()->setContextProperty("localizationController", &localization);
    engine.loadFromModule("StokDesktopShell", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }

    QTimer::singleShot(0, &controller, &DesktopShellController::start);
    return app.exec();
}
