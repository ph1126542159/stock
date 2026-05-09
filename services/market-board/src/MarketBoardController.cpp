#include "MarketBoardController.h"

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include "QCoreApplication"
#include "QDate"
#include "QDateTime"
#include "QDir"
#include "QFile"
#include "QFileInfo"
#include "QJsonArray"
#include "QJsonDocument"
#include "QJsonObject"
#include "QJsonParseError"
#include "QNetworkAccessManager"
#include "QNetworkReply"
#include "QNetworkRequest"
#include "QPointer"
#include "QProcess"
#include "QRegularExpression"
#include "QSaveFile"
#include "QSqlError"
#include "QSqlQuery"
#include "QSqlRecord"
#include "QStandardPaths"
#include "QStringConverter"
#include "QTimer"
#include "QUrl"
#include "QVariantMap"
#include "stok/services/common/ServiceConfig.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {

struct UsCatalogSeed
{
    QString symbol;
    QString name;
    QString market;
    double price = 0.0;
    QStringList aliases;
};

QString normalized_id(const QString& raw)
{
    QString normalized;
    normalized.reserve(raw.size());
    for (const QChar ch : raw)
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
    if (normalized.isEmpty())
    {
        normalized = QStringLiteral("item-%1").arg(qHash(raw));
    }
    return normalized;
}

QString normalized_symbol(const QString& raw)
{
    QString symbol;
    symbol.reserve(raw.size());
    for (const QChar ch : raw.trimmed())
    {
        if (ch.isLetterOrNumber() || ch == '.' || ch == '-')
        {
            symbol.append(ch.toUpper());
        }
    }

    if (symbol.isEmpty())
    {
        symbol = QStringLiteral("CUS%1").arg(static_cast<qulonglong>(qHash(raw)));
    }
    return symbol;
}

qint64 current_timestamp_ms()
{
    return QDateTime::currentDateTime().toMSecsSinceEpoch();
}

QVector<double> fallback_year_series(double base, double drift)
{
    QVector<double> values;
    values.reserve(24);
    double current = base;
    for (int index = 0; index < 24; ++index)
    {
        current += drift + std::sin(static_cast<double>(index) / 2.8) * 0.9;
        values.push_back(current);
    }
    return values;
}

QVector<double> fallback_hour_series(double start, double drift)
{
    QVector<double> values;
    values.reserve(12);
    double current = start;
    for (int index = 0; index < 12; ++index)
    {
        current += drift + std::sin(static_cast<double>(index) / 1.7) * 0.12;
        values.push_back(current);
    }
    return values;
}

QVector<double> sampled_price_history(const QVector<QPair<qint64, double>>& history, double fallbackPrice)
{
    QVector<double> values;
    if (history.isEmpty())
    {
        values.push_back(fallbackPrice);
        values.push_back(fallbackPrice);
        return values;
    }

    // Show the most recent 32 points so the seed's 1-minute granularity is
    // preserved end-to-end. Earlier code stride-sampled the entire buffer
    // which, after a few hours of identical-price ticks, produced a curve
    // dominated by the trailing flat tail.
    constexpr int targetPoints = 32;
    const int historySize = static_cast<int>(history.size());
    const int start = std::max(0, historySize - targetPoints);
    for (int index = start; index < historySize; ++index)
    {
        values.push_back(history.at(index).second);
    }
    if (values.isEmpty() || !qFuzzyCompare(values.last(), history.last().second))
    {
        values.push_back(history.last().second);
    }
    return values;
}

QString decode_tencent_payload(const QByteArray& payload)
{
    // Tencent's qt.gtimg.cn endpoint serves GBK-encoded text (no charset
    // header). Qt 6's built-in QStringConverter only knows UTF-8/UTF-16/
    // UTF-32/Latin1/System, so encodingForName("GB18030") returns nullopt
    // and the previous fromLocal8Bit() fallback ended up reading the bytes
    // as either UTF-8 (truncating CJK) or Latin-1 (mojibake like "Æ»¹û").
    //
    // Decode explicitly via the Win32 MultiByteToWideChar API with code page
    // 936 (GBK) which is the right codec for Tencent's payloads.
#if defined(Q_OS_WIN)
    if (payload.isEmpty())
    {
        return {};
    }
    const int wideLength = MultiByteToWideChar(
        936,
        0,
        payload.constData(),
        payload.size(),
        nullptr,
        0);
    if (wideLength > 0)
    {
        QString result(wideLength, QChar(0));
        const int converted = MultiByteToWideChar(
            936,
            0,
            payload.constData(),
            payload.size(),
            reinterpret_cast<wchar_t*>(result.data()),
            wideLength);
        if (converted == wideLength)
        {
            return result;
        }
    }
#endif
    const auto encoding = QStringConverter::encodingForName("GB18030");
    if (encoding.has_value())
    {
        QStringDecoder decoder(*encoding);
        return decoder.decode(payload);
    }
    return QString::fromLocal8Bit(payload);
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

ValueAssetEntry asset_from_object(const QJsonObject& object)
{
    ValueAssetEntry entry;
    entry.id = object.value("id").toString().trimmed();
    entry.rank = object.value("rank").toInt();
    entry.name = object.value("name").toString().trimmed();
    entry.code = object.value("code").toString().trimmed();
    entry.provider = object.value("provider").toString().trimmed();
    entry.category = object.value("category").toString().trimmed();
    entry.score = object.value("score").toDouble();
    entry.latestPrice = object.value("latestPrice").toDouble();
    entry.oneYearReturnPct = object.value("oneYearReturnPct").toDouble();
    entry.investmentAnalysis = object.value("investmentAnalysis").toString().trimmed();
    entry.sixMonthForecast = object.value("sixMonthForecast").toString().trimmed();
    entry.oneYearTrend = json_to_numbers(object.value("oneYearTrend").toArray());
    entry.oneHourDrawdown = json_to_numbers(object.value("oneHourDrawdown").toArray());
    return entry;
}

QJsonObject asset_to_object(const ValueAssetEntry& entry)
{
    return {
        {"id", entry.id},
        {"rank", entry.rank},
        {"name", entry.name},
        {"code", entry.code},
        {"provider", entry.provider},
        {"category", entry.category},
        {"score", entry.score},
        {"latestPrice", entry.latestPrice},
        {"oneYearReturnPct", entry.oneYearReturnPct},
        {"investmentAnalysis", entry.investmentAnalysis},
        {"sixMonthForecast", entry.sixMonthForecast},
        {"oneYearTrend", numbers_to_json(entry.oneYearTrend)},
        {"oneHourDrawdown", numbers_to_json(entry.oneHourDrawdown)}
    };
}

QVector<UsCatalogSeed> default_us_catalog()
{
    return {
        {QStringLiteral("NDX"), QStringLiteral(u"\u7eb3\u6307"), QStringLiteral("NASDAQ 100"), 18240.0, {QStringLiteral("NASDAQ"), QStringLiteral("NASDAQ100"), QStringLiteral(u"\u7eb3\u65af\u8fbe\u514b"), QStringLiteral(u"\u7eb3\u6307"), QStringLiteral("IXIC")}},
        {QStringLiteral("SPX"), QStringLiteral(u"\u6807\u666e500"), QStringLiteral("S&P 500"), 5268.0, {QStringLiteral("SP500"), QStringLiteral("S&P500"), QStringLiteral(u"\u6807\u666e"), QStringLiteral(u"\u6807\u666e500"), QStringLiteral("INX")}},
        {QStringLiteral("DJI"), QStringLiteral(u"\u9053\u743c\u65af"), QStringLiteral("Dow Jones"), 39320.0, {QStringLiteral("DOW"), QStringLiteral("DOWJONES"), QStringLiteral(u"\u9053\u743c\u65af"), QStringLiteral(u"\u9053\u743c\u65af\u5de5\u4e1a"), QStringLiteral("DJIA")}},
        {QStringLiteral("AAPL"), QStringLiteral("Apple"), QStringLiteral("NASDAQ"), 201.24, {QStringLiteral("APPLE"), QStringLiteral(u"\u82f9\u679c")}},
        {QStringLiteral("MSFT"), QStringLiteral("Microsoft"), QStringLiteral("NASDAQ"), 428.31, {QStringLiteral("MICROSOFT"), QStringLiteral(u"\u5fae\u8f6f")}},
        {QStringLiteral("NVDA"), QStringLiteral("NVIDIA"), QStringLiteral("NASDAQ"), 104.52, {QStringLiteral("NVIDIA"), QStringLiteral(u"\u82f1\u4f1f\u8fbe")}},
        {QStringLiteral("AMZN"), QStringLiteral("Amazon"), QStringLiteral("NASDAQ"), 186.14, {QStringLiteral("AMAZON"), QStringLiteral(u"\u4e9a\u9a6c\u900a")}},
        {QStringLiteral("META"), QStringLiteral("Meta"), QStringLiteral("NASDAQ"), 489.62, {QStringLiteral("FACEBOOK"), QStringLiteral(u"\u8138\u4e66"), QStringLiteral(u"\u5143\u5b87\u5b99")}},
        {QStringLiteral("GOOGL"), QStringLiteral("Alphabet"), QStringLiteral("NASDAQ"), 176.58, {QStringLiteral("GOOGLE"), QStringLiteral(u"\u8c37\u6b4c"), QStringLiteral("ALPHABET")}},
        {QStringLiteral("TSLA"), QStringLiteral("Tesla"), QStringLiteral("NASDAQ"), 172.83, {QStringLiteral("TESLA"), QStringLiteral(u"\u7279\u65af\u62c9")}}
    };
}

QString query_error_text(const QSqlQuery& query)
{
    return query.lastError().text().trimmed();
}

QString resolve_free_data_provider_script(const QString& configPath, const std::string& configuredPath)
{
    const QString configured = QString::fromStdString(configuredPath).trimmed();
    const QString resolved = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(configPath.toStdString(), configuredPath));
    if (QFileInfo::exists(resolved))
    {
        return resolved;
    }

    QStringList candidates;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString configDir = QFileInfo(configPath).absolutePath();
    candidates << QDir(configDir).filePath(configured)
               << QDir(appDir).filePath(configured)
               << QDir(appDir).filePath(QStringLiteral("../tools/free_data_provider.py"))
               << QDir(appDir).filePath(QStringLiteral("../../tools/free_data_provider.py"))
               << QDir(appDir).filePath(QStringLiteral("../../../tools/free_data_provider.py"))
               << QDir(QDir::currentPath()).filePath(QStringLiteral("tools/free_data_provider.py"))
               << QDir(QDir::currentPath()).filePath(QStringLiteral("../tools/free_data_provider.py"));

    for (const QString& candidate : candidates)
    {
        const QString cleanPath = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanPath))
        {
            return cleanPath;
        }
    }

    return resolved;
}

// Resolve which python interpreter to use, preferring a bundled embeddable
// Python so end users don't need to install Python or AKShare separately.
//
// The bundled interpreter is laid out by CMake at:
//   <appDir>/python-embed/python.exe
// (during dev:   build-vs2022/bin/python-embed/python.exe)
//
// Fallback chain:
//   1. If the configured command is an absolute or relative path that exists, use it as-is.
//   2. If the configured command is "py" or "python" or empty, look for the bundled python-embed.
//   3. Otherwise, return the configured command verbatim and let QProcess search PATH.
QString resolve_python_command(const QString& configuredCommand)
{
    const QString trimmed = configuredCommand.trimmed();
    const QString appDir = QCoreApplication::applicationDirPath();

    // Try the bundled embeddable interpreter first when no specific override
    // is given. This is what makes the installer self-contained.
    if (trimmed.isEmpty() || trimmed == QStringLiteral("py") ||
        trimmed == QStringLiteral("python") || trimmed == QStringLiteral("python3") ||
        trimmed == QStringLiteral("python.exe"))
    {
        QStringList bundledCandidates;
        bundledCandidates
            << QDir(appDir).filePath(QStringLiteral("python-embed/python.exe"))
            << QDir(appDir).filePath(QStringLiteral("../python-embed/python.exe"))
            << QDir(appDir).filePath(QStringLiteral("../../python-embed/python.exe"))
            << QDir(appDir).filePath(QStringLiteral("../../../python-embed/python.exe"));
        for (const QString& candidate : bundledCandidates)
        {
            const QString cleanPath = QDir::cleanPath(candidate);
            if (QFileInfo::exists(cleanPath))
            {
                return cleanPath;
            }
        }
    }

    // Configured value pointing at an explicit file path on disk (absolute
    // or relative to the current dir) that exists -- honour it.
    if (!trimmed.isEmpty())
    {
        const QString cleanPath = QDir::cleanPath(trimmed);
        if (QFileInfo::exists(cleanPath))
        {
            return cleanPath;
        }
    }

    // Last resort: leave the configured command alone (PATH lookup).
    return trimmed.isEmpty() ? QStringLiteral("py") : trimmed;
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

} // namespace

MarketBoardController::MarketBoardController(
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
    codexCommand_ = QString::fromStdString(configuration_->getString("codex.command", "")).trimmed();
    codexModel_ = QString::fromStdString(configuration_->getString("codex.model", "")).trimmed();
    codexWorkingDirectory_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("codex.workDir", ".")));
    freeDataProviderCommand_ = resolve_python_command(
        QString::fromStdString(configuration_->getString("freeData.provider.command", "py")));
    freeDataProviderScriptPath_ = resolve_free_data_provider_script(
        configPath_,
        configuration_->getString("freeData.provider.script", "../../tools/free_data_provider.py"));
    valueBoardProviderCommand_ = resolve_python_command(
        QString::fromStdString(configuration_->getString("valueBoard.provider.command", "py")));
    valueBoardProviderScriptPath_ = resolve_free_data_provider_script(
        configPath_,
        configuration_->getString("valueBoard.provider.script", "../../tools/value_board_provider.py"));
    valueBoardProviderTimeoutMs_ = configuration_->getInt("valueBoard.provider.timeoutMs", 5 * 60 * 1000);
    usHistoryProviderScriptPath_ = resolve_free_data_provider_script(
        configPath_,
        configuration_->getString("usHistory.provider.script", "../../tools/us_history_provider.py"));
    fundHoldingsScriptPath_ = resolve_free_data_provider_script(
        configPath_,
        configuration_->getString("fundHoldings.provider.script", "../../tools/fund_holdings_provider.py"));
    telemetry_.logger().information(
        QStringLiteral("Python interpreter: %1; value-board script: %2; free-data script: %3")
            .arg(valueBoardProviderCommand_, valueBoardProviderScriptPath_, freeDataProviderScriptPath_)
            .toStdString());
    codexTimeoutMs_ = configuration_->getInt("codex.timeoutMs", 3 * 60 * 1000);
    institutionRefreshIntervalMs_ =
        configuration_->getInt("update.institutions.intervalMs", 60 * 60 * 1000);
    usMarketRefreshIntervalMs_ =
        configuration_->getInt("update.usMarket.intervalMs", 1000);
    valueRefreshIntervalMs_ =
        configuration_->getInt("update.valueBoard.intervalMs", 5 * 60 * 1000);
    freeDataRefreshIntervalMs_ =
        configuration_->getInt("update.freeData.intervalMs", 5 * 60 * 1000);
    cacheDirectoryPath_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("storage.cacheDir", "data/market-board")));
    databasePath_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("storage.sqlitePath", "data/market-board/market-board.db")));
    databaseConnectionName_ = QStringLiteral("market-board-%1")
        .arg(reinterpret_cast<quintptr>(this));

    initializeDatabase();

    institutionRefreshTimer_ = new QTimer(this);
    institutionRefreshTimer_->setInterval(institutionRefreshIntervalMs_);
    connect(institutionRefreshTimer_, &QTimer::timeout, this, &MarketBoardController::refreshInstitutions);

    usMarketTimer_ = new QTimer(this);
    usMarketTimer_->setInterval(usMarketRefreshIntervalMs_);
    connect(usMarketTimer_, &QTimer::timeout, this, [this]()
    {
        updateUsMarketBoard();
    });

    valueRefreshTimer_ = new QTimer(this);
    valueRefreshTimer_->setInterval(valueRefreshIntervalMs_);
    connect(valueRefreshTimer_, &QTimer::timeout, this, &MarketBoardController::refreshValueBoard);

    selectedAssetRealtimeTimer_ = new QTimer(this);
    selectedAssetRealtimeTimer_->setInterval(1000);
    connect(selectedAssetRealtimeTimer_, &QTimer::timeout, this, &MarketBoardController::tickSelectedAssetRealtime);

    freeDataRefreshTimer_ = new QTimer(this);
    freeDataRefreshTimer_->setInterval(freeDataRefreshIntervalMs_);
    connect(freeDataRefreshTimer_, &QTimer::timeout, this, &MarketBoardController::refreshFreeDataProvider);

    networkAccess_ = new QNetworkAccessManager(this);
}

