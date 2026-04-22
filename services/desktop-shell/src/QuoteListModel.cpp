#include "QuoteListModel.h"

#include <QDateTime>

QuoteListModel::QuoteListModel(QObject* parent):
    QAbstractListModel(parent)
{
}

int QuoteListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return static_cast<int>(items_.size());
}

QVariant QuoteListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const QuoteItem& item = items_[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case SymbolRole:
        return item.symbol;
    case NameRole:
        return item.name;
    case MarketRole:
        return item.market;
    case PriceRole:
        return item.price;
    case ChangeRole:
        return item.change;
    case PercentRole:
        return item.percent;
    case VolumeRole:
        return static_cast<qulonglong>(item.volume);
    case TimestampRole:
        return item.timestampMs;
    case TimestampTextRole:
        return item.timestampMs > 0
            ? QDateTime::fromMSecsSinceEpoch(item.timestampMs).toString("hh:mm:ss")
            : QStringLiteral("--:--:--");
    case LiveRole:
        return item.live;
    default:
        return {};
    }
}

QHash<int, QByteArray> QuoteListModel::roleNames() const
{
    return {
        {SymbolRole, "symbol"},
        {NameRole, "name"},
        {MarketRole, "market"},
        {PriceRole, "price"},
        {ChangeRole, "change"},
        {PercentRole, "percent"},
        {VolumeRole, "volume"},
        {TimestampRole, "timestampMs"},
        {TimestampTextRole, "timestampText"},
        {LiveRole, "live"}
    };
}

int QuoteListModel::count() const
{
    return rowCount();
}

void QuoteListModel::seedSymbols(const QStringList& symbols)
{
    if (symbols.isEmpty())
    {
        return;
    }

    beginResetModel();
    items_.clear();
    items_.reserve(static_cast<std::size_t>(symbols.size()));
    for (const QString& symbol : symbols)
    {
        QuoteItem item;
        item.symbol = symbol;
        item.name = symbol;
        item.market = QStringLiteral("Waiting");
        items_.push_back(item);
    }
    endResetModel();
    emit countChanged();
}

void QuoteListModel::upsertQuote(
    const QString& symbol,
    const QString& name,
    const QString& market,
    double price,
    double change,
    double percent,
    quint64 volume,
    qint64 timestampMs)
{
    for (std::size_t index = 0; index < items_.size(); ++index)
    {
        if (items_[index].symbol == symbol)
        {
            QuoteItem& item = items_[index];
            item.name = name;
            item.market = market;
            item.price = price;
            item.change = change;
            item.percent = percent;
            item.volume = volume;
            item.timestampMs = timestampMs;
            item.live = true;

            const QModelIndex changed = createIndex(static_cast<int>(index), 0);
            emit dataChanged(changed, changed);
            return;
        }
    }

    const int insertionIndex = rowCount();
    beginInsertRows(QModelIndex(), insertionIndex, insertionIndex);
    QuoteItem item;
    item.symbol = symbol;
    item.name = name;
    item.market = market;
    item.price = price;
    item.change = change;
    item.percent = percent;
    item.volume = volume;
    item.timestampMs = timestampMs;
    item.live = true;
    items_.push_back(item);
    endInsertRows();
    emit countChanged();
}
