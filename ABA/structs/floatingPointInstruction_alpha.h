#pragma once
// FloatingPointInstruction.h
// Header for decoding and executing Alpha AXP Floating-Point Operate instructions
// All functions are inline for maximum performance.
// References:
//   Floating-Point Operate Instruction Format (I), Section 3.3.4 (pp. 3-12) :contentReference[oaicite:0]{index=0}
//   Table 4-13: Floating-Point Operate Instructions Summary (I), Section 4.10 (pp. 4-90–4-96) :contentReference[oaicite:1]{index=1}


#include <cstdint>
#include "../ABA/structs/Instruction.h"

#include <stdexcept>
#include "../AEJ/AlphaProcessorContext.h"
#include <cfenv>
#include "../AEJ/IExecutionContext.h"
namespace Arch {

	// Representation of a 32-bit Floating-Point Operate instruction word
	struct FloatingPointInstruction_Alpha : public Arch::Instruction {
		uint32_t raw;      ///< Raw instruction bits
		uint8_t  opcode;   ///< bits <31:26>
		uint8_t  fa;       ///< bits <25:21>
		uint8_t  fb;       ///< bits <20:16>
		uint16_t fnc;      ///< bits <15:5>
		uint8_t  fe;       ///< bits <4:0>

		RegisterBank regs; 
	public:
		
		// Decode fields from raw instruction
		inline void decode() {
			opcode = (raw >> 26) & 0x3F;
			fa = (raw >> 21) & 0x1F;
			fb = (raw >> 16) & 0x1F;
			fnc = (raw >> 5) & 0x7FF;
			fe = raw & 0x1F;
		}


		// Resolve operand registers (F31 ? zero operand)
		inline uint8_t srcA() const { return fa == 31 ? 0 : fa; }
		inline uint8_t srcB() const { return fb == 31 ? 0 : fb; }
		inline uint8_t dest() const { return fe; }


		FormatID format() const override { return FormatID::ALPHA_FP_OPERATE; }
		uint16_t getCode() const override { return opcode; }
		/// Returns true if the fnc code is for an S_floating variant (vs T_floating).
		inline bool isSinglePrecision(uint16_t fnc) {
			// In Table 4-13, bits<7:5> of the 11-bit fnc field encode subtype:
			//   2 ? S_floating, 3 ? T_floating
			return ((fnc >> 3) & 0x3u) == 2u;
		}


#pragma region Square Root
		// --- Floating-point operate implementations ---

