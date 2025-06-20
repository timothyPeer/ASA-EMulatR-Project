#ifndef executeStage_h__
#define executeStage_h__

#pragma once


#include "decodeStage.h"
#include "decodedInstruction.h"
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include "..\AEJ\constants\constFunctionIEEE.h"
#include "..\AEJ\constants\constFunctionFloatingPoint.h"
#include "..\AEJ\constants\constFunctionSQRT.h"
#include "..\AEJ\constants\constFunctionIntegerLogicalBitManipulation.h"
#include "..\AEJ\constants\constFunctionJumpInstructions.h"
#include "..\AEJ\constants\constFunctionMemoryBarrior.h"
#include "..\AEJ\constants\constFunctionMiscInstructions.h"
#include "..\AEJ\constants\constFunctionMoveInstructions.h"
#include "..\AEJ\constants\constPALCacheControl.h"
#include "..\AEJ\constants\constPALMemoryBarrier.h"
#include "..\AEJ\constants\constDefMasks.h"
#include "..\AEJ\constants\constHardware.h"
#include "..\AEJ\constants\constInternalProcessorRegister.h"
#include "..\AEJ\constants\constOpCodeBranchFormat.h"
#include "..\AEJ\constants\constOpCodeInstructions.h"
#include "..\AEJ\constants\constOpCodeIntegerFormat.h"
#include "..\AEJ\constants\constOpCodeMasks.h"
#include "..\AEJ\constants\constOpCodeMemoryFormat.h"
#include "..\AEJ\constants\constOpCodeOperateFormat.h"
#include "..\AEJ\constants\constVaxTypes.h"
#include "..\AEJ\constants\constStatusRegister.h"
#include "..\AEJ\constants\constVector.h"
#include "..\AEJ\JitPALConstants.h"
#include "ExecuteStageLogHelpers.h"
#include "AlphaCPU_refactored.h"
#include <QtGlobal>
#include "enumerations\enumExceptionTypeArithmetic.h"
#include "enumerations\enumCpuModel.h"
#include "enumerations\enumTLBException.h"
#include "GlobalMacro.h"
#include "enumInstructionPerformance.h"
#include "enumerations\enumSecurityViolationType.h"
#include "traps\trapFpType.h"


class AlphaCPU;
class AlphaMemorySystem;
class TLBSystem;
struct TLBStatistics;

/**
 * @class ExecuteStage
 * @brief Instruction execution pipeline stage
 */
class ExecuteStage : public QObject
{
    Q_OBJECT

  private:
    AlphaCPU *m_cpu;
    AlphaMemorySystem* m_memorySystem;

    QQueue<DecodedInstruction> m_instructionQueue;

    // Pipeline state
    quint32 m_pipelineDepth = 4;
    quint32 m_stallCycles = 0;
    QQueue<DecodedInstruction> m_pipelineStages;

    bool m_busy = false;

    // Statistics
    mutable QMutex m_statsMutex;
    quint64 m_totalInstructions = 0;
    quint64 m_integerInstructions = 0;
    quint64 m_floatingPointInstructions = 0;
    quint64 m_memoryInstructions = 0;
    quint64 m_branchInstructions = 0;
    quint64 m_branchMispredictions = 0;
    quint64 m_palInstructions = 0;
    quint64 m_executionCycles = 0;
    quint64 m_stallCyclesTotal = 0;
    //branch statistics
    quint64 m_unconditionalBranches = 0;
    quint64 m_integerConditionalBranches = 0;
    quint64 m_integerBranchesTaken = 0;
    quint64 m_bitTestBranches = 0;
    quint64 m_bitTestBranchesTaken = 0;
    quint64 m_floatingPointBranches = 0;
    quint64 m_floatingPointBranchesTaken = 0;

// Memory operation statistics
    quint64 m_integerLoads = 0;
    quint64 m_integerStores = 0;
    quint64 m_floatingPointLoads = 0;
    quint64 m_floatingPointStores = 0;
    quint64 m_addressCalculations = 0;
    quint64 m_unalignedAccesses = 0;
    quint64 m_lockedOperations = 0;
    quint64 m_conditionalStores = 0;
    quint64 m_conditionalStoreSuccesses = 0;


    quint64 m_currentPC = 0;

  public:
    
      
      
   explicit ExecuteStage(QObject *parent = nullptr) : QObject(parent) {}

    void attachAlphaCPU(AlphaCPU *cpu_) { m_cpu = cpu_; }
    void attachAlphaMemorySystem(AlphaMemorySystem* memSys) { m_memorySystem = memSys; }
    /**
     * @brief Execute decoded instruction
     * @param instruction Decoded instruction to execute
     */

    void execute(const DecodedInstruction &instruction)
    {
        if (!instruction.valid)
        {
            DEBUG_LOG("ExecuteStage: Attempted to execute invalid instruction");
            emit sigExecutionError("Invalid instruction");
            return;
        }

        m_busy = true;

        DEBUG_LOG(QString("ExecuteStage: Executing instruction 0x%1 (opcode=0x%2)")
                      .arg(instruction.rawInstruction, 8, 16, QChar('0'))
                      .arg(instruction.opcode, 2, 16, QChar('0')));

        // Check if pipeline is stalled
        if (m_stallCycles > 0)
        {
            m_stallCycles--;
            m_busy = false;
            return;
        }

        // Add instruction to pipeline
        if (m_pipelineStages.size() >= m_pipelineDepth)
        {
            // Pipeline is full, we need to wait
            DEBUG_LOG("ExecuteStage: Pipeline full, queueing instruction");
            m_instructionQueue.enqueue(instruction);
            m_busy = false;
            return;
        }

        m_pipelineStages.enqueue(instruction);
        try
        {
            switch (instruction.opcode)
            {
            // Operate instructions
            case OPCODE_INTA: // INTA - Integer arithmetic
            case OPCODE_INTL: // INTL - Integer logical
            case OPCODE_INTS: // INTS - Integer shift
            case OPCODE_INTM: // INTM - Integer multiply
            {
                executeIntegerGroup(instruction);
                break;
            }

            // ─────────────────────── Floating Point Operations (Consolidated) ───────────────────────
            case OPCODE_ITFP: // 0x14 - Integer to Floating Point
            case OPCODE_FLTV: // 0x15 - VAX Floating Point
            case OPCODE_FLTI: // 0x16 - IEEE Floating Point
            case OPCODE_FLTL: // 0x17 - Floating Point Function
            {
                executeFloatingPointGroup(instruction);
                break;
            }
            // ─────────────────────── PAL Operations (Consolidated) ───────────────────────
            case OPCODE_PAL: // 0x00 - Privileged Architecture Library
            {
                executePALGroup(instruction);
                break;
            }

            // ─────────────────────── Branch Operations (Consolidated) ───────────────────────
            case OPCODE_BR:   // 0x30 - Branch
            case OPCODE_BSR:  // 0x34 - Branch to Subroutine
            case OPCODE_BEQ:  // 0x39 - Branch if Equal
            case OPCODE_BNE:  // 0x3D - Branch if Not Equal
            case OPCODE_BLT:  // 0x3A - Branch if Less Than
            case OPCODE_BGE:  // 0x3E - Branch if Greater Equal
            case OPCODE_BLE:  // 0x3B - Branch if Less Equal
            case OPCODE_BGT:  // 0x3F - Branch if Greater Than
            case OPCODE_BLBC: // 0x38 - Branch if Low Bit Clear
            case OPCODE_BLBS: // 0x3C - Branch if Low Bit Set
            case OPCODE_FBEQ: // 0x31 - Floating Branch if Equal
            case OPCODE_FBNE: // 0x35 - Floating Branch if Not Equal
            case OPCODE_FBLT: // 0x32 - Floating Branch if Less Than
            case OPCODE_FBGE: // 0x36 - Floating Branch if Greater Equal
            case OPCODE_FBLE: // 0x33 - Floating Branch if Less Equal
            case OPCODE_FBGT: // 0x37 - Floating Branch if Greater Than
            {
                executeBranchGroup(instruction);
                break;
            }
            // Memory instructions
            case OPCODE_LDA:  // LDA - Load Address
            case OPCODE_LDAH: // LDAH - Load Address High
            case OPCODE_LDBU:  // LDBU - Load Byte Unsigned
            case OPCODE_LDQ_U: // LDQ_U - Load Quadword Unaligned
            case OPCODE_LDWU:  // LDWU - Load Word Unsigned
            case OPCODE_STW:   // STW - Store Word
            case OPCODE_STB:   // STB - Store Byte
            case OPCODE_STQ_U: // STQ_U - Store Quadword Unaligned
            case OPCODE_LDF: // LDF - Load F_floating
            case OPCODE_LDG: // LDG - Load G_floating
            case OPCODE_LDS: // LDS - Load S_floating
            case OPCODE_LDT: // LDT - Load T_floating
            case OPCODE_STF: // STF - Store F_floating
            case OPCODE_STG: // STG - Store G_floating
            case OPCODE_STS: // STS - Store S_floating
            case OPCODE_STT: // STT - Store T_floating
            case OPCODE_LDL:   // LDL - Load Longword
            case OPCODE_LDQ:   // LDQ - Load Quadword
            case OPCODE_LDL_L: // LDL_L - Load Longword Locked
            case OPCODE_LDQ_L: // LDQ_L - Load Quadword Locked
            case OPCODE_STL:   // STL - Store Longword
            case OPCODE_STQ:   // STQ - Store Quadword
            case OPCODE_STL_C: // STL_C - Store Longword Conditional
            case OPCODE_STQ_C: // STQ_C - Store Quadword Conditional
            {
                executeMemoryGroup(instruction);
                break;
            }

            // Jump instructions
            case OPCODE_JSR:
            {
                executeJump(instruction);
                break;
            }

            case OPCODE_MISC: // 0x18 - Miscellaneous operations
            {
                executeMiscGroup(instruction);
                break;
            }
                // Hardware Operations (0x19, 0x1B-0x1F) 
            case OPCODE_HW_MFPR:
            case OPCODE_HW_LD:
            case OPCODE_HW_MTPR:
            case OPCODE_HW_REI:
            case OPCODE_HW_ST:
            case OPCODE_HW_ST_C:
                executeHardwareGroup(instruction);
                break;

            default:
            {
                DEBUG_LOG(QString("ExecuteStage: Unknown opcode 0x%1").arg(instruction.opcode, 2, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                break;
            }
            }

            // Update statistics
            updateStatistics(instruction);

            // Emit signal
            emit sigInstructionExecuted(instruction);
        }
        catch (const std::exception &e)
        {
            DEBUG_LOG(QString("ExecuteStage: Exception during execution - %1").arg(e.what()));
            emit sigExecutionError(QString("Execution exception: %1").arg(e.what()));
            m_cpu->triggerException(ExceptionType::GENERAL_EXCEPTION, m_cpu->getPc());
        }
        m_executionCycles++;
        // m_busy = false;

        // Check if there are queued instructions to process
        if (!m_instructionQueue.isEmpty() && m_pipelineStages.size() < m_pipelineDepth)
        {
            DecodedInstruction nextInstruction = m_instructionQueue.dequeue();
            execute(nextInstruction);
        }
    }
           

    /**
     * @brief Execute implementation-specific PAL functions
     * @param palFunction The PAL function code
     * @param ra Register A field
     * @param rb Register B field
     * @param rc Register C field
     */
    void executeImplementationSpecificPAL(quint32 palFunction, quint8 ra, quint8 rb, quint8 rc)
    {
        // Handle processor-specific PAL functions based on CPU model
        CpuModel cpuModel = m_cpu->getCpuModel();

        switch (cpuModel)
        {
        case CpuModel::CPU_EV4:
        case CpuModel::CPU_EV5:
            executeEV4_EV5_SpecificPAL(palFunction, ra, rb, rc);
            break;

        case CpuModel::CPU_EV6:
        case CpuModel::CPU_EV67:
        case CpuModel::CPU_EV68:
            executeEV6_SpecificPAL(palFunction, ra, rb, rc);
            break;

        case CpuModel::CPU_EV7:
        case CpuModel::CPU_EV78:
            executeEV7_SpecificPAL(palFunction, ra, rb, rc);
            break;

        default:
            DEBUG_LOG(QString("Unsupported CPU model for PAL function 0x%1").arg(QString::number(palFunction, 16)));
            emit sigRaiseException(ExceptionType::ILLEGAL_INSTRUCTION,
                           "Unsupported PAL function for this CPU model");
            break;
        }
    }

   
    
    void executeJump(const DecodedInstruction &instruction)
    {
        quint64 raValue = (instruction.ra == 31) ? 0 : m_cpu->getRegister(instruction.ra);
        quint64 rbValue = (instruction.rb == 31) ? 0 : m_cpu->getRegister(instruction.rb);
        quint64 currentPC = m_cpu->getPc();

        // Calculate target address
        quint64 targetPC = (rbValue + (instruction.immediate & 0x3FFF)) & ~0x3ULL; // Align to 4 bytes

        switch (instruction.function)
        {
        case 0: // JMP
        {
            // Store PC+4 in Ra (prediction base for returns)
            if (instruction.ra != 31)
            {
                m_cpu->setRegister(instruction.ra, currentPC + 4);
            }

            m_cpu->setPC(targetPC);
            m_cpu->flushPipeline();

            DEBUG_LOG(
                QString("ExecuteStage: JMP to 0x%1 (Ra=%2)").arg(targetPC, 16, 16, QChar('0')).arg(instruction.ra));
            break;
        }

        case 1: // JSR - Jump to Subroutine
        {
            // Store return address in Ra
            if (instruction.ra != 31)
            {
                m_cpu->setRegister(instruction.ra, currentPC + 4);
            }

            m_cpu->setPC(targetPC);
            m_cpu->flushPipeline();

            // Push return address onto hardware return stack (if implemented)
            m_cpu->pushReturnStack(currentPC + 4);

            DEBUG_LOG(QString("ExecuteStage: JSR to 0x%1, return address 0x%2")
                          .arg(targetPC, 16, 16, QChar('0'))
                          .arg(currentPC + 4, 16, 16, QChar('0')));
            break;
        }

        case 2: // RET - Return from Subroutine
        {
            // Pop return address from hardware return stack (if implemented)
            quint64 predictedReturn = m_cpu->popReturnStack();

            // Store PC+4 in Ra (usually not used for RET)
            if (instruction.ra != 31)
            {
                m_cpu->setRegister(instruction.ra, currentPC + 4);
            }

            m_cpu->setPC(targetPC);
            m_cpu->flushPipeline();

            DEBUG_LOG(QString("ExecuteStage: RET to 0x%1 (predicted: 0x%2)")
                          .arg(targetPC, 16, 16, QChar('0'))
                          .arg(predictedReturn, 16, 16, QChar('0')));

            // Check if prediction was correct
            if (targetPC != predictedReturn)
            {
                DEBUG_LOG("ExecuteStage: Return stack misprediction detected");
                m_cpu->incrementReturnMispredictions();
            }
            break;
        }

        case 3: // JSR_COROUTINE
        {
            // Coroutine jump - similar to JSR but for coroutines
            if (instruction.ra != 31)
            {
                m_cpu->setRegister(instruction.ra, currentPC + 4);
            }

            m_cpu->setPC(targetPC);
            m_cpu->flushPipeline();

            DEBUG_LOG(QString("ExecuteStage: JSR_COROUTINE to 0x%1").arg(targetPC, 16, 16, QChar('0')));
            break;
        }

        default:
        {
            DEBUG_LOG(QString("ExecuteStage: Unknown jump function %1").arg(instruction.function));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, currentPC);
            break;
        }
        }
    }
    
    void updateStatistics(const DecodedInstruction &instruction)
    {
        QMutexLocker locker(&m_statsMutex);

        m_totalInstructions++;

        // Categorize instruction type
        switch (instruction.opcode)
        {
        // Integer arithmetic and logical
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        {
            m_integerInstructions++;
            break;
        }

        // Floating-point operations
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        {
            m_floatingPointInstructions++;
            break;
        }

        // Memory operations
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        {
            m_memoryInstructions++;
            break;
        }

        // Branch operations
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case OPCODE_BLBC:
        case OPCODE_BEQ:
        case OPCODE_BLT:
        case OPCODE_BLE:
        case OPCODE_BLBS:
        case OPCODE_BNE:
        case OPCODE_BGE:
        case OPCODE_BGT:
        case 0x1A: // Jump instructions
        {
            m_branchInstructions++;
            break;
        }

        // PAL operations
        case 0x00:
        {
            m_palInstructions++;
            break;
        }
        }
    }
    void updatePC(quint64 pc_) {m_currentPC = pc_;}
    void updateBranchStatistics(bool mispredicted)
    {
        QMutexLocker locker(&m_statsMutex);

        if (mispredicted)
        {
            m_branchMispredictions++;
        }
    }
    
    QString getInstructionTypeName(quint8 opcode) const
    {
        switch (opcode)
        {
        case OPCODE_LDA:
        {
            return "LDA"; // Confirmed
        }
        case OPCODE_LDAH:
        {
            return "LDAH"; // Confirmed
        }
        case OPCODE_LDBU:
        {
            return "LDBU"; // Confirmed
        }
        case OPCODE_LDQ_U:
        {
            return "LDQ_U"; // Confirmed
        }
        case OPCODE_LDWU:
        {
            return "LDWU"; // Confirmed
        }
        case OPCODE_STW:
        {
            return "STW";
        }
        case OPCODE_STB:
        {
            return "STB";
        }
        case OPCODE_STQ_U:
        {
            return "STQ_U";
        }
        case OPCODE_INTA:
        {
            return "INTA";
        }
        case OPCODE_INTL:
        {
            return "INTL";
        }
        case OPCODE_INTS:
        {
            return "INTS";
        }
        case OPCODE_INTM:
        {
            return "INTM";
        }
        case OPCODE_ITFP:
        {
            return "ITFP";
        }
        case OPCODE_FLTV:
        {
            return "FLTV";
        }
        case OPCODE_FLTI:
        {
            return "FLTI";
        }
        case OPCODE_FLTL:
        {
            return "FLTL";
        }
        case OPCODE_JSR:
        {
            return "JUMP";
        }
        case OPCODE_LDF:
        {
            return "LDF";
        }
        case OPCODE_LDG:
        {
            return "LDG";
        }
        case OPCODE_LDS:
        {
            return "LDS";
        }
        case OPCODE_LDT:
        {
            return "LDT";
        }
        case OPCODE_STF:
        {
            return "STF";
        }
        case OPCODE_STG:
        {
            return "STG";
        }
        case OPCODE_STS:
        {
            return "STS";
        }
        case OPCODE_STT:
        {
            return "STT";
        }
        case OPCODE_LDL:
        {
            return "LDL";
        }
        case OPCODE_LDQ:
        {
            return "LDQ";
        }
        case OPCODE_LDL_L:
        {
            return "LDL_L";
        }
        case OPCODE_LDQ_L:
        {
            return "LDQ_L";
        }
        case OPCODE_STL:
        {
            return "STL";
        }
        case OPCODE_STQ:
        {
            return "STQ";
        }
        case OPCODE_STL_C:
        {
            return "STL_C";
        }
        case OPCODE_STQ_C:
        {
            return "STQ_C";
        }
        case OPCODE_BR:
        {
            return "BR";
        }
        case OPCODE_FBEQ:
        {
            return "FBEQ";
        }
        case OPCODE_FBLT:
        {
            return "FBLT";
        }
        case OPCODE_FBLE:
        {
            return "FBLE";
        }
        case OPCODE_BSR:
        {
            return "BSR";
        }
        case OPCODE_FBNE:
        {
            return "FBNE";
        }
        case OPCODE_FBGE:
        {
            return "FBGE";
        }
        case OPCODE_FBGT:
        {
            return "FBGT";
        }
        case OPCODE_BLBC:
        {
            return "BLBC";
        }
        case OPCODE_BEQ:
        {
            return "BEQ";
        }
        case OPCODE_BLT:
        {
            return "BLT";
        }
        case OPCODE_BLE:
        {
            return "BLE";
        }
        case OPCODE_BLBS:
        {
            return "BLBS";
        }
        case OPCODE_BNE:
        {
            return "BNE";
        }
        case OPCODE_BGE:
        {
            return "BGE";
        }
        case OPCODE_BGT:
        {
            return "BGT";
        }
        case 0x00:
        {
            return "PAL";
        }
        default:
        {
            return QString("UNK_0x%1").arg(opcode, 2, 16, QChar('0'));
        }
        }
    }
    
    quint64 convertQuadToF(const DecodedInstruction &instruction, quint64 raValue);
    quint64 convertQuadToG(const DecodedInstruction &instruction, quint64 raValue);
    quint64 convertGToF(const DecodedInstruction &instruction, quint64 raValue);

    
    /**
     * @brief Check if stage is busy
     */
    bool isBusy() const { return m_busy; }

    // Enhanced features

    void printStatistics() const
    {
        QMutexLocker locker(&m_statsMutex);

        if (m_totalInstructions == 0)
        {
            DEBUG_LOG("ExecuteStage: No instructions executed yet");
            return;
        }

        double integerRate = (double)m_integerInstructions / m_totalInstructions * 100.0;
        double fpRate = (double)m_floatingPointInstructions / m_totalInstructions * 100.0;
        double memoryRate = (double)m_memoryInstructions / m_totalInstructions * 100.0;
        double branchRate = (double)m_branchInstructions / m_totalInstructions * 100.0;
        double palRate = (double)m_palInstructions / m_totalInstructions * 100.0;

        double mispredictionRate =
            m_branchInstructions > 0 ? (double)m_branchMispredictions / m_branchInstructions * 100.0 : 0.0;

        double executionRate = m_executionCycles > 0 ? (double)m_totalInstructions / m_executionCycles : 0.0;

        double stallRate = m_executionCycles > 0 ? (double)m_stallCyclesTotal / m_executionCycles * 100.0 : 0.0;

        DEBUG_LOG("ExecuteStage Statistics:");
        DEBUG_LOG(QString("  Total Instructions: %1").arg(m_totalInstructions));
        DEBUG_LOG(QString("  Execution Cycles: %1").arg(m_executionCycles));
        DEBUG_LOG(QString("  Instructions per Cycle: %1").arg(executionRate, 0, 'f', 3));
        DEBUG_LOG(QString("  Integer Instructions: %1 (%2%)").arg(m_integerInstructions).arg(integerRate, 0, 'f', 2));
        DEBUG_LOG(
            QString("  Floating-Point Instructions: %1 (%2%)").arg(m_floatingPointInstructions).arg(fpRate, 0, 'f', 2));
        DEBUG_LOG(QString("  Memory Instructions: %1 (%2%)").arg(m_memoryInstructions).arg(memoryRate, 0, 'f', 2));
        DEBUG_LOG(QString("  Branch Instructions: %1 (%2%)").arg(m_branchInstructions).arg(branchRate, 0, 'f', 2));
        DEBUG_LOG(QString("  PAL Instructions: %1 (%2%)").arg(m_palInstructions).arg(palRate, 0, 'f', 2));
        DEBUG_LOG(
            QString("  Branch Mispredictions: %1 (%2%)").arg(m_branchMispredictions).arg(mispredictionRate, 0, 'f', 2));
        DEBUG_LOG(QString("  Pipeline Stalls: %1 cycles (%2%)").arg(m_stallCyclesTotal).arg(stallRate, 0, 'f', 2));
    }
    
    void clearStatistics()
    {
        QMutexLocker locker(&m_statsMutex);

        m_totalInstructions = 0;
        m_integerInstructions = 0;
        m_floatingPointInstructions = 0;
        m_memoryInstructions = 0;
        m_branchInstructions = 0;
        m_branchMispredictions = 0;
        m_palInstructions = 0;
        m_executionCycles = 0;
        m_stallCyclesTotal = 0;

        DEBUG_LOG("ExecuteStage: Statistics cleared");
    }
    
    void setPipelineDepth(quint32 depth) { m_pipelineDepth = depth; }

    // Statistics

    double getInstructionExecutionRate() const
    {
        QMutexLocker locker(&m_statsMutex);
        return m_executionCycles > 0 ? (double)m_totalInstructions / m_executionCycles : 0.0;
    }
    
    quint64 getExecutedInstructions() const
    {
        QMutexLocker locker(&m_statsMutex);
        return m_totalInstructions;
    }
    
    quint64 getBranchMispredictions() const
    {
        QMutexLocker locker(&m_statsMutex);
        return m_branchMispredictions;
    }
    
    // Pipeline management

    void stall(quint32 cycles)
    {
        m_stallCycles = cycles;
        m_stallCyclesTotal += cycles;
        emit sigPipelineStalled(cycles);

        DEBUG_LOG(QString("ExecuteStage: Pipeline stalled for %1 cycles").arg(cycles));
    }
    
    bool isStalled() const { return m_stallCycles > 0; }

    /**
     * @brief Load Longword Unsigned (32-bit) with unaligned access support
     * @param address Virtual address to load from
     * @param value Reference to store the loaded value
     * @return true if load successful, false if exception occurred
     */
    template <typename T> bool ldu_u(quint64 address, T &value)
    {
        try
        {
            // Perform virtual to physical address translation
            TranslationResult translation = m_cpu->getMMU()->translateAddress(
                address, false, false, m_cpu->getCurrentASN(), m_cpu->isKernelMode(), sizeof(T));

            if (translation.getTLBException() != AsaExceptions::excTLBException::NONE)
            {
                // Handle TLB exception
                handleTLBException(translation.getTLBException(), address);
                return false;
            }

            quint64 physicalAddress = translation.getPhysicalAddress();

            // Check for alignment - longword loads should be 4-byte aligned for optimal performance
            // but Alpha supports unaligned access with potential performance penalty
            if (sizeof(T) == 4 && (address & 0x3) != 0)
            {
                // Unaligned longword access - handle specially
                return loadUnalignedLongword(physicalAddress, reinterpret_cast<quint32 &>(value));
            }

            // Perform the memory access through the memory system
            bool success = m_cpu->getMemorySystem()->readMemory(physicalAddress, &value, sizeof(T));

            if (!success)
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                return false;
            }

            // For unsigned loads, ensure proper zero extension
            if (sizeof(T) == 4)
            {
                // Longword unsigned - zero extend to 64 bits
                value = static_cast<T>(static_cast<quint32>(value));
            }

            DEBUG_LOG(QString("ldu_u: Loaded 0x%1 from VA=0x%2 PA=0x%3")
                          .arg(static_cast<quint64>(value), sizeof(T) * 2, 16, QChar('0'))
                          .arg(address, 16, 16, QChar('0'))
                          .arg(physicalAddress, 16, 16, QChar('0')));

            return true;
        }
        catch (const std::exception &e)
        {
            DEBUG_LOG(
                QString("ldu_u: Exception during load from 0x%1: %2").arg(address, 16, 16, QChar('0')).arg(e.what()));
            m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
            return false;
        }
    }

