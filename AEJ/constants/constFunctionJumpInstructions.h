#pragma once
#include <QtCore>

//==============================================================================
// JUMP OPERATIONS (Opcode 0x1A) - 2-bit function codes
//==============================================================================

constexpr quint32 FUNC_JMP = 0x00;           // Jump
constexpr quint32 FUNC_JSR = 0x01;           // Jump to Subroutine
constexpr quint32 FUNC_RET = 0x02;           // Return from Subroutine
constexpr quint32 FUNC_JSR_COROUTINE = 0x03; // Jump to Subroutine Return