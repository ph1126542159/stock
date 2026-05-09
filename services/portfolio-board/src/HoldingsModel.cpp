#include "HoldingsModel.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr bool kDataQualityGatePassed = false;
constexpr double kRiskGateMinimumScore = 60.0;

double clamp_score(double value)
{
    if (!std::isfinite(value))
    {
        return 0.0;
    }
    return std::clamp(value, 0.0, 100.0);
}

double volatility_pct(const QVector<double>& values)
{
    if (values.size() < 2)
    {
        return 0.0;
    }
    double sum = 0.0;
    for (const double value : values)
    {
        sum += value;
    }
    const double mean = sum / static_cast<double>(values.size());
    double variance = 0.0;
    for (const double value : values)
    {
        const double delta = value - mean;
        variance += delta * delta;
    }
    return std::sqrt(variance / static_cast<double>(values.size()));
}

double max_drawdown_pct(const QVector<double>& values)
{
    if (values.size() < 2)
    {
        return 0.0;
    }
    double peak = values.constFirst();
    double drawdown = 0.0;
    for (const double value : values)
    {
        peak = std::max(peak, value);
        drawdown = std::min(drawdown, value - peak);
    }
    return drawdown;
}

double technical_score(const HoldingEntry& entry)
{
    const double start = entry.oneHourTrend.isEmpty() ? 0.0 : entry.oneHourTrend.constFirst();
    const double end = entry.oneHourTrend.isEmpty() ? entry.oneHourChangePct : entry.oneHourTrend.constLast();
    return clamp_score(55.0 + entry.oneHourChangePct * 5.5 + (end - start) * 4.0 - volatility_pct(entry.oneHourTrend) * 3.0);
}

double fundamental_score(const HoldingEntry& entry)
{
    const double typeBonus = entry.type.contains(QStringLiteral(u"\u57fa\u91d1")) ||
            entry.type.contains(QStringLiteral("ETF"))
        ? 2.0
        : 0.0;
    return clamp_score(entry.aiScore + typeBonus);
}

double risk_score(const HoldingEntry& entry)
{
    const double concentrationPenalty = entry.symbol == QStringLiteral("NVDA") ? 8.0
        : (entry.symbol == QStringLiteral("AAPL") ? 4.0 : 0.0);
    return clamp_score(92.0 - std::abs(max_drawdown_pct(entry.oneHourTrend)) * 5.0 -
        volatility_pct(entry.oneHourTrend) * 6.0 - concentrationPenalty);
}

QString holding_action(const HoldingEntry& entry)
{
    const double technical = technical_score(entry);
    const double fundamental = fundamental_score(entry);
    const double risk = risk_score(entry);
    const double drawdown = max_drawdown_pct(entry.oneHourTrend);
    const bool gatePassed = kDataQualityGatePassed && risk >= kRiskGateMinimumScore;
    if (drawdown <= -8.0 || risk < 35.0)
    {
        return gatePassed ? QStringLiteral(u"\u6e05\u4ed3") : QStringLiteral(u"\u8f6c\u4ed3");
    }
    if (technical < 42.0 && risk < 55.0)
    {
        return gatePassed ? QStringLiteral(u"\u5356\u51fa") : QStringLiteral(u"\u8f6c\u4ed3");
    }
    if (fundamental < 58.0 && technical < 50.0)
    {
        return QStringLiteral(u"\u8f6c\u4ed3");
    }
    if (fundamental >= 76.0 && technical >= 62.0 && risk >= 60.0)
    {
        return gatePassed ? QStringLiteral(u"\u52a0\u4ed3") : QStringLiteral(u"\u8ddf\u8fdb");
    }
    return QStringLiteral(u"\u8ddf\u8fdb");
}

} // namespace

HoldingsModel::HoldingsModel(QObject* parent):
    QAbstractTableModel(parent)
{
}

int HoldingsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : entries_.size();
}

int HoldingsModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 9;
}

QVariant HoldingsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0 || index.row() >= entries_.size())
    {
        return {};
    }

    const HoldingEntry& entry = entries_.at(index.row());
    switch (index.column())
    {
    case 0:
        return entry.name;
    case 1:
        return entry.symbol;
    case 2:
        return entry.type;
    case 3:
        return entry.institution;
    case 4:
        return QString::number(entry.lastPrice, 'f', entry.lastPrice >= 1000.0 ? 2 : 3);
    case 5:
        return QStringLiteral("%1%2%")
            .arg(entry.oneHourChangePct >= 0.0 ? "+" : "")
            .arg(QString::number(entry.oneHourChangePct, 'f', 2));
    case 6:
        return QString::number(clamp_score(entry.aiScore), 'f', 1);
    case 7:
        return holding_action(entry);
    case 8:
        return entry.suggestion;
    default:
        return {};
    }
}

QVariant HoldingsModel::headerData(int section, Qt::Orientation orientation, int role) const
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
        return QStringLiteral(u"\u540d\u79f0");
    case 1:
        return QStringLiteral(u"\u4ee3\u7801");
    case 2:
        return QStringLiteral(u"\u7c7b\u578b");
    case 3:
        return QStringLiteral(u"\u673a\u6784");
    case 4:
        return QStringLiteral(u"\u6700\u65b0\u4ef7");
    case 5:
        return QStringLiteral(u"\u8fd11\u5c0f\u65f6");
    case 6:
        return QStringLiteral(u"AI \u5206");
    case 7:
        return QStringLiteral(u"\u52a8\u4f5c");
    case 8:
        return QStringLiteral(u"\u5efa\u8bae");
    default:
        return {};
    }
}

void HoldingsModel::setEntries(QVector<HoldingEntry> entries)
{
    const bool sameShape = entries_.size() == entries.size();
    bool canUpdateInPlace = sameShape;
    if (canUpdateInPlace)
    {
        for (int row = 0; row < entries_.size(); ++row)
        {
            if (entries_.at(row).id != entries.at(row).id)
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
            {Qt::DisplayRole});
    }
}

const HoldingEntry* HoldingsModel::entryAt(int row) const
{
    if (row < 0 || row >= entries_.size())
    {
        return nullptr;
    }

    return &entries_.at(row);
}
