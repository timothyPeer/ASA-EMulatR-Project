#include "AlphaSMPManager_refactored.h"

#include "../AEJ/GlobalLockTracker.h"
#include "../AEJ/GlobalMacro.h"
#include "AlphaSMPManager_refactored.h"
#include "../AEB/IRQController.h"
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>

AlphaSMPManager::AlphaSMPManager(QObject *parent)
    : QObject(parent), m_maxCpuId(0), m_memorySystem(nullptr), m_safeMemory(nullptr), m_mmioManager(nullptr),
      m_tlbSystem(nullptr), m_cpuModel(CpuModel::CPU_EV56), m_systemMemorySize(0), m_systemInitialized(false),
      m_coherencyEventId(0), m_tlbInvalidationId(0), m_nextBarrierId(1), m_statisticsTimer(nullptr),
      m_heartbeatTimer(nullptr)
{
    // Initialize statistics timer
    m_statisticsTimer = new QTimer(this);
    m_statisticsTimer->setInterval(1000); // Update every second
    connect(m_statisticsTimer, &QTimer::timeout, this, &AlphaSMPManager::onUpdateStatistics);

    // Initialize heartbeat timer
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(5000); // Heartbeat every 5 seconds
    connect(m_heartbeatTimer, &QTimer::timeout, this, &AlphaSMPManager::onSystemHeartbeat);

    DEBUG_LOG("AlphaSMPManager: SMP coordination system initialized");
}

AlphaSMPManager::~AlphaSMPManager() { cleanupSystem(); }

// =========================
// SYSTEM INITIALIZATION AND CONFIGURATION
// =========================

bool AlphaSMPManager::initializeSystem(quint16 cpuCount, quint64 memorySize, CpuModel cpuModel)
{
    if (m_systemInitialized)
    {
        WARN_LOG("AlphaSMPManager: System already initialized");
        return false;
    }

    if (cpuCount == 0 || cpuCount > 16)
    {
        ERROR_LOG(QString("AlphaSMPManager: Invalid CPU count: %1 (must be 1-16)").arg(cpuCount));
        return false;
    }

    m_cpuModel = cpuModel;
    m_systemMemorySize = memorySize;

    // Initialize system components
    if (!initializeComponents(memorySize))
    {
        ERROR_LOG("AlphaSMPManager: Failed to initialize system components");
        return false;
    }

    // Create CPUs
    for (quint16 i = 0; i < cpuCount; ++i)
    {
        if (!addCPU(i))
        {
            ERROR_LOG(QString("AlphaSMPManager: Failed to create CPU %1").arg(i));
            cleanupSystem();
            return false;
        }
    }

    m_systemInitialized = true;

    // Start timers
    m_statisticsTimer->start();
    m_heartbeatTimer->start();

    DEBUG_LOG(QString("AlphaSMPManager: System initialized with %1 CPUs, %2 MB memory")
                  .arg(cpuCount)
                  .arg(memorySize / (1024 * 1024)));

    emit sigSystemInitialized(cpuCount, memorySize);
    return true;
}

AlphaCPU *AlphaSMPManager::addCPU(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuLock);

    if (m_cpus.contains(cpuId))
    {
        WARN_LOG(QString("AlphaSMPManager: CPU %1 already exists").arg(cpuId));
        return m_cpus[cpuId];
    }

    // Create new CPU
    AlphaCPU *cpu = new AlphaCPU(this);
    cpu->setCpuId(cpuId);
    cpu->initializeCpuModel(m_cpuModel);

    // Attach system components to CPU
    if (m_memorySystem)
    {
        cpu->attachMemorySystem(m_memorySystem);
        m_memorySystem->registerCPU(cpu, cpuId);
    }

    if (m_irqController)
    {
        cpu->attachIRQController(m_irqController);
    }
//     if (m_safeMemory)
//     {
//         cpu->attachSafeMemory(m_safeMemory);
//         m_safeMemory->registerCPU(cpu, cpuId);
//     }

    if (m_mmioManager)
    {
        cpu->attachMMIOManager(m_mmioManager);
    }

