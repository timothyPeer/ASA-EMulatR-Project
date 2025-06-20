#pragma once
// =============================================================================
// IntegerInterpreterExecutor.h
// =============================================================================
// C++-only interpreter for Alpha AXP integer operate instructions.
// Implements primary opcode groups 0x10, 0x11, 0x13, 0x1C.

#include "IExecutor.h"
#include "../ABA/structs/operateInstruction.h"
#include "../AEJ/IExecutionContext.h"
#include "../AEJ/constants/const_conditionCodes.h"  // calculateConditionCodes, ProcessorStatusFlags

#include <array>
#include "../../AEC/RegisterBank.h"
#include "../AEJ/AlphaProcessorContext.h"
#include "../AEJ/helpers/calculateConditionCodes.h"
#include "../ABA//executors/IExecutor.h"
#include "../AEC/AlphaFpcrFlags.h"

namespace Alpha {

	class IntegerInterpreterExecutor : public IExecutor {
	public:
		using Handler = void (IntegerInterpreterExecutor::*)(const Arch::OperateInstruction&);

		/// Construct with references to register bank and processor context
		IntegerInterpreterExecutor(RegisterBank* regs, AlphaProcessorContext* ctx)
			: regs_(regs), ctx_(ctx), dispatchTable_(createDispatchTable()) {
		}

		/// Fetch-decode-execute entry point
		void execute(const Arch::OperateInstruction& inst)  {
			// Primary opcode index: 0x10->0, 0x11->1, 0x13->2, 0x1C->3
			int pidx = (inst.opcode == 0x10 ? 0
				: inst.opcode == 0x11 ? 1
				: inst.opcode == 0x13 ? 2
				: inst.opcode == 0x1C ? 3
				: -1);
			if (pidx < 0) return;  // unsupported primary opcode

			uint8_t fnc = inst.fnc & 0x7F;  // 7-bit function/format code
			Handler h = dispatchTable_[pidx][fnc];
			if (h) (this->*h)(inst);
		}

	private:
		RegisterBank* regs_;
		AlphaProcessorContext* ctx_;
		std::array<std::array<Handler, 128>, 4> dispatchTable_;

