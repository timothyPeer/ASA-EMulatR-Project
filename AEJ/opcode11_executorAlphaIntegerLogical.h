#pragma once
#include "constants/constFunctionIntegerLogicalBitManipulation.h"
#include <QFuture>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSharedPointer>
#include <QString>
#include <QVector>
#include <QWaitCondition>
#include <QtConcurrent>
#include <QSet>
#include <atomic>
#include "decodedInstruction.h"

// Forward declarations
class AlphaCPU;
class UnifiedDataCache;
class AlphaInstructionCache;
class AlphaTranslationCache;
struct DecodedInstruction;

// Integer instruction structure for pipeline
struct IntegerInstruction
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

    IntegerInstruction() = default;
    IntegerInstruction(const DecodedInstruction &instr, quint64 programCounter, quint64 seqNum)
        : instruction(instr), pc(programCounter), sequenceNumber(seqNum)
    {
    }
};

/**
 * @brief High-performance async integer logical/bit manipulation executor
 *
 * Implements OpCode 0x11 (Integer Logical) and 0x12 (Shift/ZAP) operations
 * with async pipeline and cache integration similar to FP executor.
 */
class opcode11_executorAlphaIntegerLogical : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;

    // Cache hierarchy (shared with FP executor)
    QSharedPointer<AlphaInstructionCache> m_instructionCache;
    QSharedPointer<UnifiedDataCache> m_level1DataCache;
    QSharedPointer<UnifiedDataCache> m_level2Cache;
    QSharedPointer<UnifiedDataCache> m_level3Cache;

    // TLB integration
    QSharedPointer<AlphaTranslationCache> m_iTLB;
    QSharedPointer<AlphaTranslationCache> m_dTLB;

    // Async pipeline
    static constexpr int MAX_PIPELINE_DEPTH = 12; // Higher than FP (simpler ops)
    static constexpr int MAX_CACHE_REQUESTS = 24;

    QQueue<IntegerInstruction> m_fetchQueue;
    QQueue<IntegerInstruction> m_decodeQueue;
    QQueue<IntegerInstruction> m_executeQueue;
    QQueue<IntegerInstruction> m_writebackQueue;

    // Pipeline synchronization
    mutable QMutex m_pipelineMutex;
    QWaitCondition m_pipelineCondition;
    std::atomic<bool> m_pipelineActive{false};
    std::atomic<quint64> m_sequenceCounter{0};

    // Performance counters
    mutable QMutex m_statsMutex;
    QAtomicInt m_intInstructions = 0;
    QAtomicInt m_logicalOps = 0;
    QAtomicInt m_bitManipOps = 0;
    QAtomicInt m_shiftOps = 0;
    QAtomicInt m_zapOps = 0;

    // Cache performance
    QAtomicInt m_l1ICacheHits = 0;
    QAtomicInt m_l1ICacheMisses = 0;
    QAtomicInt m_l1DCacheHits = 0;
    QAtomicInt m_l1DCacheMisses = 0;

    // Pipeline workers
    QFuture<void> m_fetchWorker;
    QFuture<void> m_decodeWorker;
    QFuture<void> m_executeWorker;
    QFuture<void> m_writebackWorker;

  public:
    explicit opcode11_executorAlphaIntegerLogical(AlphaCPU *cpu, QObject *parent = nullptr);
    ~opcode11_executorAlphaIntegerLogical();

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

    // Synchronous execution (fallback)
    bool executeIntegerLogical(const DecodedInstruction &instruction);
    bool decodeIntegerLogicalInstruction(IntegerInstruction &instr);
    bool executeShiftZap(const DecodedInstruction &instruction);
    bool executeIntegerMultiply(const DecodedInstruction &instruction);

    // Statistics
    void printStatistics() const;
    void clearStatistics();

  signals:
    void intInstructionExecuted(quint32 opcode, quint32 function, bool success);
    void pipelineStalled(const QString &reason);

  private:
    // Pipeline workers
    void fetchWorker();
    void decodeWorker();
    void executeWorker();
    void writebackWorker();

    // Instruction execution by opcode
    bool executeOpcode11(IntegerInstruction &instr); // Integer Logical
    bool executeOpcode12(IntegerInstruction &instr); // Shift/ZAP
    bool executeOpcode13(IntegerInstruction &instr); // Integer Multiply

    // OpCode 0x11 implementations
    bool executeAND(const IntegerInstruction &instr, quint64 &result);
    bool executeBIC(const IntegerInstruction &instr, quint64 &result);
    bool executeBIS(const IntegerInstruction &instr, quint64 &result);
    bool executeXOR(const IntegerInstruction &instr, quint64 &result);
    bool executeEQV(const IntegerInstruction &instr, quint64 &result);
    bool executeORNOT(const IntegerInstruction &instr, quint64 &result);

    // Bit manipulation functions
    bool executeMSKBL(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTBL(const IntegerInstruction &instr, quint64 &result);
    bool executeINSBL(const IntegerInstruction &instr, quint64 &result);
    bool executeMSKWL(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTWL(const IntegerInstruction &instr, quint64 &result);
    bool executeINSWL(const IntegerInstruction &instr, quint64 &result);
    bool executeMSKLL(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTLL(const IntegerInstruction &instr, quint64 &result);
    bool executeINSLL(const IntegerInstruction &instr, quint64 &result);
    bool executeMSKQL(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTQL(const IntegerInstruction &instr, quint64 &result);
    bool executeINSQL(const IntegerInstruction &instr, quint64 &result);

    // High variants
    bool executeMSKBH(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTBH(const IntegerInstruction &instr, quint64 &result);
    bool executeINSBH(const IntegerInstruction &instr, quint64 &result);
    bool executeMSKWH(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTWH(const IntegerInstruction &instr, quint64 &result);
    bool executeINSWH(const IntegerInstruction &instr, quint64 &result);
    bool executeMSKLH(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTLH(const IntegerInstruction &instr, quint64 &result);
    bool executeINSLH(const IntegerInstruction &instr, quint64 &result);
    bool executeMSKQH(const IntegerInstruction &instr, quint64 &result);
    bool executeEXTQH(const IntegerInstruction &instr, quint64 &result);
    bool executeINSQH(const IntegerInstruction &instr, quint64 &result);

    // OpCode 0x12 implementations (Shift/ZAP)
    bool executeSLL(const IntegerInstruction &instr, quint64 &result);
    bool executeSRL(const IntegerInstruction &instr, quint64 &result);
    bool executeSRA(const IntegerInstruction &instr, quint64 &result);
    bool executeZAP(const IntegerInstruction &instr, quint64 &result);
    bool executeZAPNOT(const IntegerInstruction &instr, quint64 &result);

    // OpCode 0x13 implementations (Integer Multiply)
    bool executeMULQ(const IntegerInstruction &instr, quint64 &result);
    bool executeMULQV(const IntegerInstruction &instr, quint64 &result);

    // Helper functions
    void analyzeDependencies(IntegerInstruction &instr);
    bool checkDependencies(const IntegerInstruction &instr) const;
    void updateDependencies(const IntegerInstruction &instr);

    // Cache operations
    bool fetchInstructionWithCache(quint64 pc, quint32 &instruction);
    bool readIntegerRegisterWithCache(quint8 reg, quint64 &value);
    bool writeIntegerRegisterWithCache(quint8 reg, quint64 value);

    // Bit manipulation helpers
    quint64 createByteMask(int bytePos, bool high);
    quint64 createWordMask(int wordPos, bool high);
    quint64 createLongwordMask(int longPos, bool high);
    quint64 createQuadwordMask(int quadPos, bool high);

    quint64 extractBytes(quint64 value, int pos, int count, bool high);
    quint64 insertBytes(quint64 dest, quint64 src, int pos, int count, bool high);

    // ZAP operations
    quint64 zapBytes(quint64 value, quint8 mask);
    quint64 zapNotBytes(quint64 value, quint8 mask);
};
