#pragma once
// AlphaSMPManager.h - SMP management header
#ifndef ALPHASMPMANAGER_H
#define ALPHASMPMANAGER_H
#include "AlphaCPU.h"

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QAtomicInt>
#include <QTimer>
#include <QSharedPointer>
#include <QMap>
#include <QSet>
#include <QHash>
#include <QWaitCondition>
#include <QMetaObject>
#include "alphamemorysystem.h"
#include "../AESH/QSettingsConfigLoader.h"
#include "../AEB/devicemanager.h"
#include "alphapalinterpreter.h"
#include "../AEJ/UnifiedDataCache.h"
#include "../AEJ/IprBank.h"
#include "../AEJ/GlobalLockTracker.h"
#include "../AEB/systembus.h"
#include "../AESH/SafeMemory.h"
#include "../AEB/IRQController.h"
#include "enumerations/enumCpuState.h"
#include "traps/trapFaultTraps.h"



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
    explicit AlphaSMPManager(QObject* parent = nullptr);
    ~AlphaSMPManager();


	void addCPU(int cpuId) {

		m_cpus[cpuId] = new AlphaCPU(i, this);
		// set properties
		//m_cpus[i]->setJitEnabled(this->jitEnabled);
		//m_cpus[i]->setJitThreshold(this->jitThreshold);
		m_cpus[cpuId]->attachMemorySystem(m_memorySystem.data());
		m_cpus[cpuId]->attachMMIOManager(m_mmioManager.data());
		m_cpus[cpuId]->attachIRQController(m_irqControllerPtr.data());
		m_cpus[cpuId]->attachUnifiedCache(m_unifiedCache.data());
		m_cpus[cpuId]->attachSMPManager(this);
		m_cpus[cpuId]->setCpuId(i);
		m_cpus[cpuId]->setMMUEnabled(true);  // Supports / Virtual and Flat / let's default this for now.

	}
	void addCPUConnections(int cpuID) {

		// Add this in the initialize() method when connecting signals:
		connect(m_cpus[cpuID], &AlphaCPU::sigMemoryAccessed,
			this, [this, cpuID](quint64 address, bool isWrite, int size) {
				if (isWrite) {
					this->handleMemoryWrite(cpuID, address, size);
				}
			});

		connect(this, &AlphaSMPManager::requestPause, m_cpus[cpuID], &AlphaCPU::onPauseExecution);
		connect(this, &AlphaSMPManager::requestInterrupt, m_cpus[cpuID], [=](int cpuId, int vector) {
			if (cpuId == cpuID) {
				m_cpus[cpuID]->onReceiveInterrupt(vector);
			}
			});
		// Add these new connections:

		// Connect critical CPU signals to AlphaSMPManager slots
		connect(m_cpus[cpuID], &AlphaCPU::halted, this, &AlphaSMPManager::handleCpuHalted);
		connect(m_cpus[cpuID], &AlphaCPU::sigTrapRaised, this, &AlphaSMPManager::handleTrapRaised);
		connect(m_cpus[cpuID], &AlphaCPU::sigStateChanged, this, &AlphaSMPManager::handleCpuStateChanged);
		connect(m_cpus[cpuID], &AlphaCPU::sigMemoryAccessed, this, &AlphaSMPManager::handleMemoryAccessed);
		connect(m_cpus[cpuID], &AlphaCPU::sigProcessingProgress, this, [this, cpuID](int percentComplete) { emit cpuProgress(cpuID, percentComplete); });
		connect(m_cpus[cpuID], &AlphaCPU::sigOperationStatus, this, [this, cpuID](const QString& message) { emit cpuStatusUpdate(cpuID, message);   });
		connect(m_cpus[cpuID], &AlphaCPU::sigCycleExecuted, this, [this, cpuID](quint64 cycle) {}); // Optionally handle cycle completion
		connect(m_cpus[cpuID], &AlphaCPU::sigStateChanged, this, [this, cpuID](CPUState newState) {	emit cpuStateChanged(cpuID, newState); });
		connect(m_cpus[cpuID], &AlphaCPU::halted, this, &AlphaSMPManager::handleCpuHalted);
		connect(this, &AlphaSMPManager::signalSendInterrupt, m_cpus[cpuID], [=](int cpuId, int vector) { if (cpuId == cpuID) m_cpus[cpuID]->onReceiveInterrupt(vector); });
		connect(m_cpus[cpuID], &AlphaCPU::finished, m_moved_cpus[cpuID], &QThread::quit);
		// Connect global control signals
		connect(this, &AlphaSMPManager::signalStartAll, m_cpus[cpuID], &AlphaCPU::onStartExecution);
		connect(this, &AlphaSMPManager::signalStopAll, m_cpus[cpuID], &AlphaCPU::onStopExecution);
		connect(this, &AlphaSMPManager::signalResetAll, m_cpus[cpuID], &AlphaCPU::onResetCPU);
		connect(this, &AlphaSMPManager::signalPauseAll, m_cpus[cpuID], &AlphaCPU::onPauseExecution);
		connect(this, &AlphaSMPManager::signalResumeAll, m_cpus[cpuID], &AlphaCPU::onResumeExecution);

		// cpu header is included here, so the symbol is known
		connect(m_cpus[cpuID], &AlphaCPU::sigExecutionStarted, this, &AlphaSMPManager::handleCoreStarted);
		connect(m_cpus[cpuID], &AlphaCPU::sigExecutionStopped, this, &AlphaSMPManager::handleCoreStopped);
		connect(m_cpus[cpuID], &AlphaCPU::sigExecutionPaused, this, &AlphaSMPManager::handleCorePaused);

		// Connect per-CPU interrupt
		connect(m_moved_cpus[cpuID], &QThread::finished, m_moved_cpus[i], &QObject::deleteLater);

		// Internal Register Signals
		IprBank* pIprBank = m_cpus[cpuID]->iprBank();
		connect(pIprBank, &IprBank::sigRegisterChanged, this, &AlphaSMPManager::onIprWriteTrace);

		QMetaObject::invokeMethod(m_cpus[i], "resumeExecution", Qt::QueuedConnection);


		// Connect critical CPU signals to AlphaSMPManager slots
		connect(m_cpus[cpuID], &AlphaCPU::sigCpuHalted, this, &AlphaSMPManager::handleCpuHalted);
		connect(m_cpus[cpuID], &AlphaCPU::sigTrapRaised, this, &AlphaSMPManager::handleTrapRaised);
		connect(m_cpus[cpuID], &AlphaCPU::sigStateChanged, this, &AlphaSMPManager::handleCpuStateChanged);
		connect(m_cpus[cpuID], &AlphaCPU::sigMemoryAccessed, this, &AlphaSMPManager::handleMemoryAccessed);
		// Set the program counter
		//QMetaObject::invokeMethod(m_cpus[i], "setPC", Qt::QueuedConnection, Q_ARG(quint64, pc));
		// Start CPU execution
	}

	// Configuration Loader
	bool applyConfiguration(const QJsonObject& config);  // or a custom ConfigObject
	void applyConfiguration(QString lastLoadedConfig);
	void attachAlphaMemorySystem(AlphaMemorySystem* memSys) { m_alphaMemorySystem = memSys;  }
	void attachDeviceManager(DeviceManager* devMgr) { m_deviceManager = devMgr; }
	void attachIrqController(IRQController* irqController) { m_irqController = irqController; }
	void attachMmioManager(MMIOManager* mmioManager) { m_mmioManager = mmioManager; }
	void attachSystemBus(SystemBus* sysBus) { m_systemBus = sysBus;  }
	void attachSafeMemory(SafeMemory* safeMem) { m_safeMemory = safeMem;  }

	void notifyCpuHalted(quint32 cpuId) {

		auto cpu_ = getCPU(cpuId);
		if (cpu_) {
			// If you want to actually halt the CPU:
			cpu_->halt();

			// Or if you just want to mark it as halted without calling halt():
			// You might need a setState method instead

			// Log the halt
			DEBUG_LOG(QString("CPU %1 has been halted").arg(cpuId));

			// Notify other components
			emit sigCpuHalted(cpuId);  // If using Qt signals
		}
		else {
			ERROR_LOG(QString("Failed to find CPU %1 for halt notification").arg(cpuId));
		}

	}

	////////////////////////////////////////
	// Reset and reload configuration (JSON or programmatic)
	void reset();



	

	void setIoThreadCount(int count);

	// Set Memory
	void setMemoryAlloc(qint64 _memory) {
		this->m_safeMemory->resize(_memory, /* initialize Only*/ true);
	}
	// Create a placeholder for Alpha CPUs up to the count indicated.  we don't care how many CPUs at this stage.
	void setCPUVectorPlaceHolder(quint8 cpuCnt_) {
		if (cpuCnt_ > 4)  return ;
		// Create CPUs
		for (int i = 0; i < cpuCnt_; i++) {
			m_cpus.append(nullptr); // Will be initialized later
		}
		return ;  // Less than 4 
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

	QVector<AlphaCPU*> 	getAllCpus() { return m_cpus; }		// returns all CPUs in the QVector 

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
	/**
	 * @brief Notifies the manager that a CPU successfully executed a Store-Conditional
	 *
	 * Causes other CPUs' reservations on the same cache line to be invalidated.
	 *
	 * @param cpuId The ID of the CPU executing the instruction
	 * @param cacheLine The cache line address (aligned to 64-byte boundary)
	 */
	void notifyStoreConditionalSuccess(int cpuId, quint64 cacheLine);						// Called when a CPU executes a successful SC instruction
	void     notifyTrapRaised(bool isFp, unsigned idx, uint64_t raw)  {
		//emit registerUpdated(idx, raw);  // or however you forward it
	}
	

public slots:
    // CPU control

	void cpusAllStarted();
	void onClearReservations()
	{

	}
	void onCpuStatusUpdate(int cpuId, const QString& status)
	{
		qInfo().nospace() << "[AlphaSMPManager] CPU[" << cpuId << "] status: " << status;

		// Optionally, update internal state tracking for the CPUs
		if (this->cpuStatusMap.contains(cpuId)) {
			cpuStatusMap[cpuId] = status;
		}
		else {
			cpuStatusMap.insert(cpuId, status);
		}

		// Forward this info if needed
		emit cpuStatusUpdate(cpuId, status);
	}

	void onIprWriteTrace(Ipr id, quint64 newValue)
	{

	}
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
	void handleCoreStarted(quint16 cpu_id) {
		//TODO  - handleCoreStarted(quint8 cpu_id)
	}
	void handleCoreStopped(quint16 cpu_id) {
		//TODO  - handleCoreStopped(quint8 cpu_id)
	}
	void handleCorePaused(quint16 cpu_id) {
		//TODO  - handleCorePaused(quint8 cpu_id)
	}
    void handleCpuHalted();
	void handleCpuStateChanged(int newState);
	void handleTrapRaised(Fault_TrapType trap);

    // Inter-processor communication
    void sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector);
	void sendInterprocessorInterrupt(int sourceCPU, int targetCPU, int interruptVector, int priority = -1);
	void synchronizeCPUs(int barrierID);
	void broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector);

	void broadcastInterprocessorInterrupt(int sourceCPU, int interruptVector, bool includeSource = false);
	/**
	 * @brief Notifies the manager that a CPU has executed a Load-Locked instruction
	 * 
	 * @param cpuId The ID of the CPU executing the instruction
	 * @param cacheLine The cache line address (aligned to 64-byte boundary)
	 */
	void notifyLockReservation(int cpuId, quint64 cacheLine);							// Called when a CPU executes an LL instruction

	void handleExternalMemoryWrite(quint64 physicalAddress);
    // Memory coherency
	void handleMemoryAccessed(quint64 address, quint64 value, int size, bool isWrite);	// handle all virtual address mappings for a physical address
	/**
	 * @brief Handles memory writes from any CPU
	 *
	 * Invalidates lock reservations and updates cache coherency.
	 *
	 * @param cpuId The ID of the CPU performing the write
	 * @param address The memory address being written to
	 * @param size The size of the write in bytes
	 */
	void handleMemoryWrite(int cpuId, quint64 address, int size) // Add to memory coherency handling - when any CPU writes to memory
	{
		// every store invalidates any LDx_L reservations on that 16-byte block:
		quint64 base = address & ~0xFULL;
		GlobalLockTracker::invalidate(base);
	}
    void invalidateCacheLine(int cpuId, quint64 address);


    // Synchronization
	void cycleExecuted();
	// Interface for DeviceInterface to 
	bool registerDevice(DeviceInterface* device, quint64 mmioBase, quint64 mmioSize, int irq = -1);
    void releaseAllCPUs();
  


	void stopExecution();		// 	Slot: Stop all CPUs
	

	void waitForAllCPUs();

