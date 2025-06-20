
#include "AlphaJITCompiler.h"
#include <QMutexLocker>
#include <QDebug>
#include "decodeOperate.h"
#include "tlbSystem.h"
#include "RegisterFileWrapper.h"
#include "JitFunctionConstants.h"
#include "helpers.h"
#include "JITFaultInfoStructures.h"
#include "ExcSum.h"
#include "GlobalMacro.h"
#include "FPException.h"
#include "..\AEE\MemoryAccessException.h"
#include "TranslationResult.h"

// Constructor update to connect signals
AlphaJITCompiler::AlphaJITCompiler(RegisterFileWrapper* regFileWrapper,
	SafeMemory* mem, TLBSystem* tlb,
	QObject* parent)
	: QObject(parent)
	, m_safeMemory(mem)
	, m_tlbSystem(tlb)
	, m_registerFileWrapper(regFileWrapper)
	, m_currentProcessorMode(0)         // Initialize to kernel mode (0)
	, m_palBaseAddress(0x2000)          // Default PAL base address
	, m_exceptionMask(0)                // No exceptions enabled by default
	, m_exceptionSummary(0)             // No exceptions have occurred
	, m_exceptionAddress(0)             // No exception address
	, m_performanceCounter(0)           // Initialize counter to 0
	, m_performanceControlReg(0)        // Default configuration
{
	m_profiler = new AlphaJITProfiler(this);
	if (m_safeMemory) {
		m_safeMemory->attachProfiler(&m_profiler);
	}
	m_profiler->setHotThreshold(100);

	// Connect the signal from profiler to our slot
	connect(m_profiler->data(), &AlphaJITProfiler::instructionHotspotUpdated,
		this, &AlphaJITCompiler::updateHighFrequencyCache);

	// Connect the profiler's performance alert signal to our handler
	// Connect the performance alert signal from profiler to our handler
	connect(m_profiler->data(),
		static_cast<void (AlphaJITProfiler::*)(AlphaJITProfiler::PerformanceAlertType, quint64)>(&AlphaJITProfiler::performanceAlert),
		this,
		&AlphaJITCompiler::handlePerformanceAlert);

	/*
	* Implementation & Usage Hint
	connect(m_jitCompiler->getProfiler().data(), &AlphaJITProfiler::prefetchRecorded,
		this, &YourClass::handlePrefetchRecorded);
	*/
}



// In AlphaJITCompiler.cpp (pseudocode)

void AlphaJITCompiler::generateLoadStore(JITBlock* block, alphaInstruction* instr) {
	// Generate code for memory access using AlphaMemorySystem interface
	// Use the CPU's AlphaMemorySystem instance
	emit("  movq %rbx, %rdi  // CPU context");
	emit("  movq [%rbx + offsetof(AlphaCPU, m_memorySystem)], %rsi  // AlphaMemorySystem");
	emit("  movq [%rbx + offsetof(AlphaCPU, m_tlbSystem)], %rdx  // TLBSystem");

	// Set up parameters for the memory access
	emit("  // Calculate effective address");
	// ... address calculation code

	// Call the proper function
	if (isLoad) {
		emit("  call PerformMemoryRead");
	}
	else {
		emit("  call PerformMemoryWrite");
	}

	// Check for exceptions
	emit("  testl %eax, %eax");
	emit("  jz handle_memory_exception");
}

AlphaJITCompiler::JITBlock& AlphaJITCompiler::getOrCompileBlock(quint64 pc) {
	// Update hit counter for this PC
	quint64 hits = ++hitCounters[pc];

	// Check if block is already in cache
	{
		QMutexLocker locker(&cacheMutex);
		auto it = blockCache.find(pc);
		if (it != blockCache.end()) {
			return it.value();
		}
	}

	// Compile a new block based on hit count
	JITBlock newBlock;
	if (hits >= m_profiler->getHotThreshold()) {
		newBlock = compileBlock(pc);
	}
	else {
		newBlock = createInterpreterBlock(pc);
	}

	// Insert the new block into the cache
	QMutexLocker locker(&cacheMutex);
	return blockCache.insert(pc, newBlock).value();
}

// In AlphaJITCompiler.cpp
JITBlock AlphaJITCompiler::compileBlock(quint64 pc) {
	JITBlock block;
	block.startPC = pc;
	block.isFallback = false;
	block.containsSpecialOps = false;

	quint64 currentPC = pc;
	bool foundExit = false;
	std::vector<std::unique_ptr<alphaInstruction>> instructions;

	int maxInstructions = 20; // Limit block size
	for (int i = 0; i < maxInstructions && !foundExit; i++) {
		try {
			quint32 rawInstr = m_safeMemory->readUInt32(currentPC);

			// Create instruction object
			auto instr = alphaInstruction::Create(rawInstr);
			instr->SetPC(currentPC); // Store PC for exception handling

			// Record frequency for profiling
			m_profiler->recordInstructionExecution(instr->GetOpcode(), instr->GetFunction());

			// Create operation for JIT block
			JITBlock::Operation operation;
			operation.rawInstr = rawInstr;

			// Auto-fill register fields based on instruction format
			if (instr->IsMemoryOperation()) {
				operation.ra = (rawInstr >> 21) & 0x1F;
				operation.rb = 0; // Not used in memory format
				operation.rc = (rawInstr >> 0) & 0x1F;
				operation.immediate = signExtend16(rawInstr & 0xFFFF);
			}
			else if (instr->IsOperateFormat()) {
				operation.ra = (rawInstr >> 21) & 0x1F;
				operation.rb = (rawInstr >> 16) & 0x1F;
				operation.rc = (rawInstr >> 0) & 0x1F;
				operation.function = instr->GetFunction();
			}
			else if (instr->IsBranchOperation()) {
				operation.ra = (rawInstr >> 21) & 0x1F;
				operation.rb = 0; // Not used in branch format
				operation.rc = 0; // Not used in branch format
				operation.immediate = signExtend21(rawInstr & 0x1FFFFF);
			}

			// Determine operation type based on hot path analysis
			if (isHighFrequencyInstruction(instr->GetOpcode(), instr->GetFunction())) {
				operation.type = instr->GetJitOpType();
			}
			// Special instructions that need custom handling
			else if (instr->CanFuseWith(nullptr)) {
				block.containsSpecialOps = true;
				setupSpecialOperation(operation, instr->GetOpcode(), instr->GetFunction());
			}
			// Non-frequent instructions use fallback
			else {
				operation.type = JITBlock::Operation::OpType::FALLBACK;
			}

			// Store instruction for fusion analysis
			instructions.push_back(std::move(instr));

			block.operations.append(operation);
			currentPC += 4;

			// Check for natural block boundaries
			if (instructions.back()->IsBranchOperation() ||
				instructions.back()->IsBarrierOperation()) {
				foundExit = true;
			}
		}
		catch (const std::exception& e) {
			qWarning() << "Exception during block compilation at PC="
				<< QString::number(currentPC, 16) << ": " << e.what();
			block.isFallback = true;
			return block;
		}
	}

	// Pattern fusion analysis on the whole block
	for (size_t i = 0; i < instructions.size() - 1; i++) {
		// Check if this instruction can fuse with the next
		if (instructions[i]->CanFuseWith(instructions[i + 1].get())) {
			auto fusedInstr = instructions[i]->CreateFused(instructions[i + 1].get());

			if (fusedInstr) {
				// Replace the operation with a fused version
				block.operations[i].type = fusedInstr->GetJitOpType();

				// Create specialized handler for the fused operation
				block.operations[i].specialHandler =
					[this, fusedInstr = std::unique_ptr<alphaInstruction>(fusedInstr)](
						RegisterFileWrapper* regFile,
						RegisterFileWrapper* fpRegFile,
						SafeMemory* memory) {
							// Call the fused instruction execute method
							fusedInstr->Execute(regFile, memory, m_tlbSystem);
					};

				// Mark the next operation as a NOP
				block.operations[i + 1].type = JITBlock::Operation::OpType::NOP;
				block.containsSpecialOps = true;

				// Skip the next instruction in our analysis
				i++;
			}
		}
	}

	// Advanced pattern detection across multiple instructions
	if (!block.isFallback) {
		for (int i = 0; i < block.operations.size() - 3; i++) {
			quint64 opPC = block.startPC + (i * 4);

			if (detectUnalignedPattern(block, i, opPC)) {
				// Skip ahead past the fused operations
				i += 3;
			}
		}
	}

	return block;
}
JITBlock AlphaJITCompiler::createInterpreterBlock(quint64 pc) {
	JITBlock block;
	block.startPC = pc;
	block.isFallback = true;
	qDebug() << "Creating interpreter block at PC=" << QString::number(pc, 16);
	return block;
}
/*
bool AlphaJITCompiler::detectUnalignedPattern(JITBlock& block, int startIndex, quint64 currentPC) {
	// Ensure we have enough operations to check
	if (startIndex + 3 >= block.operations.size()) {
		return false;
	}

	// Get references to operations
	const auto& op1 = block.operations[startIndex];
	const auto& op2 = block.operations[startIndex + 1];
	const auto& op3 = block.operations[startIndex + 2];
	const auto& op4 = block.operations[startIndex + 3];

	// Pattern 1: Unaligned quadword load
	if (op1.type == JITBlock::Operation::OpType::MEM_LDQ_U &&
		op2.type == JITBlock::Operation::OpType::MEM_LDQ_U) {

		// Check for byte extraction and combination pattern
		if (isExtractionOperation(op3) && isExtractionOperation(op4)) {
			// Optional: Check for fifth operation that combines the extractions
			if (startIndex + 4 < block.operations.size()) {
				const auto& op5 = block.operations[startIndex + 4];
				if (isBitwiseOr(op5)) {
					// Found full pattern - create fused unaligned load
					return createUnalignedLoad(block, startIndex, 8);
				}
			}

			// Even without explicit OR, we can still optimize
			return createUnalignedLoad(block, startIndex, 8);
		}
	}

	// Pattern 2: Unaligned longword load
	// Similar to quadword but with LDL_U if it exists, or can be synthesized
	// with LDQ_U and appropriate masks
	if (op1.type == JITBlock::Operation::OpType::MEM_LDQ_U &&
		isExtractionOperation(op2) && isExtractionOperation(op3)) {

		// Check if this matches longword extraction pattern
		if (isLongwordExtraction(op2, op3)) {
			return createUnalignedLoad(block, startIndex, 4);
		}
	}

	// Pattern 3: Unaligned word load
	// Similar checks for word-sized operations
	if (op1.type == JITBlock::Operation::OpType::MEM_LDQ_U &&
		isWordExtractionPattern(op1, op2, op3)) {
		return createUnalignedLoad(block, startIndex, 2);
	}

	// Pattern 4: Unaligned stores (more complex, using LDQ_U + INSBL + MSKBL)
	if (op1.type == JITBlock::Operation::OpType::MEM_LDQ_U &&
		isInsertOperation(op2) && isMaskOperation(op3) &&
		op4.type == JITBlock::Operation::OpType::MEM_STQ_U) {

		int storeSize = determineStoreSize(op2, op3);
		if (storeSize > 0) {
			return createUnalignedStore(block, startIndex, storeSize);
		}
	}

	return false; // No unaligned pattern detected
}
*/

// Enhanced pattern detection using class approach
bool AlphaJITCompiler::detectUnalignedPattern(JITBlock& block, int startIndex, quint64 currentPC) {
	// Create instruction objects to analyze the pattern
	if (startIndex + 3 >= block.operations.size()) {
		return false;
	}

	// Get raw instructions
	quint32 raw1 = block.operations[startIndex].rawInstr;
	quint32 raw2 = block.operations[startIndex + 1].rawInstr;
	quint32 raw3 = block.operations[startIndex + 2].rawInstr;
	quint32 raw4 = block.operations[startIndex + 3].rawInstr;

	// Create instruction objects
	auto instr1 = alphaInstruction::Create(raw1);
	auto instr2 = alphaInstruction::Create(raw2);
	auto instr3 = alphaInstruction::Create(raw3);
	auto instr4 = alphaInstruction::Create(raw4);

	// Check if first instruction can fuse with second
	if (instr1->CanFuseWith(instr2.get())) {
		// Create fused operation
		auto fusedInstr = instr1->CreateFused(instr2.get());

		if (fusedInstr) {
			// Create the fused operation in the block
			JITBlock::Operation fusedOp;
			fusedOp.type = fusedInstr->GetJitOpType();
			fusedOp.rawInstr = raw1; // Use first instruction for reference

			// Set up context for the fused operation
			if (fusedOp.type == JITBlock::Operation::OpType::MEM_UNALIGNED_LOAD_QUADWORD) {
				// Create specialized handler
				fusedOp.specialHandler = [this, currentPC](
					RegisterFileWrapper* regFile,
					RegisterFileWrapper* fpRegFile,
					SafeMemory* memory) {
						this->handleUnalignedLoadWithContext(
							regFile, memory,
							block.operations[startIndex].ra,
							block.operations[startIndex + 3].rc,
							block.operations[startIndex].immediate,
							8, // Quadword
							currentPC);
					};

				// Replace with fused operation
				block.operations[startIndex] = fusedOp;

				// Mark subsequent operations as NOPs
				for (int i = 1; i < 4; i++) {
					if (startIndex + i < block.operations.size()) {
						block.operations[startIndex + i].type = JITBlock::Operation::OpType::NOP;
					}
				}

				block.containsSpecialOps = true;
				return true;
			}
		}
	}

	return false;
}

/*
	OpCode				Typical Usage										Exceptions (Status=[C]omplete || [T]odo
	---------------	-------------------------------------------------------------------------------------------------
	OPCODE_LDQ_U	in conjunction with EXTBL/EXTWL instructions			Access Violation (C)
					to extract specific bytes from the loaded quadwords.	Fault on Read(C)
																			Translation Not Valid(C)
	OPCODE_LDQ																Access Violation (C)
																			Fault on Read(C)
																			Translation Not Valid(C)
	OPCODE_STQ
	OPCODE_LDBU
	OPCODE_LDL
	OPCODE_LDWU

*/




