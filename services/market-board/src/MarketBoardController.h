#pragma once

#include "InstitutionRankingModel.h"
#include "InvestmentOpportunityModel.h"
#include "UsMarketWatchModel.h"

#include "Poco/AutoPtr.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "stok/services/common/TelemetryBootstrap.h"
#include "stok/services/common/TextMessageBus.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <functional>

class QTimer;
class QNetworkAccessManager;
class QNetworkReply;

class MarketBoardController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(InstitutionRankingModel* institutionModel READ institutionModel CONSTANT)
    Q_PROPERTY(UsMarketWatchModel* usMarketModel READ usMarketModel CONSTANT)
    Q_PROPERTY(InvestmentOpportunityModel* fundModel READ fundModel CONSTANT)
    Q_PROPERTY(InvestmentOpportunityModel* stockModel READ stockModel CONSTANT)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString institutionBoardTitle READ institutionBoardTitle CONSTANT)
    Q_PROPERTY(QString valueBoardTitle READ valueBoardTitle CONSTANT)
    Q_PROPERTY(QString selectedInstitutionTitle READ selectedInstitutionTitle NOTIFY selectedInstitutionChanged)
    Q_PROPERTY(QString institutionStatus READ institutionStatus NOTIFY institutionStatusChanged)
    Q_PROPERTY(QString usMarketStatus READ usMarketStatus NOTIFY usMarketStatusChanged)
    Q_PROPERTY(QString valueStatus READ valueStatus NOTIFY valueStatusChanged)
    Q_PROPERTY(QString selectedAssetName READ selectedAssetName NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetCode READ selectedAssetCode NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetCategory READ selectedAssetCategory NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetProvider READ selectedAssetProvider NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetInvestmentAnalysis READ selectedAssetInvestmentAnalysis NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetSixMonthForecast READ selectedAssetSixMonthForecast NOTIFY selectedAssetChanged)
    Q_PROPERTY(QString selectedAssetKind READ selectedAssetKind NOTIFY selectedAssetChanged)
    Q_PROPERTY(double selectedAssetScore READ selectedAssetScore NOTIFY selectedAssetChanged)
    Q_PROPERTY(double selectedAssetLatestPrice READ selectedAssetLatestPrice NOTIFY selectedAssetChanged)
    Q_PROPERTY(double selectedAssetOneYearReturn READ selectedAssetOneYearReturn NOTIFY selectedAssetChanged)
    Q_PROPERTY(QVariantList selectedAssetOneYearTrend READ selectedAssetOneYearTrend NOTIFY selectedAssetChanged)
    Q_PROPERTY(QVariantList selectedAssetOneHourDrawdown READ selectedAssetOneHourDrawdown NOTIFY selectedAssetChanged)
    Q_PROPERTY(QVariantList selectedFundConfigCards READ selectedFundConfigCards NOTIFY selectedAssetChanged)
    Q_PROPERTY(QVariantList selectedFundHoldingCards READ selectedFundHoldingCards NOTIFY selectedAssetChanged)
    Q_PROPERTY(QVariantList selectedFundTopHoldings READ selectedFundTopHoldings NOTIFY selectedAssetChanged)
    Q_PROPERTY(QVariantMap freeDataSnapshot READ freeDataSnapshot NOTIFY freeDataSnapshotChanged)
    Q_PROPERTY(QString freeDataStatus READ freeDataStatus NOTIFY freeDataSnapshotChanged)
    Q_PROPERTY(QVariantMap fundHoldingsSnapshot READ fundHoldingsSnapshot NOTIFY fundHoldingsChanged)
    Q_PROPERTY(QString fundHoldingsStatus READ fundHoldingsStatus NOTIFY fundHoldingsChanged)

