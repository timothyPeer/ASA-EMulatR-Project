// alphacachehierarchy.h

#include "../AEJ/alphacachehierarchy.h"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

// Cache configuration constants
// alphacachehierarchy.cpp
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>


// Debug logging macro - adjust based on your logging system
#ifndef DEBUG_LOG
#define DEBUG_LOG(fmt, ...)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        printf("[CACHE] " fmt "\n", ##__VA_ARGS__);                                                                    \
        fflush(stdout);                                                                                                \
    } while (0)
#endif

alphaCacheHierarchy::alphaCacheHierarchy()
{
    // Initialize L1 Instruction Cache
    m_cacheLevels[static_cast<int>(CacheLevel::L1_INSTRUCTION)] =
        std::make_unique<CacheLevel_t>(AlphaCacheConfig::L1_ICACHE_SIZE, AlphaCacheConfig::L1_ASSOCIATIVITY,
                                       WritePolicy::WRITE_THROUGH // I-cache typically write-through
        );

    // Initialize L1 Data Cache
    m_cacheLevels[static_cast<int>(CacheLevel::L1_DATA)] = std::make_unique<CacheLevel_t>(
        AlphaCacheConfig::L1_DCACHE_SIZE, AlphaCacheConfig::L1_ASSOCIATIVITY, WritePolicy::WRITE_BACK);

    // Initialize L2 Unified Cache
    m_cacheLevels[static_cast<int>(CacheLevel::L2_UNIFIED)] = std::make_unique<CacheLevel_t>(
        AlphaCacheConfig::L2_CACHE_SIZE, AlphaCacheConfig::L2_ASSOCIATIVITY, WritePolicy::WRITE_BACK);

    // Initialize L3 Unified Cache
    m_cacheLevels[static_cast<int>(CacheLevel::L3_UNIFIED)] = std::make_unique<CacheLevel_t>(
        AlphaCacheConfig::L3_CACHE_SIZE, AlphaCacheConfig::L3_ASSOCIATIVITY, WritePolicy::WRITE_BACK);

    DEBUG_LOG("Alpha cache hierarchy initialized");
    DEBUG_LOG("L1I: %dKB, L1D: %dKB, L2: %dMB, L3: %dMB", AlphaCacheConfig::L1_ICACHE_SIZE / 1024,
              AlphaCacheConfig::L1_DCACHE_SIZE / 1024, AlphaCacheConfig::L2_CACHE_SIZE / (1024 * 1024),
              AlphaCacheConfig::L3_CACHE_SIZE / (1024 * 1024));
}

bool alphaCacheHierarchy::isAddressCached(uint64_t address, AccessType type)
{
    CacheLevel level = getAppropriateCacheLevel(type);
    auto &cache = *m_cacheLevels[static_cast<int>(level)];

    CacheLine *line = findCacheLine(address, cache);
    return (line != nullptr && line->valid && line->state != CacheLineState::INVALID);
}

bool alphaCacheHierarchy::accessCache(uint64_t address, AccessType type, void *data, uint32_t size)
{
    m_globalAccessCounter++;

    CacheLevel level = getAppropriateCacheLevel(type);
    auto &cache = *m_cacheLevels[static_cast<int>(level)];

    incrementAccessCounter(cache);

    // Try to find the cache line
    CacheLine *line = findCacheLine(address, cache);

    if (line && line->valid && line->state != CacheLineState::INVALID)
    {
        // Cache hit
        cache.hits++;
        updateReplacementInfo(*line, cache.sets[decomposeAddress(address, cache).index]);

        // Copy data if requested
        if (data && size > 0)
        {
            uint32_t offset = address & (AlphaCacheConfig::CACHE_LINE_SIZE - 1);
            std::memcpy(data, &line->data[offset], std::min(size, AlphaCacheConfig::CACHE_LINE_SIZE - offset));
        }

        // Handle write operations
        if (type == AccessType::WRITE)
        {
            line->dirty = true;
            line->state = CacheLineState::MODIFIED;
            maintainCoherency(address, type);
        }

        return true;
    }
    else
    {
        // Cache miss
        cache.misses++;

        // Try to pull from next level
        if (!pullFromNextLevel(address, level))
        {
            // Failed to get data from memory hierarchy
            return false;
        }

        // Allocate cache line
        line = allocateCacheLine(address, cache);
        if (!line)
        {
            return false;
        }

        // Set up the new cache line
        AddressInfo addrInfo = decomposeAddress(address, cache);
        line->tag = addrInfo.tag;
        line->valid = true;
        line->dirty = (type == AccessType::WRITE);
        line->state = (type == AccessType::WRITE) ? CacheLineState::MODIFIED : CacheLineState::EXCLUSIVE;
        line->lastAccess = m_globalAccessCounter;

        // Copy data if provided
        if (data && size > 0 && type == AccessType::WRITE)
        {
            uint32_t offset = address & (AlphaCacheConfig::CACHE_LINE_SIZE - 1);
            std::memcpy(&line->data[offset], data, std::min(size, AlphaCacheConfig::CACHE_LINE_SIZE - offset));
        }

        maintainCoherency(address, type);
        return true;
    }
}

