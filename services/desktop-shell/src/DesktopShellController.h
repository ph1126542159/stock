#pragma once

#include "LocalizationController.h"
#include "ProcessMenuModel.h"
#include "QuoteListModel.h"
#include "Poco/AutoPtr.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/StockQuoteBus.h"
#include "stok/services/common/TelemetryBootstrap.h"
#include "stok/services/common/TextMessageBus.h"
#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointer>
#include <QWindow>
#include <tuple>

class QTimer;

class DesktopShellController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QuoteListModel* quotesModel READ quotesModel CONSTANT)
    Q_PROPERTY(ProcessMenuModel* processMenuModel READ processMenuModel CONSTANT)
    Q_PROPERTY(ProcessMenuModel* logProcessMenuModel READ logProcessMenuModel CONSTANT)
    Q_PROPERTY(QString serviceName READ serviceName CONSTANT)
    Q_PROPERTY(QString topicName READ topicName CONSTANT)
    Q_PROPERTY(QString telemetryMode READ telemetryMode CONSTANT)
    Q_PROPERTY(QString feedState READ feedState NOTIFY feedStateChanged)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY statusLineChanged)
    Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(int matchedPublishers READ matchedPublishers NOTIFY matchedPublishersChanged)
    Q_PROPERTY(int quoteCount READ quoteCount NOTIFY quoteCountChanged)
    Q_PROPERTY(QString activeProcessTitle READ activeProcessTitle NOTIFY activeProcessChanged)
    Q_PROPERTY(QString activeProcessDescription READ activeProcessDescription NOTIFY activeProcessChanged)
    Q_PROPERTY(QString activeProcessStatus READ activeProcessStatus NOTIFY activeProcessStatusChanged)
    Q_PROPERTY(int activeProcessRow READ activeProcessRow NOTIFY activeProcessChanged)
    Q_PROPERTY(QWindow* activeProcessWindow READ activeProcessWindow NOTIFY activeProcessWindowChanged)
    Q_PROPERTY(bool activeProcessReady READ activeProcessReady NOTIFY activeProcessWindowChanged)
    Q_PROPERTY(bool logConsoleActive READ logConsoleActive NOTIFY logConsoleChanged)
    Q_PROPERTY(QString activeLogProcessName READ activeLogProcessName NOTIFY logConsoleChanged)
    Q_PROPERTY(QString activeLogText READ activeLogText NOTIFY logConsoleChanged)

public:
    DesktopShellController(
        const stok::services::common::ServiceIdentity& identity,
        const stok::services::common::DdsSettings& ddsSettings,
        QString configPath,
        Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration,
        stok::services::common::ServiceTelemetry& telemetry,
        LocalizationController& localization,
        QObject* parent = nullptr);
    ~DesktopShellController() override;

    QuoteListModel* quotesModel();
    ProcessMenuModel* processMenuModel();
    ProcessMenuModel* logProcessMenuModel();
    QString serviceName() const;
    QString topicName() const;
    QString telemetryMode() const;
    QString feedState() const;
    QString statusLine() const;
    QString lastUpdated() const;
    int matchedPublishers() const;
    int quoteCount() const;
    QString activeProcessTitle() const;
    QString activeProcessDescription() const;
    QString activeProcessStatus() const;
    int activeProcessRow() const;
    QWindow* activeProcessWindow() const;
    bool activeProcessReady() const;
    bool logConsoleActive() const;
    QString activeLogProcessName() const;
    QString activeLogText() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void activateProcessMenu(int index);
    Q_INVOKABLE void activateLogProcess(int index);
    Q_INVOKABLE void attachShellWindow(QWindow* window);
    void attachShellWindowId(quintptr windowId);
    void attachHostedContainerWindowId(quintptr windowId);
    Q_INVOKABLE void setHostedWindowArea(int x, int y, int width, int height);
    Q_INVOKABLE void resizeActiveProcessWindow(int width, int height);
    Q_INVOKABLE void requestManagedShutdown();

