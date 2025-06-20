#include "opcode14_executorAlphaSQRT.h"
#include "utilitySafeIncrement.h"
#include "decodeFloatingFields.h"
#include "AlphaCPU_refactored.h"

// ========== AlphaSQRTExecutor.cpp (Key Methods) ==========

opcode14_executorAlphaSQRT::opcode14_executorAlphaSQRT(AlphaCPU *cpu, QObject *parent) : QObject(parent), m_cpu(cpu)
{

    // Initialize thread pool for parallel SQRT computation
    m_sqrtThreadPool = new QThreadPool(this);
    m_sqrtThreadPool->setMaxThreadCount(MAX_SQRT_WORKERS);

    // Initialize SQRT execution units
    m_sqrtUnits.resize(MAX_SQRT_WORKERS);
    m_sqrtWorkers.resize(MAX_SQRT_WORKERS);

    qDebug() << "AlphaSQRTExecutor: Initialized with" << MAX_SQRT_WORKERS << "parallel SQRT units";
}

void opcode14_executorAlphaSQRT::startAsyncPipeline()
{
    if (m_pipelineActive.exchange(true))
    {
        return; // Already running
    }

    // Clear all pipeline queues
    {
        QMutexLocker locker(&m_pipelineMutex);
        m_fetchQueue.clear();
        m_decodeQueue.clear();
        m_dispatchQueue.clear();
        m_executeQueue.clear();
        m_completionQueue.clear();
        m_writebackQueue.clear();

        for (auto &unit : m_sqrtUnits)
        {
            unit.clear();
        }

        m_sequenceCounter.store(0);
    }

    // Start all pipeline workers
    m_fetchWorker = QtConcurrent::run([this]() { fetchWorker(); });
    m_decodeWorker = QtConcurrent::run([this]() { decodeWorker(); });
    m_dispatchWorker = QtConcurrent::run([this]() { dispatchWorker(); });
    m_completionWorker = QtConcurrent::run([this]() { completionWorker(); });
    m_writebackWorker = QtConcurrent::run([this]() { writebackWorker(); });

    // Start parallel SQRT unit workers
    for (int i = 0; i < MAX_SQRT_WORKERS; ++i)
    {
        m_sqrtWorkers[i] = QtConcurrent::run([this, i]() { sqrtUnitWorker(i); });
    }

    qDebug() << "Advanced SQRT async pipeline started with" << MAX_SQRT_WORKERS << "parallel units";
}

void opcode14_executorAlphaSQRT::stopAsyncPipeline()
{ 
    // TODO

}

bool opcode14_executorAlphaSQRT::submitInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        if (!m_pipelineActive.load())
        {
            return false;
        }

        QMutexLocker locker(&m_pipelineMutex);

        if (m_fetchQueue.size() >= MAX_PIPELINE_DEPTH)
        {
            return false; // Pipeline full
        }

        quint64 seqNum = m_sequenceCounter.fetch_add(1);
        SQRTInstruction sqrtInstr(instruction, pc, seqNum);

        // Pre-decode for pipeline optimization
        if (!decodeSQRTInstruction(sqrtInstr))
        {
            return false;
        }

        m_fetchQueue.enqueue(sqrtInstr);
        m_pipelineCondition.wakeOne();

        return true;
    }
  
void opcode14_executorAlphaSQRT::setPipelineDepth(int depth) {   
    // TODO 
    }

void opcode14_executorAlphaSQRT::setSQRTWorkerThreads(int count) {
    // TODO 
}

void opcode14_executorAlphaSQRT::clearStatistics()  { 
    // TODO
}
bool opcode14_executorAlphaSQRT::executeSQRT(const DecodedInstruction &instruction) { 
    //TODO
}

