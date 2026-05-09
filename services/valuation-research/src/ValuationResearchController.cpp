#include "ValuationResearchController.h"

#include "stok/services/common/LocalizationClient.h"
#include "stok/services/common/ServiceConfig.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVariantMap>
#include <algorithm>

namespace {

// Mirror of market-board's resolve_python_command() so the bundled
// embeddable interpreter is preferred over PATH.
QString resolve_python_command(const QString& configured)
{
    const QString trimmed = configured.trimmed();
    const QString appDir = QCoreApplication::applicationDirPath();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("py") ||
        trimmed == QStringLiteral("python") || trimmed == QStringLiteral("python3") ||
        trimmed == QStringLiteral("python.exe"))
    {
        const QStringList candidates = {
            QDir(appDir).filePath(QStringLiteral("python-embed/python.exe")),
            QDir(appDir).filePath(QStringLiteral("../python-embed/python.exe")),
            QDir(appDir).filePath(QStringLiteral("../../python-embed/python.exe")),
            QDir(appDir).filePath(QStringLiteral("../../../python-embed/python.exe")),
        };
        for (const QString& c : candidates)
        {
            const QString clean = QDir::cleanPath(c);
            if (QFileInfo::exists(clean))
            {
                return clean;
            }
        }
    }
    if (!trimmed.isEmpty())
    {
        const QString clean = QDir::cleanPath(trimmed);
        if (QFileInfo::exists(clean))
        {
            return clean;
        }
    }
    return trimmed.isEmpty() ? QStringLiteral("py") : trimmed;
}

QString resolve_script_path(const QString& configPath, const std::string& configured)
{
    const QString resolved = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(configPath.toStdString(), configured));
    if (QFileInfo::exists(resolved))
    {
        return resolved;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("../tools/valuation_research_provider.py")),
        QDir(appDir).filePath(QStringLiteral("../../tools/valuation_research_provider.py")),
        QDir(appDir).filePath(QStringLiteral("../../../tools/valuation_research_provider.py")),
    };
    for (const QString& c : candidates)
    {
        const QString clean = QDir::cleanPath(c);
        if (QFileInfo::exists(clean))
        {
            return clean;
        }
    }
    return resolved;
}

QVariantMap card(const QString& label, const QString& value, const QString& note, const QString& tone)
{
    return {{"label", label}, {"value", value}, {"note", note}, {"tone", tone}};
}

QString locTr(stok::services::common::LocalizationClient* loc, const QString& key)
{
    return loc ? loc->tr(key) : key;
}

void define(stok::services::common::LocalizationClient* loc, const QString& key,
            const QString& en, const QString& zh)
{
    if (loc) loc->define(key, en, zh);
}

QVariantMap rowToVariant(const QJsonObject& json, double buyMul, double sellMul)
{
    const double fair = json.value("fairValue").toDouble();
    QVariantMap row;
    row["symbol"] = json.value("symbol").toString();
    row["displayCode"] = json.value("displayCode").toString();
    row["name"] = json.value("name").toString();
    row["market"] = json.value("market").toString();
    row["industry"] = json.value("industry").toString();
    row["price"] = json.value("lastPrice").toDouble();
    row["lastPrice"] = json.value("lastPrice").toDouble();
    row["fairValue"] = fair;
    row["margin"] = json.value("marginOfSafety").toDouble();
    row["pe"] = json.value("pe").toDouble();
    row["pb"] = json.value("pb").toDouble();
    row["pePercentile"] = json.value("pePercentile").toDouble();
    row["pbPercentile"] = json.value("pbPercentile").toDouble();
    row["roe"] = json.value("roe").toDouble();
    row["roic"] = json.value("roic").toDouble();
    row["fcfQuality"] = json.value("fcfQuality").toDouble();
    row["dividendYield"] = json.value("dividendYield").toDouble();
    row["compositeScore"] = json.value("compositeScore").toDouble();
    row["action"] = json.value("action").toString();
    row["tone"] = json.value("tone").toString();
    row["rating"] = json.value("rating").toString();
    row["riskFlag"] = json.value("riskFlag").toString();
    row["methodology"] = json.value("methodology").toString();
    row["buyBelow"] = fair * buyMul;
    row["sellAbove"] = fair * sellMul;
    return row;
}

} // namespace