void AlphaJITCompiler::dumpState() {
	DEBUG_LOG("=================== CPU STATE DUMP ===================");
	DEBUG_LOG(QString("Current PC: 0x%1").arg(QString::number(currentPC, 16)));
	DEBUG_LOG(QString("Current Mode: %1").arg(m_currentProcessorMode));

	// Dump registers
	m_registerFileWrapper->dump();

	// Dump memory around PC and stack
	quint64 stackPtr = m_registerFileWrapper->readIntReg(30); // R30 is typically stack pointer
	m_safeMemory->dumpMemory(currentPC - 16, 64); // Dump code around PC
	m_safeMemory->dumpMemory(stackPtr, 64); // Dump top of stack

	DEBUG_LOG("=======================================================");
}
// In AlphaJITCompiler.cpp
quint64 AlphaJITCompiler::executeJITBlock(const JITBlock& block) {
	if (block.isFallback) {
		DEBUG_LOG(QString("Using interpreter fallback for block at PC=0x%1")
			.arg(QString::number(block.startPC, 16)));
		return interpretBlock(block.startPC);
	}

	quint64 currentPC = block.startPC;
	bool monitoringEnabled = m_profiler->isMonitoringEnabled();

	DEBUG_LOG(QString("Executing JIT block at PC=0x%1, operations=%2")
		.arg(QString::number(currentPC, 16))
		.arg(block.operations.size()));

	// Static counter for periodic dumps
	static int instructionCounter = 0;

	// Execute each operation in the block
	for (const auto& op : block.operations) {
		currentPC += 4; // Increment PC for each instruction

		// Skip NOPs (from fused operations)
		if (op.type == JITBlock::Operation::OpType::NOP) {
			TRACE_LOG("Skipping NOP instruction");
			continue;
		}

		// Record execution for profiling
		if (monitoringEnabled) {
			m_profiler->recordInstructionCount();
			instructionCounter++;

			// Record instruction execution by opcode and function
			quint32 opcode = (op.rawInstr >> 26) & 0x3F;
			quint32 function = 0;

			// Extract function code based on instruction format
			if (opcode >= 0x10 && opcode <= 0x1F) {
				function = (op.rawInstr >> 5) & 0x7F;
			}
			else if (opcode == 0x16 || opcode == 0x17) {
				function = (op.rawInstr >> 5) & 0x7FF;
			}

			m_profiler->recordInstructionExecution(opcode, function);

			// Profile specific instruction types
			if (op.type == JITBlock::Operation::OpType::MEM_LDQ ||
				op.type == JITBlock::Operation::OpType::MEM_LDL ||
				op.type == JITBlock::Operation::OpType::MEM_STQ ||
				op.type == JITBlock::Operation::OpType::MEM_STL) {
				m_profiler->recordMemoryOperation();
			}

			if (op.type == JITBlock::Operation::OpType::BRANCH_BEQ ||
				op.type == JITBlock::Operation::OpType::BRANCH_BNE ||
				op.type == JITBlock::Operation::OpType::BRANCH_BLT ||
				op.type == JITBlock::Operation::OpType::BRANCH_BLE ||
				op.type == JITBlock::Operation::OpType::BRANCH_BGT ||
				op.type == JITBlock::Operation::OpType::BRANCH_BGE ||
				op.type == JITBlock::Operation::OpType::BRANCH_BLBC ||
				op.type == JITBlock::Operation::OpType::BRANCH_BLBS) {
				m_profiler->recordBranchInstruction();
			}

			// Periodic state dump (every 100K instructions)
			if (instructionCounter % 100000 == 0) {
				INFO_LOG(QString("Periodic state dump after %1 instructions")
					.arg(instructionCounter));
				dumpState();
			}
		}

		DEBUG_LOG(QString("Executing instruction 0x%1 at PC=0x%2, type=%3")
			.arg(QString::number(op.rawInstr, 16))
			.arg(QString::number(currentPC - 4, 16))
			.arg(static_cast<int>(op.type)));

		try {
			// For special operations, call their handler
			if (op.specialHandler) {
				TRACE_LOG("Executing special handler");
				op.specialHandler(m_registerFileWrapper, nullptr, m_safeMemory);
				continue;
			}

			// Use inline hot path for the most frequent operations
			switch (op.type) {
				// Integer arithmetic
			case JITBlock::Operation::OpType::INT_ADD: {
				quint64 a = m_registerFileWrapper->readIntReg(op.ra);
				quint64 b = m_registerFileWrapper->readIntReg(op.rb);
				quint64 result = a + b;
				TRACE_LOG(QString("INT_ADD: R%1(0x%2) + R%3(0x%4) = 0x%5")
					.arg(op.ra)
					.arg(QString::number(a, 16))
					.arg(op.rb)
					.arg(QString::number(b, 16))
					.arg(QString::number(result, 16)));
				m_registerFileWrapper->writeIntReg(op.rc, result);
				break;
			}

			case JITBlock::Operation::OpType::INT_SUB: {
				quint64 a = m_registerFileWrapper->readIntReg(op.ra);
				quint64 b = m_registerFileWrapper->readIntReg(op.rb);
				quint64 result = a - b;
				TRACE_LOG(QString("INT_SUB: R%1(0x%2) - R%3(0x%4) = 0x%5")
					.arg(op.ra)
					.arg(QString::number(a, 16))
					.arg(op.rb)
					.arg(QString::number(b, 16))
					.arg(QString::number(result, 16)));
				m_registerFileWrapper->writeIntReg(op.rc, result);
				break;
			}
													 // Memory operations
			case JITBlock::Operation::OpType::MEM_LDQ: {
				TRACE_LOG("Executing LDQ operation");
				ExecuteLdq(op, currentPC);
				break;
			}

			case JITBlock::Operation::OpType::MEM_STQ: {
				if (monitoringEnabled) {
					m_profiler->recordMemoryOperation();
				}

				TRACE_LOG(QString("Executing STQ at PC=0x%1")
					.arg(QString::number(currentPC - 4, 16)));

				try {
					ExecuteStq(op, currentPC);
				}
				catch (const MemoryAccessException& e) {
					ERROR_LOG(QString("Memory exception during STQ at PC=0x%1: %2")
						.arg(QString::number(currentPC - 4, 16))
						.arg(e.what()));

					// Handle the memory fault
					handleMemoryFault(e.getFaultInfo());
					return currentPC - 4; // Return to faulting instruction
				}
				break;
			}

			case JITBlock::Operation::OpType::MEM_STQ_U: {
				if (monitoringEnabled) {
					m_profiler->recordMemoryOperation();
				}

				TRACE_LOG(QString("Executing STQ_U at PC=0x%1")
					.arg(QString::number(currentPC - 4, 16)));

				try {
					ExecuteStqUnaligned(op, currentPC);
				}
				catch (const MemoryAccessException& e) {
					ERROR_LOG(QString("Memory exception during STQ_U at PC=0x%1: %2")
						.arg(QString::number(currentPC - 4, 16))
						.arg(e.what()));

					// Handle the memory fault
					handleMemoryFault(e.getFaultInfo());
					return currentPC - 4; // Return to faulting instruction
				}
				break;
			}

			case JITBlock::Operation::OpType::MEM_STQ_C: {
				if (monitoringEnabled) {
					m_profiler->recordMemoryOperation();

					// Also record this as a locked operation (for performance tracking)
					m_profiler->recordLockedOperation(
						m_registerFileWrapper->readIntReg(op.ra) + op.immediate, // address
						true, // is write
						false  // success unknown yet
					);
				}

				TRACE_LOG(QString("Executing STQ_C at PC=0x%1")
					.arg(QString::number(currentPC - 4, 16)));

				try {
					ExecuteStqConditional(op, currentPC);
				}
				catch (const MemoryAccessException& e) {
					ERROR_LOG(QString("Memory exception during STQ_C at PC=0x%1: %2")
						.arg(QString::number(currentPC - 4, 16))
						.arg(e.what()));

					// Handle the memory fault
					handleMemoryFault(e.getFaultInfo());
					return currentPC - 4; // Return to faulting instruction
				}
				break;
			}

			case JITBlock::Operation::OpType::MEM_UNALIGNED_STORE_QUADWORD: {
				if (monitoringEnabled) {
					m_profiler->recordMemoryOperation();
				}

				TRACE_LOG(QString("Executing fused unaligned quadword store at PC=0x%1")
					.arg(QString::number(currentPC - 4, 16)));

				// These operations have specialized handlers
				if (op.specialHandler) {
					try {
						op.specialHandler(m_registerFileWrapper, nullptr, m_safeMemory);
					}
					catch (const MemoryAccessException& e) {
						ERROR_LOG(QString("Memory exception during fused unaligned store at PC=0x%1: %2")
							.arg(QString::number(currentPC - 4, 16))
							.arg(e.what()));

						// Handle the memory fault
						handleMemoryFault(e.getFaultInfo());
						return currentPC - 4; // Return to faulting instruction
					}
				}
				else {
					WARN_LOG(QString("Missing handler for unaligned store at PC=0x%1")
						.arg(QString::number(currentPC - 4, 16)));
				}
				break;
			}

													   // Branch operations
			case JITBlock::Operation::OpType::BRANCH_BEQ: {
				quint64 a = m_registerFileWrapper->readIntReg(op.ra);
				bool condition = (a == 0);

				// Calculate target address
				quint64 target;
				if (condition) {
					target = currentPC + (op.immediate << 2);
				}
				else {
					target = currentPC;
				}

				TRACE_LOG(QString("BRANCH_BEQ: R%1=0x%2, condition=%3, target=0x%4")
					.arg(op.ra)
					.arg(QString::number(a, 16))
					.arg(condition)
					.arg(QString::number(target, 16)));

				// Take the branch
				if (condition) {
					return target; // Break out of the block
				}
				break;
			}

			case JITBlock::Operation::OpType::BRANCH_BNE: {
				quint64 a = m_registerFileWrapper->readIntReg(op.ra);
				bool condition = (a != 0);

				// Calculate target address
				quint64 target;
				if (condition) {
					target = currentPC + (op.immediate << 2);
				}
				else {
					target = currentPC;
				}

				TRACE_LOG(QString("BRANCH_BNE: R%1=0x%2, condition=%3, target=0x%4")
					.arg(op.ra)
					.arg(QString::number(a, 16))
					.arg(condition)
					.arg(QString::number(target, 16)));

				// Take the branch
				if (condition) {
					return target; // Break out of the block
				}
				break;
			}

			case JITBlock::Operation::OpType::FALLBACK:
			{
				DEBUG_LOG(QString("Using instruction interpreter for 0x%1")
					.arg(QString::number(op.rawInstr, 16)));
				interpretInstruction(op.rawInstr, currentPC - 4);
				break;
			}

			default:
				WARN_LOG(QString("Unhandled operation type %1 for instruction 0x%2")
					.arg(static_cast<int>(op.type))
					.arg(QString::number(op.rawInstr, 16)));
				interpretInstruction(op.rawInstr, currentPC - 4);
				break;
			}
		}
		catch (const MemoryAccessException& e) {
			ERROR_LOG(QString("Memory access exception at PC=0x%1: %2")
				.arg(QString::number(currentPC - 4, 16))
				.arg(e.what()));

			// Dump state on exception for debugging
			dumpState();

			// Handle the fault and return to faulting instruction
			handleMemoryFault(e.getFaultInfo());
			return currentPC - 4;
		}
		catch (const FPException& e) {
			ERROR_LOG(QString("Floating point exception at PC=0x%1: type=%2")
				.arg(QString::number(currentPC - 4, 16))
				.arg(static_cast<int>(e.getTrapType())));

			// Dump state on exception for debugging
			dumpState();

			// Handle the exception and return to faulting instruction
			handleFPException(e.getTrapType(), e.getPC());
			return currentPC - 4;
		}
		catch (const std::exception& e) {
			CRITICAL_LOG(QString("Unexpected exception at PC=0x%1: %2")
				.arg(QString::number(currentPC - 4, 16))
				.arg(e.what()));

			// Dump state on exception for debugging
			dumpState();

			// Rethrow unexpected exceptions
			throw;
		}
	}

	DEBUG_LOG(QString("Completed JIT block execution, next PC=0x%1")
		.arg(QString::number(currentPC, 16)));

	return currentPC;
}


// Add to AlphaJITCompiler.cpp
void AlphaJITCompiler::ExecuteLdq(const JITBlock::Operation& op, quint64 currentPC) {
	quint64 virtualAddr = m_registerFileWrapper->readIntReg(op.ra) + op.immediate;

	DEBUG_LOG(QString("ExecuteLdq: VA=0x%1, dest=R%2")
		.arg(QString::number(virtualAddr, 16))
		.arg(op.rc));

	// Check for prefetch hint (LDQ with R31 as destination)
	if (op.rc == 31) {
		// Extract the prefetch hint type from the instruction
		quint32 hintType = (op.rawInstr >> 13) & 0x3;
		bool evictNext = (hintType == PREFETCH_HINT_EN || hintType == PREFETCH_HINT_MEN);
		bool modifyIntent = (hintType == PREFETCH_HINT_M || hintType == PREFETCH_HINT_MEN);

		DEBUG_LOG(QString("Prefetch hint detected: type=%1, modifyIntent=%2, evictNext=%3")
			.arg(hintType)
			.arg(modifyIntent)
			.arg(evictNext));

		// Handle as prefetch hint
		HandlePrefetch(virtualAddr, modifyIntent, evictNext);
		return;
	}

	// Check alignment for non-prefetch operations
	if (virtualAddr & 0x7) {
		WARN_LOG(QString("LDQ alignment check: Address 0x%1 is not 8-byte aligned")
			.arg(QString::number(virtualAddr, 16)));
	}

	quint64 value = 0;

	// Perform the memory access with full fault handling
	MemoryFaultInfo faultInfo = PerformMemoryAccess(
		virtualAddr,       // Virtual address
		&value,            // Where to store the result
		8,                 // Quadword size (8 bytes)
		false,             // Not a write
		false,             // Not an instruction fetch
		currentPC - 4,     // PC of the instruction
		op.rawInstr        // Raw instruction for fault reporting
	);

	// If fault occurred, throw exception
	if (faultInfo.faultType != MemoryFaultType::NONE) {
		throw MemoryAccessException(faultInfo);
	}

	// Update the destination register
	DEBUG_LOG(QString("LDQ result: 0x%1 -> R%2")
		.arg(QString::number(value, 16))
		.arg(op.rc));
	m_registerFileWrapper->writeIntReg(op.rc, value);
}

