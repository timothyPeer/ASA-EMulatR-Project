// File: TLBSystem.cpp
#include "TLBSystem.h"
#include <string>
#include <QReadLocker>
#include <QVector>
#include <QString>
#include <QWriteLocker>
#include <QHash>
#include "GlobalMacro.h"
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutex>
#include <QMutexLocker>

#ifdef ALPHA_BUILD
  #pragma message(">>> ALPHA_BUILD is DEFINED")
#else
  #pragma message(">>> ALPHA_BUILD is UNDEFINED")
#endif


TLBSystem::TLBSystem(int capacity, quint16 maxCpus)
    : m_tlbCapacity(capacity), m_maxCpus(maxCpus), m_translationCache(nullptr), m_instructionCache(nullptr)
{
    // Don't pre-allocate CPU TLBs - create them on demand when CPUs register
    DEBUG_LOG(QString("TLBSystem: Initialized with capacity=%1, maxCpus=%2").arg(capacity).arg(maxCpus));
}

TLBSystem::~TLBSystem()
{
    QWriteLocker locker(&m_cpuTLBLock);
    m_cpuTLBMap.clear();
    DEBUG_LOG("TLBSystem: Destroyed and cleaned up all per-CPU TLBs");
}
quint64 TLBSystem::checkTB(quint16 cpuId, quint64 virtualAddress, quint64 asn, bool isKernelMode)
{
    //QReadLocker locker(&m_cpuTLBLock);
    QWriteLocker writeLocker(&m_cpuTLBLock);
    const PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return 0ULL; // CPU not registered
    }

    // Search through this CPU's TLB entries
    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        const TLBEntry &entry = tlbData->entries[i];

        if (!entry.isValid())
        {
            continue;
        }

        // Check ASN match (unless global entry)
        if (entry.getAsn() != asn && !entry.isGlobal())
        {
            continue;
        }

        // Check address match
        quint64 pageSize = entry.getPageSize();
        quint64 entryBaseVA = (entry.getVirtualAddress() / pageSize) * pageSize;
        quint64 requestBaseVA = (virtualAddress / pageSize) * pageSize;

        if (entryBaseVA == requestBaseVA)
        {
            // Check permissions
            if (!entry.isReadable(isKernelMode))
            {
                continue;
            }

            // TLB hit - update LRU timestamp (need to get write access)
           // locker.unlock();
            //QWriteLocker writeLocker(&m_cpuTLBLock);
            PerCPUTLBData *writableTlbData = getCPUTLBData(cpuId);
            if (writableTlbData && i < writableTlbData->lastUsed.size())
            {
                writableTlbData->ageCounter++;
                writableTlbData->lastUsed[i] = writableTlbData->ageCounter;
                writableTlbData->hits++;
            }

            // Calculate physical address
            quint64 offset = virtualAddress & (pageSize - 1);
            return entry.getPhysicalAddress() + offset;
        }
    }

    // TLB miss
    //locker.unlock();
    //  QWriteLocker writeLocker(&m_cpuTLBLock);
    PerCPUTLBData *writableTlbData = getCPUTLBData(cpuId);
    if (writableTlbData)
    {
        writableTlbData->misses++;
    }

    return 0ULL;
}


TLBEntry *TLBSystem::findTLBEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn, bool isExec, bool isWrite)
{
    QWriteLocker locker(&m_cpuTLBLock);
    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return nullptr; // CPU not registered
    }

    // Search through this CPU's TLB entries
    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        TLBEntry &entry = tlbData->entries[i];

        if (!entry.isValid())
        {
            continue;
        }

        // Check ASN match (unless global entry)
        if (entry.getAsn() != asn && !entry.isGlobal())
        {
            continue;
        }

        // Check address match
        quint64 pageSize = entry.getPageSize();
        quint64 entryBaseVA = (entry.getVirtualAddress() / pageSize) * pageSize;
        quint64 requestBaseVA = (virtualAddress / pageSize) * pageSize;

        if (entryBaseVA == requestBaseVA)
        {
            // Check permissions
            if (isWrite && !entry.isWritable())
            {
                continue;
            }
            if (isExec && !entry.isExecutable())
            {
                continue;
            }

            // Update LRU timestamp
            tlbData->ageCounter++;
            tlbData->lastUsed[i] = tlbData->ageCounter;
            tlbData->hits++;

            return &entry;
        }
    }

    // TLB miss
    tlbData->misses++;
    return nullptr;
}


