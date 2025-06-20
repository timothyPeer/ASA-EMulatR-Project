#include "decodeOperate.h"
#include "decodedInstruction.h"
#include "enumerations/enumInstructionSections.h"

void decodeOperate(quint32 instruction, DecodedInstruction &result)
{
    //DecodedInstruction *result;

    // Extract basic fields
    result.opcode = (instruction >> 26) & 0x3F; // Bits 26-31
    result.ra = (instruction >> 21) & 0x1F;     // Bits 21-25
    result.rb = (instruction >> 16) & 0x1F;     // Bits 16-20
    result.function = instruction & 0xFFFF;     // Bits 0-15
    result.rc = (instruction >> 0) & 0x1F;      // Bits 0-4

    // Store decoded operands in the map
    result.decodedOperands["ra"] = result.ra;
    result.decodedOperands["rb"] = result.rb;
    result.decodedOperands["rc"] = result.rc;

    // Determine instructionFormat based on opcode
    switch (static_cast<InstructionSections>(result.opcode))
    {
    case InstructionSections::SECTION_INTEGER:
        result.instructionFormat = InstructionFormat::FORMAT_OPERATE;
        result.operands = {"ra", "rb", "rc"};

        // Set mnemonic based on function code
        switch (result.function & 0xFF)
        {
        case 0x20:
            result.mnemonic = "ADDQ";
            break;
        case 0x29:
            result.mnemonic = "SUBQ";
            break;
        case 0x2D:
            result.mnemonic = "CMPEQ";
            break;
        default:
            result.mnemonic = "INT_OP";
            break;
        }
        break;

    case InstructionSections::SECTION_FLOATING_POINT:
        result.instructionFormat = InstructionFormat::FORMAT_FLOAT;
        result.operands = {"ra", "rb", "rc"};

        // Set mnemonic based on function code
        switch (result.function)
        {
        case 0x580:
            result.mnemonic = "ADDF";
            break;
        case 0x581:
            result.mnemonic = "SUBF";
            break;
        case 0x582:
            result.mnemonic = "MULF";
            break;
        default:
            result.mnemonic = "FP_OP";
            break;
        }
        break;

    case InstructionSections::SECTION_CONTROL:
        result.instructionFormat = InstructionFormat::FORMAT_BRANCH;
        result.operands = {"ra", "displacement"};
        result.decodedOperands["displacement"] = instruction & 0x1FFFFF; // 21-bit displacement

        // Set mnemonic based on function
        switch (result.function >> 14)
        { // Only using high 2 bits for this example
        case 0:
            result.mnemonic = "BR";
            break;
        case 1:
            result.mnemonic = "BEQ";
            break;
        case 2:
            result.mnemonic = "BNE";
            break;
        default:
            result.mnemonic = "BRANCH";
            break;
        }
        break;

    case InstructionSections::SECTION_PAL:
        result.instructionFormat = InstructionFormat::FORMAT_PAL;
        result.operands = {"palcode"};
        result.decodedOperands["palcode"] = instruction & 0x3FFFFFF; // 26-bit PAL function
        result.mnemonic = "PAL";
        break;

    default:
        result.instructionFormat = InstructionFormat::FORMAT_OPERATE;
        result.operands = {"ra", "rb", "rc"};
        result.mnemonic = "UNKNOWN";
        break;
    }
}