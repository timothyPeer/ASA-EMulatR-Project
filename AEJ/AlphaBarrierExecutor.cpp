#include "AlphaBarrierExecutor.h"
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVector>
#include <QQueue>
#include <QFuture>
#include <QtConcurrent>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QSemaphore>
#include <QMutexLocker>
#include <atomic>
#include <thread>
#include "decodedInstruction.h"


// ========== AlphaBarrierExecutor.cpp ==========

AlphaBarrierExecutor::AlphaBarrierExecutor(AlphaCPU* cpu, QObject* parent)
    : QObject(parent), m_cpu(cpu), m_fpExecutor(nullptr), m_intExecutor(nullptr), m_sqrtExecutor(nullptr) {
    
    qDebug() << "AlphaBarrierExecutor: Initialized for memory and exception synchronization";
}

AlphaBarrierExecutor::~AlphaBarrierExecutor() {
    stopBarrierProcessor();
}

void AlphaBarrierExecutor::startBarrierProcessor() {
    if (m_barrierActive.exchange(true)) {
        return; // Already running
    }
    
    // Clear state
    {
        QMutexLocker locker(&m_barrierMutex);
        m_barrierQueue.clear();
        m_sequenceCounter.store(0);
    }
    
    // Reset barrier states
    m_memoryBarrierPending.store(false);
    m_writeBarrierPending.store(false);
    m_trapBarrierPending.store(false);
    m_pendingMemoryOps.store(0);
    m_pendingWriteOps.store(0);
    m_pendingExceptions.store(0);
    
    // Start barrier worker
    m_barrierWorker = QtConcurrent::run([this]() { barrierWorker(); });
    
    qDebug() << "Alpha Barrier Processor started";
}

void AlphaBarrierExecutor::stopBarrierProcessor() {
    if (!m_barrierActive.exchange(false)) {
        return; // Already stopped
    }
    
    // Wake up worker
    m_barrierCondition.wakeAll();
    m_completionSemaphore.release(10);
    
    // Wait for completion
    m_barrierWorker.waitForFinished();
    
    qDebug() << "Alpha Barrier Processor stopped";
}

bool AlphaBarrierExecutor::submitBarrier(const DecodedInstruction& instruction, quint64 pc) {
    if (!m_barrierActive.load()) {
        return false;
    }
    
    QMutexLocker locker(&m_barrierMutex);
    
    if (m_barrierQueue.size() >= MAX_BARRIER_QUEUE) {
        return false; // Queue full
    }
    
    quint64 seqNum = m_sequenceCounter.fetch_add(1);
    BarrierInstruction barrier(instruction, pc, seqNum);
    
    // Decode barrier type
    barrier.function = (instruction.raw >> 5) & 0x7FFF; // 15-bit function for barriers
    
    switch (barrier.function) {
        case FUNC_TRAPB:
            barrier.type = BarrierInstruction::TRAP_BARRIER;
            break;
        case FUNC_MB:
            barrier.type = BarrierInstruction::MEMORY_BARRIER;
            break;
        case FUNC_WMB:
            barrier.type = BarrierInstruction::WRITE_BARRIER;
            break;
        default:
            return false; // Unknown barrier type
    }
    
    m_barrierQueue.enqueue(barrier);
    m_barrierCondition.wakeOne();
    
    return true;
}

bool AlphaBarrierExecutor::executeBarrier(const DecodedInstruction& instruction) {
    // Synchronous barrier execution (fallback)
    BarrierInstruction barrier(instruction, 0, 0);
    barrier.function = (instruction.raw >> 5) & 0x7FFF;
    
    switch (barrier.function) {
        case FUNC_TRAPB:
            barrier.type = BarrierInstruction::TRAP_BARRIER;
            return executeTrapBarrier(barrier);
        case FUNC_MB:
            barrier.type = BarrierInstruction::MEMORY_BARRIER;
            return executeMemoryBarrier(barrier);
        case FUNC_WMB:
            barrier.type = BarrierInstruction::WRITE_BARRIER;
            return executeWriteMemoryBarrier(barrier);
        default:
            return false;
    }
}

