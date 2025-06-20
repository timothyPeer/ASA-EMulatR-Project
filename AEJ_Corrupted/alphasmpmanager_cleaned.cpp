// AlphaSMPManager.cpp - SMP management implementation
#include "AlphaSMPManager.h"
#include "AlphaCPU.h"
#include "AlphaMemorySystem.h"
#include <QDebug>
#include <QThread>


AlphaSMPManager::AlphaSMPManager(int cpuCount, QObject* parent)
	: QObject(parent),
	memorySystem(nullptr),
	m_activeCPUCount(0),
	m_waitingCPUCount(0)
{
	
}

AlphaSMPManager::~AlphaSMPManager()
{
	// Clean up CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		delete m_cpus[i];
	}

	// Memory system is owned by the system, not this manager
}

/*
   This function relies on two vectors.		
   For each CPU, it will be launched through a dedicated thread.
*/
void AlphaSMPManager::startAllCPUs_MoveToThread()
{
	if (m_moved_cpus.count()==0)
		m_moved_cpus.resize(m_cpus.size());			// Initialize the vector size to match the CPU count
	for (int i = 0; i < m_cpus.size(); i++) {
		AlphaCPU* alphaCPU = m_cpus[i];
		m_moved_cpus[i]  = new QThread(this);		// Move this thread to the Moved CPU threads vector
		m_cpus[i]->moveToThread(m_moved_cpus[i]);

		connect(m_moved_cpus[i], &QThread::started, m_cpus[i], &AlphaCPU::startExecution);
		connect(m_cpus[i], &AlphaCPU::executionFinished, m_moved_cpus[i], &QThread::quit);
		connect(m_moved_cpus[i], &QThread::finished, m_moved_cpus[i], &QObject::deleteLater);
		connect(m_cpus[i], &AlphaCPU::halted, m_moved_cpus[i], &QThread::quit);
		connect(this, &AlphaSMPManager::signalStartAll, alphaCPU, &AlphaCPU::startExecution);
		connect(this, &AlphaSMPManager::signalStopAll, alphaCPU, &AlphaCPU::stopExecution);
		connect(this, &AlphaSMPManager::signalResetAll, alphaCPU, &AlphaCPU::resetCPU);
		connect(this, &AlphaSMPManager::signalPauseAll, alphaCPU, &AlphaCPU::pauseExecution);
		connect(this, &AlphaSMPManager::signalResumeAll, alphaCPU, &AlphaCPU::resumeExecution);
		connect(this, &AlphaSMPManager::signalSendInterrupt, alphaCPU, [=](int cpuId, int vector) {
			if (cpuId == i) alphaCPU->receiveInterrupt(vector);
			});

		connect(this, &AlphaSMPManager::signalResumeAll, alphaCPU, &AlphaCPU::resumeExecution);
		connect(alphaCPU, &AlphaCPU::finished, m_moved_cpus[i], &QThread::quit);
		connect(m_moved_cpus[i], &QThread::finished, m_moved_cpus[i], &QObject::deleteLater);
		m_moved_cpus[i]->start();
	}
}


void AlphaSMPManager::applyConfiguration(QString lastLoadedConfig)
{

}
bool AlphaSMPManager::applyConfiguration(const QJsonObject& config)
{
	QJsonObject sys = config.value("System").toObject();
	int cpuCount = sys.value("CPU").toObject().value("Processor-Count").toInt(1);

	// Step 1: stop + clear previous state
	stopExecution();
	qDeleteAll(m_cpus);
	qDeleteAll(m_moved_cpus);
	m_cpus.clear();
	m_moved_cpus.clear();

	// Step 2: recreate CPUs and threads
	for (int i = 0; i < cpuCount; ++i) {
		AlphaCPU* cpu = new AlphaCPU(i, memorySystem, this);
		QThread* thread = new QThread(this);

		cpu->moveToThread(thread);
		connect(thread, &QThread::started, cpu, &AlphaCPU::startExecution);

		m_cpus.append(cpu);
		m_moved_cpus.append(thread);
		thread->start();
	}

	// Optional: load memory size, ROM images, etc.
	// Optional: set PC for CPU0 to SRM entry point
	if (!m_cpus.isEmpty()) {
		m_cpus[0]->setPC(0x21000000);  // SRM PALcode vector
	}

	return true;
}

