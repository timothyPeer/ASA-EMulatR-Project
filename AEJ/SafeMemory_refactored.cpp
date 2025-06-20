#include "SafeMemory_refactored.h"
// SafeMemory.cpp - Complete SMP Implementation
#include "../AEJ/enumerations/enumMemoryFaultType.h"
#include "../AEJ/structures/enumMemoryPerm.h"
#include "../AEJ/GlobalLockTracker.h"
#include "AlphaCPU_refactored.h"
#include "GlobalMacro.h"
#include "MemoryAccessException.h"
#include "TraceManager.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>

SafeMemory::SafeMemory(QObject *parent) : QObject(parent), m_irqController(nullptr), m_configLoader(nullptr)
{
    m_ram.fill(0);

    // Initialize SMP statistics
    resetSMPStatistics();

    DEBUG_LOG("SafeMemory: SMP-aware memory system initialized");
}

SafeMemory::~SafeMemory()
{
    // Unregister all CPUs
    QWriteLocker locker(&m_cpuRegistryLock);
    m_attachedCpus.clear();
    m_cpuRegistry.clear();
}

// =========================
// SMP CPU MANAGEMENT
// =========================

void SafeMemory::registerCPU(AlphaCPU *cpu_, quint16 cpuId)
{
    if (!cpu_)
    {
        ERROR_LOG("SafeMemory: Cannot register null CPU");
        return;
    }

    QWriteLocker locker(&m_cpuRegistryLock);

    // Check if CPU ID is already registered
    if (m_cpuRegistry.contains(cpuId))
    {
        WARN_LOG(QString("SafeMemory: CPU ID %1 already registered").arg(cpuId));
        return;
    }

    // Create registry entry
    CPUAccessInfo info;
    info.cpuId = cpuId;
    info.lastAccessTime = QDateTime::currentMSecsSinceEpoch();
    info.accessCount = 0;
    info.hasReservation = false;
    info.reservationAddr = 0;

    m_cpuRegistry.insert(cpuId, info);
    m_attachedCpus.append(cpu_);

    // Initialize reservations for this CPU
    QWriteLocker reservationLocker(&m_reservationLock);
    m_reservations.insert(cpuId, QHash<quint64, int>());

    DEBUG_LOG(QString("SafeMemory: Registered CPU %1 (total: %2)").arg(cpuId).arg(m_cpuRegistry.size()));

    emit sigCPURegistered(cpuId);
}

void SafeMemory::deregisterCPU(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuRegistryLock);

    if (!m_cpuRegistry.contains(cpuId))
    {
        WARN_LOG(QString("SafeMemory: CPU ID %1 not registered").arg(cpuId));
        return;
    }

    // Clear CPU's reservations
    clearReservation(cpuId, 0);

    // Remove from registry
    m_cpuRegistry.remove(cpuId);

    // Remove from attached CPUs list
    for (int i = 0; i < m_attachedCpus.size(); ++i)
    {
        if (m_attachedCpus[i] && m_attachedCpus[i]->getCpuId() == cpuId)
        {
            m_attachedCpus.removeAt(i);
            break;
        }
    }

    {
        QWriteLocker reservationLocker(&m_reservationLock);
        m_reservations.remove(cpuId);
    }

    DEBUG_LOG(QString("SafeMemory: Unregistered CPU %1 (remaining: %2)").arg(cpuId).arg(m_cpuRegistry.size()));

    emit sigCPUUnregistered(cpuId);
}

QVector<AlphaCPU *> SafeMemory::getRegisteredCPUs() const
{
    QReadLocker locker(&m_cpuRegistryLock);
    return m_attachedCpus;
}

bool SafeMemory::isValidPhysicalAddress(quint64 address, int size) const
{
    QReadLocker locker(&m_memoryLock);
    return (address + size <= static_cast<quint64>(m_ram.size()));
}

// =========================
// SMP-AWARE MEMORY OPERATIONS
// =========================