void AlphaJITCompiler::HandlePrefetch(quint64 virtualAddr, bool modifyIntent, bool evictNext) {
	// Translate address
	bool isKernelMode = (m_currentProcessorMode == 0);
	quint64 currentASN = m_tlbSystem->getCurrentASN();

	TranslationResult tlbResult =
		m_tlbSystem->translateAddress(virtualAddr, modifyIntent, false, currentASN, isKernelMode);

	// Only proceed if translation succeeded - prefetch ignores exceptions
	if (tlbResult.tlbException == TLBException::NONE) {
		// Provide hint to memory system
		m_safeMemory->prefetchHint(tlbResult.physicalAddress, 8, evictNext);

		// If monitoring is enabled, record the prefetch
		if (m_profiler->isMonitoringEnabled()) {
			m_profiler->recordPrefetch(virtualAddr, 8, modifyIntent, evictNext);
		}
	}
}
// Add detailed CPU state tracking:
void AlphaJITCompiler::updateCPUState(quint64 newPC, bool isBranch) {
	// Track CPU state for optimizations
	// ...
}


quint64 AlphaJITCompiler::interpretBlock(quint64 pc) {
	qWarning() << "Interpreter fallback at PC=" << QString::number(pc, 16);
	return pc + 4; // Just advance PC by 4 for fallback implementation
}

void AlphaJITCompiler::updateBranchPredictor(quint64 pc, quint64 actualTarget) {
	branchPredictor[pc] = actualTarget;
}

bool AlphaJITCompiler::isHighFrequencyInstruction(quint32 opcode, quint32 function) {
	// Create composite key
	quint64 key = ((quint64)opcode << 32) | function;

	// Check if it's in our high-frequency set
	if (highFrequencyInstructions.contains(key)) {
		return true;
	}

	// Default high-frequency instructions (always included)
	switch (opcode) {
	case JITBlock::OpCodeClass::OpInteger_Operate:
		// Common integer operations
		return (function == FUNC_ADDQ || function == FUNC_ADDL ||
			function == FUNC_SUBQ || function == FUNC_SUBL ||
			function == FUNC_MULQ || function == FUNC_AND ||
			function == FUNC_BIS || function == FUNC_XOR);

	case JITBlock::OpCodeClass::OpFP_Operate:
		// Common floating-point operations
		return (function == FUNC_ADDT || function == FUNC_SUBT ||
			function == FUNC_MULT || function == FUNC_DIVT);

	case JITBlock::OpCodeClass::OpMemory_Store:  // This covers the range 0x28-0x2F
		// Common memory store operations
		return (opcode == OPCODE_STQ || opcode == OPCODE_STL);

		// And in another case statement:
	case JITBlock::OpCodeClass::OpMemory_Load:  // For memory loads like LDQ, LDL
		// This would need to check specific opcode values as they don't all fall in 0x08-0x0F
		return (opcode == OPCODE_LDQ || opcode == OPCODE_LDL);

	default:
		return false;
	}
}
// In AlphaJITCompiler.cpp
void AlphaJITCompiler::updateHighFrequencyCache() {
	// Clear the current cache
	highFrequencyInstructions.clear();

	// Get top N instructions from profiler
	auto topInstructions = m_profiler->getTopInstructions(20); // Top 20 most frequent

	// Add them to our high-frequency set
	for (const auto& pair : topInstructions) {
		quint32 opcode = pair.first;
		quint32 function = pair.second;

		// Create composite key
		quint64 key = ((quint64)opcode << 32) | function;
		highFrequencyInstructions.insert(key);
	}

	qDebug() << "Updated high-frequency instruction cache with"
		<< highFrequencyInstructions.size() << "instructions";
}

JITBlock::Operation::OpType AlphaJITCompiler::mapInstructionToOpType(quint32 opcode, quint32 function)
{
	using OpType = JITBlock::Operation::OpType;



	// Integer arithmetic operations
	if (opcode == JITBlock::OpCodeClass::OpInteger_Operate) {
		switch (function) {
		case FUNC_ADDQ:  return OpType::INT_ADD;
		case FUNC_ADDL:  return OpType::INT_ADD;
		case FUNC_SUBQ:  return OpType::INT_SUB;
		case FUNC_SUBL:  return OpType::INT_SUB;
		case FUNC_MULQ:  return OpType::INT_MUL;
		case FUNC_MULL:  return OpType::INT_MUL;
		case FUNC_UMULH: return OpType::INT_UMULH;

			// Logical operations
		case FUNC_AND:   return OpType::INT_AND;
		case FUNC_BIC:   return OpType::INT_BIC;
		case FUNC_BIS:   return OpType::INT_BIS;
		case FUNC_ORNOT: return OpType::INT_ORNOT;
		case FUNC_XOR:   return OpType::INT_XOR;

			// Comparison operations
		case FUNC_CMPEQ:  return OpType::CMP_EQ;
		case FUNC_CMPLE:  return OpType::CMP_LE;
		case FUNC_CMPLT:  return OpType::CMP_LT;
		case FUNC_CMPULT: return OpType::CMP_ULT;

			// Conditional moves
		case FUNC_CMOVEQ: return OpType::CMOVE_EQ;
		case FUNC_CMOVNE: return OpType::CMOVE_NE;
		case FUNC_CMOVGT: return OpType::CMOVE_GT;
		default: break;  // Handle unmatched opcodes
		}
	}

	// Shift operations
	else if (opcode == JITBlock::OpCodeClass::OpIntegerShift) {
		switch (function) {
		case FUNC_SLL: return OpType::INT_SLL;
		case FUNC_SRL: return OpType::INT_SRL;
		case FUNC_SRA: return OpType::INT_SRA;
		default: break;  // Handle unmatched opcodes
		}
	}

	// Memory operations
	else if (opcode >= 0x08 && opcode <= 0x0F) {
		switch (opcode) {
		case OPCODE_LDA:  return OpType::MEM_LDA;
		case OPCODE_LDAH: return OpType::MEM_LDAH;		// Load Address High
		case OPCODE_LDBU: return OpType::MEM_LDBU;      // Load Zero-Extended Byte from Memory to Register
		case OPCODE_LDQ_U: return OpType::MEM_LDQ_U;	// Load Quadword from Memory to Register - added 5/9/2025
		case OPCODE_LDWU: return OpType::MEM_LDWU;		// Load Zero-Extended Word from Memory to Register
		case OPCODE_STB:  return OpType::MEM_STB;
		case OPCODE_STW:  return OpType::MEM_STW;
		default: break;  // Handle unmatched opcodes
		}
	}
	else if (opcode >= 0x28 && opcode <= 0x2F) {
		switch (opcode) {
		case OPCODE_LDL:  return OpType::MEM_LDL;		// Load Sign-Extended Longword from memory to register
		case OPCODE_LDL_L: return OpType::MEM_LDL_L;
		case OPCODE_LDQ:   return OpType::MEM_LDQ;
		case OPCODE_LDQ_L: return OpType::MEM_LDQ_L;
		case OPCODE_STL:   return OpType::MEM_STL;
		case OPCODE_STL_C: return OpType::MEM_STL_C;
		case OPCODE_STQ:   return OpType::MEM_STQ;
		case OPCODE_STQ_C: return OpType::MEM_STQ_C;
			// PREFETCH
		case OPCODE_PREFETCH_EN: return OpType::MEM_PREFETCH; // Same implementation
		default: break;  // Handle unmatched opcodes
		}
	}

	// Floating-point operations
	else if (opcode == JITBlock::OpCodeClass::OpFP_Operate) {
		switch (function) {
		case FUNC_ADDF: return OpType::FP_ADD;
		case FUNC_ADDD: return OpType::FP_ADD;
		case FUNC_ADDG: return OpType::FP_ADD;
		case FUNC_ADDT: return OpType::FP_ADD;
		case FUNC_SUBF: return OpType::FP_SUB;
		case FUNC_SUBD: return OpType::FP_SUB;
		case FUNC_SUBG: return OpType::FP_SUB;
		case FUNC_SUBT: return OpType::FP_SUB;
		case FUNC_MULF: return OpType::FP_MUL;
		case FUNC_MULD: return OpType::FP_MUL;
		case FUNC_MULG: return OpType::FP_MUL;
		case FUNC_MULT: return OpType::FP_MUL;
		case FUNC_DIVF: return OpType::FP_DIV;
		case FUNC_DIVD: return OpType::FP_DIV;
		case FUNC_DIVG: return OpType::FP_DIV;
		case FUNC_DIVT: return OpType::FP_DIV;

			// FP comparisons
		case FUNC_CMPTEQ: return OpType::FP_CMP_EQ;
		case FUNC_CMPTLT: return OpType::FP_CMP_LT;
		case FUNC_CMPTLE: return OpType::FP_CMP_LE;

			// FP converts
		case FUNC_CVTQF: return OpType::FP_CVT;
		case FUNC_CVTQG: return OpType::FP_CVT;
		case FUNC_CVTQT: return OpType::FP_CVT;
		default: break;  // Handle unmatched opcodes

		}
	}

	// Memory barriers
	else if (opcode == JITBlock::OpCodeClass::OpMemory_Barrier) {
		return OpType::SYS_MEMORY_BARRIER;
	}

	// PAL calls
	else if (opcode == OPCODE_PAL) {
		return OpType::SYS_CALL_PAL;
	}


	// Fall through to fallback for anything not explicitly handled
	return OpType::FALLBACK;
}

bool AlphaJITCompiler::isSpecialInstruction(quint32 instruction) {
	// Extract opcode (bits 31-26)
	quint32 opcode = (instruction >> 26) & 0x3F;

	if (opcode == OPCODE_PAL) {
		// Extract PAL function code (bits 25-0)
		quint32 palCode = instruction & 0x03FFFFFF;

		// Check if this is one of the special PAL operations
		return (palCode == PAL_TBIS || palCode == PAL_IMB ||
			palCode == FUNC_PAL_MFPR || palCode == FUNC_PAL_MTPR ||
			palCode == FUNC_PAL_REI);
	}
	else if (opcode == OPCODE_MISC) {
		// For memory barriers, check the function code (bits 15-0)
		quint32 function = instruction & 0xFFFF;
		return (function == FUNC_MB || function == FUNC_WMB);
	}

	return false;
}

// Special instruction handlers
void AlphaJITCompiler::handleMemoryBarrier(quint32 instruction) {
	// Extract function code (bits 15-0)
	quint32 function = instruction & 0xFFFF;

	switch (function) {
	case FUNC_MB:
		// Full memory barrier
		qDebug() << "MB: Executing full memory barrier";
		// Implementation would ensure all memory operations complete
		break;

	case FUNC_WMB:
		// Write memory barrier
		qDebug() << "WMB: Executing write memory barrier";
		// Implementation would ensure all writes complete
		break;

	default:
		qWarning() << "Unknown memory barrier function:" << function;
		break;
	}
}

void AlphaJITCompiler::handleMemoryBarrier(quint32 type)
{
	// For MB (full barrier), invalidate lock reservations
	if (type == FUNC_MB) {
		// Full memory barrier
		qDebug() << "MB: Executing full memory barrier";
		// Invalidate lock reservation
		m_lockValid = false;

		// Additional barrier operations...
	}
	// For WMB (write barrier), we don't invalidate lock reservations
	else if (type == FUNC_WMB) {
		// Write memory barrier
		qDebug() << "WMB: Executing write memory barrier";
		// WMB doesn't affect load reservations
	}
}

void AlphaJITCompiler::handlePALCall(quint32 instruction) {
	// Extract PAL function code (bits 25-0)
	quint32 palCode = instruction & 0x03FFFFFF;

	switch (palCode) {
	case PAL_TBIS:
		// Handle Translation Buffer Invalidate Single
	{
		quint64 va = m_registerFileWrapper->readIntReg(16); // Usually in R16
		qDebug() << "PAL_TBIS: Invalidating TLB entry for VA=" << QString::number(va, 16);
		// Implementation would interface with TLB subsystem
	}
	break;

	case PAL_IMB:
		// Handle I-Stream Memory Barrier
		qDebug() << "IMB: Executing instruction memory barrier";
		// Flush icache, etc.
		break;

	case FUNC_PAL_MFPR: // PAL_MFPR:
		// Move From Processor Register
	{
		quint64 prNum = m_registerFileWrapper->readIntReg(16); // PR# in R16
		quint64 value = readProcessorRegister(prNum);
		m_registerFileWrapper->writeIntReg(0, value); // Result to R0
	}
	break;

	case FUNC_PAL_MTPR: // PAL_MTPR:
		// Move To Processor Register
	{
		quint64 prNum = m_registerFileWrapper->readIntReg(16); // PR# in R16
		quint64 value = m_registerFileWrapper->readIntReg(17); // Value in R17
		writeProcessorRegister(prNum, value);
	}
	break;

	case FUNC_PAL_REI: // PAL_REI:
		// Return from Exception or Interrupt
		qDebug() << "REI: Returning from exception/interrupt";
		// Complex implementation to restore processor state
		break;

	default:
		qWarning() << "Unknown PAL code:" << palCode;
		break;
	}
}


// Add the handler method
void AlphaJITCompiler::handlePerformanceAlert(
	AlphaJITProfiler::PerformanceAlertType alertType, quint64 value) {

	QString alertMessage;

	switch (alertType) {
	case AlphaJITProfiler::PerformanceAlertType::InstructionCountExceeded:
		alertMessage = "Instruction count threshold exceeded";
		break;

	case AlphaJITProfiler::PerformanceAlertType::MemoryOperationsExceeded:
		alertMessage = "Memory operations threshold exceeded";
		break;

	case AlphaJITProfiler::PerformanceAlertType::BranchMispredictionsExceeded:
		alertMessage = "Branch misprediction threshold exceeded";
		break;

	case AlphaJITProfiler::PerformanceAlertType::CacheMissesExceeded:
		alertMessage = "Cache miss threshold exceeded";
		break;

	case AlphaJITProfiler::PerformanceAlertType::TLBMissesExceeded:
		alertMessage = "TLB miss threshold exceeded";
		break;

	case AlphaJITProfiler::PerformanceAlertType::CustomEventExceeded:
		alertMessage = "Custom event threshold exceeded";
		break;
	}

	qDebug() << "PERFORMANCE ALERT:" << alertMessage << "-" << value;

	// Forward the alert to any connected clients
	emit performanceAlertTriggered(alertMessage, value);
}
// Method to interpret a single instruction instead of a whole block
void AlphaJITCompiler::interpretInstruction(quint32 rawInstr, quint64 pc) {
	qDebug() << "Unhandled - Interpreting instruction at PC=" << QString::number(pc, 16);
	// Call into your existing interpreter for just this instruction
	// This allows for mixed-mode execution within a single block
}

