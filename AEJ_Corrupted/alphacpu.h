// AlphaCPU.h - Main CPU class header
#ifndef ALPHACPU_H
#define ALPHACPU_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QByteArray>
#include <QMutexLocker>

class AlphaCPUInterface;

#include "Helpers.h"
//#include "UnifiedExecutors.h"
#include "StackFrame.h"
#include "alphamemorysystem.h"
#include "vectorExecutor.h"
#include "IExecutionContext.h"
#include "ExecutorOpcodeEnumeration.h"
#include "decodeOperate.h"
#include "IntegerExecutor.h"
#include "floatingpointexecutor.h"
#include "ControlExecutor.h"
#include "SafeMemory.h"

/// <summary>
/// Enumerator which informs AlphaCPU which executor to use for an instruction
/// 
/// Memory Access: 
///     -- All memory operations (including instruction fetch) should go through to ensure Protection bits are enforced, Instruction fetches don't hit/bypass MMIO and faults are trapped properly: 
///         memorySystem->readVirtualMemory(...)
///         memorySystem->writeVirtualMemory(...)
/// 
/// </summary>
enum class ExecutorType {
	Integer,
	FloatingPoint,
	Control,
	Vector,
	Unknown
};




// Forward declarations
class AlphaMemorySystem;
class AlphaJITCompiler;
class AlphaPALInterpreter;
/**
 * @brief AlphaCPU - Represents a single Alpha CPU in the system
 *
 * This class encapsulates all functionality for an Alpha CPU including
 * register state, execution control, and exception handling.
 * Each CPU runs in its own QThread.
 */
//class AlphaCPU : public QObject, public AlphaCPUInterface

class AlphaCPU : public QObject, public IExecutionContext
{
    Q_OBJECT

public:
 
    // Constructor/Destructor
    explicit AlphaCPU(int cpuId, AlphaMemorySystem* memSystem, QObject* parent = nullptr);


    // IExecutionContext Overrides

	// Register access  
	uint64_t readIntReg(unsigned idx) override {
		return m_intRegisters.value(idx, 0);
	}
	void writeIntReg(unsigned idx, uint64_t v) override {
		// update storage  
		if (idx < m_intRegisters.size()) m_intRegisters[idx] = v;
		emit registerChanged(idx, helpers_JIT::RegisterType::INTEGER_REG, v);
	}

	double readFpReg(unsigned idx) override {
		return m_fpRegisters.value(idx, 0.0);
	}
	void writeFpReg(unsigned idx, double f) override {
		if (idx < m_fpRegisters.size()) m_fpRegisters[idx] = f;
		// bit-cast to raw for notification  
		uint64_t raw;
		static_assert(sizeof(raw) == sizeof(f), "");
		std::memcpy(&raw, &f, sizeof(raw));
		emit registerChanged(idx, helpers_JIT::RegisterType::FLOATING_REG, raw);
	}

	// Memory  
	bool readMemory(uint64_t addr, void* buf, size_t sz) override {
		return m_memorySystem->readVirtualMemory(this, addr, buf, sz);
	}
	bool writeMemory(uint64_t addr, void* buf, size_t sz) override {
		return m_memorySystem->writeVirtualMemory(this, addr, buf, sz);
	}

	// Control/status  
	void raiseTrap(int trapCode) override {
		// map your int to TrapType, then…  
		dispatchException(static_cast<helpers_JIT::ExceptionType>(trapCode), m_pc);
	}

