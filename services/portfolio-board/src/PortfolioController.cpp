#include "PortfolioController.h"

#include "QCoreApplication"
#include "QDate"
#include "QDateTime"
#include "QDir"
#include "QFile"
#include "QFileInfo"
#include "QJsonArray"
#include "QJsonDocument"
#include "QJsonObject"
#include "QMetaObject"
#include "QPointer"
#include "QProcess"
#include "QRandomGenerator"
#include "QRegularExpression"
#include "QSaveFile"
#include "QSqlError"
#include "QSqlQuery"
#include "QStandardPaths"
#include "QTimer"
#include "stok/services/common/ServiceConfig.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr bool kDataQualityGatePassed = false;
constexpr double kRiskGateMinimumScore = 60.0;

double clamp_score(double value)
{
    if (!std::isfinite(value))
    {
        return 0.0;
    }
    return std::clamp(value, 0.0, 100.0);
}

double trend_volatility_pct(const QVector<double>& values)
{
    if (values.size() < 2)
    {
        return 0.0;
    }

    double sum = 0.0;
    for (const double value : values)
    {
        sum += value;
    }
    const double mean = sum / static_cast<double>(values.size());
    double variance = 0.0;
    for (const double value : values)
    {
        const double delta = value - mean;
        variance += delta * delta;
    }
    return std::sqrt(variance / static_cast<double>(values.size()));
}

double trend_max_drawdown_pct(const QVector<double>& values)
{
    if (values.size() < 2)
    {
        return 0.0;
    }

    double peak = values.constFirst();
    double maxDrawdown = 0.0;
    for (const double value : values)
    {
        peak = std::max(peak, value);
        maxDrawdown = std::min(maxDrawdown, value - peak);
    }
    return maxDrawdown;
}

double technical_score(const HoldingEntry& entry)
{
    const double start = entry.oneHourTrend.isEmpty() ? 0.0 : entry.oneHourTrend.constFirst();
    const double end = entry.oneHourTrend.isEmpty() ? entry.oneHourChangePct : entry.oneHourTrend.constLast();
    const double momentum = end - start;
    const double volatility = trend_volatility_pct(entry.oneHourTrend);
    return clamp_score(55.0 + entry.oneHourChangePct * 5.5 + momentum * 4.0 - volatility * 3.0);
}

double fundamental_score(const HoldingEntry& entry)
{
    const double typeBonus = entry.type.contains(QStringLiteral(u"\u57fa\u91d1")) ||
            entry.type.contains(QStringLiteral("ETF"))
        ? 2.0
        : 0.0;
    return clamp_score(entry.aiScore + typeBonus);
}

double safety_score(const HoldingEntry& entry)
{
    const double drawdown = std::abs(trend_max_drawdown_pct(entry.oneHourTrend));
    const double volatility = trend_volatility_pct(entry.oneHourTrend);
    const double concentrationPenalty = entry.symbol == QStringLiteral("NVDA") ? 8.0
        : (entry.symbol == QStringLiteral("AAPL") ? 4.0 : 0.0);
    return clamp_score(92.0 - drawdown * 5.0 - volatility * 6.0 - concentrationPenalty);
}

QString action_for_scores(double technical, double fundamental, double safety, double drawdown)
{
    if (drawdown <= -8.0 || safety < 35.0)
    {
        return QStringLiteral(u"\u6e05\u4ed3");
    }
    if (technical < 42.0 && safety < 55.0)
    {
        return QStringLiteral(u"\u5356\u51fa");
    }
    if (fundamental < 58.0 && technical < 50.0)
    {
        return QStringLiteral(u"\u8f6c\u4ed3");
    }
    if (fundamental >= 76.0 && technical >= 62.0 && safety >= 60.0)
    {
        return QStringLiteral(u"\u52a0\u4ed3");
    }
    return QStringLiteral(u"\u8ddf\u8fdb");
}

QString gate_action(const QString& action, double safety)
{
    const bool riskGatePassed = safety >= kRiskGateMinimumScore;
    if (kDataQualityGatePassed && riskGatePassed)
    {
        return action;
    }
    if (action == QStringLiteral(u"\u52a0\u4ed3"))
    {
        return QStringLiteral(u"\u8ddf\u8fdb");
    }
    if (action == QStringLiteral(u"\u6e05\u4ed3") || action == QStringLiteral(u"\u5356\u51fa"))
    {
        return QStringLiteral(u"\u8f6c\u4ed3");
    }
    return action;
}

QString action_for_entry(const HoldingEntry& entry)
{
    const double safety = safety_score(entry);
    return gate_action(
        action_for_scores(
            technical_score(entry),
            fundamental_score(entry),
            safety,
            trend_max_drawdown_pct(entry.oneHourTrend)),
        safety);
}

QString gate_reason(double safety)
{
    QStringList reasons;
    if (!kDataQualityGatePassed)
    {
        reasons << QStringLiteral(u"\u6570\u636e\u8d28\u91cf\u6a21\u62df\u95e8\u7981\u672a\u901a\u8fc7");
    }
    if (safety < kRiskGateMinimumScore)
    {
        reasons << QStringLiteral(u"\u98ce\u63a7\u5206 %1 \u4f4e\u4e8e %2")
            .arg(safety, 0, 'f', 0)
            .arg(kRiskGateMinimumScore, 0, 'f', 0);
    }
    return reasons.join(QStringLiteral(u"\uff1b"));
}

QString normalized_id(const QString& raw)
{
    QString normalized;
    normalized.reserve(raw.size());
    for (const QChar ch : raw.trimmed())
    {
        if (ch.isLetterOrNumber())
        {
            normalized.append(ch.toLower());
        }
        else if (ch.isSpace() || ch == '-' || ch == '_' || ch == '/')
        {
            normalized.append('-');
        }
    }

    while (normalized.contains(QStringLiteral("--")))
    {
        normalized.replace(QStringLiteral("--"), QStringLiteral("-"));
    }

    normalized.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return normalized.isEmpty() ? QStringLiteral("holding-%1").arg(qHash(raw)) : normalized;
}

QString normalized_symbol(const QString& raw)
{
    QString symbol;
    for (const QChar ch : raw.trimmed())
    {
        if (ch.isLetterOrNumber() || ch == '.' || ch == '-')
        {
            symbol.append(ch.toUpper());
        }
    }
    return symbol;
}

qint64 current_timestamp_ms()
{
    return QDateTime::currentDateTime().toMSecsSinceEpoch();
}

QJsonArray numbers_to_json(const QVector<double>& values)
{
    QJsonArray array;
    for (double value : values)
    {
        array.append(value);
    }
    return array;
}

QVector<double> json_to_numbers(const QJsonArray& array)
{
    QVector<double> values;
    values.reserve(array.size());
    for (const QJsonValue& value : array)
    {
        values.push_back(value.toDouble());
    }
    return values;
}