// [3.7.4] ASA Vol II-A – TLB Match Requirements
bool TLBSystem::hasValidMapping(quint64 virtualAddress, quint16 cpuid) const
{
    QReadLocker locker(&m_cpuTLBLock);

    const PerCPUTLBData *tlbData = getCPUTLBData(cpuid);
    if (!tlbData)
        return false;

    for (const TLBEntry &entry : tlbData->entries)
    {
        if (!entry.isValid())
            continue;

        quint64 pageSize = entry.getPageSize();
        quint64 entryBaseVA = (entry.getVirtualAddress() / pageSize) * pageSize;
        quint64 requestBaseVA = (virtualAddress / pageSize) * pageSize;

        if (entryBaseVA == requestBaseVA)
            return true;
    }

    return false;
}

void TLBSystem::insertTLBEntry(quint16 cpuId, const TLBEntry &newEntry)
{
    QWriteLocker locker(&m_cpuTLBLock);

    if (!ensureCPURegistered(cpuId))
    {
        ERROR_LOG(QString("TLBSystem: Failed to register CPU %1 for TLB entry insertion").arg(cpuId));
        return;
    }

    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);
    if (!tlbData)
    {
        return;
    }

    int replaceIdx = -1;

    // First, try to find an invalid entry
    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        if (!tlbData->entries[i].isValid())
        {
            replaceIdx = i;
            break;
        }
    }

    // If no invalid entry found, evict LRU entry
    if (replaceIdx < 0)
    {
        quint64 minAge = tlbData->lastUsed[0];
        replaceIdx = 0;
        for (int i = 1; i < tlbData->lastUsed.size(); ++i)
        {
            if (tlbData->lastUsed[i] < minAge)
            {
                minAge = tlbData->lastUsed[i];
                replaceIdx = i;
            }
        }
    }

    // Insert the new entry
    tlbData->entries[replaceIdx] = newEntry;
    tlbData->entries[replaceIdx].setValid(true);
    tlbData->ageCounter++;
    tlbData->lastUsed[replaceIdx] = tlbData->ageCounter;

    DEBUG_LOG(QString("TLBSystem: Inserted TLB entry for CPU %1 at index %2, VA=0x%3")
                  .arg(cpuId)
                  .arg(replaceIdx)
                  .arg(newEntry.getVirtualAddress(), 0, 16));
}

void TLBSystem::invalidateAll(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuTLBLock);
    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return; // CPU not registered
    }

    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        tlbData->entries[i].setValid(false);
        tlbData->lastUsed[i] = 0;
    }
    tlbData->ageCounter = 0;

    if (m_translationCache)
    {
        m_translationCache->invalidateAll();
    }

    DEBUG_LOG(QString("TLBSystem: Invalidated all entries for CPU %1").arg(cpuId));
}

void TLBSystem::invalidateEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn)
{
    QWriteLocker locker(&m_cpuTLBLock);
    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return; // CPU not registered
    }

    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        TLBEntry &entry = tlbData->entries[i];

        if (!entry.isValid())
        {
            continue;
        }

        if (entry.getAsn() != asn && !entry.isGlobal())
        {
            continue;
        }

        quint64 pageSize = entry.getPageSize();
        quint64 baseVA = (virtualAddress / pageSize) * pageSize;
        quint64 entryBaseVA = (entry.getVirtualAddress() / pageSize) * pageSize;

        if (entryBaseVA == baseVA)
        {
            entry.setValid(false);
            tlbData->lastUsed[i] = 0;

            if (m_translationCache)
            {
                m_translationCache->invalidateAddress(virtualAddress, asn);
            }
        }
    }

    // Also invalidate cached instructions at this VA
    if (m_instructionCache)
    {
        m_instructionCache->invalidate(virtualAddress);
    }

    DEBUG_LOG(QString("TLBSystem: Invalidated entry for CPU %1, VA=0x%2, ASN=%3")
                  .arg(cpuId)
                  .arg(virtualAddress, 0, 16)
                  .arg(asn));
}


void TLBSystem::invalidateByASN(quint16 cpuId, quint64 asn)
{
    QWriteLocker locker(&m_cpuTLBLock);
    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return; // CPU not registered
    }

    int invalidatedCount = 0;
    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        TLBEntry &entry = tlbData->entries[i];

        if (entry.isValid() && entry.getAsn() == asn && !entry.isGlobal())
        {
            entry.setValid(false);
            tlbData->lastUsed[i] = 0;
            invalidatedCount++;
        }
    }

    if (m_translationCache)
    {
        m_translationCache->invalidateASN(asn);
    }

    DEBUG_LOG(
        QString("TLBSystem: Invalidated %1 entries by ASN %2 for CPU %3").arg(invalidatedCount).arg(asn).arg(cpuId));
}

