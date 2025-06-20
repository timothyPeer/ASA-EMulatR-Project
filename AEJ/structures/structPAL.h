#pragma once
#include <QtGlobal>
struct PalEntryPoint
{
    quint64 address;   // Physical address of the PAL handler
    quint64 excSumBit; // Bit in ExcSum register for this exception
    const char *name;  // Name for debugging
};
