#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <vector>

class WealthProductModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit WealthProductModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct ProductRow
    {
        int rank = 0;
        QString institution;
        QString entry;
        QString category;
        QString riskLevel;
        QString liquidity;
        QString notes;
    };

    std::vector<ProductRow> rows_;
};