bool opcode14_executorAlphaSQRT::decodeSQRTInstruction(SQRTInstruction &instr)
{
    return decodeFloatingFields(instr.instruction.raw, instr);
}
// bool AlphaSQRTExecutor::decodeSQRTInstruction(SQRTInstruction &instr)
// {
// 
//   
//     quint32 raw = instr.instruction.raw;
//     instr.function = (raw >> 5) & 0x7FF; // 11-bit function code for 0x14
//     instr.srcRegister = (raw >> 16) & 0x1F;
//     instr.dstRegister = raw & 0x1F;
// 
//     // Determine precision from function code
//     if ((instr.function & 0x00F) == 0x00A)
//     {
//         instr.precision = SQRTInstruction::F_FLOAT;
//     }
//     else if ((instr.function & 0x00F) == 0x00B)
//     {
//         instr.precision = SQRTInstruction::S_FLOAT;
//     }
//     else if ((instr.function & 0x0F0) == 0x020 || (instr.function & 0x0F0) == 0x0A0)
//     {
//         instr.precision = SQRTInstruction::G_FLOAT;
//     }
//     else if ((instr.function & 0x00F) == 0x00B && (instr.function & 0x0F0) != 0x000)
//     {
//         instr.precision = SQRTInstruction::T_FLOAT;
//     }
// 
//     // Determine rounding mode from function code
//     if (instr.function & 0x400)
//     { // Scaled bit
//         if (instr.function & 0x200)
//         { // Additional rounding control
//             instr.rounding = SQRTInstruction::CHOPPED;
//         }
//         else
//         {
//             instr.rounding = SQRTInstruction::DEFAULT;
//         }
//     }
//     else
//     {
//         instr.rounding = SQRTInstruction::DEFAULT;
//     }
// 
//     // Estimate execution complexity
//     analyzeSQRTComplexity(instr);
// 
//     return true;
// }

void opcode14_executorAlphaSQRT::dispatchWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_dispatchQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 10); // Fast dispatch
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_dispatchQueue.isEmpty())
        {
            SQRTInstruction instr = m_dispatchQueue.dequeue();

            // Intelligent dispatch to optimal SQRT unit
            int bestUnit = selectOptimalSQRTUnit(instr);
            m_sqrtUnits[bestUnit].enqueue(instr);

            m_pipelineCondition.wakeAll(); // Wake all SQRT units
        }
    }
}

void opcode14_executorAlphaSQRT::sqrtUnitWorker(int unitId)
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_sqrtUnits[unitId].isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 20);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_sqrtUnits[unitId].isEmpty())
        {
            SQRTInstruction instr = m_sqrtUnits[unitId].dequeue();
            locker.unlock();

            // Perform actual SQRT computation
            quint64 startTime = QDateTime::currentMSecsSinceEpoch();
            bool success = false;

            switch (instr.precision)
            {
            case SQRTInstruction::F_FLOAT:
                success = executeSQRTF(instr, instr.result);
                break;
            case SQRTInstruction::S_FLOAT:
                success = executeSQRTS(instr, instr.result);
                break;
            case SQRTInstruction::G_FLOAT:
                success = executeSQRTG(instr, instr.result);
                break;
            case SQRTInstruction::T_FLOAT:
                success = executeSQRTT(instr, instr.result);
                break;
            }

            quint64 endTime = QDateTime::currentMSecsSinceEpoch();
            int actualCycles = static_cast<int>(endTime - startTime);

            locker.relock();
            instr.isCompleted = success;

            // Update performance metrics
            {
                QMutexLocker statsLocker(&m_statsMutex);
                m_totalSqrtCycles += actualCycles;
                asa_utils::safeIncrement( m_sqrtInstructions);
            }

            m_completionQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();

            emit sqrtInstructionExecuted(instr.function, actualCycles, success);
        }
    }
}