	// Events  
	void notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t raw) override;



  
    ~AlphaCPU();

    // Initialization and configuration
    void initialize();
    void initializeSystem();
    int cpuId() const { return m_cpuId; }

    /*
	This is determined by the low 2 bits of PSR, which define the current MMU privilege mode.
    Bits	Mode
    00	    Kernel
    01  	Executive
    10  	Supervisor
    11  	User
    */

    helpers_JIT::MmuMode currentMode() const;
   // helpers_JIT::CPUState state() const { return m_state; }
    quint64 getProgramCounter() const { return m_pc; }
    int getCurrentIPL() const { return m_currentIPL; }

    // Register access
    quint64 getRegister(int regNum, helpers_JIT::RegisterType type = helpers_JIT::RegisterType::INTEGER_REG) const;
    void setRegister(int regNum, quint64 value, helpers_JIT::RegisterType type = helpers_JIT::RegisterType::INTEGER_REG);

    // Check if m_memorySystem is initialized otherwise return nullptr
	SafeMemory* getSafeMemory() const { return m_memorySystem ? m_memorySystem->getSafeMemory() : nullptr; }

	helpers_JIT::CPUState getState() const
	{
		return m_state;
	}

	void setRunning(bool running);                  // AlphaPALInterpreter
	void setState(helpers_JIT::CPUState state);     // AlphaPALInterpreter

    // Instruction fetch/decode
    quint32 fetchInstruction(quint64 address);

	// Stack Pointer and GP Accessors
	inline void setKernelSP(quint64 sp) { m_kernelSP = sp; }
	inline quint64 getKernelSP() const { return m_kernelSP; }

	inline void setUserSP(quint64 sp) { m_userSP = sp; }
	inline quint64 getUserSP() const { return m_userSP; }

	inline void setKernelGP(quint64 gp) { m_kernelGP = gp; }
	inline quint64 getKernelGP() const { return m_kernelGP; }
    //
	void haltExecution();

	bool translate(quint64 virtualAddr, quint64& physicalAddr, int accessType)
	{
		if (!m_memorySystem) {
			qWarning() << QString("[AlphaCPU%1] Translation failed: No memory system available.")
				.arg(m_cpuId);
			return false;
		}

		bool success = m_memorySystem->translate(this, virtualAddr, physicalAddr, accessType);

		if (!success) {
			// Trap logging: PC, VA, access type
			qWarning() << QString("[AlphaCPU%1] Address translation fault at PC=0x%2, VA=0x%3, type=%4")
				.arg(m_cpuId)
				.arg(m_pc, 8, 16, QChar('0'))
				.arg(virtualAddr, 8, 16, QChar('0'))
				.arg(accessType);
		}

		return success;
	}




    //  
    quint64 getPC() const { return m_pc; }

    /*
    * The JIT Compiler should be passed into the class
    * - EmulatorManager
    * --- AlphaSMPManager
    * ---- AlphaCPU
    */ 

    void setJITCompiler(AlphaJITCompiler* compiler) { m_jitCompiler = compiler; }
    void setJitThreshold(quint32 threshold_) { m_jitThreshold = threshold_; }
    void setOptimizationLevels(int level_) { jitOptimizationLevel = level_; }
    void setJitEnabled(bool bEnabled) { m_jitEnabled = bEnabled; }
    
    bool isKernelMode() const  { return m_kernelMode; }

	void writeRegister(int regNum, quint64 value) 
	{
		if (regNum >= 0 && regNum < 32)
            m_intRegisters[regNum] = value;
	}

	quint64 readRegister(int regNum) const 
	{
		if (regNum >= 0 && regNum < 32)
			return m_intRegisters[regNum];
		return 0;
	}

    void pushFrame(const StackFrame& frame);
    void raiseException(helpers_JIT::ExceptionType type, quint64 faultAddr);


    bool isMMUEnabled();


	void notifyRegisterUpdated(bool isFp, quint8 reg, quint64 value);
	void notifyMemoryAccessed(quint64 addr, quint64 value, bool isWrite);
	void notifyTrapRaised(helpers_JIT::TrapType type);
	void buildDispatchTable();

	void notifyFpRegisterUpdated(unsigned idx, double value) override;
    void notifyIllegalInstruction(quint64 instructionWord, quint64 pc);
public slots:
    // Execution control
    void startExecution();
    void pauseExecution();
    void resumeExecution();
    void stopExecution();
    void requestStop();         // Flag that tells the CPU execution loop to exit cleanly
    void setPC(quint64 pc);
    void setMode(helpers_JIT::MmuMode mode_);
    void setIPL(quint8 ipl_); 
    void setMMUEnabled(bool bMMUnEnabled = true) {
        m_bMMUEnabled = bMMUnEnabled;
		if (bMMUnEnabled)
			m_psr |= 0x8;  // bit 3 = MMU enable
		else
			m_psr &= ~0x8;
    }
    void setFPEnabled(bool bFpEnabled = false) {

    }

    void setJitProperties(bool bjitEnabled, int jitThreshold_) {
        m_jitEnabled = bjitEnabled;
        m_jitThreshold = jitThreshold_;
    }
    // Interrupt handling
    void handleInterrupt(int interruptVector);
    void handleIPLChange(int newIPL);

    // JIT compilation related
    void notifyBlockCompiled(quint64 startAddr, const QByteArray& nativeCode);
    void invalidateCompiledBlock(quint64 startAddr);

    // Memory system notifications
    void handleMemoryProtectionFault(quint64 address, int accessType);
    void handleTranslationMiss(quint64 virtualAddr);
    
    //Instructions
	void handleIllegalInstruction(quint64 instructionWord, quint64 pc); // IntegerExecutor

    // Trap Handles
    void handleFpTrapRaised(helpers_JIT::TrapType trapType);
    void handleFpTrap(const QString& reason);
    void handleMemoryException(quint64 address, int accessType);
    void handleMemoryRead(quint64 address, quint64 value, int size);
    void handleMemoryWrite(quint64 address, quint64 value, int size);
    void handleHalt();
    void handleReset();
    void raiseTrap(helpers_JIT::TrapType trapType);
    void returnFromTrap();
    void trapRaised(helpers_JIT::TrapType type, quint64 currentPC);
    void trapOccurred(helpers_JIT::ExceptionType trapType, quint64 pc, quint8 cpuId_);
    void resetRequested();
    void handleTrapRaised(helpers_JIT::TrapType type);  // IntegerExecutor
    void finish();
    void instructionFaulted_(quint64 pc, quint32 instr);
    void executionFinished();

    // CPU Signal-Slots

	void resetCPU();