MarketBoardController::~MarketBoardController()
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

InstitutionRankingModel* MarketBoardController::institutionModel()
{
    return &institutionModel_;
}

UsMarketWatchModel* MarketBoardController::usMarketModel()
{
    return &usMarketModel_;
}

InvestmentOpportunityModel* MarketBoardController::fundModel()
{
    return &fundModel_;
}

InvestmentOpportunityModel* MarketBoardController::stockModel()
{
    return &stockModel_;
}

int MarketBoardController::currentPage() const
{
    return currentPage_;
}

QString MarketBoardController::institutionBoardTitle() const
{
    return QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c");
}

QString MarketBoardController::valueBoardTitle() const
{
    return QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c");
}

QString MarketBoardController::selectedInstitutionTitle() const
{
    return selectedInstitutionName_;
}

QString MarketBoardController::institutionStatus() const
{
    return institutionStatus_;
}

QString MarketBoardController::usMarketStatus() const
{
    return usMarketStatus_;
}

QString MarketBoardController::valueStatus() const
{
    return valueStatus_;
}

QString MarketBoardController::selectedAssetName() const
{
    return hasSelectedAsset_ ? selectedAsset_.name : QString();
}

QString MarketBoardController::selectedAssetCode() const
{
    return hasSelectedAsset_ ? selectedAsset_.code : QString();
}

QString MarketBoardController::selectedAssetCategory() const
{
    return hasSelectedAsset_ ? selectedAsset_.category : QString();
}

QString MarketBoardController::selectedAssetProvider() const
{
    return hasSelectedAsset_ ? selectedAsset_.provider : QString();
}

QString MarketBoardController::selectedAssetInvestmentAnalysis() const
{
    return hasSelectedAsset_ ? selectedAsset_.investmentAnalysis : QString();
}

QString MarketBoardController::selectedAssetSixMonthForecast() const
{
    return hasSelectedAsset_ ? selectedAsset_.sixMonthForecast : QString();
}

QString MarketBoardController::selectedAssetKind() const
{
    return hasSelectedAsset_ ? selectedAssetKind_ : QString();
}

double MarketBoardController::selectedAssetScore() const
{
    return hasSelectedAsset_ ? selectedAsset_.score : 0.0;
}

double MarketBoardController::selectedAssetLatestPrice() const
{
    return hasSelectedAsset_ ? selectedAsset_.latestPrice : 0.0;
}

double MarketBoardController::selectedAssetOneYearReturn() const
{
    return hasSelectedAsset_ ? selectedAsset_.oneYearReturnPct : 0.0;
}

QVariantList MarketBoardController::selectedAssetOneYearTrend() const
{
    return hasSelectedAsset_ ? seriesToVariantList(selectedAsset_.oneYearTrend) : QVariantList{};
}

QVariantList MarketBoardController::selectedAssetOneHourDrawdown() const
{
    return hasSelectedAsset_ ? seriesToVariantList(selectedAsset_.oneHourDrawdown) : QVariantList{};
}

QVariantList MarketBoardController::selectedFundConfigCards() const
{
    if (!hasSelectedAsset_ || selectedAssetKind_ != QStringLiteral("fund"))
    {
        return {};
    }

    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u57fa\u91d1\u7c7b\u578b")}, {QStringLiteral("value"), selectedAsset_.category}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u7ba1\u7406\u4eba")}, {QStringLiteral("value"), selectedAsset_.provider}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u6700\u65b0\u51c0\u503c")}, {QStringLiteral("value"), QString::number(selectedAsset_.latestPrice, 'f', selectedAsset_.latestPrice >= 1000.0 ? 2 : 3)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u8fd11\u5e74\u6536\u76ca")}, {QStringLiteral("value"), QStringLiteral("%1%").arg(QString::number(selectedAsset_.oneYearReturnPct, 'f', 1))}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u4ef7\u503c\u5206")}, {QStringLiteral("value"), QString::number(selectedAsset_.score, 'f', 1)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u6301\u4ed3\u6570\u636e")}, {QStringLiteral("value"), QStringLiteral(u"\u5f85\u63a5\u5165\u5b63\u62a5\u6e90")}}
    };
}

QVariantList MarketBoardController::selectedFundHoldingCards() const
{
    if (!hasSelectedAsset_ || selectedAssetKind_ != QStringLiteral("fund"))
    {
        return {};
    }

    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u80a1\u7968\u4ed3\u4f4d")}, {QStringLiteral("value"), QStringLiteral(u"\u6682\u65e0\u771f\u5b9e\u5b63\u62a5\u6570\u636e")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u524d\u5341\u6301\u4ed3")}, {QStringLiteral("value"), QStringLiteral(u"\u7b49\u5f85\u57fa\u91d1\u6301\u4ed3\u63a5\u53e3")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u6362\u624b\u72b6\u6001")}, {QStringLiteral("value"), QStringLiteral(u"\u5f85\u8ba1\u7b97")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral(u"\u98ce\u683c\u66b4\u9732")}, {QStringLiteral("value"), selectedAsset_.category}}
    };
}

QVariantList MarketBoardController::selectedFundTopHoldings() const
{
    if (!hasSelectedAsset_ || selectedAssetKind_ != QStringLiteral("fund"))
    {
        return {};
    }

    return {};
}

QVariantMap MarketBoardController::freeDataSnapshot() const
{
    return freeDataSnapshot_;
}

QString MarketBoardController::freeDataStatus() const
{
    return freeDataStatus_;
}

QVariantMap MarketBoardController::fundHoldingsSnapshot() const
{
    return fundHoldingsSnapshot_;
}

QString MarketBoardController::fundHoldingsStatus() const
{
    return fundHoldingsStatus_;
}

void MarketBoardController::requestFundHoldings(const QString& fundCode)
{
    const QString trimmed = fundCode.trimmed();
    if (trimmed.isEmpty())
    {
        return;
    }
    // Skip if we already have this code's data fresh in memory.
    if (fundHoldingsSnapshot_.value(QStringLiteral("fundCode")).toString() == trimmed &&
        fundHoldingsSnapshot_.value(QStringLiteral("topHoldings")).toList().size() > 0)
    {
        return;
    }
    if (fundHoldingsRequestInFlight_ && fundHoldingsRequestedCode_ == trimmed)
    {
        return;
    }
    if (valueBoardProviderCommand_.isEmpty() || fundHoldingsScriptPath_.isEmpty())
    {
        fundHoldingsStatus_ = QStringLiteral(u"基金穿透：未配置抓取脚本");
        emit fundHoldingsChanged();
        return;
    }
    if (!QFileInfo::exists(fundHoldingsScriptPath_))
    {
        fundHoldingsStatus_ = QStringLiteral(u"基金穿透：脚本未找到 %1").arg(fundHoldingsScriptPath_);
        emit fundHoldingsChanged();
        return;
    }

    fundHoldingsRequestInFlight_ = true;
    fundHoldingsRequestedCode_ = trimmed;
    fundHoldingsStatus_ = QStringLiteral(u"基金穿透：抓取 %1 重仓股 ...").arg(trimmed);
    emit fundHoldingsChanged();

    auto* process = new QProcess(this);
    QStringList args;
    if (valueBoardProviderCommand_ == QStringLiteral("py")) args << QStringLiteral("-3");
    args << fundHoldingsScriptPath_
         << QStringLiteral("--fund") << trimmed
         << QStringLiteral("--json");
    process->setWorkingDirectory(QFileInfo(fundHoldingsScriptPath_).absolutePath());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    telemetry_.logger().information(
        QStringLiteral("Fund holdings provider: %1 %2 (fund=%3)")
            .arg(valueBoardProviderCommand_, fundHoldingsScriptPath_, trimmed)
            .toStdString());

    QPointer<MarketBoardController> guard(this);
    connect(process, &QProcess::finished, this,
        [this, guard, process, trimmed](int exitCode, QProcess::ExitStatus)
    {
        const QString out = QString::fromUtf8(process->readAllStandardOutput());
        const QString err = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        if (!guard) return;

        fundHoldingsRequestInFlight_ = false;
        if (exitCode != 0)
        {
            fundHoldingsStatus_ = QStringLiteral(u"基金穿透：抓取失败 %1").arg(err.left(120));
            emit fundHoldingsChanged();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            fundHoldingsStatus_ = QStringLiteral(u"基金穿透：解析失败 %1").arg(parseError.errorString());
            emit fundHoldingsChanged();
            return;
        }
        const QJsonArray funds = doc.object().value(QStringLiteral("funds")).toArray();
        if (funds.isEmpty() || !funds.first().isObject())
        {
            fundHoldingsStatus_ = QStringLiteral(u"基金穿透：返回为空");
            emit fundHoldingsChanged();
            return;
        }
        const QJsonObject snap = funds.first().toObject();
        fundHoldingsSnapshot_ = snap.toVariantMap();
        fundHoldingsStatus_ = QStringLiteral(u"基金穿透：%1（%2）")
            .arg(snap.value(QStringLiteral("fundCode")).toString(),
                 snap.value(QStringLiteral("asOf")).toString());
        emit fundHoldingsChanged();
    });

    process->start(valueBoardProviderCommand_, args);
    if (!process->waitForStarted(3000))
    {
        process->deleteLater();
        fundHoldingsRequestInFlight_ = false;
        fundHoldingsStatus_ = QStringLiteral(u"基金穿透：启动脚本失败");
        emit fundHoldingsChanged();
        return;
    }
    QTimer::singleShot(60 * 1000, process, [process]()
    {
        if (process->state() != QProcess::NotRunning) process->kill();
    });
}

void MarketBoardController::start()
{
    QDir().mkpath(cacheDirectoryPath());
    QDir().mkpath(QFileInfo(databasePath()).absolutePath());

    // Config updates are currently delivered through the UI state; starting the
    // DDS config subscriber here can crash the board during process bootstrap.
    purgeNonRealValueBoardData();
    loadInstitutionBoard();
    loadUsMarketBoard();

    institutionRefreshTimer_->start();
    if (realtimeActive_)
    {
        seedUsHistoryFromProvider();
        updateUsMarketBoard();
        usMarketTimer_->start();
    }

    if (!institutionLoadedFromStorage_ || shouldRefresh(institutionUpdatedAt_, institutionRefreshIntervalMs_))
    {
        refreshInstitutions();
    }

    startFreeDataProvider();
}

void MarketBoardController::setRealtimeActive(bool active)
{
    if (realtimeActive_ == active)
    {
        return;
    }

    realtimeActive_ = active;
    if (!usMarketTimer_ || !selectedAssetRealtimeTimer_)
    {
        return;
    }

    if (realtimeActive_)
    {
        updateUsMarketBoard();
        usMarketTimer_->start();
        return;
    }

    usMarketTimer_->stop();
}

void MarketBoardController::openValueBoard(int institutionRow)
{
    const InstitutionBoardEntry* entry = institutionModel_.entryAt(institutionRow);
    if (!entry)
    {
        return;
    }

    selectedInstitutionId_ = entry->id;
    selectedInstitutionName_ = entry->name;
    emit selectedInstitutionChanged();

    currentPage_ = ValueBoardPage;
    emit currentPageChanged();

    loadValueBoard(selectedInstitutionId_);
    valueRefreshTimer_->start();

    if (!valueLoadedFromStorage_ || shouldRefresh(valueUpdatedAt_, valueRefreshIntervalMs_))
    {
        refreshValueBoard();
    }
}

void MarketBoardController::backToInstitutionBoard()
{
    currentPage_ = InstitutionBoardPage;
    emit currentPageChanged();
    valueRefreshTimer_->stop();
}

void MarketBoardController::selectFund(int row)
{
    setSelectedAsset(fundModel_.entryAt(row), QStringLiteral("fund"));
}

void MarketBoardController::selectStock(int row)
{
    setSelectedAsset(stockModel_.entryAt(row), QStringLiteral("stock"));
}

void MarketBoardController::refreshInstitutions()
{
    requestInstitutionBoard();
}

void MarketBoardController::refreshValueBoard()
{
    if (selectedInstitutionId_.isEmpty())
    {
        return;
    }

    requestValueBoard(selectedInstitutionId_, selectedInstitutionName_);
}

bool MarketBoardController::addUsWatchSymbol(const QString& input)
{
    const UsMarketWatchEntry resolved = resolveUsWatchInput(input);
    if (resolved.symbol.isEmpty())
    {
        usMarketStatus_ = QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u8bf7\u8f93\u5165\u80a1\u7968\u4ee3\u53f7\u6216\u540d\u79f0");
        emit usMarketStatusChanged();
        return false;
    }

    const auto existingIt = std::find_if(
        usWatchEntries_.cbegin(),
        usWatchEntries_.cend(),
        [&resolved](const UsMarketWatchEntry& entry)
        {
            return entry.symbol.compare(resolved.symbol, Qt::CaseInsensitive) == 0;
        });
    if (existingIt != usWatchEntries_.cend())
    {
        usMarketStatus_ = QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a%1 \u5df2\u5728\u81ea\u9009\u5217\u8868").arg(resolved.symbol);
        emit usMarketStatusChanged();
        return false;
    }

    const qint64 nowMs = current_timestamp_ms();
    UsMarketWatchEntry entry = resolved;
    entry.updatedAt = QDateTime::fromMSecsSinceEpoch(nowMs);

    saveUsWatchDefinitionToDatabase(entry, nowMs);
    saveUsQuoteToDatabase(entry.symbol, entry.name, entry.market, entry.lastPrice, nowMs);

    usWatchEntries_.push_back(entry);
    usQuoteHistory_.insert(entry.symbol, QVector<QPair<qint64, double>>{{nowMs, entry.lastPrice}});
    entry.oneHourChangePct = 0.0;
    usWatchEntries_.last() = entry;
    syncUsMarketModel();

    usMarketStatus_ = QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u5df2\u65b0\u589e %1").arg(entry.symbol);
    emit usMarketStatusChanged();
    return true;
}

