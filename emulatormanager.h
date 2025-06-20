// EmulatorManager.h - Enhanced version
#pragma once
#ifndef EmulatorManager_h__
#define EmulatorManager_h__

#include <QObject>
#include <QThread>
#include <QScopedPointer>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QWaitCondition>
#include "AlphaCoreContext.h"
#include "./AEJ/SafeMemory_refactored.h"
#include "MMIOManager.h"
#include "DeviceManager.h"
#include "IRQController.h"
#include "SystemBus.h"
#include "../AEJ/alphasmpmanager.h"
#include "./AEJ/AlphaMemorySystem_refactored.h"
#include "./AEJ/UnifiedDataCache.h"

/**
 * @brief Central manager for the Alpha emulator system
 *
 * EmulatorManager creates and coordinates all components of the
 * emulation system, including CPUs, memory, devices, and buses.
 * It provides a high-level interface for controlling the emulation.
 * 
 * EmulatorManager
 ├── IrqController
 ├── SystemLoader
 ├── DeviceManager             ◄── owns + wires devices (e.g., SCSI, NIC)
 ├── AlphaSMPManager
 │    ├── SafeMemory
 │    ├── MMIOManager
 │    ├── SystemBus           ◄── MMIO/Memory-aware bus for CPU/device interconnect
 │    ├── AlphaCPU Threads
 │         └── AlphaCPU
 │              ├── Pipeline
 │              └──  Fetch 
                |--  Decode
                |--  execute
                |--  writeback
                |--  Exception Processing




 // somewhere in EmulatorManager.cpp (after safeMemory and cpu are constructed)

constexpr quint64 PAL_BASE     = 0xC0000000ULL;  // SRM/PALcode window
constexpr quint64 RESET_VECTOR = 0xC0000080ULL;  // entry point offset

// map of descriptive names → hex filenames
const QMap<QString, QString> firmwareMap = {
	{ "HPM"     , ":/firmware/hpmrom.hex"   },
	{ "HPM-FS"  , ":/firmware/hpmfsrom.hex" },
	{ "PBM"     , ":/firmware/pbmrom.hex"   },
	{ "PBM-FS"  , ":/firmware/pbmfsrom.hex" },
	{ "PSM"     , ":/firmware/psmrom.hex"   },
	{ "PSM-FS"  , ":/firmware/psmfsrom.hex" },
	{ "SCM"     , ":/firmware/scmrom.hex"   },
	{ "SCM-FS"  , ":/firmware/scmfsrom.hex" },
	{ "WF_xs"   , ":/firmware/wf_xsrom.hex" }
};

for (auto it = firmwareMap.constBegin(); it != firmwareMap.constEnd(); ++it) {
	const QString &variant = it.key();
	const QString &hexFile  = it.value();

	qDebug() << "Loading" << variant << "firmware from" << hexFile;
	if (!IntelHexLoader::loadHexFile(hexFile, safeMemory, PAL_BASE)) {
		qFatal("Failed to load %s", qPrintable(variant));
	}
}

// after all are loaded (you’ll only actually call one of the above
// in practice, depending on your machine-type), point the CPU at SRM:
cpu->setPC(RESET_VECTOR);

 * 
 */
class EmulatorManager : public QObject {
    Q_OBJECT

public:
    enum class EmulationState {
        Uninitialized,
        Initialized,
        Running,
        Paused,
        Stopped
    };

    /**
     * @brief Construct a new EmulatorManager
     * @param parent Optional QObject parent
     */
    explicit EmulatorManager(QObject* parent = nullptr);

    /**
     * @brief Destroy the EmulatorManager
     */
    ~EmulatorManager();


    void buildAlphaSystem()
    {
        // configure defaults
        // 4GB memory, with 4 Alpha Processors
       initialize(1024 * 1024 * 2014 * 4, 4);
       setupSharedResources(); // configure CPU placehholder
    }
    /**
     * @brief Initialize the emulation system
     * @param memorySize Physical memory size in bytes
     * @param cpuCount Number of CPU cores to create
     * @return True if initialization was successful
     */
    bool initialize(quint64 memorySize = 64 * 1024 * 1024, int cpuCount = 1);

    void initialize_signalsAndSlots();

