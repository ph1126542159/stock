#include "MarketBoardBridge.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QWindow>

MarketBoardBridge::MarketBoardBridge(
    QString serverName,
    QString menuId,
    QString menuTitle,
    QString menuDescription,
    QObject* parent):
    QObject(parent),
    serverName_(std::move(serverName)),
    menuId_(std::move(menuId)),
    menuTitle_(std::move(menuTitle)),
    menuDescription_(std::move(menuDescription))
{
    retryTimer_ = new QTimer(this);
    retryTimer_->setInterval(1000);
    connect(retryTimer_, &QTimer::timeout, this, &MarketBoardBridge::ensureConnection);

    socket_ = new QLocalSocket(this);
    connect(socket_, &QLocalSocket::connected, this, [this]()
    {
        registrationSent_ = false;
        sendRegistration();
    });
    connect(socket_, &QLocalSocket::disconnected, this, [this]()
    {
        registrationSent_ = false;
        retryTimer_->start();
    });
    connect(socket_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError)
    {
        registrationSent_ = false;
        if (!retryTimer_->isActive())
        {
            retryTimer_->start();
        }
    });
}

void MarketBoardBridge::setHostWindow(QWindow* window)
{
    hostWindow_ = window;
    sendRegistration();
}

void MarketBoardBridge::start()
{
    ensureConnection();
}

void MarketBoardBridge::ensureConnection()
{
    if (serverName_.isEmpty())
    {
        return;
    }

    if (socket_->state() == QLocalSocket::ConnectedState)
    {
        sendRegistration();
        return;
    }

    if (socket_->state() == QLocalSocket::ConnectingState)
    {
        return;
    }

    socket_->abort();
    socket_->connectToServer(serverName_, QIODevice::ReadWrite);
}

void MarketBoardBridge::sendRegistration()
{
    if (registrationSent_ ||
        socket_->state() != QLocalSocket::ConnectedState ||
        !hostWindow_)
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
        {"status", QStringLiteral("在线")},
        {"windowId", QString::number(static_cast<qulonglong>(windowId))},
        {"processId", QString::number(QCoreApplication::applicationPid())}
    };

    socket_->write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    socket_->write("\n");
    socket_->flush();

    registrationSent_ = true;
    retryTimer_->stop();
}
