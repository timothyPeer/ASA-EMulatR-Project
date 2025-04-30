
// AlphaExceptionHandler.cpp - Exception handling implementation
#include "AlphaExceptionHandler.h"
#include "AlphaCPU.h"
#include "AlphaSMPManager.h"
#include <QDebug>

// AlphaExceptionHandler.cpp - Complete implementation
#include "alphaexceptionhandler.h"
#include "AlphaCPU.h"
#include "AlphaSMPManager.h"
#include <QDebug>
#include "alphaexceptionvectors.h"

AlphaExceptionHandler::AlphaExceptionHandler(AlphaSMPManager* smpManager, QObject* parent)
	: QObject(parent), m_smpManager(smpManager)
{
}

AlphaExceptionHandler::~AlphaExceptionHandler()
{
	// Clean up
}

void AlphaExceptionHandler::initialize()
{
	// Connect to CPU exception signals
	for (int i = 0; i < m_smpManager->getCPUCount(); i++) {
		AlphaCPU* cpu = m_smpManager->getCPU(i);

		connect(cpu, &AlphaCPU::exceptionRaised, this, [this, i](helpers_JIT::ExceptionType exceptionType, quint64 pc, quint64 faultAddr) {
			handleException(i, exceptionType, pc, faultAddr);
			});

		connect(cpu, &AlphaCPU::trapOccurred,
			this, [this, i](helpers_JIT::ExceptionType trapType, quint64 pc) {
				this->handleTrap(i, trapType, pc);
			});
	}

	qDebug() << "Exception handler initialized";
}

void AlphaExceptionHandler::registerTrapHandler(helpers_JIT::ExceptionType trapType, QObject* receiver, const char* method)
{
	QMutexLocker locker(&m_handlerLock);

	// Add to handler registry
	if (!m_trapHandlers.contains(trapType)) {
		m_trapHandlers[trapType] = QList<QPair<QObject*, const char*>>();
	}

	m_trapHandlers[trapType].append(qMakePair(receiver, method));

	qDebug() << "Registered handler for trap type" << static_cast<quint64>(trapType);
}

void AlphaExceptionHandler::unregisterTrapHandler(helpers_JIT::ExceptionType trapType, QObject* receiver, const char* method)
{
	QMutexLocker locker(&m_handlerLock);

	// Remove from handler registry
	if (m_trapHandlers.contains(trapType)) {
		QList<QPair<QObject*, const char*>>& handlers = m_trapHandlers[trapType];

		for (int i = 0; i < handlers.size(); i++) {
			if (handlers[i].first == receiver && qstrcmp(handlers[i].second, method) == 0) {
				handlers.removeAt(i);
				break;
			}
		}

		// Remove empty lists
		if (handlers.isEmpty()) {
			m_trapHandlers.remove(trapType);
		}
	}

	qDebug() << "Unregistered handler for trap type" << static_cast<quint64>(trapType);
}

void AlphaExceptionHandler::handleException(int cpuId, helpers_JIT::ExceptionType exceptionType, quint64 pc, quint64 faultAddr)
{
	qDebug() << "Handling exception" << static_cast<quint64>(exceptionType) << "on CPU" << cpuId
		<< "at PC =" << Qt::hex << pc << "fault address =" << Qt::hex << faultAddr;

	// Dispatch to kernel
	dispatchToKernel(cpuId, exceptionType, pc, faultAddr);

	emit exceptionHandled(cpuId, exceptionType);
}

void AlphaExceptionHandler::handleTrap(int cpuId, helpers_JIT::ExceptionType trapType, quint64 pc)
{
	qDebug() << "Handling trap" << static_cast<quint64>(trapType) << "on CPU" << cpuId << "at PC =" << Qt::hex << pc;

	// Try to call registered handlers
	bool handled = callRegisteredHandlers(trapType, cpuId, pc);

	if (!handled) {
		// No handler found, dispatch to kernel
		dispatchToKernel(cpuId, trapType, pc, 0);
	}

	emit trapHandled(cpuId, trapType);
}

void AlphaExceptionHandler::handleInterrupt(int cpuId, int interruptVector)
{
	qDebug() << "Handling interrupt vector" << interruptVector << "on CPU" << cpuId;

	// In a real implementation, this would dispatch to the appropriate interrupt handler

	emit interruptHandled(cpuId, interruptVector);
}

