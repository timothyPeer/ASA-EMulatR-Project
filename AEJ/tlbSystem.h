#pragma once
#include "AlphaTranslationCache.h"
#include "TLBEntry.h"
#include "UnifiedDataCache.h"
#include <QHash>
#include <QReadWriteLock>
#include <QVector>
#include <QMutex>

// Debug macros (define these in your main debug header)
#ifndef DEBUG_LOG
#define DEBUG_LOG(msg) qDebug() << msg
#endif
#ifndef ERROR_LOG
#define ERROR_LOG(msg) qCritical() << msg
#endif
#ifndef WARN_LOG
#define WARN_LOG(msg) qWarning() << msg
#endif

class UnifiedDataCache;
/**
 * @class TLBSystem
 * @brief Per-CPU Translation Lookaside Buffer management for Alpha AXP SMP systems
 *
 * Manages separate TLB instances for each CPU with proper SMP coordination.
 * Supports LRU replacement, cache coherency, and broadcast invalidation.
 *
 * Responsibilities:
 *  - Per-CPU VA -> PA translation caching with LRU eviction
 *  - SMP-aware TLB invalidation (single CPU, broadcast, by ASN)
 *  - Integration with translation cache and instruction cache
 *  - Performance monitoring and debugging support
 *
 * See ASA Manual, Vol II-A section 3.7 for TLB behavior and replacement guidance.
 */
class TLBSystem
{
  public:
    /**
     * @brief TLB statistics structure for monitoring performance
     */
    struct TLBStats
    {
        quint64 entries;      ///< Total TLB entries
        quint64 validEntries; ///< Currently valid entries
        quint64 ageCounter;   ///< Current age counter value
        quint64 hits;         ///< TLB hit count
        quint64 misses;       ///< TLB miss count

        TLBStats() : entries(0), validEntries(0), ageCounter(0), hits(0), misses(0) {}
    };

  private:
    /**
     * @brief Per-CPU TLB data structure
     */
    struct PerCPUTLBData
    {
        QVector<TLBEntry> entries; ///< TLB entries for this CPU
        QVector<quint64> lastUsed; ///< LRU timestamps per entry
        quint64 ageCounter;        ///< Monotonic counter for LRU
        quint64 hits;              ///< Hit counter for statistics
        quint64 misses;            ///< Miss counter for statistics



        QHash<quint16, PerCPUTLBData> m_perCpuTLBs; ///< Per-CPU TLB storage
        PerCPUTLBData(int capacity) : entries(capacity), lastUsed(capacity, 0), ageCounter(0), hits(0), misses(0) {}
    };

    // Private data members
    QHash<quint16, PerCPUTLBData> m_cpuTLBMap; ///< Per-CPU TLB storage
    QHash<quint16, quint64> m_cpuASNs;
    QAtomicInt m_contextSwitches{0};
    int m_tlbCapacity;                          ///< TLB capacity per CPU
    quint16 m_maxCpus;                          ///< Maximum supported CPUs
    mutable QReadWriteLock m_cpuTLBLock;        ///< Protect per-CPU TLB access
    QHash<quint16, PerCPUTLBData> m_instructionTLBs;
    QHash<quint16, PerCPUTLBData> m_dataTLBs;
    // Cache integration
    AlphaTranslationCache *m_translationCache; ///< Optional translation cache
    UnifiedDataCache *m_instructionCache;      ///< For invalidating cached instructions

    signals:
    void sigCPUContextUpdated(quint16 cpuId, quint64 newASN);

  public:
    /**
     * @brief Create a TLBSystem supporting multiple CPUs
     * @param capacity TLB entry count per CPU
     * @param maxCpus Maximum number of CPUs supported (default 16)
     */
    explicit TLBSystem(int capacity = 64, quint16 maxCpus = 16);

    /**
     * @brief Destructor - cleans up all per-CPU TLB data
     */
    ~TLBSystem();

    // =======================
    // CPU MANAGEMENT
    // =======================

    /**
     * @brief Register a new CPU and allocate its TLB
     * @param cpuId CPU identifier (0-based)
     * @return True if registration successful
     */
    bool registerCPU(quint16 cpuId);

    /**
     * @brief Unregister CPU and clean up its TLB
     * @param cpuId CPU identifier
     * @return True if deregistration successful
     */
    bool unregisterCPU(quint16 cpuId);

	void updateCPUContext(quint16 cpuId, quint64 newASN);
    /**
     * @brief Check if CPU is registered
     * @param cpuId CPU identifier
     * @return True if CPU has allocated TLB
     */
    bool isCPURegistered(quint16 cpuId) const;

    /**
     * @brief Get list of registered CPU IDs
     * @return Vector of registered CPU IDs
     */
    QVector<quint16> getRegisteredCPUs() const;

    // =======================
    // CACHE INTEGRATION
    // =======================

    /**
     * @brief Attach translation cache for coherency notifications
     * @param tc Translation cache instance
     */
    void attachTranslationCache(AlphaTranslationCache *tc) { m_translationCache = tc; }

    /**
     * @brief Attach instruction cache for invalidation notifications
     * @param icache Instruction cache instance
     */
    void attachInstructionCache(UnifiedDataCache *icache) { m_instructionCache = icache; }

    // =======================
    // CPU-AWARE CORE OPERATIONS
    // =======================

