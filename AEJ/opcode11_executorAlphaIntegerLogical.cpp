#include "opcode11_executorAlphaIntegerLogical.h"
#include <QMutexLocker>
#include "utilitySafeIncrement.h "
#include "AlphaCPU_refactored.h"
#include "opcode14_executorAlphaSQRT.h"

opcode11_executorAlphaIntegerLogical::opcode11_executorAlphaIntegerLogical(AlphaCPU *cpu, QObject *parent) 
    : QObject(parent)
    , m_cpu(cpu)
{
    // Constructor implementation
}

opcode11_executorAlphaIntegerLogical::~opcode11_executorAlphaIntegerLogical() { stopAsyncPipeline(); }

void opcode11_executorAlphaIntegerLogical::startAsyncPipeline()
{
    if (m_pipelineActive.exchange(true))
    {
        return; // Already running
    }

    // Clear pipeline state
    {
        QMutexLocker locker(&m_pipelineMutex);
        m_fetchQueue.clear();
        m_decodeQueue.clear();
        m_executeQueue.clear();
        m_writebackQueue.clear();
        m_sequenceCounter.store(0);
    }

    // Start worker threads
    m_fetchWorker = QtConcurrent::run([this]() { fetchWorker(); });
    m_decodeWorker = QtConcurrent::run([this]() { decodeWorker(); });
    m_executeWorker = QtConcurrent::run([this]() { executeWorker(); });
    m_writebackWorker = QtConcurrent::run([this]() { writebackWorker(); });

    DEBUG_LOG("Async Integer Logical pipeline started");
}

bool opcode11_executorAlphaIntegerLogical::submitInstruction(const DecodedInstruction &instruction, quint64 pc)
{
    if (!m_pipelineActive.load())
    {
        return false;
    }

    QMutexLocker locker(&m_pipelineMutex);

    if (m_fetchQueue.size() >= MAX_PIPELINE_DEPTH)
    {
        emit pipelineStalled("Pipeline full");
        return false;
    }

    quint64 seqNum = m_sequenceCounter.fetch_add(1);
    IntegerInstruction intInstr(instruction, pc, seqNum);
    analyzeDependencies(intInstr);

    m_fetchQueue.enqueue(intInstr);
    m_pipelineCondition.wakeOne();

    return true;
}