void AlphaJITCompiler::handleTLBOperation(quint32 palCode) {
	switch (palCode) {
	case PAL_TBIS: {
		// TBIS - Invalidate single TLB entry
		// R16 contains the virtual address to invalidate
		quint64 virtualAddress = m_registerFileWrapper->readIntReg(16);
		qDebug() << "TBIS: Invalidating TLB entry for VA=" << QString::number(virtualAddress, 16);

		m_tlbSystem->invalidateEntry(virtualAddress);
		break;
	}

	case PAL_TBIA: {
		// TBIA - Invalidate all TLB entries
		qDebug() << "TBIA: Invalidating all TLB entries";

		m_tlbSystem->invalidateAll();
		break;
	}

	case PAL_TBIM: {
		// TBIM - Invalidate multiple TLB entries
		// R16 contains the Address Space Number (ASN)
		quint64 asn = m_registerFileWrapper->readIntReg(16);
		qDebug() << "TBIM: Invalidating TLB entries for ASN=" << asn;

		m_tlbSystem->invalidateByASN(asn);
		break;
	}

	case PAL_TB_FLUSH: {
		// TB_FLUSH - Flush translation buffer
		qDebug() << "TB_FLUSH: Flushing translation buffer";

		m_tlbSystem->flush();
		break;
	}

	case PAL_TB_FLUSH_ASM: {
		// TB_FLUSH_ASM - Flush TB for specific address space
		// R16 contains the Address Space Number (ASN)
		quint64 asn = m_registerFileWrapper->readIntReg(16);
		qDebug() << "TB_FLUSH_ASM: Flushing TLB for ASN=" << asn;

		m_tlbSystem->flushByASN(asn);
		break;
	}

	default:
		qWarning() << "Unknown TLB operation PAL code:" << palCode;
		break;
	}
}

void AlphaJITCompiler::handleFPException(helpers_JIT::Fault_TrapType trapType, quint64 pc) {
	qDebug() << "Handling FP exception type:" << (int)trapType << "at PC:" << QString::number(pc, 16);

	// Update exception registers
	m_exceptionAddress = pc;

	// Update the exception summary register based on the trap type
	switch (trapType) {
	case helpers_JIT::Fault_TrapType::DivideByZero_fp:
		m_exceptionSummary |= 0x01; // Set divide-by-zero bit
		break;

	case helpers_JIT::Fault_TrapType::OverFlow_fp:
		m_exceptionSummary |= 0x02; // Set overflow bit
		break;

	case helpers_JIT::Fault_TrapType::UnderFlow_fp:
		m_exceptionSummary |= 0x04; // Set underflow bit
		break;

	case helpers_JIT::Fault_TrapType::Inexact_fp:
		m_exceptionSummary |= 0x08; // Set inexact result bit
		break;

	case helpers_JIT::Fault_TrapType::Invalid_fp:
		m_exceptionSummary |= 0x10; // Set invalid operation bit
		break;

	default:
		m_exceptionSummary |= 0x80; // Set generic exception bit
		break;
	}

	// Check if we need to jump to exception handler based on the mask
	if (m_exceptionSummary & m_exceptionMask) {
		// Save current PC to return address register
		m_registerFileWrapper->writeIntReg(26, pc + 4); // R26 is typically used for return address

		// Jump to exception handler in PAL code
		currentPC = m_palBaseAddress + 0x100; // Offset to exception handler
	}
}

bool AlphaJITCompiler::canFuseUnalignedOperations(const JITBlock& block, int startIndex) {
	// First, ensure we have at least 4 operations
	if (startIndex + 3 >= block.operations.size()) {
		return false;
	}

	// Check for expected pattern: LDQ_U, LDQ_U, followed by extraction
	const auto& op1 = block.operations[startIndex];
	const auto& op2 = block.operations[startIndex + 1];
	const auto& op3 = block.operations[startIndex + 2];
	const auto& op4 = block.operations[startIndex + 3];

	// First two must be LDQ_U
	if (op1.type != JITBlock::Operation::OpType::MEM_LDQ_U ||
		op2.type != JITBlock::Operation::OpType::MEM_LDQ_U) {
		return false;
	}

	// Check if the addresses are 7 bytes apart (typical unaligned quadword pattern)
	// This is a heuristic and may need adjustment
	if (op1.ra != op2.ra || op2.immediate - op1.immediate != 7) {
		return false;
	}

	// Check for extraction operations
	// We're looking for EXTQL/EXTQH or EXTLL/EXTLH patterns
	// These would be in the FALLBACK or a specific BYTE_EXT* type
	if (op3.type != JITBlock::Operation::OpType::BYTE_EXTLL &&
		op3.type != JITBlock::Operation::OpType::BYTE_EXTQL &&
		op3.type != JITBlock::Operation::OpType::FALLBACK) {
		return false;
	}

	if (op4.type != JITBlock::Operation::OpType::BYTE_EXTLH &&
		op4.type != JITBlock::Operation::OpType::BYTE_EXTQH &&
		op4.type != JITBlock::Operation::OpType::FALLBACK) {
		return false;
	}

	// If we've made it this far, the pattern looks fusible
	return true;
}

bool AlphaJITCompiler::checkAlignment(quint64 address, int accessSize, quint32 opcode) const
{
	// Special cases for unaligned access instructions
	if (opcode == OPCODE_LDQ_U || opcode == OPCODE_STQ_U) {
		return true; // These don't generate alignment faults
	}

	// Byte operations like LDBU and STB never have alignment issues
	if (accessSize == 1) {
		return true; // Byte access is always aligned
	}
	// Standard alignment checks
	switch (accessSize) {
	case 2:
		return (address & 0x1) == 0; // Word alignment
	case 4:
		return (address & 0x3) == 0; // Longword alignment
	case 8:
		return (address & 0x7) == 0; // Quadword alignment
	default:
		return false; // Unknown size
	}
}

bool AlphaJITCompiler::checkLockReservation(quint64 address) const
{
	return m_lockValid && m_lockReservationAddr == address;
}

quint64 AlphaJITCompiler::readProcessorRegister(quint64 prNum) {
	// Handle different processor registers based on their numbers
	switch (prNum) {
	case PR_FPCR:    // Floating-point control register
		return m_registerFileWrapper->readFpcr().raw;

	case PR_ITBMISS: // ITB miss register
		return m_tlbSystem->getITBMissReg();

	case PR_DTBMISS: // DTB miss register
		return m_tlbSystem->getDTBMissReg();

	case PR_PERFCTR: // Performance counter
		return m_profiler->getPerformanceCounter();

	case PR_EXC_ADDR: // Exception address
		return m_exceptionAddress;

	case PR_EXC_SUM:  // Exception summary
		return m_exceptionSummary;

	case PR_EXC_MASK: // Exception mask
		return m_exceptionMask;

	case PR_PAL_BASE: // PAL base address
		return m_palBaseAddress;

	case PR_ICACHE_FLUSH_CTL: // Instruction cache flush control
		return 0; // Typically returns 0 on read

	case PR_CURRENT_MODE: // Current processor mode
		return m_currentProcessorMode;

	case PR_ASN:     // Address space number
		return m_tlbSystem->getCurrentASN();

	default:
		qWarning() << "Unknown processor register read:" << prNum;
		return 0;
	}
}

void AlphaJITCompiler::writeProcessorRegister(quint64 prNum, quint64 value) {
	switch (prNum) {
	case PR_FPCR:    // Floating-point control register
	{
		FpcrRegister fpcr;
		fpcr.raw = value;
		m_registerFileWrapper->writeFpcr(fpcr);
	}
	break;

	case PR_ITBMISS: // ITB miss register
		m_tlbSystem->setITBMissReg(value);
		break;

	case PR_DTBMISS: // DTB miss register
		m_tlbSystem->setDTBMissReg(value);
		break;

	case PR_PERFCTR: // Performance counter
		// Reset or configure performance counter
		m_profiler->configurePerformanceCounter(value);
		break;

	case PR_EXC_ADDR: // Exception address
		m_exceptionAddress = value;
		break;

	case PR_EXC_SUM:  // Exception summary
		m_exceptionSummary = value;
		break;

	case PR_EXC_MASK: // Exception mask
		m_exceptionMask = value;
		break;

	case PR_PAL_BASE: // PAL base address
		m_palBaseAddress = value;
		break;

	case PR_ICACHE_FLUSH_CTL: // Instruction cache flush control
		// Flush the instruction cache if the value is non-zero
		if (value != 0) {
			flushInstructionCache();
		}
		break;

	case PR_CURRENT_MODE: // Current processor mode
		m_currentProcessorMode = value;
		break;

	case PR_ASN:     // Address space number
		m_tlbSystem->setCurrentASN(value);
		break;

	default:
		qWarning() << "Unknown processor register write:" << prNum;
		break;
	}
}

void AlphaJITCompiler::flushInstructionCache() {
	qDebug() << "Flushing instruction cache";

	// Option 1: Invalidate specific address ranges based on control value
	// (For a complete flush, we'd invalidate everything)
	QMutexLocker locker(&cacheMutex);
	blockCache.clear();
	m_instructionCacheValid.clear();

	// Reset hit counters to force recompilation
	hitCounters.clear();

	// Reset branch predictor
	branchPredictor.clear();

	// Notify that cache was flushed
	emit instructionCacheFlushed();
}

void AlphaJITCompiler::invalidateInstructionCacheEntry(quint64 virtualAddress) {
	// Invalidate a specific cache entry
	QMutexLocker locker(&cacheMutex);

	// Remove from JIT block cache if present
	blockCache.remove(virtualAddress);

	// Mark as invalid in the tracking map
	m_instructionCacheValid[virtualAddress] = false;

	// Reset hit counter for this address
	hitCounters.remove(virtualAddress);

	// Reset branch prediction for this address
	branchPredictor.remove(virtualAddress);
}

void AlphaJITCompiler::invalidateLockReservationIfMatch(quint64 address)
{
	// In a multiprocessor system, check if the address matches the lock
	if (m_lockValid) {
		// Basic check: exact address match
		if (m_lockReservationAddr == address) {
			m_lockValid = false;
			return;
		}

		// More complex check: address falls within the same cache line
		// Alpha typically uses 64-byte cache lines
		constexpr quint64 CACHE_LINE_MASK = ~0x3F;
		if ((m_lockReservationAddr & CACHE_LINE_MASK) == (address & CACHE_LINE_MASK)) {
			m_lockValid = false;
		}
	}
}

// Check if an instruction is a memory format instruction
bool AlphaJITCompiler::isMemoryFormat(quint32 opcode) {
	// Memory format instructions typically have opcodes in these ranges
	return (opcode >= 0x08 && opcode <= 0x0F) || // Load format
		(opcode >= 0x20 && opcode <= 0x27) || // Float load format
		(opcode >= 0x28 && opcode <= 0x2F);   // Store format
}

// Check if an instruction is an operate format instruction
bool AlphaJITCompiler::isOperateFormat(quint32 opcode) {
	// Operate format instructions have opcodes in these ranges
	return (opcode >= 0x10 && opcode <= 0x13) || // Integer operate
		(opcode >= 0x16 && opcode <= 0x17);   // Float operate
}

// Check if an instruction is a branch format instruction
bool AlphaJITCompiler::isBranchFormat(quint32 opcode) {
	// Branch format instructions have opcodes in this range
	return (opcode >= 0x30 && opcode <= 0x3F);
}

// Check if an instruction is a control transfer instruction
bool AlphaJITCompiler::isControlTransfer(quint32 opcode, quint32 function) {
	// Branch instructions
	if (opcode >= 0x30 && opcode <= 0x3F) {
		return true;
	}

	// Jump instructions
	if (opcode == 0x1A) {
		return true;
	}

	// PAL calls
	if (opcode == 0x00) {
		return true;
	}

	// RET and similar in the operate format
	if (opcode == 0x1A && (function == 0x00 || function == 0x01)) {
		return true;
	}

	return false;
}

// Setup a special operation handler
void AlphaJITCompiler::setupSpecialOperation(JITBlock::Operation& operation, quint32 opcode, quint32 function) {
	if (opcode == OPCODE_PAL) {
		operation.type = JITBlock::Operation::OpType::SYS_CALL_PAL;

		// TLB-related operations
		if (function == PAL_TBIS || function == PAL_TBIA ||
			function == PAL_TBIM || function == PAL_TB_FLUSH ||
			function == PAL_TB_FLUSH_ASM) {

			operation.type = JITBlock::Operation::OpType::SYS_TLB_OP;

			// Create a lambda to handle this TLB operation
			operation.specialHandler = [function, this](
				RegisterFileWrapper* regFile,
				RegisterFileWrapper* fpRegFile,
				SafeMemory* memory) {
					this->handleTLBOperation(function);
				};
		}
		// Memory barrier operations
		else if (function == FUNC_MB || function == FUNC_WMB) {
			operation.type = JITBlock::Operation::OpType::SYS_MEMORY_BARRIER;

			// Create a lambda to handle memory barrier
			operation.specialHandler = [function, this](
				RegisterFileWrapper* regFile,
				RegisterFileWrapper* fpRegFile,
				SafeMemory* memory) {
					this->handleMemoryBarrier(function);
				};
		}
		// Other PAL operations
		else {
			// Create a lambda to handle generic PAL call
			operation.specialHandler = [function, this](
				RegisterFileWrapper* regFile,
				RegisterFileWrapper* fpRegFile,
				SafeMemory* memory) {
					this->handleCallPAL(function);
				};
		}
	}
}

// Sign-extend helper functions
#pragma region Helper Functions

/**
 * @brief Helper function to sign-extend 16-bit values
 * @param value The 16-bit value to extend
 * @return Sign-extended 64-bit value
 */
