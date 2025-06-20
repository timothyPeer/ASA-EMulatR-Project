#pragma once
#include <QtCore>


// Alpha PS Register Layout - Only 3 bits used for IPL
constexpr quint64 IPL_MASK = 0x00000007ULL; // Bits 2:0
constexpr quint64 IPL_SHIFT = 0;
constexpr quint64 RESERVED_MASK = 0xFFFFFFFFFFFFFFF8ULL; // Bits 63:3 (must be zero)

// IPL Values
constexpr quint64 IPL_0 = 0x00000000ULL; // All interrupts enabled
constexpr quint64 IPL_1 = 0x00000001ULL; // Software interrupt 1
constexpr quint64 IPL_2 = 0x00000002ULL; // Software interrupt 2
constexpr quint64 IPL_3 = 0x00000003ULL; // Clock interrupt
constexpr quint64 IPL_4 = 0x00000004ULL; // I/O device interrupt
constexpr quint64 IPL_5 = 0x00000005ULL; // Reserved
constexpr quint64 IPL_6 = 0x00000006ULL; // High priority I/O
constexpr quint64 IPL_7 = 0x00000007ULL; // All interrupts disabled

// -----------------------------------------------------------------------------
// Bit positions for each condition code flag (zero-based)
// -----------------------------------------------------------------------------
static constexpr quint32 PS_Z_BIT = 0; ///< Zero flag bit index
static constexpr quint32 PS_N_BIT = 1; ///< Negative flag bit index
static constexpr quint32 PS_V_BIT = 2; ///< Overflow flag bit index
static constexpr quint32 PS_C_BIT = 3; ///< Carry flag bit index
static constexpr quint32 PS_ZNVC_MASK = (1u << PS_Z_BIT) | (1u << PS_N_BIT) | (1u << PS_V_BIT) | (1u << PS_C_BIT);



/**
 * @struct ProcessorStatusFlags
 * @brief Stores condition codes (Z, N, V, C, T) after ALU operations.
 *
 * Bitfields are used to reflect processor status:
 * - Zero        (Z): result == 0
 * - Negative    (N): result < 0
 * - Overflow    (V): two's complement overflow
 * - Carry       (C): unsigned carry or borrow
 * - TraceEnable (T): software-controlled (PAL use only)
 */
struct ProcessorStatusFlags
{
    quint8 zero : 1;        ///< Z flag: set if result is zero
    quint8 negative : 1;    ///< N flag: set if result is negative
    quint8 overflow : 1;    ///< V flag: two's-complement overflow
    quint8 carry : 1;       ///< C flag: unsigned carry/borrow
    quint8 traceEnable : 1; ///< T flag: trace enable (handled by PAL)

    /// All flags initialized to zero
    ProcessorStatusFlags() : zero(0), negative(0), overflow(0), carry(0), traceEnable(0) {}
};