signals:
    void feedStateChanged();
    void statusLineChanged();
    void lastUpdatedChanged();
    void matchedPublishersChanged();
    void quoteCountChanged();
    void activeProcessChanged();
    void activeProcessStatusChanged();
    void activeProcessWindowChanged();
    void logConsoleChanged();

private:
    struct HostedProcessView
    {
        QString id;
        QString title;
        QString description;
        QString group;
        QString status = QStringLiteral("Offline");
        quintptr windowId = 0;
        qint64 processId = 0;
        QPointer<QWindow> foreignWindow;
        bool running = false;
        qint64 lastSeenMs = 0;
        // Tracks whether this view's HWND is currently shown inside the
        // shell's container. Used so updateNativeHostedWindows() can skip
        // cross-process ShowWindow calls on transitions that are already
        // in the desired state.
        bool nativelyShown = false;
    };

    void startProcessBridge();
    void stopProcessBridge();
    void startLogBridge();
    void stopLogBridge();
    void refreshMainProcessLog();
    void registerHostedProcess(const QString& payload);
    void registerLogMessage(const QString& payload);
    void discoverLocalHostedWindows();
    void refreshHostedProcessState();
    bool ensureHostedWindow(int index);
    int preferredHostedViewIndex() const;
    void syncHostedWindowVisibility();
    void hideInactiveHostedWindows();
    void updateNativeHostedWindows();
    void freezeShellRedrawForHostedSwitch();
    void restoreShellRedraw();
    int viewIndexForId(const QString& id) const;
    void setActiveProcessIndex(int index);
    void refreshActiveProcessSignals();
    void handleQuote(const stok::services::common::StockQuote& quote);
    void handleMatchCount(int count);
    void applyLocalization();

    stok::services::common::ServiceIdentity identity_;
    stok::services::common::DdsSettings ddsSettings_;
    QString configPath_;
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration_;
    stok::services::common::ServiceTelemetry& telemetry_;
    LocalizationController& localization_;
    QuoteListModel quotesModel_;
    ProcessMenuModel processMenuModel_;
    ProcessMenuModel logProcessMenuModel_;
    stok::services::common::StockQuoteSubscriber subscriber_;
    QString telemetryMode_;
    QString feedState_ = QStringLiteral("Connecting");
    QString statusLine_ = QStringLiteral("Waiting for the market bus");
    QString lastUpdated_ = QStringLiteral("No quotes yet");
    int matchedPublishers_ = 0;
    QTimer* heartbeatTimer_ = nullptr;
    QTimer* mainProcessLogTimer_ = nullptr;
    bool localHostedDiscoveryInProgress_ = false;
    stok::services::common::TextMessageSubscriber hostedViewSubscriber_;
    stok::services::common::TextMessageSubscriber logSubscriber_;
    QString hostedViewTopicName_;
    QString logTopicName_;
    QString mainProcessLogPath_;
    qint64 mainProcessLogPosition_ = 0;
    QString defaultHostedViewId_;
    int hostedViewMatches_ = 0;
    QVector<HostedProcessView> hostedViews_;
    int activeHostedViewIndex_ = -1;
    bool logConsoleActive_ = false;
    bool userSelectedHostedView_ = false;
    QString activeLogProcessName_;
    QHash<QString, QStringList> logLinesByProcess_;
    bool shellStarted_ = false;
    bool processBridgeStarted_ = false;
    bool logBridgeStarted_ = false;
    QElapsedTimer hostedWindowLossGuard_;
    QPointer<QWindow> shellWindow_;
    quintptr shellWindowId_ = 0;
    quintptr hostedContainerWindowId_ = 0;
    QRect hostedWindowArea_;
    QTimer* hostedSwitchRedrawTimer_ = nullptr;
    bool shellRedrawFrozen_ = false;
    // (parent winId, active view index, log-console flag, area x, y, w, h,
    //  active view's running flag, active view's windowId).
    // The trailing two fields make the key change when a child registers or
    // exits, so updateNativeHostedWindows() re-runs the embedding work.
    std::tuple<quintptr, int, bool, int, int, int, int, bool, quintptr>
        lastEmbeddingStateKey_{0, -1, false, 0, 0, 0, 0, false, 0};
};
