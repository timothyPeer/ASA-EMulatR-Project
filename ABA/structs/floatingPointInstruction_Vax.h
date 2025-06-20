#pragma once
// FloatingPointInstruction.h
// Header for decoding and executing Alpha AXP Floating-Point Operate instructions
// All functions are inline for maximum performance.
// References:
//   Floating-Point Operate Instruction Format (I), Section 3.3.4 (pp. 3-12) :contentReference[oaicite:0]{index=0}
//   Table 4-13: Floating-Point Operate Instructions Summary (I), Section 4.10 (pp. 4-90–4-96) :contentReference[oaicite:1]{index=1}


#include <cstdint>
#include "../ABA/structs/Instruction.h"

namespace Arch {

	// Representation of a 32-bit Floating-Point Operate instruction word
	struct FloatingPointInstruction_VAX : public Arch::Instruction {
		uint32_t raw;      ///< Raw instruction bits
		uint8_t  opcode;   ///< bits <31:26>
		uint8_t  fa;       ///< bits <25:21>
		uint8_t  fb;       ///< bits <20:16>
		uint16_t fnc;      ///< bits <15:5>
		uint8_t  fe;       ///< bits <4:0>

	public:
		// Decode fields from raw instruction
		inline void decode() {
			opcode = (raw >> 26) & 0x3F;
			fa = (raw >> 21) & 0x1F;
			fb = (raw >> 16) & 0x1F;
			fnc = (raw >> 5) & 0x7FF;
			fe = raw & 0x1F;
		}

		/// Returns true if the fnc code is for an S_floating variant (vs T_floating).
		inline bool isSinglePrecision(uint16_t fnc) {
			// In Table 4-13, bits<7:5> of the 11-bit fnc field encode subtype:
			//   2 ? S_floating, 3 ? T_floating
			return ((fnc >> 3) & 0x3u) == 2u;
		}

		// Resolve operand registers (F31 ? zero operand)
		inline uint8_t srcA() const { return fa == 31 ? 0 : fa; }
		inline uint8_t srcB() const { return fb == 31 ? 0 : fb; }
		inline uint8_t dest() const { return fe; }


		FormatID format() const override { return FormatID::VAX_FP; }
		uint16_t getCode() const override { return opcode; }