ValuationResearchController::ValuationResearchController(
    stok::services::common::LocalizationClient* localization,
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
    QString configPath,
    stok::services::common::ServiceTelemetry* telemetry,
    QObject* parent):
    QObject(parent),
    localization_(localization),
    configuration_(std::move(configuration)),
    configPath_(std::move(configPath)),
    telemetry_(telemetry),
    configSubscriber_(
        [&]() {
            auto settings = stok::services::common::readDdsSettings(*configuration_);
            settings.topicName = configuration_->getString("dds.configTopic", "stok.ui.config");
            return settings;
        }(),
        telemetry ? telemetry->client() : Poco::OpenTelemetry::TelemetryClient())
{
    if (localization_)
    {
        define(localization_, QStringLiteral("vr.action.followup"), QStringLiteral("Follow up"), QStringLiteral(u"跟进"));
        define(localization_, QStringLiteral("vr.action.add"), QStringLiteral("Add"), QStringLiteral(u"加仓"));
        define(localization_, QStringLiteral("vr.action.sell"), QStringLiteral("Sell"), QStringLiteral(u"卖出"));
        define(localization_, QStringLiteral("vr.action.rotate"), QStringLiteral("Rotate"), QStringLiteral(u"转仓"));
        define(localization_, QStringLiteral("vr.status.fmt"),
               QStringLiteral("Valuation models updated  %1"),
               QStringLiteral(u"估值模型已更新  %1"));
        define(localization_, QStringLiteral("vr.status.refreshing"),
               QStringLiteral("Recomputing valuation models..."),
               QStringLiteral(u"正在重新计算估值模型..."));
        define(localization_, QStringLiteral("vr.status.empty"),
               QStringLiteral("Waiting for first refresh"),
               QStringLiteral(u"首次抓取中，请稍候"));
        define(localization_, QStringLiteral("vr.status.fail.fmt"),
               QStringLiteral("Refresh failed: %1"),
               QStringLiteral(u"刷新失败：%1"));

        define(localization_, QStringLiteral("vr.card.margin.label"), QStringLiteral("Average margin of safety"), QStringLiteral(u"安全边际均值"));
        define(localization_, QStringLiteral("vr.card.margin.note"), QStringLiteral("DCF vs current price"), QStringLiteral(u"DCF 与当前价差"));
        define(localization_, QStringLiteral("vr.card.attractive.label"), QStringLiteral("Add-eligible names"), QStringLiteral(u"可加仓标的"));
        define(localization_, QStringLiteral("vr.card.attractive.note"), QStringLiteral("Margin, cash flow and valuation percentile all clear"), QStringLiteral(u"安全边际、现金流与估值分位同时达标"));
        define(localization_, QStringLiteral("vr.card.fcf.label"), QStringLiteral("FCF quality"), QStringLiteral(u"FCF 质量"));
        define(localization_, QStringLiteral("vr.card.fcf.note"), QStringLiteral("Cash flow coverage and stability"), QStringLiteral(u"现金流覆盖利润和稳定性"));
        define(localization_, QStringLiteral("vr.card.rotate.label"), QStringLiteral("Rotate / sell"), QStringLiteral(u"需转仓/卖出"));
        define(localization_, QStringLiteral("vr.card.rotate.note"), QStringLiteral("Overvalued or weak fundamentals"), QStringLiteral(u"高估值或基本面质量不足"));

        define(localization_, QStringLiteral("vr.thesis.business.title"), QStringLiteral("Business quality"), QStringLiteral(u"商业质量"));
        define(localization_, QStringLiteral("vr.thesis.business.detail"), QStringLiteral("Moat, pricing power, return on capital and cash conversion."), QStringLiteral(u"护城河、定价权、资本回报率和现金流转化率。"));
        define(localization_, QStringLiteral("vr.thesis.business.evidence"), QStringLiteral("Required to add"), QStringLiteral(u"通过后才可加仓"));
        define(localization_, QStringLiteral("vr.thesis.valuation.title"), QStringLiteral("Valuation discipline"), QStringLiteral(u"估值纪律"));
        define(localization_, QStringLiteral("vr.thesis.valuation.detail"), QStringLiteral("DCF, PE/PB percentile and dividend yield jointly confirm buy/sell triggers."), QStringLiteral(u"DCF、PE/PB 分位和股息率共同确认买入/卖出线。"));
        define(localization_, QStringLiteral("vr.thesis.valuation.evidence"), QStringLiteral("Act only below buy line"), QStringLiteral(u"低于买入线才行动"));
        define(localization_, QStringLiteral("vr.thesis.invalidation.title"), QStringLiteral("Invalidation"), QStringLiteral(u"失效条件"));
        define(localization_, QStringLiteral("vr.thesis.invalidation.detail"), QStringLiteral("Falling ROIC, deteriorating FCF, or repeated guidance cuts."), QStringLiteral(u"ROIC 下滑、FCF 恶化或管理层指引连续下修。"));
        define(localization_, QStringLiteral("vr.thesis.invalidation.evidence"), QStringLiteral("Triggers sell/rotate"), QStringLiteral(u"触发卖出/转仓"));

        define(localization_, QStringLiteral("action.refresh"), QStringLiteral("Refresh"), QStringLiteral(u"刷新"));
        define(localization_, QStringLiteral("vr.section.matrix"), QStringLiteral("Valuation matrix"), QStringLiteral(u"估值模型矩阵"));
        define(localization_, QStringLiteral("vr.section.matrix.subtitle"), QStringLiteral("DCF / PE-PB percentile / ROE-ROIC / FCF"), QStringLiteral(u"DCF / PE-PB 分位 / ROE-ROIC / FCF"));
        define(localization_, QStringLiteral("vr.col.symbol"), QStringLiteral("Symbol"), QStringLiteral(u"代码"));
        define(localization_, QStringLiteral("vr.col.name"), QStringLiteral("Name"), QStringLiteral(u"名称"));
        define(localization_, QStringLiteral("vr.col.price"), QStringLiteral("Price"), QStringLiteral(u"现价"));
        define(localization_, QStringLiteral("vr.col.fair"), QStringLiteral("Fair value"), QStringLiteral(u"合理价"));
        define(localization_, QStringLiteral("vr.col.margin"), QStringLiteral("Margin"), QStringLiteral(u"安全边际"));
        define(localization_, QStringLiteral("vr.col.bands"), QStringLiteral("Buy / Sell"), QStringLiteral(u"买/卖线"));
        define(localization_, QStringLiteral("vr.col.action"), QStringLiteral("Action"), QStringLiteral(u"动作"));
        define(localization_, QStringLiteral("vr.section.research"), QStringLiteral("Research files"), QStringLiteral(u"研究档案"));
        define(localization_, QStringLiteral("vr.section.thesis"), QStringLiteral("Investment thesis"), QStringLiteral(u"投资 Thesis 流程"));
        define(localization_, QStringLiteral("vr.band.buy"), QStringLiteral("Buy"), QStringLiteral(u"买"));
        define(localization_, QStringLiteral("vr.band.sell"), QStringLiteral("Sell"), QStringLiteral(u"卖"));

        connect(localization_, &stok::services::common::LocalizationClient::languageChanged,
                this, [this]() { rebuildResearchAndThesis(); rebuildAggregates(); emit dataChanged(); });
    }

    discountRate_ = configuration_->getDouble("valuation.dcf.discountRate", 0.085);
    terminalGrowth_ = configuration_->getDouble("valuation.dcf.terminalGrowth", 0.025);
    growthRate_ = configuration_->getDouble("valuation.dcf.growthRate", 0.06);
    horizonYears_ = configuration_->getInt("valuation.dcf.horizonYears", 10);
    historyYears_ = configuration_->getInt("valuation.dcf.historyYears", 8);
    buyMultiplier_ = configuration_->getDouble("valuation.bands.buyMultiplier", 0.85);
    sellMultiplier_ = configuration_->getDouble("valuation.bands.sellMultiplier", 1.18);
    refreshIntervalMs_ = configuration_->getInt("update.valuation.intervalMs", 30 * 60 * 1000);
    providerTimeoutMs_ = configuration_->getInt("valuation.provider.timeoutMs", 8 * 60 * 1000);
    providerCommand_ = resolve_python_command(QString::fromStdString(
        configuration_->getString("valuation.provider.command", "")));
    providerScriptPath_ = resolve_script_path(configPath_,
        configuration_->getString("valuation.provider.script", "../../tools/valuation_research_provider.py"));

    cacheDirectoryPath_ = QString::fromStdString(stok::services::common::resolveConfigRelativePath(
        configPath_.toStdString(),
        configuration_->getString("storage.cacheDir", "data/valuation-research")));
    databasePath_ = QString::fromStdString(stok::services::common::resolveConfigRelativePath(
        configPath_.toStdString(),
        configuration_->getString("storage.sqlitePath", "data/valuation-research/valuation-research.db")));
    databaseConnectionName_ = QStringLiteral("valuation-research-%1").arg(reinterpret_cast<qulonglong>(this));

    QDir().mkpath(cacheDirectoryPath_);
    QDir().mkpath(QFileInfo(databasePath_).absolutePath());
    initializeDatabase();
    seedWatchlist();

    rebuildResearchAndThesis();
    loadFromDatabase();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(refreshIntervalMs_);
    connect(refreshTimer_, &QTimer::timeout, this, &ValuationResearchController::runProvider);
    refreshTimer_->start();

    if (telemetry_)
    {
        telemetry_->logger().information(
            QStringLiteral("Valuation research: python=%1 script=%2 watchlist=%3")
                .arg(providerCommand_, providerScriptPath_, watchlist_.join(","))
                .toStdString());
    }

    std::string subErr;
    configSubscriber_.start(
        (telemetry_ ? telemetry_->identity().name : std::string("stok.valuation-research")) + "-config",
        [this](const stok::services::common::TextMessage& message)
        {
            const QString payload = QString::fromUtf8(message.payload);
            QMetaObject::invokeMethod(this, [this, payload]() { handleConfigMessage(payload); }, Qt::QueuedConnection);
        },
        {},
        &subErr);

    QTimer::singleShot(2000, this, &ValuationResearchController::runProvider);
}

