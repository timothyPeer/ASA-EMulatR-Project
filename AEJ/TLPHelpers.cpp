#include "TLPHelpers.h"

// AlphaCPU.cpp or TLBHelpers.cpp - Implementation of missing TLB exception mapping

#include "JITFaultInfoStructures.h"
#include "TLBSystem.h"
#include "GlobalMacro.h"
#include "MemoryAccessException.h"
#include "enumerations/enumTLBException.h"
#include "enumerations/enumMemoryFaultType.h"

/**
 * @brief Map TLB exceptions to memory fault types
 *
 * This function converts TLB-specific exception types into the generic
 * MemoryFaultType enumeration used throughout the system.
 *
 * @param exception TLB exception type
 * @return Corresponding memory fault type
 */
MemoryFaultType MapTLBExceptionToMemoryFaultType(excTLBException exception)
{
    switch (exception)
    {
    case excTLBException::NONE:
        return MemoryFaultType::NONE;

    case excTLBException::PAGE_FAULT:
    case excTLBException::TRANSLATION_NOT_VALID:
        // Page not present in TLB or page table
        return MemoryFaultType::PAGE_FAULT;

    case excTLBException::ACCESS_VIOLATION:
    case excTLBException::PRIVILEGE_VIOLATION:
        // Access denied due to protection bits or privilege level
        return MemoryFaultType::ACCESS_VIOLATION;

    case excTLBException::ALIGNMENT_FAULT:
        // Address not properly aligned for access size
        return MemoryFaultType::ALIGNMENT_FAULT;

    case excTLBException::PROTECTION_VIOLATION:
        // Page protection violation (e.g., write to read-only page)
        return MemoryFaultType::PROTECTION_VIOLATION;

    case excTLBException::WRITE_PROTECTION_FAULT:
        // Attempted write to write-protected page
        return MemoryFaultType::PROTECTION_VIOLATION;

    case excTLBException::EXECUTE_PROTECTION_FAULT:
        // Attempted execution of non-executable page
        return MemoryFaultType::EXECUTION_FAULT;

    case excTLBException::INVALID_ADDRESS:
        // Address outside valid virtual address space
        return MemoryFaultType::INVALID_ADDRESS;

    case excTLBException::TLB_MISS:
        // TLB miss - needs page table walk
        return MemoryFaultType::PAGE_FAULT;

    case excTLBException::DOUBLE_FAULT:
        // Exception during exception handling
        return MemoryFaultType::DOUBLE_FAULT;

    case excTLBException::MACHINE_CHECK:
        // Hardware error
        return MemoryFaultType::MACHINE_CHECK;

    default:
        // Unknown or unsupported exception type
        ERROR_LOG(QString("Unknown TLB exception type: %1").arg(static_cast<int>(exception)));
        return MemoryFaultType::GENERAL_PROTECTION_FAULT;
    }
}

// Alternative implementation if your excTLBException types are different:
#ifdef ALTERNATIVE_TLB_EXCEPTION_NAMES
MemoryFaultType MapTLBExceptionToMemoryFaultType(excTLBException exception)
{
    switch (exception)
    {
    case excTLBException::NONE:
        return MemoryFaultType::NONE;

    case excTLBException::PAGE_NOT_PRESENT:
    case excTLBException::TLB_MISS:
        return MemoryFaultType::PAGE_FAULT;

    case excTLBException::READ_PROTECTION_VIOLATION:
    case excTLBException::WRITE_PROTECTION_VIOLATION:
    case excTLBException::EXECUTE_PROTECTION_VIOLATION:
        return MemoryFaultType::PROTECTION_VIOLATION;

    case excTLBException::ACCESS_CONTROL_VIOLATION:
        return MemoryFaultType::ACCESS_VIOLATION;

    case excTLBException::DATA_ALIGNMENT_FAULT:
        return MemoryFaultType::ALIGNMENT_FAULT;

    default:
        return MemoryFaultType::GENERAL_PROTECTION_FAULT;
    }
}
#endif



