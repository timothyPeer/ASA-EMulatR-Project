#include "UnifiedDataCache.h"
#include "globalMacro.h"
#include <QMetaObject>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include "CacheSet.h"

// Configuration management
void UnifiedDataCache::Config::loadFromSettings(QSettings *settings)
{
    if (!settings)
        return;

    settings->beginGroup("UnifiedDataCache");

    numSets = settings->value("numSets", numSets).toULongLong();
    associativity = settings->value("associativity", associativity).toULongLong();
    lineSize = settings->value("lineSize", lineSize).toULongLong();
    totalSize = settings->value("totalSize", totalSize).toULongLong();
    enablePrefetch = settings->value("enablePrefetch", enablePrefetch).toBool();
    enableStatistics = settings->value("enableStatistics", enableStatistics).toBool();
    enableCoherency = settings->value("enableCoherency", enableCoherency).toBool();
    coherencyProtocol = settings->value("coherencyProtocol", coherencyProtocol).toString();
    statusUpdateInterval = settings->value("statusUpdateInterval", 1000).toUInt(); // Default 1000ms

    settings->endGroup();

    // Validate configuration
    if (!validate())
    {
        DEBUG_LOG(QString("Cache :: Invalid configuration loaded, using defaults"));
        *this = Config{}; // Reset to defaults
    }

    DEBUG_LOG(
        QString("Cache :: Loaded configuration: sets=%1, assoc=%2, lineSize=%3, totalSize=%4, interval=%5")
                .arg(numSets)
                  .arg(associativity)
                  .arg(lineSize)
                  .arg(totalSize)
                  .arg(statusUpdateInterval));
}

void UnifiedDataCache::Config::saveToSettings(QSettings *settings) const
{
    if (!settings)
        return;

    settings->beginGroup("UnifiedDataCache");

    settings->setValue("numSets", static_cast<qulonglong>(numSets));
    settings->setValue("associativity", static_cast<qulonglong>(associativity));
    settings->setValue("lineSize", static_cast<qulonglong>(lineSize));
    settings->setValue("totalSize", static_cast<qulonglong>(totalSize));
    settings->setValue("enablePrefetch", enablePrefetch);
    settings->setValue("enableStatistics", enableStatistics);
    settings->setValue("enableCoherency", enableCoherency);
    settings->setValue("coherencyProtocol", coherencyProtocol);
    settings->setValue("statsUpdateInterval", statusUpdateInterval);
    settings->endGroup();
}

// Constructors
UnifiedDataCache::UnifiedDataCache(const Config &config, QObject *parent)
    : QObject(parent), m_config(config), m_cacheSets(config.numSets)
{
    // Initialize cache sets
    CacheSet::Config setConfig(config.associativity, config.lineSize, config.enablePrefetch, config.enableStatistics);

    for (size_t i = 0; i < m_config.numSets; ++i)
    {
        m_cacheSets[i] = std::make_unique<CacheSet>(setConfig);
    }

    // Setup statistics update timer if enabled
    if (m_config.enableStatistics)
    {
        m_statsTimer = new QTimer(this);
        connect(m_statsTimer, &QTimer::timeout, this, &UnifiedDataCache::onStatsUpdateTimer);
        //m_statsTimer->start(STATS_UPDATE_INTERVAL_MS);
        m_statsTimer->start(m_config.statusUpdateInterval); // we will extract the timer from the .ini file. 
    }

//     DEBUG_LOG("Cache :: Initialized unified data cache: %zu sets, %zu-way associative, %zu byte lines",
//               m_config.numSets, m_config.associativity, m_config.lineSize);
    DEBUG_LOG(QString("Cache :: Initialized unified data cache: %1 sets, %2-way associative, %3 byte lines")
                  .arg(m_config.numSets)
                  .arg(m_config.associativity)
                  .arg(m_config.lineSize));
}

