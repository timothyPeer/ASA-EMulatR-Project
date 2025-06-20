#pragma once
#include <QObject>

// Lock reservation tracking for LL/SC operations
struct LockReservation
{
    quint64 virtualAddress = 0;
    quint64 physicalAddress = 0;
    quint64 asn = 0;
    bool valid = false;
    int cpuId = 0; // Which CPU holds this reservation
};
