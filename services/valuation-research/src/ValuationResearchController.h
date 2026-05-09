#pragma once

#include "Poco/AutoPtr.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "stok/services/common/TelemetryBootstrap.h"
#include "stok/services/common/TextMessageBus.h"
#include "stok/services/feature_page/FeaturePageRunner.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QTimer;

namespace stok::services::common { class LocalizationClient; }

class ValuationResearchController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scoreCards READ scoreCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList valuationRows READ valuationRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList researchRows READ researchRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList thesisRows READ thesisRows NOTIFY dataChanged)
    Q_PROPERTY(QStringList watchlist READ watchlist NOTIFY watchlistChanged)
    Q_PROPERTY(QVariantList watchlistRich READ watchlistRich NOTIFY dataChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QString lastUpdatedText READ lastUpdatedText NOTIFY dataChanged)
    Q_PROPERTY(QString assumptionSummary READ assumptionSummary NOTIFY dataChanged)

public:
    explicit ValuationResearchController(
        stok::services::common::LocalizationClient* localization,
        Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
        QString configPath,
        stok::services::common::ServiceTelemetry* telemetry,
        QObject* parent = nullptr);

    ~ValuationResearchController() override;

    QString status() const;
    QVariantList scoreCards() const;
    QVariantList valuationRows() const;
    QVariantList researchRows() const;
    QVariantList thesisRows() const;
    QStringList watchlist() const;
    QVariantList watchlistRich() const;
    bool refreshing() const;
    QString lastUpdatedText() const;
    QString assumptionSummary() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool addSymbol(const QString& spec);
    Q_INVOKABLE bool removeSymbol(const QString& spec);

signals:
    void dataChanged();
    void watchlistChanged();
    void refreshingChanged();

private:
    void initializeDatabase();
    void ensureSchema();
    void loadFromDatabase();
    void saveToDatabase(const QVariantList& rows, const QDateTime& generatedAt);
    void seedWatchlist();
    void writeWatchlist();
    QStringList loadWatchlist() const;

    void runProvider();
    void onProviderFinished(int exitCode, const QString& stdoutText, const QString& stderrText);

    void rebuildAggregates();
    void recomputeBands();
    void rebuildResearchAndThesis();

    void handleConfigMessage(const QString& payload);
    void applyConfigValue(const QString& key, const QString& value);

    QPointer<stok::services::common::LocalizationClient> localization_;
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration_;
    QString configPath_;
    stok::services::common::ServiceTelemetry* telemetry_ = nullptr;

    QString providerCommand_;
    QString providerScriptPath_;
    int providerTimeoutMs_ = 8 * 60 * 1000;
    int refreshIntervalMs_ = 30 * 60 * 1000;

    double discountRate_ = 0.085;
    double terminalGrowth_ = 0.025;
    double growthRate_ = 0.06;
    int horizonYears_ = 10;
    int historyYears_ = 8;
    double buyMultiplier_ = 0.85;
    double sellMultiplier_ = 1.18;

    QString cacheDirectoryPath_;
    QString databasePath_;
    QString databaseConnectionName_;
    QSqlDatabase database_;

    QStringList watchlist_;
    QString status_;
    QVariantList scoreCards_;
    QVariantList valuationRows_;
    QVariantList researchRows_;
    QVariantList thesisRows_;
    QDateTime generatedAt_;
    bool refreshing_ = false;

    QTimer* refreshTimer_ = nullptr;
    stok::services::common::TextMessageSubscriber configSubscriber_;
};

QObject* createValuationResearchController(
    const stok::services::feature_page::FeatureControllerContext& ctx);