signals:
    // State changes
    void executionPaused();
    void executionStarted();
    void executionStopped();
    void stateChanged(helpers_JIT::CPUState newState);
    /**
     * @brief Emitted when the CPU halts execution.
     */
    void halted();                              // Emitted after system is halted
    void systemInitialized();                   // Emitted after system initialization

    // Exception and trap notifications
    void exceptionRaised(helpers_JIT::ExceptionType exceptionType, quint64 pc, quint64 faultAddr);
    void iplChanged(int oldIPL, int newIPL);
    void trapOccurred(helpers_JIT::ExceptionType trapType, quint64 pc);
    void trapOccurred(helpers_JIT::ExceptionType trapType, quint64 pc, quint8 cpuId_);

  
    void instructionFaulted(quint64 pc, quint32 inst);


    // Monitoring signals
    void instructionExecuted(quint64 pc, quint32 instruction);
    void memoryAccessed(quint64 address, bool isWrite, int size);
 
    //TODO - receiveInterrupt  /* cpuId check on processor zero   */
	void receiveInterrupt(int cpuId, int vector) {
		if (m_cpuId >= 0 ) {
			QMetaObject::invokeMethod(this, "receiveInterrupt",
				Qt::QueuedConnection, Q_ARG(int, vector));
		}
	}
    void registerChanged(int regNum, helpers_JIT::RegisterType type, quint64 value);

    // JIT compilation requests
    void hotSpotDetected(quint64 startAddr, quint64 endAddr, int execCount);
    void requestBlockCompilation(quint64 startAddr, const QByteArray& instructions);

	// Progress Messages signals:
	void processingProgress(int percentComplete);
	void operationStatus(const QString& message);
	void cycleExecuted(quint64 cycle);
    void operationCompleted();

    // QThread and CPU Signals
    void finished();

private:

    mutable QMutex  m_stateLock;                    // Mutex before saving old state                           
    // CPU state and identification
    int m_cpuId;
    QAtomicInteger<bool> stopRequested{ false };   // Flag that tells the CPU execution loop to exit cleanly
	bool m_running = false;                        // AlphaPALInterpreter::handleHalt(AlphaCPU* cpu)

    QThread* m_cpuThread;
	helpers_JIT::CPUState m_state 
        = helpers_JIT::CPUState::IDLE;             // AlphaPALInterpreter::handleHalt(AlphaCPU* cpu)

    // Register state
    QVector<quint64> m_intRegisters;                            // 32 integer registers (R0-R31)
    QVector<double> m_fpRegisters;                              // 32 floating-point registers (F0-F31)
    QMap<int, quint64> m_specialRegisters;                      // Special registers (FPCR, etc)
    QMap<helpers_JIT::MmuMode, QVector<StackFrame>> m_stacks;   // Stack Vector.  Stacks.   
    QScopedPointer<RegisterBank> registerBank;                  // Register Bank Register
    QScopedPointer<FpRegisterBankcls> fpRegisterBank;           // Floating-point Register Bank Register
    FPCRContext  m_fpcr;                                        // Alpha AXP FPCR context representation

    // Processor State
    quint64 m_pc;                       // Program counter
	quint64 m_kernelSP = 0;             // Kernel Stack Pointer
	quint64 m_userSP = 0;               // User Stack Pointer
	quint64 m_kernelGP = 0;             // Kernel Global Pointer
	quint64 m_fp = 0;                   // FramePointer
	quint64 m_psr = 0;                  // Process Status Register
    quint64 m_savedPsr = 0;             // Saved Process Status Register
	bool    m_lockFlag = false;         // TODO Define
	quint64 m_lockedPhysicalAddress = 0;// Locked Physical Address
    /*  TODO Block */
	bool        m_astEnable = false;        // TODO - m_astEnable
	quint64     m_asn = 0;                  // TODO - m_asn
    quint64     m_uniqueValue = 0;
    quint64     m_processorStatus = 0;
    quint64     m_usp = 0;              // User Stack Pointer
    quint64     m_vptptr = 0;           // Virtual Page Table Pointer

    AlphaPALInterpreter* m_palInterpreter;
    // Executors
    QScopedPointer<FloatingPointExecutor> floatingpointExecutor;
    QScopedPointer<ControlExecutor> controlExecutor;
    QScopedPointer<VectorExecutor> vecExec;
    QScopedPointer<IntegerExecutor> integerExec;

	// dispatch arrays
    QVector<std::function<void(const OperateInstruction&)>> vecDispatch;
	QVector<std::function<void(const OperateInstruction&)>> intDispatch;
    QVector<std::function<void(const OperateInstruction&)>> fpDispatch;
    QVector<std::function<void(quint32)>> ctrlDispatch;



	//signals and slots
	void Initialize_SignalsAndSlots();
    // Stack 

    StackFrame m_stackFrame;

    // PAL Structures


    // Control state
    int m_currentIPL;                   // Current Interrupt Priority Level
    bool m_kernelMode = false;          // True if in kernel mode
    bool m_palMode = false;             // The processor is in PAL Mode
    //Progress 
	int m_totalSteps = 100;             // Total number of operations
	int m_currentStep = 0;              // Current operation
    quint64 m_maxCycles =0;
    quint64 m_currentCycle = 0;

    // JIT Variables

    // Synchronization 
    QWaitCondition m_waitForInterrupt;

    // Executors