ValuationResearchController::~ValuationResearchController()
{
    configSubscriber_.stop();
    if (database_.isValid())
    {
        database_.close();
        QSqlDatabase::removeDatabase(databaseConnectionName_);
    }
}

QString ValuationResearchController::status() const { return status_; }
QVariantList ValuationResearchController::scoreCards() const { return scoreCards_; }
QVariantList ValuationResearchController::valuationRows() const { return valuationRows_; }
QVariantList ValuationResearchController::researchRows() const { return researchRows_; }
QVariantList ValuationResearchController::thesisRows() const { return thesisRows_; }
QStringList ValuationResearchController::watchlist() const { return watchlist_; }
bool ValuationResearchController::refreshing() const { return refreshing_; }

QString ValuationResearchController::lastUpdatedText() const
{
    if (!generatedAt_.isValid())
    {
        return locTr(localization_, QStringLiteral("vr.status.empty"));
    }
    return locTr(localization_, QStringLiteral("vr.status.fmt"))
        .arg(generatedAt_.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
}

QString ValuationResearchController::assumptionSummary() const
{
    return QStringLiteral("WACC %1%  g %2%  buy×%3  sell×%4")
        .arg(discountRate_ * 100, 0, 'f', 1)
        .arg(terminalGrowth_ * 100, 0, 'f', 1)
        .arg(buyMultiplier_, 0, 'f', 2)
        .arg(sellMultiplier_, 0, 'f', 2);
}

void ValuationResearchController::refresh()
{
    runProvider();
}

bool ValuationResearchController::addSymbol(const QString& spec)
{
    const QString trimmed = spec.trimmed().toUpper();
    if (trimmed.isEmpty() || !trimmed.contains(':') || watchlist_.contains(trimmed))
    {
        return false;
    }
    watchlist_.append(trimmed);
    writeWatchlist();
    emit watchlistChanged();
    runProvider();
    return true;
}

bool ValuationResearchController::removeSymbol(const QString& spec)
{
    const QString trimmed = spec.trimmed().toUpper();
    if (!watchlist_.removeOne(trimmed))
    {
        return false;
    }
    writeWatchlist();
    emit watchlistChanged();

    QSqlQuery q(database_);
    q.prepare("DELETE FROM valuation_assets WHERE spec = :s");
    q.bindValue(":s", trimmed);
    q.exec();

    loadFromDatabase();
    return true;
}

void ValuationResearchController::initializeDatabase()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
    {
        return;
    }
    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), databaseConnectionName_);
    database_.setDatabaseName(databasePath_);
    if (!database_.open())
    {
        return;
    }
    ensureSchema();
}