qint64 AlphaJITCompiler::SEXT16(quint16 value) const {
	// Sign extend the 16-bit value to 64 bits
	if (value & 0x8000) {
		return static_cast<qint64>(value | 0xFFFFFFFFFFFF0000ULL);
	}
	return static_cast<qint64>(value);
}
bool AlphaJITCompiler::isExtractionOperation(const JITBlock::Operation& op) {
	// Check if this is a byte/word/longword/quadword extraction instruction

	// If we have specific operation types for extractions
	if (op.type == JITBlock::Operation::OpType::BYTE_EXTBL ||
		op.type == JITBlock::Operation::OpType::BYTE_EXTWL ||
		op.type == JITBlock::Operation::OpType::BYTE_EXTLL ||
		op.type == JITBlock::Operation::OpType::BYTE_EXTQL) {
		return true;
	}

	// If extraction is handled via FALLBACK, check the raw instruction
	if (op.type == JITBlock::Operation::OpType::FALLBACK) {
		quint32 opcode = (op.rawInstr >> 26) & 0x3F;
		quint32 function = (op.rawInstr >> 5) & 0x7F;

		// Check against known extraction function codes
		if (opcode == JITBlock::OpCodeClass::OpInteger_Operate) {
			return (function == FUNC_EXTBL || function == FUNC_EXTWL ||
				function == FUNC_EXTLL || function == FUNC_EXTQL ||
				function == FUNC_EXTBH || function == FUNC_EXTWH ||
				function == FUNC_EXTLH || function == FUNC_EXTQH);
		}
	}

	return false;
}

bool AlphaJITCompiler::isInsertOperation(const JITBlock::Operation& op) {
	// Similar to extraction but for insert operations (INSBL, INSWL, etc.)
	// These are used for unaligned stores

	if (op.type == JITBlock::Operation::OpType::FALLBACK) {
		quint32 opcode = (op.rawInstr >> 26) & 0x3F;
		quint32 function = (op.rawInstr >> 5) & 0x7F;

		if (opcode == JITBlock::OpCodeClass::OpInteger_Operate) {
			return (function == FUNC_INSBL || function == FUNC_INSWL ||
				function == FUNC_INSLL || function == FUNC_INSQL);
		}
	}

	return false;
}

bool AlphaJITCompiler::isMaskOperation(const JITBlock::Operation& op) {
	// Check for mask operations (MSKBL, MSKWL, etc.)
	// These are used for unaligned stores

	if (op.type == JITBlock::Operation::OpType::FALLBACK) {
		quint32 opcode = (op.rawInstr >> 26) & 0x3F;
		quint32 function = (op.rawInstr >> 5) & 0x7F;

		if (opcode == JITBlock::OpCodeClass::OpInteger_Operate) {
			return (function == FUNC_MSKBL || function == FUNC_MSKWL ||
				function == FUNC_MSKLL || function == FUNC_MSKQL);
		}
	}

	return false;
}

bool AlphaJITCompiler::isBitwiseOr(const JITBlock::Operation& op) {
	// Check if this is a BIS (bitwise OR) operation

	if (op.type == JITBlock::Operation::OpType::INT_BIS) {
		return true;
	}

	if (op.type == JITBlock::Operation::OpType::FALLBACK) {
		quint32 opcode = (op.rawInstr >> 26) & 0x3F;
		quint32 function = (op.rawInstr >> 5) & 0x7F;

		return (opcode == JITBlock::OpCodeClass::OpInteger_Operate &&
			function == FUNC_BIS);
	}

	return false;
}

int AlphaJITCompiler::determineStoreSize(const JITBlock::Operation& insertOp,
	const JITBlock::Operation& maskOp) {
	// Determine store size based on the insert and mask operations

	quint32 insertFunc = (insertOp.rawInstr >> 5) & 0x7F;
	quint32 maskFunc = (maskOp.rawInstr >> 5) & 0x7F;

	// Check the function codes to determine size
	if (insertFunc == FUNC_INSBL && maskFunc == FUNC_MSKBL) {
		return 1; // Byte store
	}
	if (insertFunc == FUNC_INSWL && maskFunc == FUNC_MSKWL) {
		return 2; // Word store
	}
	if (insertFunc == FUNC_INSLL && maskFunc == FUNC_MSKLL) {
		return 4; // Longword store
	}
	if (insertFunc == FUNC_INSQL && maskFunc == FUNC_MSKQL) {
		return 8; // Quadword store
	}

	return 0; // Unknown pattern
}



MemoryFaultInfo AlphaJITCompiler::executeLdqUnaligned(quint64 baseReg, qint16 displacement, quint8 destReg, quint64 pc, quint32 rawInstr)
{
	MemoryFaultInfo faultInfo;
	faultInfo.pc = pc;
    faultInfo.physicalAddress = rawInstr;
	faultInfo.isWrite = false;
	faultInfo.isExecute = false;
	faultInfo.accessSize = 8;  // Quadword (8 bytes)

	// Get base register value
	quint64 base = m_registerFileWrapper->readIntReg(baseReg);

	// Add sign-extended displacement
	quint64 virtualAddr = base + SEXT16(displacement);
	faultInfo.faultAddress = virtualAddr;

	// LDQ_U doesn't check for alignment - it adjusts the address instead
	// Clear bottom 3 bits to align to 8-byte boundary (LDQ_U behavior)
	quint64 alignedAddr = virtualAddr & ~0x7ULL;

	// First check: Address translation
	bool isKernelMode = (m_currentProcessorMode == 0);
	quint64 currentASN = m_tlbSystem->getCurrentASN();

	TranslationResult tlbResult =
		m_tlbSystem->translateAddress(alignedAddr, false, false, currentASN, isKernelMode);

	// Check for TLB exceptions
	if (tlbResult.tlbException != TLBException::NONE) {
		// Set DTB miss register for virtual address that caused fault
		m_tlbSystem->setDTBMissReg(alignedAddr);

		switch (tlbResult.tlbException) {
		case TLBException::INVALID_ENTRY:
			faultInfo.faultType = MemoryFaultType::TRANSLATION_NOT_VALID;
			return faultInfo;

		case TLBException::PROTECTION_FAULT:
			faultInfo.faultType = MemoryFaultType::ACCESS_VIOLATION;
			return faultInfo;

		case TLBException::ALIGNMENT_FAULT:
			faultInfo.faultType = MemoryFaultType::ALIGNMENT_FAULT;
			return faultInfo;
		}
	}

	// Store physical address in fault info
	faultInfo.physicalAddress = tlbResult.physicalAddress;

	// Check if it's an MMIO access
	if (m_mmioManager && m_mmioManager->isMMIOAddress(alignedAddr)) {
		try {
			// MMIO read
			quint64 value = m_mmioManager->readMMIO(alignedAddr, 8);
			m_registerFileWrapper->writeIntReg(destReg, value);
			return faultInfo; // Success - no fault
		}
		catch (const std::exception& e) {
			// MMIO access error
			faultInfo.faultType = MemoryFaultType::FAULT_ON_READ;
			qWarning() << "MMIO error during LDQ_U at VA="
				<< QString::number(alignedAddr, 16)
				<< " - " << e.what();
			return faultInfo;
		}
	}

	// Regular memory access
	try {
		// Load the quadword from the aligned address
		quint64 value = m_safeMemory->readUInt64(tlbResult.physicalAddress);
		m_registerFileWrapper->writeIntReg(destReg, value);
		return faultInfo; // Success - no fault
	}
	catch (const MemoryAccessException& e)
	{
		// Convert exception to fault info
		MemoryFaultInfo faultInfo;
		faultInfo.faultAddress = e.getAddress();
		faultInfo.faultType = e.getType();
		faultInfo.accessSize = e.getSize();
		faultInfo.pc = currentPC - 4;
		// Handle the fault
		handleMemoryFault(faultInfo);
		// Return to faulting instruction
		return faultInfo;
	}
}

// Then implement in AlphaJITCompiler.cpp
void AlphaJITCompiler::configurePerformanceCounter(quint64 value) {
	// Configure the performance counter based on the value written
	// Bits 0-3: Counter type
	// Bits 4-7: Control flags
	// Bits 8-63: Initial counter value or threshold

	m_performanceControlReg = value & 0xFF; // Save control bits

	// Check if we should reset the counter
	if (value & 0x10) { // Reset bit
		m_performanceCounter = (value >> 8) & 0xFFFFFFFFFFFFFF00; // Initial value
	}

	// Check if we should enable monitoring
	bool enableMonitoring = (value & 0x20) != 0; // Enable bit
	m_profiler->setMonitoringEnabled(enableMonitoring);

	// Configure which events to track
	int eventType = m_performanceControlReg & 0x7; // Lower 3 bits determine counter type
	m_profiler->configureEventTracking(eventType);
}

// Modification to our unaligned pattern detection function
bool AlphaJITCompiler::createUnalignedLoad(JITBlock& block, int startIndex, int size) {
	// Create a fused unaligned load operation replacing the sequence

	// Extract necessary information
	JITBlock::Operation& firstOp = block.operations[startIndex];
	JITBlock::Operation& secondOp = block.operations[startIndex + 1];

	// Determine destination register and offset
	quint8 baseReg = firstOp.ra;
	quint64 offset = firstOp.immediate;

	// Determine destination register (from the final BIS or extraction op)
	quint8 destReg = 0;
	if (startIndex + 4 < block.operations.size() &&
		isBitwiseOr(block.operations[startIndex + 4])) {
		// Destination comes from the BIS operation
		destReg = block.operations[startIndex + 4].rc;
	}
	else {
		// Destination comes from one of the extraction operations
		destReg = block.operations[startIndex + 3].rc;
	}

	// Create the fused operation
	JITBlock::Operation fusedOp;
	fusedOp.type = (size == 2) ? JITBlock::Operation::OpType::MEM_UNALIGNED_LOAD_WORD :
		(size == 4) ? JITBlock::Operation::OpType::MEM_UNALIGNED_LOAD_LONGWORD :
		JITBlock::Operation::OpType::MEM_UNALIGNED_LOAD_QUADWORD;

	fusedOp.ra = baseReg;
	fusedOp.rc = destReg;
	fusedOp.immediate = offset;
	fusedOp.rawInstr = firstOp.rawInstr; // For fault reporting

	// Use a captured lambda to pass the context
	quint64 currentPC = /* get the current PC */;
	fusedOp.specialHandler = [this, baseReg, destReg, offset, size, currentPC](
		RegisterFileWrapper* regFile,
		RegisterFileWrapper* fpRegFile,
		SafeMemory* memory) {
			this->handleUnalignedLoadWithContext(
				regFile, memory, baseReg, destReg, offset, size, currentPC);
		};

	// Replace sequence with our fused operation
	block.operations[startIndex] = fusedOp;

	// Mark subsequent operations as NOPs
	int numOps = (size == 8 && startIndex + 4 < block.operations.size() &&
		isBitwiseOr(block.operations[startIndex + 4])) ? 5 : 4;

	for (int i = 1; i < numOps; i++) {
		if (startIndex + i < block.operations.size()) {
			block.operations[startIndex + i].type = JITBlock::Operation::OpType::NOP;
		}
	}

	block.containsSpecialOps = true;
	return true;
}

bool AlphaJITCompiler::createUnalignedStore(JITBlock& block, int startIndex, int size) {
	// Create a fused unaligned store operation

	// Extract necessary information
	JITBlock::Operation& loadOp = block.operations[startIndex];
	JITBlock::Operation& storeOp = block.operations[startIndex + 3];

	quint8 baseReg = loadOp.ra;
	quint64 offset = loadOp.immediate;

	// Determine value register (the one being stored)
	// This typically comes from the insert operation
	quint8 valueReg = block.operations[startIndex + 1].ra;

	// Create the fused operation
	JITBlock::Operation fusedOp;
	fusedOp.type = (size == 2) ? JITBlock::Operation::OpType::MEM_UNALIGNED_STORE_WORD :
		(size == 4) ? JITBlock::Operation::OpType::MEM_UNALIGNED_STORE_LONGWORD :
		JITBlock::Operation::OpType::MEM_UNALIGNED_STORE_QUADWORD;

	fusedOp.ra = baseReg;
	fusedOp.rc = valueReg;
	fusedOp.immediate = offset;
	fusedOp.rawInstr = storeOp.rawInstr; // For fault reporting

	// Use a captured lambda to pass the context
	quint64 currentPC = /* get the current PC */;
	fusedOp.specialHandler = [this, baseReg, valueReg, offset, size, currentPC](
		RegisterFileWrapper* regFile,
		RegisterFileWrapper* fpRegFile,
		SafeMemory* memory) {
			this->handleUnalignedStoreWithContext(
				regFile, memory, baseReg, valueReg, offset, size, currentPC);
		};

	// Replace sequence with our fused operation
	block.operations[startIndex] = fusedOp;

	// Mark subsequent ops as NOPs (store sequences vary in length)
	for (int i = 1; i <= 3; i++) {
		if (startIndex + i < block.operations.size()) {
			block.operations[startIndex + i].type = JITBlock::Operation::OpType::NOP;
		}
	}

	block.containsSpecialOps = true;
	return true;
}

quint64 AlphaJITCompiler::getPerformanceCounter() {
	// Return the current performance counter value
	// This could be cycle count, instruction count, or other metric
	// depending on the configuration in m_performanceControlReg

	switch (m_performanceControlReg & 0x7) { // Lower 3 bits determine counter type
	case 0: // Cycle counter
		return m_performanceCounter; // Simple cycle count

	case 1: // Instruction count
		return m_profiler->getTotalInstructionCount();

	case 2: // Memory operation count
		return m_profiler->getMemoryOperationCount();

	case 3: // Branch instruction count
		return m_profiler->.getBranchInstructionCount();

	case 4: // Branch misprediction count
		return m_profiler->getBranchMispredictionCount();

	case 5: // Cache miss count
		return m_profiler->getCacheMissCount();

	case 6: // TLB miss count
		return m_tlbSystem->getTlbMissCount();

	case 7: // Custom event count
		return m_profiler->getCustomEventCount();

	default:
		return m_performanceCounter;
	}
}
// Check if address is in kernel space (typically high addresses on Alpha)
bool AlphaJITCompiler::isInKernelSpace(quint64 virtualAddr) const {
	// Alpha kernel addresses are typically in the high half of the address space
	return (virtualAddr & (1ULL << 63)) != 0;
}

// Check if access would cross page boundaries
bool AlphaJITCompiler::isPhysicallyContiguous(quint64 virtualAddr, quint32 size) const {
	quint64 endAddr = virtualAddr + size - 1;
	// Check if start and end address are on the same page
	return (virtualAddr & ~(TLBSystem::PAGE_SIZE - 1)) == (endAddr & ~(TLBSystem::PAGE_SIZE - 1));
}

