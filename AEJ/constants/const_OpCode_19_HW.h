#pragma once
#include <QtCore>

/*===========================================================================
 *  executeIntegerLogical.H - Alpha AXP Execute Stage, integer groups
 *  Header-only, Qt-style.  Nest opcode -> function switches to avoid collisions
 *  between identically-encoded function fields in different primary opcodes.
 *
 *  ? Primary opcode 0x10 : INT-logical/arithmetic   (AND/BIC/BIS/XOR/EQV/ORNOT)
 *  ? Primary opcode 0x11 : Conditional moves        (CMOVxx group)
 *  ? Primary opcode 0x12 : Extract/Insert/Mask      (MSK/EXT/INS family)
 *
 *  References
 *    • Alpha AXP System Ref. Manual v6, sec. 4.2-4.3  (integer formats)  p.4-7->4-13
 *    • Appendix C-1 & C-2 (opcode/function tables)                   p.C-2->C-6
 *=============================================================================*/


//
// Hardware-Specific Instruction Opcodes
// 

constexpr quint8 OPCODE_HW_MFPR = 0x19; // Move from Processor Register
constexpr quint8 OPCODE_HW_LD = 0x1B;   // Hardware Load
constexpr quint8 OPCODE_HW_MTPR = 0x1C; // Move to Processor Register
constexpr quint8 OPCODE_HW_REI = 0x1D;  // Return from Exception/Interrupt
constexpr quint8 OPCODE_HW_ST = 0x1E;   // Hardware Store
constexpr quint8 OPCODE_HW_ST_C = 0x1F; // Hardware Store Conditional
