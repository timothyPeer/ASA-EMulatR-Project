#include "decodeStage.h"

#pragma region DecodeStage

// DecodeStage.cpp
#include "AlphaCPU_refactored.h"
#include "AlphaCpuTypes.h"
#include "TraceManager.h"
#include <QMutexLocker>
#include "GlobalMacro.h"

DecodeStage::DecodeStage(AlphaCPU *cpu) : m_cpu(cpu)
{
    // Initialize with invalid instruction
    m_currentInstruction.valid = false;
    DEBUG_LOG("DecodeStage initialized");
}

void DecodeStage::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);

    m_totalInstructions = 0;
    m_memoryInstructions = 0;
    m_branchInstructions = 0;
    m_operateInstructions = 0;
    m_palInstructions = 0;
    m_jumpInstructions = 0;
    m_unknownInstructions = 0;

    DEBUG_LOG("DecodeStage: Statistics cleared");
}

double DecodeStage::getMemoryInstructionRate() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_totalInstructions > 0 ? (double)m_memoryInstructions / m_totalInstructions * 100.0 : 0.0;
}

DecodeStage::DecodedInstruction DecodeStage::decode(quint32 instruction, quint64 pc)
{
    DecodedInstruction decoded;
    decoded.rawInstruction = instruction;

    // Handle NOP instruction (0x47FF041F is the standard Alpha NOP)
    if (instruction == 0x47FF041F)
    {
        decoded.opcode = 0x11;   // Arithmetic/Logic opcode
        decoded.function = 0x20; // BIS function (OR)
        decoded.ra = 31;         // R31 (always zero)
        decoded.rb = 31;         // R31 (always zero)
        decoded.rc = 31;         // R31 (always zero)
        decoded.immediate = 0;
        decoded.valid = true;

        DEBUG_LOG(QString("DecodeStage: NOP instruction at PC=0x%1").arg(pc, 16, 16, QChar('0')));

        m_currentInstruction = decoded;
        return decoded;
    }

    // Extract opcode (bits 31-26)
    decoded.opcode = (instruction >> 26) & 0x3F;

    // Alpha instruction formats:
    // Memory format: opcode(6) + ra(5) + rb(5) + displacement(16)
    // Branch format: opcode(6) + ra(5) + displacement(21)
    // Operate format: opcode(6) + ra(5) + rb(5) + sbz(3) + function(7) + rc(5)

    switch (decoded.opcode)
    {
    // Memory instructions (loads/stores)
    case OPCODE_LDA:   // LDA - Load Address
    case OPCODE_LDAH:  // LDAH - Load Address High
    case OPCODE_LDBU:  // LDBU - Load Byte Unsigned
    case OPCODE_LDQ_U: // LDQ_U - Load Quadword Unaligned
    case OPCODE_LDWU:  // LDWU - Load Word Unsigned
    case OPCODE_STW:   // STW - Store Word
    case OPCODE_STB:   // STB - Store Byte
    case OPCODE_STQ_U: // STQ_U - Store Quadword Unaligned
    case OPCODE_LDF:   // LDF - Load F_floating
    case OPCODE_LDG:   // LDG - Load G_floating
    case OPCODE_LDS:   // LDS - Load S_floating
    case OPCODE_LDT:   // LDT - Load T_floating
    case OPCODE_STF:   // STF - Store F_floating
    case OPCODE_STG:   // STG - Store G_floating
    case OPCODE_STS:   // STS - Store S_floating
    case OPCODE_STT:   // STT - Store T_floating
    case OPCODE_LDL:   // LDL - Load Longword
    case OPCODE_LDQ:   // LDQ - Load Quadword
    case OPCODE_LDL_L: // LDL_L - Load Longword Locked
    case OPCODE_LDQ_L: // LDQ_L - Load Quadword Locked
    case OPCODE_STL:   // STL - Store Longword
    case OPCODE_STQ:   // STQ - Store Quadword
    case OPCODE_STL_C: // STL_C - Store Longword Conditional
    case OPCODE_STQ_C: // STQ_C - Store Quadword Conditional
    {
        // Memory format
        decoded.ra = (instruction >> 21) & 0x1F;
        decoded.rb = (instruction >> 16) & 0x1F;
        decoded.rc = 0;                                                // Not used in memory format
        decoded.function = 0;                                          // Not used in memory format
        decoded.immediate = static_cast<qint16>(instruction & 0xFFFF); // Sign-extend 16-bit displacement

        DEBUG_LOG(QString("DecodeStage: Memory instruction 0x%1 at PC=0x%2 (ra=%3, rb=%4, disp=%5)")
                      .arg(instruction, 8, 16, QChar('0'))
                      .arg(pc, 16, 16, QChar('0'))
                      .arg(decoded.ra)
                      .arg(decoded.rb)
                      .arg(decoded.immediate));
        break;
    }

    // Branch instructions
    case OPCODE_BR:   // BR - Branch
    case OPCODE_FBEQ: // FBEQ - Floating Branch if Equal
    case OPCODE_FBLT: // FBLT - Floating Branch if Less Than
    case OPCODE_FBLE: // FBLE - Floating Branch if Less Than or Equal
    case OPCODE_BSR:  // BSR - Branch to Subroutine
    case OPCODE_FBNE: // FBNE - Floating Branch if Not Equal
    case OPCODE_FBGE: // FBGE - Floating Branch if Greater Than or Equal
    case OPCODE_FBGT: // FBGT - Floating Branch if Greater Than
    case OPCODE_BLBC: // BLBC - Branch if Low Bit Clear
    case OPCODE_BEQ:  // BEQ - Branch if Equal
    case OPCODE_BLT:  // BLT - Branch if Less Than
    case OPCODE_BLE:  // BLE - Branch if Less Than or Equal
    case OPCODE_BLBS: // BLBS - Branch if Low Bit Set
    case OPCODE_BNE:  // BNE - Branch if Not Equal
    case OPCODE_BGE:  // BGE - Branch if Greater Than or Equal
    case OPCODE_BGT:  // BGT - Branch if Greater Than
    {
        // Branch format
        decoded.ra = (instruction >> 21) & 0x1F;
        decoded.rb = 0;       // Not used in branch format
        decoded.rc = 0;       // Not used in branch format
        decoded.function = 0; // Not used in branch format
        // Sign-extend 21-bit displacement and left-shift by 2 (word alignment)
        qint32 disp21 = static_cast<qint32>((instruction & 0x1FFFFF) << 11) >> 11;
        decoded.immediate = disp21 << 2;

        DEBUG_LOG(QString("DecodeStage: Branch instruction 0x%1 at PC=0x%2 (ra=%3, disp=%4)")
                      .arg(instruction, 8, 16, QChar('0'))
                      .arg(pc, 16, 16, QChar('0'))
                      .arg(decoded.ra)
                      .arg(decoded.immediate));
        break;
    }

    // Operate instructions (arithmetic and logical)
    case OPCODE_INTA: // INTA - Integer arithmetic
    case OPCODE_INTL: // INTL - Integer logical
    case OPCODE_INTS: // INTS - Integer shift
    case OPCODE_INTM: // INTM - Integer multiply
    case OPCODE_ITFP: // ITFP - Integer to floating-point
    case OPCODE_FLTV: // FLTV - Floating-point VAX
    case OPCODE_FLTI: // FLTI - Floating-point IEEE
    case OPCODE_FLTL: // FLTL - Floating-point convert
    {
        // Operate format
        decoded.ra = (instruction >> 21) & 0x1F;
        decoded.rb = (instruction >> 16) & 0x1F;
        decoded.rc = (instruction >> 0) & 0x1F;
        decoded.function = (instruction >> 5) & 0x7F;

        // Check if immediate mode (bit 12)
        if (instruction & 0x1000)
        {
            // Immediate mode - 8-bit immediate in bits 20-13
            decoded.immediate = (instruction >> 13) & 0xFF;
            decoded.rb = decoded.immediate; // For operate format, immediate goes in rb field conceptually

            DEBUG_LOG(QString("DecodeStage: Operate instruction 0x%1 at PC=0x%2 (ra=%3, imm=%4, rc=%5, func=0x%6)")
                          .arg(instruction, 8, 16, QChar('0'))
                          .arg(pc, 16, 16, QChar('0'))
                          .arg(decoded.ra)
                          .arg(decoded.immediate)
                          .arg(decoded.rc)
                          .arg(decoded.function, 2, 16, QChar('0')));
        }
        else
        {
            // Register mode
            decoded.immediate = 0;

            DEBUG_LOG(QString("DecodeStage: Operate instruction 0x%1 at PC=0x%2 (ra=%3, rb=%4, rc=%5, func=0x%6)")
                          .arg(instruction, 8, 16, QChar('0'))
                          .arg(pc, 16, 16, QChar('0'))
                          .arg(decoded.ra)
                          .arg(decoded.rb)
                          .arg(decoded.rc)
                          .arg(decoded.function, 2, 16, QChar('0')));
        }
        break;
    }

    // PAL (Privileged Architecture Library) instructions
    case OPCODE_PAL:
    {
        decoded.ra = 0;
        decoded.rb = 0;
        decoded.rc = 0;
        decoded.function = instruction & 0x3FFFFFF; // 26-bit PAL function code
        decoded.immediate = decoded.function;

        DEBUG_LOG(QString("DecodeStage: PAL instruction 0x%1 at PC=0x%2 (func=0x%3)")
                      .arg(instruction, 8, 16, QChar('0'))
                      .arg(pc, 16, 16, QChar('0'))
                      .arg(decoded.function, 6, 16, QChar('0')));
        break;
    }

    // Jump instructions
    case OPCODE_JSR:
    {
        // JMP/JSR/RET/JSR_COROUTINE format
        decoded.ra = (instruction >> 21) & 0x1F;
        decoded.rb = (instruction >> 16) & 0x1F;
        decoded.rc = 0;
        decoded.function = (instruction >> 14) & 0x3; // 2-bit hint
        decoded.immediate = instruction & 0x3FFF;     // 14-bit displacement

        DEBUG_LOG(QString("DecodeStage: Jump instruction 0x%1 at PC=0x%2 (ra=%3, rb=%4, hint=%5)")
                      .arg(instruction, 8, 16, QChar('0'))
                      .arg(pc, 16, 16, QChar('0'))
                      .arg(decoded.ra)
                      .arg(decoded.rb)
                      .arg(decoded.function));
        break;
    }

    default:
    {
        // Unknown/unsupported instruction
        DEBUG_LOG(QString("DecodeStage: Unknown instruction 0x%1 at PC=0x%2 (opcode=0x%3)")
                      .arg(instruction, 8, 16, QChar('0'))
                      .arg(pc, 16, 16, QChar('0'))
                      .arg(decoded.opcode, 2, 16, QChar('0')));

        // Mark as valid but with special opcode to indicate unknown
        decoded.valid = false;
        m_currentInstruction = decoded;
        return decoded;
    }
    }

    decoded.valid = true;
    m_currentInstruction = decoded;
    return decoded;
}

