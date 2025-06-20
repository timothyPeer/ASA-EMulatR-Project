#include "opcode18_executorAlphaMemoryBarrier.h"
#include "globalMacro.h"
#include "utilitySafeIncrement.h"
#include "AlphaCPU_refactored.h"
#include "executorAlphaFloatingPoint.h"
#include "AlphaInstructionCache.h"
#include "opcode11_executorAlphaIntegerLogical.h"
#include "executorAlphaPAL.h"
#include "AlphaTranslationCache.h"
#include "DecodedInstruction.h"
#include "UnifiedDataCache.h"
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QThread>
#include <QtConcurrent>


// Memory Barrier Function Codes (OpCode 0x18)
static constexpr quint32 FUNC_TRAPB = 0x0000;   // Trap Barrier
static constexpr quint32 FUNC_EXCB = 0x0400;    // Exception Barrier
static constexpr quint32 FUNC_MB = 0x4000;      // Memory Barrier
static constexpr quint32 FUNC_WMB = 0x4400;     // Write Memory Barrier
static constexpr quint32 FUNC_FETCH = 0x8000;   // Fetch instruction
static constexpr quint32 FUNC_FETCH_M = 0xA000; // Fetch with intent to modify
static constexpr quint32 FUNC_RPCC = 0xC000;    // Read Process Cycle Counter
static constexpr quint32 FUNC_RC = 0xE000;      // Read Unique
static constexpr quint32 FUNC_RS = 0xF000;      // Read and Set

opcode18_executorAlphaMemoryBarrier::opcode18_executorAlphaMemoryBarrier(AlphaCPU *cpu, QObject *parent)
    : QObject(parent), m_cpu(cpu), m_fpExecutor(nullptr), m_intExecutor(nullptr), m_palExecutor(nullptr),
      m_smpTimeoutTimer(nullptr)
{
    DEBUG_LOG("executorAlphaMemoryBarrier: Initializing for OpCode 0x18 Memory Barrier instructions");

    initialize();
    initializeSignalsAndSlots();
}

opcode18_executorAlphaMemoryBarrier::~opcode18_executorAlphaMemoryBarrier()
{
    stopAsyncPipeline();

    if (m_smpTimeoutTimer)
    {
        m_smpTimeoutTimer->stop();
        delete m_smpTimeoutTimer;
    }
}

void opcode18_executorAlphaMemoryBarrier::initialize()
{
    // Initialize SMP coordination timer
    m_smpTimeoutTimer = new QTimer(this);
    m_smpTimeoutTimer->setSingleShot(true);

    // Initialize barrier frequency tracking
    m_barrierExecutionCount.clear();
    m_frequentBarriers.clear();
    m_lastBarrierTime.clear();

    // Clear all barrier states
    m_memoryBarrierPending.storeRelaxed(false);
    m_writeBarrierPending.storeRelaxed(false);
    m_trapBarrierPending.storeRelaxed(false);
    m_instructionBarrierPending.storeRelaxed(false);
    m_smpCoordinationActive.storeRelaxed(false);

    DEBUG_LOG("executorAlphaMemoryBarrier: Initialization complete");
}

void opcode18_executorAlphaMemoryBarrier::initializeSignalsAndSlots()
{
    // Connect SMP timeout timer
    connect(m_smpTimeoutTimer, &QTimer::timeout, this, &opcode18_executorAlphaMemoryBarrier::onSMPTimeout);

    DEBUG_LOG("executorAlphaMemoryBarrier: Signals and slots initialized");
}

void opcode18_executorAlphaMemoryBarrier::startAsyncPipeline()
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
        m_pendingMemoryBarriers.clear();
        m_pendingWriteBarriers.clear();
        m_pendingTrapBarriers.clear();
        m_pendingInstructionBarriers.clear();
        m_sequenceCounter.storeRelaxed(0);
    }

    // Initialize SMP coordination if needed
    initializeSMPCoordination();

    // Start worker threads
    m_fetchWorker = QtConcurrent::run([this]() { fetchWorker(); });
    m_decodeWorker = QtConcurrent::run([this]() { decodeWorker(); });
    m_executeWorker = QtConcurrent::run([this]() { executeWorker(); });
    m_writebackWorker = QtConcurrent::run([this]() { writebackWorker(); });
    m_barrierCoordinatorWorker = QtConcurrent::run([this]() { barrierCoordinatorWorker(); });
    m_smpCoordinatorWorker = QtConcurrent::run([this]() { smpCoordinatorWorker(); });

    DEBUG_LOG("executorAlphaMemoryBarrier: Async pipeline started");
}

void opcode18_executorAlphaMemoryBarrier::stopAsyncPipeline()
{
    if (!m_pipelineActive.exchange(false))
    {
        return; // Already stopped
    }

    // Wake up all workers
    m_pipelineCondition.wakeAll();
    m_barrierStateCondition.wakeAll();
    m_smpCoordinationCondition.wakeAll();

    // Stop SMP coordination
    m_smpCoordinationActive.store(false);
    if (m_smpTimeoutTimer)
    {
        m_smpTimeoutTimer->stop();
    }

    // Wait for workers to complete
    m_fetchWorker.waitForFinished();
    m_decodeWorker.waitForFinished();
    m_executeWorker.waitForFinished();
    m_writebackWorker.waitForFinished();
    m_barrierCoordinatorWorker.waitForFinished();
    m_smpCoordinatorWorker.waitForFinished();

    DEBUG_LOG("executorAlphaMemoryBarrier: Async pipeline stopped");
}

bool opcode18_executorAlphaMemoryBarrier::submitInstructionFixed(const DecodedInstruction &instruction, quint64 pc)
{
    if (!m_pipelineActive.load())
    {
        return false;
    }

    QMutexLocker locker(&m_pipelineMutex);

    if (m_fetchQueue.size() >= MAX_PIPELINE_DEPTH)
    {
        DEBUG_LOG("Memory Barrier pipeline full");
        return false;
    }

    // Fixed sequence counter usage
    quint64 seqNum = getNextSequenceNumber();
    MemoryBarrierInstruction mbInstr(instruction, pc, seqNum);
    analyzeMemoryBarrierInstruction(mbInstr);

    m_fetchQueue.enqueue(mbInstr);
    m_pipelineCondition.wakeOne();

    return true;
}
bool opcode18_executorAlphaMemoryBarrier::submitInstruction(const DecodedInstruction &instruction, quint64 pc)
{
    if (!m_pipelineActive.load())
    {
        return false;
    }

    QMutexLocker locker(&m_pipelineMutex);

    if (m_fetchQueue.size() >= MAX_PIPELINE_DEPTH)
    {
        DEBUG_LOG("Memory Barrier pipeline full");
        return false;
    }
    asa_utils::safeIncrement(m_sequenceCounter);
    quint64 seqNum = m_sequenceCounter;
    MemoryBarrierInstruction mbInstr(instruction, pc, seqNum);
    analyzeMemoryBarrierInstruction(mbInstr);

    m_fetchQueue.enqueue(mbInstr);
    m_pipelineCondition.wakeOne();

    return true;
}

