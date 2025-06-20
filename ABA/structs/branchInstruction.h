#pragma once
// BranchInstruction.h
// Header for decoding and executing Alpha AXP branch instructions
// All functions are inline for maximum performance.
//
// References:
//   Branch Instruction Format (Figure 3-3), Section 3.3.2, p. 3-10 :contentReference[oaicite:0]{index=0}
//   Conditional Branch Instructions, Section 4.3.1, pp. 4-18–4-19 :contentReference[oaicite:1]{index=1}
//   Unconditional Branch Instructions, Section 4.3.2, pp. 4-19–4-20 :contentReference[oaicite:2]{index=2}


#include <cstdint>
#include "../ABA/structs/Instruction.h"
#include "../AEJ/AlphaProcessorContext.h"

// -----------------------------------------------------------------------------
// Branch format (conditional and unconditional):
//   opcode[31:26], Ra[25:21], Branch_disp[20:0]
//   Target VA = (PC + 4) + 4 * SEXT(Branch_disp)
// -----------------------------------------------------------------------------

namespace Arch {
	struct BranchInstruction : public Arch::Instruction {
		uint32_t raw;
		uint8_t  opcode;
		uint8_t  ra;
		uint32_t disp;
		

		inline void decode(uint32_t bits) {
			raw = bits;
			opcode = (bits >> 26) & 0x3F;
			ra = (bits >> 21) & 0x1F;
			disp = bits & 0x1FFFFF;
		}

		FormatID format() const override { return FormatID::ALPHA_BRANCH; }
		uint16_t getCode() const override { return opcode; }

		// Compute the branch target: PC is the address of this instruction
		inline uint64_t computeTarget(uint64_t pc) const {
			// Sign-extend 21-bit disp, then multiply by 4
			int32_t disp21 = static_cast<int32_t>(disp << 11) >> 11;
			return pc + 4 + (static_cast<int64_t>(disp21) << 2);
		}

		static inline void emitAlpha_BR(const BranchInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			// Return address = next instruction
			uint64_t nextPC = ctx->getProgramCounter() + 4;
			regs->writeIntReg(i.ra, nextPC);
			// Compute branch target: sign-extend disp and shift left by 2
			int64_t disp = int64_t(i.disp) << 2;
			uint64_t target = nextPC + disp;
			ctx->setProgramCounter(target);
		}


	
		// Floating-point conditional branches (use FP flags)
		// [Opcode FBEQ] Floating-point branch if equal (FPCC == 01)
		static inline void emitAlpha_FBEQ(const BranchInstruction& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			bool ge, lt;
			ctx->getFPConditionFlags(ge, lt);
			bool equal = ge && !lt;
			if (equal) emitAlpha_BR(i, regs, ctx);
			else        ctx->advancePC();
		}

		// [Opcode FBLT] Floating-point branch if less than (FPCC == 10)
		static inline void emitAlpha_FBLT(const BranchInstruction& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			bool ge, lt;
			ctx->getFPConditionFlags(ge, lt);
			bool less = !ge && lt;
			if (less) emitAlpha_BR(i, regs, ctx);
			else      ctx->advancePC();
		}

		// [Opcode FBNE] Floating-point branch if not equal (FPCC != 01)
		static inline void emitAlpha_FBNE(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			bool ge, lt;
			ctx->getFPConditionFlags(ge, lt);
			bool equal = ge && !lt;
			if (!equal) emitAlpha_BR(i, regs, ctx);
			else        ctx->advancePC();
		}


		// [Opcode FBGE] Floating-point branch if greater or equal (FPCC == 11)
		static inline void emitAlpha_FBGE(const BranchInstruction& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			bool ge, lt;
			ctx->getFPConditionFlags(ge, lt);
			bool greater = ge && lt;
			if (greater) emitAlpha_BR(i, regs, ctx);
			else         ctx->advancePC();
		}


		static void emitAlpha_FBLE(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			bool ge, lt;
			ctx->getFPConditionFlags(ge, lt);
			bool equal = ge && !lt;
			bool less = !ge && lt;
			if (equal || less) emitAlpha_BR(i, regs, ctx);
			else            ctx->advancePC();
		}

		static void emitAlpha_FBGT(const BranchInstruction & i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			bool ge, lt;
			ctx->getFPConditionFlags(ge, lt);
			bool equal = ge && !lt;
			bool less = !ge && lt;
			if (!equal && !less) emitAlpha_BR(i, regs, ctx);
			else                 ctx->advancePC();
		}

		// Branch on carry bit clear/set
		static void emitAlpha_BLBC(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (!flags.carry) emitAlpha_BR(i, regs, ctx);
			else             ctx->advancePC();
		}


		// Integer conditional branches
		static void emitAlpha_BEQ(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (flags.zero) emitAlpha_BR(i, regs, ctx);
			else            ctx->advancePC();
		}

		static void emitAlpha_BLT(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (flags.negative) emitAlpha_BR(i, regs, ctx);
			else              ctx->advancePC();
		}

		
		static void emitAlpha_BLE(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (flags.zero || flags.negative) emitAlpha_BR(i, regs, ctx);
			else             ctx->advancePC();
		}

		static void emitAlpha_BLBS(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (flags.carry) emitAlpha_BR(i, regs, ctx);
			else             ctx->advancePC();
		}

		
		static void emitAlpha_BNE(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (!flags.zero) emitAlpha_BR(i, regs, ctx);
			else             ctx->advancePC();
		}

		static void emitAlpha_BGE(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (!flags.negative) emitAlpha_BR(i, regs, ctx);
			else             ctx->advancePC();
		}


		static void emitAlpha_BGT(const BranchInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx) 
		{
			auto flags = ctx->getConditionFlags();
			if (!flags.zero && !flags.negative) emitAlpha_BR(i, regs, ctx);
			else             ctx->advancePC();
		}

	};

}