quint8 SafeMemory::readUInt8(quint64 address, quint64 pc, quint16 cpuId)
{
    QReadLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 1))
    {
        throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS, address, 1, false, pc);
    }

    quint8 value = m_ram[static_cast<int>(address)];

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, false);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    emit sigMemoryRead(address, value, 1);
    emit sigMemoryAccessSMP(address, value, 1, false, cpuId);

    return value;
}

quint16 SafeMemory::readUInt16(quint64 address, quint64 pc, quint16 cpuId)
{
    if (address & 1)
    {
        throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT, address, 2, false, pc);
    }

    QReadLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 2))
    {
        throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS, address, 2, false, pc);
    }

    quint16 value = qFromLittleEndian<quint16>(&m_ram[static_cast<int>(address)]);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, false);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    emit sigMemoryRead(address, value, 2);
    emit sigMemoryAccessSMP(address, value, 2, false, cpuId);

    return value;
}

quint32 SafeMemory::readUInt32(quint64 address, quint64 pc, quint16 cpuId)
{
    if (address & 3)
    {
        throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT, address, 4, false, pc);
    }

    QReadLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 4))
    {
        throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS, address, 4, false, pc);
    }

    quint32 value = qFromLittleEndian<quint32>(&m_ram[static_cast<int>(address)]);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, false);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    emit sigMemoryRead(address, value, 4);
    emit sigMemoryAccessSMP(address, value, 4, false, cpuId);

    return value;
}

quint64 SafeMemory::readUInt64(quint64 address, quint64 pc, quint16 cpuId)
{
    if (address & 0x7)
    {
        throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT, address, 8, false, pc);
    }

    QReadLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 8))
    {
        throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS, address, 8, false, pc);
    }

    quint64 value = qFromLittleEndian<quint64>(&m_ram[static_cast<int>(address)]);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, false);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    emit sigMemoryRead(address, value, 8);
    emit sigMemoryAccessSMP(address, value, 8, false, cpuId);

    return value;
}

void SafeMemory::writeUInt8(quint64 address, quint8 value, quint64 pc, quint16 cpuId)
{
    QWriteLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 1))
    {
        WARN_LOG("SafeMemory: writeUInt8() out of bounds: 0x%1  RAM Size: %2 bytes", QString::number(address, 16),
                 m_ram.size());
        return;
    }

    try
    {
        m_ram[static_cast<int>(address)] = value;
    }
    catch (const std::exception &e)
    {
        throw MemoryAccessException(MemoryFaultType::WRITE_ERROR, address, 1, true, pc);
    }

    // Clear overlapping reservations on write
    clearOverlappingReservations(address, 1, cpuId);

    // Invalidate in all attached L3 caches
    invalidateInAttachedCaches(address, 1, cpuId);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, true);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    // Notify cache coherency
    notifyCacheCoherency(address, "INVALIDATE", cpuId);

    emit sigMemoryWritten(address, value, 1);
    emit sigMemoryAccessSMP(address, value, 1, true, cpuId);
}