		/// Build the 2D dispatch table for primary opcodes and function codes
		static std::array<std::array<Handler, 128>, 4> createDispatchTable() {
			std::array<std::array<Handler, 128>, 4> table{};
			auto& t10 = table[0], & t11 = table[1], & t13 = table[2], & t1C = table[3];

			// === Fmt10: Integer arithmetic ===
			t10[0x00] = &IntegerInterpreterExecutor::interpAddl;
			t10[0x40] = &IntegerInterpreterExecutor::interpAddL_V;
			t10[0x20] = &IntegerInterpreterExecutor::interpAddq;
			t10[0x60] = &IntegerInterpreterExecutor::interpAddQ_V;
			t10[0x09] = &IntegerInterpreterExecutor::interpSubL;
			t10[0x49] = &IntegerInterpreterExecutor::interpSubL_V;
			t10[0x29] = &IntegerInterpreterExecutor::interpSubQ;
			t10[0x69] = &IntegerInterpreterExecutor::interpSubQ_V;

			t10[0x0F] = &IntegerInterpreterExecutor::interpCmpBge;  // CMPBGE
			t10[0x2D] = &IntegerInterpreterExecutor::interpCmpeq;   // CMPEQ (Opr 10.2D) 
			t10[0x6D] = &IntegerInterpreterExecutor::interpCmple;   // CMPLE (Opr 10.6D) 
			t10[0x4D] = &IntegerInterpreterExecutor::interpCmplt;   // CMPLT (Opr 10.4D) 
			t10[0x3D] = &IntegerInterpreterExecutor::interpCmpule;  // CMPULE (Opr 10.3D) 
			t10[0x1D] = &IntegerInterpreterExecutor::interpCmpult;  // CMPULT (Opr 10.1D) 
			t10[0x02] = &IntegerInterpreterExecutor::interpS4Addl;  // S4ADDL
			t10[0x22] = &IntegerInterpreterExecutor::interpS4Addq;  // S4ADDQ
			t10[0x0B] = &IntegerInterpreterExecutor::interpS4Subl;  // S4Subl, Scaled (4-bit) Longword Sub   (Opr 10.0B) 
			t10[0x2B] = &IntegerInterpreterExecutor::interpS4Subq;  // S4Subq, Scaled (4-bit) Quadword Sub   (Opr 10.2B) 
			t10[0x12] = &IntegerInterpreterExecutor::interpS8Addl;  // S8ADDL
			t10[0x32] = &IntegerInterpreterExecutor::interpS8Addq;  // S8ADDQ
			t10[0x1B] = &IntegerInterpreterExecutor::interpS8SubL;    // S8SUBL (Opr 10.09) 
			t10[0x3B] = &IntegerInterpreterExecutor::interpS8Subq;    // S8SUBQ

			// (Other Fmt10 handlers: CMP*, S4*, S8*, etc. can be added similarly)

			// === Fmt11: Logical operations ===
			t11[0x00] = &IntegerInterpreterExecutor::interpAnd;
			t11[0x08] = &IntegerInterpreterExecutor::interpBic;
			t11[0x20] = &IntegerInterpreterExecutor::interpBis;
			t11[0x28] = &IntegerInterpreterExecutor::interpOrNot;
			t11[0x40] = &IntegerInterpreterExecutor::interpXor;
			t11[0x61] = &IntegerInterpreterExecutor::interpAMask;   // AMASK
			t11[0x6C] = &IntegerInterpreterExecutor::interpIMPLVER; // IMPLVER
			t11[0x24] = &IntegerInterpreterExecutor::interpCMoveQ;  // CMOVEQ
			t11[0x46] = &IntegerInterpreterExecutor::interpCMovGe;  // CMOVGE
			t11[0x66] = &IntegerInterpreterExecutor::interpCMovGt;   // CMOVGT
			t11[0x16] = &IntegerInterpreterExecutor::interpCMovlBc;   // CMOVLBC
			t11[0x14] = &IntegerInterpreterExecutor::interpCMovLbs;  // CMOVLBS
			t11[0x64] = &IntegerInterpreterExecutor::interpCMovLe;  // CMOVLE
			t11[0x44] = &IntegerInterpreterExecutor::interpCMovLt;  // CMOVLT
			t11[0x26] = &IntegerInterpreterExecutor::interpCMovNe;  // CMOVNE
			t11[0x24] = &IntegerInterpreterExecutor::interpCMoveQ;  // CMOVEQ
			t11[0x48] = &IntegerInterpreterExecutor::interpEqv;   // EQV
			// (Add shifts and conditionals as needed)

			// === Fmt13: Multiply ===
			t13[0x00] = &IntegerInterpreterExecutor::interpMull;
			t13[0x40] = &IntegerInterpreterExecutor::interpMull_V;
			t13[0x20] = &IntegerInterpreterExecutor::interpMulq;
			t13[0x60] = &IntegerInterpreterExecutor::interpMulQ_V;
			t13[0x30] = &IntegerInterpreterExecutor::interpUmulh;  // UMULH, Unsigned Multiply High  (Opr 13.30) :contentReference[oaicite:23]{index=23}
			// === Fmt1C: Count/Pack/Unpack ===
			t1C[0x30] = &IntegerInterpreterExecutor::interpCtpop;
			t1C[0x32] = &IntegerInterpreterExecutor::interpCtlz;

			t1C[0x33] = &IntegerInterpreterExecutor::interpCttz;
			// (Other 1C handlers omitted for brevity)

			return table;
		}

		inline void checkOverflowAndTrap(int64_t result, int64_t a, int64_t b, bool subtract) {
			auto flags = AlphaPS::calculateConditionCodes(result, a, b, subtract);
			if (flags.overflow && ctx_->isIntegerOverflowEnabled()) {
				ctx_->notifyTrapRaised(TrapType::ArithmeticTrap);
			}
		}

		/**
 * @brief Helper to compute flags and raise overflow trap if enabled.
 */
		static inline void checkOverflowAndTrap(AlphaProcessorContext* ctx, qint64 result, qint64 a, qint64 b, bool isSub)
		{
			auto flags = AlphaPS::calculateConditionCodes(result, a, b, isSub);
			if (flags.overflow && ctx->isIntegerOverflowEnabled()) {
				ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			}
		}
		// === Fmt10 Compare Handlers ===
		inline void interpCmpBge(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			uint64_t res = (a >= b) ? 1ULL : 0ULL;
			regs_->writeIntReg(i.dest(), res);
			ctx_->updateConditionCodes(static_cast<int64_t>(res), a, b, false);
		}

