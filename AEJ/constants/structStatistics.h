#pragma once

#include <QtCore>
#include <QAtomicInt>
#include "../utilitySafeIncrement.h"

 // Performance statistics
struct Statistics
{
    QAtomicInt hits = 0;
    QAtomicInt misses = 0;
    QAtomicInt invalidations = 0;
    QAtomicInt prefetches = 0;
    QAtomicInt replacements = 0;
    QAtomicInt coherencyEvents = 0;

    void incHit() { asa_utils::safeIncrement(hits); }
    void incMisses() { asa_utils::safeIncrement(misses); }
    void incInvalidations() { asa_utils::safeIncrement(invalidations); }
    void incPrefetches() { asa_utils::safeIncrement(prefetches); }
    void incReplacements() { asa_utils::safeIncrement(replacements); }
    void incCoherencyEvents() { asa_utils::safeIncrement(coherencyEvents); }
    double getHitRate() const
    {
        quint64 total = hits + misses;
        return total > 0 ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
    }
};