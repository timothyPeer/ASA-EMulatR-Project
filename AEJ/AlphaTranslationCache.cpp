#include "AlphaTranslationCache.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QFuture>
#include <QThread>
#include <QtConcurrent>
#include "GlobalMacro.h"

AlphaTranslationCache::AlphaTranslationCache(quint64 sets, quint64 ways, quint64 pageSize)
    : m_sets(sets)
	, m_ways(ways)
	, m_pageSize(pageSize)
	, m_pageMask(~(pageSize - 1))
	, m_setMask(sets - 1)
	, m_setShift(static_cast<quint64>(std::log2(pageSize)))
	, m_maxSets(sets * 4) // Allow 4x expansion
    , m_maxWays(ways * 2) // Allow 2x expansion
    , m_activeSets(sets), m_activeWays(ways)
{
	// Ensure sets and pageSize are powers of 2
	Q_ASSERT((sets & (sets - 1)) == 0 && "Sets must be power of 2");
	Q_ASSERT((pageSize & (pageSize - 1)) == 0 && "Page size must be power of 2");

	// Initialize cache storage
	m_cache.resize(m_maxSets);
	m_lruCounters.resize(m_maxSets);
	m_globalCounter.resize(m_maxSets);

	for (quint64 set = 0; set < m_maxSets; ++set)
        {
		  m_cache[set].resize(m_maxWays);
          m_lruCounters[set].resize(m_maxWays);
		  m_globalCounter[set] = 0;

		// Initialize all ways as invalid
          for (quint64 way = 0; way < m_maxWays; ++way)
          {
			m_cache[set][way].isValid = false;
			m_lruCounters[set][way] = 0;
		  }
	}

  DEBUG_LOG(QString("AlphaTranslationCache: Initialized %1/%2 sets x %3/%4 ways")
                      .arg(m_activeSets)
                      .arg(m_maxSets)
                      .arg(m_activeWays)
                      .arg(m_maxWays));
}

/* consTblPipeLine::constTuningOperationThrottle */
void AlphaTranslationCache::autoTune()
{
    // Only tune every 10000 operations to avoid overhead - //TODO we should make configurable
    if (m_autoTuneCounter.fetchAndAddRelaxed(1) % constTuningOperationThrottle != 0) 
    {
        return;
    }

    Statistics stats = getStatistics();
    quint64 contentions = m_lockContentionCounter.loadRelaxed();

    qDebug() << QString("TLB AutoTune: Hit Rate=%.2f%%, Contentions=%1, Sets=%2, Ways=%3")
                    .arg(contentions)
                    .arg(m_activeSets)
                    .arg(m_activeWays)
                    .arg(stats.hitRate() * 100.0);

    // Decision logic for tuning
    bool needsMoreCapacity = stats.hitRate() < 0.85 && stats.lookups > 1000;
    bool hasHighContention = contentions > 1000;
    bool hasLowUtilization = stats.hitRate() > 0.98 && m_activeWays > 4;

    if (needsMoreCapacity)
    {
        if (m_activeSets < m_maxSets)
        {
            expandSets();
        }
        else if (m_activeWays < m_maxWays)
        {
            expandWays();
        }
    }

    if (hasHighContention)
    {
        if (!m_partitioningEnabled)
        {
            enablePartitioning();
        }
        else if (m_activeWays > 4)
        {
            reduceWays(); // Faster lookups
        }
    }

    if (hasLowUtilization)
    {
        reduceWays();
    }

    // Reset contention counter
    m_lockContentionCounter.storeRelaxed(0);
}

