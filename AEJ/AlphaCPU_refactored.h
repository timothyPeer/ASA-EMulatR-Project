#pragma once
#include <QObject>
#include <QAtomicInt>
#include <QDateTime>
/*#include <QObject>*/
#include <QScopedPointer>
#include <QSet>
#include <QSharedPointer>
#include <QTimer>

#include "../AEC/RegisterBank.h"
#include "../AEE/FPException.h"
#include "../AEE/MemoryFaultInfo.h"
#include "../AEJ/IprBank.h"
#include "../AEJ/PendingLoad.h"
#include "../AEJ/UnifiedDataCache.h"
#include "../AEJ/constants/constAsaPerformance.h"
//#include "../AEJ/constants/constEXC_SUM.h"
#include "../AEJ/constants/constExceptions.h"
#include "../AEJ/constants/constStackConstants.h"
#include "../AEJ/constants/constStatusRegister.h"
//#include "../AEJ/decodeStage.h"
#include "../AEJ/enumerations/enumCpuState.h"
//#include "../AEJ/enumerations/enumExceptionTypeArithmetic.h"
#include "../AEJ/enumerations/enumFPCompare.h"
#include "../AEJ/enumerations/enumFPFormat.h"
#include "../AEJ/enumerations/enumIPRNumbers.h"
//#include "../AEJ/enumerations/enumIprBank.h"/
#include "../AEJ/enumerations/enumMachineCheckType.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"
#include "../AEJ/enumerations/enumProcessorMode.h"
#include "../AEJ/enumerations/enumRegisterType.h"
#include "../AEJ/enumerations/enumRoundingMode.h"
//#include "../AEJ/structures/enumPalCodes.h"
#include "../AEJ/structures/structSystemEntryPoints.h"
#include "../AEJ/traps/trapFaultTraps.h"
#include "../AEJ/traps/trapFpType.h"
#include "../AEU/StackFrame.h"
#include "../AEU/StackManager.h"
#include "IExecutionContext.h"
#include "SafeMemory_refactored.h"
#include "alphasmpManager_refactored.h"
#
// #include "../AEJ/instructionTLB.h"
#include "../AEJ/enumerations/enumDenormalMode.h"
#include "../AEE/TLBExceptionQ.h"
#include "AlphaInstructionCache.h"
#include "MMIOManager.h"
#include "constants/const_EXC_SUM.h"
#include "enumInstructionPerformance.h"
#include "decodedInstruction.h"
#include "structures/structPALInstruction.h"
#include "enumerations/enumSecurityViolationType.h"
#include "AlphaProcessorContext.h"
#include "AlphaProcessorStatus.h"
#include "../ABA/helpers/IExecutor.h"
#include "../ABA/structs/operateInstruction.h"
#include "../ABA/executors/IntegerInterpreterExecutor.h"
#include <AlphaProcessorStatus.cpp>
#include "../ABA/memoryInterpreterExecutor.h"
#include "../ABA/IntegerInterpreterExecutor.h"
#include "../ABA/branchInterpreterExecutor.h"
#include "../ABA/structs/branchInstruction.h"
#include "../ABA/executors/IntegerJitExecutor.h"


// Key changes needed in AlphaCPU.h for SMP support

// 1. Update constructor to accept CPU ID
class AlphaCPU : public QObject {
    Q_OBJECT


      struct CPUTopology
  {
      quint16 cpuId;
      quint16 coreId;
      quint16 packageId;
      quint16 threadId;
      bool isHyperthreaded;
      QVector<quint16> siblingCpus;
  };

  public:

