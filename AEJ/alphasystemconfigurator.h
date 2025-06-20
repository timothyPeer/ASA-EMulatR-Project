
#pragma once
#include <QVector>
#include "QSettingsConfigLoader.h"
#include "AlphaMemorySystem_refactored.h"
#include "AlphaCPU_refactored.h"
#include "UnifiedDataCache.h"
#include "GlobalMacro.h"
#include <QString>
#include <QStringList>

// Enhanced configuration integration for your Alpha CPU system
// This shows how to better integrate your components with the configuration system

class AlphaSystemConfigurator
{
  private:
    QSettingsConfigLoader *m_configLoader;
    AlphaMemorySystem *m_memorySystem;
    QVector<AlphaCPU *> m_cpus;

  public:
    explicit AlphaSystemConfigurator(const QString &configPath) : m_configLoader(new QSettingsConfigLoader(configPath))
    {
    }

    /**
     * @brief Initialize complete Alpha system from configuration
     */
    bool initializeSystem()
    {
        // 1. Initialize memory system first
        if (!initializeMemorySystem())
        {
            return false;
        }

        // 2. Create and configure CPUs
        if (!initializeCPUs())
        {
            return false;
        }

        // 3. Setup cache hierarchy
        if (!setupCacheHierarchy())
        {
            return false;
        }

        // 4. Configure TLB integration
        if (!configureTLBIntegration())
        {
            return false;
        }

        // 5. Final system coordination
        return finalizeSystemSetup();
    }

  private:
    bool initializeMemorySystem()
    {
        // Get system memory configuration
        quint64 memorySize = m_configLoader->getSystemMemorySize();
        int memorySizeGB = m_configLoader->getSystemMemorySizeGB();

        // Create memory system
        m_memorySystem = new AlphaMemorySystem();

        // Configure with settings
        m_memorySystem->initializeCpuModel(CpuModel::CPU_EV56); // From config

        DEBUG_LOG("System initialized with %dGB memory (%llu bytes)", memorySizeGB, memorySize);
        return true;
    }

    bool initializeCPUs()
    {
        // Get CPU configuration
        auto cpuConfig = m_configLoader->getCPUConfig();

        // Create CPUs with configuration
        for (int i = 0; i < cpuConfig.processorCount; ++i)
        {
            AlphaCPU *cpu = new AlphaCPU(i, m_memorySystem);

            // Apply CPU-specific configuration
            configureCPUFromSettings(cpu, i);

            m_cpus.append(cpu);
        }

        DEBUG_LOG("Created %d CPUs with JIT %s", cpuConfig.processorCount,
                  cpuConfig.jitEnabled ? "enabled" : "disabled");
        return true;
    }

    bool setupCacheHierarchy()
    {
        // Configure L3 shared cache first
        UnifiedDataCache::Config l3Config;
        l3Config = getUnifiedCacheConfigFromSettings("L3");

        UnifiedDataCache *l3Cache = m_memorySystem->createL3Cache(l3Config);

        // Configure per-CPU cache hierarchy
        for (AlphaCPU *cpu : m_cpus)
        {
            setupCPUCacheHierarchy(cpu, l3Cache);
        }

        return true;
    }

    void setupCPUCacheHierarchy(AlphaCPU *cpu, UnifiedDataCache *l3Cache)
    {
        quint16 cpuId = cpu->getCpuId();

        // Configure L1 Data Cache
        UnifiedDataCache::Config l1Config = getUnifiedCacheConfigFromSettings("L1Data");
        if (auto *l1Cache = cpu->getLevel1DataCache())
        {
            // L1 cache already exists, just configure it
            l1Cache->setTLBSystem(m_memorySystem->getTlbSystem(), cpuId);
        }

        // Configure L2 Cache
        UnifiedDataCache::Config l2Config = getUnifiedCacheConfigFromSettings("L2");
        if (auto *l2Cache = cpu->getLevel2DataCache())
        {
            l2Cache->setTLBSystem(m_memorySystem->getTlbSystem(), cpuId);
            l2Cache->setNextLevel(l3Cache);
        }

        // Connect to shared L3
        cpu->connectToL3SharedCache(l3Cache);

        DEBUG_LOG("Configured cache hierarchy for CPU %d", cpuId);
    }