void opcode11_executorAlphaIntegerLogical::executeWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_executeQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 50); // Fast wake for integer ops
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_executeQueue.isEmpty())
        {
            IntegerInstruction instr = m_executeQueue.dequeue();

            if (!checkDependencies(instr))
            {
                m_executeQueue.enqueue(instr); // Requeue
                continue;
            }

            locker.unlock(); // Release lock during execution

            // Execute based on opcode
            bool success = false;
            quint32 opcode = (instr.instruction.raw >> 26) & 0x3F;

            switch (opcode)
            {
            case 0x11:
                success = executeOpcode11(instr);
                break;
            case 0x12:
                success = executeOpcode12(instr);
                break;
            case 0x13:
                success = executeOpcode13(instr);
                break;
            default:
                success = false;
                break;
            }

            locker.relock();
            instr.isCompleted = success;

            m_writebackQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

bool opcode11_executorAlphaIntegerLogical::executeOpcode11(IntegerInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_intInstructions);

    quint32 function = (instr.instruction.raw >> 5) & 0x7F; // 7-bit function code
    quint64 result = 0;
    bool success = false;

    switch (function)
    {
    // Logical operations
    case FUNC_AND:
        success = executeAND(instr, result);
        asa_utils::safeIncrement(m_logicalOps);
        break;
    case FUNC_BIC:
        success = executeBIC(instr, result);
        asa_utils::safeIncrement(m_logicalOps);
        break;
    case FUNC_BIS:
        success = executeBIS(instr, result);
        asa_utils::safeIncrement(m_logicalOps);
        break;
    case FUNC_XOR:
        success = executeXOR(instr, result);
        asa_utils::safeIncrement(m_logicalOps);
        break;
    case FUNC_EQV:
        success = executeEQV(instr, result);
        asa_utils::safeIncrement(m_logicalOps);
        break;
    case FUNC_ORNOT:
        success = executeORNOT(instr, result);
        asa_utils::safeIncrement(m_logicalOps);
        break;

    // Byte manipulation - Low
    case FUNC_MSKBL:
        success = executeMSKBL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_EXTBL:
        success = executeEXTBL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_INSBL:
        success = executeINSBL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;

    // Word manipulation - Low
    case FUNC_MSKWL:
        success = executeMSKWL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_EXTWL:
        success = executeEXTWL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_INSWL:
        success = executeINSWL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;

    // Longword manipulation - Low
    case FUNC_MSKLL:
        success = executeMSKLL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_EXTLL:
        success = executeEXTLL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_INSLL:
        success = executeINSLL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;

    // Quadword manipulation - Low
    case FUNC_MSKQL:
        success = executeMSKQL(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_EXTQL:
        success = executeEXTQL(instr, result);
       asa_utils::safeIncrement( m_bitManipOps);
        break;
    case FUNC_INSQL:
        success = executeINSQL(instr, result);
       asa_utils::safeIncrement( m_bitManipOps);
        break;

    // High variants
    case FUNC_MSKBH:
        success = executeMSKBH(instr, result);
       asa_utils::safeIncrement( m_bitManipOps);
        break;
    case FUNC_EXTBH:
        success = executeEXTBH(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;
    case FUNC_INSBH:
        success = executeINSBH(instr, result);
        asa_utils::safeIncrement(m_bitManipOps);
        break;

        // Continue for all other functions...

    default:
        WARN_LOG(QString("Unknown Integer Logical function:0x%1").arg(function));
        success = false;
        break;
    }

    if (success)
    {
        instr.result = result;
        emit intInstructionExecuted(0x11, function, true);
    }

    return success;
}

// Example logical operations
bool opcode11_executorAlphaIntegerLogical::executeAND(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF; // 8-bit literal
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    result = raValue & rbValue;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeBIC(const IntegerInstruction &instr, quint64 &result)
{
    // BIC = Bit Clear = Ra AND (NOT Rb)
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    result = raValue & (~rbValue);
    return true;
}

// Example bit manipulation
bool opcode11_executorAlphaIntegerLogical::executeMSKBL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    // Mask Byte Low - create mask for low-order bytes
    int bytePos = rbValue & 0x7; // Lower 3 bits determine byte position
    quint64 mask = createByteMask(bytePos, false);

    result = raValue & mask;
    return true;
}

// Helper for bit manipulation
quint64 opcode11_executorAlphaIntegerLogical::createByteMask(int bytePos, bool high)
{
    quint64 mask = 0;

    if (high)
    {
        // High variant - mask from byte position to end
        for (int i = bytePos; i < 8; ++i)
        {
            mask |= (0xFFULL << (i * 8));
        }
    }
    else
    {
        // Low variant - mask from start to byte position
        for (int i = 0; i < bytePos; ++i)
        {
            mask |= (0xFFULL << (i * 8));
        }
    }

    return mask;
}

void opcode11_executorAlphaIntegerLogical::printStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    qDebug() << "=== Alpha Integer Logical Executor Statistics ===";
    qDebug() << "Total Integer Instructions:" << m_intInstructions;
    qDebug() << "Logical Operations:" << m_logicalOps;
    qDebug() << "Bit Manipulation Ops:" << m_bitManipOps;
    qDebug() << "Shift Operations:" << m_shiftOps;
    qDebug() << "ZAP Operations:" << m_zapOps;

    qDebug() << "\n=== Cache Performance ===";
    qDebug() << "L1 I-Cache: Hits=" << m_l1ICacheHits << ", Misses=" << m_l1ICacheMisses;
    qDebug() << "L1 D-Cache: Hits=" << m_l1DCacheHits << ", Misses=" << m_l1DCacheMisses;

    if (m_intInstructions > 0)
    {
        qDebug() << "Instructions/sec:" << (m_intInstructions * 1000) / QDateTime::currentMSecsSinceEpoch();
    }
}

// ========== COMPLETE exectorAlphaIntegerLogical Missing Methods ==========

// Pipeline Control
void opcode11_executorAlphaIntegerLogical::stopAsyncPipeline()
{
    if (!m_pipelineActive.exchange(false))
    {
        return; // Already stopped
    }

    // Wake up all workers
    m_pipelineCondition.wakeAll();

    // Wait for workers to complete
    m_fetchWorker.waitForFinished();
    m_decodeWorker.waitForFinished();
    m_executeWorker.waitForFinished();
    m_writebackWorker.waitForFinished();

    qDebug() << "Async Integer Logical pipeline stopped";
}

// Synchronous Execution Methods
// bool exectorAlphaIntegerLogical::executeIntegerLogical(const DecodedInstruction &instruction)
// {
//     IntegerInstruction instr(instruction, 0, 0);
//     if (!decodeSQRTInstruction(instr))
//         return false;
//     return executeOpcode11(instr);
// }

// ----------------------- exectorAlphaIntegerLogical.cpp -----------------------

#include "opcode11_executorAlphaIntegerLogical.h"
#include "opcode14_executorAlphaSQRT.h"
#include "utilitySafeIncrement.h"
#include <QMutexLocker>

// {constructor/destructor, pipeline startup/shutdown code unchanged) 

// Synchronous execution (fallback)
bool opcode11_executorAlphaIntegerLogical::executeIntegerLogical(const DecodedInstruction &instruction)
{
    // Build a fresh IntegerInstruction from the decoded bits
    IntegerInstruction instr(instruction, /*pc=*/0, /*seqNum=*/0);

    // Only decode & run if majorOpcode ? {0x11, 0x12, 0x13}
    if (!decodeIntegerLogicalInstruction(instr))
        return false;

    // Based on opcode, call the appropriate executeXXX method
    quint32 opcode = (instr.instruction.raw >> 26) & 0x3F;
    bool success = false;
    switch (opcode)
    {
    case 0x11:
        success = executeOpcode11(instr);
        break;
    case 0x12:
        success = executeOpcode12(instr);
        break;
    case 0x13:
        success = executeOpcode13(instr);
        break;
    default:
        success = false;
        break;
    }

    return success;
}

/**
 * @brief Decode only 'Integer Logical' (0x11), Shift/ZAP (0x12), Multiply (0x13).
 * Returns false if the majorOpcode is outside [0x11 - 0x13].
 *
 * Note: IntegerInstruction has no 'function', 'srcRegA', or 'srcRegB' members.
 *       We simply verify the opcode and leave actual function extraction
 *       to the individual executeXXX methods. Dependency tracking is done
 *       by analyzeDependencies() if/when needed.
 */
bool opcode11_executorAlphaIntegerLogical::decodeIntegerLogicalInstruction(IntegerInstruction &instr)
{
    uint32_t raw32 = instr.instruction.raw;
    uint32_t opcode = (raw32 >> 26) & 0x3F;

    // Only sections 0x11–0x13 belong here
    if (opcode < 0x11 || opcode > 0x13)
    {
        return false;
    }

    // (Optional) Mark this instruction as “decoded/ready.”
    instr.isReady = true;

    // If you want to pre?populate dependency sets, call analyzeDependencies:
    analyzeDependencies(instr);

    return true;
}

// … (executeOpcode11, executeOpcode12, executeOpcode13, etc., unchanged) …

/**
 * @brief Populate srcRegisters and dstRegisters based on raw bits.
 *
 * - Ra is bits ?25:21? (unless Ra == 31 ? R31 is hardwired zero; typically no dependency)
 * - Rb is bits ?20:16? unless the literal?bit (?12?) is set, in which case it's an 8-bit literal.
 * - Re (destination) is bits ?4:0?, unless it’s R31 (which is also read?only zero on many integer ops).
 */
void opcode11_executorAlphaIntegerLogical::analyzeDependencies(IntegerInstruction &instr)
{
    uint32_t raw32 = instr.instruction.raw;
    quint8 ra = (raw32 >> 21) & 0x1F;                      // bits ?25:21?
    bool isLiteral = ((raw32 >> 12) & 0x1) != 0;           // bit ?12?
    quint8 rb = isLiteral ? 0xFF : ((raw32 >> 16) & 0x1F); // only if not literal
    quint8 re = raw32 & 0x1F;                              // bits ?4:0?

    instr.srcRegisters.clear();
    instr.dstRegisters.clear();

    // Ra as source (if not R31)
    if (ra != 31)
        instr.srcRegisters.insert(ra);

    // Rb as source unless it’s a literal or R31
    if (!isLiteral && rb != 31)
        instr.srcRegisters.insert(rb);

    // Re as destination unless Re == 31
    if (re != 31)
        instr.dstRegisters.insert(re);
}

/**
 * @brief Read a 64-bit integer register (Ra or Rb) into 'value'.
 *
 * Return false if it fails to fetch (e.g. if m_cpu is null).
 */
bool opcode11_executorAlphaIntegerLogical::readIntegerRegisterWithCache(quint8 reg, quint64 &value)
{
    if (!m_cpu)
        return false;

    // Direct register file read ? always a “hit”
    value = m_cpu->getIntegerRegister(reg);

    QMutexLocker locker(&m_statsMutex);
   asa_utils::safeIncrement(m_l1DCacheHits);
    return true;
}

/**
 * @brief Write back a 64-bit value into an integer register (Re).
 */
bool opcode11_executorAlphaIntegerLogical::writeIntegerRegisterWithCache(quint8 reg, quint64 value)
{
    if (!m_cpu)
        return false;

    m_cpu->setIntegerRegister(reg, value);
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_l1DCacheHits);
    return true;
}

