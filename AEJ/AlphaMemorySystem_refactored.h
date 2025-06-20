#pragma once

#include "../AEJ/constants/constAlphaMemorySystem.h"
#include "../AEJ/enumerations\enumCpuModel.h"
#include "../AEJ/structures/structProbeResult.h"
#include "../AEJ/structures/structReservationState.h"
#include "../AEE/MMIOManager.h"
#include "../AEJ/SafeMemory_refactored.h"
#include "../AEJ/AlphaTranslationCache.h"
#include "CsrWindow.h"
#include "IExecutionContext.h"
#include "TranslationResult.h"
#include "../AEJ/tlbSystem.h"
#include <atomic>
#include <QAtomicInt>
#include <QAtomicInteger>
#include <QAtomicPointer>
#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QScopedPointer>
#include <QVector>
#include <QWriteLocker>
#include "../AEJ/UnifiedDataCache.h"
#include "enumerations/enumMemoryBarrierEmulationMode.h"
/*#include "tlbcacheintegrator.h"*/
#include "devicemanager.h"
#include "AlphaProcessorContext.h"



class AlphaCPU;

// Define MappingEntry here:
struct MappingEntry
{
    quint64 physicalBase;
    quint64 size;
    int protectionFlags;
};

/**
 * @brief SMP-aware CPU registration entry
 */
struct CPURegistryEntry
{
    AlphaCPU *cpu;
    quint16 cpuId;
    bool isActive;
    bool isOnline;
    QAtomicInt pendingInterrupts;
    QDateTime lastActivity;

    CPURegistryEntry() : cpu(nullptr), cpuId(0), isActive(false), isOnline(false) {}
    CPURegistryEntry(AlphaCPU *c, quint16 id)
        : cpu(c), cpuId(id), isActive(true), isOnline(true), lastActivity(QDateTime::currentDateTime())
    {
    }
};

/**
 * @brief SMP-aware memory reservation tracking
 */
struct SMPReservationState
{
    quint64 physicalAddress;
    quint64 virtualAddress;
    quint16 cpuId; // Which CPU owns this reservation
    int size;
    bool isValid;
    quint64 timestamp;
    QAtomicInt accessCount;

    SMPReservationState() : physicalAddress(0), virtualAddress(0), cpuId(0xFFFF), size(0), isValid(false), timestamp(0)
    {
    }

    bool matches(quint64 physAddr, int accessSize) const
    {
        if (!isValid)
            return false;
        quint64 aligned = physAddr & ~0x7ULL;
        return (aligned == physicalAddress) && (accessSize <= size);
    }

    void clear()
    {
        isValid = false;
        accessCount.storeRelaxed(0);
        
    }
};

/**
 * @brief Cache coherency message for inter-CPU communication
 */
struct CacheCoherencyMessage
{
    enum Type
    {
        INVALIDATE_LINE,
        FLUSH_LINE,
        WRITE_BACK,
        RESERVATION_CLEAR
    } type;

    quint64 physicalAddress;
    quint16 sourceCpuId;
    quint16 targetCpuId; // 0xFFFF = broadcast
    int size;
    quint64 timestamp;
};


// Sentinel value for no reservation
static constexpr uint64_t INVALID_RESERVATION = uint64_t(-1);

/**
 * @brief Full SMP-aware virtual memory system for Alpha CPU
 * Supports multiple CPUs with cache coherency, reservation tracking, and proper TLB management.
 */
class AlphaMemorySystem : public QObject
{
    Q_OBJECT


    uint64_t reservationAddr;

  public:
    explicit AlphaMemorySystem(QObject *parent = nullptr);
    ~AlphaMemorySystem();

/**
     * @brief Atomic Fetch wrapper: uses context to extract cpuId and PC
     * @param ctx   Processor context for CPU ID and fault PC
     * @param addr  Virtual address to fetch from
     * @param out   Out param receiving the loaded 64-bit value
     * @return      True on success; false if a memory fault occurred
     */
    inline bool atomicFetch(AlphaProcessorContext *ctx, quint64 addr, quint64 &out)
    {
        quint16 cpuId = ctx->cpuId();
        quint64 pc = ctx->getProgramCounter();
        // Invoke existing API without changing signature
        if (!readVirtualMemory(cpuId, addr, out, 8, pc))
        {
            return false; // fault captured by ctx
        }
        // Record reservation address
        reservationAddr = addr;
        return true;
    }

