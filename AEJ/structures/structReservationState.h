#pragma once
#include <QtCore>
class AlphaCPU;

 /**
 * @brief Per-CPU reservation state
 */
struct ReservationState
{
    bool isValid = false;        // Is reservation active?
    quint64 physicalAddress = 0; // Physical address of reservation
    quint64 virtualAddress = 0;  // Virtual address (for debugging)
    int size = 0;                // Size of reservation (4 or 8 bytes)
    quint64 timestamp = 0;       // When reservation was made
    AlphaCPU *cpu = nullptr;     // CPU that owns this reservation

    void clear()
    {
        isValid = false;
        physicalAddress = 0;
        virtualAddress = 0;
        size = 0;
        timestamp = 0;
        cpu = nullptr;
    }
    bool matches(quint64 physAddr, int accessSize) const
    {
        if (!isValid)
            return false;

        // Check if addresses overlap
        quint64 reserveStart = physicalAddress;
        quint64 reserveEnd = physicalAddress + size - 1;
        quint64 accessStart = physAddr;
        quint64 accessEnd = physAddr + accessSize - 1;

        return !(accessEnd < reserveStart || accessStart > reserveEnd);
    }
};