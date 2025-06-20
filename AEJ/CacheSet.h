// CacheSet.h
#pragma once
//#include "JITFaultInfoStructures.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <vector>
#include <functional>
#include <QtAlgorithms>  // for qCountTrailingZeroBits()
#include "cacheline.h"
#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"



/**
 * @brief High-performance cache set with lock-free reads and minimal locking writes
 *
 * Features:
 * - Lock-free read operations for cache hits
 * - Atomic operations for statistics
 * - Memory-aligned cache lines for performance
 * - RAII memory management
 * - Integration hooks for TLB and instruction cache
 */
class alignas(64) CacheSet
{
  public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using CacheLinePtr = std::unique_ptr<CacheLine>;

    // Statistics structure with atomic counters
    struct alignas(64) Statistics
    {
        std::atomic<uint64_t> hits{0};
        std::atomic<uint64_t> misses{0};
        std::atomic<uint64_t> evictions{0};
        std::atomic<uint64_t> invalidations{0};

        // Padding to prevent false sharing
        char padding[64 - 4 * sizeof(std::atomic<uint64_t>)];

        // Copy constructor (copies each atomic by loading it)
        Statistics(const Statistics &other) noexcept
            : hits(other.hits.load(std::memory_order_relaxed)), misses(other.misses.load(std::memory_order_relaxed)),
              evictions(other.evictions.load(std::memory_order_relaxed)),
              invalidations(other.invalidations.load(std::memory_order_relaxed))
        {
            // padding left uninitialized (it's just padding)
        }
        Statistics() noexcept : hits(0), misses(0), evictions(0), invalidations(0) {}
    };

    // Configuration structure
    struct Config
    {
        size_t associativity;
        size_t lineSize;
        size_t alignmentBits;
        bool enablePrefetch;
        bool enableStatistics;

        Config(size_t assoc = 4, size_t line_size = 64, bool prefetch = true, bool stats = true)
            : associativity(assoc)
            , lineSize(line_size)
            , alignmentBits(qCountTrailingZeroBits(static_cast<quint64>(line_size)))
            , enablePrefetch(prefetch)
            , enableStatistics(stats)
        {
        }
       

        public:
        quint16 getCacheLineSize() { return static_cast<quint16>(lineSize); }
          
    };

    
  private:
    // Cache lines storage - aligned for performance
    alignas(64) std::vector<CacheLinePtr> m_lines;

    // Configuration
    const Config m_config;

    // Statistics (atomic for thread safety)
    mutable Statistics m_stats;

    // LRU tracking with atomic operations
    alignas(64) std::vector<std::atomic<uint64_t>> m_accessTimes;
    std::atomic<uint64_t> m_globalTime{0};

    // Replacement policy state
    std::atomic<size_t> m_nextVictim{0}; // For round-robin fallback

    // Integration hooks
    class TLBSystem *m_tlbSystem{nullptr};
    class AlphaInstructionCache *m_instructionCache{nullptr};
    uint16_t m_cpuId{0};
   

  public:
    /**
     * @brief Construct a high-performance cache set
     * @param config Cache configuration
     */
    explicit CacheSet(const Config &config = Config{})
        : m_config(config), m_lines(config.associativity), m_accessTimes(config.associativity)
    {
        // Initialize cache lines
        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            m_lines[i] = std::make_unique<CacheLine>(m_config.lineSize);
            m_accessTimes[i].store(0, std::memory_order_relaxed);
        }