    /**
     * @brief Atomic Fetch-Modify wrapper: uses context to extract cpuId and PC
     * @param ctx   Processor context for CPU ID and fault PC
     * @param addr  Virtual address to fetch-modify from
     * @param out   Out param receiving the loaded 64-bit value
     * @return      True on success; false if a memory fault occurred
     */
    inline bool atomicFetchModify(AlphaProcessorContext *ctx, uint64_t addr, uint64_t &out)
    {
        quint16 cpuId = ctx->cpuId();
        quint64 pc = ctx->getProgramCounter();
        if (!readVirtualMemory(cpuId, addr, out, 8, pc))
        {
            return false; // fault captured by ctx
        }
        // Clear reservation for this CPU
        reservationAddr = INVALID_RESERVATION;
        return true;
    }
 
	bool readWithoutFault(quint64 address, quint64 &value, size_t size);
    //=========================
    // SMP CPU MANAGEMENT
    //=========================

    /**
     * @brief Register a CPU with the memory system
     * @param cpu CPU instance to register
     * @param cpuId Unique CPU identifier (0-based)
     * @return True if registration successful
     */
    bool registerCPU(AlphaCPU *cpu, quint16 cpuId);

	void updateCPUContext(quint16 cpuId, quint64 newASN);
    /**
     * @brief Unregister a CPU from the memory system
     * @param cpuId CPU identifier to unregister
     * @return True if deregistration successful
     */
    bool unregisterCPU(quint16 cpuId);

    /**
     * @brief Get CPU by ID
     * @param cpuId CPU identifier
     * @return CPU instance or nullptr if not found
     */
    AlphaCPU *getCPU(quint16 cpuId) const;

    /**
     * @brief Get all registered CPUs
     * @return Vector of CPU registry entries
     */
    QVector<CPURegistryEntry> getAllCPUs() const;

    /**
     * @brief Get number of registered CPUs
     * @return CPU count
     */
    quint16 getCPUCount() const;

    /**
     * @brief Mark CPU as online/offline
     * @param cpuId CPU identifier
     * @param isOnline True if CPU is online
     */
    void setCPUOnlineStatus(quint16 cpuId, bool isOnline);


    //=========================
    // COMPONENT ATTACHMENTS (Legacy compatibility)
    //=========================

    void attachIrqController(IRQController *irqController) { m_irqController = irqController; }
    void attachSafeMemory(SafeMemory *mem_) { m_safeMemory = mem_; }
    void attachMMIOManager(MMIOManager *mmio_) { m_mmioManager = mmio_; }
    void attachTranslationCache(AlphaTranslationCache *cache)
    {
        if (m_tlbSystem)
        {
            m_tlbSystem->attachTranslationCache(cache);
        }
    }

   /**
     * @brief Attach L3 shared cache to memory system
     * @param l3Cache Shared L3 cache instance
     */
    void attachL3Cache(UnifiedDataCache *l3Cache);
	void attachTLBCacheIntegrator(tlbCacheIntegrator *integrator);
    /**
     * @brief Create and configure L3 shared cache
     * @param config L3 cache configuration
     * @return Pointer to created L3 cache
     */
    UnifiedDataCache *createL3Cache(const UnifiedDataCache::Config &config);

    UnifiedDataCache *getL3Cache() const { return m_level3SharedCache; }
    // Cache-aware memory operations
    bool readVirtualMemoryWithCache(quint16 cpuId, quint64 virtualAddr, quint64 &value, int size, quint64 pc);
    bool writeVirtualMemoryWithCache(quint16 cpuId, quint64 virtualAddr, quint64 value, int size, quint64 pc);


    //=========================
    // MEMORY OPERATIONS (SMP-aware)
    //=========================

   	bool readPhysicalMemory(quint64 physicalAddr, quint64 &value, size_t size);