void ValuationResearchController::ensureSchema()
{
    const QStringList ddl = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS valuation_assets ("
            "spec TEXT PRIMARY KEY,"
            "symbol TEXT, display_code TEXT, name TEXT, market TEXT, industry TEXT,"
            "last_price REAL, fair_value REAL, margin REAL,"
            "pe REAL, pb REAL, pe_percentile REAL, pb_percentile REAL,"
            "roe REAL, roic REAL, fcf_quality REAL, dividend_yield REAL,"
            "composite_score REAL, action TEXT, tone TEXT, rating TEXT,"
            "risk_flag TEXT, methodology TEXT, generated_at TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS valuation_watchlist ("
            "spec TEXT PRIMARY KEY, position INTEGER, added_at TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS valuation_meta ("
            "key TEXT PRIMARY KEY, value TEXT)"),
    };
    for (const QString& sql : ddl)
    {
        QSqlQuery q(database_);
        q.exec(sql);
    }
}

void ValuationResearchController::seedWatchlist()
{
    watchlist_ = loadWatchlist();
    if (!watchlist_.isEmpty())
    {
        return;
    }
    const QString defaults = QString::fromStdString(
        configuration_->getString("watchlist.default",
            "A:600519,A:000333,A:600036,A:000651,A:601318,A:601398,A:600900,A:000858,HK:00700,HK:00941"));
    for (const QString& s : defaults.split(',', Qt::SkipEmptyParts))
    {
        const QString trimmed = s.trimmed().toUpper();
        if (!trimmed.isEmpty())
        {
            watchlist_.append(trimmed);
        }
    }
    writeWatchlist();
}