bool opcode14_executorAlphaSQRT::executeSQRTS( SQRTInstruction &instr, quint64 &result)
{
    // IEEE 754 single-precision SQRT
    if (!readFloatRegisterWithCache(instr.srcRegister, instr.operand))
    {
        return false;
    }

    // Extract single-precision float from register
    quint32 operand32 = static_cast<quint32>(instr.operand & 0xFFFFFFFF);

    // Set appropriate rounding mode
    setRoundingMode(instr.rounding);

    // Perform SQRT computation
    quint32 result32 = sqrtFloat32(operand32, instr.rounding);

    // Store result back in 64-bit format
    result = static_cast<quint64>(result32);

    restoreRoundingMode();

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        asa_utils::safeIncrement(m_floatS);
        switch (instr.rounding)
        {
        case SQRTInstruction::DEFAULT:
            asa_utils::safeIncrement(m_roundDefault);
            break;
        case SQRTInstruction::CHOPPED:
            asa_utils::safeIncrement(m_roundChop);
            break;
        case SQRTInstruction::MINUS:
            asa_utils::safeIncrement(m_roundMinus);
            break;
        case SQRTInstruction::PLUS:
            asa_utils::safeIncrement(m_roundPlus)
            break;
        }
    }

    return checkSQRTExceptions(instr.operand, result);
}

double opcode14_executorAlphaSQRT::newtonsMethodSQRT(double x, int iterations)
{
    if (x <= 0.0)
        return 0.0;

    // Initial guess using bit manipulation (fast inverse square root concept)
    double guess = x * 0.5;

    // Newton's method: x_{n+1} = 0.5 * (x_n + x / x_n)
    for (int i = 0; i < iterations; ++i)
    {
        guess = 0.5 * (guess + x / guess);
    }

    return guess;
}

void opcode14_executorAlphaSQRT::printAdvancedStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    qDebug() << "=== Advanced SQRT Executor Statistics ===";
    qDebug() << "Total SQRT Instructions:" << m_sqrtInstructions;
    qDebug() << "Precision Distribution:";
    qDebug() << "  F-Float:" << m_floatF << "S-Float:" << m_floatS;
    qDebug() << "  G-Float:" << m_floatG << "T-Float:" << m_floatT;

    qDebug() << "Rounding Mode Distribution:";
    qDebug() << "  Default:" << m_roundDefault << "Chopped:" << m_roundChop;
    qDebug() << "  Minus:" << m_roundMinus << "Plus:" << m_roundPlus;

    if (m_sqrtInstructions > 0)
    {
        double avgCycles = static_cast<double>(m_totalSqrtCycles) / m_sqrtInstructions;
        qDebug() << "Average SQRT Cycles:" << avgCycles;
        qDebug() << "Pipeline Utilization:" << m_pipelineUtilization.load() << "%";
        qDebug() << "Parallel Efficiency:" << m_parallelEfficiency.load() << "%";
    }

    qDebug() << "Exceptions Raised:" << m_exceptionsRaised;
}

int opcode14_executorAlphaSQRT::selectOptimalSQRTUnit(const SQRTInstruction &instr)
{
    // Intelligent load balancing across SQRT units
    int bestUnit = 0;
    int minQueueSize = m_sqrtUnits[0].size();

    for (int i = 1; i < MAX_SQRT_WORKERS; ++i)
    {
        if (m_sqrtUnits[i].size() < minQueueSize)
        {
            minQueueSize = m_sqrtUnits[i].size();
            bestUnit = i;
        }
    }

    return bestUnit;
}

// Pipeline Workers
void opcode14_executorAlphaSQRT::fetchWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_fetchQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_fetchQueue.isEmpty() && m_decodeQueue.size() < MAX_PIPELINE_DEPTH)
        {
            SQRTInstruction instr = m_fetchQueue.dequeue();

            // Async fetch instruction from cache
            quint32 instruction;
            if (fetchInstructionWithCache(instr.pc, instruction))
            {
                instr.isReady = true;
                m_decodeQueue.enqueue(instr);
                m_pipelineCondition.wakeOne();
            }
            else
            {
                // Cache miss - requeue for retry
                m_fetchQueue.enqueue(instr);
            }
        }
    }
}