void SafeMemory::writeUInt16(quint64 address, quint16 value, quint64 pc, quint16 cpuId)
{
    if (address & 1)
    {
        throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT, address, 2, true, pc);
    }

    QWriteLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 2))
    {
        WARN_LOG("SafeMemory: Write16 out of bounds: 0x%1 (RAM Size: %2 bytes)", QString::number(address, 16),
                 m_ram.size());
        return;
    }

    try
    {
        qToLittleEndian(value, &m_ram[static_cast<int>(address)]);
    }
    catch (const std::exception &e)
    {
        throw MemoryAccessException(MemoryFaultType::WRITE_ERROR, address, 2, true, pc);
    }

    // Clear overlapping reservations on write
    clearOverlappingReservations(address, 2, cpuId);

    // Invalidate in all attached L3 caches
    invalidateInAttachedCaches(address, 2, cpuId);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, true);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    // Notify cache coherency
    notifyCacheCoherency(address, "INVALIDATE", cpuId);

    emit sigMemoryWritten(address, value, 2);
    emit sigMemoryAccessSMP(address, value, 2, true, cpuId);
}
void SafeMemory::writeUInt32(quint64 address, quint32 value, quint64 pc, quint16 cpuId)
{
    if (address & 3)
    {
        throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT, address, 4, true, pc);
    }

    QWriteLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 4))
    {
        WARN_LOG("SafeMemory: Write32 out of bounds: 0x%1 (RAM Size: %2 bytes)", QString::number(address, 16),
                 m_ram.size());
        return;
    }

    try
    {
        qToLittleEndian(value, &m_ram[static_cast<int>(address)]);
        TRACE_LOG("SafeMemory: Write32 to 0x%1 = 0x%2", QString::number(address, 16), QString::number(value, 16));
    }
    catch (const std::exception &e)
    {
        throw MemoryAccessException(MemoryFaultType::WRITE_ERROR, address, 4, true, pc);
    }

    // Clear overlapping reservations on write
    clearOverlappingReservations(address, 4, cpuId);

    // Invalidate in all attached L3 caches
    invalidateInAttachedCaches(address, 4, cpuId);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, true);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    // Notify cache coherency
    notifyCacheCoherency(address, "INVALIDATE", cpuId);

    emit sigMemoryWritten(address, value, 4);
    emit sigMemoryAccessSMP(address, value, 4, true, cpuId);
}
void SafeMemory::writeUInt64(quint64 address, quint64 value, quint64 pc, quint16 cpuId)
{
    if (address & 0x7)
    {
        throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT, address, 8, true, pc);
    }

    QWriteLocker locker(&m_memoryLock);

    if (!isValidAddress(address, 8))
    {
        WARN_LOG("SafeMemory: Write64 out of bounds: 0x%1 (RAM Size: %2 bytes)", QString::number(address, 16),
                 m_ram.size());
        return;
    }

    try
    {
        qToLittleEndian(value, &m_ram[static_cast<int>(address)]);
        TRACE_LOG("SafeMemory: Write64 to 0x%1 = 0x%2", QString::number(address, 16), QString::number(value, 16));
    }
    catch (const std::exception &e)
    {
        throw MemoryAccessException(MemoryFaultType::WRITE_ERROR, address, 8, true, pc);
    }

    // Clear overlapping reservations on write
    clearOverlappingReservations(address, 8, cpuId);

    // Invalidate in all attached L3 caches
    invalidateInAttachedCaches(address, 8, cpuId);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, true);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    // Notify cache coherency
    notifyCacheCoherency(address, "INVALIDATE", cpuId);

    emit sigMemoryWritten(address, value, 8);
    emit sigMemoryAccessSMP(address, value, 8, true, cpuId);
}

void SafeMemory::writeBytes(quint64 address, const quint8 *data, quint64 size, quint64 pc, quint16 cpuId)
{
    QWriteLocker locker(&m_memoryLock);

    // Ensure the target region is mapped
    if (!isValidAddress(address, static_cast<int>(size)))
    {
        WARN_LOG(QString("[SafeMemory::writeBytes] out of bounds: 0x%1, size %2, RAM Size: %3 bytes")
                     .arg(address, 0, 16)
                     .arg(size)
                     .arg(m_ram.size()));
        return;
    }

    try
    {
        // Copy bytes into memory
        for (quint64 i = 0; i < size; ++i)
        {
            m_ram[static_cast<int>(address + i)] = data[i];
        }

        TRACE_LOG(
            QString("[SafeMemory::writeBytes] Wrote %1 bytes to 0x%2").arg(size).arg(address, 16, 16, QChar('0')));
    }
    catch (const std::exception &e)
    {
        throw MemoryAccessException(MemoryFaultType::WRITE_ERROR, address, static_cast<int>(size), true, pc);
    }

    // Clear overlapping reservations on write
    clearOverlappingReservations(address, static_cast<int>(size), cpuId);

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, true);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    // Notify cache coherency for the entire block
    notifyCacheCoherency(address, "INVALIDATE", cpuId);

    // Emit signal for the block write
    emit sigMemoryWritten(address, size, static_cast<int>(size));
    emit sigMemoryAccessSMP(address, size, static_cast<int>(size), true, cpuId);
}

