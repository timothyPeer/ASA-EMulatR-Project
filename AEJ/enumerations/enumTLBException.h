#pragma once
enum class excTLBException
{
    NONE = 0,                 // No exception
    ACCESS_CONTROL_VIOLATION, // ACL violation
    ACCESS_VIOLATION,         // Access denied
    ALIGNMENT_FAULT,          // Improper alignment
    DOUBLE_FAULT,             // Exception during exception
    EXECUTE_PROTECTION_FAULT, // Execute non-executable page
    INVALID_ADDRESS,          // Invalid virtual address
    INVALID_ENTRY,            // No valid translation found
    MACHINE_CHECK,            // Hardware error
    MEMORY_MANAGEMENT,      // General memory access fault
    PAGE_FAULT,               // Page not present
    PRIVILEGE_VIOLATION,      // Privilege level violation
    PROTECTION_FAULT,         // Access rights violation
    PROTECTION_VIOLATION,     // Page protection violation
    TLB_MISS,                 // TLB miss
    TRANSLATION_NOT_VALID,    // Invalid translation
    WRITE_PROTECTION_FAULT    // Write to read-only page

};
