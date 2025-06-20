#pragma once
#include <QtCore>
#include <QString>
#include "../AEE/TLBExceptionQ.h"
struct ProbeResult
{
    enum class Status
    {
        SUCCESS,              // Address is accessible
        TLB_MISS,             // TLB miss would occur
        PAGE_FAULT,           // Page not present
        PROTECTION_VIOLATION, // Access not permitted
        ALIGNMENT_FAULT,      // Misaligned access
        MMIO_REGION,          // Address maps to MMIO
        INVALID_ADDRESS,      // Address outside valid ranges
        RESERVED_ADDRESS,     // Address in reserved region
        ASN_MISMATCH          // Wrong address space
    };

    Status status = Status::SUCCESS;
    quint64 physicalAddress = 0; // Physical address if translation succeeds
    quint64 faultAddress = 0;    // Address that would cause fault
    excTLBException tlbException = excTLBException::NONE;
    bool isMMIO = false;            // True if address maps to MMIO
    bool requiresPageFault = false; // True if page fault handler needed
    QString description;            // Human-readable description
};
