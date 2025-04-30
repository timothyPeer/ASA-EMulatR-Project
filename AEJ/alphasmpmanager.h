#pragma once
// AlphaSMPManager.h - SMP management header
#ifndef ALPHASMPMANAGER_H
#define ALPHASMPMANAGER_H

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QAtomicInt>
#include <QMap>
#include <QSet>
#include <QWaitCondition>
#include "helpers.h"
#include "alphamemorysystem.h"
#include "AlphaCPU.h"

class AlphaCPU;
class AlphaMemorySystem;

/**
 * @brief AlphaSMPManager - Manages multiple Alpha CPUs for SMP processing
 *
 * This class coordinates multiple CPU instances, handles inter-processor
 * communication, and manages shared resources.
 */
class AlphaSMPManager : public QObject
{
    Q_OBJECT

public:
    explicit AlphaSMPManager(int cpuCount, QObject* parent = nullptr);
    ~AlphaSMPManager();

	// Configuration Loader
	bool applyConfiguration(const QJsonObject& config);  // or a custom ConfigObject
	void applyConfiguration(QString lastLoadedConfig);

	////////////////////////////////////////
	// Reset the SMP Manager
	void reset()
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

	void startCpu(int cpuId, quint64 pc)
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


	void startPC(quint64 startPC_)
	{
		for (int i = 0; i < m_cpus.count(); ++i) {
			AlphaCPU* cpu = new AlphaCPU(i, memorySystem, this);
			cpu->setPC(startPC_);
		}
	}
// 	void configureSystem(int cpuCount, quint64 ramSizeMB, quint64 startPC)
// 	{
// 		if (!memorySystem) {
// 			memorySystem = new AlphaMemorySystem(this);
// 		}
// 
// 		//memorySystem->initialize(ramSizeMB);
// 
// 		for (int i = 0; i < cpuCount; ++i) {
// 			AlphaCPU* cpu = new AlphaCPU(i, memorySystem, this);
// 			cpu->setPC(startPC);
// 
// // 			// Connect critical CPU signals to AlphaSMPManager slots
// // 			connect(cpu, &AlphaCPU::halted, this, &AlphaSMPManager::handleCpuHalted);
// // 			connect(cpu, &AlphaCPU::trapRaised, this, &AlphaSMPManager::handleTrapRaised);
// // 			connect(cpu, &AlphaCPU::stateChanged, this, &AlphaSMPManager::handleCpuStateChanged);
// // 			connect(cpu, &AlphaCPU::memoryAccessed, this, &AlphaSMPManager::handleMemoryAccessed);
// 
// 			m_cpus.append(cpu);
// 		}
// 	}

	void setIoThreadCount(int count)
	{
		ioThreadCount = count;
	}

	// Set Memory
	void setMemoryAlloc(qint64 _memory) {
		this->memorySystem->setMemoryAlloc(_memory);
	}
	// Create a placeholder for Alpha CPUs up to the count indidicated. 
	bool setCPUVectorPlaceHolder(quint8 cpuCnt_) {
		if (cpuCnt_ > 4)  return false;
		// Create CPUs
		for (int i = 0; i < cpuCnt_; i++) {
			m_cpus.append(nullptr); // Will be initialized later
		}
		return true;  // Less than 4 
	}


	void setSessionLog(const QString& fileName, const QString& method)
	{
		sessionLogFileName = fileName;
		sessionLogMethod = method;
	}

	void setHardwareInfo(const QString& model, const QString& serial)
	{
		hardwareModel = model;
		hardwareSerial = serial;
	}

	void setRomFile(const QString& romPath)
	{
		romFilePath = romPath;
	}

	void setSrmFile(const QString& srmPath)
	{
		srmRomFilePath = srmPath;
	}

	void setNvramFile(const QString& nvramPath)
	{
		nvramFilePath = nvramPath;
	}

	void addSerialInterface(const QString& name, const QString& iface, const QString& port = "", const QString& app = "")
	{
		qInfo() << "[AlphaSMPManager] Serial Interface added:" << name << iface << port << app;
		// Future implementation: Store or connect serial device
	}

	void addNetworkInterface(const QString& name, const QString& iface)
	{
		qInfo() << "[AlphaSMPManager] Network Interface added:" << name << iface;
		// Future implementation: Store or connect network device
	}

	void addScsiController(const QString& controllerName, int scsiId, const QList<QPair<int, QString>>& devices)
	{
		qInfo() << "[AlphaSMPManager] SCSI Controller added:" << controllerName << "SCSI-ID:" << scsiId;
		for (const auto& unit : devices) {
			qInfo() << "   Unit" << unit.first << ":" << unit.second;
		}
		// Future implementation: Create and connect SCSI controller and devices
	}

    void initialize();
 
    void shutdown();

    // CPU access
    AlphaCPU* getCPU(int index);
    int getCPUCount() const { return m_cpus.size(); }
	int getjitOptimizationLevel() { return jitOptimizationLevel; }

