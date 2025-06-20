#include "AlphaInstructionCache.h"
#include "tlbCacheSystemIntegrator.h"
#include "constants/structStatistics.h"
#include "enumerations/enumLineState.h"
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QtMath>
#include <memory>
#include "../AESH/QSettingsConfigLoader.h"
#include "structures/structCacheConfig.h"
#include "AlphaMemorySystem_refactored.h"
#include "UnifiedDataCache.h"

AlphaInstructionCache::AlphaInstructionCache(QObject *parent, AlphaMemorySystem *memorySystem,
                                             const CacheConfig &config, uint16_t cpuId)
    : QObject(parent), m_cacheSize(config.cacheSize), m_lineSize(config.lineSize),
      m_associativity(config.associativity), m_cpuId(cpuId), m_memorySystem(memorySystem)
{
    // Validate critical parameters
    if (!m_memorySystem)
    {
        ERROR_LOG("AlphaInstructionCache: Memory system is required but was null");
        return;
    }

    if (!config.isValid())
    {
        ERROR_LOG("AlphaInstructionCache: Invalid cache configuration");
        return;
    }

    // Convert CacheConfig to UnifiedDataCache::Config
    UnifiedDataCache::Config unifiedConfig = config.toUnifiedConfig();

    // Create the unified cache system
    m_unifiedCache = std::make_unique<UnifiedDataCache>(unifiedConfig, this);

    // Set up integration with TLB system through memory system
    if (m_memorySystem->getTlbSystem())
    {
        m_unifiedCache->setTLBSystem(m_memorySystem->getTlbSystem(), m_cpuId);
        DEBUG_LOG("AlphaInstructionCache", "Integrated with TLB system for CPU %u", m_cpuId);
    }

    // Set up backing read function for cache misses
    m_unifiedCache->setBackingRead([this](quint64 addr, void *buf, size_t size) -> bool
                                   { return this->loadFromMemorySystem(addr, buf, size); });

    // Connect signals for monitoring
    connect(m_unifiedCache.get(), &UnifiedDataCache::sigStatsChanged, this,
            &AlphaInstructionCache::sigStatisticsUpdated);
    connect(m_unifiedCache.get(), &UnifiedDataCache::sigLineInvalidated, this,
            &AlphaInstructionCache::sigLineInvalidated);

    DEBUG_LOG("AlphaInstructionCache", "Initialized %zuKB cache, %zu sets, %zu-way associative for CPU %u",
              m_cacheSize / 1024, unifiedConfig.numSets, m_associativity, cpuId);
}


// Static methods for CacheConfig
CacheConfig CacheConfig::fromConfigFile(const QString &configPath, const QString &cpuSection)
{
    CacheConfig config;
    config.configSource = QString("file:%1[%2]").arg(configPath, cpuSection);

    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup(cpuSection);

    // Load cache configuration with validation
    config.cacheSize = settings.value("InstructionCacheSize", config.cacheSize).toULongLong();
    config.lineSize = settings.value("InstructionCacheLineSize", config.lineSize).toULongLong();
    config.associativity = settings.value("InstructionCacheAssociativity", config.associativity).toULongLong();
    config.replacementPolicy = settings.value("InstructionCacheReplacement", config.replacementPolicy).toString();
    config.autoPrefetchEnabled = settings.value("InstructionCacheAutoPrefetch", config.autoPrefetchEnabled).toBool();

    settings.endGroup();

    if (!config.isValid())
    {
        WARN_LOG(QString("Invalid cache config in %1[%2], using defaults").arg(configPath, cpuSection));
        return CacheConfig();
    }

    DEBUG_LOG(QString("Loaded cache config from %1[%2]: %3KB, %4-way, %5B lines")
                  .arg(configPath, cpuSection)
                  .arg(config.cacheSize / 1024)
                  .arg(config.associativity)
                  .arg(config.lineSize));

    return config;
}