bool AlphaTranslationCache::expandSets()
{
    QWriteLocker locker(&m_lock);

    if (m_activeSets >= m_maxSets)
    {
        qWarning() << "Cannot expand sets: already at maximum" << m_maxSets;
        return false;
    }

    quint64 newSets = qMin(m_activeSets * 2, m_maxSets);
    quint64 oldSets = m_activeSets;

    qDebug() << QString("TLB: Expanding sets from %1 to %2").arg(oldSets).arg(newSets);

    // Update active set count
    m_activeSets = newSets;
    m_setMask = newSets - 1;

    // Rehash existing entries to new set distribution
    QVector<TranslationCacheEntry> validEntries;

    // Collect all valid entries
    for (quint64 set = 0; set < oldSets; ++set)
    {
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            if (m_cache[set][way].isValid)
            {
                validEntries.append(m_cache[set][way]);
                m_cache[set][way].isValid = false; // Clear old location
            }
        }
    }

    // Clear LRU counters for expanded range
    for (quint64 set = 0; set < newSets; ++set)
    {
        m_globalCounter[set] = 0;
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            m_lruCounters[set][way] = 0;
        }
    }

    // Reinsert entries into expanded cache
    for (const auto &entry : validEntries)
    {
        quint64 newSet = getSetIndex(entry.virtualAddress);

        // Find empty way in new set
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            if (!m_cache[newSet][way].isValid)
            {
                m_cache[newSet][way] = entry;
                updateLRU(newSet, way);
                break;
            }
        }
    }

    qDebug()
        << QString("TLB: Successfully expanded to %1 sets, rehashed %2 entries").arg(newSets).arg(validEntries.size());
    return true;
}

bool AlphaTranslationCache::expandWays()
{
    QWriteLocker locker(&m_lock);

    if (m_activeWays >= m_maxWays)
    {
        qWarning() << "Cannot expand ways: already at maximum" << m_maxWays;
        return false;
    }

    quint64 newWays = qMin(m_activeWays * 2, m_maxWays);
    quint64 oldWays = m_activeWays;

    qDebug() << QString("TLB: Expanding ways from %1 to %2").arg(oldWays).arg(newWays);

    // Update active way count
    m_activeWays = newWays;

    // Initialize new ways as invalid
    for (quint64 set = 0; set < m_activeSets; ++set)
    {
        for (quint64 way = oldWays; way < newWays; ++way)
        {
            m_cache[set][way].isValid = false;
            m_lruCounters[set][way] = 0;
        }
    }

    qDebug() << QString("TLB: Successfully expanded to %1 ways").arg(newWays);
    return true;
}

bool AlphaTranslationCache::reduceWays()
{
    QWriteLocker locker(&m_lock);

    if (m_activeWays <= 2)
    {
        qWarning() << "Cannot reduce ways: already at minimum (2)";
        return false;
    }

    quint64 newWays = m_activeWays / 2;
    quint64 oldWays = m_activeWays;

    qDebug() << QString("TLB: Reducing ways from %1 to %2").arg(oldWays).arg(newWays);

    // Invalidate entries in ways being removed
    int evicted = 0;
    for (quint64 set = 0; set < m_activeSets; ++set)
    {
        for (quint64 way = newWays; way < oldWays; ++way)
        {
            if (m_cache[set][way].isValid)
            {
                m_cache[set][way].isValid = false;
                evicted++;
            }
        }
    }

    // Update active way count
    m_activeWays = newWays;

    qDebug() << QString("TLB: Successfully reduced to %1 ways, evicted %2 entries").arg(newWays).arg(evicted);

    m_evictions.fetchAndAddRelaxed(evicted);
    return true;
}

