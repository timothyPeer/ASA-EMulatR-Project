// QSettingsConfigLoader.h - Alpha Emulator Configuration System
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QVector>
#include "GlobalMacro.h"




/**
 * @brief Configuration loader interface for Alpha emulator configuration
 */
class ConfigLoader {
public:
    virtual ~ConfigLoader() = default;

    // Basic value getters
    virtual int getIntValue(const QString& section, const QString& key, int defaultValue) = 0;
    virtual QString getStringValue(const QString& section, const QString& key, const QString& defaultValue) = 0;
    virtual bool getBoolValue(const QString& section, const QString& key, bool defaultValue) = 0;
    virtual double getDoubleValue(const QString& section, const QString& key, double defaultValue) = 0;

    // Array support for virtual devices
    virtual QStringList getStringArray(const QString& section, const QString& key) = 0;
    virtual QVector<int> getIntArray(const QString& section, const QString& key) = 0;

    // Section/key management
    virtual bool hasSection(const QString& section) = 0;
    virtual bool hasKey(const QString& section, const QString& key) = 0;
    virtual QStringList getKeysInSection(const QString& section) = 0;
    virtual QStringList getAllSections() = 0;

    // Configuration management
    virtual bool reload() = 0;
    virtual QString getConfigSource() const = 0;
};

/**
 * @brief Alpha Emulator Configuration Loader
 * Supports arrays for virtual device configurations
 */
class QSettingsConfigLoader : public ConfigLoader {
private:
    QString m_configPath;
    QSettings::Format m_format;
    mutable QSettings* m_settings;

public:

    /**
     * @brief Get TLB Cache Integration configuration
     */
    struct TlbCacheIntegrationConfig {
        quint32 prefetchDepth;
        quint32 prefetchDistance;
        quint32 cacheLineSize;
        quint32 pageSize;
        qreal efficiencyTarget;
        bool coherencyEnabled;
        bool prefetchEnabled;
        bool writebackEnabled;
    };

    TlbCacheIntegrationConfig getTlbCacheIntegrationConfig() {
        TlbCacheIntegrationConfig config;
        config.prefetchDepth = this->getIntValue("TlbCacheIntegration", "prefetchDepth", 2);
        config.prefetchDistance = this->getIntValue("TlbCacheIntegration", "prefetchDistance", 128);
        config.cacheLineSize = this->getIntValue("TlbCacheIntegration", "cacheLineSize", 64);
        config.pageSize = this->getIntValue("TlbCacheIntegration", "pageSize", 4096);
        config.efficiencyTarget = this->getDoubleValue("TlbCacheIntegration", "efficiencyTarget", 0.95);
        config.coherencyEnabled = this->getBoolValue("TlbCacheIntegration", "coherencyEnabled", true);
        config.prefetchEnabled = this->getBoolValue("TlbCacheIntegration", "prefetchEnabled", true);
        config.writebackEnabled = this->getBoolValue("TlbCacheIntegration", "writebackEnabled", true);

        DEBUG_LOG(QString("TlbCacheIntegration: prefetchDepth=%1, prefetchDistance=%2, cacheLineSize=%3, pageSize=%4")
            .arg(   config.prefetchDepth).arg(config.prefetchDistance).arg(config.cacheLineSize).arg(config.pageSize));

        return config;
    }

    /**
 * @brief Get Cache Configuration for UnifiedDataCache
 */
    struct UnifiedCacheConfig {
        size_t numSets;
        size_t associativity;
        size_t lineSize;
        size_t totalSize;
        bool enablePrefetch;
        bool enableStatistics;
        bool enableCoherency;
        quint16 statusUpdateInterval;
        QString coherencyProtocol;
    };