		inline void interpCmpeq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			uint64_t res = (a == b) ? 1ULL : 0ULL;
			regs_->writeIntReg(i.dest(), res);
			ctx_->updateConditionCodes(static_cast<int64_t>(res), a, b, false);
		}

		inline void interpCmple(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			uint64_t res = (a <= b) ? 1ULL : 0ULL;
			regs_->writeIntReg(i.dest(), res);
			ctx_->updateConditionCodes(static_cast<int64_t>(res), a, b, false);
		}

		inline void interpCmplt(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			uint64_t res = (a < b) ? 1ULL : 0ULL;
			regs_->writeIntReg(i.dest(), res);
			ctx_->updateConditionCodes(static_cast<int64_t>(res), a, b, false);
		}

		inline void interpCmpule(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = regs_->readIntReg(i.srcB());
			uint64_t res = (a <= b) ? 1ULL : 0ULL;
			regs_->writeIntReg(i.dest(), res);
			// unsigned compare: treat operands as signed for update only
			ctx_->updateConditionCodes(static_cast<int64_t>(res),
				static_cast<int64_t>(a),
				static_cast<int64_t>(b),
				false);
		}

		inline void interpCmpult(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = regs_->readIntReg(i.srcB());
			uint64_t res = (a < b) ? 1ULL : 0ULL;
			regs_->writeIntReg(i.dest(), res);
			ctx_->updateConditionCodes(static_cast<int64_t>(res),
				static_cast<int64_t>(a),
				static_cast<int64_t>(b),
				false);
		}

		// === Fmt10 Scaled 4-bit Arithmetic ===
		inline void interpS4Addl(const Arch::OperateInstruction& i) {
			int32_t a = static_cast<int32_t>(regs_->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs_->readIntReg(i.srcB()));
			int32_t r = (a << 2) + b;
			regs_->writeIntReg(i.dest(), static_cast<int64_t>(r));
			ctx_->updateConditionCodes(static_cast<int64_t>(r), a << 2, b, false);
		}

