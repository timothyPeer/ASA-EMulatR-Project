#pragma once

#include "../AEJ/constants/const_OpCode_0_PAL.h"
#include <QFuture>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QVector>
#include <QWaitCondition>
#include <QtConcurrent>
#include <atomic>
#include "structures/structPALInstruction.h"
#include "FPException.h"
#include "IllegalInstructionException.h"
#include "TLBExceptionQ.h"

// Forward declarations
class AlphaCPU;
class UnifiedDataCache;
class AlphaInstructionCache;
class AlphaTranslationCache;
class AlphaBarrierExecutor;
class executorAlphaFloatingPoint;
class opcode11_executorAlphaIntegerLogical;
struct DecodedInstruction;


/**
 * @brief High-performance Alpha PAL (Privileged Architecture Library) Executor
 *
 * Implements OpCode 0 PAL instructions with:
 * - Async pipeline execution for non-blocking system operations
 * - Cache-aware execution with L1/L2/L3 integration
 * - JIT-optimized instruction processing
 * - MESI protocol coordination for SMP systems
 * - Privilege level enforcement and system state management
 *
 * PAL instructions are the foundation of Alpha system software, providing
 * atomic operations for OS kernels, exception handling, and hardware control.
 */
class executorAlphaPAL : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;

    // Integration with other execution units
    AlphaBarrierExecutor *m_barrierExecutor;
    executorAlphaFloatingPoint *m_fpExecutor;
    opcode11_executorAlphaIntegerLogical *m_intExecutor;

    // Cache hierarchy (shared with other executors)
    QSharedPointer<AlphaInstructionCache> m_instructionCache;
    QSharedPointer<UnifiedDataCache> m_level1DataCache;
    QSharedPointer<UnifiedDataCache> m_level2Cache;
    QSharedPointer<UnifiedDataCache> m_level3Cache;

    // TLB integration for virtual memory operations
    QSharedPointer<AlphaTranslationCache> m_iTLB;
    QSharedPointer<AlphaTranslationCache> m_dTLB;

    // Async pipeline configuration
    static constexpr int MAX_PIPELINE_DEPTH = 6; // PAL ops are typically serialized
    static constexpr int MAX_CACHE_REQUESTS = 16;
    static constexpr int MAX_BARRIER_WAIT = 5000; // 5 second timeout for barriers

    // Pipeline stages
    QQueue<PALInstruction> m_fetchQueue;
    QQueue<PALInstruction> m_decodeQueue;
    QQueue<PALInstruction> m_executeQueue;
    QQueue<PALInstruction> m_writebackQueue;

    // Pipeline synchronization
    mutable QMutex m_pipelineMutex;
    QWaitCondition m_pipelineCondition;
    std::atomic<bool> m_pipelineActive{false};
    std::atomic<quint64> m_sequenceCounter{0};

    // PAL execution state
    std::atomic<bool> m_kernelModeRequired{true};
    std::atomic<bool> m_systemCallInProgress{false};
    std::atomic<quint64> m_pendingIPRWrites{0};

    // Performance counters
    mutable QMutex m_statsMutex;
    QAtomicInt m_palInstructions = 0;
    QAtomicInt m_systemCalls = 0;
    QAtomicInt m_privilegeViolations = 0;
    QAtomicInt m_iprOperations = 0;
    QAtomicInt m_tlbOperations = 0;
    QAtomicInt m_cacheFlushes = 0;
    QAtomicInt m_contextSwitches = 0;

    // Cache performance (PAL-specific)
    QAtomicInt m_l1ICacheHits = 0;
    QAtomicInt m_l1ICacheMisses = 0;
    QAtomicInt m_l1DCacheHits = 0;
    QAtomicInt m_l1DCacheMisses = 0;
    QAtomicInt m_l2CacheHits = 0;
    QAtomicInt m_l2CacheMisses = 0;
    QAtomicInt m_l3CacheHits = 0;
    QAtomicInt m_l3CacheMisses = 0;

    // Pipeline workers
    QFuture<void> m_fetchWorker;
    QFuture<void> m_decodeWorker;
    QFuture<void> m_executeWorker;
    QFuture<void> m_writebackWorker;

    // JIT optimization tracking
    QMap<quint32, quint64> m_functionExecutionCount;
    QSet<quint32> m_frequentFunctions; // Functions executed > 1000 times
    QSet<quint32> m_criticalFunctions; // Functions on critical path

  public:
    explicit executorAlphaPAL(AlphaCPU *cpu, QObject *parent = nullptr);
    ~executorAlphaPAL();

	bool readMemoryWithFaultHandling(quint64 address, quint64 &value, const PALInstruction &instr);
    bool readMemoryWithoutFault(quint64 address, quint64 &value);
    // Execution unit integration
    void attachBarrierExecutor(AlphaBarrierExecutor *barrierExecutor) { m_barrierExecutor = barrierExecutor; }
    void attachFloatingPointExecutor(executorAlphaFloatingPoint *fpExecutor) { m_fpExecutor = fpExecutor; }
    void attachIntegerExecutor(opcode11_executorAlphaIntegerLogical *intExecutor) { m_intExecutor = intExecutor; }

    // Cache and TLB attachment
    void attachInstructionCache(QSharedPointer<AlphaInstructionCache> icache) { m_instructionCache = icache; }
    void attachLevel1DataCache(QSharedPointer<UnifiedDataCache> l1dcache) { m_level1DataCache = l1dcache; }
    void attachLevel2Cache(QSharedPointer<UnifiedDataCache> l2cache) { m_level2Cache = l2cache; }
    void attachLevel3Cache(QSharedPointer<UnifiedDataCache> l3cache) { m_level3Cache = l3cache; }
    void attachTranslationCache(QSharedPointer<AlphaTranslationCache> iTLB, QSharedPointer<AlphaTranslationCache> dTLB)
    {
        m_iTLB = iTLB;
        m_dTLB = dTLB;
    }

    // Pipeline control
    void startAsyncPipeline();
    void stopAsyncPipeline();
    bool isAsyncPipelineActive() const { return m_pipelineActive.load(); }

    // Instruction submission
    bool submitInstruction(const DecodedInstruction &instruction, quint64 pc);

    // Synchronous execution (for critical PAL operations)
    bool executePALInstruction(const DecodedInstruction &instruction);

    // PAL function categories
    bool executeSystemCall(const PALInstruction &instr);
    bool executeMemoryManagement(const PALInstruction &instr);
    bool executePrivilegeOperation(const PALInstruction &instr);
    bool executePerformanceCounter(const PALInstruction &instr);
    bool executeContextSwitch(const PALInstruction &instr);

    // Statistics and monitoring
    void printStatistics() const;
    void clearStatistics();
    void printJITOptimizationStats() const;

    // System state queries
    bool isSystemCallInProgress() const { return m_systemCallInProgress.load(); }
    quint64 getPendingIPRWrites() const { return m_pendingIPRWrites.load(); }


  private slots:
    void onInstructionCacheHit(quint64 address);
    void onInstructionCacheMiss(quint64 address);
    void onCacheLineInvalidated(quint64 address);
    void onCacheCoherencyEvent(quint64 address, const QString &eventType);

  signals:
    void palInstructionExecuted(quint32 function, bool success, int cycles);
    void privilegeViolation(quint32 function, quint64 pc);
    void systemCallInvoked(quint32 function, quint64 pc);
    void contextSwitchRequested(quint64 oldContext, quint64 newContext);
    void cacheFlushRequested(const QString &type);
    void tlbOperationCompleted(const QString &operation, int entriesAffected);

  private:
    // Pipeline workers
    void fetchWorker();
    void decodeWorker();
    void executeWorker();
    void writebackWorker();

    // Instruction analysis and JIT optimization
    void analyzePALInstruction(PALInstruction &instr);
    void updateJITStats(quint32 function);
    bool isFrequentFunction(quint32 function) const;
    bool isCriticalFunction(quint32 function) const;

    // Common PAL instruction implementations
    bool executeHALT(const PALInstruction &instr);
    bool executeCFLUSH(const PALInstruction &instr);
    bool executeDRAINA(const PALInstruction &instr);
    bool executeSWPCTX(const PALInstruction &instr);
    bool executeCSERVE(const PALInstruction &instr);

    // TLB management
    bool executeMTPR_TBISD(const PALInstruction &instr);
    bool executeMTPR_TBISI(const PALInstruction &instr);
    bool executeMTPR_TBIA(const PALInstruction &instr);
    bool executeMTPR_TBIS(const PALInstruction &instr);
    bool executeTBI(const PALInstruction &instr);

	bool executeMemoryOperation(quint64 address, bool isWrite, const PALInstruction &instr);
    // IPR (Internal Processor Register) operations
    bool executeMFPR_ASTEN(const PALInstruction &instr);
    bool executeMFPR_ASTSR(const PALInstruction &instr);
    bool executeMFPR_VPTB(const PALInstruction &instr);
    bool executeMTPR_VPTB(const PALInstruction &instr);
    bool executeMFPR_FEN(const PALInstruction &instr);
    bool executeMTPR_FEN(const PALInstruction &instr);

    // System control
    bool executeWRVAL(const PALInstruction &instr);
    bool executeRDVAL(const PALInstruction &instr);
    bool executeWRENT(const PALInstruction &instr);
    bool executeSWPIPL(const PALInstruction &instr);
    bool executeRDPS(const PALInstruction &instr);
    bool executeWRKGP(const PALInstruction &instr);
    bool executeWRUSP(const PALInstruction &instr);
    bool executeRDUSP(const PALInstruction &instr);
    bool executeWRPERFMON(const PALInstruction &instr);

    // Exception and trap handling
    bool executeBPT(const PALInstruction &instr);
    bool executeBUGCHK(const PALInstruction &instr);
    bool executeCHME(const PALInstruction &instr);
    bool executeCHMS(const PALInstruction &instr);
    bool executeCHMU(const PALInstruction &instr);
    bool executeIMB(const PALInstruction &instr);
    bool executeREI(const PALInstruction &instr);

    // Queue operations (Alpha-specific)
    bool executeINSQHIL(const PALInstruction &instr);
    bool executeINSQTIL(const PALInstruction &instr);
    bool executeINSQHIQ(const PALInstruction &instr);
    bool executeREMQHIL(const PALInstruction &instr);
    bool executeREMQTIL(const PALInstruction &instr);
    bool executeREMQHIQ(const PALInstruction &instr);
    bool executeREMQTIQ(const PALInstruction &instr);

    // Memory access validation
    bool executePROBEW(const PALInstruction &instr);
    bool executePROBER(const PALInstruction &instr);

    // Platform-specific functions (Alpha/Tru64)
    bool executeAlphaSpecific(const PALInstruction &instr);
    bool executeTru64Specific(const PALInstruction &instr);

	bool writeMemoryWithFaultHandling(quint64 address, quint64 value, const PALInstruction &instr);
    // Dependency management
    void analyzeDependencies(PALInstruction &instr);
    bool checkDependencies(const PALInstruction &instr) const;
    void updateDependencies(const PALInstruction &instr);

	bool checkForTLBMiss(quint64 virtualAddress, bool isWrite);
    bool checkMemoryAccessWouldFault(quint64 virtualAddress, bool isWrite);
    // Cache operations
    bool fetchInstructionWithCache(quint64 pc, quint32 &instruction);
    bool readIntegerRegisterWithCache(quint8 reg, quint64 &value);
    bool writeIntegerRegisterWithCache(quint8 reg, quint64 value);
    bool readIPRWithCache(const QString &iprName, quint64 &value);
    bool writeIPRWithCache(const QString &iprName, quint64 value);

    // System state management
    bool checkPrivilegeLevel(const PALInstruction &instr);
    void updateSystemState(const PALInstruction &instr);
    void coordinateWithOtherExecutors(const PALInstruction &instr);

    // TLB coordination
    void invalidateTLBEntry(quint64 virtualAddress, quint64 asn = 0);
    void invalidateTLBByASN(quint64 asn);
    bool handleFloatingPointException(const FPException &fpEx, const PALInstruction &instr);
    bool handleIllegalInstructionException(const IllegalInstructionException &illEx, const PALInstruction &instr);
    bool handleMemoryAccessException(const MemoryAccessException &memEx, const PALInstruction &instr);
    bool handleMemoryFault(quint64 faultingAddress, bool isWrite, const PALInstruction &instr);
    bool handleMemoryFaultSimple(quint64 faultingAddress, bool isWrite, const PALInstruction &instr);
    void invalidateAllTLB();
    void flushInstructionTLB();
    void flushDataTLB();

	void broadcastTLBInvalidation(const QString &type, quint64 asn, quint64 virtualAddress);
    bool handleTLBException(const TLBExceptionQ &tlbEx, const PALInstruction &instr);
    // Cache coordination with MESI protocol
    void flushL1Cache(bool instructionCache = true, bool dataCache = true);
    void flushL2Cache();
    void broadcastCacheFlush(const QString &cacheLevel);
    void flushL3Cache();
    void invalidateTLBOptimized(quint64 virtualAddress, quint64 asn, bool isInstruction);
    void invalidateTLBBatch(const QVector<quint64> &virtualAddresses, quint64 asn);
    void invalidateCacheLine(quint64 address);
    void broadcastCacheInvalidation(quint64 address);

	QString determineCacheCoherencyAction(quint64 cacheLineAddr);
    void invalidateCacheRange(quint64 startAddress, quint64 endAddress);
    void invalidateCacheLineSelective(quint64 address, bool l1Only, bool l2Only, bool l3Only);

    #if defined(ALPHA_BUILD)
    bool executeAlpha_LDQP(const PALInstruction &instr);
    bool executeAlpha_STQP(const PALInstruction &instr);
    bool executeAlpha_MFPR_ASN(const PALInstruction &instr);
    bool executeAlpha_MTPR_ASTEN(const PALInstruction &instr);
    bool executeAlpha_MTPR_ASTSR(const PALInstruction &instr);
    bool executeAlpha_MFPR_MCES(const PALInstruction &instr);
    bool executeAlpha_MTPR_MCES(const PALInstruction &instr);
    bool executeAlpha_MFPR_PCBB(const PALInstruction &instr);

    bool executeAlpha_MFPR_PRBR(const PALInstruction &instr);
    
    bool executeAlpha_MTPR_PRBR(const PALInstruction &instr);
    bool executeAlpha_MFPR_PTBR(const PALInstruction &instr);
    bool executeAlpha_MTPR_SCBB(const PALInstruction &instr);
    bool executeAlpha_MTPR_SIRR(const PALInstruction &instr);
    bool executeAlpha_MFPR_SISR(const PALInstruction &instr);
    bool executeAlpha_MFPR_SSP(const PALInstruction &instr);
    bool executeAlpha_MTPR_SSP(const PALInstruction &instr);
    bool executeAlpha_MFPR_USP(const PALInstruction &instr);
    bool executeAlpha_MTPR_USP(const PALInstruction &instr);
    bool executeAlpha_MTPR_IPIR(const PALInstruction &instr);
    bool executeAlpha_MFPR_IPL(const PALInstruction &instr);
    bool executeAlpha_MTPR_IPL(const PALInstruction &instr);
    bool executeAlpha_MFPR_TBCHK(const PALInstruction &instr);
    bool executeAlpha_MTPR_TBIAP(const PALInstruction &instr);
    bool executeAlpha_MFPR_ESP(const PALInstruction &instr);
    bool executeAlpha_MTPR_ESP(const PALInstruction &instr);
    bool executeAlpha_MTPR_PERFMON(const PALInstruction &instr);
    bool executeAlpha_MFPR_WHAMI(const PALInstruction &instr);
    bool executeAlpha_READ_UNQ(const PALInstruction &instr);
    bool executeAlpha_WRITE_UNQ(const PALInstruction &instr);
    bool executeAlpha_INITPAL(const PALInstruction &instr);
    bool executeAlpha_WRENTRY(const PALInstruction &instr);
    bool executeAlpha_SWPIRQL(const PALInstruction &instr);
    bool executeAlpha_RDIRQL(const PALInstruction &instr);
    bool executeAlpha_DI(const PALInstruction &instr);
    bool executeAlpha_EI(const PALInstruction &instr);
    bool executeAlpha_SWPPAL(const PALInstruction &instr);
    bool executeAlpha_SSIR(const PALInstruction &instr);
    bool executeAlpha_CSIR(const PALInstruction &instr);
    bool executeAlpha_RFE(const PALInstruction &instr);
    bool executeAlpha_RETSYS(const PALInstruction &instr);
    bool executeAlpha_RESTART(const PALInstruction &instr);
    bool executeAlpha_SWPPROCESS(const PALInstruction &instr);
    bool executeAlpha_RDMCES(const PALInstruction &instr);
    bool executeAlpha_WRMCES(const PALInstruction &instr);
    bool executeAlpha_TBIA(const PALInstruction &instr);
    bool executeAlpha_TBIS(const PALInstruction &instr);
    bool executeAlpha_TBISASN(const PALInstruction &instr);
    bool executeAlpha_RDKSP(const PALInstruction &instr);
    bool executeAlpha_SWPKSP(const PALInstruction &instr);
    bool executeAlpha_RDPSR(const PALInstruction &instr);
    bool executeAlpha_REBOOT(const PALInstruction &instr);
    bool executeAlpha_CHMK(const PALInstruction &instr);
    bool executeAlpha_CALLKD(const PALInstruction &instr);
    bool executeAlpha_GENTRAP(const PALInstruction &instr);
    bool executeAlpha_KBPT(const PALInstruction &instr);

    #endif