//     if (m_tlbSystem)
//     {
//         cpu->attachTLBSystem(m_tlbSystem);
//     }

    // Connect CPU signals
    connectCPUSignals(cpu);

    // Add to registry
    m_cpus.insert(cpuId, cpu);
    m_cpuOnlineStatus.insert(cpuId, true);
    m_maxCpuId = qMax(m_maxCpuId, static_cast<quint16>(cpuId + 1));

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_systemStats.cpuOnlineStatus[cpuId] = true;
    m_systemStats.instructionsPerCpu[cpuId] = 0;

    DEBUG_LOG(QString("AlphaSMPManager: Added CPU %1 (total: %2)").arg(cpuId).arg(m_cpus.size()));

    emit sigCPUAdded(cpuId);
    return cpu;
}

bool AlphaSMPManager::removeCPU(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuLock);

    if (!m_cpus.contains(cpuId))
    {
        WARN_LOG(QString("AlphaSMPManager: CPU %1 not found").arg(cpuId));
        return false;
    }

    AlphaCPU *cpu = m_cpus[cpuId];

    // Stop CPU if running
    cpu->stop();

    // Disconnect signals
    disconnectCPUSignals(cpu);

    // Unregister from components
    if (m_memorySystem)
    {
        m_memorySystem-unregisterCPU(cpuId);
    }

    if (m_safeMemory)
    {
        m_safeMemory->unregisterCPU(cpuId);
    }

    // Remove from registry
    m_cpus.remove(cpuId);
    m_cpuOnlineStatus.remove(cpuId);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_systemStats.cpuOnlineStatus.remove(cpuId);
    m_systemStats.instructionsPerCpu.remove(cpuId);

    // Delete CPU
    cpu->deleteLater();

    DEBUG_LOG(QString("AlphaSMPManager: Removed CPU %1 (remaining: %2)").arg(cpuId).arg(m_cpus.size()));

    emit sigCPURemoved(cpuId);
    return true;
}

AlphaCPU *AlphaSMPManager::getCPU(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuLock);
    return m_cpus.value(cpuId, nullptr);
}

QVector<AlphaCPU *> AlphaSMPManager::getAllCPUs() const
{
    QReadLocker locker(&m_cpuLock);
    QVector<AlphaCPU *> result;
    result.reserve(m_cpus.size());

    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        result.append(it.value());
    }

    return result;
}

quint16 AlphaSMPManager::getCPUCount() const
{
    QReadLocker locker(&m_cpuLock);
    return static_cast<quint16>(m_cpus.size());
}

// =========================
// SYSTEM CONTROL AND COORDINATION
// =========================

void AlphaSMPManager::startAllCPUs()
{
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (m_cpuOnlineStatus.value(it.key(), false))
        {
            it.value()->start();
        }
    }

    DEBUG_LOG("AlphaSMPManager: Started all online CPUs");
    emit sigAllCPUsStarted();
}

void AlphaSMPManager::stopAllCPUs()
{
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        it.value()->stop();
    }

    DEBUG_LOG("AlphaSMPManager: Stopped all CPUs");
    emit sigAllCPUsStopped();
}

void AlphaSMPManager::pauseAllCPUs()
{
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        it.value()->pause();
    }

    DEBUG_LOG("AlphaSMPManager: Paused all CPUs");
    emit sigAllCPUsPaused();
}

void AlphaSMPManager::resumeAllCPUs()
{
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (m_cpuOnlineStatus.value(it.key(), false))
        {
            it.value()->resume();
        }
    }

    DEBUG_LOG("AlphaSMPManager: Resumed all online CPUs");
    emit sigAllCPUsResumed();
}

void AlphaSMPManager::resetAllCPUs()
{
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        it.value()->reset();
    }

    // Reset system statistics
    resetSystemStatistics();

    DEBUG_LOG("AlphaSMPManager: Reset all CPUs");
    emit sigAllCPUsReset();
}