    ~AlphaCPU();
    AlphaMemorySystem *getMemorySystem() { return m_memorySystem; }
    bool atomicCompareAndSwap(quint64 address, quint64 expectedValue, quint64 newValue, int size);
    bool checkLockFlag();
    bool hasPerformanceCounters();
    bool isFloatingPointEnabled();
    bool isKernelMode() const;
    bool canTakeInterrupt(int vector) const;
    bool readMemory(quint64 address, quint8 *buffer, size_t size);
    bool readPhysicalMemory(quint64 physAddr, quint64 &value);
    CpuModel getCpuModel();
    CPUTopology getCPUTopology() const;
    explicit AlphaCPU(quint16 cpuID, AlphaMemorySystem *memorySystem, QObject *parent = nullptr);
    MMIOManager *getMMU() { return m_mmIoManager; }
    quint16 getCpuId() const;
    quint64 addDFormat(quint64 faVal, quint64 rbVal);
    quint64 addFFormat(quint64 faVal, quint64 rbVal);
    quint64 addGFormat(quint64 faVal, quint64 rbVal);
    quint64 addSFormat(quint64 faVal, quint64 rbVal);
    quint64 addTFormat(quint64 faVal, quint64 rbVal);
    quint64 applySqrtVariant(quint64 val_);
    void attachMemorySystem(AlphaMemorySystem *memorySystem);
    void attachMMIOManager(MMIOManager *mmio) { m_mmIoManager = mmio; }
    void attachIRQController(IRQController *irq) { m_irqController = irq; }
    quint64 atomicFetchAndAdd(quint64 address, quint64 addValue, int size);
    void clearException();
    quint64 compareFFormat(quint64 faVal, quint64 rbVal, FPCompareType typ_);
    quint64 compareGFormat(quint64 faVal, quint64 rbVal);
    quint64 compareTFormat(quint64 faValue, quint64 fbValue, FPCompareType c_Type);
    quint64 compareTFormatSignaling(quint64 faValue, quint64 fbValue, FPCompareType c_Type);
    void connectToL3SharedCache(UnifiedDataCache *l3Cache);
    ExceptionType convertArithmeticException(ExceptionTypeArithmetic type);
    quint64 convertDToG(quint64 val_);
    quint64 convertFromVaxD(quint64 val_);
    quint64 convertFromVaxG(quint64 val_);
    quint64 convertFToOther(quint64 val_);
    quint64 convertGToD(quint64 val_);
    quint64 convertGToQuad(quint64 val_);
    quint64 convertQuadToF(const DecodedInstruction &instruction, quint64 raValue);
    quint64 convertQuadToG(const DecodedInstruction& instruction, quint64 raValue);
    quint64 convertQuadToS(quint64 faVal, quint64 rbVal);
    quint64 convertQuadToSChopped(quint64 faVal, quint64 rbVal);
    quint64 convertQuadToT(quint64 faVal, quint64 rbVal);
    quint64 convertQuadToTChopped(quint64 faVal, quint64 rbVal);
    quint64 convertSToT(quint64 faVal, quint64 rbVal);
    quint64 convertToIeeeT(quint64 val_);
    quint64 convertToVaxD(quint64 val_);
    quint64 convertToVaxF(quint64 raValue, RoundingMode rm_);
    quint64 convertToVaxFUnbiased(quint64 raValue, RoundingMode rm_);
    quint64 convertToVaxG(quint64 raValue, RoundingMode rm_);
    quint64 convertToVaxGUnbiased(quint64 raValue, RoundingMode rm_);
    quint64 convertTToQuad(quint64 faVal, quint64 rbVal);
    quint64 convertTToS(quint64 faVal, quint64 rbVal);
    quint64 convertVaxGToF(quint64 raValue, RoundingMode rm_);
    quint64 divDFormat(quint64 faVal, quint64 rbVal);
    quint64 divFFormat(quint64 faVal, quint64 rbVal);
    quint64 divGFormat(quint64 faVal, quint64 rbVal);
    quint64 divSFormat(quint64 faVal, quint64 rbVal);
    quint64 divTFormat(quint64 faVal, quint64 rbVal);
    void ensureComponentsInitialized();

    QString formatExceptionInfo() const;
    quint64 getExceptionSummary() const;
    void handleTLBMiss(quint64 virtualAddr, bool isWrite, bool isInstruction);
    void handleTLBMiss(quint16 cpuId, quint64 virtualAddr, quint64 asn, bool isWrite, bool isInstruction);
    void handleTLBInvalidation(quint64 virtualAddr, quint64 asn);
    quint64 getCurrentASN() const { 
     return m_iprs ? m_iprs->read(IPRNumbers::IPR_ASN) : 0; 
    }
    quint64 getCurrentContext() const;
    UnifiedDataCache *getLevel1DataCache() const { return m_level1DataCache ? m_level1DataCache.data() : nullptr; }
    UnifiedDataCache *getLevel2DataCache() const { return m_level2DataCache ? m_level2DataCache.data() : nullptr; }