void SafeMemory::writeBytes(quint64 address, const QByteArray &data, quint64 pc, quint16 cpuId)
{
    writeBytes(address, reinterpret_cast<const quint8 *>(data.constData()), static_cast<quint64>(data.size()), pc,
               cpuId);
}

// =========================
// SMP RESERVATION MANAGEMENT
// =========================

bool SafeMemory::setReservation(quint16 cpuId, quint64 physicalAddr, int size)
{
    QWriteLocker locker(&m_reservationLock);

    // Ensure CPU is registered
    if (!m_reservations.contains(cpuId))
    {
        m_reservations.insert(cpuId, QHash<quint64, int>());
    }

    // Align to 8-byte boundary for reservations
    quint64 alignedAddr = physicalAddr & ~0x7ULL;

    // Set the reservation
    m_reservations[cpuId].insert(alignedAddr, size);

    // Update CPU registry info
    QWriteLocker cpuLocker(&m_cpuRegistryLock);
    if (m_cpuRegistry.contains(cpuId))
    {
        m_cpuRegistry[cpuId].hasReservation = true;
        m_cpuRegistry[cpuId].reservationAddr = alignedAddr;
    }

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.reservationSets++;

    DEBUG_LOG(QString("Reservation set: CPU%1, addr=0x%2, size=%3").arg(cpuId).arg(alignedAddr, 0, 16).arg(size));

    emit sigReservationSet(cpuId, alignedAddr, size);
    return true;
}

void SafeMemory::clearReservation(quint16 cpuId, quint64 physicalAddr)
{
    QWriteLocker locker(&m_reservationLock);

    if (!m_reservations.contains(cpuId))
    {
        return;
    }

    if (physicalAddr == 0)
    {
        // Clear all reservations for this CPU
        m_reservations[cpuId].clear();

        // Update CPU registry info
        QWriteLocker cpuLocker(&m_cpuRegistryLock);
        if (m_cpuRegistry.contains(cpuId))
        {
            m_cpuRegistry[cpuId].hasReservation = false;
            m_cpuRegistry[cpuId].reservationAddr = 0;
        }

        DEBUG_LOG(QString("All reservations cleared for CPU%1").arg(cpuId));
    }
    else
    {
        // Clear specific reservation
        quint64 alignedAddr = physicalAddr & ~0x7ULL;
        m_reservations[cpuId].remove(alignedAddr);

        // Update CPU registry info if this was the tracked reservation
        QWriteLocker cpuLocker(&m_cpuRegistryLock);
        if (m_cpuRegistry.contains(cpuId) && m_cpuRegistry[cpuId].reservationAddr == alignedAddr)
        {
            m_cpuRegistry[cpuId].hasReservation = false;
            m_cpuRegistry[cpuId].reservationAddr = 0;
        }

        DEBUG_LOG(QString("Reservation cleared: CPU%1, addr=0x%2").arg(cpuId).arg(alignedAddr, 0, 16));
    }

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.reservationClears++;

    emit sigReservationCleared(cpuId, physicalAddr);
}

bool SafeMemory::hasReservation(quint16 cpuId, quint64 physicalAddr) const
{
    QReadLocker locker(&m_reservationLock);

    if (!m_reservations.contains(cpuId))
    {
        return false;
    }

    quint64 alignedAddr = physicalAddr & ~0x7ULL;
    return m_reservations[cpuId].contains(alignedAddr);
}

