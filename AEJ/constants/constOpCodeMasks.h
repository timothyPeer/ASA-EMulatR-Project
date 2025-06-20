#pragma once
#include <QtCore>


//==============================================================================
// INSTRUCTION FORMAT FIELD MASKS AND SHIFTS
//==============================================================================

// Primary opcode field (bits 31:26)
constexpr quint32 OPCODE_MASK = 0xFC000000;
constexpr quint32 OPCODE_SHIFT = 26;

// Function code fields for different instruction formats
constexpr quint32 FUNC_11_MASK = 0x000007FF; // 11-bit function (IEEE/VAX FP)
constexpr quint32 FUNC_11_SHIFT = 5;
constexpr quint32 FUNC_16_MASK = 0x0000FFFF; // 16-bit function (MISC)
constexpr quint32 FUNC_16_SHIFT = 0;
constexpr quint32 FUNC_7_MASK = 0x0000007F; // 7-bit function (Integer)
constexpr quint32 FUNC_7_SHIFT = 5;
constexpr quint32 FUNC_2_MASK = 0x00000003; // 2-bit function (Jump)
constexpr quint32 FUNC_2_SHIFT = 14;
constexpr quint32 FUNC_26_MASK = 0x03FFFFFF; // 26-bit function (PAL)
constexpr quint32 FUNC_26_SHIFT = 0;

// Register field masks and shifts
constexpr quint32 RA_MASK = 0x03E00000; // Ra field (bits 25:21)
constexpr quint32 RA_SHIFT = 21;
constexpr quint32 RB_MASK = 0x001F0000; // Rb field (bits 20:16)
constexpr quint32 RB_SHIFT = 16;
constexpr quint32 RC_MASK = 0x0000001F; // Rc field (bits 4:0)
constexpr quint32 RC_SHIFT = 0;

// Literal field mask and shift
constexpr quint32 LIT_MASK = 0x00001FE0; // Literal field (bits 12:5)
constexpr quint32 LIT_SHIFT = 5;

// Displacement field masks and shifts
constexpr quint32 DISP_16_MASK = 0x0000FFFF; // 16-bit displacement
constexpr quint32 DISP_16_SHIFT = 0;
constexpr quint32 DISP_21_MASK = 0x001FFFFF; // 21-bit displacement (branches)
constexpr quint32 DISP_21_SHIFT = 0;

// Hardware interrupt level field
constexpr quint32 HW_INT_MASK = 0x0000001F; // Hardware interrupt level (bits 4:0)
constexpr quint32 HW_INT_SHIFT = 0;