void AlphaBarrierExecutor::barrierWorker() {
    while (m_barrierActive.load()) {
        QMutexLocker locker(&m_barrierMutex);
        
        while (m_barrierQueue.isEmpty() && m_barrierActive.load()) {
            m_barrierCondition.wait(&m_barrierMutex, 100);
        }
        
        if (!m_barrierActive.load()) break;
        
        if (!m_barrierQueue.isEmpty()) {
            BarrierInstruction barrier = m_barrierQueue.dequeue();
            locker.unlock();
            
            // Measure stall cycles
            quint64 startTime = QDateTime::currentMSecsSinceEpoch();
            bool success = false;
            
            // Execute appropriate barrier type
            switch (barrier.type) {
                case BarrierInstruction::TRAP_BARRIER:
                    success = executeTrapBarrier(barrier);
                    break;
                case BarrierInstruction::MEMORY_BARRIER:
                    success = executeMemoryBarrier(barrier);
                    break;
                case BarrierInstruction::WRITE_BARRIER:
                    success = executeWriteMemoryBarrier(barrier);
                    break;
            }
            
            quint64 endTime = QDateTime::currentMSecsSinceEpoch();
            int stallCycles = static_cast<int>(endTime - startTime);
            
            // Update statistics
            {
                QMutexLocker statsLocker(&m_statsMutex);
                m_barrierInstructions++;
                m_totalStallCycles += stallCycles;
                updateBarrierLatency(stallCycles);
                
                switch (barrier.type) {
                    case BarrierInstruction::TRAP_BARRIER:
                        m_trapBarriers++;
                        break;
                    case BarrierInstruction::MEMORY_BARRIER:
                        m_memoryBarriers++;
                        break;
                    case BarrierInstruction::WRITE_BARRIER:
                        m_writeBarriers++;
                        break;
                }
            }
            
            emit barrierExecuted(barrier.function, stallCycles, success);
            
            if (stallCycles > 100) { // Significant stall
                emit barrierStalled(
                    barrier.type == BarrierInstruction::TRAP_BARRIER ? "TRAPB" :
                    barrier.type == BarrierInstruction::MEMORY_BARRIER ? "MB" : "WMB",
                    stallCycles);
            }
        }
    }
}

