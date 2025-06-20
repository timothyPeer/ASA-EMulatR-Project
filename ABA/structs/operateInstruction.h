#pragma once
// OperateInstruction.h
// Header for decoding and executing Alpha AXP Operate?format instructions in JitLoadExecutor
// All functions are inline for maximum performance.
// References:
//   Operate Instruction Format (I), Section 3.3.3, p. 3-11 :contentReference[oaicite:0]{index=0}
//   Integer Arithmetic Instructions, Section 4.4, pp. 4-23–4-28 :contentReference[oaicite:1]{index=1}
//   Logical and Shift Instructions, Section 4.5, pp. 4-37–4-40 :contentReference[oaicite:2]{index=2}


#include <cstdint>
#include "../ABA/structs/Instruction.h"
#include "../../AEJ/AlphaProcessorContext.h"

namespace Arch {
	// Representation of a 32-bit Operate instruction word
	struct OperateInstruction : public Arch::Instruction {
		uint32_t raw;          ///< raw instruction bits
		uint8_t  opcode;       ///< bits <31:26>
		uint8_t  ra;           ///< bits <25:21>
		uint8_t  rb;           ///< bits <20:16> or literal hi bits
		uint16_t fnc;          ///< bits <12:5> (extended opcode)
		uint8_t  rc;           ///< bits <4:0>
		bool     isLiteral;    ///< inst<12> == 1 indicates #literal
		uint8_t width;     // M-format: field width in bits (8,16,32,64)
		uint8_t pos;       // M-format: starting bit position (0..63)

	public:

		inline uint8_t srcA() const { return ra; }
		inline uint8_t srcB() const { return rb; }
		inline uint8_t dest() const { return rc; }

		// Decode fields from raw instruction
		inline void decode(uint32_t inst) {
			// — F-format operate instruction —
			// [31:26]=opcode, [25:21]=ra, [20:16]=rb or literal,
			// [12]=literal-flag, [11:5]=fnc, [4:0]=rc       :contentReference[oaicite:1]{index=1}
			ra = (inst >> 21) & 0x1F;
			if ((inst & 0x00001000) != 0) {
				// literal in Rb field
				rb = (inst >> 13) & 0xFF;
				isLiteral = true;
			}
			else {
				rb = (inst >> 16) & 0x1F;
				isLiteral = false;
			}
			fnc = (inst >> 5) & 0x3F;
			rc = inst & 0x1F;
			// width/pos remain unmodified (or zero)
		}
		FormatID format() const override { return FormatID::ALPHA_OPERATE; }
		uint16_t getCode() const override { return opcode; }


		/** [10.00] Load Address High (compute high-order bits) */
		static void emitAlpha_ADDL(const OperateInstruction& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// 1) read the full 64-bit values
			int64_t a = regs->readIntReg(i.srcA());
			int64_t b = regs->readIntReg(i.srcB());

			// 2) add, truncate to 32 bits
			uint32_t low = static_cast<uint32_t>(a + b);

			// 3) sign-extend the 32-bit result back to 64 bits
			int64_t se = static_cast<int32_t>(low);

			// 4) write that 64-bit signed value into the destination register
			regs->writeIntReg(i.dest(), static_cast<uint64_t>(se));

			// update integer condition codes (N, Z, V, C)
			ctx->updateConditionCodes(static_cast<int64_t>(se),
				static_cast<int64_t>(a),
				static_cast<int64_t>(b),
				/*isSubtraction=*/false);
			   // advance PC to next instruction
			ctx->advancePC();
		}


		/** [10.20] Add longword with 8-bit immediate, */
		static void emitAlpha_ADDQ(const OperateInstruction& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// 1) fetch
			uint64_t a = regs->readIntReg(i.srcA());
			uint64_t b = regs->readIntReg(i.srcB());
			// 2) add (wraps on 64 bits)
			uint64_t r = a + b;
			// 3) store
			regs->writeIntReg(i.dest(), r);
			// 4) update PS_N, PS_Z, PS_V, PS_C
			//    last arg = false ? addition
			ctx->updateConditionCodes(static_cast<int64_t>(r),
				static_cast<int64_t>(a),
				static_cast<int64_t>(b),
				/*isSubtraction=*/ false);
			    // advance PC to next instruction
				ctx->advancePC();
	
		}



