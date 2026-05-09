#include "UsMarketWatchModel.h"

#include <QString>
#include <QVariantList>

UsMarketWatchModel::UsMarketWatchModel(QObject* parent):
    QAbstractTableModel(parent)
{
}

int UsMarketWatchModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return entries_.size();
}

int UsMarketWatchModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return 6;
}

QVariant UsMarketWatchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= entries_.size())
    {
        return {};
    }

    const UsMarketWatchEntry& entry = entries_.at(index.row());
    switch (role)
    {
    case SymbolRole:
        return entry.symbol;
    case NameRole:
        return entry.name;
    case MarketRole:
        return entry.market;
    case LastPriceRole:
        return entry.lastPrice;
    case OneHourChangePctRole:
        return entry.oneHourChangePct;
    case RealtimeTrendRole:
    {
        QVariantList values;
        values.reserve(entry.realtimeTrend.size());
        for (double value : entry.realtimeTrend)
        {
            values.append(value);
        }
        return values;
    }
    case UpdatedAtRole:
        return entry.updatedAt;
    case Qt::DisplayRole:
        break;
    default:
        return {};
    }

    switch (index.column())
    {
    case 0:
        return entry.symbol;
    case 1:
        return entry.name;
    case 2:
        return entry.market;
    case 3:
        return QString::number(entry.lastPrice, 'f', entry.lastPrice >= 1000.0 ? 2 : 3);
    case 4:
        return QStringLiteral("%1%2%")
            .arg(entry.oneHourChangePct >= 0.0 ? "+" : "")
            .arg(QString::number(entry.oneHourChangePct, 'f', 2));
    case 5:
        return QStringLiteral("sparkline");
    default:
        return {};
    }
}

QVariant UsMarketWatchModel::headerData(int section, Qt::Orientation orientation, int role) const
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
        return QStringLiteral(u"\u4ee3\u7801");
    case 1:
        return QStringLiteral(u"\u540d\u79f0");
    case 2:
        return QStringLiteral(u"\u5e02\u573a");
    case 3:
        return QStringLiteral(u"\u6700\u65b0\u4ef7");
    case 4:
        return QStringLiteral(u"\u6da8\u8dcc\u5e45");
    case 5:
        return QStringLiteral(u"\u5b9e\u65f6\u66f2\u7ebf");
    default:
        return {};
    }
}

QHash<int, QByteArray> UsMarketWatchModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {SymbolRole, "symbol"},
        {NameRole, "name"},
        {MarketRole, "market"},
        {LastPriceRole, "lastPrice"},
        {OneHourChangePctRole, "oneHourChangePct"},
        {RealtimeTrendRole, "realtimeTrend"},
        {UpdatedAtRole, "updatedAt"}
    };
}

void UsMarketWatchModel::setEntries(QVector<UsMarketWatchEntry> entries)
{
    const bool sameShape = entries_.size() == entries.size();
    bool canUpdateInPlace = sameShape;
    if (canUpdateInPlace)
    {
        for (int row = 0; row < entries_.size(); ++row)
        {
            if (entries_.at(row).symbol != entries.at(row).symbol)
            {
                canUpdateInPlace = false;
                break;
            }
        }
    }

    if (!canUpdateInPlace)
    {
        beginResetModel();
        entries_ = std::move(entries);
        endResetModel();
        return;
    }


    entries_ = std::move(entries);
    if (!entries_.isEmpty())
    {
        emit dataChanged(
            index(0, 0),
            index(entries_.size() - 1, columnCount() - 1),
            {
                Qt::DisplayRole,
                LastPriceRole,
                OneHourChangePctRole,
                RealtimeTrendRole,
                UpdatedAtRole
            });
    }
}

const UsMarketWatchEntry* UsMarketWatchModel::entryAt(int row) const
{
    if (row < 0 || row >= entries_.size())
    {
        return nullptr;
    }

    return &entries_.at(row);
}
