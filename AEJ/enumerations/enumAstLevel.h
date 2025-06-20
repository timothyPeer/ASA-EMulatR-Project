#pragma once

/**
 * @enum AstLevel
 * @brief AST (Asynchronous System Trap) levels for Alpha processor
 *
 * In Alpha architecture, ASTs are used for asynchronous event handling
 * such as software interrupts, thread scheduling, and process management.
 * Different AST levels correspond to different privilege levels and
 * types of asynchronous operations.
 *
 * ASTs are particularly important for:
 * - Process/thread scheduling
 * - Software interrupts
 * - Asynchronous I/O completion
 * - Timer expiration handling
 * - Inter-process communication
 */
enum class AstLevel : quint8
{
    /**
     * @brief No AST pending
     * Default state when no asynchronous traps are pending
     */
    NONE = 0,

    /**
     * @brief Kernel-level AST
     * Highest priority AST level, used for critical kernel operations
     * - Emergency system calls
     * - Critical resource management
     * - Hardware failure handling
     */
    KERNEL = 1,

    /**
     * @brief Executive-level AST
     * High priority AST for executive/supervisor operations
     * - System service completion
     * - Privileged I/O operations
     * - Memory management operations
     */
    EXECUTIVE = 2,

    /**
     * @brief Supervisor-level AST
     * Medium priority AST for supervisor operations
     * - Process scheduling
     * - Resource allocation
     * - Inter-process synchronization
     */
    SUPERVISOR = 3,

    /**
     * @brief User-level AST
     * Lowest priority AST for user-mode operations
     * - User thread scheduling
     * - User-mode timer expiration
     * - User-mode signal delivery
     * - Asynchronous I/O completion for user processes
     */
    USER = 4,

    // Additional Alpha-specific AST levels

    /**
     * @brief Real-time AST
     * Special AST level for real-time operations
     * - Real-time thread scheduling
     * - High-priority timer events
     * - Critical real-time I/O
     */
    REALTIME = 5,

    /**
     * @brief DPC (Deferred Procedure Call) level
     * For deferred execution of lower-priority operations
     * - Deferred I/O completion
     * - Background maintenance tasks
     * - Non-critical cleanup operations
     */
    DPC = 6,

    /**
     * @brief Software interrupt level
     * For general software interrupts and signals
     * - POSIX signal delivery
     * - Software-generated interrupts
     * - Cross-processor interrupts
     */
    SOFTWARE_INTERRUPT = 7,

    /**
     * @brief Maximum AST level (for bounds checking)
     */
    MAX_LEVEL = SOFTWARE_INTERRUPT
};