    quint64 getFloatingPointNaN();
    quint64 getFloatRegister(quint64 reg_);
    quint64 getFloatRegister32(quint64 reg_);
    quint64 getFloatRegister64(quint64 reg_);
    quint64 getIntegerRegister(quint8 regNum) const;
    AlphaInstructionCache *getInstructionCache() const;
    quint64 getPALBase() const;
    quint64 getPC() const;
    quint64 getPerformanceCounter(int counterNum) const;
    bool handleMemoryFault(quint64 faultingAddress, bool isWrite, const PALInstruction &instr);
    bool hasAccessViolation() const;
    bool hasAlignmentFault() const;
    bool hasException() const;
    bool hasFaultOnRead() const;
    bool hasTranslationFault() const;
    quint64 implVersion();
    void initializeCpu();
    void initializeCacheHierarchy();
    void initializeSMP();
    quint64 mulDFormat(quint64 faVal, quint64 rbVal);
    quint64 mulFFormat(quint64 faVal, quint64 rbVal);
    quint64 mulGFormat(quint64 faVal, quint64 rbVal);
    quint64 mulSFormat(quint64 faVal, quint64 rbVal);
    quint64 mulTFormat(quint64 faVal, quint64 rbVal);
    void onReservationCleared(quint16 cpuId, quint64 physicalAddr, int size);
    quint64 readDetailedPerformanceCounter(quint64 counterNum) const;
    quint64 readIPR(QString &iprName);
    quint64 getExceptionVector(int vectorNumber);
    quint64 readIPRByName(const QString &name) const;
    void raiseMemoryException(quint64 faultingAddress, bool isWrite, bool isTranslationFault, bool isAlignmentFault);
    bool readMemory64(quint64 vaddr, quint64 &val, quint64 pc);
    bool readMemory64Locked(quint64 vaddr, quint64 &val, quint64 pc);
    bool readMemoryWithFaultHandling(quint64 address, quint64 &value, const PALInstruction &instr);
    quint64 readPerformanceCounter(quint64 counterNum) const;
    quint64 readVirtualMemory(quint64 add_, quint64 val_);
    quint64 readVirtualMemoryDTB(quint64 add_, quint64 val_);
    quint64 readVirtualMemoryITB(quint64 add_, quint64 val_);
    quint64 readWHAMI() const;

    quint64 scaleIeeeSResult(quint64 addr_);
    quint64 scaleIeeeTResult(quint64 addr_);
    quint64 scaleVaxFResult(quint64 addr_);
    quint64 scaleVaxGResult(quint64 addr_);
    void setLevel3SharedCache(UnifiedDataCache *l3Cache)
    {
        if (m_level1DataCache)
        {
            m_level1DataCache->setNextLevel(l3Cache);
        }
        if (m_level2DataCache)
        {
            m_level2DataCache->setNextLevel(l3Cache);
        }
        DEBUG_LOG("AlphaCPU: Set L3 shared cache for CPU%1", m_cpuId);
    }
    void setImplementationVariant(quint64 var_) { m_implementationVersion = var_;  }
    void setReservation(quint64 reservation) { m_cpuReservationAddress = reservation; }

    quint64 subDFormat(quint64 faVal, quint64 rbVal);
    quint64 subFFormat(quint64 faVal, quint64 rbVal);
    quint64 subGFormat(quint64 faVal, quint64 rbVal);
    quint64 subSFormat(quint64 faVal, quint64 rbVal);
    quint64 subTFormat(quint64 faVal, quint64 rbVal);
    quint64 swppalSMP(quint64 newPalBase, bool coordinated = true);
    void applyUnbiasedRounding(quint64 aur_);
    void clearLockFlag();
    void clearReservation() { 
        //TODO
        m_cpuReservationAddress = 0; 
        m_bIsReservationValid = false;
    }
    void disableInterrupts();
    void drainAborts();
    void drainaSMP();
    bool translateAddress(quint16 cpuId, quint64 virtualAddr, quint64 &physicalAddr, quint64 asn, bool isWrite,
                          bool isInstruction);
    bool translateVirtualAddress(quint64 virtualAddr, quint64 &physicalAddr, bool isWrite, bool isInstruction);
    void enableInterrupts();
    void enterPALMode(quint32 function);
    void executeCALL_PAL(quint32 function);
    void executeLDQ_L(quint8 ra, qint16 displacement, quint8 rb);
    void executeMemoryBarrier(int type);
    void executeMB();

