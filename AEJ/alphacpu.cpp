
#include <AlphaCPU.h>

// AlphaCPU.cpp - Main CPU class implementation
#include "AlphaCPU.h"
#include "AlphaMemorySystem.h"
#include "AlphaJITCompiler.h"
#include "AlphaInstructionDecoder.h"
#include <QDebug>
#include <QString>
#include "Helpers.h"
#include "alphapalinterpreter.h"
#include "UnifiedExecutors.h"
#include "TraceManager.h"
#include <QCoreApplication>
#include "StackFrame.h"


AlphaCPU::AlphaCPU(int cpuId, AlphaMemorySystem* memSystem, QObject* parent)
	: QObject(parent),
	m_cpuId(cpuId),
	m_state(helpers_JIT::CPUState::IDLE),
	m_running(0),
	m_cpuThread(nullptr),
	m_intRegisters(32, 0),  // Initialize 32 integer registers to 0
	m_fpRegisters(32, 0.0), // Initialize 32 FP registers to 0.0
	m_pc(0),
	m_currentIPL(0),
	m_kernelMode(false),
	m_memory(memSystem->getSafeMemory()),
	m_jitCompiler(nullptr),
	m_jitThreshold(50)
{
	if (m_memorySystem) {
		m_memory = getSafeMemory();
	}
	// Register 31 is hardwired to zero in Alpha architecture
	m_intRegisters[31] = 0;
    m_palInterpreter = new AlphaPALInterpreter(this); // Instantiate PAL Interpreter

    registerBank.reset(new RegisterBank(this));
    fpRegisterBank.reset(new FpRegisterBankcls(this));
    
    // configure the executors
	floatingpointExecutor.reset(new FloatingPointExecutor(this, m_memorySystem, registerBank.data(), fpRegisterBank.data(), parent));
	integerExecutor.reset(new IntegerExecutor(this, m_memorySystem, registerBank.data(), fpRegisterBank.data(), parent));
	controlExecutor.reset(new ControlExecutor(this, m_memorySystem, registerBank.data(), fpRegisterBank.data(), parent));
	vectorExecutor.reset(new VectorExecutor(this, m_memorySystem, registerBank.data(), fpRegisterBank.data(), parent));


}



void AlphaCPU::Initialize_SignalsAndSlots()
{
    if (m_memorySystem) {
        // Connect memory write signals for debugging or watchpoints
        connect(m_memorySystem, &SafeMemory::memoryWritten,
            this, &AlphaCPU::handleMemoryWrite);

        // Optional: Connect memory read signals if you want debugging
        connect(m_memorySystem, &SafeMemory::memoryRead,
            this, &AlphaCPU::handleMemoryRead);
    }

    // Connect FloatingPointExecutor traps (if executor emits traps or exceptions)
    if (floatingpointExecutor)
    {
        connect(floatingpointExecutor.data(), &FloatingPointExecutor::trapRaised, this, &AlphaCPU::handleFpTrapRaised); // Already OK
        // 🚀 
        connect(floatingpointExecutor.data(), &FloatingPointExecutor::illegalInstruction, this, &AlphaCPU::handleIllegalInstruction);
    }

     // Connect ControlExecutor traps
     if (controlExecutor) {
            connect(controlExecutor.data(), &ControlExecutor::trapRaised,
                this, &AlphaCPU::raiseTrap);
     }

        // Example: Connect CPU HALT signal
        connect(this, &AlphaCPU::halted,
            this, &AlphaCPU::handleHalt);

        // (Optional) Connect CPU reset signal if you implement soft-reset
        connect(this, &AlphaCPU::resetRequested,
            this, &AlphaCPU::handleReset);

        // Connect IntegerExecutor traps
     if (integerExecutor) {
            //             connect(integerExecutor.data(), &IntegerExecutor::trapRaised,
            //                 this, &AlphaCPU::raiseTrap);

                        // Connect executor's illegalInstruction to AlphaCPU handler
            connect(integerExecutor.data(), &IntegerExecutor::illegalInstruction,
                this, &AlphaCPU::handleIllegalInstruction);


            // Connect trapRaised to AlphaCPU
            connect(integerExecutor.data(), &IntegerExecutor::trapRaised,
                this, &AlphaCPU::handleTrapRaised);
     }

     if (vectorExecutor)
     {
            connect(integerExecutor.data(), &IntegerExecutor::trapRaised, this, &AlphaCPU::handleTrapRaised);
            connect(integerExecutor.data(), &IntegerExecutor::illegalInstruction, this, &AlphaCPU::handleIllegalInstruction);
     }


   
}
void AlphaCPU::handleTrapRaised(helpers_JIT::TrapType type)
{
	TraceManager::logInfo(QString("AlphaCPU%1: Trap raised %2")
		.arg(this->m_cpuId)
		.arg(static_cast<int>(type)));
	raiseTrap(type);
    //TODO handleTrapRaised
}

void AlphaCPU::stateChanged(helpers_JIT::CPUState newState)
{
	TraceManager::logInfo(QString("AlphaCPU%1: stateChanged %2")
		.arg(this->m_cpuId)
		.arg(static_cast<int>(newState)));
}



void AlphaCPU::halted()
{
	TraceManager::logInfo(QString("AlphaCPU%1: HALTED").arg(this->m_cpuId));
	setState(helpers_JIT::CPUState::HALTED);
	emit executionStopped();
    raiseTrap(helpers_JIT::TrapType::SoftwareInterrupt);
}




void AlphaCPU::iplChanged(int oldIPL, int newIPL)
{
	TraceManager::logInfo(QString("AlphaCPU%1: IPL changed from %2 to %3")
		.arg(this->m_cpuId)
		.arg(oldIPL)
		.arg(newIPL));

	m_currentIPL = newIPL;

	// Optional: trigger an internal trap or software interrupt if applicable
	if (newIPL > oldIPL) {
		raiseTrap(helpers_JIT::TrapType::SoftwareInterrupt);  // or define TrapType::IPLChange
	}
}