#pragma warning(pushFrame)
#pragma warning(disable:4181)   // or disable VCR001 in your settings

	void operationStatus(int cpuId, const QString& message);
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
    void cacheCoherencyEvent(int cpuId, quint64 address);
	void InitializeMemory(int cpuCount, quint64 ramSizeMB, quint64 startPC);

	/**
	 * @brief Checks if a CPU's lock reservation is still valid
	 *
	 * Called during Store-Conditional execution to verify no other
	 * CPU has written to the cache line since the reservation was made.
	 *
	 * @param cpuId The ID of the CPU to check
	 * @param cacheLine The cache line address to check
	 * @return bool True if the reservation is still valid
	 */
	bool checkLockReservationValid(int cpuId, quint64 cacheLine);					// Check if a CPU's lock reservation is still valid
	void cpuProgress(int cpuId, int percentComplete);								//  CPU progress and status
	void cpuStatusUpdate(int cpuId, const QString& message);						//  CPU progress and status
	void cpuStateChanged(int cpuId, AsaTypesAndStates::CPUState newState);				//  CPU progress and status

	// Control State
	void signalStartAll();
	void signalStopAll();
	void signalResetAll();
	void pauseAllCPUs();
	void signalResumeAll();
	void signalSendInterrupt(int cpuId, int irqVector);
