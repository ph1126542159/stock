#include "DesktopShellController.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QTimer>
#include <QWindow>
#include <utility>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

stok::services::common::DdsSettings topic_settings(
    const Poco::Util::AbstractConfiguration& configuration,
    const std::string& key,
    const std::string& fallbackTopic)
{
    auto settings = stok::services::common::readDdsSettings(configuration);
    settings.topicName = configuration.getString(key, fallbackTopic);
    return settings;
}

quintptr window_id_from_json_value(const QJsonValue& value)
{
    if (value.isString())
    {
        bool ok = false;
        const quintptr windowId = value.toString().toULongLong(&ok);
        return ok ? windowId : 0;
    }

    if (value.isDouble())
    {
        return static_cast<quintptr>(value.toDouble());
    }

    return 0;
}

bool is_valid_window_id(quintptr windowId)
{
    if (windowId == 0)
    {
        return false;
    }

#if defined(Q_OS_WIN)
    return IsWindow(reinterpret_cast<HWND>(windowId)) != FALSE;
#else
    return true;
#endif
}

#if defined(Q_OS_WIN)
struct LocalHostedWindow
{
    QString executableName;
    QString className;
    QString title;
    quintptr windowId = 0;
    qint64 processId = 0;
};

QString process_image_name(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
    {
        return {};
    }

    wchar_t buffer[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(process, 0, buffer, &size);
    CloseHandle(process);
    if (!ok)
    {
        return {};
    }

    return QFileInfo(QString::fromWCharArray(buffer, static_cast<int>(size))).fileName().toLower();
}

BOOL CALLBACK enum_hosted_window_proc(HWND hwnd, LPARAM lparam)
{
    if (!IsWindow(hwnd))
    {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0)
    {
        return TRUE;
    }

    wchar_t classNameBuffer[128] = {};
    GetClassNameW(hwnd, classNameBuffer, 128);
    const QString className = QString::fromWCharArray(classNameBuffer).trimmed();
    if (className.contains(QStringLiteral("ScreenChangeObserverWindow"), Qt::CaseInsensitive))
    {
        return TRUE;
    }

    // Children bind their QML `title` to context properties that are never
    // populated, and they stay hidden until the shell embeds them — so the
    // Win32 title is empty and the rect can be undersized. Don't filter on
    // either; the executable-name allowlist below is the authoritative gate.
    const int titleLength = GetWindowTextLengthW(hwnd);
    QString title;
    if (titleLength > 0)
    {
        QVector<wchar_t> titleBuffer(titleLength + 1);
        GetWindowTextW(hwnd, titleBuffer.data(), titleBuffer.size());
        title = QString::fromWCharArray(titleBuffer.data()).trimmed();
    }

    RECT rect = {};
    GetWindowRect(hwnd, &rect);

    const QString executableName = process_image_name(processId);
    if (executableName != QStringLiteral("stok-portfolio-board.exe") &&
        executableName != QStringLiteral("stok-market-board.exe") &&
        executableName != QStringLiteral("stok-valuation-research.exe") &&
        executableName != QStringLiteral("stok-risk-backtest.exe") &&
        executableName != QStringLiteral("stok-trade-alerts.exe") &&
        executableName != QStringLiteral("stok-data-hub.exe") &&
        executableName != QStringLiteral("stok-config-center.exe"))
    {
        return TRUE;
    }

    auto* windows = reinterpret_cast<QVector<LocalHostedWindow>*>(lparam);
    windows->push_back(LocalHostedWindow{
        executableName,
        className,
        title,
        reinterpret_cast<quintptr>(hwnd),
        static_cast<qint64>(processId)
    });
    return TRUE;
}
#endif

struct HostedViewMetadata
{
    QString title;
    QString description;
};

HostedViewMetadata hosted_view_metadata(const QString& id, const LocalizationController& localization)
{
    static const QStringList knownIds = {
        QStringLiteral("portfolio-board"),
        QStringLiteral("market-board"),
        QStringLiteral("config-center"),
        QStringLiteral("valuation-research"),
        QStringLiteral("risk-backtest"),
        QStringLiteral("trade-alerts"),
        QStringLiteral("data-hub")
    };
    if (!knownIds.contains(id))
    {
        return {};
    }

    return {
        localization.tr(QStringLiteral("view.%1.title").arg(id)),
        localization.tr(QStringLiteral("view.%1.description").arg(id))
    };
}

void hide_native_window(quintptr windowId)
{
    if (windowId == 0)
    {
        return;
    }

#if defined(Q_OS_WIN)
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!IsWindow(hwnd))
    {
        return;
    }

    ShowWindow(hwnd, SW_HIDE);
#else
    Q_UNUSED(windowId);
#endif
}