void AlphaCPU::trapOccurred_(helpers_JIT::ExceptionType trapType, quint64 pc, int cpuId_)
{
	TraceManager::logInfo(QString("AlphaCPU%1: trapOccurred %2")
		.arg(this->m_cpuId)
		.arg(static_cast<int>(trapType)));
	raiseTrap(trapType);
	//TODO trapOccurred()
}



void AlphaCPU::memoryAccessed(quint64 address, bool isWrite, int size)
{
// 	TraceManager::logInfo(QString("AlphaCPU%1: memoryAccessed %2")
// 		.arg(this->m_cpuId)
// 		.arg(static_cast<int>(type)));
//	raiseTrap(type);
    //TODO memoryAccessed(quint64 address, bool isWrite, int size)
}

void AlphaCPU::systemInitialized()
{
    //TODO AlphaCPU::systemInitialized()
}
void AlphaCPU::trapOccurred(helpers_JIT::ExceptionType trapType, quint64 pc, int cpuId_)
{
	//TODO AlphaCPU::trapOccurred()
}
void AlphaCPU::trapOccurred(helpers_JIT::ExceptionType trapType, quint64 pc, quint8 cpuId_)
{
	//TODO trapOccurred(helpers_JIT::ExceptionType trapType, quint64 pc, quint8 cpuId_)
}
void AlphaCPU::finish()
{
    //TODO finish()
}

void AlphaCPU::instructionFaulted_(quint64 pc, quint32 instr)
{

}