CacheConfig CacheConfig::fromConfigLoader(ConfigLoader *loader, const QString &cpuSection)
{
    CacheConfig config;
    config.configSource = QString("ConfigLoader[%1]").arg(cpuSection);

    if (!loader)
    {
        WARN_LOG("Null ConfigLoader provided, using default cache config");
        return config;
    }

    config.cacheSize = loader->getIntValue(cpuSection, "InstructionCacheSize", config.cacheSize);
    config.lineSize = loader->getIntValue(cpuSection, "InstructionCacheLineSize", config.lineSize);
    config.associativity = loader->getIntValue(cpuSection, "InstructionCacheAssociativity", config.associativity);
    config.replacementPolicy =
        loader->getStringValue(cpuSection, "InstructionCacheReplacement", config.replacementPolicy);
    config.autoPrefetchEnabled =
        loader->getBoolValue(cpuSection, "InstructionCacheAutoPrefetch", config.autoPrefetchEnabled);

    if (!config.isValid())
    {
        WARN_LOG(QString("Invalid cache config from ConfigLoader[%1], using defaults").arg(cpuSection));
        return CacheConfig();
    }

    return config;
}

CacheConfig CacheConfig::forCpuModel(CpuModel model)
{
    CacheConfig config;
    config.configSource = QString("CpuModel:%1").arg(static_cast<int>(model));

    // Alpha CPU model-specific cache configurations
    switch (model)
    {
    case CpuModel::CPU_EV4:
        config.cacheSize = 8192;  // 8KB I-cache
        config.lineSize = 32;     // 32-byte lines
        config.associativity = 1; // Direct mapped
        config.autoPrefetchEnabled = false;
        break;

    case CpuModel::CPU_EV5:
        config.cacheSize = 8192;  // 8KB I-cache
        config.lineSize = 32;     // 32-byte lines
        config.associativity = 2; // 2-way set associative
        config.autoPrefetchEnabled = false;
        break;

    case CpuModel::CPU_EV56:
        config.cacheSize = 16384; // 16KB I-cache
        config.lineSize = 32;     // 32-byte lines
        config.associativity = 2; // 2-way set associative
        config.autoPrefetchEnabled = true;
        break;

    case CpuModel::CPU_PCA56:
        config.cacheSize = 16384; // 16KB I-cache
        config.lineSize = 64;     // 64-byte lines
        config.associativity = 2; // 2-way set associative
        config.autoPrefetchEnabled = true;
        break;

    case CpuModel::CPU_EV6:
        config.cacheSize = 65536; // 64KB I-cache
        config.lineSize = 64;     // 64-byte lines
        config.associativity = 2; // 2-way set associative
        config.autoPrefetchEnabled = true;
        break;

    case CpuModel::CPU_EV67:
    case CpuModel::CPU_EV68:
        config.cacheSize = 65536; // 64KB I-cache
        config.lineSize = 64;     // 64-byte lines
        config.associativity = 4; // 4-way set associative
        config.autoPrefetchEnabled = true;
        break;

    default:
        // Default to EV56-like configuration
        config.cacheSize = 32768; // 32KB I-cache
        config.lineSize = 64;     // 64-byte lines
        config.associativity = 4; // 4-way set associative
        config.autoPrefetchEnabled = true;
        break;
    }

    DEBUG_LOG(QString("Cache config for CPU model %1: %2KB, %3-way, %4B lines")
                  .arg(static_cast<int>(model))
                  .arg(config.cacheSize / 1024)
                  .arg(config.associativity)
                  .arg(config.lineSize));

    return config;
}

AlphaInstructionCache::~AlphaInstructionCache()
{
    DEBUG_LOG(QString("AlphaInstructionCache: Destroyed for CPU %1").arg(m_cpuId));
}