    /*
	 QScopedPointer<SystemBus> systemBus;
	QScopedPointer<IRQController> irqController;
	QScopedPointer<MMIOManager> mmioManager;
	QScopedPointer<DeviceManager> deviceManager;
	QScopedPointer<AlphaSMPManager> m_smpManager;
    */
    void setupSharedResources()
    {
        m_systemBus.reset(new SystemBus(nullptr));
        m_mmioManager.reset(new MMIOManager(nullptr));
        m_irqController.reset(new IRQController(nullptr));
        m_alphaMemorySystem.reset(new AlphaMemorySystem(nullptr));
        m_safeMemory.reset(new SafeMemory(nullptr));
        m_unifiedCache.reset(new UnifiedDataCache(nullptr));
        m_deviceManager.reset(new DeviceManager(nullptr));
        // CPU Placeholder
        m_smpManager->setCPUVectorPlaceHolder(m_cpuCnt);
        m_smpManager->attachAlphaMemorySystem(m_alphaMemorySystem.data());
        m_smpManager->attachIrqController(m_irqController.data());
        m_smpManager->attachDeviceManager(m_deviceManager.data());
        m_smpManager->attachSystemBus(m_systemBus.data());
        m_smpManager->attachSafeMemory(m_safeMemory.data());
        m_smpManager->attachMmioManager(m_mmioManager.data());

        m_alphaMemorySystem->attachSafeMemory(m_safeMemory.data());
        m_alphaMemorySystem->attachIrqController(m_irqController.data());
        m_alphaMemorySystem->attachMMIOManager(m_mmioManager.data());

        m_systemBus->attachIrqController(m_irqController.data());
        
        m_mmioManager->attachIrqController(m_irqController.data());
        m_mmioManager->attachSystemBus(m_systemBus.data());

        m_deviceManager->attachIrqController(m_irqController.data());
        m_deviceManager->attachMmioManager(m_mmioManager.data());
        m_deviceManager->attachSystemBus(m_systemBus.data());
    }
    /**
     * @brief Start the emulation
     * @return True if emulation started successfully
     */
    bool start();

    /**
     * @brief Pause the emulation
     */
    void pause();

    /**
     * @brief Resume a paused emulation
     */
    void resume();

    /**
     * @brief Stop the emulation
     */
    void stop();

    /**
     * @brief Reset the entire system
     */
    void reset();

    /**
     * @brief Get a CPU by index
     * @param index The CPU index (0-based)
     * @return Pointer to the CPU or nullptr if invalid index
     */
    AlphaCoreContext* getCPU(int index) const;

    /**
     * @brief Get the memory subsystem
     * @return Pointer to the memory system
     */
    SafeMemory* getMemory() const;

    /**
     * @brief Get the device manager
     * @return Pointer to the device manager
     */
    DeviceManager* getDeviceManager() const;

    /**
     * @brief Get the MMIO manager
     * @return Pointer to the MMIO manager
     */
    MMIOManager* getMMIOManager() const;

    /**
     * @brief Get the IRQ controller
     * @return Pointer to the IRQ controller
     */
    IRQController* getIRQController() const;

    /**
     * @brief Get the system bus
     * @return Pointer to the system bus
     */
    SystemBus* getSystemBus() const;

    /**
     * @brief Get the current emulation state
     * @return The current state
     */
    EmulationState getState() const;

    /**
     * @brief Load a program binary into memory
     * @param filename Path to the binary file
     * @param loadAddress Physical address to load at
     * @param setCPUPC If true, set CPU program counter to loadAddress
     * @return True if load was successful
     */
    bool loadProgram(const QString& filename, quint64 loadAddress, bool setCPUPC = true);

    /**
     * @brief Save system state to a file
     * @param filename Path to save state to
     * @return True if save was successful
     */
    bool saveState(const QString& filename);

    /**
     * @brief Load system state from a file
     * @param filename Path to load state from
     * @return True if load was successful
     */
    bool loadState(const QString& filename);

    /**
     * @brief Get a status report on the emulator
     * @return Status string
     */
    QString getStatusReport() const;

    /**
     * @brief Enable or disable debug output
     * @param enable True to enable, false to disable
     */
    void setDebugOutput(bool enable);




   #pragma region Dependency Setters
 /**
     * @brief Set CPU execution speed
     * @param mips Target MIPS (instructions per second)
     * 0 means unlimited speed
     */
    void setCPUSpeed(int mips);
    
    void createExecutors(int cpuId);
    /**
     * @brief Set Memory in MB or GB
     * @param Memory Formats will be converted to MB
     * Defaults to 4096MB
     */
    void setMemoryAlloc(qint64 memory_ = 4096) {
        this->m_smpManager.data()->setMemoryAlloc(memory_);
    }
    void setAlphaCpuCnt(int cpuCnt_) {
        this->m_smpManager.data()->setCPUVectorPlaceHolder(cpuCnt_);   // CPUs are instantiated via a QVector Place Holder
    }


#pragma endregion Dependency Setters

signals:
    /**
     * @brief Signal emitted when emulation starts
     */
    void emulationStarted();

    /**
     * @brief Signal emitted when emulation pauses
     */
    void emulationPaused();

    /**
     * @brief Signal emitted when emulation resumes
     */
    void emulationResumed();

    /**
     * @brief Signal emitted when emulation stops
     */
    void emulationStopped();

    /**
     * @brief Signal emitted when system state changes
     * @param statusMessage Status message describing the change
     */
    void statusChanged(const QString& statusMessage);

    /**
     * @brief Signal emitted when a CPU trap occurs
     * @param cpuId The CPU ID
     * @param trapType The type of trap
     * @param pc The program counter where the trap occurred
     */
    void cpuTrap(int cpuId, int trapType, quint64 pc);