void AlphaCPU::executionFinished()
{
    //TODO executionFinished()
}
AlphaCPU::~AlphaCPU()
{
	// Ensure CPU thread is stopped
	stopExecution();

	// Wait for thread to finish if it exists
	if (m_cpuThread) {
		m_cpuThread->quit();
		m_cpuThread->wait();
		delete m_cpuThread;
	}
}
void AlphaCPU::initialize()
{
    Initialize_SignalsAndSlots();

    // Create CPU thread
    m_cpuThread = new QThread();
    m_cpuThread->setObjectName(QString("Alpha-CPU-%1").arg(m_cpuId));

    // Move this object to the CPU thread
    this->moveToThread(m_cpuThread);

    // Start the thread
    m_cpuThread->start();

    // Set initial state
    m_state = helpers_JIT::CPUState::IDLE;
    emit stateChanged(m_state);

    qDebug() << "CPU" << m_cpuId << "initialized";
}


		void AlphaCPU::initializeSystem()
		{
			setPC(0x20000000);
			setKernelSP(0x7FFFFFF0); // Example stack addresses
			setUserSP(0x7FF00000);
			setKernelGP(0x00000000);
			m_state = helpers_JIT::CPUState::RUNNING;
			m_running = true;
		}
        quint64 AlphaCPU::getRegister(int regNum, helpers_JIT::RegisterType type) const
        {
            if (type == helpers_JIT::RegisterType::INTEGER_REG) {
                if (regNum >= 0 && regNum < 32) {
                    return m_intRegisters[regNum];
                }
            }
            else if (type == helpers_JIT::RegisterType::FLOAT_REG) {
                if (regNum >= 0 && regNum < 32) {
                    // Convert double to quint64 bit pattern
                    union {
                        double d;
                        quint64 u;
                    } conv;
                    conv.d = m_fpRegisters[regNum];
                    return conv.u;
                }
            }
            else if (type == helpers_JIT::RegisterType::SPECIAL_REG) {
                if (m_specialRegisters.contains(regNum)) {
                    return m_specialRegisters[regNum];
                }
            }

            return 0; // Invalid register or special register not set
        }

        void AlphaCPU::setRegister(int regNum, quint64 value, helpers_JIT::RegisterType type)
        {
            if (type == helpers_JIT::RegisterType::INTEGER_REG) {
                if (regNum >= 0 && regNum < 32) {
                    // R31 is hardwired to zero in Alpha
                    if (regNum == 31) {
                        return;
                    }

                    m_intRegisters[regNum] = value;
                    emit registerChanged(regNum, type, value);
                }
            }
            else if (type == helpers_JIT::RegisterType::FLOAT_REG) {
                if (regNum >= 0 && regNum < 32) {
                    // Convert quint64 bit pattern to double
                    union {
                        double d;
                        quint64 u;
                    } conv;
                    conv.u = value;
                    m_fpRegisters[regNum] = conv.d;
                    emit registerChanged(regNum, type, value);
                }
            }
            else if (type == helpers_JIT::RegisterType::SPECIAL_REG) {
                m_specialRegisters[regNum] = value;
                emit registerChanged(regNum, type, value);
            }
        }

    void AlphaCPU::startExecution()
    {
        QMutexLocker locker(&m_stateLock);

        // Check if already running
        if (m_state == helpers_JIT::CPUState::RUNNING) {
            return;
        }

        // Set running flag
        m_running = 1;
        m_state = helpers_JIT::CPUState::RUNNING;

        // Start execution loop using a signal-slot connection to ensure it runs in the CPU thread
        QMetaObject::invokeMethod(this, "executeLoop", Qt::QueuedConnection);

        emit executionStarted();
        emit stateChanged(m_state);

        qDebug() << "CPU" << m_cpuId << "started execution at PC =" << Qt::hex << m_pc;
    }

    void AlphaCPU::pauseExecution()
    {
        QMutexLocker locker(&m_stateLock);

        // Only pause if running
        if (m_state != helpers_JIT::CPUState::RUNNING) {
            return;
        }

        m_state = helpers_JIT::CPUState::PAUSED;
        emit executionPaused();
        emit stateChanged(m_state);

        qDebug() << "CPU" << m_cpuId << "paused at PC =" << Qt::hex << m_pc;
    }

	StackFrame AlphaCPU::popFrame()
	{
		auto& stack = m_stacks[currentMode()];  // Get the stack for current mode
		if (stack.isEmpty()) {
			qWarning() << QString("[AlphaCPU%1] Trap stack underflow").arg(m_cpuId);
			return StackFrame();  // Return default/invalid frame
		}
		return stack.takeLast();  // Pop the most recent frame
	}

	void AlphaCPU::haltExecution()
	{
		QMutexLocker locker(&m_stateLock);

		// Set CPU state to PAUSED or HALTED depending on design
		m_state = helpers_JIT::CPUState::PAUSED;

		// Stop running execution loop
		m_running = false;

		// Emit CPU halted signal (optional, for GUI or debugger)
		emit halted();
		TraceManager::logInfo(QString("AlphaCPU%1: Execution halted.").arg(m_cpuId));
	}

	void AlphaCPU::pushFrame(const StackFrame& frame)
	{
		m_stacks[currentMode()].append(frame);
	}

	bool AlphaCPU::isMMUEnabled()
	{
        return this->m_bMMUEnabled;
	}

	void AlphaCPU::resumeExecution() {
		if (m_running) {
			qDebug() << QString("[AlphaCPU%1] Already running").arg(m_cpuId);
			return;
		}

		qDebug() << QString("[AlphaCPU%1] Resuming execution at PC=0x%2")
			.arg(m_cpuId)
			.arg(m_pc, 8, 16, QChar('0'));

		stopRequested.storeRelaxed(false);
		m_running = true;

		int instructionCount = 0;
		while (!stopRequested.loadRelaxed()) {
			executeNextInstruction();

			if (++instructionCount % 500 == 0) {
				QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
			}
		}

		qDebug() << QString("[AlphaCPU%1] Execution halted").arg(m_cpuId);
		m_running = false;

		emit halted();  // Signal for QThread cleanup or UI
	}


	void AlphaCPU::stopExecution()
    {
        QMutexLocker locker(&m_stateLock);

        // Set running flag to 0 to stop execution loop
        m_running = 0;

        // Only emit signals if actually running or paused
        if (m_state == helpers_JIT::CPUState::RUNNING || m_state == helpers_JIT::CPUState::PAUSED) {
            m_state = helpers_JIT::CPUState::IDLE;
            emit executionStopped();
            emit stateChanged(m_state);

            qDebug() << "CPU" << m_cpuId << "stopped at PC =" << Qt::hex << m_pc;
        }
    }

	void AlphaCPU::requestStop()
	{
		stopRequested.storeRelaxed(true);
		qDebug() << QString("[AlphaCPU%1] Stop requested").arg(m_cpuId);
	}


	void AlphaCPU::resetCPU()
	{
		qDebug() << QString("[AlphaCPU%1] Resetting CPU state").arg(m_cpuId);

		// Clear general-purpose and FP registers
		registerBank->clear();
		fpRegisterBank->clear();

		// Reset processor state
		m_pc = 0x0;
		m_fp = 0x0;
		m_currentIPL = 0;
		m_kernelMode = true;
		m_palMode = false;
		m_lockFlag = false;
		m_lockedPhysicalAddress = 0;

		// Clear traps/exceptions
		m_exceptionPending = false;
		m_exceptionVector = 0;
		m_excSum.fill(false);

		// Reset FPCR
		m_fpcr.raw = 0;

		// Clear stack frames
		m_stacks.clear();

		// Restore PSR defaults
		m_psr = 0;
		m_savedPsr = 0;
		m_astEnable = false;
		m_asn = 0;
		m_uniqueValue = 0;
		m_processorStatus = 0;
		m_usp = 0;
		m_vptptr = 0;

        m_intRegisters[31] = 0; //In Alpha AXP, register R31 is always zero.

		// Optionally reset internal JIT counters
		if (m_jitCompiler) m_jitCompiler->clear();

		// Reset execution control flags
		stopRequested.storeRelaxed(false);

		// Optional: reinitialize PAL vector (if booting from reset)
		if (m_cpuId == 0)
			m_pc = 0x21000000;  // Default SRM PAL reset vector
	}


    void AlphaCPU::setPC(quint64 pc)
    {
        QMutexLocker locker(&m_stateLock);
        m_pc = pc;
    }

    void AlphaCPU::handleInterrupt(int interruptVector)
    {
        QMutexLocker locker(&m_stateLock);

        // Only handle if IPL allows
        if (interruptVector <= m_currentIPL) {
            return;
        }

        // Wake up if waiting for interrupt
        if (m_state == helpers_JIT::CPUState::WAITING_FOR_INTERRUPT) {
            m_state = helpers_JIT::CPUState::RUNNING;
            m_waitForInterrupt.wakeAll();
        }

        // Handle the interrupt based on vector
        // This would typically save context and jump to interrupt handler
        qDebug() << "CPU" << m_cpuId << "handling interrupt vector" << interruptVector;

        // Change IPL
        handleIPLChange(interruptVector);
    }

    void AlphaCPU::handleIPLChange(int newIPL)
    {
        int oldIPL = m_currentIPL;
        m_currentIPL = newIPL;

        emit iplChanged(oldIPL, newIPL);

        qDebug() << "CPU" << m_cpuId << "IPL changed from" << oldIPL << "to" << newIPL;
    }

    void AlphaCPU::notifyBlockCompiled(quint64 startAddr, const QByteArray & nativeCode)
    {
        // Store the compiled block
        m_compiledBlocks[startAddr] = nativeCode;

        qDebug() << "CPU" << m_cpuId << "received compiled block for address" << Qt::hex << startAddr
            << "size:" << nativeCode.size() << "bytes";
    }

    void AlphaCPU::invalidateCompiledBlock(quint64 startAddr)
    {
        // Remove the compiled block
        m_compiledBlocks.remove(startAddr);

        qDebug() << "CPU" << m_cpuId << "invalidated compiled block at address" << Qt::hex << startAddr;
    }

    void AlphaCPU::handleMemoryProtectionFault(quint64 address, int accessType)
    {
        // Handle memory protection fault
        raiseException(helpers_JIT::ExceptionType::MEMORY_ACCESS_VIOLATION, address);
    }

    void AlphaCPU::handleTranslationMiss(quint64 virtualAddr)
    {
        // Handle translation miss
        raiseException(helpers_JIT::ExceptionType::MEMORY_ACCESS_VIOLATION, virtualAddr);
    }

	void AlphaCPU::handleIllegalInstruction(quint64 instructionWord, quint64 pc)
	{
		TraceManager::logInfo(QString("AlphaCPU%1: Illegal instruction 0x%2 at PC=0x%3")
			.arg(m_cpuId)
			.arg(instructionWord, 8, 16, QChar('0'))
			.arg(pc, 8, 16, QChar('0')));

		raiseTrap(helpers_JIT::TrapType::ReservedInstruction);
	}

	void AlphaCPU::handleFpTrapRaised(helpers_JIT::TrapType trapType)
	{
        //TODO handleFpTrapRaised(helpers_JIT::TrapType trapType)
	}

	void AlphaCPU::executeLoop()
    {
        // Main execution loop
        while (m_running) {
            try {
                // Check if paused
                {
                    QMutexLocker locker(&m_stateLock);
                    if (m_state == helpers_JIT::CPUState::PAUSED) {
                        locker.unlock();
                        QThread::msleep(10); // Sleep briefly to avoid busy waiting
                        continue;
                    }
                }

                // Execute a block starting at current PC
                executeBlock(m_pc);

                // Allow other events to be processed
                //QCoreApplication::processEvents();
                emit processingProgress((m_currentCycle * 100) / m_maxCycles);

            }
            catch (helpers_JIT::ExceptionType exType) {
                // Handle exceptions
                handleMemoryException(m_pc, 4); // 4 bytes for instruction fetch
            }
        }
        emit operationCompleted();
    }