void MarketBoardController::refreshFreeDataProvider()
{
    if (freeDataRequestInFlight_)
    {
        return;
    }

    if (freeDataProviderCommand_.isEmpty() || freeDataProviderScriptPath_.isEmpty())
    {
        freeDataStatus_ = QStringLiteral(u"免费数据源：未配置本地提供器");
        emit freeDataSnapshotChanged();
        return;
    }

    freeDataRequestInFlight_ = true;
    freeDataStatus_ = QStringLiteral(u"免费数据源：正在刷新");
    emit freeDataSnapshotChanged();

    auto* process = new QProcess(this);
    QStringList arguments;
    // py launcher needs an explicit version flag; bundled python.exe and most
    // other interpreters do not. We only add it for the legacy "py" alias.
    if (freeDataProviderCommand_ == QStringLiteral("py"))
    {
        arguments << QStringLiteral("-3");
    }
    arguments << freeDataProviderScriptPath_ << QStringLiteral("--json");
    process->setWorkingDirectory(QFileInfo(freeDataProviderScriptPath_).absolutePath());
    process->setProcessChannelMode(QProcess::SeparateChannels);
    telemetry_.logger().information(
        QStringLiteral("Refreshing free data provider: %1 %2")
            .arg(freeDataProviderCommand_, arguments.join(QStringLiteral(" ")))
            .toStdString());
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus)
    {
        const QString stdoutText = QString::fromUtf8(process->readAllStandardOutput());
        const QString stderrText = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        handleFreeDataProviderResult(exitCode, stdoutText, stderrText);
    });
    process->start(freeDataProviderCommand_, arguments);
    if (!process->waitForStarted(3000))
    {
        const QString errorText = process->errorString();
        process->deleteLater();
        freeDataRequestInFlight_ = false;
        freeDataStatus_ = QStringLiteral(u"免费数据源：启动失败 %1").arg(errorText);
        telemetry_.logger().warning(freeDataStatus_.toStdString());
        emit freeDataSnapshotChanged();
    }
}

void MarketBoardController::initializeDatabase()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
    {
        telemetry_.logger().warning("QSQLITE driver is unavailable for market-board.");
        return;
    }

    QDir().mkpath(QFileInfo(databasePath_).absolutePath());

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), databaseConnectionName_);
    database_.setDatabaseName(databasePath_);
    if (!database_.open())
    {
        telemetry_.logger().warning(
            QStringLiteral("Failed to open market-board SQLite database at %1: %2")
                .arg(databasePath_, database_.lastError().text())
                .toStdString());
        database_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(databaseConnectionName_);
        return;
    }

    ensureSchema();
}

void MarketBoardController::ensureSchema()
{
    if (!databaseReady())
    {
        return;
    }

    const QStringList statements{
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS domestic_institutions ("
            "id TEXT PRIMARY KEY,"
            "rank INTEGER NOT NULL,"
            "name TEXT NOT NULL,"
            "common_entry TEXT NOT NULL,"
            "core_strength TEXT NOT NULL,"
            "target_audience TEXT NOT NULL,"
            "score REAL NOT NULL,"
            "updated_at TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS value_board_assets ("
            "institution_id TEXT NOT NULL,"
            "institution_name TEXT NOT NULL,"
            "asset_kind TEXT NOT NULL,"
            "id TEXT NOT NULL,"
            "rank INTEGER NOT NULL,"
            "name TEXT NOT NULL,"
            "code TEXT NOT NULL,"
            "provider TEXT NOT NULL,"
            "category TEXT NOT NULL,"
            "score REAL NOT NULL,"
            "latest_price REAL NOT NULL,"
            "one_year_return_pct REAL NOT NULL,"
            "investment_analysis TEXT NOT NULL,"
            "six_month_forecast TEXT NOT NULL,"
            "one_year_trend_json TEXT NOT NULL,"
            "one_hour_drawdown_json TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "PRIMARY KEY (institution_id, asset_kind, id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS us_watchlist ("
            "symbol TEXT PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "market TEXT NOT NULL,"
            "created_at_ms INTEGER NOT NULL,"
            "last_price REAL NOT NULL DEFAULT 0,"
            "updated_at_ms INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS us_quote_history ("
            "symbol TEXT NOT NULL,"
            "timestamp_ms INTEGER NOT NULL,"
            "price REAL NOT NULL,"
            "PRIMARY KEY (symbol, timestamp_ms))"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_us_quote_history_symbol_timestamp "
            "ON us_quote_history(symbol, timestamp_ms)")
    };

    for (const QString& statement : statements)
    {
        QSqlQuery query(database_);
        if (!query.exec(statement))
        {
            telemetry_.logger().warning(
                QStringLiteral("Failed to ensure market-board SQLite schema: %1")
                    .arg(query_error_text(query))
                    .toStdString());
        }
    }
}

bool MarketBoardController::databaseReady() const
{
    return database_.isValid() && database_.isOpen();
}

void MarketBoardController::loadInstitutionBoard()
{
    institutionLoadedFromStorage_ = false;
    const QVector<InstitutionBoardEntry> storedEntries =
        loadInstitutionBoardFromDatabase(&institutionUpdatedAt_);
    if (!storedEntries.isEmpty())
    {
        institutionLoadedFromStorage_ = true;
        applyInstitutionBoard(storedEntries, institutionUpdatedAt_, true);
        return;
    }

    institutionUpdatedAt_ = QDateTime();
    const QVector<InstitutionBoardEntry> fallbackEntries = fallbackInstitutions();
    const QDateTime fallbackUpdatedAt = QDateTime::currentDateTime();
    saveInstitutionBoardToDatabase(fallbackEntries, fallbackUpdatedAt);
    applyInstitutionBoard(fallbackEntries, fallbackUpdatedAt, false);
}

void MarketBoardController::loadValueBoard(const QString& institutionId)
{
    valueLoadedFromStorage_ = false;
    const ValueBoardSnapshot storedSnapshot =
        loadValueBoardFromDatabase(institutionId, &valueUpdatedAt_);
    if (!storedSnapshot.funds.isEmpty() || !storedSnapshot.stocks.isEmpty())
    {
        valueLoadedFromStorage_ = true;
        applyValueBoard(storedSnapshot, valueUpdatedAt_, true);
        return;
    }

    // Empty DB: do NOT seed built-in sample anymore. The user must wait for
    // the python provider to populate real data; otherwise the UI explicitly
    // says "data not yet fetched". Built-in samples used to mislead users.
    valueUpdatedAt_ = QDateTime();
    fundModel_.setEntries({});
    stockModel_.setEntries({});
    valueStatus_ = QStringLiteral(u"价值投资榜：等待真实数据，请稍候（首次抓取需要数十秒）");
    emit valueStatusChanged();
}

void MarketBoardController::purgeNonRealValueBoardData()
{
    // The earlier build seeded value_board_assets with built-in sample data
    // (and previously also LLM-generated rows). Both are no longer trustworthy
    // sources, so on every startup we wipe the table and let the python
    // provider repopulate it from public APIs. SQLite will simply recreate the
    // rows on the next successful refresh.
    if (!databaseReady())
    {
        return;
    }

    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("DELETE FROM value_board_assets")))
    {
        telemetry_.logger().warning(
            QStringLiteral("Failed to clear stale value_board_assets: %1")
                .arg(query_error_text(query))
                .toStdString());
        return;
    }
    telemetry_.logger().information("Cleared stale value board entries on startup.");
}

void MarketBoardController::loadUsMarketBoard()
{
    usWatchEntries_ = loadUsWatchlistFromDatabase();
    const QVector<UsMarketWatchEntry> defaultEntries = defaultUsWatchlist();
    if (usWatchEntries_.isEmpty())
    {
        seedDefaultUsWatchlist();
        usWatchEntries_ = loadUsWatchlistFromDatabase();
    }

    if (usWatchEntries_.isEmpty())
    {
        usWatchEntries_ = defaultEntries;
    }

    for (const UsMarketWatchEntry& defaultEntry : defaultEntries)
    {
        const auto existingIt = std::find_if(
            usWatchEntries_.cbegin(),
            usWatchEntries_.cend(),
            [&defaultEntry](const UsMarketWatchEntry& entry)
            {
                return entry.symbol.compare(defaultEntry.symbol, Qt::CaseInsensitive) == 0;
            });
        if (existingIt == usWatchEntries_.cend())
        {
            usWatchEntries_.push_back(defaultEntry);
            saveUsWatchDefinitionToDatabase(defaultEntry, current_timestamp_ms());
        }
    }

    usQuoteHistory_.clear();

    const qint64 nowMs = current_timestamp_ms();
    const qint64 cutoffMs = nowMs - (60LL * 60LL * 1000LL);
    for (UsMarketWatchEntry& entry : usWatchEntries_)
    {
        if (!entry.updatedAt.isValid())
        {
            entry.updatedAt = QDateTime::fromMSecsSinceEpoch(nowMs);
        }
        if (entry.lastPrice <= 0.0)
        {
            const auto defaultIt = std::find_if(
                defaultEntries.cbegin(),
                defaultEntries.cend(),
                [&entry](const UsMarketWatchEntry& defaultEntry)
                {
                    return defaultEntry.symbol.compare(entry.symbol, Qt::CaseInsensitive) == 0;
                });
            if (defaultIt != defaultEntries.cend())
            {
                entry.lastPrice = defaultIt->lastPrice;
                entry.name = entry.name.isEmpty() ? defaultIt->name : entry.name;
                entry.market = entry.market.isEmpty() ? defaultIt->market : entry.market;
            }
        }

        QVector<QPair<qint64, double>> history = loadUsHistoryFromDatabase(entry.symbol, cutoffMs);
        if (history.isEmpty())
        {
            history.push_back({entry.updatedAt.toMSecsSinceEpoch(), entry.lastPrice});
            saveUsQuoteToDatabase(
                entry.symbol,
                entry.name,
                entry.market,
                entry.lastPrice,
                entry.updatedAt.toMSecsSinceEpoch());
        }

        usQuoteHistory_.insert(entry.symbol, history);
        entry.oneHourChangePct = computeUsOneHourChangePct(entry.symbol, entry.lastPrice, nowMs);
    }

    syncUsMarketModel();
}

void MarketBoardController::applyInstitutionBoard(
    const QVector<InstitutionBoardEntry>& entries,
    const QDateTime& updatedAt,
    bool persisted)
{
    institutionModel_.setEntries(entries);
    institutionUpdatedAt_ = updatedAt;
    const QString prefix = persisted
        ? QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c")
        : QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff08\u5185\u7f6e\u793a\u4f8b\uff09");
    institutionStatus_ = institutionStatusFromTimestamp(prefix, updatedAt.isValid() ? updatedAt : QDateTime::currentDateTime());
    emit institutionStatusChanged();
}

void MarketBoardController::applyValueBoard(
    const ValueBoardSnapshot& snapshot,
    const QDateTime& updatedAt,
    bool persisted)
{
    fundModel_.setEntries(snapshot.funds);
    stockModel_.setEntries(snapshot.stocks);
    valueUpdatedAt_ = updatedAt;
    const QString prefix = persisted
        ? QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c")
        : QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff08\u5185\u7f6e\u793a\u4f8b\uff09");
    valueStatus_ = institutionStatusFromTimestamp(prefix, updatedAt.isValid() ? updatedAt : QDateTime::currentDateTime());
    emit valueStatusChanged();
    selectDefaultAsset();
}

void MarketBoardController::selectDefaultAsset()
{
    if (const ValueAssetEntry* fund = fundModel_.entryAt(0))
    {
        setSelectedAsset(fund, QStringLiteral("fund"));
        return;
    }

    setSelectedAsset(stockModel_.entryAt(0), QStringLiteral("stock"));
}

void MarketBoardController::setSelectedAsset(const ValueAssetEntry* entry, const QString& assetKind)
{
    if (!entry)
    {
        hasSelectedAsset_ = false;
        selectedAsset_ = ValueAssetEntry{};
        selectedAssetKind_.clear();
        selectedAssetRealtimeBasePrice_ = 0.0;
        selectedAssetRealtimePhase_ = 0.0;
        emit selectedAssetChanged();
        return;
    }

    selectedAsset_ = *entry;
    selectedAssetKind_ = assetKind;
    hasSelectedAsset_ = true;
    selectedAssetRealtimeBasePrice_ = std::max(0.01, selectedAsset_.latestPrice);
    selectedAssetRealtimePhase_ = static_cast<double>(qAbs(qHash(selectedAsset_.code))) / 100.0;
    if (selectedAsset_.oneHourDrawdown.isEmpty())
    {
        selectedAsset_.oneHourDrawdown = fallback_hour_series(0.0, 0.03);
    }
    emit selectedAssetChanged();
}

bool MarketBoardController::shouldRefresh(const QDateTime& updatedAt, int intervalMs) const
{
    if (!updatedAt.isValid())
    {
        return true;
    }

    return updatedAt.msecsTo(QDateTime::currentDateTime()) >= intervalMs;
}

QString MarketBoardController::cacheDirectoryPath() const
{
    return cacheDirectoryPath_;
}

QString MarketBoardController::databasePath() const
{
    return databasePath_;
}

QVector<InstitutionBoardEntry> MarketBoardController::loadInstitutionBoardFromDatabase(QDateTime* updatedAt) const
{
    QVector<InstitutionBoardEntry> entries;
    if (!databaseReady())
    {
        return entries;
    }

    QSqlQuery query(database_);
    if (!query.exec(
            QStringLiteral(
                "SELECT id, rank, name, common_entry, core_strength, target_audience, score, updated_at "
                "FROM domestic_institutions ORDER BY rank ASC")))
    {
        return entries;
    }

    while (query.next())
    {
        InstitutionBoardEntry entry;
        entry.id = query.value(0).toString();
        entry.rank = query.value(1).toInt();
        entry.name = query.value(2).toString();
        entry.commonEntry = query.value(3).toString();
        entry.coreStrength = query.value(4).toString();
        entry.targetAudience = query.value(5).toString();
        entry.score = query.value(6).toDouble();
        entries.push_back(entry);

        if (updatedAt && !updatedAt->isValid())
        {
            *updatedAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
        }
    }

    return entries;
}

