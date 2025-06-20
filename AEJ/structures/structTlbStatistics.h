#pragma once
#include <QObject>

struct TLBStatistics
{
    quint64 iTLBHits = 0;
    quint64 iTLBMisses = 0;
    quint64 dTLBHits = 0;
    quint64 dTLBMisses = 0;
    quint64 totalTranslations = 0;
    quint64 pageFaults = 0;
    quint64 protectionFaults = 0;
    quint64 invalidations = 0;

    double getITLBHitRate() const
    {
        quint64 total = iTLBHits + iTLBMisses;
        return total > 0 ? (static_cast<double>(iTLBHits) / total) * 100.0 : 0.0;
    }

    double getDTLBHitRate() const
    {
        quint64 total = dTLBHits + dTLBMisses;
        return total > 0 ? (static_cast<double>(dTLBHits) / total) * 100.0 : 0.0;
    }

    double getOverallHitRate() const
    {
        quint64 totalHits = iTLBHits + dTLBHits;
        quint64 totalMisses = iTLBMisses + dTLBMisses;
        quint64 total = totalHits + totalMisses;
        return total > 0 ? (static_cast<double>(totalHits) / total) * 100.0 : 0.0;
    }
};