// 	void AlphaCPU::executeUnifiedFallback(quint32 instruction)
// 	{
// 		OperateInstruction op = decodeInstruction(instruction);
// 		quint8 opcode = instruction & 0x3F;
// 
// 		switch (getInstructionClass(opcode)) {
// 		case InstructionClass::Integer:
// 			integerExecutor->execute(op);
// 			break;
// 		case InstructionClass::FloatingPoint:
// 			floatingPointExecutor->execute(op);
// 			break;
// 		case InstructionClass::Vector:
// 			vectorExecutor->execute(op);
// 			break;
// 		case InstructionClass::Control:
// 			controlExecutor->execute(op);
// 			break;
// 		default:
// 			qCritical() << "[FATAL] Unknown instruction class for fallback execution.";
// 			raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, m_pc);
// 			break;
// 		}
// 	}

    void AlphaCPU::executeBlock(quint64 startAddr)
    {
        // Check if we have a compiled version of this block
        if (m_compiledBlocks.contains(startAddr)) {
            executeCompiledBlock(startAddr);
            return;
        }

        // Interpret instructions until branch or maximum block size
        const int maxBlockSize = 32; // Maximum number of instructions in a block
        QByteArray blockInstructions;
        quint64 currentPC = startAddr;

        for (int i = 0; i < maxBlockSize; i++) {
            // Fetch instruction
            quint32 instruction = fetchInstruction(currentPC);

            // Add to block buffer for potential JIT compilation
            blockInstructions.append(reinterpret_cast<char*>(&instruction), 4);

            // Decode and execute
            bool isBranch = decodeAndExecute(instruction);

            // Emit monitoring signal
            emit instructionExecuted(currentPC, instruction);

            // Update PC (4 bytes per instruction in Alpha)
            currentPC += 4;

            // Stop block if branch instruction
            if (isBranch) {
                break;
            }
        }

        // Update execution statistics for this block
        updateBlockStatistics(startAddr);

        // Check if block should be compiled
        if (m_blockHitCounter[startAddr] >= m_jitThreshold && m_jitCompiler) {
            // Request compilation
            emit requestBlockCompilation(startAddr, blockInstructions);

            // Reset counter
            m_blockHitCounter[startAddr] = 0;
        }
    }

    void AlphaCPU::executeCompiledBlock(quint64 startAddr)
    {
        // In a real implementation, this would execute the native code
        // For this simulation, we'll just print a message
        qDebug() << "CPU" << m_cpuId << "executing compiled block at address" << Qt::hex << startAddr;

        // We would execute the native code here using some form of function pointer or JIT execution engine
        // For now, we'll just increment the PC as a placeholder
        m_pc += 4; // Assume at least one instruction executed

        // Update statistics
        m_blockHitCounter[startAddr]++;
    }

	quint32 AlphaCPU::fetchInstruction(quint64 address)
	{
		// Attempt to fetch the instruction from memory
		quint32 instruction = 0;

		try {
			// Read a 32-bit instruction from the memory system
			if (!m_memorySystem->readVirtualMemory(this,address, &instruction, 4)) {
				// Failed to read, handle as memory exception
				handleMemoryException(address, 4); // 4 bytes for instruction access

				// Since handleMemoryException will raise an exception, 
				// we won't normally reach here, but return 0 just in case
				return 0;
			}

			// Log instruction fetch if debug tracing is enabled
			// qDebug() << "CPU" << m_cpuId << "fetched instruction" << Qt::hex << instruction << "at" << address;

			return instruction;
		}
		catch (const std::exception& e) {
			// Handle any other exceptions that might occur
			qDebug() << "CPU" << m_cpuId << "exception during instruction fetch:" << e.what();
			handleMemoryException(address, 4);
			return 0;
		}
	}


