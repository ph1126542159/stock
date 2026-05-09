#include "LogProcessModel.h"

LogProcessModel::LogProcessModel(QObject* parent):
    QAbstractListModel(parent)
{
}

int LogProcessModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : items_.size();
}

QVariant LogProcessModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
    {
        return {};
    }

    const Item& item = items_.at(index.row());
    switch (role)
    {
    case NameRole:
        return item.name;
    case LevelRole:
        return item.level;
    case PreviewRole:
        return item.preview;
    case ActiveRole:
        return item.active;
    case Qt::DisplayRole:
        return item.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> LogProcessModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {LevelRole, "level"},
        {PreviewRole, "preview"},
        {ActiveRole, "active"}
    };
}

int LogProcessModel::upsertItem(const Item& item)
{
    for (int row = 0; row < items_.size(); ++row)
    {
        if (items_[row].name != item.name)
        {
            continue;
        }

        Item updated = item;
        updated.active = items_[row].active;
        items_[row] = updated;
        emit dataChanged(index(row, 0), index(row, 0), {NameRole, LevelRole, PreviewRole, ActiveRole});
        return row;
    }

    const int row = items_.size();
    beginInsertRows(QModelIndex(), row, row);
    items_.push_back(item);
    endInsertRows();
    return row;
}

void LogProcessModel::setActiveRow(int row)
{
    for (int indexValue = 0; indexValue < items_.size(); ++indexValue)
    {
        const bool active = indexValue == row;
        if (items_[indexValue].active == active)
        {
            continue;
        }

        items_[indexValue].active = active;
        emit dataChanged(index(indexValue, 0), index(indexValue, 0), {ActiveRole});
    }
}

const LogProcessModel::Item* LogProcessModel::itemAt(int row) const
{
    if (row < 0 || row >= items_.size())
    {
        return nullptr;
    }

    return &items_.at(row);
}