void AlphaTranslationCache::enablePartitioning()
{
    QWriteLocker locker(&m_lock);

    if (m_partitioningEnabled)
    {
        qDebug() << "TLB: Partitioning already enabled";
        return;
    }

    // Start with 4 partitions, can grow to MAX_PARTITIONS
    m_currentPartitions = 4;

    qDebug() << QString("TLB: Enabling partitioning with %1 partitions").arg(m_currentPartitions);

    // Initialize partitioned storage
    m_partitionedCache.resize(m_currentPartitions);
    m_partitionedLRU.resize(m_currentPartitions);

    quint64 setsPerPartition = m_activeSets / m_currentPartitions;

    for (int partition = 0; partition < m_currentPartitions; ++partition)
    {
        m_partitionedCache[partition].resize(setsPerPartition);
        m_partitionedLRU[partition].resize(setsPerPartition);

        for (quint64 set = 0; set < setsPerPartition; ++set)
        {
            m_partitionedCache[partition][set].resize(m_activeWays);
            m_partitionedLRU[partition][set].resize(m_activeWays);

            // Initialize as invalid
            for (quint64 way = 0; way < m_activeWays; ++way)
            {
                m_partitionedCache[partition][set][way].isValid = false;
                m_partitionedLRU[partition][set][way] = 0;
            }
        }
    }

    // Migrate existing entries to partitioned structure
    int migrated = 0;
    for (quint64 set = 0; set < m_activeSets; ++set)
    {
        int targetPartition = set % m_currentPartitions;
        quint64 targetSet = set / m_currentPartitions;

        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            if (m_cache[set][way].isValid)
            {
                m_partitionedCache[targetPartition][targetSet][way] = m_cache[set][way];
                m_partitionedLRU[targetPartition][targetSet][way] = m_lruCounters[set][way];
                migrated++;
            }
        }
    }

    m_partitioningEnabled = true;

    qDebug() << QString("TLB: Partitioning enabled, migrated %1 entries to %2 partitions")
                    .arg(migrated)
                    .arg(m_currentPartitions);
}

void AlphaTranslationCache::disablePartitioning()
{
    QWriteLocker locker(&m_lock);

    if (!m_partitioningEnabled)
    {
        qDebug() << "TLB: Partitioning already disabled";
        return;
    }

    qDebug() << "TLB: Disabling partitioning";

    // Migrate entries back to unified cache
    int migrated = 0;
    quint64 setsPerPartition = m_activeSets / m_currentPartitions;

    // Clear unified cache first
    for (quint64 set = 0; set < m_activeSets; ++set)
    {
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            m_cache[set][way].isValid = false;
        }
    }

    // Migrate from partitions back to unified
    for (int partition = 0; partition < m_currentPartitions; ++partition)
    {
        for (quint64 set = 0; set < setsPerPartition; ++set)
        {
            quint64 unifiedSet = (partition * setsPerPartition) + set;
            if (unifiedSet >= m_activeSets)
                continue;

            for (quint64 way = 0; way < m_activeWays; ++way)
            {
                if (m_partitionedCache[partition][set][way].isValid)
                {
                    m_cache[unifiedSet][way] = m_partitionedCache[partition][set][way];
                    m_lruCounters[unifiedSet][way] = m_partitionedLRU[partition][set][way];
                    migrated++;
                }
            }
        }
    }

    // Clear partitioned storage
    m_partitionedCache.clear();
    m_partitionedLRU.clear();
    m_partitioningEnabled = false;
    m_currentPartitions = 1;

    qDebug() << QString("TLB: Partitioning disabled, migrated %1 entries back to unified cache").arg(migrated);
}

AlphaTranslationCache::TuningStats AlphaTranslationCache::getTuningStats() const
{
    QReadLocker locker(&m_lock);

    TuningStats stats;
    stats.lockContentions = m_lockContentionCounter.loadRelaxed();
    stats.expansions = 0; // Could add counters for these
    stats.reductions = 0;
    stats.partitioningActive = m_partitioningEnabled;
    stats.currentPartitions = m_currentPartitions;
    stats.activeSets = m_activeSets;
    stats.activeWays = m_activeWays;

    return stats;
}

