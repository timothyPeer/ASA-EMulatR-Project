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
    struct MemoryBarrierInstruction : public Arch::Instruction {
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
		/** [18.4000] Floating?point conditional move if equal (Alpha) */
        static void emitAlpha_MB(MemoryBarrierInstruction inst)
        {
            throw std::logic_error("The method or operation is not implemented.");
        }

		/** [34.] Store to stack (Alpha) */
		static void emitAlpha_BSR(MemoryBarrierInstruction inst)
		{
//TODO
		}


		/** [1A.00] Add single float */
		static void emitAlpha_JMP(MemoryBarrierInstruction inst)
		{
//TODO
		}


		/** [1A.01] Add single float */
		static void emitAlpha_JSR(MemoryBarrierInstruction inst)
		{
//TODO
		}


		/** [1A.03] Add double float (IEEE) */
		static void emitAlpha_JSR_COROUTINE(MemoryBarrierInstruction inst)
		{
//TODO
		}

    };
}



