#pragma once

#include "CacheSet.h"
#include "InstructionWord.h"
#include "constants/structStatistics.h"
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QVector>

// Forward declarations
class AlphaMemorySystem;
class TLBSystem;
class ConfigLoader;

/**
 * @brief High-performance Alpha Instruction Cache with SMP support
 *
 * Features:
 * - Set-associative cache with configurable associativity
 * - Cache line invalidation for SMP coherency
 * - Performance statistics and monitoring
 * - Prefetch support for sequential instruction streams
 * - Cache warming for frequently accessed code regions
 * - Thread-safe operations for multi-CPU systems
 * - Integration with AlphaMemorySystem for unified memory management
 */
#pragma once

#include "CacheSet.h"
#include "InstructionWord.h"
#include "UnifiedDataCache.h" // Use UnifiedDataCache instead of raw CacheSet
#include "constants/structStatistics.h"
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QVector>
#include "../AEJ/structures/structCacheConfig.h"

// Forward declarations
class AlphaMemorySystem;
class TLBSystem;
class ConfigLoader;

class AlphaInstructionCache : public QObject
{
    Q_OBJECT

  public:
    AlphaInstructionCache(QObject *parent, AlphaMemorySystem *memorySystem, const CacheConfig &config, uint16_t cpuId);
    ~AlphaInstructionCache();





    // Core cache operations
    bool read(quint64 address, void *buffer, size_t size);
    bool fetch(quint64 address, InstructionWord &instruction);
    bool fetchLine(quint64 address, QVector<InstructionWord> &instructions);

    // Cache invalidation operations
    void invalidateAll();
    void invalidateLine(quint64 address);
    bool invalidateLine(quint64 address, bool returnSuccess);
    void invalidateRange(quint64 startAddress, quint64 endAddress);
    void invalidateByTag(quint64 tag);

	void flush();
    void flushLine(quint64 address);
    // Integration with high-performance cache system
    UnifiedDataCache *getUnifiedCache() { return m_instructionCache.get(); }
    const UnifiedDataCache *getUnifiedCache() const { return m_instructionCache.get(); }

    // Cache management
    void setReplacementPolicy(const QString &policy);
    void resize(size_t newSize);
    void clear();

    // Cache management helpers
    void debugCacheAccess(quint64 address, bool hit, const QString &operation);
    void prefetch(quint64 address);
    void prefetchSequential(quint64 startAddress, size_t lineCount);
    void prefetchNextLine(quint64 address);
    void checkAutoPrefetch(quint64 address);
 
    // Statistics and monitoring
    Statistics getStatistics() const;
    void clearStatistics();
    void recordPrefetch();
    double getHitRate() const;

    // Configuration queries
    size_t getCacheSize() const { return m_cacheSize; }
    size_t getLineSize() const { return m_lineSize; }


    //void setUnifiedCache(UnifiedDataCache *cache) { m_unifiedCache = cache; }
    bool write(quint64 address, const void *buffer, size_t size);
  
    void handleSelfModifyingCode(quint64 address);

  signals:
    void sigCacheHit(quint64 address);
    void sigCacheMiss(quint64 address);
    void sigCoherencyEventHandled(quint64 address, const QString type);
    void sigLineInvalidated(quint64 address);
    void sigStatisticsUpdated();

  private:
    // Use UnifiedDataCache as the backing cache system
    

    // Cache configuration
    size_t m_cacheSize;
    size_t m_lineSize;
    size_t m_associativity;
    uint16_t m_cpuId;

    // Memory system integration
    AlphaMemorySystem *m_memorySystem;

     // Cache geometry calculations
    size_t m_numSets;
    size_t m_indexBits;
    size_t m_offsetBits;
    quint64 m_indexMask;
    quint64 m_offsetMask;


    QSharedPointer<UnifiedDataCache> m_instructionCache = QSharedPointer<UnifiedDataCache>().create();

     // Cache sets (for compatibility with legacy code)
    QVector<CacheSet> m_cacheSets;

    // Cache configuration
    QString m_replacementPolicy;

    // Hot spots tracking
    QHash<quint64, size_t> m_hotSpots;

    // Auto-prefetch state
    quint64 m_lastAccessAddress;
    int m_sequentialCount;

    // Statistics (delegated to UnifiedDataCache)
    mutable QMutex m_statsMutex;
    Statistics m_stats;

    // Helper methods
    quint64 getLineAddress(quint64 address) const;
    void recordHit();
    void recordMiss();
    void recordInvalidation();
    void flushDirtyLines();

       void recordReplacement();
    void recordCoherencyEvent();

    // Cache address calculation helpers
    quint64 getTag(quint64 address) const;
    size_t getSetIndex(quint64 address) const;
    size_t getOffset(quint64 address) const;


    // Performance and coherency
    void warmCache(quint64 startAddress, size_t size);
    void addHotSpot(quint64 address, size_t size);
    void removeHotSpot(quint64 address);
    void handleCoherencyEvent(quint64 address, const QString &type);
    void notifyOtherCaches(quint64 address, const QString &type);

    // Utility methods
    void printStatistics() const;
    size_t getUsedLines() const;
    size_t getTotalLines() const;

    bool loadLineFromMemory(quint64 address, CacheLine *line);
    bool loadFromMemorySystem(quint64 address, void *buffer, size_t size);
    void updateLocalStatistics();
    void setNextLevelCache(UnifiedDataCache *nextLevel);

    CacheLine *findLine(quint64 address, CacheSet &set);
    CacheLine *getReplacementLine(CacheSet &set);
    void updateAccessInfo(CacheLine &line);
    CacheLine *selectLRU(CacheSet &set);
    CacheLine *selectLFU(CacheSet &set);
    CacheLine *selectRandom(CacheSet &set);
};