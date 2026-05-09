#include "WealthProductModel.h"

WealthProductModel::WealthProductModel(QObject* parent):
    QAbstractTableModel(parent),
    rows_{
        {
            1,
            QStringLiteral(u"\u8682\u8681\u8d22\u5bcc"),
            QStringLiteral(u"\u652f\u4ed8\u5b9d / \u4f59\u989d\u5b9d"),
            QStringLiteral(u"\u8d27\u5e01\u57fa\u91d1 + \u7a33\u5065\u7406\u8d22"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+0 / T+1"),
            QStringLiteral(u"\u96f6\u552e\u7528\u6237\u8986\u76d6\u9762\u5f88\u5e7f")
        },
        {
            2,
            QStringLiteral(u"\u5fae\u4fe1\u7406\u8d22\u901a"),
            QStringLiteral(u"\u5fae\u4fe1\u670d\u52a1 / \u96f6\u94b1\u901a"),
            QStringLiteral(u"\u73b0\u91d1\u7ba1\u7406 + \u7a33\u5065\u7cbe\u9009"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+0 / T+1"),
            QStringLiteral(u"\u4f9d\u6258\u5fae\u4fe1\u9ad8\u9891\u6d41\u91cf")
        },
        {
            3,
            QStringLiteral(u"\u62db\u5546\u94f6\u884c"),
            QStringLiteral(u"\u62db\u884c App / \u671d\u671d\u5b9d"),
            QStringLiteral(u"\u73b0\u91d1\u7ba1\u7406 + \u56fa\u6536\u589e\u5f3a"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"\u9ad8"),
            QStringLiteral(u"\u94f6\u884c\u7406\u8d22\u6d3b\u8dc3\u7528\u6237\u591a")
        },
        {
            4,
            QStringLiteral(u"\u5929\u5929\u57fa\u91d1"),
            QStringLiteral(u"\u4e1c\u65b9\u8d22\u5bcc / \u5929\u5929\u57fa\u91d1"),
            QStringLiteral(u"\u57fa\u91d1\u7533\u8d2d + \u7ec4\u5408\u6295\u987e"),
            QStringLiteral(u"\u4e2d\u5230\u9ad8"),
            QStringLiteral(u"T+1 \u53ca\u4ee5\u4e0a"),
            QStringLiteral(u"\u57fa\u91d1\u8986\u76d6\u8303\u56f4\u5e7f")
        },
        {
            5,
            QStringLiteral(u"\u4eac\u4e1c\u91d1\u878d"),
            QStringLiteral(u"\u4eac\u4e1c\u91d1\u878d / \u5c0f\u91d1\u5e93"),
            QStringLiteral(u"\u8d27\u5e01\u57fa\u91d1 + \u94f6\u884c\u7cbe\u9009"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+0 / T+1"),
            QStringLiteral(u"\u7535\u5546\u5165\u53e3\u5bfc\u6d41\u5f3a")
        },
        {
            6,
            QStringLiteral(u"\u5ea6\u5c0f\u6ee1\u7406\u8d22"),
            QStringLiteral(u"\u5ea6\u5c0f\u6ee1 App"),
            QStringLiteral(u"\u6d3b\u671f\u73b0\u91d1 + \u94f6\u884c\u7406\u8d22"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+1"),
            QStringLiteral(u"\u4e92\u8054\u7f51\u7406\u8d22\u5165\u53e3\u7a33\u5b9a")
        },
        {
            7,
            QStringLiteral(u"\u5de5\u5546\u94f6\u884c"),
            QStringLiteral(u"\u5de5\u94f6\u7406\u8d22 / \u624b\u673a\u94f6\u884c"),
            QStringLiteral(u"\u94f6\u884c\u7406\u8d22 + \u5b58\u6b3e\u7c7b"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+1 \u5230\u5c01\u95ed\u671f"),
            QStringLiteral(u"\u56fd\u6709\u5927\u884c\u8986\u76d6\u5e7f")
        },
        {
            8,
            QStringLiteral(u"\u5efa\u8bbe\u94f6\u884c"),
            QStringLiteral(u"\u5efa\u884c\u7406\u8d22 / \u624b\u673a\u94f6\u884c"),
            QStringLiteral(u"\u7a33\u5065\u7406\u8d22 + \u56fa\u6536\u4ea7\u54c1"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+1 \u5230\u5c01\u95ed\u671f"),
            QStringLiteral(u"\u7ebf\u4e0a\u7ebf\u4e0b\u534f\u540c\u5f3a")
        },
        {
            9,
            QStringLiteral(u"\u4e2d\u56fd\u94f6\u884c"),
            QStringLiteral(u"\u4e2d\u94f6\u7406\u8d22 / \u4e2d\u884c App"),
            QStringLiteral(u"\u94f6\u884c\u7406\u8d22 + \u5916\u6c47\u7c7b"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+1 \u5230\u5c01\u95ed\u671f"),
            QStringLiteral(u"\u8de8\u5e01\u79cd\u9009\u62e9\u66f4\u591a")
        },
        {
            10,
            QStringLiteral(u"\u5e73\u5b89\u94f6\u884c"),
            QStringLiteral(u"\u53e3\u888b\u94f6\u884c / \u5e73\u5b89\u7406\u8d22"),
            QStringLiteral(u"\u73b0\u91d1\u7ba1\u7406 + \u7a33\u5065\u7cbe\u9009"),
            QStringLiteral(u"\u4f4e\u5230\u4e2d"),
            QStringLiteral(u"T+0 / T+1"),
            QStringLiteral(u"\u7efc\u5408\u91d1\u878d\u751f\u6001\u5b8c\u6574")
        }
    }
{
}

int WealthProductModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return static_cast<int>(rows_.size());
}

int WealthProductModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return 7;
}

QVariant WealthProductModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size()) ||
        index.column() < 0 ||
        index.column() >= columnCount())
    {
        return {};
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    const ProductRow& row = rows_[static_cast<std::size_t>(index.row())];
    switch (index.column())
    {
    case 0:
        return row.rank;
    case 1:
        return row.institution;
    case 2:
        return row.entry;
    case 3:
        return row.category;
    case 4:
        return row.riskLevel;
    case 5:
        return row.liquidity;
    case 6:
        return row.notes;
    default:
        return {};
    }
}

QVariant WealthProductModel::headerData(int section, Qt::Orientation orientation, int role) const
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
        return QStringLiteral(u"\u5e73\u53f0 / \u673a\u6784");
    case 2:
        return QStringLiteral(u"\u5e38\u7528\u5165\u53e3");
    case 3:
        return QStringLiteral(u"\u4e3b\u6253\u54c1\u7c7b");
    case 4:
        return QStringLiteral(u"\u98ce\u9669\u7b49\u7ea7");
    case 5:
        return QStringLiteral(u"\u6d41\u52a8\u6027");
    case 6:
        return QStringLiteral(u"\u5907\u6ce8");
    default:
        return {};
    }
}

QHash<int, QByteArray> WealthProductModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"}
    };
}
