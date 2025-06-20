#pragma once

#include "AlphaProcessorStatus.h"
#include <QMap>
#include <QString>
#include <QtTypes>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMap>
#include <QString>
#include "enumerations/enumExceptionType.h"
#include "../AEU/StackManager.h"
#include "ProgramCounter.h"
#include "helpers/calculateConditionCodes.h"
#include "traps/trapFpType.h"
#include "IExecutionContext.h"
#include "AlphaMemorySystem_refactored.h"
#include "enumerations/enumProcessorStatus.h"
#include "traps/trapExceptionMapper.h"
#include "traps/trapInterruptMapper.h"
#include "../AEC/RegisterBank.h"





static constexpr quint64 FP_COND_LT_BIT = (1ull << 22);
static constexpr quint64 FP_COND_GE_BIT = (1ull << 23);
static constexpr quint64 FP_COND_MASK = FP_COND_LT_BIT | FP_COND_GE_BIT;


// =============================================================================
// ALPHA PROCESSOR CONTEXT IMPLEMENTATION
// =============================================================================

class AlphaProcessorContext 
{
  private:
    mutable QReadWriteLock m_lock;                 // Thread safety
    //QSharedPointer<ProcessorStatus> m_status; // Alpha processor status
    std::unique_ptr<StackManager> m_stackManager;  // High-performance stack management
    quint64 m_programCounter;                      // Current PC (64-bit, 32-bit aligned)
    quint64 m_generalRegisters[32];                // R0-R31 (R31 hardwired to zero)
    quint64 m_floatingRegisters[32];               // F0-F31 (F31 hardwired to zero)
    quint64 m_excbAddress;                         // Exception continuation block address
    quint64 m_implVersion;
    bool m_synchronousTrapsEnabled;                // Synchronous trap enable
    uint64_t m_exceptionSummary;                   // Exception_Summary Register  -  is read or written by your executeMFPR()/executeMTSPR() handlers inside the CPU or FP executor
    uint64_t softwareInterruptReq;                 // backs SPR #0x12 - Assembly::emitMTSPR_SWI
    uint64_t machineCheckSummary;                  // backs SPR_MACHINE_CHECK_SUMMARY - Assembly::emitMFPR_MCES
    AlphaMemorySystem  *m_memorySystem;             // Memory System
     mutable QReadWriteLock m_lock;
    AlphaProcessorStatus *m_status;
    RegisterBank *m_regBank;                         ///< Attached general/fp register bank
    ProgramCounter m_pc;
    uint32_t m_cpuId;                               // needed for SMP memory barrier (load/Store)
    quint64 fpcrWord_;                              // fpcr bit mask
    

  public:

      quint64 psr = 0;
      explicit AlphaProcessorContext(QSharedPointer<AlphaProcessorStatus> status, int maxStackDepth = 1024, uint64_t implVersion = 0  ) {  
          // default if config not yet read) 
          m_status = status.data();

      }
      inline void advancePC(quint64 advance_bytes)
      {
          // read the current PC, add the delta, then re-align via set()
          m_pc.set(m_pc.get() + advance_bytes);
      }
      inline void advancePC()
      {
          m_pc.advance(); // adds 4 and keeps you aligned
      }

      void attachRegisterBank(RegisterBank *regs) { m_regBank = regs; }
      RegisterBank *registerBank() const { return m_regBank; }
      /**
     * @brief Check and handle FP exception conditions after an operation
     * @param regs                Pointer to the register bank
     * @param underflowOccurred   True if underflow detected
     * @param overflowOccurred    True if overflow detected
     * @param inexactOccurred     True if inexact result detected
     */
    inline void checkFPExceptions(RegisterBank *regs, bool underflowOccurred, bool overflowOccurred,
                                  bool inexactOccurred)
    {
        // Underflow
        if (underflowOccurred)
        {
            regs->setUnderflowFlag();
            if (regs->isUnderflowTrapEnabled())
            {
                raiseFPTrap(FPTrapType::FP_UNDERFLOW);
            }
        }
        // Overflow
        if (overflowOccurred)
        {
            regs->setOverflowFlag();
            if (regs->isOverflowTrapEnabled())
            {
                raiseFPTrap(FPTrapType::FP_OVERFLOW);
            }
        }
        // Inexact
        if (inexactOccurred)
        {
            regs->setInexactFlag();
            if (regs->isInexactTrapEnabled())
            {
                raiseFPTrap(FPTrapType::FP_INEXACT);
            }
        }
    }

