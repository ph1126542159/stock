#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

namespace stok::services::common { class LocalizationClient; }

class ValuationResearchController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scoreCards READ scoreCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList valuationRows READ valuationRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList researchRows READ researchRows NOTIFY dataChanged)
    Q_PROPERTY(QVariantList thesisRows READ thesisRows NOTIFY dataChanged)

public:
    explicit ValuationResearchController(stok::services::common::LocalizationClient* localization,
                                         QObject* parent = nullptr);

    QString status() const;
    QVariantList scoreCards() const;
    QVariantList valuationRows() const;
    QVariantList researchRows() const;
    QVariantList thesisRows() const;

    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    void rebuild();

    QPointer<stok::services::common::LocalizationClient> localization_;
    QString status_;
    QVariantList scoreCards_;
    QVariantList valuationRows_;
    QVariantList researchRows_;
    QVariantList thesisRows_;
};

QObject* createValuationResearchController(QObject* parent, stok::services::common::LocalizationClient* localization);
