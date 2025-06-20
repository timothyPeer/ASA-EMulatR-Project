#pragma once
#include "Assembler.h"

namespace assemblerSpace {

	class assmIntegerLogical : public Assembler
	{
		assmIntegerLogical() = default;
		~assmIntegerLogical() = default;

		// Integer ADD 

	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
		inline uint8_t modRmGp(int dst, int src) {
			return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
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
			// 1) emit REX.W=1 plus any register?extension bits  
			emitRex(/*W=*/true, /*reg=*/srcReg, /*rm=*/dstReg);
			// 2) opcode for ADD r/m64, r64  
			emitByte(0x01);
			emitByte(modRmGp(dstReg, srcReg));
		}

	};

}