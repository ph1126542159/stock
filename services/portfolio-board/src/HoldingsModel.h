#pragma once

#include "PortfolioTypes.h"

#include <QAbstractTableModel>

class HoldingsModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit HoldingsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setEntries(QVector<HoldingEntry> entries);
    const HoldingEntry* entryAt(int row) const;

private:
    QVector<HoldingEntry> entries_;
};
