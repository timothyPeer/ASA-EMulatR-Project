#pragma once
// AssemblerExtensions.h
// Extend your Assembler with SSE scalar?FP move/add/sub helpers
// Intel® 64 and IA-32 Architectures Software Developer’s Manual, Vol. 2A:
//   MOVSS (scalar single-precision move), §8.2.1   
//   ADDSS (scalar single-precision add), §8.3.1   
//   SUBSS (scalar single-precision subtract), §8.3.2   
//   MOVSD (scalar double-precision move), §8.2.2   
//   ADDSD (scalar double-precision add), §8.3.4   
//   SUBSD (scalar double-precision subtract), §8.3.3   

#ifndef ASSEMBLER_EXTENSIONS_H
#define ASSEMBLER_EXTENSIONS_H

#include "Assembler.h"
#include "base/assemblerBase.h"

// Encode ModR/M byte for register-to-register SSE: mod=11b, reg=dst, rm=src
inline uint8_t modRm(int dst, int src) {
	return static_cast<uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7));
}

struct Assembler : public AssemblerBase {
	/** MOVSS dst, src  – copy 32-bit float from XMM[src] to XMM[dst] */
	inline void movss(int dst, int src) {
		emitByte(0xF3);
		emitByte(0x0F);
		emitByte(0x10);
		emitByte(modRm(dst, src));
	}

	/** ADDSS dst, src  – dst = dst + src (32-bit float) */
	inline void addss(int dst, int src) {
		emitByte(0xF3);
		emitByte(0x0F);
		emitByte(0x58);
		emitByte(modRm(dst, src));
	}

	/** SUBSS dst, src  – dst = dst ? src (32-bit float) */
	inline void subss(int dst, int src) {
		emitByte(0xF3);
		emitByte(0x0F);
		emitByte(0x5C);
		emitByte(modRm(dst, src));
	}

	/** MOVSD dst, src  – copy 64-bit float from XMM[src] to XMM[dst] */
	inline void movsd(int dst, int src) {
		emitByte(0xF2);
		emitByte(0x0F);
		emitByte(0x10);
		emitByte(modRm(dst, src));
	}

	/** ADDSD dst, src  – dst = dst + src (64-bit float) */
	inline void addsd(int dst, int src) {
		emitByte(0xF2);
		emitByte(0x0F);
		emitByte(0x58);
		emitByte(modRm(dst, src));
	}

	/** SUBSD dst, src  – dst = dst ? src (64-bit float) */
	inline void subsd(int dst, int src) {
		emitByte(0xF2);
		emitByte(0x0F);
		emitByte(0x5C);
		emitByte(modRm(dst, src));
	}
};

#endif // ASSEMBLER_EXTENSIONS_H

