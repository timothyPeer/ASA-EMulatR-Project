#pragma once

#include <QObject>
#include <QMap>
#include <QHash>
#include <QPair>
#include <QMutex>
#include <QByteArray>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QScopedPointer>
#include <QMutexLocker>
#include <QDebug>
#include <QVector>
#include "AlphaTranslationCache.h"
#include "../AEJ/constants/constAlphaMemorySystem.h"
//#include "tlbSystem.h"
#include "..\AESH\SafeMemory.h"
#include "..\AEE\MMIOManager.h"
#include "CsrWindow.h"
#include "tlbSystem.h"
#include "../AEJ/enumerations\enumCpuModel.h"
#include "TranslationResult.h"
#include "IExecutionContext.h"
#include "../AEJ/structures/structProbeResult.h"
#include "../AEJ/structures/structReservationState.h"
#include "structReservationState.h"

class AlphaCPU;          // as a replacement to a full definition include

// Define MappingEntry here:
struct MappingEntry {
	quint64 physicalBase;
	quint64 size;
	int     protectionFlags;
};






/*

Class				Role	Issues

SafeMemory			Flat contiguous physical RAM storage, no VA translation	
MMIOManager			Manages memory-mapped devices, 8/16/32/64-bit access	
AlphaMemorySystem	Virtual → Physical translation, Protection, Traps	

TODO:: 
Item	Action
Integration	AlphaMemorySystem should route ALL access through SafeMemory or MMIOManager
API Cleanup	AlphaMemorySystem should expose only virtual address operations (no direct physical access)
Memory Mapping	AlphaMemorySystem should manage VA→PA mappings fully
Protection	AlphaMemorySystem should raise protectionFault signal if access violations occur
Trap Handling	AlphaMemorySystem should raise translationMiss signal if unmapped VA accessed
Thread Safety	Locking should be handled at AlphaMemorySystem level for mappings, but SafeMemory internal locks for actual RAM
*/


/**
 * @brief Full virtual memory system for Alpha CPU
 * Supports virtual to physical translation, memory protection, MMIO access.
 */

class AlphaCPU;

class AlphaMemorySystem : public QObject {
    Q_OBJECT


        struct WriteBufferEntry {
        quint64 physicalAddr;
        quint64 value;
        quint64 timestamp;
        int size;
        bool pending;
    };

    CpuModel m_cpuModel; 
    QVector<AlphaCPU*> m_attachedCpus; // register each CPU using the attachAlphaCPU method. This is zero referenced QVector.  The position
                                       // of CPUs is set via m_cpuIdCount and AlphaCPU::getCPUId();
    quint16 m_cpuIdCount;   // Since we are hosting all CPUs on a common AlphaMemorySystem,we need the ability access a distinct CPU
public:

    /*
    ?? Dispatch based on access target (SafeMemory handles MMIO transparently)
    ??Extend memory model with page-granular mapping?
    */
    explicit AlphaMemorySystem(QObject* parent = nullptr);

    ~AlphaMemorySystem();



    void attachIrqController(IRQController* irqController) { m_irqController = irqController;  }
    void attachSafeMemory(SafeMemory* mem_) { m_safeMemory = mem_; }
    void attachMMIOManager(MMIOManager* mmio_) { m_mmioManager = mmio_; }
    
    void attachAlphaCPU(AlphaCPU* cpu_) { 
        
        initialize_AlphaCPUsignalsAndSlots();
        m_attachedCpus.insert(m_attachedCpus.count()+1,cpu_);
        m_cpuIdCount++;                                        
    }
	void attachTLBSystem(TLBSystem* tlb) { m_tlbSystem = tlb; }
    /**
   * @brief Attach the translation cache so TLB invalidations
   *        automatically flush decoded/instruction blocks.
   */
	void attachTranslationCache(AlphaTranslationCache* cache) {
		if (m_tlbSystem) {
			m_tlbSystem->attachTranslationCache(cache);
		}
	}
    void cancelPendingOperationsByASN(quint64 asn);
  

    void cancelPendingOperationsInRange(quint64 startAddr, quint64 endAddr, quint64 asn);