UnifiedDataCache::UnifiedDataCache(QSettings *settings, QObject *parent) : UnifiedDataCache(Config{}, parent)
{
    Config config;
    config.loadFromSettings(settings);

    // Reinitialize with loaded config
    const_cast<Config &>(m_config) = config;

    // Recreate cache sets with new configuration
    m_cacheSets.clear();
    m_cacheSets.resize(config.numSets);

    CacheSet::Config setConfig(config.associativity, config.lineSize, config.enablePrefetch, config.enableStatistics);

    for (size_t i = 0; i < m_config.numSets; ++i)
    {
        m_cacheSets[i] = std::make_unique<CacheSet>(setConfig);
    }

//     DEBUG_LOG("Cache :: Initialized from settings: %zu sets, %zu-way associative", m_config.numSets,
//               m_config.associativity);
    DEBUG_LOG(
        QString("Cache :: Initialized from settings: %1 sets, %2-way associative")
        .arg(m_config.numSets)
        .arg(m_config.associativity));
}

UnifiedDataCache::UnifiedDataCache(UnifiedDataCache &&other) noexcept
    : QObject(other.parent()), m_cacheSets(std::move(other.m_cacheSets)), m_config(other.m_config), m_stats(),
      m_nextLevel(other.m_nextLevel), m_prevLevel(other.m_prevLevel), m_tlbSystem(other.m_tlbSystem),
      m_instructionCache(other.m_instructionCache), m_cpuId(other.m_cpuId), m_globalTime(other.m_globalTime.load()),
      m_statsTimer(other.m_statsTimer)
{
    // Copy atomic statistics
    m_stats.hits.store(other.m_stats.hits.load());
    m_stats.misses.store(other.m_stats.misses.load());
    m_stats.evictions.store(other.m_stats.evictions.load());
    m_stats.invalidations.store(other.m_stats.invalidations.load());
    m_stats.writebacks.store(other.m_stats.writebacks.load());
    m_stats.snoopHits.store(other.m_stats.snoopHits.load());
    m_stats.prefetchHits.store(other.m_stats.prefetchHits.load());
    m_stats.coherencyMisses.store(other.m_stats.coherencyMisses.load());

    // Transfer timer ownership
    if (m_statsTimer)
    {
        m_statsTimer->setParent(this);
    }
    other.m_statsTimer = nullptr;
}

// High-performance read operation
bool UnifiedDataCache::read(quint64 addr, void *buf, size_t size, ReadFunction backingRead)
{
    auto startTime = std::chrono::steady_clock::now();

    quint64 alignedAddr = getAlignedAddress(addr);
    size_t offset = getOffset(addr);
    quint64 tag = getTag(addr);

    // Fast path: try lock-free cache lookup
    CacheSet *cacheSet = getCacheSet(addr);
    CacheLine *line = cacheSet->findLine(alignedAddr, tag);

    if (line)
    {
        // Cache hit - read data directly
        bool success = line->readData(offset, size, buf);
        if (success)
        {
            updateAccessStatistics(true, startTime);
          /*  DEBUG_LOG("Cache :: Read hit: addr=0x%llx, size=%zu", addr, size);*/
            DEBUG_LOG(QString("Cache :: Read hit: addr=0x%1, size=%2").arg(addr, 0,16) .arg(size));
            return true;
        }
    }

    // Cache miss - need to acquire write lock for line replacement
    QWriteLocker locker(&m_hierarchyLock);

    // Try next level cache first
    bool loadSuccess = false;
    if (tryNextLevelRead(alignedAddr, nullptr, m_config.lineSize))
    {
        loadSuccess = true;
        /*DEBUG_LOG("Cache :: Loaded from next level: addr=0x%llx", alignedAddr);*/
        DEBUG_LOG(QString("Cache :: Loaded from next level: addr=0x%1").arg(addr, 0,16));
    }
    else if (backingRead)
    {
        // Allocate temporary buffer for full cache line
        std::vector<uint8_t> lineBuffer(m_config.lineSize);
        if (backingRead(alignedAddr, lineBuffer.data(), m_config.lineSize))
        {
            loadSuccess = true;
   /*         DEBUG_LOG("Cache :: Loaded from backing store: addr=0x%llx", alignedAddr);*/
            DEBUG_LOG(QString("Cache :: Loaded from backing store: addr=0x%1").arg(addr,0,16));
        }
    }

    if (!loadSuccess)
    {
        updateAccessStatistics(false, startTime);
   /*     DEBUG_LOG("Cache :: Read miss - load failed: addr=0x%llx", addr);*/
        DEBUG_LOG(QString("Cache :: Read miss - load failed: addr=0x%1").arg(addr,0,16));
        return false;
    }

    // Get replacement line and install new data
    line = cacheSet->getReplacementLine(tag, alignedAddr);
    if (!line)
    {
        updateAccessStatistics(false, startTime);
        DEBUG_LOG(QString("Cache :: Read miss - no replacement line: addr=0x%1").arg( addr));
        return false;
    }

    // Copy requested data to user buffer
    bool success = line->readData(offset, size, buf);
    updateAccessStatistics(false, startTime);

/*    DEBUG_LOG(QString("Cache :: Read miss resolved: addr=0x%llx, size=%2, success=%3").arg(addr).arg(size).arg(success));*/
    DEBUG_LOG(QString("Cache :: Read miss resolved: addr=0x%1, size=%2, success=%3")
        .arg(addr,0,16)
        .arg(size)
        .arg(success));

    return success;
}

