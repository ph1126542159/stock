#pragma once

#include "QuoteListModel.h"
#include "Poco/AutoPtr.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/StockQuoteBus.h"
#include "stok/services/common/TelemetryBootstrap.h"
#include <QObject>
#include <QString>

class QTimer;

class DesktopShellController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QuoteListModel* quotesModel READ quotesModel CONSTANT)
    Q_PROPERTY(QString serviceName READ serviceName CONSTANT)
    Q_PROPERTY(QString topicName READ topicName CONSTANT)
    Q_PROPERTY(QString telemetryMode READ telemetryMode CONSTANT)
    Q_PROPERTY(QString feedState READ feedState NOTIFY feedStateChanged)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY statusLineChanged)
    Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(int matchedPublishers READ matchedPublishers NOTIFY matchedPublishersChanged)
    Q_PROPERTY(int quoteCount READ quoteCount NOTIFY quoteCountChanged)

public:
    DesktopShellController(
        const stok::services::common::ServiceIdentity& identity,
        const stok::services::common::DdsSettings& ddsSettings,
        Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
        stok::services::common::ServiceTelemetry& telemetry,
        QObject* parent = nullptr);
    ~DesktopShellController() override;

    QuoteListModel* quotesModel();
    QString serviceName() const;
    QString topicName() const;
    QString telemetryMode() const;
    QString feedState() const;
    QString statusLine() const;
    QString lastUpdated() const;
    int matchedPublishers() const;
    int quoteCount() const;

    Q_INVOKABLE void start();

signals:
    void feedStateChanged();
    void statusLineChanged();
    void lastUpdatedChanged();
    void matchedPublishersChanged();
    void quoteCountChanged();

private:
    void handleQuote(const stok::services::common::StockQuote& quote);
    void handleMatchCount(int count);

    stok::services::common::ServiceIdentity identity_;
    stok::services::common::DdsSettings ddsSettings_;
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration_;
    stok::services::common::ServiceTelemetry& telemetry_;
    QuoteListModel quotesModel_;
    stok::services::common::StockQuoteSubscriber subscriber_;
    QString telemetryMode_;
    QString feedState_ = QStringLiteral("Connecting");
    QString statusLine_ = QStringLiteral("Waiting for the market bus");
    QString lastUpdated_ = QStringLiteral("No quotes yet");
    int matchedPublishers_ = 0;
    QTimer* heartbeatTimer_ = nullptr;
};
