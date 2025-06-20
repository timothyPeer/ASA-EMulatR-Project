// const_FPCR_AMASK.h
#pragma once

#include <QtGlobal> // for quint32, quint64

/**
 * @file    const_FPCR_AMASK.h
 * @brief   Architecture mask values for ALPHA AMASK instruction and FPCR control bits
 *
 * Source Reference:
 *   • Alpha AXP Architecture Reference Manual, Version 6 (1994),
 *     Volume 1, Section 9.2 “Floating-Point Control Register (FPCR)” (pp. 9-12 – 9-15)
 *   • Alpha AXP Architecture Reference Manual, Version 6 (1994),
 *     Appendix C, Table C-3 “AMASK Field Values”
 */

namespace AlphaFPCR
{
//
// FPCR Dynamic Rounding Mode (Volume 1, Section 9.2.1; p. 9-12)
//
/// Dynamic rounding mode mask (bits 58–59)
constexpr quint64 FPCR_DYN_ROUND_MASK = 0x0C00000000000000ULL;
/// Dynamic rounding mode shift count
constexpr quint64 FPCR_DYN_ROUND_SHIFT = 58;

//
// FPCR Enable Bits (Volume 1, Section 9.2.2; pp. 9-12 – 9-13)
//
/// Inexact enable (INED)
constexpr quint64 FPCR_INED = 0x0020000000000000ULL;
/// Underflow enable (UNFD)
constexpr quint64 FPCR_UNFD = 0x0010000000000000ULL;
/// Overflow enable (OVFD)
constexpr quint64 FPCR_OVFD = 0x0008000000000000ULL;
/// Divide-by-zero enable (DZED)
constexpr quint64 FPCR_DZED = 0x0004000000000000ULL;
/// Invalid operation enable (INVD)
constexpr quint64 FPCR_INVD = 0x0002000000000000ULL;
/// Summary bit (SUM) – reflects any unmasked exception (Volume 1, Section 9.2.3; p. 9-14)
constexpr quint64 FPCR_SUM = 0x8000000000000000ULL;

//
// FPCR Exception Status Bits (Volume 1, Section 9.2.4; p. 9-14 – 9-15)
//
/// Integer overflow (IOV)
constexpr quint64 FPCR_IOV = 0x0000020000000000ULL;
/// Inexact result (INE)
constexpr quint64 FPCR_INE = 0x0000010000000000ULL;
/// Underflow status (UNF)
constexpr quint64 FPCR_UNF = 0x0000008000000000ULL;
/// Overflow status (OVF)
constexpr quint64 FPCR_OVF = 0x0000004000000000ULL;
/// Divide by zero status (DZE)
constexpr quint64 FPCR_DZE = 0x0000002000000000ULL;
/// Invalid operation status (INV)
constexpr quint64 FPCR_INV = 0x0000001000000000ULL;

//
// AMASK Instruction “Architecture Mask” Field Values (Appendix C, Table C-3)
//
/// Byte/Word extension (BWX)
constexpr quint32 ALPHA_AMASK_BWX = 0x01;
/// Square root and integer-to-fixed-point extension (FIX)
constexpr quint32 ALPHA_AMASK_FIX = 0x02;
/// Count extension (CIX)
constexpr quint32 ALPHA_AMASK_CIX = 0x04;
/// Motion video extension (MVI)
constexpr quint32 ALPHA_AMASK_MVI = 0x08;
/// Multimedia extension (MAX)
constexpr quint32 ALPHA_AMASK_MAX = 0x10;
/// Precise exception reporting (PER)
constexpr quint32 ALPHA_AMASK_PER = 0x20;
} // namespace AlphaFPCR
