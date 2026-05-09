#include "stok/services/common/LocalizationClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

namespace stok::services::common {

LocalizationClient::LocalizationClient(QObject* parent):
    QObject(parent)
{
}

LocalizationClient::~LocalizationClient()
{
    stop();
}

LocalizationClient::Language LocalizationClient::language() const
{
    return language_;
}

void LocalizationClient::define(const QString& key, const QString& english, const QString& chinese)
{
    english_.insert(key, english);
    chinese_.insert(key, chinese);
    if (!chinese.isEmpty())
    {
        chineseToKey_.insert(chinese, key);
    }
}

QString LocalizationClient::tr(const QString& key) const
{
    const auto& table = language_ == English ? english_ : chinese_;
    const auto it = table.constFind(key);
    if (it != table.constEnd())
    {
        return it.value();
    }
    const auto& fallback = language_ == English ? chinese_ : english_;
    const auto fallbackIt = fallback.constFind(key);
    return fallbackIt != fallback.constEnd() ? fallbackIt.value() : key;
}

QString LocalizationClient::trCn(const QString& chinese) const
{
    if (chinese.isEmpty())
    {
        return chinese;
    }
    const auto it = chineseToKey_.constFind(chinese);
    if (it == chineseToKey_.constEnd())
    {
        return chinese;
    }
    return tr(it.value());
}

void LocalizationClient::apply(Language next)
{
    if (next == language_)
    {
        return;
    }
    language_ = next;
    emit languageChanged();
}

bool LocalizationClient::start(const DdsSettings& settings, const std::string& participantName)
{
    if (subscriber_)
    {
        return true;
    }

    subscriber_ = std::make_unique<TextMessageSubscriber>(settings);

    std::string error;
    const bool ok = subscriber_->start(
        participantName,
        [this](const TextMessage& message)
        {
            const auto document = QJsonDocument::fromJson(
                QByteArray(message.payload.data(), static_cast<qsizetype>(message.payload.size())));
            if (!document.isObject())
            {
                return;
            }
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("type")).toString() != QStringLiteral("language"))
            {
                return;
            }
            const QString value = object.value(QStringLiteral("value")).toString().trimmed().toLower();
            const Language next = (value == QStringLiteral("zh") || value == QStringLiteral("chinese"))
                ? Chinese
                : English;
            QMetaObject::invokeMethod(this, [this, next]()
            {
                apply(next);
            }, Qt::QueuedConnection);
        },
        {},
        &error);

    if (!ok)
    {
        subscriber_.reset();
    }
    return ok;
}

void LocalizationClient::stop()
{
    if (!subscriber_)
    {
        return;
    }
    subscriber_->stop();
    subscriber_.reset();
}

} // namespace stok::services::common