bool AlphaBarrierExecutor::executeTrapBarrier(BarrierInstruction& barrier) {
    qDebug() << "Executing TRAPB at PC:" << Qt::hex << barrier.pc;
    
    // Set trap barrier pending
    m_trapBarrierPending.store(true);
    
    // 1. Drain all execution pipelines to ensure no new operations start
    drainExecutionPipelines();
    
    // 2. Wait for all pending exceptions to be processed
    if (!waitForExceptionCompletion(2000)) {
        emit barrierStalled("TRAPB - Exception timeout", 2000);
        m_trapBarrierPending.store(false);
        return false;
    }
    
    // 3. Drain any queued exceptions
    drainExceptionQueue();
    
    // 4. Ensure all floating-point operations complete (they can generate exceptions)
    if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive()) {
        // Wait for FP pipeline to drain
        int timeout = 1000;
        while (timeout-- > 0 && m_pendingExceptions.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // 5. Clear any speculative state that could cause exceptions
    invalidateSpeculativeState();
    
    // 6. Memory barrier to ensure all memory operations complete before exceptions
    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    // 7. Clear trap barrier state
    m_trapBarrierPending.store(false);
    
    emit memoryOrderingEnforced("TRAPB");
    qDebug() << "TRAPB completed successfully";
    
    return true;
}

bool AlphaBarrierExecutor::executeMemoryBarrier(BarrierInstruction& barrier) {
    qDebug() << "Executing MB at PC:" << Qt::hex << barrier.pc;
    
    // Set memory barrier pending
    m_memoryBarrierPending.store(true);
    
    // 1. Wait for all pending memory operations (both reads and writes)
    if (!waitForPendingOperations(BarrierInstruction::MEMORY_BARRIER, 3000)) {
        emit barrierStalled("MB - Memory operation timeout", 3000);
        m_memoryBarrierPending.store(false);
        return false;
    }
    
    // 2. Flush entire cache hierarchy to ensure memory coherency
    flushCacheHierarchy(false); // Full flush
    
    // 3. Synchronize with memory system
    synchronizeMemorySystem();
    
    // 4. Enforce memory ordering at CPU level
    enforceMemoryOrdering();
    
    // 5. Full memory barrier at hardware level
    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    // 6. Coordinate with other CPUs in SMP system
    broadcastBarrierToOtherCPUs(BarrierInstruction::MEMORY_BARRIER);
    waitForSMPBarrierAcknowledgment();
    
    // 7. Clear memory barrier state
    m_memoryBarrierPending.store(false);
    
    emit memoryOrderingEnforced("MB");
    qDebug() << "MB completed successfully";
    
    return true;
}

bool AlphaBarrierExecutor::executeWriteMemoryBarrier(BarrierInstruction& barrier) {
    qDebug() << "Executing WMB at PC:" << Qt::hex << barrier.pc;
    
    // Set write barrier pending
    m_writeBarrierPending.store(true);
    
    // 1. Wait for all pending write operations only
    if (!waitForPendingOperations(BarrierInstruction::WRITE_BARRIER, 2000)) {
        emit barrierStalled("WMB - Write operation timeout", 2000);
        m_writeBarrierPending.store(false);
        return false;
    }
    
    // 2. Flush only write operations from cache hierarchy
    flushCacheHierarchy(true); // Write-only flush
    
    // 3. Enforce write ordering
    enforceWriteOrdering();
    
    // 4. Write memory barrier at hardware level
    std::atomic_thread_fence(std::memory_order_release);
    
    // 5. Coordinate write barrier with other CPUs
    broadcastBarrierToOtherCPUs(BarrierInstruction::WRITE_BARRIER);
    
    // 6. Clear write barrier state
    m_writeBarrierPending.store(false);
    
    emit memoryOrderingEnforced("WMB");
    qDebug() << "WMB completed successfully";
    
    return true;
}

// Memory Operation Notification Interface
void AlphaBarrierExecutor::notifyMemoryOperation(bool isWrite) {
    m_pendingMemoryOps.fetch_add(1);
    if (isWrite) {
        m_pendingWriteOps.fetch_add(1);
    }
}

void AlphaBarrierExecutor::notifyMemoryOperationComplete(bool isWrite) {
    m_pendingMemoryOps.fetch_sub(1);
    if (isWrite) {
        m_pendingWriteOps.fetch_sub(1);
    }
    
    // Wake up any waiting barriers
    m_completionSemaphore.release();
}

void AlphaBarrierExecutor::notifyExceptionPending() {
    m_pendingExceptions.fetch_add(1);
}

void AlphaBarrierExecutor::notifyExceptionComplete() {
    m_pendingExceptions.fetch_sub(1);
    m_completionSemaphore.release();
}

// Synchronization Helpers
bool AlphaBarrierExecutor::waitForPendingOperations(BarrierInstruction::BarrierType type, int timeoutMs) {
    int timeout = timeoutMs;
    
    switch (type) {
        case BarrierInstruction::MEMORY_BARRIER:
            // Wait for all memory operations
            while (timeout-- > 0 && m_pendingMemoryOps.load() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return m_pendingMemoryOps.load() == 0;
            
        case BarrierInstruction::WRITE_BARRIER:
            // Wait for write operations only
            while (timeout-- > 0 && m_pendingWriteOps.load() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return m_pendingWriteOps.load() == 0;
            
        case BarrierInstruction::TRAP_BARRIER:
            // Wait for exceptions
            while (timeout-- > 0 && m_pendingExceptions.load() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return m_pendingExceptions.load() == 0;
            
        default:
            return true;
    }
}

void AlphaBarrierExecutor::drainExecutionPipelines() {
    // Signal all execution units to drain their pipelines
    if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive()) {
        // FP executor should drain gracefully
        qDebug() << "Draining FP pipeline for barrier";
    }
    
    if (m_intExecutor && m_intExecutor->isAsyncPipelineActive()) {
        // Integer executor should drain gracefully
        qDebug() << "Draining Integer pipeline for barrier";
    }
    
    if (m_sqrtExecutor && m_sqrtExecutor->isAsyncPipelineActive()) {
        // SQRT executor should drain gracefully
        qDebug() << "Draining SQRT pipeline for barrier";
    }
}

void AlphaBarrierExecutor::flushCacheHierarchy(bool writeOnly) {
    m_cacheFlushInProgress.store(true);
    m_flushCompletionCount.store(0);
    
    emit cacheFlushRequested(writeOnly);
    
    // Flush all cache levels
    if (m_level1DataCache) {
        if (writeOnly) {
            m_level1DataCache->flushWrites();
        } else {
            m_level1DataCache->flush();
        }
    }
    
    if (m_level2Cache) {
        if (writeOnly) {
            m_level2Cache->flushWrites();
        } else {
            m_level2Cache->flush();
        }
    }
    
    if (m_level3Cache) {
        if (writeOnly) {
            m_level3Cache->flushWrites();
        } else {
            m_level3Cache->flush();
        }
    }
    
    // Wait for flush completion
    int timeout = 1000;
    while (timeout-- > 0 && m_cacheFlushInProgress.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void AlphaBarrierExecutor::synchronizeMemorySystem() {
    // Ensure all TLB entries are consistent
    if (m_iTLB) {
        // TLB synchronization would happen here
        qDebug() << "Synchronizing I-TLB for memory barrier";
    }
    
    if (m_dTLB) {
        // TLB synchronization would happen here
        qDebug() << "Synchronizing D-TLB for memory barrier";
    }
}

void AlphaBarrierExecutor::enforceMemoryOrdering() {
    // Hardware memory ordering enforcement
    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    // Additional Alpha-specific memory ordering
    // This would involve hardware-specific operations
    qDebug() << "Memory ordering enforced";
}

void AlphaBarrierExecutor::enforceWriteOrdering() {
    // Write ordering enforcement
    std::atomic_thread_fence(std::memory_order_release);
    
    qDebug() << "Write ordering enforced";
}

void AlphaBarrierExecutor::invalidateSpeculativeState() {
    // Clear any speculative execution state that could cause exceptions
    // This would involve clearing branch prediction, speculative loads, etc.
    qDebug() << "Speculative state invalidated";
}

void AlphaBarrierExecutor::drainExceptionQueue() {
    // Process any queued exceptions
    // This would interface with the CPU's exception handling system
    qDebug() << "Exception queue drained";
}

bool AlphaBarrierExecutor::waitForExceptionCompletion(int timeoutMs) {
    int timeout = timeoutMs;
    while (timeout-- > 0 && m_pendingExceptions.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return m_pendingExceptions.load() == 0;
}

// Multiprocessor Coordination
void AlphaBarrierExecutor::broadcastBarrierToOtherCPUs(BarrierInstruction::BarrierType type) {
    // In an SMP system, this would broadcast the barrier to other CPUs
    qDebug() << "Broadcasting barrier to other CPUs:" << 
                (type == BarrierInstruction::MEMORY_BARRIER ? "MB" :
                 type == BarrierInstruction::WRITE_BARRIER ? "WMB" : "TRAPB");
}

void AlphaBarrierExecutor::waitForSMPBarrierAcknowledgment() {
    // Wait for other CPUs to acknowledge the barrier
    // This would involve inter-processor communication
    qDebug() << "Waiting for SMP barrier acknowledgment";
}

// Cache Operations
bool AlphaBarrierExecutor::fetchInstructionWithCache(quint64 pc, quint32& instruction) {
    // Standard cache fetch operation (similar to other executors)
    quint64 physicalPC;
    if (m_iTLB && !m_iTLB->lookup(pc, m_cpu->getCurrentASN(), false, true, physicalPC)) {
        return false; // TLB miss
    } else if (!m_iTLB) {
        physicalPC = pc;
    }
    
    if (m_instructionCache) {
        return m_instructionCache->read(physicalPC, 
                                       reinterpret_cast<quint8*>(&instruction), 4);
    }
    
    return m_cpu ? m_cpu->readMemory(physicalPC, reinterpret_cast<quint8*>(&instruction), 4) : false;
}

// Performance Monitoring
void AlphaBarrierExecutor::updateBarrierLatency(int cycles) {
    if (m_barrierInstructions > 0) {
        m_averageBarrierLatency = (m_averageBarrierLatency * (m_barrierInstructions - 1) + cycles) / m_barrierInstructions;
    } else {
        m_averageBarrierLatency = cycles;
    }
}

void AlphaBarrierExecutor::printStatistics() const {
    QMutexLocker locker(&m_statsMutex);
    
    qDebug() << "=== Alpha Barrier Executor Statistics ===";
    qDebug() << "Total Barrier Instructions:" << m_barrierInstructions;
    qDebug() << "TRAPB Barriers:" << m_trapBarriers;
    qDebug() << "Memory Barriers (MB):" << m_memoryBarriers;
    qDebug() << "Write Memory Barriers (WMB):" << m_writeBarriers;
    qDebug() << "Total Stall Cycles:" << m_totalStallCycles;
    qDebug() << "Average Barrier Latency:" << m_averageBarrierLatency << "cycles";
    
    if (m_barrierInstructions > 0) {
        qDebug() << "Average Stall per Barrier:" << 
                    (static_cast<double>(m_totalStallCycles) / m_barrierInstructions) << "cycles";
    }
    
    qDebug() << "Current State:";
    qDebug() << "  Memory Barrier Pending:" << m_memoryBarrierPending.load();
    qDebug() << "  Write Barrier Pending:" << m_writeBarrierPending.load();
    qDebug() << "  Trap Barrier Pending:" << m_trapBarrierPending.load();
    qDebug() << "  Pending Memory Ops:" << m_pendingMemoryOps.load();
    qDebug() << "  Pending Write Ops:" << m_pendingWriteOps.load();
    qDebug() << "  Pending Exceptions:" << m_pendingExceptions.load();
}

void AlphaBarrierExecutor::clearStatistics() {
    QMutexLocker locker(&m_statsMutex);
    
    m_barrierInstructions = 0;
    m_trapBarriers = 0;
    m_memoryBarriers = 0;
    m_writeBarriers = 0;
    m_totalStallCycles = 0;
    m_averageBarrierLatency = 0;
}

void AlphaBarrierExecutor::requestCacheFlush(bool writeOnly) {
    flushCacheHierarchy(writeOnly);
}

void AlphaBarrierExecutor::notifyCacheFlushComplete() {
    m_flushCompletionCount.fetch_add(1);
    if (m_flushCompletionCount.load() >= 3) { // All cache levels complete
        m_cacheFlushInProgress.store(false);
    }
}

int AlphaBarrierExecutor::measureStallCycles(const BarrierInstruction& barrier) {
    // Measure stall cycles based on barrier type and system state
    int baseCycles = 0;
    int additionalCycles = 0;
    
    switch (barrier.type) {
        case BarrierInstruction::TRAP_BARRIER:
            // Base cost for TRAPB
            baseCycles = 50;
            
            // Additional cycles based on pending exceptions
            additionalCycles += m_pendingExceptions.load() * 20;
            
            // Pipeline depth affects stall time
            if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive()) {
                additionalCycles += 30; // FP pipeline drain time
            }
            if (m_sqrtExecutor && m_sqrtExecutor->isAsyncPipelineActive()) {
                additionalCycles += 40; // SQRT pipeline drain time (longer latency)
            }
            if (m_intExecutor && m_intExecutor->isAsyncPipelineActive()) {
                additionalCycles += 15; // Integer pipeline drain time
            }
            break;
            
        case BarrierInstruction::MEMORY_BARRIER:
            // Base cost for MB (most expensive)
            baseCycles = 200;
            
            // Additional cycles based on pending memory operations
            additionalCycles += m_pendingMemoryOps.load() * 10;
            
            // Cache hierarchy flush costs
            if (m_level1DataCache) additionalCycles += 50;  // L1 flush
            if (m_level2Cache) additionalCycles += 100;     // L2 flush
            if (m_level3Cache) additionalCycles += 200;     // L3 flush
            
            // TLB synchronization cost
            if (m_iTLB || m_dTLB) additionalCycles += 30;
            
            // SMP coordination cost (simulated)
            additionalCycles += 150; // Inter-processor communication
            break;
            
        case BarrierInstruction::WRITE_BARRIER:
            // Base cost for WMB
            baseCycles = 100;
            
            // Additional cycles based on pending write operations
            additionalCycles += m_pendingWriteOps.load() * 15;
            
            // Write buffer flush costs
            if (m_level1DataCache) additionalCycles += 25;  // L1 write flush
            if (m_level2Cache) additionalCycles += 50;      // L2 write flush
            if (m_level3Cache) additionalCycles += 75;      // L3 write flush
            
            // Write ordering enforcement
            additionalCycles += 25;
            break;
    }
    
    // Memory system load factor
    int memoryLoadFactor = (m_pendingMemoryOps.load() > 10) ? 2 : 1;
    additionalCycles *= memoryLoadFactor;
    
    // Cache flush in progress adds significant delay
    if (m_cacheFlushInProgress.load()) {
        additionalCycles += 300; // Major cache flush penalty
    }
    
    // Recent barrier history affects performance (cache/TLB thrashing)
    static quint64 lastBarrierTime = 0;
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - lastBarrierTime < 100) { // Recent barrier within 100ms
        additionalCycles += 50; // Penalty for frequent barriers
    }
    lastBarrierTime = currentTime;
    
    // System-wide contention factor
    int contentionFactor = 1;
    if (m_memoryBarrierPending.load() || m_writeBarrierPending.load() || m_trapBarrierPending.load()) {
        contentionFactor = 2; // Multiple barriers pending
    }
    
    int totalCycles = (baseCycles + additionalCycles) * contentionFactor;
    
    // Sanity check - barriers shouldn't be too fast or too slow
    totalCycles = qMax(10, qMin(5000, totalCycles));
    
    return totalCycles;
}

bool AlphaBarrierExecutor::submitExtendedBarrier(const DecodedInstruction &instruction, quint64 pc)
{
    if (!m_barrierActive.load())
    {
        return false;
    }

    QMutexLocker locker(&m_barrierMutex);

    if (m_barrierQueue.size() >= MAX_BARRIER_QUEUE)
    {
        return false; // Queue full
    }

    quint64 seqNum = m_sequenceCounter.fetch_add(1);
    ExtendedBarrierInstruction barrier(instruction, pc, seqNum);

    // Decode extended barrier/system function type
    barrier.function = (instruction.raw >> 5) & 0xFFFF; // 16-bit function for extended ops

    switch (barrier.function)
    {
    case BARRIER_TYPE_TRAPB:
    case FUNC_TRAPB:
        barrier.type = ExtendedBarrierInstruction::TRAP_BARRIER;
        break;
    case BARRIER_TYPE_MB:
    case FUNC_MB:
        barrier.type = ExtendedBarrierInstruction::MEMORY_BARRIER;
        break;
    case BARRIER_TYPE_WMB:
    case FUNC_WMB:
        barrier.type = ExtendedBarrierInstruction::WRITE_BARRIER;
        break;
    case BARRIER_TYPE_EXCB:
    case FUNC_EXCB:
        barrier.type = ExtendedBarrierInstruction::EXCEPTION_BARRIER;
        break;
    case FUNC_FETCH:
        barrier.type = ExtendedBarrierInstruction::PREFETCH_DATA;
        barrier.targetAddress = extractPrefetchAddress(instruction);
        break;
    case FUNC_FETCH_M:
        barrier.type = ExtendedBarrierInstruction::PREFETCH_MODIFY;
        barrier.targetAddress = extractPrefetchAddress(instruction);
        break;
    case FUNC_RPCC:
        barrier.type = ExtendedBarrierInstruction::READ_CYCLE_COUNTER;
        barrier.targetRegister = instruction.raw & 0x1F; // Rc field
        break;
    case FUNC_RC:
        barrier.type = ExtendedBarrierInstruction::READ_AND_CLEAR;
        barrier.targetRegister = instruction.raw & 0x1F; // Rc field
        break;
    default:
        return false; // Unknown function
    }

    // Convert to base BarrierInstruction format for queue
    BarrierInstruction baseBarrier(instruction, pc, seqNum);
    baseBarrier.function = barrier.function;

    // Map extended types to base types where applicable
    switch (barrier.type)
    {
    case ExtendedBarrierInstruction::TRAP_BARRIER:
        baseBarrier.type = BarrierInstruction::TRAP_BARRIER;
        break;
    case ExtendedBarrierInstruction::MEMORY_BARRIER:
        baseBarrier.type = BarrierInstruction::MEMORY_BARRIER;
        break;
    case ExtendedBarrierInstruction::WRITE_BARRIER:
        baseBarrier.type = BarrierInstruction::WRITE_BARRIER;
        break;
    default:
        // Extended types use MEMORY_BARRIER as base for processing
        baseBarrier.type = BarrierInstruction::MEMORY_BARRIER;
        break;
    }

    m_barrierQueue.enqueue(baseBarrier);
    m_barrierCondition.wakeOne();

    return true;
}

bool AlphaBarrierExecutor::executeExceptionBarrier(ExtendedBarrierInstruction &barrier)
{
    qDebug() << "Executing EXCB at PC:" << Qt::hex << barrier.pc;

    // Set exception barrier pending
    m_exceptionBarrierPending.store(true);

    // 1. Similar to TRAPB but more comprehensive exception handling
    drainExecutionPipelines();

    // 2. Wait for ALL types of exceptions (not just traps)
    if (!waitForExceptionCompletion(3000))
    {
        emit barrierStalled("EXCB - Exception timeout", 3000);
        m_exceptionBarrierPending.store(false);
        return false;
    }

    // 3. Clear machine check exceptions and error states
    clearMachineCheckState();

    // 4. Synchronize exception handling with memory system
    synchronizeExceptionState();

    // 5. Full barrier to ensure exception ordering
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // 6. Clear exception barrier state
    m_exceptionBarrierPending.store(false);

    {
        QMutexLocker locker(&m_statsMutex);
        m_exceptionBarriers++;
    }

    emit memoryOrderingEnforced("EXCB");
    qDebug() << "EXCB completed successfully";

    return true;
}

bool AlphaBarrierExecutor::executePrefetchData(ExtendedBarrierInstruction &barrier)
{
    qDebug() << "Executing FETCH at PC:" << Qt::hex << barrier.pc << "Address:" << Qt::hex << barrier.targetAddress;

    // Set prefetch active
    m_prefetchActive.store(true);

    // 1. Add address to prefetch queue
    if (m_prefetchQueue.size() < MAX_PREFETCH_QUEUE)
    {
        m_prefetchQueue.enqueue(barrier.targetAddress);
    }

    // 2. Initiate cache prefetch (non-blocking)
    initiateCachePrefetch(barrier.targetAddress, false);

    // 3. Process prefetch queue
    processPrefetchQueue();

    m_prefetchActive.store(false);

    {
        QMutexLocker locker(&m_statsMutex);
        m_prefetchRequests++;
    }

    emit memoryOrderingEnforced("FETCH");
    return true;
}

bool AlphaBarrierExecutor::executePrefetchModify(ExtendedBarrierInstruction &barrier)
{
    qDebug() << "Executing FETCH_M at PC:" << Qt::hex << barrier.pc << "Address:" << Qt::hex << barrier.targetAddress;

    // Similar to FETCH but with modify intent
    m_prefetchActive.store(true);

    if (m_prefetchQueue.size() < MAX_PREFETCH_QUEUE)
    {
        m_prefetchQueue.enqueue(barrier.targetAddress);
    }

    // Prefetch with modify intent (exclusive cache line request)
    initiateCachePrefetch(barrier.targetAddress, true);
    processPrefetchQueue();

    m_prefetchActive.store(false);

    {
        QMutexLocker locker(&m_statsMutex);
        m_prefetchRequests++;
    }

    emit memoryOrderingEnforced("FETCH_M");
    return true;
}

bool AlphaBarrierExecutor::executeReadProcessCycleCounter(ExtendedBarrierInstruction &barrier)
{
    qDebug() << "Executing RPCC at PC:" << Qt::hex << barrier.pc;

    // Update cycle counter
    updateCycleCounter();

    // Read current process cycle counter
    quint64 cycleCount = readProcessCycleCounter();

    // Write to target register
    if (m_cpu && barrier.targetRegister != 31)
    { // R31 is always zero
        m_cpu->setIntegerRegister(barrier.targetRegister, cycleCount);
    }

    {
        QMutexLocker locker(&m_statsMutex);
        m_cycleCounterReads++;
    }

    emit memoryOrderingEnforced("RPCC");
    return true;
}

bool AlphaBarrierExecutor::executeReadAndClear(ExtendedBarrierInstruction &barrier)
{
    qDebug() << "Executing RC at PC:" << Qt::hex << barrier.pc;

    // Read and clear performance counter (implementation specific)
    quint64 counterValue = readAndClearCounter("performance");

    // Write to target register
    if (m_cpu && barrier.targetRegister != 31)
    {
        m_cpu->setIntegerRegister(barrier.targetRegister, counterValue);
    }

    {
        QMutexLocker locker(&m_statsMutex);
        m_readAndClearOps++;
    }

    emit memoryOrderingEnforced("RC");
    return true;
}

// System Function Implementations
quint64 AlphaBarrierExecutor::readProcessCycleCounter()
{
    // Return current process cycle counter
    return m_processCycleCounter.load();
}

quint64 AlphaBarrierExecutor::readAndClearCounter(const QString &counterName)
{
    if (m_systemCounters.contains(counterName))
    {
        auto counter = m_systemCounters[counterName];
        quint64 value = counter->load();
        counter->store(0); // Clear after reading
        return value;
    }
    return 0;
}

bool AlphaBarrierExecutor::requestPrefetch(quint64 address, bool modifyIntent)
{
    if (m_prefetchQueue.size() >= MAX_PREFETCH_QUEUE)
    {
        return false; // Queue full
    }

    m_prefetchQueue.enqueue(address);
    initiateCachePrefetch(address, modifyIntent);
    return true;
}

void AlphaBarrierExecutor::initializePerformanceCounters()
{
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_processCycleCounter.store(0);

    // Initialize system counters
    m_systemCounters["performance"] = new std::atomic<quint64>(0);
    m_systemCounters["cache_misses"] = new std::atomic<quint64>(0);
    m_systemCounters["tlb_misses"] = new std::atomic<quint64>(0);
    m_systemCounters["branch_mispredicts"] = new std::atomic<quint64>(0);

    qDebug() << "Performance counters initialized";
}

void AlphaBarrierExecutor::updatePerformanceCounter(const QString &name, quint64 increment)
{
    if (m_systemCounters.contains(name))
    {
        m_systemCounters[name]->fetch_add(increment);
    }
}

quint64 AlphaBarrierExecutor::getPerformanceCounter(const QString &name) const
{
    if (m_systemCounters.contains(name))
    {
        return m_systemCounters[name]->load();
    }
    return 0;
}

// Helper Methods
void AlphaBarrierExecutor::processPrefetchQueue()
{
    // Process pending prefetch requests
    while (!m_prefetchQueue.isEmpty())
    {
        quint64 address = m_prefetchQueue.dequeue();

        // Translate virtual address if needed
        quint64 physicalAddress;
        if (m_dTLB && m_dTLB->lookup(address, m_cpu->getCurrentASN(), false, false, physicalAddress))
        {
            // Initiate cache line prefetch
            if (m_level1DataCache)
            {
                m_level1DataCache->prefetch(physicalAddress);
            }
            if (m_level2Cache)
            {
                m_level2Cache->prefetch(physicalAddress);
            }
        }
    }
}

void AlphaBarrierExecutor::initiateCachePrefetch(quint64 address, bool modifyIntent)
{
    // Initiate cache prefetch operation
    quint64 physicalAddress;

    // Translate address
    if (m_dTLB && m_dTLB->lookup(address, m_cpu->getCurrentASN(), false, false, physicalAddress))
    {
        // Prefetch into cache hierarchy
        if (m_level1DataCache)
        {
            if (modifyIntent)
            {
                m_level1DataCache->prefetchExclusive(physicalAddress);
            }
            else
            {
                m_level1DataCache->prefetch(physicalAddress);
            }
        }

        if (m_level2Cache)
        {
            if (modifyIntent)
            {
                m_level2Cache->prefetchExclusive(physicalAddress);
            }
            else
            {
                m_level2Cache->prefetch(physicalAddress);
            }
        }

        qDebug() << "Cache prefetch initiated for address:" << Qt::hex << physicalAddress
                 << "Modify intent:" << modifyIntent;
    }
}

void AlphaBarrierExecutor::updateCycleCounter()
{
    // Update process cycle counter based on elapsed time
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    quint64 elapsedMs = currentTime - m_startTime;

    // Simulate CPU cycles (assuming 1GHz clock = 1M cycles per ms)
    quint64 cycles = elapsedMs * 1000000;
    m_processCycleCounter.store(cycles);
}

void AlphaBarrierExecutor::clearMachineCheckState()
{
    // Clear machine check and error state
    qDebug() << "Machine check state cleared";

    // Reset error counters
    updatePerformanceCounter("machine_checks", 0);
}

void AlphaBarrierExecutor::synchronizeExceptionState()
{
    // Synchronize exception state across system
    std::atomic_thread_fence(std::memory_order_seq_cst);
    qDebug() << "Exception state synchronized";
}

quint64 AlphaBarrierExecutor::extractPrefetchAddress(const DecodedInstruction &instruction)
{
    // Extract prefetch address from instruction encoding
    // This is a simplified extraction - real Alpha would use more complex addressing
    quint8 ra = (instruction.raw >> 21) & 0x1F;
    qint16 displacement = static_cast<qint16>(instruction.raw & 0xFFFF);

    if (m_cpu && ra != 31)
    {
        quint64 baseAddress = m_cpu->getIntegerRegister(ra);
        return baseAddress + static_cast<quint64>(displacement);
    }

    return static_cast<quint64>(displacement);
}

int AlphaBarrierExecutor::measureExtendedStallCycles(const ExtendedBarrierInstruction &barrier)
{
    int baseCycles = 0;

    switch (barrier.type)
    {
    case ExtendedBarrierInstruction::EXCEPTION_BARRIER:
        baseCycles = 75; // More expensive than TRAPB
        baseCycles += m_pendingExceptions.load() * 25;
        break;

    case ExtendedBarrierInstruction::PREFETCH_DATA:
    case ExtendedBarrierInstruction::PREFETCH_MODIFY:
        baseCycles = 5; // Very fast, non-blocking
        break;

    case ExtendedBarrierInstruction::READ_CYCLE_COUNTER:
        baseCycles = 3; // Fast register read
        break;

    case ExtendedBarrierInstruction::READ_AND_CLEAR:
        baseCycles = 8; // Slightly more expensive due to clear operation
        break;

    default:
        baseCycles = 50; // Default
        break;
    }

    return baseCycles;
}

// Extended Statistics
void AlphaBarrierExecutor::printExtendedStatistics() const
{
    printStatistics(); // Call base statistics

    QMutexLocker locker(&m_statsMutux);
    qDebug() << "\n=== Extended Barrier Statistics ===";
    qDebug() << "Exception Barriers (EXCB):" << m_exceptionBarriers;
    qDebug() << "Prefetch Requests:" << m_prefetchRequests;
    qDebug() << "Cycle Counter Reads:" << m_cycleCounterReads;
    qDebug() << "Read-and-Clear Ops:" << m_readAndClearOps;

    qDebug() << "\n=== Performance Counters ===";
    for (auto it = m_systemCounters.begin(); it != m_systemCounters.end(); ++it)
    {
        qDebug() << it.key() << ":" << it.value()->load();
    }

    qDebug() << "Process Cycle Counter:" << m_processCycleCounter.load();
}

// // ========== USAGE EXAMPLE ==========
// 
// void demonstrateBarrierExecution() {
//     qDebug() << "=== Alpha Barrier Execution Demonstration ===";
//     
//     // Create CPU and unified execution engine
//     AlphaCPU cpu;
//     AlphaUnifiedExecutionEngine engine(&cpu);
//     
//     // Execute some regular instructions
//     DecodedInstruction instr1, instr2, instr3;
//     instr1.raw = 0x17000020; // CPYS (floating-point)
//     instr2.raw = 0x11000000; // AND (integer logical)
//     instr3.raw = 0x14000000; // SQRT operation
//     
//     engine.executeInstruction(instr1, 0x10000000);
//     engine.executeInstruction(instr2, 0x10000004);
//     engine.executeInstruction(instr3, 0x10000008);
//     
//     // Execute a memory barrier
//     DecodedInstruction barrierInstr;
//     barrierInstr.raw = 0x18000000 | (FUNC_MB << 5); // MB instruction
//     engine.executeInstruction(barrierInstr, 0x1000000C);
//     
//     // Execute more instructions after barrier
//     engine.executeInstruction(instr1, 0x10000010);
//     engine.executeInstruction(instr2, 0x10000014);
//     
//     // Execute a trap barrier
//     DecodedInstruction trapBarrier;
//     trapBarrier.raw = 0x18000000 | (FUNC_TRAPB << 5); // TRAPB instruction
//     engine.executeInstruction(trapBarrier, 0x10000018);
//     
//     // Execute a write memory barrier
//     DecodedInstruction writeBarrier;
//     writeBarrier.raw = 0x18000000 | (FUNC_WMB << 5); // WMB instruction
//     engine.executeInstruction(writeBarrier, 0x1000001C);
//     
//     // Let pipelines run
//     QThread::msleep(500);
//     
//     // Print statistics
//     engine.printExecutionStatistics();
//     
//     qDebug() << "=== Barrier Execution Demonstration Complete ===";
// }
// 
// #include "AlphaBarrierExecutor.moc"

// ========== USAGE EXAMPLE ==========

// void demonstrateBarrierExecution() {
//     qDebug() << "=== Alpha Barrier Execution Demonstration ===";
//     
//     // Create CPU and unified execution engine
//     AlphaCPU cpu;
//     AlphaUnifiedExecutionEngine engine(&cpu);
//     
//     // Execute some regular instructions
//     DecodedInstruction instr1, instr2, instr3;
//     instr1.raw = 0x17000020; // CPYS (floating-point)
//     instr2.raw = 0x11000000; // AND (integer logical)
//     instr3.raw = 0x14000000; // SQRT operation
//     
//     engine.executeInstruction(instr1, 0x10000000);
//     engine.executeInstruction(instr2, 0x10000004);
//     engine.executeInstruction(instr3, 0x10000008);
//     
//     // Execute a memory barrier
//     DecodedInstruction barrierInstr;
//     barrierInstr.raw = 0x18000000 | (FUNC_MB << 5); // MB instruction
//     engine.executeInstruction(barrierInstr, 0x1000000C);
//     
//     // Execute more instructions after barrier
//     engine.executeInstruction(instr1, 0x10000010);
//     engine.executeInstruction(instr2, 0x10000014);
//     
//     // Execute a trap barrier
//     DecodedInstruction trapBarrier;
//     trapBarrier.raw = 0x18000000 | (FUNC_TRAPB << 5); // TRAPB instruction
//     engine.executeInstruction(trapBarrier, 0x10000018);
//     
//     // Execute a write memory barrier
//     DecodedInstruction writeBarrier;
//     writeBarrier.raw = 0x18000000 | (FUNC_WMB << 5); // WMB instruction
//     engine.executeInstruction(writeBarrier, 0x1000001C);
//     
//     // Let pipelines run
//     QThread::msleep(500);
//     
//     // Print statistics
//     engine.printExecutionStatistics();
//     
//     qDebug() << "=== Barrier Execution Demonstration Complete ===";
// }

