#pragma once

#include "HoldingsModel.h"

#include "Poco/AutoPtr.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "stok/services/common/TelemetryBootstrap.h"
#include "stok/services/common/TextMessageBus.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <functional>

class QTimer;

class PortfolioController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(HoldingsModel* holdingsModel READ holdingsModel CONSTANT)
    Q_PROPERTY(QVariantList institutionOptions READ institutionOptions NOTIFY institutionOptionsChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool analysisBusy READ analysisBusy NOTIFY analysisBusyChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedSymbol READ selectedSymbol NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedType READ selectedType NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedInstitution READ selectedInstitution NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedAnalysis READ selectedAnalysis NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedSuggestion READ selectedSuggestion NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedIndustryOutlook READ selectedIndustryOutlook NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedAction READ selectedAction NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedPositionPlan READ selectedPositionPlan NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedStopLossPlan READ selectedStopLossPlan NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedTakeProfitPlan READ selectedTakeProfitPlan NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedFinancialSignal READ selectedFinancialSignal NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedMarketSignal READ selectedMarketSignal NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QString selectedForecastSignal READ selectedForecastSignal NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedLastPrice READ selectedLastPrice NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedAiScore READ selectedAiScore NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedTechnicalScore READ selectedTechnicalScore NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedFundamentalScore READ selectedFundamentalScore NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedRiskScore READ selectedRiskScore NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedMaxDrawdownPct READ selectedMaxDrawdownPct NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedVolatilityPct READ selectedVolatilityPct NOTIFY selectedHoldingChanged)
    Q_PROPERTY(double selectedOneHourChangePct READ selectedOneHourChangePct NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QVariantList selectedOneHourTrend READ selectedOneHourTrend NOTIFY selectedHoldingChanged)
    Q_PROPERTY(QVariantList selectedOneMonthIndustryTrend READ selectedOneMonthIndustryTrend NOTIFY selectedHoldingChanged)

public:
    explicit PortfolioController(
        Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
        QString configPath,
        stok::services::common::ServiceTelemetry& telemetry,
        QObject* parent = nullptr);
    ~PortfolioController() override;

    HoldingsModel* holdingsModel();
    QVariantList institutionOptions() const;
    QString status() const;
    bool analysisBusy() const;
    QString selectedName() const;
    QString selectedSymbol() const;
    QString selectedType() const;
    QString selectedInstitution() const;
    QString selectedAnalysis() const;
    QString selectedSuggestion() const;
    QString selectedIndustryOutlook() const;
    QString selectedAction() const;
    QString selectedPositionPlan() const;
    QString selectedStopLossPlan() const;
    QString selectedTakeProfitPlan() const;
    QString selectedFinancialSignal() const;
    QString selectedMarketSignal() const;
    QString selectedForecastSignal() const;
    double selectedLastPrice() const;
    double selectedAiScore() const;
    double selectedTechnicalScore() const;
    double selectedFundamentalScore() const;
    double selectedRiskScore() const;
    double selectedMaxDrawdownPct() const;
    double selectedVolatilityPct() const;
    double selectedOneHourChangePct() const;
    QVariantList selectedOneHourTrend() const;
    QVariantList selectedOneMonthIndustryTrend() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void selectHolding(int row);
    Q_INVOKABLE bool addHolding(const QString& input, const QString& institution);
    Q_INVOKABLE void refreshSelected();
    Q_INVOKABLE void setRealtimeActive(bool active);

signals:
    void institutionOptionsChanged();
    void statusChanged();
    void analysisBusyChanged();
    void selectedHoldingChanged();

private:
    struct QuoteState
    {
        double lastPrice = 0.0;
        double driftPct = 0.0;
        double volatilityPct = 0.0;
        double phase = 0.0;
    };

    void initializeDatabase();
    void ensureSchema();
    bool databaseReady() const;
    void loadInstitutions();
    void loadHoldings();
    void seedCommercialDemoHoldings();
    void saveHolding(const HoldingEntry& entry);
    void saveQuote(const QString& symbol, double price, qint64 timestampMs);
    QVector<QPair<qint64, double>> loadQuoteHistory(const QString& symbol, qint64 cutoffTimestampMs) const;
    void syncModel();
    void updateQuotes();
    void startConfigSubscription();
    void handleConfigMessage(const QString& payload);
    void applyConfigValue(const QString& key, const QString& valueText);
    void requestAnalysis(const QString& holdingId);
    int holdingIndexById(const QString& holdingId) const;
    int selectedIndex() const;
    void setSelectedIndex(int index);
    QVariantList seriesToVariantList(const QVector<double>& values) const;
    QString resolveCodexCommand() const;
    void runCodexStructuredTask(
        const QString& operationName,
        const QString& prompt,
        const QJsonObject& schema,
        const std::function<void(const QJsonObject&)>& onSuccess,
        const std::function<void(const QString&)>& onError);
    QString buildCodexPrompt(const QString& prompt) const;
    QString codexArtifactsDirectoryPath() const;
    bool writeTextArtifact(const QString& path, const QString& text) const;
    bool writeJsonArtifact(const QString& path, const QJsonObject& object) const;
    QString readTextFile(const QString& path) const;
    QString summarizeCodexFailure(const QString& stdoutText, const QString& stderrText) const;
    QJsonObject holdingAnalysisSchema() const;

    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration_;
    QString configPath_;
    stok::services::common::ServiceTelemetry& telemetry_;
    HoldingsModel holdingsModel_;
    QVariantList institutionOptions_;
    QString status_;
    bool analysisBusy_ = false;
    QVector<HoldingEntry> holdings_;
    QHash<QString, QuoteState> quoteStates_;
    QHash<QString, QVector<QPair<qint64, double>>> quoteHistory_;
    int selectedHoldingIndex_ = -1;
    QTimer* quoteTimer_ = nullptr;
    QTimer* analysisTimer_ = nullptr;
    stok::services::common::TextMessageSubscriber configSubscriber_;
    QString databasePath_;
    QString marketBoardDatabasePath_;
    QString databaseConnectionName_;
    QSqlDatabase database_;
    QString codexCommand_;
    QString codexModel_;
    QString codexWorkingDirectory_;
    QString cacheDirectoryPath_;
    int codexTimeoutMs_ = 180000;
    int quoteIntervalMs_ = 1000;
    int analysisIntervalMs_ = 300000;
    bool analysisInFlight_ = false;
    bool realtimeActive_ = true;
    qint64 lastQuotePersistMs_ = 0;
};