        static_assert(sizeof(CacheSet) % 64 == 0, "CacheSet should be cache-line aligned");
    }
    
     CacheSet(CacheSet &&other) noexcept
        : m_lines(std::move(other.m_lines)), m_config(other.m_config),
          m_stats(), // default-construct, then load from other
          m_accessTimes(std::move(other.m_accessTimes)), m_globalTime(other.m_globalTime.load()),
          m_nextVictim(other.m_nextVictim.load()), m_tlbSystem(other.m_tlbSystem),
          m_instructionCache(other.m_instructionCache), m_cpuId(other.m_cpuId)
    {
        // Copy atomic statistics into this->m_stats
        m_stats.hits.store(other.m_stats.hits.load(std::memory_order_relaxed));
        m_stats.misses.store(other.m_stats.misses.load(std::memory_order_relaxed));
        m_stats.evictions.store(other.m_stats.evictions.load(std::memory_order_relaxed));
        m_stats.invalidations.store(other.m_stats.invalidations.load(std::memory_order_relaxed));
    }

    // Move constructor and assignment
   
    // No copy constructor/assignment (expensive and unnecessary)
    CacheSet(const CacheSet &) = delete;
    CacheSet &operator=(const CacheSet &) = delete;
    CacheSet &operator=(CacheSet &&) = delete;

    /**
     * @brief Find cache line for given address (lock-free read)
     * @param address Physical address to look up
     * @param tag Cache tag for the address
     * @return Pointer to cache line if found, nullptr otherwise
     */
    CacheLine *findLine(uint64_t address, uint64_t tag) noexcept
    {
        const uint64_t currentTime = m_globalTime.fetch_add(1, std::memory_order_relaxed);

        // Lock-free search through cache lines
        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();

            // Check if line is valid and tags match
            if (line->isValid() && line->getTag() == tag)
            {
                // Update access time atomically
                m_accessTimes[i].store(currentTime, std::memory_order_relaxed);

                // Update statistics
                if (m_config.enableStatistics)
                {
                    m_stats.hits.fetch_add(1, std::memory_order_relaxed);
                }

                return line;
            }
        }

        // Cache miss
        if (m_config.enableStatistics)
        {
            m_stats.misses.fetch_add(1, std::memory_order_relaxed);
        }

        return nullptr;
    }
     /**
     * @brief Get cache line size
     * @return Cache line size in bytes
     */
    quint16 getCacheLineSize() const { return static_cast<quint16>(m_config.lineSize); }
    /**
     * @brief Get cache line for replacement (may require write lock)
     * @param tag Tag for the new line
     * @param address Address for the new line
     * @return Pointer to cache line for replacement
     */
    CacheLine *getReplacementLine(uint64_t tag, uint64_t address)
    {
        // First try to find an invalid line (no eviction needed)
        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();
            if (!line->isValid())
            {
                line->setTag(tag);
                line->setAddress(address);
                line->setValid(true);
                return line;
            }
        }

        // All lines valid - need to evict using LRU
        size_t victimIndex = selectLRUVictim();
        CacheLine *victim = m_lines[victimIndex].get();

        // Handle eviction notifications
        if (victim->isValid())
        {
            handleEviction(victim->getAddress(), victim->getTag());

            if (m_config.enableStatistics)
            {
                m_stats.evictions.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Prepare victim line for new data
        victim->setTag(tag);
        victim->setAddress(address);
        victim->setValid(true);
        victim->clear(); // Clear old data

        // Update access time
        const uint64_t currentTime = m_globalTime.fetch_add(1, std::memory_order_relaxed);
        m_accessTimes[victimIndex].store(currentTime, std::memory_order_relaxed);

        return victim;
    }

    /**
     * @brief Invalidate cache line by address
     * @param address Address to invalidate
     * @return True if line was found and invalidated
     */
    bool invalidateLine(uint64_t address)
    {
        bool found = false;

        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();

            if (line->isValid() && line->getAddress() == address)
            {
                line->setValid(false);
                line->clear();
                m_accessTimes[i].store(0, std::memory_order_relaxed);
                found = true;

                // Notify integration points
                handleInvalidation(address);

                if (m_config.enableStatistics)
                {
                    m_stats.invalidations.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        return found;
    }

    /**
     * @brief Invalidate cache line by tag
     * @param tag Tag to invalidate
     * @return Number of lines invalidated
     */
    size_t invalidateByTag(uint64_t tag)
    {
        size_t count = 0;

        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();

            if (line->isValid() && line->getTag() == tag)
            {
                uint64_t address = line->getAddress();
                line->setValid(false);
                line->clear();
                m_accessTimes[i].store(0, std::memory_order_relaxed);
                count++;

                // Notify integration points
                handleInvalidation(address);
            }
        }

        if (m_config.enableStatistics && count > 0)
        {
            m_stats.invalidations.fetch_add(count, std::memory_order_relaxed);
        }

        return count;
    }

    /**
     * @brief Invalidate all cache lines
     */
    void invalidateAll()
    {
        size_t count = 0;

        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();

            if (line->isValid())
            {
                uint64_t address = line->getAddress();
                line->setValid(false);
                line->clear();
                m_accessTimes[i].store(0, std::memory_order_relaxed);
                count++;

                // Notify integration points
                handleInvalidation(address);
            }
        }

        if (m_config.enableStatistics && count > 0)
        {
            m_stats.invalidations.fetch_add(count, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Get cache statistics
     * @return Current statistics snapshot
     */
    Statistics getStatistics() const
    {
        Statistics stats;
        stats.hits.store(m_stats.hits.load(std::memory_order_relaxed));
        stats.misses.store(m_stats.misses.load(std::memory_order_relaxed));
        stats.evictions.store(m_stats.evictions.load(std::memory_order_relaxed));
        stats.invalidations.store(m_stats.invalidations.load(std::memory_order_relaxed));
        return stats;
    }

    /**
     * @brief Clear statistics
     */
    void clearStatistics()
    {
        m_stats.hits.store(0, std::memory_order_relaxed);
        m_stats.misses.store(0, std::memory_order_relaxed);
        m_stats.evictions.store(0, std::memory_order_relaxed);
        m_stats.invalidations.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Get cache utilization (0.0 to 1.0)
     * @return Fraction of cache lines that are valid
     */
    double getUtilization() const
    {
        size_t validLines = 0;
        for (const auto &line : m_lines)
        {
            if (line->isValid())
            {
                validLines++;
            }
        }
        return static_cast<double>(validLines) / m_config.associativity;
    }

    // Integration methods
    void setTLBSystem(TLBSystem *tlb, uint16_t cpuId)
    {
        m_tlbSystem = tlb;
        m_cpuId = cpuId;
    }

    void setInstructionCache(AlphaInstructionCache *icache) { m_instructionCache = icache; }

    // Configuration accessors
    size_t getAssociativity() const { return m_config.associativity; }
    /**
     * @brief Get all dirty lines in this set
     * @return Vector of pairs containing (address, line_pointer) for dirty lines
     */
    std::vector<std::pair<uint64_t, CacheLine *>> getDirtyLines()
    {
        std::vector<std::pair<uint64_t, CacheLine *>> dirtyLines;

        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();
            if (line->isValid() && line->isDirty())
            {
                dirtyLines.emplace_back(line->getAddress(), line);
            }
        }

        return dirtyLines;
    }

    size_t getLineSize() const { return m_config.lineSize; }
    bool isPrefetchEnabled() const { return m_config.enablePrefetch; }

    /**
     * @brief Write back all dirty cache lines in this set
     * @param backingWrite Function to perform write-back
     * @return True if all write-backs succeeded
     */
    bool writeBackAllDirty(std::function<bool(uint64_t, const void *, size_t)> backingWrite)
    {
        bool allSuccess = true;
        size_t writeBackCount = 0;

        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            CacheLine *line = m_lines[i].get();
            if (line->isValid() && line->isDirty())
            {
                uint64_t address = line->getAddress();

                if (backingWrite && backingWrite(address, line->getData(), line->getSize()))
                {
                    line->setDirty(false);
                    writeBackCount++;

                    if (m_config.enableStatistics)
                    {
                        // Note: This would need a writebacks counter added to CacheSet::Statistics
                    }
                }
                else
                {
                    allSuccess = false;
                }
            }
        }

        return allSuccess;
    }


  private:
    /**
     * @brief Select LRU victim for replacement
     * @return Index of the LRU cache line
     */
    size_t selectLRUVictim()
    {
        uint64_t oldestTime = UINT64_MAX;
        size_t victimIndex = 0;

        // Find the line with the oldest access time
        for (size_t i = 0; i < m_config.associativity; ++i)
        {
            uint64_t accessTime = m_accessTimes[i].load(std::memory_order_relaxed);
            if (accessTime < oldestTime)
            {
                oldestTime = accessTime;
                victimIndex = i;
            }
        }

        return victimIndex;
    }

    /**
     * @brief Handle cache line eviction notification
     * @param address Address being evicted
     * @param tag Tag being evicted
     */
    void handleEviction(uint64_t address, uint64_t tag)
    {
        // Notify TLB system about eviction if needed
        if (m_tlbSystem)
        {
            // TLB might want to know about instruction cache evictions
            // for coherency purposes
        }

        // Notify instruction cache about eviction
        if (m_instructionCache)
        {
            // Instruction cache might want to prefetch replacement
        }
    }

    /**
     * @brief Handle cache line invalidation notification
     * @param address Address being invalidated
     */
    void handleInvalidation(uint64_t address)
    {
        // Notify TLB system about invalidation
        if (m_tlbSystem)
        {
            // For instruction cache invalidations, also invalidate TLB instruction entries
            // This maintains coherency between instruction cache and TLB
        }

        // Notify instruction cache about invalidation
        if (m_instructionCache)
        {
            // Cross-invalidation between different cache levels
        }
    }
};