		/** [10.22] Add quadword (64-bit addition, signed) */
		static inline void emitAlpha_S4ADDQ(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// 1) Read operands
			int64_t a = int64_t(regs->readIntReg(inst.ra));
			int64_t b = inst.isLiteral
				? int8_t(inst.rb)    // sign-extend 8-bit literal
				: int64_t(regs->readIntReg(inst.rb));
			// 2) Compute result just once
			int64_t  r = a + b;
			regs->writeIntReg(inst.rc, uint64_t(r));
			// 3) Update condition codes: N, Z, V, C
			//    - first arg is the signed result
			//    - next two are the signed operands
			//    - isSubtraction = false for an add
			ctx->updateConditionCodes(
				r,
				a,
				b,
				/*isSubtraction=*/false
			);
			// 4) Advance PC by one instruction
			ctx->advancePC();  // assuming advancePC() is defined to step by 4
			ctx->advancePC();
		}


		/** [10.32] Add quadword (64-bit addition, unsigned) */
		static inline void emitAlpha_S8ADDQ(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// identical to ADDQ on Alpha (no overflow traps here)
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);

			int64_t  r = a + b;
			//regs->writeIntReg(inst.rc, uint64_t(r));
			// 3) Update condition codes: N, Z, V, C
			//    - first arg is the signed result
			//    - next two are the signed operands
			//    - isSubtraction = false for an add
			ctx->updateConditionCodes(
				r,
				a,
				b,
				/*isSubtraction=*/false
			);
			// 4) Advance PC by one instruction
			ctx->advancePC();  // assuming advancePC() is defined to step by 4
			regs->writeIntReg(inst.rc, r);
			ctx->advancePC();
		}

		/** [10.0B] Subtract signed 32-bit */
		static inline void emitAlpha_S4SUBL(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			int32_t a = int32_t(regs->readIntReg(inst.ra));
			int32_t b = inst.isLiteral
				? int32_t(int8_t(inst.rb))
				: int32_t(regs->readIntReg(inst.rb));
			int32_t r = a - b;
			//regs->writeIntReg(inst.rc, uint64_t(r));
			// 3) Update condition codes: N, Z, V, C
			//    - first arg is the signed result
			//    - next two are the signed operands
			//    - isSubtraction = false for an add
			ctx->updateConditionCodes(
				r,
				a,
				b,
				/*isSubtraction=*/true
			);
			// 4) Advance PC by one instruction
			ctx->advancePC();  // assuming advancePC() is defined to step by 4
			regs->writeIntReg(inst.rc, uint64_t(r));
			ctx->advancePC();
		}

		/** [10.1B] Subtract unsigned 32-bit */
		static inline void emitAlpha_S8SUBL(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint32_t a = uint32_t(regs->readIntReg(inst.ra));
			uint32_t b = inst.isLiteral
				? uint8_t(inst.rb)
				: uint32_t(regs->readIntReg(inst.rb));
			uint32_t r = a - b;
	    	// 3) Update condition codes: N, Z, V, C
			//    - first arg is the signed result
			//    - next two are the signed operands
			//    - isSubtraction = false for an add
			ctx->updateConditionCodes(
				r,
				a,
				b,
				/*isSubtraction=*/true
			);
			// 4) Advance PC by one instruction
			ctx->advancePC();  // assuming advancePC() is defined to step by 4
			regs->writeIntReg(inst.rc, uint64_t(r));
			ctx->advancePC();
		}

