#pragma once

#include <QString>
#include <QtGlobal>

/**
 * @brief Represents a single Alpha instruction word with metadata
 *
 * This class encapsulates a 32-bit Alpha instruction along with its
 * program counter and provides methods for instruction decoding and analysis.
 */
class InstructionWord
{
  public:
    // Constructors
    InstructionWord() : m_instruction(0), m_pc(0), m_valid(false) {}

    InstructionWord(quint32 instruction, quint64 pc = 0) : m_instruction(instruction), m_pc(pc), m_valid(true) {}

    // Copy constructor and assignment
    InstructionWord(const InstructionWord &other) = default;
    InstructionWord &operator=(const InstructionWord &other) = default;

    // Accessors
    quint32 getRawInstruction() const { return m_instruction; }
    quint64 getProgramCounter() const { return m_pc; }
    bool isValid() const { return m_valid; }

    // Setters
    void setInstruction(quint32 instruction)
    {
        m_instruction = instruction;
        m_valid = true;
    }
    void setProgramCounter(quint64 pc) { m_pc = pc; }
    void setValid(bool valid) { m_valid = valid; }

    // Alpha instruction field extraction
    quint8 getOpcode() const { return (m_instruction >> 26) & 0x3F; }                      // Bits 31:26
    quint8 getRa() const { return (m_instruction >> 21) & 0x1F; }                          // Bits 25:21
    quint8 getRb() const { return (m_instruction >> 16) & 0x1F; }                          // Bits 20:16
    quint8 getRc() const { return m_instruction & 0x1F; }                                  // Bits 4:0
    quint8 getFunction() const { return (m_instruction >> 5) & 0x7F; }                     // Bits 11:5
    quint8 getLiteral() const { return (m_instruction >> 13) & 0xFF; }                     // Bits 20:13
    qint16 getDisplacement() const { return static_cast<qint16>(m_instruction & 0xFFFF); } // Bits 15:0
    quint32 getImmediate() const { return m_instruction & 0x1FFFFFF; }                     // Bits 25:0 (PAL)

    // Add these methods for AlphaInstructionCache compatibility
    quint32 getRaw() const { return getRawInstruction(); }
    quint64 getAddress() const { return getProgramCounter(); }

    // Instruction type identification
    bool isPALInstruction() const { return getOpcode() == 0x00; }
    bool isMemoryInstruction() const
    {
        quint8 op = getOpcode();
        return (op >= 0x20 && op <= 0x2F) || (op >= 0x30 && op <= 0x3F);
    }
    bool isBranchInstruction() const
    {
        quint8 op = getOpcode();
        return (op >= 0x30 && op <= 0x3F) || (op == 0x1A);
    }
    bool isFloatingPointInstruction() const
    {
        quint8 op = getOpcode();
        return (op >= 0x14 && op <= 0x17);
    }
    bool isIntegerInstruction() const
    {
        quint8 op = getOpcode();
        return (op >= 0x10 && op <= 0x13);
    }

    // Instruction format identification
    enum class InstructionFormat
    {
        MEMORY,  // Memory format (LDA, LDQ, STQ, etc.)
        BRANCH,  // Branch format (BR, BSR, conditional branches)
        OPERATE, // Operate format (ADD, SUB, etc.)
        PAL,     // PAL format (CALL_PAL)
        UNKNOWN
    };

    InstructionFormat getFormat() const
    {
        if (isPALInstruction())
            return InstructionFormat::PAL;
        if (isBranchInstruction())
            return InstructionFormat::BRANCH;
        if (isMemoryInstruction())
            return InstructionFormat::MEMORY;
        if (isIntegerInstruction() || isFloatingPointInstruction())
            return InstructionFormat::OPERATE;
        return InstructionFormat::UNKNOWN;
    }

    // Utility methods
    QString toString() const
    {
        if (!m_valid)
            return "INVALID";
        return QString("0x%1: 0x%2").arg(m_pc, 16, 16, QChar('0')).arg(m_instruction, 8, 16, QChar('0'));
    }