bool AlphaInstructionCache::read(quint64 address, void *buffer, size_t size)
{
    if (!buffer || size == 0 || !m_unifiedCache)
    {
        return false;
    }

    // Use UnifiedDataCache for the actual read operation
    bool success = m_unifiedCache->read(address, buffer, size, [this](quint64 addr, void *buf, size_t sz) -> bool
                                        { return this->loadFromMemorySystem(addr, buf, sz); });

    if (success)
    {
        emit sigCacheHit(address);
        DEBUG_LOG("InstructionCache", "Read hit: addr=0x%llx, size=%zu", address, size);
    }
    else
    {
        emit sigCacheMiss(address);
        DEBUG_LOG("InstructionCache", "Read miss: addr=0x%llx, size=%zu", address, size);
    }

    return success;
}

bool AlphaInstructionCache::fetch(quint64 address, InstructionWord &instruction)
{
    uint32_t rawInstruction = 0;

    if (read(address, &rawInstruction, sizeof(rawInstruction)))
    {
        instruction.setInstruction(rawInstruction);
        instruction.setProgramCounter(address);
        return true;
    }

    return false;
}

bool AlphaInstructionCache::fetchLine(quint64 address, QVector<InstructionWord> &instructions)
{
    quint64 lineAddr = getLineAddress(address);
    QVector<uint8_t> lineData(m_lineSize);

    if (read(lineAddr, lineData.data(), m_lineSize))
    {
        instructions.clear();
        instructions.reserve(m_lineSize / 4); // 4 bytes per instruction

        for (size_t i = 0; i < m_lineSize; i += 4)
        {
            if (i + 4 <= m_lineSize)
            {
                uint32_t rawInstr;
                memcpy(&rawInstr, lineData.constData() + i, 4);
                InstructionWord instr(rawInstr, lineAddr + i);
                instructions.append(instr);
            }
        }
        return true;
    }
    return false;
}

void AlphaInstructionCache::invalidateAll()
{
    if (m_unifiedCache)
    {
        m_unifiedCache->invalidateAll();
        DEBUG_LOG("InstructionCache", "Invalidated all lines for CPU %u", m_cpuId);
    }
    emit sigStatisticsUpdated();
}

void AlphaInstructionCache::invalidateLine(quint64 address)
{
    if (m_unifiedCache)
    {
        m_unifiedCache->invalidateLine(address);
        DEBUG_LOG("InstructionCache", "Invalidated line: addr=0x%llx, CPU %u", address, m_cpuId);
    }
}

bool AlphaInstructionCache::invalidateLine(quint64 address, bool returnSuccess)
{
    if (!m_unifiedCache)
    {
        return false;
    }

    try
    {
        m_unifiedCache->invalidateLine(address);

        // Notify TLB system about instruction cache invalidation
        if (m_memorySystem)
        {
            // Notify other CPU caches
            m_memorySystem->invalidateCacheLines(address, m_lineSize, m_cpuId);
            m_memorySystem->invalidateTlbSingleInstruction(address, m_cpuId);
        }

        emit sigLineInvalidated(address);
        return true;
    }
    catch (...)
    {
        if (returnSuccess)
        {
            DEBUG_LOG("InstructionCache", "Invalidation failed: addr=0x%llx", address);
        }
        return false;
    }
}

void AlphaInstructionCache::invalidateRange(quint64 startAddress, quint64 endAddress)
{
    DEBUG_LOG(QString("AlphaInstructionCache: Invalidating range 0x%1 - 0x%2")
                  .arg(startAddress, 0, 16)
                  .arg(endAddress, 0, 16));

    quint64 lineStart = getLineAddress(startAddress);
    quint64 lineEnd = getLineAddress(endAddress);

    for (quint64 addr = lineStart; addr <= lineEnd; addr += m_lineSize)
    {
        invalidateLine(addr);
    }
}

void AlphaInstructionCache::invalidateByTag(quint64 tag)
{
    for (auto &set : m_cacheSets)
    {
        set.invalidateByTag(tag);
    }
}

void AlphaInstructionCache::flush()
{
    DEBUG_LOG("AlphaInstructionCache: Flushing cache");
    invalidateAll();
}

void AlphaInstructionCache::flushLine(quint64 address) { invalidateLine(address); }