void DecodeStage::flush()
{
    m_currentInstruction.valid = false;
    m_currentInstruction.rawInstruction = 0;
    m_currentInstruction.opcode = 0;
    m_currentInstruction.function = 0;
    m_currentInstruction.ra = 0;
    m_currentInstruction.rb = 0;
    m_currentInstruction.rc = 0;
    m_currentInstruction.immediate = 0;

    DEBUG_LOG("DecodeStage: Pipeline flushed");
}

void DecodeStage::printStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    if (m_totalInstructions == 0)
    {
        DEBUG_LOG("DecodeStage: No instructions decoded yet");
        return;
    }

    double memoryRate = (double)m_memoryInstructions / m_totalInstructions * 100.0;
    double branchRate = (double)m_branchInstructions / m_totalInstructions * 100.0;
    double operateRate = (double)m_operateInstructions / m_totalInstructions * 100.0;
    double palRate = (double)m_palInstructions / m_totalInstructions * 100.0;
    double jumpRate = (double)m_jumpInstructions / m_totalInstructions * 100.0;
    double unknownRate = (double)m_unknownInstructions / m_totalInstructions * 100.0;

    DEBUG_LOG("DecodeStage Statistics:");
    DEBUG_LOG(QString("  Total Instructions: %1").arg(m_totalInstructions));
    DEBUG_LOG(QString("  Memory Instructions: %1 (%2%)").arg(m_memoryInstructions).arg(memoryRate, 0, 'f', 2));
    DEBUG_LOG(QString("  Branch Instructions: %1 (%2%)").arg(m_branchInstructions).arg(branchRate, 0, 'f', 2));
    DEBUG_LOG(QString("  Operate Instructions: %1 (%2%)").arg(m_operateInstructions).arg(operateRate, 0, 'f', 2));
    DEBUG_LOG(QString("  PAL Instructions: %1 (%2%)").arg(m_palInstructions).arg(palRate, 0, 'f', 2));
    DEBUG_LOG(QString("  Jump Instructions: %1 (%2%)").arg(m_jumpInstructions).arg(jumpRate, 0, 'f', 2));
    DEBUG_LOG(QString("  Unknown Instructions: %1 (%2%)").arg(m_unknownInstructions).arg(unknownRate, 0, 'f', 2));
}

