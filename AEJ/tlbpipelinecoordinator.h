#pragma once
#include "tlbaddresstranslator.h"


#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"
#include <QtCore/QAtomicInteger>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QtAlgorithms>

/*
tlbPipelineCoordinator coordinates with the collision detector and state manager to ensure proper ordering and resource
allocation for TLB operations, maintaining high throughput while preventing deadlocks.
*/
class tlbPipelineCoordinator : public QObject
{
    Q_OBJECT

  public:
    enum PipelineStage
    {
        STAGE_IDLE = 0,
        STAGE_ADDRESS_DECODE,
        STAGE_TLB_LOOKUP,
        STAGE_PERMISSION_CHECK,
        STAGE_COLLISION_DETECT,
        STAGE_TRANSLATION_COMPLETE,
        STAGE_STALLED,
        STAGE_REPLAY_PENDING
    };

    enum StallReason
    {
        NO_STALL = 0,
        COLLISION_STALL,
        PERMISSION_STALL,
        RESOURCE_STALL,
        DEPENDENCY_STALL,
        QUEUE_FULL_STALL
    };

    enum OperationType
    {
        OP_LOAD = 0,
        OP_STORE,
        OP_INSTRUCTION_FETCH,
        OP_PREFETCH
    };

    struct PipelineOperation
    {
        quint64 operationId;
        OperationType opType;
        quint64 virtualAddress;
        quint32 processId;
        quint32 threadId;
        PipelineStage currentStage;
        StallReason stallReason;
        quint64 entryTimestamp;
        quint64 stageStartTime;
        quint32 replayCount;
        bool isHighPriority;

        PipelineOperation()
            : operationId(0), opType(OP_LOAD), virtualAddress(0), processId(0), threadId(0), currentStage(STAGE_IDLE),
              stallReason(NO_STALL), entryTimestamp(0), stageStartTime(0), replayCount(0), isHighPriority(false)
        {
        }
    };

  private:
    static const qint32 MAX_PIPELINE_DEPTH = 8;
    static const qint32 MAX_STALL_QUEUE_SIZE = 16;
    static const qint32 MAX_REPLAY_COUNT = 3;
    static const qint32 STALL_TIMEOUT_MS = 1000;

    QQueue<PipelineOperation> m_activeOperations;
    QQueue<PipelineOperation> m_stalledOperations;
    QQueue<PipelineOperation> m_replayQueue;
    QMutex m_pipelineMutex;
    QMutex m_stallMutex;
    QMutex m_replayMutex;

    QAtomicInteger<quint64> m_operationCounter;
    QAtomicInteger<quint64> m_completedOperations;
    QAtomicInteger<quint64> m_stalledOperationsCount;
    QAtomicInteger<quint64> m_replayedOperations;
    QAtomicInteger<quint64> m_droppedOperations;
    QAtomicInteger<quint64> m_collisionStalls;
    QAtomicInteger<quint64> m_resourceStalls;

    bool m_initialized;
    bool m_pipelineEnabled;

  public:
    explicit tlbPipelineCoordinator(QObject *parent = nullptr)
        : QObject(parent), m_operationCounter(0), m_completedOperations(0), m_stalledOperations(0),
          m_replayedOperations(0), m_droppedOperations(0), m_collisionStalls(0), m_resourceStalls(0),
          m_initialized(false), m_pipelineEnabled(true)
    {
        initialize();
    }

    ~tlbPipelineCoordinator()
    {
        quint64 total = m_operationCounter.loadAcquire();
        quint64 completed = m_completedOperations.loadAcquire();
        quint64 efficiency = total > 0 ? (completed * 100) / total : 0;
        DEBUG_LOG("tlbPipelineCoordinator destroyed - Ops: %llu, Completed: %llu, Efficiency: %llu%%", total, completed,
                  efficiency);
    }

    void initialize()
    {
        if (m_initialized)
            return;

        m_activeOperations.clear();
        m_stalledOperations.clear();
        m_replayQueue.clear();

        m_initialized = true;
        DEBUG_LOG("tlbPipelineCoordinator initialized - Max depth: %d, Max stall queue: %d", MAX_PIPELINE_DEPTH,
                  MAX_STALL_QUEUE_SIZE);
    }

    void initialize_SignalsAndSlots()
    {
        // Connect pipeline stage progression signals
    }