void TLBSystem::invalidateTranslationCacheASN(quint16 cpuId, quint64 asn)
{
    if (m_translationCache)
        m_translationCache->invalidateASN(asn);
}


void TLBSystem::invalidateTranslationCacheAll()
{
    if (m_translationCache)
        m_translationCache->invalidateAll();
}


void TLBSystem::invalidateDataEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn)
{
    QWriteLocker locker(&m_cpuTLBLock);
    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return; // CPU not registered
    }

    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        TLBEntry &entry = tlbData->entries[i];

        if (!entry.isValid())
        {
            continue;
        }

        if (entry.getAsn() != asn && !entry.isGlobal())
        {
            continue;
        }

        // Only invalidate data TLB entries (not instruction entries)
        if (entry.isInstructionEntry())
        {
            continue;
        }

        quint64 pageSize = entry.getPageSize();
        quint64 baseVA = (virtualAddress / pageSize) * pageSize;
        quint64 entryBaseVA = (entry.getVirtualAddress() / pageSize) * pageSize;

        if (entryBaseVA == baseVA)
        {
            entry.setValid(false);
            tlbData->lastUsed[i] = 0;

            if (m_translationCache)
            {
                m_translationCache->invalidateAddress(virtualAddress, asn);
            }
        }
    }

    DEBUG_LOG(QString("TLBSystem: Invalidated data entry for CPU %1, VA=0x%2, ASN=%3")
                  .arg(cpuId)
                  .arg(virtualAddress, 0, 16)
                  .arg(asn));
}

void TLBSystem::invalidateInstructionEntry(quint16 cpuId, quint64 virtualAddress, quint64 asn)
{
    QWriteLocker locker(&m_cpuTLBLock);
    PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        return; // CPU not registered
    }

    for (int i = 0; i < tlbData->entries.size(); ++i)
    {
        TLBEntry &entry = tlbData->entries[i];

        if (!entry.isValid())
        {
            continue;
        }

        if (entry.getAsn() != asn && !entry.isGlobal())
        {
            continue;
        }

        // Only invalidate instruction TLB entries (not data entries)
        if (!entry.isInstructionEntry())
        {
            continue;
        }

        quint64 pageSize = entry.getPageSize();
        quint64 baseVA = (virtualAddress / pageSize) * pageSize;
        quint64 entryBaseVA = (entry.getVirtualAddress() / pageSize) * pageSize;

        if (entryBaseVA == baseVA)
        {
            entry.setValid(false);
            tlbData->lastUsed[i] = 0;

            if (m_translationCache)
            {
                m_translationCache->invalidateAddress(virtualAddress, asn);
            }
        }
    }

    // Also invalidate cached instructions at this VA
    if (m_instructionCache)
    {
        m_instructionCache->invalidate(virtualAddress);
    }

    DEBUG_LOG(QString("TLBSystem: Invalidated instruction entry for CPU %1, VA=0x%2, ASN=%3")
                  .arg(cpuId)
                  .arg(virtualAddress, 0, 16)
                  .arg(asn));
}

bool TLBSystem::registerCPU(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuTLBLock);

    if (cpuId >= m_maxCpus)
    {
        ERROR_LOG(QString("TLBSystem: CPU ID %1 exceeds maximum %2").arg(cpuId).arg(m_maxCpus));
        return false;
    }

    if (m_cpuTLBMap.contains(cpuId))
    {
        WARN_LOG(QString("TLBSystem: CPU %1 already registered").arg(cpuId));
        return false;
    }

    // Create new TLB data for this CPU
    m_cpuTLBMap.insert(cpuId, PerCPUTLBData(m_tlbCapacity));

    DEBUG_LOG(QString("TLBSystem: Registered CPU %1 with %2 TLB entries").arg(cpuId).arg(m_tlbCapacity));
    return true;
}

bool TLBSystem::unregisterCPU(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuTLBLock);

    if (!m_cpuTLBMap.contains(cpuId))
    {
        WARN_LOG(QString("TLBSystem: CPU %1 not registered").arg(cpuId));
        return false;
    }

    // Clean up translation cache entries for this CPU if needed
    if (m_translationCache)
    {
        // Note: This would need ASN information - might need to be done by caller
        // m_translationCache->invalidateAll(); // Conservative approach
    }

    m_cpuTLBMap.remove(cpuId);

    DEBUG_LOG(QString("TLBSystem: Unregistered CPU %1").arg(cpuId));
    return true;
}

