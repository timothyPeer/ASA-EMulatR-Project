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
#include "SafeMemory.h"
#include "MMIOManager.h"
#include "DeviceManager.h"
#include "IRQController.h"
#include "SystemBus.h"
#include "alphasmpmanager.h"

/**
 * @brief Central manager for the Alpha emulator system
 *
 * EmulatorManager creates and coordinates all components of the
 * emulation system, including CPUs, memory, devices, and buses.
 * It provides a high-level interface for controlling the emulation.
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

    /**
     * @brief Initialize the emulation system
     * @param memorySize Physical memory size in bytes
     * @param cpuCount Number of CPU cores to create
     * @return True if initialization was successful
     */
    bool initialize(quint64 memorySize = 64 * 1024 * 1024, int cpuCount = 1);

    void initialize_signalsAndSlots();
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
    QScopedPointer<SafeMemory> memory;
    QScopedPointer<SystemBus> systemBus;
    QScopedPointer<IRQController> irqController;
    QScopedPointer<MMIOManager> mmioManager;
    QScopedPointer<DeviceManager> deviceManager;
    QScopedPointer<AlphaSMPManager> m_smpManager;
    QVector<QThread*> cpuThreads;
    QVector<AlphaCoreContext*> cpus;
  

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