QStringList ValuationResearchController::loadWatchlist() const
{
    QStringList out;
    if (!database_.isValid()) return out;
    QSqlQuery q(database_);
    if (q.exec("SELECT spec FROM valuation_watchlist ORDER BY position ASC"))
    {
        while (q.next())
        {
            out.append(q.value(0).toString());
        }
    }
    return out;
}

void ValuationResearchController::writeWatchlist()
{
    if (!database_.isValid()) return;
    database_.transaction();
    QSqlQuery clear(database_); clear.exec("DELETE FROM valuation_watchlist");
    QSqlQuery ins(database_);
    ins.prepare("INSERT INTO valuation_watchlist(spec, position, added_at) VALUES(?, ?, ?)");
    for (int i = 0; i < watchlist_.size(); ++i)
    {
        ins.bindValue(0, watchlist_.at(i));
        ins.bindValue(1, i);
        ins.bindValue(2, QDateTime::currentDateTime().toString(Qt::ISODate));
        ins.exec();
    }
    database_.commit();
}

void ValuationResearchController::loadFromDatabase()
{
    valuationRows_.clear();
    if (!database_.isValid())
    {
        rebuildAggregates();
        emit dataChanged();
        return;
    }
    QSqlQuery q(database_);
    q.exec("SELECT spec, symbol, display_code, name, market, industry, last_price, fair_value, margin, "
           "pe, pb, pe_percentile, pb_percentile, roe, roic, fcf_quality, dividend_yield, "
           "composite_score, action, tone, rating, risk_flag, methodology, generated_at "
           "FROM valuation_assets ORDER BY composite_score DESC");
    while (q.next())
    {
        QVariantMap row;
        row["spec"] = q.value(0).toString();
        row["symbol"] = q.value(1).toString();
        row["displayCode"] = q.value(2).toString();
        row["name"] = q.value(3).toString();
        row["market"] = q.value(4).toString();
        row["industry"] = q.value(5).toString();
        row["price"] = q.value(6).toDouble();
        row["lastPrice"] = q.value(6).toDouble();
        const double fair = q.value(7).toDouble();
        row["fairValue"] = fair;
        row["margin"] = q.value(8).toDouble();
        row["pe"] = q.value(9).toDouble();
        row["pb"] = q.value(10).toDouble();
        row["pePercentile"] = q.value(11).toDouble();
        row["pbPercentile"] = q.value(12).toDouble();
        row["roe"] = q.value(13).toDouble();
        row["roic"] = q.value(14).toDouble();
        row["fcfQuality"] = q.value(15).toDouble();
        row["dividendYield"] = q.value(16).toDouble();
        row["compositeScore"] = q.value(17).toDouble();
        row["action"] = q.value(18).toString();
        row["tone"] = q.value(19).toString();
        row["rating"] = q.value(20).toString();
        row["riskFlag"] = q.value(21).toString();
        row["methodology"] = q.value(22).toString();
        row["buyBelow"] = fair * buyMultiplier_;
        row["sellAbove"] = fair * sellMultiplier_;
        valuationRows_.append(row);
    }
    QSqlQuery meta(database_);
    meta.exec("SELECT value FROM valuation_meta WHERE key = 'generated_at'");
    if (meta.next())
    {
        generatedAt_ = QDateTime::fromString(meta.value(0).toString(), Qt::ISODate);
    }
    rebuildAggregates();
    emit dataChanged();
}