void AlphaSMPManager::setCPUOnlineStatus(quint16 cpuId, bool isOnline)
{
    QWriteLocker locker(&m_cpuLock);

    if (!m_cpus.contains(cpuId))
    {
        WARN_LOG(QString("AlphaSMPManager: CPU %1 not found").arg(cpuId));
        return;
    }

    bool currentStatus = m_cpuOnlineStatus.value(cpuId, false);
    if (currentStatus == isOnline)
    {
        return; // No change
    }

    m_cpuOnlineStatus[cpuId] = isOnline;

    if (isOnline)
    {
        handleCPUOnline(cpuId);
    }
    else
    {
        handleCPUOffline(cpuId);
    }

    // Update memory system
    if (m_memorySystem)
    {
        m_memorySystem->setCPUOnlineStatus(cpuId, isOnline);
    }

    DEBUG_LOG(QString("AlphaSMPManager: CPU %1 %2").arg(cpuId).arg(isOnline ? "online" : "offline"));

    emit sigCPUOnlineStatusChanged(cpuId, isOnline);
}

// =========================
// INTER-PROCESSOR COMMUNICATION
// =========================

void AlphaSMPManager::sendIPI(quint16 sourceCpuId, quint16 targetCpuId, int vector)
{
    QMutexLocker locker(&m_ipiMutex);

    // Validate CPUs
    if (!isValidCPUId(sourceCpuId) || !isValidCPUId(targetCpuId))
    {
        ERROR_LOG(QString("AlphaSMPManager: Invalid CPU IDs for IPI: %1 -> %2").arg(sourceCpuId).arg(targetCpuId));
        return;
    }

    // Create IPI message
    IPIMessage ipi;
    ipi.sourceCpuId = sourceCpuId;
    ipi.targetCpuId = targetCpuId;
    ipi.vector = vector;
    ipi.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_pendingIPIs.enqueue(ipi);

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_systemStats.ipisSent++;

    DEBUG_LOG(QString("AlphaSMPManager: IPI queued from CPU%1 to CPU%2, vector=%3")
                  .arg(sourceCpuId)
                  .arg(targetCpuId)
                  .arg(vector));

    emit sigIPISent(sourceCpuId, targetCpuId, vector);

    // Process immediately
    locker.unlock();
    processPendingIPIs();
}

void AlphaSMPManager::broadcastIPI(quint16 sourceCpuId, int vector)
{
    QReadLocker cpuLocker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (it.key() != sourceCpuId && m_cpuOnlineStatus.value(it.key(), false))
        {
            sendIPI(sourceCpuId, it.key(), vector);
        }
    }

    DEBUG_LOG(QString("AlphaSMPManager: Broadcast IPI from CPU%1, vector=%2").arg(sourceCpuId).arg(vector));
}

void AlphaSMPManager::sendSystemNotification(const QString &eventType, quint64 data)
{
    DEBUG_LOG(QString("AlphaSMPManager: System notification: %1, data=0x%2").arg(eventType).arg(data, 0, 16));
    emit sigSystemNotification(eventType, data);
}

// =========================
// CACHE COHERENCY MANAGEMENT
// =========================

void AlphaSMPManager::coordinateCacheCoherency(quint64 physicalAddr, const QString &eventType, quint16 sourceCpuId)
{
    QMutexLocker locker(&m_coherencyMutex);

    quint64 eventId = m_coherencyEventId.fetchAndAddAcquire(1);

    // Coordinate with memory system
    if (m_memorySystem)
    {
        if (eventType == "INVALIDATE")
        {
            m_memorySystem->invalidateCacheLines(physicalAddr, 64, sourceCpuId);
        }
        else if (eventType == "FLUSH")
        {
            m_memorySystem->flushCacheLines(physicalAddr, 64, sourceCpuId);
        }
    }

    // Notify all other CPUs
    QReadLocker cpuLocker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (it.key() != sourceCpuId && m_cpuOnlineStatus.value(it.key(), false))
        {
            it.value()->handleCacheCoherencyEvent(physicalAddr, eventType);
        }
    }

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_systemStats.cacheCoherencyEvents++;

    DEBUG_LOG(QString("AlphaSMPManager: Cache coherency coordinated: %1 at 0x%2 from CPU%3")
                  .arg(eventType)
                  .arg(physicalAddr, 0, 16)
                  .arg(sourceCpuId));

    emit sigCacheCoherencyEvent(physicalAddr, sourceCpuId, eventType);
}