    bool submitOperation(quint64 virtualAddress, OperationType opType, quint32 processId, quint32 threadId,
                         bool isHighPriority = false)
    {
        QMutexLocker locker(&m_pipelineMutex);

        if (!m_pipelineEnabled)
        {
            DEBUG_LOG("Pipeline disabled - rejecting operation VA=0x%llx", virtualAddress);
            return false;
        }

        if (m_activeOperations.size() >= MAX_PIPELINE_DEPTH)
        {
            m_droppedOperations.fetchAndAddAcquire(1);
            DEBUG_LOG("Pipeline full - dropping operation VA=0x%llx", virtualAddress);
            emit sigOperationDropped(virtualAddress, processId, QUEUE_FULL_STALL);
            return false;
        }

        PipelineOperation op;
        op.operationId = m_operationCounter.fetchAndAddAcquire(1);
        op.opType = opType;
        op.virtualAddress = virtualAddress;
        op.processId = processId;
        op.threadId = threadId;
        op.currentStage = STAGE_ADDRESS_DECODE;
        op.stallReason = NO_STALL;
        op.entryTimestamp = QDateTime::currentMSecsSinceEpoch();
        op.stageStartTime = op.entryTimestamp;
        op.replayCount = 0;
        op.isHighPriority = isHighPriority;

        if (isHighPriority)
        {
            // Insert high priority operations at the front
            m_activeOperations.prepend(op);
        }
        else
        {
            m_activeOperations.enqueue(op);
        }

        DEBUG_LOG("Operation submitted: ID=%llu, Type=%d, VA=0x%llx, PID=%u, Priority=%s", op.operationId, opType,
                  virtualAddress, processId, isHighPriority ? "HIGH" : "NORMAL");

        emit sigOperationSubmitted(op.operationId, virtualAddress, processId, opType);
        return true;
    }

    bool advanceStage(quint64 operationId, PipelineStage newStage)
    {
        QMutexLocker locker(&m_pipelineMutex);

        for (qint32 i = 0; i < m_activeOperations.size(); ++i)
        {
            if (m_activeOperations[i].operationId == operationId)
            {
                PipelineOperation &op = m_activeOperations[i];
                PipelineStage oldStage = op.currentStage;
                op.currentStage = newStage;
                op.stageStartTime = QDateTime::currentMSecsSinceEpoch();

                DEBUG_LOG("Stage advance: ID=%llu, %d->%d, VA=0x%llx", operationId, oldStage, newStage,
                          op.virtualAddress);

                emit sigStageAdvanced(operationId, oldStage, newStage, op.virtualAddress);

                if (newStage == STAGE_TRANSLATION_COMPLETE)
                {
                    completeOperation(i);
                }
                return true;
            }
        }
        return false;
    }

    bool stallOperation(quint64 operationId, StallReason reason)
    {
        QMutexLocker activeLock(&m_pipelineMutex);
        QMutexLocker stallLock(&m_stallMutex);

        for (qint32 i = 0; i < m_activeOperations.size(); ++i)
        {
            if (m_activeOperations[i].operationId == operationId)
            {
                PipelineOperation op = m_activeOperations.takeAt(i);
                op.currentStage = STAGE_STALLED;
                op.stallReason = reason;
                op.stageStartTime = QDateTime::currentMSecsSinceEpoch();

                if (m_stalledOperations.size() < MAX_STALL_QUEUE_SIZE)
                {
                    m_stalledOperations.enqueue(op);
                    m_stalledOperationsCount.fetchAndAddAcquire(1);

                    // Update stall type counters
                    if (reason == COLLISION_STALL)
                    {
                        m_collisionStalls.fetchAndAddAcquire(1);
                    }
                    else if (reason == RESOURCE_STALL)
                    {
                        m_resourceStalls.fetchAndAddAcquire(1);
                    }

                    DEBUG_LOG("Operation stalled: ID=%llu, Reason=%d, VA=0x%llx", operationId, reason,
                              op.virtualAddress);

                    emit sigOperationStalled(operationId, op.virtualAddress, reason);
                    return true;
                }
                else
                {
                    // Stall queue full - drop operation
                    m_droppedOperations.fetchAndAddAcquire(1);
                    DEBUG_LOG("Stall queue full - dropping operation ID=%llu", operationId);
                    emit sigOperationDropped(op.virtualAddress, op.processId, QUEUE_FULL_STALL);
                    return false;
                }
            }
        }
        return false;
    }

    bool unstallOperation(quint64 operationId)
    {
        QMutexLocker stallLock(&m_stallMutex);
        QMutexLocker activeLock(&m_pipelineMutex);

        for (qint32 i = 0; i < m_stalledOperations.size(); ++i)
        {
            if (m_stalledOperations[i].operationId == operationId)
            {
                PipelineOperation op = m_stalledOperations.takeAt(i);
                op.currentStage = op.stallReason == COLLISION_STALL ? STAGE_COLLISION_DETECT : STAGE_TLB_LOOKUP;
                op.stallReason = NO_STALL;
                op.stageStartTime = QDateTime::currentMSecsSinceEpoch();

                if (m_activeOperations.size() < MAX_PIPELINE_DEPTH)
                {
                    if (op.isHighPriority)
                    {
                        m_activeOperations.prepend(op);
                    }
                    else
                    {
                        m_activeOperations.enqueue(op);
                    }

                    DEBUG_LOG("Operation unstalled: ID=%llu, VA=0x%llx", operationId, op.virtualAddress);

                    emit sigOperationUnstalled(operationId, op.virtualAddress);
                    return true;
                }
                else
                {
                    // Pipeline full - queue for replay
                    queueForReplay(op);
                    return false;
                }
            }
        }
        return false;
    }