void AlphaInstructionCache::flushDirtyLines()
{
    // For instruction cache, there are no dirty lines (read-only)
    // This is a no-op for I-cache
}

bool AlphaInstructionCache::loadLineFromMemory(quint64 address, CacheLine *line)
{
    if (!m_memorySystem || !line)
    {
        return false;
    }

    quint64 lineAddr = getLineAddress(address);

    // Allocate temporary buffer for line data
    std::vector<uint8_t> lineData(m_lineSize);

    // Read full cache line from memory system
    quint64 value;
    bool success = true;

    for (size_t i = 0; i < m_lineSize; i += 8)
    {
        if (m_memorySystem->readPhysicalMemory(lineAddr + i, value, 8))
        {
            // Copy 8 bytes to line data
            memcpy(lineData.data() + i, &value, qMin(size_t(8), m_lineSize - i));
        }
        else
        {
            success = false;
            break;
        }
    }

    if (success)
    {
        // Copy data to cache line
        if (line->writeData(0, m_lineSize, lineData.data()))
        {
            line->setAddress(lineAddr);
            line->setTag(getTag(lineAddr));
            line->setValid(true);

            return true;
        }
    }

    return false;
}

bool AlphaInstructionCache::loadFromMemorySystem(quint64 address, void *buffer, size_t size)
{
    if (!m_memorySystem || !buffer || size == 0)
    {
        return false;
    }

    quint64 lineAddr = getLineAddress(address);

    // Read from memory system in 8-byte chunks (Alpha word size)
    for (size_t offset = 0; offset < size; offset += 8)
    {
        quint64 value = 0;
        size_t readSize = std::min(size_t(8), size - offset);

        if (!m_memorySystem->readPhysicalMemory(lineAddr + offset, value, readSize))
        {
            DEBUG_LOG("InstructionCache", "Failed to load from memory: addr=0x%llx", lineAddr + offset);
            return false;
        }

        // Copy to buffer
        std::memcpy(static_cast<uint8_t *>(buffer) + offset, &value, readSize);
    }

    DEBUG_LOG("InstructionCache", "Loaded from memory system: addr=0x%llx, size=%zu", address, size);
    return true;
}

// Additional helper method implementations...

quint64 AlphaInstructionCache::getTag(quint64 address) const { return address >> (m_indexBits + m_offsetBits); }

size_t AlphaInstructionCache::getSetIndex(quint64 address) const { return (address >> m_offsetBits) & m_indexMask; }

size_t AlphaInstructionCache::getOffset(quint64 address) const { return address & m_offsetMask; }

quint64 AlphaInstructionCache::getLineAddress(quint64 address) const { return address & ~m_offsetMask; }

void AlphaInstructionCache::recordHit()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.incHit();
}

void AlphaInstructionCache::recordMiss()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.incMisses();
}

void AlphaInstructionCache::recordInvalidation()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.incInvalidations();
}


void AlphaInstructionCache::recordReplacement()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.incReplacements();
}
void AlphaInstructionCache::recordPrefetch()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.incPrefetches();
}
void AlphaInstructionCache::recordCoherencyEvent()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.incCoherencyEvents();
}

double AlphaInstructionCache::getHitRate() const
{
    if (m_unifiedCache)
    {
        return m_unifiedCache->getHitRate();
    }
    return 0.0;
}

Statistics AlphaInstructionCache::getStatistics() const
{
    const_cast<AlphaInstructionCache *>(this)->updateLocalStatistics();
    QMutexLocker locker(&m_statsMutex);
    return m_stats;
}

void AlphaInstructionCache::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats = Statistics();
}

void AlphaInstructionCache::debugCacheAccess(quint64 address, bool hit, const QString &operation)
{
#ifdef DEBUG_INSTRUCTION_CACHE
    DEBUG_LOG(QString("I-Cache %1: 0x%2 %3 (set=%4, tag=0x%5, CPU=%6)")
                  .arg(operation)
                  .arg(address, 0, 16)
                  .arg(hit ? "HIT" : "MISS")
                  .arg(getSetIndex(address))
                  .arg(getTag(address), 0, 16)
                  .arg(m_cpuId));
#endif
}

