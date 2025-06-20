#pragma once
#include "../enumerations/enumExceptionType.h"


/**
 * @brief Map InterruptType to ExceptionType for PAL exception routing.
 */
#pragma once
/**
 * @file trapInterruptMapper.h
 * @brief Maps InterruptType to ExceptionType for Alpha PAL exception dispatch.
 *
 * This file provides a conversion layer so internal interrupt sources (e.g. hardware, software, timer)
 * are converted to architectural ExceptionType enums for use in handleException().
 */

#include "../enumerations/enumExceptionType.h"


/**
 * @brief Maps an interrupt type to its corresponding architectural exception.
 *
 * This mapping ensures that handleException() is dispatched correctly
 * for software/hardware-triggered asynchronous events.
 *
 * @param type The internal InterruptType (source of interrupt)
 * @return ExceptionType for PAL dispatch
 */
inline ExceptionType exceptionTypeFromInterrupt(InterruptType type)
{
    switch (type)
    {
    case InterruptType::SOFTWARE_INTERRUPT:
        return ExceptionType::SOFTWARE_INTERRUPT;

    case InterruptType::HARDWARE:
        return ExceptionType::INTERRUPT;

    case InterruptType::TIMER:
        return ExceptionType::TIMER_INTERRUPT;

    case InterruptType::PERFORMANCE_COUNTER:
        return ExceptionType::PERFORMANCE_MONITOR;

    case InterruptType::POWER_FAIL:
        return ExceptionType::MACHINE_CHECK;

    case InterruptType::PAL:
        return ExceptionType::PAL_CALL;

    default:
        return ExceptionType::INTERRUPT; // Fallback
    }
}
