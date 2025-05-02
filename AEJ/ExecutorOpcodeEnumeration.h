// VectorOpcodes.h
#pragma once
#include <cstdint>

enum class VectorOpcode : uint8_t {
	// === Load/Store and Sign-Extension ===

	OP_VLD = 0,       ///< Load 64-bit value into vector register (lane 0)
	OP_LDBU,          ///< Load unsigned byte and zero-extend to 64-bit
	OP_LDWU,          ///< Load unsigned word (16-bit) and zero-extend
	OP_STB,           ///< Store lower 8 bits (byte) from vector register
	OP_STW,           ///< Store lower 16 bits (word) from vector register
	OP_SEXTW,         ///< Sign-extend 16-bit value in lane 0 to 64-bit
	OP_SEXTBU,        ///< Sign-extend 8-bit value in lane 0 to 64-bit

	// === Core ALU ===

	OP_VADD = 10,     ///< Vector integer addition (lane-wise)
	OP_VSUB,          ///< Vector subtraction (lane-wise)
	OP_VAND,          ///< Vector bitwise AND (lane-wise)
	OP_VOR,           ///< Vector bitwise OR (lane-wise)
	OP_VXOR,          ///< Vector bitwise XOR (lane-wise)
	OP_VMUL,          ///< Vector multiply (lane-wise, lower 64-bit product)

	// === Multimedia MAX/MIN Extensions ===

	OP_MAXSB8 = 20,   ///< Maximum of signed 8-bit values (8 per 64-bit lane)
	OP_MINUB8,        ///< Minimum of unsigned 8-bit values (8 per 64-bit lane)
	OP_MINSB8,        ///< Minimum of signed 8-bit values (8 per lane)
	OP_MAXUB8,        ///< Maximum of unsigned 8-bit values (8 per lane)
	OP_MINUW4,        ///< Minimum of unsigned 16-bit values (4 per lane)
	OP_MAXSW4,        ///< Maximum of signed 16-bit values (4 per lane)

	// === Packing and Unpacking ===

	OP_PKLB = 30,     ///< Pack low bytes from 64-bit lanes into lower half
	OP_PKWB,          ///< Pack low words (16-bit) from lanes into lower half
	OP_UNPKBL,        ///< Unpack bytes into longwords (sign-extended)
	OP_UNPKBW,        ///< Unpack bytes into words (zero-extended)
	OP_PERR,          ///< Parallel error detection (multimedia diagnostic instruction)

	OP_COUNT          ///< Total number of vector opcode entries
};
// IntegerOpcodes.h


enum class IntegerOpcode : uint8_t {
	// — Arithmetic
	OP_ADDL = 0x00,    // execADDL
	OP_ADDQ = 0X20,		// execADDQ
	OP_SUB = 0x21,
	OP_MUL = 0x30,
	OP_DIV = 0xE0,    // custom fallback, or PAL dispatch
	OP_MOD = 0xE1,    // custom
	OP_NOT = 0xE2,    // simulate with XOR ~0

	// — Logical
	OP_AND = 0x08,        // execAND
	OP_OR = 0x0A,         // execOR
	OP_XOR = 0x0B,        // execXOR


	// — Shifts
	OP_SLL = 0x39,        // logical left
	OP_SRL = 0x34,        // logical right
	OP_SRA = 0x3C,        // arithmetic right

	// — Memory (byte / word)
	OP_LDB,        // execLDB (sign-ext byte)
	OP_LDBU,       // execLDBU
	OP_LDW,        // execLDW (sign-ext word)
	OP_LDWU,       // execLDWU
	OP_STB,        // execSTB
	OP_STW,        // execSTW

	// — Constants / comparisons
	OP_CMP_EQ = 0x2D,     // execCMPEQ
	OP_CMP_LT = 0x4D,     // execCMPLT
	OP_CMP_LE = 0x6D,     // execCMPLE

	OP_INT_COUNT   // number of integer opcodes
};

enum class AlphaFPOpcode : quint8 {
	ADDF = 0x00,
	SUBF = 0x01,
	MULF = 0x02,
	DIVF = 0x03,
	CVTQS = 0x06,
	CVTTQ = 0x07,
	CPYS = 0x1E,
	CPYSN = 0x1F,
	CPYSE = 0x20,
	FCMOVEQ = 0x23,
	FCMOVNE = 0x24,
	FCMOVLT = 0x25,
	FCMOVLE = 0x26,
	FCMOVGT = 0x27,
	FCMOVGE = 0x28,
	MT_FPCR = 0x2C,
	MF_FPCR = 0x2D,
	OP_FP_COUNT   // number of integer opcodes
};

enum class AlphaLogicalOpcode
{
	AND = 0x00,
	BIC = 0x08,
	BIS = 0x20,
	XOR = 0x40,
	EQV = 0x48,
	OP_LO_COUNT =5
};

/**
 * @brief Enumeration of Alpha AXP Control Flow Primary Opcodes
 * Reference: Alpha Architecture Handbook, Vol. I, §4.3.2
 */
enum class AlphaControlOpcode : uint8_t {
	// Unconditional Branches
	OP_CTRL_BR = 0x30,  ///< Branch (BR)
	OP_CTRL_BSR = 0x34,  ///< Branch to Subroutine (BSR)

	// Conditional Branches (integer test on RA)
	OP_CTRL_BEQ = 0x39,  ///< Branch if Equal (RA == 0)
	OP_CTRL_BNE = 0x3D,  ///< Branch if Not Equal
	OP_CTRL_BLT = 0x3A,  ///< Branch if Less Than
	OP_CTRL_BLE = 0x3B,  ///< Branch if Less or Equal
	OP_CTRL_BGT = 0x3F,  ///< Branch if Greater Than
	OP_CTRL_BGE = 0x3E,  ///< Branch if Greater or Equal

	// Conditional Branches (bit test on RA)
	OP_CTRL_BLBC = 0x38,  ///< Branch if Low Bit Clear
	OP_CTRL_BLBS = 0x3C,  ///< Branch if Low Bit Set

	// Trap Return / PAL Instruction
	OP_CTRL_REI = 0x1F,   ///< Return from Exception or Interrupt (REI)
	OP_CTRL_COUNT
};
