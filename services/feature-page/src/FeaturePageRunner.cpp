#include "stok/services/feature_page/FeaturePageRunner.h"

#include "QCommandLineOption"
#include "QCommandLineParser"
#include "QCoreApplication"
#include "QDateTime"
#include "QDir"
#include "QFile"
#include "QGuiApplication"
#include "QQmlApplicationEngine"
#include "QQmlContext"
#include "QTextStream"
#include "QWindow"
#include "stok/services/common/HostedViewPublisher.h"
#include "stok/services/common/LocalizationClient.h"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TelemetryBootstrap.h"

namespace {

QString bootstrap_log_path(const QString& serviceId)
{
    const QString logDirectory = QDir(QDir::currentPath()).filePath("logs");
    QDir().mkpath(logDirectory);
    return QDir(logDirectory).filePath(QStringLiteral("%1-bootstrap.log").arg(serviceId));
}

void append_bootstrap_log(const QString& serviceId, const QString& message)
{
    QFile file(bootstrap_log_path(serviceId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << " "
           << message
           << Qt::endl;
}

} // namespace

namespace stok::services::feature_page {

int run(int argc, char* argv[], const FeaturePageSpec& spec)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromUtf8(spec.applicationName));
    QCoreApplication::setOrganizationName("stok");

    QCommandLineParser parser;
    parser.setApplicationDescription(QString::fromUtf8(spec.description));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        QStringList() << "c" << "config",
        "Path to the service configuration file.",
        "file"));
    parser.process(app);

    const QString defaultConfigPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromUtf8(spec.defaultConfigFile));
    const std::string configPath = parser.isSet("config")
        ? parser.value("config").toStdString()
        : defaultConfigPath.toStdString();

    auto configuration = stok::services::common::loadServiceConfiguration(configPath);
    const auto identity = stok::services::common::readServiceIdentity(
        *configuration,
        spec.defaultServiceName);

    const QString fallbackMenuId = QString::fromUtf8(spec.defaultMenuId);
    append_bootstrap_log(fallbackMenuId, QStringLiteral("QML feature page configuration loaded"));

    stok::services::common::ServiceTelemetry telemetry(identity, configuration, configPath);
    telemetry.install();

    auto hostedViewSettings = stok::services::common::readDdsSettings(*configuration);
    hostedViewSettings.topicName = configuration->getString("dds.hostedViewTopic", "stok.ui.hosted-views");

    QString menuId = QString::fromStdString(configuration->getString("ui.menu.id", spec.defaultMenuId));
    QString menuTitle = QString::fromStdString(configuration->getString("ui.menu.title", ""));
    QString menuDescription = QString::fromStdString(configuration->getString("ui.menu.description", ""));
    QString menuGroup = QString::fromStdString(configuration->getString("ui.menu.group", "main"));
    if (menuTitle.isEmpty())
    {
        menuTitle = QString::fromUtf8(spec.fallbackTitle);
    }
    if (menuDescription.isEmpty())
    {
        menuDescription = QString::fromUtf8(spec.fallbackDescription);
    }

    telemetry.recordStartup(
        menuId.toStdString(),
        {
            {"dds.hostedViewTopic", hostedViewSettings.topicName},
            {"ui.menu.id", menuId.toStdString()},
            {"ui.framework", "QML"}
        });

    auto* localization = new stok::services::common::LocalizationClient(&app);
    localization->define(QStringLiteral("window.title"),
        QString::fromUtf8(spec.fallbackTitle),
        menuTitle);
    localization->define(QStringLiteral("window.description"),
        QString::fromUtf8(spec.fallbackDescription),
        menuDescription);

    QObject* pageController = nullptr;
    if (spec.controllerFactoryExt)
    {
        FeatureControllerContext ctx;
        ctx.parent = &app;
        ctx.localization = localization;
        ctx.configuration = configuration;
        ctx.configPath = configPath;
        ctx.telemetry = &telemetry;
        pageController = spec.controllerFactoryExt(ctx);
    }
    else if (spec.controllerFactory)
    {
        pageController = spec.controllerFactory(&app, localization);
    }

    {
        auto localeSettings = stok::services::common::readDdsSettings(*configuration);
        localeSettings.topicName = configuration->getString("dds.localeTopic", "stok.ui.locale");
        localization->start(localeSettings, identity.name + "-locale-subscriber");
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("localizationController", localization);
    engine.rootContext()->setContextProperty("pageTitle", menuTitle);
    engine.rootContext()->setContextProperty("pageDescription", menuDescription);
    engine.rootContext()->setContextProperty("pageController", pageController);
    engine.loadFromModule(QString::fromUtf8(spec.qmlModuleUri), "Main");
    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }

    auto* window = qobject_cast<QWindow*>(engine.rootObjects().first());
    stok::services::common::HostedViewPublisher bridge(
        hostedViewSettings,
        menuId,
        menuTitle,
        menuDescription,
        menuGroup,
        telemetry.client());
    if (window)
    {
        window->winId();
        bridge.setHostWindow(window);
        bridge.start(QStringLiteral("%1-hosted-view").arg(menuId));
    }

    append_bootstrap_log(menuId, QStringLiteral("entering QML event loop"));
    return app.exec();
}

} // namespace stok::services::feature_page
