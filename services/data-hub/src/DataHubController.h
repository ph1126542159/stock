#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

namespace stok::services::common { class LocalizationClient; }

class DataHubController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scoreCards READ scoreCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList sourceRows READ sourceRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList latencyRows READ latencyRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList governanceRows READ governanceRows NOTIFY dataChanged)

public:
    explicit DataHubController(stok::services::common::LocalizationClient* localization,
                               QObject* parent = nullptr);

    QString status() const;
    QVariantList scoreCards() const;
    QVariantList sourceRows() const;
    QVariantList latencyRows() const;
    QVariantList governanceRows() const;

    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    void rebuild();

    QPointer<stok::services::common::LocalizationClient> localization_;
    QString status_;
    QVariantList scoreCards_;
    QVariantList sourceRows_;
    QVariantList latencyRows_;
    QVariantList governanceRows_;
};

QObject* createDataHubController(QObject* parent, stok::services::common::LocalizationClient* localization);
