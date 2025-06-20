#pragma once
#include <QtCore>

//------------------------------------------------------------------------------
// Memory-format primary opcodes (load/store floating) – group 0x20
//------------------------------------------------------------------------------
constexpr quint32 OPCODE_LDF = 0x20;
constexpr quint32 OPCODE_LDG = 0x21;
constexpr quint32 OPCODE_LDS = 0x22;
constexpr quint32 OPCODE_LDT = 0x23;
constexpr quint32 OPCODE_STF = 0x24;
constexpr quint32 OPCODE_STG = 0x25;
constexpr quint32 OPCODE_STS = 0x26;
constexpr quint32 OPCODE_STT = 0x27;
constexpr quint32 OPCODE_JSR = 0x1A;        // Jump/Branch
constexpr quint32 OPCODE_RESERVED1B = 0x1B; // Reserved
constexpr quint32 OPCODE_RESERVED1C = 0x1C; // Reserved
constexpr quint32 OPCODE_RESERVED1D = 0x1D; // Reserved
constexpr quint32 OPCODE_RESERVED1E = 0x1E; // Reserved
constexpr quint32 OPCODE_RESERVED1F = 0x1F; // Reserved

// Memory format instructions (Load/Store with displacement)
constexpr quint32 OPCODE_LDL = 0x28;   // Load Longword
constexpr quint32 OPCODE_LDQ = 0x29;   // Load Quadword
constexpr quint32 OPCODE_LDL_L = 0x2A; // Load Longword Locked
constexpr quint32 OPCODE_LDQ_L = 0x2B; // Load Quadword Locked
constexpr quint32 OPCODE_STL = 0x2C;   // Store Longword
constexpr quint32 OPCODE_STQ = 0x2D;   // Store Quadword
constexpr quint32 OPCODE_STL_C = 0x2E; // Store Longword Conditional
constexpr quint32 OPCODE_STQ_C = 0x2F; // Store Quadword Conditional

// Memory format instructions (Load/Store operations)
constexpr quint32 OPCODE_PAL = 0x00;        // Reserved to PALcode
constexpr quint32 OPCODE_RESERVED01 = 0x01; // Reserved for implementation
constexpr quint32 OPCODE_RESERVED02 = 0x02; // Reserved for implementation
constexpr quint32 OPCODE_RESERVED03 = 0x03; // Reserved for implementation
constexpr quint32 OPCODE_RESERVED04 = 0x04; // Reserved for implementation
constexpr quint32 OPCODE_RESERVED05 = 0x05; // Reserved for implementation
constexpr quint32 OPCODE_RESERVED06 = 0x06; // Reserved for implementation
constexpr quint32 OPCODE_RESERVED07 = 0x07; // Reserved for implementation

constexpr quint32 OPCODE_LDA = 0x08;   // Load Address
constexpr quint32 OPCODE_LDAH = 0x09;  // Load Address High
constexpr quint32 OPCODE_LDBU = 0x0A;  // Load Byte Unsigned
constexpr quint32 OPCODE_LDQ_U = 0x0B; // Load Quadword Unaligned
constexpr quint32 OPCODE_LDWU = 0x0C;  // Load Word Unsigned
constexpr quint32 OPCODE_STW = 0x0D;   // Store Word
constexpr quint32 OPCODE_STB = 0x0E;   // Store Byte
constexpr quint32 OPCODE_STQ_U = 0x0F; // Store Quadword Unaligned
