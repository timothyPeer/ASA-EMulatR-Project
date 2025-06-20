// AlphaSQRTExecutor.h
#pragma once

#include "../AEJ/constants/const_OpCode_14_SQRT.h"
#include "decodedInstruction.h"
#include <QFuture>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSharedPointer>
#include <QString>
#include <QThreadPool>
#include <QVector>
#include <QWaitCondition>
#include <QtConcurrent>
#include <atomic>
#include <cfenv>
#include <cmath>

// Forward declarations
class AlphaCPU;
class UnifiedDataCache;
class AlphaInstructionCache;
class AlphaTranslationCache;
struct DecodedInstruction;

/**
 * @brief SQRT instruction structure for pipeline
 *
 * Holds all necessary fields to decode a 0x14-SQRT operation:
 *  - 'instruction.raw' contains the 32-bit raw opcode
 *  - 'srcRegister' and 'dstRegister' select floating-point registers F0–F31
 *  - 'operand' is the 64-bit value read from the source register (GFloat, DFloat, etc.)
 *  - 'result' is the 64-bit output to be written back
 *
 * Floating-point registers F0–F31 are 64 bits wide (Vol. 1, Sec. 3.1.3, Floating?Point
 * Registers) 
 */
struct SQRTInstruction
{
    DecodedInstruction instruction;
    quint64 pc;
    quint64 sequenceNumber;
    bool isReady = false;
    bool isCompleted = false;
    bool hasException = false;
    quint32 exceptionType = 0;

    // SQRT-specific data
    quint32 function;   ///< 11-bit function field for SQRT (bits <10:0>)
    quint8 srcRegister; ///< Fa (source floating-point register)
    quint8 dstRegister; ///< Fe (destination floating-point register)
    quint64 operand;    ///< 64-bit contents of Fa fetched from register file
    quint64 result = 0; ///< 64-bit result to write back to Fe

    // Execution characteristics
    int expectedCycles = 15; ///< Estimated execution latency
    bool isHighLatency = true;

    // Precision and rounding
    enum Precision
    {
        F_FLOAT,
        S_FLOAT,
        G_FLOAT,
        T_FLOAT
    } precision;
    enum RoundingMode
    {
        DEFAULT,
        CHOPPED,
        MINUS,
        PLUS,
        DYNAMIC
    } rounding;

    SQRTInstruction() = default;
    SQRTInstruction(const DecodedInstruction &instr, quint64 programCounter, quint64 seqNum)
        : instruction(instr), pc(programCounter), sequenceNumber(seqNum)
    {
    }
};

/**
 * @brief High-performance async SQRT executor with advanced pipelining
 *
 * Implements OpCode 0x14 (SQRT) for both VAX and IEEE formats:
 *  - Multi-stage pipeline (fetch ? decode ? dispatch ? execute ? completion  writeback)
 *  - Parallel SQRT computation across multiple worker threads
 *  - IEEE 754–compliant rounding and exception handling (Vol. 1, Sec. 3.1.3 Floating?Point Registers)
 * :contentReference[oaicite:2]{index=2}
 *  - Cache/TLB–aware instruction fetch and data register reads
 *
 * Fix for C2664 error: any pipeline stage or helper (e.g. executeSQRTS) that needs to call
 * readFloatRegisterWithCache(quint8, quint64 &) must accept SQRTInstruction& instr rather
 * than const SQRTInstruction& instr. This allows instr.operand to be passed as a non?const
 * quint64&.
 */
