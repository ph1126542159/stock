#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

namespace stok::services::common { class LocalizationClient; }

class TradeAlertsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scoreCards READ scoreCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList planRows READ planRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList alertRows READ alertRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList reviewRows READ reviewRows NOTIFY dataChanged)

public:
    explicit TradeAlertsController(stok::services::common::LocalizationClient* localization,
                                   QObject* parent = nullptr);

    QString status() const;
    QVariantList scoreCards() const;
    QVariantList planRows() const;
    QVariantList alertRows() const;
    QVariantList reviewRows() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void acknowledgeAlert(int index);

signals:
    void dataChanged();

private:
    void rebuild();

    QPointer<stok::services::common::LocalizationClient> localization_;
    QString status_;
    QVariantList scoreCards_;
    QVariantList planRows_;
    QVariantList alertRows_;
    QVariantList reviewRows_;
};

QObject* createTradeAlertsController(QObject* parent, stok::services::common::LocalizationClient* localization);