// Get direct pointer to a physical memory location (use with caution!)
quint8* AlphaJITCompiler::getPhysicalPointer(quint64 virtualAddr) {

	bool isKernelMode = (m_currentProcessorMode == 0);
	quint64 currentASN = m_tlbSystem->getCurrentASN();

	// Translate the address (assuming no faults)
	TranslationResult tlbResult =
		m_tlbSystem->translateAddress(virtualAddr, false, false, currentASN, isKernelMode);

	// If translation succeeded, get a pointer to physical memory
	if (tlbResult.tlbException == TLBException::NONE) {
		// This assumes SafeMemory provides a method to get direct pointer to a physical address
		return m_safeMemory->getPhysicalPointer(tlbResult.physicalAddress);
	}

	return nullptr;
}


// Converts exceptions between TLBSystem and AlphaJITCompiler
MemoryFaultType AlphaJITCompiler::mapTLBExceptionToMemoryFaultType(TLBException exception) {
	switch (exception) {
	case TLBException::INVALID_ENTRY:
		return MemoryFaultType::TRANSLATION_NOT_VALID;
	case TLBException::PROTECTION_FAULT:
		return MemoryFaultType::ACCESS_VIOLATION;
	case TLBException::ALIGNMENT_FAULT:
		return MemoryFaultType::ALIGNMENT_FAULT;
	default:
		return MemoryFaultType::NONE;
	}
}

// Replace the existing executeLdqUnaligned with this implementation
// Code to facilitate common LDQ_U + byte extraction patterns:
void AlphaJITCompiler::optimizeUnalignedAccess(JITBlock& block, int startIndex) {
	// Look for LDQ_U pair followed by extraction instructions
	if (startIndex + 3 < block.operations.size() &&
		block.operations[startIndex].type == JITBlock::Operation::OpType::MEM_LDQ_U &&
		block.operations[startIndex + 1].type == JITBlock::Operation::OpType::MEM_LDQ_U) {

		// Mark this as a special unaligned access pattern
		// Potentially combine into a single optimized operation
		block.containsSpecialOps = true;

		// Fuse operations if appropriate
		if (canFuseUnalignedOperations(block, startIndex)) {
			JITBlock::Operation fusedOp;
			fusedOp.type = JITBlock::Operation::OpType::MEM_UNALIGNED_ACCESS;
			fusedOp.ra = block.operations[startIndex].ra;  // Base register
			fusedOp.immediate = block.operations[startIndex].immediate;  // Offset

			// Add specialized handler
			fusedOp.specialHandler = [this](
				RegisterFileWrapper* regFile,
				RegisterFileWrapper* fpRegFile,
				SafeMemory* memory) {
					this->handleUnalignedAccess(regFile, memory);
				};

			// Replace the sequence with our fused operation
			block.operations[startIndex] = fusedOp;
			// Mark subsequent ops as NOPs
			for (int i = 1; i < 4; i++) {
				block.operations[startIndex + i].type = JITBlock::Operation::OpType::NOP;
			}
		}
	}
}

MemoryFaultInfo AlphaJITCompiler::performMemoryAccess(
	quint64 virtualAddress,
	void* valuePtr,
	int accessSize,
	bool isWrite,
	bool isExec,
	quint64 pc,
	quint32 rawInstr)
{
	MemoryFaultInfo faultInfo;
	faultInfo.faultAddress = virtualAddress;
	faultInfo.pc = pc;
	faultInfo.instruction = rawInstr;
	faultInfo.isWrite = isWrite;
	faultInfo.isExecute = isExec;
	faultInfo.accessSize = accessSize;

	// Check alignment based on access size and opcode
	quint32 opcode = (rawInstr >> 26) & 0x3F;
	if (!checkAlignment(virtualAddress, accessSize, opcode)) {
		faultInfo.faultType = MemoryFaultType::ALIGNMENT_FAULT;
		return faultInfo;
	}

	// Perform address translation
	bool isKernelMode = (m_currentProcessorMode == 0);
	quint64 currentASN = m_tlbSystem->getCurrentASN();

	TranslationResult tlbResult =
		m_tlbSystem->translateAddress(virtualAddress, isWrite, isExec, currentASN, isKernelMode, accessSize);

	// Handle translation exceptions by mapping to MemoryFaultType
	if (tlbResult.tlbException != TLBException::NONE) {
		// Set appropriate miss register
		if (isExec) {
			m_tlbSystem->setITBMissReg(virtualAddress);
		}
		else {
			m_tlbSystem->setDTBMissReg(virtualAddress);
		}

		// Map TLB exceptions to our fault types
		faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
		return faultInfo;
	}

	// Store translated physical address
	faultInfo.physicalAddress = tlbResult.physicalAddress;

	// Check if this is an MMIO access
	if (m_mmioManager && m_mmioManager->isMMIOAddress(virtualAddress)) {
		try {
			if (isWrite) {
				// Write to MMIO device
				quint64 valueToWrite = 0;

				// Copy from the provided buffer based on size
				switch (accessSize) {
				case 1:
					valueToWrite = *static_cast<quint8*>(valuePtr);
					break;
				case 2:
					valueToWrite = *static_cast<quint16*>(valuePtr);
					break;
				case 4:
					valueToWrite = *static_cast<quint32*>(valuePtr);
					break;
				case 8:
					valueToWrite = *static_cast<quint64*>(valuePtr);
					break;
				}

				m_mmioManager->writeMMIO(virtualAddress, valueToWrite, accessSize);
			}
			else {
				// Read from MMIO device
				quint64 mmioValue = m_mmioManager->readMMIO(virtualAddress, accessSize);

				// Copy to output based on size
				switch (accessSize) {
				case 1:
					*static_cast<quint8*>(valuePtr) = static_cast<quint8>(mmioValue);
					break;
				case 2:
					*static_cast<quint16*>(valuePtr) = static_cast<quint16>(mmioValue);
					break;
				case 4:
					*static_cast<quint32*>(valuePtr) = static_cast<quint32>(mmioValue);
					break;
				case 8:
					*static_cast<quint64*>(valuePtr) = mmioValue;
					break;
				}
			}

			// Success
			return faultInfo;
		}
		catch (const std::exception& e) {
			// MMIO device reported an error
			faultInfo.faultType = isWrite ?
				MemoryFaultType::FAULT_ON_WRITE :
				MemoryFaultType::FAULT_ON_READ;

			qWarning() << "MMIO error at VA=" << QString::number(virtualAddress, 16)
				<< " - " << e.what();
			return faultInfo;
		}
	}

	// Regular memory access - THIS IS THE PART YOU'RE ASKING ABOUT
	try {
		if (isWrite) {
			// Write operations
			switch (accessSize) {
			case 1:
				m_safeMemory->writeUInt8(tlbResult.physicalAddress, *static_cast<quint8*>(valuePtr));
				break;
			case 2:
				m_safeMemory->writeUInt16(tlbResult.physicalAddress, *static_cast<quint16*>(valuePtr));
				break;
			case 4:
				m_safeMemory->writeUInt32(tlbResult.physicalAddress, *static_cast<quint32*>(valuePtr));
				break;
			case 8:
				m_safeMemory->writeUInt64(tlbResult.physicalAddress, *static_cast<quint64*>(valuePtr));
				break;
			}
		}
		else {
			// Read operations
			switch (accessSize) {
			case 1:
				*static_cast<quint8*>(valuePtr) = m_safeMemory->readUInt8(tlbResult.physicalAddress);
				break;
			case 2:
				*static_cast<quint16*>(valuePtr) = m_safeMemory->readUInt16(tlbResult.physicalAddress);
				break;
			case 4:
				*static_cast<quint32*>(valuePtr) = m_safeMemory->readUInt32(tlbResult.physicalAddress);
				break;
			case 8:
				*static_cast<quint64*>(valuePtr) = m_safeMemory->readUInt64(tlbResult.physicalAddress);
				break;
			}
		}

		// Success - no fault
		return faultInfo;
	}
	catch (const MemoryAccessException& e) {
		// Convert exception to fault info
		faultInfo.faultAddress = e.getAddress();
		faultInfo.faultType = e.getType();
		faultInfo.accessSize = e.getSize();
		return faultInfo;
	}
	catch (const std::exception& e) {
		// Hardware error during memory access
		faultInfo.faultType = isWrite ?
			MemoryFaultType::FAULT_ON_WRITE :
			MemoryFaultType::FAULT_ON_READ;

		qWarning() << "Memory hardware error at VA=" << QString::number(virtualAddress, 16)
			<< " PA=" << QString::number(tlbResult.physicalAddress, 16)
			<< " - " << e.what();
		return faultInfo;
	}
}

void AlphaJITCompiler::handleExternalMemoryWrite(quint64 physicalAddress)
{
	// Get all CPUs in the system
	auto allCpus = m_smpManagerPtr->getAllCpus();

	// Convert physical address to virtual (may require TLB reverse lookup)
	quint64 virtualAddress = m_tlbSystem->getVirtualAddressFromPhysical(physicalAddress);

	// Notify all other CPUs (not including this one)
	for (auto cpu : allCpus) {
		if (cpu->getCpuId() != this->m_cpuId) {
			// Cast to our CPU type
			AlphaJITCompiler* otherCpu = static_cast<AlphaJITCompiler*>(cpu);
			// Invalidate the other CPU's lock reservation if it matches
			otherCpu->invalidateLockReservationIfMatch(virtualAddress);
		}
	}
}

#pragma endregion Helper Functions

#pragma region Error Handlers 

void AlphaJITCompiler::handleUnalignedLoadWithContext(
	RegisterFileWrapper* regFile,
	SafeMemory* memory,
	quint8 baseReg,
	quint8 destReg,
	quint64 offset,
	int size,
	quint64 pc) {

	// Get base address
	quint64 baseAddr = regFile->readIntReg(baseReg);
	quint64 addr = baseAddr + offset;

	// Calculate the two quadwords we need to access
	quint64 lowAddr = addr & ~0x7ULL;
	quint64 highAddr = (addr + size - 1) & ~0x7ULL;

	// Determine if we need one or two quadwords
	bool needTwoQuadwords = (lowAddr != highAddr);

	// Set up for fault handling
	bool faultOccurred = false;
	MemoryFaultInfo faultInfo;
	faultInfo.pc = pc;
	faultInfo.instruction = 0; // We don't have the raw instruction here
	faultInfo.isWrite = false;
	faultInfo.isExecute = false;
	faultInfo.accessSize = size;
	faultInfo.faultAddress = addr;

	// Perform the load (with fault handling)
	quint64 lowQword = 0;
	quint64 highQword = 0;

	// First quadword
	try {
		// Translate address
		bool isKernelMode = (m_currentProcessorMode == 0);
		quint64 currentASN = m_tlbSystem->getCurrentASN();

		TranslationResult tlbResult =
			m_tlbSystem->translateAddress(lowAddr, false, false, currentASN, isKernelMode);

		if (tlbResult.tlbException != TLBException::NONE) {
			// Handle translation fault
			faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}

		// Read memory
		lowQword = memory->readUInt64(tlbResult.physicalAddress);
	}
	catch (const MemoryAccessException& e) {
		// Handle memory exception
		faultInfo.faultType = e.getType();
		faultOccurred = true;
		handleMemoryFault(faultInfo);
		return;
	}

	// Second quadword (if needed)
	if (needTwoQuadwords && !faultOccurred) {
		try {
			// Translate address
			bool isKernelMode = (m_currentProcessorMode == 0);
			quint64 currentASN = m_tlbSystem->getCurrentASN();

			TranslationResult tlbResult =
				m_tlbSystem->translateAddress(highAddr, false, false, currentASN, isKernelMode);

			if (tlbResult.tlbException != TLBException::NONE) {
				// Handle translation fault
				faultInfo.faultAddress = highAddr;
				faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
				faultOccurred = true;
				handleMemoryFault(faultInfo);
				return;
			}

			// Read memory
			highQword = memory->readUInt64(tlbResult.physicalAddress);
		}
		catch (const MemoryAccessException& e) {
			// Handle memory exception
			faultInfo.faultAddress = highAddr;
			faultInfo.faultType = e.getType();
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}
	}

	// If no fault occurred, extract and combine the value
	if (!faultOccurred) {
		quint64 result = 0;
		int byteOffset = addr & 0x7;

		if (needTwoQuadwords) {
			// Calculate shifts based on size and offset
			int lowShift = byteOffset * 8;
			int highShift = (8 - byteOffset) * 8;

			// Extract relevant parts
			quint64 lowPart = lowQword >> lowShift;
			quint64 highPart = 0;

			// Apply appropriate mask based on size
			if (size == 2) {
				// 16-bit unaligned access
				lowPart &= 0xFFFF >> (byteOffset * 8);
				highPart = (highQword << highShift) & 0xFFFF;
			}
			else if (size == 4) {
				// 32-bit unaligned access
				lowPart &= 0xFFFFFFFF >> (byteOffset * 8);
				highPart = (highQword << highShift) & 0xFFFFFFFF;
			}
			else {
				// 64-bit unaligned access
				lowPart &= ~0ULL >> (byteOffset * 8);
				highPart = highQword << highShift;
			}

			// Combine parts
			result = lowPart | highPart;
		}
		else {
			// Single quadword access with appropriate shift and mask
			result = (lowQword >> (byteOffset * 8));

			// Apply size mask if needed
			if (size == 2) {
				result &= 0xFFFF;
			}
			else if (size == 4) {
				result &= 0xFFFFFFFF;
			}
		}

		// Sign extend for longword if needed
		if (size == 4) {
			result = static_cast<quint64>(static_cast<qint64>(static_cast<qint32>(result)));
		}

		// Store result
		regFile->writeIntReg(destReg, result);
	}
}

