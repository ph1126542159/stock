#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct InstitutionBoardEntry
{
    QString id;
    int rank = 0;
    QString name;
    QString commonEntry;
    QString coreStrength;
    QString targetAudience;
    double score = 0.0;
};

struct ValueAssetEntry
{
    QString id;
    int rank = 0;
    QString name;
    QString code;
    QString provider;
    QString category;
    double score = 0.0;
    double latestPrice = 0.0;
    double oneYearReturnPct = 0.0;
    QString investmentAnalysis;
    QString sixMonthForecast;
    QVector<double> oneYearTrend;
    QVector<double> oneHourDrawdown;
};

struct ValueBoardSnapshot
{
    QString institutionId;
    QString institutionName;
    QVector<ValueAssetEntry> funds;
    QVector<ValueAssetEntry> stocks;
};

struct UsMarketWatchEntry
{
    QString symbol;
    QString name;
    QString market;
    double lastPrice = 0.0;
    double oneHourChangePct = 0.0;
    QVector<double> realtimeTrend;
    QDateTime updatedAt;
};
