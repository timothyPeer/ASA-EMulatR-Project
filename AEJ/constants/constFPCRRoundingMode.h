#pragma once
#include <QtCore>

//==============================================================================
// FLOATING-POINT ROUNDING MODE CONSTANTS
//==============================================================================

// IEEE rounding modes (FPCR bits)
constexpr quint32 FPCR_ROUND_CHOPPED = 0x00; // Round toward zero
constexpr quint32 FPCR_ROUND_MINUS = 0x01;   // Round toward minus infinity
constexpr quint32 FPCR_ROUND_NORMAL = 0x02;  // Round to nearest (default)
constexpr quint32 FPCR_ROUND_PLUS = 0x03;    // Round toward plus infinity

// IEEE exception enable bits (FPCR)
 constexpr quint32 FPCR_INED = 0x00800000; // Inexact disable
 constexpr quint32 FPCR_UNFD = 0x00400000; // Underflow disable
 constexpr quint32 FPCR_OVFD = 0x00200000; // Overflow disable
 constexpr quint32 FPCR_DZED = 0x00100000; // Divide by zero disable
 constexpr quint32 FPCR_INVD = 0x00080000; // Invalid operation disable