    /**
     * @brief Load Quadword Unsigned (64-bit) with unaligned access support
     * @param address Virtual address to load from
     * @param value Reference to store the loaded value
     * @return true if load successful, false if exception occurred
     */
    template <typename T> bool ldq_u(quint64 address, T &value)
    {
        try
        {
            // Perform virtual to physical address translation
            TranslationResult translation = m_cpu->getMMU()->translateAddress(
                address, false, false, m_cpu->getCurrentASN(), m_cpu->isKernelMode(), sizeof(T));

            if (translation.getTLBException() != excTLBException::NONE)
            {
                handleTLBException(translation.getTLBException(), address);
                return false;
            }

            quint64 physicalAddress = translation.getPhysicalAddress();

            // Check for alignment - quadword loads should be 8-byte aligned for optimal performance
            if (sizeof(T) == 8 && (address & 0x7) != 0)
            {
                // Unaligned quadword access - handle specially
                return loadUnalignedQuadword(physicalAddress, reinterpret_cast<quint64 &>(value));
            }

            // Perform the memory access
            bool success = m_cpu->getMemorySystem()->readMemory(physicalAddress, &value, sizeof(T));

            if (!success)
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                return false;
            }

            DEBUG_LOG(QString("ldq_u: Loaded 0x%1 from VA=0x%2 PA=0x%3")
                          .arg(static_cast<quint64>(value), sizeof(T) * 2, 16, QChar('0'))
                          .arg(address, 16, 16, QChar('0'))
                          .arg(physicalAddress, 16, 16, QChar('0')));

            return true;
        }
        catch (const std::exception &e)
        {
            DEBUG_LOG(
                QString("ldq_u: Exception during load from 0x%1: %2").arg(address, 16, 16, QChar('0')).arg(e.what()));
            m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
            return false;
        }
    }

    /**
     * @brief Load Byte Unsigned (8-bit) - always aligned
     * @param address Virtual address to load from
     * @param value Reference to store the loaded value
     * @return true if load successful, false if exception occurred
     */
    bool ldbu(quint64 address, quint8 &value)
    {
        try
        {
            // Perform virtual to physical address translation
            TranslationResult translation = m_cpu->getMMU()->translateAddress(
                address, false, false, m_cpu->getCurrentASN(), m_cpu->isKernelMode(), 1);

            if (translation.getTLBException() != excTLBException::NONE)
            {
                handleTLBException(translation.getTLBException(), address);
                return false;
            }

            quint64 physicalAddress = translation.getPhysicalAddress();

            // Byte loads are always aligned - no special handling needed
            bool success = m_cpu->getMemorySystem()->readMemory(physicalAddress, &value, 1);

            if (!success)
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                return false;
            }

            DEBUG_LOG(QString("ldbu: Loaded 0x%1 from VA=0x%2 PA=0x%3")
                          .arg(value, 2, 16, QChar('0'))
                          .arg(address, 16, 16, QChar('0'))
                          .arg(physicalAddress, 16, 16, QChar('0')));

            return true;
        }
        catch (const std::exception &e)
        {
            DEBUG_LOG(
                QString("ldbu: Exception during load from 0x%1: %2").arg(address, 16, 16, QChar('0')).arg(e.what()));
            m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
            return false;
        }
    }

    /**
     * @brief Handle unaligned longword (32-bit) loads
     * @param physicalAddress Physical address to load from
     * @param value Reference to store the loaded value
     * @return true if successful
     */
    bool loadUnalignedLongword(quint64 physicalAddress, quint32 &value)
    {
        // Alpha handles unaligned access by loading individual bytes
        // This is slower but maintains correctness
        quint8 bytes[4];

        for (int i = 0; i < 4; i++)
        {
            if (!m_cpu->getMemorySystem()->readMemory(physicalAddress + i, &bytes[i], 1))
            {
                return false;
            }
        }

        // Assemble the longword (little-endian)
        value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);

        DEBUG_LOG(QString("loadUnalignedLongword: Assembled 0x%1 from PA=0x%2")
                      .arg(value, 8, 16, QChar('0'))
                      .arg(physicalAddress, 16, 16, QChar('0')));

        return true;
    }

    /**
     * @brief Handle unaligned quadword (64-bit) loads
     * @param physicalAddress Physical address to load from
     * @param value Reference to store the loaded value
     * @return true if successful
     */
    bool loadUnalignedQuadword(quint64 physicalAddress, quint64 &value)
    {
        // Alpha handles unaligned access by loading individual bytes
        quint8 bytes[8];

        for (int i = 0; i < 8; i++)
        {
            if (!m_cpu->getMemorySystem()->readMemory(physicalAddress + i, &bytes[i], 1))
            {
                return false;
            }
        }

        // Assemble the quadword (little-endian)
        value = static_cast<quint64>(bytes[0]) | (static_cast<quint64>(bytes[1]) << 8) |
                (static_cast<quint64>(bytes[2]) << 16) | (static_cast<quint64>(bytes[3]) << 24) |
                (static_cast<quint64>(bytes[4]) << 32) | (static_cast<quint64>(bytes[5]) << 40) |
                (static_cast<quint64>(bytes[6]) << 48) | (static_cast<quint64>(bytes[7]) << 56);

        DEBUG_LOG(QString("loadUnalignedQuadword: Assembled 0x%1 from PA=0x%2")
                      .arg(value, 16, 16, QChar('0'))
                      .arg(physicalAddress, 16, 16, QChar('0')));

        return true;
    }

    /**
     * @brief Handle TLB exceptions during memory access
     * @param exception The TLB exception type
     * @param address The virtual address that caused the exception
     */
    void handleTLBException(excTLBException exception, quint64 address)
    {
        DEBUG_LOG(QString("TLB Exception: %1 for address 0x%2")
                      .arg(static_cast<int>(exception))
                      .arg(address, 16, 16, QChar('0')));

        switch (exception)
        {
        case excTLBException::INVALID_ENTRY:
            m_cpu->triggerException(ExceptionType::PAGE_FAULT, address);
            break;

        case excTLBException::PROTECTION_FAULT:
            m_cpu->triggerException(ExceptionType::ACCESS_CONTROL_VIOLATION, address);
            break;

        case excTLBException::ALIGNMENT_FAULT:
            m_cpu->triggerException(ExceptionType::ALIGNMENT_FAULT, address);
            break;

        default:
            m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
            break;
        }
    }

    /*──────────────────────────────────────────────────────────────────────────────
     *  executeIntegerLogical.H   –   Alpha AXP Execute Stage, integer groups
     *  Header-only, Qt-style.  Nest opcode → function switches to avoid collisions
     *  between identically-encoded function fields in different primary opcodes.
     *
     *  ▸ Primary opcode 0x10 : INT-logical/arithmetic   (AND/BIC/BIS/XOR/EQV/ORNOT)
     *  ▸ Primary opcode 0x11 : Conditional moves        (CMOVxx group)
     *  ▸ Primary opcode 0x12 : Extract/Insert/Mask      (MSK/EXT/INS family)
     *
     *  References
     *    • Alpha AXP System Ref. Manual v6, §4.2-4.3  (integer formats)  p.4-7→4-13
     *    • Appendix C-1 & C-2 (opcode/function tables)                   p.C-2→C-6
     *────────────────────────────────────────────────────────────────────────────*/

