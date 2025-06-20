// ========== FINAL CLEANED HEADER (AlphaTranslationCache.h) ==========

#pragma once

#include "constants/constTBLPipeline.h"
#include <QAtomicInt>
#include <QFuture>
#include <QHash>
#include <QReadWriteLock>
#include <QThread>
#include <QVector>
#include <QtConcurrent/QtConcurrent>

/**
 * @brief Translation Cache Entry for Alpha address translation
 *
 * Stores the virtual-to-physical mapping along with associated metadata
 * such as ASN, protection bits, and validity information.
 */
struct TranslationCacheEntry
{
    quint64 virtualAddress;  // Virtual page address (page-aligned)
    quint64 physicalAddress; // Physical page address
    quint64 asn;             // Address Space Number
    quint16 protectionBits;  // Read/Write/Execute permissions
    bool isValid;            // Entry validity flag
    bool isKernel;           // Kernel vs User space
    bool isInstruction;      // Instruction vs Data access
    quint64 accessCount;     // Usage counter for LRU
    quint64 timestamp;       // Last access time

    TranslationCacheEntry()
        : virtualAddress(0), physicalAddress(0), asn(0), protectionBits(0), isValid(false), isKernel(false),
          isInstruction(false), accessCount(0), timestamp(0)
    {
    }
};

/**
 * @brief High-performance translation cache for Alpha virtual memory
 *
 * Per-CPU software representation of the Alpha Translation Buffer (ITB/DTB).
 * Implements look-up, insert, LRU/LFSR replacement with dynamic tuning
 * and async lookup support for pipeline optimization.
 *
 * Does NOT attempt cross-CPU coherency; that is handled by
 * AlphaCPU::invalidateTlb{All|Process|Single|Data|Instruction} via
 * inter-processor signalling through AlphaSMPManager.
 *
 * Architectural ref: Alpha AXP System Ref Man v6, section: 3.8 "Translation Buffer"
 */
class AlphaTranslationCache
{
  public:
    /**
     * @brief Batch processing structure for multiple TLB operations
     */
    struct TLBBatch
    {
        QVector<quint64> virtualAddresses;
        QVector<quint64> results;
        QPromise<QVector<quint64>> promise;
    };

    /**
     * @brief Performance and usage statistics
     */
    struct Statistics
    {
        quint64 lookups, hits, misses, insertions, evictions, invalidations;
        double hitRate() const { return lookups > 0 ? static_cast<double>(hits) / lookups : 0.0; }
    };

    /**
     * @brief Dynamic tuning statistics
     */
    struct TuningStats
    {
        quint64 lockContentions, expansions, reductions;
        bool partitioningActive;
        int currentPartitions;
        quint64 activeSets, activeWays;
    };

    // Constructor and destructor
    explicit AlphaTranslationCache(quint64 sets = MAX_SETS, quint64 ways = MAX_WAYS, quint64 pageSize = PAGE_SIZE);
    ~AlphaTranslationCache() = default;

    // Core TLB operations
    bool lookup(quint64 virtualAddress, quint64 asn, bool isKernel, bool isInstruction, quint64 &physicalAddress) const;

    void insert(quint64 virtualAddress, quint64 physicalAddress, quint64 asn, quint16 protectionBits, bool isKernel,
                bool isInstruction);

    // Asynchronous operations
    QFuture<bool> lookupAsync(quint64 virtualAddress, quint64 asn, bool isKernel, bool isInstruction) const;
    void processBatch(const TLBBatch &batch);

    // Cache invalidation
    void invalidateAll();
    void invalidateASN(quint64 asn);
    void invalidateAddress(quint64 virtualAddress, quint64 asn = 0);
    void invalidateInstructionEntries(bool isInstruction);

    // Statistics and monitoring
    Statistics getStatistics() const;
    void resetStatistics();

    // Dynamic tuning
    void autoTune();
    bool expandSets();
    bool expandWays();
    bool reduceWays();
    void enablePartitioning();
    void disablePartitioning();
    TuningStats getTuningStats() const;