    bool isIntegerOverflowEnabled() const { return m_status->isFlagSet(enumProcessorStatus::INT_OVERFLOW_ENABLE); }

    AlphaMemorySystem *memSystem() { return m_memorySystem;  }
    void setFPConditionFlags(bool ge, bool lt)
    {
        fpcrWord_ &= ~FPCC_MASK;
        if (ge)
            fpcrWord_ |= FP_COND_GE_BIT;
        if (lt)
            fpcrWord_ |= FP_COND_LT_BIT;
    }

    void getFPConditionFlags(bool &ge, bool &lt) const
    {
        ge = (fpcrWord_ & FP_COND_GE_BIT) != 0;
        lt = (fpcrWord_ & FP_COND_LT_BIT) != 0;
    }
     /**
     * @brief Raise a floating-point trap
     * @param type The FP exception type to raise
     */
    inline void raiseFPTrap(FPTrapType type_)
    {
        // Notify the pipeline/context of the FP trap
        notifyTrapRaised(type_);
    }
    void updateFPConditionCodes(float result)
    {
        // Promote to double so there is a single implementation point
        updateFPConditionCodes(static_cast<double>(result));
    }

    void updateFPConditionCodes(double result)
    {
        // For IEEE754, comparisons with NaN yield false for both >=0 and <0,
        // which encodes the “unordered” state (both flags clear).
        bool ge = (result >= 0.0);
        bool lt = (result < 0.0);

        // Now set the two FP-condition bits in the FPCR (or however your
        // context tracks them).  Your setFPConditionFlags(a,b) should map:
        //   a == true -> non-negative qe 0)
        //   b == true -> negative ( lt 0)
        //
        // Zero gives (ge=true, lt=false).  NaN gives (ge=false, lt=false).
        setFPConditionFlags(ge, lt);
    }
      /// Return the loaded implementation?version identifier
      inline uint64_t getImplementationVersion() const
      {
          QReadLocker lock(&m_lock);
          return m_implVersion;
      }

      inline void setImplementationVersion(uint64_t v)
      {
          QWriteLocker lock(&m_lock);
          m_implVersion = v;
      }
    // Architecture identification
    ProcessorArchitecture getArchitecture() const  { return ProcessorArchitecture::ARCHITECTURE_ALPHA; }

    QString getArchitectureName() const  { return "Alpha AXP"; }

    AlphaProcessorStatus *getProcessorStatus()  { return m_status; }
    const AlphaProcessorStatus *getProcessorStatus() const { return m_status; }

     quint64 getProcessorStatusRaw() const { return m_status->raw(); }
    // Program counter management (Alpha-specific: 64-bit, 32-bit instruction aligned)
    quint64 getProgramCounter() const 
    {
        QReadLocker lock(&m_lock);
        return m_programCounter;
    }


    inline uint32_t cpuId() const { return m_cpuId; }
    inline void setCpuId(uint32_t cpuId) { m_cpuId = cpuId; }

    bool isProcessorFlagSet(enumProcessorStatus flag) const { return m_status->getFlag(flag); }

bool isFlagSet(quint64 bitMask, enumFlagDomain domain) const;

    void setProgramCounter(quint64 pc) 
    {
        QWriteLocker lock(&m_lock);
        m_programCounter = pc & ~0x3ULL; // Force 32-bit alignment
    }


    quint64 getNextInstructionPC() const
    {
        QReadLocker lock(&m_lock);
        return m_programCounter + 4; // Alpha instructions are always 4 bytes
    }
	


    bool isValidPC(quint64 pc) const 
    {
        return (pc & 0x3) == 0; // Must be 32-bit aligned
    }

    quint32 getInstructionSize(quint64 pc) const 
    {
        Q_UNUSED(pc);
        return 4; // Alpha instructions are always 4 bytes
    }