void AlphaJITCompiler::handleUnalignedStoreWithContext(
	RegisterFileWrapper* regFile,
	SafeMemory* memory,
	quint8 baseReg,
	quint8 valueReg,
	quint64 offset,
	int size,
	quint64 pc) {

	// Get base address and value to store
	quint64 baseAddr = regFile->readIntReg(baseReg);
	quint64 addr = baseAddr + offset;
	quint64 value = regFile->readIntReg(valueReg);

	// Calculate the two quadwords we need to access
	quint64 lowAddr = addr & ~0x7ULL;
	quint64 highAddr = (addr + size - 1) & ~0x7ULL;

	// Determine if we need one or two quadwords
	bool needTwoQuadwords = (lowAddr != highAddr);

	// Set up for fault handling
	bool faultOccurred = false;
	MemoryFaultInfo faultInfo;
	faultInfo.pc = pc;
	faultInfo.instruction = 0; // We don't have the raw instruction here
	faultInfo.isWrite = true;
	faultInfo.isExecute = false;
	faultInfo.accessSize = size;
	faultInfo.faultAddress = addr;

	// Prepare the read-modify-write operation
	quint64 lowQword = 0;
	quint64 highQword = 0;

	// First, read the low quadword
	try {
		// Translate address
		bool isKernelMode = (m_currentProcessorMode == 0);
		quint64 currentASN = m_tlbSystem->getCurrentASN();

		TranslationResult tlbResult =
			m_tlbSystem->translateAddress(lowAddr, true, false, currentASN, isKernelMode);

		if (tlbResult.tlbException != TLBException::NONE) {
			// Handle translation fault
			faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}

		// Read current value
		lowQword = memory->readUInt64(tlbResult.physicalAddress);

		// Calculate byte offset and prepare masks
		int byteOffset = addr & 0x7;
		quint64 valueMask = 0;

		if (size == 1) {
			valueMask = 0xFF;
		}
		else if (size == 2) {
			valueMask = 0xFFFF;
		}
		else if (size == 4) {
			valueMask = 0xFFFFFFFF;
		}
		else {
			valueMask = ~0ULL;
		}

		// Create mask for the bytes we'll modify
		quint64 lowMask = ~(valueMask << (byteOffset * 8));

		// Update low quadword (keep unmodified bytes, insert new bytes)
		quint64 updatedLowQword = (lowQword & lowMask) |
			((value & valueMask) << (byteOffset * 8));

		// Write back
		memory->writeUInt64(tlbResult.physicalAddress, updatedLowQword);
	}
	catch (const MemoryAccessException& e) {
		// Handle memory exception
		faultInfo.faultType = e.getType();
		faultOccurred = true;
		handleMemoryFault(faultInfo);
		return;
	}

	// If we need to update high quadword and no fault occurred
	if (needTwoQuadwords && !faultOccurred) {
		try {
			// Translate address
			bool isKernelMode = (m_currentProcessorMode == 0);
			quint64 currentASN = m_tlbSystem->getCurrentASN();

                TranslationResult tlbResult =
				m_tlbSystem->translateAddress(highAddr, true, false, currentASN, isKernelMode);

			if (tlbResult.tlbException != TLBException::NONE) {
				// Handle translation fault
				faultInfo.faultAddress = highAddr;
				faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
				faultOccurred = true;
				handleMemoryFault(faultInfo);
				return;
			}

			// Read current value
			highQword = memory->readUInt64(tlbResult.physicalAddress);

			// Calculate shifts and masks
			int byteOffset = addr & 0x7;
			int highShift = (8 - byteOffset) * 8;
			quint64 valueMask = 0;

			if (size == 1) {
				valueMask = 0xFF;
			}
			else if (size == 2) {
				valueMask = 0xFFFF;
			}
			else if (size == 4) {
				valueMask = 0xFFFFFFFF;
			}
			else {
				valueMask = ~0ULL;
			}

			// Create mask for high quadword
			quint64 highMask = ~(valueMask >> highShift);

			// Update high quadword
			quint64 updatedHighQword = (highQword & highMask) |
				((value & valueMask) >> highShift);

			// Write back
			memory->writeUInt64(tlbResult.physicalAddress, updatedHighQword);
		}
		catch (const MemoryAccessException& e) {
			// Handle memory exception
			faultInfo.faultAddress = highAddr;
			faultInfo.faultType = e.getType();
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}
	}
}

void AlphaJITCompiler::handleUnalignedLoad(RegisterFileWrapper* regFile,
	SafeMemory* memory,
	int size) {
	// This function should be called by the specialHandler lambda
	// Implement unaligned memory load with proper fault handling

	// Extract context from the operation
	const JITBlock::Operation* op = /* current operation */;
	quint8 baseReg = op->unalignedContext.baseReg;
	quint8 destReg = op->unalignedContext.valueReg;
	quint64 offset = op->unalignedContext.offset;

	// Get base address
	quint64 baseAddr = regFile->readIntReg(baseReg);
	quint64 addr = baseAddr + offset;

	// Calculate the two quadwords we need to access
	quint64 lowAddr = addr & ~0x7ULL;
	quint64 highAddr = (addr + size - 1) & ~0x7ULL;

	// Determine if we need one or two quadwords
	bool needTwoQuadwords = (lowAddr != highAddr);

	// Set up for fault handling
	bool faultOccurred = false;
	MemoryFaultInfo faultInfo;
	faultInfo.pc = /* current PC */;
	faultInfo.instruction = op->rawInstr;
	faultInfo.isWrite = false;
	faultInfo.isExecute = false;
	faultInfo.accessSize = size;
	faultInfo.faultAddress = addr;

	// Perform the load (with fault handling)
	quint64 lowQword = 0;
	quint64 highQword = 0;

	// First quadword
	try {
		// Translate address
		bool isKernelMode = (m_currentProcessorMode == 0);
		quint64 currentASN = m_tlbSystem->getCurrentASN();

		TranslationResult tlbResult =
			m_tlbSystem->translateAddress(lowAddr, false, false, currentASN, isKernelMode);

		if (tlbResult.tlbException != TLBException::NONE) {
			// Handle translation fault
			faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}

		// Read memory
		lowQword = memory->readUInt64(tlbResult.physicalAddress);
	}
	catch (const MemoryAccessException& e) {
		// Handle memory exception
		faultInfo.faultType = e.getType();
		faultOccurred = true;
		handleMemoryFault(faultInfo);
		return;
	}

	// Second quadword (if needed)
	if (needTwoQuadwords && !faultOccurred) {
		try {
			// Translate address
			bool isKernelMode = (m_currentProcessorMode == 0);
			quint64 currentASN = m_tlbSystem->getCurrentASN();

			TranslationResult tlbResult =
				m_tlbSystem->translateAddress(highAddr, false, false, currentASN, isKernelMode);

			if (tlbResult.tlbException != TLBException::NONE) {
				// Handle translation fault
				faultInfo.faultAddress = highAddr;
				faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
				faultOccurred = true;
				handleMemoryFault(faultInfo);
				return;
			}

			// Read memory
			highQword = memory->readUInt64(tlbResult.physicalAddress);
		}
		catch (const MemoryAccessException& e) {
			// Handle memory exception
			faultInfo.faultAddress = highAddr;
			faultInfo.faultType = e.getType();
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}
	}

	// If no fault occurred, extract and combine the value
	if (!faultOccurred) {
		quint64 result = 0;
		int byteOffset = addr & 0x7;

		if (needTwoQuadwords) {
			// Calculate shifts based on size and offset
			int lowShift = byteOffset * 8;
			int highShift = (8 - byteOffset) * 8;

			// Extract relevant parts
			quint64 lowPart = lowQword >> lowShift;
			quint64 highPart = 0;

			// Apply appropriate mask based on size
			if (size == 2) {
				// 16-bit unaligned access
				lowPart &= 0xFFFF >> (byteOffset * 8);
				highPart = (highQword << highShift) & 0xFFFF;
			}
			else if (size == 4) {
				// 32-bit unaligned access
				lowPart &= 0xFFFFFFFF >> (byteOffset * 8);
				highPart = (highQword << highShift) & 0xFFFFFFFF;
			}
			else {
				// 64-bit unaligned access
				lowPart &= ~0ULL >> (byteOffset * 8);
				highPart = highQword << highShift;
			}

			// Combine parts
			result = lowPart | highPart;
		}
		else {
			// Single quadword access with appropriate shift and mask
			result = (lowQword >> (byteOffset * 8));

			// Apply size mask if needed
			if (size == 2) {
				result &= 0xFFFF;
			}
			else if (size == 4) {
				result &= 0xFFFFFFFF;
			}
		}

		// Sign extend for longword if needed
		if (size == 4) {
			result = static_cast<quint64>(static_cast<qint64>(static_cast<qint32>(result)));
		}

		// Store result
		regFile->writeIntReg(destReg, result);
	}
}

void AlphaJITCompiler::handleUnalignedStore(RegisterFileWrapper* regFile,
	SafeMemory* memory,
	int size) {
	// Implement unaligned memory store with fault handling

	// Extract context from the operation
	const JITBlock::Operation* op = /* current operation */;
	quint8 baseReg = op->unalignedContext.baseReg;
	quint8 valueReg = op->unalignedContext.valueReg;
	quint64 offset = op->unalignedContext.offset;

	// Get base address and value to store
	quint64 baseAddr = regFile->readIntReg(baseReg);
	quint64 addr = baseAddr + offset;
	quint64 value = regFile->readIntReg(valueReg);

	// Calculate the two quadwords we need to access
	quint64 lowAddr = addr & ~0x7ULL;
	quint64 highAddr = (addr + size - 1) & ~0x7ULL;

	// Determine if we need one or two quadwords
	bool needTwoQuadwords = (lowAddr != highAddr);

	// Set up for fault handling
	bool faultOccurred = false;
	MemoryFaultInfo faultInfo;
	faultInfo.pc = /* current PC */;
	faultInfo.instruction = op->rawInstr;
	faultInfo.isWrite = true;
	faultInfo.isExecute = false;
	faultInfo.accessSize = size;
	faultInfo.faultAddress = addr;

	// Prepare the read-modify-write operation
	quint64 lowQword = 0;
	quint64 highQword = 0;

	// First, read the low quadword
	try {
		// Translate address
		bool isKernelMode = (m_currentProcessorMode == 0);
		quint64 currentASN = m_tlbSystem->getCurrentASN();

		TranslationResult tlbResult =
			m_tlbSystem->translateAddress(lowAddr, true, false, currentASN, isKernelMode);

		if (tlbResult.tlbException() != TLBException::NONE) {
			// Handle translation fault
			faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException());
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}

		// Read current value
		lowQword = memory->readUInt64(tlbResult.physicalAddress);

		// Calculate byte offset and prepare masks
		int byteOffset = addr & 0x7;
		quint64 valueMask = 0;

		if (size == 1) {
			valueMask = 0xFF;
		}
		else if (size == 2) {
			valueMask = 0xFFFF;
		}
		else if (size == 4) {
			valueMask = 0xFFFFFFFF;
		}
		else {
			valueMask = ~0ULL;
		}

		// Create mask for the bytes we'll modify
		quint64 lowMask = ~(valueMask << (byteOffset * 8));

		// Update low quadword (keep unmodified bytes, insert new bytes)
		quint64 updatedLowQword = (lowQword & lowMask) |
			((value & valueMask) << (byteOffset * 8));

		// Write back
		memory->writeUInt64(tlbResult.physicalAddress, updatedLowQword);
	}
	catch (const MemoryAccessException& e) {
		// Handle memory exception
		faultInfo.faultType = e.getType();
		faultOccurred = true;
		handleMemoryFault(faultInfo);
		return;
	}

	// If we need to update high quadword and no fault occurred
	if (needTwoQuadwords && !faultOccurred) {
		try {
			// Translate address
			bool isKernelMode = (m_currentProcessorMode == 0);
			quint64 currentASN = m_tlbSystem->getCurrentASN();

			TranslationResult tlbResult =
				m_tlbSystem->translateAddress(highAddr, true, false, currentASN, isKernelMode);

			if (tlbResult.tlbException != TLBException::NONE) {
				// Handle translation fault
				faultInfo.faultAddress = highAddr;
				faultInfo.faultType = mapTLBExceptionToMemoryFaultType(tlbResult.tlbException);
				faultOccurred = true;
				handleMemoryFault(faultInfo);
				return;
			}

			// Read current value
			highQword = memory->readUInt64(tlbResult.physicalAddress);

			// Calculate shifts and masks
			int byteOffset = addr & 0x7;
			int highShift = (8 - byteOffset) * 8;
			quint64 valueMask = 0;

			if (size == 1) {
				valueMask = 0xFF;
			}
			else if (size == 2) {
				valueMask = 0xFFFF;
			}
			else if (size == 4) {
				valueMask = 0xFFFFFFFF;
			}
			else {
				valueMask = ~0ULL;
			}

			// Create mask for high quadword
			quint64 highMask = ~(valueMask >> highShift);

			// Update high quadword
			quint64 updatedHighQword = (highQword & highMask) |
				((value & valueMask) >> highShift);

			// Write back
			memory->writeUInt64(tlbResult.physicalAddress, updatedHighQword);
		}
		catch (const MemoryAccessException& e) {
			// Handle memory exception
			faultInfo.faultAddress = highAddr;
			faultInfo.faultType = e.getType();
			faultOccurred = true;
			handleMemoryFault(faultInfo);
			return;
		}
	}
}

void AlphaJITCompiler::handleMemoryAccessViolation(quint64 virtualAddress, quint64 pc) {
	qDebug() << "Handling Memory Access Violation at VA=" << QString::number(virtualAddress, 16)
		<< " PC=" << QString::number(pc, 16);

	// Update exception registers
	m_exceptionAddress = virtualAddress;
	m_exceptionSummary |= 0x200; // Access violation bit

	// Check if this exception is enabled in the mask
	if (m_exceptionMask & 0x200) {
		// Save return PC for when we return from exception
		m_registerFileWrapper->writeIntReg(26, pc + 4);

		// Jump to the access violation handler in PAL code
		currentPC = m_palBaseAddress + 0x200; // Example offset for access violation handler
	}
}