void AlphaInstructionCache::prefetch(quint64 address)
{
    quint64 lineAddr = getLineAddress(address);
    size_t setIndex = getSetIndex(lineAddr);

    if (setIndex >= m_cacheSets.size())
    {
        return;
    }

    CacheSet &set = m_cacheSets[setIndex];
    quint64 tag = getTag(lineAddr);

    // Check if line is already cached
    CacheLine *line = set.findLine(address, tag);
    if (line && line->isValid())
    {
        return; // Already cached
    }

    // Get a line for prefetch
    if (!line)
    {
        line = set.getReplacementLine(tag, address);
    }

    if (line && loadLineFromMemory(lineAddr, line))
    {
        line->setPrefetched(true);
        recordPrefetch();
        debugCacheAccess(address, false, "PREFETCH");
    }
}

void AlphaInstructionCache::prefetchSequential(quint64 startAddress, size_t lineCount)
{
    quint64 addr = getLineAddress(startAddress);
    for (size_t i = 0; i < lineCount; ++i)
    {
        prefetch(addr);
        addr += m_lineSize;
    }
}

void AlphaInstructionCache::updateLocalStatistics()
{
    if (!getUnifiedCache())
        return;

    auto unifiedStats = getUnifiedCache()->getStatistics();

    QMutexLocker locker(&m_statsMutex);
    m_stats.hits = unifiedStats.hits.load(std::memory_order_relaxed);
    m_stats.misses = unifiedStats.misses.load(std::memory_order_relaxed);
    m_stats.invalidations = unifiedStats.invalidations.load(std::memory_order_relaxed);
    // Add other statistics as needed
}

void AlphaInstructionCache::warmCache(quint64 startAddress, size_t size)
{
    DEBUG_LOG(
        QString("AlphaInstructionCache: Warming cache for region 0x%1, size %2").arg(startAddress, 0, 16).arg(size));

    quint64 endAddress = startAddress + size;
    size_t lineCount = (size + m_lineSize - 1) / m_lineSize;

    prefetchSequential(startAddress, lineCount);
}

// Add this method to AlphaInstructionCache.h in the public section:

/**
 * @brief Write data to the instruction cache (typically for cache line fills)
 * @param address Physical address to write to
 * @param buffer Data buffer to write from
 * @param size Number of bytes to write
 * @return true if write succeeded, false otherwise
 *
 * Note: This is primarily used for cache line fills from L2/memory, not for
 * normal instruction writes (which would be rare in instruction caches)
 */
// bool AlphaInstructionCache::write(quint64 address, const void *buffer, size_t size);

// Add this implementation to AlphaInstructionCache.cpp:

// bool AlphaInstructionCache::write(quint64 address, const void *buffer, size_t size)
// {
//     if (!buffer || size == 0 || !m_unifiedCache)
//     {
//         return false;
//     }
// 
//     // For instruction caches, writes are typically cache line fills from L2/memory
//     // Use UnifiedDataCache for the actual write operation
//     bool success = m_unifiedCache->write(address, buffer, size);
// 
//     if (success)
//     {
//         DEBUG_LOG("InstructionCache", "Write (line fill): addr=0x%llx, size=%zu", address, size);
// 
//         // Update statistics for cache line fills
//         recordReplacement(); // This counts as a line replacement/fill
// 
//         // Notify about cache line update
//         emit sigStatisticsUpdated();
//     }
//     else
//     {
//         DEBUG_LOG("InstructionCache", "Write failed: addr=0x%llx, size=%zu", address, size);
//     }
// 
//     return success;
// }

// Alternative implementation if you prefer to be more explicit about instruction cache behavior:

bool AlphaInstructionCache::write(quint64 address, const void *buffer, size_t size)
{
    if (!buffer || size == 0 || !getUnifiedCache())
    {
        return false;
    }

    // Instruction caches typically only accept writes for:
    // 1. Cache line fills from lower levels of memory hierarchy
    // 2. Self-modifying code scenarios (rare on Alpha)

    quint64 lineAddr = getLineAddress(address);

    // Validate that this is likely a cache line fill operation
    if (size > m_lineSize)
    {
        DEBUG_LOG("InstructionCache", "Write size %zu exceeds line size %zu", size, m_lineSize);
        return false;
    }

    // Use UnifiedDataCache for the actual write operation
    bool success = getUnifiedCache()->write(address, buffer, size);

    if (success)
    {
        DEBUG_LOG("InstructionCache", "Cache line fill: addr=0x%llx, size=%zu", address, size);

        // Update statistics
        recordReplacement();

        // If this might be self-modifying code, handle coherency
        if (m_memorySystem)
        {
            // Notify other CPU caches about this instruction modification
            m_memorySystem->invalidateCacheLines(lineAddr, m_lineSize, m_cpuId);
        }

        emit sigStatisticsUpdated();
    }

    return success;
}

void AlphaInstructionCache::addHotSpot(quint64 address, size_t size)
{
    m_hotSpots[getLineAddress(address)] = size;
    warmCache(address, size);
}

void AlphaInstructionCache::removeHotSpot(quint64 address) { m_hotSpots.remove(getLineAddress(address)); }

void AlphaInstructionCache::handleCoherencyEvent(quint64 address, const QString &type)
{
    recordCoherencyEvent();

    if (type == "INVALIDATE")
    {
        invalidateLine(address);
    }
    else if (type == "FLUSH")
    {
        flushLine(address);
    }
    else if (type == "SHARED")
    {
        size_t setIndex = getSetIndex(address);
        if (setIndex < m_cacheSets.size())
        {
            CacheSet &set = m_cacheSets[setIndex];
            quint64 tag = getTag(address);
            CacheLine *line = set.findLine(address, tag);
            if (line && line->isValid())
            {
                // Cache line found and valid - coherency state updated
            }
        }
    }
    else if (type.startsWith("MEMORY_WRITE"))
    {
        handleSelfModifyingCode(address);
    }

    emit sigCoherencyEventHandled(address, type);
}

void AlphaInstructionCache::handleSelfModifyingCode(quint64 address)
{
    // Self-modifying code detected - invalidate both cache and TLB
    qDebug() << QString("AlphaInstructionCache: Self-modifying code detected at 0x%1 for CPU %2")
                    .arg(address, 0, 16)
                    .arg(m_cpuId);

    // Invalidate cache line
    invalidateLine(address);

    // The invalidateLine() call above will also handle TLB invalidation
    // through the SMP TLB coordination we added
}

void AlphaInstructionCache::notifyOtherCaches(quint64 address, const QString &type)
{
    if (m_memorySystem)
    {
        DEBUG_LOG(
            QString("AlphaInstructionCache: Notifying other caches about %1 at 0x%2").arg(type).arg(address, 0, 16));
    }
}

void AlphaInstructionCache::setReplacementPolicy(const QString &policy)
{
    if (policy == "LRU" || policy == "LFU" || policy == "RANDOM")
    {
        m_replacementPolicy = policy;
        DEBUG_LOG(QString("AlphaInstructionCache: Replacement policy set to %1").arg(policy));
    }
    else
    {
        WARN_LOG(QString("AlphaInstructionCache: Unknown replacement policy %1").arg(policy));
    }
}

void AlphaInstructionCache::resize(size_t newSize)
{
    WARN_LOG("AlphaInstructionCache: Runtime resize not supported");
    WARN_LOG(
        QString("Current cache: %1KB. To change size, recreate cache with new CacheConfig").arg(m_cacheSize / 1024));
    WARN_LOG(QString("Requested size: %1KB will be ignored").arg(newSize / 1024));

#ifdef DEBUG_BUILD
    DEBUG_LOG("AlphaInstructionCache: Use CacheConfig::fromConfigFile() or");
    DEBUG_LOG("CacheConfig::forCpuModel() to set cache size at initialization");
#endif
}