void AlphaSMPManager::reset()
{
	stopExecution();

	// Clean up all CPU threads
	for (QThread* thread : m_moved_cpus) {
		if (thread) {
			thread->quit();
			thread->wait();
			thread->deleteLater();
		}
	}

	m_moved_cpus.clear();

	// Clear CPU objects
	for (AlphaCPU* cpu : m_cpus) {
		delete cpu;
	}

	m_cpus.clear();

	// Reload configuration (e.g., from file)
	applyConfiguration(lastLoadedConfig);

	// Optionally: start all again
	startExecution();
}

void AlphaSMPManager::startCpu(int cpuId, quint64 pc)
{
	if (cpuId < 0 || cpuId >= m_cpus.size()) {
		qWarning() << "[AlphaSMP] Invalid CPU index:" << cpuId;
		return;
	}

	AlphaCPU* cpu = m_cpus[cpuId];
	if (!cpu) return;

	// Set the program counter
	QMetaObject::invokeMethod(cpu, "setPC", Qt::QueuedConnection, Q_ARG(quint64, pc));

	// Start CPU execution
	QMetaObject::invokeMethod(cpu, "resumeExecution", Qt::QueuedConnection);
}

void AlphaSMPManager::setIoThreadCount(int count)
{
	ioThreadCount = count;
}



void AlphaSMPManager::initialize()
{
	// Create the memory system if not provided
	if (!memorySystem) {
		memorySystem = new AlphaMemorySystem(this);
	}

	// Create and initialize CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		if (!m_cpus[i])
		{
			m_cpus[i] = new AlphaCPU(i, memorySystem, this);
			// set properties
			m_cpus[i]->setJitEnabled(this->jitEnabled);
			m_cpus[i]->setJitThreshold(this->jitThreshold);
			m_cpus[i]->setMMUEnabled(true);  // Supports / Virtual and Flat / let's default this for now.
			m_cpus[i]->setOptimizationLevels(jitOptimizationLevel());

			// Connect signals

			// Add this in the initialize() method when connecting signals:
			connect(m_cpus[i], &AlphaCPU::memoryAccessed,
				this, [this, i](quint64 address, bool isWrite, int size) {
					if (isWrite) {
						this->handleMemoryWrite(i, address, size);
					}
				});

			// Add these new connections:

			// Connect critical CPU signals to AlphaSMPManager slots
			connect(m_cpus[i], &AlphaCPU::halted, this, &AlphaSMPManager::handleCpuHalted);
			connect(m_cpus[i], &AlphaCPU::trapRaised, this, &AlphaSMPManager::handleTrapRaised);
			connect(m_cpus[i], &AlphaCPU::stateChanged, this, &AlphaSMPManager::handleCpuStateChanged);
			connect(m_cpus[i], &AlphaCPU::memoryAccessed, this, &AlphaSMPManager::handleMemoryAccessed);
			connect(m_cpus[i], &AlphaCPU::processingProgress, this, [this, i](int percentComplete) {
				emit cpuProgress(i, percentComplete); });
			connect(m_cpus[i], &AlphaCPU::operationStatus, this, [this, i](const QString& message) {
				emit cpuStatusUpdate(i, message);   });
			connect(m_cpus[i], &AlphaCPU::cycleExecuted, this, [this, i](quint64 cycle) { // Optionally handle cycle completion	});
			connect(m_cpus[i], &AlphaCPU::stateChanged, this, [this, i](helpers_JIT::CPUState newState) {
				emit cpuStateChanged(i, newState); });
			connect(m_cpus[i], &AlphaCPU::halted, this, &AlphaSMPManager::handleCpuHalted);
			connect(this, &AlphaSMPManager::signalSendInterrupt, m_cpus[i], [=](int cpuId, int vector) {
				if (cpuId == i)
					m_cpus[i]->receiveInterrupt(vector);
				});
			connect(m_cpus[i], &AlphaCPU::finished, thread, &QThread::quit);

			// Connect global control signals
			connect(this, &AlphaSMPManager::signalStartAll, m_cpus[i], &AlphaCPU::startExecution);
			connect(this, &AlphaSMPManager::signalStopAll, m_cpus[i], &AlphaCPU::stopExecution);
			connect(this, &AlphaSMPManager::signalResetAll, m_cpus[i], &AlphaCPU::resetCPU);
			connect(this, &AlphaSMPManager::signalPauseAll, m_cpus[i], &AlphaCPU::pauseExecution);
			connect(this, &AlphaSMPManager::signalResumeAll, m_cpus[i], &AlphaCPU::resumeExecution);
	

		
			// Connect per-CPU interrupt
		
			connect(thread, &QThread::finished, thread, &QObject::deleteLater);

				// Initialize the CPU
			m_cpus[i]->initialize();
			
		}
		emit systemInitialized();

		qDebug() << "SMP manager initialized with" << m_cpus.size() << "CPUs";
	}
}


	

	//<setJitProperties(bool jitEnabled, int jitThreshold_) {

	}
