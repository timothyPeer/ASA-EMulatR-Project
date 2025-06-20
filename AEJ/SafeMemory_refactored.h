#pragma once
// SafeMemory.h - SMP Updates

#include "../AEE/MMIOManager.h"
#include "../AESH/QSettingsConfigLoader.h"
#include "../intelhexloader.h"
#include "alphajitprofiler.h"
#include "../AEJ/enumerations/enumMemoryPerm.h"
#include <QAtomicInt>
#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QVector>
#include <QWriteLocker>
#include <QDateTime>
#include "UnifiedDataCache.h"
#include "structures/enumMemoryPerm.h"


// Forward declarations
class AlphaCPU;
class IRQController;
class ConfigLoader;

// SMP Statistics structure
struct SMPStatistics
{
    quint64 totalAccesses = 0;
    quint64 reservationSets = 0;
    quint64 reservationClears = 0;
    quint64 cacheInvalidations = 0;
    quint64 memoryBarriers = 0;
    QHash<quint16, quint64> accessesPerCpu;

    void reset()
    {
        totalAccesses = 0;
        reservationSets = 0;
        reservationClears = 0;
        cacheInvalidations = 0;
        memoryBarriers = 0;
        accessesPerCpu.clear();
    }
};

// CPU Access Information
struct CPUAccessInfo
{
    quint16 cpuId = 0;
    qint64 lastAccessTime = 0;
    quint64 accessCount = 0;
    bool hasReservation = false;
    quint64 reservationAddr = 0;
};

class SafeMemory : public QObject
{
    Q_OBJECT


private:
  QVector<UnifiedDataCache *> m_attachedL3Caches;
  
public:
    explicit SafeMemory(QObject *parent = nullptr);
    virtual ~SafeMemory();

    void attachL3Cache(UnifiedDataCache *cache);
    // SMP CPU Management
    void registerCPU(AlphaCPU *cpu, quint16 cpuId);
    void deregisterCPU(quint16 cpuId);
    QVector<AlphaCPU *> getRegisteredCPUs() const;
    bool isValidPhysicalAddress(quint64 address, int size) const;

    // SMP-aware Memory Operations

    void flushWritesForCPU(quint16 cpuId);
    quint8 readUInt8(quint64 address, quint64 pc, quint16 cpuId);
    bool readPhysicalMemoryForCache(quint64 addr, void *buf, size_t size);
    quint16 readUInt16(quint64 address, quint64 pc, quint16 cpuId);
    quint32 readUInt32(quint64 address, quint64 pc, quint16 cpuId);
    quint64 readUInt64(quint64 address, quint64 pc, quint16 cpuId);



    void writeBytes(quint64 address, const quint8 *data, quint64 size, quint64 pc, quint16 cpuId);
    void writeBytes(quint64 address, const QByteArray &data, quint64 pc, quint16 cpuId);

    // SMP Reservation Management
    bool setReservation(quint16 cpuId, quint64 physicalAddr, int size);
    void clearReservation(quint16 cpuId, quint64 physicalAddr);
    bool hasReservation(quint16 cpuId, quint64 physicalAddr) const;
    void clearOverlappingReservations(quint64 physicalAddr, int size, quint16 excludeCpuId);
  
    void detachL3Cache(UnifiedDataCache *cache);

    // Cache Coherency Support
    void invalidateCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId);
    void invalidateInAttachedCaches(quint64 address, int size, quint16 sourceCpuId);
    void flushAttachedCaches(quint64 address, int size);
    void flushCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId);
    void memoryBarrierSMP(int type, quint16 sourceCpuId);

    // Legacy Compatibility Methods
    void resize(quint64 newSize, bool initialize = false);
    quint64 size() const;

    quint8 readUInt8(quint64 address, quint64 pc = 0);
    quint16 readUInt16(quint64 address, quint64 pc = 0);
    quint32 readUInt32(quint64 address, quint64 pc = 0);
    quint64 readUInt64(quint64 address, quint64 pc = 0);

    void writeUInt8(quint64 address, quint8 value, quint64 pc = 0);
    void writeUInt8(quint64 address, quint8 value, quint64 pc, quint16 cpuId);
    void writeUInt16(quint64 address, quint16 value, quint64 pc = 0);
    void writeUInt16(quint64 address, quint16 value, quint64 pc, quint16 cpuId);
    void writeUInt32(quint64 address, quint32 value, quint64 pc = 0);
    void writeUInt32(quint64 address, quint32 value, quint64 pc, quint16 cpuId);
    void writeUInt64(quint64 address, quint64 value, quint64 pc = 0);
    void writeUInt64(quint64 address, quint64 value, quint64 pc, quint16 cpuId);
    void writeBytes(quint64 address, const quint8 *data, quint64 size, quint64 pc = 0);
    void writeBytes(quint64 address, const QByteArray &data, quint64 pc = 0);

    // Utility Methods
    void mapRegion(quint64 address, quint64 size, enumMemoryPerm perm);
    void unmapRegion(quint64 address, quint64 size);
    bool isValidAddress(quint64 address, int size) const;
    void zero(quint64 address, quint64 size);
   
    void fill(quint64 address, quint64 size, quint8 value);
    QByteArray readBytes(quint64 address, quint64 size, quint64 pc = 0, quint16 cpuId = 0);

    // Statistics and Monitoring
    SMPStatistics getSMPStatistics() const;
    void resetSMPStatistics();
    QHash<quint16, CPUAccessInfo> getCPUAccessInfo() const;

    // Configuration
    void attachIRQController(IRQController *controller);
    void attachConfigLoader(ConfigLoader *loader);

  signals:
    // Memory access signals
    void sigMemoryRead(quint64 address, quint64 value, int size);
    void sigMemoryWritten(quint64 address, quint64 value, int size);
    void sigMemoryAccessSMP(quint64 address, quint64 value, int size, bool isWrite, quint16 cpuId);

    // SMP-specific signals
    void sigCPURegistered(quint16 cpuId);
    void sigCPUUnregistered(quint16 cpuId);
    void sigReservationSet(quint16 cpuId, quint64 address, int size);
    void sigReservationCleared(quint16 cpuId, quint64 address);
    void sigCacheCoherencyEvent(quint64 address, const QString &operation, quint16 sourceCpuId);

  private:
    // Core memory storage
    QByteArray m_ram;
    mutable QReadWriteLock m_memoryLock;

    // SMP CPU Management
    QVector<AlphaCPU *> m_attachedCpus;
    QHash<quint16, CPUAccessInfo> m_cpuRegistry;
    mutable QReadWriteLock m_cpuRegistryLock;

    // SMP Reservation System
    QHash<quint16, QHash<quint64, int>> m_reservations; // CPU ID -> {Address -> Size}
    mutable QReadWriteLock m_reservationLock;

    // Statistics and Monitoring
    SMPStatistics m_smpStats;
    mutable QMutex m_statsMutex;

    // External components
    IRQController *m_irqController;
    ConfigLoader *m_configLoader;

    // Private helper methods
    void updateCPUAccessTracking(quint16 cpuId, quint64 address, bool isWrite);
    void notifyCacheCoherency(quint64 address, const QString &operation, quint16 sourceCpuId);
};