     /**
     * Notify that a synchronous trap of the given type should be raised at the
     * current PC. Raises the trap if synchronous traps are enabled.
     * ASA II-A, Section 6.16: Synchronous traps and arithmetic traps. 
     */
    inline void notifyTrapRaised(AlphaTrapType type)
    {
        QWriteLocker lock(&m_lock);
        if (m_synchronousTrapsEnabled)
        {
            lock.unlock();
            handleTrap(type, getProgramCounter());
        }
    }
    inline void notifyTrapRaised(FPTrapType type)
    {
        QWriteLocker lock(&m_lock);
        if (m_synchronousTrapsEnabled)
        {
            lock.unlock();
            handleTrap(type, getProgramCounter());
        }
    }

    /**
     * Update the condition code flags (Z, N, V, C, T) based on the given
     * operation result and operands. Uses two's-complement overflow detection
     * and unsigned carry semantics. ASA I, Section 4.4: Quadword Add/Subtract
     */
    inline void updateConditionCodes(qint64 result, qint64 op1, qint64 op2, bool isSubtraction = false)
     {
         QWriteLocker lock(&m_lock);
         ProcessorStatusFlags flags = AlphaPS::calculateConditionCodes(result, op1, op2, isSubtraction);
         m_status->setFlag(flags);
     }

    void updateConditionFlags(const ProcessorStatusFlags &flags)
    {
        if (!m_status)
            return;

        // Update each bit in ProcessorStatus from flags
        m_status->setFlag(enumProcessorStatus::PS_FLAG_ZERO, flags.zero);
        m_status->setFlag(enumProcessorStatus::PS_FLAG_NEGATIVE, flags.negative);
        m_status->setFlag(enumProcessorStatus::PS_FLAG_OVERFLOW, flags.overflow);
        m_status->setFlag(enumProcessorStatus::PS_FLAG_CARRY, flags.carry);
        // TraceEnable is managed separately by PAL/trap logic
    }

    /**
     * Retrieve the current condition code flags from the Processor Status register.
     * These include Zero, Negative, Overflow, Carry, and Trace enable. 
     */
    inline ProcessorStatusFlags getConditionFlags() const
    {
        QReadLocker lock(&m_lock);
        return m_status->getFlags();
    }
    // Stack management using existing StackManager
    StackManager *getStackManager()  { return m_stackManager.get(); }
    const StackManager *getStackManager() const  { return m_stackManager.get(); }

    bool pushExceptionFrame(ExceptionType type, quint64 parameter = 0) 
    {
        QWriteLocker lock(&m_lock);

        // Build Alpha-style exception frame using existing infrastructure
        ExceptionFrame frame = FrameHelpers::makeExceptionFrame(
            m_programCounter,                              // pc
            m_status->saveForException(),                  // ps (Alpha PS register)
            static_cast<quint64>(type) | (parameter << 8), // excSum
            m_generalRegisters,                            // gpr
            m_floatingRegisters[32]                        // Use a floating point register for FPCR simulation
        );

        return m_stackManager->pushFrame(frame) >= 0;
    }

    bool popExceptionFrame() 
    {
        QWriteLocker lock(&m_lock);
        return m_stackManager->popFrame();
    }

    std::optional<StackFrame> getCurrentFrame() const 
    {
        QReadLocker lock(&m_lock);
        return m_stackManager->top();
    }

    QVector<StackFrame> getStackSnapshot() const 
    {
        QReadLocker lock(&m_lock);
        return m_stackManager->snapshot();
    }

    quint64 getStackPointer() const 
    {
        QReadLocker lock(&m_lock);
        return m_generalRegisters[30]; // R30 is stack pointer
    }

    void setStackPointer(quint64 sp) 
    {
        QWriteLocker lock(&m_lock);
        m_generalRegisters[30] = sp;
    }

    bool isValidStackAddress(quint64 address) const 
    {
        return (address & 0x7) == 0; // 64-bit aligned for Alpha
    }

    // Exception handling using Alpha PALcode semantics
    void handleException(ExceptionType type, quint64 parameter = 0) 
    {
        QWriteLocker lock(&m_lock);

        // Alpha exception handling via PALcode
        m_status->enterPALMode();

        // Push exception frame using high-performance StackManager
        pushExceptionFrame(type, parameter);

        // Vector to appropriate PAL routine
        quint64 palEntryPoint = getAlphaPALEntryPoint(type);
        setProgramCounter(palEntryPoint);
    }