void SafeMemory::attachL3Cache(UnifiedDataCache *cache)
{
    if (!cache)
    {
        WARN_LOG("SafeMemory: Cannot attach null L3 cache");
        return;
    }

    QWriteLocker locker(&m_memoryLock);

    if (!m_attachedL3Caches.contains(cache))
    {
        m_attachedL3Caches.append(cache);

        // Connect cache signals for coherency
        connect(cache, &UnifiedDataCache::sigLineEvicted, this,
                [this](quint64 address, bool wasDirty)
                {
                    if (wasDirty)
                    {
                        notifyCacheCoherency(address, "WRITEBACK", 0xFFFF);
                    }
                    DEBUG_LOG("SafeMemory: L3 cache line evicted: addr=0x%1, dirty=%2", QString::number(address, 16),
                              wasDirty);
                });

        connect(cache, &UnifiedDataCache::sigWriteBack, this,
                [this](quint64 address, size_t size)
                {
                    // Cache is writing back to memory - no action needed from SafeMemory
                    DEBUG_LOG("SafeMemory: L3 cache writeback: addr=0x%1, size=%2", QString::number(address, 16), size);
                });

        // Set up cache backing store integration
        cache->setBackingRead([this](quint64 addr, void *buf, size_t size) -> bool
                              { return readPhysicalMemoryForCache(addr, buf, size); });

        DEBUG_LOG("SafeMemory: Attached L3 cache, total caches: %1", m_attachedL3Caches.size());
    }
}

void SafeMemory::clearOverlappingReservations(quint64 physicalAddr, int size, quint16 excludeCpuId)
{
    QWriteLocker locker(&m_reservationLock);

    quint64 startAddr = physicalAddr & ~0x7ULL;            // Align to 8-byte boundary
    quint64 endAddr = (physicalAddr + size + 7) & ~0x7ULL; // Round up to next 8-byte boundary

    for (auto cpuIt = m_reservations.begin(); cpuIt != m_reservations.end(); ++cpuIt)
    {
        if (cpuIt.key() == excludeCpuId)
        {
            continue; // Skip the CPU that initiated the write
        }

        // Check all reservations for this CPU
        auto &reservations = cpuIt.value();
        auto resIt = reservations.begin();
        while (resIt != reservations.end())
        {
            quint64 reservationAddr = resIt.key();
            int reservationSize = resIt.value();
            quint64 reservationEnd = reservationAddr + reservationSize;

            // Check for overlap
            if (!(startAddr >= reservationEnd || endAddr <= reservationAddr))
            {
                // Overlapping reservation found - clear it
                DEBUG_LOG(QString("Clearing overlapping reservation: CPU%1, addr=0x%2")
                              .arg(cpuIt.key())
                              .arg(reservationAddr, 0, 16));

                resIt = reservations.erase(resIt);

                // Update CPU registry
                QWriteLocker cpuLocker(&m_cpuRegistryLock);
                if (m_cpuRegistry.contains(cpuIt.key()) &&
                    m_cpuRegistry[cpuIt.key()].reservationAddr == reservationAddr)
                {
                    m_cpuRegistry[cpuIt.key()].hasReservation = false;
                    m_cpuRegistry[cpuIt.key()].reservationAddr = 0;
                }

                emit sigReservationCleared(cpuIt.key(), reservationAddr);
            }
            else
            {
                ++resIt;
            }
        }
    }
}

// =========================
// CACHE COHERENCY SUPPORT
// =========================

void SafeMemory::invalidateInAttachedCaches(quint64 address, int size, quint16 sourceCpuId)
{
    QReadLocker locker(&m_memoryLock);

    for (auto *cache : m_attachedL3Caches)
    {
        if (cache)
        {
            // Invalidate cache lines that overlap with this address range
            quint64 cacheLineSize = cache->getLineSize();
            quint64 startLine = (address / cacheLineSize) * cacheLineSize;
            quint64 endAddr = address + size;

            for (quint64 lineAddr = startLine; lineAddr < endAddr; lineAddr += cacheLineSize)
            {
                cache->invalidateLine(lineAddr);
            }

            DEBUG_LOG("SafeMemory: Invalidated cache lines for addr=0x%1, size=%2, source=CPU%3",
                      QString::number(address, 16), size, sourceCpuId);
        }
    }
}
void SafeMemory::invalidateCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    notifyCacheCoherency(physicalAddr, "INVALIDATE", sourceCpuId);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.cacheInvalidations++;

    DEBUG_LOG(QString("Cache invalidation: addr=0x%1, size=%2, source=CPU%3")
                  .arg(physicalAddr, 0, 16)
                  .arg(size)
                  .arg(sourceCpuId));
}

