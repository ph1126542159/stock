#pragma once

#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TextMessageBus.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <memory>

class LocalizationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Language language READ language NOTIFY languageChanged)
    Q_PROPERTY(QString toggleLabel READ toggleLabel NOTIFY languageChanged)

public:
    enum Language
    {
        English,
        Chinese
    };
    Q_ENUM(Language)

    explicit LocalizationController(QObject* parent = nullptr);
    ~LocalizationController() override;

    // Start broadcasting language changes on a DDS topic so child services can
    // sync. Called by the shell's main() once DDS settings are known.
    void startBroadcast(
        const stok::services::common::DdsSettings& settings,
        const std::string& participantName);

    Language language() const;
    QString toggleLabel() const;

    Q_INVOKABLE QString tr(const QString& key) const;
    Q_INVOKABLE void toggle();

signals:
    void languageChanged();

private:
    void publishCurrentLanguage();

    Language language_ = Chinese;
    QHash<QString, QString> english_;
    QHash<QString, QString> chinese_;
    std::unique_ptr<stok::services::common::TextMessagePublisher> publisher_;
};