void TLBSystem::updateCPUContext(quint16 cpuId, quint64 newASN)
{
    QWriteLocker locker(&m_cpuTLBLock);

    if (!isCPURegistered(cpuId))
    {
        WARN_LOG("TLBSystem: Cannot update context for unregistered CPU %1", cpuId);
        return;
    }

    // Update ASN for this CPU's TLB entries
    auto &cpuTLB = m_cpuTLBMap[cpuId];

    // Update current ASN tracking
    if (m_cpuASNs.contains(cpuId))
    {
        quint64 oldASN = m_cpuASNs[cpuId];
        m_cpuASNs[cpuId] = newASN;

        DEBUG_LOG("TLBSystem: Updated CPU %1 ASN: %2 -> %3", cpuId, oldASN, newASN);

        // If ASN changed significantly, might want to flush some entries
        if (oldASN != newASN)
        {
            // Optionally invalidate entries for old ASN
            // invalidateByASN(cpuId, oldASN);
        }
    }
    else
    {
        m_cpuASNs[cpuId] = newASN;
        DEBUG_LOG("TLBSystem: Set initial ASN for CPU %1: %2", cpuId, newASN);
    }

    // Update TLB system statistics
    asa_utils::safeIncrement(m_contextSwitches);

    emit sigCPUContextUpdated(cpuId, newASN);
}

bool TLBSystem::isCPURegistered(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuTLBLock);
    return m_cpuTLBMap.contains(cpuId);
}

QVector<quint16> TLBSystem::getRegisteredCPUs() const
{
    QReadLocker locker(&m_cpuTLBLock);
    QVector<quint16> cpuIds;
    cpuIds.reserve(m_cpuTLBMap.size());

    for (auto it = m_cpuTLBMap.constBegin(); it != m_cpuTLBMap.constEnd(); ++it)
    {
        cpuIds.append(it.key());
    }

    return cpuIds;
}

//Helpers

TLBSystem::PerCPUTLBData *TLBSystem::getCPUTLBData(quint16 cpuId)
{
    // Note: Caller must hold appropriate lock
    auto it = m_cpuTLBMap.find(cpuId);
    return (it != m_cpuTLBMap.end()) ? &it.value() : nullptr;
}

const TLBSystem::PerCPUTLBData *TLBSystem::getCPUTLBData(quint16 cpuId) const
{
    // Note: Caller must hold appropriate lock
    auto it = m_cpuTLBMap.constFind(cpuId);
    return (it != m_cpuTLBMap.constEnd()) ? &it.value() : nullptr;
}

bool TLBSystem::ensureCPURegistered(quint16 cpuId)
{
    QReadLocker readLocker(&m_cpuTLBLock);
    if (m_cpuTLBMap.contains(cpuId))
    {
        return true;
    }
    readLocker.unlock();

    // Need to register CPU
    return registerCPU(cpuId);
}


void TLBSystem::invalidateEntryAllCPUs(quint64 virtualAddress, quint64 asn, quint16 excludeCpuId)
{
    QReadLocker locker(&m_cpuTLBLock);

    for (auto it = m_cpuTLBMap.constBegin(); it != m_cpuTLBMap.constEnd(); ++it)
    {
        quint16 cpuId = it.key();
        if (cpuId != excludeCpuId)
        {
            locker.unlock();
            invalidateEntry(cpuId, virtualAddress, asn);
            locker.relock();
        }
    }

    DEBUG_LOG(QString("TLBSystem: Broadcast invalidate entry VA=0x%1, ASN=%2, excluding CPU %3")
                  .arg(virtualAddress, 0, 16)
                  .arg(asn)
                  .arg(excludeCpuId));
}

void TLBSystem::invalidateByASNAllCPUs(quint64 asn, quint16 excludeCpuId)
{
    QReadLocker locker(&m_cpuTLBLock);

    for (auto it = m_cpuTLBMap.constBegin(); it != m_cpuTLBMap.constEnd(); ++it)
    {
        quint16 cpuId = it.key();
        if (cpuId != excludeCpuId)
        {
            locker.unlock();
            invalidateByASN(cpuId, asn);
            locker.relock();
        }
    }

    DEBUG_LOG(QString("TLBSystem: Broadcast invalidate ASN=%1, excluding CPU %2").arg(asn).arg(excludeCpuId));
}

