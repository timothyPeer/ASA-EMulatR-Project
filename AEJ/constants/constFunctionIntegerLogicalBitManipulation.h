#pragma once
#include <QtCore>
//==============================================================================
// INTEGER LOGICAL/BIT MANIPULATION (Opcode 0x11) - 7-bit function codes
//==============================================================================

// Logical operations
constexpr quint32 FUNC_AND = 0x00;   // Logical AND
constexpr quint32 FUNC_BIC = 0x08;   // Bit Clear (AND NOT)
constexpr quint32 FUNC_BIS = 0x20;   // Bit Set (OR)
constexpr quint32 FUNC_EQV = 0x48;   // Equivalence (XOR NOT)
constexpr quint32 FUNC_ORNOT = 0x28; // OR with NOT
constexpr quint32 FUNC_XOR = 0x40;   // Exclusive OR

// Byte Mask/Extract/Insert operations - Low
constexpr quint32 FUNC_MSKBL = 0x02; // Mask Byte Low
constexpr quint32 FUNC_EXTBL = 0x06; // Extract Byte Low
constexpr quint32 FUNC_INSBL = 0x0B; // Insert Byte Low

// Word Mask/Extract/Insert operations - Low
constexpr quint32 FUNC_MSKWL = 0x12; // Mask Word Low
constexpr quint32 FUNC_EXTWL = 0x16; // Extract Word Low
constexpr quint32 FUNC_INSWL = 0x1B; // Insert Word Low

// Longword Mask/Extract/Insert operations - Low
constexpr quint32 FUNC_MSKLL = 0x22; // Mask Longword Low
constexpr quint32 FUNC_EXTLL = 0x26; // Extract Longword Low
constexpr quint32 FUNC_INSLL = 0x2B; // Insert Longword Low

// Quadword Mask/Extract/Insert operations - Low
constexpr quint32 FUNC_MSKQL = 0x32; // Mask Quadword Low
constexpr quint32 FUNC_EXTQL = 0x36; // Extract Quadword Low
constexpr quint32 FUNC_INSQL = 0x3B; // Insert Quadword Low

// Byte Mask/Extract/Insert operations - High
constexpr quint32 FUNC_MSKBH = 0x42; // Mask Byte High
constexpr quint32 FUNC_EXTBH = 0x46; // Extract Byte High
constexpr quint32 FUNC_INSBH = 0x4B; // Insert Byte High

// Word Mask/Extract/Insert operations - High
constexpr quint32 FUNC_MSKWH = 0x52; // Mask Word High
constexpr quint32 FUNC_EXTWH = 0x5A; // Extract Word High
constexpr quint32 FUNC_INSWH = 0x57; // Insert Word High


//------------------------------------------------------------------------------
// Integer-logical (opcode 0x11) – implementation inquiry
//------------------------------------------------------------------------------

// Longword Mask/Extract/Insert operations - High
constexpr quint32 FUNC_MSKLH = 0x62; // Mask Longword High
constexpr quint32 FUNC_EXTLH = 0x6A; // Extract Longword High
constexpr quint32 FUNC_INSLH = 0x6B; // Insert Longword High

// Quadword Mask/Extract/Insert operations - High
constexpr quint32 FUNC_MSKQH = 0x72; // Mask Quadword High
constexpr quint32 FUNC_EXTQH = 0x7A; // Extract Quadword High
constexpr quint32 FUNC_INSQH = 0x77; // Insert Quadword High

//==============================================================================
// INTEGER SHIFT/ZAP (Opcode 0x12) - 7-bit function codes
//==============================================================================

// Shift operations
constexpr quint32 FUNC_SLL = 0x39; // Shift Left Logical
constexpr quint32 FUNC_SRL = 0x34; // Shift Right Logical
constexpr quint32 FUNC_SRA = 0x3C; // Shift Right Arithmetic

// Shift operations with overflow
constexpr quint32 FUNC_SLLV = 0x79; // Shift Left Logical with overflow
constexpr quint32 FUNC_SRLV = 0x74; // Shift Right Logical with overflow
constexpr quint32 FUNC_SRAV = 0x7C; // Shift Right Arithmetic with overflow

// Zero byte operations
constexpr quint32 FUNC_ZAP = 0x30;    // Zero bytes
constexpr quint32 FUNC_ZAPNOT = 0x31; // Zero bytes NOT

//==============================================================================
// INTEGER MULTIPLY (Opcode 0x13) - 7-bit function codes
//==============================================================================

// Longword multiply operations
constexpr quint32 FUNC_MULLV_13 = 0x53; // Multiply Longword with overflow (alt opcode)
constexpr quint32 FUNC_MULQ = 0x20;     // MULQ (Multiply Quadword) instruction
// We will implement both variants with a single constexpr FUNC_MULQ
constexpr quint32 FUNC_MULQV = 0x60; // Multiply Quadword with overflow


/* AlphaIntegerLogicalExecutor */