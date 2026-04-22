#include "DesktopShellController.h"

#include <QMetaObject>
#include <QTimer>
#include <QStringList>
#include <chrono>

namespace {

QStringList watchlist_from_config(const Poco::Util::AbstractConfiguration& configuration)
{
    const auto items = stok::services::common::splitList(
        configuration.getString("ui.watchlist", "AAPL,MSFT,NVDA,TSLA,600519.SH,000001.SZ"));

    QStringList list;
    for (const auto& item : items)
    {
        list.append(QString::fromStdString(item));
    }
    return list;
}

} // namespace

DesktopShellController::DesktopShellController(
    const stok::services::common::ServiceIdentity& identity,
    const stok::services::common::DdsSettings& ddsSettings,
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
    stok::services::common::ServiceTelemetry& telemetry,
    QObject* parent):
    QObject(parent),
    identity_(identity),
    ddsSettings_(ddsSettings),
    configuration_(std::move(configuration)),
    telemetry_(telemetry),
    subscriber_(ddsSettings_, telemetry_.client())
{
    telemetryMode_ = configuration_->getBool("telemetry.export.enabled", false)
        ? QStringLiteral("OTLP export enabled")
        : QStringLiteral("Local telemetry buffer");

    quotesModel_.seedSymbols(watchlist_from_config(*configuration_));
    connect(&quotesModel_, &QuoteListModel::countChanged, this, &DesktopShellController::quoteCountChanged);

    heartbeatTimer_ = new QTimer(this);
    heartbeatTimer_->setInterval(5000);
    connect(heartbeatTimer_, &QTimer::timeout, this, [this]()
    {
        telemetry_.client().metric(
            "stok.ui.visible_quotes",
            static_cast<double>(quotesModel_.count()),
            "count",
            "Number of watchlist rows visible in the desktop shell",
            {{"service.name", identity_.name}});
    });
}

DesktopShellController::~DesktopShellController()
{
    subscriber_.stop();
}

QuoteListModel* DesktopShellController::quotesModel()
{
    return &quotesModel_;
}

QString DesktopShellController::serviceName() const
{
    return QString::fromStdString(identity_.name);
}

QString DesktopShellController::topicName() const
{
    return QString::fromStdString(ddsSettings_.topicName);
}

QString DesktopShellController::telemetryMode() const
{
    return telemetryMode_;
}

QString DesktopShellController::feedState() const
{
    return feedState_;
}

QString DesktopShellController::statusLine() const
{
    return statusLine_;
}

QString DesktopShellController::lastUpdated() const
{
    return lastUpdated_;
}

int DesktopShellController::matchedPublishers() const
{
    return matchedPublishers_;
}

int DesktopShellController::quoteCount() const
{
    return quotesModel_.count();
}

void DesktopShellController::start()
{
    std::string error;
    const bool started = subscriber_.start(
        identity_.name,
        [this](const stok::services::common::StockQuote& quote)
        {
            handleQuote(quote);
        },
        [this](int count)
        {
            handleMatchCount(count);
        },
        &error);

    if (!started)
    {
        feedState_ = QStringLiteral("Offline");
        statusLine_ = QStringLiteral("Failed to subscribe: %1").arg(QString::fromStdString(error));
        emit feedStateChanged();
        emit statusLineChanged();
        telemetry_.logger().error(statusLine_.toStdString());
        return;
    }

    heartbeatTimer_->start();
    telemetry_.recordStartup(
        "desktop-shell",
        {{"dds.topic", ddsSettings_.topicName}, {"ui.watchlist.count", std::to_string(quotesModel_.count())}});
}

void DesktopShellController::handleQuote(const stok::services::common::StockQuote& quote)
{
    QMetaObject::invokeMethod(this, [this, quote]()
    {
        quotesModel_.upsertQuote(
            QString::fromStdString(quote.symbol),
            QString::fromStdString(quote.name),
            QString::fromStdString(quote.market),
            quote.lastPrice,
            quote.change,
            quote.percentChange,
            static_cast<quint64>(quote.volume),
            static_cast<qint64>(quote.timestampMs));

        feedState_ = QStringLiteral("Live");
        statusLine_ = QStringLiteral("%1 updated at %2")
            .arg(QString::fromStdString(quote.symbol))
            .arg(QString::fromStdString(std::to_string(quote.timestampMs)));
        lastUpdated_ = QStringLiteral("%1 / %2")
            .arg(QString::fromStdString(quote.symbol))
            .arg(QString::fromStdString(quote.market));

        emit feedStateChanged();
        emit statusLineChanged();
        emit lastUpdatedChanged();
        emit quoteCountChanged();
    }, Qt::QueuedConnection);
}

void DesktopShellController::handleMatchCount(int count)
{
    QMetaObject::invokeMethod(this, [this, count]()
    {
        matchedPublishers_ = count;
        if (matchedPublishers_ <= 0)
        {
            feedState_ = QStringLiteral("Waiting for publisher");
            emit feedStateChanged();
        }
        emit matchedPublishersChanged();
    }, Qt::QueuedConnection);
}