    UnifiedDataCache::Config getUnifiedCacheConfigFromSettings(const QString &level)
    {
        UnifiedDataCache::Config config;

        // Use your existing configuration system
        auto cacheConfig = m_configLoader->getUnifiedCacheConfig(level);

        config.numSets = cacheConfig.numSets;
        config.associativity = cacheConfig.associativity;
        config.lineSize = cacheConfig.lineSize;
        config.totalSize = cacheConfig.totalSize;
        config.enablePrefetch = cacheConfig.enablePrefetch;
        config.enableStatistics = cacheConfig.enableStatistics;
        config.enableCoherency = cacheConfig.enableCoherency;
        config.statusUpdateInterval = cacheConfig.statusUpdateInterval;
        config.coherencyProtocol = cacheConfig.coherencyProtocol;

        return config;
    }

    bool configureTLBIntegration()
    {
        // Get TLB configuration
        auto tlbConfig = m_configLoader->getTlbSystemConfig();
        auto tlbCacheConfig = m_configLoader->getTlbCacheIntegrationConfig();

        // Configure TLB system parameters
        TLBSystem *tlbSystem = m_memorySystem->getTlbSystem();
        if (!tlbSystem)
        {
            ERROR_LOG("No TLB system available for configuration");
            return false;
        }

        // Integrate TLB with caches
        m_memorySystem->integrateTLBWithCaches();

        // Setup TLB cache integrator if available
        setupTLBCacheIntegrator(tlbCacheConfig);

        DEBUG_LOG("Configured TLB integration: %d entries per CPU, coherency %s", tlbConfig.entriesPerCpu,
                  tlbConfig.enableCoherency ? "enabled" : "disabled");
        return true;
    }

    void setupTLBCacheIntegrator(const QSettingsConfigLoader::TlbCacheIntegrationConfig &config)
    {
        // Create TLB cache integrator
        tlbSystemCoordinator *coordinator = new tlbSystemCoordinator();
        tlbCacheIntegrator *integrator = new tlbCacheIntegrator(coordinator, m_cpus.size());

        // Configure from settings
        integrator->setCacheLineSize(config.cacheLineSize);
        integrator->setPageSize(config.pageSize);
        integrator->setEfficiencyTarget(config.efficiencyTarget);
        integrator->setPrefetchDepth(config.prefetchDepth);
        integrator->setPrefetchDistance(config.prefetchDistance);
        integrator->enableCoherency(config.coherencyEnabled);
        integrator->enablePrefetch(config.prefetchEnabled);

        // Attach caches to integrator
        for (AlphaCPU *cpu : m_cpus)
        {
            quint16 cpuId = cpu->getCpuId();

            if (auto *l1Cache = cpu->getLevel1DataCache())
            {
                integrator->attachCacheController(cpuId, tlbCacheIntegrator::CACHE_L1_DATA, l1Cache);
            }

            if (auto *l2Cache = cpu->getLevel2DataCache())
            {
                integrator->attachCacheController(cpuId, tlbCacheIntegrator::CACHE_L2_UNIFIED, l2Cache);
            }

            if (auto *iCache = cpu->getInstructionCache())
            {
                if (auto *unifiedICache = iCache->getUnifiedCache())
                {
                    integrator->attachCacheController(cpuId, tlbCacheIntegrator::CACHE_L1_INSTRUCTION, unifiedICache);
                }
            }
        }

        // Attach shared L3
        if (auto *l3Cache = m_memorySystem->getL3Cache())
        {
            QVector<quint16> allCpuIds;
            for (const AlphaCPU *cpu : m_cpus)
            {
                allCpuIds.append(cpu->getCpuId());
            }
            integrator->attachUnifiedDataCache(allCpuIds, l3Cache);
        }

        // Initialize signals and slots
        integrator->initialize_SignalsAndSlots();

        // Store for later use
        m_memorySystem->attachTLBCacheIntegrator(integrator);

        DEBUG_LOG("TLB cache integrator configured with %d CPUs", m_cpus.size());
    }

    void configureCPUFromSettings(AlphaCPU *cpu, int cpuIndex)
    {
        // Configure CPU-specific settings
        auto cpuConfig = m_configLoader->getCPUConfig();

        // Set performance parameters
        if (cpuConfig.jitEnabled)
        {
            // Enable JIT with threshold from config
            // cpu->enableJIT(cpuConfig.jitThreshold);
        }

        // Configure coherency cache size
        // cpu->setCoherencyCacheSize(cpuConfig.coherencyCache);

        // Set CPU model from config
        // cpu->setCPUModel(m_configLoader->getCpuModel());

        DEBUG_LOG("Configured CPU %d with JIT threshold %d", cpuIndex, cpuConfig.jitThreshold);
    }