void opcode14_executorAlphaSQRT::decodeWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_decodeQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 50);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_decodeQueue.isEmpty() && m_dispatchQueue.size() < MAX_PIPELINE_DEPTH)
        {
            SQRTInstruction instr = m_decodeQueue.dequeue();

            // Read source operand
            if (readFloatRegisterWithCache(instr.srcRegister, instr.operand))
            {
                // Analyze complexity for optimal dispatch
                analyzeSQRTComplexity(instr);
                instr.expectedCycles = estimateExecutionCycles(instr);

                m_dispatchQueue.enqueue(instr);
                m_pipelineCondition.wakeOne();
            }
            else
            {
                // Register read failed - requeue
                m_decodeQueue.enqueue(instr);
            }
        }
    }
}

void opcode14_executorAlphaSQRT::completionWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_completionQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 30);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_completionQueue.isEmpty())
        {
            // Process completed SQRT operations in sequence order
            SQRTInstruction instr = m_completionQueue.dequeue();

            // Handle exceptions if any occurred
            if (instr.hasException)
            {
                raiseSQRTException(instr.exceptionType, instr);
            }

            // Move to writeback queue (maintain program order)
            m_writebackQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

void opcode14_executorAlphaSQRT::writebackWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_writebackQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 40);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_writebackQueue.isEmpty())
        {
            SQRTInstruction instr = m_writebackQueue.dequeue();

            if (instr.isCompleted && !instr.hasException)
            {
                // Write result back to register
                writeFloatRegisterWithCache(instr.dstRegister, instr.result);
            }

            // Update performance metrics
            updateUtilizationMetrics();
        }
    }
}


// Analysis and Estimation Methods
void opcode14_executorAlphaSQRT::analyzeSQRTComplexity(SQRTInstruction &instr)
{
    // Analyze complexity based on function code and operand
    instr.isHighLatency = true; // All SQRT operations are high latency

    // Complexity factors
    bool isScaled = (instr.function & 0x400) != 0;
    bool hasRounding = (instr.function & 0x300) != 0;
    bool hasChecking = (instr.function & 0x100) != 0;

    // Adjust expected cycles based on complexity
    int baseCycles = 15; // Minimum SQRT cycles

    if (instr.precision == SQRTInstruction::T_FLOAT)
    {
        baseCycles = 45; // Double precision takes longer
    }
    else if (instr.precision == SQRTInstruction::G_FLOAT)
    {
        baseCycles = 35; // VAX G_floating
    }
    else if (instr.precision == SQRTInstruction::S_FLOAT)
    {
        baseCycles = 25; // IEEE single precision
    }
    else
    {                    // F_FLOAT
        baseCycles = 20; // VAX F_floating
    }

    if (isScaled)
        baseCycles += 5;
    if (hasRounding)
        baseCycles += 3;
    if (hasChecking)
        baseCycles += 2;

    instr.expectedCycles = baseCycles;
}

int opcode14_executorAlphaSQRT::estimateExecutionCycles(const SQRTInstruction &instr)
{
    int cycles = instr.expectedCycles;

    // Adjust based on operand characteristics
    double operandValue;
    if (instr.precision == SQRTInstruction::S_FLOAT || instr.precision == SQRTInstruction::F_FLOAT)
    {
        float temp;
        quint32 temp32 = static_cast<quint32>(instr.operand);
        std::memcpy(&temp, &temp32, sizeof(float));
        operandValue = static_cast<double>(temp);
    }
    else
    {
        std::memcpy(&operandValue, &instr.operand, sizeof(double));
    }

    // Special cases that affect timing
    if (operandValue == 0.0 || operandValue == 1.0)
    {
        cycles = 5; // Trivial cases
    }
    else if (operandValue < 0.0)
    {
        cycles += 10; // Exception handling overhead
    }
    else if (std::isinf(operandValue) || std::isnan(operandValue))
    {
        cycles = 8; // Special value handling
    }

    return cycles;
}