void AlphaSMPManager::cycleExecuted()
{
	//TODO
}

void configureSystem(int cpuCount, quint64 ramSizeMB, quint64 startPC)
{
	if (!memorySystem) {
		memorySystem = new AlphaMemorySystem(this);
	}

	memorySystem->initialize(ramSizeMB);

	for (int i = 0; i < cpuCount; ++i) {
		AlphaCPU* cpu = new AlphaCPU(i, memorySystem, this);
		cpu->setPC(startPC);

		// Connect critical CPU signals to AlphaSMPManager slots
		connect(cpu, &AlphaCPU::halted, this, &AlphaSMPManager::handleCpuHalted);
		connect(cpu, &AlphaCPU::trapRaised, this, &AlphaSMPManager::handleTrapRaised);
		connect(cpu, &AlphaCPU::stateChanged, this, &AlphaSMPManager::handleCpuStateChanged);
		connect(cpu, &AlphaCPU::memoryAccessed, this, &AlphaSMPManager::handleMemoryAccessed);

		m_cpus.append(cpu);
	}
}

void AlphaSMPManager::shutdown()
{
	// Stop all CPUs
	stopAllCPUs();

	qDebug() << "SMP manager shutdown";
}

AlphaCPU* AlphaSMPManager::getCPU(int index)
{
	if (index >= 0 && index < m_cpus.size()) {
		return m_cpus[index];
	}
	return nullptr;
}

void AlphaSMPManager::startSystem(quint64 entryPoint)
{
	// Set entry point for all CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		m_cpus[i]->setPC(entryPoint);
	}

	// Start all CPUs
	startAllCPUs();

	emit systemStarted();

	qDebug() << "System started at entry point" << Qt::hex << entryPoint;
}

void AlphaSMPManager::startSystem()
{

}

void AlphaSMPManager::pauseSystem()
{
	// Pause all CPUs
	pausedAllCPUs();

	emit systemPaused();

	qDebug() << "System paused";
}

void AlphaSMPManager::resumeSystem()
{
	// Resume all CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		if (m_cpus[i]->getState() == helpers_JIT::CPUState::PAUSED) {
			m_cpus[i]->startExecution();
		}
	}

	emit systemResumed();

	qDebug() << "System resumed";
}

void AlphaSMPManager::stopSystem()
{
	// Stop all CPUs
	stopAllCPUs();

	emit systemStopped();

	qDebug() << "System stopped";
}
void AlphaSMPManager::stoppedSystem()
{
// TODO
}

void AlphaSMPManager::startFromPALBase()
{

}

void AlphaSMPManager::cpusAllStarted() {

}
void AlphaSMPManager::setTraceLevel(int traceLevel)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaSMPManager::startAllCPUs()
{
	// Start all CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		m_cpus[i]->startExecution();
	}

	// Update active count
	m_activeCPUCount = m_cpus.size();

	emit allCPUsStarted();

	qDebug() << "All CPUs started";
}

void AlphaSMPManager::pauseAllCPUs()
{
	// Pause all CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		m_cpus[i]->pauseExecution();
	}

	emit allCPUsPaused();

	qDebug() << "All CPUs paused";
}


void AlphaSMPManager::pausedAllCPUs()
{
	//TODO - allCPUsPaused slot
}

