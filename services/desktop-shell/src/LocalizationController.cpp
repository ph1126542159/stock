#include "LocalizationController.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

LocalizationController::LocalizationController(QObject* parent):
    QObject(parent)
{
    // Shell chrome
    english_.insert("app.title", QStringLiteral("Stok Investment Workbench"));
    chinese_.insert("app.title", QStringLiteral(u"Stok 投资工作台"));

    english_.insert("nav.header", QStringLiteral("Value Investing"));
    chinese_.insert("nav.header", QStringLiteral(u"价值投资"));

    english_.insert("nav.subheader", QStringLiteral("Holdings, quotes, research and risk in one place"));
    chinese_.insert("nav.subheader", QStringLiteral(u"持仓、行情、研究、风控一体化"));

    english_.insert("log.label", QStringLiteral("Logs"));
    chinese_.insert("log.label", QStringLiteral(u"日志"));

    english_.insert("log.title.live", QStringLiteral("Live logs"));
    chinese_.insert("log.title.live", QStringLiteral(u"实时日志"));

    english_.insert("log.subtitle.live", QStringLiteral("Output from the main process and child processes"));
    chinese_.insert("log.subtitle.live", QStringLiteral(u"查看主进程和子进程运行输出"));

    english_.insert("log.mode", QStringLiteral("Log mode"));
    chinese_.insert("log.mode", QStringLiteral(u"日志模式"));

    english_.insert("log.prefix", QStringLiteral("Logs / "));
    chinese_.insert("log.prefix", QStringLiteral(u"日志 / "));

    english_.insert("log.empty", QStringLiteral("Select a log source"));
    chinese_.insert("log.empty", QStringLiteral(u"选择日志进程"));

    english_.insert("content.loading", QStringLiteral("Loading page"));
    chinese_.insert("content.loading", QStringLiteral(u"正在加载页面"));

    // Shell-driven status text (controller pushes these into the model)
    english_.insert("status.waiting", QStringLiteral("Waiting for child process"));
    chinese_.insert("status.waiting", QStringLiteral(u"等待子进程"));

    english_.insert("status.waiting.placeholder", QStringLiteral("The right pane will load a window once a child process registers"));
    chinese_.insert("status.waiting.placeholder", QStringLiteral(u"右侧容器会在子进程注册后自动装载窗口"));

    english_.insert("status.disconnected", QStringLiteral("Not connected"));
    chinese_.insert("status.disconnected", QStringLiteral(u"未连接"));

    english_.insert("status.online", QStringLiteral("Online"));
    chinese_.insert("status.online", QStringLiteral(u"在线"));

    english_.insert("status.offline", QStringLiteral("Offline"));
    chinese_.insert("status.offline", QStringLiteral(u"离线"));

    english_.insert("status.ready", QStringLiteral("Ready"));
    chinese_.insert("status.ready", QStringLiteral(u"就绪"));

    english_.insert("status.windowMissing", QStringLiteral("Window unavailable"));
    chinese_.insert("status.windowMissing", QStringLiteral(u"窗口不可用"));

    english_.insert("status.connectedFmt", QStringLiteral("Connected to %1 child processes"));
    chinese_.insert("status.connectedFmt", QStringLiteral(u"已连接 %1 个子进程"));

    english_.insert("status.waitingForRegistration", QStringLiteral("Waiting for child process registration"));
    chinese_.insert("status.waitingForRegistration", QStringLiteral(u"等待子进程注册"));

    // Main-process log entry
    english_.insert("mainProcess.title", QStringLiteral("Main process"));
    chinese_.insert("mainProcess.title", QStringLiteral(u"主进程"));

    english_.insert("mainProcess.description", QStringLiteral("macchina.exe service log"));
    chinese_.insert("mainProcess.description", QStringLiteral(u"macchina.exe 管理进程日志"));

    english_.insert("mainProcess.waitingFmt", QStringLiteral("Waiting for the main process log file: %1"));
    chinese_.insert("mainProcess.waitingFmt", QStringLiteral(u"等待主进程日志文件：%1"));

    english_.insert("mainProcess.absentFmt", QStringLiteral("Main process log file not yet available: %1"));
    chinese_.insert("mainProcess.absentFmt", QStringLiteral(u"主进程日志文件暂不存在：%1"));

    english_.insert("mainProcess.waitingPrefix", QStringLiteral("Waiting for the main process log file"));
    chinese_.insert("mainProcess.waitingPrefix", QStringLiteral(u"等待主进程日志文件"));

    english_.insert("childLog.waitingFmt", QStringLiteral("Waiting for live logs from %1..."));
    chinese_.insert("childLog.waitingFmt", QStringLiteral(u"等待 %1 的实时日志..."));

    // Hosted-view titles and descriptions
    english_.insert("view.portfolio-board.title", QStringLiteral("Portfolio"));
    chinese_.insert("view.portfolio-board.title", QStringLiteral(u"持仓"));
    english_.insert("view.portfolio-board.description", QStringLiteral("Portfolio management, analytics and live curves"));
    chinese_.insert("view.portfolio-board.description", QStringLiteral(u"持仓管理、分析和实时曲线"));

    english_.insert("view.market-board.title", QStringLiteral("Market"));
    chinese_.insert("view.market-board.title", QStringLiteral(u"行情榜"));
    english_.insert("view.market-board.description", QStringLiteral("US-stock watchlists and rankings"));
    chinese_.insert("view.market-board.description", QStringLiteral(u"美股观察和榜单"));

    english_.insert("view.config-center.title", QStringLiteral("Config"));
    chinese_.insert("view.config-center.title", QStringLiteral(u"配置中心"));
    english_.insert("view.config-center.description", QStringLiteral("Global parameters and per-page settings"));
    chinese_.insert("view.config-center.description", QStringLiteral(u"全局参数和页面配置"));

    english_.insert("view.valuation-research.title", QStringLiteral("Valuation"));
    chinese_.insert("view.valuation-research.title", QStringLiteral(u"估值研究"));
    english_.insert("view.valuation-research.description", QStringLiteral("DCF, PE/PB percentiles, cash-flow quality and research notes"));
    chinese_.insert("view.valuation-research.description", QStringLiteral(u"DCF、PE/PB 分位、现金流质量和研究档案"));

    english_.insert("view.risk-backtest.title", QStringLiteral("Risk / Backtest"));
    chinese_.insert("view.risk-backtest.title", QStringLiteral(u"风控回测"));
    english_.insert("view.risk-backtest.description", QStringLiteral("Exposure, drawdown, correlation, simulated portfolios and backtests"));
    chinese_.insert("view.risk-backtest.description", QStringLiteral(u"暴露、回撤、相关性、模拟组合和回测"));

    english_.insert("view.trade-alerts.title", QStringLiteral("Trade alerts"));
    chinese_.insert("view.trade-alerts.title", QStringLiteral(u"交易提醒"));
    english_.insert("view.trade-alerts.description", QStringLiteral("Buy/sell rules, stop-loss/take-profit, invalidation and live alerts"));
    chinese_.insert("view.trade-alerts.description", QStringLiteral(u"买卖条件、止损止盈、失效条件和实时提醒"));

    english_.insert("view.data-hub.title", QStringLiteral("Data hub"));
    chinese_.insert("view.data-hub.title", QStringLiteral(u"数据中心"));
    english_.insert("view.data-hub.description", QStringLiteral("Quote sources, financials, FX, fund NAVs and latency monitor"));
    chinese_.insert("view.data-hub.description", QStringLiteral(u"行情源、财报源、汇率、基金净值和延迟监控"));
}