		static void emitAlpha_SUBL(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float b = static_cast<float>(regs->readFpReg(i.srcB()));
			float r = a - b;
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SUBQ(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double b = regs->readFpReg(i.srcB());
			double r = a - b;
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SUBL_V(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* /*ctx*/) {
			uint64_t a = regs->readRawFpBits(i.srcA());
			uint64_t result = (a & 0xFFFFFFFF00000000ull) | (a >> 32);
			regs->writeRawFpBits(i.dest(), result);
		}

		static void emitAlpha_SUBQ_V(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* /*ctx*/) {
			uint64_t a = regs->readRawFpBits(i.srcA());
			uint64_t hi = (a >> 32);
			uint64_t lo = (a & 0xFFFFFFFFull);
			uint64_t result = (lo << 32) | hi;
			regs->writeRawFpBits(i.dest(), result);
		}

		static void emitAlpha_CMPBGE(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double b = regs->readFpReg(i.srcB());
			bool ge = (a >= b);
			ctx->setFPConditionFlags(ge, false);
		}

		static void emitAlpha_CMPEQ(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double b = regs->readFpReg(i.srcB());
			ctx->setFPConditionFlags(a == b, false);
		}

		static void emitAlpha_CMPLT(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double b = regs->readFpReg(i.srcB());
			ctx->setFPConditionFlags(false, a < b);
		}

		static void emitAlpha_CMPLE(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double b = regs->readFpReg(i.srcB());
			bool le = (a <= b);
			ctx->setFPConditionFlags(le, le && (a < b));
		}

		static void emitAlpha_CMOVNE(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* /*ctx*/) {
			double a = regs->readFpReg(i.srcA());
			double b = regs->readFpReg(i.srcB());
			if (a != b)
				regs->writeFpReg(i.dest(), regs->readFpReg(i.dest()));
		}

		// --- Floating-point square-root operations ---

		static void emitAlpha_SQRTF_UC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float r = std::sqrt(a);
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTS_UC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTG_UC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTT_UC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTS_UM(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTT_UM(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTF_U(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float r = std::sqrt(a);
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTS_U(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTG_U(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		// --- Additional square-root variants ---

		static void emitAlpha_SQRTT_U(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTS_UD(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTT_UD(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTF_SC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float r = std::sqrt(a);
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTG_SC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTF_S(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float r = std::sqrt(a);
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
		}

		// SQRTG_S: standard sqrt, round-to-nearest, no traps
		static void emitAlpha_SQRTG_S(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			// save/restore rounding mode
			int oldRm = std::fesetround(FE_TONEAREST);
			std::feclearexcept(FE_ALL_EXCEPT);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		static void emitAlpha_SQRTF_SUC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float r = std::sqrt(a);
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
		}

		static void emitAlpha_SQRTS_SUC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
		}

		// SQRTG_SUC: sqrt, round-to-zero (chop), trap on invalid & underflow
		static void emitAlpha_SQRTG_SUC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_INVALID | FE_UNDERFLOW);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_INVALID)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			if (std::fetestexcept(FE_UNDERFLOW)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTT_SUC: double-sqrt, round-to-zero, trap on invalid & underflow
		static void emitAlpha_SQRTT_SUC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_INVALID | FE_UNDERFLOW);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_INVALID)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			if (std::fetestexcept(FE_UNDERFLOW)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTS_SUM: sqrt, round-to-nearest, trap only on underflow
		static void emitAlpha_SQRTS_SUM(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TONEAREST);
			std::feclearexcept(FE_UNDERFLOW);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_UNDERFLOW)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTT_SUM: double-sqrt, round-to-nearest, trap only on underflow
		static void emitAlpha_SQRTT_SUM(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TONEAREST);
			std::feclearexcept(FE_UNDERFLOW);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_UNDERFLOW)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTF_SU: sqrt, round-to-zero, no traps
		static void emitAlpha_SQRTF_SU(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_ALL_EXCEPT);
			float a = static_cast<float>(regs->readFpReg(i.srcA()));
			float r = std::sqrt(a);
			regs->writeFpReg(i.dest(), static_cast<double>(r));
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTS_SU: sqrt, round-to-zero, no traps
		static void emitAlpha_SQRTS_SU(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_ALL_EXCEPT);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTG_SU: double-sqrt, round-to-zero, no traps
		static void emitAlpha_SQRTG_SU(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_ALL_EXCEPT);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTT_SU: double-sqrt, round-to-zero, no traps
		static void emitAlpha_SQRTT_SU(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_ALL_EXCEPT);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTS_SUD: sqrt, round-to-nearest, trap on underflow only
		static void emitAlpha_SQRTS_SUD(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TONEAREST);
			std::feclearexcept(FE_UNDERFLOW);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_UNDERFLOW)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTT_SUD: double-sqrt, round-to-nearest, trap on underflow only
		static void emitAlpha_SQRTT_SUD(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TONEAREST);
			std::feclearexcept(FE_UNDERFLOW);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_UNDERFLOW)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
		// SQRTS_SUIC: sqrt, round-to-zero, trap on invalid only
		static void emitAlpha_SQRTS_SUIC(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx) {
			int oldRm = std::fesetround(FE_TOWARDZERO);
			std::feclearexcept(FE_INVALID);
			double a = regs->readFpReg(i.srcA());
			double r = std::sqrt(a);
			if (std::fetestexcept(FE_INVALID)) ctx->notifyTrapRaised(TrapType::ArithmeticTrap);
			regs->writeFpReg(i.dest(), r);
			ctx->updateFPConditionCodes(r);
			std::fesetround(oldRm);
		}
#pragma endregion Square Root
		/** [10.09] Add longword with 4-bit immediate  */
		static void emitAlpha_SUBL(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [10.29] Add quadword with 4-bit  */
	    // Double?precision subtraction: FD_new = FD_ra – FD_rb
		static void emitAlpha_SUBQ(const FloatingPointInstruction_Alpha& i,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{

		}


		/** [10.49] Extract word high  */
		static void emitAlpha_SUBL_V(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [10.69] Extract quadword high  */
		static void emitAlpha_SUBQ_V(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [10.0F] Multiply signed 64-bit  */
		static void emitAlpha_CMPBGE(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [10.2D] Floating-point add  */
		static void emitAlpha_CMPEQ(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [10.4D] Floating-point divide  */
		static void emitAlpha_CMPLT(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [10.6D] Floating-point add (immediate constant) */
		static void emitAlpha_CMPLE(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [11.26] Floating-point G-multiply (immediate constant) */
		static void emitAlpha_CMOVNE(FloatingPointInstruction_Alpha inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [14.0AF] Store longword with check (Alpha) */
		static void emitVAX_CVTGQ(FloatingPointInstruction_VAX inst,
			AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}




		/** [15.000] Floating-point G-divide (round toward zero) */
		static void emitVAX_ADDF_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.001] Convert quad to single (round toward zero) */
		static void emitVAX_SUBF_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.002] Convert quad to double (round toward zero) */
		static void emitVAX_MULF_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.003] Convert quad to quad-precision (nearest) */
		static void emitVAX_DIVF_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.020] Floating-point subtract (signaling) */
		static void emitVAX_ADDG_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.021] Floating-point multiply (signaling) */
		static void emitVAX_SUBG_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.022] Floating-point divide (signaling) */
		static void emitVAX_MULG_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.023] Convert double to quad-precision (signaling) */
		static void emitVAX_DIVG_C(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.80] Convert double to quad-precision (round toward zero) */
		static void emitAlpha_ADDF(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.080] Convert quad to double (signaling) */
		static void emitVAX_ADDF(FloatingPointInstruction_VAX inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.81] Floating-point G-add (round toward zero) */
		static void emitAlpha_SUBF(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}



		/** [15.82] Floating-point G-subtract (round toward zero) */
		static void emitAlpha_MULF(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		


		/** [15.09E] Floating-point multiply (integer set) */
		static void emitAlpha_CVTDG(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0A0] Floating-point divide (integer set) */
		static void emitAlpha_ADDG(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0A1] Convert double to quad-precision (integer set) */
		static void emitAlpha_SUBG(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0A2] Floating-point G-add (integer set) */
		static void emitAlpha_MULG(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0A3] Floating-point G-subtract (integer set) */
		static void emitAlpha_DIVG(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0A5] Floating-point G-multiply (integer set) */
		static void emitAlpha_CMPGEQ(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0A6] Floating-point G-divide (integer set) */
		static void emitAlpha_CMPGLT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		


		/** [15.0AC] Compare quad-precision < (integer convert) */
		static void emitAlpha_CVTGF(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0AD] Compare quad-precision ? (integer convert) */
		static void emitAlpha_CVTGD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0AF] Write performance monitor (Alpha) */
		static void emitAlpha_CVTGQ(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0BC] Convert quad to single (integer set) */
		static void emitAlpha_CVTQF(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [15.0BE] Convert quad to double (integer set) */
		static void emitAlpha_CVTQG(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		


		/** [16.000] Call kernel debugger (Alpha) */
		static void emitAlpha_ADDS_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E0] Convert single-precision float -> integer with trap  */
		static void emitAlpha_ADDS_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E0] Convert quad-precision float -> integer (double) */
		static void emitAlpha_ADDT_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E3] Divide modulo  */
		static void emitAlpha_DIVT_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E3] Convert single-precision float -> integer (unordered carry) */
		static void emitAlpha_DIVTID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E2] 0  */
		static void emitAlpha_MULT_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E2] Convert integer -> quad-precision float (double) */
		static void emitAlpha_MULTID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E1] Add saturating  */
		static void emitAlpha_SUBT_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0E1] Convert integer -> single-precision float (double) */
		static void emitAlpha_SUBTID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1E0] Convert quad-precision float -> integer (vector double) */
		static void emitAlpha_ADDT_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.01] Subtract saturating  */
		static void emitAlpha_SUBS_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.02] Multiply saturating  */
		static void emitAlpha_MULS_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.03] Divide saturating  */
		static void emitAlpha_DIVS_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5E0] Convert quad-precision float -> integer (suppress underflow  */
		static void emitAlpha_ADDT_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7E0] Convert single-precision float -> signed integer (round-to-nearest) (IEEE) */
		static void emitAlpha_ADDT_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1E1] Convert integer -> single-precision float  */
		static void emitAlpha_SUBT_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.020] 0  */
		static void emitAlpha_ADDT_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.21] Add unsigned carry  */
		static void emitAlpha_SUBT_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.22] Subtract unsigned carry  */
		static void emitAlpha_MULT_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.23] Multiply unsigned carry  */
		static void emitAlpha_DIVT_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.40] Divide unsigned carry  */
		static void emitAlpha_ADDS_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.41] Add with unsigned carry  */
		static void emitAlpha_SUBS_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.42] Subtract with unsigned carry  */
		static void emitAlpha_MULS_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.43] Multiply unsigned carry  */
		static void emitAlpha_DIVS_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5E1] Convert integer -> single-precision float (scalar) */
		static void emitAlpha_SUBT_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.60] Divide unsigned carry  */
		static void emitAlpha_ADDT_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.61] 0  */
		static void emitAlpha_SUBT_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.62] Add unsigned modulo  */
		static void emitAlpha_MULT_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.63] Subtract unsigned modulo  */
		static void emitAlpha_DIVT_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7E1] Convert quad-precision float -> signed integer (vector) (IEEE) */
		static void emitAlpha_SUBT_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.80] Multiply unsigned modulo  */
		static void emitAlpha_ADDS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.81] Divide unsigned modulo  */
		static void emitAlpha_SUBS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.82] Add with unsigned carry modulo  */
		static void emitAlpha_MULS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.83] Subtract with unsigned carry modulo  */
		static void emitAlpha_DIVS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.100] Multiply unsigned carry modulo  */
		static void emitAlpha_ADDS_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1E2] Convert single-precision float -> integer (suppress underflow) */
		static void emitAlpha_MULTIUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.101] Divide unsigned carry modulo  */
		static void emitAlpha_SUBS_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.102] 0  */
		static void emitAlpha_MULS_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.103] Add (round toward zero) */
		static void emitAlpha_DIVS_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.120] Subtract (round toward zero) */
		static void emitAlpha_ADDT_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.121] Multiply (round toward zero) */
		static void emitAlpha_SUBT_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.122] Divide (round toward zero) */
		static void emitAlpha_MULT_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.123] 0  */
		static void emitAlpha_DIVT_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.140] Add (suppress underflow/inexact) */
		static void emitAlpha_ADDS_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.141] Subtract (suppress underflow/inexact) */
		static void emitAlpha_SUBS_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.142] Multiply (suppress underflow/inexact) */
		static void emitAlpha_MULS_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.143] Divide (suppress underflow/inexact) */
		static void emitAlpha_DIVS_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.160] 0  */
		static void emitAlpha_ADDT_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.161] Add with suppress underflow  */
		static void emitAlpha_SUBT_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.162] Subtract with suppress underflow  */
		static void emitAlpha_MULT_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.163] Multiply with suppress underflow  */
		static void emitAlpha_DIVT_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.180] Divide with suppress underflow  */
		static void emitAlpha_ADDS_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.181] 0  */
		static void emitAlpha_SUBS_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.182] Add (suppress underflow & round-to-nearest) */
		static void emitAlpha_MULS_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.183] Subtract (suppress underflow & round-to-nearest) */
		static void emitAlpha_DIVS_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.500] Multiply (suppress underflow & round-to-nearest) */
		static void emitAlpha_ADDS_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5E2] Convert single-precision float -> integer (suppress underflow/inexact) */
		static void emitAlpha_MULT_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.501] Divide (suppress underflow & round-to-nearest) */
		static void emitAlpha_SUBS_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.502] Add with suspend underflow & round-to-nearest  */
		static void emitAlpha_MULS_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.503] Subtract with suspend underflow & round-to-nearest  */
		static void emitAlpha_DIVS_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.520] Multiply with suspend underflow & round-to-nearest  */
		static void emitAlpha_ADDT_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.521] Divide with suspend underflow & round-to-nearest  */
		static void emitAlpha_SUBT_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.522] 0  */
		static void emitAlpha_MULT_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.523] Add (round-to-nearest toward zero) */
		static void emitAlpha_DIVT_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.540] Subtract (round-to-nearest toward zero) */
		static void emitAlpha_ADDS_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.541] Multiply (round-to-nearest toward zero) */
		static void emitAlpha_SUBS_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.542] Divide (round-to-nearest toward zero) */
		static void emitAlpha_MULS_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.543] 0  */
		static void emitAlpha_DIVS_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.560] Add (suppress underflow/inexact  */
		static void emitAlpha_ADDT_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.561] Subtract (suppress underflow/inexact  */
		static void emitAlpha_SUBT_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.562] Multiply (suppress underflow/inexact  */
		static void emitAlpha_MULT_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.563] Divide (suppress underflow/inexact  */
		static void emitAlpha_DIVT_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.580] Add with suppress underflow/inexact & integer carry  */
		static void emitAlpha_ADDS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.580] Instruction memory barrier (Alpha) */
		static void emitAlpha_ADDS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.581] Subtract with suppress underflow/inexact & integer carry  */
		static void emitAlpha_SUBS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.582] Multiply with suppress underflow/inexact & integer carry  */
		static void emitAlpha_MULS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.583] Divide with suppress underflow/inexact & integer carry  */
		static void emitAlpha_DIVS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.700] 0  */
		static void emitAlpha_ADDS_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7E2] Convert signed integer -> single-precision float (IEEE) */
		static void emitAlpha_MULT_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.701] Add (suppress underflow/inexact & unsigned modulo) */
		static void emitAlpha_SUBS_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.702] Subtract (suppress underflow/inexact & unsigned modulo) */
		static void emitAlpha_MULS_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.703] Multiply (suppress underflow/inexact & unsigned modulo) */
		static void emitAlpha_DIVS_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.720] Divide (suppress underflow/inexact & unsigned modulo) */
		static void emitAlpha_ADDT_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.721] Add with suppress underflow/inexact & unsigned carry modulo  */
		static void emitAlpha_SUBT_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.722] Subtract with suppress underflow/inexact & unsigned carry modulo  */
		static void emitAlpha_MULT_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.723] Multiply with suppress underflow/inexact & unsigned carry modulo  */
		static void emitAlpha_DIVT_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.740] Divide with suppress underflow/inexact & unsigned carry modulo  */
		static void emitAlpha_ADDS_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.740] Generate trap (Alpha) */
		static void emitAlpha_ADDS_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.741] 0  */
		static void emitAlpha_SUBS_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.742] Add (round-to-nearest & unsigned integer) */
		static void emitAlpha_MULS_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.743] Subtract (round-to-nearest & unsigned integer) */
		static void emitAlpha_DIVS_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.760] Multiply (round-to-nearest & unsigned integer) */
		static void emitAlpha_ADDT_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}

		/** [16.762] Floating-point divide (round toward zero  */
		static void emitAlpha_MULT_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.763] Convert single-precision float -> signed integer (truncate) */
		static void emitAlpha_DIVT_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.780] Convert quad-precision float -> signed integer (truncate) */
		static void emitAlpha_ADDS_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.780] Kernel breakpoint (Alpha) */
		static void emitAlpha_ADDS_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.781] Convert signed integer -> single-precision float (truncate) */
		static void emitAlpha_SUBS_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.782] Convert signed integer -> quad-precision float (truncate) */
		static void emitAlpha_MULS_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.783] Convert single-precision float -> signed integer (round-to-nearest) */
		static void emitAlpha_DIVS_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1E3] Convert quad-precision float -> integer (vector carry) */
		static void emitAlpha_DIVT_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5E3] Convert quad-precision float -> integer (signaling & vector carry) */
		static void emitAlpha_DIVT_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7E3] Convert signed integer -> quad-precision float (IEEE) */
		static void emitAlpha_DIVT_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.02C] Convert quad-precision float -> signed integer (round-to-nearest) */
		static void emitAlpha_CVTTS_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.02F] Convert signed integer -> single-precision float (round-to-nearest) */
		static void emitAlpha_CVTTQ_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.03C] Convert signed integer -> quad-precision float (round-to-nearest) */
		static void emitAlpha_CVTQS_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.03E] Add with carry  */
		static void emitAlpha_CVTQT_C(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.06C] Subtract with borrow  */
		static void emitAlpha_CVTTS_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.06F] Multiply signed integer  */
		static void emitAlpha_CVTTQ_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.07C] Divide signed integer  */
		static void emitAlpha_CVTQS_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.07E] Compare unordered (floating-point) */
		static void emitAlpha_CVTQT_M(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A0] Compare equal (floating-point) */
		static void emitAlpha_ADDT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A1] Compare less-than (floating-point) */
		static void emitAlpha_SUBT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A2] Compare ? (floating-point) */
		static void emitAlpha_MULT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A3] Convert single-precision float -> signed integer  */
		static void emitAlpha_DIVT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A4] Convert quad-precision float -> signed integer  */
		static void emitAlpha_CMPTUN(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A5] Convert signed integer -> single-precision float  */
		static void emitAlpha_CMPTEQ(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A6] Convert signed integer -> quad-precision float  */
		static void emitAlpha_CMPTLT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0A7] Subtract with integer rounding  */
		static void emitAlpha_CMPTLE(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0AC] Multiply signed integer (round-to-nearest) */
		static void emitAlpha_CVTTS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0AF] Divide signed integer (round-to-nearest) */
		static void emitAlpha_CVTTQ(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0BC] Add double-precision float (round-to-nearest) */
		static void emitAlpha_CVTQS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0BE] Add with carry (integer) */
		static void emitAlpha_CVTQT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0C0] Software squash (Alpha) */
		static void emitAlpha_ADDS_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0C1] Subtract with integer rounding  */
		static void emitAlpha_SUBSID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0C2] Multiply signed integer (round-to-nearest) */
		static void emitAlpha_MULSID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0C3] Divide signed integer (round-to-nearest) */
		static void emitAlpha_DIVSID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0EC] Convert quad-precision float -> integer (vector carry) */
		static void emitAlpha_CVTTSID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0EF] Convert single-precision float -> integer (unsigned modulo) */
		static void emitAlpha_CVTTQD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0FC] Convert quad-precision float -> integer (vector modulo) */
		static void emitAlpha_CVTQS_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.0FE] Add with carry (round toward zero) */
		static void emitAlpha_CVTQT_D(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.12C] Subtract with borrow (round toward zero) */
		static void emitAlpha_CVTTS_UC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.12F] Multiply unsigned integer  */
		static void emitAlpha_CVTTQ_VC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.16C] Divide unsigned integer  */
		static void emitAlpha_CVTTS_UM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.16F] Convert single-precision float -> integer (round toward zero) */
		static void emitAlpha_CVTTQ_VM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1A0] Convert quad-precision float -> integer (round-to-nearest–even) */
		static void emitAlpha_ADDT_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1A1] Add double-precision float (unsigned round) */
		static void emitAlpha_SUBT_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1A2] Subtract double-precision float (unsigned round) */
		static void emitAlpha_MULT_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1A3] Multiply double-precision float (unsigned round) */
		static void emitAlpha_DIVT_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1AC] Divide double-precision float (unsigned round) */
		static void emitAlpha_CVTTS_U(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1AF] Add with carry (unsigned) */
		static void emitAlpha_CVTTQ_V(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1C0] Load unaligned quadword (Alpha) */
		static void emitAlpha_ADDS_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1C0] Subtract with borrow (unsigned) */
		static void emitAlpha_ADDS_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1C1] Multiply unsigned integer  */
		static void emitAlpha_SUBSIUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1C2] Divide unsigned integer  */
		static void emitAlpha_MULS_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1C3] Convert single-precision float -> integer (unsigned round) */
		static void emitAlpha_DIVS_UD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1EC] Convert single-precision float -> integer (suppress underflow & round) */
		static void emitAlpha_CVTTSIUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.1EF] Convert quad-precision float -> integer (suppress underflow & vector modulo) */
		static void emitAlpha_CVTTQ_VD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.2AC] Add with carry (round-to-nearest) */
		static void emitAlpha_CVTST(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.52C] Subtract with borrow (round-to-nearest) */
		static void emitAlpha_CVTTS_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.52F] Multiply signed integer (round-to-nearest) */
		static void emitAlpha_CVTTQ_SVC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.56C] Divide signed integer (round-to-nearest) */
		static void emitAlpha_CVTTS_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.56F] Compare unordered (signed  */
		static void emitAlpha_CVTTQ_SVM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A0] Compare equal (signed  */
		static void emitAlpha_ADDT_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A1] Compare less-than (signed  */
		static void emitAlpha_SUBT_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A2] Compare ? (signed  */
		static void emitAlpha_MULT_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A3] Convert single-precision float -> integer (signed  */
		static void emitAlpha_DIVT_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A4] Convert quad-precision float -> integer (signed  */
		static void emitAlpha_CMPTUN_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A5] Add double-precision float (suppress underflow  */
		static void emitAlpha_CMPTEQ_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A6] Subtract double-precision float (suppress underflow  */
		static void emitAlpha_CMPTLT_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5A7] Multiply double-precision float (suppress underflow  */
		static void emitAlpha_CMPTLE_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5AC] Divide double-precision float (suppress underflow  */
		static void emitAlpha_CVTTS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5AF] Add with carry (suppress underflow  */
		static void emitAlpha_CVTTQ_SV(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5C0] Store unaligned quadword (Alpha) */
		static void emitAlpha_ADDS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5C0] Subtract with borrow (suppress underflow  */
		static void emitAlpha_ADDS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5C1] Multiply signed integer (suppress underflow  */
		static void emitAlpha_SUBS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5C2] Divide signed integer (suppress underflow  */
		static void emitAlpha_MULS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5C3] Convert single-precision float -> integer (suppress underflow  */
		static void emitAlpha_DIVS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5EC] Convert signed integer -> single-precision float (suppress underflow) */
		static void emitAlpha_CVTTS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.5EF] Convert signed integer -> quad-precision float (suppress underflow) */
		static void emitAlpha_CVTTQ_SVD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.6AC] Convert single-precision float -> integer (suppress underflow/inexact & unsigned modulo) */
		static void emitAlpha_CVTST_S(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.72C] Convert quad-precision float -> integer (suppress underflow/inexact & vector modulo) */
		static void emitAlpha_CVTTS_SUIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.72F] Convert signed integer -> single-precision float (suppress underflow & round) */
		static void emitAlpha_CVTTQ_SVIC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.73C] Convert signed integer -> quad-precision float (suppress underflow & round) */
		static void emitAlpha_CVTQS_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.73E] Add with carry (round-to-nearest & unsigned) */
		static void emitAlpha_CVTQT_SUC(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.76C] Subtract with borrow (round-to-nearest & unsigned) */
		static void emitAlpha_CVTTS_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.76F] Multiply signed integer (round-to-nearest & unsigned) */
		static void emitAlpha_CVTTQ_SVIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.77C] Divide signed integer (round-to-nearest & unsigned) */
		static void emitAlpha_CVTQS_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.77E] Convert single-precision float -> integer (signed+unsigned  */
		static void emitAlpha_CVTQT_SUM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7A0] Convert quad-precision float -> integer (signed+vector integer) */
		static void emitAlpha_ADDT_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7A1] Convert signed integer -> single-precision float (round-to-nearest & unsigned) */
		static void emitAlpha_SUBT_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7A2] Convert signed integer -> quad-precision float (round-to-nearest & unsigned) */
		static void emitAlpha_MULT_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7A3] Add double-precision float (suppress underflow & integer carry) */
		static void emitAlpha_DIVT_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7AC] Subtract double-precision float (suppress underflow & integer carry) */
		static void emitAlpha_CVTTS_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7AF] Multiply double-precision float (suppress underflow & integer carry) */
		static void emitAlpha_CVTTQ_SVI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7BC] Divide double-precision float (suppress underflow & integer carry) */
		static void emitAlpha_CVTQS_SU(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7BE] Add with carry (suppress underflow & integer carry) */
		static void emitAlpha_CVTQT_SUI(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7C0] Convert to quadword float (Common_Vax) */
		static void emitAlpha_ADDS_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7C1] Subtract with borrow (signed->unsigned integer  */
			static void emitAlpha_SUBS_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7C2] Multiply signed->unsigned integer (IEEE) */
		static void emitAlpha_MULS_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7C3] Divide signed->unsigned integer (IEEE) */
		static void emitAlpha_DIVS_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7EC] Add double-precision float with signed->unsigned carry (IEEE) */
		static void emitAlpha_CVTTS_SUID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7EF] Subtract double-precision float with signed->unsigned carry (IEEE) */
		static void emitAlpha_CVTTQ_SVID(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7FC] Multiply double-precision float with signed->unsigned carry (IEEE) */
		static void emitAlpha_CVTQS_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [16.7FE] Divide double-precision float with signed->unsigned carry (IEEE) */
		static void emitAlpha_CVTQT_SUD(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.10] Add with carry (signed->unsigned integer  */
		static void emitAlpha_CVTLQ(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.20] Subtract with borrow (signed->unsigned integer  */
		static void emitAlpha_CPYS(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.21] Multiply signed->unsigned integer  */
		static void emitAlpha_CPYSN(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.22] Divide signed->unsigned integer  */
		static void emitAlpha_CPYSE(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.24] Convert single-precision float -> integer (signed->unsigned  */
		static void emitAlpha_MT_FPCR(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.25] Convert quad-precision float -> integer (signed vector integer) (IEEE) */
		static void emitAlpha_MF_FPCR(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.30] Convert unsigned integer -> single-precision float (IEEE) */
		static void emitAlpha_CVTQL(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.02A] Convert unsigned integer -> quad-precision float (IEEE) */
		static void emitAlpha_FCMOVEQ(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.02B] Convert integer -> long-quad float (Alpha) */
		static void emitAlpha_FCMOVNE(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.02C] Copy single-precision float (Alpha) */
		static void emitAlpha_FCMOVLT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.02D] Copy sign bit of single-precision float (Alpha) */
		static void emitAlpha_FCMOVGE(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.02E] Copy exponent of single-precision float (Alpha) */
		static void emitAlpha_FCMOVLE(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [17.02F] Move to floating-point control register (Alpha) */
		static void emitAlpha_FCMOVGT(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}
		static void emitAlpha_SUBT_SUIM(FloatingPointInstruction_Alpha inst, AlphaProcessorContext* ctx)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}

	};


	

	
}