bool AlphaTranslationCache::contains(quint64 virtualAddress, quint64 asn) const
{
    // More efficient version that doesn't need the physical address
    const quint64 pageAddr = getPageAddress(virtualAddress);
    const quint64 setIndex = getSetIndex(pageAddr);

    QReadLocker locker(&m_lock);

    for (quint64 way = 0; way < m_activeWays; ++way)
    {
        const auto &entry = m_cache[setIndex][way];
        if (entry.isValid && entry.virtualAddress == pageAddr && entry.asn == asn)
        {
            return true;
        }
    }
    return false;
}

void AlphaTranslationCache::invalidateAll()
{
    QWriteLocker locker(&m_lock);

    // FIXED: Use m_activeSets and m_activeWays instead of m_sets/m_ways
    for (quint64 set = 0; set < m_activeSets; ++set)
    {
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            m_cache[set][way].isValid = false;
        }
        m_globalCounter[set] = 0;
    }

    m_invalidations.fetchAndAddRelaxed(m_activeSets * m_activeWays);
    qDebug() << "AlphaTranslationCache: Invalidated all entries";
}

void AlphaTranslationCache::invalidateASN(quint64 asn)
{
	QWriteLocker locker(&m_lock);

	int invalidated = 0;
        for (quint64 set = 0; set < m_activeSets; ++set)
        {
            for (quint64 way = 0; way < m_activeWays; ++way)
            {
			auto& entry = m_cache[set][way];
			if (entry.isValid && entry.asn == asn) {
				entry.isValid = false;
				invalidated++;
			}
		}
	}

	m_invalidations.fetchAndAddRelaxed(invalidated);
	qDebug() << QString("AlphaTranslationCache: Invalidated %1 entries for ASN %2")
		.arg(invalidated).arg(asn);
}

void AlphaTranslationCache::invalidateAddress(quint64 virtualAddress, quint64 asn)
{
	QWriteLocker locker(&m_lock);

	const quint64 pageAddr = getPageAddress(virtualAddress);
	const quint64 setIndex = getSetIndex(pageAddr);

	int invalidated = 0;
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
		auto& entry = m_cache[setIndex][way];
		if (entry.isValid &&
			entry.virtualAddress == pageAddr &&
			(asn == 0 || entry.asn == asn)) {
			entry.isValid = false;
			invalidated++;
		}
	}

	m_invalidations.fetchAndAddRelaxed(invalidated);
	qDebug() << QString("AlphaTranslationCache: Invalidated %1 entries for VA 0x%2")
		.arg(invalidated).arg(virtualAddress, 0, 16);
}

void AlphaTranslationCache::invalidateInstructionEntries(bool isInstruction)
{
	QWriteLocker locker(&m_lock);

	int invalidated = 0;
	for (quint64 set = 0; set < m_activeSets; ++set) {
            for (quint64 way = 0; way < m_activeWays; ++way)
            {
			auto& entry = m_cache[set][way];
			if (entry.isValid && entry.isInstruction == isInstruction) {
				entry.isValid = false;
				invalidated++;
			}
		}
	}

	m_invalidations.fetchAndAddRelaxed(invalidated);
	qDebug() << QString("AlphaTranslationCache: Invalidated %1 %2 entries")
		.arg(invalidated)
		.arg(isInstruction ? "instruction" : "data");
}

AlphaTranslationCache::Statistics AlphaTranslationCache::getStatistics() const
{
	QReadLocker locker(&m_lock);

	Statistics stats;
	stats.lookups = m_lookups.loadRelaxed();
	stats.hits = m_hits.loadRelaxed();
	stats.misses = m_misses.loadRelaxed();
	stats.insertions = m_insertions.loadRelaxed();
	stats.evictions = m_evictions.loadRelaxed();
	stats.invalidations = m_invalidations.loadRelaxed();

	return stats;
}

void AlphaTranslationCache::resetStatistics()
{
	QWriteLocker locker(&m_lock);

	m_lookups.storeRelaxed(0);
	m_hits.storeRelaxed(0);
	m_misses.storeRelaxed(0);
	m_insertions.storeRelaxed(0);
	m_evictions.storeRelaxed(0);
	m_invalidations.storeRelaxed(0);
}