// High-performance write operation
bool UnifiedDataCache::write(quint64 addr, const void *data, size_t size, WriteBackFunction backingWrite)
{
    auto startTime = std::chrono::steady_clock::now();

    quint64 alignedAddr = getAlignedAddress(addr);
    size_t offset = getOffset(addr);
    quint64 tag = getTag(addr);

    // Always acquire write lock for write operations to maintain coherency
    QWriteLocker locker(&m_hierarchyLock);

    CacheSet *cacheSet = getCacheSet(addr);
    CacheLine *line = cacheSet->findLine(alignedAddr, tag);

    if (!line)
    {
        // Cache miss - need to load line first
        bool loadSuccess = false;
        std::vector<uint8_t> lineBuffer(m_config.lineSize, 0);

        // Try next level first
        if (tryNextLevelRead(alignedAddr, lineBuffer.data(), m_config.lineSize))
        {
            loadSuccess = true;
           /* DEBUG_LOG("Cache :: Loaded from next level for write: addr=0x%llx", alignedAddr);*/
            DEBUG_LOG(
                QString("Cache :: Loaded from next level for write: addr=0x%1").arg(addr,0,16));
        }
        // For partial writes, try to load existing data
        else if (offset != 0 || size != m_config.lineSize)
        {
            // This is a partial write - we need the existing data
            if (backingWrite && backingWrite(alignedAddr, lineBuffer.data(), m_config.lineSize))
            {
                loadSuccess = true;
      /*          DEBUG_LOG("Cache :: Loaded from backing store for partial write: addr=0x%llx", addr);*/
                DEBUG_LOG(QString("Cache :: Loaded from backing store for partial write: addr=0x%1")
                              .arg(addr,0,16));
            }
            else
            {
                // If we can't load, zero-fill for partial writes
               /* DEBUG_LOG("Cache :: Partial write with no backing data, zero-filling: addr=0x%llx", addr);*/
                DEBUG_LOG(QString("Cache :: Partial write with no backing data, zero-filling: addr=0x%1")
                              .arg(addr,0,16));
                loadSuccess = true; // Continue with zero-filled buffer
            }
        }
        else
        {
            // Full line write - no need to load existing data
           /* DEBUG_LOG("Cache :: Full line write, no load needed: addr=0x%llx", addr);*/
            DEBUG_LOG(
                QString("Cache :: Full line write, no load needed: addr=0x%1").arg(addr,0,16));
            loadSuccess = true;
        }

        if (!loadSuccess)
        {
            updateAccessStatistics(false, startTime);
           /* DEBUG_LOG("Cache :: Write miss - load failed: addr=0x%llx", addr);*/
            DEBUG_LOG(QString("Cache :: Write miss - load failed: addr=0x%1").arg(addr,0,16));
            return false;
        }

        // Get replacement line
        line = cacheSet->getReplacementLine(tag, alignedAddr);
        if (!line)
        {
            updateAccessStatistics(false, startTime);
            DEBUG_LOG(QString("Cache :: Write miss - no replacement line: addr=0x%1")
                          .arg(addr,0,16));
            return false;
        }

        // If we loaded data, copy it to the cache line first
        if (loadSuccess && (offset != 0 || size != m_config.lineSize))
        {
            // Copy the loaded data to the cache line
            std::memcpy(line->getMutableData(), lineBuffer.data(), m_config.lineSize);
        }
    }

    // Perform the write
    bool success = line->writeData(offset, size, data);
    if (success)
    {
        // Notify about write-back when line becomes dirty
        if (line->isDirty())
        {
            emit sigWriteBack(alignedAddr, m_config.lineSize);
        }

        updateAccessStatistics(line != nullptr, startTime);
       /* DEBUG_LOG("Cache :: Write completed: addr=0x%llx, size=%zu", addr, size);*/
        DEBUG_LOG(
            QString("Cache :: Write completed: addr=0x%1  size=%2")
            .arg(addr,0,16)
            .arg(size)
        );
    }
    else
    {
        updateAccessStatistics(false, startTime);
        DEBUG_LOG(QString("Cache :: Write failed: addr=0x%1 size=%2")
            .arg(addr,0,16)
            .arg( size));

    }

    return success;
}