ValueBoardSnapshot MarketBoardController::loadValueBoardFromDatabase(
    const QString& institutionId,
    QDateTime* updatedAt) const
{
    ValueBoardSnapshot snapshot;
    snapshot.institutionId = institutionId;

    if (!databaseReady())
    {
        return snapshot;
    }

    QSqlQuery query(database_);
    query.prepare(
        QStringLiteral(
            "SELECT institution_name, asset_kind, id, rank, name, code, provider, category, score, "
            "latest_price, one_year_return_pct, investment_analysis, six_month_forecast, "
            "one_year_trend_json, one_hour_drawdown_json, updated_at "
            "FROM value_board_assets WHERE institution_id = ? ORDER BY asset_kind ASC, rank ASC"));
    query.addBindValue(institutionId);
    if (!query.exec())
    {
        return snapshot;
    }

    while (query.next())
    {
        snapshot.institutionName = query.value(0).toString();

        ValueAssetEntry entry;
        entry.id = query.value(2).toString();
        entry.rank = query.value(3).toInt();
        entry.name = query.value(4).toString();
        entry.code = query.value(5).toString();
        entry.provider = query.value(6).toString();
        entry.category = query.value(7).toString();
        entry.score = query.value(8).toDouble();
        entry.latestPrice = query.value(9).toDouble();
        entry.oneYearReturnPct = query.value(10).toDouble();
        entry.investmentAnalysis = query.value(11).toString();
        entry.sixMonthForecast = query.value(12).toString();
        entry.oneYearTrend = json_to_numbers(
            QJsonDocument::fromJson(query.value(13).toString().toUtf8()).array());
        entry.oneHourDrawdown = json_to_numbers(
            QJsonDocument::fromJson(query.value(14).toString().toUtf8()).array());

        const QString assetKind = query.value(1).toString();
        if (assetKind == QStringLiteral("fund"))
        {
            snapshot.funds.push_back(entry);
        }
        else
        {
            snapshot.stocks.push_back(entry);
        }

        if (updatedAt && !updatedAt->isValid())
        {
            *updatedAt = QDateTime::fromString(query.value(15).toString(), Qt::ISODate);
        }
    }

    return snapshot;
}

QVector<UsMarketWatchEntry> MarketBoardController::loadUsWatchlistFromDatabase() const
{
    QVector<UsMarketWatchEntry> entries;
    if (!databaseReady())
    {
        return entries;
    }

    QSqlQuery query(database_);
    if (!query.exec(
            QStringLiteral(
                "SELECT symbol, name, market, last_price, updated_at_ms "
                "FROM us_watchlist ORDER BY created_at_ms ASC")))
    {
        return entries;
    }

    while (query.next())
    {
        UsMarketWatchEntry entry;
        entry.symbol = query.value(0).toString();
        entry.name = query.value(1).toString();
        entry.market = query.value(2).toString();
        entry.lastPrice = query.value(3).toDouble();
        entry.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong());
        entries.push_back(entry);
    }

    return entries;
}

QVector<QPair<qint64, double>> MarketBoardController::loadUsHistoryFromDatabase(
    const QString& symbol,
    qint64 cutoffTimestampMs) const
{
    QVector<QPair<qint64, double>> history;
    if (!databaseReady())
    {
        return history;
    }

    QSqlQuery query(database_);
    query.prepare(
        QStringLiteral(
            "SELECT timestamp_ms, price FROM us_quote_history "
            "WHERE symbol = ? AND timestamp_ms >= ? ORDER BY timestamp_ms ASC"));
    query.addBindValue(symbol);
    query.addBindValue(cutoffTimestampMs);
    if (!query.exec())
    {
        return history;
    }

    while (query.next())
    {
        history.push_back({query.value(0).toLongLong(), query.value(1).toDouble()});
    }
    return history;
}

