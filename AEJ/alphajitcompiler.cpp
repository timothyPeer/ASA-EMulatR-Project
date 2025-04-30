// AlphaJITCompiler.cpp - JIT compiler implementation
 #include "AlphaJITCompiler.h"
 #include "AlphaInstructionDecoder.h"
 #include <QDebug>
 #include "Helpers.h"
 #include "TraceManager.h"
// 
 AlphaJITCompiler::AlphaJITCompiler(QObject* parent)
 	: QObject(parent),
 	m_compilerThread(nullptr),
 	m_running(0),
 	m_paused(0),
 	m_optimizationLevel(helpers_JIT::OptimizationLevel::BASIC)
 {
 }
// 
 AlphaJITCompiler::~AlphaJITCompiler()
 {
 	// Ensure compiler thread is stopped
 	shutdown();
 }
// 
void AlphaJITCompiler::initialize()
{
	// Create compiler thread
 	m_compilerThread = new QThread();
 	m_compilerThread->setObjectName("Alpha-JIT-Compiler");
 
 	// Move this object to the compiler thread
 	this->moveToThread(m_compilerThread);
// 
 	// Start the thread
 	m_compilerThread->start();
 
 	qDebug() << "JIT compiler initialized";
 }
 
void AlphaJITCompiler::shutdown()
{
 	// Stop the compiler
 	stopCompiler();
 
 	// Wait for thread to finish if it exists
 	if (m_compilerThread) {
 		m_compilerThread->quit();
 		m_compilerThread->wait();
 		delete m_compilerThread;
 		m_compilerThread = nullptr;
 	}
// 
 	qDebug() << "JIT compiler shutdown";
 }
// 
void <AlphaJITCompiler::setOptimizationLevel(helpers_JIT::OptimizationLevel level)
{
 	m_optimizationLevel = level;
 /*	qDebug() << "JIT compiler optimization level set to" << level;*/
 }
// 
void AlphaJITCompiler::compileBlock(quint64 startAddr, const QByteArray& instructions)
{
 	QMutexLocker locker(&m_compilerLock);
// 
 	// Add to compilation queue
 	m_compilationQueue.enqueue(qMakePair(startAddr, instructions));
 
 	// Signal that work is available
 	m_workAvailable.wakeOne();
 
 	qDebug() << "Added block at address" << Qt::hex << startAddr << "to compilation queue, size:"
 		<< instructions.size() << "bytes";
}
// 
 bool AlphaJITCompiler::compileInstruction(quint64 address, quint32 instruction)
 {
 	if (isRecognizedAndSupported(instruction)) {
 		emitNativeCode(instruction);
 		return true; // ✅ Successfully JIT compiled
 	}
 	else {
 		emitFallbackCall(address, instruction);
 		return false; // ❌ Not handled, fallback needed
 	}
 }
void AlphaJITCompiler::invalidateBlock(quint64 startAddr)
{
 	QMutexLocker locker(&m_compilerLock);
 
 	// Remove from compilation queue if present
 	for (int i = 0; i < m_compilationQueue.size(); i++) {
 		if (m_compilationQueue[i].first == startAddr) {
 			m_compilationQueue.removeAt(i);
 			break;
 		}
 	}
 
 	qDebug() << "Invalidated block at address" << Qt::hex << startAddr;
}
// 
void AlphaJITCompiler::prioritizeCompilation(quint64 startAddr)
{
 	QMutexLocker locker(&m_compilerLock);
 
 	// Add to priority queue
 	if (!m_priorityQueue.contains(startAddr)) {
 		m_priorityQueue.enqueue(startAddr);
 
 		// Signal that work is available
 		m_workAvailable.wakeOne();
 
 		qDebug() << "Prioritized compilation of block at address" << Qt::hex << startAddr;
 	}
}
// 
void AlphaJITCompiler::startCompiler()
{
 	// Set running flag
 	m_running = 1;
 	m_paused = 0;
 
 	// Start compiler thread
 	QMetaObject::invokeMethod(this, "compilerThreadMain", Qt::QueuedConnection);
 
 	emit compilerStatusChanged(true);
 
 	qDebug() << "JIT compiler started";
 }
 
void AlphaJITCompiler::stopCompiler()
{
 	// Clear running flag
 	m_running = 0;
 
 	// Wake up thread if it's waiting
 	m_workAvailable.wakeAll();
 
 	emit compilerStatusChanged(false);
 
 	qDebug() << "JIT compiler stopped";
}
// 
void AlphaJITCompiler::pauseCompiler()
{
 	// Set paused flag
 	m_paused = 1;
 
 	qDebug() << "JIT compiler paused";
}
// 
void AlphaJITCompiler::resumeCompiler()
{
 	// Clear paused flag
 	m_paused = 0;
 
 	// Wake up thread if it's waiting
 	m_workAvailable.wakeAll();
 
 	qDebug() << "JIT compiler resumed";
 }
 