void ValuationResearchController::saveToDatabase(const QVariantList& rows, const QDateTime& generatedAt)
{
    if (!database_.isValid()) return;
    database_.transaction();
    QSqlQuery del(database_); del.exec("DELETE FROM valuation_assets");
    QSqlQuery ins(database_);
    ins.prepare(
        "INSERT INTO valuation_assets(spec, symbol, display_code, name, market, industry, "
        "last_price, fair_value, margin, pe, pb, pe_percentile, pb_percentile, roe, roic, "
        "fcf_quality, dividend_yield, composite_score, action, tone, rating, risk_flag, "
        "methodology, generated_at) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    for (const QVariant& v : rows)
    {
        const QVariantMap row = v.toMap();
        const QString spec = QStringLiteral("%1:%2").arg(row.value("market").toString(),
                                                          row.value("symbol").toString());
        ins.bindValue(0, spec);
        ins.bindValue(1, row.value("symbol"));
        ins.bindValue(2, row.value("displayCode"));
        ins.bindValue(3, row.value("name"));
        ins.bindValue(4, row.value("market"));
        ins.bindValue(5, row.value("industry"));
        ins.bindValue(6, row.value("lastPrice"));
        ins.bindValue(7, row.value("fairValue"));
        ins.bindValue(8, row.value("margin"));
        ins.bindValue(9, row.value("pe"));
        ins.bindValue(10, row.value("pb"));
        ins.bindValue(11, row.value("pePercentile"));
        ins.bindValue(12, row.value("pbPercentile"));
        ins.bindValue(13, row.value("roe"));
        ins.bindValue(14, row.value("roic"));
        ins.bindValue(15, row.value("fcfQuality"));
        ins.bindValue(16, row.value("dividendYield"));
        ins.bindValue(17, row.value("compositeScore"));
        ins.bindValue(18, row.value("action"));
        ins.bindValue(19, row.value("tone"));
        ins.bindValue(20, row.value("rating"));
        ins.bindValue(21, row.value("riskFlag"));
        ins.bindValue(22, row.value("methodology"));
        ins.bindValue(23, generatedAt.toString(Qt::ISODate));
        ins.exec();
    }
    QSqlQuery meta(database_);
    meta.prepare("INSERT OR REPLACE INTO valuation_meta(key, value) VALUES('generated_at', ?)");
    meta.bindValue(0, generatedAt.toString(Qt::ISODate));
    meta.exec();
    database_.commit();
}

void ValuationResearchController::runProvider()
{
    if (refreshing_) return;
    if (providerCommand_.isEmpty() || providerScriptPath_.isEmpty()) return;
    if (!QFileInfo::exists(providerScriptPath_)) return;
    if (watchlist_.isEmpty()) return;

    refreshing_ = true;
    status_ = locTr(localization_, QStringLiteral("vr.status.refreshing"));
    emit refreshingChanged();
    emit dataChanged();

    auto* process = new QProcess(this);
    QStringList args;
    if (providerCommand_ == QStringLiteral("py")) args << QStringLiteral("-3");
    args << providerScriptPath_
         << QStringLiteral("--symbols") << watchlist_.join(',')
         << QStringLiteral("--discount-rate") << QString::number(discountRate_, 'f', 4)
         << QStringLiteral("--terminal-growth") << QString::number(terminalGrowth_, 'f', 4)
         << QStringLiteral("--growth-rate") << QString::number(growthRate_, 'f', 4)
         << QStringLiteral("--horizon-years") << QString::number(horizonYears_)
         << QStringLiteral("--history-years") << QString::number(historyYears_)
         << QStringLiteral("--buy-multiplier") << QString::number(buyMultiplier_, 'f', 3)
         << QStringLiteral("--sell-multiplier") << QString::number(sellMultiplier_, 'f', 3)
         << QStringLiteral("--json");
    process->setWorkingDirectory(QFileInfo(providerScriptPath_).absolutePath());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);

    if (telemetry_)
    {
        telemetry_->logger().information(
            QStringLiteral("Valuation provider: %1 %2").arg(providerCommand_, args.join(' ')).toStdString());
    }

    QPointer<ValuationResearchController> guard(this);
    connect(process, &QProcess::finished, this,
        [this, guard, process](int code, QProcess::ExitStatus)
    {
        const QString out = QString::fromUtf8(process->readAllStandardOutput());
        const QString err = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        if (!guard) return;
        onProviderFinished(code, out, err);
    });
    process->start(providerCommand_, args);
    if (!process->waitForStarted(3000))
    {
        process->deleteLater();
        refreshing_ = false;
        status_ = locTr(localization_, QStringLiteral("vr.status.fail.fmt")).arg(QStringLiteral("start failed"));
        emit refreshingChanged();
        emit dataChanged();
        return;
    }
    QTimer::singleShot(providerTimeoutMs_, process, [process]()
    {
        if (process->state() != QProcess::NotRunning) process->kill();
    });
}