class opcode14_executorAlphaSQRT : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;

    // Cache hierarchy
    QSharedPointer<AlphaInstructionCache> m_instructionCache;
    QSharedPointer<UnifiedDataCache> m_level1DataCache;
    QSharedPointer<UnifiedDataCache> m_level2Cache;
    QSharedPointer<UnifiedDataCache> m_level3Cache;

    // TLB integration
    QSharedPointer<AlphaTranslationCache> m_iTLB;
    QSharedPointer<AlphaTranslationCache> m_dTLB;

    // Advanced async pipeline
    static constexpr int MAX_PIPELINE_DEPTH = 32; ///< Pipeline depth for SQRT
    static constexpr int MAX_SQRT_WORKERS = 8;    ///< Number of parallel SQRT units
    static constexpr int MAX_CACHE_REQUESTS = 64; ///< Max outstanding cache/TLB requests

    // Multi-stage pipeline queues
    QQueue<SQRTInstruction> m_fetchQueue;
    QQueue<SQRTInstruction> m_decodeQueue;
    QQueue<SQRTInstruction> m_dispatchQueue;   ///< Dispatch to SQRT units
    QQueue<SQRTInstruction> m_executeQueue;    ///< For unit-level execution
    QQueue<SQRTInstruction> m_completionQueue; ///< Completion ordering
    QQueue<SQRTInstruction> m_writebackQueue;  ///< Writeback to registers

    // Parallel SQRT units
    QVector<QQueue<SQRTInstruction>> m_sqrtUnits; ///< One queue per worker
    QVector<QFuture<void>> m_sqrtWorkers;         ///< Worker futures

    // Pipeline synchronization
    mutable QMutex m_pipelineMutex;
    QWaitCondition m_pipelineCondition;
    std::atomic<bool> m_pipelineActive{false};
    std::atomic<quint64> m_sequenceCounter{0};

    // Thread pool for heavy SQRT ops
    QThreadPool *m_sqrtThreadPool;

    // Performance counters (protected by m_statsMutex)
    mutable QMutex m_statsMutex;
    QAtomicInt m_sqrtInstructions = 0;
    QAtomicInt m_floatF = 0, m_floatS = 0, m_floatG = 0, m_floatT = 0;
    QAtomicInt m_roundDefault = 0, m_roundChop = 0, m_roundMinus = 0, m_roundPlus = 0;
    QAtomicInt m_exceptionsRaised = 0;
    QAtomicInt m_totalSqrtCycles = 0;

       // Cache performance (PAL-specific)
    QAtomicInt m_l1ICacheHits = 0;
    QAtomicInt m_l1ICacheMisses = 0;
    QAtomicInt m_l1DCacheHits = 0;
    QAtomicInt m_l1DCacheMisses = 0;
    QAtomicInt m_l2CacheHits = 0;
    QAtomicInt m_l2CacheMisses = 0;
    QAtomicInt m_l3CacheHits = 0;
    QAtomicInt m_l3CacheMisses = 0;

    // Advanced metrics
    std::atomic<quint64> m_pipelineUtilization{0};
    std::atomic<quint64> m_averageLatency{0};
    std::atomic<quint64> m_parallelEfficiency{0};

    // Pipeline workers
    QFuture<void> m_fetchWorker;
    QFuture<void> m_decodeWorker;
    QFuture<void> m_dispatchWorker;
    QFuture<void> m_completionWorker;
    QFuture<void> m_writebackWorker;

  public:
    explicit opcode14_executorAlphaSQRT(AlphaCPU *cpu, QObject *parent = nullptr);
    ~opcode14_executorAlphaSQRT();

    // Attach caches/TLBs (same as other executors)
    void attachInstructionCache(QSharedPointer<AlphaInstructionCache> icache) { m_instructionCache = icache; }
    void attachLevel1DataCache(QSharedPointer<UnifiedDataCache> l1dcache) { m_level1DataCache = l1dcache; }
    void attachLevel2Cache(QSharedPointer<UnifiedDataCache> l2cache) { m_level2Cache = l2cache; }
    void attachLevel3Cache(QSharedPointer<UnifiedDataCache> l3cache) { m_level3Cache = l3cache; }
    void attachTranslationCache(QSharedPointer<AlphaTranslationCache> iTLB, QSharedPointer<AlphaTranslationCache> dTLB)
    {
        m_iTLB = iTLB;
        m_dTLB = dTLB;
    }

    // Advanced pipeline control
    void startAsyncPipeline();
    void stopAsyncPipeline();
    bool isAsyncPipelineActive() const { return m_pipelineActive.load(); }

    // SQRT instruction submission (to Fetch stage)
    bool submitInstruction(const DecodedInstruction &instruction, quint64 pc);

    // Synchronous fallback (unused by async pipeline)
    bool executeSQRT(const DecodedInstruction &instruction);

    // Performance monitoring
    void printStatistics() const;
    void printAdvancedStatistics() const;
    void clearStatistics();

    // Configuration
    void setSQRTWorkerThreads(int count);
    void setPipelineDepth(int depth);

  signals:
    void sqrtInstructionExecuted(quint32 function, int cycles, bool success);
    void sqrtExceptionRaised(quint32 exceptionType, quint64 pc);
    void pipelineUtilizationChanged(double utilization);

  private:
    // ------------------------------------------------------------------------
    // Pipeline Workers (declarations only; definitions in .cpp)
    // ------------------------------------------------------------------------

    void fetchWorker();
    void decodeWorker();

    /**
     * @brief Dispatch ready instructions to one of the parallel SQRT units
     *
     * Picks the least?busy SQRT unit via selectOptimalSQRTUnit(), then enqueues
     * into that unit’s QQueue for execution in sqrtUnitWorker().
     */
    void dispatchWorker();

    /**
     * @brief One SQRT unit’s execution loop
     *
     * Dequeues SQRTInstruction from m_sqrtUnits[unitId], performs the actual SQRT
     * (executeSQRTF/S/executeSQRTT), then enqueues into m_completionQueue.
     */
    void sqrtUnitWorker(int unitId);
    void completionWorker();
    void writebackWorker();

    // ------------------------------------------------------------------------
    // Instruction?specific decode & analysis
    // ------------------------------------------------------------------------

    /**
     * @brief Decode raw opcode into SQRTInstruction fields (function, registers, rounding, etc.)
     * @param instr [in/out] instruction to decode; its fields (function, srcRegister, dstRegister,
     *              precision, rounding) are written here.
     * @return true if decode succeeded
     */
    bool decodeSQRTInstruction(SQRTInstruction &instr);

    /**
     * @brief Analyze SQRT complexity to estimate expectedCycles
     * @param instr [in/out] instruction to annotate with expectedCycles
     */
    void analyzeSQRTComplexity(SQRTInstruction &instr);

    /**
     * @brief Make a more detailed cycle?estimate based on operands value/classification
     * @param instr [in] decoded instruction (with instr.operand already read)
     * @return estimated cycle count
     */
    int estimateExecutionCycles(const SQRTInstruction &instr);

    // ------------------------------------------------------------------------
    // SQRT execution methods (must accept SQRTInstruction& so that instr.operand can be modified)
    // ------------------------------------------------------------------------

    /**
     * @brief VAX F_floating SQRT (single?precision)
     * @param instr [in/out] instr.operand already holds source bits; instr.result will receive output bits
     * @param result [out] 64?bit container for the single?precision result
     * @return true if no exceptions prevented completion
     */
    bool executeSQRTF(SQRTInstruction &instr, quint64 &result);
    bool executeSQRTF(const SQRTInstruction &instr, quint64 &result);
    /**
     * @brief IEEE 754 S_floating SQRT (single?precision)
     * @param instr [in/out] instr.operand already holds source bits; instr.result will receive output bits
     * @param result [out] receives 64?bit zero?extended result bits
     * @return true if no exceptions prevented completion
     *
     * Fix for C2664: instr is non?const so instr.operand can be passed to
     * readFloatRegisterWithCache(quint8, quint64 &) as a non-const reference. 
     */
    bool executeSQRTS(SQRTInstruction &instr, quint64 &result);
    /**
     * @brief VAX G_floating SQRT (double?precision)
     */
    bool executeSQRTG(SQRTInstruction &instr, quint64 &result);
    bool executeSQRTG(const SQRTInstruction &instr, quint64 &result);
    /**
     * @brief IEEE 754 T_floating SQRT (double?precision)
     */
    bool executeSQRTT(SQRTInstruction &instr, quint64 &result);
    bool executeSQRTT(const SQRTInstruction &instr, quint64 &result);
    // ------------------------------------------------------------------------
    // Rounding?mode helpers (wrap <cfenv>)
    // ------------------------------------------------------------------------

    void setRoundingMode(SQRTInstruction::RoundingMode mode);
    void restoreRoundingMode();

    // ------------------------------------------------------------------------
    // Low?level SQRT implementations for each format
    // ------------------------------------------------------------------------

    quint32 sqrtFloat32(quint32 operand, SQRTInstruction::RoundingMode rounding)
    {
        // Convert to IEEE 754 float
        float value;
        std::memcpy(&value, &operand, sizeof(float));

        // Handle special cases
        if (value < 0.0f)
        {
            raiseSQRTException(0x10, SQRTInstruction()); // Invalid operation
            return 0x7FC00000;                           // NaN
        }

        if (value == 0.0f || std::isinf(value))
        {
            return operand; // sqrt(0) = 0, sqrt(inf) = inf
        }

        // High-precision SQRT computation
        double preciseResult = newtonsMethodSQRT(static_cast<double>(value));
        float result = static_cast<float>(preciseResult);

        quint32 resultBits;
        std::memcpy(&resultBits, &result, sizeof(float));

        return resultBits;
    }
    
    quint64 sqrtFloat64(quint64 operand, SQRTInstruction::RoundingMode rounding);

    quint32 sqrtVAXF(quint32 operand, SQRTInstruction::RoundingMode rounding);
    quint64 sqrtVAXG(quint64 operand, SQRTInstruction::RoundingMode rounding);

    double newtonsMethodSQRT(double x, int iterations = 10);
    double fastInverseSQRT(double x); //<– Quake?style fast inverse square root

    // ------------------------------------------------------------------------
    // Exception handling
    // ------------------------------------------------------------------------

    void raiseSQRTException(quint32 exceptionType, const SQRTInstruction &instr);

    bool checkSQRTExceptions(quint64 operand, quint64 result)
    {
        // Check for various floating-point exceptions

        // Check operand for invalid cases
        if (std::isnan(*reinterpret_cast<const double *>(&operand)))
        {
            raiseSQRTException(0x10, SQRTInstruction()); // Invalid operation
            return false;
        }

        // Check for negative operand
        double operand_val = *reinterpret_cast<const double *>(&operand);
        if (operand_val < 0.0)
        {
            raiseSQRTException(0x10, SQRTInstruction()); // Invalid operation
            return false;
        }

        // Check result for exceptions
        double result_val = *reinterpret_cast<const double *>(&result);

        if (std::isnan(result_val))
        {
            raiseSQRTException(0x10, SQRTInstruction()); // Invalid operation
            return false;
        }

        if (std::isinf(result_val))
        {
            raiseSQRTException(0x04, SQRTInstruction()); // Overflow
            return false;
        }

        // Check for underflow
        if (result_val != 0.0 && std::fpclassify(result_val) == FP_SUBNORMAL)
        {
            raiseSQRTException(0x02, SQRTInstruction()); // Underflow
            // Continue execution - underflow is often non-fatal
        }

        return true;
    }
    
    // ------------------------------------------------------------------------
    // Performance optimization & metrics
    // ------------------------------------------------------------------------

    void optimizePipelineBalance();
    void updateUtilizationMetrics();
    int selectOptimalSQRTUnit(const SQRTInstruction &instr);

    // ------------------------------------------------------------------------
    // Cache/TLB operations (borrowed from FP executor pattern)
    // ------------------------------------------------------------------------

    bool fetchInstructionWithCache(quint64 pc, quint32 &instruction);

    /**
     * @brief Read a 64?bit floating?point register (Fa) into 'value'
     * @param reg   Fa index (0–31)
     * @param value [out] receives register contents
     * @return true if read succeeded
     *
     * Because SQRTInstruction must be non?const here, instr.operand can safely be passed:
     *    readFloatRegisterWithCache(instr.srcRegister, instr.operand);
     */
    bool readFloatRegisterWithCache(quint8 reg, quint64 &value);

    bool writeFloatRegisterWithCache(quint8 reg, quint64 value);
};