    UnifiedCacheConfig getUnifiedCacheConfig(const QString& cacheLevel = "L1Data") {
        QString section = QString("Cache-%1").arg(cacheLevel);

        UnifiedCacheConfig config;
        config.numSets = this->getIntValue(section, "numSets", 64);
        config.associativity = this->getIntValue(section, "associativity", 4);
        config.lineSize = this->getIntValue(section, "lineSize", 64);
        config.totalSize = config.numSets * config.associativity * config.lineSize;
        config.enablePrefetch = this->getBoolValue(section, "enablePrefetch", true);
        config.enableStatistics = this->getBoolValue(section, "enableStatistics", true);
        config.enableCoherency = this->getBoolValue(section, "enableCoherency", true);
        config.statusUpdateInterval = this->getIntValue(section, "statusUpdateInterval", 1000);
        config.coherencyProtocol = this->getStringValue(section, "coherencyProtocol", "MESI");

        DEBUG_LOG("Cache-%1: sets=%2, assoc=%3, lineSize=%4, totalSize=%5",
            cacheLevel, config.numSets, config.associativity, config.lineSize, config.totalSize);

        return config;
    }

    /**
 * @brief Get TLB System configuration
 */
    struct TlbSystemConfig {
        int entriesPerCpu;
        int maxCpus;
        bool enableStatistics;
        bool enableCoherency;
        int replacementPolicy; // 0=LRU, 1=Random, 2=FIFO
    };

    TlbSystemConfig getTlbSystemConfig() {
        TlbSystemConfig config;
        config.entriesPerCpu = this->getIntValue("TlbSystem", "entriesPerCpu", 128);
        config.maxCpus = this->getIntValue("TlbSystem", "maxCpus", 16);
        config.enableStatistics = this->getBoolValue("TlbSystem", "enableStatistics", true);
        config.enableCoherency = this->getBoolValue("TlbSystem", "enableCoherency", true);
        config.replacementPolicy = this->getIntValue("TlbSystem", "replacementPolicy", 0); // LRU default

        DEBUG_LOG(QString("TlbSystem: entriesPerCpu=%1, maxCpus=%2, enableStats=%3, enableCoherency=%4")
            .arg(config.entriesPerCpu).arg(config.maxCpus).arg(config.enableStatistics).arg(config.enableCoherency));

        return config;
    }

    explicit QSettingsConfigLoader(const QString& configPath,
        QSettings::Format format = QSettings::IniFormat)
        : m_configPath(configPath)
        , m_format(format)
        , m_settings(nullptr) {

        if (!QFile::exists(configPath)) {
            ERROR_LOG(QString("ConfigLoader: Configuration file not found: %1").arg(configPath));
        }

        m_settings = new QSettings(configPath, format);
        DEBUG_LOG(QString("ConfigLoader: Loaded Alpha emulator configuration from %1").arg(configPath));
    }

    ~QSettingsConfigLoader() {
        delete m_settings;
    }

    // Basic value getters
    int getIntValue(const QString& section, const QString& key, int defaultValue) override {
        if (!m_settings) return defaultValue;

        m_settings->beginGroup(section);
        int value = m_settings->value(key, defaultValue).toInt();
        m_settings->endGroup();

        return value;
    }

    QString getStringValue(const QString& section, const QString& key, const QString& defaultValue) override {
        if (!m_settings) return defaultValue;

        m_settings->beginGroup(section);
        QString value = m_settings->value(key, defaultValue).toString();
        m_settings->endGroup();

        return value;
    }

    bool getBoolValue(const QString& section, const QString& key, bool defaultValue) override {
        if (!m_settings) return defaultValue;

        m_settings->beginGroup(section);
        bool value = m_settings->value(key, defaultValue).toBool();
        m_settings->endGroup();

        return value;
    }

    double getDoubleValue(const QString& section, const QString& key, double defaultValue) override {
        if (!m_settings) return defaultValue;

        m_settings->beginGroup(section);
        double value = m_settings->value(key, defaultValue).toDouble();
        m_settings->endGroup();

        return value;
    }

    // Array support for virtual devices
    QStringList getStringArray(const QString& section, const QString& key) override {
        if (!m_settings) return QStringList();

        m_settings->beginGroup(section);

        // Check for array notation: key[0], key[1], etc.
        QStringList result;
        int index = 0;
        while (true) {
            QString arrayKey = QString("%1[%2]").arg(key).arg(index);
            if (!m_settings->contains(arrayKey)) {
                break;
            }
            result.append(m_settings->value(arrayKey).toString());
            index++;
        }

        // If no array found, check for single value
        if (result.isEmpty() && m_settings->contains(key)) {
            result.append(m_settings->value(key).toString());
        }

        m_settings->endGroup();
        return result;
    }