// Lock-free cache line lookup
CacheLine *UnifiedDataCache::findLine(quint64 addr) noexcept
{
    quint64 alignedAddr = getAlignedAddress(addr);
    quint64 tag = getTag(addr);

    CacheSet *cacheSet = getCacheSet(addr);
    return cacheSet->findLine(alignedAddr, tag);
}

// Cache line invalidation
void UnifiedDataCache::invalidateLine(quint64 physicalAddr)
{
    QWriteLocker locker(&m_hierarchyLock);

    quint64 alignedAddr = getAlignedAddress(physicalAddr);
    CacheSet *cacheSet = getCacheSet(physicalAddr);

    if (cacheSet->invalidateLine(alignedAddr))
    {
        emit sigLineInvalidated(alignedAddr);
        DEBUG_LOG(QString("Cache :: Invalidated line: addr=0x%1").arg(physicalAddr, 0, 16));
    }

    // Propagate to next level
    if (m_nextLevel)
    {
        m_nextLevel->invalidateLine(physicalAddr);
    }
}

// Cache line flush
void UnifiedDataCache::flushLine(quint64 physicalAddr)
{
    QWriteLocker locker(&m_hierarchyLock);

    quint64 alignedAddr = getAlignedAddress(physicalAddr);
    CacheLine *line = findLine(physicalAddr);

    if (line && line->isValid())
    {
        // Write back if dirty
        if (line->isDirty())
        {
            if (performWriteBack(line, alignedAddr, {}))
            {
                line->setDirty(false);
                DEBUG_LOG(QString("Cache :: Flushed dirty line: addr=0x%1")
                              .arg(physicalAddr, 0, 16));
            }
            else
            {
                line->setDirty(false);
                DEBUG_LOG(QString("Cache :: Flush write-back failed: addr=0x%1")
                              .arg(physicalAddr, 0, 16));
            }
        }

        // Invalidate the line
        invalidateLine(physicalAddr);
    }

    // Propagate to next level
    if (m_nextLevel)
    {
        m_nextLevel->flushLine(physicalAddr);
    }
}

// Check if line is dirty
bool UnifiedDataCache::isDirty(quint64 physicalAddr) const
{
    QReadLocker locker(&m_hierarchyLock);

    const CacheLine *line = const_cast<UnifiedDataCache *>(this)->findLine(physicalAddr);
    return line && line->isValid() && line->isDirty();
}