    /**
     * @brief Quick TLB check for specific CPU
     * @param cpuId CPU making the request
     * @param virtualAddress Virtual address to lookup
     * @param asn Address Space Number
     * @param isKernelMode True if CPU is in kernel mode
     * @return Physical address on hit, 0 on miss
     */
    quint64 checkTB(quint16 cpuId, quint64 virtualAddress, quint64 asn, bool isKernelMode);

    /**
     * @brief Full TLB lookup with permission checks for specific CPU
     * @param cpuId CPU making the request
     * @param virtualAddress Virtual address to lookup
     * @param asn Address Space Number
     * @param isExec True if this is an instruction fetch
     * @param isWrite True if this is a write access
     * @return Pointer to TLB entry on hit, nullptr on miss
     */
    TLBEntry *findTLBEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn, bool isExec, bool isWrite);

	bool hasValidMapping(quint64 virtualAddress) const;
    bool hasValidMapping(quint64 virtualAddress, quint16 cpuid) const;
    /**
     * @brief Insert or replace TLB entry for specific CPU
     * @param cpuId CPU owning this entry
     * @param entry TLB entry to insert
     */
    void insertTLBEntry(quint16 cpuId, const TLBEntry &entry);

    // =======================
    // CPU-SPECIFIC INVALIDATION
    // =======================

    /**
     * @brief Invalidate TLB entry on specific CPU
     * @param cpuId Target CPU
     * @param virtualAddress Virtual address to invalidate
     * @param asn Address Space Number
     */
    void invalidateEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn);

    /**
     * @brief Invalidate data TLB entry on specific CPU
     * @param cpuId Target CPU
     * @param virtualAddress Virtual address to invalidate
     * @param asn Address Space Number
     */
    void invalidateDataEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn);

    /**
     * @brief Invalidate instruction TLB entry on specific CPU
     * @param cpuId Target CPU
     * @param virtualAddress Virtual address to invalidate
     * @param asn Address Space Number
     */
    void invalidateInstructionEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn);

    /**
     * @brief Invalidate all TLB entries for ASN on specific CPU
     * @param cpuId Target CPU
     * @param asn Address Space Number to invalidate
     */
    void invalidateByASN(quint16 cpuId, quint64 asn);

    /**
     * @brief Invalidate all TLB entries on specific CPU
     * @param cpuId Target CPU
     */
    void invalidateAll(quint16 cpuId);

    // =======================
    // SMP BROADCAST INVALIDATION
    // =======================

    /**
     * @brief Invalidate TLB entry across all registered CPUs
     * @param virtualAddress Virtual address to invalidate
     * @param asn Address Space Number
     * @param excludeCpuId CPU to exclude from invalidation (0xFFFF = none)
     */
    void invalidateEntryAllCPUs(quint64 virtualAddress, quint64 asn, quint16 excludeCpuId = 0xFFFF);

    /**
     * @brief Invalidate by ASN across all registered CPUs
     * @param asn Address Space Number to invalidate
     * @param excludeCpuId CPU to exclude from invalidation (0xFFFF = none)
     */
    void invalidateByASNAllCPUs(quint64 asn, quint16 excludeCpuId = 0xFFFF);

    /**
     * @brief Global TLB flush across all registered CPUs
     * @param excludeCpuId CPU to exclude from invalidation (0xFFFF = none)
     */
    void invalidateAllCPUs(quint16 excludeCpuId = 0xFFFF);

    /**
     * @brief Synonym for invalidateAllCPUs(), matching PAL TBIA semantics
     * @param excludeCpuId CPU to exclude from invalidation (0xFFFF = none)
     */
    void invalidateTLB(quint16 excludeCpuId = 0xFFFF) { invalidateAllCPUs(excludeCpuId); }

    // =======================
    // TRANSLATION CACHE COORDINATION
    // =======================

    /**
     * @brief Invalidate translation-cache entries for ASN only
     * @param asn Address Space Number
     */
    void invalidateTranslationCacheASN(quint16 cpuId, quint64 asn);

    /**
     * @brief Invalidate the entire translation cache
     */
    void invalidateTranslationCacheAll();

    // =======================
    // STATISTICS AND DEBUGGING
    // =======================

    /**
     * @brief Get TLB statistics for specific CPU
     * @param cpuId CPU identifier
     * @return TLB statistics structure
     */
    TLBStats getTLBStats(quint16 cpuId) const;

    /**
     * @brief Get TLB statistics for all registered CPUs
     * @return Hash map of CPU ID to statistics
     */
    QHash<quint16, TLBStats> getAllTLBStats() const;

    /**
     * @brief Dump TLB state for debugging
     * @param cpuId CPU to dump (0xFFFF = all registered CPUs)
     */
    void dumpTLBState(quint16 cpuId = 0xFFFF) const;

  private:
    // =======================
    // PRIVATE HELPER METHODS
    // =======================

    /**
     * @brief Get CPU TLB data (assumes lock held)
     * @param cpuId CPU identifier
     * @return Pointer to TLB data or nullptr if not registered
     */
    PerCPUTLBData *getCPUTLBData(quint16 cpuId);
    const PerCPUTLBData *getCPUTLBData(quint16 cpuId) const;

    /**
     * @brief Ensure CPU is registered, create TLB if needed
     * @param cpuId CPU identifier
     * @return True if CPU is ready for TLB operations
     */
    bool ensureCPURegistered(quint16 cpuId);

    /**
     * @brief Dump single CPU TLB state (helper for dumpTLBState)
     * @param cpuId CPU to dump
     */
    void dumpSingleCPUTLB(quint16 cpuId) const;
};