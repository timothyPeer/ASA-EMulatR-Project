// AlphaJITCompiler.h
/*

void attachSafeMemory(SafeMemory* safeMemory) { memory = safeMemory; }
void attachRegisterBank(RegisterBank* regBank) { integerRegs = regBank; }
void attachFPRegisterWrapper(RegisterFileWrapper* fpRegBank) { m_registerFileWrapper = fpRegBank; }

*/
// AlphaJITCompiler.h
#pragma once

#ifndef ALPHA_JIT_COMPILER_H
#define ALPHA_JIT_COMPILER_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <functional>
#include "..\AESH\helpers.h"
#include "AlphaJITProfiler.h"
#include "..\AESH\safememory.h"
#include "tlbSystem.h"
#include "RegisterFileWrapper.h"

// Alpha Instruction Set Opcodes

// Section enum values (referenced in the switch statement)
#include "JitFunctionConstants.h"
#include "..\AEU\StackManager.h"
#include "AlphaSMPManager.h"
#include "JITFaultInfoStructures.h"
#include "JITBlock.h"
#include "MemoryAccessException.h"


/**
 * @brief Just-In-Time (JIT) compiler for Alpha instruction blocks with profiling.
 *
 * Integrates runtime profiling (hit counters) to auto-trigger compilation of hot blocks,
 * plus a simple branch predictor for future execution hints.
 */
class AlphaJITCompiler : public QObject {
	Q_OBJECT


	
	

	

public:
	// Block representation - a sequence of operations

	
	using BlockFunc = std::function<quint64(const JITBlock&)>;

	explicit AlphaJITCompiler(RegisterFileWrapper* intRegs,
		SafeMemory* memory, TLBSystem* tlb,
		QObject* parent = nullptr);

	AlphaJITCompiler(QObject* parent = nullptr) : QObject(parent) { 	}

	void generateLoadStore(JITBlock* block, alphaInstruction* instr);
	/**
	 * @brief Execute a block at the given PC, compiling it if necessary
	 */
	quint64 executeBlock(quint64 pc) {
		JITBlock& block = getOrCompileBlock(pc);
		return executeJITBlock(block);
	}

	/*   Connects pointer references to the JIT system */
	void attachMMIOManager(MMIOManager* mmio_) { m_mmioManager = mmio_; }
	void attachRegisterFileWrapper(RegisterFileWrapper* fpRegBank) { m_registerFileWrapper = fpRegBank; }
	void attachSafeMemory(SafeMemory* safeMemory) { m_safeMemory = safeMemory; }
//	void attachTLBSystem(TLBSystem* tlb) { m_tlbSystem = tlb; }
	/**
	 * @brief Sets a pointer to the system-wide SMP manager
	 *	      Required for cross-CPU reservation invalidation.
	 * @param sysPtr Pointer to the AlphaSMPManager instance
	 */
	void attachSMPManager(AlphaSMPManager* smpManager) { m_smpManagerPtr = smpManager; }

	void configurePerformanceCounter(quint64 value);
	bool createUnalignedLoad(JITBlock& block, int startIndex, int size);
	/**
	 * @brief Returns true if we have a compiled block cached
	 */
	bool hasBlock(quint64 pc) const {
		QMutexLocker l(&cacheMutex);
		return blockCache.contains(pc);
	}
	bool createUnalignedStore(JITBlock& block, int startIndex, int size);
	quint64 getPerformanceCounter();

	/**
	* @brief Invalidates a lock reservation if it matches a specific address
	* @param address The address that was written to by another processor
	*/
	void invalidateLockReservationIfMatch(quint64 address);						  // Invalidate a lock reservation if it matches a specific address
	MemoryFaultType mapTLBExceptionToMemoryFaultType(TLBException exception);
	quint64 readProcessorRegister(quint64 prNum);
	void writeProcessorRegister(quint64 prNum, quint64 value);

private slots:

	void handlePerformanceAlert(AlphaJITProfiler::PerformanceAlertType alertType, quint64 value);
	// Slot for updating high-frequency instruction cache when profiler signals
	

signals:
	void instructionCacheFlushed();
	void performanceAlertTriggered(QString alertMessage, quint64 value);
private:
	RegisterFileWrapper*			m_registerFileWrapper;
	SafeMemory*						m_safeMemory;
	TLBSystem*						m_tlbSystem;
	MMIOManager*					m_mmioManager;

