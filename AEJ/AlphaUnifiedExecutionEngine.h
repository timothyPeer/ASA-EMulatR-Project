#pragma once
// AlphaUnifiedExecutionEngine.cpp - Complete execution engine with PAL support

#include "AlphaBarrierExecutor.h"
#include "AlphaCPU_refactored.h"
#include "executorAlphaFloatingPoint.h"
#include "opcode11_executorAlphaIntegerLogical.h"
#include "executorAlphaPAL.h"
#include "AlphaUnifiedExecutionEngine.h"
#include "DecodedInstruction.h"
#include <QDebug>
#include <QMutexLocker>
#include "structures/utilitySafeIncrement.h"

/**
 * @brief Complete Alpha execution engine with OpCode 0 (PAL) support
 *
 * This unified engine coordinates all execution units and provides
 * high-performance instruction dispatch with cache optimization.
 */
class AlphaUnifiedExecutionEngine : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;

    // Execution units
    std::unique_ptr<executorAlphaPAL> m_palExecutor;
    std::unique_ptr<executorAlphaFloatingPoint> m_fpExecutor;
    std::unique_ptr<opcode11_executorAlphaIntegerLogical> m_intExecutor;
    std::unique_ptr<AlphaBarrierExecutor> m_barrierExecutor;

    // Performance tracking
    mutable QMutex m_statsMutex;
    quint64 m_totalInstructions = 0;
    quint64 m_palInstructions = 0;
    quint64 m_fpInstructions = 0;
    quint64 m_intInstructions = 0;
    quint64 m_barrierInstructions = 0;
    quint64 m_unknownInstructions = 0;

    // JIT optimization
    QMap<quint32, quint64> m_opcodeFrequency;
    QSet<quint32> m_hotOpcodes;

  public:
    explicit AlphaUnifiedExecutionEngine(AlphaCPU *cpu, QObject *parent = nullptr) : QObject(parent), m_cpu(cpu)
    {
        initializeExecutionUnits();
        connectExecutionUnits();
        startAsyncPipelines();

        qDebug() << "Alpha Unified Execution Engine initialized";
    }

    ~AlphaUnifiedExecutionEngine()
    {
        stopAsyncPipelines();
        qDebug() << "Alpha Unified Execution Engine shutdown";
    }

    /**
     * @brief Main instruction execution entry point
     */
    bool executeInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        QMutexLocker locker(&m_statsMutex);
        m_totalInstructions++;

        // Extract opcode
        quint32 opcode = (instruction.raw >> 26) & 0x3F;

        // Update JIT statistics
        updateJITStats(opcode);

        locker.unlock();

        // Dispatch to appropriate execution unit
        switch (opcode)
        {
        case 0x00: // PAL instructions
            return executePALInstruction(instruction, pc);

        case 0x11: // Integer logical
        case 0x12: // Shift/ZAP
        case 0x13: // Integer multiply
            return executeIntegerInstruction(instruction, pc);

        case 0x17: // Floating-point
            return executeFloatingPointInstruction(instruction, pc);

        case 0x18: // Memory barriers
            return executeBarrierInstruction(instruction, pc);

        // Add more opcodes as needed
        case 0x08: // LDA
        case 0x09: // LDAH
        case 0x28: // LDL
        case 0x29: // LDQ
        case 0x2C: // STL
        case 0x2D: // STQ
            return executeMemoryInstruction(instruction, pc);

        case 0x30: // BR
        case 0x34: // BSR
        case 0x38: // BLBC
        case 0x39: // BEQ
        case 0x3A: // BLT
        case 0x3B: // BLE
        case 0x3C: // BLBS
        case 0x3D: // BNE
        case 0x3E: // BGE
        case 0x3F: // BGT
            return executeBranchInstruction(instruction, pc);

        default:
        {
            QMutexLocker locker(&m_statsMutex);
            m_unknownInstructions++;
        }
            qWarning() << "Unknown opcode:" << Qt::hex << opcode;
            return false;
        }
    }

    /**
     * @brief Execute PAL instruction (OpCode 0)
     */
    bool executePALInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        QMutexLocker locker(&m_statsMutex);
        m_palInstructions++;
        locker.unlock();

        if (!m_palExecutor)
        {
            qWarning() << "PAL executor not available";
            return false;
        }

        // For critical PAL operations, use synchronous execution
        quint32 function = (instruction.raw >> 5) & 0x3FFFFFF;
        PALFunctionClass classification = classifyPALFunction(function);

        if (classification == PALFunctionClass::SYSTEM_CALL || classification == PALFunctionClass::CONTEXT_SWITCH ||
            classification == PALFunctionClass::INTERRUPT_HANDLING)
        {
            // Synchronous execution for critical operations
            return m_palExecutor->executePALInstruction(instruction);
        }
        else
        {
            // Async execution for non-critical operations
            return m_palExecutor->submitInstruction(instruction, pc);
        }
    }

    /**
     * @brief Execute integer instruction
     */
    bool executeIntegerInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        QMutexLocker locker(&m_statsMutex);
        asa_utils::safeIncrement(m_intInstructions);
        locker.unlock();

        if (!m_intExecutor)
        {
            qWarning() << "Integer executor not available";
            return false;
        }

        // Most integer operations can be async
        if (m_intExecutor->isAsyncPipelineActive())
        {
            return m_intExecutor->submitInstruction(instruction, pc);
        }
        else
        {
            // Fallback to synchronous execution
            quint32 opcode = (instruction.raw >> 26) & 0x3F;
            switch (opcode)
            {
            case 0x11:
                return m_intExecutor->executeIntegerLogical(instruction);
            case 0x12:
                return m_intExecutor->executeShiftZap(instruction);
            case 0x13:
                return m_intExecutor->executeIntegerMultiply(instruction);
            default:
                return false;
            }
        }
    }

    /**
     * @brief Execute floating-point instruction
     */
    bool executeFloatingPointInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        QMutexLocker locker(&m_statsMutex);
        m_fpInstructions++;
        locker.unlock();

        if (!m_fpExecutor)
        {
            qWarning() << "FP executor not available";
            return false;
        }

        // Floating-point operations are typically async
        if (m_fpExecutor->isAsyncPipelineActive())
        {
            return m_fpExecutor->submitInstruction(instruction, pc);
        }
        else
        {
            // Fallback to synchronous execution
            return m_fpExecutor->executeFLTLFunction(instruction);
        }
    }

    /**
     * @brief Execute barrier instruction
     */
    bool executeBarrierInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        QMutexLocker locker(&m_statsMutex);
        m_barrierInstructions++;
        locker.unlock();

        if (!m_barrierExecutor)
        {
            qWarning() << "Barrier executor not available";
            return false;
        }

        // Barriers are typically synchronous for ordering guarantees
        return m_barrierExecutor->executeBarrier(instruction);
    }

    /**
     * @brief Execute memory instruction (load/store)
     */
    bool executeMemoryInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        if (!m_cpu)
            return false;

        quint32 opcode = (instruction.raw >> 26) & 0x3F;
        quint8 ra = (instruction.raw >> 21) & 0x1F;
        quint8 rb = (instruction.raw >> 16) & 0x1F;
        qint16 displacement = static_cast<qint16>(instruction.raw & 0xFFFF);

        // Calculate effective address
        quint64 baseAddr = (rb != 31) ? m_cpu->getIntegerRegister(rb) : 0;
        quint64 effectiveAddr = baseAddr + static_cast<qint64>(displacement);

        switch (opcode)
        {
        case 0x08: // LDA - Load Address
            if (ra != 31)
            {
                m_cpu->setIntegerRegister(ra, effectiveAddr);
            }
            return true;

        case 0x09: // LDAH - Load Address High
            if (ra != 31)
            {
                quint64 value = effectiveAddr << 16;
                m_cpu->setIntegerRegister(ra, value);
            }
            return true;

        case 0x28: // LDL - Load Longword
        {
            quint32 value;
            if (m_cpu->readMemory(effectiveAddr, reinterpret_cast<quint8 *>(&value), 4))
            {
                if (ra != 31)
                {
                    // Sign extend 32-bit to 64-bit
                    m_cpu->setIntegerRegister(ra, static_cast<qint64>(static_cast<qint32>(value)));
                }
                return true;
            }
            return false;
        }

        case 0x29: // LDQ - Load Quadword
        {
            quint64 value;
            if (m_cpu->readMemory(effectiveAddr, reinterpret_cast<quint8 *>(&value), 8))
            {
                if (ra != 31)
                {
                    m_cpu->setIntegerRegister(ra, value);
                }
                return true;
            }
            return false;
        }

        case 0x2C: // STL - Store Longword
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            quint32 value = static_cast<quint32>(regValue & 0xFFFFFFFF);
            return m_cpu->writeMemory(effectiveAddr, reinterpret_cast<quint8 *>(&value), 4);
        }

        case 0x2D: // STQ - Store Quadword
        {
            quint64 value = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            return m_cpu->writeMemory(effectiveAddr, reinterpret_cast<quint8 *>(&value), 8);
        }

        default:
            return false;
        }
    }

    /**
     * @brief Execute branch instruction
     */
    bool executeBranchInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        if (!m_cpu)
            return false;

        quint32 opcode = (instruction.raw >> 26) & 0x3F;
        quint8 ra = (instruction.raw >> 21) & 0x1F;
        qint32 displacement = static_cast<qint32>((instruction.raw & 0x1FFFFF) << 11) >> 11; // Sign extend 21-bit

        quint64 targetAddr = pc + 4 + (displacement * 4);
        bool takeBranch = false;

        switch (opcode)
        {
        case 0x30: // BR - Unconditional branch
            takeBranch = true;
            if (ra != 31)
            {
                m_cpu->setIntegerRegister(ra, pc + 4); // Return address
            }
            break;

        case 0x34: // BSR - Branch to subroutine
            takeBranch = true;
            if (ra != 31)
            {
                m_cpu->setIntegerRegister(ra, pc + 4); // Return address
            }
            break;

        case 0x38: // BLBC - Branch if low bit clear
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            takeBranch = (regValue & 1) == 0;
        }
        break;

        case 0x39: // BEQ - Branch if equal to zero
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            takeBranch = (regValue == 0);
        }
        break;

        case 0x3A: // BLT - Branch if less than zero
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            qint64 signedValue = static_cast<qint64>(regValue);
            takeBranch = (signedValue < 0);
        }
        break;

        case 0x3B: // BLE - Branch if less than or equal to zero
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            qint64 signedValue = static_cast<qint64>(regValue);
            takeBranch = (signedValue <= 0);
        }
        break;

        case 0x3C: // BLBS - Branch if low bit set
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            takeBranch = (regValue & 1) != 0;
        }
        break;

        case 0x3D: // BNE - Branch if not equal to zero
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            takeBranch = (regValue != 0);
        }
        break;

        case 0x3E: // BGE - Branch if greater than or equal to zero
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            qint64 signedValue = static_cast<qint64>(regValue);
            takeBranch = (signedValue >= 0);
        }
        break;

        case 0x3F: // BGT - Branch if greater than zero
        {
            quint64 regValue = (ra != 31) ? m_cpu->getIntegerRegister(ra) : 0;
            qint64 signedValue = static_cast<qint64>(regValue);
            takeBranch = (signedValue > 0);
        }
        break;

        default:
            return false;
        }

        if (takeBranch)
        {
            m_cpu->setPC(targetAddr);
        }

        return true;
    }

    /**
     * @brief Print comprehensive execution statistics
     */
    void printExecutionStatistics() const
    {
        QMutexLocker locker(&m_statsMutex);

        qDebug() << "=== Alpha Unified Execution Engine Statistics ===";
        qDebug() << "Total Instructions:" << m_totalInstructions;
        qDebug() << "PAL Instructions:" << m_palInstructions;
        qDebug() << "Integer Instructions:" << m_intInstructions;
        qDebug() << "Floating-Point Instructions:" << m_fpInstructions;
        qDebug() << "Barrier Instructions:" << m_barrierInstructions;
        qDebug() << "Unknown Instructions:" << m_unknownInstructions;

        if (m_totalInstructions > 0)
        {
            qDebug() << "\nInstruction Mix:";
            qDebug() << QString("  PAL: %1%").arg((m_palInstructions * 100.0) / m_totalInstructions, 0, 'f', 2);
            qDebug() << QString("  Integer: %1%").arg((m_intInstructions * 100.0) / m_totalInstructions, 0, 'f', 2);
            qDebug() << QString("  FP: %1%").arg((m_fpInstructions * 100.0) / m_totalInstructions, 0, 'f', 2);
            qDebug() << QString("  Barrier: %1%").arg((m_barrierInstructions * 100.0) / m_totalInstructions, 0, 'f', 2);
        }

        qDebug() << "\nHot Opcodes:";
        for (quint32 opcode : m_hotOpcodes)
        {
            quint64 count = m_opcodeFrequency.value(opcode, 0);
            qDebug() << QString("  0x%1: %2 times").arg(opcode, 2, 16, QChar('0')).arg(count);
        }

        // Print individual executor statistics
        if (m_palExecutor)
        {
            qDebug() << "\n--- PAL Executor Statistics ---";
            m_palExecutor->printStatistics();
        }

        if (m_fpExecutor)
        {
            qDebug() << "\n--- FP Executor Statistics ---";
            m_fpExecutor->printStatistics();
        }

        if (m_intExecutor)
        {
            qDebug() << "\n--- Integer Executor Statistics ---";
            m_intExecutor->printStatistics();
        }

        if (m_barrierExecutor)
        {
            qDebug() << "\n--- Barrier Executor Statistics ---";
            m_barrierExecutor->printStatistics();
        }
    }

    /**
     * @brief Clear all statistics
     */
    void clearStatistics()
    {
        QMutexLocker locker(&m_statsMutex);

        m_totalInstructions = 0;
        m_palInstructions = 0;
        m_fpInstructions = 0;
        m_intInstructions = 0;
        m_barrierInstructions = 0;
        m_unknownInstructions = 0;

        m_opcodeFrequency.clear();
        m_hotOpcodes.clear();

        // Clear individual executor statistics
        if (m_palExecutor)
            m_palExecutor->clearStatistics();
        if (m_fpExecutor)
            m_fpExecutor->clearStatistics();
        if (m_intExecutor)
            m_intExecutor->clearStatistics();
        if (m_barrierExecutor)
            m_barrierExecutor->clearStatistics();
    }

  signals:
    void instructionExecuted(quint32 opcode, bool success);
    void executionError(const QString &error);

  private:
    /**
     * @brief Initialize all execution units
     */
    void initializeExecutionUnits()
    {
        // Create PAL executor
        m_palExecutor = std::make_unique<executorAlphaPAL>(m_cpu, this);

        // Create floating-point executor
        m_fpExecutor = std::make_unique<executorAlphaFloatingPoint>(m_cpu, this);

        // Create integer executor
        m_intExecutor = std::make_unique<opcode11_executorAlphaIntegerLogical>(m_cpu, this);

        // Create barrier executor
        m_barrierExecutor = std::make_unique<AlphaBarrierExecutor>(m_cpu, this);

        qDebug() << "All execution units initialized";
    }

    /**
     * @brief Connect execution units for coordination
     */
    void connectExecutionUnits()
    {
        // Connect PAL executor to other units
        m_palExecutor->attachBarrierExecutor(m_barrierExecutor.get());
        m_palExecutor->attachFloatingPointExecutor(m_fpExecutor.get());
        m_palExecutor->attachIntegerExecutor(m_intExecutor.get());

        // Connect barrier executor to other units
        m_barrierExecutor->registerFloatingPointExecutor(m_fpExecutor.get());
        m_barrierExecutor->registerIntegerExecutor(m_intExecutor.get());

        // Connect signals for coordination
        connect(m_palExecutor.get(), &executorAlphaPAL::cacheFlushRequested, m_barrierExecutor.get(),
                &AlphaBarrierExecutor::requestCacheFlush);

        connect(m_barrierExecutor.get(), &AlphaBarrierExecutor::memoryOrderingEnforced, this,
                [this](const QString &type) { qDebug() << "Memory ordering enforced:" << type; });

        // Connect instruction completion signals
        connect(m_palExecutor.get(), &executorAlphaPAL::palInstructionExecuted, this,
                [this](quint32 function, bool success, int cycles)
                {
                    emit instructionExecuted(0x00, success); // OpCode 0 for PAL
                });

        connect(m_fpExecutor.get(), &executorAlphaFloatingPoint::fpInstructionExecuted, this,
                [this](quint32 function, bool success)
                {
                    emit instructionExecuted(0x17, success); // OpCode 0x17 for FP
                });

        connect(m_intExecutor.get(), &opcode11_executorAlphaIntegerLogical::intInstructionExecuted, this,
                [this](quint32 opcode, quint32 function, bool success) { emit instructionExecuted(opcode, success); });

        qDebug() << "Execution units connected";
    }

    /**
     * @brief Start async pipelines for all units
     */
    void startAsyncPipelines()
    {
        if (m_palExecutor)
        {
            m_palExecutor->startAsyncPipeline();
        }

        if (m_fpExecutor)
        {
            m_fpExecutor->startAsyncPipeline();
        }

        if (m_intExecutor)
        {
            m_intExecutor->startAsyncPipeline();
        }

        if (m_barrierExecutor)
        {
            m_barrierExecutor->startBarrierProcessor();
        }

        qDebug() << "All async pipelines started";
    }

    /**
     * @brief Stop async pipelines for all units
     */
    void stopAsyncPipelines()
    {
        if (m_palExecutor)
        {
            m_palExecutor->stopAsyncPipeline();
        }

        if (m_fpExecutor)
        {
            m_fpExecutor->stopAsyncPipeline();
        }

        if (m_intExecutor)
        {
            m_intExecutor->stopAsyncPipeline();
        }

        if (m_barrierExecutor)
        {
            m_barrierExecutor->stopBarrierProcessor();
        }

        qDebug() << "All async pipelines stopped";
    }

    /**
     * @brief Update JIT optimization statistics
     */
    void updateJITStats(quint32 opcode)
    {
        m_opcodeFrequency[opcode]++;

        // Mark as hot if executed more than 1000 times
        if (m_opcodeFrequency[opcode] > 1000)
        {
            m_hotOpcodes.insert(opcode);
        }
    }
};