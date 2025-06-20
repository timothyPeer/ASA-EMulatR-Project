#pragma once
#include <QtCore>














//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 15.x3C / 15.xBC – Convert Quad -> F-float
// 16.x3C / 16.x7C / 16.xBC / 16.xFC – Convert Quad-> S-float
// 16.x3E / 16.x7E / 16.xBE / 16.xFE – Convert Quad-> T-float
// 16.xAF / 16.x2F  – Convert T-float -> Quadword (CVTTQ family)
// constexpr quint32 FUNC_CMPULT_G = 0x3D;  // Compare unsigned <, quad-word operands (Ra < Rb ? 1 : 0)
// constexpr quint32 FUNC_CVTQF = 0x0BC;
// constexpr quint32 FUNC_CVTQF_C = 0x03C;
// constexpr quint32 FUNC_CVTQS = 0x0BC;
// constexpr quint32 FUNC_CVTQT = 0x0BE;
// constexpr quint32 FUNC_CVTTQ = 0x0AF;
// Integer arithmetic - Longword operations
// Integer arithmetic - Longword with overflow detection
// Integer arithmetic - Quadword operations
// Integer arithmetic - Quadword with overflow detection
// Integer arithmetic - Special operations
// INTEGER ARITHMETIC FUNCTION CODES (Opcode 0x10) - 7-bit function codes
// Integer-Multiply (opcode 0x13) functions
// L = longword (32-bit), G = quadword (64-bit)
// Width-qualified unsigned compare functions for clarity
//==============================================================================
//==============================================================================
constexpr quint32 FUNC_ADDL = 0x00;   // Add Longword
constexpr quint32 FUNC_ADDLV = 0x40;   // Add Longword with overflow
constexpr quint32 FUNC_ADDQ = 0x20;   // Add Quadword
constexpr quint32 FUNC_ADDQV = 0x60;   // Add Quadword with overflow
constexpr quint32 FUNC_CMPBGE = 0x0F; // Compare Byte Mask Greater or Equal
constexpr quint32 FUNC_CMPEQ = 0x2D;  // Compare Equal
constexpr quint32 FUNC_CMPLE = 0x6D; // Compare Less Than or Equal
constexpr quint32 FUNC_CMPLT = 0x4D; // Compare signed <


constexpr quint32 FUNC_CMPULT = 0x1D; // Compare Unsigned Less Than



constexpr quint32 FUNC_UMULH = 0x30; // Unsigned Multiply High Quadword