    bool checkAccess(quint64 vaddr, int accessType) const;
    /**
        * @brief Check TLB without causing exceptions (delegated to TLBSystem)
        * @param virtualAddr Virtual address to check
        * @param asn Address Space Number
        * @param isKernelMode True if kernel mode check
        * @return Encoded TLB check result
        */
    quint64 checkTB(quint64 virtualAddr, quint64 asn, bool isKernelMode) {
        if (m_tlbSystem) {
            return m_tlbSystem->checkTB(virtualAddr, asn, isKernelMode);
        }
        return 0; // No TLB system available
    }
    void clearMappings();
    void clearReservations(quint64 physicalAddr, int size);

    void clearCpuReservations(AlphaCPU* cpu_);
    void clearPendingTranslationDependentOperations();
    void clearProtectionCache();
    void forceMemorySystemReset();
    /**
     * @brief Check if a CPU has a reservation at an address
     *
     * @param cpu CPU to check
     * @param physAddr Physical address
     * @return true if CPU has valid reservation
     */
    bool hasReservation( AlphaCPU* cpu_, quint64 physAddr) const;

    void incrementASNClearCount();
    void incrementMappingClearCount();
    void incrementRangeClearCount();

    void initialize();
    void initializeCpuModel(CpuModel cpuModel = CpuModel::CPU_EV56) { m_cpuModel = cpuModel; }
    void initialize_AlphaCPUsignalsAndSlots();


    TLBSystem* getTlbSystem() { return m_tlbSystem; }
    /**
       * @brief Check if an address is for MMIO
       * @param physicalAddr Physical address to check
       * @return True if address is for MMIO device
  */
    bool isPALVisible(quint64 physicalAddress) const {
        // Most Alpha PALcode implementations are restricted to the lowest 512MB
        return (physicalAddress < 0x20000000);
    }

    /**
     * @brief Perform a load-locked operation
     *
     * @param cpu CPU performing the operation
     * @param vaddr Virtual address to load from
     * @param value Reference to store loaded value
     * @param size Size of load (4 or 8 bytes)
     * @param pc Program counter for fault handling
     * @return true if load succeeded, false if fault occurred
     */
    bool loadLocked( quint64 vaddr, quint64& value, int size, quint64 pc);

    void mapMemory(quint64 virtualAddr, quint64 physicalAddr, quint64 size, int protectionFlags);
    bool readVirtualMemory(quint64 virtualAddr, quint64& value, int size, quint64 pc = 0);
    bool readVirtualMemory(quint64 virtualAddr, void* value, quint16 size, quint64 pc = 0);
    bool readBlock(quint64 physicalAddr, void* buffer, size_t size, quint64 pc = 0);
    void resetMappingStatistics();
   

    void setMemoryAlloc(quint64 memory_) { m_safeMemory->resize(memory_); }
    /**
     * @brief Perform a store-conditional operation
     *
     * @param cpu CPU performing the operation
     * @param vaddr Virtual address to store to
     * @param value Value to store
     * @param size Size of store (4 or 8 bytes)
     * @param pc Program counter for fault handling
     * @return true if store succeeded, false if reservation failed
     */
    bool storeConditional(AlphaCPU* cpu_, quint64 vaddr, quint64 value, int size, quint64 pc);
    bool translate(quint64 virtualAddr, quint64& physicalAddr, int accessType);

    void unmapMemory(quint64 virtualAddr);
    bool writeBlock(quint64 physicalAddr, const void* buffer, size_t size, quint64 pc = 0);
    bool writeVirtualMemory(quint64 virtualAddress, quint64 value, int size, quint64 pc = 0);
    bool writeVirtualMemory(quint64 virtualAddr, void* value, int size, quint64 pc = 0);

    QVector<QPair<quint64, MappingEntry>> getMappedRegions() const;
    SafeMemory* getSafeMemory() { return m_safeMemory; }




    // =====================================================
    // TLB MANAGEMENT INTERFACE - For system-level operations
    // ======================================================