    // Execution control
    void startSystem(quint64 entryPoint);
    void pauseSystem();
    void resumeSystem();
    void stopSystem();

    void startFromPALBase();
	  
	void setTraceLevel(int traceLevel);
public slots:
    // CPU control
    void startAllCPUs();
	void startAllCPUs_MoveToThread();
    void pauseAllCPUs();
    void stopAllCPUs();
    void handleCpuHalted();
	void handleTrapRaised(helpers_JIT::TrapType trap);
	void handleCpuStateChanged(int newState);

    // Inter-processor communication
    void sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector);
    void broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector);

    // Memory coherency
    void handleMemoryWrite(int cpuId, quint64 address, int size);
    void invalidateCacheLine(int cpuId, quint64 address);

	void handleMemoryAccessed(quint64 address, quint64 value, int size, bool isWrite);
    // Synchronization
    void waitForAllCPUs();
    void releaseAllCPUs();
    void cycleExecuted();

		//Control State
	void startExecution()	{
		for (int i = 0; i < m_cpus.size(); ++i) {
			if (m_moved_cpus[i] && m_moved_cpus[i]->isRunning()) {
				QMetaObject::invokeMethod(m_cpus[i], "startExecution", Qt::QueuedConnection);
			}
		}
	}
	void stopExecution() {
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
	void resetCPUs()	{
		for (int i = 0; i < m_cpus.size(); ++i) {
			QMetaObject::invokeMethod(m_cpus[i], "resetCPU", Qt::QueuedConnection);
		}
	}
	;		// reset state of each CPU only
	void pauseExecution() {
		for (int i = 0; i < m_cpus.size(); ++i) {
			QMetaObject::invokeMethod(m_cpus[i], "pauseExecution", Qt::QueuedConnection);
		}
	}
	void resumeExecution() {
		for (int i = 0; i < m_cpus.size(); ++i) {
			QMetaObject::invokeMethod(m_cpus[i], "resumeExecution", Qt::QueuedConnection);
		}
	}

	void receiveInterrupt(int cpuId, int vector) {
		if (cpuId >= 0 && cpuId < m_cpus.size()) {
			QMetaObject::invokeMethod(m_cpus[cpuId], "receiveInterrupt",
				Qt::QueuedConnection, Q_ARG(int, vector));
		}
	}

signals:
    // System state
    void systemInitialized();
    void systemStarted();
    void systemPaused();
    void systemResumed();
    void systemStopped();

    // CPU state aggregation
    void allCPUsStarted();
    void allCPUsPaused();
    void allCPUsStopped();

    // SMP events
    void interprocessorInterruptSent(int sourceCPU, int targetCPU, int interruptVector);
    void cacheCoherencyEvent(int cpuId, quint64 address);

	// New signals for CPU progress and status
	void cpuProgress(int cpuId, int percentComplete);
	void cpuStatusUpdate(int cpuId, const QString& message);
	void cpuStateChanged(int cpuId, helpers_JIT::CPUState newState);

	// Control State
	void signalStartAll();
	void signalStopAll();
	void signalResetAll();
	void signalPauseAll();
	void signalResumeAll();
	void signalSendInterrupt(int cpuId, int irqVector);
private:
    // CPUs and Threads - they are both synchronized in SMP configuration. 
    QVector<AlphaCPU*> m_cpus;
	QVector<QThread*> m_moved_cpus;

	AlphaMemorySystem* memorySystem; // Memory and MMIO configuration is managed here. 
    QMutex m_smpLock;

    // Configuration Private Properties
	int ioThreadCount = 1;
	QString sessionLogFileName;
	QString sessionLogMethod;
	QString hardwareModel;
	QString hardwareSerial;

	QString romFilePath;
	QString srmRomFilePath;
	QString nvramFilePath;
	QString lastLoadedConfig;			// The JSon Configuration 
	bool jitEnabled;					// enable the JIT engine
	int  jitThreshold = 50;				// set the JIT threshold 
	int  jitOptimizationLevel = 2;		//   set to disable (0), 
	                                    //   Basic Compilation (1), 
										//   Register Allocation (2), 
										//   Function Inline/Vectorization (3)

   
    // Synchronization
    QAtomicInt m_activeCPUCount;
    QAtomicInt m_waitingCPUCount;
    QMutex m_barrierLock;
    QWaitCondition m_barrierCondition;

    // Cache coherency tracking (simplified)
    QMap<quint64, QSet<int>> m_sharedCacheLines; // Maps address to set of CPUs sharing it

    // Helper methods
    void synchronizeBarrier();
    void resetBarrier();
    void handleMemoryCoherency(quint64 address, int sourceCPU);
    void updateSharedCacheStatus(quint64 address, int cpuId, bool isSharing);
	
};

#endif // ALPHASMPMANAGER_H