void TLBSystem::invalidateAllCPUs(quint16 excludeCpuId)
{
    QReadLocker locker(&m_cpuTLBLock);

    for (auto it = m_cpuTLBMap.constBegin(); it != m_cpuTLBMap.constEnd(); ++it)
    {
        quint16 cpuId = it.key();
        if (cpuId != excludeCpuId)
        {
            locker.unlock();
            invalidateAll(cpuId);
            locker.relock();
        }
    }

    DEBUG_LOG(QString("TLBSystem: Broadcast invalidate all TLBs, excluding CPU %1").arg(excludeCpuId));
}

TLBSystem::TLBStats TLBSystem::getTLBStats(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuTLBLock);
    const PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    TLBStats stats = {};

    if (!tlbData)
    {
        return stats; // CPU not registered
    }

    stats.entries = tlbData->entries.size();
    stats.ageCounter = tlbData->ageCounter;
    stats.hits = tlbData->hits;
    stats.misses = tlbData->misses;

    // Count valid entries
    stats.validEntries = 0;
    for (const TLBEntry &entry : tlbData->entries)
    {
        if (entry.isValid())
        {
            stats.validEntries++;
        }
    }

    return stats;
}

QHash<quint16, TLBSystem::TLBStats> TLBSystem::getAllTLBStats() const
{
    QReadLocker locker(&m_cpuTLBLock);
    QHash<quint16, TLBStats> allStats;

    for (auto it = m_cpuTLBMap.constBegin(); it != m_cpuTLBMap.constEnd(); ++it)
    {
        quint16 cpuId = it.key();
        locker.unlock();
        allStats.insert(cpuId, getTLBStats(cpuId));
        locker.relock();
    }

    return allStats;
}

void TLBSystem::dumpTLBState(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuTLBLock);

    if (cpuId == 0xFFFF)
    {
        // Dump all CPUs
        DEBUG_LOG("=== TLBSystem State Dump (All CPUs) ===");
        for (auto it = m_cpuTLBMap.constBegin(); it != m_cpuTLBMap.constEnd(); ++it)
        {
            locker.unlock();
            dumpSingleCPUTLB(it.key());
            locker.relock();
        }
    }
    else
    {
        dumpSingleCPUTLB(cpuId);
    }
}

void TLBSystem::dumpSingleCPUTLB(quint16 cpuId) const
{
    const PerCPUTLBData *tlbData = getCPUTLBData(cpuId);

    if (!tlbData)
    {
        DEBUG_LOG(QString("CPU %1: Not registered").arg(cpuId));
        return;
    }

    TLBStats stats = getTLBStats(cpuId);

    DEBUG_LOG(QString("=== CPU %1 TLB State ===").arg(cpuId));
    DEBUG_LOG(QString("  Entries: %1/%2 valid").arg(stats.validEntries).arg(stats.entries));
    DEBUG_LOG(QString("  Age Counter: %1").arg(stats.ageCounter));
    DEBUG_LOG(QString("  Hits: %1, Misses: %2").arg(stats.hits).arg(stats.misses));

    if (stats.hits + stats.misses > 0)
    {
        double hitRate = (double)stats.hits / (stats.hits + stats.misses) * 100.0;
        DEBUG_LOG(QString("  Hit Rate: %1%").arg(hitRate, 0, 'f', 2));
    }

    // Dump individual entries (limit to first 10 for readability)
    int entriesToShow = qMin(10, (int)tlbData->entries.size());
    for (int i = 0; i < entriesToShow; ++i)
    {
        const TLBEntry &entry = tlbData->entries[i];
        if (entry.isValid())
        {
            DEBUG_LOG(QString("  [%1] VA=0x%2 -> PA=0x%3, ASN=%4, Size=%5KB, %6%7%8")
                          .arg(i, 2)
                          .arg(entry.getVirtualAddress(), 0, 16)
                          .arg(entry.getPhysicalAddress(), 0, 16)
                          .arg(entry.getAsn())
                          .arg(entry.getPageSize() / 1024)
                          .arg(entry.isReadable() ? "R" : "-")
                          .arg(entry.isWritable() ? "W" : "-")
                          .arg(entry.isExecutable() ? "X" : "-"));
        }
    }

    if (tlbData->entries.size() > entriesToShow)
    {
        DEBUG_LOG(QString("  ... and %1 more entries").arg(tlbData->entries.size() - entriesToShow));
    }
}