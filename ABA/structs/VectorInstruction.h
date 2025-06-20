#pragma once
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
#include <stdexcept>

// -----------------------------------------------------------------------------
// Branch format (conditional and unconditional):
//   opcode[31:26], Ra[25:21], Branch_disp[20:0]
//   Target VA = (PC + 4) + 4 * SEXT(Branch_disp)
// -----------------------------------------------------------------------------

namespace Arch {
	struct VectorInstruction : public Arch::Instruction {
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

		FormatID format() const override { return FormatID::ALPHA_VECTOR; }
		uint16_t getCode() const override { return opcode; }



		/** [13.40] Floating-point multiply (round toward zero) */
		static void emitAlpha_MULL_V(VectorInstruction inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [13.60] Floating-point divide (round toward zero) */
		static void emitAlpha_MULQ_V(VectorInstruction inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}





	};

}