void alphaCacheHierarchy::flushAll()
{
    DEBUG_LOG("Flushing all cache levels");

    for (auto &cachePtr : m_cacheLevels)
    {
        if (!cachePtr)
            continue;

        auto &cache = *cachePtr;
        for (uint32_t setIdx = 0; setIdx < cache.numSets; setIdx++)
        {
            auto &set = cache.sets[setIdx];
            for (auto &line : set.lines)
            {
                if (line.valid && line.dirty)
                {
                    // Write back dirty lines
                    uint64_t address = composeAddress(line.tag, setIdx, 0, cache);
                    writebackLine(line, address);
                    cache.writebacks++;
                }
                // Invalidate line
                line.valid = false;
                line.state = CacheLineState::INVALID;
                line.dirty = false;
            }
        }
    }
}

void alphaCacheHierarchy::flushInstructionCache()
{
    DEBUG_LOG("Flushing instruction cache");

    auto &cache = *m_cacheLevels[static_cast<int>(CacheLevel::L1_INSTRUCTION)];
    for (uint32_t setIdx = 0; setIdx < cache.numSets; setIdx++)
    {
        auto &set = cache.sets[setIdx];
        for (auto &line : set.lines)
        {
            if (line.valid)
            {
                if (line.dirty)
                {
                    uint64_t address = composeAddress(line.tag, setIdx, 0, cache);
                    writebackLine(line, address);
                    cache.writebacks++;
                }
                line.valid = false;
                line.state = CacheLineState::INVALID;
                line.dirty = false;
            }
        }
    }
}

void alphaCacheHierarchy::flushDataCache()
{
    DEBUG_LOG("Flushing data cache");

    auto &cache = *m_cacheLevels[static_cast<int>(CacheLevel::L1_DATA)];
    for (uint32_t setIdx = 0; setIdx < cache.numSets; setIdx++)
    {
        auto &set = cache.sets[setIdx];
        for (auto &line : set.lines)
        {
            if (line.valid)
            {
                if (line.dirty)
                {
                    uint64_t address = composeAddress(line.tag, setIdx, 0, cache);
                    writebackLine(line, address);
                    cache.writebacks++;
                }
                line.valid = false;
                line.state = CacheLineState::INVALID;
                line.dirty = false;
            }
        }
    }
}

void alphaCacheHierarchy::invalidateLine(uint64_t address)
{
    DEBUG_LOG("Invalidating cache line for address 0x%016llX", address);

    for (auto &cachePtr : m_cacheLevels)
    {
        if (!cachePtr)
            continue;

        CacheLine *line = findCacheLine(address, *cachePtr);
        if (line && line->valid)
        {
            if (line->dirty)
            {
                writebackLine(*line, address);
                cachePtr->writebacks++;
            }
            line->valid = false;
            line->state = CacheLineState::INVALID;
            line->dirty = false;
        }
    }
}

void alphaCacheHierarchy::invalidateRange(uint64_t startAddr, uint64_t endAddr)
{
    DEBUG_LOG("Invalidating cache range 0x%016llX - 0x%016llX", startAddr, endAddr);

    uint64_t lineSize = AlphaCacheConfig::CACHE_LINE_SIZE;
    uint64_t alignedStart = startAddr & ~(lineSize - 1);
    uint64_t alignedEnd = (endAddr + lineSize - 1) & ~(lineSize - 1);

    for (uint64_t addr = alignedStart; addr < alignedEnd; addr += lineSize)
    {
        invalidateLine(addr);
    }
}