void AlphaSMPManager::stopAllCPUs()
{
	for (int i = 0; i < m_cpus.size(); ++i) {
		AlphaCPU* alphaCPU = m_cpus[i];
		QThread* thread = m_moved_cpus[i];

		if (alphaCPU && thread) {
			qDebug() << QString("[AlphaSMP] Requesting CPU%1 to stop").arg(i);
			QMetaObject::invokeMethod(alphaCPU, "requestStop", Qt::QueuedConnection);  // ensures it runs in the CPU's thread

			// Optionally emit a custom signal if cpu->requestStop() isn't public
			// emit signalStopCpu(i);
		}
	}

	// Give CPUs time to stop gracefully
	QThread::msleep(10);

	// Wait for all threads to finish
	for (int i = 0; i < m_moved_cpus.size(); ++i) {
		QThread* thread = m_moved_cpus[i];
		if (thread) {
			qDebug() << QString("[AlphaSMP] Waiting for CPU thread %1 to finish...").arg(i);
			thread->quit();
			thread->wait();  // Blocks until thread completes
			thread->deleteLater();
		}
	}

	m_moved_cpus.clear();  // Clean up thread list
}

void AlphaSMPManager::sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector)
{
	// Validate CPU indices
	if (targetCPU < 0 || targetCPU >= m_cpus.size()) {
		qDebug() << "Invalid target CPU index:" << targetCPU;
		return;
	}

	// Send the interrupt
	m_cpus[targetCPU]->handleInterrupt(interruptVector);

	emit interprocessorInterruptSent(sourceCPU, targetCPU, interruptVector);

	qDebug() << "CPU" << sourceCPU << "sent interrupt vector" << interruptVector << "to CPU" << targetCPU;
}

void AlphaSMPManager::broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector)
{
	// Send interrupt to all CPUs except source
	for (int i = 0; i < m_cpus.size(); i++) {
		if (i != sourceCPU) {
			sendInterprocessorInterrupt(sourceCPU, i, interruptVector);
		}
	}

	qDebug() << "CPU" << sourceCPU << "broadcast interrupt vector" << interruptVector;
}



void AlphaSMPManager::handleMemoryWrite(int cpuId, quint64 address, int size)
{
	// Handle cache coherency
	handleMemoryCoherency(address, cpuId);
}

void AlphaSMPManager::invalidateCacheLine(int cpuId, quint64 address)
{
	// In a real implementation, this would invalidate the cache line on the specified CPU
	qDebug() << "Invalidating cache line at address" << Qt::hex << address << "on CPU" << cpuId;
}

void AlphaSMPManager::resetCPUs()
{
	for (int i = 0; i < m_cpus.size(); ++i) {
		QMetaObject::invokeMethod(m_cpus[i], "resetCPU", Qt::QueuedConnection);
	}
}

void AlphaSMPManager::resumeExecution()
{
	for (int i = 0; i < m_cpus.size(); ++i) {
		QMetaObject::invokeMethod(m_cpus[i], "resumeExecution", Qt::QueuedConnection);
	}
}

void AlphaSMPManager::startExecution()
{
	for (int i = 0; i < m_cpus.size(); ++i) {
		if (m_moved_cpus[i] && m_moved_cpus[i]->isRunning()) {
			QMetaObject::invokeMethod(m_cpus[i], "startExecution", Qt::QueuedConnection);
		}
	}
}

void AlphaSMPManager::stopExecution()
{
	for (int i = 0; i < m_cpus.size(); ++i) {
		QMetaObject::invokeMethod(m_cpus[i], "requestStop", Qt::QueuedConnection);
	}

	QThread::msleep(10);

	for (auto* thread : m_moved_cpus) {
		if (thread) {
			thread->quit();
			thread->wait();
		}
	}
}

void AlphaSMPManager::waitForAllCPUs()
{
	QMutexLocker locker(&m_barrierLock);

	// Increment waiting count
	m_waitingCPUCount++;

	if (m_waitingCPUCount < m_activeCPUCount) {
		// Not all CPUs are waiting yet
		m_barrierCondition.wait(&m_barrierLock);
	}
	else {
		// All CPUs are waiting, reset the barrier
		resetBarrier();

		// Wake up all waiting CPUs
		m_barrierCondition.wakeAll();
	}
}

void AlphaSMPManager::releaseAllCPUs()
{
	QMutexLocker locker(&m_barrierLock);

	// Reset the barrier
	resetBarrier();

	// Wake up all waiting CPUs
	m_barrierCondition.wakeAll();

	qDebug() << "Released all CPUs from barrier";
}