QVector<double> fallback_series(int count, double base, double drift, double wave)
{
    QVector<double> values;
    values.reserve(count);
    double current = base;
    for (int index = 0; index < count; ++index)
    {
        current += drift + std::sin(static_cast<double>(index) / 3.5) * wave;
        values.push_back(current);
    }
    return values;
}

stok::services::common::DdsSettings topic_settings(
    const Poco::Util::AbstractConfiguration& configuration,
    const std::string& key,
    const std::string& fallbackTopic)
{
    auto settings = stok::services::common::readDdsSettings(configuration);
    settings.topicName = configuration.getString(key, fallbackTopic);
    return settings;
}

QStringList fallback_institutions()
{
    return {
        QStringLiteral(u"\u8682\u8681\u8d22\u5bcc"),
        QStringLiteral(u"\u5fae\u4fe1\u7406\u8d22\u901a"),
        QStringLiteral(u"\u62db\u5546\u94f6\u884c"),
        QStringLiteral(u"\u5929\u5929\u57fa\u91d1"),
        QStringLiteral(u"\u4eac\u4e1c\u91d1\u878d")
    };
}

HoldingEntry make_demo_holding(
    const QString& symbol,
    const QString& name,
    const QString& type,
    const QString& institution,
    double lastPrice,
    double oneHourChangePct,
    double aiScore,
    const QString& suggestion,
    const QString& analysis,
    const QString& industryOutlook,
    double intradayBase,
    double industryBase)
{
    HoldingEntry entry;
    entry.id = normalized_id(symbol);
    entry.symbol = symbol;
    entry.name = name;
    entry.type = type;
    entry.institution = institution;
    entry.lastPrice = lastPrice;
    entry.oneHourChangePct = oneHourChangePct;
    entry.aiScore = clamp_score(aiScore);
    entry.suggestion = suggestion;
    entry.analysis = analysis;
    entry.industryOutlook = industryOutlook;
    entry.oneHourTrend = fallback_series(120, intradayBase, oneHourChangePct / 160.0, 0.035);
    entry.oneMonthIndustryTrend = fallback_series(30, industryBase, 0.42, 0.95);
    entry.updatedAt = QDateTime::currentDateTime();
    return entry;
}

} // namespace

PortfolioController::PortfolioController(
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
    QString configPath,
    stok::services::common::ServiceTelemetry& telemetry,
    QObject* parent):
    QObject(parent),
    configuration_(std::move(configuration)),
    configPath_(std::move(configPath)),
    telemetry_(telemetry),
    configSubscriber_(
        topic_settings(*configuration_, "dds.configTopic", "stok.ui.config"),
        telemetry_.client())
{
    databasePath_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("storage.sqlitePath", "data/portfolio-board/portfolio-board.db")));
    marketBoardDatabasePath_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("storage.marketBoardSqlitePath", "data/market-board/market-board.db")));
    databaseConnectionName_ = QStringLiteral("portfolio-board-%1")
        .arg(reinterpret_cast<quintptr>(this));
    codexCommand_ = QString::fromStdString(configuration_->getString("codex.command", "")).trimmed();
    codexModel_ = QString::fromStdString(configuration_->getString("codex.model", "")).trimmed();
    codexWorkingDirectory_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("codex.workDir", ".")));
    cacheDirectoryPath_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("storage.cacheDir", "data/portfolio-board")));
    codexTimeoutMs_ = configuration_->getInt("codex.timeoutMs", 180000);
    quoteIntervalMs_ = configuration_->getInt("update.quotes.intervalMs", 3000);
    analysisIntervalMs_ = configuration_->getInt("update.analysis.intervalMs", 300000);

    initializeDatabase();

    quoteTimer_ = new QTimer(this);
    quoteTimer_->setInterval(quoteIntervalMs_);
    connect(quoteTimer_, &QTimer::timeout, this, &PortfolioController::updateQuotes);

    analysisTimer_ = new QTimer(this);
    analysisTimer_->setInterval(analysisIntervalMs_);
    connect(analysisTimer_, &QTimer::timeout, this, &PortfolioController::refreshSelected);

    status_ = QStringLiteral(u"\u7b49\u5f85\u6301\u4ed3\u6570\u636e");
}

PortfolioController::~PortfolioController()
{
    configSubscriber_.stop();
    if (database_.isValid())
    {
        database_.close();
    }
    database_ = QSqlDatabase();
    if (!databaseConnectionName_.isEmpty())
    {
        QSqlDatabase::removeDatabase(databaseConnectionName_);
    }
}

HoldingsModel* PortfolioController::holdingsModel()
{
    return &holdingsModel_;
}

QVariantList PortfolioController::institutionOptions() const
{
    return institutionOptions_;
}

QString PortfolioController::status() const
{
    return status_;
}

bool PortfolioController::analysisBusy() const
{
    return analysisBusy_;
}

QString PortfolioController::selectedName() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).name : QString();
}

QString PortfolioController::selectedSymbol() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).symbol : QString();
}

QString PortfolioController::selectedType() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).type : QString();
}

QString PortfolioController::selectedInstitution() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).institution : QString();
}

QString PortfolioController::selectedAnalysis() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).analysis : QString();
}

QString PortfolioController::selectedSuggestion() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).suggestion : QString();
}

QString PortfolioController::selectedIndustryOutlook() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).industryOutlook : QString();
}

QString PortfolioController::selectedAction() const
{
    return selectedIndex() >= 0 ? action_for_entry(holdings_.at(selectedIndex())) : QStringLiteral(u"\u8ddf\u8fdb");
}