void AlphaJITCompiler::trapRaised(helpers_JIT::TrapType trapType)
{
 	TraceManager::logInfo(QString("Trap raised: %1").arg(static_cast<int>(trapType)));
 	//TODO trapRaised(helpers_JIT::TrapType trapType)
}
// 
void AlphaJITCompiler::handleTrap(helpers_JIT::TrapType trapType)
// {
// 	TraceManager::logInfo(QString("Trap raised: %1").arg(static_cast<int>(trapType)));
// 	//TODO handleTrap(helpers_JIT::TrapType trapType)
// 
// }
// 
// //<AlphaJITCompiler::compilerThreadMain()
// {
// 	m_running.storeRelease(1);
// 	emit compilerStatusChanged(true);
// 
// 
// 
// 	while (m_running.loadAcquire()) {
// 		// Handle paused state with wait condition
// 		if (m_paused.loadAcquire()) {
// 			QMutexLocker locker(&m_compilerLock);
// 			// Wait until resumed or stopped
// 			m_workAvailable.wait(&m_compilerLock);
// 			continue;
// 		}
// 
// 		// Get the next compilation job
// 		QPair<quint64, QByteArray> job;
// 		{
// 			QMutexLocker locker(&m_compilerLock);
// 
// 			// If no work, wait for more
// 			if (m_compilationQueue.isEmpty() && m_priorityQueue.isEmpty()) {
// 				m_workAvailable.wait(&m_compilerLock);
// 				continue;
// 			}
// 
// 			job = getNextCompilationJob();
// 		}
// 
// 		if (!job.second.isEmpty()) {
// 			try {
// 				// Compile the block
// 				QByteArray nativeCode = generateNativeCode(job.first, job.second);
// 
// 				// Signal completion
// 				emit blockCompiled(job.first, nativeCode);
// 			}
// 			catch (const std::exception& e) {
// 				emit compilationError(job.first, QString("Compilation error: %1").arg(e.what()));
// 			}
// 			catch (...) {
// 				emit compilationError(job.first, "Unknown error during compilation");
// 			}
// 		}
// 
// 		// Short sleep to a//<100% CPU usage and allow thread to be interrupted
// 		QThread::msleep(1);
// 	}
// 
// 	emit compilerStatusChanged(false);
// }
// 
// QPair<quint64, QByteArray> AlphaJITCompiler::getNextCompilationJob()
// {
// 	// Priority queue takes precedence
// 	if (!m_priorityQueue.isEmpty()) {
// 		quint64 addr = m_priorityQueue.dequeue();
// 
// 		// Find the corresponding instructions in the normal queue
// 		for (int i = 0; i < m_compilationQueue.size(); i++) {
// 			if (m_compilationQueue[i].first == addr) {
// 				return m_compilationQueue.takeAt(i);
// 			}
// 		}
// 	}
// 
// 	// Otherwise take the next job from the normal queue
// 	if (!m_compilationQueue.isEmpty()) {
// 		return m_compilationQueue.dequeue();
// 	}
// 
// 	// No job available
// 	return qMakePair(0, QByteArray());
// }
// 
// QByteArray AlphaJITCompiler::generateNativeCode(quint64 startAddr, const QByteArray& alphaCode)
// {
// 	// Convert the block to intermediate representation
// 	QByteArray ir = decodeToIntermediateRepresentation(alphaCode);
// 
// 	// Apply optimizations
// 	applyOptimizations(ir);
// 
// 	// Generate machine code
// 	QByteArray machineCode = generateMachineCode(ir);
// 
// 	return machineCode;
// }
// 
// QByteArray AlphaJITCompiler::decodeToIntermediateRepresentation(const QByteArray& alphaCode)
// {
// 	// In a real implementation, this would decode Alpha instructions to an IR
// 	// For this simulation, we'll just use a placeholder
// 	qDebug() << "Decoding" << alphaCode.size() / 4 << "Alpha instructions to IR";
// 
// 	// Create a simple IR (this is just a placeholder)
// 	QByteArray ir;
// 	ir.append("IR_VERSION=1\n");
// 
// 	// Process each instruction
// 	for (int i = 0; i < alphaCode.size(); i += 4) {
// 		quint32 instruction;
// 		memcpy(&instruction, alphaCode.constData() + i, 4);
// 
// 		// Add to IR (placeholder)
// 		QString irLine = QString("INSTR_%1=%2\n").arg(i / 4).arg(instruction, 8, 16, QChar('0'));
// 		ir.append(irLine.toUtf8());
// 	}
// 
// 	return ir;
// }
// 
// //<AlphaJITCompiler::applyOptimizations(QByteArray& ir)
// {
// 	// Apply optimizations based on level
// 	switch (m_optimizationLevel) {
// 	case helpers_JIT::OptimizationLevel::AGGRESSIVE:
// 		applyInstructionScheduling(ir);
// 		// Fall through
// 
// 	case helpers_JIT::OptimizationLevel::ADVANCED:
// 		applyCommonSubexpressionElimination(ir);
// 		applyRegisterAllocation(ir);
// 		// Fall through
// 
// 	case helpers_JIT::OptimizationLevel::BASIC:
// 		applyConstantFolding(ir);
// 		eliminateDeadCode(ir);
// 		break;
// 
// 	case helpers_JIT::OptimizationLevel::NONE:
// 		// No optimizations
// 		break;
// 	}
// }
// 
// QByteArray AlphaJITCompiler::generateMachineCode(const QByteArray& ir)
// {
// 	// In a real implementation, this would generate native machine code
// 	// For this simulation, we'll just use a placeholder
// 	qDebug() << "Generating machine code from IR, size:" << ir.size() << "bytes";
// 
// 	// Create a simple native code representation (this is just a placeholder)
// 	QByteArray nativeCode;
// 	nativeCode.append("NATIVE_CODE_VERSION=1\n");
// 	nativeCode.append(ir);
// 	nativeCode.append("END\n");
// 
// 	return nativeCode;
// }
// 
// //<AlphaJITCompiler::applyConstantFolding(QByteArray& ir)
// {
// 	// In a real implementation, this would perform constant folding
// 	qDebug() << "Applying constant folding optimization";
// 
// 	// This is just a placeholder
// 	ir.append("# Constant folding applied\n");
// }
// 
// //<AlphaJITCompiler::eliminateDeadCode(QByteArray& ir)
// {
// 	// In a real implementation, this would eliminate dead code
// 	qDebug() << "Applying dead code elimination";
// 
// 	// This is just a placeholder
// 	ir.append("# Dead code elimination applied\n");
// }
// 
// //<AlphaJITCompiler::emitFallbackCall(quint64 address, quint32 instruction)
// {
// 	qWarning() << "[JIT WARNING] Unhandled instruction at address"
// 		<< Qt::hex << address
// 		<< "- falling back to interpreter";
// 
// 	// Emit machine code that calls back into the interpreter dispatcher
// 	nativeEmitter.emitCallUnifiedExecutor(address, instruction);
// }
// 
// //<AlphaJITCompiler::applyCommonSubexpressionElimination(QByteArray& ir)
// {
// 	// In a real implementation, this would eliminate common subexpressions
// 	qDebug() << "Applying common subexpression elimination";
// 
// 	// This is just a placeholder
// 	ir.append("# Common subexpression elimination applied\n");
// }
// 
// //<AlphaJITCompiler::applyRegisterAllocation(QByteArray& ir)
// {
// 	// In a real implementation, this would perform register allocation
// 	qDebug() << "Applying register allocation";
// 
// 	// This is just a placeholder
// 	ir.append("# Register allocation applied\n");
// }
// 
// //<AlphaJITCompiler::applyInstructionScheduling(QByteArray& ir)
// {
// 	// In a real implementation, this would schedule instructions for the target CPU
// 	qDebug() << "Applying instruction scheduling";
// 
// 	// This is just a placeholder
// 	ir.append("# Instruction scheduling applied\n");
// }