// SQRT Execution Methods by Precision
bool opcode14_executorAlphaSQRT::executeSQRTF(const SQRTInstruction &instr, quint64 &result)
{
    // VAX F_floating SQRT
    quint32 operand32 = static_cast<quint32>(instr.operand & 0xFFFFFFFF);

    setRoundingMode(instr.rounding);
    quint32 result32 = sqrtVAXF(operand32, instr.rounding);
    restoreRoundingMode();

    result = static_cast<quint64>(result32);

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_floatF);

    return checkSQRTExceptions(instr.operand, result);
}

bool opcode14_executorAlphaSQRT::executeSQRTG(const SQRTInstruction &instr, quint64 &result)
{
    // VAX G_floating SQRT
    setRoundingMode(instr.rounding);
    result = sqrtVAXG(instr.operand, instr.rounding);
    restoreRoundingMode();

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_floatG);

    return checkSQRTExceptions(instr.operand, result);
}

bool opcode14_executorAlphaSQRT::executeSQRTT(const SQRTInstruction &instr, quint64 &result)
{
    // IEEE T_floating (double precision) SQRT
    setRoundingMode(instr.rounding);
    result = sqrtFloat64(instr.operand, instr.rounding);
    restoreRoundingMode();

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_floatT);

    return checkSQRTExceptions(instr.operand, result);
}

// Rounding Mode Control
void opcode14_executorAlphaSQRT::setRoundingMode(SQRTInstruction::RoundingMode mode)
{
    switch (mode)
    {
    case SQRTInstruction::DEFAULT:
        std::fesetround(FE_TONEAREST);
        break;
    case SQRTInstruction::CHOPPED:
        std::fesetround(FE_TOWARDZERO);
        break;
    case SQRTInstruction::MINUS:
        std::fesetround(FE_DOWNWARD);
        break;
    case SQRTInstruction::PLUS:
        std::fesetround(FE_UPWARD);
        break;
    case SQRTInstruction::DYNAMIC:
        // Use FPCR setting from CPU
        // std::fesetround(m_cpu->getFPCRRoundingMode());
        std::fesetround(FE_TONEAREST); // Default fallback
        break;
    }
}

void opcode14_executorAlphaSQRT::restoreRoundingMode()
{
    // Restore to IEEE default
    std::fesetround(FE_TONEAREST);
}

// IEEE 754 Double Precision SQRT
quint64 opcode14_executorAlphaSQRT::sqrtFloat64(quint64 operand, SQRTInstruction::RoundingMode rounding)
{
    double value;
    std::memcpy(&value, &operand, sizeof(double));

    // Handle special cases
    if (value < 0.0)
    {
        raiseSQRTException(0x10, SQRTInstruction()); // Invalid operation
        quint64 nan = 0x7FF8000000000000ULL;         // Quiet NaN
        return nan;
    }

    if (value == 0.0 || std::isinf(value))
    {
        return operand; // sqrt(0) = 0, sqrt(inf) = inf
    }

    // High-precision SQRT computation
    double result = newtonsMethodSQRT(value, 15); // More iterations for double precision

    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));

    return resultBits;
}


