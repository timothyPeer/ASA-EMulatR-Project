#pragma once
#include <QtCore>
namespace AlphaPS
{
// CORRECT - Alpha PS Register Layout (only 3 bits used for IPL)
constexpr quint64 IPL_MASK = 0x00000007ULL; // Bits 2:0 only
constexpr quint64 IPL_SHIFT = 0;
constexpr quint64 RESERVED_MASK = 0xFFFFFFFFFFFFFFF8ULL; // Bits 63:3 (must be zero)

// IPL Values (these ARE correct)
constexpr quint64 IPL_0 = 0x00000000ULL; // All interrupts enabled
constexpr quint64 IPL_1 = 0x00000001ULL; // Software interrupt 1
constexpr quint64 IPL_2 = 0x00000002ULL; // Software interrupt 2
constexpr quint64 IPL_3 = 0x00000003ULL; // Clock interrupt
constexpr quint64 IPL_4 = 0x00000004ULL; // I/O device interrupt
constexpr quint64 IPL_5 = 0x00000005ULL; // Reserved
constexpr quint64 IPL_6 = 0x00000006ULL; // High priority I/O
constexpr quint64 IPL_7 = 0x00000007ULL; // All interrupts disabled

// Mode is stored SEPARATELY, not in PS register
// (These would go in a separate enum or namespace)
} // namespace AlphaPS