// ... the rest of your executeXXX functions (e.g. executeAND, executeBIC, etc.)
//     all remain exactly as you wrote them, since they extract ra/rb from raw bits.
//     They do not—and cannot—refer to any instr.srcRegA or instr.srcRegB members.
//     Instead, they do something like:
//
//     quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
//     quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
//     ...
//
//     which matches your existing code perfectly.
//



bool opcode11_executorAlphaIntegerLogical::executeShiftZap(const DecodedInstruction &instruction)
{
    IntegerInstruction instr(instruction, 0, 0);
    return executeOpcode12(instr);
}

bool opcode11_executorAlphaIntegerLogical::executeIntegerMultiply(const DecodedInstruction &instruction)
{
    IntegerInstruction instr(instruction, 0, 0);
    return executeOpcode13(instr);
}

void opcode11_executorAlphaIntegerLogical::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);

    m_intInstructions = 0;
    m_logicalOps = 0;
    m_bitManipOps = 0;
    m_shiftOps = 0;
    m_zapOps = 0;
    m_l1ICacheHits = 0;
    m_l1ICacheMisses = 0;
    m_l1DCacheHits = 0;
    m_l1DCacheMisses = 0;
}

// Pipeline Workers
void opcode11_executorAlphaIntegerLogical::fetchWorker()
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
            IntegerInstruction instr = m_fetchQueue.dequeue();

            // Fetch instruction from cache
            quint32 instruction;
            if (fetchInstructionWithCache(instr.pc, instruction))
            {
                instr.isReady = true;
                m_decodeQueue.enqueue(instr);
                m_pipelineCondition.wakeOne();
            }
            else
            {
                // Cache miss - requeue
                m_fetchQueue.enqueue(instr);
            }
        }
    }
}