    void processTimeouts()
    {
        QMutexLocker stallLock(&m_stallMutex);
        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        for (qint32 i = m_stalledOperations.size() - 1; i >= 0; --i)
        {
            const PipelineOperation &op = m_stalledOperations[i];
            if ((currentTime - op.stageStartTime) > STALL_TIMEOUT_MS)
            {
                PipelineOperation timeoutOp = m_stalledOperations.takeAt(i);

                if (timeoutOp.replayCount < MAX_REPLAY_COUNT)
                {
                    queueForReplay(timeoutOp);
                    DEBUG_LOG("Operation timeout -> replay: ID=%llu, Count=%u", timeoutOp.operationId,
                              timeoutOp.replayCount);
                }
                else
                {
                    m_droppedOperations.fetchAndAddAcquire(1);
                    DEBUG_LOG("Operation timeout -> dropped: ID=%llu, Max replays exceeded", timeoutOp.operationId);
                    emit sigOperationDropped(timeoutOp.virtualAddress, timeoutOp.processId, DEPENDENCY_STALL);
                }
            }
        }
    }

    void drainPipeline()
    {
        QMutexLocker activeLock(&m_pipelineMutex);
        QMutexLocker stallLock(&m_stallMutex);
        QMutexLocker replayLock(&m_replayMutex);

        qint32 drained = m_activeOperations.size() + m_stalledOperations.size() + m_replayQueue.size();

        m_activeOperations.clear();
        m_stalledOperations.clear();
        m_replayQueue.clear();

        DEBUG_LOG("Pipeline drained: %d operations cleared", drained);
        emit sigPipelineDrained(drained);
    }

    void enablePipeline(bool enable)
    {
        QMutexLocker locker(&m_pipelineMutex);
        m_pipelineEnabled = enable;
        DEBUG_LOG("Pipeline %s", enable ? "enabled" : "disabled");
    }

    // Statistics and status methods
    qint32 getActiveOperationCount() const
    {
        QMutexLocker locker(&m_pipelineMutex);
        return m_activeOperations.size();
    }

    qint32 getStalledOperationCount() const
    {
        QMutexLocker locker(&m_stallMutex);
        return m_stalledOperations.size();
    }

    qint32 getReplayQueueSize() const
    {
        QMutexLocker locker(&m_replayMutex);
        return m_replayQueue.size();
    }

    qreal getPipelineUtilization() const
    {
        QMutexLocker locker(&m_pipelineMutex);
        return static_cast<qreal>(m_activeOperations.size()) / static_cast<qreal>(MAX_PIPELINE_DEPTH);
    }

    // Performance statistics
    quint64 getCompletedOperations() const { return m_completedOperations.loadAcquire(); }
    quint64 getStalledOperationsTotal() const { return m_stalledOperationsCount.loadAcquire(); }
    quint64 getReplayedOperations() const { return m_replayedOperations.loadAcquire(); }
    quint64 getDroppedOperations() const { return m_droppedOperations.loadAcquire(); }
    quint64 getCollisionStalls() const { return m_collisionStalls.loadAcquire(); }
    quint64 getResourceStalls() const { return m_resourceStalls.loadAcquire(); }

    void resetStatistics()
    {
        m_operationCounter.storeRelease(0);
        m_completedOperations.storeRelease(0);
        m_stalledOperationsCount.storeRelease(0);
        m_replayedOperations.storeRelease(0);
        m_droppedOperations.storeRelease(0);
        m_collisionStalls.storeRelease(0);
        m_resourceStalls.storeRelease(0);
        DEBUG_LOG("Pipeline statistics reset");
    }

  signals:
    void sigOperationSubmitted(quint64 operationId, quint64 virtualAddress, quint32 processId, OperationType opType);
    void sigStageAdvanced(quint64 operationId, PipelineStage oldStage, PipelineStage newStage, quint64 virtualAddress);
    void sigOperationStalled(quint64 operationId, quint64 virtualAddress, StallReason reason);
    void sigOperationUnstalled(quint64 operationId, quint64 virtualAddress);
    void sigOperationCompleted(quint64 operationId, quint64 virtualAddress, quint64 processingTime);
    void sigOperationDropped(quint64 virtualAddress, quint32 processId, StallReason reason);
    void sigPipelineDrained(qint32 operationCount);

  private:
    void completeOperation(qint32 index)
    {
        PipelineOperation op = m_activeOperations.takeAt(index);
        quint64 processingTime = QDateTime::currentMSecsSinceEpoch() - op.entryTimestamp;

        m_completedOperations.fetchAndAddAcquire(1);

        DEBUG_LOG("Operation completed: ID=%llu, VA=0x%llx, Time=%llu ms", op.operationId, op.virtualAddress,
                  processingTime);

        emit sigOperationCompleted(op.operationId, op.virtualAddress, processingTime);
    }

    void queueForReplay(PipelineOperation &op)
    {
        QMutexLocker replayLock(&m_replayMutex);

        op.replayCount++;
        op.currentStage = STAGE_REPLAY_PENDING;
        op.stageStartTime = QDateTime::currentMSecsSinceEpoch();

        m_replayQueue.enqueue(op);
        m_replayedOperations.fetchAndAddAcquire(1);

        DEBUG_LOG("Operation queued for replay: ID=%llu, Count=%u", op.operationId, op.replayCount);
    }
};