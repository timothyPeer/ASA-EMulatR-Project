#pragma once

/**
 * @brief PAL function classification for optimization
 */
enum class PALFunctionClass
{
    SYSTEM_CALL,         // User->Kernel transitions
    MEMORY_MANAGEMENT,   // TLB, page table operations
    CACHE_CONTROL,       // Cache flush, invalidation
    CONTEXT_SWITCH,      // Process context switching
    INTERRUPT_HANDLING,  // Interrupt/exception processing
    PERFORMANCE_COUNTER, // Performance monitoring
    PRIVILEGE_OPERATION, // IPR access, mode changes
    QUEUE_OPERATION,     // Interlocked queue operations
    UTILITY              // Miscellaneous support functions
};