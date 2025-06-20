#pragma once

#include <QtCore>



//==============================================================================
// IEEE FLOATING-POINT (Opcode 0x16) - 11-bit function codes
//==============================================================================

// IEEE Single-precision with dynamic rounding
constexpr quint32 FUNC_ADDS_D = 0x040;  // Add S_floating (dynamic)
constexpr quint32 FUNC_SUBS_D = 0x041;  // Subtract S_floating (dynamic)
constexpr quint32 FUNC_MULS_D = 0x042;  // Multiply S_floating (dynamic)
constexpr quint32 FUNC_DIVS_D = 0x043;  // Divide S_floating (dynamic)
constexpr quint32 FUNC_SQRTS_D = 0x0CD; // Square Root S_floating (dynamic)
// constexpr quint32 FUNC_SQRTS_D = 0x0CB; // 14.0CB  SQRTS/D

// ??? ITFP (opcode 0x14) – integer ? floating converts ??????????????
// NOTE: 11-bit function field (bits 10:0)
constexpr quint32 FUNC_ITOFS = 0x000; // Integer ? VAX F_floating
constexpr quint32 FUNC_ITOFF = 0x001; // Integer ? VAX F_floating (round-to-nearest, 64-bit dest)
constexpr quint32 FUNC_ITOFT = 0x002; // Integer ? IEEE T_floating

// IEEE Double-precision (T_floating) operations
constexpr quint32 FUNC_ADDT = 0x0A0;   // Add T_floating
constexpr quint32 FUNC_SUBT = 0x0A1;   // Subtract T_floating
constexpr quint32 FUNC_MULT = 0x0A2;   // Multiply T_floating
constexpr quint32 FUNC_DIVT = 0x0A3;   // Divide T_floating
constexpr quint32 FUNC_CMPTUN = 0x0A4; // Compare T_floating Unordered
constexpr quint32 FUNC_CMPTEQ = 0x0A5; // Compare T_floating Equal
constexpr quint32 FUNC_CMPTLT = 0x0A6; // Compare T_floating Less Than
constexpr quint32 FUNC_CMPTLE = 0x0A7; // Compare T_floating Less Than or Equal
constexpr quint32 FUNC_SQRTT = 0x0AB;  // Square Root T_floating

// IEEE Double-precision with dynamic rounding
constexpr quint32 FUNC_ADDT_D = 0x0E0; // Add T_floating (dynamic)
constexpr quint32 FUNC_SUBT_D = 0x0E1; // Subtract T_floating (dynamic)
constexpr quint32 FUNC_MULT_D = 0x0E2; // Multiply T_floating (dynamic)
constexpr quint32 FUNC_DIVT_D = 0x0E3; // Divide T_floating (dynamic)
// constexpr quint32 FUNC_SQRTT_D = 0x0EB; // Square Root T_floating (dynamic)

// IEEE Compare with software completion
constexpr quint32 FUNC_CMPTUNS = 0x5A4; // Compare T_floating Unordered (signaling)
constexpr quint32 FUNC_CMPTEQS = 0x5A5; // Compare T_floating Equal (signaling)
constexpr quint32 FUNC_CMPTLTS = 0x5A6; // Compare T_floating Less Than (signaling)
constexpr quint32 FUNC_CMPTLES = 0x5A7; // Compare T_floating Less Than or Equal (signaling)

// NON-STANDARD: unsigned Greater-Than / Greater-Or-Equal (project extension)
constexpr quint32 FUNC_CMPUGT = 0x5D; // warning: reserved in real Alpha
constexpr quint32 FUNC_CMPUGE = 0x6F; // (Signed - quadword)

// Format conversions
constexpr quint32 FUNC_CVTQS = 0x0BC;   // Convert Quadword to S_floating
constexpr quint32 FUNC_CVTQT = 0x0BE;   // Convert Quadword to T_floating
constexpr quint32 FUNC_CVTST = 0x2AC;   // Convert S_floating to T_floating
constexpr quint32 FUNC_CVTTS = 0x2BC;   // Convert T_floating to S_floating
constexpr quint32 FUNC_CVTTQ = 0x2BC;   // Convert T_floating to Quadword
constexpr quint32 FUNC_CVTTSC = 0x2CC;  // Convert T_floating to S_floating (chopped)
constexpr quint32 FUNC_CVTTQC = 0x2FC;  // Convert T_floating to Quadword (chopped)
constexpr quint32 FUNC_CVTTQV = 0x1BC;  // Convert T_floating to Quadword with overflow
constexpr quint32 FUNC_CVTTQVC = 0x1FC; // Convert T_floating to Quadword with overflow (chopped)