bool opcode18_executorAlphaMemoryBarrier::executeMemoryBarrier(const DecodedInstruction &instruction)
{
    MemoryBarrierInstruction instr(instruction, 0, 0);
    analyzeMemoryBarrierInstruction(instr);

    // Determine barrier type and execute
    switch (instr.barrierType)
    {
    case FUNC_MB:
        return executeMB(instr);
    case FUNC_WMB:
        return executeWMB(instr);
    case FUNC_TRAPB:
        return executeTRAP B(instr);
    case FUNC_EXCB:
        return executeEXCB(instr);
    default:
        DEBUG_LOG(QString("Unknown memory barrier function: 0x%1").arg(instr.barrierType, 0, 16));
        return false;
    }
}

bool opcode18_executorAlphaMemoryBarrier::executeFETCH(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing FETCH at PC: 0x%1").arg(instr.pc, 0, 16));

    // Extract register fields
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 address = 0;
    if (!readIntegerRegisterWithCache(rb, address))
    {
        return false;
    }

    // Perform cache line fetch
    const quint64 cacheLineSize = 64;
    quint64 cacheLineAddr = address & ~(cacheLineSize - 1);

    // Prefetch the cache line
    if (m_level1DataCache)
    {
        m_level1DataCache->prefetch(cacheLineAddr, cacheLineSize);
    }

    if (m_level2Cache)
    {
        m_level2Cache->prefetch(cacheLineAddr, cacheLineSize);
    }

    // Return the cache line address in Ra
    const_cast<MemoryBarrierInstruction &>(instr).result = cacheLineAddr;
    const_cast<MemoryBarrierInstruction &>(instr).writeResult = true;
    const_cast<MemoryBarrierInstruction &>(instr).targetRegister = ra;

    return true;
}

bool opcode18_executorAlphaMemoryBarrier::executeFETCH_M(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing FETCH_M at PC: 0x%1").arg(instr.pc, 0, 16));

    // Extract register fields
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 address = 0;
    if (!readIntegerRegisterWithCache(rb, address))
    {
        return false;
    }

    // Perform cache line fetch with intent to modify
    const quint64 cacheLineSize = 64;
    quint64 cacheLineAddr = address & ~(cacheLineSize - 1);

    // Prefetch with exclusive access hint
    if (m_level1DataCache)
    {
        // Request exclusive ownership for modification
        m_level1DataCache->prefetchExclusive(cacheLineAddr, cacheLineSize);
    }

    if (m_level2Cache)
    {
        m_level2Cache->prefetchExclusive(cacheLineAddr, cacheLineSize);
    }

    // For SMP systems, request exclusive cache line ownership
    if (m_cpu)
    {
        m_cpu->requestExclusiveCacheLine(cacheLineAddr);
    }

    // Return the cache line address in Ra
    const_cast<MemoryBarrierInstruction &>(instr).result = cacheLineAddr;
    const_cast<MemoryBarrierInstruction &>(instr).writeResult = true;
    const_cast<MemoryBarrierInstruction &>(instr).targetRegister = ra;

    return true;
}

void opcode18_executorAlphaMemoryBarrier::fetchWorker()
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
            MemoryBarrierInstruction instr = m_fetchQueue.dequeue();

            // Fetch instruction from cache
            quint32 instruction;
            if (fetchInstructionWithCache(instr.pc, instruction))
            {
                instr.isReady = true;
                instr.startTime = QDateTime::currentMSecsSinceEpoch();
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

void opcode18_executorAlphaMemoryBarrier::decodeWorker()
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
            MemoryBarrierInstruction instr = m_decodeQueue.dequeue();

            // Analyze dependencies and barrier requirements
            analyzeDependencies(instr);
            instr.isReady = true;

            m_executeQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}



void opcode18_executorAlphaMemoryBarrier::barrierCoordinatorWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_barrierStateMutex);

        // Wait for barrier coordination work
        m_barrierStateCondition.wait(&m_barrierStateMutex, 100);

        if (!m_pipelineActive.load())
            break;

        // Process pending barriers
        bool workDone = false;

        // Check for completed memory operations
        if (m_memoryBarrierPending.load())
        {
            if (!m_pendingMemoryBarriers.isEmpty())
            {
                // Check if all pending memory operations have completed
                if (waitForPendingMemoryOperations(100)) // 100ms timeout
                {
                    // Complete all pending memory barriers
                    while (!m_pendingMemoryBarriers.isEmpty())
                    {
                        MemoryBarrierInstruction instr = m_pendingMemoryBarriers.dequeue();
                        instr.isCompleted = true;
                        instr.completionTime = QDateTime::currentMSecsSinceEpoch();

                        // Add back to writeback queue
                        QMutexLocker pipelineLocker(&m_pipelineMutex);
                        m_writebackQueue.enqueue(instr);
                        m_pipelineCondition.wakeOne();
                    }

                    m_memoryBarrierPending.store(false);
                    workDone = true;
                }
            }
        }

        // Similar processing for write barriers
        if (m_writeBarrierPending.load())
        {
            if (!m_pendingWriteBarriers.isEmpty())
            {
                if (waitForPendingWriteOperations(100))
                {
                    while (!m_pendingWriteBarriers.isEmpty())
                    {
                        MemoryBarrierInstruction instr = m_pendingWriteBarriers.dequeue();
                        instr.isCompleted = true;
                        instr.completionTime = QDateTime::currentMSecsSinceEpoch();

                        QMutexLocker pipelineLocker(&m_pipelineMutex);
                        m_writebackQueue.enqueue(instr);
                        m_pipelineCondition.wakeOne();
                    }

                    m_writeBarrierPending.store(false);
                    workDone = true;
                }
            }
        }

        // Process trap barriers
        if (m_trapBarrierPending.load())
        {
            if (!m_pendingTrapBarriers.isEmpty())
            {
                if (waitForPendingTrapOperations(100))
                {
                    while (!m_pendingTrapBarriers.isEmpty())
                    {
                        MemoryBarrierInstruction instr = m_pendingTrapBarriers.dequeue();
                        instr.isCompleted = true;
                        instr.completionTime = QDateTime::currentMSecsSinceEpoch();

                        QMutexLocker pipelineLocker(&m_pipelineMutex);
                        m_writebackQueue.enqueue(instr);
                        m_pipelineCondition.wakeOne();
                    }

                    m_trapBarrierPending.store(false);
                    workDone = true;
                }
            }
        }

        // Process instruction barriers
        if (m_instructionBarrierPending.load())
        {
            if (!m_pendingInstructionBarriers.isEmpty())
            {
                if (flushInstructionPipeline())
                {
                    while (!m_pendingInstructionBarriers.isEmpty())
                    {
                        MemoryBarrierInstruction instr = m_pendingInstructionBarriers.dequeue();
                        instr.isCompleted = true;
                        instr.completionTime = QDateTime::currentMSecsSinceEpoch();

                        QMutexLocker pipelineLocker(&m_pipelineMutex);
                        m_writebackQueue.enqueue(instr);
                        m_pipelineCondition.wakeOne();
                    }

                    m_instructionBarrierPending.store(false);
                    workDone = true;
                }
            }
        }

        if (workDone)
        {
            DEBUG_LOG("MemoryBarrierExecutor: Barrier coordination completed");
        }
    }
}