void DecodeStage::updateStatistics(InstructionType type)
{
    QMutexLocker locker(&m_statsMutex);

    m_totalInstructions++;

    switch (type)
    {
    case MEMORY:
        m_memoryInstructions++;
        break;
    case BRANCH:
        m_branchInstructions++;
        break;
    case OPERATE:
        m_operateInstructions++;
        break;
    case PAL:
        m_palInstructions++;
        break;
    case JUMP:
        m_jumpInstructions++;
        break;
    case UNKNOWN:
        m_unknownInstructions++;
        break;
    default:
        break;
    }
}

DecodeStage::InstructionType DecodeStage::getInstructionType(quint32 opcode) const
{
    // Memory instructions
    if ((opcode >= OPCODE_LDA && opcode <= OPCODE_STQ_U) || (opcode >= OPCODE_LDF && opcode <= OPCODE_STQ_C))
    {
        return MEMORY;
    }

    // Branch instructions
    if (opcode >= OPCODE_BR && opcode <= OPCODE_BGT)
    {
        return BRANCH;
    }

    // Operate instructions
    if (opcode >= OPCODE_INTA && opcode <= OPCODE_FLTL)
    {
        return OPERATE;
    }

    // PAL instruction
    if (opcode == OPCODE_PAL)
    {
        return PAL;
    }

    // Jump instruction
    if (opcode == OPCODE_JSR)
    {
        return JUMP;
    }

    return UNKNOWN;
}