    QString getDisassembly() const
    {
        if (!m_valid)
            return "INVALID";

        quint8 opcode = getOpcode();
        switch (opcode)
        {
        case 0x00:
            return QString("CALL_PAL 0x%1").arg(getImmediate(), 0, 16);
        case 0x08:
            return "LDA";
        case 0x09:
            return "LDAH";
        case 0x0A:
            return "LDBU";
        case 0x0B:
            return "LDQ_U";
        case 0x0C:
            return "LDWU";
        case 0x0D:
            return "STW";
        case 0x0E:
            return "STB";
        case 0x0F:
            return "STQ_U";
        case 0x10:
            return getIntegerOpName();
        case 0x11:
            return getIntegerOpName();
        case 0x12:
            return getIntegerOpName();
        case 0x13:
            return getIntegerOpName();
        case 0x14:
            return "ITFP";
        case 0x15:
            return "FLTV";
        case 0x16:
            return "FLTI";
        case 0x17:
            return "FLTL";
        case 0x18:
            return getMemoryBarrierName();
        case 0x1A:
            return getJumpName();
        case 0x20:
            return "LDF";
        case 0x21:
            return "LDG";
        case 0x22:
            return "LDS";
        case 0x23:
            return "LDT";
        case 0x24:
            return "STF";
        case 0x25:
            return "STG";
        case 0x26:
            return "STS";
        case 0x27:
            return "STT";
        case 0x28:
            return "LDL";
        case 0x29:
            return "LDQ";
        case 0x2A:
            return "LDL_L";
        case 0x2B:
            return "LDQ_L";
        case 0x2C:
            return "STL";
        case 0x2D:
            return "STQ";
        case 0x2E:
            return "STL_C";
        case 0x2F:
            return "STQ_C";
        case 0x30:
            return "BR";
        case 0x31:
            return "FBEQ";
        case 0x32:
            return "FBLT";
        case 0x33:
            return "FBLE";
        case 0x34:
            return "BSR";
        case 0x35:
            return "FBNE";
        case 0x36:
            return "FBGE";
        case 0x37:
            return "FBGT";
        case 0x38:
            return "BLBC";
        case 0x39:
            return "BEQ";
        case 0x3A:
            return "BLT";
        case 0x3B:
            return "BLE";
        case 0x3C:
            return "BLBS";
        case 0x3D:
            return "BNE";
        case 0x3E:
            return "BGE";
        case 0x3F:
            return "BGT";
        default:
            return QString("UNK_0x%1").arg(opcode, 2, 16, QChar('0'));
        }
    }

    // Comparison operators
    bool operator==(const InstructionWord &other) const
    {
        return m_instruction == other.m_instruction && m_pc == other.m_pc;
    }

    bool operator!=(const InstructionWord &other) const { return !(*this == other); }

    // Hash support for use - QHash is not fully supported using wrapper qHash.  This is an alternate method
    uint qHash() const
    {
        uint h1 = static_cast<uint>(m_instruction);
        uint h2 = static_cast<uint>(m_pc) ^ static_cast<uint>(m_pc >> 32);
        return h1 ^ (h2 << 1);
    }

  private:
    quint32 m_instruction; // 32-bit instruction word
    quint64 m_pc;          // Program counter where this instruction resides
    bool m_valid;          // Whether this instruction word is valid



    // Helper methods for disassembly
    QString getIntegerOpName() const
    {
        quint8 func = getFunction();
        switch (func)
        {
        case 0x00:
            return "ADDL";
        case 0x02:
            return "S4ADDL";
        case 0x09:
            return "SUBL";
        case 0x0B:
            return "S4SUBL";
        case 0x0F:
            return "CMPBGE";
        case 0x12:
            return "S8ADDL";
        case 0x1B:
            return "S8SUBL";
        case 0x1D:
            return "CMPULT";
        case 0x20:
            return "ADDQ";
        case 0x22:
            return "S4ADDQ";
        case 0x29:
            return "SUBQ";
        case 0x2B:
            return "S4SUBQ";
        case 0x2D:
            return "CMPEQ";
        case 0x32:
            return "S8ADDQ";
        case 0x3B:
            return "S8SUBQ";
        case 0x3D:
            return "CMPULE";
        case 0x40:
            return "ADDL/V";
        case 0x49:
            return "SUBL/V";
        case 0x4D:
            return "CMPLT";
        case 0x60:
            return "ADDQ/V";
        case 0x69:
            return "SUBQ/V";
        case 0x6D:
            return "CMPLE";
        default:
            return QString("INTOP_0x%1").arg(func, 2, 16, QChar('0'));
        }
    }

    QString getMemoryBarrierName() const
    {
        quint8 func = getFunction();
        switch (func)
        {
        case 0x0000:
            return "TRAPB";
        case 0x4000:
            return "MB";
        case 0x4400:
            return "WMB";
        case 0x8000:
            return "FETCH";
        case 0xA000:
            return "FETCH_M";
        case 0xC000:
            return "RPCC";
        case 0xE000:
            return "RC";
        case 0xE800:
            return "ECB";
        case 0xF000:
            return "RS";
        case 0xF800:
            return "WH64";
        default:
            return QString("MISC_0x%1").arg(func, 4, 16, QChar('0'));
        }
    }

    QString getJumpName() const
    {
        quint8 func = (m_instruction >> 14) & 0x3;
        switch (func)
        {
        case 0:
            return "JMP";
        case 1:
            return "JSR";
        case 2:
            return "RET";
        case 3:
            return "JSR_COROUTINE";
        default:
            return "JUMP";
        }
    }
    // Hash function for QHash support
    inline uint qHash(const InstructionWord &instruction) { return instruction.qHash(); }
};

