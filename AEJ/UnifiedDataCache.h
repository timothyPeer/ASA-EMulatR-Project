// UnifiedDataCache.h - High-performance unified data cache with Qt integration
#pragma once

#include "CacheSet.h"
#include "JITFaultInfoStructures.h"
#include "../AEJ/globalMacro.h"
#include "CacheSet.h"
#include "CacheLine.h"
#include <QByteArray>
#include <QDebug>
#include <QObject>
#include <QReadWriteLock>
#include <QSettings>
#include <QVector>
#include <QtGlobal>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

class TLBSystem;
class AlphaInstructionCache;
class CacheSet;
class CacheLine;

#include <cstdint>
#include <cstddef>

#if defined(_MSC_VER)
// MSVC: use _BitScanForward64 (needs <intrin.h>)
#include <intrin.h>
static inline unsigned int countTrailingZeros64(uint64_t x) noexcept
{
    // If x is guaranteed non‐zero, _BitScanForward64 writes the index into 'idx'
    // and returns a nonzero value. We assume lineSize is a power of two, so x!=0 here.
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<unsigned int>(idx);
}
#else
// GCC/Clang: use builtin __builtin_ctzll (counts trailing zero bits of unsigned long long)
static inline unsigned int countTrailingZeros64(uint64_t x) noexcept
{
    return static_cast<unsigned int>(__builtin_ctzll(x));
}
#endif
/**
 * @brief High-performance unified data cache with Qt integration
 *
 * Features:
 * - Lock-free read operations using CacheSet
 * - Configurable via QSettings
 * - SMP cache coherency support
 * - Qt signals for cache events
 * - Integration with TLB and instruction cache systems
 */
class alignas(64) UnifiedDataCache : public QObject
{
    Q_OBJECT

    Q_PROPERTY(quint32 prefetchDepth READ getPrefetchDepth WRITE setPrefetchDepth)
    Q_PROPERTY(quint32 prefetchDistance READ getPrefetchDistance WRITE setPrefetchDistance)

  public:
    using WriteBackFunction = std::function<bool(quint64, const void *, size_t)>;
    using ReadFunction = std::function<bool(quint64, void *, size_t)>;
    using TimePoint = std::chrono::steady_clock::time_point;

    // Cache configuration structure
    struct Config
    {
        size_t numSets;
        size_t associativity;
        size_t lineSize;
        size_t totalSize;
        bool enablePrefetch;
        bool enableStatistics;
        bool enableCoherency;
        quint16 statusUpdateInterval;
        QString coherencyProtocol; // "MESI", "MOESI", etc.

        Config()
            : numSets(64), associativity(4), lineSize(64), totalSize(16384), enablePrefetch(true),
              enableStatistics(true), enableCoherency(true), coherencyProtocol("MESI")
        {
        }

        // Load configuration from QSettings
        void loadFromSettings(QSettings *settings);
        void saveToSettings(QSettings *settings) const;

        size_t getIndexMask() const { return numSets - 1; }
        size_t getOffsetMask() const { return lineSize - 1; }

        size_t getTagShift() const { return static_cast<size_t>(std::log2(numSets * lineSize)); }

       // size_t getTagShift() const { return __builtin_ctzl(numSets * lineSize); }
        bool validate() const
        {
            // Ensure power-of-2 constraints
            if (!isPowerOf2(numSets) || !isPowerOf2(lineSize))
            {
                return false;
            }

            // Check reasonable bounds
            if (associativity == 0 || associativity > 32)
            {
                return false;
            }

            if (lineSize < 32 || lineSize > 1024)
            {
                return false;
            }

            if (numSets == 0 || numSets > 65536)
            {
                return false;
            }

            // Check total size consistency
            size_t calculatedSize = numSets * associativity * lineSize;
            if (calculatedSize != totalSize)
            {
                return false;
            }

            return true;
        }
        private:
        static bool isPowerOf2(size_t value) { return value > 0 && (value & (value - 1)) == 0; }
    };