// Mark line as clean
void UnifiedDataCache::markClean(quint64 physicalAddr)
{
    QWriteLocker locker(&m_hierarchyLock);

    CacheLine *line = findLine(physicalAddr);
    if (line && line->isValid())
    {
        line->setDirty(false);
        DEBUG_LOG(
            QString("Cache :: Marked line clean: addr=0x%1").arg(physicalAddr, 0, 16));
    }
}

// SMP cache snooping
void UnifiedDataCache::snoop(quint64 physicalAddr, const QString &operation)
{
    QWriteLocker locker(&m_hierarchyLock);

    CacheLine *line = findLine(physicalAddr);
    if (!line || !line->isValid())
    {
        return; // No action needed for invalid lines
    }

    m_stats.snoopHits.fetch_add(1, std::memory_order_relaxed);

    if (operation == "READ")
    {
        // READ operation - could transition to Shared state in MESI
         DEBUG_LOG(QString("Cache :: Snooped READ: addr=0x%1").arg(physicalAddr, 0, 16));
    }
    else if (operation == "WRITE" || operation == "RFO")
    {
        // WRITE or Read-For-Ownership - invalidate our copy
        quint64 alignedAddr = getAlignedAddress(physicalAddr);
        CacheSet *cacheSet = getCacheSet(physicalAddr);

        if (cacheSet->invalidateLine(alignedAddr))
        {
            emit sigLineInvalidated(alignedAddr);
             DEBUG_LOG(QString("Cache :: Snooped %1, invalidated line: addr=0x%2")
                          .arg(qPrintable(operation))
                          .arg(physicalAddr, 0, 16));
        }
    }
    else if (operation == "INVALIDATE")
    {
        // Explicit invalidation
        invalidateLine(physicalAddr);
  /*      DEBUG_LOG(QString("Cache :: Snooped INVALIDATE: addr=0x%1").arg(physicalAddr, 0, 16));*/
        DEBUG_LOG(QString("Cache :: Snooped INVALIDATE: addr=0x%1").arg(physicalAddr, 0, 16));
    }
    else if (operation == "FLUSH")
    {
        // Flush operation
        flushLine(physicalAddr);
        DEBUG_LOG(QString("Cache :: Snooped FLUSH: addr=0x%1").arg(physicalAddr, 0, 16));
    }
    else
    {
        // Unknown operation
        notifyCoherencyViolation(physicalAddr, operation);
        DEBUG_LOG(QString("Cache :: Unknown snoop operation '%s': addr=0x%llx")
            .arg(qPrintable(operation))
            .arg(physicalAddr, 0, 16));
    }

    // Propagate to next level
    if (m_nextLevel)
    {
        m_nextLevel->snoop(physicalAddr, operation);
    }
}

// Write back specific line
bool UnifiedDataCache::writeBackLine(quint64 physicalAddr, WriteBackFunction backingWrite)
{
    QWriteLocker locker(&m_hierarchyLock);

    CacheLine *line = findLine(physicalAddr);
    if (!line || !line->isValid() || !line->isDirty())
    {
        return true; // Nothing to write back
    }

    quint64 alignedAddr = getAlignedAddress(physicalAddr);
    return performWriteBack(line, alignedAddr, backingWrite);
}

// Write back all dirty lines
bool UnifiedDataCache::writeBackAllDirty(WriteBackFunction backingWrite)
{
    QWriteLocker locker(&m_hierarchyLock);

    bool allSuccess = true;
    size_t totalWriteBackCount = 0;

    for (size_t setIndex = 0; setIndex < m_config.numSets; ++setIndex)
    {
        CacheSet *cacheSet = m_cacheSets[setIndex].get();

        // Get all dirty lines in this set
        auto dirtyLines = cacheSet->getDirtyLines();

        for (const auto &[address, line] : dirtyLines)
        {
            if (performWriteBack(line, address, backingWrite))
            {
                totalWriteBackCount++;
            }
            else
            {
                allSuccess = false;
                DEBUG_LOG(QString("Cache :: Write-back failed for line: addr=0x%1")
                              .arg(address, 0, 16));
            }
        }
    }

    if (totalWriteBackCount > 0)
    {
        DEBUG_LOG(QString("Cache :: Wrote back %1 dirty lines, success=%2").arg(totalWriteBackCount).arg( allSuccess));
    }

    return allSuccess;
}

