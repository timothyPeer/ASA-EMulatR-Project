#pragma once
#include "executorAlphaFloatingPoint.h"
#include "opcode11_executorAlphaIntegerLogical.h"
#include "opcode14_executorAlphaSQRT.h"

#include "AlphaTranslationCache.h"
#include "decodedInstruction.h"
#include <QScopedPointer>
#include "UnifiedDataCache.h"
#include "AlphaBarrierExecutor.h"



// ========== INTEGRATION EXAMPLE ==========

/**
 * Example of how to integrate AlphaBarrierExecutor with other execution units
 */
class AlphaUnifiedExecutionEngine : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;
    executorAlphaFloatingPoint *m_fpExecutor;
    opcode11_executorAlphaIntegerLogical *m_intExecutor;
    opcode14_executorAlphaSQRT *m_sqrtExecutor;
    AlphaBarrierExecutor *m_barrierExecutor;

    // Shared resources
    QSharedPointer<AlphaTranslationCache> m_iTLB, m_dTLB;
    QSharedPointer<UnifiedDataCache> m_l1Cache, m_l2Cache, m_l3Cache;

  public:
    AlphaUnifiedExecutionEngine(AlphaCPU *cpu) : m_cpu(cpu)
    {
        // Initialize TLBs
        m_iTLB = QSharedPointer<AlphaTranslationCache>::create(512, 8, 8192);
        m_dTLB = QSharedPointer<AlphaTranslationCache>::create(512, 8, 8192);

        // Initialize cache hierarchy
        m_l1Cache = QSharedPointer<UnifiedDataCache>::create(32768, 4, 64);
        m_l2Cache = QSharedPointer<UnifiedDataCache>::create(262144, 8, 64);
        m_l3Cache = QSharedPointer<UnifiedDataCache>::create(8388608, 16, 64);

        // Create execution units
        m_fpExecutor = new executorAlphaFloatingPoint(m_cpu, this);
        m_intExecutor = new opcode11_executorAlphaIntegerLogical(m_cpu, this);
        m_sqrtExecutor = new opcode14_executorAlphaSQRT(m_cpu, this);
        m_barrierExecutor = new AlphaBarrierExecutor(m_cpu, this);

        // Attach shared resources to all executors
        attachSharedResources();

        // Register execution units with barrier executor
        m_barrierExecutor->registerFloatingPointExecutor(m_fpExecutor);
        m_barrierExecutor->registerIntegerExecutor(m_intExecutor);
        m_barrierExecutor->registerSQRTExecutor(m_sqrtExecutor);

        // Connect memory operation notifications
        connectMemoryOperationSignals();

        // Start all execution pipelines
        startAllPipelines();
    }

    ~AlphaUnifiedExecutionEngine() { stopAllPipelines(); }

    void executeInstruction(const DecodedInstruction &instr, quint64 pc)
    {
        quint32 opcode = (instr.raw >> 26) & 0x3F;

        // Check if this is a barrier instruction
        if (opcode == 0x18)
        { // MISC opcode for barriers
            quint32 function = (instr.raw >> 5) & 0x7FFF;
            if (function == FUNC_TRAPB || function == FUNC_MB || function == FUNC_WMB)
            {
                m_barrierExecutor->submitBarrier(instr, pc);
                return;
            }
        }

        // Route to appropriate execution unit
        switch (opcode)
        {
        case 0x11:
        case 0x12:
        case 0x13: // Integer logical/shift/multiply
            // Notify barrier executor of memory operation
            m_barrierExecutor->notifyMemoryOperation(false);
            m_intExecutor->submitInstruction(instr, pc);
            break;

        case 0x14: // SQRT operations
            m_barrierExecutor->notifyMemoryOperation(false);
            m_sqrtExecutor->submitInstruction(instr, pc);
            break;

        case 0x17: // Floating-point operations
            m_barrierExecutor->notifyMemoryOperation(false);
            m_fpExecutor->submitInstruction(instr, pc);
            break;

        default:
            qWarning() << "Unknown opcode:" << Qt::hex << opcode;
            break;
        }
    }

  private:
    void attachSharedResources()
    {
        // Attach TLBs to all executors
        m_fpExecutor->attachTranslationCache(m_iTLB, m_dTLB);
        m_intExecutor->attachTranslationCache(m_iTLB, m_dTLB);
        m_sqrtExecutor->attachTranslationCache(m_iTLB, m_dTLB);
        m_barrierExecutor->attachTranslationCache(m_iTLB, m_dTLB);

        // Attach cache hierarchy to all executors
        auto attachCaches = [&](auto executor)
        {
            executor->attachLevel1DataCache(m_l1Cache);
            executor->attachLevel2Cache(m_l2Cache);
            executor->attachLevel3Cache(m_l3Cache);
        };

        attachCaches(m_fpExecutor);
        attachCaches(m_intExecutor);
        attachCaches(m_sqrtExecutor);
        attachCaches(m_barrierExecutor);
    }

    void connectMemoryOperationSignals()
    {
        // Connect completion signals to barrier executor
        connect(m_fpExecutor, &executorAlphaFloatingPoint::fpInstructionExecuted,
                [this](quint32, bool) { m_barrierExecutor->notifyMemoryOperationComplete(false); });

        connect(m_intExecutor, &opcode11_executorAlphaIntegerLogical::intInstructionExecuted,
                [this](quint32, quint32, bool) { m_barrierExecutor->notifyMemoryOperationComplete(false); });

        connect(m_sqrtExecutor, &opcode14_executorAlphaSQRT::sqrtInstructionExecuted,
                [this](quint32, int, bool) { m_barrierExecutor->notifyMemoryOperationComplete(false); });

        // Connect exception signals
        connect(m_fpExecutor, &executorAlphaFloatingPoint::fpExceptionRaised,
                [this](quint32, quint64) { m_barrierExecutor->notifyExceptionPending(); });

        connect(m_sqrtExecutor, &opcode14_executorAlphaSQRT::sqrtExceptionRaised,
                [this](quint32, quint64) { m_barrierExecutor->notifyExceptionPending(); });
    }

    void startAllPipelines()
    {
        m_fpExecutor->startAsyncPipeline();
        m_intExecutor->startAsyncPipeline();
        m_sqrtExecutor->startAsyncPipeline();
        m_barrierExecutor->startBarrierProcessor();

        qDebug() << "All Alpha execution pipelines started";
    }

    void stopAllPipelines()
    {
        m_barrierExecutor->stopBarrierProcessor();
        m_sqrtExecutor->stopAsyncPipeline();
        m_intExecutor->stopAsyncPipeline();
        m_fpExecutor->stopAsyncPipeline();

        qDebug() << "All Alpha execution pipelines stopped";
    }

  public slots:
    void printExecutionStatistics()
    {
        qDebug() << "\n=== UNIFIED ALPHA EXECUTION ENGINE STATISTICS ===";
        m_fpExecutor->printStatistics();
        qDebug() << "";
        m_intExecutor->printStatistics();
        qDebug() << "";
        m_sqrtExecutor->printAdvancedStatistics();
        qDebug() << "";
        m_barrierExecutor->printStatistics();
        qDebug() << "\n=== TLB STATISTICS ===";
        auto iTLBStats = m_iTLB->getStatistics();
        auto dTLBStats = m_dTLB->getStatistics();
        qDebug() << "I-TLB Hit Rate:" << iTLBStats.hitRate() * 100.0 << "%";
        qDebug() << "D-TLB Hit Rate:" << dTLBStats.hitRate() * 100.0 << "%";
    }

    void autoTuneAllSystems()
    {
        m_iTLB->autoTune();
        m_dTLB->autoTune();
        // Additional auto-tuning could be added here
        qDebug() << "All systems auto-tuned";
    }
};