void MarketBoardController::saveInstitutionBoardToDatabase(
    const QVector<InstitutionBoardEntry>& entries,
    const QDateTime& updatedAt)
{
    if (!databaseReady())
    {
        return;
    }

    if (!database_.transaction())
    {
        return;
    }

    QSqlQuery clearQuery(database_);
    if (!clearQuery.exec(QStringLiteral("DELETE FROM domestic_institutions")))
    {
        database_.rollback();
        return;
    }

    QSqlQuery insertQuery(database_);
    insertQuery.prepare(
        QStringLiteral(
            "INSERT INTO domestic_institutions "
            "(id, rank, name, common_entry, core_strength, target_audience, score, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    const QString updatedAtText = updatedAt.toString(Qt::ISODate);
    for (const InstitutionBoardEntry& entry : entries)
    {
        insertQuery.bindValue(0, entry.id);
        insertQuery.bindValue(1, entry.rank);
        insertQuery.bindValue(2, entry.name);
        insertQuery.bindValue(3, entry.commonEntry);
        insertQuery.bindValue(4, entry.coreStrength);
        insertQuery.bindValue(5, entry.targetAudience);
        insertQuery.bindValue(6, entry.score);
        insertQuery.bindValue(7, updatedAtText);
        if (!insertQuery.exec())
        {
            database_.rollback();
            return;
        }
    }

    database_.commit();
}

void MarketBoardController::saveValueBoardToDatabase(
    const ValueBoardSnapshot& snapshot,
    const QDateTime& updatedAt)
{
    if (!databaseReady())
    {
        return;
    }

    if (!database_.transaction())
    {
        return;
    }

    QSqlQuery clearQuery(database_);
    clearQuery.prepare(QStringLiteral("DELETE FROM value_board_assets WHERE institution_id = ?"));
    clearQuery.addBindValue(snapshot.institutionId);
    if (!clearQuery.exec())
    {
        database_.rollback();
        return;
    }

    QSqlQuery insertQuery(database_);
    insertQuery.prepare(
        QStringLiteral(
            "INSERT INTO value_board_assets "
            "(institution_id, institution_name, asset_kind, id, rank, name, code, provider, category, "
            "score, latest_price, one_year_return_pct, investment_analysis, six_month_forecast, "
            "one_year_trend_json, one_hour_drawdown_json, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    const QString updatedAtText = updatedAt.toString(Qt::ISODate);
    const auto writeAssets = [&](const QVector<ValueAssetEntry>& entries, const QString& assetKind) -> bool
    {
        for (const ValueAssetEntry& entry : entries)
        {
            insertQuery.bindValue(0, snapshot.institutionId);
            insertQuery.bindValue(1, snapshot.institutionName);
            insertQuery.bindValue(2, assetKind);
            insertQuery.bindValue(3, entry.id);
            insertQuery.bindValue(4, entry.rank);
            insertQuery.bindValue(5, entry.name);
            insertQuery.bindValue(6, entry.code);
            insertQuery.bindValue(7, entry.provider);
            insertQuery.bindValue(8, entry.category);
            insertQuery.bindValue(9, entry.score);
            insertQuery.bindValue(10, entry.latestPrice);
            insertQuery.bindValue(11, entry.oneYearReturnPct);
            insertQuery.bindValue(12, entry.investmentAnalysis);
            insertQuery.bindValue(13, entry.sixMonthForecast);
            insertQuery.bindValue(14, QString::fromUtf8(
                QJsonDocument(numbers_to_json(entry.oneYearTrend)).toJson(QJsonDocument::Compact)));
            insertQuery.bindValue(15, QString::fromUtf8(
                QJsonDocument(numbers_to_json(entry.oneHourDrawdown)).toJson(QJsonDocument::Compact)));
            insertQuery.bindValue(16, updatedAtText);
            if (!insertQuery.exec())
            {
                return false;
            }
        }
        return true;
    };

    if (!writeAssets(snapshot.funds, QStringLiteral("fund")) ||
        !writeAssets(snapshot.stocks, QStringLiteral("stock")))
    {
        database_.rollback();
        return;
    }

    database_.commit();
}

bool MarketBoardController::saveUsWatchDefinitionToDatabase(const UsMarketWatchEntry& entry, qint64 createdAtMs)
{
    if (!databaseReady())
    {
        return false;
    }

    qint64 existingCreatedAtMs = createdAtMs > 0 ? createdAtMs : current_timestamp_ms();
    double existingLastPrice = entry.lastPrice;
    qint64 existingUpdatedAtMs = entry.updatedAt.isValid()
        ? entry.updatedAt.toMSecsSinceEpoch()
        : existingCreatedAtMs;

    QSqlQuery selectQuery(database_);
    selectQuery.prepare(QStringLiteral("SELECT created_at_ms, last_price, updated_at_ms FROM us_watchlist WHERE symbol = ?"));
    selectQuery.addBindValue(entry.symbol);
    if (selectQuery.exec() && selectQuery.next())
    {
        existingCreatedAtMs = selectQuery.value(0).toLongLong();
        existingLastPrice = selectQuery.value(1).toDouble() > 0.0
            ? selectQuery.value(1).toDouble()
            : entry.lastPrice;
        existingUpdatedAtMs = selectQuery.value(2).toLongLong() > 0
            ? selectQuery.value(2).toLongLong()
            : existingUpdatedAtMs;
    }

    QSqlQuery replaceQuery(database_);
    replaceQuery.prepare(
        QStringLiteral(
            "REPLACE INTO us_watchlist "
            "(symbol, name, market, created_at_ms, last_price, updated_at_ms) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
    replaceQuery.addBindValue(entry.symbol);
    replaceQuery.addBindValue(entry.name);
    replaceQuery.addBindValue(entry.market);
    replaceQuery.addBindValue(existingCreatedAtMs);
    replaceQuery.addBindValue(existingLastPrice);
    replaceQuery.addBindValue(existingUpdatedAtMs);
    return replaceQuery.exec();
}

void MarketBoardController::saveUsQuoteToDatabase(
    const QString& symbol,
    const QString& name,
    const QString& market,
    double lastPrice,
    qint64 timestampMs)
{
    if (!databaseReady())
    {
        return;
    }

    QSqlQuery updateWatchlistQuery(database_);
    updateWatchlistQuery.prepare(
        QStringLiteral(
            "UPDATE us_watchlist SET name = ?, market = ?, last_price = ?, updated_at_ms = ? "
            "WHERE symbol = ?"));
    updateWatchlistQuery.addBindValue(name);
    updateWatchlistQuery.addBindValue(market);
    updateWatchlistQuery.addBindValue(lastPrice);
    updateWatchlistQuery.addBindValue(timestampMs);
    updateWatchlistQuery.addBindValue(symbol);
    updateWatchlistQuery.exec();

    QSqlQuery insertHistoryQuery(database_);
    insertHistoryQuery.prepare(
        QStringLiteral(
            "INSERT OR REPLACE INTO us_quote_history (symbol, timestamp_ms, price) "
            "VALUES (?, ?, ?)"));
    insertHistoryQuery.addBindValue(symbol);
    insertHistoryQuery.addBindValue(timestampMs);
    insertHistoryQuery.addBindValue(lastPrice);
    insertHistoryQuery.exec();

    QSqlQuery pruneQuery(database_);
    pruneQuery.prepare(
        QStringLiteral("DELETE FROM us_quote_history WHERE symbol = ? AND timestamp_ms < ?"));
    pruneQuery.addBindValue(symbol);
    pruneQuery.addBindValue(timestampMs - (2LL * 60LL * 60LL * 1000LL));
    pruneQuery.exec();
}

void MarketBoardController::seedDefaultUsWatchlist()
{
    const qint64 nowMs = current_timestamp_ms();
    for (UsMarketWatchEntry entry : defaultUsWatchlist())
    {
        entry.updatedAt = QDateTime::fromMSecsSinceEpoch(nowMs);
        saveUsWatchDefinitionToDatabase(entry, nowMs);
        saveUsQuoteToDatabase(entry.symbol, entry.name, entry.market, entry.lastPrice, nowMs);
    }
}

QVector<InstitutionBoardEntry> MarketBoardController::fallbackInstitutions() const
{
    return {
        {QStringLiteral("ant-fortune"), 1, QStringLiteral(u"\u8682\u8681\u8d22\u5bcc"), QStringLiteral(u"\u652f\u4ed8\u5b9d / \u4f59\u989d\u5b9d"), QStringLiteral(u"\u6d3b\u94b1\u7ba1\u7406\u4e0e\u7a33\u5065\u914d\u7f6e"), QStringLiteral(u"\u5927\u4f17\u96f6\u552e\u6295\u8d44\u8005"), 95.0},
        {QStringLiteral("wechat-licaitong"), 2, QStringLiteral(u"\u5fae\u4fe1\u7406\u8d22\u901a"), QStringLiteral(u"\u5fae\u4fe1 / \u96f6\u94b1\u901a"), QStringLiteral(u"\u793e\u4ea4\u573a\u666f\u5185\u8f6c\u5316\u6548\u7387\u9ad8"), QStringLiteral(u"\u9ad8\u9891\u4f7f\u7528\u5fae\u4fe1\u7684\u7528\u6237"), 93.0},
        {QStringLiteral("cmb"), 3, QStringLiteral(u"\u62db\u5546\u94f6\u884c"), QStringLiteral(u"\u62db\u884c App / \u671d\u671d\u5b9d"), QStringLiteral(u"\u73b0\u91d1\u7ba1\u7406\u548c\u56fa\u6536\u7b56\u7565\u6210\u719f"), QStringLiteral(u"\u4e2d\u7b49\u98ce\u9669\u504f\u597d\u5ba2\u6237"), 91.5},
        {QStringLiteral("tiantian-fund"), 4, QStringLiteral(u"\u5929\u5929\u57fa\u91d1"), QStringLiteral(u"\u4e1c\u65b9\u8d22\u5bcc / \u5929\u5929\u57fa\u91d1"), QStringLiteral(u"\u57fa\u91d1\u4ea7\u54c1\u5e93\u5168\u3001\u9009\u57fa\u5de5\u5177\u591a"), QStringLiteral(u"\u57fa\u91d1\u914d\u7f6e\u7528\u6237"), 89.8},
        {QStringLiteral("jd-finance"), 5, QStringLiteral(u"\u4eac\u4e1c\u91d1\u878d"), QStringLiteral(u"\u4eac\u4e1c\u91d1\u878d / \u5c0f\u91d1\u5e93"), QStringLiteral(u"\u8d27\u5e01 + \u94f6\u884c\u7406\u8d22\u5165\u53e3\u660e\u786e"), QStringLiteral(u"\u504f\u7a33\u5065\u73b0\u91d1\u7ba1\u7406\u7528\u6237"), 87.9},
        {QStringLiteral("duxiaoman"), 6, QStringLiteral(u"\u5ea6\u5c0f\u6ee1\u7406\u8d22"), QStringLiteral(u"\u5ea6\u5c0f\u6ee1 App"), QStringLiteral(u"\u4e92\u8054\u7f51\u7a33\u5065\u7406\u8d22\u5165\u53e3"), QStringLiteral(u"\u6c42\u7a33\u7684\u4e92\u8054\u7f51\u7406\u8d22\u7528\u6237"), 86.3},
        {QStringLiteral("icbc"), 7, QStringLiteral(u"\u5de5\u5546\u94f6\u884c"), QStringLiteral(u"\u5de5\u94f6\u7406\u8d22 / \u624b\u673a\u94f6\u884c"), QStringLiteral(u"\u56fd\u6709\u5927\u884c\u8d44\u4ea7\u4fe1\u4efb\u5ea6\u9ad8"), QStringLiteral(u"\u4fdd\u5b88\u578b\u4f18\u5148"), 85.7},
        {QStringLiteral("ccb"), 8, QStringLiteral(u"\u5efa\u8bbe\u94f6\u884c"), QStringLiteral(u"\u5efa\u884c\u7406\u8d22 / \u624b\u673a\u94f6\u884c"), QStringLiteral(u"\u7ebf\u4e0a\u7ebf\u4e0b\u534f\u540c\u80fd\u529b\u5f3a"), QStringLiteral(u"\u504f\u5411\u7a33\u5065\u7684\u94f6\u884c\u5ba2\u7fa4"), 84.9},
        {QStringLiteral("boc"), 9, QStringLiteral(u"\u4e2d\u56fd\u94f6\u884c"), QStringLiteral(u"\u4e2d\u94f6\u7406\u8d22 / \u4e2d\u884c App"), QStringLiteral(u"\u5916\u6c47\u548c\u8de8\u5e01\u79cd\u4ea7\u54c1\u6709\u4f18\u52bf"), QStringLiteral(u"\u6709\u5916\u6c47\u914d\u7f6e\u9700\u6c42\u7684\u7528\u6237"), 83.8},
        {QStringLiteral("pingan-bank"), 10, QStringLiteral(u"\u5e73\u5b89\u94f6\u884c"), QStringLiteral(u"\u53e3\u888b\u94f6\u884c / \u5e73\u5b89\u7406\u8d22"), QStringLiteral(u"\u7efc\u5408\u91d1\u878d\u751f\u6001\u5354\u540c"), QStringLiteral(u"\u8ffd\u6c42\u4ea7\u54c1\u7ec4\u5408\u548c\u670d\u52a1\u4f53\u9a8c"), 82.6}
    };
}

ValueBoardSnapshot MarketBoardController::fallbackValueBoard(
    const QString& institutionId,
    const QString& institutionName) const
{
    ValueBoardSnapshot snapshot;
    snapshot.institutionId = institutionId;
    snapshot.institutionName = institutionName;
    snapshot.funds = {
        {QStringLiteral("fund-1"), 1, QStringLiteral(u"\u4e2d\u6b27\u4ef7\u503c\u667a\u9009"), QStringLiteral("A001"), QStringLiteral(u"\u4e2d\u6b27\u57fa\u91d1"), QStringLiteral(u"\u4e3b\u52a8\u80a1\u7968\u578b"), 93.6, 1.842, 18.5, QStringLiteral(u"\u4f30\u503c\u4ecd\u5904\u4e8e\u5386\u53f2\u4e2d\u4f4e\u4f4d\uff0c\u7ec4\u5408\u91cd\u4ed3\u4ee5\u73b0\u91d1\u6d41\u7a33\u5b9a\u7684\u884c\u4e1a\u9f99\u5934\u4e3a\u4e3b\u3002"), QStringLiteral(u"\u82e5\u5e02\u573a\u98ce\u683c\u7ee7\u7eed\u504f\u5411\u4f4e\u4f30\u503c\u548c\u9ad8\u80a1\u4e1c\u56de\u62a5\uff0c6 \u4e2a\u6708\u5185\u4ecd\u6709\u671b\u4fdd\u6301\u7a33\u5065\u8d85\u989d\u3002"), fallback_year_series(94.0, 0.75), fallback_hour_series(-0.8, 0.03)},
        {QStringLiteral("fund-2"), 2, QStringLiteral(u"\u6613\u65b9\u8fbe\u84dd\u7b79\u7cbe\u9009"), QStringLiteral("A002"), QStringLiteral(u"\u6613\u65b9\u8fbe"), QStringLiteral(u"\u504f\u80a1\u6df7\u5408\u578b"), 92.1, 2.103, 16.8, QStringLiteral(u"\u62e5\u6709\u9ad8 ROE \u4e0e\u9ad8\u81ea\u7531\u73b0\u91d1\u6d41\u7684\u84dd\u7b79\u6301\u4ed3\uff0c\u7a7f\u8d8a\u5468\u671f\u80fd\u529b\u8f83\u5f3a\u3002"), QStringLiteral(u"\u82e5\u6d41\u52a8\u6027\u6539\u5584\uff0c\u5927\u76d8\u4ef7\u503c\u98ce\u683c\u53ef\u80fd\u7ee7\u7eed\u53d7\u76ca\uff0c6 \u4e2a\u6708\u4ee5\u7a33\u4e2d\u5411\u4e0a\u4e3a\u4e3b\u3002"), fallback_year_series(96.0, 0.68), fallback_hour_series(-0.6, 0.02)},
        {QStringLiteral("fund-3"), 3, QStringLiteral(u"\u5bcc\u56fd\u6838\u5fc3\u6210\u957f"), QStringLiteral("A003"), QStringLiteral(u"\u5bcc\u56fd\u57fa\u91d1"), QStringLiteral(u"\u6df7\u5408\u578b"), 90.7, 1.678, 15.2, QStringLiteral(u"\u4ee5\u76c8\u5229\u8d28\u91cf\u548c\u4f30\u503c\u5339\u914d\u4e3a\u6838\u5fc3\uff0c\u5bf9\u4f18\u8d28\u6210\u957f\u516c\u53f8\u4fdd\u6301\u8f83\u9ad8\u4ed3\u4f4d\u3002"), QStringLiteral(u"\u82e5\u76c8\u5229\u9884\u671f\u7a33\u5b9a\u5411\u4e0a\uff0c\u540e\u7eed 6 \u4e2a\u6708\u5177\u5907\u7f13\u6162\u6298\u4ef7\u4fee\u590d\u7684\u57fa\u7840\u3002"), fallback_year_series(92.0, 0.7), fallback_hour_series(-0.9, 0.05)},
        {QStringLiteral("fund-4"), 4, QStringLiteral(u"\u666f\u987a\u957f\u57ce\u9f99\u5934\u4f18\u9009"), QStringLiteral("A004"), QStringLiteral(u"\u666f\u987a\u957f\u57ce"), QStringLiteral(u"\u504f\u80a1\u6df7\u5408\u578b"), 89.6, 1.532, 14.6, QStringLiteral(u"\u805a\u7126\u9f99\u5934\u516c\u53f8\u4e0e\u4ea7\u4e1a\u5347\u7ea7\u8fc7\u7a0b\u4e2d\u7684\u9ad8\u58c1\u5792\u8d44\u4ea7\u3002"), QStringLiteral(u"\u82e5\u7ecf\u6d4e\u4fee\u590d\u8282\u594f\u7eed\u884c\uff0c6 \u4e2a\u6708\u7ef4\u5ea6\u6709\u671b\u4ee5\u9707\u8361\u4e0a\u884c\u4e3a\u4e3b\u3002"), fallback_year_series(93.0, 0.56), fallback_hour_series(-1.1, 0.07)},
        {QStringLiteral("fund-5"), 5, QStringLiteral(u"\u5357\u65b9\u7a33\u5065\u589e\u957f"), QStringLiteral("A005"), QStringLiteral(u"\u5357\u65b9\u57fa\u91d1"), QStringLiteral(u"\u6df7\u5408\u578b"), 88.9, 1.421, 13.4, QStringLiteral(u"\u7ec4\u5408\u575a\u6301\u4f30\u503c\u5b89\u5168\u8fb9\u9645\uff0c\u540c\u65f6\u4fdd\u7559\u5bf9\u4f18\u8d28\u6210\u957f\u7684\u66b4\u9732\u3002"), QStringLiteral(u"\u5982\u98ce\u9669\u504f\u597d\u7f13\u6162\u56de\u5347\uff0c6 \u4e2a\u6708\u5185\u6709\u671b\u4f53\u73b0\u7a33\u4e2d\u6709\u8fdb\u7684\u8868\u73b0\u3002"), fallback_year_series(94.0, 0.45), fallback_hour_series(-0.7, 0.04)},
        {QStringLiteral("fund-6"), 6, QStringLiteral(u"\u6c47\u6dfb\u5bcc\u4ef7\u503c\u53d1\u73b0"), QStringLiteral("A006"), QStringLiteral(u"\u6c47\u6dfb\u5bcc"), QStringLiteral(u"\u80a1\u7968\u578b"), 87.8, 1.365, 12.9, QStringLiteral(u"\u504f\u597d\u5b9a\u4ef7\u9519\u6740\u800c\u57fa\u672c\u9762\u4e0d\u5dee\u7684\u4ef7\u503c\u7c7b\u8d44\u4ea7\u3002"), QStringLiteral(u"\u82e5\u4f30\u503c\u6269\u5f20\u672a\u660e\u663e\uff0c6 \u4e2a\u6708\u66f4\u53ef\u80fd\u8868\u73b0\u4e3a\u7f13\u6b65\u62ac\u5347\u3002"), fallback_year_series(91.0, 0.5), fallback_hour_series(-1.0, 0.06)},
        {QStringLiteral("fund-7"), 7, QStringLiteral(u"\u534e\u590f\u7ea2\u5229\u7cbe\u9009"), QStringLiteral("A007"), QStringLiteral(u"\u534e\u590f\u57fa\u91d1"), QStringLiteral(u"\u504f\u80a1\u6df7\u5408\u578b"), 86.9, 1.284, 11.8, QStringLiteral(u"\u4ee5\u80a1\u606f\u7387\u548c\u73b0\u91d1\u6d41\u5b89\u5168\u6027\u4e3a\u7b5b\u9009\u4e3b\u7ebf\u3002"), QStringLiteral(u"\u82e5\u7ea2\u5229\u98ce\u683c\u5ef6\u7eed\uff0c\u540e\u7eed 6 \u4e2a\u6708\u4ecd\u5177\u9632\u5fa1\u6027\u4e0e\u7a33\u5b9a\u56de\u62a5\u6f5c\u529b\u3002"), fallback_year_series(95.0, 0.38), fallback_hour_series(-0.5, 0.02)},
        {QStringLiteral("fund-8"), 8, QStringLiteral(u"\u5609\u5b9e\u6cbb\u7406\u9a71\u52a8"), QStringLiteral("A008"), QStringLiteral(u"\u5609\u5b9e\u57fa\u91d1"), QStringLiteral(u"\u6df7\u5408\u578b"), 85.8, 1.197, 10.6, QStringLiteral(u"\u805a\u7126\u516c\u53f8\u6cbb\u7406\u7ed3\u6784\u548c\u8d44\u672c\u5f00\u652f\u7eaa\u5f8b\u3002"), QStringLiteral(u"\u82e5\u5e02\u573a\u5bf9\u57fa\u672c\u9762\u5b9a\u4ef7\u56de\u5f52\uff0c6 \u4e2a\u6708\u7ef4\u5ea6\u5177\u5907\u4e00\u5b9a\u5411\u4e0a\u5f39\u6027\u3002"), fallback_year_series(90.0, 0.35), fallback_hour_series(-1.2, 0.08)},
        {QStringLiteral("fund-9"), 9, QStringLiteral(u"\u5de5\u94f6\u745e\u4fe1\u5185\u9700\u4ef7\u503c"), QStringLiteral("A009"), QStringLiteral(u"\u5de5\u94f6\u745e\u4fe1"), QStringLiteral(u"\u80a1\u7968\u578b"), 84.7, 1.105, 9.8, QStringLiteral(u"\u5185\u9700\u4e3b\u7ebf\u53e0\u52a0\u4f30\u503c\u4e0e\u73b0\u91d1\u6d41\u53cc\u91cd\u7b5b\u9009\u3002"), QStringLiteral(u"\u82e5\u6d88\u8d39\u4fee\u590d\u98ce\u9669\u6e29\u548c\u843d\u5730\uff0c6 \u4e2a\u6708\u5185\u53ef\u80fd\u6709\u7ed3\u6784\u6027\u8868\u73b0\u3002"), fallback_year_series(89.0, 0.31), fallback_hour_series(-0.9, 0.05)},
        {QStringLiteral("fund-10"), 10, QStringLiteral(u"\u94f6\u534e\u8d28\u91cf\u6df7\u5408"), QStringLiteral("A010"), QStringLiteral(u"\u94f6\u534e\u57fa\u91d1"), QStringLiteral(u"\u6df7\u5408\u578b"), 83.9, 1.021, 8.7, QStringLiteral(u"\u7ec4\u5408\u4ee5\u8d28\u91cf\u8d44\u4ea7\u548c\u4f4e\u6ce2\u52a8\u6301\u4ed3\u4e3a\u57fa\u5e95\u3002"), QStringLiteral(u"\u672a\u6765 6 \u4e2a\u6708\u66f4\u9002\u5408\u4ee5\u5b88\u4e3a\u653b\uff0c\u4ee5\u7a33\u5b9a\u4e3a\u4e3b\u3002"), fallback_year_series(88.0, 0.28), fallback_hour_series(-0.6, 0.03)}
    };

    snapshot.stocks = {
        {QStringLiteral("stock-1"), 1, QStringLiteral(u"\u8d35\u5dde\u8305\u53f0"), QStringLiteral("600519.SH"), QStringLiteral(u"\u98df\u54c1\u996e\u6599"), QStringLiteral(u"A \u80a1"), 95.2, 1742.0, 19.3, QStringLiteral(u"\u54c1\u724c\u5b9a\u4ef7\u6743\u3001\u9ad8 ROE \u4e0e\u73b0\u91d1\u6d41\u786e\u5b9a\u6027\u5171\u540c\u6784\u6210\u957f\u671f\u4ef7\u503c\u6838\u5fc3\u3002"), QStringLiteral(u"\u672a\u6765 6 \u4e2a\u6708\u66f4\u53ef\u80fd\u4ee5\u4f30\u503c\u7ef4\u6301 + \u4e1a\u7ee9\u7a33\u5b9a\u6d88\u5316\u4e3a\u4e3b\uff0c\u542b\u7f13\u6162\u4e0a\u884c\u503e\u5411\u3002"), fallback_year_series(98.0, 0.85), fallback_hour_series(-0.7, 0.04)},
        {QStringLiteral("stock-2"), 2, QStringLiteral(u"\u5b81\u5fb7\u65f6\u4ee3"), QStringLiteral("300750.SZ"), QStringLiteral(u"\u7535\u529b\u8bbe\u5907"), QStringLiteral(u"A \u80a1"), 92.8, 226.4, 22.1, QStringLiteral(u"\u884c\u4e1a\u9f99\u5934\u4ecd\u5177\u89c4\u6a21\u548c\u6210\u672c\u4f18\u52bf\uff0c\u4f30\u503c\u56de\u5230\u4e2d\u4f4e\u533a\u95f4\u540e\u5438\u5f15\u529b\u63d0\u5347\u3002"), QStringLiteral(u"\u82e5\u65b0\u80fd\u6e90\u94fe\u6761\u9700\u6c42\u7a33\u5b9a\uff0c6 \u4e2a\u6708\u7ef4\u5ea6\u53ef\u80fd\u51fa\u73b0\u4f30\u503c\u4fee\u590d\u3002"), fallback_year_series(92.0, 1.0), fallback_hour_series(-1.4, 0.08)},
        {QStringLiteral("stock-3"), 3, QStringLiteral(u"\u817e\u8baf\u63a7\u80a1"), QStringLiteral("0700.HK"), QStringLiteral(u"\u4e92\u8054\u7f51"), QStringLiteral(u"\u6e2f\u80a1"), 91.7, 381.2, 17.4, QStringLiteral(u"\u5185\u5bb9\u3001\u793e\u4ea4\u3001\u652f\u4ed8\u751f\u6001\u4ecd\u5177\u5f3a\u58c1\u5792\uff0c\u73b0\u91d1\u6d41\u5145\u6c9b\u3002"), QStringLiteral(u"\u82e5\u6e2f\u80a1\u4f30\u503c\u98ce\u683c\u7ee7\u7eed\u4fee\u590d\uff0c6 \u4e2a\u6708\u5185\u6709\u671b\u7ef4\u6301\u6e29\u548c\u4e0a\u884c\u3002"), fallback_year_series(96.0, 0.72), fallback_hour_series(-0.8, 0.03)},
        {QStringLiteral("stock-4"), 4, QStringLiteral(u"\u7f8e\u7684\u96c6\u56e2"), QStringLiteral("000333.SZ"), QStringLiteral(u"\u5bb6\u7535"), QStringLiteral(u"A \u80a1"), 89.4, 71.6, 14.2, QStringLiteral(u"\u9f99\u5934\u5bb6\u7535\u8d44\u4ea7\u5177\u5907\u5168\u7403\u5316\u5236\u9020\u548c\u7a33\u5b9a\u5206\u7ea2\u80fd\u529b\u3002"), QStringLiteral(u"\u5982\u5185\u9700\u4fee\u590d\u4e0e\u6210\u672c\u7aef\u4f18\u5316\u5ef6\u7eed\uff0c6 \u4e2a\u6708\u5185\u4ecd\u6709\u671b\u7a33\u6b65\u5411\u4e0a\u3002"), fallback_year_series(95.0, 0.46), fallback_hour_series(-0.4, 0.02)},
        {QStringLiteral("stock-5"), 5, QStringLiteral(u"\u4e2d\u56fd\u6d77\u6cb9"), QStringLiteral("0883.HK"), QStringLiteral(u"\u80fd\u6e90"), QStringLiteral(u"\u6e2f\u80a1"), 88.3, 20.7, 12.6, QStringLiteral(u"\u80a1\u606f\u7387\u4f18\u52bf\u663e\u8457\uff0c\u8d44\u672c\u5f00\u652f\u7eaa\u5f8b\u4e0e\u73b0\u91d1\u56de\u62a5\u903b\u8f91\u6e05\u6670\u3002"), QStringLiteral(u"\u672a\u6765 6 \u4e2a\u6708\u9700\u5173\u6ce8\u5927\u5b97\u5546\u54c1\u6ce2\u52a8\uff0c\u4f46\u9632\u5fa1\u5c5e\u6027\u4ecd\u7136\u8f83\u5f3a\u3002"), fallback_year_series(94.0, 0.42), fallback_hour_series(-1.0, 0.05)}
    };

    return snapshot;
}

QVector<UsMarketWatchEntry> MarketBoardController::defaultUsWatchlist() const
{
    QVector<UsMarketWatchEntry> entries;
    const QDateTime now = QDateTime::currentDateTime();
    for (const UsCatalogSeed& seed : default_us_catalog())
    {
        UsMarketWatchEntry entry;
        entry.symbol = seed.symbol;
        entry.name = seed.name;
        entry.market = seed.market;
        entry.lastPrice = seed.price;
        entry.updatedAt = now;
        entries.push_back(entry);
    }
    return entries;
}

UsMarketWatchEntry MarketBoardController::resolveUsWatchInput(const QString& input) const
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    const QString normalizedInput = normalized_symbol(trimmed);
    const QString folded = trimmed.trimmed().toLower();
    for (const UsCatalogSeed& seed : default_us_catalog())
    {
        if (seed.symbol.compare(normalizedInput, Qt::CaseInsensitive) == 0 ||
            seed.name.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return {seed.symbol, seed.name, seed.market, seed.price, 0.0, {}, QDateTime::currentDateTime()};
        }

        for (const QString& alias : seed.aliases)
        {
            if (alias.trimmed().toLower() == folded)
            {
                return {seed.symbol, seed.name, seed.market, seed.price, 0.0, {}, QDateTime::currentDateTime()};
            }
        }
    }

    return {
        normalizedInput,
        trimmed,
        QStringLiteral("US Watch"),
        0.0,
        0.0,
        {},
        QDateTime::currentDateTime()
    };
}

void MarketBoardController::syncUsMarketModel()
{
    for (UsMarketWatchEntry& entry : usWatchEntries_)
    {
        entry.realtimeTrend = sampled_price_history(
            usQuoteHistory_.value(entry.symbol),
            entry.lastPrice);
        // Always append the live snapshot price as the trailing point so
        // each per-second tick produces a series whose length changes by
        // one. QML's binding comparator picks that up and re-fires
        // Sparkline.onSeriesChanged → requestPaint, which is what makes the
        // curve visibly scroll left every second instead of looking frozen.
        // Cap at 64 points so the buffer can't grow forever.
        entry.realtimeTrend.append(entry.lastPrice);
        while (entry.realtimeTrend.size() > 64)
        {
            entry.realtimeTrend.removeFirst();
        }
    }
    usMarketModel_.setEntries(usWatchEntries_);
    if (usMarketStatus_.isEmpty())
    {
        usMarketStatus_ = formatUsStatus();
        emit usMarketStatusChanged();
    }
}

void MarketBoardController::updateUsMarketBoard()
{
    if (!realtimeActive_ || usQuoteRequestInFlight_)
    {
        return;
    }

    if (usWatchEntries_.isEmpty())
    {
        usMarketStatus_ = QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u6682\u65e0\u81ea\u9009\u6807\u7684");
        emit usMarketStatusChanged();
        return;
    }

    QStringList querySymbols;
    querySymbols.reserve(usWatchEntries_.size());
    for (const UsMarketWatchEntry& entry : usWatchEntries_)
    {
        const QString querySymbol = tencentUsQuerySymbol(entry.symbol);
        if (!querySymbol.isEmpty())
        {
            querySymbols << querySymbol;
        }
    }

    if (querySymbols.isEmpty())
    {
        usMarketStatus_ = QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u6682\u65e0\u53ef\u67e5\u8be2\u7684\u771f\u5b9e\u884c\u60c5\u4ee3\u7801");
        emit usMarketStatusChanged();
        return;
    }

    usQuoteRequestInFlight_ = true;
    // Plain http: Qt's bundled OpenSSL has handshake issues against Tencent's
    // qt.gtimg.cn endpoint on this build (same root cause as the eastmoney
    // push2 endpoint). The data is public read-only quotes; http is fine.
    QNetworkRequest request(QUrl(QStringLiteral("http://qt.gtimg.cn/q=%1").arg(querySymbols.join(','))));
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    QNetworkReply* reply = networkAccess_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        handleTencentUsQuoteReply(reply);
    });
}

QString MarketBoardController::tencentUsQuerySymbol(const QString& symbol) const
{
    const QString normalized = normalized_symbol(symbol);
    if (normalized == QStringLiteral("NDX"))
    {
        return QStringLiteral("usNDX");
    }
    if (normalized == QStringLiteral("SPX"))
    {
        return QStringLiteral("usINX");
    }
    if (normalized == QStringLiteral("DJI"))
    {
        return QStringLiteral("usDJI");
    }
    return QStringLiteral("us%1").arg(normalized);
}

void MarketBoardController::handleTencentUsQuoteReply(QNetworkReply* reply)
{
    usQuoteRequestInFlight_ = false;
    const QString errorText = reply->error() == QNetworkReply::NoError
        ? QString()
        : reply->errorString();
    const QString text = decode_tencent_payload(reply->readAll());
    reply->deleteLater();

    if (!errorText.isEmpty())
    {
        usMarketStatus_ = QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u771f\u5b9e\u884c\u60c5\u83b7\u53d6\u5931\u8d25 %1").arg(errorText);
        emit usMarketStatusChanged();
        return;
    }

    const qint64 nowMs = current_timestamp_ms();
    const bool shouldPersistQuotes =
        lastUsQuotePersistMs_ <= 0 ||
        nowMs - lastUsQuotePersistMs_ >= 15000;
    if (shouldPersistQuotes && databaseReady())
    {
        database_.transaction();
    }

    int updatedCount = 0;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString& line : lines)
    {
        const int quoteStart = line.indexOf('"');
        const int quoteEnd = line.lastIndexOf('"');
        if (quoteStart < 0 || quoteEnd <= quoteStart)
        {
            continue;
        }

        const QStringList fields = line.mid(quoteStart + 1, quoteEnd - quoteStart - 1).split('~');
        if (fields.size() < 32)
        {
            continue;
        }

        QString symbol = fields.value(2).trimmed();
        symbol.remove(QRegularExpression(QStringLiteral("^\\.")));
        symbol.remove(QRegularExpression(QStringLiteral("\\..*$")));
        if (symbol == QStringLiteral("INX"))
        {
            symbol = QStringLiteral("SPX");
        }

        const double lastPrice = fields.value(3).toDouble();
        // Tencent qt.gtimg.cn US-stock field layout (1-based in their docs,
        // 0-based here):
        //   [ 1] name
        //   [ 2] full code (.NDX / AAPL.OQ)
        //   [ 3] last price
        //   [ 4] previous close
        //   [29] empty placeholder
        //   [30] timestamp "yyyy-MM-dd HH:mm:ss"
        //   [31] absolute change in dollars
        //   [32] day change percentage  <-- this is what the UI shows
        //   [33] day high
        //   [34] day low
        // Earlier code read [29] for timestamp and [31] for percent change,
        // which gave a blank time and accidentally rendered the dollar delta
        // as a percentage (e.g. DJI -313.62 USD became "-313.62%").
        const double dayChangePct = fields.value(32).toDouble();
        if (lastPrice <= 0.0)
        {
            continue;
        }

        auto entryIt = std::find_if(
            usWatchEntries_.begin(),
            usWatchEntries_.end(),
            [&symbol](const UsMarketWatchEntry& entry)
            {
                return normalized_symbol(entry.symbol).compare(symbol, Qt::CaseInsensitive) == 0;
            });
        if (entryIt == usWatchEntries_.end())
        {
            continue;
        }

        entryIt->name = fields.value(1).trimmed().isEmpty() ? entryIt->name : fields.value(1).trimmed();
        entryIt->lastPrice = lastPrice;
        entryIt->updatedAt = QDateTime::fromString(fields.value(30), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!entryIt->updatedAt.isValid())
        {
            entryIt->updatedAt = QDateTime::fromMSecsSinceEpoch(nowMs);
        }

        QVector<QPair<qint64, double>>& history = usQuoteHistory_[entryIt->symbol];
        // Only discard the cached series if the last point is wildly stale
        // (>4 hours, i.e. the previous trading session) or the price moved
        // more than 15% since then (a likely split/symbol-reuse signal).
        // The earlier 5-minute / 8% thresholds erased the history-seed every
        // tick after the akshare backfill, so the sparkline collapsed to a
        // single point per fetch.
        const bool legacyHistory =
            !history.isEmpty() &&
            (entryIt->updatedAt.toMSecsSinceEpoch() - history.last().first > 24LL * 60LL * 60LL * 1000LL ||
             history.last().second <= 0.0 ||
             std::abs(lastPrice / history.last().second - 1.0) > 0.15);
        if (legacyHistory)
        {
            history.clear();
        }
        // Append per-second snapshots only when the price actually moved or
        // when 60s have passed (so the buffer doesn't degenerate to a flat
        // tail off-hours but still records a live data point during trading
        // hours). The seeded intraday curve from akshare is preserved.
        const bool priceMoved =
            history.isEmpty() || std::abs(lastPrice - history.last().second) > 1e-6;
        const bool sampleStale =
            !history.isEmpty() && nowMs - history.last().first >= 60LL * 1000LL;
        if (priceMoved || sampleStale)
        {
            history.push_back({nowMs, lastPrice});
        }
        while (!history.isEmpty() && history.first().first < nowMs - (24LL * 60LL * 60LL * 1000LL))
        {
            history.remove(0);
        }

        entryIt->oneHourChangePct = dayChangePct;
        entryIt->realtimeTrend = sampled_price_history(history, lastPrice);
        if (shouldPersistQuotes)
        {
            saveUsQuoteToDatabase(entryIt->symbol, entryIt->name, entryIt->market, entryIt->lastPrice, nowMs);
        }
        ++updatedCount;
    }

    if (shouldPersistQuotes && databaseReady())
    {
        database_.commit();
        lastUsQuotePersistMs_ = nowMs;
    }

    usMarketStatus_ = updatedCount > 0
        ? QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u771f\u5b9e\u884c\u60c5 %1 \u6761\uff0c%2")
            .arg(updatedCount)
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        : QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u771f\u5b9e\u884c\u60c5\u8fd4\u56de\u4e3a\u7a7a");
    // Route through syncUsMarketModel so each tick re-derives realtimeTrend
    // (and the trailing watermark sample) consistently across all entry
    // points. Skipping it here would leave the sparkline frozen because the
    // realtimeTrend array would never receive a new trailing point.
    syncUsMarketModel();
    emit usMarketStatusChanged();

    // Diagnostic log so the operator can confirm the per-second fetch loop
    // really is reaching Tencent and updating the model. Throttled to once
    // per minute so the log doesn't drown in noise.
    static qint64 sLastLog = 0;
    const qint64 nowLogMs = QDateTime::currentMSecsSinceEpoch();
    if (nowLogMs - sLastLog >= 60000)
    {
        sLastLog = nowLogMs;
        QString sampleSummary;
        for (const UsMarketWatchEntry& entry : usWatchEntries_)
        {
            const auto histIt = usQuoteHistory_.constFind(entry.symbol);
            const int histSize = histIt == usQuoteHistory_.constEnd() ? 0 : histIt.value().size();
            sampleSummary += QStringLiteral("%1=%2/%3 ")
                .arg(entry.symbol)
                .arg(histSize)
                .arg(QString::number(entry.lastPrice, 'f', 2));
        }
        telemetry_.logger().information(
            QStringLiteral("US watch tick: updated=%1 lines=%2 hist[size/last] %3")
                .arg(updatedCount).arg(lines.size()).arg(sampleSummary)
                .toStdString());

    }
}

double MarketBoardController::computeUsOneHourChangePct(
    const QString& symbol,
    double currentPrice,
    qint64 nowMs) const
{
    Q_UNUSED(nowMs);

    const QVector<QPair<qint64, double>> history = usQuoteHistory_.value(symbol);
    if (history.isEmpty())
    {
        return 0.0;
    }

    const double basePrice = history.first().second;
    if (std::abs(basePrice) < 0.00001)
    {
        return 0.0;
    }

    return ((currentPrice - basePrice) / basePrice) * 100.0;
}

QString MarketBoardController::formatUsStatus() const
{
    if (usWatchEntries_.isEmpty())
    {
        return QStringLiteral(u"\u7f8e\u80a1\u89c2\u5bdf\uff1a\u7b49\u5f85\u81ea\u9009\u6807\u7684");
    }

    QDateTime latestUpdate;
    for (const UsMarketWatchEntry& entry : usWatchEntries_)
    {
        if (!latestUpdate.isValid() || entry.updatedAt > latestUpdate)
        {
            latestUpdate = entry.updatedAt;
        }
    }

    return QStringLiteral(
        u"\u7f8e\u80a1\u89c2\u5bdf\uff1a%1 \u4e2a\u6807\u7684\uff0c\u6700\u8fd1\u66f4\u65b0 %2\uff0c\u8f7b\u91cf\u5b9e\u65f6\u5237\u65b0")
        .arg(usWatchEntries_.size())
        .arg(latestUpdate.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

void MarketBoardController::requestInstitutionBoard()
{
    if (institutionRequestInFlight_)
    {
        return;
    }

    // No real-data source for the wealth-institution catalog yet, and the
    // earlier LLM/codex path produced unauditable rankings. Keep whatever is
    // already in the SQLite cache (curated list of mainstream platforms) and
    // skip re-running codex unless the operator explicitly configures a real
    // data source.
    if (resolveCodexCommand().isEmpty())
    {
        institutionStatus_ = QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff1a\u5217\u8868\u4e3a\u4eba\u5de5\u7ef4\u62a4\u7684\u4e3b\u6d41\u5e73\u53f0\u540d\u5355\uff0c\u672a\u505a\u5b9e\u65f6\u8bc4\u7ea7\u3002");
        emit institutionStatusChanged();
        return;
    }

    institutionRequestInFlight_ = true;
    institutionStatus_ = QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff1a\u6b63\u5728\u66f4\u65b0");
    emit institutionStatusChanged();

    const QString prompt = QStringLiteral(
        "Today is %1. Build a China-focused wealth institution ranking for a desktop app. "
        "Return the top 10 mainstream wealth-management institutions commonly used by mainland investors. "
        "Include platforms like Alipay wealth, WeChat Licaitong, large banks, and fund supermarkets. "
        "The output must be concise, product-oriented, and suitable for a leaderboard UI. "
        "Use Mandarin Chinese for display fields, ASCII slugs for ids, and scores between 0 and 100.")
        .arg(currentDateLabel());

    runCodexStructuredTask(
        QStringLiteral("institution-board"),
        prompt,
        institutionSchema(),
        [this](const QJsonObject& payload)
        {
            institutionRequestInFlight_ = false;

            const QVector<InstitutionBoardEntry> entries = parseInstitutions(payload);
            if (entries.isEmpty())
            {
                institutionStatus_ = QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\uff1a\u8fd4\u56de\u4e3a\u7a7a\uff0c\u4fdd\u7559\u73b0\u6709\u6570\u636e");
                emit institutionStatusChanged();
                return;
            }

            const QDateTime updatedAt = QDateTime::currentDateTime();
            saveInstitutionBoardToDatabase(entries, updatedAt);
            institutionLoadedFromStorage_ = true;
            applyInstitutionBoard(entries, updatedAt, true);
        },
        [this](const QString& error)
        {
            institutionRequestInFlight_ = false;
            institutionStatus_ = QStringLiteral(u"\u7406\u8d22\u673a\u6784\u699c\u66f4\u65b0\u5931\u8d25\uff1a%1").arg(error);
            emit institutionStatusChanged();
        });
}

bool MarketBoardController::requestValueBoardViaPython(
    const QString& institutionId,
    const QString& institutionName)
{
    if (valueBoardProviderCommand_.isEmpty() || valueBoardProviderScriptPath_.isEmpty())
    {
        return false;
    }
    if (!QFileInfo::exists(valueBoardProviderScriptPath_))
    {
        return false;
    }

    auto* process = new QProcess(this);
    QStringList arguments;
    if (valueBoardProviderCommand_ == QStringLiteral("py"))
    {
        arguments << QStringLiteral("-3");
    }
    arguments
        << valueBoardProviderScriptPath_
        << QStringLiteral("--institution-id") << institutionId
        << QStringLiteral("--institution-name") << institutionName
        << QStringLiteral("--funds") << QStringLiteral("10")
        << QStringLiteral("--stocks") << QStringLiteral("5")
        << QStringLiteral("--json");
    // Make pandas/numpy quiet on Windows: no progress bars in stdout JSON.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(QFileInfo(valueBoardProviderScriptPath_).absolutePath());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    telemetry_.logger().information(
        QStringLiteral("Refreshing value board via python: %1 %2")
            .arg(valueBoardProviderCommand_, arguments.join(QStringLiteral(" ")))
            .toStdString());

    QPointer<MarketBoardController> guard(this);
    connect(process, &QProcess::finished, this,
        [this, guard, process, institutionId, institutionName](int exitCode, QProcess::ExitStatus)
    {
        const QString stdoutText = QString::fromUtf8(process->readAllStandardOutput());
        const QString stderrText = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        if (!guard)
        {
            return;
        }
        handleValueBoardProviderResult(institutionId, institutionName, exitCode, stdoutText, stderrText);
    });

    process->start(valueBoardProviderCommand_, arguments);
    if (!process->waitForStarted(3000))
    {
        const QString errorText = process->errorString();
        process->deleteLater();
        valueRequestInFlight_ = false;
        valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u542f\u52a8\u6293\u53d6\u811a\u672c\u5931\u8d25 %1").arg(errorText);
        telemetry_.logger().warning(valueStatus_.toStdString());
        emit valueStatusChanged();
        return false;
    }

    QTimer::singleShot(valueBoardProviderTimeoutMs_, process, [process]()
    {
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
        }
    });
    return true;
}

void MarketBoardController::handleValueBoardProviderResult(
    const QString& institutionId,
    const QString& institutionName,
    int exitCode,
    const QString& stdoutText,
    const QString& stderrText)
{
    valueRequestInFlight_ = false;

    if (exitCode != 0)
    {
        const QString head = stderrText.left(200).trimmed();
        valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\u66f4\u65b0\u5931\u8d25\uff08\u8131\u9519\u7801 %1\uff09\uff1a%2")
            .arg(QString::number(exitCode), head);
        telemetry_.logger().warning(
            QStringLiteral("Value board provider failed: exit=%1 stderr=%2")
                .arg(QString::number(exitCode), head).toStdString());
        emit valueStatusChanged();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u89e3\u6790\u811a\u672c\u8f93\u51fa\u5931\u8d25 %1").arg(parseError.errorString());
        telemetry_.logger().warning(valueStatus_.toStdString());
        emit valueStatusChanged();
        return;
    }

    const QJsonObject payload = document.object();
    if (selectedInstitutionId_ != institutionId)
    {
        if (currentPage_ == ValueBoardPage && !selectedInstitutionId_.isEmpty())
        {
            requestValueBoard(selectedInstitutionId_, selectedInstitutionName_);
        }
        return;
    }

    const ValueBoardSnapshot snapshot = parseValueBoard(institutionId, institutionName, payload);
    if (snapshot.funds.isEmpty() && snapshot.stocks.isEmpty())
    {
        valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u8fd4\u56de\u4e3a\u7a7a\uff0c\u4fdd\u7559\u73b0\u6709\u6570\u636e");
        emit valueStatusChanged();
        return;
    }

    const QDateTime updatedAt = QDateTime::currentDateTime();
    saveValueBoardToDatabase(snapshot, updatedAt);
    valueLoadedFromStorage_ = true;
    applyValueBoard(snapshot, updatedAt, true);
}

void MarketBoardController::seedUsHistoryFromProvider()
{
    // After bootstrap the per-second Tencent quote endpoint provides only the
    // latest snapshot price. Outside US trading hours the price is constant,
    // which makes the realtime sparkline a flat line. Run the akshare-backed
    // history provider once at startup to populate usQuoteHistory_ with the
    // most recent intraday minute curve so the sparkline shows real movement
    // immediately, while live updates continue to append per-second points.
    if (usHistoryRequestInFlight_ || usHistorySeeded_)
    {
        return;
    }
    if (valueBoardProviderCommand_.isEmpty() || usHistoryProviderScriptPath_.isEmpty())
    {
        return;
    }
    if (!QFileInfo::exists(usHistoryProviderScriptPath_))
    {
        return;
    }

    QStringList symbols;
    for (const UsMarketWatchEntry& entry : usWatchEntries_)
    {
        symbols << entry.symbol;
    }
    if (symbols.isEmpty())
    {
        return;
    }

    usHistoryRequestInFlight_ = true;

    auto* process = new QProcess(this);
    QStringList arguments;
    if (valueBoardProviderCommand_ == QStringLiteral("py"))
    {
        arguments << QStringLiteral("-3");
    }
    arguments
        << usHistoryProviderScriptPath_
        << QStringLiteral("--symbols") << symbols.join(',')
        << QStringLiteral("--lookback") << QStringLiteral("60")
        << QStringLiteral("--json");
    process->setWorkingDirectory(QFileInfo(usHistoryProviderScriptPath_).absolutePath());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    telemetry_.logger().information(
        QStringLiteral("Seeding US history via python: %1 %2")
            .arg(valueBoardProviderCommand_, arguments.join(QStringLiteral(" ")))
            .toStdString());

    QPointer<MarketBoardController> guard(this);
    connect(process, &QProcess::finished, this,
        [this, guard, process](int exitCode, QProcess::ExitStatus)
    {
        const QString stdoutText = QString::fromUtf8(process->readAllStandardOutput());
        const QString stderrText = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        if (!guard)
        {
            return;
        }
        handleUsHistoryProviderResult(exitCode, stdoutText, stderrText);
    });

    process->start(valueBoardProviderCommand_, arguments);
    if (!process->waitForStarted(3000))
    {
        process->deleteLater();
        usHistoryRequestInFlight_ = false;
        telemetry_.logger().warning("US history provider failed to start.");
    }

    QTimer::singleShot(60 * 1000, process, [process]()
    {
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
        }
    });
}

void MarketBoardController::handleUsHistoryProviderResult(
    int exitCode,
    const QString& stdoutText,
    const QString& stderrText)
{
    usHistoryRequestInFlight_ = false;
    if (exitCode != 0)
    {
        telemetry_.logger().warning(
            QStringLiteral("US history provider exit %1: %2")
                .arg(exitCode).arg(stderrText.left(200))
                .toStdString());
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        telemetry_.logger().warning(
            QStringLiteral("US history provider parse error: %1").arg(parseError.errorString())
                .toStdString());
        return;
    }

    const QJsonObject series = document.object().value(QStringLiteral("series")).toObject();
    int seededSymbols = 0;
    int seededPoints = 0;
    for (auto it = series.constBegin(); it != series.constEnd(); ++it)
    {
        const QString symbol = it.key();
        const QJsonObject obj = it.value().toObject();
        const QJsonArray timestamps = obj.value(QStringLiteral("timestamps")).toArray();
        const QJsonArray prices = obj.value(QStringLiteral("prices")).toArray();
        if (timestamps.size() != prices.size() || timestamps.isEmpty())
        {
            continue;
        }

        QVector<QPair<qint64, double>> history;
        history.reserve(timestamps.size());
        for (int i = 0; i < timestamps.size(); ++i)
        {
            const qint64 ts = static_cast<qint64>(timestamps.at(i).toDouble());
            const double price = prices.at(i).toDouble();
            if (price <= 0.0)
            {
                continue;
            }
            history.push_back({ts, price});
        }
        if (history.isEmpty())
        {
            continue;
        }

        usQuoteHistory_.insert(symbol, history);
        ++seededSymbols;
        seededPoints += history.size();
    }

    usHistorySeeded_ = seededSymbols > 0;
    telemetry_.logger().information(
        QStringLiteral("US history seeded: %1 symbols, %2 points total")
            .arg(seededSymbols).arg(seededPoints).toStdString());

    // Force a refresh so the sparkline column reflects the seeded history
    // even before the next per-second Tencent tick.
    syncUsMarketModel();
}

void MarketBoardController::requestValueBoard(
    const QString& institutionId,
    const QString& institutionName)
{
    if (valueRequestInFlight_)
    {
        return;
    }

    valueRequestInFlight_ = true;
    valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u6b63\u5728\u66f4\u65b0");
    emit valueStatusChanged();

    // Prefer the real-data Python provider. Codex / LLM fallback only kicks
    // in when the script is not configured or the file is missing.
    if (requestValueBoardViaPython(institutionId, institutionName))
    {
        return;
    }

    const QString prompt = QStringLiteral(
        "Today is %1. Build a value-investment board for users entering from the wealth institution '%2'. "
        "Return 10 funds and 5 stocks that fit a long-term value-investment style. "
        "Use Mandarin Chinese for display text, ASCII ids, and concise professional analysis. "
        "For each asset provide: score, latest price or net value, one-year return percentage, "
        "a 24-point one-year normalized trend series, a 12-point recent one-hour drawdown series, "
        "investment value analysis, and a six-month forecast. "
        "Keep the result UI-friendly and do not include markdown.")
        .arg(currentDateLabel(), institutionName);

    runCodexStructuredTask(
        QStringLiteral("value-board"),
        prompt,
        valueBoardSchema(),
        [this, institutionId, institutionName](const QJsonObject& payload)
        {
            valueRequestInFlight_ = false;
            if (selectedInstitutionId_ != institutionId)
            {
                if (currentPage_ == ValueBoardPage && !selectedInstitutionId_.isEmpty())
                {
                    requestValueBoard(selectedInstitutionId_, selectedInstitutionName_);
                }
                return;
            }

            const ValueBoardSnapshot snapshot =
                parseValueBoard(institutionId, institutionName, payload);
            if (snapshot.funds.isEmpty() && snapshot.stocks.isEmpty())
            {
                valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\uff1a\u8fd4\u56de\u4e3a\u7a7a\uff0c\u4fdd\u7559\u73b0\u6709\u6570\u636e");
                emit valueStatusChanged();
                return;
            }

            const QDateTime updatedAt = QDateTime::currentDateTime();
            saveValueBoardToDatabase(snapshot, updatedAt);
            valueLoadedFromStorage_ = true;
            applyValueBoard(snapshot, updatedAt, true);
        },
        [this](const QString& error)
        {
            valueRequestInFlight_ = false;
            valueStatus_ = QStringLiteral(u"\u4ef7\u503c\u6295\u8d44\u699c\u66f4\u65b0\u5931\u8d25\uff1a%1").arg(error);
            emit valueStatusChanged();
        });
}

void MarketBoardController::startConfigSubscription()
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

void MarketBoardController::handleConfigMessage(const QString& payload)
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
        target != QStringLiteral("market-board") &&
        target != QStringLiteral("*") &&
        target != QString::fromStdString(telemetry_.identity().name))
    {
        return;
    }

    const QString key = object.value("key").toString().trimmed();
    const QString valueText = object.value("value").toVariant().toString().trimmed();
    if (key.isEmpty())
    {
        return;
    }

    applyConfigValue(key, valueText);
}

