#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct HoldingEntry
{
    QString id;
    QString symbol;
    QString name;
    QString type;
    QString institution;
    double lastPrice = 0.0;
    double oneHourChangePct = 0.0;
    double aiScore = 0.0;
    QString suggestion;
    QString analysis;
    QString industryOutlook;
    QVector<double> oneHourTrend;
    QVector<double> oneMonthIndustryTrend;
    QDateTime updatedAt;
};