    // Enhanced statistics with atomic counters
    struct alignas(64) Statistics
    {
        std::atomic<uint64_t> hits{0};
        std::atomic<uint64_t> misses{0};
        std::atomic<uint64_t> evictions{0};
        std::atomic<uint64_t> invalidations{0};
        std::atomic<uint64_t> writebacks{0};
        std::atomic<uint64_t> snoopHits{0};
        std::atomic<uint64_t> prefetchHits{0};
        std::atomic<uint64_t> coherencyMisses{0};

        // Performance metrics
        std::atomic<uint64_t> totalAccessTime{0};
        std::atomic<uint64_t> accessCount{0};

        // Make sure the array is never zero-sized on any platform:
        static constexpr size_t atomicBlockSize = 8 * sizeof(std::atomic<uint64_t>); // usually 8*8 = 64
        static constexpr size_t padSize = (64 > atomicBlockSize ? 64 - atomicBlockSize : 1);
        char padding[padSize];
        double getHitRate() const
        {
            uint64_t total = hits.load(std::memory_order_relaxed) + misses.load(std::memory_order_relaxed);
            return total > 0 ? (static_cast<double>(hits.load(std::memory_order_relaxed)) / total) * 100.0 : 0.0;
        }

        double getAverageAccessTime() const
        {
            uint64_t count = accessCount.load(std::memory_order_relaxed);
            return count > 0 ? static_cast<double>(totalAccessTime.load(std::memory_order_relaxed)) / count : 0.0;
        }
    };


    struct StatisticsSnapshot
    {
        quint64 hits;
        quint64 misses;
        quint64 evictions;
        quint64 invalidations;
        quint64 writebacks;
        quint64 snoopHits;
        quint64 prefetchHits;
        quint64 coherencyMisses;
        quint64 totalAccessTime;
        quint64 accessCount;

    };
    // Cache coherency states for MESI protocol
    enum class CoherencyState : uint8_t
    {
        Invalid = 0,
        Shared = 1,
        Exclusive = 2,
        Modified = 3
    };

  private:
    // Cache sets storage - aligned for performance
    alignas(64) std::vector<std::unique_ptr<CacheSet> > m_cacheSets;

    // Configuration
    const Config m_config;

    // Statistics
    mutable Statistics m_stats;

    // Cache hierarchy
    UnifiedDataCache *m_nextLevel{nullptr};
    UnifiedDataCache *m_prevLevel{nullptr};

    // Integration systems
    class TLBSystem *m_tlbSystem{nullptr};
    class AlphaInstructionCache *m_instructionCache{nullptr};
    uint16_t m_cpuId{0};

    // Thread safety for write operations
    mutable QReadWriteLock m_hierarchyLock;

    ReadFunction m_backingRead{nullptr};

    // Performance tracking
    std::atomic<uint64_t> m_globalTime{0};

  public:

    quint32 getPrefetchDepth() const { return 2; } // Default value, should be configurable
    void setPrefetchDepth(quint32 depth) { /* Implementation needed */ }

    quint32 getPrefetchDistance() const { return 128; } // Default value, should be configurable
    void setPrefetchDistance(quint32 distance) { /* Implementation needed */ }
    /**
     * @brief Construct high-performance unified data cache
     * @param config Cache configuration
     * @param parent Qt parent object
     */
    explicit UnifiedDataCache(const Config &config = Config{}, QObject *parent = nullptr);

    /**
     * @brief Construct from QSettings
     * @param settings Qt settings object
     * @param parent Qt parent object
     */
    explicit UnifiedDataCache(QSettings *settings, QObject *parent = nullptr);

    // Move constructor and assignment
    UnifiedDataCache(UnifiedDataCache &&other) noexcept;
    UnifiedDataCache &operator=(UnifiedDataCache &&other) = delete;

    // No copy constructor/assignment
    UnifiedDataCache(const UnifiedDataCache &) = delete;
    UnifiedDataCache &operator=(const UnifiedDataCache &) = delete;