// Private methods

quint64 AlphaTranslationCache::getSetIndex(quint64 virtualAddress) const
{
	return (virtualAddress >> m_setShift) & m_setMask;
}

quint64 AlphaTranslationCache::getPageAddress(quint64 address) const
{
	return address & m_pageMask;
}

quint64 AlphaTranslationCache::findLRUWay(quint64 set) const
{
	quint64 lruWay = 0;
	quint64 minCounter = m_lruCounters[set][0];

	for (quint64 way = 1; way < m_activeWays; ++way) {
		if (m_lruCounters[set][way] < minCounter) {
			minCounter = m_lruCounters[set][way];
			lruWay = way;
		}
	}

	return lruWay;
}

void AlphaTranslationCache::updateLRU(quint64 set, quint64 way)
{
	m_lruCounters[set][way] = ++m_globalCounter[set];
}

/**
 * @brief Internal invalidation helper
 * @param pred Predicate function for conditional invalidation
 */
template<typename Predicate>
void AlphaTranslationCache::invalidateIf(Predicate pred)
{
	QWriteLocker locker(&m_lock);
	int invalidated = 0;
	for (quint64 set = 0; set < m_activeSets; ++set) {
		for (quint64 way = 0; way < m_activeWays; ++way) {
			auto& entry = m_cache[set][way];
			if (entry.isValid && pred(entry)) {
				entry.isValid = false;
				invalidated++;
			}
		}
	}
	m_invalidations.fetchAndAddRelaxed(invalidated);
}

// 
QFuture<bool> AlphaTranslationCache::lookupAsync(quint64 virtualAddress, quint64 asn, bool isKernel,
                                                 bool isInstruction) const
{
    // Capture this pointer correctly for the lambda
    return QtConcurrent::run(
        [this, virtualAddress, asn, isKernel, isInstruction]() -> bool
        {
            quint64 physicalAddress = 0;
            return lookupLockFree(virtualAddress, asn, isKernel, isInstruction, physicalAddress);
        });
}

// 
bool AlphaTranslationCache::lookupLockFree(quint64 virtualAddress, quint64 asn, bool isKernel, bool isInstruction,
                                           quint64 &physicalAddress) const
{
    // Lock-free lookup using atomic reads
    const quint64 pageAddr = getPageAddress(virtualAddress);

    // Calculate set index
    const quint64 setIndex = (pageAddr >> m_setShift) & m_setMask;

    // Ensure we don't go out of bounds
    if (setIndex >= m_activeSets)
    {
        return false;
    }

    // Search ways without taking locks
    for (quint64 way = 0; way < m_activeWays; ++way)
    {
        const auto &entry = m_cache[setIndex][way];

        // Atomic-like read of entry (Qt containers are not truly lock-free,
        // but this minimizes lock time)
        if (entry.isValid && entry.virtualAddress == pageAddr && entry.asn == asn && entry.isKernel == isKernel &&
            entry.isInstruction == isInstruction)
        {

            const quint64 pageOffset = virtualAddress & (~m_pageMask);
            physicalAddress = entry.physicalAddress | pageOffset;

            // Update statistics atomically
            m_lookups.fetchAndAddRelaxed(1);
            m_hits.fetchAndAddRelaxed(1);
            return true;
        }
    }

    m_lookups.fetchAndAddRelaxed(1);
    m_misses.fetchAndAddRelaxed(1);
    return false;
}