QString PortfolioController::selectedPositionPlan() const
{
    if (selectedIndex() < 0)
    {
        return {};
    }

    const HoldingEntry& entry = holdings_.at(selectedIndex());
    const QString action = action_for_entry(entry);
    const double technical = technical_score(entry);
    const double safety = safety_score(entry);
    const QString blockedReason = gate_reason(safety);
    if (!blockedReason.isEmpty())
    {
        return QStringLiteral(u"\u95e8\u7981\u539f\u56e0\uff1a%1\u3002\u6570\u636e\u8d28\u91cf/\u98ce\u63a7\u8fbe\u6807\u524d\uff0c\u539f\u6709\u52a0\u4ed3/\u5356\u51fa/\u6e05\u4ed3\u7b49\u5f3a\u52a8\u4f5c\u964d\u7ea7\u4e3a\u8ddf\u8fdb\u6216\u8f6c\u4ed3\uff0c\u6682\u4e0d\u589e\u52a0\u66b4\u9732\u3002")
            .arg(blockedReason);
    }
    if (action == QStringLiteral(u"\u52a0\u4ed3"))
    {
        return QStringLiteral(u"\u56de\u8c03\u6216\u91cf\u80fd\u786e\u8ba4\u540e\u5206\u6279\u52a0\u4ed3\uff0c\u5355\u6807\u7684\u4ed3\u4f4d\u4e0d\u8d85\u8fc7 12%-15%\u3002");
    }
    if (action == QStringLiteral(u"\u6e05\u4ed3"))
    {
        return QStringLiteral(u"\u98ce\u9669\u5206\u964d\u81f3 %1\uff0c\u5148\u4fdd\u62a4\u672c\u91d1\uff0c\u5206\u6279\u964d\u4e3a 0%-2% \u89c2\u5bdf\u4ed3\u3002")
            .arg(safety, 0, 'f', 0);
    }
    if (action == QStringLiteral(u"\u5356\u51fa"))
    {
        return QStringLiteral(u"\u6280\u672f\u5206 %1 \u504f\u5f31\uff0c\u5148\u51cf\u4ed3 30%-50%\uff0c\u7b49\u8d8b\u52bf\u4fee\u590d\u518d\u8bc4\u4f30\u3002")
            .arg(technical, 0, 'f', 0);
    }
    if (action == QStringLiteral(u"\u8f6c\u4ed3"))
    {
        return QStringLiteral(u"\u6536\u76ca/\u98ce\u9669\u6bd4\u4e0d\u5360\u4f18\uff0c\u8f6c\u5411\u4f4e\u4f30\u503c\u3001\u4f4e\u76f8\u5173\u6216\u66f4\u9ad8\u80dc\u7387\u6807\u7684\u3002");
    }
    return QStringLiteral(u"\u7ee7\u7eed\u8ddf\u8e2a\u8d22\u62a5\u3001\u884c\u4e1a\u8d8b\u52bf\u548c\u4ef7\u683c\u5f3a\u5f31\uff0c\u4fdd\u6301\u73b0\u6709\u4ed3\u4f4d\u3002");
}

QString PortfolioController::selectedStopLossPlan() const
{
    if (selectedIndex() < 0)
    {
        return {};
    }

    const double drawdown = selectedMaxDrawdownPct();
    const double stop = std::min(-5.0, drawdown - 2.0);
    return QStringLiteral(u"\u786c\u6b62\u635f %1%\uff1b\u82e5\u8dcc\u7834\u540c\u65f6\u8d22\u62a5/\u884c\u4e1a\u8d8b\u52bf\u8f6c\u5f31\uff0c\u89e6\u53d1\u6e05\u4ed3\u590d\u6838\u3002")
        .arg(stop, 0, 'f', 1);
}

QString PortfolioController::selectedTakeProfitPlan() const
{
    if (selectedIndex() < 0)
    {
        return {};
    }

    const HoldingEntry& entry = holdings_.at(selectedIndex());
    const double target = std::max(8.0, 18.0 - trend_volatility_pct(entry.oneHourTrend) * 2.0);
    return QStringLiteral(u"\u5206\u6279\u6b62\u76c8 %1%\uff1b\u82e5\u4f30\u503c\u6ea2\u4ef7\u6269\u5927\u6216\u98ce\u9669\u5206\u8dcc\u7834 55\uff0c\u964d\u4ed3\u9501\u5b9a\u6536\u76ca\u3002")
        .arg(target, 0, 'f', 1);
}

QString PortfolioController::selectedFinancialSignal() const
{
    if (selectedIndex() < 0)
    {
        return {};
    }
    const HoldingEntry& entry = holdings_.at(selectedIndex());
    return fundamental_score(entry) >= 75.0
        ? QStringLiteral(u"\u8d22\u62a5\u8d28\u91cf\u8f83\u5f3a\uff0c\u5173\u6ce8\u73b0\u91d1\u6d41\u3001\u6bdb\u5229\u7387\u548c\u4e0b\u671f\u6307\u5f15\u662f\u5426\u7ee7\u7eed\u652f\u6491\u4f30\u503c\u3002")
        : QStringLiteral(u"\u8d22\u62a5\u4fe1\u53f7\u5c1a\u4e0d\u5145\u5206\uff0c\u9700\u8865\u5145\u5229\u6da6\u589e\u901f\u3001\u73b0\u91d1\u6d41\u548c\u8d1f\u503a\u538b\u529b\u9a8c\u8bc1\u3002");
}

QString PortfolioController::selectedMarketSignal() const
{
    if (selectedIndex() < 0)
    {
        return {};
    }
    const HoldingEntry& entry = holdings_.at(selectedIndex());
    return technical_score(entry) >= 60.0
        ? QStringLiteral(u"\u77ed\u7ebf\u8d8b\u52bf\u76f8\u5bf9\u5360\u4f18\uff0c\u53ef\u7528\u6210\u4ea4\u91cf\u548c\u884c\u4e1a\u5f3a\u5f31\u505a\u4e8c\u6b21\u786e\u8ba4\u3002")
        : QStringLiteral(u"\u77ed\u7ebf\u52a8\u80fd\u504f\u5f31\uff0c\u907f\u514d\u8ffd\u9ad8\uff0c\u7b49\u5f85\u6b62\u8dcc\u3001\u653e\u91cf\u6216\u56de\u5230\u5173\u952e\u5747\u7ebf\u4e0a\u65b9\u3002");
}

QString PortfolioController::selectedForecastSignal() const
{
    if (selectedIndex() < 0)
    {
        return {};
    }
    const HoldingEntry& entry = holdings_.at(selectedIndex());
    return safety_score(entry) >= 65.0
        ? QStringLiteral(u"\u60c5\u666f\u9884\u6d4b\u504f\u7a33\uff0c\u57fa\u51c6\u60c5\u666f\u7ee7\u7eed\u6301\u6709\uff1b\u538b\u529b\u60c5\u666f\u4ee5\u56de\u64a4\u548c\u76f8\u5173\u6027\u4e3a\u4e3b\u8981\u98ce\u9669\u3002")
        : QStringLiteral(u"\u538b\u529b\u60c5\u666f\u6743\u91cd\u5347\u9ad8\uff0c\u5efa\u8bae\u964d\u4ed3\u6216\u8f6c\u5165\u4f4e\u6ce2\u52a8\u3001\u4f4e\u76f8\u5173\u6807\u7684\u3002");
}

double PortfolioController::selectedLastPrice() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).lastPrice : 0.0;
}

double PortfolioController::selectedAiScore() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).aiScore : 0.0;
}

double PortfolioController::selectedTechnicalScore() const
{
    return selectedIndex() >= 0 ? technical_score(holdings_.at(selectedIndex())) : 0.0;
}

double PortfolioController::selectedFundamentalScore() const
{
    return selectedIndex() >= 0 ? fundamental_score(holdings_.at(selectedIndex())) : 0.0;
}

double PortfolioController::selectedRiskScore() const
{
    return selectedIndex() >= 0 ? safety_score(holdings_.at(selectedIndex())) : 0.0;
}

double PortfolioController::selectedMaxDrawdownPct() const
{
    return selectedIndex() >= 0 ? trend_max_drawdown_pct(holdings_.at(selectedIndex()).oneHourTrend) : 0.0;
}

