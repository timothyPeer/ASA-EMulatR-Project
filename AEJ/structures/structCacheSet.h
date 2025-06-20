#pragma once
#include <QtCore>
#include "structCacheLine.h"

// Cache set (multiple lines per set for associativity)
struct CacheSet
{
    QVector<CacheLine> lines;
    mutable QMutex mutex; // Per-set locking for performance

    CacheSet(size_t associativity, size_t lineSize) 
        : lines(associativity)
        , m_lineSize(lineSize) {
    
    }
};