void AlphaSMPManager::invalidateAllCaches(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    coordinateCacheCoherency(physicalAddr, "INVALIDATE", sourceCpuId);
    emit sigCacheInvalidated(physicalAddr, size, sourceCpuId);
}

void AlphaSMPManager::flushAllCaches(quint16 sourceCpuId)
{
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (m_cpuOnlineStatus.value(it.key(), false))
        {
            it.value()->flushCache();
        }
    }

    DEBUG_LOG(QString("AlphaSMPManager: All caches flushed by CPU%1").arg(sourceCpuId));
    emit sigCacheFlushed(sourceCpuId);
}

// =========================
// TLB COORDINATION
// =========================

void AlphaSMPManager::coordinateTLBInvalidation(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    QMutexLocker locker(&m_tlbMutex);

    quint64 invalidationId = m_tlbInvalidationId.fetchAndAddAcquire(1);

    // Coordinate with memory system
    if (m_memorySystem)
    {
        if (virtualAddr == 0)
        {
            m_memorySystem->invalidateAllTLB(sourceCpuId);
        }
        else
        {
            m_memorySystem->invalidateTLBEntry(virtualAddr, asn, sourceCpuId);
        }
    }

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_systemStats.tlbInvalidations++;

    DEBUG_LOG(QString("AlphaSMPManager: TLB invalidation coordinated: VA=0x%1, ASN=%2 from CPU%3")
                  .arg(virtualAddr, 0, 16)
                  .arg(asn)
                  .arg(sourceCpuId));

    emit sigTLBInvalidated(virtualAddr, asn, sourceCpuId);
}

void AlphaSMPManager::invalidateAllTLBsByASN(quint64 asn, quint16 sourceCpuId)
{
    if (m_memorySystem)
    {
        m_memorySystem->invalidateTLBByASN(asn, sourceCpuId);
    }

    DEBUG_LOG(QString("AlphaSMPManager: All TLBs invalidated by ASN %1 from CPU%2").arg(asn).arg(sourceCpuId));
    emit sigTLBInvalidatedByASN(asn, sourceCpuId);
}

// =========================
// MEMORY SYNCHRONIZATION
// =========================

void AlphaSMPManager::executeMemoryBarrier(int type, quint16 sourceCpuId)
{
    QMutexLocker locker(&m_barrierMutex);

    // Coordinate with memory system
    if (m_memorySystem)
    {
        m_memorySystem->executeMemoryBarrier(type, sourceCpuId);
    }

    // Execute barrier on all CPUs
    QReadLocker cpuLocker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (m_cpuOnlineStatus.value(it.key(), false))
        {
            it.value()->executeMemoryBarrier(type);
        }
    }

    // Update statistics
    QMutexLocker statsLocker(&m_statsMutex);
    m_systemStats.memoryBarriers++;

    DEBUG_LOG(QString("AlphaSMPManager: Memory barrier executed: type=%1 from CPU%2").arg(type).arg(sourceCpuId));
    emit sigMemoryBarrierExecuted(type, sourceCpuId);
}

