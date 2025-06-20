// AlphaSMPManager.h - SMP Coordination Updates
#pragma once

#include "AlphaCPU_refactored.h"
#include "MMIOManager.h"
#include "SafeMemory_refactored.h"
#include "TLBSystem.h"
#include <QAtomicInt>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QThread>
#include <QTimer>
#include <QVector>
#include "AlphaMemorySystem_refactored.h"

/**
 * @brief SMP system coordination and management
 *
 * Enhanced AlphaSMPManager that properly coordinates multiple Alpha CPUs
 * with shared memory system, cache coherency, and inter-processor communication.
 */
class AlphaSMPManager : public QObject
{
    Q_OBJECT

  public:
    explicit AlphaSMPManager(QObject *parent = nullptr);
    ~AlphaSMPManager();

    // =========================
    // SYSTEM INITIALIZATION AND CONFIGURATION
    // =========================

    /**
     * @brief Initialize SMP system with specified CPU count
     * @param cpuCount Number of CPUs to create (1-16)
     * @param memorySize Total system memory size
     * @param cpuModel CPU model to use for all processors
     * @return True if initialization successful
     */
    bool initializeSystem(quint16 cpuCount, quint64 memorySize, CpuModel cpuModel = CpuModel::CPU_EV56);

    /**
     * @brief Add a CPU to the system
     * @param cpuId Unique CPU identifier (0-based)
     * @return Pointer to created CPU or nullptr on failure
     */
    AlphaCPU *addCPU(quint16 cpuId);

    /**
     * @brief Remove a CPU from the system
     * @param cpuId CPU identifier to remove
     * @return True if removal successful
     */
    bool removeCPU(quint16 cpuId);

    /**
     * @brief Get CPU by ID
     * @param cpuId CPU identifier
     * @return CPU instance or nullptr if not found
     */
    AlphaCPU *getCPU(quint16 cpuId) const;

    /**
     * @brief Get all CPUs in the system
     * @return Vector of all CPU instances
     */
    QVector<AlphaCPU *> getAllCPUs() const;

    /**
     * @brief Get number of CPUs in system
     * @return CPU count
     */
    quint16 getCPUCount() const;

    // =========================
    // SYSTEM CONTROL AND COORDINATION
    // =========================

    /**
     * @brief Start all CPUs
     */
    void startAllCPUs();

    /**
     * @brief Stop all CPUs
     */
    void stopAllCPUs();

    /**
     * @brief Pause all CPUs
     */
    void pauseAllCPUs();

    /**
     * @brief Resume all CPUs
     */
    void resumeAllCPUs();

    /**
     * @brief Reset all CPUs
     */
    void resetAllCPUs();

    /**
     * @brief Set CPU online/offline status
     * @param cpuId CPU identifier
     * @param isOnline True to bring CPU online
     */
    void setCPUOnlineStatus(quint16 cpuId, bool isOnline);

    // =========================
    // INTER-PROCESSOR COMMUNICATION
    // =========================

    /**
     * @brief Send inter-processor interrupt
     * @param sourceCpuId Source CPU
     * @param targetCpuId Target CPU (0xFFFF = broadcast)
     * @param vector Interrupt vector
     */
    void sendIPI(quint16 sourceCpuId, quint16 targetCpuId, int vector);


//      // Enhanced translation with conflict detection
//     bool translateAddressWithConflictDetection(quint16 cpuId, quint64 virtualAddr, quint64 &physicalAddr, quint64 asn,
//                                                bool isWrite, bool isInstruction);

//     // TLB configuration
//     void tlbConfigureTLBImplementation(TLBImplementationType type, const TLBConfig &config);
//     tlbConflictStatistics getTLBConflictStatistics() const;
// 
//     // Conflict mitigation
//     void tlbEnableConflictMitigation(bool enabled);
//     void tlbSetConflictMitigationStrategy(ConflictMitigationStrategy strategy);
    

    /**
     * @brief Broadcast IPI to all CPUs except source
     * @param sourceCpuId Source CPU
     * @param vector Interrupt vector
     */
    void broadcastIPI(quint16 sourceCpuId, int vector);

    /**
     * @brief Send system-wide notification
     * @param eventType Type of event
     * @param data Optional event data
     */
    void sendSystemNotification(const QString &eventType, quint64 data = 0);

    // =========================
    // CACHE COHERENCY MANAGEMENT
    // =========================

