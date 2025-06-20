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
#include "../SystemLoader.h"
#include "../AESH/helpers.h"
#include "../AEB/devicemanager.h"
#include "alphapalinterpreter.h"

class AlphaCPU;
class AlphaMemorySystem;

/**
 * @brief AlphaSMPManager - Manages multiple Alpha CPUs for SMP processing
 *
 * This class coordinates multiple CPU instances, handles inter-processor
 * communication, and manages shared resources.
 * 
 *  -- Run Processing Setup
 *  --- InitializeAll()
 *  ----  ApplyConfiguration
 *  ----  InitializeMemory()
 *  ----  InitializeCPUs
 *  ---
 *  ----  RegisterDevices()
 *  -----   RegisterDevice
 *  -- Start Processing
 *  --- startAllCPUs_MoveToThread (0x20000000)		


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


	

	void setIoThreadCount(int count);

	// Set Memory
	void setMemoryAlloc(qint64 _memory) {
		this->m_safeMemory->resize(_memory, /* initialize Only*/ true);
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

    void initializeCPUs(quint8	cpuCount_);
 
    void shutdown();

    // CPU access
    AlphaCPU* getCPU(int index);
    int getCPUCount() const { return m_cpus.size(); }
	int getjitOptimizationLevel() { return jitOptimizationLevel; }

    // Execution control


    void resumeSystem();
	void startSystem(quint64 entryPoint);
    void stopSystem();

	// TODO // Move to emulator manager
	void setTraceLevel(int traceLevel); // we should implement this in emulator manager
    void startFromPALBase();
	
	// IExecutionContext Overrides
	uint64_t readIntReg(unsigned idx)  { /* … */ }
	void     writeIntReg(unsigned idx, uint64_t v)  { /* … */ }

	double   readFpReg(unsigned idx)  { /* … */ }
	void     writeFpReg(unsigned idx, double f)  { /* … */ }

	// Memory
	bool     readMemory(uint64_t addr, void* buf, size_t sz)  { /* … */ }
	bool     writeMemory(uint64_t addr, const void* buf, size_t sz)  { /* … */ }

	// Control/status
	void     raiseTrap(int trapCode)  { /* … */ }

	// Events
	void     notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t raw)  {
		//emit registerUpdated(idx, raw);  // or however you forward it
	}
	void     notifyTrapRaised(bool isFp, unsigned idx, uint64_t raw)  {
		//emit registerUpdated(idx, raw);  // or however you forward it
	}
	

public slots:
    // CPU control

	void cpusAllStarted();
    void startAllCPUs();
	void startAllCPUs_MoveToThread(quint64 pc_init);
	void startSystem();
	void pauseSystem();

	

	void stopAllCPUs();

		void requestStop() {
			stopRequested.storeRelaxed(true);
			qDebug() << "[AlphaCPU] Stop requested";
		}


	void stoppedSystem();
	void handleCoreStarted(quint8 cpu_id) {
		//TODO  - handleCoreStarted(quint8 cpu_id)
	}
	void handleCoreStopped(quint8 cpu_id) {
		//TODO  - handleCoreStopped(quint8 cpu_id)
	}
	void handleCorePaused(quint8 cpu_id) {
		//TODO  - handleCorePaused(quint8 cpu_id)
	}
    void handleCpuHalted();
	void handleCpuStateChanged(int newState);
	void handleTrapRaised(helpers_JIT::TrapType trap);

	// These scoped Pointers are created by initialize() 
	IRQController* getIRQController() { return m_irqController.data(); }
	SystemLoader* getSystemLoader() { return m_systemLoader.data();  }

    // Inter-processor communication
    void sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector);
    void broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector);

    // Memory coherency
	void handleMemoryAccessed(quint64 address, quint64 value, int size, bool isWrite);
    void handleMemoryWrite(int cpuId, quint64 address, int size);
    void invalidateCacheLine(int cpuId, quint64 address);


    // Synchronization
	void cycleExecuted();
	// Interface for DeviceInterface to 
	bool registerDevice(DeviceInterface* device, quint64 mmioBase, quint64 mmioSize, int irq = -1);
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

#pragma warning(push)
#pragma warning(disable:4181)   // or disable VCR001 in your settings
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
	void InitializeMemory(int cpuCount, quint64 ramSizeMB, quint64 startPC);

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
#pragma warning(pop)
private:
    // CPUs and Threads - they are both synchronized in SMP configuration. 
    QVector<AlphaCPU*> m_cpus;
	QVector<QThread*> m_moved_cpus;

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

	AlphaMemorySystem*	m_memorySystem;	// Shared virtual-to-physical address space map
	IRQController*		m_irqController;// Owned by Emulator System
	SystemLoader*		m_systemLoader;		// Owned by EmulatorManager
	SafeMemory* m_safeMemory;			// Owned by EmulatorManager
	AlphaPALInterpreter* m_palInterpreter;  // ASM must own this otherwise a circular reference on AlphaCPU
   
    // Synchronization
    QAtomicInt m_activeCPUCount;
    QAtomicInt m_waitingCPUCount;
	QAtomicInt stopRequested;
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