void AlphaSMPManager::synchronizeAtBarrier(quint64 barrierId, quint16 sourceCpuId)
{
    QMutexLocker locker(&m_barrierMutex);

    // Add CPU to barrier participants
    m_barrierParticipants[barrierId].insert(sourceCpuId);

    quint16 onlineCount = getOnlineCPUCount();
    if (m_barrierParticipants[barrierId].size() >= onlineCount)
    {
        // All CPUs have reached the barrier
        DEBUG_LOG(QString("AlphaSMPManager: Barrier %1 synchronized with %2 CPUs")
                      .arg(barrierId)
                      .arg(m_barrierParticipants[barrierId].size()));

        // Release all CPUs
        QReadLocker cpuLocker(&m_cpuLock);
        for (quint16 cpuId : m_barrierParticipants[barrierId])
        {
            AlphaCPU *cpu = m_cpus.value(cpuId, nullptr);
            if (cpu)
            {
                cpu->releaseFromBarrier(barrierId);
            }
        }

        // Clean up barrier
        m_barrierParticipants.remove(barrierId);

        emit sigBarrierSynchronization(barrierId, sourceCpuId);
    }
    else
    {
        DEBUG_LOG(QString("AlphaSMPManager: CPU%1 waiting at barrier %2 (%3/%4)")
                      .arg(sourceCpuId)
                      .arg(barrierId)
                      .arg(m_barrierParticipants[barrierId].size())
                      .arg(onlineCount));
    }
}

// =========================
// SYSTEM MONITORING AND STATISTICS
// =========================

AlphaSMPManager::SystemStatistics AlphaSMPManager::getSystemStatistics() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_systemStats;
}

void AlphaSMPManager::resetSystemStatistics()
{
    QMutexLocker locker(&m_statsMutex);
    m_systemStats = SystemStatistics();

    // Maintain CPU online status
    QReadLocker cpuLocker(&m_cpuLock);
    for (auto it = m_cpuOnlineStatus.begin(); it != m_cpuOnlineStatus.end(); ++it)
    {
        m_systemStats.cpuOnlineStatus[it.key()] = it.value();
        m_systemStats.instructionsPerCpu[it.key()] = 0;
    }

    DEBUG_LOG("AlphaSMPManager: System statistics reset");
    emit sigSystemStatisticsUpdated();
}

double AlphaSMPManager::getCPUUtilization(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuLock);
    AlphaCPU *cpu = m_cpus.value(cpuId, nullptr);
    if (cpu)
    {
        return cpu->getCPUUtilization();
    }
    return 0.0;
}

// =========================
// SIGNAL HANDLERS (SLOTS)
// =========================

void AlphaSMPManager::onCPUStateChanged(quint16 cpuId, int newState)
{
    DEBUG_LOG(QString("AlphaSMPManager: CPU%1 state changed to %2").arg(cpuId).arg(newState));

    // Handle state-specific logic
    if (newState == 0)
    { // Halted
        handleCPUOffline(cpuId);
    }
    else if (newState == 1)
    { // Running
        handleCPUOnline(cpuId);
    }
}

void AlphaSMPManager::onCPUHalted(quint16 cpuId)
{
    DEBUG_LOG(QString("AlphaSMPManager: CPU%1 halted").arg(cpuId));
    handleCPUOffline(cpuId);
}

void AlphaSMPManager::onCPUException(quint16 cpuId, int exceptionType, quint64 pc)
{
    DEBUG_LOG(QString("AlphaSMPManager: CPU%1 exception %2 at PC=0x%3").arg(cpuId).arg(exceptionType).arg(pc, 0, 16));
}

void AlphaSMPManager::onIPIRequest(quint16 sourceCpuId, quint16 targetCpuId, int vector)
{
    sendIPI(sourceCpuId, targetCpuId, vector);
}

void AlphaSMPManager::onBroadcastIPIRequest(quint16 sourceCpuId, int vector) { broadcastIPI(sourceCpuId, vector); }

void AlphaSMPManager::onCacheCoherencyRequest(quint64 physicalAddr, const QString &eventType, quint16 sourceCpuId)
{
    coordinateCacheCoherency(physicalAddr, eventType, sourceCpuId);
}

void AlphaSMPManager::onCacheInvalidationRequest(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    invalidateAllCaches(physicalAddr, size, sourceCpuId);
}

void AlphaSMPManager::onCacheFlushRequest(quint16 sourceCpuId) { flushAllCaches(sourceCpuId); }

void AlphaSMPManager::onTLBInvalidationRequest(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    coordinateTLBInvalidation(virtualAddr, asn, sourceCpuId);
}