//     bool AlphaCPU::decodeAndExecute(quint32 instruction)
//     {
//         // Extract opcode (bits 31-26 in Alpha)
//         quint32 opcode = (instruction >> 26) & 0x3F;
// 
//         // Determine instruction type based on opcode
//         bool isBranch = false;
// 
//         switch (opcode) {
//             // Integer operate format
//         case 0x10: // Integer arithmetic
//         case 0x11: // Logical operations
//         case 0x12: // Shift and byte manipulation
//         case 0x13: // Multiply
//             executeIntegerOperation(instruction);
//             break;
// 
//             // Floating point operate format
//         case 0x16: // Floating point arithmetic
//         case 0x17: // Floating point other
//             executeFloatingPointOperation(instruction);
//             break;
// 
//             // Memory format
//         case 0x28: // Load
//         case 0x29: // Load quadword
//         case 0x2A: // Load locked
//         case 0x2B: // Load quadword locked
//         case 0x2C: // Store
//         case 0x2D: // Store quadword
//         case 0x2E: // Store conditional
//         case 0x2F: // Store quadword conditional
//             executeMemoryOperation(instruction);
//             break;
// 
//             // Branch format
//         case 0x30: // Branch
//         case 0x34: // Branch to subroutine
//         case 0x38: // Branch if zero
//         case 0x39: // Branch if equal
//         case 0x3C: // Branch if set
//         case 0x3D: // Branch if not equal
//             executeBranchOperation(instruction);
//             isBranch = true;
//             break;
// 
//             // Special operations
//         case 0x00: // PAL code call
//             executePALOperation(instruction);
//             break;
// 
//         default:
//             // Unknown opcode
//             raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, m_pc);
//             break;
//         }
// 
//         return isBranch;
//     }

    /*
    
	Opcode (hex)	Class	                Meaning
    00–0F	        Integer Operate	        ADDL, SUBQ, CMPLT, etc.
    10–17	        Memory Load/Store	    LDA, LDQ, STQ
    18–1F	        Vector/SIMD (optional)	VADD, VSUB, etc.
    20–2F	        Floating-Point	        ADDF, MULF, CVTGF
    30–3F	        Branch/Control	        BR, BSR, JSR, RET

    */

	bool AlphaCPU::decodeAndExecute(quint32 instruction)
	{
		// Extract the primary opcode (bits 31:26)
		quint8 primaryOpcode = static_cast<quint8>((instruction >> 26) & 0x3F);

#ifdef QT_DEBUG
		qDebug() << "[AlphaCPU] Decoding instruction:"
			<< QString("0x%1").arg(instruction, 8, 16)
			<< " Primary opcode:"
			<< QString("0x%1").arg(primaryOpcode, 2, 16);
#endif

		// Dispatch based on primary opcode
		if (primaryOpcode <= 0x0F && integerExecutor) {
			// Integer Operate instructions (ADD, SUB, CMP, etc.)
            integerExecutor->execute(instruction);
		}
		else if (primaryOpcode >= 0x20 && primaryOpcode <= 0x2F && floatingpointExecutor) {
			// Floating-point instructions (ADDF, MULF, CVTGF, etc.)
			floatingpointExecutor->execute(instruction);
		}
		else if (primaryOpcode >= 0x30 && primaryOpcode <= 0x3F && controlExecutor) {
			// Control instructions (BR, BSR, JSR, RET, etc.)
            controlExecutor->execute(instruction);
            return true;
		}
		else if (primaryOpcode >= 0x18 && primaryOpcode <= 0x1F && vectorExecutor) {
			// Vector/SIMD instructions (if implemented)
            vectorExecutor->execute(instruction);
		}
		else {
			// Unknown or reserved instruction
			qWarning() << "[AlphaCPU] Reserved or unknown opcode:"
				<< QString("0x%1").arg(primaryOpcode, 2, 16);
			raiseTrap(helpers_JIT::TrapType::MMUAccessFault);
		}
        return false;
	}


    void AlphaCPU::executeIntegerOperation(quint32 instruction)
    {
        // Extract fields
        quint32 opcode = (instruction >> 26) & 0x3F;
        quint32 ra = (instruction >> 21) & 0x1F;
        quint32 rb = (instruction >> 16) & 0x1F;
        quint32 function = (instruction >> 5) & 0x7F;
        quint32 rc = instruction & 0x1F;

        // Extract literal if bit 12 is set (immediate mode)
        bool useImmediate = (instruction >> 12) & 0x1;
        quint32 literal = 0;

        if (useImmediate) {
            literal = (instruction >> 13) & 0xFF; // 8-bit literal in bits 20-13
        }

        // Get operand values
        quint64 operandA = m_intRegisters[ra];
        quint64 operandB = useImmediate ? literal : m_intRegisters[rb];
        quint64 result = 0;

        // Execute based on function code
        switch (function) {
        case 0x00: // ADDL
            result = (quint32)operandA + (quint32)operandB;
            break;
        case 0x20: // ADDQ
            result = operandA + operandB;
            break;
        case 0x09: // SUBL
            result = (quint32)operandA - (quint32)operandB;
            break;
        case 0x29: // SUBQ
            result = operandA - operandB;
            break;
        case 0x0C: // MULL
            result = (quint32)operandA * (quint32)operandB;
            break;
        case 0x2C: // MULQ
            result = operandA * operandB;
            break;
            // Add more operations as needed
        default:
            // Unknown function
            raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, m_pc);
            return;
        }

        // Store result
        setRegister(rc, result, helpers_JIT::RegisterType::INTEGER_REG);
    }

    void AlphaCPU::executeFloatingPointOperation(quint32 instruction)
    {
        // Extract fields
        quint32 opcode = (instruction >> 26) & 0x3F;
        quint32 fa = (instruction >> 21) & 0x1F;
        quint32 fb = (instruction >> 16) & 0x1F;
        quint32 function = (instruction >> 5) & 0x7F;
        quint32 fc = instruction & 0x1F;

        // Get operand values
        double operandA = m_fpRegisters[fa];
        double operandB = m_fpRegisters[fb];
        double result = 0.0;

        // Execute based on function code
        switch (function) {
        case 0x00: // ADDF
        case 0x01: // ADDD
            result = operandA + operandB;
            break;
        case 0x20: // SUBF
        case 0x21: // SUBD
            result = operandA - operandB;
            break;
        case 0x08: // MULF
        case 0x09: // MULD
            result = operandA * operandB;
            break;
        case 0x18: // DIVF
        case 0x19: // DIVD
            if (operandB == 0.0) {
                raiseException(helpers_JIT::ExceptionType::ARITHMETIC_TRAP, m_pc);
                return;
            }
            result = operandA / operandB;
            break;
            // Add more operations as needed
        default:
            // Unknown function
            raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, m_pc);
            return;
        }

        // Store result
        union {
            double d;
            quint64 u;
        } conv;
        conv.d = result;
        setRegister(fc, conv.u, helpers_JIT::RegisterType::FLOAT_REG);
    }

    void AlphaCPU::executeMemoryOperation(quint32 instruction)
    {
        // Extract fields
        quint32 opcode = (instruction >> 26) & 0x3F;
        quint32 ra = (instruction >> 21) & 0x1F;
        quint32 rb = (instruction >> 16) & 0x1F;
        qint16 displacement = instruction & 0xFFFF; // 16-bit signed displacement

        // Calculate effective address
        quint64 address = m_intRegisters[rb] + displacement;

        // Determine operation type
        bool isLoad = (opcode >= 0x28 && opcode <= 0x2B);
        bool isQuadword = (opcode == 0x29 || opcode == 0x2B || opcode == 0x2D || opcode == 0x2F);
        bool isLocked = (opcode == 0x2A || opcode == 0x2B || opcode == 0x2E || opcode == 0x2F);

        // Size of access
        int size = isQuadword ? 8 : 4; // 8 bytes for quadword, 4 bytes for longword

        try {
            if (isLoad) {
                // Load operation
                if (isQuadword) {
                    // Quadword load
                    quint64 value = 0;
                    if (m_memorySystem->readVirtualMemory(this,address, &value, 8)) {
                        setRegister(ra, value, helpers_JIT::RegisterType::INTEGER_REG);
                    }
                }
                else {
                    // Longword load
                    quint32 value = 0;
                    if (m_memorySystem->readVirtualMemory(this,address, &value,  4)) {
                        // Sign-extend to 64 bits
                        qint64 signExtended = (qint32)value;
                        setRegister(ra, signExtended, helpers_JIT::RegisterType::INTEGER_REG);
                    }
                }

                // Emit memory access signal
                emit memoryAccessed(address, false, size);

            }
            else {
                // Store operation
                if (isQuadword) {
                    // Quadword store
                    quint64 value = m_intRegisters[ra];
                    m_memorySystem->writeVirtualMemory(this,address, value, 8);
                }
                else {
                    // Longword store
                    quint64 value = (quint64)m_intRegisters[ra];
                    m_memorySystem->writeVirtualMemory(this,address, value, 4);
                }

                // Emit memory access signal
                emit memoryAccessed(address, true, size);

                // For conditional stores, we would set a success/failure indicator in ra
                // This is simplified here
                if (opcode == 0x2E || opcode == 0x2F) {
                    // Always indicate success for this implementation
                    setRegister(ra, 1, helpers_JIT::RegisterType::INTEGER_REG);
                }
            }
        }
        catch (...) {
            // Handle memory access error
            handleMemoryException(address, size);
        }
    }
	void AlphaCPU::setMode(helpers_JIT::MmuMode mode_)
	{
		m_psr = (m_psr & ~0x3) | (static_cast<quint8>(mode_) & 0x3);
	}

	void AlphaCPU::setIPL(quint8 ipl_)
	{
		m_currentIPL = ipl_;
		m_psr = (m_psr & ~(0xF << 8)) | ((ipl_ & 0xF) << 8);
	}

	void AlphaCPU::setRunning(bool running)
	{
		m_running = running;
	}

	void AlphaCPU::setState(helpers_JIT::CPUState state)
	{
		m_state = state;
	}
    void AlphaCPU::executeBranchOperation(quint32 instruction)
    {
        // Extract fields
        quint32 opcode = (instruction >> 26) & 0x3F;
        quint32 ra = (instruction >> 21) & 0x1F;
        qint32 displacement = (instruction & 0x1FFFFF); // 21-bit signed displacement

        // Sign-extend the displacement if needed
        if (displacement & 0x100000) {
            displacement |= ~0x1FFFFF; // Extend sign bit
        }

        // Displacement is in longwords (4 bytes), so multiply by 4
        displacement *= 4;

        // Determine branch type
        bool unconditional = (opcode == 0x30 || opcode == 0x34); // BR, BSR
        bool takeBranch = unconditional;

        if (!unconditional) {
            // Conditional branch based on register value
            quint64 value = m_intRegisters[ra];

            switch (opcode) {
            case 0x38: // Branch if zero (BLBC)
                takeBranch = (value == 0);
                break;
            case 0x39: // Branch if equal (BEQ)
                takeBranch = (value == 0);
                break;
            case 0x3C: // Branch if set (BLBS)
                takeBranch = (value != 0);
                break;
            case 0x3D: // Branch if not equal (BNE)
                takeBranch = (value != 0);
                break;
                // Add more conditions as needed
            }
        }

        if (takeBranch) {
            // Calculate new PC
            quint64 newPC = m_pc + 4 + displacement; // PC+4 because PC points to current instruction

            // For BSR, save return address
            if (opcode == 0x34) { // BSR
                setRegister(ra, m_pc + 4, helpers_JIT::RegisterType::INTEGER_REG); // Return address is next instruction
            }

            // Update PC
            m_pc = newPC;
        }
        else {
            // Just move to next instruction
            m_pc += 4;
        }
    }