#if defined(TRU64_BUILD)
    bool executeTru64_REBOOT(const PALInstruction &instr);
    bool executeTru64_INITPAL(const PALInstruction &instr);
    bool executeTru64_SWPIRQL(const PALInstruction &instr);
    bool executeTru64_RDIRQL(const PALInstruction &instr);
    bool executeTru64_DI(const PALInstruction &instr);
    bool executeTru64_RDMCES(const PALInstruction &instr);
    bool executeTru64_WRMCES(const PALInstruction &instr);
    bool executeTru64_RDPCBB(const PALInstruction &instr);
    bool executeTru64_WRPRBR(const PALInstruction &instr);
    bool executeTru64_TBIA(const PALInstruction &instr);
    bool executeTru64_TBIA(const PALInstruction &instr);
    bool executeTru64_THIS(const PALInstruction &instr);
    bool executeTru64_DTBIS(const PALInstruction &instr);
    bool executeTru64_TBISASN(const PALInstruction &instr);
    bool executeTru64_RDKSP(const PALInstruction &instr);
    bool executeTru64_SWPKSP(const PALInstruction &instr);
    bool executeTru64_WRUSP(const PALInstruction &instr);
    bool executeTru64_RDCOUNTERS(const PALInstruction &instr);
    bool executeTru64_CALLSYS(const PALInstruction &instr);
    bool executeTru64_SSIR(const PALInstruction &instr);
    bool executeTru64_WRIPIR(const PALInstruction &instr);
    bool executeTru64_RFE(const PALInstruction &instr);
    bool executeTru64_RETSYS(const PALInstruction &instr);
    bool executeTru64_RDPSR(const PALInstruction &instr);
    bool executeTru64_RDPER(const PALInstruction &instr);
    bool executeTru64_RDTHREAD(const PALInstruction &instr);
    bool executeTru64_SWPCTX(const PALInstruction &instr);
    bool executeTru64_RTI(const PALInstruction &instr);
    bool executeTru64_RDUNIQUE(const PALInstruction &instr);
    bool executeTru64_WRUNIQUE(const PALInstruction &instr);
    bool executeTru64_WRFEN(const PALInstruction &instr);
    bool executeTru64_RDUSP(const PALInstruction &instr);
    bool executeTru64_SWPIPL(const PALInstruction &instr);
    bool executeTru64_WRPERFMON(const PALInstruction &instr);
    #endif
   
   
    bool isCriticalPALAddress(quint64 address) const;
    void updateCacheStatistics(const QString &level, bool hit);
    int measureExecutionCycles(const PALInstruction &instr);
    void trackCriticalPath(quint32 function);

    // JIT optimization helpers
    void optimizeFrequentFunction(quint32 function);
    void preloadCriticalInstructions();
    void prefetchSystemData();
};