    /**
     * @brief Signal emitted when a CPU instruction is executed
     * @param cpuId The CPU ID
     * @param pc The program counter
     * @param instruction The instruction word
     */
    void instructionExecuted(int cpuId, quint64 pc, quint32 instruction);

    /**
     * @brief Signal emitted when memory is accessed
     * @param address The memory address
     * @param value The value read or written
     * @param isWrite True for write, false for read
     * @param size The size of the access (1, 2, 4, or 8 bytes)
     */
    void memoryAccessed(quint64 address, quint64 value, bool isWrite, int size);

    /**
     * @brief Signal emitted when a device is accessed
     * @param deviceId The device identifier
     * @param offset The device-relative address
     * @param value The value read or written
     * @param isWrite True for write, false for read
     * @param size The size of the access (1, 2, 4, or 8 bytes)
     */
    void deviceAccessed(const QString& deviceId, quint64 offset, quint64 value, bool isWrite, int size);


	
private:
    EmulationState state;
    quint8 m_cpuCnt = 1;
    QScopedPointer<SystemBus> m_systemBus;
    QScopedPointer<IRQController> m_irqController;
    QScopedPointer<MMIOManager> m_mmioManager;
    QScopedPointer<DeviceManager> m_deviceManager;
    QScopedPointer<AlphaSMPManager> m_smpManager;
    QScopedPointer<AlphaMemorySystem> m_alphaMemorySystem;
    QScopedPointer<SafeMemory> m_safeMemory;
    QScopedPointer<UnifiedDataCache> m_unifiedCache;
//     QVector<QThread*> cpuThreads;
//     QVector<AlphaCoreContext*> cpus;
  

    bool debugEnabled;
    int cpuSpeedMIPS;
    QMutex stateLock;

    /**
     * @brief Create CPU cores
     * @param count Number of CPUs to create
     */
    void createCPUs(int count);

    /**
     * @brief Create CPU threads
     */
    void createCPUThreads();

    /**
     * @brief Connect signals and slots for system components
     */
    void connectSignals();

    /**
     * @brief Clean up resources
     */
    void cleanup();

    /**
     * @brief CPU thread worker function
     * @param cpuId The CPU ID
     */
    void cpuThreadWorker(int cpuId);
};

#endif // EmulatorManager_h__


/*

/*
// Usage Examples for TLB Cache Integration

// Load emulator configuration
QSettingsConfigLoader *loader = new QSettingsConfigLoader("alpha_emulator.ini");

// Get TLB Cache Integration configuration
auto tlbCacheConfig = loader->getTlbCacheIntegrationConfig();
qDebug() << "TLB Cache: prefetchDepth=" << tlbCacheConfig.prefetchDepth
         << "prefetchDistance=" << tlbCacheConfig.prefetchDistance;

// Get TLB System configuration
auto tlbSystemConfig = loader->getTlbSystemConfig();
qDebug() << "TLB System: entries=" << tlbSystemConfig.entriesPerCpu
         << "maxCPUs=" << tlbSystemConfig.maxCpus;

// Get cache configurations for different levels
auto l1DataConfig = loader->getUnifiedCacheConfig("L1Data");
auto l1InstConfig = loader->getUnifiedCacheConfig("L1Instruction");
auto l2Config = loader->getUnifiedCacheConfig("L2");
auto l3Config = loader->getUnifiedCacheConfig("L3");

// Initialize TLB Cache Integrator with configuration
tlbCacheIntegrator *integrator = new tlbCacheIntegrator(tlbCoordinator, tlbSystemConfig.maxCpus);
integrator->setCacheLineSize(tlbCacheConfig.cacheLineSize);
integrator->setPageSize(tlbCacheConfig.pageSize);
integrator->setPrefetchDepth(tlbCacheConfig.prefetchDepth);
integrator->setPrefetchDistance(tlbCacheConfig.prefetchDistance);
integrator->setEfficiencyTarget(tlbCacheConfig.efficiencyTarget);
integrator->enableCoherency(tlbCacheConfig.coherencyEnabled);
integrator->enablePrefetch(tlbCacheConfig.prefetchEnabled);

// Initialize caches with configuration
UnifiedDataCache::Config l1Config;
l1Config.numSets = l1DataConfig.numSets;
l1Config.associativity = l1DataConfig.associativity;
l1Config.lineSize = l1DataConfig.lineSize;
l1Config.enablePrefetch = l1DataConfig.enablePrefetch;
l1Config.enableStatistics = l1DataConfig.enableStatistics;
l1Config.enableCoherency = l1DataConfig.enableCoherency;
l1Config.statusUpdateInterval = l1DataConfig.statusUpdateInterval;
l1Config.coherencyProtocol = l1DataConfig.coherencyProtocol;

UnifiedDataCache *l1Cache = new UnifiedDataCache(l1Config);
*/
*/