public:
    enum Page
    {
        InstitutionBoardPage = 0,
        ValueBoardPage = 1
    };
    Q_ENUM(Page)

    MarketBoardController(
        Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
        QString configPath,
        stok::services::common::ServiceTelemetry& telemetry,
        QObject* parent = nullptr);
    ~MarketBoardController() override;

    InstitutionRankingModel* institutionModel();
    UsMarketWatchModel* usMarketModel();
    InvestmentOpportunityModel* fundModel();
    InvestmentOpportunityModel* stockModel();
    int currentPage() const;
    QString institutionBoardTitle() const;
    QString valueBoardTitle() const;
    QString selectedInstitutionTitle() const;
    QString institutionStatus() const;
    QString usMarketStatus() const;
    QString valueStatus() const;
    QString selectedAssetName() const;
    QString selectedAssetCode() const;
    QString selectedAssetCategory() const;
    QString selectedAssetProvider() const;
    QString selectedAssetInvestmentAnalysis() const;
    QString selectedAssetSixMonthForecast() const;
    QString selectedAssetKind() const;
    double selectedAssetScore() const;
    double selectedAssetLatestPrice() const;
    double selectedAssetOneYearReturn() const;
    QVariantList selectedAssetOneYearTrend() const;
    QVariantList selectedAssetOneHourDrawdown() const;
    QVariantList selectedFundConfigCards() const;
    QVariantList selectedFundHoldingCards() const;
    QVariantList selectedFundTopHoldings() const;
    QVariantMap freeDataSnapshot() const;
    QString freeDataStatus() const;
    QVariantMap fundHoldingsSnapshot() const;
    QString fundHoldingsStatus() const;
    Q_INVOKABLE void requestFundHoldings(const QString& fundCode);

    Q_INVOKABLE void start();
    Q_INVOKABLE void openValueBoard(int institutionRow);
    Q_INVOKABLE void backToInstitutionBoard();
    Q_INVOKABLE void selectFund(int row);
    Q_INVOKABLE void selectStock(int row);
    Q_INVOKABLE void refreshInstitutions();
    Q_INVOKABLE void refreshValueBoard();
    Q_INVOKABLE bool addUsWatchSymbol(const QString& input);
    Q_INVOKABLE void setRealtimeActive(bool active);
    Q_INVOKABLE void refreshFreeDataProvider();

signals:
    void currentPageChanged();
    void selectedInstitutionChanged();
    void institutionStatusChanged();
    void usMarketStatusChanged();
    void valueStatusChanged();
    void selectedAssetChanged();
    void freeDataSnapshotChanged();
    void fundHoldingsChanged();

