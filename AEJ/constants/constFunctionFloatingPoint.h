#pragma once
#include <QtCore>

/* FLOATING-POINT INSTRUCTION NOTES:
 * - IEEE floating-point operates on 32-bit (S_floating) and 64-bit (T_floating) formats
 * - VAX floating-point supports F_floating (32-bit), G_floating (64-bit), and D_floating (64-bit)
 * - Function codes for FP instructions include rounding mode and exception handling flags 
 */


constexpr quint64 PS_FP_ENABLE = 0x4;

//==============================================================================
// FLOATING-POINT FUNCTION (Opcode 0x17) - 11-bit function codes
//==============================================================================

// Integer/Long integer conversions for floating-point
constexpr quint32 FUNC_CVTLQ = 0x010;   // Convert Longword to Quadword
constexpr quint32 FUNC_CPYS = 0x020;    // Copy Sign
constexpr quint32 FUNC_CPYSN = 0x021;   // Copy Sign Negate
constexpr quint32 FUNC_CPYSE = 0x022;   // Copy Sign and Exponent
constexpr quint32 FUNC_MT_FPCR = 0x024; // Move to Floating-Point Control Register
constexpr quint32 FUNC_MF_FPCR = 0x025; // Move from Floating-Point Control Register
constexpr quint32 FUNC_FCMOVEQ = 0x02A; // Floating Conditional Move if Equal
constexpr quint32 FUNC_FCMOVNE = 0x02B; // Floating Conditional Move if Not Equal
constexpr quint32 FUNC_FCMOVLT = 0x02C; // Floating Conditional Move if Less Than
constexpr quint32 FUNC_FCMOVGE = 0x02D; // Floating Conditional Move if Greater or Equal
constexpr quint32 FUNC_FCMOVLE = 0x02E; // Floating Conditional Move if Less or Equal
constexpr quint32 FUNC_FCMOVGT = 0x02F; // Floating Conditional Move if Greater Than
constexpr quint32 FUNC_CVTQL = 0x030;   // Convert Quadword to Longword
constexpr quint32 FUNC_CVTQLV = 0x130;  // Convert Quadword to Longword with overflow
constexpr quint32 FUNC_CVTQLSV = 0x530; // Convert Quadword to Longword with software completion


/* AlphaFloatingPointExecutor */