    /**
     * @brief Coordinate cache coherency across all CPUs
     * @param physicalAddr Physical address
     * @param eventType Event type (INVALIDATE, FLUSH, WRITEBACK)
     * @param sourceCpuId CPU that initiated the event
     */
    void coordinateCacheCoherency(quint64 physicalAddr, const QString &eventType, quint16 sourceCpuId);

    /**
     * @brief Invalidate cache lines on all CPUs
     * @param physicalAddr Physical address
     * @param size Size of region
     * @param sourceCpuId Source CPU
     */
    void invalidateAllCaches(quint64 physicalAddr, int size, quint16 sourceCpuId);

    /**
     * @brief Flush all CPU caches
     * @param sourceCpuId Source CPU
     */
    void flushAllCaches(quint16 sourceCpuId);

    // =========================
    // TLB COORDINATION
    // =========================

    /**
     * @brief Coordinate TLB invalidation across all CPUs
     * @param virtualAddr Virtual address (0 = all entries)
     * @param asn Address Space Number (0 = all ASNs)
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void coordinateTLBInvalidation(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);

    /**
     * @brief Invalidate TLB on all CPUs for specific ASN
     * @param asn Address Space Number
     * @param sourceCpuId Source CPU
     */
    void invalidateAllTLBsByASN(quint64 asn, quint16 sourceCpuId);

    // =========================
    // MEMORY SYNCHRONIZATION
    // =========================

    /**
     * @brief Execute memory barrier across all CPUs
     * @param type Barrier type (0=load, 1=store, 2=full)
     * @param sourceCpuId CPU that initiated the barrier
     */
    void executeMemoryBarrier(int type, quint16 sourceCpuId);

    /**
     * @brief Synchronize all CPUs at a barrier point
     * @param barrierId Unique barrier identifier
     * @param sourceCpuId CPU requesting synchronization
     */
    void synchronizeAtBarrier(quint64 barrierId, quint16 sourceCpuId);

    // =========================
    // SYSTEM MONITORING AND STATISTICS
    // =========================

    struct SystemStatistics
    {
        quint64 totalInstructions = 0;
        quint64 totalMemoryAccesses = 0;
        quint64 cacheCoherencyEvents = 0;
        quint64 ipisSent = 0;
        quint64 tlbInvalidations = 0;
        quint64 memoryBarriers = 0;
        QHash<quint16, quint64> instructionsPerCpu;
        QHash<quint16, bool> cpuOnlineStatus;
    };

    SystemStatistics getSystemStatistics() const;
    void resetSystemStatistics();

    /**
     * @brief Get CPU utilization
     * @param cpuId CPU identifier
     * @return Utilization percentage (0.0-100.0)
     */
    double getCPUUtilization(quint16 cpuId) const;

    // =========================
    // COMPONENT ACCESS
    // =========================

    AlphaMemorySystem *getMemorySystem() const { return m_memorySystem; }
    SafeMemory *getSafeMemory() const { return m_safeMemory; }
    MMIOManager *getMMIOManager() const { return m_mmioManager; }
    TLBSystem *getTLBSystem() const { return m_tlbSystem; }

  signals:
    // System-wide signals
    void sigSystemInitialized(quint16 cpuCount, quint64 memorySize);
    void sigAllCPUsStarted();
    void sigAllCPUsStopped();
    void sigAllCPUsPaused();
    void sigAllCPUsResumed();
    void sigAllCPUsReset();

    // CPU management signals
    void sigCPUAdded(quint16 cpuId);
    void sigCPURemoved(quint16 cpuId);
    void sigCPUOnlineStatusChanged(quint16 cpuId, bool isOnline);

    // IPI signals
    void sigIPISent(quint16 sourceCpuId, quint16 targetCpuId, int vector);
    void sigIPIReceived(quint16 targetCpuId, quint16 sourceCpuId, int vector);

    // Cache coherency signals
    void sigCacheCoherencyEvent(quint64 physicalAddr, quint16 sourceCpuId, const QString &eventType);
    void sigCacheInvalidated(quint64 physicalAddr, int size, quint16 sourceCpuId);
    void sigCacheFlushed(quint16 sourceCpuId);

    // TLB coordination signals
    void sigTLBInvalidated(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);
    void sigTLBInvalidatedByASN(quint64 asn, quint16 sourceCpuId);

    // Memory synchronization signals
    void sigMemoryBarrierExecuted(int type, quint16 sourceCpuId);
    void sigBarrierSynchronization(quint64 barrierId, quint16 sourceCpuId);

    // System status signals
    void sigSystemNotification(const QString &eventType, quint64 data);
    void sigSystemStatisticsUpdated();