// Cache containment check
bool UnifiedDataCache::contains(quint64 addr) const
{
    const CacheLine *line = const_cast<UnifiedDataCache *>(this)->findLine(addr);
    return line && line->isValid();
}

// Remove cache line with optional write-back
bool UnifiedDataCache::remove(quint64 addr, WriteBackFunction backingWrite)
{
    QWriteLocker locker(&m_hierarchyLock);

    CacheLine *line = findLine(addr);
    if (!line || !line->isValid())
    {
        return false; // Line not found
    }

    quint64 alignedAddr = getAlignedAddress(addr);
    bool wasDirty = line->isDirty();

    // Write back if dirty
    if (wasDirty && !performWriteBack(line, alignedAddr, backingWrite))
    {
        DEBUG_LOG(QString("Cache :: Remove failed - write-back error: addr=0x%1")
                      .arg(addr,0,16));
        return false;
    }

    // Invalidate the line
    line->setValid(false);
    line->setDirty(false);
    line->clear();

    emit sigLineEvicted(alignedAddr, wasDirty);
    DEBUG_LOG(QString("Cache :: Removed line: addr=0x%1, was_dirty=%2")
                  .arg(addr,0,16)
                  .arg(wasDirty));

    return true;
}

// Invalidate all cache lines
void UnifiedDataCache::invalidateAll()
{
    QWriteLocker locker(&m_hierarchyLock);

    size_t invalidatedCount = 0;

    for (auto &cacheSet : m_cacheSets)
    {
        // Invalidate all lines in this set
        for (size_t way = 0; way < m_config.associativity; ++way)
        {
            // This requires extending CacheSet to support bulk invalidation
            // For now, we'll use the existing invalidateAll method if available
        }
        cacheSet->invalidateAll();
        invalidatedCount += m_config.associativity;
    }

    DEBUG_LOG(QString("Cache :: Invalidated all cache lines: %zu lines").arg(invalidatedCount));
    emit sigStatsChanged();
}

// Flush entire cache
void UnifiedDataCache::flush()
{
    QWriteLocker locker(&m_hierarchyLock);

    // Write back all dirty lines first
    writeBackAllDirty({});

    // Then invalidate all
    invalidateAll();

    DEBUG_LOG("Cache :: Flushed entire cache");
}

// Cache hierarchy management
void UnifiedDataCache::setNextLevel(UnifiedDataCache *nextLevel)
{
    QWriteLocker locker(&m_hierarchyLock);

    m_nextLevel = nextLevel;
    if (nextLevel)
    {
        nextLevel->m_prevLevel = this;
    }

    emit sigHierarchyChanged(nextLevel);
    DEBUG_LOG(QString("Cache :: Set next level cache: %1").arg(reinterpret_cast<quintptr>(nextLevel), 0, 16));
}

void UnifiedDataCache::setPrevLevel(UnifiedDataCache *prevLevel)
{
    QWriteLocker locker(&m_hierarchyLock);

    m_prevLevel = prevLevel;
    DEBUG_LOG(QString("Cache :: Set previous level cache: %1").arg(reinterpret_cast<quintptr>(prevLevel), 0, 16));
}

// Integration system setup
void UnifiedDataCache::setTLBSystem(TLBSystem *tlb, uint16_t cpuId)
{
    m_tlbSystem = tlb;
    m_cpuId = cpuId;

    // Configure all cache sets with TLB integration
    for (auto &cacheSet : m_cacheSets)
    {
        cacheSet->setTLBSystem(tlb, cpuId);
    }

    DEBUG_LOG(QString("Cache :: Set TLB system: %1, CPU ID: %2").arg(reinterpret_cast<quintptr>(tlb), 0, 16).arg(cpuId));
}

