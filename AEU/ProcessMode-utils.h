#pragma once
#ifndef ProcessMode_h__
#define ProcessMode_h__

#include <QString>

/**
 * @brief The ProcessMode enum represents various processor execution modes
 * Used for Alpha processor's privilege levels
 */
enum class ProcessMode {
    KERNEL = 0,     // Most privileged mode (kernel/PAL mode)
    EXECUTIVE = 1,  // Executive mode
    SUPERVISOR = 2, // Supervisor mode
    USER = 3        // Least privileged mode (user mode)
};

/**
 * @brief Utility class for working with ProcessMode enum
 */
class ProcessModeUtils {
public:
    /**
     * @brief Convert ProcessMode enum to string representation
     */
    static QString toString(ProcessMode mode) {
        switch (mode) {
        case ProcessMode::KERNEL:
            return "Kernel";
        case ProcessMode::EXECUTIVE:
            return "Executive";
        case ProcessMode::SUPERVISOR:
            return "Supervisor";
        case ProcessMode::USER:
            return "User";
        default:
            return "Unknown";
        }
    }

    /**
     * @brief Convert int value to ProcessMode enum
     */
    static ProcessMode fromInt(int value) {
        switch (value) {
        case 0:
            return ProcessMode::KERNEL;
        case 1:
            return ProcessMode::EXECUTIVE;
        case 2:
            return ProcessMode::SUPERVISOR;
        case 3:
            return ProcessMode::USER;
        default:
            return ProcessMode::USER; // Default to USER mode for safety
        }
    }

    /**
     * @brief Check if a mode is more privileged than another
     * @return true if mode1 is more privileged than mode2
     */
    static bool isMorePrivileged(ProcessMode mode1, ProcessMode mode2) {
        return static_cast<int>(mode1) < static_cast<int>(mode2);
    }
};

#endif // ProcessMode_h__