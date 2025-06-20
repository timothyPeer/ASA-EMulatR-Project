#pragma once
#include <QtCore>

// Operate format instructions (Integer operations)
constexpr quint32 OPCODE_INTA = 0x10; // Integer Arithmetic
constexpr quint32 OPCODE_INTL = 0x11; // Integer Logical
constexpr quint32 OPCODE_INTS = 0x12; // Integer Shift
constexpr quint32 OPCODE_INTM = 0x13; // Integer Multiply
constexpr quint32 OPCODE_ITFP = 0x14; // ITFP edited from Reserved
constexpr quint32 OPCODE_FLTV = 0x15; // VAX Floating Point
constexpr quint32 OPCODE_FLTI = 0x16; // IEEE Floating Point
constexpr quint32 OPCODE_FLTL = 0x17; // FP Function

// Miscellaneous instructions

constexpr quint32 OPCODE_RESERVED19 = 0x19; // Reserved