void opcode18_executorAlphaMemoryBarrier::smpCoordinatorWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_smpCoordinationMutex);

        // Wait for SMP coordination work
        m_smpCoordinationCondition.wait(&m_smpCoordinationMutex, 200);

        if (!m_pipelineActive.load())
            break;

        // Process incoming SMP messages
        processIncomingSMPMessages();

        // Check for SMP coordination timeouts
        // This would be handled by the timeout timer
    }
}

void opcode18_executorAlphaMemoryBarrier::analyzeMemoryBarrierInstruction(MemoryBarrierInstruction &instr)
{
    quint32 raw = instr.instruction.raw;

    // Extract function code from bits [15:0]
    instr.barrierType = raw & 0xFFFF;

    // Determine barrier requirements
    switch (instr.barrierType)
    {
    case FUNC_MB:
        instr.requiresMemoryBarrier = true;
        instr.requiresSMPCoordination = true;
        break;
    case FUNC_WMB:
        instr.requiresWriteBarrier = true;
        instr.requiresSMPCoordination = true;
        break;
    case FUNC_TRAPB:
        instr.requiresTrapBarrier = true;
        break;
    case FUNC_EXCB:
        instr.requiresTrapBarrier = true;
        break;
    case FUNC_FETCH:
    case FUNC_FETCH_M:
        instr.touchesMemory = true;
        break;
    case FUNC_RPCC:
    case FUNC_RC:
    case FUNC_RS:
        // These are register operations, not barriers
        instr.writeResult = true;
        instr.targetRegister = (raw >> 21) & 0x1F; // Ra field
        break;
    default:
        DEBUG_LOG(QString("Unknown barrier type: 0x%1").arg(instr.barrierType, 0, 16));
        break;
    }
}

void opcode18_executorAlphaMemoryBarrier::analyzeDependencies(MemoryBarrierInstruction &instr)
{
    quint32 raw = instr.instruction.raw;

    // Clear existing dependencies
    instr.srcRegisters.clear();
    instr.dstRegisters.clear();

    // Extract register fields
    quint8 ra = (raw >> 21) & 0x1F;
    quint8 rb = (raw >> 16) & 0x1F;

    // Determine register usage based on barrier type
    switch (instr.barrierType)
    {
    case FUNC_FETCH:
    case FUNC_FETCH_M:
        // FETCH uses Rb as source (address), Ra as destination
        if (rb != 31)
            instr.srcRegisters.insert(rb);
        if (ra != 31)
            instr.dstRegisters.insert(ra);
        break;

    case FUNC_RPCC:
    case FUNC_RC:
    case FUNC_RS:
        // These write to Ra
        if (ra != 31)
            instr.dstRegisters.insert(ra);
        break;

    default:
        // Most barriers don't use registers
        break;
    }
}

bool opcode18_executorAlphaMemoryBarrier::checkDependencies(const MemoryBarrierInstruction &instr) const
{
    // Memory barriers typically need to wait for all preceding operations
    // to complete, so dependency checking is minimal

    // Check if this barrier conflicts with pending operations
    switch (instr.barrierType)
    {
    case FUNC_MB:
        // Memory barrier must wait for all memory operations
        return !isAnyBarrierPending();

    case FUNC_WMB:
        // Write memory barrier must wait for write operations
        return !m_writeBarrierPending.load();

    case FUNC_TRAPB:
        // Trap barrier must wait for trap operations
        return !m_trapBarrierPending.load();

    default:
        return true; // Most instructions can proceed
    }
}

void opcode18_executorAlphaMemoryBarrier::updateDependencies(const MemoryBarrierInstruction &instr)
{
    // Update barrier state based on completed instruction
    switch (instr.barrierType)
    {
    case FUNC_MB:
        if (instr.isCompleted)
        {
            m_memoryBarrierPending.store(false);
        }
        break;

    case FUNC_WMB:
        if (instr.isCompleted)
        {
            m_writeBarrierPending.store(false);
        }
        break;

    case FUNC_TRAPB:
        if (instr.isCompleted)
        {
            m_trapBarrierPending.store(false);
        }
        break;

    default:
        break;
    }
}


bool opcode18_executorAlphaMemoryBarrier::executeIMB(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing Instruction Memory Barrier at PC: 0x%1").arg(instr.pc, 0, 16));

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_instructionBarriers);

    // Set instruction barrier as pending
    m_instructionBarrierPending.store(true);

    // Add to pending queue
    {
        QMutexLocker barrierLocker(&m_barrierStateMutex);
        m_pendingInstructionBarriers.enqueue(const_cast<MemoryBarrierInstruction &>(instr));
        m_barrierStateCondition.wakeOne();
    }

    // Flush instruction cache and pipeline
    invalidateInstructionCache();

    // Coordinate with execution units to flush instruction pipelines
    coordinateWithExecutor("FloatingPoint", "InstructionBarrier");
    coordinateWithExecutor("Integer", "InstructionBarrier");
    coordinateWithExecutor("PAL", "InstructionBarrier");

    // Flush instruction pipeline
    bool success = flushInstructionPipeline();

    // For SMP systems, coordinate instruction cache coherency
    if (instr.requiresSMPCoordination && m_cpu)
    {
        broadcastBarrierToAllCPUs(FUNC_FETCH, instr.sequenceNumber);
        success &= waitForSMPBarrierCompletion(FUNC_FETCH);
    }

    return success;
}

bool opcode18_executorAlphaMemoryBarrier::executeMB(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing Memory Barrier at PC: 0x%1").arg(instr.pc, 0, 16));

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_memoryBarriers);

    // Set memory barrier as pending
    m_memoryBarrierPending.store(true);

    // Add to pending queue for coordination
    {
        QMutexLocker barrierLocker(&m_barrierStateMutex);
        m_pendingMemoryBarriers.enqueue(const_cast<MemoryBarrierInstruction &>(instr));
        m_barrierStateCondition.wakeOne();
    }

    // Coordinate with other execution units
    coordinateWithExecutor("FloatingPoint", "MemoryBarrier");
    coordinateWithExecutor("Integer", "MemoryBarrier");
    coordinateWithExecutor("PAL", "MemoryBarrier");

    // Flush memory subsystem
    drainWriteBuffers();

    // For SMP systems, coordinate with other CPUs
    if (instr.requiresSMPCoordination && m_cpu)
    {
        broadcastBarrierToAllCPUs(FUNC_MB, instr.sequenceNumber);
        return waitForSMPBarrierCompletion(FUNC_MB);
    }

    return true;
}

