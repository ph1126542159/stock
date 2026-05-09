#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QVector>

class QWindow;

class ProcessMenuModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        StatusRole,
        ActiveRole,
        RunningRole,
        WindowRole
    };
    Q_ENUM(Role)

    struct Item
    {
        QString id;
        QString title;
        QString description;
        QString status;
        bool active = false;
        bool running = false;
        QPointer<QWindow> window;
    };

    explicit ProcessMenuModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<Item> items);
    int upsertItem(const Item& item);
    void setItemStatus(int row, const QString& status);
    void setItemRunning(int row, bool running);
    void setItemWindow(int row, QWindow* window);
    void setActiveRow(int row);
    QString idAt(int row) const;
    int rowForId(const QString& id) const;

private:
    QVector<Item> items_;
};