    void handleTrap(AlphaTrapType type, quint64 faultingPC)
    {
        QWriteLocker lock(&m_lock);
        if (m_synchronousTrapsEnabled)
        {
            lock.unlock();
            ExceptionType exType = exceptionTypeFromAlphaTrap(type);
            handleException(exType, faultingPC);
        }
    }
    void handleTrap(FPTrapType type, quint64 faultingPC)
    {
        QWriteLocker lock(&m_lock);
        if (m_synchronousTrapsEnabled)
        {
            lock.unlock();
            ExceptionType exType = exceptionTypeFromAlphaTrap(type);
            handleException(exType, faultingPC);
        }
    }


 void handleInterrupt(InterruptType type, quint8 level)
    {
        QReadLocker lock(&m_lock);
        if (m_status->canTakeInterrupt(level))
        {
            lock.unlock();
            ExceptionType ex = exceptionTypeFromInterrupt(type);
            handleException(ex, static_cast<quint64>(level));
        }
    }



    void handleMachineCheck(quint64 errorInfo) 
    {
        handleException(ExceptionType::MACHINE_CHECK, errorInfo);
    }

    // Context operations using existing SavedContext infrastructure
    bool saveFullContext() 
    {
        QWriteLocker lock(&m_lock);

        SavedContext *savedCtx = m_stackManager->allocateSavedContextForTop();
        if (!savedCtx)
            return false;

        // Save complete Alpha register state
        memcpy(savedCtx->intRegs, m_generalRegisters, sizeof(m_generalRegisters));
        memcpy(savedCtx->fpRegs, m_floatingRegisters, sizeof(m_floatingRegisters));
        savedCtx->fpcr = 0; // Would be actual FPCR in real implementation
        savedCtx->asn = 0;  // Address Space Number
        savedCtx->ptbr = 0; // Page Table Base Register

        return true;
    }

    bool restoreFullContext() 
    {
        QWriteLocker lock(&m_lock);

        auto currentFrame = m_stackManager->top();
        if (!currentFrame || !currentFrame->savedCtx)
            return false;

        const SavedContext &savedCtx = *currentFrame->savedCtx;

        // Restore complete Alpha register state
        memcpy(m_generalRegisters, savedCtx.intRegs, sizeof(m_generalRegisters));
        memcpy(m_floatingRegisters, savedCtx.fpRegs, sizeof(m_floatingRegisters));

        // Ensure R31 and F31 remain zero (hardwired)
        m_generalRegisters[31] = 0;
        m_floatingRegisters[31] = 0;

        return true;
    }

    bool switchContext(AlphaProcessorContext *newContext) 
    {
        if (!newContext || newContext->getArchitecture() != getArchitecture())
            return false;

        // Save current context
        if (!saveFullContext())
            return false;

        // Alpha context switch via PALcode SWPCTX would happen here
        // This is a simplified implementation

        return true;
    }

    // Alignment handling (Alpha requires strict alignment)
    bool isAligned(quint64 address, quint32 alignment) const  { return (address & (alignment - 1)) == 0; }

    bool isInstructionAligned(quint64 pc) const 
    {
        return (pc & 0x3) == 0; // 32-bit aligned
    }

    void handleAlignmentFault(quint64 faultingAddress) 
    {
        handleException(ExceptionType::UNALIGNED_ACCESS, faultingAddress);
    }

    // EXCB support (Alpha-specific Exception Continuation Block)
    bool hasExceptionContinuation() const 
    {
        QReadLocker lock(&m_lock);
        return m_excbAddress != 0;
    }

    quint64 getExceptionContinuationAddress() const 
    {
        QReadLocker lock(&m_lock);
        return m_excbAddress;
    }

    void setExceptionContinuationAddress(quint64 address) 
    {
        QWriteLocker lock(&m_lock);
        m_excbAddress = address;
    }

    void executeExceptionContinuation() 
    {
        QWriteLocker lock(&m_lock);
        if (hasExceptionContinuation())
        {
            setProgramCounter(m_excbAddress);
            m_excbAddress = 0;
        }
    }

    // Synchronous trap support
    void enableSynchronousTraps(bool enable) 
    {
        QWriteLocker lock(&m_lock);
        m_synchronousTrapsEnabled = enable;
    }