void ValuationResearchController::onProviderFinished(int exitCode, const QString& stdoutText, const QString& stderrText)
{
    refreshing_ = false;
    emit refreshingChanged();
    if (exitCode != 0)
    {
        status_ = locTr(localization_, QStringLiteral("vr.status.fail.fmt"))
            .arg(QStringLiteral("exit %1, %2").arg(exitCode).arg(stderrText.left(120)));
        emit dataChanged();
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        status_ = locTr(localization_, QStringLiteral("vr.status.fail.fmt")).arg(parseError.errorString());
        emit dataChanged();
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonArray rows = root.value("rows").toArray();
    QVariantList parsed;
    for (const QJsonValue& v : rows)
    {
        if (!v.isObject()) continue;
        parsed.append(rowToVariant(v.toObject(), buyMultiplier_, sellMultiplier_));
    }
    if (parsed.isEmpty())
    {
        // Keep previous data so the UI does not blank out.
        status_ = locTr(localization_, QStringLiteral("vr.status.fail.fmt")).arg(QStringLiteral("empty result"));
        emit dataChanged();
        return;
    }
    valuationRows_ = parsed;
    generatedAt_ = QDateTime::currentDateTime();
    saveToDatabase(parsed, generatedAt_);
    rebuildAggregates();
    status_ = locTr(localization_, QStringLiteral("vr.status.fmt")).arg(generatedAt_.toString(QStringLiteral("HH:mm:ss")));
    emit dataChanged();
}

void ValuationResearchController::recomputeBands()
{
    for (QVariant& v : valuationRows_)
    {
        QVariantMap m = v.toMap();
        const double fair = m.value("fairValue").toDouble();
        m["buyBelow"] = fair * buyMultiplier_;
        m["sellAbove"] = fair * sellMultiplier_;
        v = m;
    }
}

void ValuationResearchController::rebuildAggregates()
{
    auto* loc = localization_.data();
    double totalMargin = 0.0;
    double totalFcf = 0.0;
    int attractive = 0;
    int needRotate = 0;
    const int n = valuationRows_.size();
    for (const QVariant& v : valuationRows_)
    {
        const QVariantMap m = v.toMap();
        totalMargin += m.value("margin").toDouble();
        totalFcf += m.value("fcfQuality").toDouble();
        const QString tone = m.value("tone").toString();
        if (tone == QStringLiteral("green")) ++attractive;
        else if (tone == QStringLiteral("red") || tone == QStringLiteral("amber")) ++needRotate;
    }
    const double avgMargin = n > 0 ? totalMargin / n : 0.0;
    const double avgFcf = n > 0 ? totalFcf / n : 0.0;

    scoreCards_ = {
        card(locTr(loc, QStringLiteral("vr.card.margin.label")),
             QStringLiteral("%1%").arg(avgMargin, 0, 'f', 1),
             locTr(loc, QStringLiteral("vr.card.margin.note")),
             avgMargin >= 0.0 ? QStringLiteral("green") : QStringLiteral("red")),
        card(locTr(loc, QStringLiteral("vr.card.attractive.label")),
             QStringLiteral("%1").arg(attractive),
             locTr(loc, QStringLiteral("vr.card.attractive.note")),
             attractive > 0 ? QStringLiteral("green") : QStringLiteral("amber")),
        card(locTr(loc, QStringLiteral("vr.card.fcf.label")),
             QStringLiteral("%1").arg(avgFcf, 0, 'f', 0),
             locTr(loc, QStringLiteral("vr.card.fcf.note")), QStringLiteral("blue")),
        card(locTr(loc, QStringLiteral("vr.card.rotate.label")),
             QStringLiteral("%1").arg(needRotate),
             locTr(loc, QStringLiteral("vr.card.rotate.note")), QStringLiteral("amber")),
    };
    if (status_.isEmpty())
    {
        status_ = lastUpdatedText();
    }
}

void ValuationResearchController::rebuildResearchAndThesis()
{
    auto* loc = localization_.data();
    researchRows_ = {
        QVariantMap{{"title", QStringLiteral(u"DCF 估值假设")},
                    {"detail", assumptionSummary()},
                    {"tag", QStringLiteral("DCF")},
                    {"evidence", QStringLiteral(u"运行时可由配置中心调节")}, {"tone", "blue"}},
        QVariantMap{{"title", QStringLiteral(u"评分公式（透明权重）")},
                    {"detail", QStringLiteral(u"30% 安全边际 + 20% PE 分位 + 20% FCF 质量 + 15% ROE + 15% ROIC")},
                    {"tag", QStringLiteral(u"评分")},
                    {"evidence", QStringLiteral(u"权重写在 valuation_research_provider.py")}, {"tone", "green"}},
        QVariantMap{{"title", QStringLiteral(u"动作判定")},
                    {"detail", QStringLiteral(u"安全边际≥15% 且评分≥75 → 加仓；安全边际≤-12% 或评分≤40 → 卖出；其余跟进/转仓")},
                    {"tag", QStringLiteral(u"动作")},
                    {"evidence", QStringLiteral(u"非投资建议")}, {"tone", "amber"}},
        QVariantMap{{"title", QStringLiteral(u"数据来源")},
                    {"detail", QStringLiteral(u"A 股财务来自 AKShare stock_financial_abstract，行情来自腾讯 qt.gtimg.cn；港股 / 美股财务通过 AKShare 东财端口")},
                    {"tag", QStringLiteral(u"真实")},
                    {"evidence", QStringLiteral(u"无 LLM 生成数据")}, {"tone", "blue"}}
    };

    thesisRows_ = {
        QVariantMap{{"step", "1"}, {"title", locTr(loc, QStringLiteral("vr.thesis.business.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.thesis.business.detail"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.thesis.business.evidence"))}},
        QVariantMap{{"step", "2"}, {"title", locTr(loc, QStringLiteral("vr.thesis.valuation.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.thesis.valuation.detail"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.thesis.valuation.evidence"))}},
        QVariantMap{{"step", "3"}, {"title", locTr(loc, QStringLiteral("vr.thesis.invalidation.title"))},
                    {"detail", locTr(loc, QStringLiteral("vr.thesis.invalidation.detail"))},
                    {"evidence", locTr(loc, QStringLiteral("vr.thesis.invalidation.evidence"))}},
    };
}

void ValuationResearchController::handleConfigMessage(const QString& payload)
{
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(payload.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) return;
    const QJsonObject obj = d.object();
    if (obj.value("type").toString() != QStringLiteral("config_update")) return;
    const QString target = obj.value("target").toString().trimmed();
    if (!target.isEmpty() &&
        target != QStringLiteral("valuation-research") &&
        target != QStringLiteral("*"))
    {
        return;
    }
    applyConfigValue(obj.value("key").toString().trimmed(),
                     obj.value("value").toVariant().toString().trimmed());
}

void ValuationResearchController::applyConfigValue(const QString& key, const QString& value)
{
    bool ok = false;
    const double dbl = value.toDouble(&ok);
    bool changed = false;
    if (key == QStringLiteral("valuation.dcf.discountRate") && ok && dbl > 0)
    {
        discountRate_ = dbl; changed = true;
    }
    else if (key == QStringLiteral("valuation.dcf.terminalGrowth") && ok)
    {
        terminalGrowth_ = dbl; changed = true;
    }
    else if (key == QStringLiteral("valuation.bands.buyMultiplier") && ok && dbl > 0)
    {
        buyMultiplier_ = dbl; recomputeBands();
        rebuildResearchAndThesis();
        emit dataChanged();
        return;
    }
    else if (key == QStringLiteral("valuation.bands.sellMultiplier") && ok && dbl > 0)
    {
        sellMultiplier_ = dbl; recomputeBands();
        rebuildResearchAndThesis();
        emit dataChanged();
        return;
    }
    else if (key == QStringLiteral("update.valuation.intervalMs"))
    {
        const int v = value.toInt(&ok);
        if (ok && v > 0 && refreshTimer_)
        {
            refreshTimer_->setInterval(v);
            refreshIntervalMs_ = v;
        }
        return;
    }
    if (changed)
    {
        rebuildResearchAndThesis();
        runProvider();
    }
}

QObject* createValuationResearchController(
    const stok::services::feature_page::FeatureControllerContext& ctx)
{
    return new ValuationResearchController(
        ctx.localization,
        ctx.configuration,
        QString::fromStdString(ctx.configPath),
        ctx.telemetry,
        ctx.parent);
}
