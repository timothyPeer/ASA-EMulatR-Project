#pragma once
#include "decodedInstruction.h"
#include "globalmacro.h"
#include "utilitySafeIncrement.h"
#include <QFuture>
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

// Forward declarations
class AlphaCPU;
class UnifiedDataCache;
class AlphaInstructionCache;
class AlphaTranslationCache;
struct DecodedInstruction;

// Opcode 04 instruction structure for pipeline
struct Opcode04Instruction
{
    DecodedInstruction instruction;
    quint64 pc;
    quint64 sequenceNumber;
    bool isReady = false;
    bool isCompleted = false;
    bool hasException = false;

    // Dependency tracking
    QSet<quint8> srcRegisters;
    QSet<quint8> dstRegisters;

    // Execution results
    quint64 result = 0;
    bool writeResult = true;

    Opcode04Instruction() = default;
    Opcode04Instruction(const DecodedInstruction &instr, quint64 programCounter, quint64 seqNum)
        : instruction(instr), pc(programCounter), sequenceNumber(seqNum)
    {
    }
};

/**
 * @brief High-performance async opcode 0x04 executor
 *
 * Implements OpCode 0x04 operations with async pipeline, JIT optimization,
 * hot/warm path caching, and full memory/type safety.
 * Designed for maximum performance with Alpha CPU integration.
 */
class opcode04_executorAlpha : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;

    // Cache hierarchy integration
    QSharedPointer<AlphaInstructionCache> m_instructionCache;
    QSharedPointer<UnifiedDataCache> m_level1DataCache;
    QSharedPointer<UnifiedDataCache> m_level2Cache;
    QSharedPointer<UnifiedDataCache> m_level3Cache;

    // TLB integration
    QSharedPointer<AlphaTranslationCache> m_iTLB;
    QSharedPointer<AlphaTranslationCache> m_dTLB;

    // Async pipeline configuration
    static constexpr int MAX_PIPELINE_DEPTH = 16;
    static constexpr int MAX_CACHE_REQUESTS = 32;
    static constexpr int HOT_PATH_CACHE_SIZE = 256;

    // Pipeline queues
    QQueue<Opcode04Instruction> m_fetchQueue;
    QQueue<Opcode04Instruction> m_decodeQueue;
    QQueue<Opcode04Instruction> m_executeQueue;
    QQueue<Opcode04Instruction> m_writebackQueue;

    // Pipeline synchronization
    mutable QMutex m_pipelineMutex;
    QWaitCondition m_pipelineCondition;
    std::atomic<bool> m_pipelineActive{false};
    QAtomicInt m_sequenceCounter;

    // JIT hot path optimization
    mutable QMutex m_hotPathMutex;
    QHash<quint64, quint32> m_hotPathCache;
    QAtomicInt m_hotPathHits;
    QAtomicInt m_hotPathMisses;

    // Performance counters
    mutable QMutex m_statsMutex;
    QAtomicInt m_opcode04Instructions;
    QAtomicInt m_totalExecutions;
    QAtomicInt m_asyncExecutions;
    QAtomicInt m_syncExecutions;

    // Cache performance
    QAtomicInt m_l1ICacheHits;
    QAtomicInt m_l1ICacheMisses;
    QAtomicInt m_l1DCacheHits;
    QAtomicInt m_l1DCacheMisses;
    QAtomicInt m_l2CacheHits;
    QAtomicInt m_l2CacheMisses;

    // Pipeline workers
    QFuture<void> m_fetchWorker;
    QFuture<void> m_decodeWorker;
    QFuture<void> m_executeWorker;
    QFuture<void> m_writebackWorker;

    // Error handling
    QAtomicInt m_pipelineStalls;
    QAtomicInt m_executionErrors;

  public:
    explicit opcode04_executorAlpha(AlphaCPU *cpu, QObject *parent = nullptr);
    ~opcode04_executorAlpha();

    // Initialization
    void initialize();
    void initialize_SignalsAndSlots();

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

    // Instruction submission (async)
    bool submitInstruction(const DecodedInstruction &instruction, quint64 pc);

    // Synchronous execution (fallback/JIT)
    bool executeOpcode04(const DecodedInstruction &instruction);
    bool decodeOpcode04Instruction(Opcode04Instruction &instr);

    // Hot path optimization
    void warmupHotPath(quint64 pc, quint32 frequency);
    bool isHotPath(quint64 pc) const;
    void updateHotPathStats(quint64 pc);

    // Statistics and monitoring
    void printStatistics() const;
    void clearStatistics();
    qreal getPerformanceMetrics() const;

  signals:
    void sigOpcode04Executed(quint32 function, bool success);
    void sigPipelineStalled(const QString &reason);
    void sigHotPathDetected(quint64 pc, quint32 frequency);
    void sigPerformanceAlert(const QString &message);

  private slots:
    void handlePipelineStall();
    void optimizeHotPaths();

  private:
    // Pipeline workers
    void fetchWorker();
    void decodeWorker();
    void executeWorker();
    void writebackWorker();

    // Core execution engine
    bool executeOpcode04Core(Opcode04Instruction &instr);

    // Function-specific execution methods
    bool executeFunction00(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction01(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction02(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction03(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction04(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction05(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction06(const Opcode04Instruction &instr, quint64 &result);
    bool executeFunction07(const Opcode04Instruction &instr, quint64 &result);

    // JIT optimization methods
    void jitOptimizeInstruction(Opcode04Instruction &instr);
    bool executeJITOptimized(const Opcode04Instruction &instr, quint64 &result);
    void cacheOptimizedPath(quint64 pc, const Opcode04Instruction &instr);

    // Memory safety and type checking
    bool validateInstructionSafety(const Opcode04Instruction &instr) const;
    bool checkMemoryBounds(quint64 address, quint32 size) const;
    bool checkRegisterAccess(quint8 reg) const;

    // Dependency analysis
    void analyzeDependencies(Opcode04Instruction &instr);
    bool checkDependencies(const Opcode04Instruction &instr) const;
    void updateDependencies(const Opcode04Instruction &instr);

    // Cache operations with hot/warm path optimization
    bool fetchInstructionWithCache(quint64 pc, quint32 &instruction);
    bool readRegisterWithCache(quint8 reg, quint64 &value);
    bool writeRegisterWithCache(quint8 reg, quint64 value);
    bool accessMemoryWithCache(quint64 address, quint8 *data, quint32 size, bool isWrite);

    // Performance optimization helpers
    void optimizePipelineFlow();
    void adjustPipelineDepth();
    void balanceWorkload();

    // Error handling and recovery
    void handleExecutionError(const Opcode04Instruction &instr, const QString &error);
    void recoverFromPipelineStall();
    bool validatePipelineIntegrity() const;

    // Utility methods
    quint32 extractFunction(quint32 rawInstruction) const;
    quint8 extractRegisterA(quint32 rawInstruction) const;
    quint8 extractRegisterB(quint32 rawInstruction) const;
    quint8 extractRegisterC(quint32 rawInstruction) const;
    bool isLiteralMode(quint32 rawInstruction) const;
    quint8 extractLiteral(quint32 rawInstruction) const;
};