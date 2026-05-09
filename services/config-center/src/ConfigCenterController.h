#pragma once

#include "stok/services/common/TextMessageBus.h"

#include <QObject>
#include <QPointer>
#include <QString>

namespace stok::services::common { class LocalizationClient; }

class ConfigCenterController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ConfigCenterController(
        stok::services::common::DdsSettings settings,
        stok::services::common::LocalizationClient* localization,
        QObject* parent = nullptr);

    QString status() const;

    Q_INVOKABLE void start(const QString& participantName);
    Q_INVOKABLE bool publishUpdate(const QString& target, const QString& key, const QString& value);

signals:
    void statusChanged();

private:
    QString tr(const QString& key) const;

    stok::services::common::DdsSettings settings_;
    stok::services::common::TextMessagePublisher publisher_;
    QPointer<stok::services::common::LocalizationClient> localization_;
    QString status_;
    bool started_ = false;
};