    bool writePhysicalMemory(quint64 physicalAddr, quint64 value, size_t size);
    /**
     * @brief Read from virtual memory with CPU context
     * @param cpuId CPU making the request
     * @param virtualAddr Virtual address to read
     * @param value Output value
     * @param size Access size
     * @param pc Program counter
     * @return True if successful
     */
    bool readVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 &value, int size, quint64 pc = 0);
    bool readVirtualMemory(quint16 cpuId, quint64 virtualAddr, void *value, quint16 size, quint64 pc = 0);


	bool readVirtualMemory(quint64 virtualAddr, quint64 &value, int size, quint64 pc);
    bool readVirtualMemory(quint64 virtualAddr, void *value, quint16 size, quint64 pc);
    /**
     * @brief Write to virtual memory with CPU context and cache coherency
     * @param cpuId CPU making the request
     * @param virtualAddr Virtual address to write
     * @param value Value to write
     * @param size Access size
     * @param pc Program counter
     * @return True if successful
     */
    bool writeVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 value, int size, quint64 pc = 0);
    bool writeVirtualMemory(quint16 cpuId, quint64 virtualAddr, void *value, int size, quint64 pc = 0);

	bool writeVirtualMemory(quint64 virtualAddr, quint64 value, int size, quint64 pc);
    bool writeVirtualMemory(quint64 virtualAddr, void *value, int size, quint64 pc);
    bool isPageMapped(quint64 virtualAddress, quint64 asn, bool isWrite);
    bool isKernelAddress(quint64 address);
    bool isKernelMode();
    bool isWritableAddress(quint64 address);
    //=========================
    // SMP LOAD-LOCKED/STORE-CONDITIONAL SUPPORT
    //=========================

    /**
     * @brief Perform SMP-aware load-locked operation
     * @param cpuId CPU performing the operation
     * @param vaddr Virtual address to load from
     * @param value Reference to store loaded value
     * @param size Size of load (4 or 8 bytes)
     * @param pc Program counter for fault handling
     * @return True if load succeeded
     */
    bool loadLocked(quint16 cpuId, quint64 vaddr, quint64 &value, int size, quint64 pc);

	bool loadLocked(quint64 vaddr, quint64 &value, int size, quint64 pc);
    /**
     * @brief Perform SMP-aware store-conditional operation
     * @param cpuId CPU performing the operation
     * @param vaddr Virtual address to store to
     * @param value Value to store
     * @param size Size of store (4 or 8 bytes)
     * @param pc Program counter for fault handling
     * @return True if store succeeded
     */
    bool storeConditional(quint16 cpuId, quint64 vaddr, quint64 value, int size, quint64 pc);


	
    /**
     * @brief Clear reservations for all CPUs at physical address
     * @param physicalAddr Physical address
     * @param size Access size
     * @param excludeCpuId CPU to exclude from clearing (optional)
     */
    void clearReservations(quint64 physicalAddr, int size, quint16 excludeCpuId = 0xFFFF);

    /**
     * @brief Clear all reservations for a specific CPU
     * @param cpuId CPU identifier
     */
    void clearCpuReservations(quint16 cpuId);

	quint64 getVPTB(quint64 asn);
    /**
     * @brief Check if CPU has reservation at address
     * @param cpuId CPU identifier
     * @param physAddr Physical address
     * @return True if CPU has valid reservation
     */
    bool hasReservation(quint16 cpuId, quint64 physAddr) const;

    //=========================
    // CACHE COHERENCY AND SMP COORDINATION
    //=========================

    /**
     * @brief Send cache coherency message to CPUs
     * @param message Coherency message to send
     */
    void sendCacheCoherencyMessage(const CacheCoherencyMessage &message);

    void setPhysicalMemoryRegion(quint64 base, quint64 size)
    {
        m_physicalMemoryBase = base;
        m_physicalMemorySize = size;
    }

    void setKernelMemoryRegion(quint64 base, quint64 size)
    {
        m_kernelMemoryBase = base;
        m_kernelMemorySize = size;
    }

    void setAlignmentEnforcement(bool enforce) { m_enforceAlignment = enforce; }
    void attachAlphaProcessorContext(AlphaProcessorContext* ctx) { m_processorContext = ctx;  }
    void attachDeviceManager(DeviceManager *deviceManager) { m_deviceManager = deviceManager; }

    /**
     * @brief Invalidate cache lines on all CPUs for address range
     * @param physicalAddr Physical address
     * @param size Size of region
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId);

    /**
     * @brief Flush cache lines on all CPUs for address range
     * @param physicalAddr Physical address
     * @param size Size of region
     * @param sourceCpuId CPU that initiated the flush
     */
    void flushCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId);

    //=========================
    // TLB MANAGEMENT (SMP-aware)
    //=========================

    /**
     * @brief Invalidate TLB entry on all CPUs
     * @param virtualAddr Virtual address
     * @param asn Address Space Number (0 = all ASNs)
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateTLBEntry(quint64 virtualAddr, quint64 asn = 0, quint16 sourceCpuId = 0xFFFF);

/**
     * @brief Invalidate single TLB entry across all CPUs (both instruction and data)
     * Uses internal TLB system first, then notifies external CPU objects
     * @param virtualAddr Virtual address to invalidate
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateTlbSingle(quint64 virtualAddr, quint16 sourceCpuId);
    /**
     * @brief Invalidate data TLB entries across all CPUs
     * @param virtualAddr Virtual address to invalidate
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateTlbSingleData(quint64 virtualAddr, quint16 sourceCpuId);
    /**
     * @brief Invalidate instruction TLB entries across all CPUs
     * @param virtualAddr Virtual address to invalidate
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateTlbSingleInstruction(quint64 virtualAddr, quint16 sourceCpuId);
    /**
     * @brief Invalidate all TLB entries for specific ASN across all CPUs
     * @param asn Address Space Number to invalidate
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateTLBByASN(quint64 asn, quint16 sourceCpuId = 0xFFFF);

	void integrateTLBWithCaches();
    /**
     * @brief Global TLB flush across all CPUs
     * @param sourceCpuId CPU that initiated the invalidation
     */
    void invalidateAllTLB(quint16 sourceCpuId = 0xFFFF);

     /**
     * @brief Enhanced TLB invalidation with performance monitoring
     * @param virtualAddr Virtual address to invalidate (0 = all addresses)
     * @param asn Address Space Number (0 = all ASNs)
     * @param sourceCpuId Source CPU
     * @param invalidationType Type of invalidation for statistics
     */
    void invalidateTLBWithMonitoring(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId,
                                     const QString &invalidationType);

    //=========================
    // MEMORY MAPPING (unchanged interface)
    //=========================

    void mapMemory(quint64 virtualAddr, quint64 physicalAddr, quint64 size, int protectionFlags);
    bool translateAddressNonFaulting(quint64 virtualAddress, quint64 &physicalAddress, bool isWrite);
    bool translateViaPageTable(quint64 virtualAddress, quint64 &physicalAddress, bool allowFault);
    void unmapMemory(quint64 virtualAddr);
    void clearMappings();
    bool checkAccess(quint64 vaddr, int accessType) const;
    bool translate(quint64 virtualAddr, quint64 &physicalAddr, int accessType);
    QVector<QPair<quint64, MappingEntry>> getMappedRegions() const;

    //=========================
    // UTILITY METHODS
    //=========================

    void initialize();
    void raiseMemoryAccessException(quint64 address, int size, bool isWrite, quint64 pc);
    void raiseTLBMiss(quint64 virtualAddress, quint64 asn);
    void initializeCpuModel(CpuModel cpuModel = CpuModel::CPU_EV56) { m_cpuModel = cpuModel; }
    bool readBlock(quint64 physicalAddr, void *buffer, size_t size, quint64 pc = 0);
    bool wouldCauseTLBMiss(quint64 virtualAddress, quint64 asn, bool isWrite);
    bool wouldCauseTLBMissSimple(quint64 virtualAddress, quint64 asn, bool isWrite);
    bool writeBlock(quint64 physicalAddr, const void *buffer, size_t size, quint64 pc = 0);
    SafeMemory *getSafeMemory() { return m_safeMemory; }
    //TLBSystem *getTlbSystem() { return m_tlbSystem; }
    TLBSystem *getTlbSystem() { return m_tlbSystem; }

    // Performance and statistics
    void resetMappingStatistics();
    quint64 getCurrentPC();
    quint64 getCurrentPS();
    quint64 getCurrentTimestamp() const;


    //=========================
    // ADVANCED SMP FEATURES
    //=========================

    /**
     * @brief Perform atomic read operation that coordinates with all CPUs
     * @param cpuId CPU performing the operation
     * @param virtualAddr Virtual address to read
     * @param value Buffer to store read data
     * @param size Size of read operation
     * @param pc Program counter for fault handling
     * @return True if read succeeded
     */
    bool readVirtualMemoryAtomic(quint16 cpuId, quint64 virtualAddr, void *value, int size, quint64 pc = 0);

    /**
     * @brief Perform conditional write operation (compare-and-swap style)
     * @param cpuId CPU performing the operation
     * @param virtualAddr Virtual address to write
     * @param value Buffer containing new value to write
     * @param size Size of write operation
     * @param expectedValue Expected current value at address
     * @param pc Program counter for fault handling
     * @return True if write succeeded (current value matched expected)
     */
    bool writeVirtualMemoryConditional(quint16 cpuId, quint64 virtualAddr, void *value, int size, quint64 expectedValue,
                                       quint64 pc = 0);

    /**
     * @brief Flush write buffers for specific CPU and coordinate with others
     * @param cpuId CPU to flush buffers for
     */
    void flushWriteBuffers(quint16 cpuId);

	void executeAlphaMB(quint16 cpuId);
    void executeAlphaWMB(quint16 cpuId);
    void executeLoadLockedBarrier(quint16 cpuId);
    void executeStrictMemoryBarrier(memoryBarrierEmulationModeType type, quint16 cpuId);
    void executePALBarrier(quint16 cpuId);
    /**
     * @brief Execute memory barrier and coordinate across all CPUs
     * @param type Barrier type (0=read barrier, 1=write barrier, 2=full barrier)
     * @param cpuId CPU executing the barrier
     */
    void executeMemoryBarrier(memoryBarrierEmulationModeType type, quint16 cpuId);

	void executeStoreConditionalBarrier(quint16 cpuId);
    //=========================
    // DEBUGGING AND DIAGNOSTICS
    //=========================

    /**
     * @brief Dump complete system state to debug log
     */
    void dumpSystemState() const;

    /**
     * @brief Get formatted system status string
     * @return Status string with key metrics
     */
    QString getSystemStatus() const;

    /**
     * @brief Get count of online CPUs
     * @return Number of CPUs currently online
     */
    quint16 getOnlineCPUCount() const;

    // Add to legacy compatibility section:

    /**
     * @brief Legacy store-conditional (uses CPU 0)
     * @param vaddr Virtual address
     * @param value Value to store
     * @param size Store size
     * @param pc Program counter
     * @return True if store succeeded
     */
    bool storeConditional(quint64 vaddr, quint64 value, int size, quint64 pc = 0);

    // Add to private section after existing helper methods:

    /**
     * @brief Process pending cache coherency messages queue
     */
    void processPendingCoherencyMessages();

     /**
     * @brief Validate TLB system integrity and health
     * @return True if TLB system appears healthy
     */
    bool validateTLBSystemIntegrity();

  public slots:
    // SMP-aware event handling
    void onCacheCoherencyEvent(quint64 physicalAddr, quint16 sourceCpuId, const QString &eventType);
    void onCPUStateChanged(quint16 cpuId, int newState);
    void onMappingsCleared();
    void onMappingRangeCleared(quint64 startAddr, quint64 endAddr, quint64 asn = 0);
    void onASNMappingsCleared(quint64 asn);

	excTLBException determineTLBExceptionType(quint64 virtualAddress, quint64 asn);
    MemoryFaultType determineMemoryFaultType(quint64 address, int size, bool isWrite);
  signals:
    // SMP-specific signals
    void sigCacheCoherencyEvent(quint64 physicalAddr, quint16 sourceCpuId, const QString &eventType);
    void sigCPURegistered(quint16 cpuId);
    void sigCPUUnregistered(quint16 cpuId);
    void sigCPUOnlineStatusChanged(quint16 cpuId, bool isOnline);
    void sigReservationCleared(quint16 cpuId, quint64 physicalAddr, int size);
    void sigMemoryWriteNotification(quint64 physicalAddr, int size, quint16 sourceCpuId);

    // Standard signals
    void sigProtectionFault(quint64 address, int accessType);
    void sigTranslationMiss(quint64 virtualAddress);
    void sigMemoryRead(quint64 address, quint64 value, int size);
    void sigMemoryWritten(quint64 address, quint64 value, int size);
    void sigMappingsCleared();
    void sigTlbInvalidated();
    /**
     * @brief Signal emitted with TLB invalidation performance metrics
     * @param invalidationType Type of invalidation performed
     * @param entriesInvalidated Number of entries invalidated
     * @param timeMicroseconds Time taken in microseconds
     * @param cpuCount Number of CPUs affected
     */
    void sigTLBInvalidationPerformance(const QString &invalidationType, int entriesInvalidated, qint64 timeMicroseconds,
                                       int cpuCount);
    void sigTlbFlushed();
    /**
     * @brief Signal emitted when TLB system encounters repeated errors
     * @param errorCount Number of errors encountered
     * @param errorType Description of error type
     */
    void sigTLBSystemError(int errorCount, const QString &errorType);

  private:
    //=========================
    // SMP STATE MANAGEMENT
    //=========================

    // CPU registry with thread-safe access
    mutable QReadWriteLock m_cpuRegistryLock;
    QHash<quint16, CPURegistryEntry> m_cpuRegistry;
    QAtomicInt m_nextCpuId;

    // Memory reservations per CPU
    mutable QReadWriteLock m_reservationsLock;
    QHash<quint16, SMPReservationState> m_reservations;

    // Cache coherency coordination
    QMutex m_coherencyLock;
    QQueue<CacheCoherencyMessage> m_pendingCoherencyMessages;

    // Performance counters (per-system)
    QAtomicInt m_totalMemoryAccesses;
    QAtomicInt m_cacheCoherencyEvents;
    QAtomicInt m_reservationConflicts;
    QAtomicInt m_tlbInvalidations;

    //=========================
    // EXISTING COMPONENTS (unchanged)
    //=========================

    mutable QReadWriteLock m_memoryLock;
    SafeMemory *m_safeMemory;
    MMIOManager *m_mmioManager;
    DeviceManager *m_deviceManager; // Device management
    IRQController *m_irqController;
    AlphaTranslationCache *m_translationCache;
    AlphaProcessorContext *m_processorContext;
    TLBSystem *m_tlbSystem;
    CpuModel m_cpuModel;
    // Memory configuration
    quint64 m_physicalMemoryBase;
    quint64 m_physicalMemorySize;
    quint64 m_kernelMemoryBase;
    quint64 m_kernelMemorySize;

    // Policy settings
    bool m_enforceAlignment;






    UnifiedDataCache *m_level3SharedCache; // Shared across all CPUs
    StackManager *m_exceptionHandler = nullptr; // Use the StackManager - which supports an exception stack.
    AlphaCPU *m_currentCPU = nullptr; // Current CPU for context

    QMap<quint64, MappingEntry> m_memoryMap;
    quint64 m_currentASN = 0;

    // Statistics