		inline void interpS4Addq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = (a << 2) + b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a << 2, b, false);
		}

		inline void interpS4Subl(const Arch::OperateInstruction& i) {
			int32_t a = static_cast<int32_t>(regs_->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs_->readIntReg(i.srcB()));
			int32_t r = (a << 2) - b;
			regs_->writeIntReg(i.dest(), static_cast<int64_t>(r));
			ctx_->updateConditionCodes(static_cast<int64_t>(r), a << 2, b, true);
		}

		inline void interpS4Subq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = (a << 2) - b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a << 2, b, true);
		}

		// === Fmt10 Scaled 8-bit Arithmetic ===
		inline void interpS8Addl(const Arch::OperateInstruction& i) {
			int32_t a = static_cast<int32_t>(regs_->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs_->readIntReg(i.srcB()));
			int32_t r = (a << 3) + b;
			regs_->writeIntReg(i.dest(), static_cast<int64_t>(r));
			ctx_->updateConditionCodes(static_cast<int64_t>(r), a << 3, b, false);
		}

		inline void interpS8Addq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = (a << 3) + b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a << 3, b, false);
		}

		inline void interpS8SubL(const Arch::OperateInstruction& i) {
			int32_t a = static_cast<int32_t>(regs_->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs_->readIntReg(i.srcB()));
			int32_t r = (a << 3) - b;
			regs_->writeIntReg(i.dest(), static_cast<int64_t>(r));
			ctx_->updateConditionCodes(static_cast<int64_t>(r), a << 3, b, true);
		}

		inline void interpS8Subq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = (a << 3) - b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a << 3, b, true);
		}

		// === Fmt10 Handlers ===
		inline void interpAddl(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = a + b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a, b, false);
		}

		inline void interpAddL_V(const Arch::OperateInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int32_t a = static_cast<int32_t>(regs->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs->readIntReg(i.srcB()));
			int32_t r = a + b;
			regs->writeIntReg(i.dest(), static_cast<int64_t>(r));
			checkOverflowAndTrap(ctx, r, a, b, false);
		}


		inline void interpAddq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = a + b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a, b, false);
		}

		inline void interpAddQ_V(const Arch::OperateInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = regs->readIntReg(i.srcA());
			int64_t b = regs->readIntReg(i.srcB());
			int64_t r = a + b;
			regs->writeIntReg(i.dest(), r);
			checkOverflowAndTrap(ctx, r, a, b, false);
		}



		inline void interpSubL(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = a - b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a, b, true);
		}

		inline void interpSubL_V(const Arch::OperateInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int32_t a = static_cast<int32_t>(regs->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs->readIntReg(i.srcB()));
			int32_t r = a - b;
			regs->writeIntReg(i.dest(), static_cast<int64_t>(r));
			checkOverflowAndTrap(ctx, r, a, b, true);
		}

		inline void interpSubQ(const Arch::OperateInstruction& i) { interpSubL(i); }
		inline void interpSubQ_V(const Arch::OperateInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = regs->readIntReg(i.srcA());
			int64_t b = regs->readIntReg(i.srcB());
			int64_t r = a - b;
			regs->writeIntReg(i.dest(), r);
			checkOverflowAndTrap(ctx, r, a, b, true);
		}

		// === Fmt11 Handlers (examples) ===
		inline void interpAnd(const Arch::OperateInstruction& i) {
			uint64_t r = regs_->readIntReg(i.srcA()) & regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(static_cast<int64_t>(r),
				static_cast<int64_t>(regs_->readIntReg(i.srcA())),
				static_cast<int64_t>(regs_->readIntReg(i.srcB())),
				false);
		}
		inline void interpBic(const Arch::OperateInstruction& i) { /* ... */ }

		// === Fmt13 Handlers (examples) ===
		inline void interpMull(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = static_cast<int32_t>(a) * static_cast<int32_t>(b);
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a, b, false);
		}
		inline void interpMulL_V(const Arch::OperateInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int32_t a = static_cast<int32_t>(regs->readIntReg(i.srcA()));
			int32_t b = static_cast<int32_t>(regs->readIntReg(i.srcB()));
			int64_t r = static_cast<int64_t>(a) * static_cast<int64_t>(b);
			regs->writeIntReg(i.dest(), r);
			// Trap if result cannot be represented in 32-bit signed
			if (static_cast<int32_t>(r) != r && ctx->isIntegerOverflowEnabled()) {
				ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			}
		}


		// === Fmt1C Handlers (examples) ===
		inline void interpCtpop(const Arch::OperateInstruction& i) {
			uint64_t v = regs_->readIntReg(i.srcA());
			regs_->writeIntReg(i.dest(), __builtin_popcountll(v));
			ctx_->updateConditionCodes(static_cast<int64_t>(regs_->readIntReg(i.dest())),
				static_cast<int64_t>(v), 0, false);
		}

		// ... implement remaining handlers similarly ...

			// === Fmt11 Logical Operations ===

	// BIS: Bit Set (fnc=0x20) — r = a | b
		inline void interpBis(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = regs_->readIntReg(i.srcB());
			uint64_t r = a | b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(static_cast<int64_t>(r), static_cast<int64_t>(a), static_cast<int64_t>(b), false);
		}

		// ORNOT: r = a | ~b (fnc=0x28)
		inline void interpOrNot(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = ~regs_->readIntReg(i.srcB());
			uint64_t r = a | b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(static_cast<int64_t>(r), static_cast<int64_t>(a), static_cast<int64_t>(b), false);
		}

		// XOR: Exclusive OR (fnc=0x40)
		inline void interpXor(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = regs_->readIntReg(i.srcB());
			uint64_t r = a ^ b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(static_cast<int64_t>(r), static_cast<int64_t>(a), static_cast<int64_t>(b), false);
		}

		// AMASK: Arithmetic mask (fnc=0x61)
		inline void interpAMask(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t r = 0;
			for (int byte = 0; byte < 8; ++byte) {
				uint8_t v = (a >> (byte * 8)) & 0xFF;
				uint8_t m = (v & 0x80) ? 0xFF : 0x00;
				r |= static_cast<uint64_t>(m) << (byte * 8);
			}
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(static_cast<int64_t>(r), static_cast<int64_t>(a), 0, false);
		}

		// IMPLVER: Implementation version (fnc=0x6C)
		inline void interpIMPLVER(const Arch::OperateInstruction& i) {
			uint64_t ver = ctx_->getImplementationVersion();
			regs_->writeIntReg(i.dest(), ver);
			ctx_->updateConditionCodes(static_cast<int64_t>(ver), static_cast<int64_t>(ver), 0, false);
		}

		// =============================================================================