//     QHash<quint8, std::function<void(quint32)>> dispatchIntegerTable;
//     QHash<quint8, std::function<void(quint32)>> dispatchFloatingPointTable;
//     QHash<quint8, std::function<void(quint32)>> dispatchVectorTable;
//     QHash<quint8, std::function<void(quint32)>> dispatchControlTable;

		

    // Memory system reference
    AlphaMemorySystem*  m_memorySystem;    // Memory Manager - is passed in.
    SafeMemory* m_memory;                  // Let's hold a pointer to SafeMemory [convenience]
   // SafeMemory* m_memorySystem;          // Pointer to the memory retrieved from AlphaMemorySystem

    // JIT-related components
    AlphaJITCompiler* m_jitCompiler;
    QMap<quint64, QByteArray> m_compiledBlocks;
    QHash<quint64, int> m_blockHitCounter;
    
    //     m_jitHitCounter
    bool m_jitEnabled = true;
    quint32 m_jitThreshold = 50;            // 
    QHash<quint64, int> m_jitHitCounter;    // Is a highly efficient ordered map. For each instruction (pc)
                                            // track how many times it's been interpreted.
                                            // once it crosses a hot threshold, e.g. m_jiThreshold (50), the JIT 
                                            // compiles that block.
	int  jitOptimizationLevel = 2;		//   set to disable (0), 
	//   Basic Compilation (1), 
	//   Register Allocation (2), 
	//   Function Inline/Vectorization (3)

    // Exceptions
	QBitArray m_excSum = QBitArray(64, false);  // 64-bit trap summary vector
    bool    m_exceptionPending = false;     // Exceptions are queued for execution
	quint64 m_exceptionVector = 0;          // TODO - 


 
    // Execution methods
    void executeLoop();
    //void executeUnifiedFallback(quint32 instruction);
    void executeBlock(quint64 startAddr);
    void executeCompiledBlock(quint64 startAddr);
    void interpretInstruction(quint32 instruction);


    // Instruction helpers
    bool decodeAndExecute(quint32 instruction);
    //void executeBranchOperation(quint32 instruction);
    //void executeFloatingPointOperation(quint32 instruction);
   // void executeIntegerOperation(quint32 instruction);
   
    void executeMemoryOperation(quint32 instruction);
 
    void executeNextInstruction();

    // JIT tracking & optimization
    void checkForHotSpots();
    void updateBlockStatistics(quint64 startAddr);

    // Exception handling
    void dispatchException(helpers_JIT::ExceptionType type, quint64 faultAddr);

    
    // Processor Operations
    StackFrame popFrame();                  // Used to return the processor to Processing status
   
    // PAL Operations
    void executePALOperation(quint32 instruction);

    // 
	// MMU Architecture Mode
    bool m_bMMUEnabled = true;             // Uses AlphaMemorySystem::translate()   
                                           // or Flat Mode (Virtual Addr = Physical Addr)
                                           // TODO  m_bMMUEnabled = true;    
    void buildIntegerDispatchTable();
    void buildControlDispatchTable();
    void buildVectorDispatchTable();
    void buildFloatingPointDispatchTable();
    
};

#endif // ALPHACPU_H