LocalizationController::Language LocalizationController::language() const
{
    return language_;
}

QString LocalizationController::toggleLabel() const
{
    // Show the language the user will switch *to* on click.
    return language_ == English ? QStringLiteral(u"中") : QStringLiteral("EN");
}

QString LocalizationController::tr(const QString& key) const
{
    const auto& table = language_ == English ? english_ : chinese_;
    const auto it = table.constFind(key);
    if (it != table.constEnd())
    {
        return it.value();
    }
    // Fall back to the other language so we never render the raw key.
    const auto& fallback = language_ == English ? chinese_ : english_;
    const auto fallbackIt = fallback.constFind(key);
    return fallbackIt != fallback.constEnd() ? fallbackIt.value() : key;
}

void LocalizationController::toggle()
{
    language_ = language_ == English ? Chinese : English;
    publishCurrentLanguage();
    emit languageChanged();
}

LocalizationController::~LocalizationController() = default;

void LocalizationController::startBroadcast(
    const stok::services::common::DdsSettings& settings,
    const std::string& participantName)
{
    if (publisher_)
    {
        return;
    }
    publisher_ = std::make_unique<stok::services::common::TextMessagePublisher>(settings);
    std::string error;
    publisher_->start(participantName, &error);
    publishCurrentLanguage();
}

void LocalizationController::publishCurrentLanguage()
{
    if (!publisher_)
    {
        return;
    }
    const QJsonObject object{
        {"type", QStringLiteral("language")},
        {"value", language_ == English ? QStringLiteral("en") : QStringLiteral("zh")}
    };
    stok::services::common::TextMessage message;
    message.payload = QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
    message.timestampMs = QDateTime::currentMSecsSinceEpoch();
    publisher_->publish(message, nullptr);
}
