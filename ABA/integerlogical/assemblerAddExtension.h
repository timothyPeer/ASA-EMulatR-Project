#pragma once
// AssemblerAddExtensions.h
// Helper functions for integer ADD instructions in your JIT Assembler.
//   ADD r/m32, r32 – opcode 01 /r – Intel® SDM Vol.2A, “ADD r/m32, r32” :contentReference[oaicite:0]{index=0}
//   ADD r/m64, r64 – REX.W + 01 /r – Intel® SDM Vol.2A, “ADD r/m64, r64” :contentReference[oaicite:1]{index=1}

#ifndef ASSEMBLER_ADD_EXTENSIONS_H
#define ASSEMBLER_ADD_EXTENSIONS_H

#include "../base/AssemblerBase.h"

namespace assemblerSpace {
	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	/// Build a REX prefix byte (64-bit mode):
	///   0100WRXB, where W=1 for 64-bit operand size,
	///   R=1 if srcReg?8, B=1 if dstReg?8
	/// See Intel® SDM, “REX Prefix” :contentReference[oaicite:3]{index=3}
	inline uint8_t rexByte(bool w, int srcReg, int dstReg) {
		return static_cast<uint8_t>(
			0x40 |
			(w ? 0x08 : 0) |
			((srcReg & 0x8) ? 0x04 : 0) |
			((dstReg & 0x8) ? 0x01 : 0)
			);
	}

	/// Emit a 32-bit longword ADD: ADD r/m32, r32
	///   opcode 0x01, ModR/M with reg=src, rm=dst
	/// If either reg index ?8, emit REX prefix with W=0.
	/// :contentReference[oaicite:4]{index=4}
	inline void addl(int dstReg, int srcReg) {
		bool needRex = ((dstReg | srcReg) & 0x8) != 0;
		if (needRex) {
			emitByte(rexByte(false, srcReg, dstReg));
		}
		emitByte(0x01);
		emitByte(modRmGp(dstReg, srcReg));
	}

	/// Emit a 64-bit quadword ADD: ADD r/m64, r64
	///   REX.W=1 + opcode 0x01, ModR/M with reg=src, rm=dst
	/// Always emits REX.W for 64-bit operation.
	/// :contentReference[oaicite:5]{index=5}
	inline void addq(int dstReg, int srcReg) {
		emitByte(rexByte(true, srcReg, dstReg));
		emitByte(0x01);
		emitByte(modRmGp(dstReg, srcReg));
	}
}
#endif // ASSEMBLER_ADD_EXTENSIONS_H
