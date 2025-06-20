#pragma once
/**
 * @file enumProcessorStatus.h
 * @brief Enumerates Alpha AXP Processor Status Register (PS) bits.
 *
 * This enum defines bitmask values for interpreting the 64-bit Processor Status Register.
 * [Source: Alpha System Architecture Manual, Vol I, §4.5.2]
 */

#include <cstdint>

// Bitmask-based flags for the Processor Status (PS) register.
// Use with ProcessorStatus structure and condition checks.
enum enumProcessorStatus : uint64_t
{
    INT_OVERFLOW_ENABLE = 0x0010,          // Bit 4, example
    PS_FLAG_CM = (1ull << 0),              ///< Current Mode: 0=Kernel, 1=User
    PS_FLAG_IPL_MASK = (0x1Full << 1),     ///< Interrupt Priority Level (bits 1–5)
    PS_FLAG_INT_ENABLE = (1ull << 6),      ///< Interrupt Enable (EI=1, DI=0)
    PS_FLAG_FPE = (1ull << 7),             ///< Floating Point Enable
    PS_FLAG_MM_MODE = (0x3ull << 8),       ///< Memory Management Mode (8–9)
    PS_FLAG_INTEGER_OVF_EN = (1ull << 10), ///< Integer Overflow Enable
    PS_FLAG_PRIV_VIOL_EN = (1ull << 11),   ///< Privilege Violation Trap Enable
    PS_FLAG_UNALIGNED_EN = (1ull << 12),   ///< Unaligned Access Enable
    PS_FLAG_ASN_MASK = (0xFFull << 16),    ///< Address Space Number (ASN)
    PS_FLAG_RESCHED_HINT = (1ull << 31),   ///< Software Reschedule Hint
    PS_FLAG_CURR_FPCR_MODE = (1ull << 35), ///< Use alternate FPCR mapping
    PS_FLAG_SPAD_ADDR_MODE = (1ull << 38), ///< Scratchpad Address Mode Enable
    PS_FLAG_SUPERVISOR_MODE = (1ull << 63), ///< Virtual Supervisor Mode (VSM)
    // === ALU Condition Flags (used by V instructions and PAL shadow state) ===
    PS_FLAG_ZERO = (1ull << 40),     ///< Z: Result == 0
    PS_FLAG_NEGATIVE = (1ull << 41), ///< N: Result < 0
    PS_FLAG_OVERFLOW = (1ull << 42), ///< V: Signed overflow
    PS_FLAG_CARRY = (1ull << 43),    ///< C: Unsigned carry/borrow
};

