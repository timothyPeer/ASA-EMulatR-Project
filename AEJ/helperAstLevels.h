#pragma once
#include "enumerations\enumMachineCheckType.h"
#include <QString>
#include <enumerations/enumAstLevel.h>
#include <enumerations/enumProcessorMode.h>

/**
 * @brief Helper functions for AST level management
 */


/**
 * @brief Convert AST level to processor mode
 * @param level The AST level
 * @return Corresponding processor mode
 */
inline enumProcessorMode astLevelToProcessorMode(AstLevel level)
{
    switch (level)
    {
    case AstLevel::KERNEL:
    case AstLevel::REALTIME:
        return enumProcessorMode::KERNEL;
    case AstLevel::EXECUTIVE:
        return enumProcessorMode::EXECUTIVE;
    case AstLevel::SUPERVISOR:
    case AstLevel::DPC:
        return enumProcessorMode::SUPERVISOR;
    case AstLevel::USER:
    case AstLevel::SOFTWARE_INTERRUPT:
        return enumProcessorMode::USER;
    default:
        return enumProcessorMode::KERNEL; // Default to most privileged
    }
}

/**
 * @brief Check if AST level requires privilege escalation
 * @param currentMode Current processor mode
 * @param astLevel Requested AST level
 * @return true if privilege escalation is needed
 */
inline bool requiresPrivilegeEscalation(enumProcessorMode currentMode, AstLevel astLevel)
{
    enumProcessorMode requiredMode = astLevelToProcessorMode(astLevel);
    return static_cast<int>(currentMode) > static_cast<int>(requiredMode);
}

/**
 * @brief Get priority of AST level (lower number = higher priority)
 * @param level The AST level
 * @return Priority value (0 = highest priority)
 */

inline int getAstPriority(AstLevel level)
{
    switch (level)
    {
    case AstLevel::KERNEL:
        return 0;
    case AstLevel::REALTIME:
        return 1;
    case AstLevel::EXECUTIVE:
        return 2;
    case AstLevel::SUPERVISOR:
        return 3;
    case AstLevel::DPC:
        return 4;
    case AstLevel::SOFTWARE_INTERRUPT:
        return 5;
    case AstLevel::USER:
        return 6;
    case AstLevel::NONE:
        return 7;
    default:
        return 7;
    }
}

/**
 * @brief Check if one AST level has higher priority than another
 * @param level1 First AST level
 * @param level2 Second AST level
 * @return true if level1 has higher priority than level2
 */
inline bool hasHigherPriority(AstLevel level1, AstLevel level2)
{
    return getAstPriority(level1) < getAstPriority(level2);
}

/**
 * @brief Convert AST level to string for debugging
 * @param level The AST level
 * @return String representation
 */
inline QString astLevelToString(AstLevel level)
{
    switch (level)
    {
    case AstLevel::NONE:
        return "NONE";
    case AstLevel::KERNEL:
        return "KERNEL";
    case AstLevel::EXECUTIVE:
        return "EXECUTIVE";
    case AstLevel::SUPERVISOR:
        return "SUPERVISOR";
    case AstLevel::USER:
        return "USER";
    case AstLevel::REALTIME:
        return "REALTIME";
    case AstLevel::DPC:
        return "DPC";
    case AstLevel::SOFTWARE_INTERRUPT:
        return "SOFTWARE_INTERRUPT";
    default:
        return "UNKNOWN";
    }
}