     // Cache set access
    CacheSet *getCacheSet(quint64 addr);
//     {
//         return m_cacheSets[getIndex(addr)].get();
//     }
    //     const CacheSet *getCacheSet(quint64 addr) const { return m_cacheSets[getIndex(addr)].get(); }

    UnifiedDataCache *getLevel1DataCache() const;

    /**
     * @brief High-performance read operation
     * @param addr Physical address to read from
     * @param buf Output buffer
     * @param size Number of bytes to read
     * @param backingRead Function to read from backing store on miss
     * @return True if read successful
     */
    bool read(quint64 addr, void *buf, size_t size, ReadFunction backingRead = {});

    /**
     * @brief High-performance write operation
     * @param addr Physical address to write to
     * @param data Input data buffer
     * @param size Number of bytes to write
     * @param backingWrite Function to write to backing store
     * @return True if write successful
     */
    bool write(quint64 addr, const void *data, size_t size, WriteBackFunction backingWrite = {});

    /**
     * @brief Lock-free cache line lookup
     * @param addr Physical address
     * @return Pointer to cache line if found, nullptr otherwise
     */
    CacheLine *findLine(quint64 addr) noexcept;

    /**
     * @brief Invalidate specific cache line without write-back
     * @param physicalAddr Physical address to invalidate
     */
    void invalidateLine(quint64 physicalAddr);
    void invalidate(quint64 physicalAddr)
    {
        QWriteLocker locker(&m_hierarchyLock);

        quint64 alignedAddr = getAlignedAddress(physicalAddr);
        CacheSet *cacheSet = getCacheSet(physicalAddr);

        if (cacheSet->invalidateLine(alignedAddr))
        {
            m_stats.invalidations.fetch_add(1, std::memory_order_relaxed);
            emit sigLineInvalidated(alignedAddr);
            DEBUG_LOG(QString("Cache :: Invalidated line: addr=0x%llx").arg(physicalAddr));
        }

        if (m_nextLevel)
        {
            m_nextLevel->invalidate(physicalAddr);
        }
    }

    /**
     * @brief Flush cache line (write-back if dirty, then invalidate)
     * @param physicalAddr Physical address to flush
     */
    void flushLine(quint64 physicalAddr);

    /**
     * @brief Check if cache line is dirty
     * @param physicalAddr Physical address to check
     * @return True if line is dirty
     */
    bool isDirty(quint64 physicalAddr) const;

    /**
     * @brief Mark cache line as clean
     * @param physicalAddr Physical address to mark clean
     */
    void markClean(quint64 physicalAddr);

    /**
     * @brief Handle cache snooping for SMP coherency
     * @param physicalAddr Physical address being snooped
     * @param operation Coherency operation type
     */
    void snoop(quint64 physicalAddr, const QString &operation);

    /**
     * @brief Write back specific dirty cache line
     * @param physicalAddr Physical address to write back
     * @param backingWrite Write-back function
     * @return True if successful
     */
    bool writeBackLine(quint64 physicalAddr, WriteBackFunction backingWrite = {});

    /**
     * @brief Write back all dirty cache lines
     * @param backingWrite Write-back function
     * @return True if all write-backs succeeded
     */
    bool writeBackAllDirty(WriteBackFunction backingWrite);

    /**
     * @brief Check if cache contains valid line for address
     * @param addr Physical address
     * @return True if line exists and is valid
     */
    bool contains(quint64 addr) const;

    /**
     * @brief Remove cache line with optional write-back
     * @param addr Physical address
     * @param backingWrite Optional write-back function
     * @return True if line was removed
     */
    bool remove(quint64 addr, WriteBackFunction backingWrite = {});

    /**
     * @brief Invalidate all cache lines
     */
    void invalidateAll();

    /**
     * @brief Flush entire cache
     */
    void flush();

    void setBackingRead(ReadFunction backingRead) { m_backingRead = backingRead; }