void alphaCacheHierarchy::flushLine(uint64_t address)
{
    DEBUG_LOG("Flushing cache line for address 0x%016llX", address);

    for (auto &cachePtr : m_cacheLevels)
    {
        if (!cachePtr)
            continue;

        CacheLine *line = findCacheLine(address, *cachePtr);
        if (line && line->valid && line->dirty)
        {
            writebackLine(*line, address);
            cachePtr->writebacks++;
            line->dirty = false;
        }
    }
}

void alphaCacheHierarchy::memoryBarrier()
{
    DEBUG_LOG("Executing cache memory barrier");

    // Ensure all pending cache operations complete
    for (auto &cachePtr : m_cacheLevels)
    {
        if (!cachePtr)
            continue;

        // Write back all dirty lines
        auto &cache = *cachePtr;
        for (uint32_t setIdx = 0; setIdx < cache.numSets; setIdx++)
        {
            auto &set = cache.sets[setIdx];
            for (auto &line : set.lines)
            {
                if (line.valid && line.dirty)
                {
                    uint64_t address = composeAddress(line.tag, setIdx, 0, cache);
                    writebackLine(line, address);
                    cache.writebacks++;
                    line.dirty = false;
                }
            }
        }
    }
}

void alphaCacheHierarchy::writeBarrier()
{
    DEBUG_LOG("Executing cache write barrier");

    // Similar to memory barrier but only for writes
    memoryBarrier();
}

void alphaCacheHierarchy::readBarrier()
{
    DEBUG_LOG("Executing cache read barrier");

    // Invalidate speculative loads - simplified implementation
    // In reality, this would interact with the processor pipeline
}

alphaCacheHierarchy::CacheStats alphaCacheHierarchy::getOverallStats() const
{
    CacheStats overall;

    for (const auto &cachePtr : m_cacheLevels)
    {
        if (!cachePtr)
            continue;

        const auto &cache = *cachePtr;
        overall.totalAccesses += (cache.hits + cache.misses);
        overall.totalHits += cache.hits;
        overall.totalMisses += cache.misses;
        overall.totalEvictions += cache.evictions;
        overall.totalWritebacks += cache.writebacks;
    }

    return overall;
}

alphaCacheHierarchy::CacheStats alphaCacheHierarchy::getLevelStats(CacheLevel level) const
{
    CacheStats stats;
    const auto &cache = *m_cacheLevels[static_cast<int>(level)];

    stats.totalAccesses = cache.hits + cache.misses;
    stats.totalHits = cache.hits;
    stats.totalMisses = cache.misses;
    stats.totalEvictions = cache.evictions;
    stats.totalWritebacks = cache.writebacks;

    return stats;
}

void alphaCacheHierarchy::resetStats()
{
    DEBUG_LOG("Resetting cache statistics");

    for (auto &cachePtr : m_cacheLevels)
    {
        if (!cachePtr)
            continue;

        auto &cache = *cachePtr;
        cache.hits = 0;
        cache.misses = 0;
        cache.evictions = 0;
        cache.writebacks = 0;
        cache.accessCounter = 0;
    }

    m_globalAccessCounter = 0;
}

void alphaCacheHierarchy::dumpStats() const
{
    DEBUG_LOG("=== Alpha Cache Hierarchy Statistics ===");

    const char *levelNames[] = {"L1-I", "L1-D", "L2", "L3"};

    for (int i = 0; i < 4; i++)
    {
        const auto &cache = *m_cacheLevels[i];
        uint64_t totalAccesses = cache.hits + cache.misses;
        double hitRate = totalAccesses > 0 ? (100.0 * cache.hits) / totalAccesses : 0.0;

        DEBUG_LOG("%s: Accesses=%llu, Hits=%llu (%.2f%%), Misses=%llu, Evictions=%llu, Writebacks=%llu", levelNames[i],
                  totalAccesses, cache.hits, hitRate, cache.misses, cache.evictions, cache.writebacks);
    }

    auto overall = getOverallStats();
    double overallHitRate = overall.totalAccesses > 0 ? (100.0 * overall.totalHits) / overall.totalAccesses : 0.0;
    DEBUG_LOG("Overall: Accesses=%llu, Hits=%llu (%.2f%%), Misses=%llu", overall.totalAccesses, overall.totalHits,
              overallHitRate, overall.totalMisses);
}

