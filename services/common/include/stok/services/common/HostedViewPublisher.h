#pragma once

#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TextMessageBus.h"

#include <QObject>
#include <QPointer>
#include <QString>

class QTimer;
class QWindow;

namespace stok::services::common {

class HostedViewPublisher final : public QObject
{
public:
    HostedViewPublisher(
        DdsSettings settings,
        QString menuId,
        QString menuTitle,
        QString menuDescription,
        QString menuGroup,
        Poco::OpenTelemetry::TelemetryClient telemetry = Poco::OpenTelemetry::TelemetryClient(),
        QObject* parent = nullptr);

    void setHostWindow(QWindow* window);
    void setStatus(const QString& status);
    void start(const QString& participantName);

private:
    void prepareHostedWindow();
    void publishRegistration();

    DdsSettings settings_;
    QString participantName_;
    QString menuId_;
    QString menuTitle_;
    QString menuDescription_;
    QString menuGroup_;
    QString status_;
    TextMessagePublisher publisher_;
    QPointer<QWindow> hostWindow_;
    QTimer* publishTimer_ = nullptr;
};

} // namespace stok::services::common