    QVector<int> getIntArray(const QString& section, const QString& key) override {
        if (!m_settings) return QVector<int>();

        m_settings->beginGroup(section);

        QVector<int> result;
        int index = 0;
        while (true) {
            QString arrayKey = QString("%1[%2]").arg(key).arg(index);
            if (!m_settings->contains(arrayKey)) {
                break;
            }
            result.append(m_settings->value(arrayKey).toInt());
            index++;
        }

        m_settings->endGroup();
        return result;
    }

    // Section/key management
    bool hasSection(const QString& section) override {
        if (!m_settings) return false;
        return m_settings->childGroups().contains(section);
    }

    bool hasKey(const QString& section, const QString& key) override {
        if (!m_settings) return false;

        m_settings->beginGroup(section);
        bool exists = m_settings->contains(key);
        m_settings->endGroup();

        return exists;
    }

    QStringList getKeysInSection(const QString& section) override {
        if (!m_settings) return QStringList();

        m_settings->beginGroup(section);
        QStringList keys = m_settings->childKeys();
        m_settings->endGroup();

        return keys;
    }

    QStringList getAllSections() override {
        if (!m_settings) return QStringList();
        return m_settings->childGroups();
    }

    bool reload() override {
        if (!m_settings) return false;

        delete m_settings;
        m_settings = new QSettings(m_configPath, m_format);

        DEBUG_LOG(QString("ConfigLoader: Reloaded Alpha emulator configuration from %1").arg(m_configPath));
        return true;
    }

    QString getConfigSource() const override {
        return m_configPath;
    }

    // Helper methods for common emulator configurations

    /**
     * @brief Get System Memory configuration in bytes
     * @return Memory size in bytes (minimum 4GB)
     */
    quint64 getSystemMemorySize() {
        // Get memory size in GB from config
        int memorySizeGB = this->getIntValue("System", "MemorySize", 8);

        // Enforce minimum of 4GB
        if (memorySizeGB < 4) {
            WARN_LOG(QString("System memory %1GB is below minimum 4GB, using 4GB").arg(memorySizeGB));
            memorySizeGB = 4;
        }

        // Convert GB to bytes
        quint64 memorySizeBytes = static_cast<quint64>(memorySizeGB) * 1024ULL * 1024ULL * 1024ULL;

        DEBUG_LOG(QString("System Memory: %1GB (%2 bytes)").arg(memorySizeGB).arg(memorySizeBytes));

        return memorySizeBytes;
    }

    /**
     * @brief Get System Memory configuration in GB
     * @return Memory size in GB (minimum 4GB)
     */
    int getSystemMemorySizeGB() {
        int memorySizeGB = this->getIntValue("System", "MemorySize", 8);

        // Enforce minimum of 4GB
        if (memorySizeGB < 4) {
            WARN_LOG(QString("System memory %1GB is below minimum 4GB, using 4GB").arg(memorySizeGB));
            memorySizeGB = 4;
        }

        return memorySizeGB;
    }
    struct CPUConfig {
        int processorCount;
        int coherencyCache;
        bool jitEnabled;
        int jitThreshold;
    };

    CPUConfig getCPUConfig() {
        CPUConfig config;
        config.processorCount = this->getIntValue("CPU", "Processor-Count", 1);
        config.coherencyCache = this->getIntValue("CPU", "Coherency-Cache", 2048);
        config.jitEnabled = this->getBoolValue("CPU", "JIT", true);
        config.jitThreshold = this->getIntValue("CPU", "JIT-Threshold", 50);
        return config;
    }

    /**
     * @brief Get Storage Controller device arrays
     */
    QStringList getStorageDevices(const QString& controllerName, int scsiId) {
        QString section = QString("DEV_%1").arg(controllerName);
        QString key = QString("%1_dev").arg(controllerName);
        return this->getStringArray(section, key);
    }

    /**
     * @brief Get Network Interface configuration
     */
    struct NetworkInterface {
        QString name;
        QString interface;
        QString connection;
    };

    QVector<NetworkInterface> getNetworkInterfaces(const QString& deviceType) {
        QVector<NetworkInterface> interfaces;
        QString section = QString("Network-%1").arg(deviceType);

        // This would need custom parsing for complex network configs
        // Implementation depends on exact config format needed

        return interfaces;
    }
};

