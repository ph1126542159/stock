#include "ProcessMenuModel.h"

#include <QWindow>

ProcessMenuModel::ProcessMenuModel(QObject* parent):
    QAbstractListModel(parent)
{
}

int ProcessMenuModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return items_.size();
}

QVariant ProcessMenuModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
    {
        return {};
    }

    const Item& item = items_.at(index.row());
    switch (role)
    {
    case IdRole:
        return item.id;
    case TitleRole:
        return item.title;
    case DescriptionRole:
        return item.description;
    case StatusRole:
        return item.status;
    case ActiveRole:
        return item.active;
    case RunningRole:
        return item.running;
    case WindowRole:
        return QVariant::fromValue<QWindow*>(item.window);
    default:
        return {};
    }
}

QHash<int, QByteArray> ProcessMenuModel::roleNames() const
{
    return {
        {IdRole, "menuId"},
        {TitleRole, "title"},
        {DescriptionRole, "description"},
        {StatusRole, "status"},
        {ActiveRole, "active"},
        {RunningRole, "running"},
        {WindowRole, "processWindow"}
    };
}

void ProcessMenuModel::setItems(QVector<Item> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

int ProcessMenuModel::upsertItem(const Item& item)
{
    const int existingRow = rowForId(item.id);
    if (existingRow >= 0)
    {
        Item updated = item;
        updated.active = items_[existingRow].active;
        updated.window = items_[existingRow].window;
        if (items_[existingRow].id == updated.id &&
            items_[existingRow].title == updated.title &&
            items_[existingRow].description == updated.description &&
            items_[existingRow].status == updated.status &&
            items_[existingRow].active == updated.active &&
            items_[existingRow].running == updated.running &&
            items_[existingRow].window == updated.window)
        {
            return existingRow;
        }

        items_[existingRow] = std::move(updated);
        const QModelIndex changed = index(existingRow, 0);
        emit dataChanged(
            changed,
            changed,
            {IdRole, TitleRole, DescriptionRole, StatusRole, ActiveRole, RunningRole, WindowRole});
        return existingRow;
    }

    const int insertRow = items_.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    items_.push_back(item);
    endInsertRows();
    return insertRow;
}

void ProcessMenuModel::setItemStatus(int row, const QString& status)
{
    if (row < 0 || row >= items_.size() || items_[row].status == status)
    {
        return;
    }

    items_[row].status = status;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {StatusRole});
}

void ProcessMenuModel::setItemRunning(int row, bool running)
{
    if (row < 0 || row >= items_.size() || items_[row].running == running)
    {
        return;
    }

    items_[row].running = running;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {RunningRole});
}

void ProcessMenuModel::setItemWindow(int row, QWindow* window)
{
    if (row < 0 || row >= items_.size() || items_[row].window == window)
    {
        return;
    }

    items_[row].window = window;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {WindowRole});
}

void ProcessMenuModel::setActiveRow(int row)
{
    for (int indexValue = 0; indexValue < items_.size(); ++indexValue)
    {
        const bool shouldBeActive = indexValue == row;
        if (items_[indexValue].active == shouldBeActive)
        {
            continue;
        }

        items_[indexValue].active = shouldBeActive;
        const QModelIndex changed = index(indexValue, 0);
        emit dataChanged(changed, changed, {ActiveRole});
    }
}

QString ProcessMenuModel::idAt(int row) const
{
    if (row < 0 || row >= items_.size())
    {
        return {};
    }

    return items_.at(row).id;
}

int ProcessMenuModel::rowForId(const QString& id) const
{
    for (int row = 0; row < items_.size(); ++row)
    {
        if (items_[row].id == id)
        {
            return row;
        }
    }

    return -1;
}
