#pragma once
#include "../enumerations/enumExceptionType.h"

/**
 * @brief Map AlphaTrapType to the appropriate ExceptionType
 */
inline ExceptionType exceptionTypeFromAlphaTrap(AlphaTrapType trap)
{
    using T = AlphaTrapType;
    switch (trap)
    {
    case T::INTEGER_OVERFLOW:
    case T::STATUS_ALPHA_GENTRAP:
        return ExceptionType::ARITHMETIC_TRAP;

    case T::DZE:
    case T::IOV:
    case T::INE:
    case T::UNF:
    case T::INV:
        return ExceptionType::FP_EXCEPTION;

    case T::STATUS_INVALID_ADDRESS:
        return ExceptionType::ACCESS_VIOLATION;

    case T::STATUS_ILLEGAL_INSTRUCTION:
        return ExceptionType::ILLEGAL_INSTRUCTION;

    case T::MACHINE_CHECK:
        return ExceptionType::MACHINE_CHECK;

    case T::SOFTWARE_INTERRUPT:
    case T::SWC:
        return ExceptionType::SYSTEM_CALL;

    case T::TRAP_CAUSE_UNKNOWN:
    default:
        return ExceptionType::ARITHMETIC;
    }
}
inline ExceptionType exceptionTypeFromAlphaTrap(FPTrapType trap)
{
    using F = FPTrapType;

    switch (trap)
    {
    case F::FP_INEXACT:
    case F::FP_UNDERFLOW:
    case F::FP_OVERFLOW:
    case F::FP_DIVISION_BY_ZERO:
    case F::FP_INVALID_OPERATION:
        return ExceptionType::FP_EXCEPTION;

    case F::FP_ARITHMETIC_TRAP:
        return ExceptionType::ARITHMETIC_TRAP;

    case F::UNALIGNED_ACCESS:
        return ExceptionType::UNALIGNED_ACCESS;

    case F::MACHINE_CHECK:
        return ExceptionType::MACHINE_CHECK;

    case F::PAL_FAULT:
        return ExceptionType::PAL_CALL;

    case F::UNKNOWN:
    default:
        return ExceptionType::ARITHMETIC;
    }
}
