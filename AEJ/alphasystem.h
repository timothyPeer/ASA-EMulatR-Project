// AlphaSystem.h - Overall system header
#ifndef ALPHASYSTEM_H
#define ALPHASYSTEM_H

#include <QObject>
#include <QString>
#include <QVector>

class AlphaSMPManager;
class AlphaMemorySystem;
class AlphaExceptionHandler;
class AlphaJITCompiler;
class QSettings;

/**
 * @brief AlphaSystem - Top-level system manager for the Alpha emulation
 *
 * This class coordinates all components of the emulation system and provides
 * a unified interface for control and configuration.
 */
class AlphaSystem : public QObject
{
    Q_OBJECT

public:
    // System configuration
    struct SystemConfig {
        int cpuCount;
        quint64 memorySize;
        QString bootDevice;
        bool enableJIT;
        int jitOptimizationLevel;
        bool enableSMP;
        bool enableDebugTrace;
        QString configFile;
    };

    explicit AlphaSystem(QObject* parent = nullptr);
    ~AlphaSystem();

    // Initialization and configuration
    bool initialize(const SystemConfig& config);
    bool loadConfiguration(const QString& configFile);
    bool saveConfiguration(const QString& configFile);
    void shutdown();

    // Basic control
    bool loadOperatingSystem(const QString& osImage);
    bool loadProgram(const QString& programFile, quint64& entryPoint);
    bool start(quint64 entryPoint = 0);
    void pause();
    void resume();
    void stop();

    // Component access
    AlphaSMPManager* getSMPManager() const { return m_smpManager; }
    AlphaMemorySystem* getMemorySystem() const { return m_memorySystem; }
    AlphaExceptionHandler* getExceptionHandler() const { return m_exceptionHandler; }

    // System status
    bool isRunning() const;
    bool isPaused() const;
    SystemConfig getConfig() const { return m_config; }

public slots:
    // System control slots
    void reboot();
    void reset();
    void setJITOptimizationLevel(int level);
    void enableSMP(bool enable);
    void setDebugTraceEnabled(bool enable);

    // Memory and I/O operations
    void dumpMemory(quint64 startAddr, quint64 endAddr, const QString& filename);
    void loadMemory(quint64 addr, const QString& filename);

signals:
    // System state changes
    void systemInitialized();
    void systemStarted();
    void systemPaused();
    void systemResumed();
    void systemStopping();
    void systemStopped();
    void systemError(const QString& errorMessage);

    // Configuration changes
    void configurationChanged();
    void smpStateChanged(bool enabled);
    void jitOptimizationLevelChanged(int level);
    void debugTraceStateChanged(bool enabled);

    // System events
    void programLoaded(const QString& programFile, quint64 entryPoint);
    void operatingSystemLoaded(const QString& osImage);

private:
    // Core components
    AlphaSMPManager* m_smpManager;
    AlphaMemorySystem* m_memorySystem;
    AlphaExceptionHandler* m_exceptionHandler;
    AlphaJITCompiler* m_jitCompiler;

    // Configuration
    SystemConfig m_config;
    QSettings* m_settings;

    // Helper methods
    bool initializeComponents();
    void applyConfiguration();
    void loadDefaultConfiguration();
    bool verifySystemConfiguration();
};

#endif // ALPHASYSTEM_H