void opcode11_executorAlphaIntegerLogical::decodeWorker()
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

        if (!m_decodeQueue.isEmpty() && m_executeQueue.size() < MAX_PIPELINE_DEPTH)
        {
            IntegerInstruction instr = m_decodeQueue.dequeue();

            // Decode is fast for integer operations
            instr.isReady = true;
            m_executeQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

void opcode11_executorAlphaIntegerLogical::writebackWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_writebackQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 30);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_writebackQueue.isEmpty())
        {
            IntegerInstruction instr = m_writebackQueue.dequeue();

            if (instr.isCompleted && instr.writeResult)
            {
                // Write result back to register
                writeIntegerRegisterWithCache(instr.dstRegisters.values().first(), instr.result);
            }

            // Update dependency tracking
            updateDependencies(instr);

            emit intInstructionExecuted((instr.instruction.raw >> 26) & 0x3F, (instr.instruction.raw >> 5) & 0x7F,
                                        instr.isCompleted);
        }
    }
}

// OpCode Execution Methods
bool opcode11_executorAlphaIntegerLogical::executeOpcode12(IntegerInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_intInstructions);

    quint32 function = (instr.instruction.raw >> 5) & 0x7F;
    quint64 result = 0;
    bool success = false;

    switch (function)
    {
    case FUNC_SLL:
        success = executeSLL(instr, result);
        asa_utils::safeIncrement(m_shiftOps);
        break;
    case FUNC_SRL:
        success = executeSRL(instr, result);
        asa_utils::safeIncrement(m_shiftOps);
        break;
    case FUNC_SRA:
        success = executeSRA(instr, result);
        asa_utils::safeIncrement(m_shiftOps);
        break;
    case FUNC_ZAP:
        success = executeZAP(instr, result);
        asa_utils::safeIncrement(m_zapOps);
        break;
    case FUNC_ZAPNOT:
        success = executeZAPNOT(instr, result);
        asa_utils::safeIncrement(m_zapOps);
        break;
    default:
        success = false;
        break;
    }

    if (success)
    {
        instr.result = result;
    }

    return success;
}