// Private helper method implementations
alphaCacheHierarchy::AddressInfo alphaCacheHierarchy::decomposeAddress(uint64_t address,
                                                                       const CacheLevel_t &cache) const
{
    AddressInfo info;
    info.offset = address & ((1ULL << cache.offsetBits) - 1);
    info.index = (address >> cache.offsetBits) & ((1ULL << cache.indexBits) - 1);
    info.tag = address >> (cache.offsetBits + cache.indexBits);
    return info;
}

uint64_t alphaCacheHierarchy::composeAddress(uint64_t tag, uint32_t index, uint32_t offset,
                                             const CacheLevel_t &cache) const
{
    return (tag << (cache.offsetBits + cache.indexBits)) | (static_cast<uint64_t>(index) << cache.offsetBits) | offset;
}

alphaCacheHierarchy::CacheLine *alphaCacheHierarchy::findCacheLine(uint64_t address, CacheLevel_t &cache)
{
    AddressInfo addrInfo = decomposeAddress(address, cache);
    auto &set = cache.sets[addrInfo.index];

    for (auto &line : set.lines)
    {
        if (line.valid && line.tag == addrInfo.tag)
        {
            return &line;
        }
    }

    return nullptr;
}

alphaCacheHierarchy::CacheLine *alphaCacheHierarchy::allocateCacheLine(uint64_t address, CacheLevel_t &cache)
{
    AddressInfo addrInfo = decomposeAddress(address, cache);
    auto &set = cache.sets[addrInfo.index];

    // Look for invalid line first
    for (auto &line : set.lines)
    {
        if (!line.valid)
        {
            return &line;
        }
    }

    // No invalid lines, need to evict
    CacheLine *victim = selectVictim(set, cache);
    if (victim)
    {
        evictLine(*victim, cache, addrInfo.index);
    }

    return victim;
}

alphaCacheHierarchy::CacheLine *alphaCacheHierarchy::selectVictim(CacheSet &set, const CacheLevel_t &cache)
{
    if (cache.replacementPolicy == ReplacementPolicy::LRU)
    {
        // Find least recently used
        CacheLine *lru = nullptr;
        uint64_t oldestAccess = UINT64_MAX;

        for (auto &line : set.lines)
        {
            if (line.lastAccess < oldestAccess)
            {
                oldestAccess = line.lastAccess;
                lru = &line;
            }
        }
        return lru;
    }
    else if (cache.replacementPolicy == ReplacementPolicy::RANDOM)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, set.lines.size() - 1);
        return &set.lines[dis(gen)];
    }

    // Default to first line
    return &set.lines[0];
}

void alphaCacheHierarchy::updateReplacementInfo(CacheLine &line, CacheSet &set)
{
    line.lastAccess = m_globalAccessCounter;
    set.accessCounter++;
}

void alphaCacheHierarchy::maintainCoherency(uint64_t address, AccessType type)
{
    // Simplified MESI coherency - in reality this would be much more complex
    // This would involve bus snooping, invalidation messages, etc.

    if (type == AccessType::WRITE)
    {
        // Invalidate line in other cache levels
        for (auto &cachePtr : m_cacheLevels)
        {
            if (!cachePtr)
                continue;

            CacheLine *line = findCacheLine(address, *cachePtr);
            if (line && line->valid && line->state == CacheLineState::SHARED)
            {
                line->state = CacheLineState::INVALID;
                line->valid = false;
            }
        }
    }
}

void alphaCacheHierarchy::evictLine(CacheLine &line, CacheLevel_t &cache, uint32_t setIndex)
{
    if (line.dirty)
    {
        uint64_t address = composeAddress(line.tag, setIndex, 0, cache);
        writebackLine(line, address);
        cache.writebacks++;
    }

    line.valid = false;
    line.state = CacheLineState::INVALID;
    line.dirty = false;
    cache.evictions++;
}