double PortfolioController::selectedVolatilityPct() const
{
    return selectedIndex() >= 0 ? trend_volatility_pct(holdings_.at(selectedIndex()).oneHourTrend) : 0.0;
}

double PortfolioController::selectedOneHourChangePct() const
{
    return selectedIndex() >= 0 ? holdings_.at(selectedIndex()).oneHourChangePct : 0.0;
}

QVariantList PortfolioController::selectedOneHourTrend() const
{
    return selectedIndex() >= 0
        ? seriesToVariantList(holdings_.at(selectedIndex()).oneHourTrend)
        : QVariantList{};
}

QVariantList PortfolioController::selectedOneMonthIndustryTrend() const
{
    return selectedIndex() >= 0
        ? seriesToVariantList(holdings_.at(selectedIndex()).oneMonthIndustryTrend)
        : QVariantList{};
}

void PortfolioController::start()
{
    QDir().mkpath(QFileInfo(databasePath_).absolutePath());
    QDir().mkpath(cacheDirectoryPath_);
    startConfigSubscription();
    loadInstitutions();
    loadHoldings();
    if (realtimeActive_)
    {
        quoteTimer_->start();
    }
    analysisTimer_->start();
}

void PortfolioController::setRealtimeActive(bool active)
{
    if (realtimeActive_ == active)
    {
        return;
    }

    realtimeActive_ = active;
    if (!quoteTimer_)
    {
        return;
    }

    if (realtimeActive_)
    {
        updateQuotes();
        quoteTimer_->start();
        return;
    }

    quoteTimer_->stop();
}

void PortfolioController::selectHolding(int row)
{
    setSelectedIndex(row);
}

bool PortfolioController::addHolding(const QString& input, const QString& institution)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
    {
        status_ = QStringLiteral(u"\u8bf7\u8f93\u5165\u57fa\u91d1\u6216\u80a1\u7968\u540d\u79f0/\u4ee3\u7801");
        emit statusChanged();
        return false;
    }

    const QString symbol = normalized_symbol(trimmed);
    if (symbol.isEmpty())
    {
        status_ = QStringLiteral(u"\u65e0\u6cd5\u89e3\u6790\u8f93\u5165");
        emit statusChanged();
        return false;
    }

    const auto existing = std::find_if(
        holdings_.cbegin(),
        holdings_.cend(),
        [&symbol](const HoldingEntry& entry)
        {
            return entry.symbol.compare(symbol, Qt::CaseInsensitive) == 0;
        });
    if (existing != holdings_.cend())
    {
        status_ = QStringLiteral(u"%1 \u5df2\u5728\u6301\u4ed3\u5217\u8868").arg(symbol);
        emit statusChanged();
        return false;
    }

    HoldingEntry entry;
    entry.symbol = symbol;
    entry.id = normalized_id(symbol);
    entry.name = trimmed;
    entry.type = (trimmed.contains(QStringLiteral(u"\u57fa\u91d1")) || symbol.startsWith('F'))
        ? QStringLiteral(u"\u57fa\u91d1")
        : QStringLiteral(u"\u80a1\u7968");
    entry.institution = institution.trimmed().isEmpty() && !institutionOptions_.isEmpty()
        ? institutionOptions_.constFirst().toString()
        : institution.trimmed();
    entry.lastPrice = 20.0 + static_cast<double>(qAbs(qHash(symbol)) % 8000) / 25.0;
    entry.updatedAt = QDateTime::currentDateTime();
    entry.suggestion = QStringLiteral(u"\u7b49\u5f85 AI \u5206\u6790");
    entry.analysis = QStringLiteral(u"\u6b63\u5728\u51c6\u5907\u6301\u4ed3\u5206\u6790");
    entry.industryOutlook = QStringLiteral(u"\u6b63\u5728\u51c6\u5907\u4e0b\u4e00\u4e2a\u6708\u884c\u4e1a\u8d70\u52bf");
    entry.oneHourTrend = fallback_series(120, 0.0, 0.02, 0.06);
    entry.oneMonthIndustryTrend = fallback_series(30, 78.0, 0.5, 1.2);

    holdings_.push_back(entry);
    quoteStates_.insert(symbol, QuoteState{
        entry.lastPrice,
        static_cast<double>((qHash(symbol) % 11) - 5) * 0.004,
        0.15 + static_cast<double>(qAbs(qHash(symbol)) % 5) * 0.03,
        static_cast<double>(qAbs(qHash(symbol)) % 360) / 27.0
    });
    quoteHistory_.insert(symbol, QVector<QPair<qint64, double>>{{current_timestamp_ms(), entry.lastPrice}});
    saveHolding(entry);
    saveQuote(symbol, entry.lastPrice, current_timestamp_ms());
    syncModel();
    setSelectedIndex(holdings_.size() - 1);
    requestAnalysis(entry.id);

    status_ = QStringLiteral(u"\u5df2\u65b0\u589e %1\uff0c\u6b63\u5728\u751f\u6210 AI \u5206\u6790").arg(entry.name);
    emit statusChanged();
    return true;
}

void PortfolioController::refreshSelected()
{
    if (selectedIndex() < 0)
    {
        return;
    }

    requestAnalysis(holdings_.at(selectedIndex()).id);
}

void PortfolioController::initializeDatabase()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
    {
        return;
    }

    QDir().mkpath(QFileInfo(databasePath_).absolutePath());

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), databaseConnectionName_);
    database_.setDatabaseName(databasePath_);
    if (database_.open())
    {
        ensureSchema();
    }
}

void PortfolioController::ensureSchema()
{
    if (!databaseReady())
    {
        return;
    }

    const QStringList statements{
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS holdings ("
            "id TEXT PRIMARY KEY,"
            "symbol TEXT UNIQUE NOT NULL,"
            "name TEXT NOT NULL,"
            "type TEXT NOT NULL,"
            "institution TEXT NOT NULL,"
            "last_price REAL NOT NULL,"
            "one_hour_change_pct REAL NOT NULL,"
            "ai_score REAL NOT NULL,"
            "suggestion TEXT NOT NULL,"
            "analysis TEXT NOT NULL,"
            "industry_outlook TEXT NOT NULL,"
            "one_hour_trend_json TEXT NOT NULL,"
            "one_month_industry_trend_json TEXT NOT NULL,"
            "updated_at_ms INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS holding_quote_history ("
            "symbol TEXT NOT NULL,"
            "timestamp_ms INTEGER NOT NULL,"
            "price REAL NOT NULL,"
            "PRIMARY KEY (symbol, timestamp_ms))")
    };

    for (const QString& statement : statements)
    {
        QSqlQuery query(database_);
        query.exec(statement);
    }
}

bool PortfolioController::databaseReady() const
{
    return database_.isValid() && database_.isOpen();
}