void AlphaSMPManager::onTLBInvalidationByASNRequest(quint64 asn, quint16 sourceCpuId)
{
    invalidateAllTLBsByASN(asn, sourceCpuId);
}

void AlphaSMPManager::onMemoryBarrierRequest(int type, quint16 sourceCpuId) { executeMemoryBarrier(type, sourceCpuId); }

void AlphaSMPManager::onBarrierSynchronizationRequest(quint64 barrierId, quint16 sourceCpuId)
{
    synchronizeAtBarrier(barrierId, sourceCpuId);
}

void AlphaSMPManager::onUpdateStatistics() { updateSystemStatistics(); }

void AlphaSMPManager::onSystemHeartbeat()
{
    // Process pending operations
    processPendingIPIs();

    // Check CPU health
    QReadLocker locker(&m_cpuLock);
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        if (m_cpuOnlineStatus.value(it.key(), false))
        {
            // Could add CPU health checks here
        }
    }
}

// =========================
// PRIVATE HELPER METHODS
// =========================

bool AlphaSMPManager::initializeComponents(quint64 memorySize)
{
    // Initialize SafeMemory
    m_safeMemory = new SafeMemory(this);
    m_safeMemory->resize(memorySize, true);

    // Initialize MMIOManager
    m_mmioManager = new MMIOManager(this);

    // Initialize TLBSystem
    m_tlbSystem = new TLBSystem(this);

    // Initialize AlphaMemorySystem
    m_memorySystem = new AlphaMemorySystem(this);
    m_memorySystem->attachSafeMemory(m_safeMemory);
    m_memorySystem->attachMMIOManager(m_mmioManager);
    m_memorySystem->attachTLBSystem(m_tlbSystem);
    m_memorySystem->initializeCpuModel(m_cpuModel);

    DEBUG_LOG("AlphaSMPManager: System components initialized");
    return true;
}

void AlphaSMPManager::connectCPUSignals(AlphaCPU *cpu)
{
    if (!cpu)
        return;

    // Connect CPU state signals
    connect(cpu, &AlphaCPU::sigStateChanged, this, &AlphaSMPManager::onCPUStateChanged);
    connect(cpu, &AlphaCPU::sigHalted, this, &AlphaSMPManager::onCPUHalted);
    connect(cpu, &AlphaCPU::sigException, this, &AlphaSMPManager::onCPUException);

    // Connect IPI signals
    connect(cpu, &AlphaCPU::sigIPIRequest, this, &AlphaSMPManager::onIPIRequest);
    connect(cpu, &AlphaCPU::sigBroadcastIPIRequest, this, &AlphaSMPManager::onBroadcastIPIRequest);

    // Connect cache coherency signals
    connect(cpu, &AlphaCPU::sigCacheCoherencyRequest, this, &AlphaSMPManager::onCacheCoherencyRequest);
    connect(cpu, &AlphaCPU::sigCacheInvalidationRequest, this, &AlphaSMPManager::onCacheInvalidationRequest);
    connect(cpu, &AlphaCPU::sigCacheFlushRequest, this, &AlphaSMPManager::onCacheFlushRequest);

    // Connect TLB signals
    connect(cpu, &AlphaCPU::sigTLBInvalidationRequest, this, &AlphaSMPManager::onTLBInvalidationRequest);
    connect(cpu, &AlphaCPU::sigTLBInvalidationByASNRequest, this, &AlphaSMPManager::onTLBInvalidationByASNRequest);

    // Connect memory synchronization signals
    connect(cpu, &AlphaCPU::sigMemoryBarrierRequest, this, &AlphaSMPManager::onMemoryBarrierRequest);
    connect(cpu, &AlphaCPU::sigBarrierSynchronizationRequest, this, &AlphaSMPManager::onBarrierSynchronizationRequest);
}

void AlphaSMPManager::disconnectCPUSignals(AlphaCPU *cpu)
{
    if (!cpu)
        return;

    // Disconnect all signals from this CPU
    disconnect(cpu, nullptr, this, nullptr);
}