//     void AlphaCPU::executePALOperation(quint32 instruction)
//     {
//         // Extract fields
//         quint32 palFunction = instruction & 0x3FFFFFF; // 26 bits for PAL function
// 
//         // Handle different PAL functions
//         switch (palFunction) {
//         case 0x0000: // HALT
//             m_running = 0; // Stop execution
//             m_state = helpers_JIT::CPUState::IDLE;
//             emit executionStopped();
//             emit stateChanged(m_state);
//             break;
// 
//         case 0x0001: // Privileged context switch
//             if (!m_kernelMode) {
//                 raiseException(helpers_JIT::ExceptionType::PRIVILEGED_INSTRUCTION, m_pc);
//                 return;
//             }
//             // Handle context switch...
//             break;
// 
//         case 0x0083: // System call
//             // System calls would typically be dispatched to the exception handler
//             raiseException(helpers_JIT::ExceptionType::SYSTEM_CALL, m_pc);
//             break;
// 
//             // Add more PAL functions as needed
// 
//         default:
//             // Unknown PAL function
//             raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, m_pc);
//             break;
//         }
//     }

    void AlphaCPU::checkForHotSpots()
    {
        // Find blocks that are executed frequently but not yet compiled
        for (auto it = m_blockHitCounter.begin(); it != m_blockHitCounter.end(); ++it) {
            quint64 blockAddr = it.key();
            int execCount = it.value();

            if (execCount >= m_jitThreshold && !m_compiledBlocks.contains(blockAddr)) {
                // This block is hot, notify for potential trace compilation
                quint64 endAddr = blockAddr; // In a real implementation, we'd track the end address
                emit hotSpotDetected(blockAddr, endAddr, execCount);
            }
        }
    }

	/*
	This is determined by the low 2 bits of PSR, which define the current MMU privilege mode.
	Bits	Mode
	00	    Kernel
	01  	Executive
	10  	Supervisor
	11  	User
	*/
	helpers_JIT::MmuMode AlphaCPU::currentMode() const
	{
		return static_cast<helpers_JIT::MmuMode>(m_psr & 0x3);  // bits 1:0 = CM
	}
    void AlphaCPU::updateBlockStatistics(quint64 startAddr)
    {
        // Increment execution count for this block
        m_blockHitCounter[startAddr]++;

        // Periodically check for hot spots
        if (m_blockHitCounter.size() % 100 == 0) {
            checkForHotSpots();
        }
    }

    /*
	*   AlphaCPU::raiseException(ExceptionType, PC)
             ↓ (emit)
        Qt Signal: exceptionRaised(ExceptionType, PC, FaultAddr)
             ↓ (connect)
        AlphaExceptionHandler::handleException(cpuId, ExceptionType, PC, FaultAddr)
             ↓
        Dispatch Exception to Kernel (dispatchToKernel)
             ↓
        Set new PC = exceptionVectorAddress
             ↓
        Kernel Exception Handler starts executing

		 -Saves and restores CPU state
         -Locks state during transition
         -Emits stateChanged before and after

    */

    void AlphaCPU::raiseException(helpers_JIT::ExceptionType type, quint64 faultAddr)
    {
//         qDebug() << "CPU" << m_cpuId << "raising exception" << type << "at PC =" << Qt::hex << m_pc
//             << "fault address =" << Qt::hex << faultAddr;

        // Save old state
        QMutexLocker locker(&m_stateLock);
        helpers_JIT::CPUState oldState = m_state;
        m_state = helpers_JIT::CPUState::EXCEPTION_HANDLING;
        emit stateChanged(m_state);

        // Emit exception signal
        emit exceptionRaised(type, m_pc, faultAddr);

        // In a real implementation, this would transfer control to exception handler
        // For now, we'll just restore the state
        m_state = oldState;
        emit stateChanged(m_state);
    }
    /*
	Purpose	                    Why it matters
    Centralized interpreter	    One place to route decoded instructions
    Branch logic handling	    Only update PC if not a branch
    Signal emission	            emit instructionExecuted(pc, instr) is important for debugging and trace managers
    Error handling	            Isolates fault dispatch (dispatchException) cleanly
    Reusability	                Can be called from JIT fallback, single-step mode, trap handlers, etc.
    
    */
	void AlphaCPU::interpretInstruction(quint32 instruction)
	{
		// Decode opcode: bits 31–26
		quint32 opcode = (instruction >> 26) & 0x3F;
		quint64 currentPC = m_pc;

		try {
			// Decode and dispatch — returns true if branch modified PC
			bool isBranch = decodeAndExecute(instruction);

			// Emit trace/log signal
			emit instructionExecuted(currentPC, instruction);

			// Only advance PC if not a branch or trap
			if (!isBranch) {
				m_pc += 4;  // Alpha instructions are 4 bytes wide
			}
		}
		catch (helpers_JIT::ExceptionType exType) {
			// On fault, dispatch to trap/exception handler
			dispatchException(exType, currentPC);
		}
	}