void AlphaSMPManager::synchronizeBarrier()
{
	// This is a simple implementation of a barrier synchronization
	waitForAllCPUs();
}

void AlphaSMPManager::resetBarrier()
{
	// Reset the waiting count
	m_waitingCPUCount = 0;
}

void AlphaSMPManager::handleMemoryCoherency(quint64 address, int sourceCPU)
{
	QMutexLocker locker(&m_smpLock);

	// Get the cache line address (assume 64-byte cache lines)
	quint64 cacheLine = address & ~0x3F;

	// Check if any other CPUs have this line in their cache
	if (m_sharedCacheLines.contains(cacheLine)) {
		const QSet<int>& sharers = m_sharedCacheLines[cacheLine];

		// Invalidate the line on all other CPUs
		for (int cpuId : sharers) {
			if (cpuId != sourceCPU) {
				invalidateCacheLine(cpuId, cacheLine);
			}
		}
	}

	// Update sharing information
	updateSharedCacheStatus(cacheLine, sourceCPU, true);

	emit cacheCoherencyEvent(sourceCPU, cacheLine);
}

void AlphaSMPManager::updateSharedCacheStatus(quint64 address, int cpuId, bool isSharing)
{
	// Get the cache line address
	quint64 cacheLine = address & ~0x3F;

	if (isSharing) {
		// Add CPU to sharers
		if (!m_sharedCacheLines.contains(cacheLine)) {
			m_sharedCacheLines[cacheLine] = QSet<int>();
		}
		m_sharedCacheLines[cacheLine].insert(cpuId);
	}
	else {
		// Remove CPU from sharers
		if (m_sharedCacheLines.contains(cacheLine)) {
			m_sharedCacheLines[cacheLine].remove(cpuId);

			// Remove empty sets
			if (m_sharedCacheLines[cacheLine].isEmpty()) {
				m_sharedCacheLines.remove(cacheLine);
			}
		}
	}
}

#include <QDebug>


/**
	 * @brief Notify that a CPU halted.
	 *
 */
void AlphaSMPManager::handleCpuHalted()
{
	//// Notify that a CPU halted. You could check if all CPUs halted together.
#ifdef QT_DEBUG
	qDebug() << "[AlphaSMPManager] CPU halted signal received.";
#endif
	// TODO: Check if ALL CPUs halted, maybe trigger full system halt
}

/**
 * @brief Handle critical system-wide traps.
 *
 * @param TrapType - is one of:
 * 
		PrivilegeViolation,    ///< Access violation due to privilege level
		MMUAccessFault,        ///< Memory management unit fault
		FloatingPointDisabled, ///< FP instruction when FP disabled
		ReservedInstruction    ///< Unimplemented instruction)
 */
void AlphaSMPManager::handleTrapRaised(helpers_JIT::TrapType trap)
{
#ifdef QT_DEBUG
	qDebug() << "[AlphaSMPManager] Trap raised:" << static_cast<int>(trap);
#endif
	// TODO: Handle system-wide traps like Machine Check, Reset, etc.
}

/**
 * @brief Update GUI or monitor CPU status (Running, Paused).
 *
 * @param int - is one of: 
 *		IDLE,
		RUNNING,
		PAUSED,
		WAITING_FOR_INTERRUPT,
		EXCEPTION_HANDLING,
		HALTED
 */
void AlphaSMPManager::handleCpuStateChanged(int newState)
{
#ifdef QT_DEBUG
	qDebug() << "[AlphaSMPManager] CPU state changed to:" << newState;
#endif
	// States like Running, Paused, Halted
}

/**
 * @brief Debug memory accesses or set up traps/breakpoints later.
 *
 *	This is a convenience event only.
 */
void AlphaSMPManager::handleMemoryAccessed(quint64 address, quint64 value, int size, bool isWrite)
{
#ifdef QT_DEBUG
	QString accessType = isWrite ? "Write" : "Read";
	qDebug() << "[AlphaSMPManager] Memory Access:" << accessType
		<< "Address:" << QString("0x%1").arg(address, 8, 16)
		<< "Value:" << QString("0x%1").arg(value, 8, 16)
		<< "Size:" << size;
#endif
	// Optional: Implement breakpoints, watchpoints, memory logging
	// TODO: Implement breakpoints, watchpoints, memory logging
}