void MarketBoardController::applyConfigValue(const QString& key, const QString& valueText)
{
    bool ok = false;
    const int value = valueText.toInt(&ok);
    if (!ok || value <= 0)
    {
        return;
    }

    if (key == QStringLiteral("update.institutions.intervalMs"))
    {
        institutionRefreshIntervalMs_ = value;
        institutionRefreshTimer_->setInterval(institutionRefreshIntervalMs_);
    }
    else if (key == QStringLiteral("update.valueBoard.intervalMs"))
    {
        valueRefreshIntervalMs_ = value;
        valueRefreshTimer_->setInterval(valueRefreshIntervalMs_);
    }
    else if (key == QStringLiteral("update.usMarket.intervalMs"))
    {
        usMarketRefreshIntervalMs_ = value;
        usMarketTimer_->setInterval(usMarketRefreshIntervalMs_);
    }
    else if (key == QStringLiteral("update.freeData.intervalMs"))
    {
        freeDataRefreshIntervalMs_ = value;
        if (freeDataRefreshTimer_)
        {
            freeDataRefreshTimer_->setInterval(freeDataRefreshIntervalMs_);
        }
    }
    else
    {
        return;
    }

    telemetry_.logger().information(
        QStringLiteral("Market board applied config %1=%2")
            .arg(key, QString::number(value))
            .toStdString());
}

