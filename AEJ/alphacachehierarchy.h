// alphacachehierarchy.h
#pragma once

#include <array>
#include "enumerations/countTrailingZeros.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

// Cache configuration constants
namespace AlphaCacheConfig
{
static constexpr uint32_t L1_ICACHE_SIZE = 64 * 1024;      // 64KB L1 I-cache
static constexpr uint32_t L1_DCACHE_SIZE = 64 * 1024;      // 64KB L1 D-cache
static constexpr uint32_t L2_CACHE_SIZE = 4 * 1024 * 1024; // 4MB L2 unified
static constexpr uint32_t L3_CACHE_SIZE = 8 * 1024 * 1024; // 8MB L3 unified

static constexpr uint32_t CACHE_LINE_SIZE = 64;  // 64-byte cache lines
static constexpr uint32_t L1_ASSOCIATIVITY = 2;  // 2-way set associative
static constexpr uint32_t L2_ASSOCIATIVITY = 8;  // 8-way set associative
static constexpr uint32_t L3_ASSOCIATIVITY = 16; // 16-way set associative
} // namespace AlphaCacheConfig

class alphaCacheHierarchy
{
  public:
    enum class CacheLevel
    {
        L1_INSTRUCTION = 0,
        L1_DATA = 1,
        L2_UNIFIED = 2,
        L3_UNIFIED = 3
    };

    enum class AccessType
    {
        READ,
        WRITE,
        INSTRUCTION_FETCH
    };

    enum class WritePolicy
    {
        WRITE_BACK,
        WRITE_THROUGH
    };

    enum class ReplacementPolicy
    {
        LRU,
        RANDOM,
        FIFO
    };

    // Cache line state for MESI protocol
    enum class CacheLineState
    {
        INVALID = 0,
        SHARED = 1,
        EXCLUSIVE = 2,
        MODIFIED = 3
    };

    struct CacheLine
    {
        uint64_t tag{0};
        CacheLineState state{CacheLineState::INVALID};
        bool valid{false};
        bool dirty{false};
        uint64_t lastAccess{0};
        std::array<uint8_t, AlphaCacheConfig::CACHE_LINE_SIZE> data{};

        CacheLine() = default;
    };

    struct CacheSet
    {
        std::vector<CacheLine> lines;
        uint64_t accessCounter{0};

        explicit CacheSet(uint32_t associativity) : lines(associativity) {}
    };

    struct CacheLevel_t
    {
        std::vector<CacheSet> sets;
        uint32_t size;
        uint32_t associativity;
        uint32_t numSets;
        uint32_t indexBits;
        uint32_t offsetBits;
        uint32_t tagBits;
        WritePolicy writePolicy;
        ReplacementPolicy replacementPolicy;
        uint64_t accessCounter{0};

        // Performance counters
        uint64_t hits{0};
        uint64_t misses{0};
        uint64_t evictions{0};
        uint64_t writebacks{0};

        CacheLevel_t(uint32_t cacheSize, uint32_t assoc, WritePolicy wp = WritePolicy::WRITE_BACK)
            : size(cacheSize), associativity(assoc), writePolicy(wp), replacementPolicy(ReplacementPolicy::LRU)
        {
            numSets = size / (AlphaCacheConfig::CACHE_LINE_SIZE * associativity);
            indexBits = countTrailingZeros(numSets); // Count trailing zeros
            offsetBits = countTrailingZeros(AlphaCacheConfig::CACHE_LINE_SIZE);
            tagBits = 64 - indexBits - offsetBits;

            sets.reserve(numSets);
            for (uint32_t i = 0; i < numSets; i++)
            {
                sets.emplace_back(associativity);
            }
        }
    };

    struct CacheStats
    {
        uint64_t totalAccesses{0};
        uint64_t totalHits{0};
        uint64_t totalMisses{0};
        uint64_t totalEvictions{0};
        uint64_t totalWritebacks{0};

        double getHitRate() const { return totalAccesses > 0 ? static_cast<double>(totalHits) / totalAccesses : 0.0; }

        double getMissRate() const
        {
            return totalAccesses > 0 ? static_cast<double>(totalMisses) / totalAccesses : 0.0;
        }
    };

  public:
    alphaCacheHierarchy();
    ~alphaCacheHierarchy() = default;

    // Core cache interface
    bool isAddressCached(uint64_t address, AccessType type = AccessType::READ);
    bool accessCache(uint64_t address, AccessType type, void *data = nullptr, uint32_t size = 8);

    // Cache management
    void flushAll();
    void flushInstructionCache();
    void flushDataCache();
    void invalidateLine(uint64_t address);
    void invalidateRange(uint64_t startAddr, uint64_t endAddr);
    void flushLine(uint64_t address);

    // Memory ordering
    void memoryBarrier();
    void writeBarrier();
    void readBarrier();

    // Performance and statistics
    CacheStats getOverallStats() const;
    CacheStats getLevelStats(CacheLevel level) const;
    void resetStats();
    void dumpStats() const;

    // Configuration
    void setWritePolicy(CacheLevel level, WritePolicy policy);
    void setReplacementPolicy(CacheLevel level, ReplacementPolicy policy);

    // Debug interface
    void dumpCacheState(CacheLevel level) const;
    bool validateCacheCoherency() const;

  private:
    std::array<std::unique_ptr<CacheLevel_t>, 4> m_cacheLevels;
    uint64_t m_globalAccessCounter{0};

    // Address decomposition helpers
    struct AddressInfo
    {
        uint64_t tag;
        uint32_t index;
        uint32_t offset;
    };

    AddressInfo decomposeAddress(uint64_t address, const CacheLevel_t &cache) const;
    uint64_t composeAddress(uint64_t tag, uint32_t index, uint32_t offset, const CacheLevel_t &cache) const;

    // Cache access helpers
    CacheLine *findCacheLine(uint64_t address, CacheLevel_t &cache);
    CacheLine *allocateCacheLine(uint64_t address, CacheLevel_t &cache);
    CacheLine *selectVictim(CacheSet &set, const CacheLevel_t &cache);
    void updateReplacementInfo(CacheLine &line, CacheSet &set);

    // Coherency and hierarchy management
    void maintainCoherency(uint64_t address, AccessType type);
    void evictLine(CacheLine &line, CacheLevel_t &cache, uint32_t setIndex);
    void writebackLine(const CacheLine &line, uint64_t address);
    bool pullFromNextLevel(uint64_t address, CacheLevel currentLevel);

    // Utility functions
    bool isInstructionAccess(AccessType type) const;
    CacheLevel getAppropriateCacheLevel(AccessType type) const;
    void incrementAccessCounter(CacheLevel_t &cache);
};



