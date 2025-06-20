#include "decodedInstruction.h"



 DecodedInstruction::DecodedInstruction()
    : raw(0), rawInstruction(0), pc(0), opcode(0), ra(0), rb(0), rc(0), function(0), literal(0), displacement(0),
      memoryDisplacement(0), immediate(0), isMemoryInstruction(false), isBranchInstruction(false),
      isFloatingPoint(false), isPALInstruction(false), isPrivileged(false)
{
}

DecodedInstruction::DecodedInstruction(quint32 instruction, quint64 programCounter /*= 0*/)
    : raw(instruction), rawInstruction(instruction), pc(programCounter)
{
    // Decode Alpha instruction format
    opcode = (instruction >> 26) & 0x3F;                      // Bits 31:26
    ra = (instruction >> 21) & 0x1F;                          // Bits 25:21
    rb = (instruction >> 16) & 0x1F;                          // Bits 20:16
    rc = instruction & 0x1F;                                  // Bits 4:0
    function = (instruction >> 5) & 0x7F;                     // Bits 11:5 (for operate format)
    literal = (instruction >> 13) & 0xFF;                     // Bits 20:13 (for literal format)
    displacement = static_cast<qint16>(instruction & 0xFFFF); // Bits 15:0
    memoryDisplacement = displacement;
    immediate = instruction & 0x1FFFFFF; // Bits 25:0 (for PAL format)

    // Set instruction type flags
    isPALInstruction = (opcode == 0x00);                        // OpCode 0 = PAL
    isPrivileged = isPALInstruction;                            // PAL instructions are privileged
    isMemoryInstruction = (opcode >= 0x20 && opcode <= 0x2F) || // LDF, LDG, LDS, LDT, LDL, LDQ, etc.
                          (opcode >= 0x30 && opcode <= 0x3F);   // STF, STG, STS, STT, STL, STQ, etc.
    isBranchInstruction = (opcode >= 0x30 && opcode <= 0x3F) || // Branch opcodes
                          (opcode == 0x1A);                     // JMP format
    isFloatingPoint = (opcode >= 0x14 && opcode <= 0x17);       // INTS, ITFP, FLTV, FLTI
}