void UnifiedDataCache::setInstructionCache(AlphaInstructionCache *icache)
{
    m_instructionCache = icache;

    // Configure all cache sets with instruction cache integration
    for (auto &cacheSet : m_cacheSets)
    {
        cacheSet->setInstructionCache(icache);
    }

    DEBUG_LOG(QString("Cache :: Set instruction cache: %1").arg(reinterpret_cast<quintptr>(icache), 0, 16) );
}

// Add this method to UnifiedDataCache class
UnifiedDataCache *UnifiedDataCache::getLevel1DataCache() const
{
    // This should return L1 data cache if this cache represents one
    // For now, return nullptr - proper implementation depends on cache hierarchy design
    return nullptr;
}

// Enhanced getCacheSet method for better error handling
CacheSet *UnifiedDataCache::getCacheSet(quint64 addr)
{
    size_t index = getIndex(addr);
    if (index >= m_cacheSets.size())
    {
        ERROR_LOG(QString("Error-Cache :: Invalid cache set index: %1 (max: %2)").arg(index).arg(m_cacheSets.size()));
  /*      ERROR_LOG(QString("Error-Cache :: Cache-to-cache transfer from next level: addr=0x%1")*/
  /*                    .arg(reinterpret_cast<quintptr>(alignedAddr), 0, 16));*/
        return nullptr;
    }
    return m_cacheSets[index].get();
}


// Statistics and monitoring
UnifiedDataCache::StatisticsSnapshot UnifiedDataCache::getStatistics() const
{
    StatisticsSnapshot stats{};

    // Aggregate statistics from all cache sets
    for (const auto &cacheSet : m_cacheSets)
    {
        CacheSet::Statistics setStats = cacheSet->getStatistics();
        stats.hits += setStats.hits.load(std::memory_order_relaxed);
        stats.misses += setStats.misses.load(std::memory_order_relaxed);
        stats.evictions += setStats.evictions.load(std::memory_order_relaxed);
        stats.invalidations += setStats.invalidations.load(std::memory_order_relaxed);
    }

    // Add cache-level statistics
    stats.writebacks = m_stats.writebacks.load(std::memory_order_relaxed);
    stats.snoopHits = m_stats.snoopHits.load(std::memory_order_relaxed);
    stats.prefetchHits = m_stats.prefetchHits.load(std::memory_order_relaxed);
    stats.coherencyMisses = m_stats.coherencyMisses.load(std::memory_order_relaxed);
    stats.totalAccessTime = m_stats.totalAccessTime.load(std::memory_order_relaxed);
    stats.accessCount = m_stats.accessCount.load(std::memory_order_relaxed);

    return stats;
}

void UnifiedDataCache::clearStatistics()
{
    QWriteLocker locker(&m_hierarchyLock);

    // Clear statistics in all cache sets
    for (auto &cacheSet : m_cacheSets)
    {
        cacheSet->clearStatistics();
    }

    // Clear cache-level statistics
    m_stats.hits.store(0, std::memory_order_relaxed);
    m_stats.misses.store(0, std::memory_order_relaxed);
    m_stats.evictions.store(0, std::memory_order_relaxed);
    m_stats.invalidations.store(0, std::memory_order_relaxed);
    m_stats.writebacks.store(0, std::memory_order_relaxed);
    m_stats.snoopHits.store(0, std::memory_order_relaxed);
    m_stats.prefetchHits.store(0, std::memory_order_relaxed);
    m_stats.coherencyMisses.store(0, std::memory_order_relaxed);
    m_stats.totalAccessTime.store(0, std::memory_order_relaxed);
    m_stats.accessCount.store(0, std::memory_order_relaxed);

    emit sigStatsChanged();
    DEBUG_LOG("Cache :: Cleared all statistics");
}