    bool areSynchronousTrapsEnabled() const 
    {
        QReadLocker lock(&m_lock);
        return m_synchronousTrapsEnabled;
    }

        // 0…N-1 for an N-CPU system
   

    void deliverSynchronousTrap(AlphaTrapType type) 
    {
        if (areSynchronousTrapsEnabled())
        {
            handleTrap(type, getProgramCounter());
        }
    }

    // Debug and validation
    QString getContextString() const 
    {
        QReadLocker lock(&m_lock);
        return QString("Alpha Context: PC=%1, SP=%2, RA=%3, Depth=%4, PAL=%5")
            .arg(m_programCounter, 16, 16, QChar('0'))
            .arg(getStackPointer(), 16, 16, QChar('0'))
            .arg(m_generalRegisters[26], 16, 16, QChar('0')) // R26 = RA
            .arg(m_stackManager->depth())
            .arg(m_status->isPALModeActive() ? "Active" : "Inactive");
    }

    QMap<QString, quint64> getContextValues() const 
    {
        QReadLocker lock(&m_lock);
        QMap<QString, quint64> values;

        values["Program_Counter"] = m_programCounter;
        values["Stack_Pointer"] = getStackPointer();
        values["Stack_Depth"] = m_stackManager->depth();
        values["EXCB_Address"] = m_excbAddress;
        values["Sync_Traps_Enabled"] = m_synchronousTrapsEnabled ? 1 : 0;
        values["PAL_Mode_Active"] = m_status->isPALModeActive() ? 1 : 0;

        // Add Alpha registers
        for (int i = 0; i < 32; i++)
        {
            values[QString("R%1").arg(i)] = m_generalRegisters[i];
            values[QString("F%1").arg(i)] = m_floatingRegisters[i];
        }

        return values;
    }

    bool isValidContext() const 
    {
        QReadLocker lock(&m_lock);
        return isValidPC(m_programCounter) && isValidStackAddress(getStackPointer()) && m_status->isValidState();
    }

    // Alpha-specific register access
    quint64 getGeneralRegister(int regNum) const
    {
        QReadLocker lock(&m_lock);
        if (regNum >= 0 && regNum < 32)
            return m_generalRegisters[regNum];
        return 0;
    }

    void setGeneralRegister(int regNum, quint64 value)
    {
        QWriteLocker lock(&m_lock);
        if (regNum >= 0 && regNum < 31) // R31 is hardwired to zero
            m_generalRegisters[regNum] = value;
        // R31 always remains zero
    }

    quint64 getFloatingRegister(int regNum) const
    {
        QReadLocker lock(&m_lock);
        if (regNum >= 0 && regNum < 32)
            return m_floatingRegisters[regNum];
        return 0;
    }

    void setFloatingRegister(int regNum, quint64 value)
    {
        QWriteLocker lock(&m_lock);
        if (regNum >= 0 && regNum < 31) // F31 is hardwired to zero
            m_floatingRegisters[regNum] = value;
        // F31 always remains zero
    }

  private:
    /**
     * @brief Maps software ExceptionType to canonical Alpha PAL entry point.
     * This function bridges software-level exceptions to physical trap vectors.
     * [Ref: Alpha AXP Architecture Vol II, §4.2]
     */
    quint64 getAlphaPALEntryPoint(ExceptionType type) const
    {
        switch (type)
        {
        case ExceptionType::MACHINE_CHECK:
            return static_cast<quint64>(PalEntryPoint::MACHINE_CHECK);
        case ExceptionType::ARITHMETIC_TRAP:
        case ExceptionType::ARITHMETIC:
        case ExceptionType::ARITHMETIC_OVERFLOW:
            return static_cast<quint64>(PalEntryPoint::ARITHMETIC_TRAP);
        case ExceptionType::INTERRUPT_INSTRUCTION:
            return static_cast<quint64>(PalEntryPoint::INTERRUPT);
        case ExceptionType::ALIGNMENT_FAULT:
            return static_cast<quint64>(PalEntryPoint::UNALIGNED_ACCESS);
        case ExceptionType::SYSTEM_CALL:
            return static_cast<quint64>(PalEntryPoint::SYSTEM_CALL);
        default:
            return static_cast<quint64>(PalEntryPoint::GENERIC_EXCEPTION);
        }
    }
};