		static void emitVAX_CVTGQ(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.081] Convert quad to quad-precision (signaling) */
		static void emitVAX_SUBF(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		/** [15.082] Floating-point add (integer set) */
		static void emitVAX_MULF(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.083] Floating-point subtract (integer set) */
		static void emitVAX_DIVF(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.100] Convert quad to quad-precision (integer set) */
		static void emitVAX_ADDF_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.101] Floating-point add (suppress underflow/inexact) */
		static void emitVAX_SUBF_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.102] Floating-point subtract (suppress underflow/inexact) */
		static void emitVAX_MULF_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.103] Floating-point multiply (suppress underflow/inexact) */
		static void emitVAX_DIVF_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.120] Convert double to quad-precision (suppress underflow/inexact) */
		static void emitVAX_ADDG_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.121] Floating-point G-add (signaling) */
		static void emitVAX_SUBG_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.122] Floating-point G-subtract (signaling) */
		static void emitVAX_MULG_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.123] Floating-point G-multiply (signaling) */
		static void emitVAX_DIVG_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.180] Convert quad to quad-precision (signaling) */
		static void emitVAX_ADDF_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.182] Floating-point add (integer-set) */
		static void emitVAX_MULF_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.183] Floating-point subtract (integer-set) */
		static void emitVAX_DIVF_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.400] Compare quad-precision ? (integer convert) */
		static void emitVAX_ADDF_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.401] Compare quad-precision < (integer convert) */
		static void emitVAX_SUBF_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.402] Compare quad-precision ? (integer convert) */
		static void emitVAX_MULF_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.403] Convert quad to single (integer-set) */
		static void emitVAX_DIVF_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.420] Convert quad to quad-precision (integer-set) */
		static void emitVAX_ADDG_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.422] Floating-point add (suppress underflow/inexact) */
		static void emitVAX_MULG_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.423] Floating-point subtract (suppress underflow/inexact) */
		static void emitVAX_DIVG_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.480] Floating-point G-add (suppress underflow/inexact) */
		static void emitVAX_ADDF_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.481] Floating-point G-subtract (suppress underflow/inexact) */
		static void emitVAX_SUBF_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.482] Floating-point G-multiply (suppress underflow/inexact) */
		static void emitVAX_MULF_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.483] Floating-point G-divide (suppress underflow/inexact) */
		static void emitVAX_DIVF_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.500] Floating-point G-multiply (round toward zero) */
		static void emitVAX_ADDF_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.501] Floating-point G-divide (round toward zero) */
		static void emitVAX_SUBF_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.502] Convert quad to single (round toward zero) */
		static void emitVAX_MULF_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.503] Convert quad to double (round toward zero) */
		static void emitVAX_DIVF_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.520] 0  */
		static void emitVAX_ADDG_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.521] Divide integer (truncating) */
		static void emitVAX_SUBG_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.522] Multiply integer (truncating) */
		static void emitVAX_MULG_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.523] Subtract integer (truncating) */
		static void emitVAX_DIVG_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.580] Add with carry  */
		static void emitVAX_ADDF_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.581] Subtract with carry  */
		static void emitVAX_SUBF_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.582] Multiply signed  */
		static void emitVAX_MULF_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.583] Divide signed  */
		static void emitVAX_DIVF_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.01E] Floating-point add (signaling) */
		static void emitVAX_CVTDG_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.02C] Floating-point G-add (signaling) */
		static void emitVAX_CVTGF_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.02D] Floating-point G-subtract (signaling) */
		static void emitVAX_CVTGD_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.02F] Floating-point G-multiply (signaling) */
		static void emitVAX_CVTGQ_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.02F] Store quadword with check (Alpha) */
		static void emitVAX_CVTBQ(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.03C] Floating-point G-divide (signaling) */
		static void emitVAX_CVTQF_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.03E] Convert quad to single (signaling) */
		static void emitVAX_CVTQG_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}

		/** [15.0A7] Compare quad-precision ? (integer convert) */
		static void emitVAX_CMPGLE(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}

		/** [15.11E] Floating-point divide (suppress underflow/inexact) */
		static void emitVAX_CVTDG_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.12C] Floating-point G-divide (signaling) */
		static void emitVAX_CVTGF_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.12D] Convert quad to single (signaling) */
		static void emitVAX_CVTGD_UC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.12F] Convert quad to double (signaling) */
		static void emitVAX_CVTGQ_NC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.19E] Floating-point multiply (integer-set) */
		static void emitVAX_CVTDG_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.1A0] Floating-point divide (integer-set) */
		static void emitVAX_ADDG_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.1A1] Convert double to quad-precision (integer-set) */
		static void emitVAX_SUBG_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.1A2] Floating-point G-add (integer-set) */
		static void emitVAX_MULG_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.1A3] Floating-point G-subtract (integer-set) */
		static void emitVAX_DIVG_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.1AC] Floating-point G-multiply (integer-set) */
		static void emitVAX_CVTGF_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.1AD] Floating-point G-divide (integer-set) */
		static void emitVAX_CVTGD_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}

		/** [15.41E] Convert quad to double (integer-set) */
			static void emitVAX_CVTDG_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.42C] Floating-point multiply (suppress underflow/inexact) */
		static void emitVAX_CVTGF_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.42D] Floating-point divide (suppress underflow/inexact) */
		static void emitVAX_CVTGD_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.42F] Convert double to quad-precision (suppress underflow/inexact) */
		static void emitVAX_CVTGQ_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.49E] Convert quad to single (suppress underflow/inexact) */
		static void emitVAX_CVTDG_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A0] Convert quad to double (suppress underflow/inexact) */
		static void emitVAX_ADDG_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A1] Convert quad to quad-precision (signaling suppress) */
		static void emitVAX_SUBG_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A2] 0  */
		static void emitVAX_MULG_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A3] Floating-point add (round toward zero) */
		static void emitVAX_DIVG_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A5] Floating-point subtract (round toward zero) */
		static void emitVAX_CMPGEQ_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A6] Floating-point multiply (round toward zero) */
		static void emitVAX_CMPGLT_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4A7] Floating-point divide (round toward zero) */
		static void emitVAX_CMPGLE_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4AC] Convert double to quad-precision (round toward zero) */
		static void emitVAX_CVTGF_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4AD] Floating-point G-add (round toward zero) */
		static void emitVAX_CVTGD_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.4AF] Floating-point G-subtract (round toward zero) */
		static void emitVAX_CVTGQ_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.51E] Convert quad to quad-precision (round-to-nearest) */
		static void emitVAX_CVTDG_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.52C] Subtract with carry (signed) */
		static void emitVAX_CVTGF_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.52D] Multiply signed  */
		static void emitVAX_CVTGD_SUC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.52F] Divide signed  */
		static void emitVAX_CVTGQ_SVC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.59E] 0  */
		static void emitVAX_CVTDG_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5A0] Add modulo  */
		static void emitVAX_ADDG_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5A1] Subtract modulo  */
		static void emitVAX_SUBG_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5A2] Multiply modulo  */
		static void emitVAX_MULG_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5A3] Divide modulo  */
		static void emitVAX_DIVG_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5AC] Add with carry modulo  */
		static void emitVAX_CVTGF_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5AD] Subtract with carry modulo  */
		static void emitVAX_CVTGD_SU(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [15.5AF] Multiply modulo  */
		static void emitVAX_CVTGQ_SV(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		/** [17.12F] Branch if ? (Alpha) */
		static void emitVAX_CVTBQ_VC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [18.4A4] Branch if bit set (Alpha) */
		static void emitVAX_CVTBQ_S(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		/** [19.5AF] Branch if not equal (Alpha) */
		static void emitVAX_CVTBQ_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}

		/** [19.5AF] Branch if not equal (Alpha) */
		static void emitVAX_CVTBQ_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}

		/** [20.52F] Branch if greater-or-equal (Alpha) */
		static void emitVAX_CVTBQ_SVC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}


		static void emitVAX_SUBF_U(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_DIVF_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}

		static void emitVAX_MULF_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_ADDG_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_SUBG_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_SUBG_SC(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_DIVG_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_MULG_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_ADDF_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
		static void emitVAX_SUBF_C(FloatingPointInstruction_VAX inst)
		{
			//TODO
		}
	};

	
		


}