    void executeSTQ_C(quint8 ra, qint16 displacement, quint8 rb);
    void executeWMB();
    void dispatchException(ExceptionType exceptionType, quint64 faultAddr);
    void flushPipeline();
    void flushTLBAndNotify(int scope, quint64 virtualAddr = 0);
    void flushTLBCache();
    // Cache statistics access
    QString getCacheStatistics() const;
    double getL1HitRate() const;
    double getL2HitRate() const;

    quint64 getPS() const;
    quint64 getReservationAddress() { return m_cpuReservationAddress;  }
    void halt();
    void handleCacheCoherencyEvent(quint64 physicalAddr, const QString &eventType);
    void invalidateTLBByASN(quint64 asn, quint16 sourceCpuId);
    void invalidateTLBEntry(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);
    void invalidateTlbSingleData(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);
    void invalidateTlbSingleInstruction(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);
    bool isReservationValid() { return m_bIsReservationValid; }
    void handleIPI(int vector, quint16 sourceCpuId);
    void handleMemoryFault(quint64 address, bool isWrite);
    void handleSMPException(ExceptionType exceptionType, quint64 faultAddr, bool needsCoordination = false);
    void hasBranchPredictor();
    bool fetchInstructionWithCache(quint64 pc, quint32 &instruction);
    void incrementPC();
    void incrementPerformanceCounter(enumInstructionPerformance cnt_r);
    void initializeRegisters();
    void invalidateReservation(quint64 physicalAddr, int size);
    void invalidateTBAllProcess();

	void invalidateAllCaches();
    void invalidateAllTLB(quint16 sourceCpuId);
    void invalidateTlbSingle(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId);
    bool isMMUEnabled() const;
    bool isValidMemoryAddress(quint64 address) const;
    void loadVersionFromConfig() { m_processorContext.setImplementationVersion(m_implementationVersion);    }
 
    void loadContext(quint64 contextId);
    void logSecurityViolation(SecurityViolationType svType, quint64 rawInstruction);
    void memoryBarrierSMP(int type);
    void handlePALBaseChange(quint64 newPALBase);
    void notifySystemStateChange();
    void raiseException(ExceptionType exceptionCode, quint64 faultingPC);
    void saveContext(quint64 contextId);
 
    void sendIPI(quint16 targetCpuId, int vector);
    void sendIPIBroadcast(int vector);
    void flushAllCaches();
    void flushCPUTLBCache(quint16 cpuId);
    void setCPUID(quint16 cpuId) { m_cpuId = cpuId; }
    void setCurrentASN(quint64 currentASN) {m_currentASN = currentASN; }
    void setFloatRegister(quint8 regnum, quint64 unintValue);
    void setIntegerRegister(quint8 regNum, quint64 value);
    void setMMUEnabled( bool bEnabled) { m_bMMUEnabled = bEnabled; }
    void setKernelMode(bool bIsKernelMode) {m_kernelMode = bIsKernelMode; }
    void setPC(quint64 newPC);
    void setPerformanceCounter(int cntr_, quint64 val_);
    void checkSoftwareInterrupts();
    void updateCPUContext(quint16 cpuId, quint64 newASN);
    void updateInterruptPriority();
    void updateProcessorStatus(quint64 newPS);
    void handleFloatingPointException(FPTrapType type);
    void handleInterruptPriorityChange(quint64 newIPL);
    void maskInterrupt(int level);
    void unmaskInterrupt(int level);
    bool isInterruptMasked(int level) const;
    void checkPendingInterrupts();
    void setInterruptPriorityLevel(quint64 newIPL);
    quint64 getInterruptPriorityLevel() const;
    void setPrivilegeMode(int mode);
    void setRegister(quint8 regnum, quint64 unintValue);
    void triggerException(ExceptionType eType, quint64 targetPC);
    void triggerFloatingPointException(FPTrapType fpTrap);
    void triggerSoftwareInterrupt(quint64 vector);
    void debugIPRMappings() const;
    void deliverInterrupt(int vector);
    void deliverPendingInterrupt();
    void updateSMPPerformanceCounters(int eventType, quint64 count = 1);