bool opcode11_executorAlphaIntegerLogical::executeOpcode13(IntegerInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_intInstructions);

    quint32 function = (instr.instruction.raw >> 5) & 0x7F;
    quint64 result = 0;
    bool success = false;

    switch (function)
    {
    case FUNC_MULQ:
        success = executeMULQ(instr, result);
        break;
    case FUNC_MULQV:
        success = executeMULQV(instr, result);
        break;
    default:
        success = false;
        break;
    }

    if (success)
    {
        instr.result = result;
    }

    return success;
}

// Logical Operations
bool opcode11_executorAlphaIntegerLogical::executeBIS(const IntegerInstruction &instr, quint64 &result)
{
    // BIS = Bit Set = Ra OR Rb
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    int shiftCount = rbValue & 0x3F; // Only low 6 bits
    qint64 signedValue = static_cast<qint64>(raValue);
    result = static_cast<quint64>(signedValue >> shiftCount); // Arithmetic shift
    return true;
}

// ZAP Operations
bool opcode11_executorAlphaIntegerLogical::executeZAP(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    quint8 mask = static_cast<quint8>(rbValue & 0xFF);
    result = zapBytes(raValue, mask);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeZAPNOT(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    quint8 mask = static_cast<quint8>(rbValue & 0xFF);
    result = zapNotBytes(raValue, mask);
    return true;
}

// Multiply Operations
bool opcode11_executorAlphaIntegerLogical::executeMULQ(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    result = raValue * rbValue;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeMULQV(const IntegerInstruction &instr, quint64 &result)
{
    // MULQV - Multiply Quadword with overflow detection
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    // Check for overflow in signed 64-bit multiplication
    qint64 signedA = static_cast<qint64>(raValue);
    qint64 signedB = static_cast<qint64>(rbValue);

    // Simple overflow check for 64-bit signed multiplication
    if (signedA != 0 && signedB != 0)
    {
        if ((signedA > 0 && signedB > 0 && signedA > LLONG_MAX / signedB) ||
            (signedA < 0 && signedB < 0 && signedA < LLONG_MAX / signedB) ||
            (signedA > 0 && signedB < 0 && signedB < LLONG_MIN / signedA) ||
            (signedA < 0 && signedB > 0 && signedA < LLONG_MIN / signedB))
        {
            // Overflow detected - raise exception
            emit pipelineStalled("Integer overflow in MULQV");
            return false;
        }
    }

    result = raValue * rbValue;
    return true;
}



bool opcode11_executorAlphaIntegerLogical::checkDependencies(const IntegerInstruction &instr) const
{
    // Simple dependency check - in a real implementation this would be more sophisticated
    // For now, assume no dependencies (integer operations are typically independent)
    return true;
}

void opcode11_executorAlphaIntegerLogical::updateDependencies(const IntegerInstruction &instr)
{
    // Update dependency tracking after instruction completion
    // In a real implementation, this would update register scoreboards
    // For now, this is a placeholder
}

// Cache Operations
bool opcode11_executorAlphaIntegerLogical::fetchInstructionWithCache(quint64 pc, quint32 &instruction)
{
    QMutexLocker locker(&m_statsMutex);

    // Stage 1: TLB Translation (if available)
    quint64 physicalPC;
    if (m_iTLB && !m_iTLB->lookup(pc, m_cpu->getCurrentASN(), false, true, physicalPC))
    {
        asa_utils::safeIncrement(m_l1ICacheMisses);
        return false; // TLB miss
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
            return true;
        }
        else
        {
            asa_utils::safeIncrement(m_l1ICacheMisses);
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

    // Try L3 cache
    if (m_level3Cache)
    {
        bool hit = m_level3Cache->read(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4);
        if (hit)
        {
            // Fill upper levels
            if (m_level2Cache)
            {
                m_level2Cache->write(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4);
            }
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




// Bit Manipulation Helper Functions
quint64 opcode11_executorAlphaIntegerLogical::createByteMask(int bytePos, bool high)
{
    quint64 mask = 0;

    if (high)
    {
        // High variant - mask from byte position to end
        for (int i = bytePos; i < 8; ++i)
        {
            mask |= (0xFFULL << (i * 8));
        }
    }
    else
    {
        // Low variant - mask from start to byte position
        for (int i = 0; i < bytePos; ++i)
        {
            mask |= (0xFFULL << (i * 8));
        }
    }

    return mask;
}

quint64 opcode11_executorAlphaIntegerLogical::createWordMask(int wordPos, bool high)
{
    quint64 mask = 0;

    if (high)
    {
        // High variant - mask from word position to end
        for (int i = wordPos; i < 4; ++i)
        {
            mask |= (0xFFFFULL << (i * 16));
        }
    }
    else
    {
        // Low variant - mask from start to word position
        for (int i = 0; i < wordPos; ++i)
        {
            mask |= (0xFFFFULL << (i * 16));
        }
    }

    return mask;
}

quint64 opcode11_executorAlphaIntegerLogical::createLongwordMask(int longPos, bool high)
{
    quint64 mask = 0;

    if (high)
    {
        // High variant - mask from longword position to end
        for (int i = longPos; i < 2; ++i)
        {
            mask |= (0xFFFFFFFFULL << (i * 32));
        }
    }
    else
    {
        // Low variant - mask from start to longword position
        for (int i = 0; i < longPos; ++i)
        {
            mask |= (0xFFFFFFFFULL << (i * 32));
        }
    }

    return mask;
}

quint64 opcode11_executorAlphaIntegerLogical::createQuadwordMask(int quadPos, bool high)
{
    // For quadword, there's only one 64-bit value
    if (high)
    {
        return 0xFFFFFFFFFFFFFFFFULL; // All bits set
    }
    else
    {
        return 0x0ULL; // No bits set for low variant when quadPos = 0
    }
}

quint64 opcode11_executorAlphaIntegerLogical::extractBytes(quint64 value, int pos, int count, bool high)
{
    if (high)
    {
        // Extract bytes from position to end
        int startByte = pos;
        int endByte = 8;
        quint64 result = 0;

        for (int i = startByte; i < endByte && i < startByte + count; ++i)
        {
            quint64 byte = (value >> (i * 8)) & 0xFF;
            result |= (byte << ((i - startByte) * 8));
        }
        return result;
    }
    else
    {
        // Extract bytes from start to position
        quint64 result = 0;

        for (int i = 0; i < pos && i < count; ++i)
        {
            quint64 byte = (value >> (i * 8)) & 0xFF;
            result |= (byte << (i * 8));
        }
        return result;
    }
}

quint64 opcode11_executorAlphaIntegerLogical::insertBytes(quint64 dest, quint64 src, int pos, int count, bool high)
{
    quint64 result = dest;

    if (high)
    {
        // Insert bytes from position to end
        for (int i = 0; i < count && (pos + i) < 8; ++i)
        {
            quint64 byte = (src >> (i * 8)) & 0xFF;
            int targetPos = pos + i;

            // Clear target byte and insert new byte
            result &= ~(0xFFULL << (targetPos * 8));
            result |= (byte << (targetPos * 8));
        }
    }
    else
    {
        // Insert bytes from start
        for (int i = 0; i < count && (pos + i) < 8; ++i)
        {
            quint64 byte = (src >> (i * 8)) & 0xFF;
            int targetPos = pos + i;

            // Clear target byte and insert new byte
            result &= ~(0xFFULL << (targetPos * 8));
            result |= (byte << (targetPos * 8));
        }
    }

    return result;
}

// ZAP Operations
quint64 opcode11_executorAlphaIntegerLogical::zapBytes(quint64 value, quint8 mask)
{
    quint64 result = value;

    // ZAP: Zero bytes where mask bit is 1
    for (int i = 0; i < 8; ++i)
    {
        if (mask & (1 << i))
        {
            // Zero this byte
            result &= ~(0xFFULL << (i * 8));
        }
    }

    return result;
}

quint64 opcode11_executorAlphaIntegerLogical::zapNotBytes(quint64 value, quint8 mask)
{
    quint64 result = value;

    // ZAPNOT: Zero bytes where mask bit is 0
    for (int i = 0; i < 8; ++i)
    {
        if (!(mask & (1 << i)))
        {
            // Zero this byte
            result &= ~(0xFFULL << (i * 8));
        }
    }

    return result;
})) {
        return false;
    }

if (isLiteral)
{
    rbValue = (instr.instruction.raw >> 13) & 0xFF;
}
else
{
    if (!readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }
}

result = raValue | rbValue;
return true;
}

bool opcode11_executorAlphaIntegerLogical::executeXOR(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    result = raValue ^ rbValue;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEQV(const IntegerInstruction &instr, quint64 &result)
{
    // EQV = Equivalence = Ra XOR (NOT Rb) = NOT (Ra XOR Rb)
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    result = ~(raValue ^ rbValue);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeORNOT(const IntegerInstruction &instr, quint64 &result)
{
    // ORNOT = Ra OR (NOT Rb)
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    result = raValue | (~rbValue);
    return true;
}

// Bit Manipulation - Byte Operations
bool opcode11_executorAlphaIntegerLogical::executeEXTBL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int bytePos = rbValue & 0x7;
    result = extractBytes(raValue, bytePos, 1, false);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSBL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int bytePos = rbValue & 0x7;
    result = insertBytes(0, raValue, bytePos, 1, false);
    return true;
}

// Word Operations
bool opcode11_executorAlphaIntegerLogical::executeMSKWL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int wordPos = (rbValue >> 1) & 0x3;
    quint64 mask = createWordMask(wordPos, false);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTWL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int wordPos = (rbValue >> 1) & 0x3;
    result = extractBytes(raValue, wordPos * 2, 2, false);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSWL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int wordPos = (rbValue >> 1) & 0x3;
    result = insertBytes(0, raValue, wordPos * 2, 2, false);
    return true;
}

// Longword Operations
bool opcode11_executorAlphaIntegerLogical::executeMSKLL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int longPos = (rbValue >> 2) & 0x1;
    quint64 mask = createLongwordMask(longPos, false);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTLL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int longPos = (rbValue >> 2) & 0x1;
    result = extractBytes(raValue, longPos * 4, 4, false);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSLL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int longPos = (rbValue >> 2) & 0x1;
    result = insertBytes(0, raValue, longPos * 4, 4, false);
    return true;
}

// Quadword Operations
bool opcode11_executorAlphaIntegerLogical::executeMSKQL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 mask = createQuadwordMask(0, false);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTQL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    result = extractBytes(raValue, 0, 8, false);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSQL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    result = insertBytes(0, raValue, 0, 8, false);
    return true;
}

// High Variants - Byte
bool opcode11_executorAlphaIntegerLogical::executeMSKBH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int bytePos = rbValue & 0x7;
    quint64 mask = this->createByteMask(bytePos, true);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTBH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int bytePos = rbValue & 0x7;
    result = extractBytes(raValue, bytePos, 1, true);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSBH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int bytePos = rbValue & 0x7;
    result = insertBytes(0, raValue, bytePos, 1, true);
    return true;
}