void SafeMemory::flushAttachedCaches(quint64 address, int size)
{
    QReadLocker locker(&m_memoryLock);

    for (auto *cache : m_attachedL3Caches)
    {
        if (cache)
        {
            quint64 cacheLineSize = cache->getLineSize();
            quint64 startLine = (address / cacheLineSize) * cacheLineSize;
            quint64 endAddr = address + size;

            for (quint64 lineAddr = startLine; lineAddr < endAddr; lineAddr += cacheLineSize)
            {
                cache->flushLine(lineAddr);
            }

            DEBUG_LOG("SafeMemory: Flushed cache lines for addr=0x%1, size=%2", QString::number(address, 16), size);
        }
    }
}
void SafeMemory::flushCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    notifyCacheCoherency(physicalAddr, "FLUSH", sourceCpuId);

    DEBUG_LOG(
        QString("Cache flush: addr=0x%1, size=%2, source=CPU%3").arg(physicalAddr, 0, 16).arg(size).arg(sourceCpuId));
}

void SafeMemory::memoryBarrierSMP(int type, quint16 sourceCpuId)
{
    // Coordinate memory barriers across all CPUs
    notifyCacheCoherency(0, QString("BARRIER_%1").arg(type), sourceCpuId);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.memoryBarriers++;

    DEBUG_LOG(QString("Memory barrier: type=%1, source=CPU%2").arg(type).arg(sourceCpuId));
}

// =========================
// LEGACY COMPATIBILITY METHODS
// =========================

void SafeMemory::resize(quint64 newSize, bool initialize)
{
    QWriteLocker locker(&m_memoryLock);
    if (initialize)
    {
        m_ram.resize(static_cast<int>(newSize));
        m_ram.fill(0);
        return;
    }

    // Expand Memory Boundaries
    if (newSize > static_cast<quint64>(m_ram.size()))
    {
        // Growing memory - keep existing contents
        int oldSize = m_ram.size();
        m_ram.resize(static_cast<int>(newSize));
        // Zero out the new memory region
        for (int i = oldSize; i < m_ram.size(); ++i)
        {
            m_ram[i] = 0;
        }
    }
    TRACE_LOG(QString("[SafeMemory:resize()] allocation complete :%1").arg(m_ram.size()));
}

quint64 SafeMemory::size() const
{
    QReadLocker locker(&m_memoryLock);
    return static_cast<quint64>(m_ram.size());
}

quint8 SafeMemory::readUInt8(quint64 address, quint64 pc)
{
    return readUInt8(address, pc, 0); // Default to CPU 0 for legacy compatibility
}

