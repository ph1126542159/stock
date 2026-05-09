#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

namespace stok::services::common { class LocalizationClient; }

class RiskBacktestController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scoreCards READ scoreCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList exposureRows READ exposureRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList correlationRows READ correlationRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList backtestRows READ backtestRows NOTIFY dataChanged)

public:
    explicit RiskBacktestController(stok::services::common::LocalizationClient* localization,
                                    QObject* parent = nullptr);

    QString status() const;
    QVariantList scoreCards() const;
    QVariantList exposureRows() const;
    QVariantList correlationRows() const;
    QVariantList backtestRows() const;

    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    void rebuild();

    QPointer<stok::services::common::LocalizationClient> localization_;
    QString status_;
    QVariantList scoreCards_;
    QVariantList exposureRows_;
    QVariantList correlationRows_;
    QVariantList backtestRows_;
};

QObject* createRiskBacktestController(QObject* parent, stok::services::common::LocalizationClient* localization);