void AlphaInstructionCache::clear()
{
    invalidateAll();
    clearStatistics();
}

void AlphaInstructionCache::printStatistics() const
{
    auto stats = getStatistics();

    DEBUG_LOG("=== Instruction Cache Statistics ===");
    DEBUG_LOG(QString("Cache Size: %1KB").arg(m_cacheSize / 1024));
    DEBUG_LOG(QString("Line Size: %1 bytes").arg(m_lineSize));
    DEBUG_LOG(QString("Associativity: %1-way").arg(m_associativity));
    DEBUG_LOG(QString("Number of Sets: %1").arg(m_numSets));
    DEBUG_LOG(QString("CPU ID: %1").arg(m_cpuId));
    DEBUG_LOG(QString("Hits: %1").arg(stats.hits));
    DEBUG_LOG(QString("Misses: %1").arg(stats.misses));
    DEBUG_LOG(QString("Hit Rate: %1%").arg(QString::number(stats.getHitRate(), 'f', 2)));
    DEBUG_LOG(QString("Invalidations: %1").arg(stats.invalidations));
    DEBUG_LOG(QString("Prefetches: %1").arg(stats.prefetches));
    DEBUG_LOG(QString("Replacements: %1").arg(stats.replacements));
    DEBUG_LOG(QString("Coherency Events: %1").arg(stats.coherencyEvents));
    DEBUG_LOG(QString("Used Lines: %1/%2").arg(getUsedLines()).arg(getTotalLines()));
}

size_t AlphaInstructionCache::getUsedLines() const
{
    size_t used = 0;
    for (const auto &set : m_cacheSets)
    {
        // Use the high-performance CacheSet utilization method
        used += static_cast<size_t>(set.getUtilization() * set.getAssociativity());
    }
    return used;
}

size_t AlphaInstructionCache::getTotalLines() const { return m_numSets * m_associativity; }

// Private helper methods for compatibility with old CacheSet interface
CacheLine *AlphaInstructionCache::findLine(quint64 address, CacheSet &set)
{
    quint64 tag = getTag(address);
    return set.findLine(address, tag);
}

CacheLine *AlphaInstructionCache::getReplacementLine(CacheSet &set)
{
    // Use the new high-performance CacheSet interface
    quint64 tag = getTag(0);               // Placeholder tag
    return set.getReplacementLine(tag, 0); // Placeholder address
}

void AlphaInstructionCache::updateAccessInfo(CacheLine &line)
{
    // The new CacheSet handles access tracking automatically
    // This method is kept for compatibility but delegates to CacheSet
}

CacheLine *AlphaInstructionCache::selectLRU(CacheSet &set)
{
    // The new CacheSet handles LRU selection internally
    // This method is kept for compatibility
    return set.getReplacementLine(0, 0); // LRU selection is automatic
}

CacheLine *AlphaInstructionCache::selectLFU(CacheSet &set)
{
    // The new CacheSet handles replacement policy internally
    return set.getReplacementLine(0, 0);
}

CacheLine *AlphaInstructionCache::selectRandom(CacheSet &set)
{
    // The new CacheSet handles replacement policy internally
    return set.getReplacementLine(0, 0);
}

void AlphaInstructionCache::checkAutoPrefetch(quint64 address)
{
    if (address == m_lastAccessAddress + 4)
    {
        // Sequential access detected (4-byte instructions)
        m_sequentialCount++;
        if (m_sequentialCount >= 2)
        {
            // Prefetch next line
            prefetchNextLine(address);
        }
    }
    else
    {
        m_sequentialCount = 0;
    }
    m_lastAccessAddress = address;
}

void AlphaInstructionCache::prefetchNextLine(quint64 address)
{
    quint64 nextLineAddr = getLineAddress(address) + m_lineSize;
    prefetch(nextLineAddr);
}