void AlphaCPU::dispatchException(helpers_JIT::ExceptionType type, quint64 faultAddr)
{
    QMutexLocker locker(&m_stateLock);

    // Save current state
    helpers_JIT::CPUState previousState = m_state;

    // Change state to exception handling
    m_state = helpers_JIT::CPUState::EXCEPTION_HANDLING;
    emit stateChanged(m_state);

    // Log the exception
//     qDebug() << "CPU" << m_cpuId << "exception:" << type << "at PC =" << Qt::hex << m_pc
//              << "fault address =" << Qt::hex << faultAddr;

    // Emit exception signal to be handled by the exception handler
    emit exceptionRaised(type, m_pc, faultAddr);

    // In a real system, the exception handler would update the PC to point to the
    // appropriate exception vector, save registers, etc.

    // For now, we'll just handle some basic exceptions internally
    switch (type) {
    case helpers_JIT::ExceptionType::ARITHMETIC_TRAP:
            // Set result register to 0 and continue
            m_intRegisters[m_intRegisters.size() - 1] = 0;
            break;

    case helpers_JIT::ExceptionType::MEMORY_ACCESS_VIOLATION:
            // Memory exception could allocate the page in some cases
            // Here we'll just continue (in reality, this would often terminate the program)
            break;

        case helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION:
            // Can't recover from this - terminate execution
            m_running = 0;
            m_state = helpers_JIT::CPUState::IDLE;
            emit executionStopped();
            emit stateChanged(m_state);
            return;

        default:
            // For other exceptions, try to continue execution
            break;
    }

    // Return to previous state unless we changed it above
    if (m_state == helpers_JIT::CPUState::EXCEPTION_HANDLING) {
        m_state = previousState;
        emit stateChanged(m_state);
    }
}

void AlphaCPU::handleMemoryException(quint64 address, int accessType)
{
	qDebug() << "CPU" << m_cpuId << "memory exception at address" << Qt::hex << address
		<< "access type:" << accessType;

	// Determine exception type based on access and potential cause
    helpers_JIT::ExceptionType exceptionType = helpers_JIT::ExceptionType::MEMORY_ACCESS_VIOLATION;

	// Check for alignment issues (common in Alpha architecture)
	// Alpha requires aligned memory access for most operations
	if ((accessType == 4 && (address & 0x3)) ||    // 4-byte access must be 4-byte aligned
		(accessType == 8 && (address & 0x7))) {    // 8-byte access must be 8-byte aligned
		exceptionType = helpers_JIT::ExceptionType::ALIGNMENT_FAULT;
	}

    //TODO: Exception Handling
	// In a real system, we might try to handle the fault by:
	// 1. Allocating a new page for demand paging
	// 2. Fetching data from disk for page faults
	// 3. Copy-on-write for shared pages

	// For this implementation, we'll just raise the exception
	raiseException(exceptionType, address);

	// Note: Control returns to the caller, which should then handle
	// return from exception handling if needed
}
/*
PAL Interfaces
*/