    bool writeConfigSpace(quint64 addr_, quint64 val_);
    bool writeIOSpace(quint64 addr_, quint64 val_);
    bool writeIPR(const QString &iprName, quint64 value);
    bool writeIPRByName(const QString &name, quint64 value);
    bool writeMemory32(quint64 vaddr, quint32 value, quint64 pc);
    bool writeMemory32Conditional(quint64 vaddr, quint32 value, quint64 pc);
    bool writeMemory64(quint64 vaddr, quint64 value, quint64 pc);
    bool writeMemory64Conditional(quint64 vaddr, quint64 value, quint64 pc);
    bool writeMemoryWithFaultHandling(quint64 address, quint64 value, const PALInstruction &instr);
    bool writeMemoryConditional(quint64 addr_, quint64 val_);
    bool writeMemoryWriteBack(quint64 addr_, quint64 val_);
    bool writeMemoryWriteThrough(quint64 addr_, quint64 val_);
    void writeMemoryWriteCoherent(quint64 addr_, quint64 val_);
    void writePerformanceCounter(quint64 counterNum, quint64 value);
    bool writePhysicalMemory(quint64 physAddr, quint64 value);
    bool writeVirtualMemory(quint64 addr_, quint64 val_);
    void writeVirtualMemoryDTB(quint64 add_, quint64 val_);
    void writeVirtualMemoryITB(quint64 add_, quint64 val_);
    void vectorToExceptionHandler(quint64 exceptionCode, quint64 faultingPC);
    
quint64 getProcessCycleCounter();
    quint64 readAndSetUniqueValue();
  signals: // Add new signals for SMP coordination

    // Existing signals (updated for SMP)
    void sigCacheCoherencyEvent(quint64 physicalAddress, int cpuId, const QString &eventType);
    void sigCacheCoherencyHandled(quint64 physicalAddr, quint16 cpuId, const QString &eventType);
    void sigCpuHalted(int cpuId);
    void sigCpuStateChanged(CPUState newState);
    void sigCPUStateChanged(quint16 cpuId, int newState);
    void sigCpuStatusUpdate(quint8 cpuid);
    void sigCycleExecuted(quint64 cycle);
    void sigDeliverPendingInterrupt();
    void sigExecutionError(const QString &errorMessage);
    void sigExecutionPaused(quint16 cpuId);
    void sigExecutionStarted(quint16 cpuId);
    void sigExecutionStopped(quint16 cpuId);
    void sigFpcrChanged(quint64 changedFprc);
    void sigHandleReset();
    void sigIllegalInstruction(quint64 pc, quint64 opcode);
    void sigIPIReceived(quint16 sourceCpuId, quint16 targetCpuId, int vector);
    void sigIPISent(quint16 sourceCpuId, quint16 targetCpuId, int vector);
    void sigMappingsCleared();
    void sigMemoryAccessed(quint64 address, quint64 value, bool isWrite);
    void sigMemoryBarrierExecuted(quint16 cpuId, int type);
    void sigOperationCompleted();
    void sigOperationStatus(const QString &message);
    void sigPerformanceCounterOverflow(quint16 cpuId, int counterId);
    void sigProcessingProgress(int percentComplete);
    void sigRegisterUpdated(int regNum, RegisterType type, quint64 value);
    void sigReservationInvalidated(quint16 cpuId, quint64 physicalAddr);
    void sigStateChanged();
    void sigTLBInvalidated(quint16 cpuId, quint64 virtualAddr);
    void sigTranslationMiss(quint64 virtualAddress);
    void sigTrapOccurred(Fault_TrapType type, quint64 pc, int cpuId);
    void sigTrapRaised(Fault_TrapType trap);
    void sigUserStackPointerChanged(quint64 newSP);


  public slots:     // Add slots for SMP coordination

    void onCacheCoherencyEvent(quint64 physicalAddr, quint16 sourceCpuId, const QString &eventType);
    void onMemoryWriteNotification(quint64 physicalAddr, int size, quint16 sourceCpuId);
    void onMemoryWriteNotification(quint64 physicalAddr, quint64 value, bool isWrite);
    void onNotifyMemoryAccessed(quint64 physicalAddr, quint64 value, bool isWrite);
    void onRaiseTrap(TrapType IExecutionContext_TrapType)
    {

        //TODO

    }
 
  private:
         

    AlphaMemorySystem *m_memorySystem; // Reference to shared memory system
    bool m_hasException = false;
    bool m_inExceptionHandler = false;
    bool m_interruptEnable = true;
    bool m_isRunning = true;
    bool m_reservationValid = false;
    // CreateTestCPU for private Methods
    bool m_bMMUEnabled = true;
    bool m_kernelMode = false;
    bool m_bIsReservationValid = false;