void PortfolioController::loadInstitutions()
{
    institutionOptions_.clear();

    QSqlDatabase external = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"),
        QStringLiteral("portfolio-market-board-%1").arg(reinterpret_cast<quintptr>(this)));
    external.setDatabaseName(marketBoardDatabasePath_);
    if (external.open())
    {
        QSqlQuery query(external);
        if (query.exec(QStringLiteral("SELECT name FROM domestic_institutions ORDER BY rank ASC")))
        {
            while (query.next())
            {
                institutionOptions_.push_back(query.value(0).toString());
            }
        }
        external.close();
    }
    external = QSqlDatabase();
    QSqlDatabase::removeDatabase(QStringLiteral("portfolio-market-board-%1").arg(reinterpret_cast<quintptr>(this)));

    if (institutionOptions_.isEmpty())
    {
        for (const QString& item : fallback_institutions())
        {
            institutionOptions_.push_back(item);
        }
    }

    emit institutionOptionsChanged();
}

void PortfolioController::loadHoldings()
{
    holdings_.clear();
    quoteStates_.clear();
    quoteHistory_.clear();

    if (databaseReady())
    {
        QSqlQuery query(database_);
        if (query.exec(
                QStringLiteral(
                    "SELECT id, symbol, name, type, institution, last_price, one_hour_change_pct, ai_score, "
                    "suggestion, analysis, industry_outlook, one_hour_trend_json, "
                    "one_month_industry_trend_json, updated_at_ms "
                    "FROM holdings ORDER BY updated_at_ms DESC")))
        {
            while (query.next())
            {
                HoldingEntry entry;
                entry.id = query.value(0).toString();
                entry.symbol = query.value(1).toString();
                entry.name = query.value(2).toString();
                entry.type = query.value(3).toString();
                entry.institution = query.value(4).toString();
                entry.lastPrice = query.value(5).toDouble();
                entry.oneHourChangePct = query.value(6).toDouble();
                entry.aiScore = clamp_score(query.value(7).toDouble());
                entry.suggestion = query.value(8).toString();
                entry.analysis = query.value(9).toString();
                entry.industryOutlook = query.value(10).toString();
                entry.oneHourTrend = json_to_numbers(
                    QJsonDocument::fromJson(query.value(11).toString().toUtf8()).array());
                entry.oneMonthIndustryTrend = json_to_numbers(
                    QJsonDocument::fromJson(query.value(12).toString().toUtf8()).array());
                entry.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(13).toLongLong());
                holdings_.push_back(entry);

                quoteStates_.insert(entry.symbol, QuoteState{
                    entry.lastPrice,
                    static_cast<double>((qHash(entry.symbol) % 11) - 5) * 0.004,
                    0.15 + static_cast<double>(qAbs(qHash(entry.symbol)) % 5) * 0.03,
                    static_cast<double>(qAbs(qHash(entry.symbol)) % 360) / 27.0
                });
                quoteHistory_.insert(
                    entry.symbol,
                    loadQuoteHistory(entry.symbol, current_timestamp_ms() - (60LL * 60LL * 1000LL)));
                if (quoteHistory_[entry.symbol].isEmpty())
                {
                    quoteHistory_[entry.symbol].push_back({current_timestamp_ms(), entry.lastPrice});
                }
            }
        }
    }

    if (holdings_.isEmpty())
    {
        seedCommercialDemoHoldings();
    }

    syncModel();
    if (!holdings_.isEmpty())
    {
        setSelectedIndex(0);
        status_ = QStringLiteral(u"\u5df2\u8f7d\u5165 %1 \u4e2a\u6295\u8d44\u6807\u7684\uff0c\u884c\u60c5\u4e0e\u98ce\u9669\u6307\u6807\u6b63\u5728\u66f4\u65b0")
            .arg(holdings_.size());
        emit statusChanged();
    }
}

