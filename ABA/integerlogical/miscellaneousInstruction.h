// MiscellaneousInstruction.h
// Header for Alpha AXP “Mbr”-format computed-jump instructions  
// Format: opcode[31:26], Ra[25:21], Rb[20:16], hint[15:0]  
// See Alpha AXP System Reference Manual v6, §3.3.1.2 (p. 3-10) :contentReference[oaicite:2]{index=2}

#ifndef miscellaneousInstruction_h__
#define miscellaneousInstruction_h__


#include <cstdint>

/**

 */
struct MiscellaneousInstruction {
    uint32_t raw;      ///< full 32-bit instruction word
    uint8_t  opcode;   ///< bits <31:26>
    uint8_t  ra;       ///< bits <25:21> (link or test register)
    uint8_t  rb;       ///< bits <20:16> (branch-predict hint register)
    uint16_t hint;     ///< bits <15:0>  branch-prediction hint/displacement

    /// Decode raw ? fields
    inline void decode() {
        opcode = static_cast<uint8_t>((raw >> 26) & 0x3F);
        ra = static_cast<uint8_t>((raw >> 21) & 0x1F);
        rb = static_cast<uint8_t>((raw >> 16) & 0x1F);
        hint = static_cast<uint16_t>(raw & 0xFFFF);
    }
};
#endif // miscellaneousInstruction_h__