bool AlphaSMPManager::isValidCPUId(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuLock);
    return m_cpus.contains(cpuId);
}

quint16 AlphaSMPManager::getOnlineCPUCount() const
{
    // Note: Caller should hold appropriate locks
    quint16 count = 0;
    for (auto it = m_cpuOnlineStatus.begin(); it != m_cpuOnlineStatus.end(); ++it)
    {
        if (it.value())
        {
            count++;
        }
    }
    return count;
}

void AlphaSMPManager::processPendingIPIs()
{
    QMutexLocker locker(&m_ipiMutex);

    while (!m_pendingIPIs.isEmpty())
    {
        IPIMessage ipi = m_pendingIPIs.dequeue();

        // Deliver IPI to target CPU
        QReadLocker cpuLocker(&m_cpuLock);
        AlphaCPU *targetCpu = m_cpus.value(ipi.targetCpuId, nullptr);
        if (targetCpu && m_cpuOnlineStatus.value(ipi.targetCpuId, false))
        {
            targetCpu->receiveIPI(ipi.sourceCpuId, ipi.vector);

            DEBUG_LOG(QString("AlphaSMPManager: IPI delivered from CPU%1 to CPU%2, vector=%3")
                          .arg(ipi.sourceCpuId)
                          .arg(ipi.targetCpuId)
                          .arg(ipi.vector));

            emit sigIPIReceived(ipi.targetCpuId, ipi.sourceCpuId, ipi.vector);
        }
        else
        {
            WARN_LOG(
                QString("AlphaSMPManager: Failed to deliver IPI to CPU%1 (offline or not found)").arg(ipi.targetCpuId));
        }
    }
}


bool AlphaMemorySystem::tlbTranslateAddressWithConflictDetection(quint16 cpuId, quint64 virtualAddr, quint64 &physicalAddr,
                                                              quint64 asn, bool isWrite, bool isInstruction)
{
    // Pre-translation conflict detection
    if (m_conflictAwareTLB && m_conflictAwareTLB->detectTLBConflict(cpuId, virtualAddr, isInstruction))
    {
        // Conflict detected - use alternative translation path
        return translateViaAlternativePath(cpuId, virtualAddr, physicalAddr, asn, isWrite, isInstruction);
    }

    // Normal translation path
    return translateInternal(cpuId, virtualAddr, isWrite ? 1 : (isInstruction ? 2 : 0), isInstruction);
}

bool AlphaMemorySystem::tlbTranslateViaAlternativePath(quint16 cpuId, quint64 virtualAddr, quint64 &physicalAddr,
                                                    quint64 asn, bool isWrite, bool isInstruction)
{
    // Alternative translation strategies for conflicting addresses

    // Strategy 1: Use victim cache
    if (m_victimCache && m_victimCache->lookup(cpuId, virtualAddr, asn, physicalAddr))
    {
        DEBUG_LOG("Victim cache hit for conflicting address 0x%llx", virtualAddr);
        return true;
    }

    // Strategy 2: Direct page table walk (bypass TLB)
    PageTableWalkResult walkResult;
    if (m_pageTableManager && m_pageTableManager->walkPageTable(virtualAddr, physicalAddr, walkResult))
    {
        // Don't populate TLB to avoid further conflicts
        DEBUG_LOG("Direct page table walk for conflicting address 0x%llx", virtualAddr);

        // Optionally populate victim cache instead
        if (m_victimCache)
        {
            TLBEntry victimEntry;
            if (createTLBEntryFromPTE(victimEntry, virtualAddr, physicalAddr, asn, walkResult.finalPTE, isInstruction))
            {
                m_victimCache->insert(cpuId, victimEntry);
            }
        }
        return true;
    }

    // Strategy 3: Software TLB
    return translateViaSoftwareTLB(cpuId, virtualAddr, physicalAddr, asn, isWrite, isInstruction);
}
void AlphaSMPManager::updateSystemStatistics()
{
    QMutexLocker statsLocker(&m_statsMutex);
    QReadLocker cpuLocker(&m_cpuLock);

    // Reset counters
    m_systemStats.totalInstructions = 0;
    m_systemStats.totalMemoryAccesses = 0;

    // Collect statistics from all CPUs
    for (auto it = m_cpus.begin(); it != m_cpus.end(); ++it)
    {
        quint16 cpuId = it.key();
        AlphaCPU *cpu = it.value();

        if (cpu && m_cpuOnlineStatus.value(cpuId, false))
        {
            quint64 instructions = cpu->getInstructionCount();
            m_systemStats.instructionsPerCpu[cpuId] = instructions;
            m_systemStats.totalInstructions += instructions;
        }
    }

    // Get memory statistics from memory system
    if (m_memorySystem)
    {
        // This would need to be implemented in AlphaMemorySystem
        // m_systemStats.totalMemoryAccesses = m_memorySystem->getTotalAccesses();
    }

    emit sigSystemStatisticsUpdated();
}

