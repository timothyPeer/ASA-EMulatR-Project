// MemoryInstruction.h
// Header for Alpha AXP “Mem”-format memory instructions  
// Format: opcode[31:26], Ra[25:21], Rb[20:16], disp[15:0]  
// See Alpha AXP System Reference Manual v6, §3.3.1, Fig. 3-1 (p. 3-9) :contentReference[oaicite:0]{index=0}



#include <cstdint>
#include "../ABA/structs/Instruction.h"
#include <stdexcept>

namespace Arch {
	// -----------------------------------------------------------------------------
	// Standard memory?reference format (load/store, address?compute, jumps):
	//   opcode[31:26], Ra[25:21], Rb[20:16], disp[15:0]
	//   Effective address = Rb + SEXT(disp)
	// -----------------------------------------------------------------------------
	struct MemoryFuncCode : public Arch::Instruction {
		uint32_t raw;    ///< Raw 32-bit instruction word
		uint8_t  opcode; ///< Major opcode bits <31:26>
		uint8_t  ra;     ///< Base/destination register bits <25:21>
		uint8_t  rb;     ///< Index/source register bits <20:16>
		int16_t  disp;   ///< 16-bit signed displacement bits <15:0>
		uint16_t fnc;          ///< bits <12:5> (extended opcode)

		/// Decode raw instruction into fields
		inline void decode() {
			opcode = static_cast<uint8_t>((raw >> 26) & 0x3F);
			ra = static_cast<uint8_t>((raw >> 21) & 0x1F);
			rb = static_cast<uint8_t>((raw >> 16) & 0x1F);
			disp = static_cast<int16_t>(raw & 0xFFFF);
		}

		FormatID format() const override { return FormatID::ALPHA_MEM; }
		uint16_t getCode() const override { return opcode; }
		/**
		 * Compute the virtual address for a memory access:
		 *   va = Rb_val + sign-extended displacement
		 * @param Rb_val  Value read from integer register Rb
		 * @return        64-bit effective address
		 */
		inline uint64_t computeAddress(uint64_t Rb_val) const {
			return Rb_val + static_cast<int64_t>(disp);
		}

		/** [18.0] Move from floating?point control register (Alpha), */
		static void emitAlpha_TRAPB(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [18.400] Convert quad?precision float ? long float (Alpha), */
		static void emitAlpha_EXCB(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [18.4400] Floating?point conditional move if not equal (Alpha), */
		static void emitAlpha_WMB(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [18.C000] Floating?point conditional move if ? (Alpha), */
		static void emitAlpha_RPCC(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [18.E000] Floating?point conditional move if greater?than (Alpha), */
		static void emitAlpha_RC(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [18.E800] Add single float with carry (IEEE), */
		static void emitAlpha_ECB(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [18.F000] Software breakpoint trap (Alpha), */
		static void emitAlpha_RS(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [1A.02] Add single float with signed integer (IEEE), */
		static void emitAlpha_RET(MemoryFuncCode inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}








	};
}




