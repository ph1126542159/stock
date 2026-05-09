#pragma once

#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/TextMessageBus.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <memory>

namespace stok::services::common {

// Cross-process language switch for child services. The desktop shell publishes
// the active language on a DDS topic; each child instantiates one of these,
// connects its dictionary, and exposes it to QML as a context property so
// labels can call localizationController.tr("key") and re-bind on
// languageChanged.
class LocalizationClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Language language READ language NOTIFY languageChanged)

public:
    enum Language
    {
        English,
        Chinese
    };
    Q_ENUM(Language)

    explicit LocalizationClient(QObject* parent = nullptr);
    ~LocalizationClient() override;

    Language language() const;

    // Register a (key, english, chinese) triple. Calling tr() with a key not
    // registered here returns the key itself so missing strings are visible.
    void define(const QString& key, const QString& english, const QString& chinese);

    Q_INVOKABLE QString tr(const QString& key) const;

    // Translate a literal Chinese phrase (the canonical "zh" value of some
    // registered key) into the active language. Returns the input unchanged
    // when no key with this Chinese value is registered. Useful for QML that
    // displays controller-provided values that are still hard-coded in
    // Chinese — wrap them as localizationController.trCn(value).
    Q_INVOKABLE QString trCn(const QString& chinese) const;

    // Subscribe to a DDS language-broadcast topic. The desktop shell publishes
    // {"type":"language","value":"en|zh"} payloads here whenever the user
    // toggles the language.
    bool start(const DdsSettings& settings, const std::string& participantName);
    void stop();

signals:
    void languageChanged();

private:
    void apply(Language next);

    Language language_ = Chinese;
    QHash<QString, QString> english_;
    QHash<QString, QString> chinese_;
    QHash<QString, QString> chineseToKey_;
    std::unique_ptr<TextMessageSubscriber> subscriber_;
};

} // namespace stok::services::common