#include "AlphaJITCompiler.h"
#include "AlphaCPU.h"
#include <QDebug>

AlphaJITCompiler::AlphaJITCompiler() {}

bool AlphaJITCompiler::hasBlock(quint64 pc) const {
	return m_blocks.contains(pc);
}

//<AlphaJITCompiler::runBlock(quint64 pc, AlphaCPU* cpu) {
	auto fn = m_blocks.value(pc, nullptr);
	if (fn) {
		fn(cpu);
	}
	else {
		qWarning() << "[JIT] No compiled block found at PC:" << QString("0x%1").arg(pc, 0, 16);
	}
}

//<AlphaJITCompiler::recordHit(quint64 pc) {
	m_hitCount[pc]++;
}

bool AlphaJITCompiler::shouldCompile(quint64 pc) const {
	return m_hitCount.value(pc, 0) >= m_threshold;
}

//<AlphaJITCompiler::compileBlock(quint64 pc) {
	qDebug() << "[JIT] Compiling block at PC:" << QString("0x%1").arg(pc, 0, 16);
	// TODO: real JIT logic – for now, install dummy stub
	installStub(pc, [](AlphaCPU* cpu) {
		qDebug() << "[JIT] Executed stub block.";
		cpu->setPC(cpu->getPC() + 4); // Simulate instruction
		});
}

//<AlphaJITCompiler::installStub(quint64 pc, std::function<void(AlphaCPU*)> handler) {
	m_blocks[pc] = handler;
}

// Clear the HitCounters
//<AlphaJITCompiler::clear() {
	//m_blocks.clear();
	m_hitCount.clear();
}
// Clear the JIT Block and HitCounters
//<AlphaJITCompiler::clearAll() {
	m_blocks.clear();
	m_hitCount.clear();
}

//<AlphaJITCompiler::setOptimizationLevel(int optimizationLevel)
{
	// Clamp level to valid range (0 = off, 1 = fast, 2 = aggressive)
	if (optimizationLevel < 0) optimizationLevel = 0;
	if (optimizationLevel > 2) optimizationLevel = 2;

	switch (optimizationLevel) {
	case 0:  // Disable JIT by setting a high threshold
		m_threshold = std::numeric_limits<int>::max();
		break;
	case 1:  // Default/faster profiling (less aggressive)
		m_threshold = 100;
		break;
	case 2:  // Aggressive compile-after-few-hits
		m_threshold = 20;
		break;
	default:
		m_threshold = 100;  // Safe default
		break;
	}
}