bool opcode18_executorAlphaMemoryBarrier::executeRC(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing RC at PC: 0x%1").arg(instr.pc, 0, 16));

    // Extract register field
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;

    // Read and Clear - atomic read of unique value and clear it
    quint64 uniqueValue = 0;
    if (m_cpu)
    {
        uniqueValue = m_cpu->readAndClearUniqueValue();
    }

    // Return unique value in Ra
    const_cast<MemoryBarrierInstruction &>(instr).result = uniqueValue;
    const_cast<MemoryBarrierInstruction &>(instr).writeResult = true;
    const_cast<MemoryBarrierInstruction &>(instr).targetRegister = ra;

    return true;
}
bool opcode18_executorAlphaMemoryBarrier::executeRPCC(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing RPCC at PC: 0x%1").arg(instr.pc, 0, 16));

    // Extract register field
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;

    // Read Process Cycle Counter
    quint64 cycleCount = 0;
    if (m_cpu)
    {
        cycleCount = m_cpu->getProcessCycleCounter();
    }
    else
    {
        // Fallback to system time-based counter
        cycleCount = QDateTime::currentMSecsSinceEpoch() * 1000000; // Convert to microseconds
    }

    // Return cycle count in Ra
    const_cast<MemoryBarrierInstruction &>(instr).result = cycleCount;
    const_cast<MemoryBarrierInstruction &>(instr).writeResult = true;
    const_cast<MemoryBarrierInstruction &>(instr).targetRegister = ra;

    return true;
}

bool opcode18_executorAlphaMemoryBarrier::executeRS(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing RS at PC: 0x%1").arg(instr.pc, 0, 16));

    // Extract register field
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;

    // Read and Set - atomic read of unique value and set it to 1
    quint64 uniqueValue = 0;
    if (m_cpu)
    {
        uniqueValue = m_cpu->readAndSetUniqueValue();
    }

    // Return previous unique value in Ra
    const_cast<MemoryBarrierInstruction &>(instr).result = uniqueValue;
    const_cast<MemoryBarrierInstruction &>(instr).writeResult = true;
    const_cast<MemoryBarrierInstruction &>(instr).targetRegister = ra;

    return true;
}
bool opcode18_executorAlphaMemoryBarrier::executeWMB(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Executing Write Memory Barrier at PC: 0x%1").arg(instr.pc, 0, 16));

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_writeBarriers);

    // Set write barrier as pending
    m_writeBarrierPending.store(true);

    // Add to pending queue
    {
        QMutexLocker barrierLocker(&m_barrierStateMutex);
        m_pendingWriteBarriers.enqueue(const_cast<MemoryBarrierInstruction &>(instr));
        m_barrierStateCondition.wakeOne();
    }

    // Coordinate with execution units for write operations
    coordinateWithExecutor("FloatingPoint", "WriteBarrier");
    coordinateWithExecutor("Integer", "WriteBarrier");

    // Drain write buffers
    drainWriteBuffers();

    // SMP coordination for write ordering
    if (instr.requiresSMPCoordination && m_cpu)
    {
        broadcastBarrierToAllCPUs(FUNC_WMB, instr.sequenceNumber);
        return waitForSMPBarrierCompletion(FUNC_WMB);
    }

    return true;
}

// bool executorAlphaMemoryBarrier::executeTRAP B(const MemoryBarrierInstruction &instr)
// {
//     DEBUG_LOG(QString("Executing Trap Barrier at PC: 0x%1").arg(instr.pc, 0, 16));
// 
//     QMutexLocker locker(&m_statsMutex);
//     asa_utils::safeIncrement(m_trapBarriers);
// 
//     // Set trap barrier as pending
//     m_trapBarrierPending.storeRelaxed(true);
// 
//     // Add to pending queue
//     {
//         QMutexLocker barrierLocker(&m_barrierStateMutex);
//         m_pendingTrapBarriers.enqueue(const_cast<MemoryBarrierInstruction &>(instr));
//         m_barrierStateCondition.wakeOne();
//     }
// 
//     // Coordinate with execution units to ensure no pending traps
//     coordinateWithExecutor("FloatingPoint", "TrapBarrier");
//     coordinateWithExecutor("Integer", "TrapBarrier");
//     coordinateWithExecutor("PAL", "TrapBarrier");
// 
//     // Wait for any pending exceptions to be resolved
//     return waitForPendingTrapOperations();
// }

// bool executorAlphaMemoryBarrier::executeEXCB(const MemoryBarrierInstruction &instr)
// {
//     DEBUG_LOG(QString("Executing Exception Barrier at PC: 0x%1").arg(instr.pc, 0, 16));
// 
//     // Exception barrier is similar to trap barrier but more comprehensive
//     return executeTRAP B(instr);
// }

bool opcode18_executorAlphaMemoryBarrier::fetchInstructionWithCache(quint64 pc, quint32 &instruction)
{
    QMutexLocker locker(&m_statsMutex);

    // Try instruction cache first
    if (m_instructionCache)
    {
        InstructionWord instrWord;
        if (m_instructionCache->fetch(pc, instrWord))
        {
            instruction = instrWord.getRawInstruction();
            asa_utils::safeIncrement(m_l1ICacheHits);
            updateCacheStatistics("L1I", true);
            return true;
        }
        else
        {
            asa_utils::safeIncrement(m_l1ICacheMisses);
            updateCacheStatistics("L1I", false);
        }
    }

    // Try L2 cache
    if (m_level2Cache)
    {
        if (m_level2Cache->read(pc, reinterpret_cast<quint8 *>(&instruction), 4))
        {
            asa_utils::safeIncrement(m_l2CacheHits);
            updateCacheStatistics("L2", true);

            // Fill L1 instruction cache
            if (m_instructionCache)
            {
                m_instructionCache->prefetch(pc);
            }

            return true;
        }
        else
        {
            asa_utils::safeIncrement(m_l2CacheMisses);
            updateCacheStatistics("L2", false);
        }
    }

    // Fallback to CPU memory access
    return m_cpu ? m_cpu->readMemory(pc, reinterpret_cast<quint8 *>(&instruction), 4) : false;
}

bool opcode18_executorAlphaMemoryBarrier::readIntegerRegisterWithCache(quint8 reg, quint64 &value)
{
    if (!m_cpu)
        return false;

    value = m_cpu->getIntegerRegister(reg);

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_l1DCacheHits); // Register access is always a hit
    updateCacheStatistics("L1D", true);

    return true;
}

bool opcode18_executorAlphaMemoryBarrier::writeIntegerRegisterWithCache(quint8 reg, quint64 value)
{
    if (!m_cpu)
        return false;

    m_cpu->setIntegerRegister(reg, value);

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_l1DCacheHits); // Register access is always a hit
    updateCacheStatistics("L1D", true);

    return true;
}