// High Variants - Word
bool opcode11_executorAlphaIntegerLogical::executeMSKWH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int wordPos = (rbValue >> 1) & 0x3;
    quint64 mask = createWordMask(wordPos, true);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTWH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int wordPos = (rbValue >> 1) & 0x3;
    result = this->extractBytes(raValue, wordPos * 2, 2, true);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSWH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int wordPos = (rbValue >> 1) & 0x3;
    result = this->insertBytes(0, raValue, wordPos * 2, 2, true);
    return true;
}

// High Variants - Longword
bool opcode11_executorAlphaIntegerLogical::executeMSKLH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int longPos = (rbValue >> 2) & 0x1;
    quint64 mask = this->createLongwordMask(longPos, true);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTLH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int longPos = (rbValue >> 2) & 0x1;
    result = extractBytes(raValue, longPos * 4, 4, true);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSLH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    int longPos = (rbValue >> 2) & 0x1;
    result = insertBytes(0, raValue, longPos * 4, 4, true);
    return true;
}

// High Variants - Quadword
bool opcode11_executorAlphaIntegerLogical::executeMSKQH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 mask = createQuadwordMask(0, true);
    result = raValue & mask;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeEXTQH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    result = extractBytes(raValue, 0, 8, true);
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeINSQH(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue) || !readIntegerRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    result = insertBytes(0, raValue, 0, 8, true);
    return true;
}

// Shift Operations
bool opcode11_executorAlphaIntegerLogical::executeSLL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    int shiftCount = rbValue & 0x3F; // Only low 6 bits
    result = raValue << shiftCount;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeSRL(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue))
    {
        return false;
    }

    if (isLiteral)
    {
        rbValue = (instr.instruction.raw >> 13) & 0xFF;
    }
    else
    {
        if (!readIntegerRegisterWithCache(rb, rbValue))
        {
            return false;
        }
    }

    int shiftCount = rbValue & 0x3F; // Only low 6 bits
    result = raValue >> shiftCount;
    return true;
}

bool opcode11_executorAlphaIntegerLogical::executeSRA(const IntegerInstruction &instr, quint64 &result)
{
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    bool isLiteral = (instr.instruction.raw >> 12) & 0x1;

    quint64 raValue, rbValue;

    if (!readIntegerRegisterWithCache(ra, raValue