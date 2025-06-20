#pragma once
#pragma once
#include "opcode14_executorAlphaSQRT.h"
#include "DecodedInstruction.h"

/**
 * @brief Parse the common floating-point fields (Fa, Fb, Fe, Function) out of a 32-bit raw opcode.
 * @param raw32       The 32-bit instruction word (bits <31:0>).
 * @param outInstr    A reference to a new SQRTInstruction (or any FP struct) to populate.
 * @return True if the 6-bit opcode was 0x14 (SQRT); false otherwise.
 *
 * The top six bits (?31:26?) must equal 0x14 for SQRT. The next fields:
 *   • Fa  = (raw32 >> 16) & 0x1F
 *   • Fb  = (raw32 >> 11) & 0x1F   (only used in some FP operations)
 *   • Function = (raw32 >> 5) & 0x7FF
 *   • Fe  = raw32 & 0x1F
 *
 * See ASA Vol 1 3.3.4 Floating-Point Operate Instruction Format. 
 */
static bool decodeFloatingFields(uint32_t raw32, SQRTInstruction &outInstr)
{
    const uint32_t opcode = (raw32 >> 26) & 0x3F;
    if (opcode != 0x14)
    {
        return false; // not SQRT
    }

    outInstr.function = (raw32 >> 5) & 0x7FF;
    outInstr.srcRegister = (raw32 >> 16) & 0x1F; // Fa
    outInstr.dstRegister = raw32 & 0x1F;         // Fe

    // Determine precision based on function bits (e.g., see Table C-3 for SQRT qualifiers)
    if ((outInstr.function & 0x00F) == 0x00A)
    {
        outInstr.precision = SQRTInstruction::F_FLOAT; // VAX F_fp
    }
    else if ((outInstr.function & 0x00F) == 0x00B)
    {
        outInstr.precision = SQRTInstruction::S_FLOAT; // IEEE S_fp
    }
    else if ((outInstr.function & 0x0F0) == 0x020)
    {
        outInstr.precision = SQRTInstruction::G_FLOAT; // VAX G_fp
    }
    else
    {
        outInstr.precision = SQRTInstruction::T_FLOAT; // IEEE T_fp
    }

    // Determine rounding (bits ?10:9? of function). ASA Vol 1 §4.7.3
    if (outInstr.function & 0x400)
    {
        if (outInstr.function & 0x200)
        {
            outInstr.rounding = SQRTInstruction::CHOPPED; // Truncate
        }
        else
        {
            outInstr.rounding = SQRTInstruction::DEFAULT; // Round-to-nearest
        }
    }
    else
    {
        outInstr.rounding = SQRTInstruction::DEFAULT;
    }

    return true;
}
