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

	
	void pausedAllCPUs();

	////////////////////////////////////////
	// Reset and reload configuration (JSON or programmatic)
	void reset();

	void startCpu(int cpuId, quint64 pc);


	void startPC(quint64 startPC_)
	{
		for (int i = 0; i < m_cpus.count(); ++i) {
			AlphaCPU* cpu = new AlphaCPU(i, memorySystem, this);
			cpu->setPC(startPC_);
		}
	}

	void setIoThreadCount(int count);// Set Memory
	void setMemoryAlloc(qint64 _memory) {
		this->memorySystem->setMemoryAlloc(_memory);
	}
	// Create a placeholder for Alpha CPUs up to the count indicated. 
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
		// TODO Future implementation: Store or connect serial device
	}

	void addNetworkInterface(const QString& name, const QString& iface)
	{
		qInfo() << "[AlphaSMPManager] Network Interface added:" << name << iface;
		// TODO Future implementation: Store or connect network device
	}

	void addScsiController(const QString& controllerName, int scsiId, const QList<QPair<int, QString>>& devices)
	{
		qInfo() << "[AlphaSMPManager] SCSI Controller added:" << controllerName << "SCSI-ID:" << scsiId;
		for (const auto& unit : devices) {
			qInfo() << "   Unit" << unit.first << ":" << unit.second;
		}
		// TODO Future implementation: Create and connect SCSI controller and devices
	}

    void initialize();
 
    void shutdown();

    // CPU access
    AlphaCPU* getCPU(int index);
    int getCPUCount() const { return m_cpus.size(); }
	int getjitOptimizationLevel() { return jitOptimizationLevel; }

    // Execution control

    void pauseSystem();
    void resumeSystem();
	void startSystem(quint64 entryPoint);
    void stopSystem();

	// TODO // Move to emulator manager
	void setTraceLevel(int traceLevel); // we should implement this in emulator manager
    void startFromPALBase();
	  


public slots:
    // CPU control

	void cpusAllStarted();
    void startAllCPUs();
	void startAllCPUs_MoveToThread();
	void startSystem();
	void pauseSystem();



	void stopAllCPUs();

		void requestStop() {
			stopRequested.storeRelaxed(true);
			qDebug() << "[AlphaCPU] Stop requested";
		}


	void stoppedSystem();

    void handleCpuHalted();
	void handleCpuStateChanged(int newState);
	void handleTrapRaised(helpers_JIT::TrapType trap);


    // Inter-processor communication
    void sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector);
    void broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector);

    // Memory coherency
	void handleMemoryAccessed(quint64 address, quint64 value, int size, bool isWrite);
    void handleMemoryWrite(int cpuId, quint64 address, int size);
    void invalidateCacheLine(int cpuId, quint64 address);


    // Synchronization
	void cycleExecuted();
    void releaseAllCPUs();
  


	void pauseExecution()	// Slot: Pause CPUs
	{		
		for (int i = 0; i < m_cpus.size(); ++i) {
			QMetaObject::invokeMethod(m_cpus[i], "pauseExecution", Qt::QueuedConnection);
		}
	}
	void receiveInterrupt(int cpuId, int vector) //Slot: Deliver IRQ to CPU
	{
		if (cpuId >= 0 && cpuId < m_cpus.size()) {
			QMetaObject::invokeMethod(m_cpus[cpuId], "receiveInterrupt",
				Qt::QueuedConnection, Q_ARG(int, vector));
		}
	}
	void resetCPUs();		//Slot: Reset internal CPU state


	void resumeExecution();		// Slot: Resume CPUs
	//Control State
	
	void startExecution();		// Slot: Start all CPUs
	void stopExecution();		// 	Slot: Stop all CPUs
	

	void waitForAllCPUs();

signals:
    // System state
    void systemInitialized();
    void systemStarted(); // connect to startSystem();
    void systemPaused(); // connect to pauseSystem()
    void systemResumed(); // connect to resumeSystem()
    void systemStopped(); // connect to stoppedSystem()

    // CPU state aggregation
    void allCPUsStarted(); //	void cpusAllStarted();

	void allCPUsPaused(); // pausedAllCPUs

    void allCPUsStopped();

    // SMP events
    void interprocessorInterruptSent(int sourceCPU, int targetCPU, int interruptVector);
    void cacheCoherencyEvent(int cpuId, quint64 address);
	void configureSystem(int cpuCount, quint64 ramSizeMB, quint64 startPC);

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