//     mutable std::atomic<quint64> m_totalTranslations{0};
//     //QAtomicInt<quint64> m_totalTranslations{0};
//     mutable std::atomic<quint64> m_pageFaults{0};
//     mutable std::atomic<quint64> m_protectionFaults{0};
    mutable QAtomicInteger<quint64> m_totalTranslations{0};
    mutable QAtomicInteger<quint64> m_pageFaults{0};
    mutable QAtomicInteger<quint64> m_protectionFaults{0};




    // L3 cache integration
    void setupL3CacheIntegration();
    void configureL3CacheHierarchy();

    /*
    Internal TLB Access - A VA->PA Integration with Translation Lookup
    */
    /**
     * @brief Get internal TLB system (for internal use only)
     * @return Internal TLB system pointer
     */
    TLBSystem *getInternalTLBSystem() { return m_tlbSystem; }

    /**
     * @brief Attach instruction cache to internal TLB system
     * @param icache Instruction cache instance
     */
    void attachInstructionCacheToTLB(UnifiedDataCache *icache)
    {
        if (m_tlbSystem)
        {
            m_tlbSystem->attachInstructionCache(icache);
        }
    }


    //=========================
    // PRIVATE HELPER METHODS
    //=========================

     /**
     * @brief Original instruction entry determination (now called by safe version)
     * @param virtualAddr Virtual address being accessed
     * @param isInstruction True if this was explicitly an instruction fetch
     * @param accessType Access type (0=read, 1=write, 2=execute)
     * @return True if this should be marked as an instruction entry
     */
    bool determineInstructionEntry(quint64 virtualAddr, bool isInstruction, int accessType);
  
     /**
     * @brief Original protection flags determination (now called by safe version)
     * @param virtualAddr Virtual address being accessed
     * @param accessType Access type (0=read, 1=write, 2=execute)
     * @param isInstruction True if this was an instruction fetch
     * @return Alpha TLB protection flags
     */
    quint32 determineProtectionFlags(quint64 virtualAddr, int accessType, bool isInstruction);
    /**
     * @brief Original global mapping check (now called by safe version)
     * @param virtualAddr Virtual address to check
     * @return True if this should be a global mapping
     */
    bool isGlobalMapping(quint64 virtualAddr);
    /**
     * @brief Find CPU registry entry by ID (assumes lock held)
     */
    CPURegistryEntry *findCPUEntry(quint16 cpuId);
    const CPURegistryEntry *findCPUEntry(quint16 cpuId) const;

     /**
     * @brief Original helper for memory map TLB entries (now called by safe version)
     * @param entry Output TLB entry to populate
     * @param virtualAddr Source virtual address
     * @param physicalAddr Translated physical address
     * @param asn Current Address Space Number
     * @param mapEntry Memory map entry with protection info
     * @param isInstruction True if this was an instruction fetch
     */
    void populateTLBEntryFromMemoryMap(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr, quint64 asn,
                                       const MappingEntry &mapEntry, bool isInstruction);

     /**
     * @brief Original helper functions for TLB entry creation (now called by safe versions)
     * @param entry Output TLB entry to populate
     * @param virtualAddr Source virtual address
     * @param physicalAddr Translated physical address
     * @param asn Current Address Space Number
     * @param accessType Access type (0=read, 1=write, 2=execute)
     * @param isInstruction True if this was an instruction fetch
     */
    void populateTLBEntryFromTranslation(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr, quint64 asn,
                                         int accessType, bool isInstruction);
 
    /**
     * @brief Validate CPU ID and get CPU instance
     */
    AlphaCPU *validateAndGetCPU(quint16 cpuId) const;

    /**
     * @brief Send message to specific CPU
     */
    void sendMessageToCPU(quint16 cpuId, const CacheCoherencyMessage &message);

   /* void safeIncrement(std::atomic<quint64> &counter);*/

    /**
     * @brief Broadcast message to all CPUs except source
     */
    void broadcastMessage(const CacheCoherencyMessage &message);

	void broadcastMessage(memoryBarrierEmulationModeType type, quint16 cputId);
   



    /**
     * @brief Invalidate overlapping reservations (SMP-aware)
     */
    void invalidateOverlappingReservations(quint64 physAddr, int size, quint16 excludeCpuId);

    /**
     * @brief Internal translation with CPU context
     */
    TranslationResult translateInternal(quint16 cpuId, quint64 virtualAddr, int accessType, bool isInstruction);

    /**
     * @brief Check if address is MMIO
     */
    bool isMMIOAddress(quint64 physicalAddr) const;

    /**
     * @brief Access physical memory with coherency handling
     */
    bool accessPhysicalMemory(quint64 physicalAddr, quint64 &value, int size, bool isWrite, quint64 pc, quint16 cpuId);

     /**
     * @brief Safe TLB entry population with comprehensive error checking
     * @param cpuId CPU identifier
     * @param virtualAddr Virtual address
     * @param physicalAddr Physical address
     * @param asn Address Space Number
     * @param accessType Access type (0=read, 1=write, 2=execute)
     * @param isInstruction True if instruction fetch
     * @return True if TLB entry was safely created and inserted
     */
    bool safeTLBPopulation(quint16 cpuId, quint64 virtualAddr, quint64 physicalAddr, quint64 asn, int accessType,
                           bool isInstruction);

    /**
     * @brief Safe TLB population from memory map with validation
     * @param cpuId CPU identifier
     * @param virtualAddr Virtual address
     * @param physicalAddr Physical address
     * @param asn Address Space Number
     * @param mapEntry Memory map entry
     * @param isInstruction True if instruction fetch
     * @return True if successfully populated
     */
    bool safeTLBPopulationFromMemoryMap(quint16 cpuId, quint64 virtualAddr, quint64 physicalAddr, quint64 asn,
                                        const MappingEntry &mapEntry, bool isInstruction);

    /**
     * @brief Exception-safe TLB entry creation
     * @param entry Output TLB entry
     * @param virtualAddr Virtual address
     * @param physicalAddr Physical address
     * @param asn Address Space Number
     * @param accessType Access type
     * @param isInstruction True if instruction fetch
     * @return True if entry was safely created
     */
    bool populateTLBEntrySafe(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr, quint64 asn, int accessType,
                              bool isInstruction);

    /**
     * @brief Exception-safe memory map TLB entry creation
     * @param entry Output TLB entry
     * @param virtualAddr Virtual address
     * @param physicalAddr Physical address
     * @param asn Address Space Number
     * @param mapEntry Memory map entry
     * @param isInstruction True if instruction fetch
     * @return True if entry was safely created
     */
    bool populateTLBEntryFromMemoryMapSafe(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr, quint64 asn,
                                           const MappingEntry &mapEntry, bool isInstruction);

    /**
     * @brief Validate TLB entry before insertion to prevent corruption
     * @param entry TLB entry to validate
     * @param cpuId CPU that will own this entry
     * @return True if entry is valid and safe
     */
    bool validateTLBEntry(const TLBEntry &entry, quint16 cpuId);

    /**
     * @brief Safe protection flag determination with error checking
     * @param virtualAddr Virtual address
     * @param accessType Access type
     * @param isInstruction True if instruction fetch
     * @param protectionFlags Output protection flags
     * @return True if flags were safely determined
     */
    bool determineProtectionFlagsSafe(quint64 virtualAddr, int accessType, bool isInstruction,
                                      quint32 &protectionFlags);

    /**
     * @brief Safe instruction entry determination with fallback
     * @param virtualAddr Virtual address
     * @param isInstruction True if instruction fetch
     * @param accessType Access type
     * @return True if this should be an instruction entry (safe fallback)
     */
    bool determineInstructionEntrySafe(quint64 virtualAddr, bool isInstruction, int accessType);

    /**
     * @brief Validate virtual address ranges
     * @param virtualAddr Virtual address to validate
     * @return True if address is in valid range
     */
    bool isValidVirtualAddress(quint64 virtualAddr) const;

    /**
     * @brief Validate physical address ranges
     * @param physicalAddr Physical address to validate
     * @return True if address is in valid range
     */
    bool isValidPhysicalAddress(quint64 physicalAddr) const;

    /**
     * @brief Validate TLB protection flag combinations
     * @param protectionFlags Protection flags to validate
     * @return True if flag combination is valid
     */
    bool isValidProtectionFlags(quint32 protectionFlags) const;

    /**
     * @brief Emergency TLB cleanup for error recovery
     * @param cpuId CPU to clean up (0xFFFF = all CPUs)
     */
    void emergencyTLBCleanup(quint16 cpuId);

    /**
     * @brief Global error recovery function for TLB-related issues
     * @param cpuId CPU that experienced the error (0xFFFF = all CPUs)
     * @param errorType Type of error encountered
     */
    void handleTLBError(quint16 cpuId, const QString &errorType);
};
