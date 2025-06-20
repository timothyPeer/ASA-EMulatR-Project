#pragma once
// executorAlphaFloatingPoint.h
#pragma once

#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QFuture>
#include <QMap>
#include <QWaitCondition>
#include <QSemaphore>
#include <QQueue>
#include <QVector>
#include <cfenv>
#include <cmath>
#include "decodedInstruction.h"
#include "structures/structFPCR.h"
#include "structures/structFPInstruction.h"
#include "enumerations/structCacheRequest.h"

// Forward declarations
class AlphaCPU;
class UnifiedDataCache;
class AlphaInstructionCache;
struct DecodedInstruction;





// Cache-aware Floating-Point Instruction Executor
class executorAlphaFloatingPoint : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;

    // Cache hierarchy
    QSharedPointer<AlphaInstructionCache> m_instructionCache; // L1 I-Cache
    QSharedPointer<UnifiedDataCache> m_level1DataCache;       // L1 D-Cache
    QSharedPointer<UnifiedDataCache> m_level2Cache;           // L2 Unified Cache
    QSharedPointer<UnifiedDataCache> m_level3Cache;           // L3 Shared Cache

    // Floating-point state
    FPCR m_fpcr;
    mutable QMutex m_fpcrMutex;

    // Performance counters
    mutable QMutex m_statsMutex;
    QAtomicInt m_fpInstructions = 0;
    QAtomicInt m_fpConditionalMoves = 0;
    QAtomicInt m_fpConversions = 0;
    QAtomicInt m_fpSignOperations = 0;
    QAtomicInt m_fpcrOperations = 0;

    // Cache performance
    QAtomicInt m_l1ICacheHits = 0;
    QAtomicInt m_l1ICacheMisses = 0;
    QAtomicInt m_l1DCacheHits = 0;
    QAtomicInt m_l1DCacheMisses = 0;
    QAtomicInt m_l2CacheHits = 0;
    QAtomicInt m_l2CacheMisses = 0;
    QAtomicInt m_l3CacheHits = 0;
    QAtomicInt m_l3CacheMisses = 0;


    private:
    // Asynchronous execution pipeline
    static constexpr int MAX_PIPELINE_DEPTH = 8;
    static constexpr int MAX_CACHE_REQUESTS = 16;

    // Pipeline stages
    QQueue<FPInstruction> m_fetchQueue;
    QQueue<FPInstruction> m_decodeQueue;
    QQueue<FPInstruction> m_executeQueue;
    QQueue<FPInstruction> m_writebackQueue;

    // Pipeline synchronization
    mutable QMutex m_pipelineMutex;
    QWaitCondition m_pipelineCondition;
    std::atomic<bool> m_pipelineActive{false};
    std::atomic<quint64> m_sequenceCounter{0};

    // Cache request queuing
    QQueue<CacheRequest> m_cacheRequestQueue;
    mutable QMutex m_cacheQueueMutex;
    QSemaphore m_cacheRequestSemaphore;
    QWaitCondition m_cacheQueueCondition;

    // Dependency tracking
    QMap<quint8, quint64> m_registerLastWriter;     // reg -> sequence number
    QMap<quint64, QSet<quint64>> m_dependencyGraph; // seqNum -> dependent seqNums
    quint64 m_lastFPCRWriter = 0;

    // Asynchronous workers
    QFuture<void> m_fetchWorker;
    QFuture<void> m_decodeWorker;
    QFuture<void> m_executeWorker;
    QFuture<void> m_writebackWorker;
    QFuture<void> m_cacheWorker;

    // Performance tracking
    std::atomic<quint64> m_pipelineStalls{0};
    std::atomic<quint64> m_cacheQueueStalls{0};
    std::atomic<quint64> m_dependencyStalls{0};
  public:
    explicit executorAlphaFloatingPoint(AlphaCPU *cpu, QObject *parent = nullptr);

    // Cache attachment methods
    void attachInstructionCache(QSharedPointer<AlphaInstructionCache> icache) { m_instructionCache = icache; }
    void attachLevel1DataCache(QSharedPointer<UnifiedDataCache> l1dcache) { m_level1DataCache = l1dcache; }
    void attachLevel2Cache(QSharedPointer<UnifiedDataCache> l2cache) { m_level2Cache = l2cache; }
    void attachLevel3Cache(QSharedPointer<UnifiedDataCache> l3cache) { m_level3Cache = l3cache; }

    // Main execution entry point for OPCODE_FLTL (0x17) functions
    bool executeFLTLFunction(const DecodedInstruction &instruction);

    // FPCR operations
    quint64 getFPCR() const;
    void setFPCR(quint64 value);

    // Statistics and diagnostics
    void printStatistics() const;
    void clearStatistics();

    public:
    // Asynchronous pipeline control
    void startAsyncPipeline();
    void stopAsyncPipeline();
    bool isAsyncPipelineActive() const { return m_pipelineActive.load(); }

    // Submit instruction for asynchronous execution
    bool submitInstruction(const DecodedInstruction &instruction, quint64 pc);

    // Pipeline statistics
    void printPipelineStatistics() const;


  signals:
    void fpInstructionExecuted(quint32 function, bool success);
    void fpExceptionRaised(quint32 exceptionType, quint64 pc);
    void cachePerformanceUpdate(const QString &cacheLevel, bool hit);

  private:
    // Cache-aware instruction fetch with multi-level hierarchy
    bool fetchInstructionWithCache(quint64 pc, quint32 &instruction);

    // Cache-aware data access methods
    bool readFloatRegisterWithCache(quint8 reg, quint64 &value);
    bool writeFloatRegisterWithCache(quint8 reg, quint64 value);

    // OPCODE_FLTL (0x17) function implementations
    bool executeCVTLQ(const DecodedInstruction &instruction);   // 0x010
    bool executeCPYS(const DecodedInstruction &instruction);    // 0x020
    bool executeCPYSN(const DecodedInstruction &instruction);   // 0x021
    bool executeCPYSE(const DecodedInstruction &instruction);   // 0x022
    bool executeMT_FPCR(const DecodedInstruction &instruction); // 0x024
    bool executeMF_FPCR(const DecodedInstruction &instruction); // 0x025
    bool executeFCMOVEQ(const DecodedInstruction &instruction); // 0x02A
    bool executeFCMOVNE(const DecodedInstruction &instruction); // 0x02B
    bool executeFCMOVLT(const DecodedInstruction &instruction); // 0x02C
    bool executeFCMOVGE(const DecodedInstruction &instruction); // 0x02D
    bool executeFCMOVLE(const DecodedInstruction &instruction); // 0x02E
    bool executeFCMOVGT(const DecodedInstruction &instruction); // 0x02F
    bool executeCVTQL(const DecodedInstruction &instruction);   // 0x030
    bool executeCVTQLV(const DecodedInstruction &instruction);  // 0x130
    bool executeCVTQLSV(const DecodedInstruction &instruction); // 0x530

    // Helper methods for floating-point operations
    bool isFloatZero(quint64 fpValue) const;
    bool isFloatNegative(quint64 fpValue) const;
    bool isFloatEqual(quint64 fp1, quint64 fp2) const;
    bool isFloatLessThan(quint64 fp1, quint64 fp2) const;
    bool isFloatLessOrEqual(quint64 fp1, quint64 fp2) const;
    bool isFloatGreaterThan(quint64 fp1, quint64 fp2) const;
    bool isFloatGreaterOrEqual(quint64 fp1, quint64 fp2) const;

    // IEEE 754 manipulation helpers
    quint64 copyFloatSign(quint64 source, quint64 target) const;
    quint64 copyFloatSignNegate(quint64 source, quint64 target) const;
    quint64 copyFloatSignAndExponent(quint64 source, quint64 target) const;

    // Conversion helpers
    quint64 convertLongwordToQuadword(quint32 longword) const;
    quint32 convertQuadwordToLongword(quint64 quadword, bool checkOverflow = false) const;

    // Exception handling
    void raiseFloatingPointException(quint32 exceptionType);
    bool checkFloatingPointTraps(quint64 fpResult);

    // Cache performance tracking
    void updateCacheStatistics(const QString &level, bool hit);

    private:
    // Pipeline stage workers
    void fetchWorker();
    void decodeWorker();
    void executeWorker();
    void writebackWorker();
    void cacheWorker();

    // Dependency analysis
    void analyzeDependencies(FPInstruction &instr);
    bool checkDependencies(const FPInstruction &instr) const;
    void updateDependencies(const FPInstruction &instr);

    // Asynchronous cache operations
    QFuture<bool> asyncCacheRead(quint64 address, quint8 *data, int size);
    QFuture<bool> asyncRegisterRead(quint8 reg);
    QFuture<bool> asyncRegisterWrite(quint8 reg, quint64 value);

    // Pipeline utilities
    bool canAdvanceStage(const QQueue<FPInstruction> &from, const QQueue<FPInstruction> &to) const;
    void advanceInstruction(QQueue<FPInstruction> &from, QQueue<FPInstruction> &to);
};