#if defined(Q_OS_WIN)
// Re-parent a foreign top-level window so it becomes a child of the shell's
// container. After this returns, the hosted HWND uses client-relative
// coordinates, has no caption/border, and follows the parent automatically.
bool ensure_native_hosted_child(HWND hwnd, HWND parentHwnd, Poco::Logger* logger = nullptr)
{
    if (!IsWindow(hwnd) || !IsWindow(parentHwnd))
    {
        if (logger)
        {
            logger->warning(
                Poco::format("ensure_native_hosted_child: invalid hwnd=%?d or parent=%?d",
                    reinterpret_cast<std::uintptr_t>(hwnd),
                    reinterpret_cast<std::uintptr_t>(parentHwnd)));
        }
        return false;
    }

    // Make absolutely sure the parent will clip its children's rect when
    // painting; without this Qt's GDI fill of contentFrame would overwrite
    // the embedded child every frame.
    {
        LONG_PTR parentStyle = GetWindowLongPtrW(parentHwnd, GWL_STYLE);
        if ((parentStyle & WS_CLIPCHILDREN) == 0)
        {
            SetWindowLongPtrW(parentHwnd, GWL_STYLE, parentStyle | WS_CLIPCHILDREN);
            if (logger)
            {
                logger->information(
                    Poco::format("added WS_CLIPCHILDREN to parent=%?d",
                        reinterpret_cast<std::uintptr_t>(parentHwnd)));
            }
        }
    }

    HWND currentParent = GetParent(hwnd);
    if (currentParent != parentHwnd)
    {
        // Strip top-level frame styles before reparenting; otherwise SetParent
        // can leave the window with a phantom title bar.
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style &= ~(WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_THICKFRAME |
                   WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
        style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

        SetLastError(0);
        HWND prev = SetParent(hwnd, parentHwnd);
        const DWORD err = GetLastError();
        if (logger)
        {
            logger->information(
                Poco::format("SetParent hwnd=%?d parent=%?d prev=%?d err=%lu visible=%d",
                    reinterpret_cast<std::uintptr_t>(hwnd),
                    reinterpret_cast<std::uintptr_t>(parentHwnd),
                    reinterpret_cast<std::uintptr_t>(prev),
                    static_cast<unsigned long>(err),
                    static_cast<int>(IsWindowVisible(hwnd))));
        }

        SetWindowPos(
            hwnd,
            nullptr,
            0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
    return true;
}

void prepare_native_shell_window_id(quintptr windowId)
{
    if (windowId == 0)
    {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!IsWindow(hwnd))
    {
        return;
    }

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

void prepare_native_shell_window(QWindow* window)
{
    if (!window)
    {
        return;
    }

    prepare_native_shell_window_id(static_cast<quintptr>(window->winId()));
}

void place_native_hosted_window(
    quintptr windowId,
    quintptr parentWindowId,
    const QRect& area,
    bool active,
    Poco::Logger* logger = nullptr)
{
    if (windowId == 0 || parentWindowId == 0)
    {
        if (logger)
        {
            logger->warning(
                Poco::format("place skipped: windowId=%?d parentWindowId=%?d",
                    static_cast<std::uintptr_t>(windowId),
                    static_cast<std::uintptr_t>(parentWindowId)));
        }
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(windowId);
    HWND parentHwnd = reinterpret_cast<HWND>(parentWindowId);
    if (!ensure_native_hosted_child(hwnd, parentHwnd, logger))
    {
        return;
    }

    if (!active)
    {
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    if (!area.isValid() || area.width() <= 0 || area.height() <= 0)
    {
        if (logger)
        {
            logger->warning(
                Poco::format("place skipped: invalid area x=%d y=%d w=%d h=%d",
                    area.x(), area.y(), area.width(), area.height()));
        }
        return;
    }

    // Coordinates are now client-relative because the window is a true child.
    const BOOL ok = SetWindowPos(
        hwnd,
        HWND_TOP,
        area.x(),
        area.y(),
        area.width(),
        area.height(),
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

    // SetParent clears WS_VISIBLE; SetWindowPos with SWP_SHOWWINDOW reasserts
    // it but some Qt-based child windows still need an explicit ShowWindow to
    // present their backing surface.
    ShowWindow(hwnd, SW_SHOW);

    // SetWindowPos already marks the child's freshly-uncovered region dirty,
    // and we deliberately do NOT pass RDW_ERASE on the parent: WM_ERASEBKGND
    // makes Qt's software backend repaint the dark contentFrame across the
    // child's rect, which is the visible "flash" during a tab switch.
    // RDW_UPDATENOW would also issue a cross-process synchronous WM_PAINT to
    // the child and can deadlock if the child is busy, so we omit it too.

    if (logger)
    {
        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);
        logger->information(
            Poco::format(
                "SetWindowPos hwnd=%?d x=%d y=%d w=%d h=%d ok=%d "
                "visible=%d clientRect=(%d,%d %dx%d)",
                reinterpret_cast<std::uintptr_t>(hwnd),
                area.x(), area.y(), area.width(), area.height(),
                static_cast<int>(ok),
                static_cast<int>(IsWindowVisible(hwnd)),
                static_cast<int>(clientRect.left),
                static_cast<int>(clientRect.top),
                static_cast<int>(clientRect.right - clientRect.left),
                static_cast<int>(clientRect.bottom - clientRect.top)));
    }
}
#endif

qint64 timestamp_from_json_value(const QJsonValue& value)
{
    if (value.isString())
    {
        bool ok = false;
        const qint64 timestamp = value.toString().toLongLong(&ok);
        return ok ? timestamp : 0;
    }

    if (value.isDouble())
    {
        return static_cast<qint64>(value.toDouble());
    }

    return 0;
}

} // namespace

DesktopShellController::DesktopShellController(
    const stok::services::common::ServiceIdentity& identity,
    const stok::services::common::DdsSettings& ddsSettings,
    QString configPath,
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
    stok::services::common::ServiceTelemetry& telemetry,
    LocalizationController& localization,
    QObject* parent):
    QObject(parent),
    identity_(identity),
    ddsSettings_(ddsSettings),
    configPath_(std::move(configPath)),
    configuration_(std::move(configuration)),
    telemetry_(telemetry),
    localization_(localization),
    subscriber_(ddsSettings_, telemetry_.client()),
    hostedViewSubscriber_(
        topic_settings(*configuration_, "dds.hostedViewTopic", "stok.ui.hosted-views"),
        telemetry_.client()),
    logSubscriber_(
        topic_settings(*configuration_, "dds.logTopic", "stok.ui.logs"),
        telemetry_.client())
{
    telemetryMode_ = configuration_->getBool("telemetry.export.enabled", false)
        ? QStringLiteral("OTLP export enabled")
        : QStringLiteral("Local telemetry buffer");
    hostedViewTopicName_ = QString::fromStdString(
        topic_settings(*configuration_, "dds.hostedViewTopic", "stok.ui.hosted-views").topicName);
    logTopicName_ = QString::fromStdString(
        topic_settings(*configuration_, "dds.logTopic", "stok.ui.logs").topicName);
    defaultHostedViewId_ = QString::fromStdString(
        configuration_->getString("ui.defaultHostedViewId", "market-board")).trimmed();
    mainProcessLogPath_ = QString::fromStdString(
        stok::services::common::resolveConfigRelativePath(
            configPath_.toStdString(),
            configuration_->getString("mainProcessLog.path", "")));

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
        refreshHostedProcessState();
        discoverLocalHostedWindows();
    });

    hostedSwitchRedrawTimer_ = new QTimer(this);
    hostedSwitchRedrawTimer_->setSingleShot(true);
    hostedSwitchRedrawTimer_->setInterval(90);
    connect(hostedSwitchRedrawTimer_, &QTimer::timeout, this, &DesktopShellController::restoreShellRedraw);

    mainProcessLogTimer_ = new QTimer(this);
    mainProcessLogTimer_->setInterval(1000);
    connect(mainProcessLogTimer_, &QTimer::timeout, this, &DesktopShellController::refreshMainProcessLog);

    connect(&localization_, &LocalizationController::languageChanged,
            this, &DesktopShellController::applyLocalization);
}

DesktopShellController::~DesktopShellController()
{
    restoreShellRedraw();
    stopLogBridge();
    stopProcessBridge();
    subscriber_.stop();
}

QuoteListModel* DesktopShellController::quotesModel()
{
    return &quotesModel_;
}

ProcessMenuModel* DesktopShellController::processMenuModel()
{
    return &processMenuModel_;
}

ProcessMenuModel* DesktopShellController::logProcessMenuModel()
{
    return &logProcessMenuModel_;
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

QString DesktopShellController::activeProcessTitle() const
{
    if (activeHostedViewIndex_ < 0 || activeHostedViewIndex_ >= hostedViews_.size())
    {
        return localization_.tr(QStringLiteral("status.waiting"));
    }

    return hostedViews_[activeHostedViewIndex_].title;
}

QString DesktopShellController::activeProcessDescription() const
{
    if (activeHostedViewIndex_ < 0 || activeHostedViewIndex_ >= hostedViews_.size())
    {
        return localization_.tr(QStringLiteral("status.waiting.placeholder"));
    }

    return hostedViews_[activeHostedViewIndex_].description;
}

QString DesktopShellController::activeProcessStatus() const
{
    if (activeHostedViewIndex_ < 0 || activeHostedViewIndex_ >= hostedViews_.size())
    {
        return localization_.tr(QStringLiteral("status.disconnected"));
    }

    return hostedViews_[activeHostedViewIndex_].status;
}

int DesktopShellController::activeProcessRow() const
{
    if (activeHostedViewIndex_ < 0 || activeHostedViewIndex_ >= hostedViews_.size())
    {
        return -1;
    }

    return processMenuModel_.rowForId(hostedViews_[activeHostedViewIndex_].id);
}

QWindow* DesktopShellController::activeProcessWindow() const
{
    return nullptr;
}

bool DesktopShellController::activeProcessReady() const
{
    return !logConsoleActive_ &&
        activeHostedViewIndex_ >= 0 &&
        activeHostedViewIndex_ < hostedViews_.size() &&
        hostedViews_[activeHostedViewIndex_].running &&
        is_valid_window_id(hostedViews_[activeHostedViewIndex_].windowId);
}

bool DesktopShellController::logConsoleActive() const
{
    return logConsoleActive_;
}

QString DesktopShellController::activeLogProcessName() const
{
    return activeLogProcessName_;
}

QString DesktopShellController::activeLogText() const
{
    return logLinesByProcess_.value(activeLogProcessName_).join('\n');
}

void DesktopShellController::start()
{
    if (shellStarted_)
    {
        return;
    }
    shellStarted_ = true;

    startLogBridge();
    startProcessBridge();
    QTimer::singleShot(2500, this, &DesktopShellController::discoverLocalHostedWindows);
    feedState_ = QStringLiteral("Hosted views only");
    emit feedStateChanged();

    heartbeatTimer_->start();
    if (!mainProcessLogPath_.isEmpty())
    {
        const QString service = QStringLiteral("macchina");
        logLinesByProcess_.insert(
            service,
            QStringList{localization_.tr(QStringLiteral("mainProcess.waitingFmt")).arg(mainProcessLogPath_)});
        logProcessMenuModel_.upsertItem(ProcessMenuModel::Item{
            service,
            localization_.tr(QStringLiteral("mainProcess.title")),
            localization_.tr(QStringLiteral("mainProcess.description")),
            QStringLiteral("file"),
            activeLogProcessName_ == service,
            true
        });
        refreshMainProcessLog();
        mainProcessLogTimer_->start();
    }

    telemetry_.recordStartup(
        "desktop-shell",
        {
            {"dds.topic", ddsSettings_.topicName},
            {"ui.watchlist.count", std::to_string(quotesModel_.count())},
            {"hosted_view.discovery", "win32"},
            {"ui.quote_feed", "disabled"}
        });
}

void DesktopShellController::activateProcessMenu(int index)
{
    const QString processId = processMenuModel_.idAt(index);
    if (processId.isEmpty())
    {
        return;
    }

    const int viewIndex = viewIndexForId(processId);
    if (viewIndex < 0)
    {
        return;
    }

    freezeShellRedrawForHostedSwitch();
    if (logConsoleActive_)
    {
        logConsoleActive_ = false;
        emit logConsoleChanged();
    }
    userSelectedHostedView_ = true;
    setActiveProcessIndex(viewIndex);
}

void DesktopShellController::activateLogProcess(int index)
{
    const QString processName = logProcessMenuModel_.idAt(index);
    if (processName.isEmpty())
    {
        return;
    }

    activeLogProcessName_ = processName;
    freezeShellRedrawForHostedSwitch();
    logConsoleActive_ = true;
    userSelectedHostedView_ = true;
    logProcessMenuModel_.setActiveRow(index);
    updateNativeHostedWindows();
    emit activeProcessWindowChanged();
    emit logConsoleChanged();
    QTimer::singleShot(360, this, &DesktopShellController::hideInactiveHostedWindows);
}

void DesktopShellController::attachShellWindow(QWindow* window)
{
    shellWindow_ = window;
    shellWindowId_ = window ? static_cast<quintptr>(window->winId()) : 0;
#if defined(Q_OS_WIN)
    prepare_native_shell_window(window);
#else
    Q_UNUSED(window);
#endif
    updateNativeHostedWindows();
}

void DesktopShellController::attachShellWindowId(quintptr windowId)
{
    shellWindow_ = nullptr;
    shellWindowId_ = windowId;
#if defined(Q_OS_WIN)
    prepare_native_shell_window_id(windowId);
#endif
    updateNativeHostedWindows();
}

void DesktopShellController::attachHostedContainerWindowId(quintptr windowId)
{
    hostedContainerWindowId_ = windowId;
#if defined(Q_OS_WIN)
    prepare_native_shell_window_id(windowId);
#endif
    updateNativeHostedWindows();
}

void DesktopShellController::setHostedWindowArea(int x, int y, int width, int height)
{
    const QRect area{x, y, width, height};
    if (hostedWindowArea_ == area)
    {
        return;
    }

    hostedWindowArea_ = area;
    updateNativeHostedWindows();
}

void DesktopShellController::resizeActiveProcessWindow(int width, int height)
{
    if (logConsoleActive_ ||
        activeHostedViewIndex_ < 0 ||
        activeHostedViewIndex_ >= hostedViews_.size() ||
        width <= 0 ||
        height <= 0)
    {
        return;
    }

    HostedProcessView& view = hostedViews_[activeHostedViewIndex_];
    if (!view.running || !is_valid_window_id(view.windowId))
    {
        return;
    }

    if (hostedWindowArea_.width() != width || hostedWindowArea_.height() != height)
    {
        hostedWindowArea_.setWidth(width);
        hostedWindowArea_.setHeight(height);
    }
    updateNativeHostedWindows();
}

void DesktopShellController::requestManagedShutdown()
{
    if (hostedWindowLossGuard_.isValid() && hostedWindowLossGuard_.elapsed() < 5000)
    {
        telemetry_.logger().warning(
            "Ignored managed shutdown request while recovering a hosted child window.");
        return;
    }

    telemetry_.logger().information("Desktop shell requested managed shutdown.");
#if defined(Q_OS_WIN)
    HANDLE shutdownEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\StokMacchinaShutdownRequested");
    if (shutdownEvent)
    {
        SetEvent(shutdownEvent);
        CloseHandle(shutdownEvent);
    }
#endif
    QCoreApplication::quit();
}

void DesktopShellController::startProcessBridge()
{
    if (processBridgeStarted_)
    {
        return;
    }

    std::string error;
    const bool started = hostedViewSubscriber_.start(
        identity_.name + "-hosted-views",
        [this](const stok::services::common::TextMessage& message)
        {
            const QString payload = QString::fromUtf8(message.payload);
            QMetaObject::invokeMethod(this, [this, payload]()
            {
                registerHostedProcess(payload);
            }, Qt::QueuedConnection);
        },
        [this](int count)
        {
            QMetaObject::invokeMethod(this, [this, count]()
            {
                if (count < hostedViewMatches_)
                {
                    hostedWindowLossGuard_.restart();
                }
                hostedViewMatches_ = count;
                if (count <= 0 && hostedViews_.isEmpty())
                {
                    statusLine_ = localization_.tr(QStringLiteral("status.waitingForRegistration"));
                    emit statusLineChanged();
                }
            }, Qt::QueuedConnection);
        },
        &error);

    if (!started)
    {
        statusLine_ = QStringLiteral("Hosted view bus failed: %1").arg(QString::fromStdString(error));
        emit statusLineChanged();
        telemetry_.logger().error(statusLine_.toStdString());
        return;
    }

    processBridgeStarted_ = true;
    telemetry_.logger().information(
        QStringLiteral("Desktop shell subscribed to hosted-view topic \"%1\".")
            .arg(hostedViewTopicName_)
            .toStdString());
}

void DesktopShellController::startLogBridge()
{
    if (logBridgeStarted_)
    {
        return;
    }

    std::string error;
    const bool started = logSubscriber_.start(
        identity_.name + "-logs",
        [this](const stok::services::common::TextMessage& message)
        {
            const QString payload = QString::fromUtf8(message.payload);
            QMetaObject::invokeMethod(this, [this, payload]()
            {
                registerLogMessage(payload);
            }, Qt::QueuedConnection);
        },
        {},
        &error);

    if (!started)
    {
        telemetry_.logger().warning(
            QStringLiteral("Desktop shell log subscription failed: %1")
                .arg(QString::fromStdString(error))
                .toStdString());
        return;
    }

    logBridgeStarted_ = true;
}

void DesktopShellController::stopLogBridge()
{
    logBridgeStarted_ = false;
    logSubscriber_.stop();
}

void DesktopShellController::refreshMainProcessLog()
{
    if (mainProcessLogPath_.isEmpty())
    {
        return;
    }

    QFile file(mainProcessLogPath_);
    if (!file.exists())
    {
        logLinesByProcess_[QStringLiteral("macchina")] =
            QStringList{localization_.tr(QStringLiteral("mainProcess.absentFmt")).arg(mainProcessLogPath_)};
        if (logConsoleActive_ && activeLogProcessName_ == QStringLiteral("macchina"))
        {
            emit logConsoleChanged();
        }
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    const qint64 size = file.size();
    if (mainProcessLogPosition_ <= 0)
    {
        mainProcessLogPosition_ = qMax<qint64>(0, size - 64 * 1024);
    }
    else if (size < mainProcessLogPosition_)
    {
        mainProcessLogPosition_ = 0;
    }

    if (size <= mainProcessLogPosition_)
    {
        return;
    }

    file.seek(mainProcessLogPosition_);
    const QByteArray chunk = file.readAll();
    mainProcessLogPosition_ = size;

    QString text = QString::fromLocal8Bit(chunk);
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    QStringList& lines = logLinesByProcess_[QStringLiteral("macchina")];
    if (lines.size() == 1 &&
        (lines.first().startsWith(localization_.tr(QStringLiteral("mainProcess.waitingPrefix"))) ||
         lines.first().startsWith(QStringLiteral(u"等待主进程日志文件")) ||
         lines.first().startsWith(QStringLiteral("Waiting for the main process log file"))))
    {
        lines.clear();
    }

    for (const QString& line : text.split('\n'))
    {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            lines.append(trimmed);
        }
    }
    while (lines.size() > 600)
    {
        lines.removeFirst();
    }

    const int row = logProcessMenuModel_.upsertItem(ProcessMenuModel::Item{
        QStringLiteral("macchina"),
        localization_.tr(QStringLiteral("mainProcess.title")),
        lines.isEmpty() ? localization_.tr(QStringLiteral("mainProcess.description")) : lines.last(),
        QStringLiteral("file"),
        activeLogProcessName_ == QStringLiteral("macchina"),
        true
    });
    if (activeLogProcessName_.isEmpty())
    {
        activeLogProcessName_ = QStringLiteral("macchina");
        logProcessMenuModel_.setActiveRow(row);
    }

    if (logConsoleActive_ && activeLogProcessName_ == QStringLiteral("macchina"))
    {
        emit logConsoleChanged();
    }
}

void DesktopShellController::stopProcessBridge()
{
    processBridgeStarted_ = false;
    hostedViewSubscriber_.stop();

    for (HostedProcessView& view : hostedViews_)
    {
        if (view.foreignWindow)
        {
            view.foreignWindow->setVisible(false);
            view.foreignWindow->deleteLater();
            view.foreignWindow = nullptr;
        }
    }

    hostedViews_.clear();
}

void DesktopShellController::registerLogMessage(const QString& payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8());
    if (!document.isObject())
    {
        return;
    }

    const QJsonObject object = document.object();
    const QString service = object.value("service").toString().trimmed();
    if (service.isEmpty())
    {
        return;
    }

    const QString timestamp = object.value("timestamp").toString().trimmed();
    const QString level = object.value("level").toString().trimmed();
    const QString text = object.value("text").toString().trimmed();
    const QString line = QStringLiteral("%1  [%2]  %3")
        .arg(timestamp.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODateWithMs) : timestamp,
             level.isEmpty() ? QStringLiteral("Information") : level,
             text);

    QStringList& lines = logLinesByProcess_[service];
    lines.append(line);
    while (lines.size() > 600)
    {
        lines.removeFirst();
    }

    const int row = logProcessMenuModel_.upsertItem(ProcessMenuModel::Item{
        service,
        service,
        text,
        level.isEmpty() ? QStringLiteral("Information") : level,
        activeLogProcessName_ == service,
        true
    });

    if (activeLogProcessName_.isEmpty())
    {
        activeLogProcessName_ = service;
        logProcessMenuModel_.setActiveRow(row);
    }

    if (logConsoleActive_ && activeLogProcessName_ == service)
    {
        emit logConsoleChanged();
    }
}

void DesktopShellController::discoverLocalHostedWindows()
{
#if defined(Q_OS_WIN)
    if (localHostedDiscoveryInProgress_)
    {
        return;
    }

    localHostedDiscoveryInProgress_ = true;
    struct DiscoveryReset
    {
        bool& value;
        ~DiscoveryReset() { value = false; }
    } discoveryReset{localHostedDiscoveryInProgress_};

    QVector<LocalHostedWindow> windows;
    EnumWindows(enum_hosted_window_proc, reinterpret_cast<LPARAM>(&windows));

    static const QHash<QString, QString> idForExecutable = {
        {QStringLiteral("stok-portfolio-board.exe"),    QStringLiteral("portfolio-board")},
        {QStringLiteral("stok-market-board.exe"),       QStringLiteral("market-board")},
        {QStringLiteral("stok-config-center.exe"),      QStringLiteral("config-center")},
        {QStringLiteral("stok-valuation-research.exe"), QStringLiteral("valuation-research")},
        {QStringLiteral("stok-risk-backtest.exe"),      QStringLiteral("risk-backtest")},
        {QStringLiteral("stok-trade-alerts.exe"),       QStringLiteral("trade-alerts")},
        {QStringLiteral("stok-data-hub.exe"),           QStringLiteral("data-hub")}
    };

    for (const LocalHostedWindow& window : windows)
    {
        const QString id = idForExecutable.value(window.executableName);
        if (id.isEmpty())
        {
            continue;
        }

        const HostedViewMetadata metadata = hosted_view_metadata(id, localization_);
        const int existingIndex = viewIndexForId(id);
        if (existingIndex >= 0 &&
            hostedViews_[existingIndex].windowId == window.windowId &&
            is_valid_window_id(hostedViews_[existingIndex].windowId))
        {
            // Window already known. Just refresh the liveness timestamp so
            // refreshHostedProcessState() doesn't flip the view to offline
            // after 7s of no DDS publications and hide the child HWND.
            hostedViews_[existingIndex].lastSeenMs = QDateTime::currentMSecsSinceEpoch();
            continue;
        }

        const QJsonObject object{
            {"type", QStringLiteral("register_view")},
            {"id", id},
            {"title", metadata.title},
            {"description", metadata.description},
            {"group", QStringLiteral("main")},
            {"status", localization_.tr(QStringLiteral("status.online"))},
            {"windowId", QString::number(static_cast<qulonglong>(window.windowId))},
            {"processId", QString::number(window.processId)},
            {"timestampMs", QString::number(QDateTime::currentMSecsSinceEpoch())}
        };
        registerHostedProcess(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }

    if (!logConsoleActive_)
    {
        const bool activeUnavailable =
            activeHostedViewIndex_ < 0 ||
            activeHostedViewIndex_ >= hostedViews_.size() ||
            !hostedViews_[activeHostedViewIndex_].running ||
            !is_valid_window_id(hostedViews_[activeHostedViewIndex_].windowId);
        const int preferredIndex = preferredHostedViewIndex();
        if (preferredIndex >= 0 && (activeUnavailable || !userSelectedHostedView_))
        {
            setActiveProcessIndex(preferredIndex);
        }
    }
#endif
}

void DesktopShellController::registerHostedProcess(const QString& payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        telemetry_.logger().warning(
            QStringLiteral("Ignoring invalid hosted process registration payload: %1")
                .arg(parseError.errorString())
                .toStdString());
        return;
    }

    const QJsonObject object = document.object();
    if (object.value("type").toString() != QStringLiteral("register_view"))
    {
        return;
    }

    const QString id = object.value("id").toString().trimmed();
    QString title = object.value("title").toString().trimmed();
    if (id.isEmpty())
    {
        telemetry_.logger().warning("Ignoring hosted process registration with missing id.");
        return;
    }

    QString description = object.value("description").toString().trimmed();
    const HostedViewMetadata metadata = hosted_view_metadata(id, localization_);
    if (!metadata.title.isEmpty())
    {
        title = metadata.title;
        description = metadata.description;
    }
    if (title.isEmpty())
    {
        telemetry_.logger().warning("Ignoring hosted process registration with missing title.");
        return;
    }

    const quintptr windowId = window_id_from_json_value(object.value("windowId"));
    if (!is_valid_window_id(windowId))
    {
        telemetry_.logger().warning(
            QStringLiteral("Ignoring hosted process \"%1\": invalid window id.").arg(id).toStdString());
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastSeenMs = std::max(timestamp_from_json_value(object.value("timestampMs")), nowMs);
    const int existingIndex = viewIndexForId(id);
    const int viewIndex = existingIndex >= 0 ? existingIndex : hostedViews_.size();
    const bool isNewView = existingIndex < 0;
    if (existingIndex < 0)
    {
        hostedViews_.push_back(HostedProcessView{});
    }

    HostedProcessView& view = hostedViews_[viewIndex];
    view.id = id;
    view.title = title;
    view.description = description;
    view.group = object.value("group").toString().trimmed();
    view.status = object.value("status").toString().trimmed();
    if (view.status.isEmpty())
    {
        view.status = localization_.tr(QStringLiteral("status.online"));
    }
    view.processId = object.value("processId").toString().toLongLong();
    view.lastSeenMs = lastSeenMs;
    view.running = true;

    const quintptr previousWindowId = view.windowId;
    view.windowId = windowId;

    const bool needsWindowRefresh = previousWindowId != windowId || !is_valid_window_id(previousWindowId);
    if (needsWindowRefresh)
    {
        // Different HWND than what we last embedded (or first registration).
        // Make sure the next updateNativeHostedWindows() runs the full
        // reparent + show path on the new HWND.
        view.nativelyShown = false;
        if (view.foreignWindow)
        {
            view.foreignWindow->deleteLater();
            view.foreignWindow = nullptr;
            const int refreshRow = processMenuModel_.rowForId(view.id);
            if (refreshRow >= 0)
            {
                processMenuModel_.setItemWindow(refreshRow, nullptr);
            }
        }
    }

    const int row = processMenuModel_.upsertItem(ProcessMenuModel::Item{
        view.id,
        view.title,
        view.description,
        view.status,
        false,
        view.running
    });
    const QString logServiceName = view.id.startsWith(QStringLiteral("stok."))
        ? view.id
        : QStringLiteral("stok.%1").arg(view.id);
    if (!logLinesByProcess_.contains(logServiceName))
    {
        logLinesByProcess_.insert(
            logServiceName,
            QStringList{localization_.tr(QStringLiteral("childLog.waitingFmt")).arg(view.title)});
    }
    logProcessMenuModel_.upsertItem(ProcessMenuModel::Item{
        logServiceName,
        view.title,
        view.description,
        localization_.tr(QStringLiteral("status.ready")),
        activeLogProcessName_ == logServiceName,
        view.running
    });

    statusLine_ = localization_.tr(QStringLiteral("status.connectedFmt")).arg(hostedViews_.size());
    emit statusLineChanged();

    if (isNewView || needsWindowRefresh)
    {
        telemetry_.logger().information(
            QStringLiteral("Registered hosted process \"%1\" on row %2 with window id %3.")
                .arg(view.id)
                .arg(row)
                .arg(QString::number(view.windowId))
                .toStdString());
    }

    const int preferredIndex = preferredHostedViewIndex();
    const bool activeUnavailable =
        activeHostedViewIndex_ < 0 ||
        activeHostedViewIndex_ >= hostedViews_.size() ||
        !hostedViews_[activeHostedViewIndex_].running ||
        !is_valid_window_id(hostedViews_[activeHostedViewIndex_].windowId);

    const bool isDefaultView = view.id.compare(defaultHostedViewId_, Qt::CaseInsensitive) == 0;
    if (!userSelectedHostedView_ && isDefaultView)
    {
        setActiveProcessIndex(viewIndex);
        return;
    }

    // Only fall back to the preferred (default) view when the currently
    // active one is genuinely unusable. Without this check, every time a
    // child process re-registers the default view (e.g. its HWND changed
    // because its QML window was rebuilt), we would yank the user back to
    // the default page even though they had navigated away.
    if (preferredIndex >= 0 && activeUnavailable)
    {
        setActiveProcessIndex(preferredIndex);
        return;
    }

    if (activeHostedViewIndex_ < 0)
    {
        if (!defaultHostedViewId_.isEmpty() &&
            view.id.compare(defaultHostedViewId_, Qt::CaseInsensitive) != 0)
        {
            QMetaObject::invokeMethod(this, [this]()
            {
                syncHostedWindowVisibility();
            }, Qt::QueuedConnection);
            return;
        }

        setActiveProcessIndex(viewIndex);
        return;
    }

    if (activeHostedViewIndex_ == viewIndex && view.foreignWindow)
    {
        emit activeProcessChanged();
        emit activeProcessStatusChanged();
        if (needsWindowRefresh)
        {
            emit activeProcessWindowChanged();
        }
    }

    QMetaObject::invokeMethod(this, [this]()
    {
        syncHostedWindowVisibility();
    }, Qt::QueuedConnection);
}

void DesktopShellController::refreshHostedProcessState()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool activeChanged = false;
    for (int index = 0; index < hostedViews_.size(); ++index)
    {
        HostedProcessView& view = hostedViews_[index];
        const bool shouldBeRunning =
            is_valid_window_id(view.windowId) &&
            (view.foreignWindow || (view.lastSeenMs > 0 && nowMs - view.lastSeenMs <= 7000));
        const QString newStatus = shouldBeRunning
            ? localization_.tr(QStringLiteral("status.online"))
            : localization_.tr(QStringLiteral("status.offline"));
        if (view.running == shouldBeRunning && view.status == newStatus)
        {
            continue;
        }

        view.running = shouldBeRunning;
        view.status = newStatus;
        const int menuRow = processMenuModel_.rowForId(view.id);
        if (menuRow >= 0)
        {
            processMenuModel_.setItemRunning(menuRow, shouldBeRunning);
            processMenuModel_.setItemStatus(menuRow, newStatus);
            if (!shouldBeRunning)
            {
                processMenuModel_.setItemWindow(menuRow, nullptr);
            }
        }
        const QString logServiceName = view.id.startsWith(QStringLiteral("stok."))
            ? view.id
            : QStringLiteral("stok.%1").arg(view.id);
        const int logRow = logProcessMenuModel_.rowForId(logServiceName);
        if (logRow >= 0)
        {
            logProcessMenuModel_.setItemRunning(logRow, shouldBeRunning);
            logProcessMenuModel_.setItemStatus(logRow, shouldBeRunning ? localization_.tr(QStringLiteral("status.ready")) : newStatus);
        }
        if (!shouldBeRunning && view.foreignWindow)
        {
            view.foreignWindow->setVisible(false);
        }
        if (!shouldBeRunning)
        {
            // Force the next pass through updateNativeHostedWindows to reapply
            // visibility. Otherwise, if the same windowId is later reused after
            // a restart, our cached nativelyShown stays out of sync.
            view.nativelyShown = false;
        }
        if (activeHostedViewIndex_ == index)
        {
            activeChanged = true;
        }
    }

    if (activeChanged)
    {
        emit activeProcessStatusChanged();
        emit activeProcessWindowChanged();
    }

    if (activeHostedViewIndex_ < 0 ||
        activeHostedViewIndex_ >= hostedViews_.size() ||
        !hostedViews_[activeHostedViewIndex_].running ||
        !is_valid_window_id(hostedViews_[activeHostedViewIndex_].windowId))
    {
        const int preferredIndex = preferredHostedViewIndex();
        if (preferredIndex >= 0 && preferredIndex != activeHostedViewIndex_)
        {
            setActiveProcessIndex(preferredIndex);
            return;
        }
    }

    syncHostedWindowVisibility();
}

bool DesktopShellController::ensureHostedWindow(int index)
{
    if (index < 0 || index >= hostedViews_.size())
    {
        return false;
    }

    HostedProcessView& view = hostedViews_[index];
    if (!view.running || !is_valid_window_id(view.windowId))
    {
        return false;
    }

    if (view.foreignWindow)
    {
        return true;
    }

    QWindow* foreignWindow = QWindow::fromWinId(view.windowId);
    if (!foreignWindow)
    {
        view.status = localization_.tr(QStringLiteral("status.windowMissing"));
        view.running = false;
        const int menuRow = processMenuModel_.rowForId(view.id);
        if (menuRow >= 0)
        {
            processMenuModel_.setItemRunning(menuRow, false);
            processMenuModel_.setItemStatus(menuRow, view.status);
        }
        return false;
    }

    foreignWindow->QObject::setParent(this);
    view.foreignWindow = foreignWindow;
    const int windowRow = processMenuModel_.rowForId(view.id);
    if (windowRow >= 0)
    {
        processMenuModel_.setItemWindow(windowRow, foreignWindow);
    }
    connect(foreignWindow, &QObject::destroyed, this, [this, index, foreignWindow]()
    {
        if (index < 0 || index >= hostedViews_.size())
        {
            return;
        }

        if (hostedViews_[index].foreignWindow == foreignWindow)
        {
            hostedWindowLossGuard_.restart();
            hostedViews_[index].foreignWindow = nullptr;
            hostedViews_[index].running = false;
            hostedViews_[index].status = localization_.tr(QStringLiteral("status.offline"));
            const int menuRow = processMenuModel_.rowForId(hostedViews_[index].id);
            if (menuRow >= 0)
            {
                processMenuModel_.setItemRunning(menuRow, false);
                processMenuModel_.setItemStatus(menuRow, hostedViews_[index].status);
                processMenuModel_.setItemWindow(menuRow, nullptr);
            }
            if (activeHostedViewIndex_ == index)
            {
                emit activeProcessStatusChanged();
                emit activeProcessWindowChanged();
            }
        }
    });

    return true;
}

int DesktopShellController::preferredHostedViewIndex() const
{
    auto isSelectable = [this](int index)
    {
        if (index < 0 || index >= hostedViews_.size())
        {
            return false;
        }

        const HostedProcessView& view = hostedViews_.at(index);
        return view.running && is_valid_window_id(view.windowId);
    };

    if (!defaultHostedViewId_.isEmpty())
    {
        const int preferredIndex = viewIndexForId(defaultHostedViewId_);
        if (isSelectable(preferredIndex))
        {
            return preferredIndex;
        }
        return -1;
    }

    for (int index = 0; index < hostedViews_.size(); ++index)
    {
        if (isSelectable(index))
        {
            return index;
        }
    }

    return -1;
}

int DesktopShellController::viewIndexForId(const QString& id) const
{
    for (int index = 0; index < hostedViews_.size(); ++index)
    {
        if (hostedViews_[index].id == id)
        {
            return index;
        }
    }

    return -1;
}

void DesktopShellController::setActiveProcessIndex(int index)
{
    if (index < 0 || index >= hostedViews_.size())
    {
        return;
    }

    if (activeHostedViewIndex_ == index)
    {
        syncHostedWindowVisibility();
        return;
    }

    freezeShellRedrawForHostedSwitch();
    activeHostedViewIndex_ = index;
    processMenuModel_.setActiveRow(processMenuModel_.rowForId(hostedViews_[index].id));

    refreshActiveProcessSignals();
    QMetaObject::invokeMethod(this, [this]()
    {
        syncHostedWindowVisibility();
    }, Qt::QueuedConnection);
}

void DesktopShellController::refreshActiveProcessSignals()
{
    emit activeProcessChanged();
    emit activeProcessStatusChanged();
    emit activeProcessWindowChanged();
}

void DesktopShellController::syncHostedWindowVisibility()
{
    if (!logConsoleActive_ &&
        activeHostedViewIndex_ >= 0 &&
        activeHostedViewIndex_ < hostedViews_.size())
    {
        updateNativeHostedWindows();
        return;
    }
    hideInactiveHostedWindows();
}

void DesktopShellController::hideInactiveHostedWindows()
{
    updateNativeHostedWindows();
}

void DesktopShellController::updateNativeHostedWindows()
{
    // Embed each hosted child's HWND directly via Win32 SetParent/SetWindowPos.
    // Qt's QWindow::setParent on a foreign window leaves the underlying HWND
    // top-level (Qt::Tool flag survives reparenting), and applies the parent
    // QWindow's devicePixelRatio when interpreting setGeometry — both of which
    // led to the child detaching after a few seconds and rendering at the
    // wrong size. Raw Win32 makes the child a true WS_CHILD with native-pixel
    // coordinates, which is what we want.
    QWindow* parentWindow = shellWindow_;
    if (!parentWindow)
    {
        return;
    }

#if defined(Q_OS_WIN)
    Poco::Logger* logger = &telemetry_.logger();
    const quintptr parentWindowId = static_cast<quintptr>(parentWindow->winId());
    const qreal dpr = parentWindow->devicePixelRatio() > 0.0 ? parentWindow->devicePixelRatio() : 1.0;
    const QRect nativeArea(
        qRound(hostedWindowArea_.x() * dpr),
        qRound(hostedWindowArea_.y() * dpr),
        qRound(hostedWindowArea_.width() * dpr),
        qRound(hostedWindowArea_.height() * dpr));

    const bool activeRunning =
        activeHostedViewIndex_ >= 0 && activeHostedViewIndex_ < hostedViews_.size()
            ? hostedViews_[activeHostedViewIndex_].running
            : false;
    const quintptr activeWindowId =
        activeHostedViewIndex_ >= 0 && activeHostedViewIndex_ < hostedViews_.size()
            ? hostedViews_[activeHostedViewIndex_].windowId
            : 0;
    const auto stateKey = std::make_tuple(
        parentWindowId,
        activeHostedViewIndex_,
        logConsoleActive_,
        nativeArea.x(), nativeArea.y(),
        nativeArea.width(), nativeArea.height(),
        activeRunning,
        activeWindowId);
    const bool stateChanged = stateKey != lastEmbeddingStateKey_;
    lastEmbeddingStateKey_ = stateKey;

    // The QML side calls setHostedWindowArea() on a 250 ms timer. When nothing
    // has actually changed we must NOT re-issue SetParent / SetWindowPos /
    // RedrawWindow on every tick: each call crosses a process boundary, and a
    // burst of cross-process Win32 traffic stalls the shell's UI thread when a
    // child is slow to pump messages. Bail out early when state is stable.
    if (!stateChanged)
    {
        return;
    }

    logger->information(
        Poco::format(
            "updateNativeHostedWindows: parent=%?d active=%d logConsole=%d nativeArea=(%d,%d %dx%d) dpr=%f views=%d",
            static_cast<std::uintptr_t>(parentWindowId),
            activeHostedViewIndex_,
            static_cast<int>(logConsoleActive_),
            nativeArea.x(), nativeArea.y(),
            nativeArea.width(), nativeArea.height(),
            static_cast<double>(dpr),
            static_cast<int>(hostedViews_.size())));

    const bool wantArea = nativeArea.isValid() &&
        nativeArea.width() > 0 &&
        nativeArea.height() > 0;

    // Two-pass switch to suppress flicker:
    //   1. Show + position the new active view *first* so the container is
    //      always covered by some child while the swap is in flight.
    //   2. Hide the now-inactive views afterwards.
    // Doing it in the opposite order leaves a frame where the shell's own
    // background paints into the hole.
    // Each view tracks its current nativelyShown flag so we skip cross-process
    // SetWindowPos / ShowWindow calls on views whose state hasn't changed —
    // those are perceptible lag with seven children when a tab is clicked.
    for (int index = 0; index < hostedViews_.size(); ++index)
    {
        HostedProcessView& view = hostedViews_[index];
        const bool shouldShow =
            !logConsoleActive_ &&
            index == activeHostedViewIndex_ &&
            view.running &&
            is_valid_window_id(view.windowId) &&
            wantArea;
        if (!shouldShow)
        {
            continue;
        }
        // First make-visible: full path. Subsequent re-runs (area change while
        // the same view stays active) only need a SetWindowPos to move/resize,
        // not the reparent + ShowWindow + RedrawWindow churn.
        if (!view.nativelyShown)
        {
            place_native_hosted_window(
                view.windowId,
                parentWindowId,
                nativeArea,
                true,
                logger);
            view.nativelyShown = true;
        }
        else
        {
            HWND childHwnd = reinterpret_cast<HWND>(view.windowId);
            if (IsWindow(childHwnd))
            {
                SetWindowPos(
                    childHwnd,
                    nullptr,
                    nativeArea.x(),
                    nativeArea.y(),
                    nativeArea.width(),
                    nativeArea.height(),
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            }
        }
    }
    for (int index = 0; index < hostedViews_.size(); ++index)
    {
        HostedProcessView& view = hostedViews_[index];
        const bool shouldShow =
            !logConsoleActive_ &&
            index == activeHostedViewIndex_ &&
            view.running &&
            is_valid_window_id(view.windowId) &&
            wantArea;
        if (shouldShow || !view.nativelyShown || !is_valid_window_id(view.windowId))
        {
            continue;
        }
        place_native_hosted_window(
            view.windowId,
            parentWindowId,
            nativeArea,
            false,
            logger);
        view.nativelyShown = false;
    }
#else
    Q_UNUSED(parentWindow);
#endif
}

void DesktopShellController::freezeShellRedrawForHostedSwitch()
{
}

void DesktopShellController::restoreShellRedraw()
{
    shellRedrawFrozen_ = false;
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

void DesktopShellController::applyLocalization()
{
    for (HostedProcessView& view : hostedViews_)
    {
        const HostedViewMetadata metadata = hosted_view_metadata(view.id, localization_);
        if (metadata.title.isEmpty())
        {
            continue;
        }

        view.title = metadata.title;
        view.description = metadata.description;

        processMenuModel_.upsertItem(ProcessMenuModel::Item{
            view.id,
            view.title,
            view.description,
            view.status,
            false,
            view.running
        });

        const QString logServiceName = view.id.startsWith(QStringLiteral("stok."))
            ? view.id
            : QStringLiteral("stok.%1").arg(view.id);
        logProcessMenuModel_.upsertItem(ProcessMenuModel::Item{
            logServiceName,
            view.title,
            view.description,
            localization_.tr(QStringLiteral("status.ready")),
            activeLogProcessName_ == logServiceName,
            view.running
        });
    }

    if (!mainProcessLogPath_.isEmpty())
    {
        logProcessMenuModel_.upsertItem(ProcessMenuModel::Item{
            QStringLiteral("macchina"),
            localization_.tr(QStringLiteral("mainProcess.title")),
            localization_.tr(QStringLiteral("mainProcess.description")),
            QStringLiteral("file"),
            activeLogProcessName_ == QStringLiteral("macchina"),
            true
        });
    }

    if (activeHostedViewIndex_ >= 0)
    {
        processMenuModel_.setActiveRow(processMenuModel_.rowForId(hostedViews_[activeHostedViewIndex_].id));
    }

    if (!hostedViews_.isEmpty())
    {
        statusLine_ = localization_.tr(QStringLiteral("status.connectedFmt")).arg(hostedViews_.size());
    }
    else
    {
        statusLine_ = localization_.tr(QStringLiteral("status.waitingForRegistration"));
    }
    emit statusLineChanged();
    emit activeProcessChanged();
    emit activeProcessStatusChanged();
}