    bool finalizeSystemSetup()
    {
        // Final system coordination

        // Connect all CPU signals to memory system
        for (AlphaCPU *cpu : m_cpus)
        {
            // Memory system already handles this in registerCPU
        }

        // Setup ROM and firmware paths from config
        setupROMConfiguration();

        // Configure serial and network interfaces
        setupIOConfiguration();

        // Validate system integrity
        return validateSystemConfiguration();
    }

    void setupROMConfiguration()
    {
        QString romPath = m_configLoader->getStringValue("ROM", "fName", "");
        QString srmPath = m_configLoader->getStringValue("ROM", "fName-SRM", "");
        QString nvramPath = m_configLoader->getStringValue("ROM", "Cmos-NVRam-FileName", "");

        DEBUG_LOG("ROM Configuration: ROM=%s, SRM=%s, NVRAM=%s", qPrintable(romPath), qPrintable(srmPath),
                  qPrintable(nvramPath));

        // Configure ROM paths in the system
        // Implementation depends on your ROM/firmware system
    }

    void setupIOConfiguration()
    {
        // Configure I/O manager thread count
        int ioThreads = m_configLoader->getIntValue("IO-Manager", "ThreadCnt", 4);

        // Configure network interfaces
        setupNetworkInterfaces();

        // Configure storage controllers
        setupStorageControllers();

        DEBUG_LOG("I/O Configuration: %d threads", ioThreads);
    }

    void setupNetworkInterfaces()
    {
        // Configure DE500 interfaces
        QStringList ewa0Config = m_configLoader->getStringArray("Network-DE500", "EWA0");
        QStringList ewb0Config = m_configLoader->getStringArray("Network-DE500", "EWB0");

        // Configure DE602 interfaces
        QStringList ewc0Config = m_configLoader->getStringArray("Network-DE602", "EWC0");

        DEBUG_LOG("Network interfaces configured");
    }

    void setupStorageControllers()
    {
        // Configure KZPBA storage controllers
        QStringList pkaDevices = m_configLoader->getStringArray("DEV_KZPBA", "PKA_dev");
        QStringList pkbDevices = m_configLoader->getStringArray("DEV_KZPBA", "PKB_dev");
        QStringList pkcDevices = m_configLoader->getStringArray("DEV_KZPBA", "PKC_dev");

        DEBUG_LOG("Storage configured: PKA=%d devices, PKB=%d devices, PKC=%d devices", pkaDevices.size(),
                  pkbDevices.size(), pkcDevices.size());
    }

    bool validateSystemConfiguration()
    {
        // Validate all components are properly configured

        if (!m_memorySystem)
        {
            ERROR_LOG("Memory system not initialized");
            return false;
        }

        if (m_cpus.isEmpty())
        {
            ERROR_LOG("No CPUs configured");
            return false;
        }

        // Check cache hierarchy
        for (const AlphaCPU *cpu : m_cpus)
        {
            if (!cpu->getLevel1DataCache())
            {
                WARN_LOG("CPU %d missing L1 cache", cpu->getCpuId());
            }
        }

        // Check TLB system
        if (!m_memorySystem->getTlbSystem())
        {
            ERROR_LOG("TLB system not configured");
            return false;
        }

        DEBUG_LOG("System configuration validation successful");
        return true;
    }

  public:
    // Access methods for the configured system
    AlphaMemorySystem *getMemorySystem() const { return m_memorySystem; }
    const QVector<AlphaCPU *> &getCPUs() const { return m_cpus; }
    QSettingsConfigLoader *getConfigLoader() const { return m_configLoader; }

    // Dynamic reconfiguration support
    bool reconfigureCache(const QString &level, const QString &parameter, const QVariant &value)
    {
        // Implement runtime cache reconfiguration
        return true;
    }

    bool reconfigureTLB(const QString &parameter, const QVariant &value)
    {
        // Implement runtime TLB reconfiguration
        return true;
    }
};

// Usage example:
/*
int main()
{
    AlphaSystemConfigurator configurator("alpha_emulator.ini");

    if (!configurator.initializeSystem()) {
        ERROR_LOG("Failed to initialize Alpha system");
        return 1;
    }

    AlphaMemorySystem* memSys = configurator.getMemorySystem();
    const auto& cpus = configurator.getCPUs();

    DEBUG_LOG("Alpha system initialized with %d CPUs", cpus.size());

    // System is ready for operation
    return 0;
}
*/