void AlphaExceptionHandler::handleSystemCall(int cpuId, int callNumber, const QVector<quint64>& params)
{
	qDebug() << "Handling system call" << callNumber << "on CPU" << cpuId;

	// In a real implementation, this would dispatch to the appropriate system call handler
	quint64 result = 0;

	// Handle based on call number
	switch (callNumber) {
	case 1: // Example: Process creation
		if (params.size() >= 1) {
			// Create new process with specified parameters
			result = 100; // Example process ID
		}
		break;

	case 2: // Example: Memory allocation
		if (params.size() >= 1) {
			// Allocate memory of specified size
			result = 0x10000000; // Example allocation address
		}
		break;

		// Add more system calls as needed

	default:
		qDebug() << "Unknown system call:" << callNumber;
		break;
	}

	emit systemCallHandled(cpuId, callNumber, result);
}

void AlphaExceptionHandler::handlePALCall(int cpuId, int palFunction)
{
	qDebug() << "Handling PAL call" << palFunction << "on CPU" << cpuId;

	// In a real implementation, this would dispatch to the appropriate PAL function handler

	// Handle based on PAL function
	switch (palFunction) {
	case 0x0000: // HALT
		// System halt - already handled by CPU
		break;

	case 0x0001: // Privileged context switch
		// Handle context switch
		break;

	case 0x0083: // System call
		// Already handled separately
		break;

		// Add more PAL functions as needed

	default:
		qDebug() << "Unknown PAL function:" << palFunction;
		break;
	}

	emit palCallHandled(cpuId, palFunction);
}
/*
* 
* Visual: 
* AlphaCPU raises Exception 
   → AlphaExceptionHandler lookup ExceptionType 
     → Find Vector Address 
	   → Dispatch to Kernel PC

* the address of the exceptionVector is calculated
enum class ExceptionType {

	// ------------- Core Arithmetic Exceptions -------------
	ARITHMETIC_TRAP					 = 0,   // Vector = 0x100
	ILLEGAL_INSTRUCTION				 = 2,   // Vector = 0x200
	PRIVILEGED_INSTRUCTION			 = 3,   // Vector = 0x280
	ALIGNMENT_FAULT				     = 4,   // Vector = 0x300

	// ------------- Memory Exceptions -------------
	MEMORY_ACCESS_VIOLATION			= 5,   // Vector = 0x380
	MEMORY_READ_FAULT,                // = 6, Vector = 0x400
	MEMORY_WRITE_FAULT,               // = 7, Vector = 0x480
	MEMORY_EXECUTE_FAULT,             // = 8, Vector = 0x500
	MEMORY_ALIGNMENT_FAULT,           // = 9, Vector = 0x580
	PAGE_FAULT,                       // = 10, Vector = 0x600

	// ------------- Arithmetic Exceptions -------------
	INTEGER_OVERFLOW,                 // = 11, Vector = 0x680
	INTEGER_DIVIDE_BY_ZERO,           // = 12, Vector = 0x700
	FLOATING_POINT_OVERFLOW,          // = 13, Vector = 0x780
	FLOATING_POINT_UNDERFLOW,         // = 14, Vector = 0x800
	FLOATING_POINT_DIVIDE_BY_ZERO,    // = 15, Vector = 0x880
	FLOATING_POINT_INVALID,           // = 16, Vector = 0x900

	// ------------- Instruction Exceptions -------------
	RESERVED_OPERAND,                 // = 17, Vector = 0x980

	// ------------- System Exceptions -------------
	MACHINE_CHECK,                    // = 18, Vector = 0xA00
	BUS_ERROR,                        // = 19, Vector = 0xA80
	SYSTEM_CALL,                      // = 20, Vector = 0xB00
	BREAKPOINT,                       // = 21, Vector = 0xB80

	// ------------- External Exceptions -------------
	INTERRUPT,                        // = 22, Vector = 0xC00
	HALT,                             // = 23, Vector = 0xC80

	// ------------- Other -------------
	UNKNOWN_EXCEPTION                 // = 24, Vector = 0xD00
};

...

	*/