    // Cache hierarchy management
    void setNextLevel(UnifiedDataCache *nextLevel);
    void setPrevLevel(UnifiedDataCache *prevLevel);
    UnifiedDataCache *getNextLevel() const { return m_nextLevel; }
    UnifiedDataCache *getPrevLevel() const { return m_prevLevel; }

    // Integration system setup
    void setTLBSystem(TLBSystem *tlb, uint16_t cpuId);
    void setInstructionCache(AlphaInstructionCache *icache);

    // Statistics and monitoring
    StatisticsSnapshot getStatistics() const;
    void clearStatistics();
    double getUtilization() const;

    // Configuration access
    const Config &getConfig() const { return m_config; }
    size_t getNumSets() const { return m_config.numSets; }
    size_t getAssociativity() const { return m_config.associativity; }
    size_t getLineSize() const { return m_config.lineSize; }
    size_t getTotalSize() const { return m_config.totalSize; }
    size_t getStatsUpdateInterval() const {return m_config.statusUpdateInterval; }

    // Qt property access for QML integration
    Q_PROPERTY(double hitRate READ getHitRate NOTIFY sigStatsChanged)
    Q_PROPERTY(double utilization READ getUtilization NOTIFY sigStatsChanged)
    Q_PROPERTY(quint64 totalHits READ getTotalHits NOTIFY sigStatsChanged)
    Q_PROPERTY(quint64 totalMisses READ getTotalMisses NOTIFY sigStatsChanged)

    double getHitRate() const { return m_stats.getHitRate(); }
    quint64 getTotalHits() const { return m_stats.hits.load(std::memory_order_relaxed); }
    quint64 getTotalMisses() const { return m_stats.misses.load(std::memory_order_relaxed); }

  signals:
    /**
     * @brief Emitted when cache statistics change significantly
     */
    void sigStatsChanged();

    /**
     * @brief Emitted when cache line is evicted
     * @param address Physical address of evicted line
     * @param wasDirty Whether the evicted line was dirty
     */
    void sigLineEvicted(quint64 address, bool wasDirty);

    /**
     * @brief Emitted when cache line is invalidated
     * @param address Physical address of invalidated line
     */
    void sigLineInvalidated(quint64 address);

    /**
     * @brief Emitted when write-back occurs
     * @param address Physical address of written-back line
     * @param size Size of data written back
     */
    void sigWriteBack(quint64 address, size_t size);

    /**
     * @brief Emitted when coherency violation is detected
     * @param address Physical address of violation
     * @param operation Operation that caused violation
     */
    void sigCoherencyViolation(quint64 address, const QString &operation);

    /**
     * @brief Emitted when cache hierarchy changes
     * @param nextLevel New next level cache
     */
    void sigHierarchyChanged(UnifiedDataCache *nextLevel);

  private slots:
    /**
     * @brief Handle periodic statistics updates
     */
    void onStatsUpdateTimer();

  private:
    // Address decomposition helpers
    size_t getIndex(quint64 addr) const
    {
        size_t shift = static_cast<size_t>(std::log2(m_config.lineSize));
        return (addr >> shift) & m_config.getIndexMask();
    }

    quint64 getTag(quint64 addr) const { return addr >> m_config.getTagShift(); }

    size_t getOffset(quint64 addr) const { return addr & m_config.getOffsetMask(); }

    quint64 getAlignedAddress(quint64 addr) const { return addr & ~m_config.getOffsetMask(); }

   



	

    // Helper methods
    bool tryNextLevelRead(quint64 alignedAddr, void *buffer, size_t size);
    bool performWriteBack(CacheLine *line, quint64 address, WriteBackFunction backingWrite);
    void updateAccessStatistics(bool hit, TimePoint startTime);
    void notifyCoherencyViolation(quint64 address, const QString &operation);

    // Statistics update timer
    QTimer *m_statsTimer{nullptr};
    //static constexpr int STATS_UPDATE_INTERVAL_MS = 1000;
};