// VAX F_floating SQRT
quint32 opcode14_executorAlphaSQRT::sqrtVAXF(quint32 operand, SQRTInstruction::RoundingMode rounding)
{
    // VAX F_floating format: 1 sign bit, 8 exponent bits, 23 fractional bits
    // Bias = 128, different from IEEE 754

    if (operand == 0)
        return 0; // sqrt(0) = 0

    // Extract VAX F_floating components
    bool sign = (operand & 0x80000000) != 0;
    int exponent = ((operand >> 23) & 0xFF) - 128;      // Remove VAX bias
    quint32 mantissa = (operand & 0x7FFFFF) | 0x800000; // Implicit leading 1

    if (sign)
    {
        raiseSQRTException(0x10, SQRTInstruction()); // Invalid: negative number
        return 0;                                    // VAX reserved operand
    }

    // Convert to IEEE format for computation
    float ieee_value = ldexpf(static_cast<float>(mantissa) / (1 << 23), exponent);
    double precise_result = newtonsMethodSQRT(static_cast<double>(ieee_value), 10);
    float result_float = static_cast<float>(precise_result);

    // Convert back to VAX F_floating format
    quint32 ieee_bits;
    std::memcpy(&ieee_bits, &result_float, sizeof(float));

    // Convert IEEE to VAX F_floating (simplified)
    int result_exp = ((ieee_bits >> 23) & 0xFF) - 127 + 128; // IEEE to VAX bias
    quint32 result_mant = ieee_bits & 0x7FFFFF;

    return ((result_exp & 0xFF) << 23) | result_mant;
}

// VAX G_floating SQRT
quint64 opcode14_executorAlphaSQRT::sqrtVAXG(quint64 operand, SQRTInstruction::RoundingMode rounding)
{
    // VAX G_floating format: 1 sign bit, 11 exponent bits, 52 fractional bits
    // Bias = 1024, different from IEEE 754

    if (operand == 0)
        return 0; // sqrt(0) = 0

    // Extract VAX G_floating components
    bool sign = (operand & 0x8000000000000000ULL) != 0;
    int exponent = ((operand >> 52) & 0x7FF) - 1024;                         // Remove VAX bias
    quint64 mantissa = (operand & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL; // Implicit leading 1

    if (sign)
    {
        raiseSQRTException(0x10, SQRTInstruction()); // Invalid: negative number
        return 0;                                    // VAX reserved operand
    }

    // Convert to IEEE format for computation
    double ieee_value = ldexp(static_cast<double>(mantissa) / (1ULL << 52), exponent);
    double result = newtonsMethodSQRT(ieee_value, 15);

    // Convert back to VAX G_floating format
    quint64 ieee_bits;
    std::memcpy(&ieee_bits, &result, sizeof(double));

    // Convert IEEE to VAX G_floating (simplified)
    int result_exp = ((ieee_bits >> 52) & 0x7FF) - 1023 + 1024; // IEEE to VAX bias
    quint64 result_mant = ieee_bits & 0xFFFFFFFFFFFFFULL;

    return ((static_cast<quint64>(result_exp & 0x7FF)) << 52) | result_mant;
}

// Fast Inverse Square Root (Quake algorithm)
double opcode14_executorAlphaSQRT::fastInverseSQRT(double x)
{
    if (x <= 0.0)
        return 0.0;

    // Quake-style fast inverse square root adapted for double precision
    quint64 i;
    double x2, y;
    const double threehalfs = 1.5;

    x2 = x * 0.5;
    y = x;
    std::memcpy(&i, &y, sizeof(quint64));
    i = 0x5FE6EB50C7B537A9ULL - (i >> 1); // Magic number for double precision
    std::memcpy(&y, &i, sizeof(double));
    y = y * (threehalfs - (x2 * y * y)); // 1st iteration
    y = y * (threehalfs - (x2 * y * y)); // 2nd iteration

    return 1.0 / y; // Return actual square root
}

// Exception Handling
void opcode14_executorAlphaSQRT::raiseSQRTException(quint32 exceptionType, const SQRTInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_exceptionsRaised);

    qDebug() << "SQRT Exception:" << Qt::hex << exceptionType << "at PC:" << instr.pc << "Function:" << instr.function;

    emit sqrtExceptionRaised(exceptionType, instr.pc);

    // Could integrate with CPU exception handling here
    // m_cpu->raiseFloatingPointException(exceptionType);
}