	StackManager					m_stackManager;     // Exception stack manager

	QSharedPointer<AlphaJITProfiler>	m_profiler;
	QHash<quint64, quint64>			hitCounters;
	QSet<quint64>				highFrequencyInstructions; // Composite key of opcode<<32|function
	QHash<quint64, JITBlock>	blockCache;

	static const std::vector<PalEntryPoint> PAL_ENTRY_POINTS;	// PAL entry points table
	void ExecuteStq(const JITBlock::Operation& op, quint64 currentPC);
	void ExecuteStqConditional(const JITBlock::Operation& op, quint64 currentPC);
	void ExecuteStqUnaligned(const JITBlock::Operation& op, quint64 currentPC);
	mutable QMutex				cacheMutex;
	quint64						currentPC;
 
	QHash<quint64, quint64> branchPredictor;
	// Processor state registers
	quint64 m_currentProcessorMode;    // Current processor mode (kernel, executive, supervisor, user)
	quint64 m_palBaseAddress;          // Base address of the PAL code
	quint64 m_exceptionMask;           // Mask controlling which exceptions are enabled
	quint64 m_exceptionSummary;        // Summary of recent exceptions
	quint64 m_exceptionAddress;        // Address of the most recent exception

	// Lock reservation state for LL/SC instructions   (LDL_L, LDL_Q, STL_C, STL_Q)

	// Lock reservation state

	quint64 m_lockReservationAddr = 0;  // Address with active reservation - Virtual address with active reservation
	int m_lockReservationSize = 0;      // Size of reservation (4 or 8 bytes)
	bool m_lockValid = false;           // Whether reservation is still valid
	int m_cpuId = 0;					// Identifier for this CPU in the SMP system

	// Pointer to the system-wide coordinator (if available)
	AlphaSMPManager* m_smpManagerPtr = nullptr;

	// Instruction cache management
	QHash<quint64, bool> m_instructionCacheValid; // Tracks validity of cached instructions

	// Performance monitoring
	quint64 m_performanceCounter;      // Performance counter value
	quint64 m_performanceControlReg;   // Controls what the performance counter tracks
	

	quint8* getPhysicalPointer(quint64 virtualAddr);

	bool isHighFrequencyInstruction(quint32 opcode, quint32 function);
	bool isInKernelSpace(quint64 virtualAddr) const;
	bool isPhysicallyContiguous(quint64 virtualAddr, quint32 size) const;

	bool isSpecialInstruction(quint32 instruction);

	void updateHighFrequencyCache();

	JITBlock::Operation::OpType mapInstructionToOpType(quint32 opcode, quint32 function);

	// Special instruction handlers
	/*void handleMemoryBarrier(quint32 type);*/
	void handlePALCall(quint32 instruction);
	void interpretInstruction(quint32 rawInstr, quint64 pc);

	void handleTLBOperation(quint32 tlbOp);
	void handleFPException(helpers_JIT::Fault_TrapType trapType, quint64 pc);

	bool canFuseUnalignedOperations(const JITBlock& block, int startIndex);
	// Additional helper methods
	bool checkAlignment(quint64 address, int accessSize, quint32 opcode) const;
	/**
	 * @brief Checks if a memory address has a valid lock reservation
	 * @param address The address to check
	 * @return True if the address is locked, false otherwise
	 */
	bool checkLockReservation(quint64 address) const;							// Check if a memory address has a valid lock reservation
	void flushInstructionCache();
	void invalidateInstructionCacheEntry(quint64 virtualAddress);
	/**
	 * @brief Invalidates any lock reservation
	 * This is called when another processor writes to a locked address
	 */
	void invalidateLockReservation() { m_lockValid = false; }				 // Invalidate any lock reservation