void MarketBoardController::tickSelectedAssetRealtime()
{
    // Real-time asset detail prices must come from a market data source.
    // Until that feed is wired for this detail panel, keep the persisted value unchanged.
}

void MarketBoardController::startFreeDataProvider()
{
    if (freeDataRefreshTimer_ && freeDataRefreshIntervalMs_ > 0)
    {
        freeDataRefreshTimer_->start();
    }
    refreshFreeDataProvider();
}

void MarketBoardController::handleFreeDataProviderResult(
    int exitCode,
    const QString& stdoutText,
    const QString& stderrText)
{
    freeDataRequestInFlight_ = false;
    if (exitCode != 0)
    {
        freeDataStatus_ = QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u5237\u65b0\u5931\u8d25 %1")
            .arg(stderrText.left(180).trimmed());
        telemetry_.logger().warning(freeDataStatus_.toStdString());
        emit freeDataSnapshotChanged();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        freeDataStatus_ = QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u89e3\u6790\u5931\u8d25 %1")
            .arg(parseError.errorString());
        telemetry_.logger().warning(freeDataStatus_.toStdString());
        emit freeDataSnapshotChanged();
        return;
    }

    const QJsonObject object = document.object();
    freeDataSnapshot_ = object.toVariantMap();
    const int flowCount = object.value(QStringLiteral("sectorMoneyFlow")).toArray().size();
    const int announcementCount = object.value(QStringLiteral("announcements")).toArray().size();
    const int errorCount = object.value(QStringLiteral("errors")).toArray().size();
    freeDataStatus_ = QStringLiteral(u"\u514d\u8d39\u6570\u636e\u6e90\uff1a\u5df2\u5237\u65b0 \u884c\u4e1a\u8d44\u91d1 %1 \u6761 / \u516c\u544a %2 \u6761%3")
        .arg(flowCount)
        .arg(announcementCount)
        .arg(errorCount > 0 ? QStringLiteral(u" / \u90e8\u5206\u6e90\u5931\u8d25") : QString());
    telemetry_.logger().information(freeDataStatus_.toStdString());
    emit freeDataSnapshotChanged();
}