#pragma warning(popFrame)

signals:


	void HandleMemory(quint64 address, bool bIsWrite, int size) {

	}
	void cpuHalted(quint16 cpuid);
	// Signal for standard IPI
	void interprocessorInterruptSent(int sourceCPU, int targetCPU, int vector);

	// Signal for priority-based IPI
	void interprocessorInterruptSent(int sourceCPU, int targetCPU, int vector, int priority);
	void pausedAllCPUs();
	void requestPause();
	void requestInterrupt(int cpuId, int vector);
	void initializedSystem();
	void signalPauseAll();
private:
    // CPUs and Threads - they are both synchronized in SMP configuration. 
    QVector<AlphaCPU*> m_cpus;
	QVector<QThread*> m_moved_cpus;

	AlphaMemorySystem* m_alphaMemorySystem;
	IRQController* m_irqController;
	SafeMemory* m_safeMemory;
	MMIOManager* m_mmioManager;
	UnifiedDataCache* m_unifiedCache;
	SystemBus* m_systemBus;
	DeviceManager* m_deviceManager;			// Owned by EmulatorManager (configLoader will populate)
	
	
	//SystemLoader*		m_systemLoader;		// Owned by EmulatorManager
	//MMIOManager* m_mmioManager;			// Owned by EmulatorManager


	//SafeMemory* m_safeMemory;			// Owned by EmulatorManager

	QMutex m_smpLock;									// Maps CPU IDs to their locked cache lines
	QHash<int, quint64> m_cpuLockReservations;			// cpuId -> cache line with lock
	QHash<quint64, QSet<int>> m_cacheLinesWithLocks;	// Maps cache lines to the set of CPUs that have locked them
	QSet<quint64> m_invalidatedCacheLines;				// Cache lines written to since locks were set

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

	
	//AlphaPALInterpreter* m_palInterpreter;  // ASM must own this otherwise a circular reference on AlphaCPU
   
    // Synchronization
    QAtomicInt m_activeCPUCount;
    QAtomicInt m_waitingCPUCount;
	QAtomicInt stopRequested;	
    QMutex m_barrierLock;
    QWaitCondition m_barrierCondition;
	
    // Cache coherency tracking (simplified)
    QMap<quint64, QSet<int>> m_sharedCacheLines; // Maps address to set of CPUs sharing it

	/** keeps the last status string reported by each CPU */
	QHash<int, QString> cpuStatusMap;

    // Helper methods
	/**
	 * @brief Get CPU by ID
	 * @param cpuId CPU identifier
	 * @return Pointer to CPU or nullptr if not found
	 */
	AlphaCPU* getCPU(quint16 cpuId) {
		// Implementation depends on how you store CPUs
		// Could be a vector, map, etc.
		if (cpuId < m_cpus.size()) {
			return m_cpus[cpuId];
		}
		return nullptr;
	}
    void synchronizeBarrier();
    void resetBarrier();
    void handleMemoryCoherency(quint64 address, int sourceCPU);
    void updateSharedCacheStatus(quint64 address, int cpuId, bool isSharing);
	
};

#endif // ALPHASMPMANAGER_H