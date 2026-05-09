#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QLocalSocket;
class QTimer;
class QWindow;

class MarketBoardBridge final : public QObject
{
    Q_OBJECT

public:
    MarketBoardBridge(
        QString serverName,
        QString menuId,
        QString menuTitle,
        QString menuDescription,
        QObject* parent = nullptr);

    void setHostWindow(QWindow* window);
    void start();

private:
    void ensureConnection();
    void sendRegistration();

    QString serverName_;
    QString menuId_;
    QString menuTitle_;
    QString menuDescription_;
    QLocalSocket* socket_ = nullptr;
    QTimer* retryTimer_ = nullptr;
    QPointer<QWindow> hostWindow_;
    bool registrationSent_ = false;
};
