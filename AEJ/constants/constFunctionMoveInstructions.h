#pragma once
#include <QtCore>

//==============================================================================
// CONDITIONAL MOVE FUNCTION CODES SUMMARY
//==============================================================================

// Integer conditional moves (opcode 0x18)
constexpr quint32 CMOV_FUNC_EQ = 0x124;  // If equal to zero
constexpr quint32 CMOV_FUNC_NE = 0x126;  // If not equal to zero
constexpr quint32 CMOV_FUNC_LT = 0x144;  // If less than zero
constexpr quint32 CMOV_FUNC_GE = 0x146;  // If greater than or equal to zero
constexpr quint32 CMOV_FUNC_LE = 0x164;  // If less than or equal to zero
constexpr quint32 CMOV_FUNC_GT = 0x166;  // If greater than zero
constexpr quint32 CMOV_FUNC_LBC = 0x116; // If low bit clear
constexpr quint32 CMOV_FUNC_LBS = 0x114; // If low bit set

// Floating-point conditional moves (opcode 0x17)
constexpr quint32 FCMOV_FUNC_EQ = 0x42A; // If floating equal to zero
constexpr quint32 FCMOV_FUNC_NE = 0x42B; // If floating not equal to zero
constexpr quint32 FCMOV_FUNC_LT = 0x42C; // If floating less than zero
constexpr quint32 FCMOV_FUNC_GE = 0x42D; // If floating greater or equal to zero
constexpr quint32 FCMOV_FUNC_LE = 0x42E; // If floating less than or equal to zero
constexpr quint32 FCMOV_FUNC_GT = 0x42F; // If floating greater than zero

// Integer conditional moves
constexpr quint32 FUNC_CMOVEQ = 0x024;  // Conditional Move if Equal
constexpr quint32 FUNC_CMOVNE = 0x026;  // Conditional Move if Not Equal
constexpr quint32 FUNC_CMOVLT = 0x044;  // Conditional Move if Less Than
constexpr quint32 FUNC_CMOVGE = 0x046;  // Conditional Move if Greater or Equal
constexpr quint32 FUNC_CMOVLE = 0x064;  // Conditional Move if Less or Equal
constexpr quint32 FUNC_CMOVGT = 0x066;  // Conditional Move if Greater Than
constexpr quint32 FUNC_CMOVLBC = 0x016; // Conditional Move if Low Bit Clear
constexpr quint32 FUNC_CMOVLBS = 0x014; // Conditional Move if Low Bit Set