double UnifiedDataCache::getUtilization() const
{
    size_t totalLines = m_config.numSets * m_config.associativity;
    size_t validLines = 0;

    for (const auto &cacheSet : m_cacheSets)
    {
        validLines += static_cast<size_t>(cacheSet->getUtilization() * m_config.associativity);
    }

    return totalLines > 0 ? static_cast<double>(validLines) / totalLines : 0.0;
}

// Private helper methods
bool UnifiedDataCache::tryNextLevelRead(quint64 alignedAddr, void *buffer, size_t size)
{
    if (!m_nextLevel)
    {
        return false;
    }

    // Check if next level contains the line
    if (m_nextLevel->contains(alignedAddr))
    {
        // Create temporary buffer if not provided
        std::vector<uint8_t> tempBuffer;
        void *readBuffer = buffer;

        if (!readBuffer)
        {
            tempBuffer.resize(size);
            readBuffer = tempBuffer.data();
        }

        // Read from next level (cache-to-cache transfer)
        bool success = m_nextLevel->read(alignedAddr, readBuffer, size, {});

        if (success)
        {
        /*    DEBUG_LOG(QString("Cache :: Cache-to-cache transfer from next level: addr=0x%llx").arg( alignedAddr));*/
            DEBUG_LOG(QString("Cache :: Cache-to-cache transfer from next level: addr=0x%1")
                      .arg(alignedAddr, 0, 16));
        }

        return success;
    }

    return false;
}

bool UnifiedDataCache::performWriteBack(CacheLine *line, quint64 address, WriteBackFunction backingWrite)
{
    if (!line || !line->isDirty())
    {
        return true; // Nothing to write back
    }

    // Try next level first
    if (m_nextLevel)
    {
        bool success = m_nextLevel->write(address, line->getData(), line->getSize(), {});
        if (success)
        {
            line->setDirty(false);
            m_stats.writebacks.fetch_add(1, std::memory_order_relaxed);
            emit sigWriteBack(address, line->getSize());
     /*       DEBUG_LOG(QString("Cache :: Wrote back to next level: addr=0x%llx").arg( address));*/
            DEBUG_LOG(QString("Cache :: Wrote back to next level: addr=0x%1")
                          .arg(address, 0, 16));
            return true;
        }
    }

    // Fall back to backing store
    if (backingWrite && backingWrite(address, line->getData(), line->getSize()))
    {
        line->setDirty(false);
        m_stats.writebacks.fetch_add(1, std::memory_order_relaxed);
        emit sigWriteBack(address, line->getSize());

        DEBUG_LOG(QString("Cache :: Wrote back to backing store: addr=0x%1")
                      .arg(address, 0, 16));
        return true;
    }

    DEBUG_LOG(QString("Cache :: Write-back failed: addr=0x%llx").arg(address));
    return false;
}

void UnifiedDataCache::updateAccessStatistics(bool hit, TimePoint startTime)
{
    if (!m_config.enableStatistics)
    {
        return;
    }

    // Update hit/miss counters
    if (hit)
    {
        m_stats.hits.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        m_stats.misses.fetch_add(1, std::memory_order_relaxed);
    }

    // Update timing statistics
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

    m_stats.totalAccessTime.fetch_add(duration.count(), std::memory_order_relaxed);
    m_stats.accessCount.fetch_add(1, std::memory_order_relaxed);
}

void UnifiedDataCache::notifyCoherencyViolation(quint64 address, const QString &operation)
{
    m_stats.coherencyMisses.fetch_add(1, std::memory_order_relaxed);
    emit sigCoherencyViolation(address, operation);
    DEBUG_LOG(QString("Cache :: Coherency violation: addr=0x%1, operation=%2")
        .arg( address)
        .arg(operation));
}

// Private slots
void UnifiedDataCache::onStatsUpdateTimer()
{
    // Periodically emit statistics changes for monitoring
    emit sigStatsChanged();
}