bool opcode18_executorAlphaMemoryBarrier::isAnyBarrierPending() const
{
    return m_memoryBarrierPending.load() || m_writeBarrierPending.load() || m_trapBarrierPending.load() ||
           m_instructionBarrierPending.load();
}

void opcode18_executorAlphaMemoryBarrier::initializeSMPCoordination()
{
    QMutexLocker locker(&m_smpCoordinationMutex);

    m_smpMessageQueue.clear();
    m_smpBarrierStates.clear();
    m_smpCoordinationActive.store(true);

    DEBUG_LOG("SMP coordination initialized");
}

void opcode18_executorAlphaMemoryBarrier::sendSMPBarrierMessage(quint16 targetCpu, quint32 barrierType, quint64 sequenceNumber)
{
    SMPBarrierMessage message;
    message.sourceCpuId = m_cpu ? m_cpu->getCpuId() : 0;
    message.targetCpuId = targetCpu;
    message.barrierType = barrierType;
    message.sequenceNumber = sequenceNumber;
    message.state = BarrierState::PENDING;
    message.timestamp = QDateTime::currentMSecsSinceEpoch();

    // Send message through CPU's SMP interface
    if (m_cpu)
    {
        m_cpu->sendSMPMessage(targetCpu, reinterpret_cast<const quint8 *>(&message), sizeof(message));
    }

    DEBUG_LOG(QString("Sent SMP barrier message to CPU %1, type 0x%2").arg(targetCpu).arg(barrierType, 0, 16));
}

void opcode18_executorAlphaMemoryBarrier::receiveSMPBarrierMessage(const SMPBarrierMessage &message)
{
    QMutexLocker locker(&m_smpCoordinationMutex);

    m_smpMessageQueue.enqueue(message);
    m_smpCoordinationCondition.wakeOne();

    DEBUG_LOG(QString("Received SMP barrier message from CPU %1, type 0x%2")
                  .arg(message.sourceCpuId)
                  .arg(message.barrierType, 0, 16));
}

bool opcode18_executorAlphaMemoryBarrier::waitForSMPBarrierCompletion(quint32 barrierType, quint64 timeoutMs)
{
    QMutexLocker locker(&m_smpCoordinationMutex);

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    qint64 deadline = startTime + timeoutMs;

    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        bool allCompleted = true;

        // Check if all CPUs have completed the barrier
        for (auto it = m_smpBarrierStates.begin(); it != m_smpBarrierStates.end(); ++it)
        {
            if (it.value() != BarrierState::COMPLETED)
            {
                allCompleted = false;
                break;
            }
        }

        if (allCompleted)
        {
            DEBUG_LOG("SMP barrier coordination completed successfully");
            return true;
        }

        // Wait with timeout
        m_smpCoordinationCondition.wait(&m_smpCoordinationMutex, 100);
    }

    DEBUG_LOG("SMP barrier coordination timed out");
    asa_utils::safeIncrement(m_barrierTimeouts);
    return false;
}


quint64 opcode18_executorAlphaMemoryBarrier::getNextSequenceNumber()
{
    // Use fetch_add for atomic increment and return
    return m_sequenceCounter.fetchAndAddOrdered(1);
}
void opcode18_executorAlphaMemoryBarrier::notifyMemoryOperation(bool isWrite)
{
    if (isWrite)
    {
        // Notify that a write operation is starting
        QMutexLocker locker(&m_barrierStateMutex);
        // Could track pending write operations here
    }
    else
    {
        // Notify that a read operation is starting
        QMutexLocker locker(&m_barrierStateMutex);
        // Could track pending read operations here
    }
}

void opcode18_executorAlphaMemoryBarrier::notifyMemoryOperationComplete(bool isWrite)
{
    QMutexLocker locker(&m_barrierStateMutex);

    // Wake up barrier coordinator to check for completion
    m_barrierStateCondition.wakeOne();

    DEBUG_LOG(QString("Memory operation completed: %1").arg(isWrite ? "Write" : "Read"));
}

void opcode18_executorAlphaMemoryBarrier::notifyTrapOperation()
{
    QMutexLocker locker(&m_barrierStateMutex);
    // Track pending trap operations
}

void opcode18_executorAlphaMemoryBarrier::notifyTrapOperationComplete()
{
    QMutexLocker locker(&m_barrierStateMutex);

    // Wake up barrier coordinator
    m_barrierStateCondition.wakeOne();

    DEBUG_LOG("Trap operation completed");
}

void opcode18_executorAlphaMemoryBarrier::coordinateWithExecutor(const QString &executorName, const QString &operation)
{
    DEBUG_LOG(QString("Coordinating %1 with %2 executor").arg(operation, executorName));

    // Coordinate with specific execution units
    if (executorName == "FloatingPoint" && m_fpExecutor)
    {
        // Could call specific coordination methods on FP executor
        // For now, just log the coordination
    }
    else if (executorName == "Integer" && m_intExecutor)
    {
        // Coordinate with integer executor
    }
    else if (executorName == "PAL" && m_palExecutor)
    {
        // Coordinate with PAL executor
    }
}

bool opcode18_executorAlphaMemoryBarrier::waitForPendingMemoryOperations(quint32 timeoutMs)
{
    // Check with other execution units for pending memory operations
    bool fpComplete = true;
    bool intComplete = true;
    bool palComplete = true;

    if (m_fpExecutor)
    {
        // fpComplete = m_fpExecutor->areMemoryOperationsComplete();
    }

    if (m_intExecutor)
    {
        // intComplete = m_intExecutor->areMemoryOperationsComplete();
    }

    if (m_palExecutor)
    {
        // palComplete = m_palExecutor->areMemoryOperationsComplete();
    }

    // For now, simulate completion check
    QThread::msleep(timeoutMs / 10); // Simulate some wait time

    return fpComplete && intComplete && palComplete;
}

bool opcode18_executorAlphaMemoryBarrier::waitForPendingWriteOperations(quint32 timeoutMs)
{
    // Similar to memory operations but specifically for writes
    return waitForPendingMemoryOperations(timeoutMs);
}

bool opcode18_executorAlphaMemoryBarrier::waitForPendingTrapOperations(quint32 timeoutMs)
{
    // Check for pending exceptions/traps in execution units
    bool allClear = true;

    if (m_fpExecutor)
    {
        // allClear &= m_fpExecutor->areTrapsComplete();
    }

    if (m_intExecutor)
    {
        // allClear &= m_intExecutor->areTrapsComplete();
    }

    if (m_palExecutor)
    {
        // allClear &= m_palExecutor->areTrapsComplete();
    }

    QThread::msleep(timeoutMs / 20); // Simulate wait

    return allClear;
}

bool opcode18_executorAlphaMemoryBarrier::flushInstructionPipeline()
{
    DEBUG_LOG("Flushing instruction pipeline");

    // Flush instruction cache
    invalidateInstructionCache();

    // Coordinate with execution units to flush their pipelines
    if (m_fpExecutor)
    {
        // m_fpExecutor->flushPipeline();
    }

    if (m_intExecutor)
    {
        // m_intExecutor->flushPipeline();
    }

    if (m_palExecutor)
    {
        // m_palExecutor->flushPipeline();
    }

    return true;
}