  public slots:
    // CPU event handlers
    void onCPUStateChanged(quint16 cpuId, int newState);
    void onCPUHalted(quint16 cpuId);
    void onCPUException(quint16 cpuId, int exceptionType, quint64 pc);

    // IPI handlers
    void onIPIRequest(quint16 sourceCpuId, quint16 targetCpuId, int vector);
    void onBroadcastIPIRequest(quint16 sourceCpuId, int vector);

    // Cache coherency handlers
    void onCacheCoherencyRequest(quint64 physicalAddr, const QString &eventType, quint16 sourceCpuId);
    void onCacheInvalidationRequest(quint64 physicalAddr, int size, quint16 sourceCpuId);
    void onCacheFlushRequest(quint16 sourceCpuId);

    // TLB coordination handlers
    void onTLBInvalidationRequest(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);
    void onTLBInvalidationByASNRequest(quint64 asn, quint16 sourceCpuId);

    // Memory synchronization handlers
    void onMemoryBarrierRequest(int type, quint16 sourceCpuId);
    void onBarrierSynchronizationRequest(quint64 barrierId, quint16 sourceCpuId);

  private slots:
    // Internal coordination
    void onUpdateStatistics();
    void onSystemHeartbeat();

  private:
    // =========================
    // CORE COMPONENTS
    // =========================

    // CPU management
    mutable QReadWriteLock m_cpuLock;
    QHash<quint16, AlphaCPU *> m_cpus;
    QHash<quint16, bool> m_cpuOnlineStatus;
    quint16 m_maxCpuId;

    // Shared system components
    AlphaMemorySystem *m_memorySystem;
    SafeMemory *m_safeMemory;
    MMIOManager *m_mmioManager;
    TLBSystem *m_tlbSystem;
    IRQController *m_irqController;

    // System configuration
    CpuModel m_cpuModel;
    quint64 m_systemMemorySize;
    bool m_systemInitialized;

    // =========================
    // SMP COORDINATION STATE
    // =========================

    // IPI management
    QMutex m_ipiMutex;
    struct IPIMessage
    {
        quint16 sourceCpuId;
        quint16 targetCpuId;
        int vector;
        quint64 timestamp;
    };
    QQueue<IPIMessage> m_pendingIPIs;

    // Cache coherency coordination
    QMutex m_coherencyMutex;
    QAtomicInt m_coherencyEventId;
    QHash<quint64, QSet<quint16>> m_coherencyParticipants; // eventId -> CPU set

    // TLB coordination
    QMutex m_tlbMutex;
    QAtomicInt m_tlbInvalidationId;

    // Memory barrier coordination
    QMutex m_barrierMutex;
    QHash<quint64, QSet<quint16>> m_barrierParticipants; // barrierId -> CPU set
    QAtomicInt m_nextBarrierId;

    // Statistics and monitoring
    mutable QMutex m_statsMutex;
    SystemStatistics m_systemStats;
    QTimer *m_statisticsTimer;
    QTimer *m_heartbeatTimer;

    // =========================
    // PRIVATE HELPER METHODS
    // =========================

    /**
     * @brief Initialize system components
     * @param memorySize Total memory size
     * @return True if successful
     */
    bool initializeComponents(quint64 memorySize);

    /**
     * @brief Connect CPU signals to SMP manager
     * @param cpu CPU instance to connect
     */
    void connectCPUSignals(AlphaCPU *cpu);

    /**
     * @brief Disconnect CPU signals from SMP manager
     * @param cpu CPU instance to disconnect
     */
    void disconnectCPUSignals(AlphaCPU *cpu);

    /**
     * @brief Validate CPU ID
     * @param cpuId CPU identifier to validate
     * @return True if valid
     */
    bool isValidCPUId(quint16 cpuId) const;

    /**
     * @brief Get online CPU count
     * @return Number of online CPUs
     */
    quint16 getOnlineCPUCount() const;

    /**
     * @brief Process pending IPI messages
     */
    void processPendingIPIs();

    /**
     * @brief Update system statistics
     */
    void updateSystemStatistics();

    /**
     * @brief Handle CPU going offline
     * @param cpuId CPU identifier
     */
    void handleCPUOffline(quint16 cpuId);

    /**
     * @brief Handle CPU coming online
     * @param cpuId CPU identifier
     */
    void handleCPUOnline(quint16 cpuId);

    /**
     * @brief Cleanup system resources
     */
    void cleanupSystem();

    bool tlbHandleTLBConflict(quint16 cpuId, quint64 instructionAddr, quint64 dataAddr);
    void tlbOptimizeTLBLayout(quint16 cpuId);
};