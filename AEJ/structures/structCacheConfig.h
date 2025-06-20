#pragma once
#include <QString>
#include "enumerations/enumCpuModel.h"
#include "UnifiedDataCache.h"
#include "../AESH/QSettingsConfigLoader.h"


struct CacheConfig
{
    size_t cacheSize = 32768; // 32KB default
    size_t lineSize = 64;     // 64-byte lines
    size_t associativity = 4; // 4-way associative
    QString replacementPolicy = "LRU";
    bool autoPrefetchEnabled = true;
    QString configSource = "default";
    uint64_t indexMask; // e.g. (numSets – 1)

    bool isValid() const
    {
        return (cacheSize > 0) && (lineSize > 0) && (associativity > 0) &&
               (cacheSize % (lineSize * associativity) == 0);
    }

    // Convert to UnifiedDataCache::Config
    UnifiedDataCache::Config toUnifiedConfig() const
    {
        UnifiedDataCache::Config config;
        config.totalSize = cacheSize;
        config.lineSize = lineSize;
        config.associativity = associativity;
        config.numSets = cacheSize / (lineSize * associativity);
        config.enablePrefetch = autoPrefetchEnabled;
        config.enableStatistics = true;
        config.enableCoherency = true;
        return config;
    }

    // Static factory methods (keep existing ones)
    static CacheConfig fromConfigFile(const QString &configPath, const QString &cpuSection);
    static CacheConfig fromConfigLoader(ConfigLoader *loader, const QString &cpuSection);
    static CacheConfig forCpuModel(CpuModel model);
    inline uint64_t getIndexMask() const noexcept { return indexMask; }
};