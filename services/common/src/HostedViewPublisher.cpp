#include "stok/services/common/HostedViewPublisher.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWindow>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stok::services::common {

HostedViewPublisher::HostedViewPublisher(
    DdsSettings settings,
    QString menuId,
    QString menuTitle,
    QString menuDescription,
    QString menuGroup,
    Poco::OpenTelemetry::TelemetryClient telemetry,
    QObject* parent):
    QObject(parent),
    settings_(std::move(settings)),
    menuId_(std::move(menuId)),
    menuTitle_(std::move(menuTitle)),
    menuDescription_(std::move(menuDescription)),
    menuGroup_(std::move(menuGroup)),
    status_(QStringLiteral("online")),
    publisher_(settings_, std::move(telemetry))
{
    publishTimer_ = new QTimer(this);
    publishTimer_->setInterval(5000);
    connect(publishTimer_, &QTimer::timeout, this, [this]()
    {
        publishRegistration();
    });
}

void HostedViewPublisher::setHostWindow(QWindow* window)
{
    hostWindow_ = window;
    prepareHostedWindow();
    publishRegistration();
}

void HostedViewPublisher::setStatus(const QString& status)
{
    status_ = status.trimmed();
    if (status_.isEmpty())
    {
        status_ = QStringLiteral("online");
    }
    publishRegistration();
}

void HostedViewPublisher::start(const QString& participantName)
{
    participantName_ = participantName.trimmed();
    if (participantName_.isEmpty())
    {
        participantName_ = QStringLiteral("hosted-view-publisher");
    }
    participantName_ = QStringLiteral("%1-%2")
        .arg(participantName_)
        .arg(QCoreApplication::applicationPid());

    std::string error;
    publisher_.start(participantName_.toStdString(), &error);
    publishTimer_->start();
    publishRegistration();
}

void HostedViewPublisher::prepareHostedWindow()
{
    if (!hostWindow_)
    {
        return;
    }

    Qt::WindowFlags flags = hostWindow_->flags();
    flags |= Qt::Tool;
    flags |= Qt::FramelessWindowHint;
    flags &= ~Qt::Window;
    hostWindow_->setFlags(flags);
    hostWindow_->hide();

    const WId windowId = hostWindow_->winId();
    if (windowId == 0)
    {
        return;
    }

#if defined(Q_OS_WIN)
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    exStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_HIDEWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
#endif
}

void HostedViewPublisher::publishRegistration()
{
    if (!hostWindow_)
    {
        return;
    }

    const WId windowId = hostWindow_->winId();
    if (windowId == 0)
    {
        return;
    }

    const QJsonObject object{
        {"type", QStringLiteral("register_view")},
        {"id", menuId_},
        {"title", menuTitle_},
        {"description", menuDescription_},
        {"group", menuGroup_},
        {"status", status_},
        {"windowId", QString::number(static_cast<qulonglong>(windowId))},
        {"processId", QString::number(QCoreApplication::applicationPid())},
        {"timestampMs", QString::number(QDateTime::currentMSecsSinceEpoch())}
    };

    TextMessage message;
    message.payload = QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
    message.timestampMs = QDateTime::currentMSecsSinceEpoch();
    publisher_.publish(message, nullptr);
}

} // namespace stok::services::common
