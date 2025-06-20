#pragma once
#include <QString>
#include <QSettings>
#include <QSettingsConfigLoader.h>
#include "../AEU/bit-utils.h"
#include <QDebug>
#include "enumerations/enumCpuModel.h"

struct CacheConfig
{
    size_t cacheSize = 32768;          // Configurable size
    size_t lineSize = 64;              // Configurable line size
    size_t associativity = 4;          // Configurable associativity
    QString replacementPolicy = "LRU"; // Configurable policy
    bool autoPrefetchEnabled = true;   // Configurable prefetch
    QString configSource = "default";  // Debug tracking
    static inline bool isPowerOfTwo(quint64 x) { return x && !(x & (x - 1)); }
    // Static methods for CacheConfig
    CacheConfig fromConfigFile(const QString &configPath, const QString &cpuSection)
    {
        CacheConfig config;
        config.configSource = QString("file:%1[%2]").arg(configPath, cpuSection);

        QSettings settings(configPath, QSettings::IniFormat);
        settings.beginGroup(cpuSection);

        // Load cache configuration with validation
        config.cacheSize = settings.value("InstructionCacheSize", config.cacheSize).toULongLong();
        config.lineSize = settings.value("InstructionCacheLineSize", config.lineSize).toULongLong();
        config.associativity = settings.value("InstructionCacheAssociativity", config.associativity).toULongLong();
        config.replacementPolicy = settings.value("InstructionCacheReplacement", config.replacementPolicy).toString();
        config.autoPrefetchEnabled =
            settings.value("InstructionCacheAutoPrefetch", config.autoPrefetchEnabled).toBool();

        settings.endGroup();

        if (!config.isValid())
        {
            qWarning() << QString("Invalid cache config in %1[%2], using defaults").arg(configPath, cpuSection);
            return CacheConfig(); // Return default config
        }

        qDebug() << QString("Loaded cache config from %1[%2]: %3KB, %4-way, %5B lines")
                        .arg(configPath, cpuSection)
                        .arg(config.cacheSize / 1024)
                        .arg(config.associativity)
                        .arg(config.lineSize);

        return config;
    }

    bool isValid() const
    {
        return (cacheSize > 0 && isPowerOfTwo(cacheSize) && lineSize > 0 && isPowerOfTwo(lineSize) &&
                associativity > 0 && isPowerOfTwo(associativity) && (cacheSize >= lineSize * associativity));
    }
    CacheConfig fromConfigLoader(ConfigLoader *loader, const QString &cpuSection)
    {
        CacheConfig config;
        config.configSource = QString("ConfigLoader[%1]").arg(cpuSection);

        if (!loader)
        {
            qWarning() << "Null ConfigLoader provided, using default cache config";
            return config;
        }

        // Use ConfigLoader's methods to get cache settings
        config.cacheSize = loader->getIntValue(cpuSection, "InstructionCacheSize", config.cacheSize);
        config.lineSize = loader->getIntValue(cpuSection, "InstructionCacheLineSize", config.lineSize);
        config.associativity = loader->getIntValue(cpuSection, "InstructionCacheAssociativity", config.associativity);
        config.replacementPolicy =
            loader->getStringValue(cpuSection, "InstructionCacheReplacement", config.replacementPolicy);
        config.autoPrefetchEnabled =
            loader->getBoolValue(cpuSection, "InstructionCacheAutoPrefetch", config.autoPrefetchEnabled);

        if (!config.isValid())
        {
            qWarning() << QString("Invalid cache config from ConfigLoader[%1], using defaults").arg(cpuSection);
            return CacheConfig(); // Return default config
        }

        return config;
    }

    CacheConfig forCpuModel(CpuModel model)
    {
        CacheConfig config;
        config.configSource = QString("CpuModel:%1").arg(static_cast<int>(model));

        // Alpha CPU model-specific cache configurations
        switch (model)
        {
        case CpuModel::CPU_EV4:
            config.cacheSize = 8192;  // 8KB I-cache
            config.lineSize = 32;     // 32-byte lines
            config.associativity = 1; // Direct mapped
            config.autoPrefetchEnabled = false;
            break;

        case CpuModel::CPU_EV5:
            config.cacheSize = 8192;  // 8KB I-cache
            config.lineSize = 32;     // 32-byte lines
            config.associativity = 2; // 2-way set associative
            config.autoPrefetchEnabled = false;
            break;

       case CpuModel::CPU_EV56:
            config.cacheSize = 16384; // 16KB I-cache
            config.lineSize = 32;     // 32-byte lines
            config.associativity = 2; // 2-way set associative
            config.autoPrefetchEnabled = true;
            break;

//         case CpuModel::CPU_PCA56:
//             config.cacheSize = 16384; // 16KB I-cache
//             config.lineSize = 64;     // 64-byte lines
//             config.associativity = 2; // 2-way set associative
//             config.autoPrefetchEnabled = true;
//             break;

        case CpuModel::CPU_EV6:
            config.cacheSize = 65536; // 64KB I-cache
            config.lineSize = 64;     // 64-byte lines
            config.associativity = 2; // 2-way set associative
            config.autoPrefetchEnabled = true;
            break;

        case CpuModel::CPU_EV67:
        case CpuModel::CPU_EV68:
            config.cacheSize = 65536; // 64KB I-cache
            config.lineSize = 64;     // 64-byte lines
            config.associativity = 4; // 4-way set associative
            config.autoPrefetchEnabled = true;
            break;

        default:
            // Default to EV56-like configuration
            config.cacheSize = 32768; // 32KB I-cache
            config.lineSize = 64;     // 64-byte lines
            config.associativity = 4; // 4-way set associative
            config.autoPrefetchEnabled = true;
            break;
        }

        qDebug() << QString("Cache config for CPU model %1: %2KB, %3-way, %4B lines")
                        .arg(static_cast<int>(model))
                        .arg(config.cacheSize / 1024)
                        .arg(config.associativity)
                        .arg(config.lineSize);

        return config;
    }
};
