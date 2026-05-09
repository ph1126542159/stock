#include "InstitutionRankingModel.h"

#include <QString>

InstitutionRankingModel::InstitutionRankingModel(QObject* parent):
    QAbstractTableModel(parent)
{
}

int InstitutionRankingModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return entries_.size();
}

int InstitutionRankingModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return 6;
}

QVariant InstitutionRankingModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() ||
        role != Qt::DisplayRole ||
        index.row() < 0 ||
        index.row() >= entries_.size())
    {
        return {};
    }

    const InstitutionBoardEntry& entry = entries_.at(index.row());
    switch (index.column())
    {
    case 0:
        return entry.rank;
    case 1:
        return entry.name;
    case 2:
        return entry.commonEntry;
    case 3:
        return entry.coreStrength;
    case 4:
        return entry.targetAudience;
    case 5:
        return QString::number(entry.score, 'f', 1);
    default:
        return {};
    }
}

QVariant InstitutionRankingModel::headerData(int section, Qt::Orientation orientation, int role) const
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
        return QStringLiteral(u"\u673a\u6784");
    case 2:
        return QStringLiteral(u"\u5e38\u7528\u5165\u53e3");
    case 3:
        return QStringLiteral(u"\u6838\u5fc3\u4f18\u52bf");
    case 4:
        return QStringLiteral(u"\u9002\u5408\u4eba\u7fa4");
    case 5:
        return QStringLiteral(u"\u7efc\u5408\u5206");
    default:
        return {};
    }
}

void InstitutionRankingModel::setEntries(QVector<InstitutionBoardEntry> entries)
{
    beginResetModel();
    entries_ = std::move(entries);
    endResetModel();
}

const InstitutionBoardEntry* InstitutionRankingModel::entryAt(int row) const
{
    if (row < 0 || row >= entries_.size())
    {
        return nullptr;
    }

    return &entries_.at(row);
}