    void invalidateTLBEntry(quint64 virtualAddr, quint64 asn = 0) {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateEntry(virtualAddr, asn);
        }
    }

    /**
     * @brief Invalidate all TLB entries for a given ASN (PAL TBIA process).
     */
    void invalidateTLBByASN(quint64 asn) {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateByASN(asn);
        }
    }

    /**
     * @brief Alias for invalidateTLBByASN; semantic mapping for process-wide flush.
     */
    void invalidateTlbProcess(quint64 asn) {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateByASN(asn);
        }
    }

    /**
     * @brief Invalidate one instruction TLB entry (PAL TBIS for instruction pages).
     */
    void invalidateTlbSingleInstruction(quint64 virtualAddr, quint64 asn = 0) {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateEntry(virtualAddr, asn);
        }
    }

    /**
     * @brief Invalidate all TLB entries (PAL TBIA global).
     */
    void invalidateAllTlb() {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateTLB();
        }
    }

    /**
     * @brief Invalidate translation-cache entries for a specific ASN.
     */
    void invalidateTranslationCacheASN(quint64 asn) {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateTranslationCacheASN(asn);
        }
    }

    /**
     * @brief Invalidate the entire translation cache.
     */
    void invalidateTranslationCacheAll() {
        if (m_tlbSystem) {
            m_tlbSystem->invalidateTranslationCacheAll();
        }
    }
    /**
    * @brief Flush the entire TLB (alias for invalidateAllTlb()).
    */
    void flushTLB() {
        invalidateAllTlb();
    }

    /**
    * @brief Reset the memory system: flush TLB & translation-cache.
    *        Call on soft reset or re-initialization.
    */
    void reset() {
        flushTLB();
        invalidateTranslationCacheAll();
    }

    /**
     * @brief Set current Address Space Number
     */
    void setCurrentASN(quint64 asn) { m_currentASN = asn; }

#pragma region Buffer Support


    void flushWriteBuffers(quint64 startAddr, quint64 endAddr);
    void flushWriteBuffers();
    bool writeVirtualMemoryPrivileged(quint64 virtualAddr, void* value, int size, quint64 pc = 0);
    bool writeVirtualMemoryPrivileged(quint64 virtualAddr, quint64 value, int size, quint64 pc = 0);
    bool readVirtualMemoryPrivileged(quint64 virtualAddr, void* value, int size, quint64 pc = 0);
    bool readVirtualMemoryPrivileged(quint64 virtualAddr, quint64& value, int size, quint64 pc = 0);
    void addToWriteBuffer(quint64 physicalAddr, quint64 value, int size);
    /**
     * @brief Probe if a virtual address is accessible without actually accessing it
     *
     * This method checks if a virtual address can be accessed with the given
     * permissions without actually performing the memory operation. Useful for:
     * - Prefetch decisions
     * - Speculative operations
     * - Debugger queries
     * - Exception prediction
     *
     * @param context Execution context (CPU) making the probe
     * @param virtualAddress Virtual address to probe
     * @param isWrite True if checking write access, false for read access
     * @param size Access size in bytes (1, 2, 4, or 8)
     * @return True if address is accessible, false otherwise
     */
    bool probeAddress(const IExecutionContext* context,
        quint64 virtualAddress,
        bool isWrite = false,
        int size = 1) const;
    /**
     * @brief Probe address with detailed result information
     *
     * Extended version that provides detailed information about why
     * an address might not be accessible.
     *
     * @param context Execution context making the probe
     * @param virtualAddress Virtual address to probe
     * @param isWrite True if checking write access
     * @param size Access size in bytes
     * @param result Structure to receive detailed probe results
     * @return True if address is accessible
     */
    bool probeAddressDetailed(const IExecutionContext* context,
        quint64 virtualAddress,
        bool isWrite,
        int size,
        ProbeResult& result) const;
    void processWriteBuffer();
    bool isAlignmentValid(quint64 address, int size) const;
    bool isWriteBufferFull() const;

    void drainWriteBuffer();
    void commitWriteEntry(const WriteBufferEntry& entry);

    bool translatePrivileged(quint64 virtualAddr, quint64& physicalAddr);
#pragma endregion Buffer Support

public slots:
    void onAllCPUsPaused();
    void onAllCPUsStarted();
    void onAllCPUsStopped();
    void onASNMappingsCleared(quint64 asn);
    void onClearReservation(quint64 physicalAddress, int size) {

    }
    void onCpuStatusUpdate(quint8 cpuid_) {

    }
    /**
     * @brief Handle notification that memory mappings have been cleared
     *
     * This slot is called when the system clears virtual memory mappings,
     * typically during:
     * - Process termination
     * - Address space cleanup
     * - System reset
     * - Memory reconfiguration
     *
     * The memory system needs to invalidate cached translations and
     * reset related state to maintain consistency.
     */
    void onMappingsCleared();

    /**
     * @brief Handle notification that specific mapping range was cleared
     *
     * @param startAddr Starting virtual address of cleared range
     * @param endAddr Ending virtual address of cleared range
     * @param asn Address Space Number (0 = all ASNs)
     */
    void onMappingRangeCleared(quint64 startAddr, quint64 endAddr, quint64 asn = 0);

   
    /**
     * @brief Handle notification that ASN-specific mappings were cleared
     *
     * @param asn Address Space Number to clear (0 = all ASNs)
     */
    void onASNMappingsCleared(quint64 asn);

  
    void onProtectionFault(quint64 vaddr, int accessType) {

    }

    void onTranslationMiss(quint64 vaddr) {

    }