/**
 * Execute a PAL operation by dispatching it to the PAL interpreter
 */
void AlphaCPU::executePALOperation(quint32 instruction)
{
    quint32 palFunction = instruction & 0x3FFFFFF; // Extract 26-bit PAL function
    if (m_palInterpreter) {
        m_palInterpreter->processPALInstruction(this, palFunction);
    }
    else {
        raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, m_pc);
    }
}

// Floating Point Exception Handling

void AlphaCPU::handleFpTrap(const QString& reason) {
	qWarning() << "[AlphaCPU] Floating-point trap:" << reason;

	// TODO: set trap cause, save trap frame, vector to PALcode, etc.

	// Example simple behavior for now:
	haltExecution(); // Stop CPU or set trapPending flag
}

void AlphaCPU::raiseTrap(helpers_JIT::TrapType trapType)
{
	TraceManager::logInfo(QString("AlphaCPU%1: raiseTrap %2")
        .arg(QString::number(m_cpuId))
        .arg(static_cast<int>(trapType)));

    //TODO AlphaCPU::raiseTrap(TrapType type)
}

void AlphaCPU::returnFromTrap()
{
	StackFrame frame = popFrame();

	m_fp = frame.framePointer;
	m_psr = frame.psr;
	m_pc = frame.returnAddress;
	m_usp = frame.usp;
	m_asn = frame.asn;
	m_vptptr = frame.vptptr;
	m_uniqueValue = frame.uniqueValue;
	m_astEnable = frame.astEnable;

	setMode(static_cast<helpers_JIT::MmuMode>(frame.psr & 0x3));  // restore CPU mode

	qDebug() << QString("[AlphaCPU%1] Trap return to PC=0x%2")
		.arg(m_cpuId).arg(m_pc, 8, 16, QChar('0'));

	setState(helpers_JIT::CPUState::RUNNING);
}
void AlphaCPU::trapRaised(helpers_JIT::TrapType type, quint64 currentPC)
{
	// Step 1: Save current state
	StackFrame trapFrame(currentPC, m_fp, m_psr, currentPC);
	pushFrame(trapFrame);
	m_savedPsr = m_psr;

	StackFrame frame(currentPC, m_fp, m_psr, currentPC);
	frame.usp = m_usp;
	frame.asn = m_asn;
	frame.vptptr = m_vptptr;
	frame.uniqueValue = m_uniqueValue;
	frame.astEnable = m_astEnable;

	// Step 2: Switch to Kernel Mode
	setMode(helpers_JIT::MmuMode::Kernel);
	setIPL(7);                  // Highest interrupt priority
	setMMUEnabled(true);
	setFPEnabled(false);        // PAL traps disable FP

	// Step 3: Jump to trap vector
	const quint64 trapVector = 0x1000 + (static_cast<int>(type) * 0x100);
	setPC(trapVector);

	// Step 4: Notify
	qCritical().noquote()
		<< "[TRAP]"
		<< trapTypeToString(type)
		<< QString(" | PC=0x%1").arg(currentPC, 8, 16);

	emit trapOccurred(type, currentPC, m_cpuId);
	setState(helpers_JIT::CPUState::TRAPPED);
}

void AlphaCPU::resetRequested()
{
    //TODO  resetRequested()
}

// Instruction Execution

void AlphaCPU::executeNextInstruction()
{
	if (!m_memorySystem) {
		qWarning() << "[AlphaCPU] SafeMemory not available!";
		stopRequested.storeRelaxed(true);
		return;
	}

	// 🔥 Step 1: Try JIT compiled block
	if (m_jitCompiler && m_jitCompiler->hasBlock(m_pc)) {
		m_jitCompiler->runBlock(m_pc, this);
		return;
	}

	// 🔎 Step 2: Translate virtual PC to physical instruction address
	quint64 physAddr = 0;
	if (!translate(m_pc, physAddr, /*accessType=*/2)) {
		raiseTrap(helpers_JIT::TrapType::MMUAccessFault);
		return;
	}

	// 📦 Step 3: Fetch instruction
	quint32 instruction = getSafeMemory()->readUInt32(physAddr);

	// 🛠 Step 4: Debug trace (before PC changes)
	TraceManager::logDebug(
		QString("AlphaCPU%1: Executing PC=0x%2 INST=0x%3")
		.arg(m_cpuId)
		.arg(m_pc, 8, 16, QChar('0'))
		.arg(instruction, 8, 16, QChar('0'))
	);

	// 🧠 Step 5: Execute instruction
	bool branched = decodeAndExecute(instruction);

	// ➡️ Step 6: Advance PC only if not branched
	if (!branched) m_pc += 4;

	// 🔁 Step 7: Trigger JIT compilation if hot
	if (++m_jitHitCounter[m_pc] > m_jitThreshold) {
		m_jitCompiler->compileBlock(m_pc);
	}
}




// Signaled Handlers

void AlphaCPU::handleMemoryWrite(quint64 address, quint64 value, int size)
{
#ifdef QT_DEBUG
	qDebug() << "[MemoryWrite] Address:" << QString("0x%1").arg(address, 8, 16)
		<< "Value:" << QString("0x%1").arg(value, 8, 16)
		<< "Size:" << size;
#endif
}

void AlphaCPU::handleMemoryRead(quint64 address, quint64 value, int size)
{
#ifdef QT_DEBUG
	qDebug() << "[MemoryRead] Address:" << QString("0x%1").arg(address, 8, 16)
		<< "Value:" << QString("0x%1").arg(value, 8, 16)
		<< "Size:" << size;
#endif
}

void AlphaCPU::handleHalt()
{
	qInfo() << "[AlphaCPU] CPU halted.";
	m_running = false;
}

void AlphaCPU::handleReset()
{
	qInfo() << "[AlphaCPU] CPU reset requested.";
	// TODO: Perform reset (e.g., clear state, reset PC to PAL base)
}


