#include "ConfigCenterController.h"

#include "stok/services/common/LocalizationClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

ConfigCenterController::ConfigCenterController(
    stok::services::common::DdsSettings settings,
    stok::services::common::LocalizationClient* localization,
    QObject* parent):
    QObject(parent),
    settings_(std::move(settings)),
    publisher_(settings_),
    localization_(localization)
{
    if (localization_)
    {
        localization_->define(QStringLiteral("cc.status.waiting"),
            QStringLiteral("Waiting to send config"), QStringLiteral(u"等待发送配置"));
        localization_->define(QStringLiteral("cc.status.ready"),
            QStringLiteral("Config sync ready"), QStringLiteral(u"配置同步已就绪"));
        localization_->define(QStringLiteral("cc.status.bus.fail.fmt"),
            QStringLiteral("Config bus connection failed: %1"), QStringLiteral(u"配置总线连接失败：%1"));
        localization_->define(QStringLiteral("cc.status.fail.fields"),
            QStringLiteral("Send failed: target, key and value are required"),
            QStringLiteral(u"配置发送失败：请检查目标、属性和值"));
        localization_->define(QStringLiteral("cc.status.sent.fmt"),
            QStringLiteral("Sent %1 -> %2 = %3"), QStringLiteral(u"已下发 %1 -> %2 = %3"));
        localization_->define(QStringLiteral("cc.status.send.fail.fmt"),
            QStringLiteral("Send failed: %1"), QStringLiteral(u"配置发送失败：%1"));

        connect(localization_, &stok::services::common::LocalizationClient::languageChanged,
                this, [this]()
        {
            // Re-emit status; QML re-renders. Stored status keeps its localized snapshot;
            // for simplicity reset to the appropriate state.
            if (started_)
            {
                status_ = tr(QStringLiteral("cc.status.ready"));
            }
            else
            {
                status_ = tr(QStringLiteral("cc.status.waiting"));
            }
            emit statusChanged();
        });
    }

    status_ = tr(QStringLiteral("cc.status.waiting"));
}

QString ConfigCenterController::tr(const QString& key) const
{
    return localization_ ? localization_->tr(key) : key;
}

QString ConfigCenterController::status() const
{
    return status_;
}

void ConfigCenterController::start(const QString& participantName)
{
    if (started_)
    {
        return;
    }

    std::string error;
    started_ = publisher_.start(participantName.toStdString(), &error);
    status_ = started_
        ? tr(QStringLiteral("cc.status.ready"))
        : tr(QStringLiteral("cc.status.bus.fail.fmt")).arg(QString::fromStdString(error));
    emit statusChanged();
}

bool ConfigCenterController::publishUpdate(const QString& target, const QString& key, const QString& value)
{
    if (!started_ || target.trimmed().isEmpty() || key.trimmed().isEmpty() || value.trimmed().isEmpty())
    {
        status_ = tr(QStringLiteral("cc.status.fail.fields"));
        emit statusChanged();
        return false;
    }

    const QJsonObject object{
        {"type", QStringLiteral("config_update")},
        {"target", target.trimmed()},
        {"key", key.trimmed()},
        {"value", value.trimmed()},
        {"timestampMs", QString::number(QDateTime::currentMSecsSinceEpoch())}
    };

    stok::services::common::TextMessage message;
    message.payload = QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
    message.timestampMs = QDateTime::currentMSecsSinceEpoch();

    std::string error;
    const bool published = publisher_.publish(message, &error);
    status_ = published
        ? tr(QStringLiteral("cc.status.sent.fmt")).arg(target.trimmed(), key.trimmed(), value.trimmed())
        : tr(QStringLiteral("cc.status.send.fail.fmt")).arg(QString::fromStdString(error));
    emit statusChanged();
    return published;
}