bool SafeMemory::readPhysicalMemoryForCache(quint64 addr, void *buf, size_t size)
{
    QReadLocker locker(&m_memoryLock);

    if (!isValidAddress(addr, static_cast<int>(size)))
    {
        return false;
    }

    try
    {
        std::memcpy(buf, &m_ram[static_cast<int>(addr)], size);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

quint16 SafeMemory::readUInt16(quint64 address, quint64 pc) { return readUInt16(address, pc, 0); }

quint32 SafeMemory::readUInt32(quint64 address, quint64 pc) { return readUInt32(address, pc, 0); }

quint64 SafeMemory::readUInt64(quint64 address, quint64 pc) { return readUInt64(address, pc, 0); }

void SafeMemory::writeUInt8(quint64 address, quint8 value, quint64 pc) { writeUInt8(address, value, pc, 0); }

void SafeMemory::writeUInt16(quint64 address, quint16 value, quint64 pc) { writeUInt16(address, value, pc, 0); }

void SafeMemory::writeUInt32(quint64 address, quint32 value, quint64 pc) { writeUInt32(address, value, pc, 0); }

void SafeMemory::writeUInt64(quint64 address, quint64 value, quint64 pc) { writeUInt64(address, value, pc, 0); }

void SafeMemory::writeBytes(quint64 address, const quint8 *data, quint64 size, quint64 pc)
{
    writeBytes(address, data, size, pc, 0);
}

void SafeMemory::writeBytes(quint64 address, const QByteArray &data, quint64 pc) { writeBytes(address, data, pc, 0); }

// =========================
// UTILITY METHODS
// =========================

// Complete the mapRegion function (add this to the end of the existing implementation)
void SafeMemory::mapRegion(quint64 address, quint64 size, enumMemoryPerm perm)
{
    Q_UNUSED(perm); // For now, we don't enforce permissions - just ensure memory exists

    // Calculate the end address we need to support
    quint64 endAddress = address + size;

    // If this region extends beyond current memory, resize to accommodate it
    if (endAddress > static_cast<quint64>(m_ram.size()))
    {
        resize(endAddress, false); // Don't initialize, preserve existing data
        DEBUG_LOG(QString("SafeMemory: Expanded memory to accommodate region 0x%1-0x%2 (size: %3)")
                      .arg(address, 0, 16)
                      .arg(endAddress, 0, 16)
                      .arg(size));
    }

    DEBUG_LOG(
        QString("SafeMemory: Mapped region 0x%1-0x%2 (size: %3)").arg(address, 0, 16).arg(endAddress, 0, 16).arg(size));
}

void SafeMemory::unmapRegion(quint64 address, quint64 size)
{
    // For this simple implementation, we don't actually unmap memory
    // In a more sophisticated system, this would mark pages as unmapped
    DEBUG_LOG(QString("SafeMemory: Unmapped region 0x%1-0x%2 (size: %3)")
                  .arg(address, 0, 16)
                  .arg(address + size, 0, 16)
                  .arg(size));
}

bool SafeMemory::isValidAddress(quint64 address, int size) const
{
    return (address + size <= static_cast<quint64>(m_ram.size()));
}

void SafeMemory::zero(quint64 address, quint64 size)
{
    QWriteLocker locker(&m_memoryLock);

    if (!isValidAddress(address, static_cast<int>(size)))
    {
        WARN_LOG(QString("SafeMemory::zero() - Invalid address range: 0x%1, size %2").arg(address, 0, 16).arg(size));
        return;
    }

    for (quint64 i = 0; i < size; ++i)
    {
        m_ram[static_cast<int>(address + i)] = 0;
    }

    DEBUG_LOG(QString("SafeMemory: Zeroed %1 bytes at 0x%2").arg(size).arg(address, 0, 16));
}


void SafeMemory::detachL3Cache(UnifiedDataCache *cache)
{
    if (!cache)
    {
        return;
    }

    QWriteLocker locker(&m_memoryLock);

    if (m_attachedL3Caches.removeOne(cache))
    {
        // Disconnect cache signals
        disconnect(cache, nullptr, this, nullptr);

        DEBUG_LOG("SafeMemory: Detached L3 cache, remaining caches: %1", m_attachedL3Caches.size());
    }
}


void SafeMemory::fill(quint64 address, quint64 size, quint8 value)
{
    QWriteLocker locker(&m_memoryLock);

    if (!isValidAddress(address, static_cast<int>(size)))
    {
        WARN_LOG(QString("SafeMemory::fill() - Invalid address range: 0x%1, size %2").arg(address, 0, 16).arg(size));
        return;
    }

    for (quint64 i = 0; i < size; ++i)
    {
        m_ram[static_cast<int>(address + i)] = value;
    }

    DEBUG_LOG(QString("SafeMemory: Filled %1 bytes at 0x%2 with value 0x%3")
                  .arg(size)
                  .arg(address, 0, 16)
                  .arg(value, 2, 16, QChar('0')));
}

QByteArray SafeMemory::readBytes(quint64 address, quint64 size, quint64 pc, quint16 cpuId)
{
    QReadLocker locker(&m_memoryLock);

    if (!isValidAddress(address, static_cast<int>(size)))
    {
        throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS, address, static_cast<int>(size), false, pc);
    }

    QByteArray result(static_cast<int>(size), 0);
    for (quint64 i = 0; i < size; ++i)
    {
        result[static_cast<int>(i)] = m_ram[static_cast<int>(address + i)];
    }

    // Update CPU access tracking
    updateCPUAccessTracking(cpuId, address, false);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_smpStats.totalAccesses++;
    m_smpStats.accessesPerCpu[cpuId]++;

    emit sigMemoryRead(address, size, static_cast<int>(size));
    emit sigMemoryAccessSMP(address, size, static_cast<int>(size), false, cpuId);

    return result;
}

SMPStatistics SafeMemory::getSMPStatistics() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_smpStats;
}

