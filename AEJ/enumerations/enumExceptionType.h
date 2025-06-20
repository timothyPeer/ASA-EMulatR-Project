#pragma once
#include <cstdint>

/**
 * @file enumExceptionType.h
 * @brief Alpha AXP Exception Types
 * 
 * Exception types for Alpha CPU emulation based on Alpha Architecture
 * Reference Manual. Values correspond to exception vector offsets.
 */

enum class PalEntryPoint
{
    // === Core Alpha Exception Types (match hardware) ===
    RESET = 0X0000,            // Code Reset Vector
    MACHINE_CHECK = 0x0004,    // Machine Check Handler
    INTERRUPT = 0x000C,        // External Device or clock interrupt
    UNALIGNED_ACCESS = 0x0010, // Unaligned Memory Access Fault
    SYSTEM_CALL = 0x0014,      // Software syscall vector
    ARITHMETIC_TRAP = 0x0008,  // Arithmetic fault (e.g. FP trap)
    PAL_CALL = 0x2000,         // Pal call base offset
    GENERIC_EXCEPTION = 0x0018

}; 

// === Internal Emulator/Software Exceptions ===
enum class ExceptionType : uint8_t
{
    NONE = 0x00,

    // Hardware-like base types
    MACHINE_CHECK,
    ARITHMETIC,
    MEMORY_MANAGEMENT,
    PAGE_FAULT,
    PRIVILEGE_VIOLATION,
    ILLEGAL_INSTRUCTION,
    BREAKPOINT,
    SYSTEM_CALL,

    // Extended types
    ARITHMETIC_OVERFLOW,
    INTERRUPT_INSTRUCTION,
    ALIGNMENT_FAULT,
    ACCESS_VIOLATION,
    ARITHMETIC_TRAP,
    FP_EXCEPTION,
    ILLEGAL_OPCODE,
    ILLEGAL_OPERAND,
    UNALIGNED_ACCESS = 0x0D, // or suitable value
    SOFTWARE_INTERRUPT, 
    INTERRUPT, 
    TIMER_INTERRUPT,
    PERFORMANCE_MONITOR,
    POWER_FAIL,
    PAL_CALL
};
enum class AlphaTrapType
{
    TRAP_CAUSE_UNKNOWN,
    MACHINE_CHECK,
    SOFTWARE_INTERRUPT,
    KERNEL_STACK_CORRUPTION,
    PANIC_STACK_SWITCH,
    STATUS_ALPHA_GENTRAP,
    STATUS_INVALID_ADDRESS,
    STATUS_ILLEGAL_INSTRUCTION,
    INTEGER_OVERFLOW,

    RAZ,
    INE, // INEXACT_RESULT,
    UNF, // UNDERFLOW,
    IOV, // OVERFLOW,
    DZE, // DIVISION BY ZERO
    INV, // INVALID_OPERATION
    SWC  // SOFTWARE_COMPLETION
};

enum class InterruptType
{
SOFTWARE_INTERRUPT,
HARDWARE,
TIMER,
PERFORMANCE_COUNTER,
POWER_FAIL,
PAL
};