		/** [10.1D] Compare Unsigned Less-Than */
		static inline void emitAlpha_CMPULT(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);
			regs->writeIntReg(inst.rc, a < b ? 1 : 0);
			ctx->advancePC();
		}


		// [10.2B] Signed quadword subtract (64-bit)  
		static inline void emitAlpha_S4SUBQ(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// result = (int64_t)Ra – (int64_t)Rb_or_literal
			int64_t a = int64_t(regs->readIntReg(inst.ra));
			int64_t b = inst.isLiteral
				? int8_t(inst.rb)                             // sign-extend 8-bit literal
				: int64_t(regs->readIntReg(inst.rb));
			uint64_t r = a - b;
			// 3) Update condition codes: N, Z, V, C
			//    - first arg is the signed result
			//    - next two are the signed operands
			//    - isSubtraction = false for an add
			ctx->updateConditionCodes(
				r,
				a,
				b,
				/*isSubtraction=*/true
			);
			// 4) Advance PC by one instruction
			ctx->advancePC();  // assuming advancePC() is defined to step by 4
			regs->writeIntReg(inst.rc, uint64_t(r));
			ctx->advancePC();
		}

		// [10.3B] Unsigned quadword subtract (64-bit)  
		static inline void emitAlpha_S8SUBQ(const OperateInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// result = Ra – Rb_or_literal (no sign-extension)
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)                         // zero-extend literal
				: regs->readIntReg(inst.rb);
			uint64_t r = a - b;
			// 3) Update condition codes: N, Z, V, C
			//    - first arg is the signed result
			//    - next two are the signed operands
			//    - isSubtraction = false for an add
			ctx->updateConditionCodes(
				r,
				a,
				b,
				/*isSubtraction=*/true
			);
			// 4) Advance PC by one instruction
			ctx->advancePC();  // assuming advancePC() is defined to step by 4
			regs->writeIntReg(inst.rc, r);
			ctx->advancePC();
		}

		// [10.3D] Compare unsigned less-than-or-equal  
		static inline void emitAlpha_CMPULE(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext *ctx)
		{
			// result = (Ra ? Rb_or_literal) ? 1 : 0
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);

			regs->writeIntReg(inst.rc, (a <= b) ? 1 : 0);
			ctx->advancePC();
		}

		// [11.0] Bitwise AND  
		static inline void emitAlpha_AND(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// result = Ra & Rb_or_literal
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);
			regs->writeIntReg(inst.rc, a & b);
			ctx->advancePC();
		}

		// [11.8] Bit Clear (AND with complement)  
		static inline void emitAlpha_BIC(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// result = Ra & ~Rb_or_literal
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);
			regs->writeIntReg(inst.rc, a & ~b);
			ctx->advancePC();
		}

		// [11.20] Bitwise OR  
		/** [11.20] Bitwise OR (BIS) */
		static inline void emitAlpha_BIS(const OperateInstruction & inst,
		    RegisterBank * regs,
			AlphaProcessorContext *ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				 ? uint64_t(inst.rb)
				 : regs->readIntReg(inst.rb);
			uint64_t r = a | b;
			regs->writeIntReg(inst.rc, r);
			        // logical ops only set N and Z
				ctx->updateConditionCodes(static_cast<int64_t>(r),
					static_cast<int64_t>(a),
					static_cast<int64_t>(b),
			        /*isSubtraction=*/false);
			ctx->advancePC();
		}

		// [11.28] OR NOT (Ra | ~Rb)  
		static inline void emitAlpha_ORNOT(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// result = Ra | ~Rb_or_literal
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);
			regs->writeIntReg(inst.rc, a | ~b);
			ctx->advancePC();
		}

		// [11.40] Exclusive OR  
		static inline void emitAlpha_XOR(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// result = Ra ^ Rb_or_literal
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t b = inst.isLiteral
				? uint64_t(inst.rb)
				: regs->readIntReg(inst.rb);
			regs->writeIntReg(inst.rc, a ^ b);
			ctx->advancePC();
		}


		/** [12.12] Mask Word Low (MSKWL): zero two bytes starting at Rbv'<2:0> */
		static inline void emitAlpha_MSKWL(const OperateInstruction& inst, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t shift = inst.isLiteral
				? (inst.rb & 0x7)
				: (regs->readIntReg(inst.rb) & 0x7);
			uint64_t mask = ~(0xFFFFULL << (shift * 8));
			regs->writeIntReg(inst.rc, a & mask);
			ctx->advancePC();
		}

		/** [12.22] Mask Longword Low (MSKLL): zero four bytes starting at Rbv'<2:0> */
		static inline void emitAlpha_MSKLL(const OperateInstruction& inst, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t shift = inst.isLiteral
				? (inst.rb & 0x7)
				: (regs->readIntReg(inst.rb) & 0x7);
			uint64_t mask = ~(0xFFFFFFFFULL << (shift * 8));
			regs->writeIntReg(inst.rc, a & mask);
			ctx->advancePC();
		}

		/** [12.06] Extract Byte Low (EXTBL): Re = (Ra >> (Rbv'<2:0>*8)) & 0xFF */
        /** [12.06] Extract Byte Low (EXTBL) */
		static inline void emitAlpha_EXTBL(const OperateInstruction & inst,
			RegisterBank * regs,
			AlphaProcessorContext * ctx)
			 {
			uint64_t addr = regs->readIntReg(inst.ra);
			uint64_t tmp = 0;
			        // read one byte, trap on fault
			if (!ctx->memSystem()->readVirtualMemory(ctx, addr, tmp, 1,ctx->getProgramCounter())) {
			            // PC is left at faulting instruction
			 return;
			
			}
			 uint8_t v = static_cast<uint8_t>(tmp);
			regs->writeIntReg(inst.rc, v);
			        // extract is a “logical” op, so just update N/Z flags
			ctx->updateConditionCodes(static_cast<int64_t>(v),
				static_cast<int64_t>(addr),
				                                  /*dummy*/ 0,
				                                  /*isSubtraction=*/false);
			ctx->advancePC();
		}

		/** [12.16] Extract Word Low (EXTWL): Re = (Ra >> (Rbv'<2:0>*8)) & 0xFFFF */
		static inline void emitAlpha_EXTWL(const OperateInstruction& inst, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t shiftBytes = inst.isLiteral
				? (inst.rb & 0x7)
				: (regs->readIntReg(inst.rb) & 0x7);
			uint64_t tmp = a >> (shiftBytes * 8);
			regs->writeIntReg(inst.rc, tmp & 0xFFFFULL);
			ctx->advancePC();
		}

		// Conditional?move if low bit set [11.14]
		static inline void emitAlpha_CMOVLBS(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			if ((a & 1ULL) != 0) {
				uint64_t b = inst.isLiteral
					? uint64_t(inst.rb)
					: regs->readIntReg(inst.rb);
				regs->writeIntReg(inst.rc, b);
				ctx->advancePC();
			}
		}

		// Conditional?move if signed > 0 [11.66]
		static inline void emitAlpha_CMOVGT(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = int64_t(regs->readIntReg(inst.ra));
			if (a > 0) {
				uint64_t b = inst.isLiteral
					? uint64_t(inst.rb)
					: regs->readIntReg(inst.rb);
				regs->writeIntReg(inst.rc, b);
				ctx->advancePC();
			}
		}

		// Extract Longword Low [12.26] (4-byte extract)
		static inline void emitAlpha_EXTLL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  sh = inst.pos & 0x3F;                              // 0–63 bits
			uint64_t mask = ((uint64_t(1) << inst.width) - 1);            // width bits
			regs->writeIntReg(inst.rc, (a >> sh) & mask);
			ctx->advancePC();
		}

		// Zero Bytes (ZAP) [12.30]
		// Byte_ZAP(Ra,     Rb<7:0>) — clear each byte i where bit i of Rb is 1
		static inline void emitAlpha_ZAP(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  msel = inst.isLiteral
				? inst.rb & 0xFF
				: uint8_t(regs->readIntReg(inst.rb));
			uint64_t mask = 0;
			for (int i = 0; i < 8; ++i) {
				if ((msel >> i) & 1)
					mask |= (0xFFULL << (i * 8));
			}
			regs->writeIntReg(inst.rc, a & ~mask);
			ctx->advancePC();
		}

		// Zero Bytes Not (ZAPNOT) [12.31]
		static inline void emitAlpha_ZAPNOT(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  msel = inst.isLiteral
				? inst.rb & 0xFF
				: uint8_t(regs->readIntReg(inst.rb));
			uint64_t mask = 0;
			for (int i = 0; i < 8; ++i) {
				if (!((msel >> i) & 1))
					mask |= (0xFFULL << (i * 8));
			}
			regs->writeIntReg(inst.rc, a & ~mask);
			ctx->advancePC();
		}

		// Mask Quadword Low [12.32] — zero field of width bits starting at pos
		static inline void emitAlpha_MSKQL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t mask = ((uint64_t(1) << inst.width) - 1) << inst.pos;
			regs->writeIntReg(inst.rc, a & ~mask);
			ctx->advancePC();
		}

		// Extract Quadword Low [12.36]
		static inline void emitAlpha_EXTQL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t sh = inst.pos & 0x3F;
			regs->writeIntReg(inst.rc, a >> sh);
			ctx->advancePC();
		}

		// Shift Left Logical [12.39]
		static inline void emitAlpha_SLL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  amt = inst.isLiteral
				? (inst.rb & 0x3F)
				: uint8_t(regs->readIntReg(inst.rb) & 0x3F);
			regs->writeIntReg(inst.rc, a << amt);
			ctx->advancePC();
		}

		// Mask Word High [12.52]
		static inline void emitAlpha_MSKWH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t mask = ((uint64_t(1) << inst.width) - 1)
				<< (64 - inst.pos - inst.width);
			regs->writeIntReg(inst.rc, a & ~mask);
			ctx->advancePC();
		}

		// Insert Word High [12.57]
		static inline void emitAlpha_INSWH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  sh = inst.pos;
			uint64_t val = inst.isLiteral
				? inst.rb
				: regs->readIntReg(inst.rb);
			uint64_t mask = ((uint64_t(1) << inst.width) - 1)
				<< (64 - sh - inst.width);
			uint64_t field = (val & ((uint64_t(1) << inst.width) - 1))
				<< (64 - sh - inst.width);
			regs->writeIntReg(inst.rc, (a & ~mask) | field);
			ctx->advancePC();
		}

		// Mask Longword High [12.62]
		static inline void emitAlpha_MSKLH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t mask = ((uint64_t(1) << inst.width) - 1)
				<< (64 - inst.pos - inst.width);
			regs->writeIntReg(inst.rc, a & ~mask);
			ctx->advancePC();
		}

		// Underflow?check variants simply reuse the same mask/extract/insert
		// semantics—but typically would raise an underflow exception here:
		//    regs->handleFloatingPointException(FPTrapType::FP_UNDERFLOW);
		// I’ve left them as plain ops; insert your exception call as needed.

		static inline void emitAlpha_INSLH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx) {
			emitAlpha_INSWH(inst, regs,ctx);
		}
		static inline void emitAlpha_MSKQH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx) {
			emitAlpha_MSKQL(inst, regs,ctx);
		}
		static inline void emitAlpha_INSQH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx) {
			emitAlpha_INSWH(inst, regs, ctx);
	
		}
		static inline void emitAlpha_INSBL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// Insert Byte Low: same as INSWH but pos/width refer to 8-bit field
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t val = inst.isLiteral
				? inst.rb & 0xFF
				: regs->readIntReg(inst.rb) & 0xFF;
			uint64_t mask = uint64_t(0xFF) << inst.pos;
			regs->writeIntReg(inst.rc, (a & ~mask) | (val << inst.pos));
			ctx->advancePC();
		}
		static inline void emitAlpha_INSWL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// Insert Word Low: 16-bit field
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t val = inst.isLiteral
				? (inst.rb & 0xFFFF)
				: regs->readIntReg(inst.rb) & 0xFFFF;
			uint64_t mask = (uint64_t(0xFFFF) << inst.pos);
			regs->writeIntReg(inst.rc, (a & ~mask) | (val << inst.pos));
			ctx->advancePC();
		}

		// [12.39] Shift Left Logical (SLL)
		static inline void emitAlpha_SLL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  amt = inst.isLiteral
				? (inst.rb & 0x3F)
				: (regs->readIntReg(inst.rb) & 0x3F);
			regs->writeIntReg(inst.rc, a << amt);
			ctx->advancePC();
		}

		// [12.2B] Insert Longword Low (INSLL) - 32-bit field
		static inline void emitAlpha_INSLL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t v = inst.isLiteral
				? uint32_t(inst.rb)
				: uint32_t(regs->readIntReg(inst.rb));
			uint64_t mask = uint64_t(0xFFFFFFFF) << inst.pos;
			regs->writeIntReg(inst.rc, (a & ~mask) | ((v & 0xFFFFFFFF) << inst.pos));
			ctx->advancePC();
		}

		// [12.3B] Insert Short Low (INSQL) - 16-bit field
		static inline void emitAlpha_INSQL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint64_t v = inst.isLiteral
				? uint16_t(inst.rb)
				: uint16_t(regs->readIntReg(inst.rb));
			uint64_t mask = uint64_t(0xFFFF) << inst.pos;
			regs->writeIntReg(inst.rc, (a & ~mask) | ((v & 0xFFFF) << inst.pos));
			ctx->advancePC();
		}

		// [12.3C] Shift Right Arithmetic (SRA)
		static inline void emitAlpha_SRA(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = int64_t(regs->readIntReg(inst.ra));
			uint8_t amt = inst.isLiteral
				? (inst.rb & 0x3F)
				: (regs->readIntReg(inst.rb) & 0x3F);
			regs->writeIntReg(inst.rc, uint64_t(a >> amt));
			ctx->advancePC();
		}

		// [12.5A] Extract Word High (EXTWH) - upper halfword
		static inline void emitAlpha_EXTWH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			// pos from MSB side: position of high-field = 64-pos-width
			uint8_t sh = 64 - inst.pos - inst.width;
			uint64_t tmp = (a >> sh) & ((uint64_t(1) << inst.width) - 1);
			regs->writeIntReg(inst.rc, tmp);
			ctx->advancePC();
		}

		// [12.6A] Extract Longword High (EXTLH) - 32-bit
		static inline void emitAlpha_EXTLH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t sh = 64 - inst.pos - 32;
			regs->writeIntReg(inst.rc, (a >> sh) & 0xFFFFFFFFULL);
			ctx->advancePC();
		}

		// [12.7A] Extract Quad High (EXTQH) - 64-bit (identity shift)
		static inline void emitAlpha_EXTQH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// EXTQH yields Ra right-shifted by pos
			uint64_t a = regs->readIntReg(inst.ra);
			uint8_t  sh = inst.pos;
			regs->writeIntReg(inst.rc, a >> sh);
			ctx->advancePC();
		}

		// [13.00] Multiply Longword (MULL) - signed 32×32->64
		static inline void emitAlpha_MULL(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = int32_t(regs->readIntReg(inst.ra));
			int64_t b = inst.isLiteral
				? int32_t(inst.rb)
				: int32_t(regs->readIntReg(inst.rb));
			regs->writeIntReg(inst.rc, uint64_t(a * b));
			ctx->advancePC();
		}

		// [13.20] Multiply Quadword (MULQ) - signed 64×64->64 (low)
		static inline void emitAlpha_MULQ(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = int64_t(regs->readIntReg(inst.ra));
			int64_t b = inst.isLiteral
				? int64_t(inst.rb)
				: int64_t(regs->readIntReg(inst.rb));
			regs->writeIntReg(inst.rc, uint64_t(a * b));
			ctx->advancePC();
		}

		// [13.30] Unsigned Multiply High (UMULH) - unsigned 64×64->128 high 64
		static inline void emitAlpha_UMULH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			unsigned __int128 a = regs->readIntReg(inst.ra);
			unsigned __int128 b = inst.isLiteral
				? regs->readIntReg(inst.rb)
				: regs->readIntReg(inst.rb);
			unsigned __int128 r = a * b;
			regs->writeIntReg(inst.rc, uint64_t(r >> 64));
			ctx->advancePC();
		}

		// [10.40] Vector Shift Right Arithmetic placeholder for OperateInstruction
		static inline void emitAlpha_V_SRA(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// TODO: implement vector SRA
			Q_UNUSED(inst);
			Q_UNUSED(regs);
		}

		// [10.60] Vector Extract Longword High placeholder
		static inline void emitAlpha_V_EXTLH(const OperateInstruction& inst,
			RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			// TODO: implement vector EXTLH
			Q_UNUSED(inst);
			Q_UNUSED(regs);
		}



    };

}