// 
bool AlphaTranslationCache::lookup(quint64 virtualAddress, quint64 asn, bool isKernel, bool isInstruction,
                                   quint64 &physicalAddress) const // 
{
    m_lockContentionCounter.fetchAndAddRelaxed(1);
    QReadLocker locker(&m_lock);
    m_lookups.fetchAndAddRelaxed(1);

    const quint64 pageAddr = getPageAddress(virtualAddress);

    if (m_partitioningEnabled)
    {
        //int partition = QThread::currentThreadId() % m_currentPartitions;
        //int partition = static_cast<quintptr>(QThread::currentThreadId()) % m_currentPartitions;
        int partition = qHash(QThread::currentThreadId()) % m_currentPartitions;
        quint64 setsPerPartition = m_activeSets / m_currentPartitions;
        quint64 setIndex = (pageAddr >> m_setShift) % setsPerPartition;

        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            const auto &entry = m_partitionedCache[partition][setIndex][way];
            if (entry.isValid && entry.virtualAddress == pageAddr && entry.asn == asn && entry.isKernel == isKernel &&
                entry.isInstruction == isInstruction)
            {

                m_hits.fetchAndAddRelaxed(1);
                const quint64 pageOffset = virtualAddress & (~m_pageMask);
                physicalAddress = entry.physicalAddress | pageOffset; // ? FIXED: Now assigns to reference
                return true;
            }
        }
    }
    else
    {
        const quint64 setIndex = getSetIndex(pageAddr);
        for (quint64 way = 0; way < m_activeWays; ++way)
        {
            const auto &entry = m_cache[setIndex][way];
            if (entry.isValid && entry.virtualAddress == pageAddr && entry.asn == asn && entry.isKernel == isKernel &&
                entry.isInstruction == isInstruction)
            {

                const_cast<AlphaTranslationCache *>(this)->updateLRU(setIndex, way);
                m_hits.fetchAndAddRelaxed(1);
                const quint64 pageOffset = virtualAddress & (~m_pageMask);
                physicalAddress = entry.physicalAddress | pageOffset; // ? FIXED: Now assigns to reference
                return true;
            }
        }
    }

    m_misses.fetchAndAddRelaxed(1);
    return false;
}

//
void AlphaTranslationCache::insert(quint64 virtualAddress, quint64 physicalAddress, quint64 asn, quint16 protectionBits,
                                   bool isKernel, bool isInstruction)
{
    QWriteLocker locker(&m_lock);

    const quint64 pageAddr = getPageAddress(virtualAddress);
    const quint64 physPageAddr = getPageAddress(physicalAddress);
    const quint64 setIndex = getSetIndex(pageAddr);

    // Find empty way or LRU way
    quint64 targetWay = m_activeWays; // ? FIXED: Use m_activeWays

    // First, look for an invalid entry
    for (quint64 way = 0; way < m_activeWays; ++way)
    { // ? FIXED: Use m_activeWays
        if (!m_cache[setIndex][way].isValid)
        {
            targetWay = way;
            break;
        }
    }

    // If no invalid entry, find LRU
    if (targetWay == m_activeWays)
    { // ? FIXED: Use m_activeWays
        targetWay = findLRUWay(setIndex);
        m_evictions.fetchAndAddRelaxed(1);
    }

    // Insert/update entry
    auto &entry = m_cache[setIndex][targetWay];
    entry.virtualAddress = pageAddr;
    entry.physicalAddress = physPageAddr;
    entry.asn = asn;
    entry.protectionBits = protectionBits;
    entry.isValid = true;
    entry.isKernel = isKernel;
    entry.isInstruction = isInstruction;
    entry.accessCount = 1;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();

    updateLRU(setIndex, targetWay);
    m_insertions.fetchAndAddRelaxed(1);
}

// 
void AlphaTranslationCache::processBatch(const TLBBatch &batch)
{
    // Process multiple lookups efficiently
    for (int i = 0; i < batch.virtualAddresses.size(); ++i)
    {
        quint64 physicalAddress = 0;
        bool found = lookup(batch.virtualAddresses[i], 1, false, false, physicalAddress);
        // Store results (this is a simplified version)
        if (found)
        {
            const_cast<TLBBatch &>(batch).results.append(physicalAddress);
        }
    }
}