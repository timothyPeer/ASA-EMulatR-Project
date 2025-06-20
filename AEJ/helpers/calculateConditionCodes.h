#pragma once
#include <QtCore>
#include "../AEJ/AlphaProcessorStatus.h"
#include "constants/const_conditionCodes.h"

namespace AlphaPS
{

inline ProcessorStatusFlags calculateConditionCodes(qint64 result, qint64 op1, qint64 op2,
                                                    bool isSubtraction /*= false*/
)
{
    ProcessorStatusFlags flags;

    // Z flag: result == 0
    flags.zero = (result == 0);

    // N flag: result < 0
    flags.negative = (result < 0);

    // V flag: two’s-complement overflow
    if (isSubtraction)
    {
        // Sub overflow: operands had opposite signs and result sign differs from op1
        flags.overflow = (((op1 ^ op2) & (op1 ^ result)) < 0);
    }
    else
    {
        // Add overflow: operands same sign and result sign differs
        flags.overflow = (((op1 ^ result) & (op2 ^ result)) < 0);
    }

    // C flag: unsigned carry/borrow
    if (isSubtraction)
    {
        // borrow if op1 < op2 in unsigned
        flags.carry = (static_cast<quint64>(op1) < static_cast<quint64>(op2));
    }
    else
    {
        // carry if result < op1 in unsigned
        flags.carry = (static_cast<quint64>(result) < static_cast<quint64>(op1));
    }

    // T flag (trace enable) left unchanged; set via PAL if needed
    flags.traceEnable = false;

    return flags;
}

} // namespace AlphaPS