void AlphaExceptionHandler::dispatchToKernel(int cpuId, helpers_JIT::ExceptionType exceptionType, quint64 pc, quint64 faultAddr)
{
	// Get the CPU
	AlphaCPU* cpu = m_smpManager->getCPU(cpuId);
	if (!cpu) {
		return;
	}

	// Save the process context
	saveProcessContext(cpuId);

	// Switch to kernel mode
	emit kernelModeSwitched(cpuId, true);




	// In a real implementation, this would calculate the appropriate exception vector
	// and transfer control to the kernel's exception handler
	quint64 exceptionVector = 0x100 + (static_cast<quint64>(exceptionType) * 0x80);

	// Set PC to exception vector
	cpu->setPC(exceptionVector);

	qDebug() << "Dispatched exception" << QString::number(exceptionVector) << "to kernel vector" << Qt::hex << exceptionVector;
}


void AlphaExceptionHandler::dumpException(int cpuId, helpers_JIT::ExceptionType exceptionType)
{
	for (const auto& entry : AlphaExceptionVectorTable) {
		if (entry.exceptionType == exceptionType) {
			qDebug() << "CPU" << cpuId << "Exception:" << entry.description
				<< "(Vector 0x" << Qt::hex << entry.vectorAddress << ")";
			return;
		}
	}
	qDebug() << "CPU" << cpuId << "Unknown exception" << static_cast<int>(exceptionType);
}

void AlphaExceptionHandler::saveProcessContext(int cpuId)
{
	// Get the CPU
	AlphaCPU* cpu = m_smpManager->getCPU(cpuId);
	if (!cpu) {
		return;
	}

	// Create a new context (or retrieve existing one)
	ProcessContext context;
	context.processId = 1; // Assume process ID 1 for now
	context.pc = cpu->getProgramCounter();

	// Save registers
	for (int i = 0; i < 32; i++) {
		context.registers.append(cpu->getRegister(i, helpers_JIT::RegisterType::INTEGER_REG));
		context.fpRegisters.append(cpu->getRegister(i, helpers_JIT::RegisterType::FLOAT_REG));
	}

	// Save the context
	m_processContexts[cpuId] = context;

	qDebug() << "Saved process context for CPU" << cpuId;
}

void AlphaExceptionHandler::restoreProcessContext(int cpuId, int processId)
{
	// Get the CPU
	AlphaCPU* cpu = m_smpManager->getCPU(cpuId);
	if (!cpu) {
		return;
	}

	// Check if we have a saved context
	if (!m_processContexts.contains(cpuId)) {
		qDebug() << "No saved context for CPU" << cpuId;
		return;
	}

	// Get the context
	const ProcessContext& context = m_processContexts[cpuId];

	// Check if this is the requested process
	if (context.processId != processId) {
		qDebug() << "Context mismatch: requested" << processId << "but have" << context.processId;
		return;
	}

	// Restore PC
	cpu->setPC(context.pc);

	// Restore registers
	for (int i = 0; i < qMin(32, context.registers.size()); i++) {
		cpu->setRegister(i, context.registers[i], helpers_JIT::RegisterType::INTEGER_REG);

		if (i < context.fpRegisters.size()) {
			cpu->setRegister(i, context.fpRegisters[i], helpers_JIT::RegisterType::FLOAT_REG);
		}
	}

	// Notify about context switch
	emit contextSwitched(cpuId, 0, context.processId);

	qDebug() << "Restored process context for CPU" << cpuId;
}

bool AlphaExceptionHandler::callRegisteredHandlers(helpers_JIT::ExceptionType trapType, int cpuId, quint64 pc)
{
	QMutexLocker locker(&m_handlerLock);

	// Check if we have handlers for this trap type
	if (!m_trapHandlers.contains(trapType)) {
		return false;
	}

	// Call all registered handlers
	const QList<QPair<QObject*, const char*>>& handlers = m_trapHandlers[trapType];
	bool handled = false;

	for (const auto& handler : handlers) {
		QObject* receiver = handler.first;
		const char* method = handler.second;

		// Invoke the method
		bool result = false;
		QMetaObject::invokeMethod(receiver, method, Q_RETURN_ARG(bool, result),
			Q_ARG(int, cpuId), Q_ARG(quint64, pc));

		if (result) {
			handled = true;
		}
	}

	return handled;
}