void alphaCacheHierarchy::writebackLine(const CacheLine &line, uint64_t address)
{
    // In a real implementation, this would write the cache line back to the next level
    // or to main memory. For simulation purposes, we just log it.
    DEBUG_LOG("Writing back dirty cache line at address 0x%016llX", address);
}

bool alphaCacheHierarchy::pullFromNextLevel(uint64_t address, CacheLevel currentLevel)
{
    // Simplified implementation - in reality this would pull data from the next cache level
    // or from main memory if this is the last level cache
    DEBUG_LOG("Pulling cache line from next level for address 0x%016llX", address);
    return true; // Assume success for simulation
}

bool alphaCacheHierarchy::isInstructionAccess(AccessType type) const { return type == AccessType::INSTRUCTION_FETCH; }

alphaCacheHierarchy::CacheLevel alphaCacheHierarchy::getAppropriateCacheLevel(AccessType type) const
{
    if (type == AccessType::INSTRUCTION_FETCH)
    {
        return CacheLevel::L1_INSTRUCTION;
    }
    else
    {
        return CacheLevel::L1_DATA;
    }
}

void alphaCacheHierarchy::incrementAccessCounter(CacheLevel_t &cache) { cache.accessCounter++; }

void alphaCacheHierarchy::setWritePolicy(CacheLevel level, WritePolicy policy)
{
    m_cacheLevels[static_cast<int>(level)]->writePolicy = policy;
}

void alphaCacheHierarchy::setReplacementPolicy(CacheLevel level, ReplacementPolicy policy)
{
    m_cacheLevels[static_cast<int>(level)]->replacementPolicy = policy;
}

void alphaCacheHierarchy::dumpCacheState(CacheLevel level) const
{
    const auto &cache = *m_cacheLevels[static_cast<int>(level)];
    const char *levelNames[] = {"L1-I", "L1-D", "L2", "L3"};

    DEBUG_LOG("=== Cache State Dump: %s ===", levelNames[static_cast<int>(level)]);
    DEBUG_LOG("Size: %d bytes, Sets: %d, Associativity: %d", cache.size, cache.numSets, cache.associativity);

    uint32_t validLines = 0;
    uint32_t dirtyLines = 0;

    for (uint32_t setIdx = 0; setIdx < std::min(cache.numSets, 8U); setIdx++)
    {
        const auto &set = cache.sets[setIdx];
        DEBUG_LOG("Set %d:", setIdx);

        for (uint32_t wayIdx = 0; wayIdx < cache.associativity; wayIdx++)
        {
            const auto &line = set.lines[wayIdx];
            if (line.valid)
            {
                validLines++;
                if (line.dirty)
                    dirtyLines++;

                uint64_t address = composeAddress(line.tag, setIdx, 0, cache);
                DEBUG_LOG("  Way %d: Tag=0x%llX, Addr=0x%016llX, State=%d, Dirty=%d", wayIdx, line.tag, address,
                          static_cast<int>(line.state), line.dirty);
            }
            else
            {
                DEBUG_LOG("  Way %d: Invalid", wayIdx);
            }
        }
    }

    if (cache.numSets > 8)
    {
        DEBUG_LOG("... (%d more sets)", cache.numSets - 8);
    }

    DEBUG_LOG("Valid lines: %d, Dirty lines: %d", validLines, dirtyLines);
}

bool alphaCacheHierarchy::validateCacheCoherency() const
{
    // Basic coherency validation - check for duplicate valid lines
    std::unordered_map<uint64_t, int> addressCount;

    for (int levelIdx = 0; levelIdx < 4; levelIdx++)
    {
        const auto &cache = *m_cacheLevels[levelIdx];

        for (uint32_t setIdx = 0; setIdx < cache.numSets; setIdx++)
        {
            const auto &set = cache.sets[setIdx];

            for (const auto &line : set.lines)
            {
                if (line.valid)
                {
                    uint64_t address = composeAddress(line.tag, setIdx, 0, cache);
                    addressCount[address]++;

                    if (addressCount[address] > 1)
                    {
                        DEBUG_LOG("Coherency violation: Address 0x%016llX found in multiple cache lines", address);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}