// IntegerInterpreterExecutor: CMOVEQ Handler (Fmt11 fnc=0x24)
// =============================================================================
// Conditional move if equal: Rc = (Z ? Ra : Rb)
// Based on Alpha AXP Architecture Reference Manual, Appendix C.2, Fmt11 &#40;fnc=0x24&#41;

		inline void interpCMoveQ(const Arch::OperateInstruction& i) {
			// Read the current condition flags
			ProcessorStatusFlags flags = ctx_->getConditionFlags();
			// If Zero flag is set, select Ra, else select Rb
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = regs_->readIntReg(i.srcB());
			uint64_t result = flags.zero ? a : b;
			// Write the result to destination register
			regs_->writeIntReg(i.dest(), result);
			// Update condition codes based on the moved value
			ctx_->updateConditionCodes(static_cast<int64_t>(result),
				static_cast<int64_t>(result),
				0, false);
		}


		// CMOVEQ: Conditional Move if Equal (fnc=0x24)
		inline void interpCMoveQ(const Arch::OperateInstruction& i) {
			auto flags = ctx_->getConditionFlags();
			uint64_t val = flags.zero
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVGE: Conditional Move if Greater or Equal (fnc=0x46)
		inline void interpCMovGe(const Arch::OperateInstruction& i) {
			auto flags = ctx_->getConditionFlags();
			bool cond = (!flags.negative || flags.zero);
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVGT: Conditional Move if Greater Than (fnc=0x66)
		inline void interpCMovGt(const Arch::OperateInstruction& i) {
			auto flags = ctx_->getConditionFlags();
			bool cond = (!flags.negative && !flags.zero);
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVLBC: Conditional Move if Low Bit Clear (fnc=0x16)
		inline void interpCMovlBc(const Arch::OperateInstruction& i) {
			uint64_t b = regs_->readIntReg(i.srcB());
			bool cond = ((b & 1) == 0);
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVLBS: Conditional Move if Low Bit Set (fnc=0x14)
		inline void interpCMovLbs(const Arch::OperateInstruction& i) {
			uint64_t b = regs_->readIntReg(i.srcB());
			bool cond = ((b & 1) != 0);
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVLE: Conditional Move if Less or Equal (fnc=0x64)
		inline void interpCMovLe(const Arch::OperateInstruction& i) {
			auto flags = ctx_->getConditionFlags();
			bool cond = (flags.negative || flags.zero);
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVLT: Conditional Move if Less Than (fnc=0x44)
		inline void interpCMovLt(const Arch::OperateInstruction& i) {
			auto flags = ctx_->getConditionFlags();
			bool cond = flags.negative;
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// CMOVNE: Conditional Move if Not Equal (fnc=0x26)
		inline void interpCMovNe(const Arch::OperateInstruction& i) {
			auto flags = ctx_->getConditionFlags();
			bool cond = !flags.zero;
			uint64_t val = cond
				? regs_->readIntReg(i.srcA())
				: regs_->readIntReg(i.srcB());
			regs_->writeIntReg(i.dest(), val);
			ctx_->updateConditionCodes(static_cast<int64_t>(val), static_cast<int64_t>(val), 0, false);
		}

		// EQV: Equivalent (XNOR) (fnc=0x48) — r = ~(a ^ b)
		inline void interpEqv(const Arch::OperateInstruction& i) {
			uint64_t a = regs_->readIntReg(i.srcA());
			uint64_t b = regs_->readIntReg(i.srcB());
			uint64_t r = ~(a ^ b);
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(static_cast<int64_t>(r), static_cast<int64_t>(a), static_cast<int64_t>(b), false);
		}

		// === Fmt13: Multiply Quadword Low ===
		// MULQ: Multiply signed 64-bit, low 64 bits of product (fnc=0x20)
		inline void interpMulq(const Arch::OperateInstruction& i) {
			int64_t a = regs_->readIntReg(i.srcA());
			int64_t b = regs_->readIntReg(i.srcB());
			int64_t r = a * b;
			regs_->writeIntReg(i.dest(), r);
			ctx_->updateConditionCodes(r, a, b, false);
		}

		// MULQ/V: Multiply signed 64-bit with overflow trap (fnc=0x60)
   // Detect overflow by verifying inverse division: if b != 0 and r/b != a
// 		inline void interpMulQ_V(const Arch::OperateInstruction& i) {
// 			int64_t a = regs_->readIntReg(i.srcA());
// 			int64_t b = regs_->readIntReg(i.srcB());
// 			int64_t r = a * b;
// 			regs_->writeIntReg(i.dest(), r);
// 			ctx_->updateConditionCodes(r, a, b, false);
// 			// detect signed overflow
// 			if (b != 0 && (r / b) != a) {
// 				ctx_->notifyTrapRaised(TrapType::ARITHMETIC);
// 			}
// 		}
		inline void interpMulQ_V(const Arch::OperateInstruction& i, RegisterBank* regs, AlphaProcessorContext* ctx)
		{
			int64_t a = regs->readIntReg(i.srcA());
			int64_t b = regs->readIntReg(i.srcB());
			int64_t r = a * b;
			regs->writeIntReg(i.dest(), r);
			// No overflow trap defined in ISA for full 64-bit multiply
		}

		// UMULH: Unsigned multiply high 64 bits (fnc=0x30)
/**
 * @brief UMULH: Unsigned Multiply High (fnc=0x30)
 * Computes the upper 64 bits of a 128-bit product of two unsigned 64-bit integers.
 * No overflow trap is raised, but condition codes are updated.
 */
		inline void interpUmulh(const Arch::OperateInstruction& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t a = regs->readIntReg(i.srcA());
			uint64_t b = regs->readIntReg(i.srcB());

#if defined(__SIZEOF_INT128__)
			// Perform 128-bit unsigned multiply
			__uint128_t product = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
			uint64_t high = static_cast<uint64_t>(product >> 64);
#else
			// Fallback software emulation if __uint128_t unsupported (optional)
			uint64_t a_hi = a >> 32, a_lo = a & 0xFFFFFFFF;
			uint64_t b_hi = b >> 32, b_lo = b & 0xFFFFFFFF;

			uint64_t p0 = a_lo * b_lo;
			uint64_t p1 = a_lo * b_hi;
			uint64_t p2 = a_hi * b_lo;
			uint64_t p3 = a_hi * b_hi;

			uint64_t mid1 = (p0 >> 32) + (p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF);
			uint64_t high = p3 + (p1 >> 32) + (p2 >> 32) + (mid1 >> 32);
#endif

			regs->writeIntReg(i.dest(), high);

			// Update condition codes using signed interpretation of high result
			ProcessorStatusFlags flags =
				AlphaPS::calculateConditionCodes(static_cast<int64_t>(high),
					static_cast<int64_t>(a),
					static_cast<int64_t>(b),
					false);

			ctx->updateConditionFlags(flags);
		}


		// === Fmt1C: Count Leading Zeros and Count Trailing Zeros ===
   // CTLZ: Count Leading Zeros (fnc=0x32)
		inline void interpCtlz(const Arch::OperateInstruction& i) {
			uint64_t v = regs_->readIntReg(i.srcA());
			uint64_t cnt = v ? static_cast<uint64_t>(__builtin_clzll(v)) : 64ULL;
			regs_->writeIntReg(i.dest(), cnt);
			ctx_->updateConditionCodes(static_cast<int64_t>(cnt), static_cast<int64_t>(cnt), 0, false);
		}

		// CTTZ: Count Trailing Zeros (fnc=0x33)
		inline void interpCttz(const Arch::OperateInstruction& i) {
			uint64_t v = regs_->readIntReg(i.srcA());
			uint64_t cnt = v ? static_cast<uint64_t>(__builtin_ctzll(v)) : 64ULL;
			regs_->writeIntReg(i.dest(), cnt);
			ctx_->updateConditionCodes(static_cast<int64_t>(cnt), static_cast<int64_t>(cnt), 0, false);
		}
	};

} // namespace Alpha
