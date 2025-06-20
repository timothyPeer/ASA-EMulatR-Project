// AlphaSMPManager.cpp - SMP management implementation
#include "AlphaSMPManager.h"
#include "AlphaCPU.h"
#include "AlphaMemorySystem.h"
#include <QDebug>
#include <QThread>
#include <QMutexLocker>
#include "../AESH/GlobalMacro.h"



AlphaSMPManager::AlphaSMPManager(QObject* parent)
	: QObject(parent),
	m_memorySystem(nullptr),
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
}

/*
   This function relies on two vectors.		
   For each CPU, it will be launched through a dedicated thread.
*/
void AlphaSMPManager::startAllCPUs_MoveToThread(quint64 pc_init)
{
	if (m_moved_cpus.count()==0)
		m_moved_cpus.resize(m_cpus.size());			// Initialize the vector size to match the CPU count
	for (int i = 0; i < m_cpus.size(); i++) {
		m_cpus[i]->setPC(pc_init);
		AlphaCPU* alphaCPU = m_cpus[i];
		m_moved_cpus[i]  = new QThread(this);		// Move this thread to the Moved CPU threads vector
		m_cpus[i]->moveToThread(m_moved_cpus[i]);

		connect(m_moved_cpus[i], &QThread::started, m_cpus[i], &AlphaCPU::onStartExecution);
		connect(m_cpus[i], &AlphaCPU::onExecutionFinished, m_moved_cpus[i], &QThread::quit);
		connect(m_moved_cpus[i], &QThread::finished, m_moved_cpus[i], &QObject::deleteLater);
		connect(m_cpus[i], &AlphaCPU::halted, m_moved_cpus[i], &QThread::quit);
		connect(this, &AlphaSMPManager::signalStartAll, alphaCPU, &AlphaCPU::onStartExecution);
		connect(this, &AlphaSMPManager::signalStopAll, alphaCPU, &AlphaCPU::onStopExecution);
		connect(this, &AlphaSMPManager::signalResetAll, alphaCPU, &AlphaCPU::onResetCPU);
		connect(this, &AlphaSMPManager::signalPauseAll, alphaCPU, &AlphaCPU::onPauseExecution);
		connect(this, &AlphaSMPManager::signalResumeAll, alphaCPU, &AlphaCPU::onResumeExecution);
		connect(m_cpus[i], &AlphaCPU::sigOperationStatus, this, &AlphaSMPManager::onCpuStatusUpdate);
		connect(this, &AlphaSMPManager::signalSendInterrupt, alphaCPU, [=](int cpuId, int vector) {
			if (cpuId == i) alphaCPU->receiveInterrupt(vector);
			});

		connect(this, &AlphaSMPManager::signalResumeAll, alphaCPU, &AlphaCPU::onResumeExecution);
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

void AlphaSMPManager::setIoThreadCount(int count)
{
	ioThreadCount = count;
}



void AlphaSMPManager::initializeCPUs(quint8	cpuCount_)
{
	// Create owned Pointers 
	if (m_safeMemory)	{ 	m_safeMemory.reset(new SafeMemory(this));	}

	quint16 cpuCnt = 1;
	if (m_cpus.count() == 0) 	cpuCnt = cpuCount_;
	// Create and initialize CPUs
	for (int i = 0; i < cpuCnt; i++) {
		if (!m_cpus[i])
		{
			addCPU(i);
			addCPUConnections(i);

			SafeMemory* safeMem = m_alphaMemorySystem->getSafeMemory();
			connect(safeMem, &SafeMemory::sigReservationCleared,
				m_alphaMemorySystem, &AlphaMemorySystem::onClearReservations);
			if (!m_cpus.isEmpty()) { m_cpus[0]->setPC(0x21000000); } // SRM PALcode vector	
		}

	}
}
void AlphaSMPManager::cycleExecuted()
{
	//TODO
}
bool AlphaSMPManager::registerDevice(DeviceInterface* device, quint64 mmioBase, quint64 mmioSize, int irq /*= -1*/)
{
	Q_ASSERT(device);
	Q_ASSERT(m_mmioManager && m_systemBus);

	// Set memory mapping in the device
	device->setMemoryMapping(mmioBase, mmioSize);

	// Register with MMIOManager
	if (!m_mmioManager->mapDevice(device, mmioBase, mmioSize)) {
		qWarning() << "[AlphaSMPManager] MMIO mapDevice failed for" << device->identifier();
		return false;
	}

	// Register with SystemBus
	m_systemBus->mapDevice(device, mmioBase, mmioSize); // This version accepts BusInterface*

	// Assign IRQ line if applicable
	if (irq >= 0 && m_irqControllerPtr) {
		device->setIRQLine(m_irqControllerPtr.data(), irq);
	}

	return true;
}


void AlphaSMPManager::InitializeMemory(int cpuCount, quint64 ramSizeMB, quint64 startPC)
{
		
}

bool AlphaSMPManager::checkLockReservationValid(int cpuId, quint64 cacheLine)
{
	QMutexLocker locker(&m_smpLock);
	// If the CPU has a reservation and the cache line hasn't been written by another CPU
	return m_cpuLockReservations.contains(cpuId) &&
		m_cpuLockReservations[cpuId] == cacheLine &&
		!m_invalidatedCacheLines.contains(cacheLine);
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
/*
 PC to 0x2000 0000. In other words, all four AlphaCPU 
 instances will begin execution with their program‑counter set to: 0x20000000
*/
void AlphaSMPManager::startSystem()
{
	// Set entry point for all CPUs
	for (int i = 0; i < m_cpus.size(); i++) {
		m_cpus[i]->setPC(0x20000000);
	}
	

	// Start all CPUs
	startAllCPUs();

	emit systemStarted();

	qDebug() << QString("System started at entry point: '0x20000000'");
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
		if (m_cpus[i]->getState() == CPUState::PAUSED) {
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

void AlphaSMPManager::notifyStoreConditionalSuccess(int cpuId, quint64 cacheLine)
{
	QMutexLocker locker(&m_smpLock);

	// Mark this cache line as invalidated (causes other CPUs' SCs to fail)
	m_invalidatedCacheLines.insert(cacheLine);

	// Invalidate lock reservations for all other CPUs that locked this cache line
	if (m_cacheLinesWithLocks.contains(cacheLine)) {
		QSet<int> cpusToInvalidate = m_cacheLinesWithLocks[cacheLine];
		for (int otherCpuId : cpusToInvalidate) {
			if (otherCpuId != cpuId) {
				if (m_cpus[otherCpuId]) {
					// Get the JIT compiler for this CPU
					//AlphaJITCompiler* jit = m_cpus[otherCpuId]->getJITCompiler();
// 					if (jit) {
// 						jit->invalidateLockReservation();
// 					}
				}
			}
		}

		// Clear the tracking for this cache line
		m_cacheLinesWithLocks.remove(cacheLine);
	}

	// Clear this CPU's reservation
	m_cpuLockReservations.remove(cpuId);
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

void AlphaSMPManager::signalStartAll()
{

}

void AlphaSMPManager::signalStopAll()
{

}

void AlphaSMPManager::signalResetAll()
{

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


void AlphaSMPManager::signalResumeAll()
{

}

void AlphaSMPManager::signalSendInterrupt(int cpuId, int irqVector)
{

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



void AlphaSMPManager::notifyLockReservation(int cpuId, quint64 cacheLine)
{
	QMutexLocker locker(&m_smpLock);
	m_cpuLockReservations[cpuId] = cacheLine;

	// Optionally track which CPUs have locks on which cache lines
	if (!m_cacheLinesWithLocks.contains(cacheLine)) {
		m_cacheLinesWithLocks[cacheLine] = QSet<int>();
	}
	m_cacheLinesWithLocks[cacheLine].insert(cpuId);
}


void AlphaSMPManager::handleExternalMemoryWrite(quint64 physicalAddress)
{
	// Get all CPUs in the system
	auto allCpus = getAllCpus();

	// For each CPU, convert the physical address to that CPU's virtual address mappings
	for (int i = 0; i < allCpus.size(); i++) {
		if (i == cpuId) continue; // Skip the writing CPU

		AlphaCPU* cpu = allCpus[i];
		if (!cpu) continue;

		// Get the TLB system for this CPU
		TLBSystem* tlbSystem = cpu->getTLBSystem();
		if (!tlbSystem) continue;

		// Get all virtual addresses that map to this physical address
		QVector<quint64> virtualAddresses =
			tlbSystem->getAllVirtualAddressesFromPhysical(physicalAddress);

		// No mappings for this CPU
		if (virtualAddresses.isEmpty()) continue;

		// Get the JIT compiler and cache for this CPU
		//AlphaJITCompiler* jit = cpu->getJITCompiler();
		UnifiedDataCache* cache = cpu->getDataCache();

		// Process each mapping
		for (quint64 virtualAddress : virtualAddresses) {
			// Invalidate lock reservation if applicable
			if (jit) {
				jit->invalidateLockReservationIfMatch(virtualAddress);
			}

			// Invalidate cache line if applicable
			if (cache) {
				cache->invalidateLine(virtualAddress);
			}
		}
	}

	// Record cache coherency event for debugging/monitoring
	emit cacheCoherencyEvent(physicalAddress, cpuId, "Write invalidation");
}

/**
 * @brief Handles memory writes from another CPU, maintaining cache coherency
 *
 * When a CPU writes to memory, this method ensures that other CPUs with
 * cached copies or lock reservations for the same address are properly
 * invalidated, maintaining cache coherency across the SMP system.
 *
 * @param physicalAddress The physical memory address being written to
 */
void AlphaSMPManager::handleExternalMemoryWrite(quint64 physicalAddress)
{
	TraceManager::instance().debug(QString("SMP: External memory write to PA=0x%1 - checking CPU reservations")
		.arg(physicalAddress, 0, 16));

	// Get all CPUs in the system
	auto allCpus = getAllCpus();

	// For each CPU, convert the physical address to that CPU's virtual address mappings
	for (int i = 0; i < allCpus.size(); i++) {
		AlphaCPU* cpu = allCpus[i];
		if (!cpu) continue;

		// Skip the CPU that originated the write (it will be passed in a parameter)
		if (cpu->getCpuId() == m_currentWritingCpuId) continue;

		// Get the TLB system for this CPU
		TLBSystem* tlbSystem = cpu->getTLBSystem();
		if (!tlbSystem) continue;

		// Use the multi-mapping approach for complete coverage
		QVector<quint64> virtualAddresses =
			tlbSystem->getAllVirtualAddressesFromPhysical(physicalAddress);

		// No mappings for this CPU
		if (virtualAddresses.isEmpty()) {
			TraceManager::instance().debug(QString("SMP: CPU%1 has no mappings for PA=0x%2")
				.arg(i)
				.arg(physicalAddress, 0, 16));
			continue;
		}

		// Get the JIT compiler and cache for this CPU
		AlphaJITCompiler* jit = cpu->getJITCompiler();
		UnifiedDataCache* cache = cpu->getDataCache();

		// Process each mapping
		for (quint64 virtualAddress : virtualAddresses) {
			TraceManager::instance().debug(QString("SMP: Invalidating VA=0x%1 for CPU%2 (maps to PA=0x%3)")
				.arg(virtualAddress, 0, 16)
				.arg(i)
				.arg(physicalAddress, 0, 16));

			// Invalidate lock reservation if applicable
			if (jit) {
				jit->invalidateLockReservationIfMatch(virtualAddress);
			}

			// Invalidate cache line if applicable
			if (cache) {
				cache->invalidateLine(virtualAddress);
			}
		}
	}

	// Record cache coherency event for debugging/monitoring
	emit cacheCoherencyEvent(physicalAddress, m_currentWritingCpuId, "Write invalidation");
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

void AlphaSMPManager::pauseExecution() // Slot: Pause CPUs
{

	emit requestPause();
	// 		for (AlphaCPU* cpuPtr : m_cpus) {          // QVector<QScopedPointer<AlphaCPU>>
	// 			if (cpuPtr) {     // not null
	// 				QMetaObject::invokeMethod(cpuPtr , "receiveInterrupt",
	// 					Qt::QueuedConnection, Q_ARG(int, vector));
	// // 				QMetaObject::invokeMethod(cpuPtr,
	// // 					[cpuPtr] { cpuPtr->pauseExecution(); },
	// // 					Qt::QueuedConnection);
	// 			}
	// 		}
}

void AlphaSMPManager::receiveInterrupt(int cpuId, int vector) //Slot: Deliver IRQ to CPU
{
	// 		AlphaCPU* alphaCPU = (AlphaCPU*) m_cpus[cpuId];
	// 		QMetaObject::invokeMethod(static_cast<QObject*>(alphaCPU), "receiveInterrupt",
	// 				Qt::QueuedConnection, Q_ARG(vector));
	emit requestInterrupt(cpuId, vector);
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
void AlphaSMPManager::handleTrapRaised(helpers_JIT::Fault_TrapType trap)
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

// Add interprocessor interrupt mechanism
/* 
	IPIs are often delivered through specific hardware mechanisms to interrupt request registers
*/
void AlphaSMPManager::sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector)
{
	// Validate CPU indices
	if (targetCPU < 0 || targetCPU >= m_cpus.size()) {
		TraceManager::instance().warn(QString("Invalid target CPU index: %1").arg(targetCPU));
		return;
	}

	// Get target CPU
	AlphaCPU* targetCpuObj = m_cpus[targetCPU];
	if (!targetCpuObj) {
		TraceManager::instance().warn(QString("Target CPU%1 is null").arg(targetCPU));
		return;
	}

	// Update the CPU's internal state
	if (targetCpuObj->supportsIPRAccess()) {
		// Hardware-accurate approach: Set the bit in the IPIR
		quint64 ipirValue = (1ULL << interruptVector);
		targetCpuObj->writeIPR(AlphaCPU::IPR_IPIR, ipirValue);
	}

	// Trigger the handler for immediate response
	targetCpuObj->onHandleInterrupt(interruptVector);

	// Emit signal for monitoring/debugging
	emit interprocessorInterruptSent(sourceCPU, targetCPU, interruptVector);

	// Log the event
	TraceManager::instance().debug(QString("CPU%1 sent interrupt vector %2 to CPU%3")
		.arg(sourceCPU)
		.arg(interruptVector)
		.arg(targetCPU));
}
void AlphaSMPManager::sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector, int priority = -1)
{
	// Validate CPU indices
	if (targetCPU < 0 || targetCPU >= m_cpus.size()) {
		TraceManager::instance().warn(QString("Invalid target CPU index: %1").arg(targetCPU));
		return;
	}

	// Get target CPU
	AlphaCPU* targetCpuObj = m_cpus[targetCPU];
	if (!targetCpuObj) {
		TraceManager::instance().warn(QString("Target CPU%1 is null").arg(targetCPU));
		return;
	}

	// Update the CPU's internal state
	if (targetCpuObj->supportsIPRAccess()) {
		// Hardware-accurate approach: Set the bit in the IPIR
		quint64 ipirValue = (1ULL << interruptVector);
		targetCpuObj->writeIPR(AlphaCPU::IPR_IPIR, ipirValue);

		// If priority is specified, also update priority register if applicable
		if (priority >= 0) {
			targetCpuObj->writeIPR(AlphaCPU::IPR_IPIR_PRIORITY,
				(targetCpuObj->readIPR(AlphaCPU::IPR_IPIR_PRIORITY) &
					~(0xFULL << (interruptVector * 4))) |
				((quint64)priority << (interruptVector * 4)));
		}
	}

	// Trigger the appropriate handler
	if (priority >= 0) {
		targetCpuObj->onHandleInterruptWithPriority(interruptVector, priority);
		emit interprocessorInterruptSent(sourceCPU, targetCPU, interruptVector, priority);
		TraceManager::instance().debug(QString("CPU%1 sent interrupt vector %2 (priority %3) to CPU%4")
			.arg(sourceCPU)
			.arg(interruptVector)
			.arg(priority)
			.arg(targetCPU));
	}
	else {
		targetCpuObj->onHandleInterrupt(interruptVector);
		emit interprocessorInterruptSent(sourceCPU, targetCPU, interruptVector);
		TraceManager::instance().debug(QString("CPU%1 sent interrupt vector %2 to CPU%3")
			.arg(sourceCPU)
			.arg(interruptVector)
			.arg(targetCPU));
	}
}
void AlphaSMPManager::broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector, bool includeSource = false)
{
	TraceManager::instance().debug(QString("CPU%1 broadcasting interrupt vector %2")
		.arg(sourceCPU)
		.arg(interruptVector));

	for (int i = 0; i < m_cpus.size(); i++) {
		// Skip source CPU if not includeSource
		if (!includeSource && i == sourceCPU) {
			continue;
		}

		// Skip invalid CPUs
		if (!m_cpus[i]) {
			continue;
		}

		// Deliver interrupt
		m_cpus[i]->onHandleInterrupt(interruptVector);

		// Emit signal for each target (or could emit a single broadcast signal)
		emit interprocessorInterruptSent(sourceCPU, i, interruptVector);
	}

	TraceManager::instance().debug(QString("Broadcast of interrupt vector %1 complete")
		.arg(interruptVector));
}
// Add barrier synchronization
void AlphaSMPManager::synchronizeCPUs(int barrierID) {
	// Implement hardware barrier for synchronization
	// ...
}

void AlphaSMPManager::sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector, int priority = -1)
{
	// Validate CPU indices
	if (targetCPU < 0 || targetCPU >= m_cpus.size()) {
		TraceManager::instance().warn(QString("Invalid target CPU index: %1").arg(targetCPU));
		return;
	}

	// Get target CPU
	AlphaCPU* targetCpuObj = m_cpus[targetCPU];
	if (!targetCpuObj) {
		TraceManager::instance().warn(QString("Target CPU%1 is null").arg(targetCPU));
		return;
	}

	// Send the appropriate interrupt
	if (priority >= 0) {
		// Priority-based interrupt
		targetCpuObj->onHandleInterruptWithPriority(interruptVector, priority);
		emit interprocessorInterruptSent(sourceCPU, targetCPU, interruptVector, priority);
		TraceManager::instance().debug(QString("CPU%1 sent interrupt vector %2 (priority %3) to CPU%4")
			.arg(sourceCPU)
			.arg(interruptVector)
			.arg(priority)
			.arg(targetCPU));
	}
	else {
		// Standard interrupt
		targetCpuObj->onHandleInterrupt(interruptVector);
		emit interprocessorInterruptSent(sourceCPU, targetCPU, interruptVector);
		TraceManager::instance().debug(QString("CPU%1 sent interrupt vector %2 to CPU%3")
			.arg(sourceCPU)
			.arg(interruptVector)
			.arg(targetCPU));
	}
}

