#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <vector>

class QuoteListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role
    {
        SymbolRole = Qt::UserRole + 1,
        NameRole,
        MarketRole,
        PriceRole,
        ChangeRole,
        PercentRole,
        VolumeRole,
        TimestampRole,
        TimestampTextRole,
        LiveRole
    };
    Q_ENUM(Role)

    explicit QuoteListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    void seedSymbols(const QStringList& symbols);
    void upsertQuote(
        const QString& symbol,
        const QString& name,
        const QString& market,
        double price,
        double change,
        double percent,
        quint64 volume,
        qint64 timestampMs);

signals:
    void countChanged();

private:
    struct QuoteItem
    {
        QString symbol;
        QString name;
        QString market;
        double price = 0.0;
        double change = 0.0;
        double percent = 0.0;
        quint64 volume = 0;
        qint64 timestampMs = 0;
        bool live = false;
    };

    std::vector<QuoteItem> items_;
};