void PortfolioController::seedCommercialDemoHoldings()
{
    const QString primaryInstitution = institutionOptions_.isEmpty()
        ? QStringLiteral(u"\u4e13\u4e1a\u6295\u8d44\u7ec4\u5408")
        : institutionOptions_.constFirst().toString();
    const QString secondaryInstitution = institutionOptions_.size() > 1
        ? institutionOptions_.at(1).toString()
        : primaryInstitution;

    holdings_ = {
        make_demo_holding(
            QStringLiteral("AAPL"),
            QStringLiteral("Apple"),
            QStringLiteral(u"\u7f8e\u80a1"),
            primaryInstitution,
            169.42,
            0.48,
            86.8,
            QStringLiteral(u"\u56de\u8c03\u5206\u6279\u5438\u7eb3"),
            QStringLiteral(u"\u670d\u52a1\u6536\u5165\u7a33\u5b9a\uff0c\u73b0\u91d1\u6d41\u8d28\u91cf\u5f3a\uff1b\u9700\u8ddf\u8e2a\u786c\u4ef6\u6362\u673a\u5468\u671f\u4e0e AI \u7aef\u4fa7\u843d\u5730\u901f\u5ea6\u3002"),
            QStringLiteral(u"\u6d88\u8d39\u7535\u5b50\u9f99\u5934\u76c8\u5229\u97e7\u6027\u8f83\u597d\uff0c\u4f30\u503c\u4e0a\u884c\u9700\u65b0\u589e\u957f\u53d9\u4e8b\u9a71\u52a8\u3002"),
            0.08,
            74.0),
        make_demo_holding(
            QStringLiteral("NVDA"),
            QStringLiteral(u"\u82f1\u4f1f\u8fbe"),
            QStringLiteral(u"\u7f8e\u80a1"),
            primaryInstitution,
            912.65,
            1.36,
            91.5,
            QStringLiteral(u"\u6301\u6709\uff0c\u63a7\u5236\u5355\u7968\u4e0a\u9650"),
            QStringLiteral(u"AI \u7b97\u529b\u9700\u6c42\u4ecd\u5f3a\uff0c\u6bdb\u5229\u7387\u548c\u5e93\u5b58\u5468\u8f6c\u662f\u6838\u5fc3\u89c2\u5bdf\u70b9\u3002"),
            QStringLiteral(u"\u534a\u5bfc\u4f53\u666f\u6c14\u5ea6\u9ad8\uff0c\u4f46\u9700\u9632\u6b62\u76c8\u5229\u9884\u671f\u8fc7\u5ea6\u4e00\u81f4\u540e\u7684\u56de\u64a4\u3002"),
            0.18,
            82.0),
        make_demo_holding(
            QStringLiteral("TSLA"),
            QStringLiteral(u"\u7279\u65af\u62c9"),
            QStringLiteral(u"\u7f8e\u80a1"),
            secondaryInstitution,
            174.12,
            -0.72,
            73.4,
            QStringLiteral(u"\u7b49\u5f85\u6bdb\u5229\u4fee\u590d\u4fe1\u53f7"),
            QStringLiteral(u"\u4ea4\u4ed8\u548c\u5355\u8f66\u76c8\u5229\u627f\u538b\uff0c\u673a\u5668\u4eba\u4e0e\u81ea\u52a8\u9a7e\u9a76\u662f\u957f\u671f\u671f\u6743\u3002"),
            QStringLiteral(u"\u65b0\u80fd\u6e90\u8f66\u4ef7\u683c\u7ade\u4e89\u672a\u5b8c\u5168\u7ed3\u675f\uff0c\u77ed\u671f\u66f4\u9002\u5408\u7528\u98ce\u63a7\u4ed3\u4f4d\u53c2\u4e0e\u3002"),
            -0.12,
            61.0),
        make_demo_holding(
            QStringLiteral("QQQ"),
            QStringLiteral(u"\u7eb3\u6307 100 ETF"),
            QStringLiteral("ETF"),
            secondaryInstitution,
            438.76,
            0.34,
            84.2,
            QStringLiteral(u"\u6838\u5fc3\u536b\u661f\u4ed3\u4f4d"),
            QStringLiteral(u"\u79d1\u6280\u6210\u957f\u66b4\u9732\u5206\u6563\uff0c\u9002\u5408\u627f\u63a5\u7ec4\u5408\u7684\u7f8e\u80a1 Beta\u3002"),
            QStringLiteral(u"\u5927\u578b\u79d1\u6280\u4ecd\u662f\u76c8\u5229\u8d28\u91cf\u4e3b\u7ebf\uff0c\u5229\u7387\u9884\u671f\u53d8\u5316\u4f1a\u653e\u5927\u6ce2\u52a8\u3002"),
            0.05,
            78.0),
        make_demo_holding(
            QStringLiteral("510300.SH"),
            QStringLiteral(u"\u6caa\u6df1 300 ETF"),
            QStringLiteral("ETF"),
            primaryInstitution,
            3.58,
            0.21,
            79.6,
            QStringLiteral(u"\u5de6\u4fa7\u5206\u6279\u5e03\u5c40"),
            QStringLiteral(u"\u4f30\u503c\u5904\u4e8e\u5386\u53f2\u8f83\u4f4e\u533a\u95f4\uff0c\u5206\u7ea2\u7387\u548c ROE \u7a33\u5b9a\u6027\u9700\u6301\u7eed\u9a8c\u8bc1\u3002"),
            QStringLiteral(u"\u5bbd\u57fa\u9002\u5408\u4f5c\u4e3a\u4f4e\u4f30\u503c\u5e95\u4ed3\uff0c\u53cd\u5f39\u5f3a\u5ea6\u53d6\u51b3\u4e8e\u76c8\u5229\u9884\u671f\u4fee\u590d\u3002"),
            0.02,
            67.0)
    };

    const qint64 nowMs = current_timestamp_ms();
    for (HoldingEntry& entry : holdings_)
    {
        quoteStates_.insert(entry.symbol, QuoteState{
            entry.lastPrice,
            static_cast<double>((qHash(entry.symbol) % 11) - 5) * 0.004,
            0.12 + static_cast<double>(qAbs(qHash(entry.symbol)) % 5) * 0.025,
            static_cast<double>(qAbs(qHash(entry.symbol)) % 360) / 27.0
        });
        quoteHistory_.insert(entry.symbol, QVector<QPair<qint64, double>>{{nowMs, entry.lastPrice}});
        saveHolding(entry);
        saveQuote(entry.symbol, entry.lastPrice, nowMs);
    }
}