void AlphaJITCompiler::handleMemoryFault(const MemoryFaultInfo& faultInfo) {
	// Get the appropriate exception bit for this fault type
	quint64 excSumBit = ExcSum::getExceptionBitForFault(faultInfo.faultType);
	if (excSumBit == 0) {
		qWarning() << "Unknown memory fault type:" << static_cast<int>(faultInfo.faultType);
		return;
	}

	// Map fault types to PAL handler offsets
	quint64 palHandlerOffset = 0;
	switch (faultInfo.faultType) {
	case MemoryFaultType::ACCESS_VIOLATION:
		palHandlerOffset = 0x100;
		qDebug() << "Access Violation at VA=" << QString::number(faultInfo.faultAddress, 16)
			<< "PC=" << QString::number(faultInfo.pc, 16);
		break;

	case MemoryFaultType::FAULT_ON_READ:
		palHandlerOffset = 0x200;
		qDebug() << "Fault on Read at VA=" << QString::number(faultInfo.faultAddress, 16)
			<< "PC=" << QString::number(faultInfo.pc, 16);
		break;

	case MemoryFaultType::TRANSLATION_NOT_VALID:
		palHandlerOffset = 0x300;
		qDebug() << "Translation Not Valid at VA=" << QString::number(faultInfo.faultAddress, 16)
			<< "PC=" << QString::number(faultInfo.pc, 16);
		break;

	case MemoryFaultType::ALIGNMENT_FAULT:
		palHandlerOffset = 0x400;
		qDebug() << "Alignment Fault at VA=" << QString::number(faultInfo.faultAddress, 16)
			<< "PC=" << QString::number(faultInfo.pc, 16);
		break;

	case MemoryFaultType::INSTRUCTION_ACCESS_FAULT:
		palHandlerOffset = 0x500;
		qDebug() << "Instruction Access Fault at VA=" << QString::number(faultInfo.faultAddress, 16)
			<< "PC=" << QString::number(faultInfo.pc, 16);
		break;

	default:
		qWarning() << "Unhandled memory fault type";
		return;
	}

	// Update exception registers
	m_exceptionAddress = faultInfo.faultAddress;

	// Update exception summary register using the ExcSum class
	ExcSum excSum(m_exceptionSummary);
	excSum.set(excSumBit);
	m_exceptionSummary = excSum.getValue();

	// Check if this exception type is enabled in the mask
	if ((m_exceptionMask & excSumBit) == 0) {
		qDebug() << "Exception masked, ignoring:" << QString::number(excSumBit, 16);
		return;
	}

	// Capture the current processor state
	quint64 currentPs = m_currentProcessorMode;

	// For floating-point operations, check the FPCR
	if (faultInfo.instruction & 0x11) {  // Simple check if this is a floating-point instruction
		FpcrRegister fpcr = m_registerFileWrapper->readFpcr();

		// Update FPCR status flags based on fault type
		if (faultInfo.faultType == MemoryFaultType::FAULT_ON_READ) {
			// For floating-point load faults, set appropriate FPCR status
			fpcr.raiseStatus_InvalidOp();
		}

		// Write back updated FPCR
		m_registerFileWrapper->writeFpcr(fpcr);
	}

	// Create an exception frame
	ExceptionFrame exFrame;
	exFrame.pc = faultInfo.pc;
	exFrame.ps = currentPs;
	exFrame.excSum = excSum.getValue();

	// Copy argument registers (R16-R21)
	for (int i = 0; i < 6; i++) {
		exFrame.r16_21[i] = m_registerFileWrapper->readIntReg(16 + i);
	}

	// Copy other important registers
	exFrame.ra = m_registerFileWrapper->readIntReg(26); // Return address
	exFrame.pv = m_registerFileWrapper->readIntReg(27); // Procedure value
	exFrame.sp = m_registerFileWrapper->readIntReg(30); // Stack pointer

	// Copy FPCR if floating-point enabled
	FpcrRegister fpcr = m_registerFileWrapper->readFpcr();
	exFrame.fpcr = fpcr.toRaw();

	// Push exception frame onto stack
	int frameIndex = m_stackManager.push(exFrame);
	if (frameIndex < 0) {
		qCritical() << "Failed to push exception frame - stack overflow!";
		// Emergency handling (maybe reset CPU)
		return;
	}

	// Enter PAL mode
	m_currentProcessorMode = 0;  // Kernel mode

	// Jump to the appropriate PAL handler
	currentPC = m_palBaseAddress + palHandlerOffset;

	qDebug() << "Jumped to PAL handler at" << QString::number(currentPC, 16)
		<< "for exception" << QString::number(excSumBit, 16);
}

void AlphaJITCompiler::handleMemoryReadFault(quint64 virtualAddress, quint64 pc) {
	qDebug() << "Handling Memory Read Fault at VA=" << QString::number(virtualAddress, 16)
		<< " PC=" << QString::number(pc, 16);

	// Update exception registers
	m_exceptionAddress = virtualAddress;
	m_exceptionSummary |= 0x400; // Fault on read bit

	// Check if this exception is enabled in the mask
	if (m_exceptionMask & 0x400) {
		// Save return PC for when we return from exception
		m_registerFileWrapper->writeIntReg(26, pc + 4);

		// Jump to the read fault handler in PAL code
		currentPC = m_palBaseAddress + 0x400; // Example offset for read fault handler
	}
}
/*
Integration with TLB and Memory Management
The implementation integrates with the TLB system by:

- Checking for translation validity using the TLB system
- Verifying access permissions
- Using the translated physical address for the actual memory access
- Handling each type of fault separately
- Setting appropriate exception registers
- Potentially calling into the PAL code for exception handling

*/
void AlphaJITCompiler::handleTranslationFault(quint64 virtualAddress, quint64 pc) {
	qDebug() << "Handling Translation Fault at VA=" << QString::number(virtualAddress, 16)
		<< " PC=" << QString::number(pc, 16);

	// Update exception registers
	m_exceptionAddress = virtualAddress;
	m_exceptionSummary |= 0x100; // Translation not valid bit

	// Set DTB Miss register
	m_tlbSystem->setDTBMissReg(virtualAddress);

	// Check if this exception is enabled in the mask
	if (m_exceptionMask & 0x100) {
		// Save return PC for when we return from exception
		m_registerFileWrapper->writeIntReg(26, pc + 4);

		// Jump to the translation fault handler in PAL code
		currentPC = m_palBaseAddress + 0x100; // Example offset for translation fault handler
	}
}

void AlphaJITCompiler::handleUnalignedAccess(RegisterFileWrapper* regFile, SafeMemory* memory) {
	// This function implements a fused unaligned quadword load
	// It would be called by the specialHandler lambda in the fused operation

	// Retrieve the context from the fused operation (would need to store these)
	quint64 baseReg = /* saved base register */;
	quint64 offset = /* saved offset */;
	quint8 dest1 = /* saved first destination register */;
	quint8 dest2 = /* saved second destination register */;
	quint8 finalDest = /* saved final destination register */;

	// Get the base address
	quint64 baseAddr = regFile->readIntReg(baseReg);
	quint64 addr = baseAddr + offset;

	// Calculate addresses for the two quadwords
	quint64 lowAddr = addr & ~0x7ULL;           // Aligned down (first LDQ_U)
	quint64 highAddr = (addr + 7) & ~0x7ULL;    // Next quadword (second LDQ_U)

	// Perform the loads (simplified - real code would need TLB, faults, etc.)
	quint64 lowQword = memory->readUInt64(lowAddr);
	quint64 highQword = memory->readUInt64(highAddr);

	// Extract and combine the bytes based on the unaligned address
	int byteOffset = addr & 0x7;
	quint64 lowMask = ~0ULL >> (byteOffset * 8);
	quint64 highMask = ~0ULL << ((8 - byteOffset) * 8);

	// Shift and mask to extract relevant bytes
	quint64 lowPart = (lowQword >> (byteOffset * 8));
	quint64 highPart = (highQword << ((8 - byteOffset) * 8));

	// Combine to form final unaligned quadword
	quint64 result = lowPart | highPart;

	// Store the result in the destination register
	regFile->writeIntReg(finalDest, result);
}

#pragma endregion Error Handlers 


// PAL 
// 
#pragma region PAL Structures

const std::vector<AlphaJITCompiler::PalEntryPoint> AlphaJITCompiler::PAL_ENTRY_POINTS = {
	{ 0x100, 0x0001, "AccessViolation" },
	{ 0x200, 0x0002, "FaultOnRead" },
	{ 0x300, 0x0004, "TranslationNotValid" },
	{ 0x400, 0x0008, "AlignmentFault" },
	// Add other entry points as needed
};
#pragma endregion PAL Structures

// Memory Integer Implementations
void AlphaJITCompiler::ExecuteStq(const JITBlock::Operation& op, quint64 currentPC) {
	quint64 virtualAddr = m_registerFileWrapper->readIntReg(op.ra) + op.immediate;
	quint64 value = m_registerFileWrapper->readIntReg(op.rc);

	DEBUG_LOG(QString("ExecuteStq: VA=0x%1, value=0x%2, src=R%3")
		.arg(QString::number(virtualAddr, 16))
		.arg(QString::number(value, 16))
		.arg(op.rc));

	// Check alignment for STQ operations
	if (virtualAddr & 0x7) {
		WARN_LOG(QString("STQ alignment check: Address 0x%1 is not 8-byte aligned")
			.arg(QString::number(virtualAddr, 16)));
	}

	// Perform the memory access with full fault handling
	MemoryFaultInfo faultInfo = PerformMemoryAccess(
		virtualAddr,       // Virtual address
		&value,            // Value to write
		8,                 // Quadword size (8 bytes)
		true,              // It's a write
		false,             // Not an instruction fetch
		currentPC - 4,     // PC of the instruction
		op.rawInstr        // Raw instruction for fault reporting
	);

	// If fault occurred, throw exception
	if (faultInfo.faultType != MemoryFaultType::NONE) {
		throw MemoryAccessException(faultInfo);
	}
}

// Method to execute STQ_C (Store Quadword Conditional)
void AlphaJITCompiler::ExecuteStqConditional(const JITBlock::Operation& op, quint64 currentPC) {
	quint64 virtualAddr = m_registerFileWrapper->readIntReg(op.ra) + op.immediate;
	quint64 valueToStore = m_registerFileWrapper->readIntReg(op.rb); // Value to store is in Rb

	DEBUG_LOG(QString("ExecuteStqConditional: VA=0x%1, value=0x%2, src=R%3, dest=R%4")
		.arg(QString::number(virtualAddr, 16))
		.arg(QString::number(valueToStore, 16))
		.arg(op.rb)
		.arg(op.rc));

	// First check alignment
	if (virtualAddr & 0x7) {
		// Alpha architecture: alignment failure causes SC to fail
		m_registerFileWrapper->writeIntReg(op.rc, 0); // 0 = failure

		if (m_profiler->isMonitoringEnabled()) {
			m_profiler->recordLockedOperation(virtualAddr, true, false); // true = write, false = failed
		}
		return;
	}

	// Get cache line for coherency checks
	quint64 cacheLine = virtualAddr & ~0x3F;

	// Check if lock reservation is still valid
	bool success = m_lockValid &&
		m_lockReservationAddr == virtualAddr &&
		m_lockReservationSize == 8;

	// In an SMP system, check if any other CPU has written to this cache line
	if (success && m_smpManagerPtr) {
		success = m_smpManagerPtr->checkLockReservationValid(m_cpuId, cacheLine);
	}

	// If reservation is valid, attempt the store
	if (success) {
		// Perform the memory access with full fault handling
		MemoryFaultInfo faultInfo = PerformMemoryAccess(
			virtualAddr,       // Virtual address
			&valueToStore,     // Value to write
			8,                 // Quadword size (8 bytes)
			true,              // It's a write
			false,             // Not an instruction fetch
			currentPC - 4,     // PC of the instruction
			op.rawInstr        // Raw instruction for fault reporting
		);

		// If fault occurred, handle it and mark operation as failed
		if (faultInfo.faultType != MemoryFaultType::NONE) {
			success = false;
			throw MemoryAccessException(faultInfo);
		}

		// Notify SMP system about successful store - other CPUs with lock on same line should invalidate
		if (m_smpManagerPtr) {
			m_smpManagerPtr->notifyStoreConditionalSuccess(m_cpuId, cacheLine);
		}
	}

	// Always invalidate the lock after attempting a conditional store
	m_lockValid = false;

	// Write success/failure status to destination register (1 = success, 0 = failure)
	m_registerFileWrapper->writeIntReg(op.rc, success ? 1 : 0);

	// If monitoring performance, record locked operation
	if (m_profiler->isMonitoringEnabled()) {
		m_profiler->recordLockedOperation(virtualAddr, true, success); // true = write
	}
}

// Method to execute STQ_U (Store Quadword Unaligned)
void AlphaJITCompiler::ExecuteStqUnaligned(const JITBlock::Operation& op, quint64 currentPC) {
	quint64 virtualAddr = m_registerFileWrapper->readIntReg(op.ra) + op.immediate;
	quint64 value = m_registerFileWrapper->readIntReg(op.rc);

	DEBUG_LOG(QString("ExecuteStqUnaligned: VA=0x%1, value=0x%2, src=R%3")
		.arg(QString::number(virtualAddr, 16))
		.arg(QString::number(value, 16))
		.arg(op.rc));

	// For STQ_U, align the address down to the nearest quadword boundary
	quint64 alignedAddr = virtualAddr & ~0x7ULL;

	// Calculate the offset within the quadword
	int byteOffset = virtualAddr & 0x7;

	// Perform a read-modify-write operation

	// First, read the current value at the aligned location
	quint64 currentValue = 0;
	MemoryFaultInfo readFaultInfo = PerformMemoryAccess(
		alignedAddr,     // Aligned virtual address
		&currentValue,   // Where to store the current value
		8,               // Quadword size (8 bytes)
		false,           // Not a write (yet)
		false,           // Not an instruction fetch
		currentPC - 4,   // PC of the instruction
		op.rawInstr      // Raw instruction for fault reporting
	);

	// Handle read faults
	if (readFaultInfo.faultType != MemoryFaultType::NONE) {
		throw MemoryAccessException(readFaultInfo);
	}

	// Create a mask for the bytes that should be preserved
	quint64 preserveMask;
	if (byteOffset == 0) {
		// If perfectly aligned, replace entire quadword
		preserveMask = 0;
	}
	else {
		// Otherwise, preserve bytes beyond the requested location
		preserveMask = ~0ULL << ((8 - byteOffset) * 8);
	}

	// Combine the current value (preserved bytes) with the new value
	quint64 newValue = (currentValue & preserveMask) | (value & ~preserveMask);

	// Now write the combined value back
	MemoryFaultInfo writeFaultInfo = PerformMemoryAccess(
		alignedAddr,     // Aligned virtual address
		&newValue,       // Value to write
		8,               // Quadword size (8 bytes)
		true,            // It's a write
		false,           // Not an instruction fetch
		currentPC - 4,   // PC of the instruction
		op.rawInstr      // Raw instruction for fault reporting
	);

	// Handle write faults
	if (writeFaultInfo.faultType != MemoryFaultType::NONE) {
		throw MemoryAccessException(writeFaultInfo);
	}
}