void opcode18_executorAlphaMemoryBarrier::flushL1Cache(bool instructionCache, bool dataCache)
{
    if (instructionCache && m_instructionCache)
    {
        m_instructionCache->flush();
        DEBUG_LOG("L1 instruction cache flushed");
    }

    if (dataCache && m_level1DataCache)
    {
        m_level1DataCache->flush();
        DEBUG_LOG("L1 data cache flushed");
    }

    QMutexLocker locker(&m_statsMutex);
    // Could track cache flush statistics
}

void opcode18_executorAlphaMemoryBarrier::flushL2Cache()
{
    if (m_level2Cache)
    {
        m_level2Cache->flush();
        DEBUG_LOG("L2 cache flushed");
    }
}

void opcode18_executorAlphaMemoryBarrier::flushL3Cache()
{
    if (m_level3Cache)
    {
        m_level3Cache->flush();
        DEBUG_LOG("L3 cache flushed");
    }
}

void opcode18_executorAlphaMemoryBarrier::invalidateInstructionCache()
{
    if (m_instructionCache)
    {
        m_instructionCache->invalidateAll();
        DEBUG_LOG("Instruction cache invalidated");
    }
}

void opcode18_executorAlphaMemoryBarrier::drainWriteBuffers()
{
    // Ensure all pending writes are completed
    flushL1Cache(false, true); // Flush data cache only
    flushL2Cache();
    flushL3Cache();

    // Could also coordinate with memory controller
    if (m_cpu)
    {
        // m_cpu->drainWriteBuffers();
    }

    DEBUG_LOG("Write buffers drained");
}

void opcode18_executorAlphaMemoryBarrier::updateJITStats(quint64 pc, quint32 barrierType)
{
    m_barrierExecutionCount[pc]++;

    // Mark as frequent if executed > 100 times
    if (m_barrierExecutionCount[pc] > 100)
    {
        m_frequentBarriers.insert(pc);
    }

    m_lastBarrierTime[pc] = QDateTime::currentMSecsSinceEpoch();
}

bool opcode18_executorAlphaMemoryBarrier::canEliminateBarrier(quint64 pc, quint32 barrierType)
{
    // Simple barrier elimination heuristics

    // Don't eliminate if this is the first execution
    if (m_barrierExecutionCount[pc] < 2)
    {
        return false;
    }

    // Check if recent barrier of same type at nearby address
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 recentThreshold = 100; // 100ms

    for (auto it = m_lastBarrierTime.begin(); it != m_lastBarrierTime.end(); ++it)
    {
        quint64 otherPC = it.key();
        qint64 lastTime = it.value();

        // If nearby PC had recent barrier of same type, might be redundant
        if (qAbs(static_cast<qint64>(pc - otherPC)) < 64 && // Within 64 bytes
            (currentTime - lastTime) < recentThreshold)
        {
            return true; // Eliminate as redundant
        }
    }

    return false;
}

void opcode18_executorAlphaMemoryBarrier::trackBarrierFrequency(quint64 pc)
{
    m_barrierExecutionCount[pc]++;

    if (m_barrierExecutionCount[pc] > 100)
    {
        m_frequentBarriers.insert(pc);
    }
}

bool opcode18_executorAlphaMemoryBarrier::isRedundantBarrier(const MemoryBarrierInstruction &instr)
{
    // Check if this barrier is redundant based on recent execution history
    return canEliminateBarrier(instr.pc, instr.barrierType);
}

void opcode18_executorAlphaMemoryBarrier::broadcastBarrierToAllCPUs(quint32 barrierType, quint64 sequenceNumber)
{
    if (!m_cpu)
        return;

    quint16 cpuCount = m_cpu->getSMPCpuCount();
    quint16 thisCpuId = m_cpu->getCpuId();

    for (quint16 cpuId = 0; cpuId < cpuCount; ++cpuId)
    {
        if (cpuId != thisCpuId)
        {
            sendSMPBarrierMessage(cpuId, barrierType, sequenceNumber);

            // Initialize barrier state for this CPU
            QMutexLocker locker(&m_smpCoordinationMutex);
            m_smpBarrierStates[cpuId] = BarrierState::PENDING;
        }
    }

    // Start timeout timer
    if (m_smpTimeoutTimer)
    {
        m_smpTimeoutTimer->start(MAX_SMP_WAIT_TIME);
    }

    asa_utils::safeIncrement(m_smpCoordinations);
}

bool opcode18_executorAlphaMemoryBarrier::waitForAllCPUAcknowledgments(quint32 barrierType, quint64 timeoutMs)
{
    return waitForSMPBarrierCompletion(barrierType, timeoutMs);
}

void opcode18_executorAlphaMemoryBarrier::processIncomingSMPMessages()
{
    while (!m_smpMessageQueue.isEmpty())
    {
        SMPBarrierMessage message = m_smpMessageQueue.dequeue();

        DEBUG_LOG(QString("Processing SMP message from CPU %1, type 0x%2")
                      .arg(message.sourceCpuId)
                      .arg(message.barrierType, 0, 16));

        // Update barrier state for source CPU
        updateSMPBarrierState(message.sourceCpuId, message.state);

        // If this is a barrier request, acknowledge it
        if (message.state == BarrierState::PENDING)
        {
            // Send acknowledgment back
            SMPBarrierMessage ack;
            ack.sourceCpuId = m_cpu ? m_cpu->getCpuId() : 0;
            ack.targetCpuId = message.sourceCpuId;
            ack.barrierType = message.barrierType;
            ack.sequenceNumber = message.sequenceNumber;
            ack.state = BarrierState::COMPLETED;
            ack.timestamp = QDateTime::currentMSecsSinceEpoch();

            if (m_cpu)
            {
                m_cpu->sendSMPMessage(message.sourceCpuId, reinterpret_cast<const quint8 *>(&ack), sizeof(ack));
            }
        }
    }
}

void opcode18_executorAlphaMemoryBarrier::updateSMPBarrierState(quint16 cpuId, BarrierState state)
{
    m_smpBarrierStates[cpuId] = state;

    // Wake up any threads waiting for SMP coordination
    m_smpCoordinationCondition.wakeAll();

    DEBUG_LOG(QString("Updated SMP barrier state for CPU %1: %2").arg(cpuId).arg(static_cast<int>(state)));
}

void opcode18_executorAlphaMemoryBarrier::updateCacheStatistics(const QString &level, bool hit)
{
    // Cache statistics are updated in the calling methods
    // This could emit signals or update other tracking
    emit sigBarrierInstructionExecuted(0, hit, 1);
}