	bool isMemoryFormat(quint32 opcode);
	bool isOperateFormat(quint32 opcode);
	bool isBranchFormat(quint32 opcode);
	bool isControlTransfer(quint32 opcode, quint32 function);
	/**
	 * @brief Sets the CPU ID for this JIT compiler instance
	 *        Required for proper SMP lock tracking.
	 * @param id The CPU identifier (0-based index)
	 */
	void setCpuId(int id) { m_cpuId = id; }											// Set CPU ID for this JIT instance
	void setupSpecialOperation(JITBlock::Operation& operation, quint32 opcode, quint32 function);
	qint64 SEXT16(quint16 value) const;
	bool isExtractionOperation(const JITBlock::Operation& op);
	bool isInsertOperation(const JITBlock::Operation& op);
	bool isMaskOperation(const JITBlock::Operation& op);
	bool isBitwiseOr(const JITBlock::Operation& op);
	int determineStoreSize(const JITBlock::Operation& insertOp, const JITBlock::Operation& maskOp);
	
	//quint64 executeLdqUnaligned(quint64 baseReg, qint16 displacement, quint8 destReg);

	/* Helper Functions */
	// Add this to AlphaJITCompiler.cpp
	// Replace the existing executeLdqUnaligned with this implementation
	MemoryFaultInfo executeLdqUnaligned(quint64 baseReg, qint16 displacement, quint8 destReg, quint64 pc, quint32 rawInstr);

	void optimizeUnalignedAccess(JITBlock& block, int startIndex);

	MemoryFaultInfo performMemoryAccess(quint64 virtualAddress, void* valuePtr, int accessSize, bool isWrite, bool isExec, quint64 pc, quint32 rawInstr);

	// In a method that processes memory writes from other CPUs:
	void handleExternalMemoryWrite(quint64 physicalAddress);
	void handleUnalignedLoadWithContext(RegisterFileWrapper* regFile, SafeMemory* memory, quint8 baseReg, quint8 destReg, quint64 offset, int size, quint64 pc);
	void handleUnalignedStoreWithContext(RegisterFileWrapper* regFile, SafeMemory* memory, quint8 baseReg, quint8 valueReg, quint64 offset, int size, quint64 pc);
	void handleUnalignedLoad(RegisterFileWrapper* regFile, SafeMemory* memory, int size);
	void handleUnalignedStore(RegisterFileWrapper* regFile, SafeMemory* memory, int size);
	void handleMemoryAccessViolation(quint64 virtualAddress, quint64 pc);
	// In your handleMemoryBarrier method or MB instruction case
	void handleMemoryBarrier(quint32 type);
	void handleMemoryFault(const MemoryFaultInfo& faultInfo);				// Handle memory fault and push exception frame
	void handleMemoryReadFault(quint64 virtualAddress, quint64 pc);
	void handleTranslationFault(quint64 virtualAddress, quint64 pc);
	void handleUnalignedAccess(RegisterFileWrapper* regFile, SafeMemory* memory);
	// 	void flushInstructionCache();
// 	void invalidateInstructionCacheEntry(quint64 virtualAddress);
// 	void setupSpecialOperation(JITBlock::Operation& operation, quint32 opcode, quint32 function);
 	//quint64 signExtend16(quint16 value);
 	//quint64 signExtend21(quint32 value);


	/**
     * @brief Get or compile a block at the given PC based on hit counters
     */
    JITBlock& getOrCompileBlock(quint64 pc);

    /**
     * @brief Compile a block starting at the given PC
     */
    JITBlock compileBlock(quint64 pc);										// Create a frequently executed "hot-block"

    /**
     * @brief Create an interpreter fallback block
     */
    JITBlock createInterpreterBlock(quint64 pc);							// Creates a simpler Interpreter Fallback Block

	bool detectUnalignedPattern(JITBlock& block, int startIndex, quint64 currentPC);
	void dumpState();
	/**
     * @brief Execute operations in a JIT block and return next PC
     */
    quint64 executeJITBlock(const JITBlock& block);

	void ExecuteLdq(const JITBlock::Operation& op, quint64 currentPC);
	void HandlePrefetch(quint64 virtualAddr, bool modifyIntent, bool evictNext);
	void updateCPUState(quint64 newPC, bool isBranch);
	/**
     * @brief Fallback interpreter for a block starting at `pc`
     */
    quint64 interpretBlock(quint64 pc);

    /**
     * @brief Update branch predictor with actual target for block at `pc`
     */
    void updateBranchPredictor(quint64 pc, quint64 actualTarget);
};

#endif // ALPHA_JIT_COMPILER_H



