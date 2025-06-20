#pragma once
// MemoryInstruction.h
// Header for decoding Alpha AXP memory?reference instructions (Format I)
// All functions are inline for maximum performance.
// References:
//   Memory Instruction Format (Figure 3-1, Section 3.3.1), p. 3-9 :contentReference[oaicite:0]{index=0}
//   Memory Format with Function Code (Figure 3-2, Section 3.3.1.1), p. 3-10 :contentReference[oaicite:1]{index=1}
//   Memory Integer Load/Store Instructions (Table 4-2, Section 4.2), pp. 4-2–4-4 :contentReference[oaicite:2]{index=2}

#ifndef MEMORY_INSTRUCTION_H
#define MEMORY_INSTRUCTION_H

#include <cstdint>
#include "../ABA/structs/Instruction.h"
#include "InterpreterExecutorAll.h"
/**
 * Memory-format-with-function-code instruction:
 *   31      26 25   21 20   16 15            0
 *   | opcode |  Ra  |  Rb  |      fnc        |
 */

// -----------------------------------------------------------------------------
// Memory instructions with a 16-bit function code instead of a displacement:
//   opcode[31:26], Ra[25:21], Rb[20:16], fnc[15:0]
//   Used for: Memory Barrier, Fetch, Fetch_M, RPCC, RAISE, STx_C, etc.
// -----------------------------------------------------------------------------
struct MemoryFunctionInstruction : public Arch::Instruction {
    uint32_t raw;    ///< Raw 32-bit instruction word
    uint8_t  opcode; ///< Major opcode bits <31:26>
    uint8_t  ra;     ///< Register field bits <25:21> (usage varies)
    uint8_t  rb;     ///< Register field bits <20:16> (usage varies)
    uint16_t fnc;    ///< 16-bit function code bits <15:0>

    /// Decode raw instruction into fields
    inline void decode() {
        opcode = static_cast<uint8_t>((raw >> 26) & 0x3F);
        ra = static_cast<uint8_t>((raw >> 21) & 0x1F);
        rb = static_cast<uint8_t>((raw >> 16) & 0x1F);
        fnc = static_cast<uint16_t>(raw & 0xFFFF);
    }
	FormatID format() const override { return FormatID::ALPHA_MEMFCT; }
	uint16_t getCode() const override { return opcode; }
};

#endif // MEMORY_INSTRUCTION_H