quint32 opcode18_executorAlphaMemoryBarrier::measureBarrierCycles(const MemoryBarrierInstruction &instr)
{
    if (instr.startTime > 0 && instr.completionTime > 0)
    {
        qint64 elapsedMs = instr.completionTime - instr.startTime;
        // Convert to CPU cycles (assuming 1GHz CPU for example)
        return static_cast<quint32>(elapsedMs * 1000000); // 1M cycles per ms
    }

    return 10; // Default estimate
}

void opcode18_executorAlphaMemoryBarrier::recordBarrierCompletion(const MemoryBarrierInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_barrierInstructions);

    // Track barrier-specific statistics
    switch (instr.barrierType)
    {
    case FUNC_MB:
        // Already incremented in executeMB
        break;
    case FUNC_WMB:
        // Already incremented in executeWMB
        break;
    case FUNC_TRAPB:
        // Already incremented in executeTRAP B
        break;
    default:
        break;
    }

    // Update JIT optimization tracking
    trackBarrierFrequency(instr.pc);
}

void opcode18_executorAlphaMemoryBarrier::handleBarrierTimeout(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Barrier timeout at PC: 0x%1, type: 0x%2").arg(instr.pc, 0, 16).arg(instr.barrierType, 0, 16));

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_barrierTimeouts);

    emit sigBarrierTimeout(instr.pc, instr.barrierType);

    // Could implement recovery strategies here
    recoverFromBarrierError(instr);
}

void opcode18_executorAlphaMemoryBarrier::handleSMPCoordinationFailure(quint16 cpuId, quint32 barrierType)
{
    DEBUG_LOG(QString("SMP coordination failure with CPU %1, barrier type 0x%2").arg(cpuId).arg(barrierType, 0, 16));

    // Mark CPU as failed for this barrier
    updateSMPBarrierState(cpuId, BarrierState::ERROR);

    // Could implement retry logic or failover
}

void opcode18_executorAlphaMemoryBarrier::recoverFromBarrierError(const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Attempting barrier error recovery for PC: 0x%1").arg(instr.pc, 0, 16));

    // Recovery strategies:
    // 1. Reset barrier state
    switch (instr.barrierType)
    {
    case FUNC_MB:
        m_memoryBarrierPending.store(false);
        break;
    case FUNC_WMB:
        m_writeBarrierPending.store(false);
        break;
    case FUNC_TRAPB:
        m_trapBarrierPending.store(false);
        break;
    default:
        break;
    }

    // 2. Clear pending queues
    QMutexLocker locker(&m_barrierStateMutex);
    if (instr.barrierType == FUNC_MB)
    {
        m_pendingMemoryBarriers.clear();
    }
    else if (instr.barrierType == FUNC_WMB)
    {
        m_pendingWriteBarriers.clear();
    }
    else if (instr.barrierType == FUNC_TRAPB)
    {
        m_pendingTrapBarriers.clear();
    }

    // 3. Wake up any waiting threads
    m_barrierStateCondition.wakeAll();

    DEBUG_LOG("Barrier error recovery completed");
}

void opcode18_executorAlphaMemoryBarrier::onSMPTimeout()
{
    DEBUG_LOG("SMP coordination timeout occurred");

    QMutexLocker locker(&m_smpCoordinationMutex);

    // Mark all pending CPUs as timed out
    for (auto it = m_smpBarrierStates.begin(); it != m_smpBarrierStates.end(); ++it)
    {
        if (it.value() == BarrierState::PENDING)
        {
            it.value() = BarrierState::TIMEOUT;
            handleSMPCoordinationFailure(it.key(), 0); // Generic timeout
        }
    }

    // Wake up waiting threads
    m_smpCoordinationCondition.wakeAll();

    asa_utils::safeIncrement(m_barrierTimeouts);
}

void opcode18_executorAlphaMemoryBarrier::onBarrierCoordinationTimeout()
{
    DEBUG_LOG("Barrier coordination timeout occurred");

    // This could be connected to another timer for barrier-specific timeouts
    // For now, just log the event
}

bool opcode18_executorAlphaMemoryBarrier::performMemoryRead(quint64 address, const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Performing memory read at address: 0x%1").arg(address, 0, 16));

    // Try L1 data cache first
    quint64 value = 0;
    if (m_level1DataCache)
    {
        if (m_level1DataCache->read(address, reinterpret_cast<quint8 *>(&value), 8))
        {
            asa_utils::safeIncrement(m_l1DCacheHits);
            updateCacheStatistics("L1D", true);
            return true;
        }
        else
        {
            asa_utils::safeIncrement(m_l1DCacheMisses);
            updateCacheStatistics("L1D", false);
        }
    }

    // Try L2 cache
    if (m_level2Cache)
    {
        if (m_level2Cache->read(address, reinterpret_cast<quint8 *>(&value), 8))
        {
            asa_utils::safeIncrement(m_l2CacheHits);
            updateCacheStatistics("L2", true);

            // Fill L1 cache
            if (m_level1DataCache)
            {
                m_level1DataCache->write(address, reinterpret_cast<quint8 *>(&value), 8);
            }

            return true;
        }
        else
        {
            asa_utils::safeIncrement(m_l2CacheMisses);
            updateCacheStatistics("L2", false);
        }
    }

    // Try L3 cache
    if (m_level3Cache)
    {
        if (m_level3Cache->read(address, reinterpret_cast<quint8 *>(&value), 8))
        {
            asa_utils::safeIncrement(m_l3CacheHits);
            updateCacheStatistics("L3", true);

            // Fill upper levels
            if (m_level2Cache)
            {
                m_level2Cache->write(address, reinterpret_cast<quint8 *>(&value), 8);
            }
            if (m_level1DataCache)
            {
                m_level1DataCache->write(address, reinterpret_cast<quint8 *>(&value), 8);
            }

            return true;
        }
        else
        {
            asa_utils::safeIncrement(m_l3CacheMisses);
            updateCacheStatistics("L3", false);
        }
    }

    // Fallback to CPU memory access
    return m_cpu ? m_cpu->readMemory(address, reinterpret_cast<quint8 *>(&value), 8) : false;
}
bool opcode18_executorAlphaMemoryBarrier::performMemoryWrite(quint64 address, const MemoryBarrierInstruction &instr)
{
    DEBUG_LOG(QString("Performing memory write at address: 0x%1").arg(address, 0, 16));

    quint64 value = instr.result;

    // Write through cache hierarchy
    bool success = true;

    // Write to L1 data cache
    if (m_level1DataCache)
    {
        success &= m_level1DataCache->write(address, reinterpret_cast<quint8 *>(&value), 8);
    }

    // Write to L2 cache
    if (m_level2Cache)
    {
        success &= m_level2Cache->write(address, reinterpret_cast<quint8 *>(&value), 8);
    }

    // Write to L3 cache
    if (m_level3Cache)
    {
        success &= m_level3Cache->write(address, reinterpret_cast<quint8 *>(&value), 8);
    }

    // Write to main memory via CPU
    if (m_cpu)
    {
        success &= m_cpu->writeMemory(address, reinterpret_cast<quint8 *>(&value), 8);
    }

    return success;
}