// Performance Optimization
void opcode14_executorAlphaSQRT::optimizePipelineBalance()
{
    // Analyze pipeline performance and adjust
    QMutexLocker locker(&m_statsMutex);

    if (m_sqrtInstructions < 100)
        return; // Need sufficient data

    // Calculate average utilization across SQRT units
    quint64 totalWork = 0;
    for (int i = 0; i < MAX_SQRT_WORKERS; ++i)
    {
        totalWork += m_sqrtUnits[i].size();
    }

    double avgUtilization = static_cast<double>(totalWork) / MAX_SQRT_WORKERS;

    // If imbalanced, could trigger unit redistribution
    if (avgUtilization > MAX_PIPELINE_DEPTH * 0.8)
    {
        qDebug() << "SQRT Pipeline: High utilization detected, consider more units";
    }

    // Store metrics for monitoring
    m_pipelineUtilization.store(static_cast<quint64>(avgUtilization * 100));
}

void opcode14_executorAlphaSQRT::updateUtilizationMetrics()
{
    static quint64 lastUpdate = 0;
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if (currentTime - lastUpdate < 1000)
        return; // Update every second
    lastUpdate = currentTime;

    QMutexLocker locker(&m_statsMutex);

    // Calculate pipeline efficiency
    quint64 activeUnits = 0;
    for (int i = 0; i < MAX_SQRT_WORKERS; ++i)
    {
        if (!m_sqrtUnits[i].isEmpty())
        {
            activeUnits++;
        }
    }

    double efficiency = static_cast<double>(activeUnits) / MAX_SQRT_WORKERS * 100.0;
    m_parallelEfficiency.store(static_cast<quint64>(efficiency));

    // Calculate average latency
    if (m_sqrtInstructions > 0)
    {
        quint64 avgLatency = m_totalSqrtCycles / m_sqrtInstructions;
        m_averageLatency.store(avgLatency);
    }

    // Trigger optimization if needed
    if (m_sqrtInstructions % 1000 == 0)
    {
        optimizePipelineBalance();
    }

    emit pipelineUtilizationChanged(efficiency);
}

// Cache Operations (inherited pattern from FP executor)
bool opcode14_executorAlphaSQRT::fetchInstructionWithCache(quint64 pc, quint32 &instruction)
{
    QMutexLocker locker(&m_statsMutex);

    // Stage 1: TLB Translation
    quint64 physicalPC;
    //AlphaTranslationCache *iTLB = m_iTLB.data();

    if (m_iTLB && !m_iTLB->lookup(pc, m_cpu->getCurrentASN(), false, true, physicalPC))
    {
        // TLB miss
        return false;
    }
    else if (!m_iTLB)
    {
        physicalPC = pc; // Direct mapping if no TLB
    }

    // Stage 2: Cache Hierarchy
    if (m_instructionCache)
    {
        bool hit = m_instructionCache->read(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4);
        if (hit)
        {
            asa_utils::safeIncrement(m_l1ICacheHits);
           // m_l1ICacheHits++;
            return true;
        }
        else
        {
            m_l1ICacheMisses++;
        }
    }

    // Try L2 cache
    if (m_level2Cache)
    {
        bool hit = m_level2Cache->read(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4);
        if (hit)
        {
            // Fill L1 cache
            if (m_instructionCache)
            {
                m_instructionCache->write(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4);
            }
            return true;
        }
    }

    // Fallback to CPU memory access
    return m_cpu ? m_cpu->readMemory(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4) : false;
}

bool opcode14_executorAlphaSQRT::readFloatRegisterWithCache(quint8 reg, quint64 &value)
{
    if (!m_cpu)
        return false;

    // Read from floating-point register file
    value = m_cpu->getFloatRegister(reg);

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_l1DCacheHits);
   // m_l1DCacheHits++; // Register access is always a hit

    return true;
}

bool opcode14_executorAlphaSQRT::writeFloatRegisterWithCache(quint8 reg, quint64 value)
{
    if (!m_cpu)
        return false;

    // Write to floating-point register file
    m_cpu->setFloatRegister(reg, value);

    QMutexLocker locker(&m_statsMutex);
    m_l1DCacheHits++; // Register access is always a hit

    return true;
}

