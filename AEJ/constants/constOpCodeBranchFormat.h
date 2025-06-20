#pragma once
#include <QtCore>

// Branch format instructions (0x30-0x3F)
constexpr quint32 OPCODE_BR = 0x30;   // Branch
constexpr quint32 OPCODE_FBEQ = 0x31; // Floating Branch if Equal
constexpr quint32 OPCODE_FBLT = 0x32; // Floating Branch if Less Than
constexpr quint32 OPCODE_FBLE = 0x33; // Floating Branch if Less Than or Equal
constexpr quint32 OPCODE_BSR = 0x34;  // Branch to Subroutine
constexpr quint32 OPCODE_FBNE = 0x35; // Floating Branch if Not Equal
constexpr quint32 OPCODE_FBGE = 0x36; // Floating Branch if Greater Than or Equal
constexpr quint32 OPCODE_FBGT = 0x37; // Floating Branch if Greater Than
constexpr quint32 OPCODE_BLBC = 0x38; // Branch if Low Bit Clear
constexpr quint32 OPCODE_BEQ = 0x39;  // Branch if Equal
constexpr quint32 OPCODE_BLT = 0x3A;  // Branch if Less Than
constexpr quint32 OPCODE_BLE = 0x3B;  // Branch if Less Than or Equal
constexpr quint32 OPCODE_BLBS = 0x3C; // Branch if Low Bit Set
constexpr quint32 OPCODE_BNE = 0x3D;  // Branch if Not Equal
constexpr quint32 OPCODE_BGE = 0x3E;  // Branch if Greater Than or Equal
constexpr quint32 OPCODE_BGT = 0x3F;  // Branch if Greater Than