signals:
    void sigClearReservations(quint64 physicalMemory, int size);
    void sigAllCPUsPaused();
    void sigAllCPUsStarted();
    void sigAllCPUsStopped();
    void sigCpuProgress(int cpuId, QString _txt);
    void sigExecuteMemoryBarrier(int type);
    void sigHandleCacheState(quint64 physicalAddr, int state, int cpuId);
    void sigMemoryBarrierComplete();
    /**
     * @brief Emitted when memory mappings are cleared
     *
     * Other components can connect to this signal to be notified
     * when memory mappings change.
     */
    void sigMappingsCleared();
 
    /**
     * @brief Emitted when a specific mapping range is cleared
     */
    void mappingRangeCleared(quint64 startAddr, quint64 endAddr, quint64 asn);

    void sigProtectionFault(quint64 address, int accessType);
    void sigStartAll();
    void sigPauseAll();
    void sigResumeAll();
    void sigResetAll();
    void sigSystemPaused();
    void sigStopAll();
    void sigSystemStopped();

#pragma region tlb System Signals
    /**
    * @brief Emitted when TLB has been invalidated
    */
    void sigTlbInvalidated();
    void sigTranslationMiss(quint64 virtualAddress);
    void sigMemoryRead(quint64 address, quint64 value, int size);
    void sigMemoryWritten(quint64 address, quint64 value, int size);
    void sigTlbMiss(quint64 virtualAddr, bool isInstruction);
    void sigTlbFlushed();
    void sigTlbInvalidated(quint64 virtualAddr, quint64 asn);

#pragma endregion tlb System Signals