void PortfolioController::saveHolding(const HoldingEntry& entry)
{
    if (!databaseReady())
    {
        return;
    }

    QSqlQuery query(database_);
    query.prepare(
        QStringLiteral(
            "REPLACE INTO holdings "
            "(id, symbol, name, type, institution, last_price, one_hour_change_pct, ai_score, suggestion, "
            "analysis, industry_outlook, one_hour_trend_json, one_month_industry_trend_json, updated_at_ms) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(entry.id);
    query.addBindValue(entry.symbol);
    query.addBindValue(entry.name);
    query.addBindValue(entry.type);
    query.addBindValue(entry.institution);
    query.addBindValue(entry.lastPrice);
    query.addBindValue(entry.oneHourChangePct);
    query.addBindValue(clamp_score(entry.aiScore));
    query.addBindValue(entry.suggestion);
    query.addBindValue(entry.analysis);
    query.addBindValue(entry.industryOutlook);
    query.addBindValue(QString::fromUtf8(QJsonDocument(numbers_to_json(entry.oneHourTrend)).toJson(QJsonDocument::Compact)));
    query.addBindValue(QString::fromUtf8(QJsonDocument(numbers_to_json(entry.oneMonthIndustryTrend)).toJson(QJsonDocument::Compact)));
    query.addBindValue(entry.updatedAt.toMSecsSinceEpoch());
    query.exec();
}

void PortfolioController::saveQuote(const QString& symbol, double price, qint64 timestampMs)
{
    if (!databaseReady())
    {
        return;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO holding_quote_history (symbol, timestamp_ms, price) VALUES (?, ?, ?)"));
    query.addBindValue(symbol);
    query.addBindValue(timestampMs);
    query.addBindValue(price);
    query.exec();

    QSqlQuery prune(database_);
    prune.prepare(QStringLiteral("DELETE FROM holding_quote_history WHERE symbol = ? AND timestamp_ms < ?"));
    prune.addBindValue(symbol);
    prune.addBindValue(timestampMs - (2LL * 60LL * 60LL * 1000LL));
    prune.exec();
}

QVector<QPair<qint64, double>> PortfolioController::loadQuoteHistory(const QString& symbol, qint64 cutoffTimestampMs) const
{
    QVector<QPair<qint64, double>> values;
    if (!databaseReady())
    {
        return values;
    }

    QSqlQuery query(database_);
    query.prepare(
        QStringLiteral(
            "SELECT timestamp_ms, price FROM holding_quote_history "
            "WHERE symbol = ? AND timestamp_ms >= ? ORDER BY timestamp_ms ASC"));
    query.addBindValue(symbol);
    query.addBindValue(cutoffTimestampMs);
    if (query.exec())
    {
        while (query.next())
        {
            values.push_back({query.value(0).toLongLong(), query.value(1).toDouble()});
        }
    }
    return values;
}

void PortfolioController::syncModel()
{
    holdingsModel_.setEntries(holdings_);
    emit selectedHoldingChanged();
}

void PortfolioController::updateQuotes()
{
    if (!realtimeActive_)
    {
        return;
    }

    const qint64 nowMs = current_timestamp_ms();
    const bool shouldPersistQuotes =
        lastQuotePersistMs_ <= 0 ||
        nowMs - lastQuotePersistMs_ >= 15000;
    for (HoldingEntry& entry : holdings_)
    {
        QuoteState state = quoteStates_.value(entry.symbol);
        if (state.lastPrice <= 0.0)
        {
            state.lastPrice = entry.lastPrice > 0.0 ? entry.lastPrice : 100.0;
        }

        state.phase += 0.3;
        const double oscillator = std::sin((static_cast<double>(nowMs) / 1000.0 + state.phase) / 8.0)
            * state.volatilityPct * 0.35;
        const double noise = (QRandomGenerator::global()->generateDouble() - 0.5) * state.volatilityPct;
        const double pctMove = state.driftPct + oscillator + noise;
        state.lastPrice = std::max(0.01, state.lastPrice * (1.0 + pctMove / 100.0));

        entry.lastPrice = state.lastPrice;
        entry.updatedAt = QDateTime::fromMSecsSinceEpoch(nowMs);
        quoteStates_.insert(entry.symbol, state);

        QVector<QPair<qint64, double>>& history = quoteHistory_[entry.symbol];
        history.push_back({nowMs, entry.lastPrice});
        while (!history.isEmpty() && history.first().first < nowMs - (60LL * 60LL * 1000LL))
        {
            history.remove(0);
        }

        const double basePrice = history.isEmpty() ? entry.lastPrice : history.first().second;
        entry.oneHourChangePct = std::abs(basePrice) < 0.0001
            ? 0.0
            : ((entry.lastPrice - basePrice) / basePrice) * 100.0;

        entry.oneHourTrend.clear();
        entry.oneHourTrend.reserve(history.size());
        for (const auto& point : history)
        {
            const double pct = std::abs(basePrice) < 0.0001
                ? 0.0
                : ((point.second - basePrice) / basePrice) * 100.0;
            entry.oneHourTrend.push_back(pct);
        }

        if (shouldPersistQuotes)
        {
            saveHolding(entry);
            saveQuote(entry.symbol, entry.lastPrice, nowMs);
        }
    }

    if (shouldPersistQuotes)
    {
        lastQuotePersistMs_ = nowMs;
    }

    syncModel();
}

void PortfolioController::startConfigSubscription()
{
    std::string error;
    configSubscriber_.start(
        telemetry_.identity().name + "-config",
        [this](const stok::services::common::TextMessage& message)
        {
            const QString payload = QString::fromUtf8(message.payload);
            QMetaObject::invokeMethod(this, [this, payload]()
            {
                handleConfigMessage(payload);
            }, Qt::QueuedConnection);
        },
        {},
        &error);
}

void PortfolioController::handleConfigMessage(const QString& payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8());
    if (!document.isObject())
    {
        return;
    }

    const QJsonObject object = document.object();
    if (object.value("type").toString() != QStringLiteral("config_update"))
    {
        return;
    }

    const QString target = object.value("target").toString().trimmed();
    if (!target.isEmpty() &&
        target != QStringLiteral("portfolio-board") &&
        target != QStringLiteral("*") &&
        target != QString::fromStdString(telemetry_.identity().name))
    {
        return;
    }

    applyConfigValue(
        object.value("key").toString().trimmed(),
        object.value("value").toVariant().toString().trimmed());
}

void PortfolioController::applyConfigValue(const QString& key, const QString& valueText)
{
    bool ok = false;
    const int value = valueText.toInt(&ok);
    if (!ok || value <= 0)
    {
        return;
    }

    if (key == QStringLiteral("update.quotes.intervalMs"))
    {
        quoteIntervalMs_ = value;
        quoteTimer_->setInterval(quoteIntervalMs_);
    }
    else if (key == QStringLiteral("update.analysis.intervalMs"))
    {
        analysisIntervalMs_ = value;
        analysisTimer_->setInterval(analysisIntervalMs_);
    }
}

void PortfolioController::requestAnalysis(const QString& holdingId)
{
    const int index = holdingIndexById(holdingId);
    if (index < 0 || analysisInFlight_)
    {
        return;
    }

    analysisInFlight_ = true;
    analysisBusy_ = true;
    emit analysisBusyChanged();

    const HoldingEntry entry = holdings_.at(index);
    status_ = QStringLiteral(u"\u6b63\u5728\u5206\u6790 %1").arg(entry.name);
    emit statusChanged();

    const QString prompt = QStringLiteral(
        "Today is %1. Analyze one holding candidate for a Chinese trading desktop. "
        "The objective is maximizing forward return while controlling obvious downside risk. "
        "Asset name: %2. Symbol: %3. Type: %4. Institution: %5. "
        "Return concise Mandarin Chinese fields for UI display: AI score, holding analysis, "
        "next-step suggestion, and a 30-point one-month industry trend series.")
        .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
             entry.name,
             entry.symbol,
             entry.type,
             entry.institution);

    runCodexStructuredTask(
        QStringLiteral("portfolio-analysis"),
        prompt,
        holdingAnalysisSchema(),
        [this, holdingId](const QJsonObject& payload)
        {
            analysisInFlight_ = false;
            analysisBusy_ = false;
            emit analysisBusyChanged();

            const int entryIndex = holdingIndexById(holdingId);
            if (entryIndex < 0)
            {
                return;
            }

            HoldingEntry& entry = holdings_[entryIndex];
            entry.aiScore = clamp_score(payload.value("score").toDouble(entry.aiScore));
            entry.suggestion = payload.value("suggestion").toString(entry.suggestion).trimmed();
            entry.analysis = payload.value("analysis").toString(entry.analysis).trimmed();
            entry.industryOutlook = payload.value("industryOutlook").toString(entry.industryOutlook).trimmed();
            entry.oneMonthIndustryTrend = json_to_numbers(payload.value("oneMonthIndustryTrend").toArray());
            if (entry.oneMonthIndustryTrend.isEmpty())
            {
                entry.oneMonthIndustryTrend = fallback_series(30, 78.0, 0.5, 1.2);
            }
            entry.updatedAt = QDateTime::currentDateTime();
            saveHolding(entry);
            syncModel();

            status_ = QStringLiteral(u"%1 \u7684 AI \u5206\u6790\u5df2\u66f4\u65b0").arg(entry.name);
            emit statusChanged();
        },
        [this](const QString& error)
        {
            analysisInFlight_ = false;
            analysisBusy_ = false;
            emit analysisBusyChanged();
            status_ = QStringLiteral(u"AI \u5206\u6790\u5931\u8d25\uff1a%1").arg(error);
            emit statusChanged();
        });
}

int PortfolioController::holdingIndexById(const QString& holdingId) const
{
    for (int index = 0; index < holdings_.size(); ++index)
    {
        if (holdings_[index].id == holdingId)
        {
            return index;
        }
    }
    return -1;
}

int PortfolioController::selectedIndex() const
{
    return selectedHoldingIndex_ >= 0 && selectedHoldingIndex_ < holdings_.size()
        ? selectedHoldingIndex_
        : -1;
}

void PortfolioController::setSelectedIndex(int index)
{
    if (index < 0 || index >= holdings_.size())
    {
        return;
    }

    selectedHoldingIndex_ = index;
    emit selectedHoldingChanged();
}

QVariantList PortfolioController::seriesToVariantList(const QVector<double>& values) const
{
    QVariantList list;
    list.reserve(values.size());
    for (double value : values)
    {
        list.push_back(value);
    }
    return list;
}

QString PortfolioController::resolveCodexCommand() const
{
    if (!codexCommand_.isEmpty())
    {
        return codexCommand_;
    }

    QString executable = QStandardPaths::findExecutable(QStringLiteral("codex.cmd"));
    if (executable.isEmpty())
    {
        executable = QStandardPaths::findExecutable(QStringLiteral("codex"));
    }
    if (executable.isEmpty())
    {
        const QString appData = QString::fromUtf8(qgetenv("APPDATA"));
        if (!appData.isEmpty())
        {
            const QString candidate = QDir(appData).filePath(QStringLiteral("npm/codex.cmd"));
            if (QFileInfo::exists(candidate))
            {
                executable = candidate;
            }
        }
    }
    return executable;
}

void PortfolioController::runCodexStructuredTask(
    const QString& operationName,
    const QString& prompt,
    const QJsonObject& schema,
    const std::function<void(const QJsonObject&)>& onSuccess,
    const std::function<void(const QString&)>& onError)
{
    const QString command = resolveCodexCommand();
    if (command.isEmpty())
    {
        onError(QStringLiteral(u"\u5206\u6790\u5f15\u64ce\u6682\u4e0d\u53ef\u7528"));
        return;
    }

    const QString artifactDirectory = codexArtifactsDirectoryPath();
    QDir().mkpath(artifactDirectory);

    const QString prefix = QStringLiteral("%1-%2")
        .arg(operationName)
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")));
    const QString promptPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-prompt.txt"));
    const QString schemaPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-schema.json"));
    const QString outputPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-output.json"));
    const QString stdoutPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-stdout.log"));
    const QString stderrPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-stderr.log"));

    if (!writeTextArtifact(promptPath, buildCodexPrompt(prompt)) || !writeJsonArtifact(schemaPath, schema))
    {
        onError(QStringLiteral(u"\u65e0\u6cd5\u5199\u5165 Codex \u4efb\u52a1\u6587\u4ef6"));
        return;
    }

    auto* process = new QProcess(this);
    process->setProgram(command);
    process->setWorkingDirectory(codexWorkingDirectory_);
    QStringList arguments{
        QStringLiteral("exec"),
        QStringLiteral("-C"),
        codexWorkingDirectory_,
        QStringLiteral("--skip-git-repo-check"),
        QStringLiteral("--ephemeral"),
        QStringLiteral("-s"),
        QStringLiteral("read-only"),
        QStringLiteral("--output-schema"),
        schemaPath,
        QStringLiteral("-o"),
        outputPath,
        QStringLiteral("--color"),
        QStringLiteral("never")
    };
    if (!codexModel_.isEmpty())
    {
        arguments << QStringLiteral("-m") << codexModel_;
    }
    arguments << QStringLiteral("-");
    process->setArguments(arguments);
    process->setStandardInputFile(promptPath);
    process->setStandardOutputFile(stdoutPath);
    process->setStandardErrorFile(stderrPath);

    const QPointer<QProcess> guard(process);
    QTimer::singleShot(codexTimeoutMs_, this, [guard]()
    {
        if (guard && guard->state() != QProcess::NotRunning)
        {
            guard->setProperty("timedOut", true);
            guard->kill();
        }
    });

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, outputPath, stdoutPath, stderrPath, onSuccess, onError](int exitCode, QProcess::ExitStatus exitStatus)
        {
            const bool timedOut = process->property("timedOut").toBool();
            const QString stdoutText = readTextFile(stdoutPath);
            const QString stderrText = readTextFile(stderrPath);
            process->deleteLater();

            if (timedOut)
            {
                onError(QStringLiteral(u"\u5206\u6790\u8d85\u65f6"));
                return;
            }
            if (exitStatus != QProcess::NormalExit || exitCode != 0)
            {
                onError(summarizeCodexFailure(stdoutText, stderrText));
                return;
            }

            QFile file(outputPath);
            if (!file.open(QIODevice::ReadOnly))
            {
                onError(QStringLiteral(u"\u672a\u751f\u6210\u5206\u6790\u7ed3\u679c"));
                return;
            }

            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            if (!document.isObject())
            {
                onError(QStringLiteral(u"\u5206\u6790\u7ed3\u679c\u89e3\u6790\u5931\u8d25"));
                return;
            }

            onSuccess(document.object());
        });

    process->start();
    if (!process->waitForStarted(3000))
    {
        const QString error = process->errorString();
        process->deleteLater();
        onError(error);
    }
}