void MarketBoardController::runCodexStructuredTask(
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

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString prefix = QStringLiteral("%1-%2").arg(normalized_id(operationName), timestamp);
    const QString promptPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-prompt.txt"));
    const QString schemaPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-schema.json"));
    const QString outputPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-output.json"));
    const QString stdoutPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-stdout.log"));
    const QString stderrPath = QDir(artifactDirectory).filePath(prefix + QStringLiteral("-stderr.log"));

    if (!writeTextArtifact(promptPath, buildCodexPrompt(prompt)))
    {
        onError(QStringLiteral(u"\u5199\u5165\u5206\u6790\u4efb\u52a1\u5931\u8d25"));
        return;
    }

    if (!writeJsonArtifact(schemaPath, schema))
    {
        onError(QStringLiteral(u"\u5199\u5165\u5206\u6790\u7ea6\u675f\u5931\u8d25"));
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

    const QPointer<QProcess> processGuard(process);
    QTimer::singleShot(codexTimeoutMs_, this, [processGuard]()
    {
        if (processGuard && processGuard->state() != QProcess::NotRunning)
        {
            processGuard->setProperty("timedOut", true);
            processGuard->kill();
        }
    });

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, operationName, outputPath, stdoutPath, stderrPath, onSuccess, onError](
            int exitCode,
            QProcess::ExitStatus exitStatus)
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
                const QString summary = summarizeCodexFailure(stdoutText, stderrText);
                telemetry_.logger().warning(
                    QStringLiteral("Market board Codex CLI task \"%1\" failed: %2")
                        .arg(operationName, summary)
                        .toStdString());
                onError(summary);
                return;
            }

            QFile outputFile(outputPath);
            if (!outputFile.open(QIODevice::ReadOnly))
            {
                onError(QStringLiteral(u"\u672a\u751f\u6210\u5206\u6790\u7ed3\u679c"));
                return;
            }

            const QJsonDocument structuredDocument = QJsonDocument::fromJson(outputFile.readAll());
            if (!structuredDocument.isObject())
            {
                onError(QStringLiteral(u"\u5206\u6790\u7ed3\u679c\u89e3\u6790\u5931\u8d25"));
                return;
            }

            onSuccess(structuredDocument.object());
        });

    process->start();
    if (!process->waitForStarted(3000))
    {
        const QString startError = process->errorString();
        process->deleteLater();
        onError(QStringLiteral(u"\u65e0\u6cd5\u542f\u52a8\u5206\u6790\u4efb\u52a1\uff1a%1").arg(startError));
    }
}

QString MarketBoardController::resolveCodexCommand() const
{
    if (!codexCommand_.trimmed().isEmpty())
    {
        return codexCommand_.trimmed();
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

QString MarketBoardController::codexArtifactsDirectoryPath() const
{
    return QDir(cacheDirectoryPath()).filePath(QStringLiteral("codex-artifacts"));
}

QString MarketBoardController::buildCodexPrompt(const QString& prompt) const
{
    return QStringLiteral(
        "You generate structured market-board data for a Chinese desktop app.\n"
        "Return JSON only, matching the provided output schema exactly.\n"
        "Do not include markdown, code fences, explanations, citations, or extra keys.\n"
        "Do not inspect the repository, do not run shell commands, and do not use web search or plugins.\n"
        "Answer directly from model knowledge and keep the response compact.\n"
        "Focus on concise, professional Mandarin Chinese display text.\n\n"
        "Task:\n%1\n")
        .arg(prompt.trimmed());
}

QString MarketBoardController::readTextFile(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

QString MarketBoardController::summarizeCodexFailure(
    const QString& stdoutText,
    const QString& stderrText) const
{
    const QString combined = stderrText + QStringLiteral("\n") + stdoutText;
    const QStringList lines = combined.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);

    for (int index = lines.size() - 1; index >= 0; --index)
    {
        const QString line = lines.at(index).trimmed();
        if (line.startsWith(QStringLiteral("ERROR:")))
        {
            return line.mid(6).trimmed();
        }
    }

    for (int index = lines.size() - 1; index >= 0; --index)
    {
        const QString line = lines.at(index).trimmed();
        if (line.startsWith(QStringLiteral("{\"type\":\"error\"")) ||
            line.contains(QStringLiteral("invalid_request_error")) ||
            line.contains(QStringLiteral("not supported")) ||
            line.contains(QStringLiteral("not recognized")) ||
            line.contains(QStringLiteral("No such file")))
        {
            return line;
        }
    }

    for (int index = lines.size() - 1; index >= 0; --index)
    {
        const QString line = lines.at(index).trimmed();
        if (!line.isEmpty() &&
            !line.startsWith(QStringLiteral("202")) &&
            !line.startsWith(QStringLiteral("OpenAI Codex")) &&
            !line.startsWith(QStringLiteral("--------")) &&
            !line.startsWith(QStringLiteral("workdir:")) &&
            !line.startsWith(QStringLiteral("model:")) &&
            !line.startsWith(QStringLiteral("provider:")) &&
            !line.startsWith(QStringLiteral("approval:")) &&
            !line.startsWith(QStringLiteral("sandbox:")) &&
            !line.startsWith(QStringLiteral("reasoning")) &&
            !line.startsWith(QStringLiteral("session id:")) &&
            line != QStringLiteral("user") &&
            line != QStringLiteral("codex") &&
            line != QStringLiteral("tokens used"))
        {
            return line;
        }
    }

    return QStringLiteral(u"\u5206\u6790\u5931\u8d25");
}

bool MarketBoardController::writeTextArtifact(const QString& path, const QString& text) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    file.write(text.toUtf8());
    return file.commit();
}

bool MarketBoardController::writeJsonArtifact(const QString& path, const QJsonObject& object) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject MarketBoardController::institutionSchema() const
{
    const QJsonObject itemSchema{
        {"type", "object"},
        {"additionalProperties", false},
        {"properties", QJsonObject{
            {"id", QJsonObject{{"type", "string"}}},
            {"rank", QJsonObject{{"type", "integer"}}},
            {"name", QJsonObject{{"type", "string"}}},
            {"commonEntry", QJsonObject{{"type", "string"}}},
            {"coreStrength", QJsonObject{{"type", "string"}}},
            {"targetAudience", QJsonObject{{"type", "string"}}},
            {"score", QJsonObject{{"type", "number"}, {"minimum", 0}, {"maximum", 100}}}
        }},
        {"required", QJsonArray{"id", "rank", "name", "commonEntry", "coreStrength", "targetAudience", "score"}}
    };

    return QJsonObject{
        {"type", "object"},
        {"additionalProperties", false},
        {"properties", QJsonObject{
            {"institutions", QJsonObject{
                {"type", "array"},
                {"minItems", 10},
                {"maxItems", 10},
                {"items", itemSchema}
            }}
        }},
        {"required", QJsonArray{"institutions"}}
    };
}

QJsonObject MarketBoardController::valueBoardSchema() const
{
    const QJsonObject assetSchema{
        {"type", "object"},
        {"additionalProperties", false},
        {"properties", QJsonObject{
            {"id", QJsonObject{{"type", "string"}}},
            {"rank", QJsonObject{{"type", "integer"}}},
            {"name", QJsonObject{{"type", "string"}}},
            {"code", QJsonObject{{"type", "string"}}},
            {"provider", QJsonObject{{"type", "string"}}},
            {"category", QJsonObject{{"type", "string"}}},
            {"score", QJsonObject{{"type", "number"}, {"minimum", 0}, {"maximum", 100}}},
            {"latestPrice", QJsonObject{{"type", "number"}}},
            {"oneYearReturnPct", QJsonObject{{"type", "number"}}},
            {"investmentAnalysis", QJsonObject{{"type", "string"}}},
            {"sixMonthForecast", QJsonObject{{"type", "string"}}},
            {"oneYearTrend", QJsonObject{
                {"type", "array"},
                {"minItems", 24},
                {"maxItems", 24},
                {"items", QJsonObject{{"type", "number"}}}
            }},
            {"oneHourDrawdown", QJsonObject{
                {"type", "array"},
                {"minItems", 12},
                {"maxItems", 12},
                {"items", QJsonObject{{"type", "number"}}}
            }}
        }},
        {"required", QJsonArray{
            "id",
            "rank",
            "name",
            "code",
            "provider",
            "category",
            "score",
            "latestPrice",
            "oneYearReturnPct",
            "investmentAnalysis",
            "sixMonthForecast",
            "oneYearTrend",
            "oneHourDrawdown"}}
    };

    return QJsonObject{
        {"type", "object"},
        {"additionalProperties", false},
        {"properties", QJsonObject{
            {"funds", QJsonObject{
                {"type", "array"},
                {"minItems", 10},
                {"maxItems", 10},
                {"items", assetSchema}
            }},
            {"stocks", QJsonObject{
                {"type", "array"},
                {"minItems", 5},
                {"maxItems", 5},
                {"items", assetSchema}
            }}
        }},
        {"required", QJsonArray{"funds", "stocks"}}
    };
}

QVector<InstitutionBoardEntry> MarketBoardController::parseInstitutions(const QJsonObject& payload) const
{
    QVector<InstitutionBoardEntry> entries;
    const QJsonArray institutions = payload.value("institutions").toArray();
    entries.reserve(institutions.size());

    for (const QJsonValue& value : institutions)
    {
        const QJsonObject object = value.toObject();
        InstitutionBoardEntry entry;
        entry.id = object.value("id").toString().trimmed();
        entry.rank = object.value("rank").toInt();
        entry.name = object.value("name").toString().trimmed();
        entry.commonEntry = object.value("commonEntry").toString().trimmed();
        entry.coreStrength = object.value("coreStrength").toString().trimmed();
        entry.targetAudience = object.value("targetAudience").toString().trimmed();
        entry.score = object.value("score").toDouble();

        if (entry.id.isEmpty())
        {
            entry.id = normalized_id(entry.name);
        }

        if (!entry.name.isEmpty())
        {
            entries.push_back(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const InstitutionBoardEntry& left, const InstitutionBoardEntry& right)
    {
        return left.rank < right.rank;
    });
    return entries;
}

ValueBoardSnapshot MarketBoardController::parseValueBoard(
    const QString& institutionId,
    const QString& institutionName,
    const QJsonObject& payload) const
{
    ValueBoardSnapshot snapshot;
    snapshot.institutionId = institutionId;
    snapshot.institutionName = institutionName;

    const auto loadAssets = [](const QJsonArray& array)
    {
        QVector<ValueAssetEntry> entries;
        entries.reserve(array.size());
        for (const QJsonValue& value : array)
        {
            ValueAssetEntry entry = asset_from_object(value.toObject());
            if (entry.id.isEmpty())
            {
                entry.id = normalized_id(entry.name + entry.code);
            }
            if (!entry.name.isEmpty())
            {
                entries.push_back(entry);
            }
        }

        std::sort(entries.begin(), entries.end(), [](const ValueAssetEntry& left, const ValueAssetEntry& right)
        {
            return left.rank < right.rank;
        });
        return entries;
    };

    snapshot.funds = loadAssets(payload.value("funds").toArray());
    snapshot.stocks = loadAssets(payload.value("stocks").toArray());
    return snapshot;
}

QString MarketBoardController::currentDateLabel() const
{
    return QDate::currentDate().toString(Qt::ISODate);
}

QString MarketBoardController::institutionStatusFromTimestamp(
    const QString& prefix,
    const QDateTime& updatedAt) const
{
    return QStringLiteral("%1\uff1a%2")
        .arg(prefix, updatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

QVariantList MarketBoardController::seriesToVariantList(const QVector<double>& values) const
{
    QVariantList list;
    list.reserve(values.size());
    for (double value : values)
    {
        list.push_back(value);
    }
    return list;
}
