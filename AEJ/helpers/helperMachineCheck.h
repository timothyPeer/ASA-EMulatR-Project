#pragma once
#include <QtGlobal>
#include "enumerations/enumMachineCheckType.h"
/**
 * @brief Check if a machine check type is recoverable
 * @param type The machine check type
 * @return true if the error might be recoverable
 */
inline bool isRecoverable(MachineCheckType type)
{
    switch (type)
    {
    // Generally recoverable errors
    case AsaExceptions::MachineCheckType::ICACHE_PARITY_ERROR:
    case AsaExceptions::MachineCheckType::DCACHE_PARITY_ERROR:
    case AsaExceptions::MachineCheckType::THERMAL_ERROR:
        return true;

    // Generally unrecoverable errors
    case AsaExceptions::MachineCheckType::DOUBLE_MACHINE_CHECK:
    case AsaExceptions::MachineCheckType::UNCORRECTABLE_ERROR:
    case AsaExceptions::MachineCheckType::CONTROL_LOGIC_ERROR:
    case AsaExceptions::MachineCheckType::REGISTER_FILE_ERROR:
        return false;

    default:
        return false; // Conservative default
    }
}

/**
 * @brief Get the severity level of a machine check type
 * @param type The machine check type
 * @return Severity level (0=highest, 3=lowest)
 */
inline int getSeverity(MachineCheckType type)
{
    switch (type)
    {
    case MachineCheckType::DOUBLE_MACHINE_CHECK:
    case MachineCheckType::UNCORRECTABLE_ERROR:
        return 0; // Critical

    case MachineCheckType::SYSTEM_MEMORY_ERROR:
    case MachineCheckType::CONTROL_LOGIC_ERROR:
        return 1; // High

    case MachineCheckType::CACHE_COHERENCY_ERROR:
    case MachineCheckType::SYSTEM_BUS_ERROR:
        return 2; // Medium

    case MachineCheckType::THERMAL_ERROR:
    case MachineCheckType::ICACHE_PARITY_ERROR:
        return 3; // Low

    default:
        return 1; // Default to high severity
    }
}

/**
 * @brief Convert machine check type to string for debugging
 * @param type The machine check type
 * @return String representation
 */
inline QString machineCheckTypeToString(MachineCheckType type)
{
    switch (type)
    {
    case MachineCheckType::NONE:
        return "NONE";
    case MachineCheckType::ICACHE_PARITY_ERROR:
        return "ICACHE_PARITY_ERROR";
    case MachineCheckType::DCACHE_PARITY_ERROR:
        return "DCACHE_PARITY_ERROR";
    case MachineCheckType::SCACHE_ERROR:
        return "SCACHE_ERROR";
    case MachineCheckType::BCACHE_ERROR:
        return "BCACHE_ERROR";
    case MachineCheckType::CACHE_TAG_ERROR:
        return "CACHE_TAG_ERROR";
    case MachineCheckType::CACHE_COHERENCY_ERROR:
        return "CACHE_COHERENCY_ERROR";
    case MachineCheckType::SYSTEM_MEMORY_ERROR:
        return "SYSTEM_MEMORY_ERROR";
    case MachineCheckType::MEMORY_CONTROLLER_ERROR:
        return "MEMORY_CONTROLLER_ERROR";
    case MachineCheckType::TRANSLATION_BUFFER_ERROR:
        return "TRANSLATION_BUFFER_ERROR";
    case MachineCheckType::MMU_ERROR:
        return "MMU_ERROR";
    case MachineCheckType::SYSTEM_BUS_ERROR:
        return "SYSTEM_BUS_ERROR";
    case MachineCheckType::IO_BUS_ERROR:
        return "IO_BUS_ERROR";
    case MachineCheckType::EXTERNAL_INTERFACE_ERROR:
        return "EXTERNAL_INTERFACE_ERROR";
    case MachineCheckType::INTERPROCESSOR_ERROR:
        return "INTERPROCESSOR_ERROR";
    case MachineCheckType::EXECUTION_UNIT_ERROR:
        return "EXECUTION_UNIT_ERROR";
    case MachineCheckType::INSTRUCTION_FETCH_ERROR:
        return "INSTRUCTION_FETCH_ERROR";
    case MachineCheckType::REGISTER_FILE_ERROR:
        return "REGISTER_FILE_ERROR";
    case MachineCheckType::CONTROL_LOGIC_ERROR:
        return "CONTROL_LOGIC_ERROR";
    case MachineCheckType::PIPELINE_ERROR:
        return "PIPELINE_ERROR";
    case MachineCheckType::THERMAL_ERROR:
        return "THERMAL_ERROR";
    case MachineCheckType::POWER_SUPPLY_ERROR:
        return "POWER_SUPPLY_ERROR";
    case MachineCheckType::CLOCK_ERROR:
        return "CLOCK_ERROR";
    case MachineCheckType::PALCODE_ERROR:
        return "PALCODE_ERROR";
    case MachineCheckType::SYSTEM_CHIPSET_ERROR:
        return "SYSTEM_CHIPSET_ERROR";
    case MachineCheckType::FIRMWARE_ERROR:
        return "FIRMWARE_ERROR";
    case MachineCheckType::UNCORRECTABLE_ERROR:
        return "UNCORRECTABLE_ERROR";
    case MachineCheckType::MACHINE_CHECK_TIMEOUT:
        return "MACHINE_CHECK_TIMEOUT";
    case MachineCheckType::DOUBLE_MACHINE_CHECK:
        return "DOUBLE_MACHINE_CHECK";
    case MachineCheckType::UNKNOWN_MACHINE_CHECK:
        return "UNKNOWN_MACHINE_CHECK";
    case MachineCheckType::EV4_SPECIFIC_ERROR:
        return "EV4_SPECIFIC_ERROR";
    case MachineCheckType::EV5_SPECIFIC_ERROR:
        return "EV5_SPECIFIC_ERROR";
    case MachineCheckType::EV6_SPECIFIC_ERROR:
        return "EV6_SPECIFIC_ERROR";
    case MachineCheckType::EV7_SPECIFIC_ERROR:
        return "EV7_SPECIFIC_ERROR";
    default:
        return QString("UNKNOWN_TYPE_%1").arg(static_cast<quint16>(type), 4, 16, QChar('0'));
    }
}

/**
 * @brief Check if machine check type is cache-related
 * @param type The machine check type
 * @return true if the error is cache-related
 */
inline bool isCacheRelated(MachineCheckType type)
{
    quint16 code = static_cast<quint16>(type);
    return (code >= 0x0001 && code <= 0x0006);
}

/**
 * @brief Check if machine check type is memory-related
 * @param type The machine check type
 * @return true if the error is memory-related
 */
inline bool isMemoryRelated(MachineCheckType type)
{
    quint16 code = static_cast<quint16>(type);
    return (code >= 0x0010 && code <= 0x0013);
}

/**
 * @brief Check if machine check type is bus-related
 * @param type The machine check type
 * @return true if the error is bus-related
 */
inline bool isBusRelated(MachineCheckType type)
{
    quint16 code = static_cast<quint16>(type);
    return (code >= 0x0020 && code <= 0x0023);
}