QString PortfolioController::buildCodexPrompt(const QString& prompt) const
{
    return QStringLiteral(
        "You generate structured portfolio-analysis data for a Chinese desktop trading app.\n"
        "Return JSON only. No markdown, no citations, no extra keys.\n"
        "Keep every string concise and directly usable in a UI.\n\n"
        "Task:\n%1\n")
        .arg(prompt.trimmed());
}

QString PortfolioController::codexArtifactsDirectoryPath() const
{
    return QDir(cacheDirectoryPath_).filePath(QStringLiteral("codex-artifacts"));
}

bool PortfolioController::writeTextArtifact(const QString& path, const QString& text) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    file.write(text.toUtf8());
    return file.commit();
}

bool PortfolioController::writeJsonArtifact(const QString& path, const QJsonObject& object) const
{
    return writeTextArtifact(path, QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)));
}

QString PortfolioController::readTextFile(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString PortfolioController::summarizeCodexFailure(const QString& stdoutText, const QString& stderrText) const
{
    const QStringList lines = (stderrText + QStringLiteral("\n") + stdoutText)
        .split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QStringLiteral(u"\u5206\u6790\u5931\u8d25") : lines.constLast();
}

QJsonObject PortfolioController::holdingAnalysisSchema() const
{
    return QJsonObject{
        {"type", QStringLiteral("object")},
        {"properties", QJsonObject{
            {"score", QJsonObject{
                {"type", QStringLiteral("number")},
                {"minimum", 0},
                {"maximum", 100}
            }},
            {"suggestion", QJsonObject{{"type", QStringLiteral("string")}}},
            {"analysis", QJsonObject{{"type", QStringLiteral("string")}}},
            {"industryOutlook", QJsonObject{{"type", QStringLiteral("string")}}},
            {"oneMonthIndustryTrend", QJsonObject{
                {"type", QStringLiteral("array")},
                {"items", QJsonObject{{"type", QStringLiteral("number")}}},
                {"minItems", 30},
                {"maxItems", 30}
            }}
        }},
        {"required", QJsonArray{
            QStringLiteral("score"),
            QStringLiteral("suggestion"),
            QStringLiteral("analysis"),
            QStringLiteral("industryOutlook"),
            QStringLiteral("oneMonthIndustryTrend")
        }},
        {"additionalProperties", false}
    };
}
