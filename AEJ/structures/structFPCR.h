#pragma once
#include <QtCore>

// Floating-Point Control Register structure
struct FPCR
{
    union
    {
        quint64 raw;
        struct
        {
            quint64 trap_enable_inexact : 1;   // bit 0
            quint64 trap_enable_underflow : 1; // bit 1
            quint64 trap_enable_overflow : 1;  // bit 2
            quint64 trap_enable_div_zero : 1;  // bit 3
            quint64 trap_enable_invalid : 1;   // bit 4
            quint64 reserved1 : 3;             // bits 5-7
            quint64 inexact_result : 1;        // bit 8
            quint64 underflow_result : 1;      // bit 9
            quint64 overflow_result : 1;       // bit 10
            quint64 div_zero_result : 1;       // bit 11
            quint64 invalid_result : 1;        // bit 12
            quint64 reserved2 : 3;             // bits 13-15
            quint64 rounding_mode : 2;         // bits 16-17
            quint64 reserved3 : 46;            // bits 18-63
        } fields;
    };

    FPCR() : raw(0) {}
    explicit FPCR(quint64 value) : raw(value) {}
};
