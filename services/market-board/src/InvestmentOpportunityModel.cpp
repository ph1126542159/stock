#include "InvestmentOpportunityModel.h"

#include <QString>

InvestmentOpportunityModel::InvestmentOpportunityModel(QObject* parent):
    QAbstractTableModel(parent)
{
}

int InvestmentOpportunityModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return entries_.size();
}

int InvestmentOpportunityModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return 7;
}

QVariant InvestmentOpportunityModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() ||
        role != Qt::DisplayRole ||
        index.row() < 0 ||
        index.row() >= entries_.size())
    {
        return {};
    }

    const ValueAssetEntry& entry = entries_.at(index.row());
    switch (index.column())
    {
    case 0:
        return entry.rank;
    case 1:
        return entry.name;
    case 2:
        return entry.code;
    case 3:
        return QString::number(entry.score, 'f', 1);
    case 4:
        return QString::number(entry.latestPrice, 'f', entry.latestPrice >= 1000.0 ? 2 : 3);
    case 5:
        return QStringLiteral("%1%")
            .arg(QString::number(entry.oneYearReturnPct, 'f', 1));
    case 6:
        return entry.provider + QStringLiteral(" / ") + entry.category;
    default:
        return {};
    }
}

QVariant InvestmentOpportunityModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation == Qt::Vertical)
    {
        return section + 1;
    }

    switch (section)
    {
    case 0:
        return QStringLiteral(u"\u6392\u540d");
    case 1:
        return QStringLiteral(u"\u540d\u79f0");
    case 2:
        return QStringLiteral(u"\u4ee3\u7801");
    case 3:
        return QStringLiteral(u"\u4ef7\u503c\u5206");
    case 4:
        return QStringLiteral(u"\u6700\u65b0\u4ef7");
    case 5:
        return QStringLiteral(u"\u8fd11\u5e74");
    case 6:
        return QStringLiteral(u"\u53d1\u884c / \u8d5b\u9053");
    default:
        return {};
    }
}

void InvestmentOpportunityModel::setEntries(QVector<ValueAssetEntry> entries)
{
    beginResetModel();
    entries_ = std::move(entries);
    endResetModel();
}

const ValueAssetEntry* InvestmentOpportunityModel::entryAt(int row) const
{
    if (row < 0 || row >= entries_.size())
    {
        return nullptr;
    }

    return &entries_.at(row);
}