// Enhanced execIntegerGroup using your existing constexpr constants
    // This replaces the scattered integer handling in the main execute() method

    void executeIntegerGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        quint64 raValue = R(instruction.ra);
        quint64 rbValue = (instruction.rawInstruction & 0x1000) ? instruction.immediate : R(instruction.rb);
        quint64 rcValue = R(instruction.rc); // For conditional moves
        quint64 result = 0;
        bool overflow = false;
        bool trapOnOverflow = false;

        switch (instruction.opcode)
        {
        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_INTA (0x10) - Integer Arithmetic Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_INTA:
            trapOnOverflow = (instruction.function & 0x40) != 0; // V bit in function code

            switch (instruction.function)
            {
            // ─────────────────────── 32-bit Longword Operations ───────────────────────
            case FUNC_ADDL:
            case FUNC_ADDLV:
            {
                qint32 a = static_cast<qint32>(raValue);
                qint32 b = static_cast<qint32>(rbValue);
                qint64 res64 = static_cast<qint64>(a) + static_cast<qint64>(b);
                qint32 res32 = static_cast<qint32>(res64);

                result = static_cast<quint64>(static_cast<qint64>(res32)); // Sign-extend
                overflow = (res64 != static_cast<qint64>(res32));

                DEBUG_LOG(QString("ADDL%1: %2 + %3 = %4").arg(trapOnOverflow ? "/V" : "").arg(a).arg(b).arg(res32));
                break;
            }

            case FUNC_S4ADDL:
            case FUNC_S4ADDLV:
            {
                qint32 a = static_cast<qint32>(raValue);
                qint32 b = static_cast<qint32>(rbValue);
                qint64 res64 = (static_cast<qint64>(a) << 2) + static_cast<qint64>(b);
                qint32 res32 = static_cast<qint32>(res64);

                result = static_cast<quint64>(static_cast<qint64>(res32));
                overflow = (res64 != static_cast<qint64>(res32));

                DEBUG_LOG(
                    QString("S4ADDL%1: (%2 << 2) + %3 = %4").arg(trapOnOverflow ? "/V" : "").arg(a).arg(b).arg(res32));
                break;
            }

            case FUNC_SUBL:
            case FUNC_SUBLV:
            {
                qint32 a = static_cast<qint32>(raValue);
                qint32 b = static_cast<qint32>(rbValue);
                qint64 res64 = static_cast<qint64>(a) - static_cast<qint64>(b);
                qint32 res32 = static_cast<qint32>(res64);

                result = static_cast<quint64>(static_cast<qint64>(res32));
                overflow = (res64 != static_cast<qint64>(res32));

                DEBUG_LOG(QString("SUBL%1: %2 - %3 = %4").arg(trapOnOverflow ? "/V" : "").arg(a).arg(b).arg(res32));
                break;
            }

            case FUNC_S4SUBL:
            case FUNC_S4SUBLV:
            {
                qint32 a = static_cast<qint32>(raValue);
                qint32 b = static_cast<qint32>(rbValue);
                qint64 res64 = (static_cast<qint64>(a) << 2) - static_cast<qint64>(b);
                qint32 res32 = static_cast<qint32>(res64);

                result = static_cast<quint64>(static_cast<qint64>(res32));
                overflow = (res64 != static_cast<qint64>(res32));

                DEBUG_LOG(
                    QString("S4SUBL%1: (%2 << 2) - %3 = %4").arg(trapOnOverflow ? "/V" : "").arg(a).arg(b).arg(res32));
                break;
            }

            case FUNC_CMPBGE:
            {
                result = 0;
                for (int i = 0; i < 8; i++)
                {
                    quint8 aByte = (raValue >> (i * 8)) & 0xFF;
                    quint8 bByte = (rbValue >> (i * 8)) & 0xFF;
                    if (aByte >= bByte)
                        result |= (1ULL << i);
                }
                DEBUG_LOG(QString("CMPBGE: result = 0x%1").arg(result, 2, 16, QChar('0')));
                break;
            }

            case FUNC_S8ADDL:
            case FUNC_S8ADDLV:
            {
                qint32 a = static_cast<qint32>(raValue);
                qint32 b = static_cast<qint32>(rbValue);
                qint64 res64 = (static_cast<qint64>(a) << 3) + static_cast<qint64>(b);
                qint32 res32 = static_cast<qint32>(res64);

                result = static_cast<quint64>(static_cast<qint64>(res32));
                overflow = (res64 != static_cast<qint64>(res32));

                DEBUG_LOG(
                    QString("S8ADDL%1: (%2 << 3) + %3 = %4").arg(trapOnOverflow ? "/V" : "").arg(a).arg(b).arg(res32));
                break;
            }

            case FUNC_S8SUBL:
            case FUNC_S8SUBLV:
            {
                qint32 a = static_cast<qint32>(raValue);
                qint32 b = static_cast<qint32>(rbValue);
                qint64 res64 = (static_cast<qint64>(a) << 3) - static_cast<qint64>(b);
                qint32 res32 = static_cast<qint32>(res64);

                result = static_cast<quint64>(static_cast<qint64>(res32));
                overflow = (res64 != static_cast<qint64>(res32));

                DEBUG_LOG(
                    QString("S8SUBL%1: (%2 << 3) - %3 = %4").arg(trapOnOverflow ? "/V" : "").arg(a).arg(b).arg(res32));
                break;
            }

            case FUNC_CMPULT:
            case FUNC_CMPULT_L:
            case FUNC_CMPULT_G:
            {
                // Handle both longword and quadword variants
                if (instruction.function == FUNC_CMPULT_L)
                {
                    quint32 a = static_cast<quint32>(raValue);
                    quint32 b = static_cast<quint32>(rbValue);
                    result = (a < b) ? 1 : 0;
                    DEBUG_LOG(QString("CMPULT (longword): %1 < %2 ? %3").arg(a).arg(b).arg(result));
                }
                else
                {
                    result = (raValue < rbValue) ? 1 : 0;
                    DEBUG_LOG(QString("CMPULT (quadword): %1 < %2 ? %3").arg(raValue).arg(rbValue).arg(result));
                }
                break;
            }

            // ─────────────────────── 64-bit Quadword Operations ───────────────────────
            case FUNC_ADDQ:
            case FUNC_ADDQV:
            {
                result = raValue + rbValue;
                overflow = ((raValue ^ result) & (rbValue ^ result)) >> 63;

                DEBUG_LOG(QString("ADDQ%1: 0x%2 + 0x%3 = 0x%4")
                              .arg(trapOnOverflow ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_S4ADDQ:
            case FUNC_S4ADDQV:
            {
                quint64 shiftedRA = raValue << 2;
                result = shiftedRA + rbValue;
                overflow = (shiftedRA > result) || ((raValue & 0xC000000000000000ULL) != 0);

                DEBUG_LOG(QString("S4ADDQ%1: (0x%2 << 2) + 0x%3 = 0x%4")
                              .arg(trapOnOverflow ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_SUBQ:
            case FUNC_SUBQV:
            {
                result = raValue - rbValue;
                overflow = (raValue < rbValue);

                DEBUG_LOG(QString("SUBQ%1: 0x%2 - 0x%3 = 0x%4")
                              .arg(trapOnOverflow ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_S4SUBQ:
            case FUNC_S4SUBQV:
            {
                quint64 shiftedRA = raValue << 2;
                result = shiftedRA - rbValue;
                overflow = (shiftedRA < rbValue);

                DEBUG_LOG(QString("S4SUBQ%1: (0x%2 << 2) - 0x%3 = 0x%4")
                              .arg(trapOnOverflow ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_S8ADDQ:
            case FUNC_S8ADDQV:
            {
                quint64 shiftedRA = raValue << 3;
                result = shiftedRA + rbValue;
                overflow = (shiftedRA > result) || ((raValue & 0xE000000000000000ULL) != 0);

                DEBUG_LOG(QString("S8ADDQ%1: (0x%2 << 3) + 0x%3 = 0x%4")
                              .arg(trapOnOverflow ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_S8SUBQ:
            case FUNC_S8SUBQV:
            {
                quint64 shiftedRA = raValue << 3;
                result = shiftedRA - rbValue;
                overflow = (shiftedRA < rbValue);

                DEBUG_LOG(QString("S8SUBQ%1: (0x%2 << 3) - 0x%3 = 0x%4")
                              .arg(trapOnOverflow ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            // ─────────────────────── Comparison Operations ───────────────────────
            case FUNC_CMPEQ:
            {
                result = (raValue == rbValue) ? 1 : 0;
                DEBUG_LOG(QString("CMPEQ: 0x%1 == 0x%2 ? %3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result));
                break;
            }

            case FUNC_CMPNE:
            {
                result = (raValue != rbValue) ? 1 : 0;
                DEBUG_LOG(QString("CMPNE: 0x%1 != 0x%2 ? %3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result));
                break;
            }

            case FUNC_CMPULE_L:
            case FUNC_CMPULE_G:
            {
                if (instruction.function == FUNC_CMPULE_L)
                {
                    result = (static_cast<quint32>(raValue) <= static_cast<quint32>(rbValue)) ? 1 : 0;
                    DEBUG_LOG(QString("CMPULE (longword): %1 <= %2 ? %3")
                                  .arg(static_cast<quint32>(raValue))
                                  .arg(static_cast<quint32>(rbValue))
                                  .arg(result));
                }
                else
                {
                    result = (raValue <= rbValue) ? 1 : 0;
                    DEBUG_LOG(QString("CMPULE (quadword): %1 <= %2 ? %3").arg(raValue).arg(rbValue).arg(result));
                }
                break;
            }

            case FUNC_CMPLT:
            {
                result = (static_cast<qint64>(raValue) < static_cast<qint64>(rbValue)) ? 1 : 0;
                DEBUG_LOG(QString("CMPLT: %1 < %2 ? %3")
                              .arg(static_cast<qint64>(raValue))
                              .arg(static_cast<qint64>(rbValue))
                              .arg(result));
                break;
            }

            case FUNC_CMPLE:
            {
                result = (static_cast<qint64>(raValue) <= static_cast<qint64>(rbValue)) ? 1 : 0;
                DEBUG_LOG(QString("CMPLE: %1 <= %2 ? %3")
                              .arg(static_cast<qint64>(raValue))
                              .arg(static_cast<qint64>(rbValue))
                              .arg(result));
                break;
            }

            case FUNC_CMPUGE:
            {
                result = (raValue >= rbValue) ? 1 : 0;
                DEBUG_LOG(QString("CMPUGE: %1 >= %2 ? %3").arg(raValue).arg(rbValue).arg(result));
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented INTA function 0x%1").arg(instruction.function, 2, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            // Handle overflow trap variants
            if (overflow && trapOnOverflow)
            {
                m_cpu->triggerException(ExceptionType::ARITHMETIC_TRAP, m_cpu->getPc());
            }
            break;

        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_INTL (0x11) - Integer Logical Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_INTL:
            switch (instruction.function)
            {
            // ─────────────────────── Basic Logical Operations ───────────────────────
            case FUNC_AND:
            {
                result = raValue & rbValue;
                DEBUG_LOG(QString("AND: 0x%1 & 0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_BIC:
            {
                result = raValue & ~rbValue;
                DEBUG_LOG(QString("BIC: 0x%1 & ~0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_BIS:
            {
                result = raValue | rbValue;
                DEBUG_LOG(QString("BIS: 0x%1 | 0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_XOR:
            {
                result = raValue ^ rbValue;
                DEBUG_LOG(QString("XOR: 0x%1 ^ 0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_EQV:
            {
                result = ~(raValue ^ rbValue);
                DEBUG_LOG(QString("EQV: ~(0x%1 ^ 0x%2) = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_ORNOT:
            {
                result = raValue | ~rbValue;
                DEBUG_LOG(QString("ORNOT: 0x%1 | ~0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            // ─────────────────────── Conditional Move Operations ───────────────────────
            case FUNC_CMOVLBS:
            {
                result = (raValue & 1) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVLBS: %1").arg((raValue & 1) ? "moved" : "not moved"));
                break;
            }

            case FUNC_CMOVLBC:
            {
                result = (raValue & 1) ? rcValue : rbValue;
                DEBUG_LOG(QString("CMOVLBC: %1").arg((raValue & 1) ? "not moved" : "moved"));
                break;
            }

            case FUNC_CMOVEQ:
            {
                result = (raValue == 0) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVEQ: %1").arg((raValue == 0) ? "moved" : "not moved"));
                break;
            }

            case FUNC_CMOVNE:
            {
                result = (raValue != 0) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVNE: %1").arg((raValue != 0) ? "moved" : "not moved"));
                break;
            }

            case FUNC_CMOVLT:
            {
                result = (static_cast<qint64>(raValue) < 0) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVLT: %1").arg((static_cast<qint64>(raValue) < 0) ? "moved" : "not moved"));
                break;
            }

            case FUNC_CMOVGE:
            {
                result = (static_cast<qint64>(raValue) >= 0) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVGE: %1").arg((static_cast<qint64>(raValue) >= 0) ? "moved" : "not moved"));
                break;
            }

            case FUNC_CMOVLE:
            {
                result = (static_cast<qint64>(raValue) <= 0) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVLE: %1").arg((static_cast<qint64>(raValue) <= 0) ? "moved" : "not moved"));
                break;
            }

            case FUNC_CMOVGT:
            {
                result = (static_cast<qint64>(raValue) > 0) ? rbValue : rcValue;
                DEBUG_LOG(QString("CMOVGT: %1").arg((static_cast<qint64>(raValue) > 0) ? "moved" : "not moved"));
                break;
            }

            // ─────────────────────── Architecture Instructions ───────────────────────
            case FUNC_AMASK:
            {
                result = ~0ULL; // Indicate all features unimplemented (conservative)
                DEBUG_LOG("AMASK: returning 0xFFFFFFFFFFFFFFFF");
                break;
            }

            case FUNC_IMPLVER:
            {
                result = m_cpu->implVersion();
                DEBUG_LOG(QString("IMPLVER: → %1").arg(result));
                break;
            }

            // ─────────────────────── Mask/Extract/Insert Operations ───────────────────────
            case FUNC_MSKBL:
            case FUNC_EXTBL:
            case FUNC_INSBL:
            case FUNC_MSKWL:
            case FUNC_EXTWL:
            case FUNC_INSWL:
            case FUNC_MSKLL:
            case FUNC_EXTLL:
            case FUNC_INSLL:
            case FUNC_MSKQL:
            case FUNC_EXTQL:
            case FUNC_INSQL:
            case FUNC_MSKBH:
            case FUNC_EXTBH:
            case FUNC_INSBH:
            case FUNC_MSKWH:
            case FUNC_EXTWH:
            case FUNC_INSWH:
            case FUNC_MSKLH:
            case FUNC_EXTLH:
            case FUNC_INSLH:
            case FUNC_MSKQH:
            case FUNC_EXTQH:
            case FUNC_INSQH:
            {
                // Implementation for mask/insert/extract instructions
                // These are complex and require their own detailed implementation
                DEBUG_LOG(QString("Mask/Extract/Insert instruction 0x%1 - implementation needed")
                              .arg(instruction.function, 2, 16, QChar('0')));
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented INTL function 0x%1").arg(instruction.function, 2, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }
            break;

        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_INTS (0x12) - Integer Shift Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_INTS:
        {
            quint32 shiftAmount = rbValue & 0x3F; // Only lower 6 bits used for shift amount

            switch (instruction.function)
            {
            case FUNC_SLL:
            {
                result = raValue << shiftAmount;
                DEBUG_LOG(QString("SLL: 0x%1 << %2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(shiftAmount)
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_SRA:
            {
                result = static_cast<quint64>(static_cast<qint64>(raValue) >> shiftAmount);
                DEBUG_LOG(QString("SRA: 0x%1 >> %2 = 0x%3 (arithmetic)")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(shiftAmount)
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_SRL:
            {
                result = raValue >> shiftAmount;
                DEBUG_LOG(QString("SRL: 0x%1 >> %2 = 0x%3 (logical)")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(shiftAmount)
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_EXTBL:
            {
                quint32 bytePos = shiftAmount & 0x7; // Only lower 3 bits
                result = (raValue >> (bytePos * 8)) & 0xFF;
                DEBUG_LOG(QString("EXTBL: byte %1 from 0x%2 = 0x%3")
                              .arg(bytePos)
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(result, 2, 16, QChar('0')));
                break;
            }

            case FUNC_EXTWL:
            {
                quint32 wordPos = (shiftAmount >> 1) & 0x3; // Word position
                result = (raValue >> (wordPos * 16)) & 0xFFFF;
                DEBUG_LOG(QString("EXTWL: word %1 from 0x%2 = 0x%3")
                              .arg(wordPos)
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(result, 4, 16, QChar('0')));
                break;
            }

            case FUNC_EXTLL:
            {
                quint32 longwordPos = (shiftAmount >> 2) & 0x1; // Longword position
                result = (raValue >> (longwordPos * 32)) & 0xFFFFFFFF;
                // Sign-extend result to 64 bits
                result = static_cast<quint64>(static_cast<qint64>(static_cast<qint32>(result)));
                DEBUG_LOG(QString("EXTLL: longword %1 from 0x%2 = 0x%3")
                              .arg(longwordPos)
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(result, 8, 16, QChar('0')));
                break;
            }

            case FUNC_EXTQL:
            {
                // For quadword, shift amount determines byte boundary
                result = raValue >> ((shiftAmount & 0x7) * 8);
                DEBUG_LOG(QString("EXTQL: from 0x%1 shift %2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(shiftAmount & 0x7)
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_ZAP:
            {
                // Zero bytes based on mask
                quint8 mask = rbValue & 0xFF;
                result = raValue;
                for (int i = 0; i < 8; i++)
                {
                    if (mask & (1 << i))
                    {
                        result &= ~(0xFFULL << (i * 8));
                    }
                }
                DEBUG_LOG(QString("ZAP: 0x%1 with mask 0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(mask, 2, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_ZAPNOT:
            {
                // Zero bytes NOT in mask
                quint8 mask = rbValue & 0xFF;
                result = raValue;
                for (int i = 0; i < 8; i++)
                {
                    if (!(mask & (1 << i)))
                    {
                        result &= ~(0xFFULL << (i * 8));
                    }
                }
                DEBUG_LOG(QString("ZAPNOT: 0x%1 with mask 0x%2 = 0x%3")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(mask, 2, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented INTS function 0x%1").arg(instruction.function, 2, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }
            break;
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_INTM (0x13) - Integer Multiply Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_INTM:
            switch (instruction.function)
            {
            case FUNC_MULL:
            case FUNC_MULL_V:
            {
                qint64 prod = static_cast<qint64>(static_cast<qint32>(raValue)) *
                              static_cast<qint64>(static_cast<qint32>(rbValue));

                result = static_cast<quint64>(static_cast<qint32>(prod)); // sign-extend 32-bit low part
                overflow = ((instruction.function == FUNC_MULL_V) != 0) &&
                           ((prod >> 31) != (prod >> 63)); // true if high 32 bits ≠ sign of low 32

                DEBUG_LOG(QString("MULL%1: %2 * %3 = %4")
                              .arg((instruction.function == FUNC_MULL_V) ? "/V" : "")
                              .arg(static_cast<qint32>(raValue))
                              .arg(static_cast<qint32>(rbValue))
                              .arg(static_cast<qint32>(prod)));
                break;
            }

            case FUNC_MULQ:
            case FUNC_MULQV:
            {
                result = raValue * rbValue;
                // Simplified overflow check - full implementation would need 128-bit multiply
                overflow = false;

                DEBUG_LOG(QString("MULQ%1: 0x%2 * 0x%3 → 0x%4")
                              .arg((instruction.function == FUNC_MULQV) ? "/V" : "")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_UMULH:
            {
#ifdef __SIZEOF_INT128__
                __uint128_t fullResult = static_cast<__uint128_t>(raValue) * static_cast<__uint128_t>(rbValue);
                result = static_cast<quint64>(fullResult >> 64);
#else
                // Fallback - simplified implementation using 64-bit arithmetic
                // This is a basic implementation that may not be fully accurate
                quint64 aHi = raValue >> 32;
                quint64 aLo = raValue & 0xFFFFFFFF;
                quint64 bHi = rbValue >> 32;
                quint64 bLo = rbValue & 0xFFFFFFFF;

                quint64 p1 = aLo * bLo;
                quint64 p2 = aLo * bHi;
                quint64 p3 = aHi * bLo;
                quint64 p4 = aHi * bHi;

                quint64 carry = ((p1 >> 32) + (p2 & 0xFFFFFFFF) + (p3 & 0xFFFFFFFF)) >> 32;
                result = p4 + (p2 >> 32) + (p3 >> 32) + carry;
#endif
                DEBUG_LOG(QString("UMULH: 0x%1 * 0x%2 = 0x%3 (high)")
                              .arg(raValue, 16, 16, QChar('0'))
                              .arg(rbValue, 16, 16, QChar('0'))
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented INTM function 0x%1").arg(instruction.function, 2, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            // Handle overflow for V variants
            if (overflow && (instruction.function == FUNC_MULL_V || instruction.function == FUNC_MULQV))
            {
                m_cpu->triggerException(ExceptionType::ARITHMETIC_TRAP, m_cpu->getPc());
            }
            break;

        default:
            DEBUG_LOG(
                QString("executeIntegerGroup: Unknown integer opcode 0x%1").arg(instruction.opcode, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            return;
        }

        // Store result in destination register (if not R31)
        RW(instruction.rc, result);

        // Update instruction statistics
        m_integerInstructions++;
        m_totalInstructions++;
    }

    // Enhanced executeFloatingPointGroup using your existing constexpr constants
    // This consolidates all floating-point instruction handling into a single method

    void executeFloatingPointGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto F = [this](int r) -> quint64 { return m_cpu->getFloatRegister64(r); };
        auto F32 = [this](int r) -> float { return m_cpu->getFloatRegister32(r); };
        auto FD = [this](int r) -> double { return m_cpu->getFloatRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };
        auto FW = [this](int r, quint64 v) { m_cpu->setFloatRegister(r, v); };
        auto FW32 = [this](int r, float v) { m_cpu->setFloatRegister(r, v); };
        auto FWD = [this](int r, double v) { m_cpu->setFloatRegister(r, v); };

        quint64 faValue = F(instruction.ra);
        quint64 fbValue = F(instruction.rb);
        quint64 raValue = R(instruction.ra); // For integer-to-float conversions
        quint64 rbValue = R(instruction.rb);
        quint64 result = 0;

        // Check for floating-point exceptions enabled
        if (!m_cpu->isFloatingPointEnabled())
        {
            DEBUG_LOG("ExecuteStage: Floating-point operation with FP disabled");
            m_cpu->triggerException(FPTrapType::FP_DISABLED, m_cpu->getPc());
            return;
        }

        switch (instruction.opcode)
        {
        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_ITFP (0x14) - Integer to Floating Point Conversions
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_ITFP:
            switch (instruction.function)
            {
            case FUNC_ITOFS: // Integer to F_floating Single
            {
                float result = static_cast<float>(static_cast<int32_t>(raValue));
                FW32(instruction.rc, result);
                DEBUG_LOG(QString("ITOFS: R%1 (%2) -> F%3 (%4)")
                              .arg(instruction.ra)
                              .arg(static_cast<int32_t>(raValue))
                              .arg(instruction.rc)
                              .arg(result));
                break;
            }

            case FUNC_ITOFF: // Integer to F_floating Double
            {
                double result = static_cast<double>(static_cast<int64_t>(raValue));
                FWD(instruction.rc, result);
                DEBUG_LOG(QString("ITOFF: R%1 (%2) -> F%3 (%4)")
                              .arg(instruction.ra)
                              .arg(static_cast<int64_t>(raValue))
                              .arg(instruction.rc)
                              .arg(result));
                break;
            }

            case FUNC_ITOFT: // Integer to T_floating
            {
                double result = static_cast<double>(static_cast<int64_t>(raValue));
                FWD(instruction.rc, result);
                DEBUG_LOG(QString("ITOFT: R%1 (%2) -> F%3 (%4)")
                              .arg(instruction.ra)
                              .arg(static_cast<int64_t>(raValue))
                              .arg(instruction.rc)
                              .arg(result));
                break;
            }

            // ─────────────────────── SQRT Operations ───────────────────────
            // Basic SQRT operations
            case FUNC_SQRTF_C: // SQRTF/C - Square Root F_floating (Chopped)
            case FUNC_SQRTS_C: // SQRTS/C - Square Root S_floating (Chopped)
            case FUNC_SQRTG_C: // SQRTG/C - Square Root G_floating (Chopped)
            case FUNC_SQRTT_C: // SQRTT/C - Square Root T_floating (Chopped)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);
                result = std::trunc(result); // Chopped rounding mode

                FWD(instruction.rc, result);
                DEBUG_LOG(QString("SQRT*/C: sqrt(%1) = %2 (chopped)").arg(value).arg(result));
                break;
            }

            case FUNC_SQRTS_M: // SQRTS/M - Square Root S_floating (Round to Minus Infinity)
            case FUNC_SQRTT_M: // SQRTT/M - Square Root T_floating (Round to Minus Infinity)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);
                result = std::floor(result); // Round to minus infinity

                FWD(instruction.rc, result);
                DEBUG_LOG(QString("SQRT*/M: sqrt(%1) = %2 (round -∞)").arg(value).arg(result));
                break;
            }

            case FUNC_SQRTS_D: // SQRTS/D - Square Root S_floating (Round to Plus Infinity)
            case FUNC_SQRTT_D: // SQRTT/D - Square Root T_floating (Round to Plus Infinity)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);
                result = std::ceil(result); // Round to plus infinity

                FWD(instruction.rc, result);
                DEBUG_LOG(QString("SQRT*/D: sqrt(%1) = %2 (round +∞)").arg(value).arg(result));
                break;
            }

            // Unbiased SQRT operations
            case FUNC_SQRTF_U: // SQRTF/U - Square Root F_floating (Unbiased)
            case FUNC_SQRTS_U: // SQRTS/U - Square Root S_floating (Unbiased)
            case FUNC_SQRTG_U: // SQRTG/U - Square Root G_floating (Unbiased)
            case FUNC_SQRTT_U: // SQRTT/U - Square Root T_floating (Unbiased)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);
                result = m_cpu->applyUnbiasedRounding(result);

                FWD(instruction.rc, result);
                DEBUG_LOG(QString("SQRT*/U: sqrt(%1) = %2 (unbiased)").arg(value).arg(result));
                break;
            }

            // Unbiased + Checked SQRT operations
            case FUNC_SQRTF_UC: // SQRTF/UC - Square Root F_floating (Unbiased + Checked)
            case FUNC_SQRTS_UC: // SQRTS/UC - Square Root S_floating (Unbiased + Checked)
            case FUNC_SQRTG_UC: // SQRTG/UC - Square Root G_floating (Unbiased + Checked)
            case FUNC_SQRTT_UC: // SQRTT/UC - Square Root T_floating (Unbiased + Checked)
            {
                double value = FD(instruction.rb);
                if (value < 0.0)
                {
                    m_cpu->triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
                    result = m_cpu->getFloatingPointNaN();
                }
                else
                {
                    double result = std::sqrt(value);
                    result = m_cpu->applyUnbiasedRounding(result);
                    FWD(instruction.rc, result);
                }
                DEBUG_LOG(QString("SQRT*/UC: sqrt(%1) = checked & unbiased").arg(value));
                break;
            }

            // Scaled SQRT operations
            case FUNC_SQRTF_S: // SQRTF/S - Square Root F_floating (Scaled)
            case FUNC_SQRTG_S: // SQRTG/S - Square Root G_floating (Scaled)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);

                if (instruction.function == FUNC_SQRTF_S)
                {
                    result = m_cpu->scaleVaxFResult(result);
                    DEBUG_LOG("SQRTF/S: (scaled)");
                }
                else
                {
                    result = m_cpu->scaleVaxGResult(result);
                    DEBUG_LOG("SQRTG/S: (scaled)");
                }

                FWD(instruction.rc, result);
                break;
            }

            // Scaled + Checked SQRT operations
            case FUNC_SQRTF_SC: // SQRTF/SC - Square Root F_floating (Scaled + Checked)
            case FUNC_SQRTG_SC: // SQRTG/SC - Square Root G_floating (Scaled + Checked)
            {
                double value = FD(instruction.rb);
                if (value < 0.0)
                {
                    m_cpu->triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
                    result = m_cpu->getFloatingPointNaN();
                }
                else
                {
                    double result = std::sqrt(value);
                    if (instruction.function == FUNC_SQRTF_SC)
                    {
                        result = m_cpu->scaleVaxFResult(result);
                    }
                    else
                    {
                        result = m_cpu->scaleVaxGResult(result);
                    }
                    FWD(instruction.rc, result);
                }
                DEBUG_LOG(QString("SQRT*/SC: sqrt(%1) = scaled & checked").arg(value));
                break;
            }

            // Scaled + Unbiased SQRT operations
            case FUNC_SQRTF_SU: // SQRTF/SU - Square Root F_floating (Scaled + Unbiased)
            case FUNC_SQRTS_SU: // SQRTS/SU - Square Root S_floating (Scaled + Unbiased)
            case FUNC_SQRTG_SU: // SQRTG/SU - Square Root G_floating (Scaled + Unbiased)
            case FUNC_SQRTT_SU: // SQRTT/SU - Square Root T_floating (Scaled + Unbiased)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);
                result = m_cpu->applyUnbiasedRounding(result);

                switch (instruction.function)
                {
                case FUNC_SQRTF_SU:
                    result = m_cpu->scaleVaxFResult(result);
                    break;
                case FUNC_SQRTS_SU:
                    result = m_cpu->scaleIeeeSResult(result);
                    break;
                case FUNC_SQRTG_SU:
                    result = m_cpu->scaleVaxGResult(result);
                    break;
                case FUNC_SQRTT_SU:
                    result = m_cpu->scaleIeeeTResult(result);
                    break;
                }

                FWD(instruction.rc, result);
                DEBUG_LOG(QString("SQRT*/SU: sqrt(%1) = scaled & unbiased").arg(value));
                break;
            }

            // Scaled + Unbiased + Checked SQRT operations
            case FUNC_SQRTF_SUC: // SQRTF/SUC - Square Root F_floating (Scaled + Unbiased + Checked)
            case FUNC_SQRTS_SUC: // SQRTS/SUC - Square Root S_floating (Scaled + Unbiased + Checked)
            case FUNC_SQRTG_SUC: // SQRTG/SUC - Square Root G_floating (Scaled + Unbiased + Checked)
            case FUNC_SQRTT_SUC: // SQRTT/SUC - Square Root T_floating (Scaled + Unbiased + Checked)
            {
                double value = FD(instruction.rb);
                if (value < 0.0)
                {
                    m_cpu->triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
                    result = m_cpu->getFloatingPointNaN();
                }
                else
                {
                    double result = std::sqrt(value);
                    result = m_cpu->applyUnbiasedRounding(result);

                    switch (instruction.function)
                    {
                    case FUNC_SQRTF_SUC:
                        result = m_cpu->scaleVaxFResult(result);
                        break;
                    case FUNC_SQRTS_SUC:
                        result = m_cpu->scaleIeeeSResult(result);
                        break;
                    case FUNC_SQRTG_SUC:
                        result = m_cpu->scaleVaxGResult(result);
                        break;
                    case FUNC_SQRTT_SUC:
                        result = m_cpu->scaleIeeeTResult(result);
                        break;
                    }

                    FWD(instruction.rc, result);
                }
                DEBUG_LOG(QString("SQRT*/SUC: sqrt(%1) = scaled & unbiased & checked").arg(value));
                break;
            }

            // Additional SQRT variants with rounding modes
            case FUNC_SQRTS_UM:   // SQRTS/UM - Square Root S_floating (Unbiased, round -∞)
            case FUNC_SQRTT_UM:   // SQRTT/UM - Square Root T_floating (Unbiased, round -∞)
            case FUNC_SQRTS_UD:   // SQRTS/UD - Square Root S_floating (Unbiased, round +∞)
            case FUNC_SQRTT_UD:   // SQRTT/UD - Square Root T_floating (Unbiased, round +∞)
            case FUNC_SQRTS_SUM:  // SQRTS/SUM - Square Root S_floating (Scaled + Unbiased, round -∞)
            case FUNC_SQRTT_SUM:  // SQRTT/SUM - Square Root T_floating (Scaled + Unbiased, round -∞)
            case FUNC_SQRTS_SUD:  // SQRTS/SUD - Square Root S_floating (Scaled + Unbiased, round +∞)
            case FUNC_SQRTT_SUD:  // SQRTT/SUD - Square Root T_floating (Scaled + Unbiased, round +∞)
            case FUNC_SQRTS_SUI:  // SQRTS/SUI - Square Root S_floating (Scaled + Unbiased + Inexact)
            case FUNC_SQRTT_SUI:  // SQRTT/SUI - Square Root T_floating (Scaled + Unbiased + Inexact)
            case FUNC_SQRTS_SUIC: // SQRTS/SUIC - Square Root S_floating (Scaled + Unbiased + Inexact + Checked)
            case FUNC_SQRTT_SUIC: // SQRTT/SUIC - Square Root T_floating (Scaled + Unbiased + Inexact + Checked)
            case FUNC_SQRTS_SUIM: // SQRTS/SUIM - Square Root S_floating (Scaled + Unbiased + Inexact, round -∞)
            case FUNC_SQRTT_SUIM: // SQRTT/SUIM - Square Root T_floating (Scaled + Unbiased + Inexact, round -∞)
            case FUNC_SQRTS_SUID: // SQRTS/SUID - Square Root S_floating (Scaled + Unbiased + Inexact, round +∞)
            case FUNC_SQRTT_SUID: // SQRTT/SUID - Square Root T_floating (Scaled + Unbiased + Inexact, round +∞)
            {
                double value = FD(instruction.rb);
                double result = std::sqrt(value);

                // Apply appropriate rounding and scaling based on function
                result = m_cpu->applySqrtVariant(result, instruction.function);
                FWD(instruction.rc, result);

                DEBUG_LOG(
                    QString("SQRT variant 0x%1: sqrt(%2)").arg(instruction.function, 3, 16, QChar('0')).arg(value));
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented ITFP function 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }
            break;

        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_FLTV (0x15) - VAX Floating Point Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_FLTV:
            switch (instruction.function)
            {
            // ─────────────────────── VAX F_floating Operations ───────────────────────
            case FUNC_ADDF: // ADDF - Add F_floating
            {
                result = m_cpu->addFFormat(faValue, fbValue);
                DEBUG_LOG("ADDF: VAX F_floating addition");
                break;
            }

            case FUNC_SUBF: // SUBF - Subtract F_floating
            {
                result = m_cpu->subFFormat(faValue, fbValue);
                DEBUG_LOG("SUBF: VAX F_floating subtraction");
                break;
            }

            case FUNC_MULF: // MULF - Multiply F_floating
            {
                result = m_cpu->mulFFormat(faValue, fbValue);
                DEBUG_LOG("MULF: VAX F_floating multiplication");
                break;
            }

            case FUNC_DIVF: // DIVF - Divide F_floating
            {
                result = m_cpu->divFFormat(faValue, fbValue);
                DEBUG_LOG("DIVF: VAX F_floating division");
                break;
            }

            case FUNC_SQRTF: // SQRTF - Square Root F_floating
            {
                double value = m_cpu->convertFromVaxF(faValue);
                double sqrtResult = std::sqrt(value);
                result = m_cpu->convertToVaxF(sqrtResult);
                DEBUG_LOG("SQRTF: VAX F_floating square root");
                break;
            }

            case FUNC_CMPFEQ: // CMPFEQ - Compare F_floating Equal
            {
                result = m_cpu->compareFFormat(faValue, fbValue, AsaModes::FPCompareType::FP_EQUAL);
                DEBUG_LOG("CMPFEQ: VAX F_floating compare equal");
                break;
            }

            case FUNC_CMPFLT: // CMPFLT - Compare F_floating Less Than
            {
                result = m_cpu->compareFFormat(faValue, fbValue, AsaModes::FPCompareType::FP_LESS);
                DEBUG_LOG("CMPFLT: VAX F_floating compare less than");
                break;
            }

            case FUNC_CMPFLE: // CMPFLE - Compare F_floating Less Than or Equal
            {
                result = m_cpu->compareFFormat(faValue, fbValue, AsaModes::FPCompareType::FP_LESS_EQUAL);
                DEBUG_LOG("CMPFLE: VAX F_floating compare less than or equal");
                break;
            }

            // ─────────────────────── VAX G_floating Operations ───────────────────────
            case FUNC_ADDG: // ADDG - Add G_floating
            {
                result = m_cpu->addGFormat(faValue, fbValue);
                DEBUG_LOG("ADDG: VAX G_floating addition");
                break;
            }

            case FUNC_SUBG: // SUBG - Subtract G_floating
            {
                result = m_cpu->subGFormat(faValue, fbValue);
                DEBUG_LOG("SUBG: VAX G_floating subtraction");
                break;
            }

            case FUNC_MULG: // MULG - Multiply G_floating
            {
                result = m_cpu->mulGFormat(faValue, fbValue);
                DEBUG_LOG("MULG: VAX G_floating multiplication");
                break;
            }

            case FUNC_DIVG: // DIVG - Divide G_floating
            {
                result = m_cpu->divGFormat(faValue, fbValue);
                DEBUG_LOG("DIVG: VAX G_floating division");
                break;
            }

            case FUNC_SQRTG: // SQRTG - Square Root G_floating
            {
                double value = m_cpu->convertFromVaxG(faValue);
                double sqrtResult = std::sqrt(value);
                result = m_cpu->convertToVaxG(sqrtResult);
                DEBUG_LOG("SQRTG: VAX G_floating square root");
                break;
            }

            case FUNC_CMPGEQ: // CMPGEQ - Compare G_floating Equal
            {
                result = m_cpu->compareGFormat(faValue, fbValue, FPCompareType::FP_EQUAL);
                DEBUG_LOG("CMPGEQ: VAX G_floating compare equal");
                break;
            }

            case FUNC_CMPGLT: // CMPGLT - Compare G_floating Less Than
            {
                result = m_cpu->compareGFormat(faValue, fbValue, FPCompareType::FP_LESS);
                DEBUG_LOG("CMPGLT: VAX G_floating compare less than");
                break;
            }

            case FUNC_CMPGLE: // CMPGLE - Compare G_floating Less Than or Equal
            {
                result = m_cpu->compareGFormat(faValue, fbValue, FPCompareType::FP_LESS_EQUAL);
                DEBUG_LOG("CMPGLE: VAX G_floating compare less than or equal");
                break;
            }

            // ─────────────────────── VAX D_floating Operations ───────────────────────
            case FUNC_ADDD: // ADDD - Add D_floating
            {
                result = m_cpu->addDFormat(faValue, fbValue);
                DEBUG_LOG("ADDD: VAX D_floating addition");
                break;
            }

            case FUNC_SUBD: // SUBD - Subtract D_floating
            {
                result = m_cpu->subDFormat(faValue, fbValue);
                DEBUG_LOG("SUBD: VAX D_floating subtraction");
                break;
            }

            case FUNC_MULD: // MULD - Multiply D_floating
            {
                result = m_cpu->mulDFormat(faValue, fbValue);
                DEBUG_LOG("MULD: VAX D_floating multiplication");
                break;
            }

            case FUNC_DIVD: // DIVD - Divide D_floating
            {
                result = m_cpu->divDFormat(faValue, fbValue);
                DEBUG_LOG("DIVD: VAX D_floating division");
                break;
            }

            case FUNC_SQRTD: // SQRTD - Square Root D_floating
            {
                double value = m_cpu->convertFromVaxD(faValue);
                double sqrtResult = std::sqrt(value);
                result = m_cpu->convertToVaxD(sqrtResult);
                DEBUG_LOG("SQRTD: VAX D_floating square root");
                break;
            }

            // ─────────────────────── VAX Format Conversions ───────────────────────
            case FUNC_CVTQF_C:  // CVTQF/C - Convert Quadword to F_floating (Chopped)
            case FUNC_CVTQF:    // CVTQF - Convert Quadword to F_floating
            case FUNC_CVTQF_UC: // CVTQF/UC - Convert Quadword to F_floating (Unbiased Chopped)
            {
                result = m_cpu->convertQuadToF(raValue, instruction.function);
                DEBUG_LOG(QString("CVTQF variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTQG_C:  // CVTQG/C - Convert Quadword to G_floating (Chopped)
            case FUNC_CVTQG:    // CVTQG - Convert Quadword to G_floating
            case FUNC_CVTQG_UC: // CVTQG/UC - Convert Quadword to G_floating (Unbiased Chopped)
            {
                result = m_cpu->convertQuadToG(raValue, instruction.function);
                DEBUG_LOG(QString("CVTQG variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTGF_C:  // CVTGF/C - Convert G_floating to F_floating (Chopped)
            case FUNC_CVTGF:    // CVTGF - Convert G_floating to F_floating
            case FUNC_CVTGF_UC: // CVTGF/UC - Convert G_floating to F_floating (Unbiased Chopped)
            {
                result = m_cpu->convertGToF(faValue, instruction.function);
                DEBUG_LOG(QString("CVTGF variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTGD_C:  // CVTGD/C - Convert G_floating to D_floating (Chopped)
            case FUNC_CVTGD:    // CVTGD - Convert G_floating to D_floating
            case FUNC_CVTGD_UC: // CVTGD/UC - Convert G_floating to D_floating (Unbiased Chopped)
            {
                result = m_cpu->convertGToD(faValue, instruction.function);
                DEBUG_LOG(QString("CVTGD variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTGQ_C:  // CVTGQ/C - Convert G_floating to Quadword (Chopped)
            case FUNC_CVTGQ:    // CVTGQ - Convert G_floating to Quadword
            case FUNC_CVTGQ_VC: // CVTGQ/VC - Convert G_floating to Quadword (Overflow Checked)
            case FUNC_CVTGQ_V:  // CVTGQ/V - Convert G_floating to Quadword (Overflow)
            {
                result = m_cpu->convertGToQuad(faValue, instruction.function);
                DEBUG_LOG(QString("CVTGQ variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTFG:   // CVTFG - Convert F_floating to G_floating
            case FUNC_CVTFQ:   // CVTFQ - Convert F_floating to Quadword
            case FUNC_CVTFQ_V: // CVTFQ/V - Convert F_floating to Quadword (Overflow)
            {
                result = m_cpu->convertFToOther(faValue, instruction.function);
                DEBUG_LOG(QString("CVTF* variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTDG: // CVTDG - Convert D_floating to G_floating
            {
                result = m_cpu->convertDToG(faValue);
                DEBUG_LOG("CVTDG: D_floating to G_floating conversion");
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented FLTV function 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }
            break;

        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_FLTI (0x16) - IEEE Floating Point Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_FLTI:
            switch (instruction.function)
            {
            // ─────────────────────── IEEE S_floating Operations ───────────────────────
            case FUNC_ADDS_C: // ADDS/C - Add S_floating (Chopped)
            case FUNC_ADDS_M: // ADDS/M - Add S_floating (Round to Minus Infinity)
            case FUNC_ADDS:   // ADDS - Add S_floating (Round to Nearest)
            case FUNC_ADDS_D: // ADDS/D - Add S_floating (Round to Plus Infinity)
            {
                result = m_cpu->addSFormat(faValue, fbValue, instruction.function);
                DEBUG_LOG(QString("ADDS variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_SUBS_C: // SUBS/C - Subtract S_floating (Chopped)
            case FUNC_SUBS_M: // SUBS/M - Subtract S_floating (Round to Minus Infinity)
            case FUNC_SUBS:   // SUBS - Subtract S_floating (Round to Nearest)
                // case FUNC_SUBS_D: // SUBS/D - Subtract S_floating (Round to Plus Infinity)
                {
                    result = m_cpu->subSFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("SUBS variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            case FUNC_MULS_C: // MULS/C - Multiply S_floating (Chopped)
            case FUNC_MULS_M: // MULS/M - Multiply S_floating (Round to Minus Infinity)
            case FUNC_MULS:   // MULS - Multiply S_floating (Round to Nearest)
                // case FUNC_MULS_D: // MULS/D - Multiply S_floating (Round to Plus Infinity)
                {
                    result = m_cpu->mulSFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("MULS variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            case FUNC_DIVS_C: // DIVS/C - Divide S_floating (Chopped)
            case FUNC_DIVS_M: // DIVS/M - Divide S_floating (Round to Minus Infinity)
            case FUNC_DIVS:   // DIVS - Divide S_floating (Round to Nearest)
                // case FUNC_DIVS_D: // DIVS/D - Divide S_floating (Round to Plus Infinity)
                {
                    result = m_cpu->divSFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("DIVS variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            // ─────────────────────── IEEE T_floating Operations ───────────────────────
            case FUNC_ADDT_C: // ADDT/C - Add T_floating (Chopped)
            case FUNC_ADDT_M: // ADDT/M - Add T_floating (Round to Minus Infinity)
            case FUNC_ADDT:   // ADDT - Add T_floating (Round to Nearest)
                // case FUNC_ADDT_D: // ADDT/D - Add T_floating (Round to Plus Infinity)
                {
                    result = m_cpu->addTFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("ADDT variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            case FUNC_SUBT_C: // SUBT/C - Subtract T_floating (Chopped)
            case FUNC_SUBT_M: // SUBT/M - Subtract T_floating (Round to Minus Infinity)
                // case FUNC_SUBT:   // SUBT - Subtract T_floating (Round to Nearest)
                // case FUNC_SUBT_D: // SUBT/D - Subtract T_floating (Round to Plus Infinity)
                {
                    result = m_cpu->subTFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("SUBT variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            case FUNC_MULT_C: // MULT/C - Multiply T_floating (Chopped)
            case FUNC_MULT_M: // MULT/M - Multiply T_floating (Round to Minus Infinity)
            case FUNC_MULT:   // MULT - Multiply T_floating (Round to Nearest)
                // case FUNC_MULT_D: // MULT/D - Multiply T_floating (Round to Plus Infinity)
                {
                    result = m_cpu->mulTFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("MULT variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            case FUNC_DIVT_C: // DIVT/C - Divide T_floating (Chopped)
            case FUNC_DIVT_M: // DIVT/M - Divide T_floating (Round to Minus Infinity)
                // case FUNC_DIVT:   // DIVT - Divide T_floating (Round to Nearest)
                // case FUNC_DIVT_D: // DIVT/D - Divide T_floating (Round to Plus Infinity)
                {
                    result = m_cpu->divTFormat(faValue, fbValue, instruction.function);
                    DEBUG_LOG(QString("DIVT variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                    break;
                }

            // ─────────────────────── IEEE Compare Operations ───────────────────────
            case FUNC_CMPTUN: // CMPTUN - Compare T_floating Unordered
            {
                result = m_cpu->compareTFormat(faValue, fbValue, FPCompareType::FP_UNORDERED);
                DEBUG_LOG("CMPTUN: IEEE T_floating compare unordered");
                break;
            }

            case FUNC_CMPTEQ: // CMPTEQ - Compare T_floating Equal
            {
                result = m_cpu->compareTFormat(faValue, fbValue, FPCompareType::FP_EQUAL);
                DEBUG_LOG("CMPTEQ: IEEE T_floating compare equal");
                break;
            }

            case FUNC_CMPTLT: // CMPTLT - Compare T_floating Less Than
            {
                result = m_cpu->compareTFormat(faValue, fbValue, FPCompareType::FP_LESS);
                DEBUG_LOG("CMPTLT: IEEE T_floating compare less than");
                break;
            }

            case FUNC_CMPTLE: // CMPTLE - Compare T_floating Less Than or Equal
            {
                result = m_cpu->compareTFormat(faValue, fbValue, FPCompareType::FP_LESS_EQUAL);
                DEBUG_LOG("CMPTLE: IEEE T_floating compare less than or equal");
                break;
            }

            case FUNC_SQRTT: // SQRTT - Square Root T_floating
            {
                double value = FD(instruction.rb);
                double sqrtResult = std::sqrt(value);
                result = m_cpu->convertToIeeeT(sqrtResult);
                DEBUG_LOG("SQRTT: IEEE T_floating square root");
                break;
            }

            // ─────────────────────── IEEE Compare with Software Completion ───────────────────────
            case FUNC_CMPTUNS: // CMPTUNS - Compare T_floating Unordered (Signaling)
            {
                result =
                    m_cpu->compareTFormatSignaling(faValue, fbValue, FPCompareType::FP_UNORDERED);
                DEBUG_LOG("CMPTUNS: IEEE T_floating compare unordered (signaling)");
                break;
            }

            case FUNC_CMPTEQS: // CMPTEQS - Compare T_floating Equal (Signaling)
            {
                result = m_cpu->compareTFormatSignaling(faValue, fbValue, FPCompareType::FP_EQUAL);
                DEBUG_LOG("CMPTEQS: IEEE T_floating compare equal (signaling)");
                break;
            }

            case FUNC_CMPTLTS: // CMPTLTS - Compare T_floating Less Than (Signaling)
            {
                result = m_cpu->compareTFormatSignaling(faValue, fbValue, FPCompareType::FP_LESS);
                DEBUG_LOG("CMPTLTS: IEEE T_floating compare less than (signaling)");
                break;
            }

            case FUNC_CMPTLES: // CMPTLES - Compare T_floating Less Than or Equal (Signaling)
            {
                result =
                    m_cpu->compareTFormatSignaling(faValue, fbValue, FPCompareType::FP_LESS_EQUAL);
                DEBUG_LOG("CMPTLES: IEEE T_floating compare less than or equal (signaling)");
                break;
            }

            // ─────────────────────── IEEE Format Conversions ───────────────────────
            case FUNC_CVTQS_C: // CVTQS/C - Convert Quadword to S_floating (Chopped)
            case FUNC_CVTQS_M: // CVTQS/M - Convert Quadword to S_floating (Round to Minus Infinity)
            case FUNC_CVTQS:   // CVTQS - Convert Quadword to S_floating (Round to Nearest)
            case FUNC_CVTQS_D: // CVTQS/D - Convert Quadword to S_floating (Round to Plus Infinity)
            {
                result = m_cpu->convertQuadToS(raValue, instruction.function);
                DEBUG_LOG(QString("CVTQS variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTQT_C: // CVTQT/C - Convert Quadword to T_floating (Chopped)
            case FUNC_CVTQT_M: // CVTQT/M - Convert Quadword to T_floating (Round to Minus Infinity)
            // case FUNC_CVTQT:   // CVTQT - Convert Quadword to T_floating (Round to Nearest)
            case FUNC_CVTQT_D: // CVTQT/D - Convert Quadword to T_floating (Round to Plus Infinity)
            {
                result = m_cpu->convertQuadToT(raValue, instruction.function);
                DEBUG_LOG(QString("CVTQT variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            case FUNC_CVTST: // CVTST - Convert S_floating to T_floating
            {
                result = m_cpu->convertSToT(faValue);
                DEBUG_LOG("CVTST: S_floating to T_floating conversion");
                break;
            }

            case FUNC_CVTTS: // CVTTS - Convert T_floating to S_floating
            {
                result = m_cpu->convertTToS(faValue);
                DEBUG_LOG("CVTTS: T_floating to S_floating conversion");
                break;
            }

            case FUNC_CVTTSC: // CVTTSC - Convert T_floating to S_floating (Chopped)
            {
                result = m_cpu->convertTToSChopped(faValue);
                DEBUG_LOG("CVTTSC: T_floating to S_floating (chopped)");
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented FLTI function 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }
            break;

        // ═══════════════════════════════════════════════════════════════════════════
        // OPCODE_FLTL (0x17) - Floating Point Function Operations
        // ═══════════════════════════════════════════════════════════════════════════
        case OPCODE_FLTL:
            switch (instruction.function)
            {
            // ─────────────────────── IEEE T_floating to Quadword Conversions ───────────────────────
            case FUNC_CVTTQ_C:   // CVTTQ/C - Convert T_floating to Quadword (Chopped)
            case FUNC_CVTTQ_VC:  // CVTTQ/VC - Convert T_floating to Quadword with overflow trap (Chopped)
            case FUNC_CVTTQ_SC:  // CVTTQ/SC - Convert T_floating to Quadword with software completion (Chopped)
            case FUNC_CVTTQ_SVC: // CVTTQ/SVC - Convert T_floating to Quadword, both traps (Chopped)
            // case FUNC_CVTTQ:     // CVTTQ - Convert T_floating to Quadword (Round to Nearest)
            case FUNC_CVTTQ_V:  // CVTTQ/V - Convert T_floating to Quadword with overflow trap
            case FUNC_CVTTQ_S:  // CVTTQ/S - Convert T_floating to Quadword with software completion
            case FUNC_CVTTQ_SV: // CVTTQ/SV - Convert T_floating to Quadword, both traps
            {
                result = m_cpu->convertTToQuad(faValue, instruction.function);
                DEBUG_LOG(QString("CVTTQ variant 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                break;
            }

            // ─────────────────────── Floating Point Conditional Moves ───────────────────────
            case FUNC_FCMOVEQ: // FCMOVEQ - Floating Conditional Move if Equal
            {
                result = m_cpu->floatConditionalMove(faValue, fbValue, FPCondition::FP_EQUAL,
                                                     F(instruction.rc));
                DEBUG_LOG("FCMOVEQ: Floating conditional move if equal");
                break;
            }

            case FUNC_FCMOVNE: // FCMOVNE - Floating Conditional Move if Not Equal
            {
                result = m_cpu->floatConditionalMove(faValue, fbValue, FPCondition::FP_NOT_EQUAL,
                                                     F(instruction.rc));
                DEBUG_LOG("FCMOVNE: Floating conditional move if not equal");
                break;
            }

            case FUNC_FCMOVLT: // FCMOVLT - Floating Conditional Move if Less Than
            {
                result = m_cpu->floatConditionalMove(faValue, fbValue, FPCondition::FP_LESS_THAN,
                                                     F(instruction.rc));
                DEBUG_LOG("FCMOVLT: Floating conditional move if less than");
                break;
            }

            case FUNC_FCMOVGE: // FCMOVGE - Floating Conditional Move if Greater or Equal
            {
                result = m_cpu->floatConditionalMove(faValue, fbValue, FPCondition::FP_GREATER_EQUAL,
                                                     F(instruction.rc));
                DEBUG_LOG("FCMOVGE: Floating conditional move if greater or equal");
                break;
            }

            case FUNC_FCMOVLE: // FCMOVLE - Floating Conditional Move if Less or Equal
            {
                result = m_cpu->floatConditionalMove(faValue, fbValue, FPCondition::FP_LESS_EQUAL,
                                                     F(instruction.rc));
                DEBUG_LOG("FCMOVLE: Floating conditional move if less or equal");
                break;
            }

            case FUNC_FCMOVGT: // FCMOVGT - Floating Conditional Move if Greater Than
            {
                result = m_cpu->floatConditionalMove(faValue, fbValue, FPCondition::FP_GREATER_THAN,
                                                     F(instruction.rc));
                DEBUG_LOG("FCMOVGT: Floating conditional move if greater than");
                break;
            }

            // ─────────────────────── Sign Manipulation Operations ───────────────────────
            case FUNC_CPYS: // CPYS - Copy Sign
            {
                result = m_cpu->copySign(faValue, fbValue);
                DEBUG_LOG("CPYS: Copy sign operation");
                break;
            }

            case FUNC_CPYSN: // CPYSN - Copy Sign Negate
            {
                result = m_cpu->copySignNegate(faValue, fbValue);
                DEBUG_LOG("CPYSN: Copy sign negate operation");
                break;
            }

            case FUNC_CPYSE: // CPYSE - Copy Sign and Exponent
            {
                result = m_cpu->copySignExponent(faValue, fbValue);
                DEBUG_LOG("CPYSE: Copy sign and exponent operation");
                break;
            }

            // ─────────────────────── FPCR Operations ───────────────────────
            case FUNC_MT_FPCR: // MT_FPCR - Move to Floating-Point Control Register
            {
                m_cpu->setFPCR(faValue);
                result = 0; // No destination register
                DEBUG_LOG("MT_FPCR: Move to FPCR");
                break;
            }

            case FUNC_MF_FPCR: // MF_FPCR - Move from Floating-Point Control Register
            {
                result = m_cpu->getFPCR();
                DEBUG_LOG("MF_FPCR: Move from FPCR");
                break;
            }

            // ─────────────────────── Quadword/Longword Conversions ───────────────────────
            case FUNC_CVTLQ: // CVTLQ - Convert Longword to Quadword
            {
                qint32 longwordValue = static_cast<qint32>(faValue);
                result = static_cast<quint64>(static_cast<qint64>(longwordValue));
                DEBUG_LOG(QString("CVTLQ: Convert longword %1 to quadword 0x%2")
                              .arg(longwordValue)
                              .arg(result, 16, 16, QChar('0')));
                break;
            }

            case FUNC_CVTQL: // CVTQL - Convert Quadword to Longword
            {
                qint64 quadwordValue = static_cast<qint64>(faValue);
                qint32 longwordValue = static_cast<qint32>(quadwordValue);
                result = static_cast<quint64>(static_cast<qint64>(longwordValue));
                DEBUG_LOG(QString("CVTQL: Convert quadword 0x%1 to longword %2")
                              .arg(faValue, 16, 16, QChar('0'))
                              .arg(longwordValue));
                break;
            }

            case FUNC_CVTQLV: // CVTQLV - Convert Quadword to Longword with overflow
            {
                qint64 quadwordValue = static_cast<qint64>(faValue);

                // Check for overflow
                if (quadwordValue > INT32_MAX || quadwordValue < INT32_MIN)
                {
                    m_cpu->triggerException(ExceptionType::ARITHMETIC_TRAP, m_cpu->getPc());
                }

                qint32 longwordValue = static_cast<qint32>(quadwordValue);
                result = static_cast<quint64>(static_cast<qint64>(longwordValue));
                DEBUG_LOG(QString("CVTQLV: Convert quadword 0x%1 to longword %2 (with overflow check)")
                              .arg(faValue, 16, 16, QChar('0'))
                              .arg(longwordValue));
                break;
            }

            case FUNC_CVTQLSV: // CVTQLSV - Convert Quadword to Longword with software completion
            {
                result = m_cpu->convertQuadToLongwordSoftware(faValue);
                DEBUG_LOG("CVTQLSV: Convert quadword to longword (software completion)");
                break;
            }

            default:
                DEBUG_LOG(QString("Unimplemented FLTL function 0x%1").arg(instruction.function, 3, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }
            break;

        default:
            DEBUG_LOG(QString("executeFloatingPointGroup: Unknown floating-point opcode 0x%1")
                          .arg(instruction.opcode, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            return;
        }

        // Store result in floating-point register (if not R31)
        if (instruction.rc != 31)
        {
            FW(instruction.rc, result);
        }

        // Check for floating-point exceptions
        if (m_cpu->checkFloatingPointExceptions())
        {
            m_cpu->triggerException(ExceptionType::FP_EXCEPTION, m_cpu->getPc());
        }

        // Update instruction statistics
        m_floatingPointInstructions++;
        m_totalInstructions++;
    }

    // Enhanced executePALGroup using your existing constexpr constants from JitPALConstants.h
    // This consolidates all PAL instruction handling into a single method

    void executePALGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        // Verify we're in a privileged mode to execute PAL instructions
        if (!m_cpu->isPrivilegedMode())
        {
            DEBUG_LOG("PAL instruction executed in non-privileged mode");
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            return;
        }

        // Extract PAL function code from instruction (26-bit function field)
        quint32 palFunction = instruction.rawInstruction & FUNC_26_MASK;
        quint8 ra = instruction.ra;
        quint8 rb = instruction.rb;
        quint8 rc = instruction.rc;

        DEBUG_LOG(QString("Executing PAL function 0x%1 (Ra=%2, Rb=%3, Rc=%4)")
                      .arg(palFunction, 8, 16, QChar('0'))
                      .arg(ra)
                      .arg(rb)
                      .arg(rc));

        switch (palFunction)
        {
            // ═══════════════════════════════════════════════════════════════════════════
            // System Control and Basic PAL Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_HALT: // Processor halt - but this constant doesn't exist in your file
        {
            DEBUG_LOG("PAL_HALT: Halting processor");
            m_cpu->halt();
            break;
        }

        case PAL_CFLUSH: // Cache flush - but this constant doesn't exist in your file
        {
            DEBUG_LOG("PAL_CFLUSH: Flushing caches");
            m_cpu->getMemorySystem()->flushAllCaches();
            break;
        }

        case PAL_CSERVE: // Console service
        {
            DEBUG_LOG("PAL_CSERVE: Console service operation");
            m_cpu->executeConsoleService(R(rb));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Context and Process Management
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_SWPCTX: // Switch process context
        {
            quint64 newPcb = R(rb);
            quint64 oldPcb = m_cpu->getCurrentPCB();
            m_cpu->switchContext(newPcb);
            RW(ra, oldPcb);
            DEBUG_LOG(QString("PAL_SWPCTX: Context switched from 0x%1 to 0x%2")
                          .arg(oldPcb, 16, 16, QChar('0'))
                          .arg(newPcb, 16, 16, QChar('0')));
            break;
        }

        case PAL_SWPPAL: // Switch to new PALcode base
        {
            quint64 newPalBase = R(rb);
            quint64 oldPalBase = m_cpu->getPALBase();
            m_cpu->setPALBase(newPalBase);
            RW(ra, oldPalBase);
            DEBUG_LOG(QString("PAL_SWPPAL: PAL base changed from 0x%1 to 0x%2")
                          .arg(oldPalBase, 16, 16, QChar('0'))
                          .arg(newPalBase, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Interrupt Priority Level Management
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_SWPIRQL: // Set processor interrupt priority level
        {
            quint64 newIpl = R(rb) & 0x1F; // 5 bits
            quint64 oldIpl = m_cpu->getCurrentIPL();
            m_cpu->setIPL(newIpl);
            RW(ra, oldIpl);
            DEBUG_LOG(QString("PAL_SWPIRQL: IPL changed from %1 to %2").arg(oldIpl).arg(newIpl));
            break;
        }

        case PAL_RDIRQL: // Read current interrupt priority level
        {
            quint64 currentIpl = m_cpu->getCurrentIPL();
            RW(ra, currentIpl);
            DEBUG_LOG(QString("PAL_RDIRQL: Read IPL = %1").arg(currentIpl));
            break;
        }

        case PAL_DI: // Disable interrupts (raise IPL)
        {
            quint64 oldIpl = m_cpu->getCurrentIPL();
            m_cpu->setIPL(31); // Maximum IPL disables all interrupts
            RW(ra, oldIpl);
            DEBUG_LOG(QString("PAL_DI: Interrupts disabled, old IPL = %1").arg(oldIpl));
            break;
        }

        case PAL_EI: // Enable interrupts (lower IPL)
        {
            quint64 oldIpl = m_cpu->getCurrentIPL();
            quint64 newIpl = R(rb) & 0x1F;
            m_cpu->setIPL(newIpl);
            RW(ra, oldIpl);
            DEBUG_LOG(QString("PAL_EI: Interrupts enabled, IPL %1 -> %2").arg(oldIpl).arg(newIpl));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Memory Management and TLB Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_TBIA: // Translation buffer invalidate all
        {
            DEBUG_LOG("PAL_TBIA: Invalidating all TLB entries");
            m_cpu->getMMU()->invalidateAllTLB();
            break;
        }

        case PAL_TBIS: // Translation buffer invalidate single
        {
            quint64 vaddr = R(rb);
            m_cpu->getMMU()->invalidateTLBEntry(vaddr);
            DEBUG_LOG(QString("PAL_TBIS: Invalidated TLB entry for VA 0x%1").arg(vaddr, 16, 16, QChar('0')));
            break;
        }

        case PAL_TBI: // Translation buffer operation (general)
        {
            quint64 operation = R(rb);
            quint64 address = R(rc);

            switch (operation)
            {
            case 0: // Invalidate all
                m_cpu->getMMU()->invalidateAllTLB();
                DEBUG_LOG("PAL_TBI: Invalidate all TLB entries");
                break;
            case 1: // Invalidate single
                m_cpu->getMMU()->invalidateTLBEntry(address);
                DEBUG_LOG(QString("PAL_TBI: Invalidate single TLB entry 0x%1").arg(address, 16, 16, QChar('0')));
                break;
            case 2: // Invalidate ASN
                m_cpu->getMMU()->invalidateTLBByASN(address & 0xFF);
                DEBUG_LOG(QString("PAL_TBI: Invalidate TLB for ASN %1").arg(address & 0xFF));
                break;
            default:
                DEBUG_LOG(QString("PAL_TBI: Unknown operation %1").arg(operation));
                break;
            }
            break;
        }

        case PAL_WRVPTPTR: // Write virtual page table pointer
        {
            quint64 vptPtr = R(rb);
            m_cpu->setVirtualPageTablePointer(vptPtr);
            DEBUG_LOG(QString("PAL_WRVPTPTR: Set VPT pointer to 0x%1").arg(vptPtr, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Physical Memory Access
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_LDQP: // Load quadword physical
        {
            quint64 physAddr = R(rb);
            quint64 value = m_cpu->getMemorySystem()->readPhysical64(physAddr);
            RW(ra, value);
            DEBUG_LOG(QString("PAL_LDQP: Loaded 0x%1 from physical 0x%2")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(physAddr, 16, 16, QChar('0')));
            break;
        }

        case PAL_STQP: // Store quadword physical
        {
            quint64 physAddr = R(rb);
            quint64 value = R(ra);
            m_cpu->getMemorySystem()->writePhysical64(physAddr, value);
            DEBUG_LOG(QString("PAL_STQP: Stored 0x%1 to physical 0x%2")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(physAddr, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Processor Status and Control Registers
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RDPS: // Read processor status
        {
            quint64 ps = m_cpu->getProcessorStatus();
            RW(ra, ps);
            DEBUG_LOG(QString("PAL_RDPS: Read PS = 0x%1").arg(ps, 16, 16, QChar('0')));
            break;
        }

        case PAL_WRFEN: // Write floating-point enable
        {
            quint64 fenValue = R(rb);
            m_cpu->setFloatingPointEnable(fenValue & 1);
            DEBUG_LOG(QString("PAL_WRFEN: Set FEN = %1").arg(fenValue & 1));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Stack Pointer Management
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_WRUSP: // Write user stack pointer
        {
            quint64 usp = R(rb);
            m_cpu->setUserStackPointer(usp);
            DEBUG_LOG(QString("PAL_WRUSP: Set USP to 0x%1").arg(usp, 16, 16, QChar('0')));
            break;
        }

        case PAL_RDUSP: // Read user stack pointer
        {
            quint64 usp = m_cpu->getUserStackPointer();
            RW(ra, usp);
            DEBUG_LOG(QString("PAL_RDUSP: Read USP = 0x%1").arg(usp, 16, 16, QChar('0')));
            break;
        }

        case PAL_WRKGP: // Write kernel global pointer
        {
            quint64 kernelGP = R(rb);
            m_cpu->setKernelGlobalPointer(kernelGP);
            DEBUG_LOG(QString("PAL_WRKGP: Set kernel GP to 0x%1").arg(kernelGP, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Machine Check and Error Handling
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RDMCES: // Read machine check error summary
        {
            quint64 mces = m_cpu->getMachineCheckErrorSummary();
            RW(ra, mces);
            DEBUG_LOG(QString("PAL_RDMCES: Read MCES = 0x%1").arg(mces, 16, 16, QChar('0')));
            break;
        }

        case PAL_WRMCES: // Write machine check error summary
        {
            quint64 mces = R(rb);
            m_cpu->setMachineCheckErrorSummary(mces);
            DEBUG_LOG(QString("PAL_WRMCES: Set MCES to 0x%1").arg(mces, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Trap and Exception Management
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_WRENT: // Write system entry points
        {
            quint64 entryType = R(ra);
            quint64 entryAddress = R(rb);
            m_cpu->setSystemEntryPoint(entryType, entryAddress);
            DEBUG_LOG(
                QString("PAL_WRENT: Set entry point %1 to 0x%2").arg(entryType).arg(entryAddress, 16, 16, QChar('0')));
            break;
        }

        case PAL_WTKTRP: // Write trap vector
        {
            quint64 trapVector = R(rb);
            m_cpu->setTrapVector(trapVector);
            DEBUG_LOG(QString("PAL_WTKTRP: Set trap vector to 0x%1").arg(trapVector, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Performance Monitoring
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RDPERFMON: // Read performance monitor
        {
            quint64 counterNum = R(rb) & 0x3; // 2 bits
            quint64 counterValue = m_cpu->readPerformanceCounter(counterNum);
            RW(ra, counterValue);
            DEBUG_LOG(QString("PAL_RDPERFMON: Read PMC%1 = %2").arg(counterNum).arg(counterValue));
            break;
        }

        case PAL_WRPERFMON: // Write performance monitor
        {
            quint64 counterNum = R(rb) & 0x3;
            quint64 counterValue = R(rc);
            m_cpu->writePerformanceCounter(counterNum, counterValue);
            DEBUG_LOG(QString("PAL_WRPERFMON: Set PMC%1 = %2").arg(counterNum).arg(counterValue));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Diagnostic and Value Services
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RDVAL: // Read processor value (implementation specific)
        {
            quint64 processorValue = m_cpu->getProcessorValue();
            RW(ra, processorValue);
            DEBUG_LOG(QString("PAL_RDVAL: Read processor value = 0x%1").arg(processorValue, 16, 16, QChar('0')));
            break;
        }

        case PAL_WRVAL: // Write processor value (implementation specific)
        {
            quint64 value = R(rb);
            m_cpu->setProcessorValue(value);
            DEBUG_LOG(QString("PAL_WRVAL: Set processor value to 0x%1").arg(value, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Console and Debug Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_BPT: // Breakpoint trap
        {
            DEBUG_LOG("PAL_BPT: Breakpoint trap");
            m_cpu->triggerException(ExceptionType::BREAKPOINT_TRAP, m_cpu->getPc());
            break;
        }

        case PAL_BUGCHK: // Bug check
        {
            quint64 bugcheckCode = R(ra);
            DEBUG_LOG(QString("PAL_BUGCHK: Bug check with code 0x%1").arg(bugcheckCode, 16, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::BUGCHECK, bugcheckCode);
            break;
        }

        case PAL_OPCDEC: // Opcode reserved for Digital
        {
            DEBUG_LOG("PAL_OPCDEC: Reserved opcode");
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Memory Barrier Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_IMB: // I-stream memory barrier
        {
            DEBUG_LOG("PAL_IMB: Instruction stream memory barrier");
            m_cpu->instructionMemoryBarrier();
            break;
        }

        case PAL_EXCB: // Exception barrier
        {
            DEBUG_LOG("PAL_EXCB: Exception barrier");
            m_cpu->exceptionBarrier();
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Interlocked Queue Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_INSQHILE: // Insert entry into head interlocked longword queue, exit
        case PAL_INSQTILE: // Insert entry into tail interlocked longword queue, exit
        case PAL_INSQHIQE: // Insert entry into head interlocked quadword queue, exit
        case PAL_INSQTIQE: // Insert entry into tail interlocked quadword queue, exit
        case PAL_REMQHILE: // Remove entry from head interlocked longword queue, exit
        case PAL_REMQTILE: // Remove entry from tail interlocked longword queue, exit
        case PAL_REMQHIQE: // Remove entry from head interlocked quadword queue, exit
        case PAL_REMQTIQE: // Remove entry from tail interlocked quadword queue, exit
        {
            // These are complex interlocked queue operations
            // Implementation depends on your specific queue management needs
            DEBUG_LOG(QString("PAL interlocked queue operation 0x%1").arg(palFunction, 3, 16, QChar('0')));
            m_cpu->executeInterlockedQueueOperation(palFunction, R(ra), R(rb), R(rc));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Memory Probing Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_PROBEW: // Probe write access
        {
            quint64 address = R(rb);
            bool canWrite = m_cpu->probeWriteAccess(address);
            RW(ra, canWrite ? 1 : 0);
            DEBUG_LOG(QString("PAL_PROBEW: Probe write to 0x%1 = %2")
                          .arg(address, 16, 16, QChar('0'))
                          .arg(canWrite ? "OK" : "FAULT"));
            break;
        }

        case PAL_PROBER: // Probe read access
        {
            quint64 address = R(rb);
            bool canRead = m_cpu->probeReadAccess(address);
            RW(ra, canRead ? 1 : 0);
            DEBUG_LOG(QString("PAL_PROBER: Probe read from 0x%1 = %2")
                          .arg(address, 16, 16, QChar('0'))
                          .arg(canRead ? "OK" : "FAULT"));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Privileged Instruction Handling
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_PRIV: // Privileged instruction
        {
            DEBUG_LOG("PAL_PRIV: Privileged instruction");
            m_cpu->triggerException(ExceptionType::PRIVILEGED_INSTRUCTION, m_cpu->getPc());
            break;
        }

        case PAL_CALLPRIV: // Call privileged instruction
        {
            quint64 privFunction = R(rb);
            DEBUG_LOG(QString("PAL_CALLPRIV: Call privileged function 0x%1").arg(privFunction, 16, 16, QChar('0')));
            m_cpu->executePrivilegedFunction(privFunction);
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Console PAL Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_CONSHALT: // Console halt
        {
            DEBUG_LOG("PAL_CONSHALT: Console halt");
            m_cpu->consoleHalt();
            break;
        }

        case PAL_CONSENV: // Console environment
        {
            quint64 envOperation = R(rb);
            DEBUG_LOG(QString("PAL_CONSENV: Console environment operation %1").arg(envOperation));
            m_cpu->consoleEnvironment(envOperation);
            break;
        }

        case PAL_CONSINIT: // Console initialization
        {
            DEBUG_LOG("PAL_CONSINIT: Console initialization");
            m_cpu->consoleInitialize();
            break;
        }

        case PAL_CONSRESTART: // Console restart
        {
            DEBUG_LOG("PAL_CONSRESTART: Console restart");
            m_cpu->consoleRestart();
            break;
        }

        case PAL_CONSOUT: // Console output
        {
            quint64 character = R(rb);
            m_cpu->consoleOutput(character & 0xFF);
            DEBUG_LOG(QString("PAL_CONSOUT: Output character 0x%1").arg(character & 0xFF, 2, 16, QChar('0')));
            break;
        }

        case PAL_CONSIN: // Console input
        {
            quint64 character = m_cpu->consoleInput();
            RW(ra, character);
            DEBUG_LOG(QString("PAL_CONSIN: Input character 0x%1").arg(character, 2, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Advanced Memory Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_LDQP_L: // Load quadword physical locked
        {
            quint64 physAddr = R(rb);
            quint64 value = m_cpu->getMemorySystem()->readPhysical64Locked(physAddr);
            RW(ra, value);
            DEBUG_LOG(QString("PAL_LDQP_L: Loaded 0x%1 from physical 0x%2 (locked)")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(physAddr, 16, 16, QChar('0')));
            break;
        }

        case PAL_STQP_C: // Store quadword physical conditional
        {
            quint64 physAddr = R(rb);
            quint64 value = R(ra);
            bool success = m_cpu->getMemorySystem()->writePhysical64Conditional(physAddr, value);
            RW(rc, success ? 1 : 0);
            DEBUG_LOG(QString("PAL_STQP_C: Conditional store 0x%1 to physical 0x%2 = %3")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(physAddr, 16, 16, QChar('0'))
                          .arg(success ? "SUCCESS" : "FAILED"));
            break;
        }

        case PAL_LDQP_U: // Load quadword physical unaligned
        {
            quint64 physAddr = R(rb);
            quint64 value = m_cpu->getMemorySystem()->readPhysical64Unaligned(physAddr);
            RW(ra, value);
            DEBUG_LOG(QString("PAL_LDQP_U: Loaded 0x%1 from physical 0x%2 (unaligned)")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(physAddr, 16, 16, QChar('0')));
            break;
        }

        case PAL_STQP_U: // Store quadword physical unaligned
        {
            quint64 physAddr = R(rb);
            quint64 value = R(ra);
            m_cpu->getMemorySystem()->writePhysical64Unaligned(physAddr, value);
            DEBUG_LOG(QString("PAL_STQP_U: Stored 0x%1 to physical 0x%2 (unaligned)")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(physAddr, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Atomic Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_AMOVRR: // Atomic move register to register
        {
            quint64 sourceValue = R(rb);
            quint64 oldValue = R(ra);
            RW(ra, sourceValue);
            RW(rc, oldValue);
            DEBUG_LOG(QString("PAL_AMOVRR: Atomic move R%1 -> R%2, old value R%3").arg(rb).arg(ra).arg(rc));
            break;
        }

        case PAL_AMOVRM: // Atomic move register to memory
        {
            quint64 address = R(rb);
            quint64 value = R(ra);
            quint64 oldValue = m_cpu->atomicExchange(address, value);
            RW(rc, oldValue);
            DEBUG_LOG(QString("PAL_AMOVRM: Atomic exchange 0x%1 at address 0x%2")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(address, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Cycle Counter and Unique Value Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RSCC: // Read system cycle counter
        {
            quint64 cycleCount = m_cpu->getSystemCycleCounter();
            RW(ra, cycleCount);
            DEBUG_LOG(QString("PAL_RSCC: Read cycle counter = %1").arg(cycleCount));
            break;
        }

        case PAL_READ_UNQ: // Read unique value
        {
            quint64 uniqueValue = m_cpu->getUniqueValue();
            RW(ra, uniqueValue);
            DEBUG_LOG(QString("PAL_READ_UNQ: Read unique value = 0x%1").arg(uniqueValue, 16, 16, QChar('0')));
            break;
        }

        case PAL_WRITE_UNQ: // Write unique value
        {
            quint64 uniqueValue = R(rb);
            m_cpu->setUniqueValue(uniqueValue);
            DEBUG_LOG(QString("PAL_WRITE_UNQ: Set unique value to 0x%1").arg(uniqueValue, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // AST (Asynchronous System Trap) Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_SWASTEN: // Swap AST enable
        {
            quint64 newAstEnable = R(rb) & 0xF; // 4 bits for AST enable
            quint64 oldAstEnable = m_cpu->getASTEnable();
            m_cpu->setASTEnable(newAstEnable);
            RW(ra, oldAstEnable);
            DEBUG_LOG(QString("PAL_SWASTEN: AST enable %1 -> %2").arg(oldAstEnable).arg(newAstEnable));
            break;
        }

        case PAL_WR_PS_SW: // Write processor status software field
        {
            quint64 swField = R(rb);
            m_cpu->setProcessorStatusSoftware(swField);
            DEBUG_LOG(QString("PAL_WR_PS_SW: Set PS software field to 0x%1").arg(swField, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Advanced Performance Monitoring
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RDDPERFMON: // Read detailed performance monitor
        {
            quint64 counterNum = R(rb) & 0x7; // 3 bits for detailed counters
            quint64 detailedCounterValue = m_cpu->readDetailedPerformanceCounter(counterNum);
            RW(ra, detailedCounterValue);
            DEBUG_LOG(QString("PAL_RDDPERFMON: Read detailed PMC%1 = %2").arg(counterNum).arg(detailedCounterValue));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Return from PAL Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_RET: // Return from PAL code
        {
            DEBUG_LOG("PAL_RET: Return from PAL code");
            m_cpu->returnFromPAL();
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Deferred Interlocked Queue Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_INSQHIL_D: // Insert entry into head interlocked longword queue, deferred
        case PAL_INSQTIL_D: // Insert entry into tail interlocked longword queue, deferred
        case PAL_INSQHIQ_D: // Insert entry into head interlocked quadword queue, deferred
        case PAL_INSQTIQ_D: // Insert entry into tail interlocked quadword queue, deferred
        case PAL_REMQHIL_D: // Remove entry from head interlocked longword queue, deferred
        case PAL_REMQTIL_D: // Remove entry from tail interlocked longword queue, deferred
        case PAL_REMQHIQ_D: // Remove entry from head interlocked quadword queue, deferred
        case PAL_REMQTIQ_D: // Remove entry from tail interlocked quadword queue, deferred
        {
            DEBUG_LOG(QString("PAL deferred interlocked queue operation 0x%1").arg(palFunction, 3, 16, QChar('0')));
            m_cpu->executeDeferredQueueOperation(palFunction, R(ra), R(rb), R(rc));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // TLB Extended Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case PAL_TBIE: // TLB Invalidate Entry
        {
            quint64 vaddr = R(rb);
            quint64 asn = R(rc) & 0xFF;
            m_cpu->getMMU()->invalidateTLBEntry(vaddr, asn);
            DEBUG_LOG(QString("PAL_TBIE: Invalidate TLB entry VA=0x%1 ASN=%2").arg(vaddr, 16, 16, QChar('0')).arg(asn));
            break;
        }

        case PAL_TBIM: // TLB Invalidate Multiple
        {
            quint64 startAddr = R(rb);
            quint64 endAddr = R(rc);
            m_cpu->getMMU()->invalidateTLBRange(startAddr, endAddr);
            DEBUG_LOG(QString("PAL_TBIM: Invalidate TLB range 0x%1-0x%2")
                          .arg(startAddr, 16, 16, QChar('0'))
                          .arg(endAddr, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Default case for unknown PAL functions
            // ═══════════════════════════════════════════════════════════════════════════

        default:
        {
            // Check if this is an implementation-specific or system-specific PAL function
            if (palFunction >= 0x80 && palFunction <= 0xFF)
            {
                DEBUG_LOG(QString("System-specific PAL function 0x%1").arg(palFunction, 2, 16, QChar('0')));
                executeSystemSpecificPAL(palFunction, ra, rb, rc);
            }
            else if (palFunction >= 0x100)
            {
                DEBUG_LOG(QString("Implementation-specific PAL function 0x%1").arg(palFunction, 3, 16, QChar('0')));
                executeImplementationSpecificPAL(palFunction, ra, rb, rc);
            }
            else
            {
                DEBUG_LOG(QString("Unknown PAL function 0x%1").arg(palFunction, 2, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            }
            break;
        }
        }

        // Update PAL instruction statistics
        m_palInstructions++;
        m_totalInstructions++;

        // Some PAL operations may require pipeline flush
        if (isPipelineFlushRequired(palFunction))
        {
            DEBUG_LOG("PAL operation requires pipeline flush");
            m_cpu->flushPipeline();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Helper Methods for PAL Execution
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Execute system-specific PAL functions (0x80-0xFF range)
     * @param palFunction The PAL function code
     * @param ra Register A field
     * @param rb Register B field
     * @param rc Register C field
     */
    void executeSystemSpecificPAL(quint32 palFunction, quint8 ra, quint8 rb, quint8 rc)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        switch (palFunction)
        {
        // VMS-specific PAL functions
        case 0x82: // CHMK - Change Mode to Kernel (VMS)
        {
            DEBUG_LOG("VMS PAL_CHMK: Change mode to kernel");
            m_cpu->changeMode(enumProcessorMode::KERNEL);
            break;
        }

        case 0x83: // CHME - Change Mode to Executive (VMS)
        {
            DEBUG_LOG("VMS PAL_CHME: Change mode to executive");
            m_cpu->changeMode(enumProcessorMode::EXECUTIVE);
            break;
        }

        case 0x84: // CHMS - Change Mode to Supervisor (VMS)
        {
            DEBUG_LOG("VMS PAL_CHMS: Change mode to supervisor");
            m_cpu->changeMode(enumProcessorMode::SUPERVISOR);
            break;
        }

        case 0x85: // CHMU - Change Mode to User (VMS)
        {
            DEBUG_LOG("VMS PAL_CHMU: Change mode to user");
            m_cpu->changeMode(enumProcessorMode::USER);
            break;
        }

        // UNIX/Tru64-specific PAL functions
        case 0x90: // RDPERFMON - Read Performance Monitor (UNIX)
        {
            quint64 counterNum = R(rb) & 0x3;
            quint64 counterValue = m_cpu->readPerformanceCounter(counterNum);
            RW(ra, counterValue);
            DEBUG_LOG(QString("UNIX PAL_RDPERFMON: Read PMC%1 = %2").arg(counterNum).arg(counterValue));
            break;
        }

        case 0x91: // WRPERFMON - Write Performance Monitor (UNIX)
        {
            quint64 counterNum = R(rb) & 0x3;
            quint64 counterValue = R(rc);
            m_cpu->writePerformanceCounter(counterNum, counterValue);
            DEBUG_LOG(QString("UNIX PAL_WRPERFMON: Set PMC%1 = %2").arg(counterNum).arg(counterValue));
            break;
        }

        case 0x98: // RDPS - Read Processor Status (UNIX)
        {
            quint64 ps = m_cpu->getProcessorStatus();
            RW(ra, ps);
            DEBUG_LOG(QString("UNIX PAL_RDPS: Read PS = 0x%1").arg(ps, 16, 16, QChar('0')));
            break;
        }

        case 0x99: // REI - Return from Exception or Interrupt
        {
            DEBUG_LOG("PAL_REI: Return from exception or interrupt");
            m_cpu->returnFromException();
            break;
        }

        default:
            DEBUG_LOG(QString("Unimplemented system-specific PAL function 0x%1").arg(palFunction, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            break;
        }
    }

    bool executeStore(quint64 virtualAddr, quint64 value, int size)
    {
        return m_memorySystem->writeVirtualMemory(virtualAddr, value, size, m_currentPC);
    }

    /**
     * @brief Execute implementation-specific PAL functions (0x100+ range)
     * @param palFunction The PAL function code
     * @param ra Register A field
     * @param rb Register B field
     * @param rc Register C field
     */
    void executeImplementationSpecificPAL(quint32 palFunction, quint8 ra, quint8 rb, quint8 rc)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        // Handle processor-specific PAL functions based on CPU model
        CpuModel cpuModel = m_cpu->getCpuModel();

        switch (cpuModel)
        {
        case CpuModel::CPU_EV4:
        case CpuModel::CPU_EV5:
            executeEV4_EV5_SpecificPAL(palFunction, ra, rb, rc);
            break;

        case CpuModel::CPU_EV6:
        case CpuModel::CPU_EV67:
        case CpuModel::CPU_EV68:
            executeEV6_SpecificPAL(palFunction, ra, rb, rc);
            break;

        case CpuModel::CPU_EV7:
        case CpuModel::CPU_EV79:
            executeEV7_SpecificPAL(palFunction, ra, rb, rc);
            break;

        default:
            DEBUG_LOG(QString("Unsupported CPU model for PAL function 0x%1").arg(palFunction, 3, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            break;
        }
    }

    /**
     * @brief Execute EV4/EV5-specific PAL functions
     */
    void executeEV4_EV5_SpecificPAL(quint32 palFunction, quint8 ra, quint8 rb, quint8 rc)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        switch (palFunction)
        {
        case 0x100: // EV4/EV5 specific cache operation
            DEBUG_LOG("EV4/EV5 PAL: Cache operation");
            m_cpu->ev4_ev5_CacheOperation(R(rb));
            break;

        case 0x101: // EV4/EV5 specific TLB operation
            DEBUG_LOG("EV4/EV5 PAL: TLB operation");
            m_cpu->ev4_ev5_TLBOperation(R(rb), R(rc));
            break;

        default:
            DEBUG_LOG(QString("Unknown EV4/EV5 PAL function 0x%1").arg(palFunction, 3, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            break;
        }
    }

    /**
     * @brief Execute EV6-specific PAL functions
     */
    void executeEV6_SpecificPAL(quint32 palFunction, quint8 ra, quint8 rb, quint8 rc)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        switch (palFunction)
        {
        case 0x200: // EV6 specific performance monitoring
            DEBUG_LOG("EV6 PAL: Advanced performance monitoring");
            m_cpu->ev6_AdvancedPerformanceMonitoring(R(rb), R(rc));
            break;

        case 0x201: // EV6 specific cache control
            DEBUG_LOG("EV6 PAL: Advanced cache control");
            m_cpu->ev6_AdvancedCacheControl(R(rb));
            break;

        default:
            DEBUG_LOG(QString("Unknown EV6 PAL function 0x%1").arg(palFunction, 3, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            break;
        }
    }

    /**
     * @brief Execute EV7-specific PAL functions
     */
    void executeEV7_SpecificPAL(quint32 palFunction, quint8 ra, quint8 rb, quint8 rc)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        switch (palFunction)
        {
        case 0x300: // EV7 specific multiprocessor operation
            DEBUG_LOG("EV7 PAL: Multiprocessor operation");
            m_cpu->ev7_MultiprocessorOperation(R(rb), R(rc));
            break;

        case 0x301: // EV7 specific advanced TLB operation
            DEBUG_LOG("EV7 PAL: Advanced TLB operation");
            m_cpu->ev7_AdvancedTLBOperation(R(rb), R(rc));
            break;

        default:
            DEBUG_LOG(QString("Unknown EV7 PAL function 0x%1").arg(palFunction, 3, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            break;
        }
    }

    /**
     * @brief Execute hardware-specific instruction group (opcodes 0x19, 0x1B-0x1F)
     * @param instruction Decoded instruction to execute
     *
     * Handles: HW_MFPR, HW_LD, HW_MTPR, HW_REI, HW_ST, HW_ST_C
     * These are implementation-specific instructions that vary by Alpha CPU model
     */
    void executeHardwareGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        // Check if we're in privileged mode (required for most hardware instructions)
        if (!m_cpu->isPrivilegedMode())
        {
            DEBUG_LOG(QString("Hardware instruction 0x%1 executed in non-privileged mode")
                          .arg(instruction.opcode, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::PRIVILEGED_INSTRUCTION, m_cpu->getPc());
            return;
        }

        // Get CPU model to determine which hardware instructions are supported
        CpuModel::CpuModel cpuModel = m_cpu->getCpuModel();

        DEBUG_LOG(QString("Hardware instruction: opcode=0x%1, ra=%2, rb=%3, function=0x%4, CPU=%5")
                      .arg(instruction.opcode, 2, 16, QChar('0'))
                      .arg(instruction.ra)
                      .arg(instruction.rb)
                      .arg(instruction.function, 4, 16, QChar('0'))
                      .arg(static_cast<int>(cpuModel)));

        switch (instruction.opcode)
        {
            // ═══════════════════════════════════════════════════════════════════════════
            // HW_MFPR (0x19) - Move from Processor Register
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_HW_MFPR: // 0x19 - Move from Processor Register
        {
            quint32 iprNumber = instruction.function & 0xFF; // 8-bit IPR number
            quint64 iprValue = 0;

            DEBUG_LOG(QString("HW_MFPR: Reading IPR %1 -> R%2").arg(iprNumber).arg(instruction.ra));

            // Read from Internal Processor Register based on CPU model
            switch (cpuModel)
            {
            case CpuModel::CPU_EV4:
            case CpuModel::CPU_EV5:
                iprValue = readEV4_EV5_IPR(iprNumber);
                break;

            case CpuModel::CPU_EV6:
            case CpuModel::CPU_EV67:
            case CpuModel::CPU_EV68:
                iprValue = readEV6_IPR(iprNumber);
                break;

            case CpuModel::CPU_EV7:
            case CpuModel::CPU_EV79:
                iprValue = readEV7_IPR(iprNumber);
                break;

            default:
                DEBUG_LOG(QString("HW_MFPR: Unsupported CPU model %1").arg(static_cast<int>(cpuModel)));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            RW(instruction.ra, iprValue);
            DEBUG_LOG(QString("HW_MFPR: IPR %1 = 0x%2").arg(iprNumber).arg(iprValue, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // HW_LD (0x1B) - Hardware Load
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_HW_LD: // 0x1B - Hardware Load
        {
            quint64 address = R(instruction.rb);
            quint32 loadType = instruction.function & 0xF; // Load type in function field
            quint64 value = 0;

            DEBUG_LOG(QString("HW_LD: Hardware load type %1 from address 0x%2")
                          .arg(loadType)
                          .arg(address, 16, 16, QChar('0')));

            // Execute hardware-specific load based on CPU model and load type
            switch (cpuModel)
            {
            case CpuModel::CPU_EV4:
            case CpuModel::CPU_EV5:
                if (!executeEV4_EV5_HardwareLoad(loadType, address, value))
                {
                    m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                    return;
                }
                break;

            case CpuModel::CPU_EV6:
            case CpuModel::CPU_EV67:
            case CpuModel::CPU_EV68:
                if (!executeEV6_HardwareLoad(loadType, address, value))
                {
                    m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                    return;
                }
                break;

            case CpuModel::CPU_EV7:
            case CpuModel::CPU_EV79:
                if (!executeEV7_HardwareLoad(loadType, address, value))
                {
                    m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                    return;
                }
                break;

            default:
                DEBUG_LOG(QString("HW_LD: Unsupported CPU model %1").arg(static_cast<int>(cpuModel)));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            RW(instruction.ra, value);
            DEBUG_LOG(QString("HW_LD: Loaded 0x%1 -> R%2").arg(value, 16, 16, QChar('0')).arg(instruction.ra));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // HW_MTPR (0x1C) - Move to Processor Register
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_HW_MTPR: // 0x1C - Move to Processor Register
        {
            quint32 iprNumber = instruction.function & 0xFF; // 8-bit IPR number
            quint64 iprValue = R(instruction.ra);

            DEBUG_LOG(QString("HW_MTPR: Writing 0x%1 to IPR %2").arg(iprValue, 16, 16, QChar('0')).arg(iprNumber));

            // Write to Internal Processor Register based on CPU model
            bool success = false;
            switch (cpuModel)
            {
            case CpuModel::CPU_EV4:
            case CpuModel::CPU_EV5:
                success = writeEV4_EV5_IPR(iprNumber, iprValue);
                break;

            case CpuModel::CPU_EV6:
            case CpuModel::CPU_EV67:
            case CpuModel::CPU_EV68:
                success = writeEV6_IPR(iprNumber, iprValue);
                break;

            case CpuModel::CPU_EV7:
            case CpuModel::CPU_EV79:
                success = writeEV7_IPR(iprNumber, iprValue);
                break;

            default:
                DEBUG_LOG(QString("HW_MTPR: Unsupported CPU model %1").arg(static_cast<int>(cpuModel)));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            if (!success)
            {
                DEBUG_LOG(QString("HW_MTPR: Invalid or read-only IPR %1").arg(iprNumber));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            // Some IPR writes may require pipeline flush
            if (requiresPipelineFlushOnWrite(iprNumber))
            {
                DEBUG_LOG("HW_MTPR: IPR write requires pipeline flush");
                m_cpu->flushPipeline();
            }

            DEBUG_LOG(QString("HW_MTPR: IPR %1 = 0x%2").arg(iprNumber).arg(iprValue, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // HW_REI (0x1D) - Return from Exception/Interrupt
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_HW_REI: // 0x1D - Return from Exception/Interrupt
        {
            DEBUG_LOG("HW_REI: Hardware return from exception/interrupt");

            // Hardware-specific return from exception/interrupt
            switch (cpuModel)
            {
            case CpuModel::CPU_EV4:
            case CpuModel::CPU_EV5:
                executeEV4_EV5_HardwareReturn();
                break;

            case CpuModel::CPU_EV6:
            case CpuModel::CPU_EV67:
            case CpuModel::CPU_EV68:
                executeEV6_HardwareReturn();
                break;

            case CpuModel::CPU_EV7:
            case CpuModel::CPU_EV79:
                executeEV7_HardwareReturn();
                break;

            default:
                DEBUG_LOG(QString("HW_REI: Unsupported CPU model %1").arg(static_cast<int>(cpuModel)));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            // Hardware return always flushes pipeline
            m_cpu->flushPipeline();
            DEBUG_LOG("HW_REI: Hardware return completed");
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // HW_ST (0x1E) - Hardware Store
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_HW_ST: // 0x1E - Hardware Store
        {
            quint64 address = R(instruction.rb);
            quint64 value = R(instruction.ra);
            quint32 storeType = instruction.function & 0xF; // Store type in function field

            DEBUG_LOG(QString("HW_ST: Hardware store type %1, value 0x%2 to address 0x%3")
                          .arg(storeType)
                          .arg(value, 16, 16, QChar('0'))
                          .arg(address, 16, 16, QChar('0')));

            // Execute hardware-specific store based on CPU model and store type
            bool success = false;
            switch (cpuModel)
            {
            case CpuModel::CPU_EV4:
            case CpuModel::CPU_EV5:
                success = executeEV4_EV5_HardwareStore(storeType, address, value);
                break;

            case CpuModel::CPU_EV6:
            case CpuModel::CPU_EV67:
            case CpuModel::CPU_EV68:
                success = executeEV6_HardwareStore(storeType, address, value);
                break;

            case CpuModel::CPU_EV7:
            case CpuModel::CPU_EV78:
                success = executeEV7_HardwareStore(storeType, address, value);
                break;

            default:
                DEBUG_LOG(QString("HW_ST: Unsupported CPU model %1").arg(static_cast<int>(cpuModel)));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            if (!success)
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, address);
                return;
            }

            DEBUG_LOG("HW_ST: Hardware store completed");
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // HW_ST_C (0x1F) - Hardware Store Conditional
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_HW_ST_C: // 0x1F - Hardware Store Conditional
        {
            quint64 address = R(instruction.rb);
            quint64 value = R(instruction.ra);
            quint32 storeType = instruction.function & 0xF; // Store type in function field

            DEBUG_LOG(QString("HW_ST_C: Hardware conditional store type %1, value 0x%2 to address 0x%3")
                          .arg(storeType)
                          .arg(value, 16, 16, QChar('0'))
                          .arg(address, 16, 16, QChar('0')));

            // Execute hardware-specific conditional store based on CPU model
            bool success = false;
            switch (cpuModel)
            {
            case CpuModel::CPU_EV4:
            case CpuModel::CPU_EV5:
                success = executeEV4_EV5_HardwareStoreConditional(storeType, address, value);
                break;

            case CpuModel::CPU_EV6:
            case CpuModel::CPU_EV67:
            case CpuModel::CPU_EV68:
                success = executeEV6_HardwareStoreConditional(storeType, address, value);
                break;

            case CpuModel::CPU_EV7:
            case CpuModel::CPU_EV78:
                success = executeEV7_HardwareStoreConditional(storeType, address, value);
                break;

            default:
                DEBUG_LOG(QString("HW_ST_C: Unsupported CPU model %1").arg(static_cast<int>(cpuModel)));
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                return;
            }

            // Store success/failure in Ra (0 = failed, 1 = succeeded)
            RW(instruction.ra, success ? 1 : 0);

            DEBUG_LOG(QString("HW_ST_C: Hardware conditional store %1").arg(success ? "SUCCEEDED" : "FAILED"));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Default Case - Unknown Hardware Opcode
            // ═══════════════════════════════════════════════════════════════════════════

        default:
        {
            DEBUG_LOG(QString("executeHardwareGroup: Unknown hardware opcode 0x%1")
                          .arg(instruction.opcode, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            return;
        }
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // Statistics and Performance Monitoring
        // ═══════════════════════════════════════════════════════════════════════════

        // Update instruction statistics
        updateHardwareInstructionStatistics(instruction.opcode);

        // Update general statistics
        m_totalInstructions++;

        // Update performance counters if available
        if (m_cpu->hasPerformanceCounters())
        {
            m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HARDWARE_INSTRUCTIONS);

            // Track specific hardware instruction types
            switch (instruction.opcode)
            {
            case OPCODE_HW_MFPR:
            case OPCODE_HW_MTPR:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::IPR_INSTRUCTIONS);
                break;

            case OPCODE_HW_LD:
            case OPCODE_HW_ST:
            case OPCODE_HW_ST_C:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HARDWARE_MEMORY_INSTRUCTIONS);
                break;

            case OPCODE_HW_REI:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HARDWARE_CONTROL_INSTRUCTIONS);
                break;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // CPU Model-Specific Helper Methods
    // ═══════════════════════════════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════════════════════════════
    // EV6-Specific Helper Methods (add to executeHardwareGroup)
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Read Internal Processor Register for EV6
     */
    quint64 readEV6_IPR(quint32 iprNumber)
    {
        switch (iprNumber)
        {
        case IPR_EV6_IVA_FORM:
            return m_cpu->getIVA_Form();
        case IPR_EV6_IER_CM:
            return m_cpu->getInterruptEnableCM();
        case IPR_EV6_SIRR:
            return m_cpu->getSoftwareInterruptRequest();
        case IPR_EV6_ISUM:
            return m_cpu->getInterruptSummary();
        case IPR_EV6_HW_INT_CLR:
            return m_cpu->getHardwareInterruptClear();
        case IPR_EV6_EXC_ADDR:
            return m_cpu->getExceptionAddress();
        case IPR_EV6_IC_PERR_STAT:
            return m_cpu->getIcacheParityErrorStatus();
        case IPR_EV6_IC_PERR_ADDR:
            return m_cpu->getIcacheParityErrorAddress();
        case IPR_EV6_PMCTR:
            return m_cpu->getPerformanceCounter();
        case IPR_EV6_PAL_BASE:
            return m_cpu->getPALBase();
        case IPR_EV6_I_CTL:
            return m_cpu->getIstreamControl();
        case IPR_EV6_PCTR_CTL:
            return m_cpu->getPerformanceCounterControl();
        case IPR_EV6_CLR_MAP:
            return m_cpu->getClearMap();
        case IPR_EV6_I_STAT:
            return m_cpu->getIstreamStatus();
        case IPR_EV6_SLEEP:
            return m_cpu->getSleepRegister();
        default:
            DEBUG_LOG(QString("readEV6_IPR: Unknown IPR %1").arg(iprNumber));
            return 0;
        }
    }

    /**
     * @brief Write Internal Processor Register for EV6
     */
    bool writeEV6_IPR(quint32 iprNumber, quint64 value)
    {
        switch (iprNumber)
        {
        case IPR_EV6_IVA_FORM:
            m_cpu->setIVA_Form(value);
            return true;
        case IPR_EV6_IER_CM:
            m_cpu->setInterruptEnableCM(value);
            return true;
        case IPR_EV6_SIRR:
            m_cpu->setSoftwareInterruptRequest(value);
            return true;
        case IPR_EV6_ISUM:
            return false; // Read-only
        case IPR_EV6_HW_INT_CLR:
            m_cpu->setHardwareInterruptClear(value);
            return true;
        case IPR_EV6_EXC_ADDR:
            return false; // Read-only
        case IPR_EV6_IC_PERR_STAT:
            return false; // Read-only
        case IPR_EV6_IC_PERR_ADDR:
            return false; // Read-only
        case IPR_EV6_PMCTR:
            m_cpu->setPerformanceCounter(value);
            return true;
        case IPR_EV6_PAL_BASE:
            m_cpu->setPALBase(value);
            return true;
        case IPR_EV6_I_CTL:
            m_cpu->setIstreamControl(value);
            return true;
        case IPR_EV6_PCTR_CTL:
            m_cpu->setPerformanceCounterControl(value);
            return true;
        case IPR_EV6_CLR_MAP:
            m_cpu->setClearMap(value);
            return true;
        case IPR_EV6_I_STAT:
            return false; // Read-only
        case IPR_EV6_SLEEP:
            m_cpu->setSleepRegister(value);
            return true;
        default:
            DEBUG_LOG(QString("writeEV6_IPR: Unknown IPR %1").arg(iprNumber));
            return false;
        }
    }

    /**
     * @brief Execute EV6 hardware load operation
     */
    bool executeEV6_HardwareLoad(quint32 loadType, quint64 address, quint64 &value)
    {
        switch (loadType)
        {
        case HW_LD_EV6_PHYSICAL:
            return m_cpu->readPhysicalMemory(address, value);
        case HW_LD_EV6_VIRTUAL:
            return m_cpu->readVirtualMemory(address, value);
        case HW_LD_EV6_IO_SPACE:
            return m_cpu->readIOSpace(address, value);
        case HW_LD_EV6_CONFIG_SPACE:
            return m_cpu->readConfigSpace(address, value);
        case HW_LD_EV6_LOCK:
            return m_cpu->readMemoryLocked(address, value);
        case HW_LD_EV6_PREFETCH:
            m_cpu->prefetchMemory(address);
            value = 0; // Prefetch doesn't return data
            return true;
        default:
            DEBUG_LOG(QString("executeEV6_HardwareLoad: Unknown load type %1").arg(loadType));
            return false;
        }
    }

    /**
     * @brief Execute EV6 hardware store operation
     */
    bool executeEV6_HardwareStore(quint32 storeType, quint64 address, quint64 value)
    {
        switch (storeType)
        {
        case HW_ST_EV6_PHYSICAL:
            return m_cpu->writePhysicalMemory(address, value);
        case HW_ST_EV6_VIRTUAL:
            return m_cpu->writeVirtualMemory(address, value);
        case HW_ST_EV6_IO_SPACE:
            return m_cpu->writeIOSpace(address, value);
        case HW_ST_EV6_CONFIG_SPACE:
            return m_cpu->writeConfigSpace(address, value);
        case HW_ST_EV6_CONDITIONAL:
            return m_cpu->writeMemoryConditional(address, value);
        case HW_ST_EV6_WRITETHROUGH:
            return m_cpu->writeMemoryWriteThrough(address, value);
        default:
            DEBUG_LOG(QString("executeEV6_HardwareStore: Unknown store type %1").arg(storeType));
            return false;
        }
    }

    /**
     * @brief Execute EV6 hardware conditional store operation
     */
    bool executeEV6_HardwareStoreConditional(quint32 storeType, quint64 address, quint64 value)
    {
        if (!m_cpu->checkLockFlag())
        {
            return false;
        }

        bool result = executeEV6_HardwareStore(storeType, address, value);
        if (result)
        {
            m_cpu->clearLockFlag();
        }
        return result;
    }

    /**
     * @brief Execute EV6 hardware return from exception
     */
    void executeEV6_HardwareReturn()
    {
        m_cpu->restoreProcessorState();
        m_cpu->enableInterrupts();
        m_cpu->returnFromHardwareException();
        m_cpu->updatePerformanceCounters();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // EV7-Specific Helper Methods
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Read Internal Processor Register for EV7
     */
    quint64 readEV7_IPR(quint32 iprNumber)
    {
        switch (iprNumber)
        {
        case IPR_EV7_IVA_FORM:
            return m_cpu->getIVA_Form();
        case IPR_EV7_IER:
            return m_cpu->getInterruptEnable();
        case IPR_EV7_SIRR:
            return m_cpu->getSoftwareInterruptRequest();
        case IPR_EV7_ISUM:
            return m_cpu->getInterruptSummary();
        case IPR_EV7_EXC_ADDR:
            return m_cpu->getExceptionAddress();
        case IPR_EV7_EXC_SUM:
            return m_cpu->getExceptionSummary();
        case IPR_EV7_EXC_MASK:
            return m_cpu->getExceptionMask();
        case IPR_EV7_PAL_BASE:
            return m_cpu->getPALBase();
        case IPR_EV7_I_CTL:
            return m_cpu->getIstreamControl();
        case IPR_EV7_I_STAT:
            return m_cpu->getIstreamStatus();
        case IPR_EV7_DC_CTL:
            return m_cpu->getDcacheControl();
        case IPR_EV7_DC_STAT:
            return m_cpu->getDcacheStatus();
        case IPR_EV7_C_DATA:
            return m_cpu->getCacheData();
        case IPR_EV7_C_SHIFT:
            return m_cpu->getCacheShift();
        case IPR_EV7_PMCTR0:
            return m_cpu->getPerformanceCounter(0);
        case IPR_EV7_PMCTR1:
            return m_cpu->getPerformanceCounter(1);
        case IPR_EV7_PMCTR2:
            return m_cpu->getPerformanceCounter(2);
        case IPR_EV7_PMCTR3:
            return m_cpu->getPerformanceCounter(3);
        default:
            DEBUG_LOG(QString("readEV7_IPR: Unknown IPR %1").arg(iprNumber));
            return 0;
        }
    }

    /**
     * @brief Write Internal Processor Register for EV7
     */
    bool writeEV7_IPR(quint32 iprNumber, quint64 value)
    {
        switch (iprNumber)
        {
        case IPR_EV7_IVA_FORM:
            m_cpu->setIVA_Form(value);
            return true;
        case IPR_EV7_IER:
            m_cpu->setInterruptEnable(value);
            return true;
        case IPR_EV7_SIRR:
            m_cpu->setSoftwareInterruptRequest(value);
            return true;
        case IPR_EV7_ISUM:
            return false; // Read-only
        case IPR_EV7_EXC_ADDR:
            return false; // Read-only
        case IPR_EV7_EXC_SUM:
            m_cpu->setExceptionSummary(value);
            return true;
        case IPR_EV7_EXC_MASK:
            m_cpu->setExceptionMask(value);
            return true;
        case IPR_EV7_PAL_BASE:
            m_cpu->setPALBase(value);
            return true;
        case IPR_EV7_I_CTL:
            m_cpu->setIstreamControl(value);
            return true;
        case IPR_EV7_I_STAT:
            return false; // Read-only
        case IPR_EV7_DC_CTL:
            m_cpu->setDcacheControl(value);
            return true;
        case IPR_EV7_DC_STAT:
            return false; // Read-only
        case IPR_EV7_C_DATA:
            m_cpu->setCacheData(value);
            return true;
        case IPR_EV7_C_SHIFT:
            m_cpu->setCacheShift(value);
            return true;
        case IPR_EV7_PMCTR0:
            m_cpu->setPerformanceCounter(0, value);
            return true;
        case IPR_EV7_PMCTR1:
            m_cpu->setPerformanceCounter(1, value);
            return true;
        case IPR_EV7_PMCTR2:
            m_cpu->setPerformanceCounter(2, value);
            return true;
        case IPR_EV7_PMCTR3:
            m_cpu->setPerformanceCounter(3, value);
            return true;
        default:
            DEBUG_LOG(QString("writeEV7_IPR: Unknown IPR %1").arg(iprNumber));
            return false;
        }
    }

    /**
     * @brief Execute EV7 hardware load operation
     */
    bool executeEV7_HardwareLoad(quint32 loadType, quint64 address, quint64 &value)
    {
        switch (loadType)
        {
        case HW_LD_EV7_PHYSICAL:
            return m_cpu->readPhysicalMemory(address, value);
        case HW_LD_EV7_VIRTUAL:
            return m_cpu->readVirtualMemory(address, value);
        case HW_LD_EV7_IO_SPACE:
            return m_cpu->readIOSpace(address, value);
        case HW_LD_EV7_CONFIG_SPACE:
            return m_cpu->readConfigSpace(address, value);
        case HW_LD_EV7_LOCK:
            return m_cpu->readMemoryLocked(address, value);
        case HW_LD_EV7_PREFETCH:
            m_cpu->prefetchMemory(address);
            value = 0;
            return true;
        case HW_LD_EV7_SPECULATIVE:
            return m_cpu->readMemorySpeculative(address, value);
        case HW_LD_EV7_COHERENT:
            return m_cpu->readMemoryCoherent(address, value);
        default:
            DEBUG_LOG(QString("executeEV7_HardwareLoad: Unknown load type %1").arg(loadType));
            return false;
        }
    }

    /**
     * @brief Execute EV7 hardware store operation
     */
    bool executeEV7_HardwareStore(quint32 storeType, quint64 address, quint64 value)
    {
        switch (storeType)
        {
        case HW_ST_EV7_PHYSICAL:
            return m_cpu->writePhysicalMemory(address, value);
        case HW_ST_EV7_VIRTUAL:
            return m_cpu->writeVirtualMemory(address, value);
        case HW_ST_EV7_IO_SPACE:
            return m_cpu->writeIOSpace(address, value);
        case HW_ST_EV7_CONFIG_SPACE:
            return m_cpu->writeConfigSpace(address, value);
        case HW_ST_EV7_CONDITIONAL:
            return m_cpu->writeMemoryConditional(address, value);
        case HW_ST_EV7_WRITETHROUGH:
            return m_cpu->writeMemoryWriteThrough(address, value);
        case HW_ST_EV7_WRITEBACK:
            return m_cpu->writeMemoryWriteBack(address, value);
        case HW_ST_EV7_COHERENT:
            return m_cpu->writeMemoryCoherent(address, value);
        default:
            DEBUG_LOG(QString("executeEV7_HardwareStore: Unknown store type %1").arg(storeType));
            return false;
        }
    }

    /**
     * @brief Execute EV7 hardware conditional store operation
     */
    bool executeEV7_HardwareStoreConditional(quint32 storeType, quint64 address, quint64 value)
    {
        if (!m_cpu->checkLockFlag())
        {
            return false;
        }

        bool result = executeEV7_HardwareStore(storeType, address, value);
        if (result)
        {
            m_cpu->clearLockFlag();
        }
        return result;
    }

    /**
     * @brief Execute EV7 hardware return from exception
     */
    void executeEV7_HardwareReturn()
    {
        m_cpu->restoreProcessorState();
        m_cpu->enableInterrupts();
        m_cpu->returnFromHardwareException();
        m_cpu->updatePerformanceCounters();
        m_cpu->synchronizeMultiprocessor();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Generic Hardware Instruction Utilities
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Check if CPU model supports hardware instructions
     */
    bool supportsHardwareInstructions() const
    {
        CpuModel model = m_cpu->getCpuModel();
        return (model != CpuModel::CPU_UNKNOWN);
    }

    /**
     * @brief Get hardware instruction capability mask for current CPU
     */
    quint32 getHardwareInstructionMask() 
    {
        CpuModel model m_cpu->getCpuModel();

        switch (model)
        {
        case CpuModel::CPU_EV4:
        case CpuModel::CPU_EV5:
            return HW_MASK_EV4_EV5;

        case CpuModel::CPU_EV6:
        case CpuModel::CPU_EV67:
        case CpuModel::CPU_EV68:
            return HW_MASK_EV6;

        case CpuModel::CPU_EV7:
        case CpuModel::CPU_EV78:
            return HW_MASK_EV7;

        default:
            return 0; // No hardware instructions supported
        }
    }

    /**
     * @brief Validate hardware instruction for current CPU model
     */
    bool validateHardwareInstruction(const DecodedInstruction &instruction) const
    {
        quint32 mask = getHardwareInstructionMask();
        quint32 opcodeFlag = 1 << (instruction.opcode - OPCODE_HW_MFPR);

        return (mask & opcodeFlag) != 0;
    }

    /**
     * @brief Print hardware instruction statistics
     */
    void printHardwareStatistics() const
    {
        QMutexLocker locker(&m_statsMutex);

        // These would need to be added as member variables
        DEBUG_LOG("Hardware Instruction Statistics:");
        DEBUG_LOG("  IPR Instructions: [implement counter]");
        DEBUG_LOG("  Hardware Memory Instructions: [implement counter]");
        DEBUG_LOG("  Hardware Control Instructions: [implement counter]");
        DEBUG_LOG(QString("  Supported CPU Model: %1").arg(static_cast<int>(m_cpu->getCpuModel())));
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // Error Handling and Validation
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Handle hardware instruction privilege violation
     */
    void handleHardwarePrivilegeViolation(const DecodedInstruction &instruction)
    {
        DEBUG_LOG(QString("Hardware instruction privilege violation: opcode=0x%1, mode=%2")
                      .arg(instruction.opcode, 2, 16, QChar('0'))
                      .arg(m_cpu->isPrivilegedMode() ? "PRIVILEGED" : "USER"));

        // Log the attempted operation for security auditing
        m_cpu->logSecurityViolation(SecurityViolationType::HARDWARE_INSTRUCTION_VIOLATION,
                                    instruction.rawInstruction);

        // Trigger privilege exception
        m_cpu->triggerException(ExceptionType::PRIVILEGED_INSTRUCTION, m_cpu->getPc());
    }

    /**
     * @brief Handle unsupported hardware instruction
     */
    void handleUnsupportedHardwareInstruction(const DecodedInstruction &instruction)
    {
        DEBUG_LOG(QString("Unsupported hardware instruction: opcode=0x%1, CPU model=%2")
                      .arg(instruction.opcode, 2, 16, QChar('0'))
                      .arg(static_cast<int>(m_cpu->getCpuModel())));

        // Some implementations might want to emulate unsupported instructions
        if (m_cpu->hasHardwareEmulation())
        {
            bool emulated = m_cpu->emulateHardwareInstruction(instruction);
            if (emulated)
            {
                DEBUG_LOG("Hardware instruction successfully emulated");
                return;
            }
        }

        // Trigger illegal instruction exception
        m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
    }

    /**
     * @brief Validate IPR access permissions
     */
    bool validateIPRAccess(quint32 iprNumber, bool isWrite) const
    {
        // Check if IPR exists for current CPU model
        if (!isValidIPR(iprNumber))
        {
            return false;
        }

        // Check if IPR is writable (if this is a write operation)
        if (isWrite && !isWritableIPR(iprNumber))
        {
            return false;
        }

        // Check privilege level requirements
        if (!hasIPRPrivilege(iprNumber))
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Check if IPR number is valid for current CPU model
     */
    bool isValidIPR(quint32 iprNumber) const
    {
        CpuModel model = m_cpu->getCpuModel();

        switch (model)
        {
        case CPU_EV4:
        case CPU_EV5:
            return (iprNumber <= IPR_EV4_EV5_ASTRR);

        case CPU_EV6:
        case CPU_EV67:
        case CPU_EV68:
            return (iprNumber <= IPR_EV6_SLEEP);

        case CPU_EV7:
        case CPU_EV79:
            return (iprNumber <= IPR_EV7_PMCTR3);

        default:
            return false;
        }
    }

    /**
     * @brief Check if IPR is writable
     */
    bool isWritableIPR(quint32 iprNumber) const
    {
        // Read-only IPRs (varies by CPU model)
        switch (iprNumber)
        {
        case IPR_EV4_EV5_EXC_ADDR:
        case IPR_EV6_ISUM:
        case IPR_EV6_EXC_ADDR:
        case IPR_EV6_IC_PERR_STAT:
        case IPR_EV6_IC_PERR_ADDR:
        case IPR_EV6_I_STAT:
        case IPR_EV7_ISUM:
        case IPR_EV7_EXC_ADDR:
        case IPR_EV7_I_STAT:
        case IPR_EV7_DC_STAT:
            return false; // Read-only
        default:
            return true; // Most IPRs are writable
        }
    }

    /**
     * @brief Check if current privilege level can access IPR
     */
    bool hasIPRPrivilege(quint32 iprNumber) const
    {
        // All IPRs require privileged mode
        return m_cpu->isPrivilegedMode();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Performance and Debugging Utilities
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Log hardware instruction execution for debugging
     */
    void logHardwareInstruction(const DecodedInstruction &instruction, const QString &operation) const
    {
        if (m_cpu->isHardwareInstructionLoggingEnabled())
        {
            DEBUG_LOG(QString("HW_INSTR: %1 - opcode=0x%2, ra=%3, rb=%4, func=0x%5, CPU=%6")
                          .arg(operation)
                          .arg(instruction.opcode, 2, 16, QChar('0'))
                          .arg(instruction.ra)
                          .arg(instruction.rb)
                          .arg(instruction.function, 4, 16, QChar('0'))
                          .arg(static_cast<int>(m_cpu->getCpuModel())));
        }
    }

    /**
     * @brief Update hardware instruction performance metrics
     */
    void updateHardwarePerformanceMetrics(quint8 opcode, quint64 cycles)
    {
        if (m_cpu->hasPerformanceCounters())
        {
            m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HARDWARE_INSTRUCTIONS);
            m_cpu->addPerformanceCycles(enumInstructionPerformance::HARDWARE_INSTRUCTION_CYCLES, cycles);

            // Track per-instruction type metrics
            switch (opcode)
            {
            case OPCODE_HW_MFPR:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HW_MFPR_COUNT);
                break;
            case OPCODE_HW_MTPR:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HW_MTPR_COUNT);
                break;
            case OPCODE_HW_LD:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HW_LD_COUNT);
                break;
            case OPCODE_HW_ST:
            case OPCODE_HW_ST_C:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HW_ST_COUNT);
                break;
            case OPCODE_HW_REI:
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::HW_REI_COUNT);
                break;
            }
        }
    }

    /**
     * @brief Read Internal Processor Register for EV4/EV5
     */
    quint64 readEV4_EV5_IPR(quint32 iprNumber)
    {
        // EV4/EV5 specific IPR implementations
        switch (iprNumber)
        {
        case IPR_EV4_EV5_ICSR:
            return m_cpu->getICSR();
        case IPR_EV4_EV5_IBOX:
            return m_cpu->getIBOX();
        case IPR_EV4_EV5_ICCSR:
            return m_cpu->getICCSR();
        case IPR_EV4_EV5_ITB_PTE:
            return m_cpu->getITB_PTE();
        case IPR_EV4_EV5_DTB_PTE:
            return m_cpu->getDTB_PTE();
        case IPR_EV4_EV5_PS:
            return m_cpu->getProcessorStatus();
        case IPR_EV4_EV5_EXC_ADDR:
            return m_cpu->getExceptionAddress();
        case IPR_EV4_EV5_EXC_SUM:
            return m_cpu->getExceptionSummary();
        case IPR_EV4_EV5_PAL_BASE:
            return m_cpu->getPALBase();
        case IPR_EV4_EV5_HIRR:
            return m_cpu->getHardwareInterruptRequest();
        case IPR_EV4_EV5_SIRR:
            return m_cpu->getSoftwareInterruptRequest();
        case IPR_EV4_EV5_ASTRR:
            return m_cpu->getAST_Request();
        default:
            DEBUG_LOG(QString("readEV4_EV5_IPR: Unknown IPR %1").arg(iprNumber));
            return 0;
        }
    }

    /**
     * @brief Write Internal Processor Register for EV4/EV5
     */
    bool writeEV4_EV5_IPR(quint32 iprNumber, quint64 value)
    {
        // EV4/EV5 specific IPR implementations
        switch (iprNumber)
        {
        case IPR_EV4_EV5_ICSR:
            m_cpu->setICSR(value);
            return true;
        case IPR_EV4_EV5_IBOX:
            m_cpu->setIBOX(value);
            return true;
        case IPR_EV4_EV5_ICCSR:
            m_cpu->setICCSR(value);
            return true;
        case IPR_EV4_EV5_ITB_PTE:
            m_cpu->setITB_PTE(value);
            return true;
        case IPR_EV4_EV5_DTB_PTE:
            m_cpu->setDTB_PTE(value);
            return true;
        case IPR_EV4_EV5_PS:
            m_cpu->setProcessorStatus(value);
            return true;
        case IPR_EV4_EV5_EXC_ADDR:
            return false; // Read-only
        case IPR_EV4_EV5_EXC_SUM:
            m_cpu->setExceptionSummary(value);
            return true;
        case IPR_EV4_EV5_PAL_BASE:
            m_cpu->setPALBase(value);
            return true;
        case IPR_EV4_EV5_HIRR:
            m_cpu->setHardwareInterruptRequest(value);
            return true;
        case IPR_EV4_EV5_SIRR:
            m_cpu->setSoftwareInterruptRequest(value);
            return true;
        case IPR_EV4_EV5_ASTRR:
            m_cpu->setAST_Request(value);
            return true;
        default:
            DEBUG_LOG(QString("writeEV4_EV5_IPR: Unknown IPR %1").arg(iprNumber));
            return false;
        }
    }

    /**
     * @brief Execute EV4/EV5 hardware load operation
     */
    bool executeEV4_EV5_HardwareLoad(quint32 loadType, quint64 address, quint64 &value)
    {
        switch (loadType)
        {
        case HW_LD_EV4_EV5_PHYSICAL:
            return m_cpu->readPhysicalMemory(address, value);
        case HW_LD_EV4_EV5_VIRTUAL_ITB:
            return m_cpu->readVirtualMemoryITB(address, value);
        case HW_LD_EV4_EV5_VIRTUAL_DTB:
            return m_cpu->readVirtualMemoryDTB(address, value);
        default:
            DEBUG_LOG(QString("executeEV4_EV5_HardwareLoad: Unknown load type %1").arg(loadType));
            return false;
        }
    }

    /**
     * @brief Execute EV4/EV5 hardware store operation
     */
    bool executeEV4_EV5_HardwareStore(quint32 storeType, quint64 address, quint64 value)
    {
        switch (storeType)
        {
        case HW_ST_EV4_EV5_PHYSICAL:
            return m_cpu->writePhysicalMemory(address, value);
        case HW_ST_EV4_EV5_VIRTUAL_ITB:
            return m_cpu->writeVirtualMemoryITB(address, value);
        case HW_ST_EV4_EV5_VIRTUAL_DTB:
            return m_cpu->writeVirtualMemoryDTB(address, value);
        default:
            DEBUG_LOG(QString("executeEV4_EV5_HardwareStore: Unknown store type %1").arg(storeType));
            return false;
        }
    }

    /**
     * @brief Execute EV4/EV5 hardware conditional store operation
     */
    bool executeEV4_EV5_HardwareStoreConditional(quint32 storeType, quint64 address, quint64 value)
    {
        // Check lock flag for conditional operations
        if (!m_cpu->checkLockFlag())
        {
            return false;
        }

        bool result = executeEV4_EV5_HardwareStore(storeType, address, value);
        if (result)
        {
            m_cpu->clearLockFlag();
        }
        return result;
    }

    /**
     * @brief Execute EV4/EV5 hardware return from exception
     */
    void executeEV4_EV5_HardwareReturn()
    {
        // EV4/EV5 specific hardware return sequence
        m_cpu->restoreProcessorState();
        m_cpu->enableInterrupts();
        m_cpu->returnFromHardwareException();
    }

    // Similar methods would be implemented for EV6 and EV7 variants...
    // (readEV6_IPR, writeEV6_IPR, executeEV6_HardwareLoad, etc.)

    /**
     * @brief Check if IPR write requires pipeline flush
     */
    bool requiresPipelineFlushOnWrite(quint32 iprNumber) const
    {
        switch (iprNumber)
        {
        case IPR_EV4_EV5_ICSR:
        case IPR_EV4_EV5_ICCSR:
        case IPR_EV4_EV5_PS:
        case IPR_EV4_EV5_PAL_BASE:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief Update hardware instruction statistics
     */
    void updateHardwareInstructionStatistics(quint8 opcode)
    {
        QMutexLocker locker(&m_statsMutex);

        // Add hardware instruction counters
        static quint64 s_iprInstructions = 0;
        static quint64 s_hardwareMemoryInstructions = 0;
        static quint64 s_hardwareControlInstructions = 0;

        switch (opcode)
        {
        case OPCODE_HW_MFPR:
        case OPCODE_HW_MTPR:
            s_iprInstructions++;
            break;

        case OPCODE_HW_LD:
        case OPCODE_HW_ST:
        case OPCODE_HW_ST_C:
            s_hardwareMemoryInstructions++;
            break;

        case OPCODE_HW_REI:
            s_hardwareControlInstructions++;
            break;
        }
    }


    /**
     * @brief Check if a PAL function requires pipeline flush
     * @param palFunction The PAL function code
     * @return true if pipeline flush is required
     */
    bool isPipelineFlushRequired(quint32 palFunction) const
    {
        switch (palFunction)
        {
        case PAL_SWPCTX:  // Context switch
        case PAL_SWPPAL:  // PAL base change
        case PAL_TBIA:    // TLB invalidate all
        case PAL_SWPIRQL: // IPL change
        case PAL_WRFEN:   // FP enable change
        case PAL_IMB:     // Instruction memory barrier
            return true;

        default:
            return false;
        }
    }

    
    // Enhanced executeBranchGroup using your existing constexpr constants
    // This consolidates all branch instruction handling into a single method

    void executeBranchGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto F = [this](int r) -> quint64 { return m_cpu->getFloatRegister64(r); };
        auto FD = [this](int r) -> double { return m_cpu->getFloatRegister32(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        quint64 raValue = R(instruction.ra);
        quint64 currentPC = m_cpu->getPc();
        quint64 targetPC =
            currentPC + (static_cast<qint64>(static_cast<qint32>(instruction.immediate << 2))); // Sign-extend and scale
        bool takeBranch = false;
        bool isPredicted = false;
        QString branchType;

        DEBUG_LOG(QString("Branch instruction: opcode=0x%1, ra=%2, disp=0x%3, target=0x%4")
                      .arg(instruction.opcode, 2, 16, QChar('0'))
                      .arg(instruction.ra)
                      .arg(instruction.immediate, 8, 16, QChar('0'))
                      .arg(targetPC, 16, 16, QChar('0')));

        switch (instruction.opcode)
        {
            // ═══════════════════════════════════════════════════════════════════════════
            // Unconditional Branch Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_BR: // BR - Unconditional Branch
        {
            // Store return address in Ra if not R31
            if (instruction.ra != 31)
            {
                RW(instruction.ra, currentPC + 4);
            }

            takeBranch = true;
            branchType = "BR (Unconditional)";

            DEBUG_LOG(QString("BR: Unconditional branch to 0x%1, return address in R%2")
                          .arg(targetPC, 16, 16, QChar('0'))
                          .arg(instruction.ra));
            break;
        }

        case OPCODE_BSR: // BSR - Branch to Subroutine
        {
            // Always store return address (even if Ra is 31)
            if (instruction.ra != 31)
            {
                RW(instruction.ra, currentPC + 4);
            }

            // Push return address onto hardware return stack (if implemented)
            m_cpu->pushReturnStack(currentPC + 4);

            takeBranch = true;
            branchType = "BSR (Subroutine)";

            DEBUG_LOG(QString("BSR: Branch to subroutine 0x%1, return address 0x%2 in R%3")
                          .arg(targetPC, 16, 16, QChar('0'))
                          .arg(currentPC + 4, 16, 16, QChar('0'))
                          .arg(instruction.ra));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Integer Conditional Branch Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_BEQ: // BEQ - Branch if Equal (to zero)
        {
            takeBranch = (raValue == 0);
            branchType = "BEQ";

            DEBUG_LOG(QString("BEQ: R%1=0x%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(raValue, 16, 16, QChar('0'))
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_BNE: // BNE - Branch if Not Equal (to zero)
        {
            takeBranch = (raValue != 0);
            branchType = "BNE";

            DEBUG_LOG(QString("BNE: R%1=0x%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(raValue, 16, 16, QChar('0'))
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_BLT: // BLT - Branch if Less Than (zero, signed)
        {
            takeBranch = (static_cast<qint64>(raValue) < 0);
            branchType = "BLT";

            DEBUG_LOG(QString("BLT: R%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(static_cast<qint64>(raValue))
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_BGE: // BGE - Branch if Greater than or Equal (to zero, signed)
        {
            takeBranch = (static_cast<qint64>(raValue) >= 0);
            branchType = "BGE";

            DEBUG_LOG(QString("BGE: R%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(static_cast<qint64>(raValue))
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_BLE: // BLE - Branch if Less than or Equal (to zero, signed)
        {
            takeBranch = (static_cast<qint64>(raValue) <= 0);
            branchType = "BLE";

            DEBUG_LOG(QString("BLE: R%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(static_cast<qint64>(raValue))
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_BGT: // BGT - Branch if Greater Than (zero, signed)
        {
            takeBranch = (static_cast<qint64>(raValue) > 0);
            branchType = "BGT";

            DEBUG_LOG(QString("BGT: R%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(static_cast<qint64>(raValue))
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Bit Test Branch Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_BLBC: // BLBC - Branch if Low Bit Clear
        {
            takeBranch = ((raValue & 1) == 0);
            branchType = "BLBC";

            DEBUG_LOG(QString("BLBC: R%1=0x%2, low bit=%3, condition=%4")
                          .arg(instruction.ra)
                          .arg(raValue, 16, 16, QChar('0'))
                          .arg(raValue & 1)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_BLBS: // BLBS - Branch if Low Bit Set
        {
            takeBranch = ((raValue & 1) == 1);
            branchType = "BLBS";

            DEBUG_LOG(QString("BLBS: R%1=0x%2, low bit=%3, condition=%4")
                          .arg(instruction.ra)
                          .arg(raValue, 16, 16, QChar('0'))
                          .arg(raValue & 1)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Floating-Point Branch Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_FBEQ: // FBEQ - Floating Branch if Equal (to zero)
        {
            double faValueFloat = FD(instruction.ra);
            takeBranch = (faValueFloat == 0.0);
            branchType = "FBEQ";

            DEBUG_LOG(QString("FBEQ: F%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(faValueFloat)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_FBNE: // FBNE - Floating Branch if Not Equal (to zero)
        {
            double faValueFloat = FD(instruction.ra);
            takeBranch = (faValueFloat != 0.0);
            branchType = "FBNE";

            DEBUG_LOG(QString("FBNE: F%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(faValueFloat)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_FBLT: // FBLT - Floating Branch if Less Than (zero)
        {
            double faValueFloat = FD(instruction.ra);
            takeBranch = (faValueFloat < 0.0);
            branchType = "FBLT";

            DEBUG_LOG(QString("FBLT: F%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(faValueFloat)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_FBGE: // FBGE - Floating Branch if Greater than or Equal (to zero)
        {
            double faValueFloat = FD(instruction.ra);
            takeBranch = (faValueFloat >= 0.0);
            branchType = "FBGE";

            DEBUG_LOG(QString("FBGE: F%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(faValueFloat)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_FBLE: // FBLE - Floating Branch if Less Than or Equal (to zero)
        {
            double faValueFloat = FD(instruction.ra);
            takeBranch = (faValueFloat <= 0.0);
            branchType = "FBLE";

            DEBUG_LOG(QString("FBLE: F%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(faValueFloat)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

        case OPCODE_FBGT: // FBGT - Floating Branch if Greater Than (zero)
        {
            double faValueFloat = FD(instruction.ra);
            takeBranch = (faValueFloat > 0.0);
            branchType = "FBGT";

            DEBUG_LOG(QString("FBGT: F%1=%2, condition=%3")
                          .arg(instruction.ra)
                          .arg(faValueFloat)
                          .arg(takeBranch ? "TRUE" : "FALSE"));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Unknown Branch Opcode
            // ═══════════════════════════════════════════════════════════════════════════

        default:
        {
            DEBUG_LOG(
                QString("executeBranchGroup: Unknown branch opcode 0x%1").arg(instruction.opcode, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            return;
        }
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // Branch Prediction and Execution
        // ═══════════════════════════════════════════════════════════════════════════

        // Check branch predictor (if implemented)
        if (m_cpu->hasBranchPredictor())
        {
            isPredicted = m_cpu->predictBranch(currentPC, targetPC, branchType);

            // Update branch predictor with actual outcome
            m_cpu->updateBranchPredictor(currentPC, takeBranch);

            // Check for misprediction
            if (isPredicted != takeBranch)
            {
                DEBUG_LOG(QString("Branch misprediction: predicted=%1, actual=%2")
                              .arg(isPredicted ? "TAKEN" : "NOT_TAKEN")
                              .arg(takeBranch ? "TAKEN" : "NOT_TAKEN"));

                // Update misprediction statistics
                updateBranchStatistics(true);

                // Misprediction penalty - flush pipeline
                m_cpu->flushPipeline();
                m_cpu->addMispredictionPenalty();
            }
            else
            {
                DEBUG_LOG("Branch correctly predicted");
                updateBranchStatistics(false);
            }
        }

        // Execute the branch
        if (takeBranch)
        {
            // Validate target address alignment
            if ((targetPC & 0x3) != 0)
            {
                DEBUG_LOG(QString("Branch target misalignment: 0x%1").arg(targetPC, 16, 16, QChar('0')));
                m_cpu->triggerException(ExceptionType::ALIGNMENT_FAULT, targetPC);
                return;
            }

            // Set new PC
            m_cpu->setPc(targetPC);

            // Flush pipeline if not predicted correctly or no predictor
            if (!m_cpu->hasBranchPredictor() || (isPredicted != takeBranch))
            {
                m_cpu->flushPipeline();
            }

            DEBUG_LOG(QString("%1: Branch TAKEN to 0x%2").arg(branchType).arg(targetPC, 16, 16, QChar('0')));
        }
        else
        {
            DEBUG_LOG(QString("%1: Branch NOT TAKEN, continuing to 0x%2")
                          .arg(branchType)
                          .arg(currentPC + 4, 16, 16, QChar('0')));
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // Branch Statistics and Performance Monitoring
        // ═══════════════════════════════════════════════════════════════════════════

        // Update branch instruction statistics
        m_branchInstructions++;
        m_totalInstructions++;

        // Track branch type statistics
        updateBranchTypeStatistics(instruction.opcode, takeBranch);

        // Update performance counters
        if (m_cpu->hasPerformanceCounters())
        {
            m_cpu->incrementPerformanceCounter(enumInstructionPerformance::BRANCH_INSTRUCTIONS);

            if (takeBranch)
            {
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::BRANCHES_TAKEN);
            }
            else
            {
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::BRANCHES_NOT_TAKEN);
            }

            if (isPredicted != takeBranch)
            {
                m_cpu->incrementPerformanceCounter(enumInstructionPerformance::BRANCH_MISPREDICTIONS);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Helper Methods for Branch Statistics and Analysis
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Update branch statistics with misprediction information
     * @param mispredicted Whether the branch was mispredicted
     */
    void updateBranchStatistics(bool mispredicted)
    {
        QMutexLocker locker(&m_statsMutex);

        if (mispredicted)
        {
            m_branchMispredictions++;
        }
    }

    /**
     * @brief Update detailed branch type statistics
     * @param opcode The branch instruction opcode
     * @param taken Whether the branch was taken
     */
    void updateBranchTypeStatistics(quint8 opcode, bool taken)
    {
        // This could be expanded to track detailed statistics per branch type
        switch (opcode)
        {
        case OPCODE_BR:
        case OPCODE_BSR:
            // Unconditional branches - always taken
            m_unconditionalBranches++;
            break;

        case OPCODE_BEQ:
        case OPCODE_BNE:
        case OPCODE_BLT:
        case OPCODE_BGE:
        case OPCODE_BLE:
        case OPCODE_BGT:
            // Integer conditional branches
            m_integerConditionalBranches++;
            if (taken)
                m_integerBranchesTaken++;
            break;

        case OPCODE_BLBC:
        case OPCODE_BLBS:
            // Bit test branches
            m_bitTestBranches++;
            if (taken)
                m_bitTestBranchesTaken++;
            break;

        case OPCODE_FBEQ:
        case OPCODE_FBNE:
        case OPCODE_FBLT:
        case OPCODE_FBGE:
        case OPCODE_FBLE:
        case OPCODE_FBGT:
            // Floating-point branches
            m_floatingPointBranches++;
            if (taken)
                m_floatingPointBranchesTaken++;
            break;
        }
    }

    /**
     * @brief Get branch prediction accuracy statistics
     * @return Branch prediction accuracy as a percentage
     */
    double getBranchPredictionAccuracy() const
    {
        QMutexLocker locker(&m_statsMutex);

        if (m_branchInstructions == 0)
            return 0.0;

        quint64 correctPredictions = m_branchInstructions - m_branchMispredictions;
        return (static_cast<double>(correctPredictions) / m_branchInstructions) * 100.0;
    }

    /**
     * @brief Print detailed branch statistics
     */
    void printBranchStatistics() const
    {
        QMutexLocker locker(&m_statsMutex);

        if (m_branchInstructions == 0)
        {
            DEBUG_LOG("Branch Statistics: No branch instructions executed");
            return;
        }

        double mispredictionRate = (static_cast<double>(m_branchMispredictions) / m_branchInstructions) * 100.0;
        double predictionAccuracy = getBranchPredictionAccuracy();

        DEBUG_LOG("Branch Statistics:");
        DEBUG_LOG(QString("  Total Branch Instructions: %1").arg(m_branchInstructions));
        DEBUG_LOG(
            QString("  Branch Mispredictions: %1 (%2%)").arg(m_branchMispredictions).arg(mispredictionRate, 0, 'f', 2));
        DEBUG_LOG(QString("  Branch Prediction Accuracy: %1%").arg(predictionAccuracy, 0, 'f', 2));

        if (m_unconditionalBranches > 0)
        {
            DEBUG_LOG(QString("  Unconditional Branches: %1").arg(m_unconditionalBranches));
        }

        if (m_integerConditionalBranches > 0)
        {
            double integerTakenRate =
                (static_cast<double>(m_integerBranchesTaken) / m_integerConditionalBranches) * 100.0;
            DEBUG_LOG(QString("  Integer Conditional Branches: %1 (taken: %2, %3%)")
                          .arg(m_integerConditionalBranches)
                          .arg(m_integerBranchesTaken)
                          .arg(integerTakenRate, 0, 'f', 2));
        }

        if (m_bitTestBranches > 0)
        {
            double bitTestTakenRate = (static_cast<double>(m_bitTestBranchesTaken) / m_bitTestBranches) * 100.0;
            DEBUG_LOG(QString("  Bit Test Branches: %1 (taken: %2, %3%)")
                          .arg(m_bitTestBranches)
                          .arg(m_bitTestBranchesTaken)
                          .arg(bitTestTakenRate, 0, 'f', 2));
        }

        if (m_floatingPointBranches > 0)
        {
            double fpTakenRate = (static_cast<double>(m_floatingPointBranchesTaken) / m_floatingPointBranches) * 100.0;
            DEBUG_LOG(QString("  Floating-Point Branches: %1 (taken: %2, %3%)")
                          .arg(m_floatingPointBranches)
                          .arg(m_floatingPointBranchesTaken)
                          .arg(fpTakenRate, 0, 'f', 2));
        }
    }

    bool executeLoad(quint64 virtualAddr, quint64 &value, int size)
    {
        // Same clean interface for all memory operations
        return m_memorySystem->readVirtualMemory(m_cpu, virtualAddr, value, size, m_currentPC);
    }

    // Enhanced executeMemoryGroup using your existing constexpr constants
    // This consolidates all memory instruction handling into a single method

    void executeMemoryGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto F = [this](int r) -> quint64 { return m_cpu->getFloatRegister64(r); };
        auto F32 = [this](int r) -> float { return m_cpu->getFloatRegister32(r); };
        auto FD = [this](int r) -> double { return m_cpu->getFloatRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };
        auto FW = [this](int r, quint64 v) { m_cpu->setFloatRegister(r, v); };
        auto FW32 = [this](int r, float v) { m_cpu->setFloatRegister(r, v); };
        auto FWD = [this](int r, double v) { m_cpu->setFloatRegister(r, v); };

        // Calculate effective address (base + displacement)
        quint64 baseValue = R(instruction.rb);
        qint64 signedDisplacement =
            static_cast<qint64>(static_cast<qint16>(instruction.immediate)); // Sign-extend 16-bit displacement
        quint64 effectiveAddress = baseValue + signedDisplacement;

        DEBUG_LOG(QString("Memory operation: opcode=0x%1, ra=%2, rb=%3, disp=%4, EA=0x%5")
                      .arg(instruction.opcode, 2, 16, QChar('0'))
                      .arg(instruction.ra)
                      .arg(instruction.rb)
                      .arg(signedDisplacement)
                      .arg(effectiveAddress, 16, 16, QChar('0')));

        switch (instruction.opcode)
        {
            // ═══════════════════════════════════════════════════════════════════════════
            // Address Calculation Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_LDA: // LDA - Load Address
        {
            // LDA calculates effective address and stores it in Ra
            RW(instruction.ra, effectiveAddress);

            DEBUG_LOG(QString("LDA: R%1 = 0x%2 + %3 = 0x%4")
                          .arg(instruction.ra)
                          .arg(baseValue, 16, 16, QChar('0'))
                          .arg(signedDisplacement)
                          .arg(effectiveAddress, 16, 16, QChar('0')));
            break;
        }

        case OPCODE_LDAH: // LDAH - Load Address High
        {
            // LDAH calculates effective address with displacement shifted left 16 bits
            quint64 result = baseValue + (signedDisplacement << 16);
            RW(instruction.ra, result);

            DEBUG_LOG(QString("LDAH: R%1 = 0x%2 + (%3 << 16) = 0x%4")
                          .arg(instruction.ra)
                          .arg(baseValue, 16, 16, QChar('0'))
                          .arg(signedDisplacement)
                          .arg(result, 16, 16, QChar('0')));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Integer Load Operations (Unaligned and Special)
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_LDBU: // LDBU - Load Byte Unsigned
        {
            quint8 value;
            if (m_cpu->readMemory8(effectiveAddress, value))
            {
                RW(instruction.ra, static_cast<quint64>(value)); // Zero-extend to 64 bits
                DEBUG_LOG(QString("LDBU: R%1 = 0x%2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(value, 2, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDWU: // LDWU - Load Word Unsigned
        {
            quint16 value;
            if (m_cpu->readMemory16(effectiveAddress, value))
            {
                RW(instruction.ra, static_cast<quint64>(value)); // Zero-extend to 64 bits
                DEBUG_LOG(QString("LDWU: R%1 = 0x%2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(value, 4, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDQ_U: // LDQ_U - Load Quadword Unaligned
        {
            // Alpha LDQ_U loads from quadword boundary, ignoring low 3 bits of address
            quint64 alignedAddress = effectiveAddress & ~0x7ULL;
            quint64 value;
            if (m_cpu->readMemory64(alignedAddress, value))
            {
                RW(instruction.ra, value);
                DEBUG_LOG(QString("LDQ_U: R%1 = 0x%2 from aligned EA=0x%3 (original EA=0x%4)")
                              .arg(instruction.ra)
                              .arg(value, 16, 16, QChar('0'))
                              .arg(alignedAddress, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, alignedAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Integer Store Operations (Unaligned and Special)
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_STB: // STB - Store Byte
        {
            quint8 value = static_cast<quint8>(R(instruction.ra));
            if (m_cpu->writeMemory8(effectiveAddress, value))
            {
                DEBUG_LOG(QString("STB: Stored 0x%1 to EA=0x%2")
                              .arg(value, 2, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_STW: // STW - Store Word
        {
            quint16 value = static_cast<quint16>(R(instruction.ra));
            if (m_cpu->writeMemory16(effectiveAddress, value))
            {
                DEBUG_LOG(QString("STW: Stored 0x%1 to EA=0x%2")
                              .arg(value, 4, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_STQ_U: // STQ_U - Store Quadword Unaligned
        {
            // Alpha STQ_U stores to quadword boundary, ignoring low 3 bits of address
            quint64 alignedAddress = effectiveAddress & ~0x7ULL;
            quint64 value = R(instruction.ra);
            if (m_cpu->writeMemory64(alignedAddress, value))
            {
                DEBUG_LOG(QString("STQ_U: Stored 0x%1 to aligned EA=0x%2 (original EA=0x%3)")
                              .arg(value, 16, 16, QChar('0'))
                              .arg(alignedAddress, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, alignedAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Floating-Point Load Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_LDF: // LDF - Load F_floating (VAX 32-bit)
        {
            quint32 rawValue;
            if (m_cpu->readMemory32(effectiveAddress, rawValue))
            {
                // Convert VAX F_floating to internal representation
                quint64 convertedValue = m_cpu->convertVaxFToInternal(rawValue);
                FW(instruction.ra, convertedValue);
                DEBUG_LOG(QString("LDF: F%1 = VAX F_floating 0x%2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(rawValue, 8, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDG: // LDG - Load G_floating (VAX 64-bit)
        {
            quint64 rawValue;
            if (m_cpu->readMemory64(effectiveAddress, rawValue))
            {
                // Convert VAX G_floating to internal representation
                quint64 convertedValue = m_cpu->convertVaxGToInternal(rawValue);
                FW(instruction.ra, convertedValue);
                DEBUG_LOG(QString("LDG: F%1 = VAX G_floating 0x%2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(rawValue, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDS: // LDS - Load S_floating (IEEE 32-bit)
        {
            float value;
            if (m_cpu->readMemoryFloat(effectiveAddress, value))
            {
                FW32(instruction.ra, value);
                DEBUG_LOG(QString("LDS: F%1 = IEEE S_floating %2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(value)
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDT: // LDT - Load T_floating (IEEE 64-bit)
        {
            double value;
            if (m_cpu->readMemoryDouble(effectiveAddress, value))
            {
                FWD(instruction.ra, value);
                DEBUG_LOG(QString("LDT: F%1 = IEEE T_floating %2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(value)
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Floating-Point Store Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_STF: // STF - Store F_floating (VAX 32-bit)
        {
            quint64 internalValue = F(instruction.ra);
            quint32 vaxFValue = m_cpu->convertInternalToVaxF(internalValue);
            if (m_cpu->writeMemory32(effectiveAddress, vaxFValue))
            {
                DEBUG_LOG(QString("STF: Stored VAX F_floating 0x%1 to EA=0x%2")
                              .arg(vaxFValue, 8, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_STG: // STG - Store G_floating (VAX 64-bit)
        {
            quint64 internalValue = F(instruction.ra);
            quint64 vaxGValue = m_cpu->convertInternalToVaxG(internalValue);
            if (m_cpu->writeMemory64(effectiveAddress, vaxGValue))
            {
                DEBUG_LOG(QString("STG: Stored VAX G_floating 0x%1 to EA=0x%2")
                              .arg(vaxGValue, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_STS: // STS - Store S_floating (IEEE 32-bit)
        {
            float value = F32(instruction.ra);
            if (m_cpu->writeMemoryFloat(effectiveAddress, value))
            {
                DEBUG_LOG(QString("STS: Stored IEEE S_floating %1 to EA=0x%2")
                              .arg(value)
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_STT: // STT - Store T_floating (IEEE 64-bit)
        {
            double value = FD(instruction.ra);
            if (m_cpu->writeMemoryDouble(effectiveAddress, value))
            {
                DEBUG_LOG(QString("STT: Stored IEEE T_floating %1 to EA=0x%2")
                              .arg(value)
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Integer Load Operations (Aligned)
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_LDL: // LDL - Load Longword (32-bit signed)
        {
            qint32 value;
            if (m_cpu->readMemory32(effectiveAddress, reinterpret_cast<quint32 &>(value)))
            {
                // Sign-extend to 64 bits
                quint64 result = static_cast<quint64>(static_cast<qint64>(value));
                RW(instruction.ra, result);
                DEBUG_LOG(QString("LDL: R%1 = %2 (0x%3) from EA=0x%4")
                              .arg(instruction.ra)
                              .arg(value)
                              .arg(result, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDQ: // LDQ - Load Quadword (64-bit)
        {
            quint64 value;
            if (m_cpu->readMemory64(effectiveAddress, value))
            {
                RW(instruction.ra, value);
                DEBUG_LOG(QString("LDQ: R%1 = 0x%2 from EA=0x%3")
                              .arg(instruction.ra)
                              .arg(value, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Locked Load Operations (for atomic operations)
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_LDL_L: // LDL_L - Load Longword Locked
        {
            qint32 value;
            if (m_cpu->readMemory32Locked(effectiveAddress, reinterpret_cast<quint32 &>(value)))
            {
                quint64 result = static_cast<quint64>(static_cast<qint64>(value));
                RW(instruction.ra, result);
                DEBUG_LOG(QString("LDL_L: R%1 = %2 (0x%3) from EA=0x%4 (LOCKED)")
                              .arg(instruction.ra)
                              .arg(value)
                              .arg(result, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_LDQ_L: // LDQ_L - Load Quadword Locked
        {
            quint64 value;
            if (m_cpu->readMemory64Locked(effectiveAddress, value))
            {
                RW(instruction.ra, value);
                DEBUG_LOG(QString("LDQ_L: R%1 = 0x%2 from EA=0x%3 (LOCKED)")
                              .arg(instruction.ra)
                              .arg(value, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Integer Store Operations (Aligned)
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_STL: // STL - Store Longword (32-bit)
        {
            quint32 value = static_cast<quint32>(R(instruction.ra));
            if (m_cpu->writeMemory32(effectiveAddress, value))
            {
                DEBUG_LOG(QString("STL: Stored 0x%1 to EA=0x%2")
                              .arg(value, 8, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

        case OPCODE_STQ: // STQ - Store Quadword (64-bit)
        {
            quint64 value = R(instruction.ra);
            if (m_cpu->writeMemory64(effectiveAddress, value))
            {
                DEBUG_LOG(QString("STQ: Stored 0x%1 to EA=0x%2")
                              .arg(value, 16, 16, QChar('0'))
                              .arg(effectiveAddress, 16, 16, QChar('0')));
            }
            else
            {
                m_cpu->triggerException(ExceptionType::MEMORY_ACCESS_FAULT, effectiveAddress);
            }
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Conditional Store Operations (for atomic operations)
            // ═══════════════════════════════════════════════════════════════════════════

        case OPCODE_STL_C: // STL_C - Store Longword Conditional
        {
            quint32 value = static_cast<quint32>(R(instruction.ra));
            bool success = m_cpu->writeMemory32Conditional(effectiveAddress, value);

            // Store success/failure in Ra (0 = failed, 1 = succeeded)
            RW(instruction.ra, success ? 1 : 0);

            DEBUG_LOG(QString("STL_C: Conditional store 0x%1 to EA=0x%2 = %3")
                          .arg(value, 8, 16, QChar('0'))
                          .arg(effectiveAddress, 16, 16, QChar('0'))
                          .arg(success ? "SUCCESS" : "FAILED"));
            break;
        }

        case OPCODE_STQ_C: // STQ_C - Store Quadword Conditional
        {
            quint64 value = R(instruction.ra);
            bool success = m_cpu->writeMemory64Conditional(effectiveAddress, value);

            // Store success/failure in Ra (0 = failed, 1 = succeeded)
            RW(instruction.ra, success ? 1 : 0);

            DEBUG_LOG(QString("STQ_C: Conditional store 0x%1 to EA=0x%2 = %3")
                          .arg(value, 16, 16, QChar('0'))
                          .arg(effectiveAddress, 16, 16, QChar('0'))
                          .arg(success ? "SUCCESS" : "FAILED"));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Unknown Memory Opcode
            // ═══════════════════════════════════════════════════════════════════════════

        default:
        {
            DEBUG_LOG(
                QString("executeMemoryGroup: Unknown memory opcode 0x%1").arg(instruction.opcode, 2, 16, QChar('0')));
            m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            return;
        }
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // Memory Operation Statistics and Performance Monitoring
        // ═══════════════════════════════════════════════════════════════════════════

        // Update memory instruction statistics
        m_memoryInstructions++;
        m_totalInstructions++;

        // Track memory operation type statistics
        updateMemoryOperationStatistics(instruction.opcode, effectiveAddress);

        // Update performance counters
        if (m_cpu->hasPerformanceCounters())
        {
            m_cpu->incrementPerformanceCounter(AsaPerformance::MEMORY_INSTRUCTIONS);

            // Categorize by operation type
            if (isLoadOperation(instruction.opcode))
            {
                m_cpu->incrementPerformanceCounter(AsaPerformance::LOAD_INSTRUCTIONS);
            }
            else if (isStoreOperation(instruction.opcode))
            {
                m_cpu->incrementPerformanceCounter(AsaPerformance::STORE_INSTRUCTIONS);
            }

            // Track cache performance (if cache simulation is enabled)
            if (m_cpu->hasCacheSimulation())
            {
                bool cacheHit = m_cpu->checkCacheHit(effectiveAddress);
                if (cacheHit)
                {
                    m_cpu->incrementPerformanceCounter(AsaPerformance::CACHE_HITS);
                }
                else
                {
                    m_cpu->incrementPerformanceCounter(AsaPerformance::CACHE_MISSES);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Helper Methods for Memory Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Check if an opcode is a load operation
     */
    bool isLoadOperation(quint8 opcode) const
    {
        switch (opcode)
        {
        case OPCODE_LDA:
        case OPCODE_LDAH:
        case OPCODE_LDBU:
        case OPCODE_LDWU:
        case OPCODE_LDQ_U:
        case OPCODE_LDF:
        case OPCODE_LDG:
        case OPCODE_LDS:
        case OPCODE_LDT:
        case OPCODE_LDL:
        case OPCODE_LDQ:
        case OPCODE_LDL_L:
        case OPCODE_LDQ_L:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief Check if an opcode is a store operation
     */
    bool isStoreOperation(quint8 opcode) const
    {
        switch (opcode)
        {
        case OPCODE_STB:
        case OPCODE_STW:
        case OPCODE_STQ_U:
        case OPCODE_STF:
        case OPCODE_STG:
        case OPCODE_STS:
        case OPCODE_STT:
        case OPCODE_STL:
        case OPCODE_STQ:
        case OPCODE_STL_C:
        case OPCODE_STQ_C:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief Update detailed memory operation statistics
     */
    void updateMemoryOperationStatistics(quint8 opcode, quint64 address)
    {
        // Track memory access patterns
        switch (opcode)
        {
        // Integer loads
        case OPCODE_LDBU:
        case OPCODE_LDWU:
        case OPCODE_LDL:
        case OPCODE_LDQ:
        case OPCODE_LDQ_U:
        case OPCODE_LDL_L:
        case OPCODE_LDQ_L:
            m_integerLoads++;
            break;

        // Integer stores
        case OPCODE_STB:
        case OPCODE_STW:
        case OPCODE_STL:
        case OPCODE_STQ:
        case OPCODE_STQ_U:
        case OPCODE_STL_C:
        case OPCODE_STQ_C:
            m_integerStores++;
            break;

        // Floating-point loads
        case OPCODE_LDF:
        case OPCODE_LDG:
        case OPCODE_LDS:
        case OPCODE_LDT:
            m_floatingPointLoads++;
            break;

        // Floating-point stores
        case OPCODE_STF:
        case OPCODE_STG:
        case OPCODE_STS:
        case OPCODE_STT:
            m_floatingPointStores++;
            break;

        // Address calculations
        case OPCODE_LDA:
        case OPCODE_LDAH:
            m_addressCalculations++;
            break;
        }

        // Track alignment
        if ((address & 0x7) != 0)
        {
            m_unalignedAccesses++;
        }
    }

    /**
     * @brief Print detailed memory operation statistics
     */
    void printMemoryStatistics() const
    {
        QMutexLocker locker(&m_statsMutex);

        if (m_memoryInstructions == 0)
        {
            DEBUG_LOG("Memory Statistics: No memory instructions executed");
            return;
        }

        DEBUG_LOG("Memory Operation Statistics:");
        DEBUG_LOG(QString("  Total Memory Instructions: %1").arg(m_memoryInstructions));
        DEBUG_LOG(QString("  Integer Loads: %1").arg(m_integerLoads));
        DEBUG_LOG(QString("  Integer Stores: %1").arg(m_integerStores));
        DEBUG_LOG(QString("  Floating-Point Loads: %1").arg(m_floatingPointLoads));
        DEBUG_LOG(QString("  Floating-Point Stores: %1").arg(m_floatingPointStores));
        DEBUG_LOG(QString("  Address Calculations: %1").arg(m_addressCalculations));
        DEBUG_LOG(QString("  Unaligned Accesses: %1").arg(m_unalignedAccesses));

        if (m_memoryInstructions > 0)
        {
            double unalignedRate = (static_cast<double>(m_unalignedAccesses) / m_memoryInstructions) * 100.0;
            DEBUG_LOG(QString("  Unaligned Access Rate: %1%").arg(unalignedRate, 0, 'f', 2));
        }
    }


    /**
     * @brief Execute miscellaneous instruction group (opcode 0x18)
     * @param instruction Decoded instruction to execute
     *
     * Handles: TRAPB, EXCB, MB, WMB, FETCH, FETCH_M, RPCC, RC, RS, ECB
     */
    void executeMiscGroup(const DecodedInstruction &instruction)
    {
        // Helper lambdas for register access
        auto R = [this](int r) -> quint64 { return (r == 31) ? 0 : m_cpu->getRegister(r); };
        auto RW = [this](int r, quint64 v)
        {
            if (r != 31)
                m_cpu->setRegister(r, v);
        };

        quint64 raValue = R(instruction.ra);
        quint64 rbValue = R(instruction.rb);
        quint64 result = 0;

        DEBUG_LOG(QString("Miscellaneous instruction: function=0x%1, ra=%2, rb=%3, rc=%4")
                      .arg(instruction.function, 4, 16, QChar('0'))
                      .arg(instruction.ra)
                      .arg(instruction.rb)
                      .arg(instruction.rc));

        switch (instruction.function)
        {
            // ═══════════════════════════════════════════════════════════════════════════
            // Memory and Exception Barrier Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_TRAPB: // 0x0000 - Trap Barrier
        {
            DEBUG_LOG("TRAPB: Trap barrier - ensuring all prior instructions complete");

            // Ensure all prior instructions complete before any traps can occur
            // This includes:
            // 1. All pending arithmetic operations complete
            // 2. All pending memory operations complete
            // 3. All pending floating-point operations complete
            // 4. Pipeline is synchronized

            m_cpu->synchronizePipeline();
            m_cpu->flushPendingTraps();

            // Stall pipeline briefly to ensure synchronization
            stall(1);

            DEBUG_LOG("TRAPB: Trap barrier completed");
            break;
        }

        case FUNC_EXCB: // 0x0400 - Exception Barrier
        {
            DEBUG_LOG("EXCB: Exception barrier - ensuring exception ordering");

            // Ensure that:
            // 1. All prior instructions complete before any exceptions
            // 2. No instruction after EXCB can complete before EXCB
            // 3. Memory ordering is preserved

            m_cpu->exceptionBarrier();
            m_cpu->synchronizePipeline();

            // Brief stall to ensure proper ordering
            stall(1);

            DEBUG_LOG("EXCB: Exception barrier completed");
            break;
        }

        case FUNC_MB: // 0x4000 - Memory Barrier
        {
            DEBUG_LOG("MB: Memory barrier - full memory synchronization");

            // Full memory barrier:
            // 1. All prior loads and stores complete
            // 2. No subsequent loads or stores can begin until all prior complete
            // 3. Flush write buffers
            // 4. Invalidate speculative loads

            m_cpu->fullMemoryBarrier();
            m_cpu->flushWriteBuffers();
            m_cpu->invalidateSpeculativeLoads();

            // Memory barriers can be expensive - add realistic stall
            stall(2);

            DEBUG_LOG("MB: Full memory barrier completed");
            break;
        }

        case FUNC_WMB: // 0x4400 - Write Memory Barrier
        {
            DEBUG_LOG("WMB: Write memory barrier - store ordering");

            // Write memory barrier:
            // 1. All prior stores complete before any subsequent stores
            // 2. Flush write buffers
            // 3. Ensure store ordering

            m_cpu->writeMemoryBarrier();
            m_cpu->flushWriteBuffers();

            // Write barriers are typically less expensive than full barriers
            stall(1);

            DEBUG_LOG("WMB: Write memory barrier completed");
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Cache Management Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_FETCH: // 0x8000 - Fetch (Cache Prefetch Hint)
        {
            quint64 address = rbValue;

            DEBUG_LOG(QString("FETCH: Cache prefetch hint for address 0x%1").arg(address, 16, 16, QChar('0')));

            // Prefetch hint - attempt to bring cache line into cache
            // This is a performance hint and should not affect correctness
            if (m_cpu->hasCache())
            {
                m_cpu->prefetchCacheLine(address, false); // Non-exclusive prefetch
            }

            // Prefetch is asynchronous and should not stall
            DEBUG_LOG("FETCH: Prefetch request issued");
            break;
        }

        case FUNC_FETCH_M: // 0xA000 - Fetch with Modify Intent
        {
            quint64 address = rbValue;

            DEBUG_LOG(
                QString("FETCH_M: Cache prefetch (modify) hint for address 0x%1").arg(address, 16, 16, QChar('0')));

            // Prefetch with modify intent - bring cache line into cache for writing
            if (m_cpu->hasCache())
            {
                m_cpu->prefetchCacheLine(address, true); // Exclusive prefetch for modification
            }

            DEBUG_LOG("FETCH_M: Prefetch (modify) request issued");
            break;
        }

        case FUNC_ECB: // 0xE800 - Evict Cache Block
        {
            quint64 address = rbValue;

            DEBUG_LOG(QString("ECB: Evict cache block for address 0x%1").arg(address, 16, 16, QChar('0')));

            // Evict cache block - remove from all cache levels
            if (m_cpu->hasCache())
            {
                m_cpu->evictCacheBlock(address);
            }

            DEBUG_LOG("ECB: Cache block eviction completed");
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Performance and Timing Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_RPCC: // 0xC000 - Read Process Cycle Counter
        {
            quint64 cycleCount = m_cpu->getProcessCycleCounter();
            RW(instruction.ra, cycleCount);

            DEBUG_LOG(QString("RPCC: Read process cycle counter = %1 -> R%2").arg(cycleCount).arg(instruction.ra));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Lock Flag Operations
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_RC: // 0xE000 - Read and Clear Lock Flag
        {
            quint64 lockFlag = m_cpu->readLockFlag();
            m_cpu->clearLockFlag();
            RW(instruction.ra, lockFlag);

            DEBUG_LOG(QString("RC: Read and clear lock flag = %1 -> R%2").arg(lockFlag).arg(instruction.ra));
            break;
        }

        case FUNC_RS: // 0xF000 - Read and Set Lock Flag
        {
            quint64 lockFlag = m_cpu->readLockFlag();
            m_cpu->setLockFlag();
            RW(instruction.ra, lockFlag);

            DEBUG_LOG(
                QString("RS: Read and set lock flag = %1 -> R%2, flag now set").arg(lockFlag).arg(instruction.ra));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Implementation-Specific Extensions
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_IMPLVER: // 0x6000 - Implementation Version (duplicate check)
        {
            // This might appear in MISC group on some implementations
            result = m_cpu->getImplementationVersion();
            RW(instruction.ra, result);

            DEBUG_LOG(QString("IMPLVER: Implementation version = 0x%1 -> R%2")
                          .arg(result, 16, 16, QChar('0'))
                          .arg(instruction.ra));
            break;
        }

        case FUNC_AMASK: // 0x6100 - Architecture Mask (duplicate check)
        {
            // This might appear in MISC group on some implementations
            quint64 mask = rbValue;
            result = m_cpu->getArchitectureMask(mask);
            RW(instruction.ra, result);

            DEBUG_LOG(QString("AMASK: Architecture mask 0x%1 -> 0x%2 -> R%3")
                          .arg(mask, 16, 16, QChar('0'))
                          .arg(result, 16, 16, QChar('0'))
                          .arg(instruction.ra));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Advanced Cache Operations (EV6+ specific)
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_WH64: // 0xF800 - Write Hint 64 bytes
        {
            quint64 address = rbValue;

            DEBUG_LOG(QString("WH64: Write hint for 64 bytes at address 0x%1").arg(address, 16, 16, QChar('0')));

            // Write hint - prepare cache for upcoming write operations
            if (m_cpu->hasCache() && m_cpu->supportsWriteHints())
            {
                m_cpu->writeHint64(address);
            }

            DEBUG_LOG("WH64: Write hint completed");
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Performance Monitoring (Implementation Specific)
            // ═══════════════════════════════════════════════════════════════════════════

        case FUNC_RDPERF: // 0x9000 - Read Performance Counter (if not in PAL)
        {
            quint64 counterSelect = rbValue & 0x3; // Counter selection
            quint64 perfCount = m_cpu->readPerformanceCounter(counterSelect);
            RW(instruction.ra, perfCount);

            DEBUG_LOG(QString("RDPERF: Read performance counter %1 = %2 -> R%3")
                          .arg(counterSelect)
                          .arg(perfCount)
                          .arg(instruction.ra));
            break;
        }

            // ═══════════════════════════════════════════════════════════════════════════
            // Default Case - Unknown Miscellaneous Function
            // ═══════════════════════════════════════════════════════════════════════════

        default:
        {
            DEBUG_LOG(
                QString("executeMiscGroup: Unknown MISC function 0x%1").arg(instruction.function, 4, 16, QChar('0')));

            // Check if this might be a processor-specific extension
            if (m_cpu->hasProcessorSpecificMisc())
            {
                bool handled = m_cpu->executeProcessorSpecificMisc(instruction);
                if (!handled)
                {
                    m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
                }
            }
            else
            {
                m_cpu->triggerException(ExceptionType::ILLEGAL_INSTRUCTION, m_cpu->getPc());
            }
            return;
        }
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // Statistics and Performance Monitoring
        // ═══════════════════════════════════════════════════════════════════════════

        // Update instruction statistics
        updateMiscInstructionStatistics(instruction.function);

        // Update general statistics
        m_totalInstructions++;

        // Update performance counters if available
        if (m_cpu->hasPerformanceCounters())
        {
            m_cpu->incrementPerformanceCounter(AsaPerformance::MISC_INSTRUCTIONS);

            // Track specific types
            if (isBarrierInstruction(instruction.function))
            {
                m_cpu->incrementPerformanceCounter(AsaPerformance::BARRIER_INSTRUCTIONS);
            }
            else if (isCacheInstruction(instruction.function))
            {
                m_cpu->incrementPerformanceCounter(AsaPerformance::CACHE_INSTRUCTIONS);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Helper Methods for Miscellaneous Instruction Statistics
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Update statistics for miscellaneous instructions
     */
    void updateMiscInstructionStatistics(quint32 function)
    {
        QMutexLocker locker(&m_statsMutex);

        // Add misc instruction counters to the class if not already present
        static quint64 s_barrierInstructions = 0;
        static quint64 s_cacheInstructions = 0;
        static quint64 s_timingInstructions = 0;
        static quint64 s_lockInstructions = 0;

        switch (function)
        {
        case FUNC_TRAPB:
        case FUNC_EXCB:
        case FUNC_MB:
        case FUNC_WMB:
            s_barrierInstructions++;
            break;

        case FUNC_FETCH:
        case FUNC_FETCH_M:
        case FUNC_ECB:
        case FUNC_WH64:
            s_cacheInstructions++;
            break;

        case FUNC_RPCC:
        case FUNC_RDPERF:
            s_timingInstructions++;
            break;

        case FUNC_RC:
        case FUNC_RS:
            s_lockInstructions++;
            break;
        }
    }



    /**
     * @brief Check if function code represents a barrier instruction
     */
    bool isBarrierInstruction(quint32 function) const
    {
        switch (function)
        {
        case FUNC_TRAPB:
        case FUNC_EXCB:
        case FUNC_MB:
        case FUNC_WMB:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief Check if function code represents a cache instruction
     */
    bool isCacheInstruction(quint32 function) const
    {
        switch (function)
        {
        case FUNC_FETCH:
        case FUNC_FETCH_M:
        case FUNC_ECB:
        case FUNC_WH64:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief Print miscellaneous instruction statistics
     */
    void printMiscStatistics() const
    {
        // This would need to be integrated with the existing statistics system
        DEBUG_LOG("Miscellaneous Instruction Statistics:");
        DEBUG_LOG("  Barrier Instructions: [implement counter]");
        DEBUG_LOG("  Cache Instructions: [implement counter]");
        DEBUG_LOG("  Timing Instructions: [implement counter]");
        DEBUG_LOG("  Lock Instructions: [implement counter]");
    }



/*─────────────────────────────  Helper logging macros  ───────────────────────
 * Put these near DebugLog.h or inline above so the compiler inlines them away
 * when DEBUG_BUILD is off.  They keep the flow inside the switch readable.
 */
                                                                                           \
    DEBUG_LOG(QStringLiteral("AND 0x%1 & 0x%2 = 0x%3")                                                                 \
                  .arg(a, 16, 16, QChar('0'))                                                                          \
                  .arg(b, 16, 16, QChar('0'))                                                                          \
                  .arg(r, 16, 16, QChar('0')))
    /* Add LOG_BIC, LOG_BIS, … similarly, or replace with one generic helper. */

    
  signals:
    void sigInstructionExecuted(const DecodedInstruction &instruction);
    void sigExecutionError(QString error);
    void sigPipelineStalled(quint32 cycles);
    void sigRaiseException(ExceptionType exceptType, QString msg);

  public:

 /**
  * @brief Convert Quadword to F_floating format
  * @param instruction The decoded instruction containing function code and registers
  * @param raValue The quadword value to convert
  * @return The converted F_floating value as quint64
  */
 quint64 convertQuadToF(const DecodedInstruction &instruction, quint64 raValue)
 {
     quint64 result = 0;
     bool overflow = false;

     switch (instruction.function)
     {
     case FUNC_CVTQF_C: // CVTQF/C - Convert Quadword to F_floating (Chopped)
     {
         // Convert signed 64-bit integer to VAX F-format (32-bit) with chopped rounding
         qint64 intValue = static_cast<qint64>(raValue);

         // VAX F-format has 24-bit mantissa, so we may lose precision
         if (intValue == 0)
         {
             result = 0; // VAX F-format zero
         }
         else
         {
             // Convert to F-format using chopped (truncate) rounding
             result = m_cpu->convertToVaxF(intValue, RoundingMode::ROUND_CHOPPED);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTQF/C %1 -> 0x%2").arg(intValue).arg(result, 8, 16, QChar('0')));
         break;
     }

     case FUNC_CVTQF: // CVTQF - Convert Quadword to F_floating (Round to Nearest)
     {
         // Convert signed 64-bit integer to VAX F-format with round-to-nearest
         qint64 intValue = static_cast<qint64>(raValue);

         if (intValue == 0)
         {
             result = 0; // VAX F-format zero
         }
         else
         {
             // Convert to F-format using round-to-nearest
             result = m_cpu->convertToVaxF(intValue, RoundingMode::ROUND_NEAREST);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTQF %1 -> 0x%2").arg(intValue).arg(result, 8, 16, QChar('0')));
         break;
     }

     case FUNC_CVTQF_UC: // CVTQF/UC - Convert Quadword to F_floating (Unbiased/Scaled Chopped)
     {
         // Convert with unbiased/scaled chopped rounding
         // This variant handles very large numbers by scaling
         qint64 intValue = static_cast<qint64>(raValue);

         if (intValue == 0)
         {
             result = 0; // VAX F-format zero
         }
         else
         {
             // Use unbiased chopped conversion for better handling of large values
             result = m_cpu->convertToVaxFUnbiased(intValue, RoundingMode::ROUND_CHOPPED);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTQF/UC %1 -> 0x%2").arg(intValue).arg(result, 8, 16, QChar('0')));
         break;
     }
     }

     return result;
 }

 /**
  * @brief Convert Quadword to G_floating format
  * @param instruction The decoded instruction containing function code and registers
  * @param raValue The quadword value to convert
  * @return The converted G_floating value as quint64
  */
 quint64 convertQuadToG(const DecodedInstruction &instruction, quint64 raValue)
 {
     quint64 result = 0;

     switch (instruction.function)
     {
     case FUNC_CVTQG_C: // CVTQG/C - Convert Quadword to G_floating (Chopped)
     {
         // Convert signed 64-bit integer to VAX G-format (64-bit) with chopped rounding
         qint64 intValue = static_cast<qint64>(raValue);

         if (intValue == 0)
         {
             result = 0; // VAX G-format zero
         }
         else
         {
             // Convert to G-format using chopped (truncate) rounding
             result = m_cpu->convertToVaxG(intValue, RoundingMode::ROUND_CHOPPED);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTQG/C %1 -> 0x%2").arg(intValue).arg(result, 16, 16, QChar('0')));
         break;
     }

     case FUNC_CVTQG: // CVTQG - Convert Quadword to G_floating (Round to Nearest)
     {
         // Convert signed 64-bit integer to VAX G-format with round-to-nearest
         qint64 intValue = static_cast<qint64>(raValue);

         if (intValue == 0)
         {
             result = 0; // VAX G-format zero
         }
         else
         {
             // Convert to G-format using round-to-nearest
             result = m_cpu->convertToVaxG(intValue, RoundingMode::ROUND_NEAREST);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTQG %1 -> 0x%2").arg(intValue).arg(result, 16, 16, QChar('0')));
         break;
     }

     case FUNC_CVTQG_UC: // CVTQG/UC - Convert Quadword to G_floating (Unbiased/Scaled Chopped)
     {
         // Convert with unbiased/scaled chopped rounding
         qint64 intValue = static_cast<qint64>(raValue);

         if (intValue == 0)
         {
             result = 0; // VAX G-format zero
         }
         else
         {
             // Use unbiased chopped conversion for better handling of large values
             result = m_cpu->convertToVaxGUnbiased(intValue, RoundingMode::ROUND_CHOPPED);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTQG/UC %1 -> 0x%2").arg(intValue).arg(result, 16, 16, QChar('0')));
         break;
     }
     }

     return result;
 }

 /**
  * @brief Convert G_floating to F_floating format
  * @param instruction The decoded instruction containing function code and registers
  * @param raValue The G_floating value to convert
  * @return The converted F_floating value as quint64
  */
 quint64 convertGToF(const DecodedInstruction &instruction, quint64 raValue)
 {
     quint64 result = 0;

     switch (instruction.function)
     {
     case FUNC_CVTGF_C: // CVTGF/C - Convert G_floating to F_floating (Chopped)
     {
         // Convert VAX G-format (64-bit) to VAX F-format (32-bit) with chopped rounding
         if (raValue == 0)
         {
             result = 0; // VAX F-format zero
         }
         else
         {
             // Extract and validate G-format value, then convert to F-format
             // G-format has more precision than F-format, so we need to truncate
             result = m_cpu->convertVaxGToF(raValue, RoundingMode::ROUND_CHOPPED);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTGF/C 0x%1 -> 0x%2")
                       .arg(raValue, 16, 16, QChar('0'))
                       .arg(result, 8, 16, QChar('0')));
         break;
     }

     case FUNC_CVTGF: // CVTGF - Convert G_floating to F_floating (Round to Nearest)
     {
         // Convert VAX G-format to VAX F-format with round-to-nearest
         if (raValue == 0)
         {
             result = 0; // VAX F-format zero
         }
         else
         {
             // Convert G-format to F-format using round-to-nearest
             result = m_cpu->convertVaxGToF(raValue, RoundingMode::ROUND_NEAREST);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTGF 0x%1 -> 0x%2")
                       .arg(raValue, 16, 16, QChar('0'))
                       .arg(result, 8, 16, QChar('0')));
         break;
     }

     case FUNC_CVTGF_UC: // CVTGF/UC - Convert G_floating to F_floating (Unbiased/Scaled Chopped)
     {
         // Convert with unbiased/scaled chopped rounding
         if (raValue == 0)
         {
             result = 0; // VAX F-format zero
         }
         else
         {
             // Use unbiased chopped conversion for better precision handling
             result = m_cpu->convertVaxGToFUnbiased(raValue, RoundingMode::ROUND_CHOPPED);
         }

         DEBUG_LOG(QString("ExecuteStage: CVTGF/UC 0x%1 -> 0x%2")
                       .arg(raValue, 16, 16, QChar('0'))
                       .arg(result, 8, 16, QChar('0')));
         break;
     }
     }

     return result;
  }
};
#endif // executeStage_h__