// --------------------------------
//  FLTI (primary-opcode 0x16) – IEEE S- and T-float arithmetic
//  Rounding-mode encoding in bits 7-6:
//    00 = /C (chopped)      01 = /M (minus-infinity)
//    10 = default (nearest) 11 = /D (plus-infinity)
// -------------------------------
// ----------------- S-floating (low nibble 0x0)
//  ADDS  (add)
constexpr quint32 FUNC_ADDS_C = 0x000; // 16.000  ADDS/C
constexpr quint32 FUNC_ADDS_M = 0x040; // 16.040  ADDS/M
constexpr quint32 FUNC_ADDS = 0x080;   // 16.080  ADDS   (existing)
// constexpr quint32 FUNC_ADDS_D = 0x0C0; // 16.0C0  ADDS/D
//  SUBS  (subtract)
constexpr quint32 FUNC_SUBS_C = 0x001; // 16.001  SUBS/C
constexpr quint32 FUNC_SUBS_M = 0x041; // 16.041  SUBS/M
constexpr quint32 FUNC_SUBS = 0x081;   // 16.081  SUBS   (existing)
// constexpr quint32 FUNC_SUBS_D = 0x0C1; // 16.0C1  SUBS/D
//  MULS  (multiply)
constexpr quint32 FUNC_MULS_C = 0x002; // 16.002  MULS/C
constexpr quint32 FUNC_MULS_M = 0x042; // 16.042  MULS/M
constexpr quint32 FUNC_MULS = 0x082;   // 16.082  MULS   (existing)
// constexpr quint32 FUNC_MULS_D = 0x0C2; // 16.0C2  MULS/D
//  DIVS  (divide)
constexpr quint32 FUNC_DIVS_C = 0x003; // 16.003  DIVS/C
constexpr quint32 FUNC_DIVS_M = 0x043; // 16.043  DIVS/M
constexpr quint32 FUNC_DIVS = 0x083;   // 16.083  DIVS   (existing)
// constexpr quint32 FUNC_DIVS_D = 0x0C3; // 16.0C3  DIVS/D
//  ?? T-floating (low nibble 0x20)
constexpr quint32 FUNC_ADDT_C = 0x020; // 16.020  ADDT/C
constexpr quint32 FUNC_ADDT_M = 0x060; // 16.060  ADDT/M
// constexpr quint32 FUNC_ADDT = 0x0A0;   // 16.0A0  ADDT   (existing)
// constexpr quint32 FUNC_ADDT_D = 0x0E0; // 16.0E0  ADDT/D

constexpr quint32 FUNC_SUBT_C = 0x021; // 16.021  SUBT/C
constexpr quint32 FUNC_SUBT_M = 0x061; // 16.061  SUBT/M
// constexpr quint32 FUNC_SUBT = 0x0A1;   // 16.0A1  SUBT   (existing)
// constexpr quint32 FUNC_SUBT_D = 0x0E1; // 16.0E1  SUBT/D

constexpr quint32 FUNC_MULT_C = 0x022; // 16.022  MULT/C
constexpr quint32 FUNC_MULT_M = 0x062; // 16.062  MULT/M
// constexpr quint32 FUNC_MULT = 0x0A2;   // 16.0A2  MULT   (existing)
// constexpr quint32 FUNC_MULT_D = 0x0E2; // 16.0E2  MULT/D

constexpr quint32 FUNC_DIVT_C = 0x023; // 16.023  DIVT/C
constexpr quint32 FUNC_DIVT_M = 0x063; // 16.063  DIVT/M
// constexpr quint32 FUNC_DIVT = 0x0A3;   // 16.0A3  DIVT   (existing)
// constexpr quint32 FUNC_DIVT_D = 0x0E3; // 16.0E3  DIVT/D