private:

    mutable QReadWriteLock memoryLock;
    SafeMemory* m_safeMemory;        // SafeMemory is expected to have been initialized by AlphaSMPManager 
    AlphaCPU* m_cpu; // AlphaCPU is expected to have been initialized by Alpha
    MMIOManager* m_mmioManager;          //    MMIOManager should have been initialized by AlphaSMPManager
    IRQController* m_irqController;
    AlphaTranslationCache* m_translationCache;   // Translation Cache
    QMap<quint64, MappingEntry> memoryMap;  // 🆕 Virtual Address → MappingEntry
    TLBSystem* m_tlbSystem;  // Owned by SMP or AlphaCPU

    QVector<WriteBufferEntry> m_writeBuffer;
    QMutex m_writeBufferMutex;
    quint64 m_writeBufferTimestamp = 0;
    	
	quint64 m_currentASN = 0; // Current processor state
    quint64 m_pageTableBase = 0; 	// Page table configuration
    QMap<quint64, MappingEntry> m_memoryMap; 	// Memory mapping for simulation
	mutable QReadWriteLock m_memoryLock; 	// Thread safety

	// Statistics
	mutable std::atomic<quint64> m_totalTranslations{ 0 };
	mutable std::atomic<quint64> m_pageFaults{ 0 };
	mutable std::atomic<quint64> m_protectionFaults{ 0 };


	// Map from CPU to its reservation state
	QHash<AlphaCPU*, ReservationState> m_reservations;



	// Statistics
	quint64 m_loadLockedCount = 0;
	quint64 m_storeConditionalSuccessCount = 0;
	quint64 m_storeConditionalFailureCount = 0;
	quint64 m_reservationClearCount = 0;

	// Helper methods
	void invalidateOverlappingReservations(quint64 physAddr, int size, AlphaCPU* excludeCpu = nullptr);
    bool translateVirtualToPhysical(AlphaCPU* cpu_, quint64 vaddr, quint64& paddr, bool isWrite);
	quint64 getCurrentTimestamp() const;

    /**
     * @brief Internal probe implementation
     */
    bool probeAddressInternal(const IExecutionContext* context,
        quint64 virtualAddress,
        bool isWrite,
        int size,
        ProbeResult* result = nullptr) const;



    // Cached page table information
	quint64 m_cachedPageTableBase = 0;
	quint64 m_cachedASN = 0;

	// Protection cache
	struct ProtectionCacheEntry {
		quint64 address = 0;
		bool isValid = false;
		bool canRead = false;
		bool canWrite = false;
		bool canExecute = false;
	} m_lastProtectionCheck;

	// Pending virtual operations
	struct PendingVirtualOperation {
		quint64 virtualAddress;
		quint64 asn;
		bool isWrite;
		int size;
		QString description;

		void cancel(const QString& reason) {
			// Implementation to cancel the operation
		}

// 	void installTsunami(MMIOManager& mmio)
// 	{
// // 		m_mmioManager->addWindow(new PCIDenseWindow(0, 0x8000'0000'0000ULL, 0x0010'0000'0000ULL));
// //         m_mmioManager->addWindow(new PCIDenseWindow(1, 0x8010'0000'0000ULL, 0x0010'0000'0000ULL));
// //         m_mmioManager->addWindow(new PCISparseWindow(0, 0x8040'0000'0000ULL, 0x0010'0000'0000ULL));
// //         m_mmioManager->addWindow(new PCISparseWindow(1, 0x8050'0000'0000ULL, 0x0010'0000'0000ULL));
// //         m_mmioManager->addWindow(new CsrWindow("tsunami", 0x8060'0000'0000ULL, 0x0010'0000'0000ULL));
// 	}


    // ═══════════════════════════════════════════════════════════════════════════
    // INTERNAL IMPLEMENTATION METHODS
    // ═══════════════════════════════════════════════════════════════════════════

	inline bool isMMIO(quint64 pa) const //catches (0x4, 0x5), dense (0x6), and the high CSR space (0x7), while anything below 0x4 is normal DRAM or reserved
	{
		const quint64 topBits = pa >> 31;      // keep <33:31>
		/* 0b100xx == 0x4–0x7 means "I/O hose"      */
		return topBits >= 0x4 && topBits <= 0x7;
	}


    bool isMapped(quint64 vaddr) const;
    /**
     * @brief Internal translation with TLB integration
     */
    TranslationResult translateInternal(quint64 virtualAddr, quint64 currentASN,
        int accessType, bool isInstruction);

    /**
     * @brief Handle TLB miss - perform page table walk
     */
    TranslationResult handleTLBMiss(quint64 virtualAddr, quint64 asn,
        int accessType, bool isInstruction);

    /**
     * @brief Perform page table walk using internal memory access
     */
    bool walkPageTable(quint64 virtualAddr, quint64 asn, quint64& physicalAddr,
        quint8& protection);

    /**
     * @brief Direct physical memory access for page table reads
     * (bypasses virtual memory for page table access)
     */


    /**
     * @brief Read physical memory directly bypassing virtual memory translation
     * Used for page table walks and other low-level operations
     */
    bool readPhysicalDirect(quint64 physicalAddr, quint64 & value, int size)
    {
        QReadLocker locker(&m_memoryLock);

        if (!m_safeMemory) {
            return false;
        }

        try {
            // Route to appropriate memory subsystem
            if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
                // MMIO access
                value = m_mmioManager->readMMIO(physicalAddr, size);
                return true;
            }
            else {
                // Direct physical memory access
                switch (size) {
                case 1:
                    value = m_safeMemory->readUInt8(physicalAddr, 0);
                    break;
                case 2:
                    value = m_safeMemory->readUInt16(physicalAddr, 0);
                    break;
                case 4:
                    value = m_safeMemory->readUInt32(physicalAddr, 0);
                    break;
                case 8:
                    value = m_safeMemory->readUInt64(physicalAddr, 0);
                    break;
                default:
                    return false;
                }
                return true;
            }
        }
        catch (...) {
            return false;
        }
    }

    /**
     * @brief Check if address should use MMIO
     */
    bool isMMIOAddress(quint64 physicalAddr) const;
  

    /**
     * @brief Route to appropriate memory subsystem
     */
    bool accessPhysicalMemory(quint64 physicalAddr, quint64 & value, int size,
        bool isWrite, quint64 pc);

	// TLB system (private implementation detail)



	

  

};