// ===============================
// Example Alpha Emulator Configuration File
// ===============================

/*
; alpha_emulator.ini - Alpha Server Emulator Configuration

[System]
; Memory size in GB (minimum 4GB, typical values: 4, 8, 16, 32)
MemorySize=8

[CPU]
Processor-Count=4
Coherency-Cache=2048
JIT=true
JIT-Threshold=50

[Session-Log]
fName=c:\asa\es40_instance.log
Method=append
hw-Model=AlphaServer 40
hw-Serial-Number=AX122312341243134

[ROM]
fName=c:\asa\ev67.bin
fName-SRM=clipper.bin
Cmos-NVRam-FileName=clipper.dat

[Serial-Lines]
OPA0_name=OPA0
OPA0_iface=com1:
OPA1_name=OPA0
OPA1_iface=Net:
OPA1_net-cfg_Port=12345
OPA1_application=putty -load opa0

[IO-Manager]
ThreadCnt=4

[Network-DE500]
EWA0_name=EWA0
EWA0_iface=connection:Ethernet0
EWB0_name=EWB0
EWB0_iface=connection:Ethernet1

[Network-DE602]
EWC0_name=EWC0
EWC0_iface=connection:Ethernet2

[Storage-Controllers-KZPBA]
PKA_name=PKA
PKA_scsi-id=2
PKB_name=PKB
PKB_scsi-id=3
PKC_name=PKC
PKC_scsi-id=4

[DEV_KZPBA]
; PKA devices (array support)
PKA_dev[0]=G:\Charon\PaDS20\dka0.vdisk
PKA_dev[1]=\\.\PhysicalDrive0
PKA_dev[2]=G:\Charon\PaDS20\dka0_sys\ST136403LC.vdisk

; PKB devices
PKB_dev[0]=G:\Charon\PaDS20\dka2_dka3.vdisk

; PKC devices
PKC_dev[0]=G:\Charon\PaDS20\ST3146854LC_raid.vdisk
PKC_dev[100]=\\.\Tape0
PKC_dev[101]=file.iso

; Unit configurations with sub-arrays
PKA_units[0]=0,G:\Charon\PaDS20\dka0.vdisk
PKA_units[1]=1,\\.\PhysicalDrive0
PKB_units[0]=0,G:\Charon\PaDS20\dka2.vdisk
PKC_units[0]=0,\\.\Tape0

[CPU]
Processor-Count=4
Coherency-Cache=2048
JIT=true
JIT-Threshold=50
; Shared instruction cache configuration for all CPUs
InstructionCacheSize=32768
InstructionCacheLineSize=64
InstructionCacheAssociativity=4
InstructionCacheReplacement=LRU
InstructionCacheAutoPrefetch=true
*/

// ===============================
// Usage Examples
// ===============================

/*
// Load emulator configuration
QSettingsConfigLoader *loader = new QSettingsConfigLoader("alpha_emulator.ini");

// Get system memory configuration (consolidated and GB-based)
quint64 memorySize = loader->getSystemMemorySize();        // Returns bytes
int memorySizeGB = loader->getSystemMemorySizeGB();        // Returns GB
qDebug() << "System Memory:" << memorySizeGB << "GB (" << memorySize << "bytes)";

QString logFile = loader->getStringValue("Session-Log", "fName", "emulator.log");

// Get CPU configuration with shared cache settings
auto cpuConfig = loader->getCPUConfig();
qDebug() << "CPUs:" << cpuConfig.processorCount << "JIT:" << cpuConfig.jitEnabled;

// Get storage device arrays
QStringList pkaDevices = loader->getStringArray("DEV_KZPBA", "PKA_dev");
for (const QString &device : pkaDevices) {
    qDebug() << "PKA Device:" << device;
}

// Use shared cache config for ALL CPUs
CacheConfig cacheConfig = CacheConfig::fromConfigLoader(loader, "CPU"); // Note: "CPU" not "CPU0"

// Create caches for all CPUs using same configuration
for (int cpuId = 0; cpuId < cpuConfig.processorCount; ++cpuId) {
    AlphaInstructionCache *cache = new AlphaInstructionCache(parent, memorySystem, cacheConfig, cpuId);
    // All CPUs now use the same cache configuration from [CPU] section
}
*/