    // Utility methods
    bool contains(quint64 virtualAddress, quint64 asn) const;
    quint64 getSets() const { return m_activeSets; }
    quint64 getWays() const { return m_activeWays; }
    quint64 getPageSize() const { return m_pageSize; }
    quint64 getTotalEntries() const { return m_activeSets * m_activeWays; }

  private:
    // Core cache geometry
    const quint64 m_sets, m_ways, m_pageSize;
    const quint64 m_pageMask, m_setShift;
    quint64 m_setMask; // Non-const for dynamic expansion

    // Dynamic sizing limits
    const quint64 m_maxSets, m_maxWays;
    quint64 m_activeSets, m_activeWays;

    // Cache storage
    QVector<QVector<TranslationCacheEntry>> m_cache; // [set][way]
    QVector<QVector<quint64>> m_lruCounters;         // [set][way]
    QVector<quint64> m_globalCounter;                // [set]

    // Partitioning support
    bool m_partitioningEnabled{false};
    int m_currentPartitions{1};
    static constexpr int MAX_PARTITIONS = 16;
    QVector<QVector<QVector<TranslationCacheEntry>>> m_partitionedCache;
    QVector<QVector<QVector<quint64>>> m_partitionedLRU;

    // Statistics (thread-safe)
    mutable QAtomicInt m_lookups{0}, m_hits{0}, m_misses{0};
    QAtomicInt m_insertions{0}, m_evictions{0}, m_invalidations{0};

    // Dynamic tuning state
    mutable QAtomicInt m_lockContentionCounter{0};
    mutable QAtomicInt m_autoTuneCounter{0};

    // Thread synchronization
    mutable QReadWriteLock m_lock;

    // Private helper methods
    quint64 getSetIndex(quint64 virtualAddress) const;
    quint64 getPageAddress(quint64 address) const;
    quint64 findLRUWay(quint64 set) const;
    void updateLRU(quint64 set, quint64 way);

    // Lock-free operations for async pipeline
    bool lookupLockFree(quint64 virtualAddress, quint64 asn, bool isKernel, bool isInstruction,
                        quint64 &physicalAddress) const;

    // Template helper for conditional invalidation
    template <typename Predicate> void invalidateIf(Predicate pred);
};

// ========== SIMPLE TEST TO VERIFY IMPLEMENTATION ==========
// Add this to a test file to verify your TLB works:

inline void testAlphaTranslationCache()
{
    qDebug() << "Testing AlphaTranslationCache...";

    AlphaTranslationCache tlb(512, 8, 8192);

    // Test basic insert/lookup
    tlb.insert(0x1000, 0x2000, 1, 0x7, false, false);

    quint64 physAddr = 0;
    bool found = tlb.lookup(0x1000, 1, false, false, physAddr);

    qDebug() << "Basic lookup:" << found << "Physical:" << Qt::hex << physAddr;
    Q_ASSERT(found && physAddr == 0x2000);

    // Test async lookup
    auto future = tlb.lookupAsync(0x1000, 1, false, false);
    bool asyncFound = future.result();
    qDebug() << "Async lookup:" << asyncFound;
    Q_ASSERT(asyncFound);

    // Test contains
    bool contains = tlb.contains(0x1000, 1);
    Q_ASSERT(contains);

    // Test statistics
    auto stats = tlb.getStatistics();
    qDebug() << "Hit rate:" << stats.hitRate() * 100.0 << "% Lookups:" << stats.lookups;

    // Test invalidation
    tlb.invalidateAddress(0x1000, 1);
    bool afterInvalidate = tlb.contains(0x1000, 1);
    Q_ASSERT(!afterInvalidate);

    // Test auto-tuning
    tlb.autoTune();
    auto tuningStats = tlb.getTuningStats();
    qDebug() << "Active sets:" << tuningStats.activeSets << "ways:" << tuningStats.activeWays;

    qDebug() << "All tests passed! TLB is working correctly.";
}