     /// Select the C++ interpreter for integer operations
    void useInterpreterExecutor() { m_currentIntegerExecutor = m_interpreterExecutor.get(); }

    /// Select the JIT executor for integer operations
    void useJITExecutor() { m_currentIntegerExecutor = m_jitExecutor.get(); }
      void initExecutors()
    {
          // Integer executors
        m_integerExecutor = std::make_unique<Alpha::IntegerInterpreterExecutor>(&m_registerBank, &m_processorContext);
        m_jitExecutor = std::make_unique<IntegerJITExecutor>(&m_assembler, &m_registerBank, &m_processorContext);
        m_currentIntegerExecutor = m_interpreterExecutor.get();
         // Memory executor
        m_memoryExecutor = std::make_unique<Alpha::MemoryInterpreterExecutor>(&m_registerBank, &m_processorContext);
        m_branchExecutor = std::make_unique<Alpha::BranchInterpreterExecutor>(&m_registerBank, &m_processorContext);
    }

void run()
    {
        while (running_)
        {
            uint32_t raw = fetchInstruction(pc_);
            uint8_t primary = raw >> 26;

            if (primary == 0x10 || primary == 0x11 || primary == 0x13 || primary == 0x1C)
            {
                // integer?operate group
                OperateInstruction op;
                op.decode(raw);
                m_integerExecutor->execute(op);
            }
            else if (primary >= 0x14 && primary <= 0x1B)
            {
                // memory?reference group
                MemoryInstruction m;
                m.raw = raw;
                m.decode();
                m_memoryExecutor->execute(m);
            }
            else if (primary == 0x30 || primary == 0x34 )
            {
                BranchInstruction br;
                br.decode(raw);
                m_branchExecutor->execute(br);
            }
            else
            {
                // other groups: branches, FP, etc.
                handleOther(primary, raw);
            }

            pc_ += 4;
        }
    }

    enumProcessorMode m_currentMode = enumProcessorMode::USER;
    IprBank *m_iprs; 
    MMIOManager *m_mmIoManager;
    IRQController *m_irqController;
    QScopedPointer<AlphaProcessorContext> * m_processorContext;
    AlphaProcessorStatus m_processorStatus; 
    RegisterBank *m_registerBank;
 
    QAtomicInt interruptPending{0};
    QAtomicInt m_coherencyEvents;  // Count of cache coherency events handled
    QAtomicInt m_ipiCount;         // Count of IPIs sent/received
    QScopedPointer<AlphaInstructionCache> m_instructionCache;
    QScopedPointer<UnifiedDataCache> m_level1DataCache;
    QScopedPointer<UnifiedDataCache> m_level2DataCache;
    QSet<int> pendingInterrupts;
    QSet<quint16> m_connectedCpus; // Set of CPUs we can communicate with
    quint16 m_cpuId; 
    quint64 m_cpuModel; 
    quint64 m_currentASN;
    quint64 m_currentPC = 0;
    quint64 m_entryPoints[8] = {0};         // Exception entry points
    quint64 m_ipisReceived = 0;
    quint64 m_ipisSent = 0;
    quint64 m_palCodeBase; 
    quint64 m_cpuReservationAddress;
    //quint64 m_pc;
    quint64 m_implementationVersion;
    QAtomicInt m_performanceCounters[8] = {0}; // 8 performance counters
    quint64 m_processorStatus = 0;
    quint64 m_reservationAddr = 0;
    quint64 m_reservationInvalidations = 0;
    quint64 m_savedPC;
    quint64 m_tlbInvalidationsReceived = 0;
    RegisterBank *m_registers = nullptr;    // Integer register bank
    UnifiedDataCache *m_level3DataCache; // SMP Cache
    std::unique_ptr<Alpha::IntegerInterpreterExecutor> m_integerExecutor;     ///< C++ interpreter
    std::unique_ptr<IntegerJITExecutor> m_jitExecutor;                        ///< JIT emitter
    std::unique_ptr<Alpha::MemoryInterpreterExecutor> m_memoryExecutor;       ///< C++ memory interpreter BranchInterpreterExecutor
    std::unique_ptr<Alpha::BranchInterpreterExecutor> m_branchExecutor;
    IExecutor *m_currentIntegerExecutor = nullptr;
    QScopedPointer<Assembler> m_assembler;

public:
    quint64 readAndClearUniqueValue();
 
};