void opcode18_executorAlphaMemoryBarrier::printStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    DEBUG_LOG("=== Alpha Memory Barrier Executor Statistics ===");
    DEBUG_LOG(QString("Total Barrier Instructions: %1").arg(m_barrierInstructions.load()));
    DEBUG_LOG(QString("Memory Barriers: %1").arg(m_memoryBarriers.load()));
    DEBUG_LOG(QString("Write Barriers: %1").arg(m_writeBarriers.load()));
    DEBUG_LOG(QString("Trap Barriers: %1").arg(m_trapBarriers.load()));
    DEBUG_LOG(QString("Instruction Barriers: %1").arg(m_instructionBarriers.load()));
    DEBUG_LOG(QString("SMP Coordinations: %1").arg(m_smpCoordinations.load()));
    DEBUG_LOG(QString("Barrier Timeouts: %1").arg(m_barrierTimeouts.load()));
    DEBUG_LOG(QString("Barrier Eliminations: %1").arg(m_barrierEliminations.load()));

    DEBUG_LOG("\n=== Cache Performance ===");
    DEBUG_LOG(QString("L1 I-Cache: Hits=%1, Misses=%2").arg(m_l1ICacheHits.load()).arg(m_l1ICacheMisses.load()));
    DEBUG_LOG(QString("L1 D-Cache: Hits=%1, Misses=%2").arg(m_l1DCacheHits.load()).arg(m_l1DCacheMisses.load()));
    DEBUG_LOG(QString("L2 Cache: Hits=%1, Misses=%2").arg(m_l2CacheHits.load()).arg(m_l2CacheMisses.load()));
    DEBUG_LOG(QString("L3 Cache: Hits=%1, Misses=%2").arg(m_l3CacheHits.load()).arg(m_l3CacheMisses.load()));

    // Calculate hit rates
    quint64 totalL1IAccess = m_l1ICacheHits.load() + m_l1ICacheMisses.load();
    quint64 totalL1DAccess = m_l1DCacheHits.load() + m_l1DCacheMisses.load();
    quint64 totalL2Access = m_l2CacheHits.load() + m_l2CacheMisses.load();
    quint64 totalL3Access = m_l3CacheHits.load() + m_l3CacheMisses.load();

    if (totalL1IAccess > 0)
    {
        double hitRate = (static_cast<double>(m_l1ICacheHits.load()) / totalL1IAccess) * 100.0;
        DEBUG_LOG(QString("L1 I-Cache Hit Rate: %1%").arg(QString::number(hitRate, 'f', 2)));
    }

    if (totalL1DAccess > 0)
    {
        double hitRate = (static_cast<double>(m_l1DCacheHits.load()) / totalL1DAccess) * 100.0;
        DEBUG_LOG(QString("L1 D-Cache Hit Rate: %1%").arg(QString::number(hitRate, 'f', 2)));
    }

    if (totalL2Access > 0)
    {
        double hitRate = (static_cast<double>(m_l2CacheHits.load()) / totalL2Access) * 100.0;
        DEBUG_LOG(QString("L2 Cache Hit Rate: %1%").arg(QString::number(hitRate, 'f', 2)));
    }

    if (totalL3Access > 0)
    {
        asa_utils::safeIncrement(m_l3CacheHits);
        double hitRate = (static_cast<double>(m_l3CacheHits) / totalL3Access) * 100.0;
        DEBUG_LOG(QString("L3 Cache Hit Rate: %1%").arg(QString::number(hitRate, 'f', 2)));
    }
}

void opcode18_executorAlphaMemoryBarrier::checkPageTableEntry(quint64 virtualAddress, bool isWrite)
{
    DEBUG_LOG(QString("Checking page table entry for VA: 0x%1, Write: %2").arg(virtualAddress, 0, 16).arg(isWrite));

    // This is a placeholder for page table validation
    // In a real implementation, this would:
    // 1. Translate virtual to physical address
    // 2. Check page permissions
    // 3. Update TLB if needed
    // 4. Handle page faults

    if (m_dTLB)
    {
        quint64 physicalAddr = 0;
        quint64 currentASN = m_cpu ? m_cpu->getCurrentASN() : 0;

        bool tlbHit = m_dTLB->lookup(virtualAddress, currentASN, isWrite, false, physicalAddr);

        if (!tlbHit)
        {
            DEBUG_LOG(QString("TLB miss for VA: 0x%1").arg(virtualAddress, 0, 16));

            // Could trigger page table walk here
            if (m_cpu)
            {
                m_cpu->handleTLBMiss(virtualAddress, isWrite);
            }
        }
    }
}


void opcode18_executorAlphaMemoryBarrier::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);

    m_barrierInstructions.storeRelaxed(0);
    m_memoryBarriers.storeRelaxed(0);
    m_writeBarriers.storeRelaxed(0);
    m_trapBarriers.storeRelaxed(0);
    m_instructionBarriers.storeRelaxed(0);
    m_smpCoordinations.storeRelaxed(0);
    m_barrierTimeouts.storeRelaxed(0);
    m_barrierEliminations.storeRelaxed(0);

    m_l1ICacheHits.storeRelaxed(0);
    m_l1ICacheMisses.storeRelaxed(0);
    m_l1DCacheHits.storeRelaxed(0);
    m_l1DCacheMisses.storeRelaxed(0);
    m_l2CacheHits.storeRelaxed(0);
    m_l2CacheMisses.storeRelaxed(0);
    m_l3CacheHits.storeRelaxedRe(0);
    m_l3CacheMisses.storeRelaxed(0);

    // Clear JIT optimization tracking
    m_barrierExecutionCount.clear();
    m_frequentBarriers.clear();
    m_lastBarrierTime.clear();
    m_eliminatedBarriers.storeRelaxed(0);
}




void opcode18_executorAlphaMemoryBarrier::printJITOptimizationStats() const {

//TODO
// 
}

void opcode18_executorAlphaMemoryBarrier::writebackWorker()
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
            MemoryBarrierInstruction instr = m_writebackQueue.dequeue();

            // Record completion and update statistics
            recordBarrierCompletion(instr);

            // Update dependency tracking
            updateDependencies(instr);

            // Emit completion signals
            emit sigBarrierInstructionExecuted(instr.barrierType, instr.isCompleted, instr.cyclesWaited);

            // Specific barrier completion signals
            if (instr.isCompleted)
            {
                switch (instr.barrierType)
                {
                case FUNC_MB:
                    emit sigMemoryBarrierCompleted(instr.pc, instr.cyclesWaited);
                    break;
                case FUNC_WMB:
                    emit sigWriteBarrierCompleted(instr.pc, instr.cyclesWaited);
                    break;
                case FUNC_TRAPB:
                    emit sigTrapBarrierCompleted(instr.pc, instr.cyclesWaited);
                    break;
                    default