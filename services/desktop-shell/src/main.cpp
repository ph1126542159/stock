#include "DesktopShellController.h"

#include "QQmlApplicationEngine"
#include "QQmlContext"
#include "QCommandLineOption"
#include "QCommandLineParser"
#include "QDir"
#include "QGuiApplication"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TelemetryBootstrap.h"

int main(int argc, char* argv[])
{
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

    DesktopShellController controller(identity, ddsSettings, configuration, telemetry);
    controller.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("shellController", &controller);
    engine.loadFromModule("StokDesktopShell", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }

    return app.exec();
}