void SafeMemory::resetSMPStatistics()
{
    QMutexLocker locker(&m_statsMutex);
    m_smpStats.reset();
    DEBUG_LOG("SafeMemory: SMP statistics reset");
}

QHash<quint16, CPUAccessInfo> SafeMemory::getCPUAccessInfo() const
{
    QReadLocker locker(&m_cpuRegistryLock);
    return m_cpuRegistry;
}

void SafeMemory::attachIRQController(IRQController *controller)
{
    m_irqController = controller;
    DEBUG_LOG("SafeMemory: IRQ Controller set");
}

void SafeMemory::attachConfigLoader(ConfigLoader *loader)
{
    m_configLoader = loader;
    DEBUG_LOG("SafeMemory: Config Loader set");
}

// Private helper methods
void SafeMemory::updateCPUAccessTracking(quint16 cpuId, quint64 address, bool isWrite)
{
    QWriteLocker locker(&m_cpuRegistryLock);

    if (m_cpuRegistry.contains(cpuId))
    {
        CPUAccessInfo &info = m_cpuRegistry[cpuId];
        info.lastAccessTime = QDateTime::currentMSecsSinceEpoch();
        info.accessCount++;

        TRACE_LOG(QString("CPU%1 %2 access to 0x%3 (total accesses: %4)")
                      .arg(cpuId)
                      .arg(isWrite ? "write" : "read")
                      .arg(address, 0, 16)
                      .arg(info.accessCount));
    }
}

void SafeMemory::notifyCacheCoherency(quint64 address, const QString &operation, quint16 sourceCpuId)
{
    // Notify all registered CPUs about cache coherency events
    QReadLocker locker(&m_cpuRegistryLock);

    for (AlphaCPU *cpu : m_attachedCpus)
    {
        if (cpu && cpu->getCpuId() != sourceCpuId)
        {
            // Notify CPU about cache coherency event
            cpu->handleCacheCoherencyEvent(address, operation);

            TRACE_LOG("SafeMemory: Cache coherency notification: %1 at 0x%2 from CPU%3 to CPU%4", operation,
                      QString::number(address, 16), sourceCpuId, cpu->getCpuId());
        }
    }

    // Notify attached L3 caches
    {
        QReadLocker memLocker(&m_memoryLock);
        for (auto *cache : m_attachedL3Caches)
        {
            if (cache)
            {
                cache->snoop(address, operation);
            }
        }
    }

    emit sigCacheCoherencyEvent(address, operation, sourceCpuId);
}

void SafeMemory::flushWritesForCPU(quint16 cpuId)
{
    // In a real implementation, this would flush any pending writes
    // for the specific CPU from write buffers or caches

    // For this implementation, we'll use a memory barrier to ensure
    // all previous writes from this CPU are visible to other CPUs
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Update CPU access tracking to mark flush operation
    QWriteLocker locker(&m_cpuRegistryLock);
    if (m_cpuRegistry.contains(cpuId))
    {
        CPUAccessInfo &info = m_cpuRegistry[cpuId];
        info.lastAccessTime = QDateTime::currentMSecsSinceEpoch();

        DEBUG_LOG(QString("SafeMemory: Flushed writes for CPU%1").arg(cpuId));
    }

    // Notify cache coherency system
    notifyCacheCoherency(0, "FLUSH_ALL", cpuId);
}