void AlphaSMPManager::handleCPUOffline(quint16 cpuId)
{
    // Clear any pending operations for this CPU
    QMutexLocker ipiLocker(&m_ipiMutex);

    // Remove IPIs targeting this CPU
    QQueue<IPIMessage> filteredIPIs;
    while (!m_pendingIPIs.isEmpty())
    {
        IPIMessage ipi = m_pendingIPIs.dequeue();
        if (ipi.targetCpuId != cpuId)
        {
            filteredIPIs.enqueue(ipi);
        }
    }
    m_pendingIPIs = filteredIPIs;
    ipiLocker.unlock();

    // Clear barrier participations
    QMutexLocker barrierLocker(&m_barrierMutex);
    for (auto it = m_barrierParticipants.begin(); it != m_barrierParticipants.end(); ++it)
    {
        it.value().remove(cpuId);
    }
    barrierLocker.unlock();

    // Clear memory reservations
    if (m_memorySystem)
    {
        m_memorySystem->clearCpuReservations(cpuId);
    }

    DEBUG_LOG(QString("AlphaSMPManager: CPU%1 offline handling completed").arg(cpuId));
}

void AlphaSMPManager::handleCPUOnline(quint16 cpuId)
{
    // Initialize CPU state
    AlphaCPU *cpu = getCPU(cpuId);
    if (cpu)
    {
        // Reset CPU to known state
        cpu->resetToKnownState();

        // Flush TLBs to ensure clean state
        if (m_memorySystem)
        {
            m_memorySystem->invalidateAllTLB(cpuId);
        }
    }

    DEBUG_LOG(QString("AlphaSMPManager: CPU%1 online handling completed").arg(cpuId));
}

void AlphaSMPManager::cleanupSystem()
{
    if (!m_systemInitialized)
    {
        return;
    }

    // Stop timers
    if (m_statisticsTimer)
    {
        m_statisticsTimer->stop();
    }
    if (m_heartbeatTimer)
    {
        m_heartbeatTimer->stop();
    }

    // Stop all CPUs
    stopAllCPUs();

    // Remove all CPUs
    QWriteLocker locker(&m_cpuLock);
    QList<quint16> cpuIds = m_cpus.keys();
    locker.unlock();

    for (quint16 cpuId : cpuIds)
    {
        removeCPU(cpuId);
    }

    // Clean up components
    if (m_memorySystem)
    {
        m_memorySystem->deleteLater();
        m_memorySystem = nullptr;
    }

    if (m_safeMemory)
    {
        m_safeMemory->deleteLater();
        m_safeMemory = nullptr;
    }

    if (m_mmioManager)
    {
        m_mmioManager->deleteLater();
        m_mmioManager = nullptr;
    }

    if (m_tlbSystem)
    {
        m_tlbSystem->deleteLater();
        m_tlbSystem = nullptr;
    }

    // Clear state
    m_cpus.clear();
    m_cpuOnlineStatus.clear();
    m_pendingIPIs.clear();
    m_coherencyParticipants.clear();
    m_barrierParticipants.clear();

    m_systemInitialized = false;

    DEBUG_LOG("AlphaSMPManager: System cleanup completed");
}