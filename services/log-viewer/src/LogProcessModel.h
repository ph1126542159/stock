#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class LogProcessModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        LevelRole,
        PreviewRole,
        ActiveRole
    };

    struct Item
    {
        QString name;
        QString level;
        QString preview;
        bool active = false;
    };

    explicit LogProcessModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int upsertItem(const Item& item);
    void setActiveRow(int row);
    const Item* itemAt(int row) const;

private:
    QVector<Item> items_;
};