private:
    void initializeDatabase();
    void ensureSchema();
    bool databaseReady() const;

    void loadInstitutionBoard();
    void loadValueBoard(const QString& institutionId);
    void loadUsMarketBoard();
    void applyInstitutionBoard(const QVector<InstitutionBoardEntry>& entries, const QDateTime& updatedAt, bool persisted);
    void applyValueBoard(const ValueBoardSnapshot& snapshot, const QDateTime& updatedAt, bool persisted);
    void selectDefaultAsset();
    void setSelectedAsset(const ValueAssetEntry* entry, const QString& assetKind = QString());
    bool shouldRefresh(const QDateTime& updatedAt, int intervalMs) const;
    QString cacheDirectoryPath() const;
    QString databasePath() const;

    QVector<InstitutionBoardEntry> loadInstitutionBoardFromDatabase(QDateTime* updatedAt) const;
    ValueBoardSnapshot loadValueBoardFromDatabase(const QString& institutionId, QDateTime* updatedAt) const;
    QVector<UsMarketWatchEntry> loadUsWatchlistFromDatabase() const;
    QVector<QPair<qint64, double>> loadUsHistoryFromDatabase(const QString& symbol, qint64 cutoffTimestampMs) const;
    void saveInstitutionBoardToDatabase(const QVector<InstitutionBoardEntry>& entries, const QDateTime& updatedAt);
    void saveValueBoardToDatabase(const ValueBoardSnapshot& snapshot, const QDateTime& updatedAt);
    bool saveUsWatchDefinitionToDatabase(const UsMarketWatchEntry& entry, qint64 createdAtMs = 0);
    void saveUsQuoteToDatabase(const QString& symbol, const QString& name, const QString& market, double lastPrice, qint64 timestampMs);
    void seedDefaultUsWatchlist();

    QVector<InstitutionBoardEntry> fallbackInstitutions() const;
    ValueBoardSnapshot fallbackValueBoard(const QString& institutionId, const QString& institutionName) const;
    QVector<UsMarketWatchEntry> defaultUsWatchlist() const;
    UsMarketWatchEntry resolveUsWatchInput(const QString& input) const;
    void syncUsMarketModel();
    void updateUsMarketBoard();
    QString tencentUsQuerySymbol(const QString& symbol) const;
    void handleTencentUsQuoteReply(QNetworkReply* reply);
    double computeUsOneHourChangePct(const QString& symbol, double currentPrice, qint64 nowMs) const;
    QString formatUsStatus() const;

    void requestInstitutionBoard();
    void requestValueBoard(const QString& institutionId, const QString& institutionName);
    void seedUsHistoryFromProvider();
    void handleUsHistoryProviderResult(int exitCode, const QString& stdoutText, const QString& stderrText);
    bool requestValueBoardViaPython(const QString& institutionId, const QString& institutionName);
    void handleValueBoardProviderResult(
        const QString& institutionId,
        const QString& institutionName,
        int exitCode,
        const QString& stdoutText,
        const QString& stderrText);
    void purgeNonRealValueBoardData();
    void startConfigSubscription();
    void handleConfigMessage(const QString& payload);
    void applyConfigValue(const QString& key, const QString& valueText);
    void tickSelectedAssetRealtime();
    void startFreeDataProvider();
    void handleFreeDataProviderResult(int exitCode, const QString& stdoutText, const QString& stderrText);
    void runCodexStructuredTask(
        const QString& operationName,
        const QString& prompt,
        const QJsonObject& schema,
        const std::function<void(const QJsonObject&)>& onSuccess,
        const std::function<void(const QString&)>& onError);
    QString resolveCodexCommand() const;
    QString codexArtifactsDirectoryPath() const;
    QString buildCodexPrompt(const QString& prompt) const;
    QString readTextFile(const QString& path) const;
    QString summarizeCodexFailure(const QString& stdoutText, const QString& stderrText) const;
    bool writeTextArtifact(const QString& path, const QString& text) const;
    bool writeJsonArtifact(const QString& path, const QJsonObject& object) const;
    QJsonObject institutionSchema() const;
    QJsonObject valueBoardSchema() const;
    QVector<InstitutionBoardEntry> parseInstitutions(const QJsonObject& payload) const;
    ValueBoardSnapshot parseValueBoard(const QString& institutionId, const QString& institutionName, const QJsonObject& payload) const;
    QString currentDateLabel() const;
    QString institutionStatusFromTimestamp(const QString& prefix, const QDateTime& updatedAt) const;
    QVariantList seriesToVariantList(const QVector<double>& values) const;

    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration_;
    QString configPath_;
    stok::services::common::ServiceTelemetry& telemetry_;
    InstitutionRankingModel institutionModel_;
    UsMarketWatchModel usMarketModel_;
    InvestmentOpportunityModel fundModel_;
    InvestmentOpportunityModel stockModel_;
    QTimer* institutionRefreshTimer_ = nullptr;
    QTimer* usMarketTimer_ = nullptr;
    QTimer* valueRefreshTimer_ = nullptr;
    QTimer* selectedAssetRealtimeTimer_ = nullptr;
    QNetworkAccessManager* networkAccess_ = nullptr;
    int currentPage_ = InstitutionBoardPage;
    QString selectedInstitutionId_;
    QString selectedInstitutionName_;
    ValueAssetEntry selectedAsset_;
    QString selectedAssetKind_;
    bool hasSelectedAsset_ = false;
    QDateTime institutionUpdatedAt_;
    QDateTime valueUpdatedAt_;
    QString institutionStatus_;
    QString usMarketStatus_;
    QString valueStatus_;
    QString codexCommand_;
    QString codexModel_;
    QString codexWorkingDirectory_;
    QString freeDataProviderCommand_;
    QString freeDataProviderScriptPath_;
    QString valueBoardProviderCommand_;
    QString valueBoardProviderScriptPath_;
    int valueBoardProviderTimeoutMs_ = 5 * 60 * 1000;
    QString usHistoryProviderScriptPath_;
    bool usHistoryRequestInFlight_ = false;
    bool usHistorySeeded_ = false;
    int codexTimeoutMs_ = 3 * 60 * 1000;
    int institutionRefreshIntervalMs_ = 60 * 60 * 1000;
    int usMarketRefreshIntervalMs_ = 1000;
    int valueRefreshIntervalMs_ = 5 * 60 * 1000;
    int freeDataRefreshIntervalMs_ = 5 * 60 * 1000;
    QString cacheDirectoryPath_;
    QString databasePath_;
    QString databaseConnectionName_;
    QSqlDatabase database_;
    stok::services::common::TextMessageSubscriber configSubscriber_;
    QVector<UsMarketWatchEntry> usWatchEntries_;
    QHash<QString, QVector<QPair<qint64, double>>> usQuoteHistory_;
    QTimer* freeDataRefreshTimer_ = nullptr;
    QVariantMap freeDataSnapshot_;
    QString fundHoldingsScriptPath_;
    QVariantMap fundHoldingsSnapshot_;
    QString fundHoldingsStatus_;
    QString fundHoldingsRequestedCode_;
    bool fundHoldingsRequestInFlight_ = false;
    QString freeDataStatus_;
    bool freeDataRequestInFlight_ = false;
    bool institutionLoadedFromStorage_ = false;
    bool valueLoadedFromStorage_ = false;
    bool institutionRequestInFlight_ = false;
    bool valueRequestInFlight_ = false;
    bool usQuoteRequestInFlight_ = false;
    bool realtimeActive_ = true;
    qint64 lastUsQuotePersistMs_ = 0;
    double selectedAssetRealtimeBasePrice_ = 0.0;
    double selectedAssetRealtimePhase_ = 0.0;
};