QString DecodeStage::getInstructionMnemonic(const DecodedInstruction &instruction) const
{
    switch (instruction.opcode)
    {
    // Memory instructions
    case OPCODE_LDA:
        return "LDA";
    case OPCODE_LDAH:
        return "LDAH";
    case OPCODE_LDBU:
        return "LDBU";
    case OPCODE_LDQ_U:
        return "LDQ_U";
    case OPCODE_LDWU:
        return "LDWU";
    case OPCODE_STW:
        return "STW";
    case OPCODE_STB:
        return "STB";
    case OPCODE_STQ_U:
        return "STQ_U";
    case OPCODE_LDF:
        return "LDF";
    case OPCODE_LDG:
        return "LDG";
    case OPCODE_LDS:
        return "LDS";
    case OPCODE_LDT:
        return "LDT";
    case OPCODE_STF:
        return "STF";
    case OPCODE_STG:
        return "STG";
    case OPCODE_STS:
        return "STS";
    case OPCODE_STT:
        return "STT";
    case OPCODE_LDL:
        return "LDL";
    case OPCODE_LDQ:
        return "LDQ";
    case OPCODE_LDL_L:
        return "LDL_L";
    case OPCODE_LDQ_L:
        return "LDQ_L";
    case OPCODE_STL:
        return "STL";
    case OPCODE_STQ:
        return "STQ";
    case OPCODE_STL_C:
        return "STL_C";
    case OPCODE_STQ_C:
        return "STQ_C";

    // Branch instructions
    case OPCODE_BR:
        return "BR";
    case OPCODE_FBEQ:
        return "FBEQ";
    case OPCODE_FBLT:
        return "FBLT";
    case OPCODE_FBLE:
        return "FBLE";
    case OPCODE_BSR:
        return "BSR";
    case OPCODE_FBNE:
        return "FBNE";
    case OPCODE_FBGE:
        return "FBGE";
    case OPCODE_FBGT:
        return "FBGT";
    case OPCODE_BLBC:
        return "BLBC";
    case OPCODE_BEQ:
        return "BEQ";
    case OPCODE_BLT:
        return "BLT";
    case OPCODE_BLE:
        return "BLE";
    case OPCODE_BLBS:
        return "BLBS";
    case OPCODE_BNE:
        return "BNE";
    case OPCODE_BGE:
        return "BGE";
    case OPCODE_BGT:
        return "BGT";
    case OPCODE_INTA:
        return "INTA";
    case OPCODE_INTL:
        return "INTL";
    case OPCODE_INTS:
        return "INTS";
    case OPCODE_INTM:
        return "INTM";
    case OPCODE_ITFP:
        return "ITFP";
    case OPCODE_FLTV:
        return "FLTV";
    case OPCODE_FLTI:
        return "FLTI";
    case OPCODE_FLTL:
        return "FLTL";

    // Operate instructions (subset)
    case 0x10:
    {
        switch (instruction.function)
        {
        case FUNC_ADDL:
            return "ADDL";
        case FUNC_S4ADDL:
            return "S4ADDL";
        case FUNC_SUBL:
            return "SUBL";
        case FUNC_S4SUBL:
            return "S4SUBL";
        case FUNC_CMPBGE:
            return "CMPBGE";
        case FUNC_S8ADDL:
            return "S8ADDL";
        case FUNC_S8SUBL:
            return "S8SUBL";
        case FUNC_CMPULE_L:
            return "CMPULE_L";
        case FUNC_ADDQ:
            return "ADDQ";
        case FUNC_S4ADDQ:
            return "S4ADDQ";
        case FUNC_SUBQ:
            return "SUBQ";
        case FUNC_S4SUBQ:
            return "S4SUBQ";
        case FUNC_CMPEQ:
            return "CMPEQ";
        case FUNC_CMPNE:    // Synthesized as (CMPEQ XOR)
            return "CMPNE"; // *** ADDED ***
        case FUNC_S8ADDQ:
            return "S8ADDQ";
        case FUNC_S8SUBQ:
            return "S8SUBQ";
        case FUNC_CMPULT_L:
            return "CMPULT_L";
        case FUNC_CMPULT_G:
            return "CMPULT_G";
        case FUNC_CMPGEQ:
            return "CMPGE"; // *** ADDED ***
        case FUNC_ADDLV:
            return "ADDL/V";
        case FUNC_S4ADDLV:
            return "S4ADDL/V"; // *** ADDED ***
        case FUNC_SUBLV:
            return "SUBL/V"; // *** ADDED ***
        case FUNC_S4SUBLV:
            return "S4SUBL/V"; // *** ADDED ***
        case FUNC_CMPULE_G:
            return "CMPULE"; // *** UPDATED ***
        case FUNC_S8ADDLV:
            return "S8ADDL/V"; // *** ADDED ***
        case FUNC_S8SUBLV:
            return "S8SUBL/V"; // *** ADDED ***
        case FUNC_ADDQV:
            return "ADDQ/V"; // *** ADDED ***
        case FUNC_S4ADDQV:
            return "S4ADDQ/V"; // *** ADDED ***
        case FUNC_SUBQV:
            return "SUBQ/V"; // *** ADDED ***
        case 0x6D:
            return "CMPUGT"; // *** ADDED ***
        case 0x6F:
            return "CMPUGE"; // *** ADDED ***
        case FUNC_S8ADDQV:
            return "S8ADDQ/V"; // *** ADDED ***
        case FUNC_S8SUBQV:
            return "S8SUBQ/V"; // *** ADDED ***
        default:
            return QString("INTA_0x%1").arg(instruction.function, 2, 16, QChar('0'));
        }
    }
    case 0x11:
    {
        switch (instruction.function)
        {
        case FUNC_AND:
            return "AND";
        case FUNC_BIC:
            return "BIC";
        case FUNC_CMOVLBS:
            return "CMOVLBS";
        case FUNC_CMOVLBC:
            return "CMOVLBC";
        case FUNC_BIS:
            return "BIS"; // OR
        case FUNC_CMOVEQ:
            return "CMOVEQ";
        case FUNC_CMOVNE:
            return "CMOVNE";
        case FUNC_ORNOT:
            return "ORNOT";
        case FUNC_XOR:
            return "XOR";
        case FUNC_CMOVLT:
            return "CMOVLT";
        case FUNC_CMOVGE:
            return "CMOVGE";
        case FUNC_EQV:
            return "EQV";
        case FUNC_AMASK:
            return "AMASK";
        case FUNC_CMOVLE:
            return "CMOVLE";
        case FUNC_CMOVGT:
            return "CMOVGT";
        case 0x6C:
            return "IMPLVER";
        default:
            return QString("INTL_0x%1").arg(instruction.function, 2, 16, QChar('0'));
        }
    }

    // PAL
    case OPCODE_PAL:
        return QString("PAL_0x%1").arg(instruction.function, 6, 16, QChar('0'));

    // Jump
    case OPCODE_JSR:
    {
        switch (instruction.function)
        {
        case FUNC_JMP:
            return "JMP";
        case FUNC_JSR:
            return "JSR";
        case FUNC_RET:
            return "RET";
        case FUNC_JSR_COROUTINE:
            return "JSR_COROUTINE";
        default:
            return QString("JUMP_%1").arg(instruction.function);
        }
    }

    default:
        return QString("UNK_0x%1").arg(instruction.opcode, 2, 16, QChar('0'));
    }
}



#pragma endregion DecodeStage