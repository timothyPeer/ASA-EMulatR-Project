#ifndef ALPHAINSTRUCTIONBASE_H
#define ALPHAINSTRUCTIONBASE_H

#include "globalmacro.h"
#include "utilitySafeIncrement.h"
#include <atomic>
#include <cstdint>
#include <cmath>

class alphaMemorySystem;
class alphaCacheHierarchy;

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaInstructionBase
{
  public:
    explicit alphaInstructionBase(uint32_t opcode)
        : m_opcode(opcode), m_executionCount(0), m_cycleCount(0), m_isValid(true)
    {
        DEBUG_LOG("alphaInstructionBase constructed with opcode: 0x%08X", opcode);
    }

    virtual ~alphaInstructionBase() = default;

    // Core execution interface
    virtual bool execute() = 0;
    virtual void decode() = 0;
    virtual uint32_t getCycleLatency() const = 0;

    // Performance-critical accessors (inline)
    inline uint32_t getOpcode() const { return m_opcode; }
    inline bool isValid() const { return m_isValid; }
    inline uint64_t getExecutionCount() const { return m_executionCount.load(std::memory_order_relaxed); }
    inline uint64_t getCycleCount() const { return m_cycleCount.load(std::memory_order_relaxed); }

    // Hot path performance tracking
    inline void incrementExecutionCount() { m_executionCount.fetch_add(1, std::memory_order_relaxed); }

    inline void addCycles(uint32_t cycles) { m_cycleCount.fetch_add(cycles, std::memory_order_relaxed); }

    // Instruction classification
    virtual bool isBranch() const { return false; }
    virtual bool isMemoryOperation() const { return false; }
    virtual bool isFloatingPoint() const { return false; }
    inline uint32_t getCycleLatency() const
    {
        // Default implementation - derived classes should override
        return 1;
    }

  protected:
    void invalidate() { m_isValid = false; }

  private:
    const uint32_t m_opcode;
    std::atomic<uint64_t> m_executionCount;
    std::atomic<uint64_t> m_cycleCount;
    bool m_isValid;

    // Prevent copying for performance
    alphaInstructionBase(const alphaInstructionBase &) = delete;
    alphaInstructionBase &operator=(const alphaInstructionBase &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaIntegerInstruction : public alphaInstructionBase
{
  public:
    enum class IntegerOpType : uint8_t
    {
        ADD = 0x10,
        SUB = 0x11,
        MUL = 0x12,
        DIV = 0x13,
        AND = 0x20,
        OR = 0x21,
        XOR = 0x22,
        SHL = 0x30,
        SHR = 0x31,
        CMP = 0x40
    };

    explicit alphaIntegerInstruction(uint32_t opcode, IntegerOpType opType, uint8_t destReg, uint8_t srcReg1,
                                     uint8_t srcReg2)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_immediate(0), m_useImmediate(false), m_result(0), m_overflowCount(0)
    {
        DEBUG_LOG("alphaIntegerInstruction created - OpType: %d, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2);
    }

    // Immediate mode constructor
    explicit alphaIntegerInstruction(uint32_t opcode, IntegerOpType opType, uint8_t destReg, uint8_t srcReg1,
                                     int16_t immediate)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(0),
          m_immediate(immediate), m_useImmediate(true), m_result(0), m_overflowCount(0)
    {
        DEBUG_LOG("alphaIntegerInstruction created (immediate) - OpType: %d, Dest: R%d, Src: R%d, Imm: %d",
                  static_cast<int>(opType), destReg, srcReg1, immediate);
    }

    virtual ~alphaIntegerInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding integer instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields based on Alpha operate format
        // Bits 31-26: Primary opcode
        // Bits 25-21: Ra (source register 1)
        // Bits 20-16: Rb (source register 2) or literal
        // Bits 15-13: SBZ (should be zero)
        // Bit 12: IsLit (literal mode)
        // Bits 11-5: Function code
        // Bits 4-0: Rc (destination register)

        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        bool isLiteral = (opcode >> 12) & 0x1;
        uint8_t function = (opcode >> 5) & 0x7F;
        uint8_t rc = opcode & 0x1F;

        // Set instruction operands
        m_srcReg1 = ra;
        if (isLiteral)
        {
            m_immediate = rb; // In literal mode, Rb field contains 8-bit literal
            m_useImmediate = true;
            m_srcReg2 = 0;
        }
        else
        {
            m_srcReg2 = rb;
            m_useImmediate = false;
        }
        m_destReg = rc;

        // Determine operation type based on primary opcode and function
        switch (primaryOpcode)
        {
        case 0x10: // Integer arithmetic operations
            switch (function)
            {
            case 0x00:
                m_opType = IntegerOpType::ADD;
                break;
            case 0x09:
                m_opType = IntegerOpType::SUB;
                break;
            case 0x20:
                m_opType = IntegerOpType::MUL;
                break;
            case 0x30:
                m_opType = IntegerOpType::DIV;
                break;
            default:
                DEBUG_LOG("Unknown integer arithmetic function: 0x%02X", function);
                m_opType = IntegerOpType::ADD; // Default fallback
                break;
            }
            break;

        case 0x11: // Logical operations
            switch (function)
            {
            case 0x00:
                m_opType = IntegerOpType::AND;
                break;
            case 0x20:
                m_opType = IntegerOpType::OR;
                break;
            case 0x40:
                m_opType = IntegerOpType::XOR;
                break;
            default:
                DEBUG_LOG("Unknown logical function: 0x%02X", function);
                m_opType = IntegerOpType::AND; // Default fallback
                break;
            }
            break;

        case 0x12: // Shift operations
            switch (function)
            {
            case 0x39:
                m_opType = IntegerOpType::SHL;
                break;
            case 0x34:
                m_opType = IntegerOpType::SHR;
                break;
            default:
                DEBUG_LOG("Unknown shift function: 0x%02X", function);
                m_opType = IntegerOpType::SHL; // Default fallback
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown integer primary opcode: 0x%02X", primaryOpcode);
            m_opType = IntegerOpType::ADD; // Default fallback
            break;
        }

        DEBUG_LOG("Integer instruction decoded - Type: %d, Dest: R%d, Src1: R%d, Src2: R%d, Literal: %s",
                  static_cast<int>(m_opType), m_destReg, m_srcReg1, m_srcReg2, m_useImmediate ? "Yes" : "No");
    }


    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        case IntegerOpType::ADD:
        case IntegerOpType::SUB:
        case IntegerOpType::AND:
        case IntegerOpType::OR:
        case IntegerOpType::XOR:
            return 1;
        case IntegerOpType::SHL:
        case IntegerOpType::SHR:
        case IntegerOpType::CMP:
            return 1;
        case IntegerOpType::MUL:
            return 3;
        case IntegerOpType::DIV:
            return 23;
        default:
            return 1;
        }
    }

    // Performance-critical accessors
    inline IntegerOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg1() const { return m_srcReg1; }
    inline uint8_t getSrcReg2() const { return m_srcReg2; }
    inline int16_t getImmediate() const { return m_immediate; }
    inline bool usesImmediate() const { return m_useImmediate; }
    inline int64_t getResult() const { return m_result; }
    inline uint64_t getOverflowCount() const { return m_overflowCount.load(std::memory_order_relaxed); }

    // Hot path execution support
    inline void setOperands(int64_t op1, int64_t op2)
    {
        m_operand1 = op1;
        m_operand2 = op2;
    }

    inline void setOperand1(int64_t op1) { m_operand1 = op1; }
    inline void setOperand2(int64_t op2) { m_operand2 = op2; }

  private:
    bool performOperation()
    {
        int64_t op2 = m_useImmediate ? static_cast<int64_t>(m_immediate) : m_operand2;

        switch (m_opType)
        {
        case IntegerOpType::ADD:
            m_result = m_operand1 + op2;
            checkOverflow(m_operand1, op2, m_result);
            break;
        case IntegerOpType::SUB:
            m_result = m_operand1 - op2;
            checkOverflow(m_operand1, -op2, m_result);
            break;
        case IntegerOpType::MUL:
            m_result = m_operand1 * op2;
            break;
        case IntegerOpType::DIV:
            if (op2 == 0)
            {
                DEBUG_LOG("Division by zero in integer instruction");
                return false;
            }
            m_result = m_operand1 / op2;
            break;
        case IntegerOpType::AND:
            m_result = m_operand1 & op2;
            break;
        case IntegerOpType::OR:
            m_result = m_operand1 | op2;
            break;
        case IntegerOpType::XOR:
            m_result = m_operand1 ^ op2;
            break;
        case IntegerOpType::SHL:
            m_result = m_operand1 << (op2 & 0x3F);
            break;
        case IntegerOpType::SHR:
            m_result = m_operand1 >> (op2 & 0x3F);
            break;
        case IntegerOpType::CMP:
            m_result = (m_operand1 < op2) ? -1 : ((m_operand1 > op2) ? 1 : 0);
            break;
        default:
            return false;
        }

        return true;
    }

    inline void checkOverflow(int64_t a, int64_t b, int64_t result)
    {
        if (((a > 0) && (b > 0) && (result < 0)) || ((a < 0) && (b < 0) && (result > 0)))
        {
            m_overflowCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

  private:
     IntegerOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg1;
     uint8_t m_srcReg2;
     int16_t m_immediate;
     bool m_useImmediate;

    int64_t m_operand1 = 0;
    int64_t m_operand2 = 0;
    int64_t m_result;
    std::atomic<uint64_t> m_overflowCount;

    // Prevent copying for performance
    alphaIntegerInstruction(const alphaIntegerInstruction &) = delete;
    alphaIntegerInstruction &operator=(const alphaIntegerInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif


#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaMemoryInstruction : public alphaInstructionBase
{
  public:
    enum class MemoryOpType : uint8_t
    {
        // Load instructions
        LDB = 0x0A,  // Load byte
		LDBU = 0x0B, // Load byte unsigned
	    LDW = 0x0C,  // Load word (16-bit)
		LDWU = 0x0D, // Load word unsigned
 
        LDL = 0x28,  // Load longword (32-bit)
		LDL_L = 0X2A, // Load longword (sign-extended)
        LDQ = 0x29,  // Load quadword (64-bit)
		LDQ_L = 0x2B, // Load quadword (locked)
		LDQ_U = 0x0B, // Load quadword (unaligned)
        LDA = 0x08,  // Load address
        LDAH = 0x09, // Load address high

        // Store instructions
        STB = 0x0E, // Store byte
        STW = 0x0F, // Store word (16-bit)
        STL = 0x2C, // Store longword (32-bit)
        STQ = 0x2D, // Store quadword (64-bit)

        // Prefetch instructions
        PREFETCH = 0xF0, // Prefetch data

        // Memory barrier
        MB = 0xF8, // Memory barrier
        WMB = 0xF9 // Write memory barrier
    };

    enum class AddressingMode : uint8_t
    {
        REGISTER_OFFSET,  // Ra + displacement
        REGISTER_INDEXED, // Ra + Rb
        IMMEDIATE         // Direct address
    };

    explicit alphaMemoryInstruction(uint32_t opcode, MemoryOpType opType, uint8_t dataReg, uint8_t baseReg,
                                    int16_t displacement)
        : alphaInstructionBase(opcode), m_opType(opType), m_dataReg(dataReg), m_baseReg(baseReg), m_indexReg(0),
          m_displacement(displacement), m_addressingMode(AddressingMode::REGISTER_OFFSET), m_effectiveAddress(0),
          m_dataValue(0), m_cacheHitCount(0), m_cacheMissCount(0), m_tlbHitCount(0), m_tlbMissCount(0)
    {
        DEBUG_LOG("alphaMemoryInstruction created - OpType: 0x%02X, DataReg: R%d, BaseReg: R%d, Disp: %d",
                  static_cast<int>(opType), dataReg, baseReg, displacement);
    }

    // Indexed addressing constructor
    explicit alphaMemoryInstruction(uint32_t opcode, MemoryOpType opType, uint8_t dataReg, uint8_t baseReg,
                                    uint8_t indexReg)
        : alphaInstructionBase(opcode), m_opType(opType), m_dataReg(dataReg), m_baseReg(baseReg), m_indexReg(indexReg),
          m_displacement(0), m_addressingMode(AddressingMode::REGISTER_INDEXED), m_effectiveAddress(0), m_dataValue(0),
          m_cacheHitCount(0), m_cacheMissCount(0), m_tlbHitCount(0), m_tlbMissCount(0)
    {
        DEBUG_LOG(
            "alphaMemoryInstruction created (indexed) - OpType: 0x%02X, DataReg: R%d, BaseReg: R%d, IndexReg: R%d",
            static_cast<int>(opType), dataReg, baseReg, indexReg);
    }

    virtual ~alphaMemoryInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        if (!calculateEffectiveAddress())
        {
            return false;
        }

        bool success = performMemoryOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding memory instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields based on Alpha memory format
        // Bits 31-26: Primary opcode
        // Bits 25-21: Ra (data register)
        // Bits 20-16: Rb (base register)
        // Bits 15-0: Displacement (16-bit signed)

        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        int16_t displacement = static_cast<int16_t>(opcode & 0xFFFF);

        m_dataReg = ra;
        m_baseReg = rb;
        m_displacement = displacement;
        m_addressingMode = AddressingMode::REGISTER_OFFSET;

        // Determine operation type based on primary opcode
        switch (primaryOpcode)
        {
        case 0x08:
            m_opType = MemoryOpType::LDA;
            break;
        case 0x09:
            m_opType = MemoryOpType::LDAH;
            break;
        case 0x0A:
            m_opType = MemoryOpType::LDB;
            break;
        case 0x0A:
            m_opType = MemoryOpType::LDBU;
            break;
        case 0x0C:
            m_opType = MemoryOpType::LDW;
            break;
        case 0x0D:
            m_opType = MemoryOpType::LDWU;
            break;
        case 0x0E:
            m_opType = MemoryOpType::STB;
            break;
        case 0x0F:
            m_opType = MemoryOpType::STW;
            break;
        case 0x28:
            m_opType = MemoryOpType::LDL;
            break;
        case 0x29:
            m_opType = MemoryOpType::LDQ;
            break;
        case 0x2C:
            m_opType = MemoryOpType::STL;
            break;
        case 0x2D:
            m_opType = MemoryOpType::STQ;
            break;
        case 0xF0:
            m_opType = MemoryOpType::PREFETCH;
            break;
        case 0xF8:
            m_opType = MemoryOpType::MB;
            break;
        case 0xF9:
            m_opType = MemoryOpType::WMB;
            break;
        default:
            DEBUG_LOG("Unknown memory opcode: 0x%02X", primaryOpcode);
            m_opType = MemoryOpType::LDQ; // Default fallback
            break;
        }

        DEBUG_LOG("Memory instruction decoded - Type: 0x%02X, DataReg: R%d, BaseReg: R%d, Disp: %d",
                  static_cast<int>(m_opType), m_dataReg, m_baseReg, m_displacement);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        case MemoryOpType::LDA:
        case MemoryOpType::LDAH:
            return 1; // Address calculation only
        case MemoryOpType::LDB:
        case MemoryOpType::LDW:
        case MemoryOpType::LDL:
        case MemoryOpType::LDQ:
        case MemoryOpType::LDBU:
        case MemoryOpType::LDWU:
            return 3; // Load latency (cache hit)
        case MemoryOpType::STB:
        case MemoryOpType::STW:
        case MemoryOpType::STL:
        case MemoryOpType::STQ:
            return 1; // Store latency
        case MemoryOpType::PREFETCH:
            return 1; // Prefetch is async
        case MemoryOpType::MB:
        case MemoryOpType::WMB:
            return 10; // Memory barrier overhead
        default:
            return 3;
        }
    }

    // Memory operation classification
    bool isMemoryOperation() const override { return true; }

    inline bool isLoad() const { return (m_opType >= MemoryOpType::LDB && m_opType <= MemoryOpType::LDAH); }

    inline bool isStore() const { return (m_opType >= MemoryOpType::STB && m_opType <= MemoryOpType::STQ); }

    // Performance-critical accessors
    inline MemoryOpType getOpType() const { return m_opType; }
    inline uint8_t getDataReg() const { return m_dataReg; }
    inline uint8_t getBaseReg() const { return m_baseReg; }
    inline uint8_t getIndexReg() const { return m_indexReg; }
    inline int16_t getDisplacement() const { return m_displacement; }
    inline AddressingMode getAddressingMode() const { return m_addressingMode; }
    inline uint64_t getEffectiveAddress() const { return m_effectiveAddress; }
    inline uint64_t getDataValue() const { return m_dataValue; }

    // Performance counters
    inline uint64_t getCacheHitCount() const { return m_cacheHitCount.load(std::memory_order_relaxed); }
    inline uint64_t getCacheMissCount() const { return m_cacheMissCount.load(std::memory_order_relaxed); }
    inline uint64_t getTlbHitCount() const { return m_tlbHitCount.load(std::memory_order_relaxed); }
    inline uint64_t getTlbMissCount() const { return m_tlbMissCount.load(std::memory_order_relaxed); }

    // Hot path execution support
    inline void setBaseAddress(uint64_t baseAddr) { m_baseAddress = baseAddr; }
    inline void setIndexValue(uint64_t indexVal) { m_indexValue = indexVal; }
    inline void setDataValue(uint64_t dataVal) { m_dataValue = dataVal; }

    // Memory access result tracking
    inline void recordCacheHit() { m_cacheHitCount.fetch_add(1, std::memory_order_relaxed); }
    inline void recordCacheMiss() { m_cacheMissCount.fetch_add(1, std::memory_order_relaxed); }
    inline void recordTlbHit() { m_tlbHitCount.fetch_add(1, std::memory_order_relaxed); }
    inline void recordTlbMiss() { m_tlbMissCount.fetch_add(1, std::memory_order_relaxed); }

    // Memory access size
    inline uint32_t getAccessSize() const
    {
        switch (m_opType)
        {
        case MemoryOpType::LDB:
        case MemoryOpType::LDBU:
        case MemoryOpType::STB:
            return 1;
        case MemoryOpType::LDW:
        case MemoryOpType::LDWU:
        case MemoryOpType::STW:
            return 2;
        case MemoryOpType::LDL:
        case MemoryOpType::STL:
            return 4;
        case MemoryOpType::LDQ:
        case MemoryOpType::STQ:
        case MemoryOpType::LDA:
        case MemoryOpType::LDAH:
            return 8;
        default:
            return 0;
        }
    }

  private:
    bool calculateEffectiveAddress()
    {
        switch (m_addressingMode)
        {
        case AddressingMode::REGISTER_OFFSET:
            m_effectiveAddress = m_baseAddress + static_cast<int64_t>(m_displacement);
            break;
        case AddressingMode::REGISTER_INDEXED:
            m_effectiveAddress = m_baseAddress + m_indexValue;
            break;
        case AddressingMode::IMMEDIATE:
            m_effectiveAddress = static_cast<uint64_t>(m_displacement);
            break;
        default:
            return false;
        }

        // Address alignment check
        uint32_t accessSize = getAccessSize();
        if (accessSize > 1 && (m_effectiveAddress % accessSize) != 0)
        {
            DEBUG_LOG("Memory alignment fault: addr=0x%016llX, size=%d", m_effectiveAddress, accessSize);
            return false;
        }

        return true;
    }

    bool performMemoryOperation()
    {
        switch (m_opType)
        {
        case MemoryOpType::LDA:
            m_dataValue = m_effectiveAddress;
            return true;
        case MemoryOpType::LDAH:
            m_dataValue = m_effectiveAddress & 0xFFFF0000ULL;
            return true;
        case MemoryOpType::LDB:
        case MemoryOpType::LDW:
        case MemoryOpType::LDL:
        case MemoryOpType::LDQ:
        case MemoryOpType::LDBU:
        case MemoryOpType::LDWU:
            return performLoad();
        case MemoryOpType::STB:
        case MemoryOpType::STW:
        case MemoryOpType::STL:
        case MemoryOpType::STQ:
            return performStore();
        case MemoryOpType::PREFETCH:
            // Prefetch hint - always succeeds
            return true;
        case MemoryOpType::MB:
        case MemoryOpType::WMB:
            // Memory barrier - always succeeds
            return true;
        default:
            return false;
        }
    }

    bool performLoad()
    {
        // Simulate memory access - actual implementation would interface with memory system
        DEBUG_LOG("Load operation: addr=0x%016llX, size=%d", m_effectiveAddress, getAccessSize());

        // Sign extension for signed loads
        switch (m_opType)
        {
        case MemoryOpType::LDB:
            // Sign extend byte to 64-bit
            m_dataValue = static_cast<int64_t>(static_cast<int8_t>(m_dataValue & 0xFF));
            break;
        case MemoryOpType::LDW:
            // Sign extend word to 64-bit
            m_dataValue = static_cast<int64_t>(static_cast<int16_t>(m_dataValue & 0xFFFF));
            break;
        case MemoryOpType::LDL:
            // Sign extend longword to 64-bit
            m_dataValue = static_cast<int64_t>(static_cast<int32_t>(m_dataValue & 0xFFFFFFFF));
            break;
        default:
            break;
        }

        return true;
    }

    bool performStore()
    {
        DEBUG_LOG("Store operation: addr=0x%016llX, size=%d, data=0x%016llX", m_effectiveAddress, getAccessSize(),
                  m_dataValue);
        return true;
    }

  private:
     MemoryOpType m_opType;
     uint8_t m_dataReg;
     uint8_t m_baseReg;
     uint8_t m_indexReg;
     int16_t m_displacement;
     AddressingMode m_addressingMode;

    uint64_t m_baseAddress = 0;
    uint64_t m_indexValue = 0;
    uint64_t m_effectiveAddress;
    uint64_t m_dataValue;

    std::atomic<uint64_t> m_cacheHitCount;
    std::atomic<uint64_t> m_cacheMissCount;
    std::atomic<uint64_t> m_tlbHitCount;
    std::atomic<uint64_t> m_tlbMissCount;

    // Prevent copying for performance
    alphaMemoryInstruction(const alphaMemoryInstruction &) = delete;
    alphaMemoryInstruction &operator=(const alphaMemoryInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaFloatingPointInstruction : public alphaInstructionBase
{
  public:
    enum class FloatingPointOpType : uint8_t
    {
        // Single precision arithmetic (IEEE 754)
        ADDS = 0x80,  // Add single
        SUBS = 0x81,  // Subtract single
        MULS = 0x82,  // Multiply single
        DIVS = 0x83,  // Divide single
        SQRTS = 0x8B, // Square root single

        // Double precision arithmetic (IEEE 754)
        ADDT = 0x84,  // Add double (T-format)
        SUBT = 0x85,  // Subtract double
        MULT = 0x86,  // Multiply double
        DIVT = 0x87,  // Divide double
        SQRTT = 0x8F, // Square root double

        // VAX floating point (legacy)
        ADDF = 0x88, // Add F-format (VAX)
        SUBF = 0x89, // Subtract F-format
        MULF = 0x8A, // Multiply F-format
        DIVF = 0x8B, // Divide F-format

        ADDG = 0x8C, // Add G-format (VAX)
        SUBG = 0x8D, // Subtract G-format
        MULG = 0x8E, // Multiply G-format
        DIVG = 0x8F, // Divide G-format

        // Comparison operations
        CMPTUN = 0x90, // Compare unordered
        CMPTEQ = 0x91, // Compare equal
        CMPTLT = 0x92, // Compare less than
        CMPTLE = 0x93, // Compare less than or equal

        // Conversion operations
        CVTQS = 0x94, // Convert quadword to single
        CVTQT = 0x95, // Convert quadword to double
        CVTTS = 0x96, // Convert double to single
        CVTST = 0x97, // Convert single to double
        CVTTQ = 0x98, // Convert double to quadword
        CVTSQ = 0x99, // Convert single to quadword

        // Move operations
        CPYS = 0x9A,  // Copy sign
        CPYSN = 0x9B, // Copy sign negate
        CPYSE = 0x9C, // Copy sign and exponent

        // Conditional moves
        FCMOVEQ = 0x9D, // Floating conditional move if equal
        FCMOVNE = 0x9E, // Floating conditional move if not equal
        FCMOVLT = 0x9F, // Floating conditional move if less than
        FCMOVGE = 0xA0, // Floating conditional move if greater or equal
        FCMOVLE = 0xA1, // Floating conditional move if less or equal
        FCMOVGT = 0xA2, // Floating conditional move if greater than

        // Special operations
        MF_FPCR = 0xA3, // Move from floating point control register
        MT_FPCR = 0xA4  // Move to floating point control register
    };

    enum class FloatingPointFormat : uint8_t
    {
        IEEE_SINGLE = 0, // 32-bit IEEE 754
        IEEE_DOUBLE = 1, // 64-bit IEEE 754
        VAX_F = 2,       // 32-bit VAX F-format
        VAX_G = 3,       // 64-bit VAX G-format
        VAX_D = 4        // 64-bit VAX D-format
    };

    enum class RoundingMode : uint8_t
    {
        ROUND_NEAREST = 0,
        ROUND_DOWN = 1,
        ROUND_UP = 2,
        ROUND_TOWARD_ZERO = 3,
        ROUND_DYNAMIC = 4
    };

    explicit alphaFloatingPointInstruction(uint32_t opcode, FloatingPointOpType opType, uint8_t destReg,
                                           uint8_t srcReg1, uint8_t srcReg2)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_format(determineFormat(opType)), m_roundingMode(RoundingMode::ROUND_NEAREST), m_operand1(0.0),
          m_operand2(0.0), m_result(0.0), m_fpcr(0), m_exceptionCount(0), m_underflowCount(0), m_overflowCount(0),
          m_invalidOpCount(0), m_divideByZeroCount(0), m_inexactCount(0)
    {
        DEBUG_LOG("alphaFloatingPointInstruction created - OpType: 0x%02X, Dest: F%d, Src1: F%d, Src2: F%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2);
    }

    // Single operand constructor (for conversions, moves)
    explicit alphaFloatingPointInstruction(uint32_t opcode, FloatingPointOpType opType, uint8_t destReg,
                                           uint8_t srcReg1)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(0),
          m_format(determineFormat(opType)), m_roundingMode(RoundingMode::ROUND_NEAREST), m_operand1(0.0),
          m_operand2(0.0), m_result(0.0), m_fpcr(0), m_exceptionCount(0), m_underflowCount(0), m_overflowCount(0),
          m_invalidOpCount(0), m_divideByZeroCount(0), m_inexactCount(0)
    {
        DEBUG_LOG("alphaFloatingPointInstruction created (single op) - OpType: 0x%02X, Dest: F%d, Src: F%d",
                  static_cast<int>(opType), destReg, srcReg1);
    }

    virtual ~alphaFloatingPointInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performFloatingPointOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

 void decode()
    {
        DEBUG_LOG("Decoding floating point instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields based on Alpha floating point format
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        uint16_t function = opcode & 0x7FF; // 11-bit function field
        uint8_t rc = (opcode >> 0) & 0x1F;

        m_srcReg1 = ra;
        m_srcReg2 = rb;
        m_destReg = rc;

        // Determine operation type based on primary opcode and function
        switch (primaryOpcode)
        {
        case 0x14: // VAX floating point operations
            switch (function)
            {
            case 0x080:
                m_opType = FloatingPointOpType::ADDF;
                break;
            case 0x081:
                m_opType = FloatingPointOpType::SUBF;
                break;
            case 0x082:
                m_opType = FloatingPointOpType::MULF;
                break;
            case 0x083:
                m_opType = FloatingPointOpType::DIVF;
                break;
            case 0x0A0:
                m_opType = FloatingPointOpType::ADDG;
                break;
            case 0x0A1:
                m_opType = FloatingPointOpType::SUBG;
                break;
            case 0x0A2:
                m_opType = FloatingPointOpType::MULG;
                break;
            case 0x0A3:
                m_opType = FloatingPointOpType::DIVG;
                break;
            default:
                DEBUG_LOG("Unknown VAX FP function: 0x%03X", function);
                m_opType = FloatingPointOpType::ADDF;
                break;
            }
            break;

        case 0x15: // IEEE floating point operations
            switch (function)
            {
            case 0x080:
                m_opType = FloatingPointOpType::ADDS;
                break;
            case 0x081:
                m_opType = FloatingPointOpType::SUBS;
                break;
            case 0x082:
                m_opType = FloatingPointOpType::MULS;
                break;
            case 0x083:
                m_opType = FloatingPointOpType::DIVS;
                break;
            case 0x0A0:
                m_opType = FloatingPointOpType::ADDT;
                break;
            case 0x0A1:
                m_opType = FloatingPointOpType::SUBT;
                break;
            case 0x0A2:
                m_opType = FloatingPointOpType::MULT;
                break;
            case 0x0A3:
                m_opType = FloatingPointOpType::DIVT;
                break;
            case 0x14B:
                m_opType = FloatingPointOpType::SQRTS;
                break;
            case 0x14F:
                m_opType = FloatingPointOpType::SQRTT;
                break;
            default:
                DEBUG_LOG("Unknown IEEE FP function: 0x%03X", function);
                m_opType = FloatingPointOpType::ADDS;
                break;
            }
            break;

        case 0x16: // Floating point compare and conditional move
            switch (function)
            {
            case 0x0A5:
                m_opType = FloatingPointOpType::CMPTEQ;
                break;
            case 0x0A6:
                m_opType = FloatingPointOpType::CMPTLT;
                break;
            case 0x0A7:
                m_opType = FloatingPointOpType::CMPTLE;
                break;
            case 0x0A4:
                m_opType = FloatingPointOpType::CMPTUN;
                break;
            case 0x02A:
                m_opType = FloatingPointOpType::FCMOVEQ;
                break;
            case 0x02B:
                m_opType = FloatingPointOpType::FCMOVNE;
                break;
            case 0x02C:
                m_opType = FloatingPointOpType::FCMOVLT;
                break;
            case 0x02D:
                m_opType = FloatingPointOpType::FCMOVGE;
                break;
            case 0x02E:
                m_opType = FloatingPointOpType::FCMOVLE;
                break;
            case 0x02F:
                m_opType = FloatingPointOpType::FCMOVGT;
                break;
            default:
                DEBUG_LOG("Unknown FP compare function: 0x%03X", function);
                m_opType = FloatingPointOpType::CMPTEQ;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown FP primary opcode: 0x%02X", primaryOpcode);
            m_opType = FloatingPointOpType::ADDS;
            break;
        }

        DEBUG_LOG("FP instruction decoded - Type: 0x%02X, Dest: F%d, Src1: F%d, Src2: F%d", static_cast<int>(m_opType),
                  m_destReg, m_srcReg1, m_srcReg2);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Single precision operations
        case FloatingPointOpType::ADDS:
        case FloatingPointOpType::SUBS:
            return 4;
        case FloatingPointOpType::MULS:
            return 4;
        case FloatingPointOpType::DIVS:
            return 12;
        case FloatingPointOpType::SQRTS:
            return 18;

        // Double precision operations
        case FloatingPointOpType::ADDT:
        case FloatingPointOpType::SUBT:
            return 4;
        case FloatingPointOpType::MULT:
            return 4;
        case FloatingPointOpType::DIVT:
            return 15;
        case FloatingPointOpType::SQRTT:
            return 34;

        // VAX format operations
        case FloatingPointOpType::ADDF:
        case FloatingPointOpType::SUBF:
        case FloatingPointOpType::ADDG:
        case FloatingPointOpType::SUBG:
            return 4;
        case FloatingPointOpType::MULF:
        case FloatingPointOpType::MULG:
            return 4;
        case FloatingPointOpType::DIVF:
        case FloatingPointOpType::DIVG:
            return 15;

        // Comparison operations
        case FloatingPointOpType::CMPTUN:
        case FloatingPointOpType::CMPTEQ:
        case FloatingPointOpType::CMPTLT:
        case FloatingPointOpType::CMPTLE:
            return 4;

        // Conversion operations
        case FloatingPointOpType::CVTQS:
        case FloatingPointOpType::CVTQT:
        case FloatingPointOpType::CVTTS:
        case FloatingPointOpType::CVTST:
        case FloatingPointOpType::CVTTQ:
        case FloatingPointOpType::CVTSQ:
            return 4;

        // Move operations (fast)
        case FloatingPointOpType::CPYS:
        case FloatingPointOpType::CPYSN:
        case FloatingPointOpType::CPYSE:
            return 1;

        // Conditional moves
        case FloatingPointOpType::FCMOVEQ:
        case FloatingPointOpType::FCMOVNE:
        case FloatingPointOpType::FCMOVLT:
        case FloatingPointOpType::FCMOVGE:
        case FloatingPointOpType::FCMOVLE:
        case FloatingPointOpType::FCMOVGT:
            return 1;

        // Control register operations
        case FloatingPointOpType::MF_FPCR:
        case FloatingPointOpType::MT_FPCR:
            return 1;

        default:
            return 4;
        }
    }

    // Floating point classification
    bool isFloatingPoint() const override { return true; }

    // Performance-critical accessors
    inline FloatingPointOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg1() const { return m_srcReg1; }
    inline uint8_t getSrcReg2() const { return m_srcReg2; }
    inline FloatingPointFormat getFormat() const { return m_format; }
    inline RoundingMode getRoundingMode() const { return m_roundingMode; }
    inline double getResult() const { return m_result; }
    inline uint64_t getFPCR() const { return m_fpcr; }

    // Performance counters
    inline uint64_t getExceptionCount() const { return m_exceptionCount.load(std::memory_order_relaxed); }
    inline uint64_t getUnderflowCount() const { return m_underflowCount.load(std::memory_order_relaxed); }
    inline uint64_t getOverflowCount() const { return m_overflowCount.load(std::memory_order_relaxed); }
    inline uint64_t getInvalidOpCount() const { return m_invalidOpCount.load(std::memory_order_relaxed); }
    inline uint64_t getDivideByZeroCount() const { return m_divideByZeroCount.load(std::memory_order_relaxed); }
    inline uint64_t getInexactCount() const { return m_inexactCount.load(std::memory_order_relaxed); }

    // Hot path execution support
    inline void setOperands(double op1, double op2)
    {
        m_operand1 = op1;
        m_operand2 = op2;
    }

    inline void setOperand1(double op1) { m_operand1 = op1; }
    inline void setOperand2(double op2) { m_operand2 = op2; }
    inline void setRoundingMode(RoundingMode mode) { m_roundingMode = mode; }
    inline void setFPCR(uint64_t fpcr) { m_fpcr = fpcr; }

  private:
    FloatingPointFormat determineFormat(FloatingPointOpType opType) const
    {
        switch (opType)
        {
        case FloatingPointOpType::ADDS:
        case FloatingPointOpType::SUBS:
        case FloatingPointOpType::MULS:
        case FloatingPointOpType::DIVS:
        case FloatingPointOpType::SQRTS:
        case FloatingPointOpType::CVTQS:
        case FloatingPointOpType::CVTTS:
        case FloatingPointOpType::CVTSQ:
            return FloatingPointFormat::IEEE_SINGLE;

        case FloatingPointOpType::ADDT:
        case FloatingPointOpType::SUBT:
        case FloatingPointOpType::MULT:
        case FloatingPointOpType::DIVT:
        case FloatingPointOpType::SQRTT:
        case FloatingPointOpType::CVTQT:
        case FloatingPointOpType::CVTST:
        case FloatingPointOpType::CVTTQ:
            return FloatingPointFormat::IEEE_DOUBLE;

        case FloatingPointOpType::ADDF:
        case FloatingPointOpType::SUBF:
        case FloatingPointOpType::MULF:
        case FloatingPointOpType::DIVF:
            return FloatingPointFormat::VAX_F;

        case FloatingPointOpType::ADDG:
        case FloatingPointOpType::SUBG:
        case FloatingPointOpType::MULG:
        case FloatingPointOpType::DIVG:
            return FloatingPointFormat::VAX_G;

        default:
            return FloatingPointFormat::IEEE_DOUBLE;
        }
    }

    bool performFloatingPointOperation()
    {
        switch (m_opType)
        {
        // Arithmetic operations
        case FloatingPointOpType::ADDS:
        case FloatingPointOpType::ADDT:
        case FloatingPointOpType::ADDF:
        case FloatingPointOpType::ADDG:
            return performAdd();

        case FloatingPointOpType::SUBS:
        case FloatingPointOpType::SUBT:
        case FloatingPointOpType::SUBF:
        case FloatingPointOpType::SUBG:
            return performSubtract();

        case FloatingPointOpType::MULS:
        case FloatingPointOpType::MULT:
        case FloatingPointOpType::MULF:
        case FloatingPointOpType::MULG:
            return performMultiply();

        case FloatingPointOpType::DIVS:
        case FloatingPointOpType::DIVT:
        case FloatingPointOpType::DIVF:
        case FloatingPointOpType::DIVG:
            return performDivide();

        case FloatingPointOpType::SQRTS:
        case FloatingPointOpType::SQRTT:
            return performSquareRoot();

        // Comparison operations
        case FloatingPointOpType::CMPTUN:
        case FloatingPointOpType::CMPTEQ:
        case FloatingPointOpType::CMPTLT:
        case FloatingPointOpType::CMPTLE:
            return performCompare();

        // Conversion operations
        case FloatingPointOpType::CVTQS:
        case FloatingPointOpType::CVTQT:
        case FloatingPointOpType::CVTTS:
        case FloatingPointOpType::CVTST:
        case FloatingPointOpType::CVTTQ:
        case FloatingPointOpType::CVTSQ:
            return performConversion();

        // Move operations
        case FloatingPointOpType::CPYS:
        case FloatingPointOpType::CPYSN:
        case FloatingPointOpType::CPYSE:
            return performCopySign();

        // Conditional moves
        case FloatingPointOpType::FCMOVEQ:
        case FloatingPointOpType::FCMOVNE:
        case FloatingPointOpType::FCMOVLT:
        case FloatingPointOpType::FCMOVGE:
        case FloatingPointOpType::FCMOVLE:
        case FloatingPointOpType::FCMOVGT:
            return performConditionalMove();

        // Control register operations
        case FloatingPointOpType::MF_FPCR:
        case FloatingPointOpType::MT_FPCR:
            return performControlRegister();

        default:
            return false;
        }
    }

    bool performAdd()
    {
        m_result = m_operand1 + m_operand2;
        return checkFloatingPointResult();
    }

    bool performSubtract()
    {
        m_result = m_operand1 - m_operand2;
        return checkFloatingPointResult();
    }

    bool performMultiply()
    {
        m_result = m_operand1 * m_operand2;
        return checkFloatingPointResult();
    }

    bool performDivide()
    {
        if (m_operand2 == 0.0)
        {
            m_divideByZeroCount.fetch_add(1, std::memory_order_relaxed);
            m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Floating point divide by zero");
            return false;
        }
        m_result = m_operand1 / m_operand2;
        return checkFloatingPointResult();
    }

    bool performSquareRoot()
    {
        if (m_operand1 < 0.0)
        {
            m_invalidOpCount.fetch_add(1, std::memory_order_relaxed);
            m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Square root of negative number");
            return false;
        }
        m_result = std::sqrt(m_operand1);
        return checkFloatingPointResult();
    }

    bool performCompare()
    {
        switch (m_opType)
        {
        case FloatingPointOpType::CMPTUN:
            m_result = (std::isnan(m_operand1) || std::isnan(m_operand2)) ? 1.0 : 0.0;
            break;
        case FloatingPointOpType::CMPTEQ:
            m_result = (m_operand1 == m_operand2) ? 1.0 : 0.0;
            break;
        case FloatingPointOpType::CMPTLT:
            m_result = (m_operand1 < m_operand2) ? 1.0 : 0.0;
            break;
        case FloatingPointOpType::CMPTLE:
            m_result = (m_operand1 <= m_operand2) ? 1.0 : 0.0;
            break;
        default:
            return false;
        }
        return true;
    }

    bool performConversion()
    {
        // Simplified conversion - actual implementation would handle format specifics
        m_result = m_operand1;
        return checkFloatingPointResult();
    }

    bool performCopySign()
    {
        switch (m_opType)
        {
        case FloatingPointOpType::CPYS:
            m_result = std::copysign(m_operand1, m_operand2);
            break;
        case FloatingPointOpType::CPYSN:
            m_result = std::copysign(m_operand1, -m_operand2);
            break;
        case FloatingPointOpType::CPYSE:
            // Copy sign and exponent - simplified
            m_result = std::copysign(m_operand1, m_operand2);
            break;
        default:
            return false;
        }
        return true;
    }

    bool performConditionalMove()
    {
        // Simplified conditional move based on operand1 condition
        bool condition = false;
        switch (m_opType)
        {
        case FloatingPointOpType::FCMOVEQ:
            condition = (m_operand1 == 0.0);
            break;
        case FloatingPointOpType::FCMOVNE:
            condition = (m_operand1 != 0.0);
            break;
        case FloatingPointOpType::FCMOVLT:
            condition = (m_operand1 < 0.0);
            break;
        case FloatingPointOpType::FCMOVGE:
            condition = (m_operand1 >= 0.0);
            break;
        case FloatingPointOpType::FCMOVLE:
            condition = (m_operand1 <= 0.0);
            break;
        case FloatingPointOpType::FCMOVGT:
            condition = (m_operand1 > 0.0);
            break;
        default:
            return false;
        }

        if (condition)
        {
            m_result = m_operand2;
        }
        return true;
    }

    bool performControlRegister()
    {
        switch (m_opType)
        {
        case FloatingPointOpType::MF_FPCR:
            m_result = static_cast<double>(m_fpcr);
            break;
        case FloatingPointOpType::MT_FPCR:
            m_fpcr = static_cast<uint64_t>(m_operand1);
            break;
        default:
            return false;
        }
        return true;
    }

    bool checkFloatingPointResult()
    {
        if (std::isnan(m_result))
        {
            m_invalidOpCount.fetch_add(1, std::memory_order_relaxed);
            m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        if (std::isinf(m_result))
        {
            m_overflowCount.fetch_add(1, std::memory_order_relaxed);
            m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Check for underflow (subnormal numbers)
        if (m_result != 0.0 && std::abs(m_result) < std::numeric_limits<double>::min())
        {
            m_underflowCount.fetch_add(1, std::memory_order_relaxed);
            m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
        }

        return true;
    }

  private:
     FloatingPointOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg1;
     uint8_t m_srcReg2;
     FloatingPointFormat m_format;

    RoundingMode m_roundingMode;
    double m_operand1;
    double m_operand2;
    double m_result;
    uint64_t m_fpcr;

    std::atomic<uint64_t> m_exceptionCount;
    std::atomic<uint64_t> m_underflowCount;
    std::atomic<uint64_t> m_overflowCount;
    std::atomic<uint64_t> m_invalidOpCount;
    std::atomic<uint64_t> m_divideByZeroCount;
    std::atomic<uint64_t> m_inexactCount;

    // Prevent copying for performance
    alphaFloatingPointInstruction(const alphaFloatingPointInstruction &) = delete;
    alphaFloatingPointInstruction &operator=(const alphaFloatingPointInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaBranchInstruction : public alphaInstructionBase
{
  public:
    enum class BranchOpType : uint8_t
    {
        // Unconditional branches
        BR = 0x30,  // Branch unconditional
        BSR = 0x34, // Branch to subroutine

        // Integer conditional branches
        BEQ = 0x39, // Branch if equal to zero
        BNE = 0x3D, // Branch if not equal to zero
        BLT = 0x3A, // Branch if less than zero
        BLE = 0x3B, // Branch if less than or equal to zero
        BGT = 0x3F, // Branch if greater than zero
        BGE = 0x3E, // Branch if greater than or equal to zero

        // Bit test branches
        BLBC = 0x38, // Branch if low bit clear
        BLBS = 0x3C, // Branch if low bit set

        // Floating point conditional branches
        FBEQ = 0x31, // Floating branch if equal to zero
        FBNE = 0x35, // Floating branch if not equal to zero
        FBLT = 0x32, // Floating branch if less than zero
        FBLE = 0x33, // Floating branch if less than or equal to zero
        FBGT = 0x37, // Floating branch if greater than zero
        FBGE = 0x36, // Floating branch if greater than or equal to zero

        // Jump instructions
        JMP = 0x1A,           // Jump
        JSR = 0x1A,           // Jump to subroutine (same opcode, different hint)
        RET = 0x1A,           // Return from subroutine (same opcode, different hint)
        JSR_COROUTINE = 0x1A, // Jump subroutine coroutine (same opcode, different hint)

        // System calls and traps
        CALL_PAL = 0x00, // Call privileged architecture library

        // Conditional move (branch-like behavior)
        CMOVEQ = 0x24, // Conditional move if equal
        CMOVNE = 0x26, // Conditional move if not equal
        CMOVLT = 0x44, // Conditional move if less than
        CMOVLE = 0x64, // Conditional move if less than or equal
        CMOVGT = 0x66, // Conditional move if greater than
        CMOVGE = 0x46  // Conditional move if greater than or equal
    };

    enum class BranchHint : uint8_t
    {
        NONE = 0,
        LIKELY_TAKEN = 1,
        LIKELY_NOT_TAKEN = 2,
        SUBROUTINE_CALL = 3,
        SUBROUTINE_RETURN = 4
    };

    enum class PredictionResult : uint8_t
    {
        NOT_PREDICTED = 0,
        PREDICTED_TAKEN_CORRECT = 1,
        PREDICTED_TAKEN_INCORRECT = 2,
        PREDICTED_NOT_TAKEN_CORRECT = 3,
        PREDICTED_NOT_TAKEN_INCORRECT = 4
    };

    explicit alphaBranchInstruction(uint32_t opcode, BranchOpType opType, uint8_t conditionReg, int32_t displacement)
        : alphaInstructionBase(opcode), m_opType(opType), m_conditionReg(conditionReg), m_targetReg(0),
          m_displacement(displacement), m_hint(BranchHint::NONE), m_conditionValue(0), m_targetAddress(0),
          m_returnAddress(0), m_branchTaken(false), m_takenCount(0), m_notTakenCount(0), m_mispredictCount(0),
          m_correctPredictCount(0), m_returnStackHitCount(0), m_returnStackMissCount(0)
    {
        DEBUG_LOG("alphaBranchInstruction created - OpType: 0x%02X, CondReg: R%d, Disp: %d", static_cast<int>(opType),
                  conditionReg, displacement);
    }

    // Jump instruction constructor (register indirect)
    explicit alphaBranchInstruction(uint32_t opcode, BranchOpType opType, uint8_t conditionReg, uint8_t targetReg,
                                    BranchHint hint)
        : alphaInstructionBase(opcode), m_opType(opType), m_conditionReg(conditionReg), m_targetReg(targetReg),
          m_displacement(0), m_hint(hint), m_conditionValue(0), m_targetAddress(0), m_returnAddress(0),
          m_branchTaken(false), m_takenCount(0), m_notTakenCount(0), m_mispredictCount(0), m_correctPredictCount(0),
          m_returnStackHitCount(0), m_returnStackMissCount(0)
    {
        DEBUG_LOG("alphaBranchInstruction created (jump) - OpType: 0x%02X, CondReg: R%d, TargetReg: R%d, Hint: %d",
                  static_cast<int>(opType), conditionReg, targetReg, static_cast<int>(hint));
    }

    virtual ~alphaBranchInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = evaluateBranchCondition() && calculateTargetAddress();
        if (success)
        {
            updateBranchStatistics();
            addCycles(getCycleLatency());
        }

        return success;
    }

    void decode() override { DEBUG_LOG("Decoding branch instruction opcode: 0x%08X", getOpcode());
        // TODO
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Unconditional branches - fast
        case BranchOpType::BR:
            return 1;
        case BranchOpType::BSR:
            return 1;

        // Conditional branches - depends on prediction
        case BranchOpType::BEQ:
        case BranchOpType::BNE:
        case BranchOpType::BLT:
        case BranchOpType::BLE:
        case BranchOpType::BGT:
        case BranchOpType::BGE:
        case BranchOpType::BLBC:
        case BranchOpType::BLBS:
            return m_branchTaken ? 1 : 1; // Predicted correctly

        // Floating point branches - slightly slower
        case BranchOpType::FBEQ:
        case BranchOpType::FBNE:
        case BranchOpType::FBLT:
        case BranchOpType::FBLE:
        case BranchOpType::FBGT:
        case BranchOpType::FBGE:
            return 2;

        // Jump instructions - register indirect
        case BranchOpType::JMP:
        case BranchOpType::JSR:
        case BranchOpType::RET:
        case BranchOpType::JSR_COROUTINE:
            return 1;

        // System calls
        case BranchOpType::CALL_PAL:
            return 10; // Significant overhead

        // Conditional moves
        case BranchOpType::CMOVEQ:
        case BranchOpType::CMOVNE:
        case BranchOpType::CMOVLT:
        case BranchOpType::CMOVLE:
        case BranchOpType::CMOVGT:
        case BranchOpType::CMOVGE:
            return 1;

        default:
            return 1;
        }
    }

    // Branch classification
    bool isBranch() const override { return true; }

    inline bool isUnconditional() const
    {
        return (m_opType == BranchOpType::BR || m_opType == BranchOpType::BSR || m_opType == BranchOpType::JMP);
    }

    inline bool isConditional() const
    {
        return !isUnconditional() && !isConditionalMove() && (m_opType != BranchOpType::CALL_PAL);
    }

    inline bool isSubroutineCall() const
    {
        return (m_opType == BranchOpType::BSR || m_opType == BranchOpType::JSR ||
                m_hint == BranchHint::SUBROUTINE_CALL);
    }

    inline bool isSubroutineReturn() const
    {
        return (m_opType == BranchOpType::RET || m_hint == BranchHint::SUBROUTINE_RETURN);
    }

    inline bool isConditionalMove() const
    {
        return (m_opType >= BranchOpType::CMOVEQ && m_opType <= BranchOpType::CMOVGE);
    }

    inline bool isFloatingPointBranch() const
    {
        return (m_opType >= BranchOpType::FBEQ && m_opType <= BranchOpType::FBGE);
    }

    // Performance-critical accessors
    inline BranchOpType getOpType() const { return m_opType; }
    inline uint8_t getConditionReg() const { return m_conditionReg; }
    inline uint8_t getTargetReg() const { return m_targetReg; }
    inline int32_t getDisplacement() const { return m_displacement; }
    inline BranchHint getHint() const { return m_hint; }
    inline uint64_t getTargetAddress() const { return m_targetAddress; }
    inline uint64_t getReturnAddress() const { return m_returnAddress; }
    inline bool wasBranchTaken() const { return m_branchTaken; }

    // Performance counters
    inline uint64_t getTakenCount() const { return m_takenCount.load(std::memory_order_relaxed); }
    inline uint64_t getNotTakenCount() const { return m_notTakenCount.load(std::memory_order_relaxed); }
    inline uint64_t getMispredictCount() const { return m_mispredictCount.load(std::memory_order_relaxed); }
    inline uint64_t getCorrectPredictCount() const { return m_correctPredictCount.load(std::memory_order_relaxed); }
    inline uint64_t getReturnStackHitCount() const { return m_returnStackHitCount.load(std::memory_order_relaxed); }
    inline uint64_t getReturnStackMissCount() const { return m_returnStackMissCount.load(std::memory_order_relaxed); }

    // Branch prediction accuracy
    inline double getBranchTakenRate() const
    {
        uint64_t total = getTakenCount() + getNotTakenCount();
        return total > 0 ? static_cast<double>(getTakenCount()) / total : 0.0;
    }

    inline double getPredictionAccuracy() const
    {
        uint64_t total = getCorrectPredictCount() + getMispredictCount();
        return total > 0 ? static_cast<double>(getCorrectPredictCount()) / total : 0.0;
    }

    // Hot path execution support
    inline void setConditionValue(uint64_t value) { m_conditionValue = value; }
    inline void setTargetAddress(uint64_t address) { m_targetAddress = address; }
    inline void setReturnAddress(uint64_t address) { m_returnAddress = address; }
    inline void setPredictionResult(PredictionResult result) { m_predictionResult = result; }

  private:
    bool evaluateBranchCondition()
    {
        switch (m_opType)
        {
        // Unconditional branches
        case BranchOpType::BR:
        case BranchOpType::BSR:
        case BranchOpType::JMP:
        case BranchOpType::JSR:
        case BranchOpType::RET:
        case BranchOpType::JSR_COROUTINE:
        case BranchOpType::CALL_PAL:
            m_branchTaken = true;
            return true;

        // Integer conditional branches
        case BranchOpType::BEQ:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) == 0);
            break;
        case BranchOpType::BNE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) != 0);
            break;
        case BranchOpType::BLT:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) < 0);
            break;
        case BranchOpType::BLE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) <= 0);
            break;
        case BranchOpType::BGT:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) > 0);
            break;
        case BranchOpType::BGE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) >= 0);
            break;

        // Bit test branches
        case BranchOpType::BLBC:
            m_branchTaken = ((m_conditionValue & 1) == 0);
            break;
        case BranchOpType::BLBS:
            m_branchTaken = ((m_conditionValue & 1) == 1);
            break;

        // Floating point branches (simplified - would need proper FP comparison)
        case BranchOpType::FBEQ:
            m_branchTaken = (m_conditionValue == 0);
            break;
        case BranchOpType::FBNE:
            m_branchTaken = (m_conditionValue != 0);
            break;
        case BranchOpType::FBLT:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) < 0);
            break;
        case BranchOpType::FBLE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) <= 0);
            break;
        case BranchOpType::FBGT:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) > 0);
            break;
        case BranchOpType::FBGE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) >= 0);
            break;

        // Conditional moves (always execute, but set taken flag for statistics)
        case BranchOpType::CMOVEQ:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) == 0);
            break;
        case BranchOpType::CMOVNE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) != 0);
            break;
        case BranchOpType::CMOVLT:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) < 0);
            break;
        case BranchOpType::CMOVLE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) <= 0);
            break;
        case BranchOpType::CMOVGT:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) > 0);
            break;
        case BranchOpType::CMOVGE:
            m_branchTaken = (static_cast<int64_t>(m_conditionValue) >= 0);
            break;

        default:
            return false;
        }

        return true;
    }

    bool calculateTargetAddress()
    {
        if (m_targetReg != 0)
        {
            // Register indirect addressing (JMP, JSR, RET)
            // Target address would be loaded from register - simplified here
            DEBUG_LOG("Register indirect branch to R%d", m_targetReg);
        }
        else
        {
            // PC-relative addressing
            // In real implementation, would use current PC + (displacement * 4)
            m_targetAddress = static_cast<uint64_t>(m_displacement * 4);
            DEBUG_LOG("PC-relative branch, displacement: %d, target: 0x%016llX", m_displacement, m_targetAddress);
        }
        return true;
    }

    void updateBranchStatistics()
    {
        if (m_branchTaken)
        {
            m_takenCount.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            m_notTakenCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Update prediction statistics
        switch (m_predictionResult)
        {
        case PredictionResult::PREDICTED_TAKEN_CORRECT:
        case PredictionResult::PREDICTED_NOT_TAKEN_CORRECT:
            m_correctPredictCount.fetch_add(1, std::memory_order_relaxed);
            break;
        case PredictionResult::PREDICTED_TAKEN_INCORRECT:
        case PredictionResult::PREDICTED_NOT_TAKEN_INCORRECT:
            m_mispredictCount.fetch_add(1, std::memory_order_relaxed);
            break;
        default:
            break;
        }

        // Update return stack statistics for subroutine operations
        if (isSubroutineReturn())
        {
            // Would check return stack prediction accuracy
            m_returnStackHitCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

  private:
     BranchOpType m_opType;
     uint8_t m_conditionReg;
     uint8_t m_targetReg;
     int32_t m_displacement;
     BranchHint m_hint;

    uint64_t m_conditionValue;
    uint64_t m_targetAddress;
    uint64_t m_returnAddress;
    bool m_branchTaken;
    PredictionResult m_predictionResult = PredictionResult::NOT_PREDICTED;

    std::atomic<uint64_t> m_takenCount;
    std::atomic<uint64_t> m_notTakenCount;
    std::atomic<uint64_t> m_mispredictCount;
    std::atomic<uint64_t> m_correctPredictCount;
    std::atomic<uint64_t> m_returnStackHitCount;
    std::atomic<uint64_t> m_returnStackMissCount;

    // Prevent copying for performance
    alphaBranchInstruction(const alphaBranchInstruction &) = delete;
    alphaBranchInstruction &operator=(const alphaBranchInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

#if defined(_MSC_VER) && defined(_WIN64)
// Microsoft VS2022 host system architecture
#pragma pack(push, 8)
#endif

// Hot Path Class - Optimized for execution performance
class alphaBranchAdvInstruction : public alphaInstructionBase
{
  public:
    // Direct initialization for hot path performance
    explicit alphaBranchAdvInstruction() = default;
    ~alphaBranchAdvInstruction() = default;

    // Core pipeline methods
    void decode()
    {
        DEBUG_LOG("Decoding branch instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields based on Alpha branch format
        // Bits 31-26: Primary opcode
        // Bits 25-21: Ra (condition register)
        // Bits 20-0: Displacement (21-bit signed)

        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        int32_t displacement = static_cast<int32_t>((opcode & 0x1FFFFF) << 11) >> 11; // Sign extend 21-bit

        m_conditionReg = ra;
        m_displacement = displacement;
        m_targetReg = 0; // Most branches use PC-relative addressing
        m_hint = BranchHint::NONE;

        // Determine operation type based on primary opcode
        switch (primaryOpcode)
        {
        case 0x30:
            m_opType = BranchOpType::BR;
            break;
        case 0x31:
            m_opType = BranchOpType::FBEQ;
            break;
        case 0x32:
            m_opType = BranchOpType::FBLT;
            break;
        case 0x33:
            m_opType = BranchOpType::FBLE;
            break;
        case 0x34:
            m_opType = BranchOpType::BSR;
            m_hint = BranchHint::SUBROUTINE_CALL;
            break;
        case 0x35:
            m_opType = BranchOpType::FBNE;
            break;
        case 0x36:
            m_opType = BranchOpType::FBGE;
            break;
        case 0x37:
            m_opType = BranchOpType::FBGT;
            break;
        case 0x38:
            m_opType = BranchOpType::BLBC;
            break;
        case 0x39:
            m_opType = BranchOpType::BEQ;
            break;
        case 0x3A:
            m_opType = BranchOpType::BLT;
            break;
        case 0x3B:
            m_opType = BranchOpType::BLE;
            break;
        case 0x3C:
            m_opType = BranchOpType::BLBS;
            break;
        case 0x3D:
            m_opType = BranchOpType::BNE;
            break;
        case 0x3E:
            m_opType = BranchOpType::BGE;
            break;
        case 0x3F:
            m_opType = BranchOpType::BGT;
            break;

        case 0x1A: // Jump instructions (register indirect)
        {
            // For jump instructions, extract hint from function field
            uint16_t function = (opcode >> 14) & 0x3;
            m_targetReg = (opcode >> 16) & 0x1F; // Rb field contains target register

            switch (function)
            {
            case 0x0:
                m_opType = BranchOpType::JMP;
                break;
            case 0x1:
                m_opType = BranchOpType::JSR;
                m_hint = BranchHint::SUBROUTINE_CALL;
                break;
            case 0x2:
                m_opType = BranchOpType::RET;
                m_hint = BranchHint::SUBROUTINE_RETURN;
                break;
            case 0x3:
                m_opType = BranchOpType::JSR_COROUTINE;
                break;
            }
        }
        break;

        case 0x00:
            m_opType = BranchOpType::CALL_PAL;
            break;

        // Conditional move (branch-like behavior)
        case 0x24:
            m_opType = BranchOpType::CMOVEQ;
            break;
        case 0x26:
            m_opType = BranchOpType::CMOVNE;
            break;
        case 0x44:
            m_opType = BranchOpType::CMOVLT;
            break;
        case 0x64:
            m_opType = BranchOpType::CMOVLE;
            break;
        case 0x66:
            m_opType = BranchOpType::CMOVGT;
            break;
        case 0x46:
            m_opType = BranchOpType::CMOVGE;
            break;

        default:
            DEBUG_LOG("Unknown branch opcode: 0x%02X", primaryOpcode);
            m_opType = BranchOpType::BR;
            break;
        }

        DEBUG_LOG("Branch instruction decoded - Type: 0x%02X, CondReg: R%d, Displacement: %d",
                  static_cast<int>(m_opType), m_conditionReg, m_displacement);
    }

    bool execute() ;
    void writeback() ;
    const char *typeName() const  { return "BranchAdv"; }

    // Branch prediction hint types
    enum class PredictionHint : uint8_t
    {
        None = 0,          // No hint provided
        Likely = 1,        // Branch likely to be taken (>75% probability)
        Unlikely = 2,      // Branch unlikely to be taken (<25% probability)
        AlwaysTaken = 3,   // Branch always taken (static analysis)
        NeverTaken = 4,    // Branch never taken (static analysis)
        LoopEnd = 5,       // Loop termination branch
        FunctionCall = 6,  // Function call branch
        FunctionReturn = 7 // Function return branch
    };

    // Coroutine operation types
    enum class CoroutineOp : uint8_t
    {
        None = 0,    // Not a coroutine operation
        Yield = 1,   // Suspend current coroutine and yield control
        Resume = 2,  // Resume suspended coroutine execution
        Call = 3,    // Create and call new coroutine
        Return = 4,  // Return from coroutine to caller
        Destroy = 5, // Destroy coroutine context and cleanup
        Switch = 6,  // Switch between coroutines directly
        Await = 7    // Await coroutine completion
    };

    // Branch prediction state
    enum class PredictionState : uint8_t
    {
        StronglyNotTaken = 0, // 00 - Strong bias toward not taken
        WeaklyNotTaken = 1,   // 01 - Weak bias toward not taken
        WeaklyTaken = 2,      // 10 - Weak bias toward taken
        StronglyTaken = 3     // 11 - Strong bias toward taken
    };

  private:
    // Basic branch fields - aligned for cache efficiency
    alignas(8) bool m_conditionMet{false};
    bool m_isConditional{true};
    bool m_predictedTaken{false};
    bool m_predictionCorrect{false};

    uint64_t m_targetAddress{0};
    int64_t m_conditionValue{0};

    // Branch prediction hints - grouped for cache locality
    PredictionHint m_staticHint{PredictionHint::None};
    PredictionState m_predictionState{PredictionState::WeaklyNotTaken};
    uint8_t m_confidence{50};        // Prediction confidence (0-100)
    uint8_t m_branchType{0};         // Encoded branch type
    uint32_t m_branchHistory{0};     // Branch history register (16-bit)
    uint64_t m_predictorIndex{0};    // Index into branch predictor table
    uint64_t m_lastTargetAddress{0}; // Previous target for indirect branches

    // Coroutine fields - grouped together
    CoroutineOp m_coroutineOperation{CoroutineOp::None};
    bool m_isCoroutineInstruction{false};
    uint8_t m_coroutinePriority{0};  // Coroutine scheduling priority
    uint32_t m_coroutineId{0};       // Unique coroutine identifier
    uint64_t m_coroutineContext{0};  // Pointer to coroutine context
    uint64_t m_stackFramePtr{0};     // Coroutine stack frame pointer
    uint64_t m_yieldValue{0};        // Value yielded/resumed with
    uint64_t m_parentCoroutineId{0}; // Parent coroutine for call/return

  public:
    // Performance tracking - hot path optimized atomics
    static std::atomic<uint64_t> s_totalPredictions;
    static std::atomic<uint64_t> s_correctPredictions;
    static std::atomic<uint64_t> s_totalCoroutineOps;
    static std::atomic<uint64_t> s_mispredictionPenalty;

    // Inline hot path methods for maximum performance
    inline void updatePrediction(bool actualTaken)
    {
        s_totalPredictions.fetch_add(1, std::memory_order_relaxed);

        if ((actualTaken && m_predictedTaken) || (!actualTaken && !m_predictedTaken))
        {
            m_predictionCorrect = true;
            s_correctPredictions.fetch_add(1, std::memory_order_relaxed);
            updatePredictionState(true);
        }
        else
        {
            m_predictionCorrect = false;
            s_mispredictionPenalty.fetch_add(10, std::memory_order_relaxed); // 10 cycle penalty
            updatePredictionState(false);
        }

        // Update branch history for next prediction
        m_branchHistory = ((m_branchHistory << 1) | (actualTaken ? 1 : 0)) & 0xFFFF;
    }

    inline void updatePredictionState(bool correct)
    {
        if (correct)
        {
            // Strengthen current prediction
            if (m_predictedTaken && m_predictionState != PredictionState::StronglyTaken)
            {
                m_predictionState = static_cast<PredictionState>(static_cast<uint8_t>(m_predictionState) + 1);
            }
            else if (!m_predictedTaken && m_predictionState != PredictionState::StronglyNotTaken)
            {
                m_predictionState = static_cast<PredictionState>(static_cast<uint8_t>(m_predictionState) - 1);
            }
        }
        else
        {
            // Weaken current prediction
            if (m_predictedTaken)
            {
                m_predictionState = static_cast<PredictionState>(static_cast<uint8_t>(m_predictionState) - 1);
            }
            else
            {
                m_predictionState = static_cast<PredictionState>(static_cast<uint8_t>(m_predictionState) + 1);
            }
        }
    }

    inline double getPredictionAccuracy() const
    {
        uint64_t total = s_totalPredictions.load(std::memory_order_relaxed);
        return total > 0 ? static_cast<double>(s_correctPredictions.load(std::memory_order_relaxed)) / total : 0.0;
    }

    inline bool isCoroutineRelated() const { return m_isCoroutineInstruction; }

    inline void recordCoroutineOperation()
    {
        if (m_isCoroutineInstruction)
        {
            s_totalCoroutineOps.fetch_add(1, std::memory_order_relaxed);
        }
    }

    inline void setPredictionHint(PredictionHint hint, uint8_t confidence = 75)
    {
        m_staticHint = hint;
        m_confidence = confidence;

        // Initialize prediction state based on hint
        switch (hint)
        {
        case PredictionHint::AlwaysTaken:
        case PredictionHint::Likely:
            m_predictionState = PredictionState::StronglyTaken;
            m_predictedTaken = true;
            break;
        case PredictionHint::NeverTaken:
        case PredictionHint::Unlikely:
            m_predictionState = PredictionState::StronglyNotTaken;
            m_predictedTaken = false;
            break;
        case PredictionHint::LoopEnd:
            m_predictionState = PredictionState::WeaklyTaken;
            m_predictedTaken = true;
            break;
        default:
            m_predictionState = PredictionState::WeaklyNotTaken;
            m_predictedTaken = false;
            break;
        }
    }

    inline void setCoroutineOperation(CoroutineOp op, uint32_t coroutineId = 0, uint64_t context = 0)
    {
        m_coroutineOperation = op;
        m_isCoroutineInstruction = (op != CoroutineOp::None);
        m_coroutineId = coroutineId;
        m_coroutineContext = context;
    }

    // Accessors for performance monitoring
    inline PredictionHint getStaticHint() const { return m_staticHint; }
    inline PredictionState getPredictionState() const { return m_predictionState; }
    inline CoroutineOp getCoroutineOperation() const { return m_coroutineOperation; }
    inline uint32_t getBranchHistory() const { return m_branchHistory; }
    inline uint8_t getConfidence() const { return m_confidence; }
    inline bool wasPredictionCorrect() const { return m_predictionCorrect; }

    // String conversion for debugging (cold path)
    const char *getCoroutineOpName() const;
    const char *getPredictionHintName() const;
    const char *getPredictionStateName() const;

	bool executeYield();
    bool executeResume();
    bool executeCoroutineReturn();
    bool executeCoroutineDestroy();
    bool executeCoroutineSwitch();
    bool executeCoroutineAwait();
        uint64_t *getCurrentRegisters();

    uint64_t getCurrentStackPointer()
    {
        // Return current stack pointer value
        // Implementation would interface with processor state
        return 0x7FFFFFFF0000ULL;
    }
    
        void setRegisterValue(uint8_t reg, uint64_t value);
        void setProgramCounter(uint64_t pc);
        uint32_t allocateCoroutineId();
        void deallocateCoroutineId(uint32_t id);
        void switchToCoroutine(uint32_t coroutineId);
        void returnToScheduler();
        uint64_t *getStoredRegisters(uint32_t coroutineId);
        uint64_t getStoredStackPointer(uint32_t coroutineId);
        uint32_t alphaBranchAdvInstruction::extractCoroutineId();
    uint64_t getStoredProgramCounter(uint32_t coroutineId);
        void setCoroutineParent(uint32_t childId, uint32_t parentId);
        void cleanupCoroutineContext(uint32_t coroutineId);
        void setCoroutineStatus(uint32_t coroutineId, CoroutineStatus status);
    void saveCoroutineRegisters(uint32_t coroutineId, const uint64_t *registers);
    void saveCoroutineProgramCounter(uint32_t coroutineId, uint64_t pc);
    void restoreCoroutineRegisters(const uint64_t *registers);
    void setStackPointer(uint64_t stackPointer);
void initializeCoroutineStack(uint64_t stackBase, uint32_t stackSize);
    uint64_t extractYieldValue();
    uint64_t extractResumeValue();
    void detectCoroutineOperation(uint32_t function);
    // Advanced prediction methods
    void initializeBranchPredictor(uint64_t programCounter, uint32_t instruction);
    bool shouldTakeBranch() const;
    void updateBranchTarget(uint64_t newTarget);

	bool executeCoroutineOperation();
    // Coroutine context management
    void setupCoroutineContext(uint64_t stackBase, uint32_t stackSize, uint8_t priority = 0);
    void saveCoroutineState(uint64_t *registers, uint64_t stackPointer);
    void restoreCoroutineState(const uint64_t *registers, uint64_t stackPointer);

  private:
    // Internal helper methods
    inline uint64_t calculatePredictorIndex(uint64_t pc) const
    {
        return (pc ^ (m_branchHistory << 2)) & 0x3FF; // 1024-entry predictor table
    }

    inline bool evaluateBranchCondition() const
    {
        // Alpha-specific condition evaluation
        switch (m_branchType)
        {
        case 0x30:
            return m_conditionValue != 0; // BR - unconditional
        case 0x31:
            return m_conditionValue == 0; // FBEQ
        case 0x32:
            return m_conditionValue < 0; // FBLT
        case 0x33:
            return m_conditionValue <= 0; // FBLE
        case 0x34:
            return m_conditionValue == 0; // BSR
        case 0x35:
            return m_conditionValue != 0; // FBNE
        case 0x36:
            return m_conditionValue >= 0; // FBGE
        case 0x37:
            return m_conditionValue > 0; // FBGT
        case 0x38:
            return (m_conditionValue & 1) != 0; // BLBC
        case 0x39:
            return (m_conditionValue & 1) == 0; // BEQ
        case 0x3A:
            return m_conditionValue < 0; // BLT
        case 0x3B:
            return m_conditionValue <= 0; // BLE
        case 0x3C:
            return (m_conditionValue & 1) == 0; // BLBS
        case 0x3D:
            return m_conditionValue != 0; // BNE
        case 0x3E:
            return m_conditionValue >= 0; // BGE
        case 0x3F:
            return m_conditionValue > 0; // BGT
        default:
            return false;
        }
    }
};

// Static member initialization
std::atomic<uint64_t> alphaBranchAdvInstruction::s_totalPredictions{0};
std::atomic<uint64_t> alphaBranchAdvInstruction::s_correctPredictions{0};
std::atomic<uint64_t> alphaBranchAdvInstruction::s_totalCoroutineOps{0};
std::atomic<uint64_t> alphaBranchAdvInstruction::s_mispredictionPenalty{0};

#if defined(_MSC_VER) && defined(_WIN64)
#pragma pack(pop)
#endif



#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaSQRTInstruction : public alphaInstructionBase
{
  public:
    enum class SQRTOpType : uint8_t
    {
        // IEEE 754 square root operations
        SQRTS = 0x14B, // Square root single precision (S-format)
        SQRTT = 0x14F, // Square root double precision (T-format)

        // VAX square root operations (legacy)
        SQRTF = 0x14A, // Square root F-format (VAX single)
        SQRTG = 0x14E, // Square root G-format (VAX double)

        // Integer square root (non-standard extension)
        ISQRT = 0x1C0,   // Integer square root (64-bit)
        ISQRT32 = 0x1C1, // Integer square root (32-bit)

        // Reciprocal square root approximation (performance optimization)
        RSQRTS = 0x15B, // Reciprocal square root single
        RSQRTT = 0x15F, // Reciprocal square root double

        // Square root with different rounding modes
        SQRTS_C = 0x16B,   // Square root single chopped
        SQRTT_C = 0x16F,   // Square root double chopped
        SQRTS_M = 0x17B,   // Square root single minus infinity
        SQRTT_M = 0x17F,   // Square root double minus infinity
        SQRTS_D = 0x18B,   // Square root single dynamic rounding
        SQRTT_D = 0x18F,   // Square root double dynamic rounding
        SQRTS_U = 0x19B,   // Square root single underflow disabled
        SQRTT_U = 0x19F,   // Square root double underflow disabled
        SQRTS_SU = 0x1AB,  // Square root single software completion
        SQRTT_SU = 0x1AF,  // Square root double software completion
        SQRTS_SUI = 0x1BB, // Square root single software, inexact disabled
        SQRTT_SUI = 0x1BF  // Square root double software, inexact disabled
    };

    enum class SQRTFormat : uint8_t
    {
        IEEE_SINGLE = 0, // 32-bit IEEE 754
        IEEE_DOUBLE = 1, // 64-bit IEEE 754
        VAX_F = 2,       // 32-bit VAX F-format
        VAX_G = 3,       // 64-bit VAX G-format
        INTEGER_64 = 4,  // 64-bit integer
        INTEGER_32 = 5   // 32-bit integer
    };

    enum class RoundingMode : uint8_t
    {
        ROUND_NEAREST = 0,     // Round to nearest (default)
        ROUND_DOWN = 1,        // Round toward minus infinity
        ROUND_UP = 2,          // Round toward plus infinity
        ROUND_TOWARD_ZERO = 3, // Round toward zero (chopped)
        ROUND_DYNAMIC = 4      // Use dynamic rounding mode from FPCR
    };

    enum class SQRTMethod : uint8_t
    {
        HARDWARE_NATIVE = 0,  // Use hardware square root unit
        NEWTON_RAPHSON = 1,   // Newton-Raphson iteration
        LOOKUP_TABLE = 2,     // Table lookup with interpolation
        SOFTWARE_LIBRARY = 3, // Software library implementation
        RECIPROCAL_APPROX = 4 // Reciprocal square root approximation
    };

    explicit alphaSQRTInstruction(uint32_t opcode, SQRTOpType opType, uint8_t destReg, uint8_t srcReg)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg(srcReg),
          m_format(determineFormat(opType)), m_roundingMode(determineRoundingMode(opType)),
          m_method(SQRTMethod::HARDWARE_NATIVE), m_operand(0.0), m_result(0.0), m_intOperand(0), m_intResult(0),
          m_iterations(0), m_precision(0), m_domainErrorCount(0), m_underflowCount(0), m_overflowCount(0),
          m_inexactCount(0), m_denormalInputCount(0), m_negativeInputCount(0), m_zeroInputCount(0),
          m_infinityInputCount(0), m_nanInputCount(0), m_iterationCount(0)
    {
        DEBUG_LOG("alphaSQRTInstruction created - OpType: 0x%03X, Dest: F%d, Src: F%d", static_cast<int>(opType),
                  destReg, srcReg);

        // Set precision based on format
        switch (m_format)
        {
        case SQRTFormat::IEEE_SINGLE:
        case SQRTFormat::VAX_F:
        case SQRTFormat::INTEGER_32:
            m_precision = 24; // 24-bit mantissa
            break;
        case SQRTFormat::IEEE_DOUBLE:
        case SQRTFormat::VAX_G:
        case SQRTFormat::INTEGER_64:
            m_precision = 53; // 53-bit mantissa
            break;
        }
    }

    virtual ~alphaSQRTInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performSQRTOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

    void decode() override { DEBUG_LOG("Decoding SQRT instruction opcode: 0x%08X", getOpcode()); }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // IEEE single precision
        case SQRTOpType::SQRTS:
        case SQRTOpType::SQRTS_C:
        case SQRTOpType::SQRTS_M:
        case SQRTOpType::SQRTS_D:
        case SQRTOpType::SQRTS_U:
            return 18; // Hardware single precision sqrt

        // IEEE double precision
        case SQRTOpType::SQRTT:
        case SQRTOpType::SQRTT_C:
        case SQRTOpType::SQRTT_M:
        case SQRTOpType::SQRTT_D:
        case SQRTOpType::SQRTT_U:
            return 34; // Hardware double precision sqrt

        // VAX formats
        case SQRTOpType::SQRTF:
            return 20; // VAX F-format
        case SQRTOpType::SQRTG:
            return 36; // VAX G-format

        // Integer square root
        case SQRTOpType::ISQRT:
            return 25; // 64-bit integer sqrt
        case SQRTOpType::ISQRT32:
            return 15; // 32-bit integer sqrt

        // Reciprocal square root (approximation)
        case SQRTOpType::RSQRTS:
            return 8; // Fast approximation single
        case SQRTOpType::RSQRTT:
            return 12; // Fast approximation double

        // Software completion variants (slower)
        case SQRTOpType::SQRTS_SU:
        case SQRTOpType::SQRTS_SUI:
            return 25; // Software single
        case SQRTOpType::SQRTT_SU:
        case SQRTOpType::SQRTT_SUI:
            return 45; // Software double

        default:
            return 34;
        }
    }

    // Floating point classification
    bool isFloatingPoint() const override
    {
        return (m_format == SQRTFormat::IEEE_SINGLE || m_format == SQRTFormat::IEEE_DOUBLE ||
                m_format == SQRTFormat::VAX_F || m_format == SQRTFormat::VAX_G);
    }

    // Performance-critical accessors
    inline SQRTOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg() const { return m_srcReg; }
    inline SQRTFormat getFormat() const { return m_format; }
    inline RoundingMode getRoundingMode() const { return m_roundingMode; }
    inline SQRTMethod getMethod() const { return m_method; }
    inline double getResult() const { return m_result; }
    inline uint64_t getIntResult() const { return m_intResult; }
    inline uint32_t getIterations() const { return m_iterations; }
    inline uint32_t getPrecision() const { return m_precision; }

    // Performance counters
    inline uint64_t getDomainErrorCount() const { return m_domainErrorCount.load(std::memory_order_relaxed); }
    inline uint64_t getUnderflowCount() const { return m_underflowCount.load(std::memory_order_relaxed); }
    inline uint64_t getOverflowCount() const { return m_overflowCount.load(std::memory_order_relaxed); }
    inline uint64_t getInexactCount() const { return m_inexactCount.load(std::memory_order_relaxed); }
    inline uint64_t getDenormalInputCount() const { return m_denormalInputCount.load(std::memory_order_relaxed); }
    inline uint64_t getNegativeInputCount() const { return m_negativeInputCount.load(std::memory_order_relaxed); }
    inline uint64_t getZeroInputCount() const { return m_zeroInputCount.load(std::memory_order_relaxed); }
    inline uint64_t getInfinityInputCount() const { return m_infinityInputCount.load(std::memory_order_relaxed); }
    inline uint64_t getNanInputCount() const { return m_nanInputCount.load(std::memory_order_relaxed); }
    inline uint64_t getIterationCount() const { return m_iterationCount.load(std::memory_order_relaxed); }

    // Hot path execution support
    inline void setOperand(double operand) { m_operand = operand; }
    inline void setIntOperand(uint64_t operand) { m_intOperand = operand; }
    inline void setMethod(SQRTMethod method) { m_method = method; }
    inline void setRoundingMode(RoundingMode mode) { m_roundingMode = mode; }

    // Special input classification
    inline bool isSpecialInput() const
    {
        if (isFloatingPoint())
        {
            return (std::isnan(m_operand) || std::isinf(m_operand) || m_operand == 0.0 || m_operand < 0.0);
        }
        else
        {
            return (m_intOperand == 0);
        }
    }

  private:
    SQRTFormat determineFormat(SQRTOpType opType) const
    {
        switch (opType)
        {
        case SQRTOpType::SQRTS:
        case SQRTOpType::SQRTS_C:
        case SQRTOpType::SQRTS_M:
        case SQRTOpType::SQRTS_D:
        case SQRTOpType::SQRTS_U:
        case SQRTOpType::SQRTS_SU:
        case SQRTOpType::SQRTS_SUI:
        case SQRTOpType::RSQRTS:
            return SQRTFormat::IEEE_SINGLE;

        case SQRTOpType::SQRTT:
        case SQRTOpType::SQRTT_C:
        case SQRTOpType::SQRTT_M:
        case SQRTOpType::SQRTT_D:
        case SQRTOpType::SQRTT_U:
        case SQRTOpType::SQRTT_SU:
        case SQRTOpType::SQRTT_SUI:
        case SQRTOpType::RSQRTT:
            return SQRTFormat::IEEE_DOUBLE;

        case SQRTOpType::SQRTF:
            return SQRTFormat::VAX_F;
        case SQRTOpType::SQRTG:
            return SQRTFormat::VAX_G;

        case SQRTOpType::ISQRT:
            return SQRTFormat::INTEGER_64;
        case SQRTOpType::ISQRT32:
            return SQRTFormat::INTEGER_32;

        default:
            return SQRTFormat::IEEE_DOUBLE;
        }
    }

    RoundingMode determineRoundingMode(SQRTOpType opType) const
    {
        switch (opType)
        {
        case SQRTOpType::SQRTS_C:
        case SQRTOpType::SQRTT_C:
            return RoundingMode::ROUND_TOWARD_ZERO;
        case SQRTOpType::SQRTS_M:
        case SQRTOpType::SQRTT_M:
            return RoundingMode::ROUND_DOWN;
        case SQRTOpType::SQRTS_D:
        case SQRTOpType::SQRTT_D:
            return RoundingMode::ROUND_DYNAMIC;
        default:
            return RoundingMode::ROUND_NEAREST;
        }
    }

    bool performSQRTOperation()
    {
        if (isFloatingPoint())
        {
            return performFloatingPointSQRT();
        }
        else
        {
            return performIntegerSQRT();
        }
    }

    bool performFloatingPointSQRT()
    {
        // Classify input
        classifyFloatingPointInput();

        // Handle special cases
        if (std::isnan(m_operand))
        {
            m_result = m_operand; // Propagate NaN
            m_nanInputCount.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        if (m_operand < 0.0)
        {
            m_result = std::numeric_limits<double>::quiet_NaN();
            m_negativeInputCount.fetch_add(1, std::memory_order_relaxed);
            m_domainErrorCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Square root of negative number: %f", m_operand);
            return false;
        }

        if (m_operand == 0.0)
        {
            m_result = m_operand; // Preserve sign of zero
            m_zeroInputCount.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        if (std::isinf(m_operand))
        {
            m_result = m_operand; // +inf -> +inf
            m_infinityInputCount.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        // Perform square root based on method
        switch (m_method)
        {
        case SQRTMethod::HARDWARE_NATIVE:
            return performHardwareSQRT();
        case SQRTMethod::NEWTON_RAPHSON:
            return performNewtonRaphsonSQRT();
        case SQRTMethod::LOOKUP_TABLE:
            return performLookupTableSQRT();
        case SQRTMethod::SOFTWARE_LIBRARY:
            return performSoftwareSQRT();
        case SQRTMethod::RECIPROCAL_APPROX:
            return performReciprocalSQRT();
        default:
            return performHardwareSQRT();
        }
    }

    bool performIntegerSQRT()
    {
        if (m_intOperand == 0)
        {
            m_intResult = 0;
            m_zeroInputCount.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        // Integer square root using binary search
        uint64_t operand = (m_format == SQRTFormat::INTEGER_32) ? (m_intOperand & 0xFFFFFFFF) : m_intOperand;

        uint64_t left = 0;
        uint64_t right = operand;
        uint64_t result = 0;
        uint32_t iterations = 0;

        while (left <= right && iterations < 64)
        {
            uint64_t mid = left + (right - left) / 2;
            uint64_t square = mid * mid;

            if (square == operand)
            {
                result = mid;
                break;
            }
            else if (square < operand)
            {
                result = mid;
                left = mid + 1;
            }
            else
            {
                right = mid - 1;
            }
            iterations++;
        }

        m_intResult = result;
        m_iterations = iterations;
        m_iterationCount.fetch_add(iterations, std::memory_order_relaxed);

        DEBUG_LOG("Integer SQRT: %llu -> %llu (%d iterations)", operand, result, iterations);
        return true;
    }

    bool performHardwareSQRT()
    {
        m_result = std::sqrt(m_operand);
        return checkFloatingPointResult();
    }

    bool performNewtonRaphsonSQRT()
    {
        // Newton-Raphson: x_{n+1} = 0.5 * (x_n + a/x_n)
        double x = m_operand * 0.5; // Initial guess
        uint32_t iterations = 0;
        const uint32_t maxIterations = (m_format == SQRTFormat::IEEE_SINGLE) ? 4 : 6;
        const double epsilon = (m_format == SQRTFormat::IEEE_SINGLE) ? 1e-7 : 1e-15;

        for (iterations = 0; iterations < maxIterations; iterations++)
        {
            double x_new = 0.5 * (x + m_operand / x);
            if (std::abs(x_new - x) < epsilon)
            {
                break;
            }
            x = x_new;
        }

        m_result = x;
        m_iterations = iterations;
        m_iterationCount.fetch_add(iterations, std::memory_order_relaxed);

        DEBUG_LOG("Newton-Raphson SQRT: %f -> %f (%d iterations)", m_operand, m_result, iterations);
        return checkFloatingPointResult();
    }

    bool performLookupTableSQRT()
    {
        // Simplified lookup table approach
        m_result = std::sqrt(m_operand);
        m_iterations = 1; // Single table lookup
        return checkFloatingPointResult();
    }

    bool performSoftwareSQRT()
    {
        // Software implementation (falls back to library)
        m_result = std::sqrt(m_operand);
        return checkFloatingPointResult();
    }

    bool performReciprocalSQRT()
    {
        // Reciprocal square root: 1/sqrt(x)
        if (m_operand == 0.0)
        {
            m_result = std::numeric_limits<double>::infinity();
            m_overflowCount.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        m_result = 1.0 / std::sqrt(m_operand);
        return checkFloatingPointResult();
    }

    void classifyFloatingPointInput()
    {
        if (std::isnan(m_operand))
        {
            m_nanInputCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (std::isinf(m_operand))
        {
            m_infinityInputCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (m_operand == 0.0)
        {
            m_zeroInputCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (m_operand < 0.0)
        {
            m_negativeInputCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (std::abs(m_operand) < std::numeric_limits<double>::min())
        {
            m_denormalInputCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    bool checkFloatingPointResult()
    {
        if (std::isnan(m_result))
        {
            m_domainErrorCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        if (std::isinf(m_result))
        {
            m_overflowCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Check for underflow
        if (m_result != 0.0 && std::abs(m_result) < std::numeric_limits<double>::min())
        {
            m_underflowCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Check for inexact result (simplified)
        double exact_check = m_result * m_result;
        if (std::abs(exact_check - m_operand) > std::numeric_limits<double>::epsilon())
        {
            m_inexactCount.fetch_add(1, std::memory_order_relaxed);
        }

        return true;
    }

  private:
     SQRTOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg;
     SQRTFormat m_format;

    RoundingMode m_roundingMode;
    SQRTMethod m_method;
    double m_operand;
    double m_result;
    uint64_t m_intOperand;
    uint64_t m_intResult;
    uint32_t m_iterations;
    uint32_t m_precision;

    std::atomic<uint64_t> m_domainErrorCount;
    std::atomic<uint64_t> m_underflowCount;
    std::atomic<uint64_t> m_overflowCount;
    std::atomic<uint64_t> m_inexactCount;
    std::atomic<uint64_t> m_denormalInputCount;
    std::atomic<uint64_t> m_negativeInputCount;
    std::atomic<uint64_t> m_zeroInputCount;
    std::atomic<uint64_t> m_infinityInputCount;
    std::atomic<uint64_t> m_nanInputCount;
    std::atomic<uint64_t> m_iterationCount;

    // Prevent copying for performance
    alphaSQRTInstruction(const alphaSQRTInstruction &) = delete;
    alphaSQRTInstruction &operator=(const alphaSQRTInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaCallPALInstruction : public alphaInstructionBase
{
  public:
    enum class PALFunction : uint32_t
    {
        // System control functions
        HALT = 0x0000,    // Halt processor
        RESTART = 0x0001, // Restart processor
        DRAINA = 0x0002,  // Drain write buffers
        REBOOT = 0x0003,  // Reboot system
        INITPAL = 0x0004, // Initialize PAL
        WRENTRY = 0x0005, // Write system entry address
        SWPIRQL = 0x0006, // Swap IRQ level
        RDIRQL = 0x0007,  // Read IRQ level
        DI = 0x0008,      // Disable interrupts
        EI = 0x0009,      // Enable interrupts
        SWPPAL = 0x000A,  // Swap PAL base

        // Memory management functions
        SSIR = 0x000C,      // Set software interrupt request
        CSIR = 0x000D,      // Clear software interrupt request
        RFE = 0x000E,       // Return from exception
        RETSYS = 0x000F,    // Return from system call
        SWPCTX = 0x0030,    // Swap process context
        WRVAL = 0x0031,     // Write system value
        RDVAL = 0x0032,     // Read system value
        TBI = 0x0033,       // Translation buffer invalidate
        WRENT = 0x0034,     // Write system entry
        SWPIPL = 0x0035,    // Swap IPL
        RDPS = 0x0036,      // Read processor status
        WRKGP = 0x0037,     // Write kernel global pointer
        WRUSP = 0x0038,     // Write user stack pointer
        WRPERFMON = 0x0039, // Write performance monitor
        RDUSP = 0x003A,     // Read user stack pointer

        // VMS-specific functions
        PROBER = 0x003B,   // Probe read access
        PROBEW = 0x003C,   // Probe write access
        RDTHREAD = 0x003E, // Read thread pointer
        TBISYNC = 0x003F,  // TBI synchronize
        WRTHREAD = 0x0040, // Write thread pointer

        // Cache and TLB management
        TBIA = 0x0041,        // Translation buffer invalidate all
        TBIS = 0x0042,        // Translation buffer invalidate single
        TBISYNC_ALT = 0x0043, // Alternative TBI sync

        // Interrupt and exception handling
        GENTRAP = 0x00AA,  // Generate trap
        RDUNIQUE = 0x009E, // Read unique value
        WRUNIQUE = 0x009F, // Write unique value

        // Digital UNIX specific
        BPT = 0x0080,     // Breakpoint trap
        BUGCHK = 0x0081,  // Bug check
        CALLSYS = 0x0083, // System call
        IMB = 0x0086,     // Instruction memory barrier

        // OpenVMS specific
        CFLUSH = 0x0001,     // Cache flush
        DRAINA_VMS = 0x0002, // VMS drain
        LDQP = 0x0003,       // Load quad physical
        STQP = 0x0004,       // Store quad physical
        SWPCTX_VMS = 0x0005, // VMS swap context
        MFPR_ASN = 0x0006,   // Move from processor register ASN
        MTPR_ASTEN = 0x0007, // Move to processor register ASTEN
        MTPR_ASTSR = 0x0008, // Move to processor register ASTSR
        CSERVE = 0x0009,     // Console service
        SWPPAL_VMS = 0x000A, // VMS swap PAL
        MFPR_FEN = 0x000B,   // Move from processor register FEN
        MTPR_FEN = 0x000C,   // Move to processor register FEN
        MTPR_IPIR = 0x000D,  // Move to processor register IPIR
        MFPR_IPL = 0x000E,   // Move from processor register IPL
        MTPR_IPL = 0x000F,   // Move to processor register IPL
        MFPR_MCES = 0x0010,  // Move from processor register MCES
        MTPR_MCES = 0x0011,  // Move to processor register MCES
        MFPR_PCBB = 0x0012,  // Move from processor register PCBB
        MFPR_PRBR = 0x0013,  // Move from processor register PRBR
        MTPR_PRBR = 0x0014,  // Move to processor register PRBR
        MFPR_PTBR = 0x0015,  // Move from processor register PTBR
        MFPR_SCBB = 0x0016,  // Move from processor register SCBB
        MTPR_SCBB = 0x0017,  // Move to processor register SCBB
        MTPR_SIRR = 0x0018,  // Move to processor register SIRR
        MFPR_SISR = 0x0019,  // Move from processor register SISR
        MFPR_TBCHK = 0x001A, // Move from processor register TBCHK
        MTPR_TBIA = 0x001B,  // Move to processor register TBIA
        MTPR_TBIAP = 0x001C, // Move to processor register TBIAP
        MTPR_TBIS = 0x001D,  // Move to processor register TBIS
        MFPR_ESP = 0x001E,   // Move from processor register ESP
        MTPR_ESP = 0x001F,   // Move to processor register ESP
        MFPR_SSP = 0x0020,   // Move from processor register SSP
        MTPR_SSP = 0x0021,   // Move to processor register SSP
        MFPR_USP = 0x0022,   // Move from processor register USP
        MTPR_USP = 0x0023,   // Move to processor register USP
        MTPR_TBISD = 0x0024, // Move to processor register TBISD
        MTPR_TBISI = 0x0025, // Move to processor register TBISI
        MFPR_ASTEN = 0x0026, // Move from processor register ASTEN
        MFPR_ASTSR = 0x0027, // Move from processor register ASTSR

        // Performance monitoring
        MTPR_PERFMON = 0x002A, // Move to performance monitor
        MFPR_PERFMON = 0x002B, // Move from performance monitor

        // Unknown/invalid function
        UNKNOWN = 0xFFFFFFFF
    };

    enum class PrivilegeLevel : uint8_t
    {
        KERNEL = 0,     // Kernel mode (most privileged)
        EXECUTIVE = 1,  // Executive mode
        SUPERVISOR = 2, // Supervisor mode
        USER = 3        // User mode (least privileged)
    };

    enum class PALMode : uint8_t
    {
        VMS = 0,    // OpenVMS PAL
        UNIX = 1,   // Digital UNIX PAL
        NT = 2,     // Windows NT PAL
        CONSOLE = 3 // Console/Firmware PAL
    };

    explicit alphaCallPALInstruction(uint32_t opcode, PALFunction palFunction)
        : alphaInstructionBase(opcode), m_palFunction(palFunction), m_palMode(PALMode::UNIX),
          m_currentPrivilegeLevel(PrivilegeLevel::USER), m_targetPrivilegeLevel(PrivilegeLevel::KERNEL),
          m_argumentValue(0), m_returnValue(0), m_exceptionCode(0), m_palBaseAddress(0), m_entryPointOffset(0),
          m_privilegeViolationCount(0), m_invalidFunctionCount(0), m_systemCallCount(0), m_contextSwitchCount(0),
          m_tlbInvalidateCount(0), m_cacheFlushCount(0), m_interruptDisableCount(0), m_exceptionCount(0),
          m_performanceMonitorCount(0), m_memoryBarrierCount(0)
    {
        DEBUG_LOG("alphaCallPALInstruction created - PAL Function: 0x%08X (%s)", static_cast<uint32_t>(palFunction),
                  getPALFunctionName(palFunction).c_str());
    }

    virtual ~alphaCallPALInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        if (!checkPrivilegeLevel())
        {
            return false;
        }

        bool success = executePALFunction();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding CALL_PAL instruction opcode: 0x%08X, function: 0x%08X", getOpcode(),
                  static_cast<uint32_t>(m_palFunction));

        uint32_t opcode = getOpcode();

        // CALL_PAL format: opcode(6) + function(26)
        uint32_t functionCode = opcode & 0x3FFFFFF; // 26-bit function field

        // Map function code to PAL function type
        switch (functionCode)
        {
        case 0x0000:
            m_palFunction = PALFunction::HALT;
            break;
        case 0x0001:
            m_palFunction = PALFunction::RESTART;
            break;
        case 0x0002:
            m_palFunction = PALFunction::DRAINA;
            break;
        case 0x0003:
            m_palFunction = PALFunction::REBOOT;
            break;
        case 0x0004:
            m_palFunction = PALFunction::INITPAL;
            break;
        case 0x0005:
            m_palFunction = PALFunction::WRENTRY;
            break;
        case 0x0006:
            m_palFunction = PALFunction::SWPIRQL;
            break;
        case 0x0007:
            m_palFunction = PALFunction::RDIRQL;
            break;
        case 0x0008:
            m_palFunction = PALFunction::DI;
            break;
        case 0x0009:
            m_palFunction = PALFunction::EI;
            break;
        case 0x000A:
            m_palFunction = PALFunction::SWPPAL;
            break;
        case 0x000C:
            m_palFunction = PALFunction::SSIR;
            break;
        case 0x000D:
            m_palFunction = PALFunction::CSIR;
            break;
        case 0x000E:
            m_palFunction = PALFunction::RFE;
            break;
        case 0x000F:
            m_palFunction = PALFunction::RETSYS;
            break;
        case 0x0030:
            m_palFunction = PALFunction::SWPCTX;
            break;
        case 0x0031:
            m_palFunction = PALFunction::WRVAL;
            break;
        case 0x0032:
            m_palFunction = PALFunction::RDVAL;
            break;
        case 0x0033:
            m_palFunction = PALFunction::TBI;
            break;
        case 0x0034:
            m_palFunction = PALFunction::WRENT;
            break;
        case 0x0035:
            m_palFunction = PALFunction::SWPIPL;
            break;
        case 0x0036:
            m_palFunction = PALFunction::RDPS;
            break;
        case 0x0037:
            m_palFunction = PALFunction::WRKGP;
            break;
        case 0x0038:
            m_palFunction = PALFunction::WRUSP;
            break;
        case 0x0039:
            m_palFunction = PALFunction::WRPERFMON;
            break;
        case 0x003A:
            m_palFunction = PALFunction::RDUSP;
            break;
        case 0x0080:
            m_palFunction = PALFunction::BPT;
            break;
        case 0x0081:
            m_palFunction = PALFunction::BUGCHK;
            break;
        case 0x0083:
            m_palFunction = PALFunction::CALLSYS;
            break;
        case 0x0086:
            m_palFunction = PALFunction::IMB;
            break;
        case 0x00AA:
            m_palFunction = PALFunction::GENTRAP;
            break;
        case 0x009E:
            m_palFunction = PALFunction::RDUNIQUE;
            break;
        case 0x009F:
            m_palFunction = PALFunction::WRUNIQUE;
            break;
        default:
            DEBUG_LOG("Unknown PAL function code: 0x%08X", functionCode);
            m_palFunction = PALFunction::UNKNOWN;
            break;
        }

        // Determine target privilege level
        if (m_palFunction == PALFunction::UNKNOWN)
        {
            m_targetPrivilegeLevel = PrivilegeLevel::USER;
        }
        else
        {
            m_targetPrivilegeLevel = PrivilegeLevel::KERNEL; // Most PAL functions require kernel privilege
        }

        DEBUG_LOG("CALL_PAL decoded - Function: 0x%08X (%s)", static_cast<uint32_t>(m_palFunction),
                  getPALFunctionName(m_palFunction).c_str());
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_palFunction)
        {
        // Fast operations
        case PALFunction::RDPS:
        case PALFunction::RDIRQL:
        case PALFunction::RDUSP:
        case PALFunction::RDVAL:
        case PALFunction::RDUNIQUE:
        case PALFunction::RDTHREAD:
            return 5;

        // Medium operations
        case PALFunction::EI:
        case PALFunction::DI:
        case PALFunction::SWPIRQL:
        case PALFunction::SWPIPL:
        case PALFunction::WRUSP:
        case PALFunction::WRVAL:
        case PALFunction::WRUNIQUE:
        case PALFunction::WRTHREAD:
        case PALFunction::WRKGP:
            return 10;

        // TLB operations
        case PALFunction::TBI:
        case PALFunction::TBIS:
        case PALFunction::TBIA:
        case PALFunction::TBISYNC:
        case PALFunction::TBISYNC_ALT:
            return 15;

        // Cache operations
        case PALFunction::DRAINA:
        case PALFunction::CFLUSH:
        case PALFunction::IMB:
            return 20;

        // Context switching
        case PALFunction::SWPCTX:
        case PALFunction::SWPCTX_VMS:
            return 50;

        // System calls
        case PALFunction::CALLSYS:
        case PALFunction::RETSYS:
        case PALFunction::RFE:
            return 25;

        // PAL swapping
        case PALFunction::SWPPAL:
        case PALFunction::SWPPAL_VMS:
        case PALFunction::INITPAL:
            return 100;

        // System control
        case PALFunction::HALT:
        case PALFunction::RESTART:
        case PALFunction::REBOOT:
            return 200;

        // Performance monitoring
        case PALFunction::WRPERFMON:
        case PALFunction::MTPR_PERFMON:
        case PALFunction::MFPR_PERFMON:
            return 8;

        // Exception and trap handling
        case PALFunction::GENTRAP:
        case PALFunction::BPT:
        case PALFunction::BUGCHK:
            return 30;

        // Memory probing
        case PALFunction::PROBER:
        case PALFunction::PROBEW:
            return 12;

        default:
            return 25; // Default PAL call overhead
        }
    }

    // Performance-critical accessors
    inline PALFunction getPALFunction() const { return m_palFunction; }
    inline PALMode getPALMode() const { return m_palMode; }
    inline PrivilegeLevel getCurrentPrivilegeLevel() const { return m_currentPrivilegeLevel; }
    inline PrivilegeLevel getTargetPrivilegeLevel() const { return m_targetPrivilegeLevel; }
    inline uint64_t getArgumentValue() const { return m_argumentValue; }
    inline uint64_t getReturnValue() const { return m_returnValue; }
    inline uint32_t getExceptionCode() const { return m_exceptionCode; }
    inline uint64_t getPALBaseAddress() const { return m_palBaseAddress; }
    inline uint32_t getEntryPointOffset() const { return m_entryPointOffset; }

    // Performance counters
    inline uint64_t getPrivilegeViolationCount() const
    {
        return m_privilegeViolationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getInvalidFunctionCount() const { return m_invalidFunctionCount.load(std::memory_order_relaxed); }
    inline uint64_t getSystemCallCount() const { return m_systemCallCount.load(std::memory_order_relaxed); }
    inline uint64_t getContextSwitchCount() const { return m_contextSwitchCount.load(std::memory_order_relaxed); }
    inline uint64_t getTlbInvalidateCount() const { return m_tlbInvalidateCount.load(std::memory_order_relaxed); }
    inline uint64_t getCacheFlushCount() const { return m_cacheFlushCount.load(std::memory_order_relaxed); }
    inline uint64_t getInterruptDisableCount() const { return m_interruptDisableCount.load(std::memory_order_relaxed); }
    inline uint64_t getExceptionCount() const { return m_exceptionCount.load(std::memory_order_relaxed); }
    inline uint64_t getPerformanceMonitorCount() const
    {
        return m_performanceMonitorCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getMemoryBarrierCount() const { return m_memoryBarrierCount.load(std::memory_order_relaxed); }

    // Classification
    inline bool isSystemCall() const { return (m_palFunction == PALFunction::CALLSYS); }

    inline bool isPrivileged() const { return (m_targetPrivilegeLevel == PrivilegeLevel::KERNEL); }

    inline bool isMemoryManagement() const
    {
        return (m_palFunction >= PALFunction::TBI && m_palFunction <= PALFunction::TBISYNC_ALT) ||
               (m_palFunction >= PALFunction::MTPR_TBIA && m_palFunction <= PALFunction::MTPR_TBISI);
    }

    inline bool isCacheOperation() const
    {
        return (m_palFunction == PALFunction::DRAINA || m_palFunction == PALFunction::CFLUSH ||
                m_palFunction == PALFunction::IMB || m_palFunction == PALFunction::DRAINA_VMS);
    }

    inline bool isInterruptControl() const
    {
        return (m_palFunction == PALFunction::EI || m_palFunction == PALFunction::DI ||
                m_palFunction == PALFunction::SWPIRQL || m_palFunction == PALFunction::SWPIPL);
    }

    // Hot path execution support
    inline void setPALMode(PALMode mode) { m_palMode = mode; }
    inline void setCurrentPrivilegeLevel(PrivilegeLevel level) { m_currentPrivilegeLevel = level; }
    inline void setArgumentValue(uint64_t value) { m_argumentValue = value; }
    inline void setPALBaseAddress(uint64_t address) { m_palBaseAddress = address; }

  private:
    bool checkPrivilegeLevel()
    {
        // Most PAL functions require kernel privilege
        if (m_targetPrivilegeLevel == PrivilegeLevel::KERNEL && m_currentPrivilegeLevel != PrivilegeLevel::KERNEL)
        {

            m_privilegeViolationCount.fetch_add(1, std::memory_order_relaxed);
            m_exceptionCode = 0x0004; // Privilege violation
            DEBUG_LOG("PAL privilege violation: function 0x%08X requires kernel mode",
                      static_cast<uint32_t>(m_palFunction));
            return false;
        }

        return true;
    }

    bool executePALFunction()
    {
        switch (m_palFunction)
        {
        // System control
        case PALFunction::HALT:
            return executeHalt();
        case PALFunction::RESTART:
            return executeRestart();
        case PALFunction::REBOOT:
            return executeReboot();
        case PALFunction::INITPAL:
            return executeInitPAL();

        // Interrupt control
        case PALFunction::EI:
            return executeEnableInterrupts();
        case PALFunction::DI:
            return executeDisableInterrupts();
        case PALFunction::SWPIRQL:
        case PALFunction::SWPIPL:
            return executeSwapIPL();
        case PALFunction::RDIRQL:
            return executeReadIPL();

        // Memory management
        case PALFunction::TBI:
        case PALFunction::TBIS:
        case PALFunction::TBIA:
            return executeTLBInvalidate();
        case PALFunction::TBISYNC:
        case PALFunction::TBISYNC_ALT:
            return executeTLBSync();

        // Context switching
        case PALFunction::SWPCTX:
        case PALFunction::SWPCTX_VMS:
            return executeSwapContext();

        // System calls
        case PALFunction::CALLSYS:
            return executeSystemCall();
        case PALFunction::RETSYS:
            return executeReturnFromSystemCall();
        case PALFunction::RFE:
            return executeReturnFromException();

        // Cache operations
        case PALFunction::DRAINA:
        case PALFunction::DRAINA_VMS:
            return executeDrainWriteBuffers();
        case PALFunction::CFLUSH:
            return executeCacheFlush();
        case PALFunction::IMB:
            return executeInstructionMemoryBarrier();

        // Register access
        case PALFunction::RDPS:
            return executeReadProcessorStatus();
        case PALFunction::RDUSP:
        case PALFunction::MFPR_USP:
            return executeReadUserStackPointer();
        case PALFunction::WRUSP:
        case PALFunction::MTPR_USP:
            return executeWriteUserStackPointer();
        case PALFunction::RDVAL:
            return executeReadSystemValue();
        case PALFunction::WRVAL:
            return executeWriteSystemValue();
        case PALFunction::RDUNIQUE:
            return executeReadUniqueValue();
        case PALFunction::WRUNIQUE:
            return executeWriteUniqueValue();
        case PALFunction::RDTHREAD:
            return executeReadThreadPointer();
        case PALFunction::WRTHREAD:
            return executeWriteThreadPointer();

        // Performance monitoring
        case PALFunction::WRPERFMON:
        case PALFunction::MTPR_PERFMON:
            return executeWritePerformanceMonitor();
        case PALFunction::MFPR_PERFMON:
            return executeReadPerformanceMonitor();

        // Exception handling
        case PALFunction::GENTRAP:
            return executeGenerateTrap();
        case PALFunction::BPT:
            return executeBreakpoint();
        case PALFunction::BUGCHK:
            return executeBugCheck();

        // Memory probing
        case PALFunction::PROBER:
            return executeProbeRead();
        case PALFunction::PROBEW:
            return executeProbeWrite();

        default:
            m_invalidFunctionCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Unknown PAL function: 0x%08X", static_cast<uint32_t>(m_palFunction));
            return false;
        }
    }

    // PAL function implementations (simplified for hot path performance)
    bool executeHalt()
    {
        DEBUG_LOG("PAL HALT executed");
        return true;
    }

    bool executeRestart()
    {
        DEBUG_LOG("PAL RESTART executed");
        return true;
    }

    bool executeReboot()
    {
        DEBUG_LOG("PAL REBOOT executed");
        return true;
    }

    bool executeInitPAL()
    {
        DEBUG_LOG("PAL INITPAL executed");
        return true;
    }

    bool executeEnableInterrupts()
    {
        DEBUG_LOG("PAL Enable Interrupts executed");
        return true;
    }

    bool executeDisableInterrupts()
    {
        m_interruptDisableCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Disable Interrupts executed");
        return true;
    }

    bool executeSwapIPL()
    {
        DEBUG_LOG("PAL Swap IPL executed, arg: 0x%016llX", m_argumentValue);
        return true;
    }

    bool executeReadIPL()
    {
        m_returnValue = 0x07; // Example IPL value
        DEBUG_LOG("PAL Read IPL executed, result: 0x%016llX", m_returnValue);
        return true;
    }

    bool executeTLBInvalidate()
    {
        m_tlbInvalidateCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL TLB Invalidate executed");
        return true;
    }

    bool executeTLBSync()
    {
        DEBUG_LOG("PAL TLB Sync executed");
        return true;
    }

    bool executeSwapContext()
    {
        m_contextSwitchCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Swap Context executed");
        return true;
    }

    bool executeSystemCall()
    {
        m_systemCallCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL System Call executed, number: 0x%016llX", m_argumentValue);
        return true;
    }

    bool executeReturnFromSystemCall()
    {
        DEBUG_LOG("PAL Return from System Call executed");
        return true;
    }

    bool executeReturnFromException()
    {
        DEBUG_LOG("PAL Return from Exception executed");
        return true;
    }

    bool executeDrainWriteBuffers()
    {
        m_memoryBarrierCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Drain Write Buffers executed");
        return true;
    }

    bool executeCacheFlush()
    {
        m_cacheFlushCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Cache Flush executed");
        return true;
    }

    bool executeInstructionMemoryBarrier()
    {
        m_memoryBarrierCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Instruction Memory Barrier executed");
        return true;
    }

    bool executeReadProcessorStatus()
    {
        m_returnValue = 0x0008; // Example PS value
        DEBUG_LOG("PAL Read Processor Status executed, result: 0x%016llX", m_returnValue);
        return true;
    }

    bool executeReadUserStackPointer()
    {
        m_returnValue = 0x7FFFFFFF0000ULL; // Example USP value
        DEBUG_LOG("PAL Read USP executed, result: 0x%016llX", m_returnValue);
        return true;
    }

    bool executeWriteUserStackPointer()
    {
        DEBUG_LOG("PAL Write USP executed, value: 0x%016llX", m_argumentValue);
        return true;
    }

    bool executeReadSystemValue()
    {
        m_returnValue = 0x0; // System-dependent value
        DEBUG_LOG("PAL Read System Value executed");
        return true;
    }

    bool executeWriteSystemValue()
    {
        DEBUG_LOG("PAL Write System Value executed, value: 0x%016llX", m_argumentValue);
        return true;
    }

    bool executeReadUniqueValue()
    {
        m_returnValue = 0x123456789ABCDEFULL; // Example unique value
        DEBUG_LOG("PAL Read Unique executed, result: 0x%016llX", m_returnValue);
        return true;
    }

    bool executeWriteUniqueValue()
    {
        DEBUG_LOG("PAL Write Unique executed, value: 0x%016llX", m_argumentValue);
        return true;
    }

    bool executeReadThreadPointer()
    {
        m_returnValue = 0x0; // Thread pointer value
        DEBUG_LOG("PAL Read Thread Pointer executed");
        return true;
    }

    bool executeWriteThreadPointer()
    {
        DEBUG_LOG("PAL Write Thread Pointer executed, value: 0x%016llX", m_argumentValue);
        return true;
    }

    bool executeWritePerformanceMonitor()
    {
        m_performanceMonitorCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Write Performance Monitor executed");
        return true;
    }

    bool executeReadPerformanceMonitor()
    {
        m_performanceMonitorCount.fetch_add(1, std::memory_order_relaxed);
        m_returnValue = 0x0; // Performance counter value
        DEBUG_LOG("PAL Read Performance Monitor executed");
        return true;
    }

    bool executeGenerateTrap()
    {
        m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Generate Trap executed");
        return true;
    }

    bool executeBreakpoint()
    {
        m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Breakpoint executed");
        return true;
    }

    bool executeBugCheck()
    {
        m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
        DEBUG_LOG("PAL Bug Check executed");
        return true;
    }

    bool executeProbeRead()
    {
        m_returnValue = 0x1; // Access allowed
        DEBUG_LOG("PAL Probe Read executed");
        return true;
    }

    bool executeProbeWrite()
    {
        m_returnValue = 0x1; // Access allowed
        DEBUG_LOG("PAL Probe Write executed");
        return true;
    }

    std::string getPALFunctionName(PALFunction function) const
    {
        switch (function)
        {
        case PALFunction::HALT:
            return "HALT";
        case PALFunction::RESTART:
            return "RESTART";
        case PALFunction::DRAINA:
            return "DRAINA";
        case PALFunction::REBOOT:
            return "REBOOT";
        case PALFunction::INITPAL:
            return "INITPAL";
        case PALFunction::EI:
            return "EI";
        case PALFunction::DI:
            return "DI";
        case PALFunction::SWPIRQL:
            return "SWPIRQL";
        case PALFunction::RDIRQL:
            return "RDIRQL";
        case PALFunction::CALLSYS:
            return "CALLSYS";
        case PALFunction::IMB:
            return "IMB";
        case PALFunction::BPT:
            return "BPT";
        case PALFunction::BUGCHK:
            return "BUGCHK";
        case PALFunction::GENTRAP:
            return "GENTRAP";
        default:
            return "UNKNOWN";
        }
    }

  private:
    mutable PALFunction m_palFunction;
    PALMode m_palMode;
    PrivilegeLevel m_currentPrivilegeLevel;
    PrivilegeLevel m_targetPrivilegeLevel;

    uint64_t m_argumentValue;
    uint64_t m_returnValue;
    uint32_t m_exceptionCode;
    uint64_t m_palBaseAddress;
    uint32_t m_entryPointOffset;

    std::atomic<uint64_t> m_privilegeViolationCount;
    std::atomic<uint64_t> m_invalidFunctionCount;
    std::atomic<uint64_t> m_systemCallCount;
    std::atomic<uint64_t> m_contextSwitchCount;
    std::atomic<uint64_t> m_tlbInvalidateCount;
    std::atomic<uint64_t> m_cacheFlushCount;
    std::atomic<uint64_t> m_interruptDisableCount;
    std::atomic<uint64_t> m_exceptionCount;
    std::atomic<uint64_t> m_performanceMonitorCount;
    std::atomic<uint64_t> m_memoryBarrierCount;

    // Prevent copying for performance
    alphaCallPALInstruction(const alphaCallPALInstruction &) = delete;
    alphaCallPALInstruction &operator=(const alphaCallPALInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaMultimediaInstruction : public alphaInstructionBase
{
  public:
    enum class MultimediaOpType : uint16_t
    {
        // Mask operations (clear specified bytes)
        MSKBL = 0x12, // Mask byte low
        MSKWL = 0x32, // Mask word low
        MSKLL = 0x52, // Mask longword low
        MSKQL = 0x72, // Mask quadword low
        MSKBH = 0x92, // Mask byte high
        MSKWH = 0xB2, // Mask word high
        MSKLH = 0xD2, // Mask longword high
        MSKQH = 0xF2, // Mask quadword high

        // Insert operations (insert bytes into specified positions)
        INSBL = 0x1B, // Insert byte low
        INSWL = 0x3B, // Insert word low
        INSLL = 0x5B, // Insert longword low
        INSQL = 0x7B, // Insert quadword low
        INSBH = 0x9B, // Insert byte high
        INSWH = 0xBB, // Insert word high
        INSLH = 0xDB, // Insert longword high
        INSQH = 0xFB, // Insert quadword high

        // Extract operations (extract bytes from specified positions)
        EXTBL = 0x06, // Extract byte low
        EXTWL = 0x26, // Extract word low
        EXTLL = 0x46, // Extract longword low
        EXTQL = 0x66, // Extract quadword low
        EXTBH = 0x86, // Extract byte high
        EXTWH = 0xA6, // Extract word high
        EXTLH = 0xC6, // Extract longword high
        EXTQH = 0xE6, // Extract quadword high

        // Zero byte operations
        ZAP = 0x30,    // Zero bytes as specified by mask
        ZAPNOT = 0x31, // Zero bytes NOT specified by mask

        // Pack/Unpack operations for pixel data
        PKWB = 0x34,   // Pack word to bytes
        UNPKBW = 0x35, // Unpack bytes to words
        UNPKBL = 0x36, // Unpack bytes to longwords

        // Pixel error calculation (for motion estimation)
        PERR = 0x31, // Pixel error

        // Multimedia arithmetic extensions
        ADDLV = 0x40, // Add longword vector (SIMD)
        SUBLV = 0x49, // Subtract longword vector
        MULLV = 0x48, // Multiply longword vector

        // Byte manipulation for graphics
        BYTESWAP = 0x50, // Byte swap operations
        BYTEREPL = 0x51, // Byte replication
        BYTEMIN = 0x52,  // Byte minimum
        BYTEMAX = 0x53,  // Byte maximum

        // Color space conversion helpers
        RGB2YUV = 0x60, // RGB to YUV conversion
        YUV2RGB = 0x61, // YUV to RGB conversion

        // Bit field operations
        BFEXT = 0x70, // Bit field extract
        BFINS = 0x71, // Bit field insert
        BFCLR = 0x72, // Bit field clear
        BFSET = 0x73, // Bit field set

        // Alpha blending operations
        ABLEND = 0x80, // Alpha blending
        AMIX = 0x81,   // Alpha mixing

        // Texture operations
        TEXLOD = 0x90,  // Texture level of detail calculation
        TEXFILT = 0x91, // Texture filtering

        // Vector operations
        VADD = 0xA0,   // Vector add
        VSUB = 0xA1,   // Vector subtract
        VMUL = 0xA2,   // Vector multiply
        VDOT = 0xA3,   // Vector dot product
        VCROSS = 0xA4, // Vector cross product
        VNORM = 0xA5,  // Vector normalize

        // Unknown operation
        UNKNOWN = 0xFFFF
    };

    enum class DataSize : uint8_t
    {
        BYTE = 1,     // 8-bit operations
        WORD = 2,     // 16-bit operations
        LONGWORD = 4, // 32-bit operations
        QUADWORD = 8  // 64-bit operations
    };

    enum class OperandFormat : uint8_t
    {
        SCALAR = 0,     // Single value operations
        VECTOR_2 = 1,   // 2-element vector
        VECTOR_4 = 2,   // 4-element vector
        VECTOR_8 = 3,   // 8-element vector
        MATRIX_2X2 = 4, // 2x2 matrix
        MATRIX_4X4 = 5  // 4x4 matrix
    };

    explicit alphaMultimediaInstruction(uint32_t opcode, MultimediaOpType opType, uint8_t destReg, uint8_t srcReg1,
                                        uint8_t srcReg2)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_immediate(0), m_useImmediate(false), m_dataSize(determineDataSize(opType)),
          m_operandFormat(OperandFormat::SCALAR), m_operand1(0), m_operand2(0), m_result(0), m_mask(0),
          m_shiftAmount(0), m_byteMaskOperationCount(0), m_packUnpackCount(0), m_pixelOperationCount(0),
          m_vectorOperationCount(0), m_bitFieldOperationCount(0), m_alphaBlendCount(0), m_textureOperationCount(0),
          m_colorConversionCount(0), m_simdOperationCount(0), m_overflowCount(0)
    {
        DEBUG_LOG("alphaMultimediaInstruction created - OpType: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2);
    }

    // Immediate mode constructor
    explicit alphaMultimediaInstruction(uint32_t opcode, MultimediaOpType opType, uint8_t destReg, uint8_t srcReg1,
                                        uint8_t immediate)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(0),
          m_immediate(immediate), m_useImmediate(true), m_dataSize(determineDataSize(opType)),
          m_operandFormat(OperandFormat::SCALAR), m_operand1(0), m_operand2(0), m_result(0), m_mask(0),
          m_shiftAmount(0), m_byteMaskOperationCount(0), m_packUnpackCount(0), m_pixelOperationCount(0),
          m_vectorOperationCount(0), m_bitFieldOperationCount(0), m_alphaBlendCount(0), m_textureOperationCount(0),
          m_colorConversionCount(0), m_simdOperationCount(0), m_overflowCount(0)
    {
        DEBUG_LOG("alphaMultimediaInstruction created (immediate) - OpType: 0x%04X, Dest: R%d, Src: R%d, Imm: %d",
                  static_cast<int>(opType), destReg, srcReg1, immediate);
    }

    virtual ~alphaMultimediaInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performMultimediaOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding multimedia instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields based on Alpha operate format
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        bool isLiteral = (opcode >> 12) & 0x1;
        uint8_t function = (opcode >> 5) & 0x7F;
        uint8_t rc = opcode & 0x1F;

        m_srcReg1 = ra;
        m_destReg = rc;

        if (isLiteral)
        {
            m_immediate = rb;
            m_useImmediate = true;
            m_srcReg2 = 0;
        }
        else
        {
            m_srcReg2 = rb;
            m_useImmediate = false;
        }

        // Determine multimedia operation type
        switch (primaryOpcode)
        {
        case 0x12: // Byte manipulation operations
            switch (function)
            {
            case 0x12:
                m_opType = MultimediaOpType::MSKBL;
                break;
            case 0x32:
                m_opType = MultimediaOpType::MSKWL;
                break;
            case 0x52:
                m_opType = MultimediaOpType::MSKLL;
                break;
            case 0x72:
                m_opType = MultimediaOpType::MSKQL;
                break;
            case 0x92:
                m_opType = MultimediaOpType::MSKBH;
                break;
            case 0xB2:
                m_opType = MultimediaOpType::MSKWH;
                break;
            case 0xD2:
                m_opType = MultimediaOpType::MSKLH;
                break;
            case 0xF2:
                m_opType = MultimediaOpType::MSKQH;
                break;
            case 0x1B:
                m_opType = MultimediaOpType::INSBL;
                break;
            case 0x3B:
                m_opType = MultimediaOpType::INSWL;
                break;
            case 0x5B:
                m_opType = MultimediaOpType::INSLL;
                break;
            case 0x7B:
                m_opType = MultimediaOpType::INSQL;
                break;
            case 0x9B:
                m_opType = MultimediaOpType::INSBH;
                break;
            case 0xBB:
                m_opType = MultimediaOpType::INSWH;
                break;
            case 0xDB:
                m_opType = MultimediaOpType::INSLH;
                break;
            case 0xFB:
                m_opType = MultimediaOpType::INSQH;
                break;
            case 0x06:
                m_opType = MultimediaOpType::EXTBL;
                break;
            case 0x26:
                m_opType = MultimediaOpType::EXTWL;
                break;
            case 0x46:
                m_opType = MultimediaOpType::EXTLL;
                break;
            case 0x66:
                m_opType = MultimediaOpType::EXTQL;
                break;
            case 0x86:
                m_opType = MultimediaOpType::EXTBH;
                break;
            case 0xA6:
                m_opType = MultimediaOpType::EXTWH;
                break;
            case 0xC6:
                m_opType = MultimediaOpType::EXTLH;
                break;
            case 0xE6:
                m_opType = MultimediaOpType::EXTQH;
                break;
            case 0x30:
                m_opType = MultimediaOpType::ZAP;
                break;
            case 0x31:
                m_opType = MultimediaOpType::ZAPNOT;
                break;
            default:
                DEBUG_LOG("Unknown multimedia function: 0x%02X", function);
                m_opType = MultimediaOpType::ZAP;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown multimedia primary opcode: 0x%02X", primaryOpcode);
            m_opType = MultimediaOpType::UNKNOWN;
            break;
        }

        // Set operand format based on operation
        m_operandFormat = OperandFormat::SCALAR; // Most operations are scalar

        DEBUG_LOG("Multimedia instruction decoded - Type: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(m_opType), m_destReg, m_srcReg1, m_srcReg2);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Simple mask operations
        case MultimediaOpType::MSKBL:
        case MultimediaOpType::MSKWL:
        case MultimediaOpType::MSKLL:
        case MultimediaOpType::MSKQL:
        case MultimediaOpType::MSKBH:
        case MultimediaOpType::MSKWH:
        case MultimediaOpType::MSKLH:
        case MultimediaOpType::MSKQH:
            return 1;

        // Insert operations
        case MultimediaOpType::INSBL:
        case MultimediaOpType::INSWL:
        case MultimediaOpType::INSLL:
        case MultimediaOpType::INSQL:
        case MultimediaOpType::INSBH:
        case MultimediaOpType::INSWH:
        case MultimediaOpType::INSLH:
        case MultimediaOpType::INSQH:
            return 1;

        // Extract operations
        case MultimediaOpType::EXTBL:
        case MultimediaOpType::EXTWL:
        case MultimediaOpType::EXTLL:
        case MultimediaOpType::EXTQL:
        case MultimediaOpType::EXTBH:
        case MultimediaOpType::EXTWH:
        case MultimediaOpType::EXTLH:
        case MultimediaOpType::EXTQH:
            return 1;

        // Zero byte operations
        case MultimediaOpType::ZAP:
        case MultimediaOpType::ZAPNOT:
            return 1;

        // Pack/Unpack operations
        case MultimediaOpType::PKWB:
        case MultimediaOpType::UNPKBW:
        case MultimediaOpType::UNPKBL:
            return 2;

        // Pixel operations
        case MultimediaOpType::PERR:
            return 3;

        // SIMD arithmetic
        case MultimediaOpType::ADDLV:
        case MultimediaOpType::SUBLV:
            return 2;
        case MultimediaOpType::MULLV:
            return 4;

        // Byte manipulation
        case MultimediaOpType::BYTESWAP:
        case MultimediaOpType::BYTEREPL:
            return 1;
        case MultimediaOpType::BYTEMIN:
        case MultimediaOpType::BYTEMAX:
            return 2;

        // Color space conversion
        case MultimediaOpType::RGB2YUV:
        case MultimediaOpType::YUV2RGB:
            return 6;

        // Bit field operations
        case MultimediaOpType::BFEXT:
        case MultimediaOpType::BFINS:
        case MultimediaOpType::BFCLR:
        case MultimediaOpType::BFSET:
            return 2;

        // Alpha blending
        case MultimediaOpType::ABLEND:
        case MultimediaOpType::AMIX:
            return 4;

        // Texture operations
        case MultimediaOpType::TEXLOD:
        case MultimediaOpType::TEXFILT:
            return 8;

        // Vector operations
        case MultimediaOpType::VADD:
        case MultimediaOpType::VSUB:
            return 3;
        case MultimediaOpType::VMUL:
            return 5;
        case MultimediaOpType::VDOT:
            return 6;
        case MultimediaOpType::VCROSS:
            return 8;
        case MultimediaOpType::VNORM:
            return 12;

        default:
            return 2;
        }
    }

    // Performance-critical accessors
    inline MultimediaOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg1() const { return m_srcReg1; }
    inline uint8_t getSrcReg2() const { return m_srcReg2; }
    inline uint8_t getImmediate() const { return m_immediate; }
    inline bool usesImmediate() const { return m_useImmediate; }
    inline DataSize getDataSize() const { return m_dataSize; }
    inline OperandFormat getOperandFormat() const { return m_operandFormat; }
    inline uint64_t getResult() const { return m_result; }
    inline uint8_t getMask() const { return m_mask; }
    inline uint8_t getShiftAmount() const { return m_shiftAmount; }

    // Performance counters
    inline uint64_t getByteMaskOperationCount() const
    {
        return m_byteMaskOperationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getPackUnpackCount() const { return m_packUnpackCount.load(std::memory_order_relaxed); }
    inline uint64_t getPixelOperationCount() const { return m_pixelOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getVectorOperationCount() const { return m_vectorOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getBitFieldOperationCount() const
    {
        return m_bitFieldOperationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getAlphaBlendCount() const { return m_alphaBlendCount.load(std::memory_order_relaxed); }
    inline uint64_t getTextureOperationCount() const { return m_textureOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getColorConversionCount() const { return m_colorConversionCount.load(std::memory_order_relaxed); }
    inline uint64_t getSimdOperationCount() const { return m_simdOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getOverflowCount() const { return m_overflowCount.load(std::memory_order_relaxed); }

    // Operation classification
    inline bool isByteMaskOperation() const
    {
        return (m_opType >= MultimediaOpType::MSKBL && m_opType <= MultimediaOpType::MSKQH) ||
               (m_opType == MultimediaOpType::ZAP || m_opType == MultimediaOpType::ZAPNOT);
    }

    inline bool isInsertExtractOperation() const
    {
        return (m_opType >= MultimediaOpType::INSBL && m_opType <= MultimediaOpType::INSQH) ||
               (m_opType >= MultimediaOpType::EXTBL && m_opType <= MultimediaOpType::EXTQH);
    }

    inline bool isPackUnpackOperation() const
    {
        return (m_opType >= MultimediaOpType::PKWB && m_opType <= MultimediaOpType::UNPKBL);
    }

    inline bool isVectorOperation() const
    {
        return (m_opType >= MultimediaOpType::VADD && m_opType <= MultimediaOpType::VNORM);
    }

    inline bool isPixelOperation() const { return (m_opType == MultimediaOpType::PERR); }

    // Hot path execution support
    inline void setOperands(uint64_t op1, uint64_t op2)
    {
        m_operand1 = op1;
        m_operand2 = op2;
    }

    inline void setMask(uint8_t mask) { m_mask = mask; }
    inline void setShiftAmount(uint8_t shift) { m_shiftAmount = shift; }
    inline void setOperandFormat(OperandFormat format) { m_operandFormat = format; }

  private:
    DataSize determineDataSize(MultimediaOpType opType) const
    {
        switch (opType)
        {
        case MultimediaOpType::MSKBL:
        case MultimediaOpType::MSKBH:
        case MultimediaOpType::INSBL:
        case MultimediaOpType::INSBH:
        case MultimediaOpType::EXTBL:
        case MultimediaOpType::EXTBH:
            return DataSize::BYTE;

        case MultimediaOpType::MSKWL:
        case MultimediaOpType::MSKWH:
        case MultimediaOpType::INSWL:
        case MultimediaOpType::INSWH:
        case MultimediaOpType::EXTWL:
        case MultimediaOpType::EXTWH:
            return DataSize::WORD;

        case MultimediaOpType::MSKLL:
        case MultimediaOpType::MSKLH:
        case MultimediaOpType::INSLL:
        case MultimediaOpType::INSLH:
        case MultimediaOpType::EXTLL:
        case MultimediaOpType::EXTLH:
        case MultimediaOpType::ADDLV:
        case MultimediaOpType::SUBLV:
        case MultimediaOpType::MULLV:
            return DataSize::LONGWORD;

        case MultimediaOpType::MSKQL:
        case MultimediaOpType::MSKQH:
        case MultimediaOpType::INSQL:
        case MultimediaOpType::INSQH:
        case MultimediaOpType::EXTQL:
        case MultimediaOpType::EXTQH:
            return DataSize::QUADWORD;

        default:
            return DataSize::QUADWORD;
        }
    }

    bool performMultimediaOperation()
    {
        switch (m_opType)
        {
        // Mask operations
        case MultimediaOpType::MSKBL:
        case MultimediaOpType::MSKWL:
        case MultimediaOpType::MSKLL:
        case MultimediaOpType::MSKQL:
        case MultimediaOpType::MSKBH:
        case MultimediaOpType::MSKWH:
        case MultimediaOpType::MSKLH:
        case MultimediaOpType::MSKQH:
            return performMaskOperation();

        // Insert operations
        case MultimediaOpType::INSBL:
        case MultimediaOpType::INSWL:
        case MultimediaOpType::INSLL:
        case MultimediaOpType::INSQL:
        case MultimediaOpType::INSBH:
        case MultimediaOpType::INSWH:
        case MultimediaOpType::INSLH:
        case MultimediaOpType::INSQH:
            return performInsertOperation();

        // Extract operations
        case MultimediaOpType::EXTBL:
        case MultimediaOpType::EXTWL:
        case MultimediaOpType::EXTLL:
        case MultimediaOpType::EXTQL:
        case MultimediaOpType::EXTBH:
        case MultimediaOpType::EXTWH:
        case MultimediaOpType::EXTLH:
        case MultimediaOpType::EXTQH:
            return performExtractOperation();

        // Zero byte operations
        case MultimediaOpType::ZAP:
        case MultimediaOpType::ZAPNOT:
            return performZapOperation();

        // Pack/Unpack operations
        case MultimediaOpType::PKWB:
        case MultimediaOpType::UNPKBW:
        case MultimediaOpType::UNPKBL:
            return performPackUnpackOperation();

        // Pixel operations
        case MultimediaOpType::PERR:
            return performPixelError();

        // SIMD arithmetic
        case MultimediaOpType::ADDLV:
        case MultimediaOpType::SUBLV:
        case MultimediaOpType::MULLV:
            return performSIMDArithmetic();

        // Byte manipulation
        case MultimediaOpType::BYTESWAP:
        case MultimediaOpType::BYTEREPL:
        case MultimediaOpType::BYTEMIN:
        case MultimediaOpType::BYTEMAX:
            return performByteManipulation();

        // Color space conversion
        case MultimediaOpType::RGB2YUV:
        case MultimediaOpType::YUV2RGB:
            return performColorConversion();

        // Bit field operations
        case MultimediaOpType::BFEXT:
        case MultimediaOpType::BFINS:
        case MultimediaOpType::BFCLR:
        case MultimediaOpType::BFSET:
            return performBitFieldOperation();

        // Alpha blending
        case MultimediaOpType::ABLEND:
        case MultimediaOpType::AMIX:
            return performAlphaBlending();

        // Texture operations
        case MultimediaOpType::TEXLOD:
        case MultimediaOpType::TEXFILT:
            return performTextureOperation();

        // Vector operations
        case MultimediaOpType::VADD:
        case MultimediaOpType::VSUB:
        case MultimediaOpType::VMUL:
        case MultimediaOpType::VDOT:
        case MultimediaOpType::VCROSS:
        case MultimediaOpType::VNORM:
            return performVectorOperation();

        default:
            return false;
        }
    }

    bool performMaskOperation()
    {
        m_byteMaskOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint64_t shift = m_useImmediate ? m_immediate : (m_operand2 & 0x7);
        uint64_t mask = 0;

        switch (m_opType)
        {
        case MultimediaOpType::MSKBL:
            mask = 0xFF00000000000000ULL >> (shift * 8);
            break;
        case MultimediaOpType::MSKWL:
            mask = 0xFFFF000000000000ULL >> (shift * 8);
            break;
        case MultimediaOpType::MSKLL:
            mask = 0xFFFFFFFF00000000ULL >> (shift * 8);
            break;
        case MultimediaOpType::MSKQL:
            mask = 0xFFFFFFFFFFFFFFFFULL >> (shift * 8);
            break;
        case MultimediaOpType::MSKBH:
            mask = 0x00000000000000FFULL << (shift * 8);
            break;
        case MultimediaOpType::MSKWH:
            mask = 0x000000000000FFFFULL << (shift * 8);
            break;
        case MultimediaOpType::MSKLH:
            mask = 0x00000000FFFFFFFFULL << (shift * 8);
            break;
        case MultimediaOpType::MSKQH:
            mask = 0xFFFFFFFFFFFFFFFFULL << (shift * 8);
            break;
        default:
            return false;
        }

        m_result = m_operand1 & ~mask;

        DEBUG_LOG("Mask operation: 0x%016llX & ~0x%016llX = 0x%016llX", m_operand1, mask, m_result);
        return true;
    }

    bool performInsertOperation()
    {
        uint64_t shift = m_useImmediate ? m_immediate : (m_operand2 & 0x7);
        uint64_t data = m_operand1;

        switch (m_opType)
        {
        case MultimediaOpType::INSBL:
            m_result = (data & 0xFF) << (shift * 8);
            break;
        case MultimediaOpType::INSWL:
            m_result = (data & 0xFFFF) << (shift * 8);
            break;
        case MultimediaOpType::INSLL:
            m_result = (data & 0xFFFFFFFF) << (shift * 8);
            break;
        case MultimediaOpType::INSQL:
            m_result = data << (shift * 8);
            break;
        case MultimediaOpType::INSBH:
            m_result = (data & 0xFF) >> ((8 - shift) * 8);
            break;
        case MultimediaOpType::INSWH:
            m_result = (data & 0xFFFF) >> ((8 - shift) * 8);
            break;
        case MultimediaOpType::INSLH:
            m_result = (data & 0xFFFFFFFF) >> ((8 - shift) * 8);
            break;
        case MultimediaOpType::INSQH:
            m_result = data >> ((8 - shift) * 8);
            break;
        default:
            return false;
        }

        DEBUG_LOG("Insert operation: data=0x%016llX, shift=%llu, result=0x%016llX", data, shift, m_result);
        return true;
    }

    bool performExtractOperation()
    {
        uint64_t shift = m_useImmediate ? m_immediate : (m_operand2 & 0x7);
        uint64_t data = m_operand1;

        switch (m_opType)
        {
        case MultimediaOpType::EXTBL:
            m_result = (data >> (shift * 8)) & 0xFF;
            break;
        case MultimediaOpType::EXTWL:
            m_result = (data >> (shift * 8)) & 0xFFFF;
            break;
        case MultimediaOpType::EXTLL:
            m_result = (data >> (shift * 8)) & 0xFFFFFFFF;
            break;
        case MultimediaOpType::EXTQL:
            m_result = data >> (shift * 8);
            break;
        case MultimediaOpType::EXTBH:
            m_result = (data << ((8 - shift) * 8)) & 0xFF00000000000000ULL;
            break;
        case MultimediaOpType::EXTWH:
            m_result = (data << ((8 - shift) * 8)) & 0xFFFF000000000000ULL;
            break;
        case MultimediaOpType::EXTLH:
            m_result = (data << ((8 - shift) * 8)) & 0xFFFFFFFF00000000ULL;
            break;
        case MultimediaOpType::EXTQH:
            m_result = data << ((8 - shift) * 8);
            break;
        default:
            return false;
        }

        DEBUG_LOG("Extract operation: data=0x%016llX, shift=%llu, result=0x%016llX", data, shift, m_result);
        return true;
    }

    bool performZapOperation()
    {
        m_byteMaskOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint8_t zapMask = m_useImmediate ? m_immediate : (m_operand2 & 0xFF);
        uint64_t result = m_operand1;

        for (int i = 0; i < 8; i++)
        {
            bool zapByte = (m_opType == MultimediaOpType::ZAP) ? (zapMask & (1 << i)) != 0 : (zapMask & (1 << i)) == 0;

            if (zapByte)
            {
                uint64_t byteMask = 0xFFULL << (i * 8);
                result &= ~byteMask;
            }
        }

        m_result = result;

        DEBUG_LOG("ZAP operation: data=0x%016llX, mask=0x%02X, result=0x%016llX", m_operand1, zapMask, m_result);
        return true;
    }

    bool performPackUnpackOperation()
    {
        m_packUnpackCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MultimediaOpType::PKWB:
            // Pack 4 words into 4 bytes
            m_result = ((m_operand1 & 0xFF000000) >> 24) | ((m_operand1 & 0x00FF0000) >> 8) |
                       ((m_operand1 & 0x0000FF00) << 8) | ((m_operand1 & 0x000000FF) << 24);
            break;

        case MultimediaOpType::UNPKBW:
            // Unpack 4 bytes into 4 words
            m_result = ((m_operand1 & 0xFF000000ULL) << 24) | ((m_operand1 & 0x00FF0000ULL) << 8) |
                       ((m_operand1 & 0x0000FF00ULL) >> 8) | ((m_operand1 & 0x000000FFULL) >> 24);
            break;

        case MultimediaOpType::UNPKBL:
            // Unpack 4 bytes into 4 longwords
            m_result = (m_operand1 & 0xFF) | ((m_operand1 & 0xFF00) << 8) | ((m_operand1 & 0xFF0000ULL) << 16) |
                       ((m_operand1 & 0xFF000000ULL) << 24);
            break;

        default:
            return false;
        }

        DEBUG_LOG("Pack/Unpack operation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performPixelError()
    {
        m_pixelOperationCount.fetch_add(1, std::memory_order_relaxed);

        // Calculate sum of absolute differences between bytes (for motion estimation)
        uint64_t sum = 0;
        for (int i = 0; i < 8; i++)
        {
            uint8_t byte1 = (m_operand1 >> (i * 8)) & 0xFF;
            uint8_t byte2 = (m_operand2 >> (i * 8)) & 0xFF;
            sum += (byte1 > byte2) ? (byte1 - byte2) : (byte2 - byte1);
        }

        m_result = sum;

        DEBUG_LOG("Pixel error: op1=0x%016llX, op2=0x%016llX, error=%llu", m_operand1, m_operand2, sum);
        return true;
    }

    bool performSIMDArithmetic()
    {
        m_simdOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MultimediaOpType::ADDLV:
            // Add two 32-bit values with saturation
            {
                int64_t result = static_cast<int32_t>(m_operand1) + static_cast<int32_t>(m_operand2);
                if (result > 0x7FFFFFFF)
                {
                    m_result = 0x7FFFFFFF;
                    m_overflowCount.fetch_add(1, std::memory_order_relaxed);
                }
                else if (result < -0x80000000LL)
                {
                    m_result = 0x80000000;
                    m_overflowCount.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    m_result = static_cast<uint64_t>(result);
                }
            }
            break;

        case MultimediaOpType::SUBLV:
            // Subtract two 32-bit values with saturation
            {
                int64_t result = static_cast<int32_t>(m_operand1) - static_cast<int32_t>(m_operand2);
                if (result > 0x7FFFFFFF)
                {
                    m_result = 0x7FFFFFFF;
                    m_overflowCount.fetch_add(1, std::memory_order_relaxed);
                }
                else if (result < -0x80000000LL)
                {
                    m_result = 0x80000000;
                    m_overflowCount.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    m_result = static_cast<uint64_t>(result);
                }
            }
            break;

        case MultimediaOpType::MULLV:
            // Multiply two 32-bit values
            {
                int64_t result = static_cast<int32_t>(m_operand1) * static_cast<int32_t>(m_operand2);
                m_result = static_cast<uint64_t>(result);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("SIMD arithmetic: op1=0x%08X, op2=0x%08X, result=0x%016llX", static_cast<uint32_t>(m_operand1),
                  static_cast<uint32_t>(m_operand2), m_result);
        return true;
    }

    bool performByteManipulation()
    {
        switch (m_opType)
        {
        case MultimediaOpType::BYTESWAP:
            // Swap bytes in quadword
            m_result = ((m_operand1 & 0x00000000000000FFULL) << 56) | ((m_operand1 & 0x000000000000FF00ULL) << 40) |
                       ((m_operand1 & 0x0000000000FF0000ULL) << 24) | ((m_operand1 & 0x00000000FF000000ULL) << 8) |
                       ((m_operand1 & 0x000000FF00000000ULL) >> 8) | ((m_operand1 & 0x0000FF0000000000ULL) >> 24) |
                       ((m_operand1 & 0x00FF000000000000ULL) >> 40) | ((m_operand1 & 0xFF00000000000000ULL) >> 56);
            break;

        case MultimediaOpType::BYTEREPL:
            // Replicate lowest byte to all positions
            {
                uint8_t byte = m_operand1 & 0xFF;
                m_result = 0;
                for (int i = 0; i < 8; i++)
                {
                    m_result |= (static_cast<uint64_t>(byte) << (i * 8));
                }
            }
            break;

        case MultimediaOpType::BYTEMIN:
            // Byte-wise minimum
            m_result = 0;
            for (int i = 0; i < 8; i++)
            {
                uint8_t byte1 = (m_operand1 >> (i * 8)) & 0xFF;
                uint8_t byte2 = (m_operand2 >> (i * 8)) & 0xFF;
                uint8_t minByte = (byte1 < byte2) ? byte1 : byte2;
                m_result |= (static_cast<uint64_t>(minByte) << (i * 8));
            }
            break;

        case MultimediaOpType::BYTEMAX:
            // Byte-wise maximum
            m_result = 0;
            for (int i = 0; i < 8; i++)
            {
                uint8_t byte1 = (m_operand1 >> (i * 8)) & 0xFF;
                uint8_t byte2 = (m_operand2 >> (i * 8)) & 0xFF;
                uint8_t maxByte = (byte1 > byte2) ? byte1 : byte2;
                m_result |= (static_cast<uint64_t>(maxByte) << (i * 8));
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Byte manipulation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performColorConversion()
    {
        m_colorConversionCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MultimediaOpType::RGB2YUV:
            // RGB to YUV conversion (simplified)
            {
                uint8_t r = (m_operand1 >> 16) & 0xFF;
                uint8_t g = (m_operand1 >> 8) & 0xFF;
                uint8_t b = m_operand1 & 0xFF;

                uint8_t y = static_cast<uint8_t>((299 * r + 587 * g + 114 * b) / 1000);
                uint8_t u = static_cast<uint8_t>(((-169 * r - 331 * g + 500 * b) / 1000) + 128);
                uint8_t v = static_cast<uint8_t>(((500 * r - 419 * g - 81 * b) / 1000) + 128);

                m_result =
                    (static_cast<uint64_t>(y) << 16) | (static_cast<uint64_t>(u) << 8) | static_cast<uint64_t>(v);
            }
            break;

        case MultimediaOpType::YUV2RGB:
            // YUV to RGB conversion (simplified)
            {
                uint8_t y = (m_operand1 >> 16) & 0xFF;
                uint8_t u = (m_operand1 >> 8) & 0xFF;
                uint8_t v = m_operand1 & 0xFF;

                int32_t c = y - 16;
                int32_t d = u - 128;
                int32_t e = v - 128;

                uint8_t r = static_cast<uint8_t>((298 * c + 409 * e + 128) >> 8);
                uint8_t g = static_cast<uint8_t>((298 * c - 100 * d - 208 * e + 128) >> 8);
                uint8_t b = static_cast<uint8_t>((298 * c + 516 * d + 128) >> 8);

                m_result =
                    (static_cast<uint64_t>(r) << 16) | (static_cast<uint64_t>(g) << 8) | static_cast<uint64_t>(b);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Color conversion: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performBitFieldOperation()
    {
        m_bitFieldOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint8_t start = m_shiftAmount & 0x3F;
        uint8_t length = m_mask & 0x3F;

        switch (m_opType)
        {
        case MultimediaOpType::BFEXT:
            // Extract bit field
            {
                uint64_t mask = (1ULL << length) - 1;
                m_result = (m_operand1 >> start) & mask;
            }
            break;

        case MultimediaOpType::BFINS:
            // Insert bit field
            {
                uint64_t mask = (1ULL << length) - 1;
                uint64_t clearMask = ~(mask << start);
                m_result = (m_operand1 & clearMask) | ((m_operand2 & mask) << start);
            }
            break;

        case MultimediaOpType::BFCLR:
            // Clear bit field
            {
                uint64_t mask = (1ULL << length) - 1;
                uint64_t clearMask = ~(mask << start);
                m_result = m_operand1 & clearMask;
            }
            break;

        case MultimediaOpType::BFSET:
            // Set bit field
            {
                uint64_t mask = (1ULL << length) - 1;
                m_result = m_operand1 | (mask << start);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Bit field operation: start=%d, length=%d, 0x%016llX -> 0x%016llX", start, length, m_operand1,
                  m_result);
        return true;
    }

    bool performAlphaBlending()
    {
        m_alphaBlendCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MultimediaOpType::ABLEND:
            // Alpha blending: result = src * alpha + dst * (1 - alpha)
            {
                uint8_t alpha = (m_operand2 >> 24) & 0xFF;
                uint8_t invAlpha = 255 - alpha;

                uint8_t srcR = (m_operand1 >> 16) & 0xFF;
                uint8_t srcG = (m_operand1 >> 8) & 0xFF;
                uint8_t srcB = m_operand1 & 0xFF;

                uint8_t dstR = (m_operand2 >> 16) & 0xFF;
                uint8_t dstG = (m_operand2 >> 8) & 0xFF;
                uint8_t dstB = m_operand2 & 0xFF;

                uint8_t resultR = (srcR * alpha + dstR * invAlpha) / 255;
                uint8_t resultG = (srcG * alpha + dstG * invAlpha) / 255;
                uint8_t resultB = (srcB * alpha + dstB * invAlpha) / 255;

                m_result = (static_cast<uint64_t>(resultR) << 16) | (static_cast<uint64_t>(resultG) << 8) |
                           static_cast<uint64_t>(resultB);
            }
            break;

        case MultimediaOpType::AMIX:
            // Alpha mixing with fixed 50/50 blend
            {
                uint8_t srcR = (m_operand1 >> 16) & 0xFF;
                uint8_t srcG = (m_operand1 >> 8) & 0xFF;
                uint8_t srcB = m_operand1 & 0xFF;

                uint8_t dstR = (m_operand2 >> 16) & 0xFF;
                uint8_t dstG = (m_operand2 >> 8) & 0xFF;
                uint8_t dstB = m_operand2 & 0xFF;

                uint8_t resultR = (srcR + dstR) / 2;
                uint8_t resultG = (srcG + dstG) / 2;
                uint8_t resultB = (srcB + dstB) / 2;

                m_result = (static_cast<uint64_t>(resultR) << 16) | (static_cast<uint64_t>(resultG) << 8) |
                           static_cast<uint64_t>(resultB);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Alpha blending: src=0x%016llX, dst=0x%016llX, result=0x%016llX", m_operand1, m_operand2, m_result);
        return true;
    }

    bool performTextureOperation()
    {
        m_textureOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MultimediaOpType::TEXLOD:
            // Texture level of detail calculation (simplified)
            {
                // Calculate mipmap level based on texture coordinate derivatives
                uint32_t dudx = (m_operand1 >> 32) & 0xFFFF;
                uint32_t dvdx = (m_operand1 >> 16) & 0xFFFF;
                uint32_t dudy = (m_operand1) & 0xFFFF;
                uint32_t dvdy = (m_operand2) & 0xFFFF;

                uint32_t rho = (dudx * dudx + dvdx * dvdx + dudy * dudy + dvdy * dvdy);
                uint32_t lod = 0;

                // Simple LOD calculation (log2 approximation)
                while (rho > 1)
                {
                    rho >>= 1;
                    lod++;
                }

                m_result = lod;
            }
            break;

        case MultimediaOpType::TEXFILT:
            // Texture filtering (bilinear interpolation simplified)
            {
                uint8_t t00 = (m_operand1 >> 24) & 0xFF;
                uint8_t t01 = (m_operand1 >> 16) & 0xFF;
                uint8_t t10 = (m_operand1 >> 8) & 0xFF;
                uint8_t t11 = m_operand1 & 0xFF;

                uint8_t u = (m_operand2 >> 8) & 0xFF;
                uint8_t v = m_operand2 & 0xFF;

                uint16_t top = (t00 * (255 - u) + t01 * u) / 255;
                uint16_t bottom = (t10 * (255 - u) + t11 * u) / 255;
                uint8_t result = (top * (255 - v) + bottom * v) / 255;

                m_result = result;
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Texture operation: op1=0x%016llX, op2=0x%016llX, result=0x%016llX", m_operand1, m_operand2,
                  m_result);
        return true;
    }

    bool performVectorOperation()
    {
        m_vectorOperationCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified vector operations (treating operands as 4-element vectors)
        switch (m_opType)
        {
        case MultimediaOpType::VADD:
            // Vector addition
            {
                uint16_t a0 = (m_operand1 >> 48) & 0xFFFF;
                uint16_t a1 = (m_operand1 >> 32) & 0xFFFF;
                uint16_t a2 = (m_operand1 >> 16) & 0xFFFF;
                uint16_t a3 = m_operand1 & 0xFFFF;

                uint16_t b0 = (m_operand2 >> 48) & 0xFFFF;
                uint16_t b1 = (m_operand2 >> 32) & 0xFFFF;
                uint16_t b2 = (m_operand2 >> 16) & 0xFFFF;
                uint16_t b3 = m_operand2 & 0xFFFF;

                m_result = (static_cast<uint64_t>(a0 + b0) << 48) | (static_cast<uint64_t>(a1 + b1) << 32) |
                           (static_cast<uint64_t>(a2 + b2) << 16) | static_cast<uint64_t>(a3 + b3);
            }
            break;

        case MultimediaOpType::VSUB:
            // Vector subtraction
            {
                uint16_t a0 = (m_operand1 >> 48) & 0xFFFF;
                uint16_t a1 = (m_operand1 >> 32) & 0xFFFF;
                uint16_t a2 = (m_operand1 >> 16) & 0xFFFF;
                uint16_t a3 = m_operand1 & 0xFFFF;

                uint16_t b0 = (m_operand2 >> 48) & 0xFFFF;
                uint16_t b1 = (m_operand2 >> 32) & 0xFFFF;
                uint16_t b2 = (m_operand2 >> 16) & 0xFFFF;
                uint16_t b3 = m_operand2 & 0xFFFF;

                m_result = (static_cast<uint64_t>(a0 - b0) << 48) | (static_cast<uint64_t>(a1 - b1) << 32) |
                           (static_cast<uint64_t>(a2 - b2) << 16) | static_cast<uint64_t>(a3 - b3);
            }
            break;

        case MultimediaOpType::VMUL:
            // Vector multiplication (element-wise)
            {
                uint16_t a0 = (m_operand1 >> 48) & 0xFFFF;
                uint16_t a1 = (m_operand1 >> 32) & 0xFFFF;
                uint16_t a2 = (m_operand1 >> 16) & 0xFFFF;
                uint16_t a3 = m_operand1 & 0xFFFF;

                uint16_t b0 = (m_operand2 >> 48) & 0xFFFF;
                uint16_t b1 = (m_operand2 >> 32) & 0xFFFF;
                uint16_t b2 = (m_operand2 >> 16) & 0xFFFF;
                uint16_t b3 = m_operand2 & 0xFFFF;

                m_result = (static_cast<uint64_t>((a0 * b0) >> 16) << 48) |
                           (static_cast<uint64_t>((a1 * b1) >> 16) << 32) |
                           (static_cast<uint64_t>((a2 * b2) >> 16) << 16) | static_cast<uint64_t>((a3 * b3) >> 16);
            }
            break;

        case MultimediaOpType::VDOT:
            // Vector dot product
            {
                uint16_t a0 = (m_operand1 >> 48) & 0xFFFF;
                uint16_t a1 = (m_operand1 >> 32) & 0xFFFF;
                uint16_t a2 = (m_operand1 >> 16) & 0xFFFF;
                uint16_t a3 = m_operand1 & 0xFFFF;

                uint16_t b0 = (m_operand2 >> 48) & 0xFFFF;
                uint16_t b1 = (m_operand2 >> 32) & 0xFFFF;
                uint16_t b2 = (m_operand2 >> 16) & 0xFFFF;
                uint16_t b3 = m_operand2 & 0xFFFF;

                m_result = (a0 * b0 + a1 * b1 + a2 * b2 + a3 * b3) >> 16;
            }
            break;

        case MultimediaOpType::VCROSS:
            // Vector cross product (3D, treating first 3 elements)
            {
                int16_t a0 = static_cast<int16_t>((m_operand1 >> 48) & 0xFFFF);
                int16_t a1 = static_cast<int16_t>((m_operand1 >> 32) & 0xFFFF);
                int16_t a2 = static_cast<int16_t>((m_operand1 >> 16) & 0xFFFF);

                int16_t b0 = static_cast<int16_t>((m_operand2 >> 48) & 0xFFFF);
                int16_t b1 = static_cast<int16_t>((m_operand2 >> 32) & 0xFFFF);
                int16_t b2 = static_cast<int16_t>((m_operand2 >> 16) & 0xFFFF);

                int16_t c0 = a1 * b2 - a2 * b1;
                int16_t c1 = a2 * b0 - a0 * b2;
                int16_t c2 = a0 * b1 - a1 * b0;

                m_result = (static_cast<uint64_t>(c0) << 48) | (static_cast<uint64_t>(c1) << 32) |
                           (static_cast<uint64_t>(c2) << 16);
            }
            break;

        case MultimediaOpType::VNORM:
            // Vector normalization (simplified)
            {
                uint16_t a0 = (m_operand1 >> 48) & 0xFFFF;
                uint16_t a1 = (m_operand1 >> 32) & 0xFFFF;
                uint16_t a2 = (m_operand1 >> 16) & 0xFFFF;
                uint16_t a3 = m_operand1 & 0xFFFF;

                uint32_t magnitude = a0 * a0 + a1 * a1 + a2 * a2 + a3 * a3;

                // Simplified square root and normalization
                if (magnitude > 0)
                {
                    uint32_t invMag = 0x10000 / magnitude; // Approximation
                    m_result = (static_cast<uint64_t>((a0 * invMag) >> 16) << 48) |
                               (static_cast<uint64_t>((a1 * invMag) >> 16) << 32) |
                               (static_cast<uint64_t>((a2 * invMag) >> 16) << 16) |
                               static_cast<uint64_t>((a3 * invMag) >> 16);
                }
                else
                {
                    m_result = 0;
                }
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Vector operation: op1=0x%016llX, op2=0x%016llX, result=0x%016llX", m_operand1, m_operand2, m_result);
        return true;
    }

  private:
     MultimediaOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg1;
     uint8_t m_srcReg2;
     uint8_t m_immediate;
     bool m_useImmediate;
     DataSize m_dataSize;

    OperandFormat m_operandFormat;
    uint64_t m_operand1;
    uint64_t m_operand2;
    uint64_t m_result;
    uint8_t m_mask;
    uint8_t m_shiftAmount;

    std::atomic<uint64_t> m_byteMaskOperationCount;
    std::atomic<uint64_t> m_packUnpackCount;
    std::atomic<uint64_t> m_pixelOperationCount;
    std::atomic<uint64_t> m_vectorOperationCount;
    std::atomic<uint64_t> m_bitFieldOperationCount;
    std::atomic<uint64_t> m_alphaBlendCount;
    std::atomic<uint64_t> m_textureOperationCount;
    std::atomic<uint64_t> m_colorConversionCount;
    std::atomic<uint64_t> m_simdOperationCount;
    std::atomic<uint64_t> m_overflowCount;

    // Prevent copying for performance
    alphaMultimediaInstruction(const alphaMultimediaInstruction &) = delete;
    alphaMultimediaInstruction &operator=(const alphaMultimediaInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaBitManipulationInstruction : public alphaInstructionBase
{
  public:
    enum class BitManipulationOpType : uint16_t
    {
        // Shift operations
        SLL = 0x39, // Shift left logical
        SRL = 0x34, // Shift right logical
        SRA = 0x3C, // Shift right arithmetic

        // Rotate operations
        ROL = 0x3A,  // Rotate left
        ROR = 0x3B,  // Rotate right
        ROLV = 0x1A, // Rotate left variable
        RORV = 0x1B, // Rotate right variable

        // Count operations
        CTPOP = 0x30, // Count population (number of 1 bits)
        CTLZ = 0x31,  // Count leading zeros
        CTTZ = 0x32,  // Count trailing zeros
        CTLO = 0x33,  // Count leading ones
        CTTO = 0x35,  // Count trailing ones

        // Find bit operations
        FFS = 0x36, // Find first set bit
        FLS = 0x37, // Find last set bit
        FFZ = 0x38, // Find first zero bit
        FLZ = 0x3D, // Find last zero bit

        // Bit reversal and manipulation
        BREV = 0x40,   // Bit reversal
        BREV8 = 0x41,  // Bit reversal in bytes
        BREV16 = 0x42, // Bit reversal in words
        BREV32 = 0x43, // Bit reversal in longwords

        // Bit test operations
        BT = 0x50,  // Bit test
        BTS = 0x51, // Bit test and set
        BTR = 0x52, // Bit test and reset
        BTC = 0x53, // Bit test and complement

        // Bit scan operations
        BSF = 0x54, // Bit scan forward
        BSR = 0x55, // Bit scan reverse

        // Advanced bit operations
        PDEP = 0x60, // Parallel bits deposit
        PEXT = 0x61, // Parallel bits extract
        ANDN = 0x62, // AND NOT operation

        // Bit field operations (extended)
        BEXTR = 0x70,  // Bit extract
        BZHI = 0x71,   // Zero high bits
        BLSI = 0x72,   // Isolate lowest set bit
        BLSMSK = 0x73, // Mask up to lowest set bit
        BLSR = 0x74,   // Reset lowest set bit

        // Parity operations
        PARITY = 0x80,  // Calculate parity
        PARITY8 = 0x81, // Calculate parity of each byte

        // Gray code operations
        GRAY = 0x90,  // Binary to Gray code
        IGRAY = 0x91, // Gray code to binary

        // Bit interleaving/deinterleaving
        INTLV = 0xA0,  // Interleave bits
        DINTLV = 0xA1, // De-interleave bits

        // Bit matrix operations
        BTRANS = 0xB0, // Bit matrix transpose

        // Population count variants
        POPCNT8 = 0xC0,  // Population count in each byte
        POPCNT16 = 0xC1, // Population count in each word
        POPCNT32 = 0xC2, // Population count in each longword

        // Unknown operation
        UNKNOWN = 0xFFFF
    };

    enum class BitWidth : uint8_t
    {
        BIT_8 = 8,   // 8-bit operations
        BIT_16 = 16, // 16-bit operations
        BIT_32 = 32, // 32-bit operations
        BIT_64 = 64  // 64-bit operations
    };

    enum class ShiftType : uint8_t
    {
        LOGICAL = 0,    // Logical shift (fill with zeros)
        ARITHMETIC = 1, // Arithmetic shift (sign extend)
        ROTATE = 2      // Rotate (circular shift)
    };

    explicit alphaBitManipulationInstruction(uint32_t opcode, BitManipulationOpType opType, uint8_t destReg,
                                             uint8_t srcReg1, uint8_t srcReg2)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_immediate(0), m_useImmediate(false), m_bitWidth(BitWidth::BIT_64), m_shiftType(determineShiftType(opType)),
          m_operand1(0), m_operand2(0), m_result(0), m_shiftAmount(0), m_bitPosition(0), m_shiftOperationCount(0),
          m_countOperationCount(0), m_findOperationCount(0), m_reversalOperationCount(0), m_testOperationCount(0),
          m_advancedBitOpCount(0), m_parityOperationCount(0), m_matrixOperationCount(0), m_interleaveOperationCount(0),
          m_grayCodeOperationCount(0), m_overflowCount(0)
    {
        DEBUG_LOG("alphaBitManipulationInstruction created - OpType: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2);
    }

    // Immediate mode constructor
    explicit alphaBitManipulationInstruction(uint32_t opcode, BitManipulationOpType opType, uint8_t destReg,
                                             uint8_t srcReg1, uint8_t immediate)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(0),
          m_immediate(immediate), m_useImmediate(true), m_bitWidth(BitWidth::BIT_64),
          m_shiftType(determineShiftType(opType)), m_operand1(0), m_operand2(0), m_result(0), m_shiftAmount(0),
          m_bitPosition(0), m_shiftOperationCount(0), m_countOperationCount(0), m_findOperationCount(0),
          m_reversalOperationCount(0), m_testOperationCount(0), m_advancedBitOpCount(0), m_parityOperationCount(0),
          m_matrixOperationCount(0), m_interleaveOperationCount(0), m_grayCodeOperationCount(0), m_overflowCount(0)
    {
        DEBUG_LOG("alphaBitManipulationInstruction created (immediate) - OpType: 0x%04X, Dest: R%d, Src: R%d, Imm: %d",
                  static_cast<int>(opType), destReg, srcReg1, immediate);
    }

    virtual ~alphaBitManipulationInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performBitManipulationOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding bit manipulation instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        bool isLiteral = (opcode >> 12) & 0x1;
        uint8_t function = (opcode >> 5) & 0x7F;
        uint8_t rc = opcode & 0x1F;

        m_srcReg1 = ra;
        m_destReg = rc;

        if (isLiteral)
        {
            m_immediate = rb;
            m_useImmediate = true;
            m_srcReg2 = 0;
        }
        else
        {
            m_srcReg2 = rb;
            m_useImmediate = false;
        }

        // Determine bit manipulation operation type
        switch (primaryOpcode)
        {
        case 0x12: // Bit shift and manipulation
            switch (function)
            {
            case 0x39:
                m_opType = BitManipulationOpType::SLL;
                break;
            case 0x34:
                m_opType = BitManipulationOpType::SRL;
                break;
            case 0x3C:
                m_opType = BitManipulationOpType::SRA;
                break;
            case 0x30:
                m_opType = BitManipulationOpType::CTPOP;
                break;
            case 0x31:
                m_opType = BitManipulationOpType::CTLZ;
                break;
            case 0x32:
                m_opType = BitManipulationOpType::CTTZ;
                break;
            case 0x36:
                m_opType = BitManipulationOpType::FFS;
                break;
            case 0x37:
                m_opType = BitManipulationOpType::FLS;
                break;
            default:
                DEBUG_LOG("Unknown bit manipulation function: 0x%02X", function);
                m_opType = BitManipulationOpType::SLL;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown bit manipulation primary opcode: 0x%02X", primaryOpcode);
            m_opType = BitManipulationOpType::UNKNOWN;
            break;
        }

        // Set bit width based on operation
        m_bitWidth = BitWidth::BIT_64; // Default to 64-bit operations

        DEBUG_LOG("Bit manipulation instruction decoded - Type: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(m_opType), m_destReg, m_srcReg1, m_srcReg2);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Simple shift operations
        case BitManipulationOpType::SLL:
        case BitManipulationOpType::SRL:
        case BitManipulationOpType::SRA:
            return 1;

        // Rotate operations
        case BitManipulationOpType::ROL:
        case BitManipulationOpType::ROR:
        case BitManipulationOpType::ROLV:
        case BitManipulationOpType::RORV:
            return 1;

        // Count operations (hardware-dependent)
        case BitManipulationOpType::CTPOP:
        case BitManipulationOpType::CTLZ:
        case BitManipulationOpType::CTTZ:
        case BitManipulationOpType::CTLO:
        case BitManipulationOpType::CTTO:
            return 2;

        // Find operations
        case BitManipulationOpType::FFS:
        case BitManipulationOpType::FLS:
        case BitManipulationOpType::FFZ:
        case BitManipulationOpType::FLZ:
            return 2;

        // Bit reversal (complex operation)
        case BitManipulationOpType::BREV:
        case BitManipulationOpType::BREV8:
        case BitManipulationOpType::BREV16:
        case BitManipulationOpType::BREV32:
            return 3;

        // Bit test operations
        case BitManipulationOpType::BT:
        case BitManipulationOpType::BTS:
        case BitManipulationOpType::BTR:
        case BitManipulationOpType::BTC:
            return 1;

        // Bit scan operations
        case BitManipulationOpType::BSF:
        case BitManipulationOpType::BSR:
            return 2;

        // Advanced bit operations
        case BitManipulationOpType::PDEP:
        case BitManipulationOpType::PEXT:
            return 4;
        case BitManipulationOpType::ANDN:
            return 1;

        // Bit field operations
        case BitManipulationOpType::BEXTR:
        case BitManipulationOpType::BZHI:
        case BitManipulationOpType::BLSI:
        case BitManipulationOpType::BLSMSK:
        case BitManipulationOpType::BLSR:
            return 2;

        // Parity operations
        case BitManipulationOpType::PARITY:
        case BitManipulationOpType::PARITY8:
            return 2;

        // Gray code operations
        case BitManipulationOpType::GRAY:
        case BitManipulationOpType::IGRAY:
            return 2;

        // Bit interleaving
        case BitManipulationOpType::INTLV:
        case BitManipulationOpType::DINTLV:
            return 5;

        // Matrix operations
        case BitManipulationOpType::BTRANS:
            return 8;

        // Population count variants
        case BitManipulationOpType::POPCNT8:
        case BitManipulationOpType::POPCNT16:
        case BitManipulationOpType::POPCNT32:
            return 3;

        default:
            return 2;
        }
    }

    // Performance-critical accessors
    inline BitManipulationOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg1() const { return m_srcReg1; }
    inline uint8_t getSrcReg2() const { return m_srcReg2; }
    inline uint8_t getImmediate() const { return m_immediate; }
    inline bool usesImmediate() const { return m_useImmediate; }
    inline BitWidth getBitWidth() const { return m_bitWidth; }
    inline ShiftType getShiftType() const { return m_shiftType; }
    inline uint64_t getResult() const { return m_result; }
    inline uint8_t getShiftAmount() const { return m_shiftAmount; }
    inline uint8_t getBitPosition() const { return m_bitPosition; }

    // Performance counters
    inline uint64_t getShiftOperationCount() const { return m_shiftOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getCountOperationCount() const { return m_countOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getFindOperationCount() const { return m_findOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getReversalOperationCount() const
    {
        return m_reversalOperationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getTestOperationCount() const { return m_testOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getAdvancedBitOpCount() const { return m_advancedBitOpCount.load(std::memory_order_relaxed); }
    inline uint64_t getParityOperationCount() const { return m_parityOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getMatrixOperationCount() const { return m_matrixOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getInterleaveOperationCount() const
    {
        return m_interleaveOperationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getGrayCodeOperationCount() const
    {
        return m_grayCodeOperationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getOverflowCount() const { return m_overflowCount.load(std::memory_order_relaxed); }

    // Operation classification
    inline bool isShiftOperation() const
    {
        return (m_opType >= BitManipulationOpType::SLL && m_opType <= BitManipulationOpType::RORV);
    }

    inline bool isCountOperation() const
    {
        return (m_opType >= BitManipulationOpType::CTPOP && m_opType <= BitManipulationOpType::CTTO) ||
               (m_opType >= BitManipulationOpType::POPCNT8 && m_opType <= BitManipulationOpType::POPCNT32);
    }

    inline bool isFindOperation() const
    {
        return (m_opType >= BitManipulationOpType::FFS && m_opType <= BitManipulationOpType::FLZ);
    }

    inline bool isTestOperation() const
    {
        return (m_opType >= BitManipulationOpType::BT && m_opType <= BitManipulationOpType::BSR);
    }

    // Hot path execution support
    inline void setOperands(uint64_t op1, uint64_t op2)
    {
        m_operand1 = op1;
        m_operand2 = op2;
    }

    inline void setShiftAmount(uint8_t shift) { m_shiftAmount = shift; }
    inline void setBitPosition(uint8_t pos) { m_bitPosition = pos; }
    inline void setBitWidth(BitWidth width) { m_bitWidth = width; }

  private:
    ShiftType determineShiftType(BitManipulationOpType opType) const
    {
        switch (opType)
        {
        case BitManipulationOpType::SLL:
        case BitManipulationOpType::SRL:
            return ShiftType::LOGICAL;
        case BitManipulationOpType::SRA:
            return ShiftType::ARITHMETIC;
        case BitManipulationOpType::ROL:
        case BitManipulationOpType::ROR:
        case BitManipulationOpType::ROLV:
        case BitManipulationOpType::RORV:
            return ShiftType::ROTATE;
        default:
            return ShiftType::LOGICAL;
        }
    }

    bool performBitManipulationOperation()
    {
        switch (m_opType)
        {
        // Shift operations
        case BitManipulationOpType::SLL:
        case BitManipulationOpType::SRL:
        case BitManipulationOpType::SRA:
            return performShiftOperation();

        // Rotate operations
        case BitManipulationOpType::ROL:
        case BitManipulationOpType::ROR:
        case BitManipulationOpType::ROLV:
        case BitManipulationOpType::RORV:
            return performRotateOperation();

        // Count operations
        case BitManipulationOpType::CTPOP:
        case BitManipulationOpType::CTLZ:
        case BitManipulationOpType::CTTZ:
        case BitManipulationOpType::CTLO:
        case BitManipulationOpType::CTTO:
            return performCountOperation();

        // Find operations
        case BitManipulationOpType::FFS:
        case BitManipulationOpType::FLS:
        case BitManipulationOpType::FFZ:
        case BitManipulationOpType::FLZ:
            return performFindOperation();

        // Bit reversal
        case BitManipulationOpType::BREV:
        case BitManipulationOpType::BREV8:
        case BitManipulationOpType::BREV16:
        case BitManipulationOpType::BREV32:
            return performReversalOperation();

        // Bit test operations
        case BitManipulationOpType::BT:
        case BitManipulationOpType::BTS:
        case BitManipulationOpType::BTR:
        case BitManipulationOpType::BTC:
            return performBitTestOperation();

        // Bit scan operations
        case BitManipulationOpType::BSF:
        case BitManipulationOpType::BSR:
            return performBitScanOperation();

        // Advanced bit operations
        case BitManipulationOpType::PDEP:
        case BitManipulationOpType::PEXT:
        case BitManipulationOpType::ANDN:
            return performAdvancedBitOperation();

        // Bit field operations
        case BitManipulationOpType::BEXTR:
        case BitManipulationOpType::BZHI:
        case BitManipulationOpType::BLSI:
        case BitManipulationOpType::BLSMSK:
        case BitManipulationOpType::BLSR:
            return performBitFieldOperation();

        // Parity operations
        case BitManipulationOpType::PARITY:
        case BitManipulationOpType::PARITY8:
            return performParityOperation();

        // Gray code operations
        case BitManipulationOpType::GRAY:
        case BitManipulationOpType::IGRAY:
            return performGrayCodeOperation();

        // Bit interleaving
        case BitManipulationOpType::INTLV:
        case BitManipulationOpType::DINTLV:
            return performInterleaveOperation();

        // Matrix operations
        case BitManipulationOpType::BTRANS:
            return performMatrixOperation();

        // Population count variants
        case BitManipulationOpType::POPCNT8:
        case BitManipulationOpType::POPCNT16:
        case BitManipulationOpType::POPCNT32:
            return performPopulationCountVariant();

        default:
            return false;
        }
    }

    bool performShiftOperation()
    {
        m_shiftOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint8_t shiftCount = m_useImmediate ? m_immediate : (m_operand2 & 0x3F);

        switch (m_opType)
        {
        case BitManipulationOpType::SLL:
            // Shift left logical
            if (shiftCount >= 64)
            {
                m_result = 0;
            }
            else
            {
                m_result = m_operand1 << shiftCount;
            }
            break;

        case BitManipulationOpType::SRL:
            // Shift right logical
            if (shiftCount >= 64)
            {
                m_result = 0;
            }
            else
            {
                m_result = m_operand1 >> shiftCount;
            }
            break;

        case BitManipulationOpType::SRA:
            // Shift right arithmetic
            if (shiftCount >= 64)
            {
                m_result = (static_cast<int64_t>(m_operand1) < 0) ? 0xFFFFFFFFFFFFFFFFULL : 0;
            }
            else
            {
                m_result = static_cast<uint64_t>(static_cast<int64_t>(m_operand1) >> shiftCount);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Shift operation: 0x%016llX %s %d = 0x%016llX", m_operand1,
                  (m_opType == BitManipulationOpType::SLL)   ? "<<"
                  : (m_opType == BitManipulationOpType::SRL) ? ">>"
                                                             : ">>",
                  shiftCount, m_result);
        return true;
    }

    bool performRotateOperation()
    {
        m_shiftOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint8_t rotateCount = m_useImmediate ? m_immediate : (m_operand2 & 0x3F);
        rotateCount &= 0x3F; // Modulo 64

        switch (m_opType)
        {
        case BitManipulationOpType::ROL:
        case BitManipulationOpType::ROLV:
            // Rotate left
            m_result = (m_operand1 << rotateCount) | (m_operand1 >> (64 - rotateCount));
            break;

        case BitManipulationOpType::ROR:
        case BitManipulationOpType::RORV:
            // Rotate right
            m_result = (m_operand1 >> rotateCount) | (m_operand1 << (64 - rotateCount));
            break;

        default:
            return false;
        }

        DEBUG_LOG("Rotate operation: 0x%016llX rotated %d = 0x%016llX", m_operand1, rotateCount, m_result);
        return true;
    }

    bool performCountOperation()
    {
        m_countOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint64_t value = m_operand1;

        switch (m_opType)
        {
        case BitManipulationOpType::CTPOP:
            // Count population (number of 1 bits)
            m_result = __builtin_popcountll(value);
            break;

        case BitManipulationOpType::CTLZ:
            // Count leading zeros
            m_result = value ? __builtin_clzll(value) : 64;
            break;

        case BitManipulationOpType::CTTZ:
            // Count trailing zeros
            m_result = value ? __builtin_ctzll(value) : 64;
            break;

        case BitManipulationOpType::CTLO:
            // Count leading ones
            m_result = __builtin_clzll(~value);
            break;

        case BitManipulationOpType::CTTO:
            // Count trailing ones
            m_result = __builtin_ctzll(~value);
            break;

        default:
            return false;
        }

        DEBUG_LOG("Count operation: 0x%016llX -> %llu", value, m_result);
        return true;
    }

    bool performFindOperation()
    {
        m_findOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint64_t value = m_operand1;

        switch (m_opType)
        {
        case BitManipulationOpType::FFS:
            // Find first set bit (1-indexed)
            m_result = value ? (__builtin_ctzll(value) + 1) : 0;
            break;

        case BitManipulationOpType::FLS:
            // Find last set bit (1-indexed)
            m_result = value ? (64 - __builtin_clzll(value)) : 0;
            break;

        case BitManipulationOpType::FFZ:
            // Find first zero bit (1-indexed)
            m_result = (~value) ? (__builtin_ctzll(~value) + 1) : 0;
            break;

        case BitManipulationOpType::FLZ:
            // Find last zero bit (1-indexed)
            m_result = (~value) ? (64 - __builtin_clzll(~value)) : 0;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Find operation: 0x%016llX -> bit position %llu", value, m_result);
        return true;
    }

    bool performReversalOperation()
    {
        m_reversalOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint64_t value = m_operand1;
        m_result = 0;

        switch (m_opType)
        {
        case BitManipulationOpType::BREV:
            // Bit reversal of entire 64-bit value
            for (int i = 0; i < 64; i++)
            {
                if (value & (1ULL << i))
                {
                    m_result |= (1ULL << (63 - i));
                }
            }
            break;

        case BitManipulationOpType::BREV8:
            // Bit reversal within each byte
            for (int byte = 0; byte < 8; byte++)
            {
                uint8_t b = (value >> (byte * 8)) & 0xFF;
                uint8_t reversed = 0;
                for (int bit = 0; bit < 8; bit++)
                {
                    if (b & (1 << bit))
                    {
                        reversed |= (1 << (7 - bit));
                    }
                }
                m_result |= (static_cast<uint64_t>(reversed) << (byte * 8));
            }
            break;

        case BitManipulationOpType::BREV16:
            // Bit reversal within each word
            for (int word = 0; word < 4; word++)
            {
                uint16_t w = (value >> (word * 16)) & 0xFFFF;
                uint16_t reversed = 0;
                for (int bit = 0; bit < 16; bit++)
                {
                    if (w & (1 << bit))
                    {
                        reversed |= (1 << (15 - bit));
                    }
                }
                m_result |= (static_cast<uint64_t>(reversed) << (word * 16));
            }
            break;

        case BitManipulationOpType::BREV32:
            // Bit reversal within each longword
            for (int lword = 0; lword < 2; lword++)
            {
                uint32_t lw = (value >> (lword * 32)) & 0xFFFFFFFF;
                uint32_t reversed = 0;
                for (int bit = 0; bit < 32; bit++)
                {
                    if (lw & (1U << bit))
                    {
                        reversed |= (1U << (31 - bit));
                    }
                }
                m_result |= (static_cast<uint64_t>(reversed) << (lword * 32));
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Bit reversal: 0x%016llX -> 0x%016llX", value, m_result);
        return true;
    }

    bool performBitTestOperation()
    {
        m_testOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint8_t bitPos = m_useImmediate ? m_immediate : (m_operand2 & 0x3F);
        uint64_t value = m_operand1;
        uint64_t bitMask = 1ULL << bitPos;
        bool bitSet = (value & bitMask) != 0;

        switch (m_opType)
        {
        case BitManipulationOpType::BT:
            // Bit test
            m_result = bitSet ? 1 : 0;
            break;

        case BitManipulationOpType::BTS:
            // Bit test and set
            m_result = bitSet ? 1 : 0;
            m_operand1 = value | bitMask; // Set the bit
            break;

        case BitManipulationOpType::BTR:
            // Bit test and reset
            m_result = bitSet ? 1 : 0;
            m_operand1 = value & ~bitMask; // Clear the bit
            break;

        case BitManipulationOpType::BTC:
            // Bit test and complement
            m_result = bitSet ? 1 : 0;
            m_operand1 = value ^ bitMask; // Toggle the bit
            break;

        default:
            return false;
        }

        DEBUG_LOG("Bit test operation: bit %d of 0x%016llX = %d", bitPos, value, static_cast<int>(m_result));
        return true;
    }

    bool performBitScanOperation()
    {
        m_findOperationCount.fetch_add(1, std::memory_order_relaxed);

        uint64_t value = m_operand1;

        switch (m_opType)
        {
        case BitManipulationOpType::BSF:
            // Bit scan forward (find first set bit, 0-indexed)
            m_result = value ? __builtin_ctzll(value) : 64;
            break;

        case BitManipulationOpType::BSR:
            // Bit scan reverse (find last set bit, 0-indexed)
            m_result = value ? (63 - __builtin_clzll(value)) : 64;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Bit scan: 0x%016llX -> position %llu", value, m_result);
        return true;
    }

    bool performAdvancedBitOperation()
    {
        m_advancedBitOpCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case BitManipulationOpType::PDEP:
            // Parallel bits deposit
            {
                uint64_t src = m_operand1;
                uint64_t mask = m_operand2;
                m_result = 0;

                for (int i = 0, k = 0; i < 64; i++)
                {
                    if (mask & (1ULL << i))
                    {
                        if (src & (1ULL << k))
                        {
                            m_result |= (1ULL << i);
                        }
                        k++;
                    }
                }
            }
            break;

        case BitManipulationOpType::PEXT:
            // Parallel bits extract
            {
                uint64_t src = m_operand1;
                uint64_t mask = m_operand2;
                m_result = 0;

                for (int i = 0, k = 0; i < 64; i++)
                {
                    if (mask & (1ULL << i))
                    {
                        if (src & (1ULL << i))
                        {
                            m_result |= (1ULL << k);
                        }
                        k++;
                    }
                }
            }
            break;

        case BitManipulationOpType::ANDN:
            // AND NOT operation
            m_result = (~m_operand1) & m_operand2;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Advanced bit operation: 0x%016llX, 0x%016llX -> 0x%016llX", m_operand1, m_operand2, m_result);
        return true;
    }

    bool performBitFieldOperation()
    {
        uint8_t start = (m_operand2 >> 8) & 0xFF;
        uint8_t length = m_operand2 & 0xFF;

        switch (m_opType)
        {
        case BitManipulationOpType::BEXTR:
            // Bit extract
            {
                if (length == 0 || length > 64)
                {
                    m_result = 0;
                }
                else
                {
                    uint64_t mask = (1ULL << length) - 1;
                    m_result = (m_operand1 >> start) & mask;
                }
            }
            break;

        case BitManipulationOpType::BZHI:
            // Zero high bits
            if (start >= 64)
            {
                m_result = m_operand1;
            }
            else
            {
                uint64_t mask = (1ULL << start) - 1;
                m_result = m_operand1 & mask;
            }
            break;

        case BitManipulationOpType::BLSI:
            // Isolate lowest set bit
            m_result = m_operand1 & (-static_cast<int64_t>(m_operand1));
            break;

        case BitManipulationOpType::BLSMSK:
            // Mask up to lowest set bit
            m_result = m_operand1 ^ (m_operand1 - 1);
            break;

        case BitManipulationOpType::BLSR:
            // Reset lowest set bit
            m_result = m_operand1 & (m_operand1 - 1);
            break;

        default:
            return false;
        }

        DEBUG_LOG("Bit field operation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performParityOperation()
    {
        m_parityOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case BitManipulationOpType::PARITY:
            // Calculate parity of entire value
            m_result = __builtin_parityll(m_operand1);
            break;

        case BitManipulationOpType::PARITY8:
            // Calculate parity of each byte
            m_result = 0;
            for (int i = 0; i < 8; i++)
            {
                uint8_t byte = (m_operand1 >> (i * 8)) & 0xFF;
                uint8_t parity = __builtin_parity(byte);
                m_result |= (static_cast<uint64_t>(parity) << i);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Parity operation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performGrayCodeOperation()
    {
        m_grayCodeOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case BitManipulationOpType::GRAY:
            // Binary to Gray code
            m_result = m_operand1 ^ (m_operand1 >> 1);
            break;

        case BitManipulationOpType::IGRAY:
            // Gray code to binary
            {
                uint64_t gray = m_operand1;
                m_result = gray;
                for (int i = 1; i < 64; i <<= 1)
                {
                    m_result ^= (m_result >> i);
                }
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Gray code operation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performInterleaveOperation()
    {
        m_interleaveOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case BitManipulationOpType::INTLV:
            // Interleave bits (Morton encoding)
            {
                uint32_t x = m_operand1 & 0xFFFFFFFF;
                uint32_t y = (m_operand1 >> 32) & 0xFFFFFFFF;
                m_result = 0;

                for (int i = 0; i < 32; i++)
                {
                    if (x & (1U << i))
                        m_result |= (1ULL << (2 * i));
                    if (y & (1U << i))
                        m_result |= (1ULL << (2 * i + 1));
                }
            }
            break;

        case BitManipulationOpType::DINTLV:
            // De-interleave bits
            {
                uint64_t interleaved = m_operand1;
                uint32_t x = 0, y = 0;

                for (int i = 0; i < 32; i++)
                {
                    if (interleaved & (1ULL << (2 * i)))
                        x |= (1U << i);
                    if (interleaved & (1ULL << (2 * i + 1)))
                        y |= (1U << i);
                }

                m_result = (static_cast<uint64_t>(y) << 32) | x;
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Interleave operation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performMatrixOperation()
    {
        m_matrixOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case BitManipulationOpType::BTRANS:
            // Bit matrix transpose (8x8 matrix)
            {
                uint64_t matrix = m_operand1;
                m_result = 0;

                for (int i = 0; i < 8; i++)
                {
                    for (int j = 0; j < 8; j++)
                    {
                        if (matrix & (1ULL << (i * 8 + j)))
                        {
                            m_result |= (1ULL << (j * 8 + i));
                        }
                    }
                }
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Matrix operation: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

    bool performPopulationCountVariant()
    {
        m_countOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case BitManipulationOpType::POPCNT8:
            // Population count in each byte
            m_result = 0;
            for (int i = 0; i < 8; i++)
            {
                uint8_t byte = (m_operand1 >> (i * 8)) & 0xFF;
                uint8_t count = __builtin_popcount(byte);
                m_result |= (static_cast<uint64_t>(count) << (i * 8));
            }
            break;

        case BitManipulationOpType::POPCNT16:
            // Population count in each word
            m_result = 0;
            for (int i = 0; i < 4; i++)
            {
                uint16_t word = (m_operand1 >> (i * 16)) & 0xFFFF;
                uint16_t count = __builtin_popcount(word);
                m_result |= (static_cast<uint64_t>(count) << (i * 16));
            }
            break;

        case BitManipulationOpType::POPCNT32:
            // Population count in each longword
            m_result = 0;
            for (int i = 0; i < 2; i++)
            {
                uint32_t lword = (m_operand1 >> (i * 32)) & 0xFFFFFFFF;
                uint32_t count = __builtin_popcount(lword);
                m_result |= (static_cast<uint64_t>(count) << (i * 32));
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Population count variant: 0x%016llX -> 0x%016llX", m_operand1, m_result);
        return true;
    }

  private:
     BitManipulationOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg1;
     uint8_t m_srcReg2;
     uint8_t m_immediate;
     bool m_useImmediate;

    BitWidth m_bitWidth;
    ShiftType m_shiftType;
    uint64_t m_operand1;
    uint64_t m_operand2;
    uint64_t m_result;
    uint8_t m_shiftAmount;
    uint8_t m_bitPosition;

    std::atomic<uint64_t> m_shiftOperationCount;
    std::atomic<uint64_t> m_countOperationCount;
    std::atomic<uint64_t> m_findOperationCount;
    std::atomic<uint64_t> m_reversalOperationCount;
    std::atomic<uint64_t> m_testOperationCount;
    std::atomic<uint64_t> m_advancedBitOpCount;
    std::atomic<uint64_t> m_parityOperationCount;
    std::atomic<uint64_t> m_matrixOperationCount;
    std::atomic<uint64_t> m_interleaveOperationCount;
    std::atomic<uint64_t> m_grayCodeOperationCount;
    std::atomic<uint64_t> m_overflowCount;

    // Prevent copying for performance
    alphaBitManipulationInstruction(const alphaBitManipulationInstruction &) = delete;
    alphaBitManipulationInstruction &operator=(const alphaBitManipulationInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaAdvancedFloatingPointInstruction : public alphaInstructionBase
{
  public:
    enum class AdvancedFPOpType : uint16_t
    {
        // Integer/Floating Point Conversion Operations
        CVTLQ = 0x010, // Convert longword to quadword
        CVTQL = 0x030, // Convert quadword to longword
        CVTQF = 0x0BC, // Convert quadword to F-format
        CVTQG = 0x0BE, // Convert quadword to G-format
        CVTQS = 0x0BC, // Convert quadword to S-format
        CVTQT = 0x0BE, // Convert quadword to T-format
        CVTFQ = 0x0AF, // Convert F-format to quadword
        CVTGQ = 0x0AF, // Convert G-format to quadword
        CVTSQ = 0x0AF, // Convert S-format to quadword
        CVTTQ = 0x0AF, // Convert T-format to quadword

        // Integer/Float Transfer Operations
        ITOFS = 0x014, // Transfer integer to float single
        ITOFT = 0x024, // Transfer integer to float double
        ITOFF = 0x015, // Transfer integer to float F-format
        ITOFG = 0x025, // Transfer integer to float G-format
        FTOIT = 0x01C, // Transfer float to integer
        FTOIS = 0x01D, // Transfer float single to integer
        FTOIG = 0x02D, // Transfer float G-format to integer
        FTOIF = 0x01E, // Transfer float F-format to integer

        // Advanced Floating Point Conditional Moves
        FCMOVEQ = 0x02A,  // Float conditional move if equal
        FCMOVNE = 0x02B,  // Float conditional move if not equal
        FCMOVLT = 0x02C,  // Float conditional move if less than
        FCMOVGE = 0x02D,  // Float conditional move if greater or equal
        FCMOVLE = 0x02E,  // Float conditional move if less or equal
        FCMOVGT = 0x02F,  // Float conditional move if greater than
        FCMOVUN = 0x030,  // Float conditional move if unordered
        FCMOVORD = 0x031, // Float conditional move if ordered

        // Extended Floating Point Comparisons
        CMPTEQL = 0x0A5, // Compare equal with exception on NaN
        CMPTUN = 0x0A4,  // Compare unordered
        CMPTLT = 0x0A6,  // Compare less than
        CMPTLE = 0x0A7,  // Compare less than or equal
        CMPTGT = 0x0A6,  // Compare greater than (swapped operands)
        CMPTGE = 0x0A7,  // Compare greater than or equal (swapped operands)

        // Floating Point Control Register Operations
        EXCB = 0x004,    // Exception barrier
        TRAPB = 0x000,   // Trap barrier
        MF_FPCR = 0x025, // Move from floating point control register
        MT_FPCR = 0x024, // Move to floating point control register

        // Advanced Floating Point Arithmetic
        ADDQ = 0x000,  // Add quadword (integer in FP register)
        SUBQ = 0x001,  // Subtract quadword
        MULQ = 0x003,  // Multiply quadword
        UMULH = 0x030, // Unsigned multiply high

        // Floating Point Select Operations
        FSEL = 0x032, // Floating point select
        FMAX = 0x033, // Floating point maximum
        FMIN = 0x034, // Floating point minimum
        FABS = 0x035, // Floating point absolute value
        FNEG = 0x036, // Floating point negate

        // IEEE 754 Special Operations
        FPCLASS = 0x040,  // Floating point classify
        ISINF = 0x041,    // Test for infinity
        ISNAN = 0x042,    // Test for NaN
        ISNORMAL = 0x043, // Test for normal number
        ISFINITE = 0x044, // Test for finite number
        ISZERO = 0x045,   // Test for zero
        SIGNBIT = 0x046,  // Test sign bit

        // Floating Point Scale Operations
        SCALB = 0x050, // Scale by power of 2
        LOGB = 0x051,  // Extract exponent
        FREXP = 0x052, // Extract mantissa and exponent
        LDEXP = 0x053, // Load exponent

        // Floating Point Rounding Operations
        RINT = 0x060,      // Round to nearest integer
        NEARBYINT = 0x061, // Round to nearest integer (no exceptions)
        TRUNC = 0x062,     // Truncate to integer
        FLOOR = 0x063,     // Floor function
        CEIL = 0x064,      // Ceiling function
        ROUND = 0x065,     // Round to nearest integer (away from zero)

        // Floating Point Remainder Operations
        FREM = 0x070,      // Floating point remainder
        REMAINDER = 0x071, // IEEE remainder
        REMQUO = 0x072,    // Remainder and quotient

        // Floating Point Next/Previous Operations
        NEXTAFTER = 0x080, // Next representable value
        NEXTUP = 0x081,    // Next value toward positive infinity
        NEXTDOWN = 0x082,  // Next value toward negative infinity

        // Floating Point Multiply-Add Operations
        FMADD = 0x090,  // Fused multiply-add
        FMSUB = 0x091,  // Fused multiply-subtract
        FNMADD = 0x092, // Fused negative multiply-add
        FNMSUB = 0x093, // Fused negative multiply-subtract

        // Decimal Floating Point Operations
        DFADD = 0x0A0, // Decimal floating point add
        DFSUB = 0x0A1, // Decimal floating point subtract
        DFMUL = 0x0A2, // Decimal floating point multiply
        DFDIV = 0x0A3, // Decimal floating point divide

        // Vector Floating Point Operations
        VFADD = 0x0B0, // Vector floating point add
        VFSUB = 0x0B1, // Vector floating point subtract
        VFMUL = 0x0B2, // Vector floating point multiply
        VFDIV = 0x0B3, // Vector floating point divide
        VFDOT = 0x0B4, // Vector floating point dot product

        // Unknown operation
        UNKNOWN = 0xFFFF
    };

    enum class FloatingPointFormat : uint8_t
    {
        IEEE_SINGLE = 0, // 32-bit IEEE 754 single precision
        IEEE_DOUBLE = 1, // 64-bit IEEE 754 double precision
        VAX_F = 2,       // 32-bit VAX F-format
        VAX_G = 3,       // 64-bit VAX G-format
        VAX_D = 4,       // 64-bit VAX D-format
        DECIMAL32 = 5,   // 32-bit decimal floating point
        DECIMAL64 = 6,   // 64-bit decimal floating point
        QUADWORD_INT = 7 // 64-bit integer in FP register
    };

    enum class RoundingMode : uint8_t
    {
        ROUND_NEAREST = 0,     // Round to nearest (ties to even)
        ROUND_DOWN = 1,        // Round toward negative infinity
        ROUND_UP = 2,          // Round toward positive infinity
        ROUND_TOWARD_ZERO = 3, // Round toward zero
        ROUND_DYNAMIC = 4      // Use dynamic rounding mode from FPCR
    };

    enum class FPClass : uint8_t
    {
        SNAN = 0,          // Signaling NaN
        QNAN = 1,          // Quiet NaN
        NEG_INF = 2,       // Negative infinity
        NEG_NORMAL = 3,    // Negative normal
        NEG_SUBNORMAL = 4, // Negative subnormal
        NEG_ZERO = 5,      // Negative zero
        POS_ZERO = 6,      // Positive zero
        POS_SUBNORMAL = 7, // Positive subnormal
        POS_NORMAL = 8,    // Positive normal
        POS_INF = 9        // Positive infinity
    };

    explicit alphaAdvancedFloatingPointInstruction(uint32_t opcode, AdvancedFPOpType opType, uint8_t destReg,
                                                   uint8_t srcReg1, uint8_t srcReg2)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_srcReg3(0), m_format(determineFormat(opType)), m_roundingMode(RoundingMode::ROUND_DYNAMIC), m_operand1(0.0),
          m_operand2(0.0), m_operand3(0.0), m_result(0.0), m_intOperand1(0), m_intOperand2(0), m_intResult(0),
          m_fpcr(0), m_fpClass(FPClass::POS_ZERO), m_conversionCount(0), m_transferCount(0), m_conditionalMoveCount(0),
          m_comparisonCount(0), m_controlRegisterCount(0), m_specialOpCount(0), m_roundingCount(0), m_remainderCount(0),
          m_fusedOpCount(0), m_vectorOpCount(0), m_classificationCount(0), m_exceptionCount(0)
    {
        DEBUG_LOG("alphaAdvancedFloatingPointInstruction created - OpType: 0x%04X, Dest: F%d, Src1: F%d, Src2: F%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2);
    }

    // Three-operand constructor (for fused operations)
    explicit alphaAdvancedFloatingPointInstruction(uint32_t opcode, AdvancedFPOpType opType, uint8_t destReg,
                                                   uint8_t srcReg1, uint8_t srcReg2, uint8_t srcReg3)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_srcReg3(srcReg3), m_format(determineFormat(opType)), m_roundingMode(RoundingMode::ROUND_DYNAMIC),
          m_operand1(0.0), m_operand2(0.0), m_operand3(0.0), m_result(0.0), m_intOperand1(0), m_intOperand2(0),
          m_intResult(0), m_fpcr(0), m_fpClass(FPClass::POS_ZERO), m_conversionCount(0), m_transferCount(0),
          m_conditionalMoveCount(0), m_comparisonCount(0), m_controlRegisterCount(0), m_specialOpCount(0),
          m_roundingCount(0), m_remainderCount(0), m_fusedOpCount(0), m_vectorOpCount(0), m_classificationCount(0),
          m_exceptionCount(0)
    {
        DEBUG_LOG("alphaAdvancedFloatingPointInstruction created (3-op) - OpType: 0x%04X, Dest: F%d, Src1: F%d, Src2: "
                  "F%d, Src3: F%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2, srcReg3);
    }

    virtual ~alphaAdvancedFloatingPointInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performAdvancedFPOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding advanced floating point instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        uint16_t function = opcode & 0x7FF;
        uint8_t rc = (opcode >> 0) & 0x1F;

        m_srcReg1 = ra;
        m_srcReg2 = rb;
        m_destReg = rc;
        m_srcReg3 = 0; // Will be set for 3-operand instructions

        // Determine advanced FP operation type
        switch (primaryOpcode)
        {
        case 0x17: // Floating point operate
            switch (function)
            {
            case 0x010:
                m_opType = AdvancedFPOpType::CVTLQ;
                break;
            case 0x030:
                m_opType = AdvancedFPOpType::CVTQL;
                break;
            case 0x0BC:
                m_opType = AdvancedFPOpType::CVTQS;
                break;
            case 0x0BE:
                m_opType = AdvancedFPOpType::CVTQT;
                break;
            case 0x0AF:
                m_opType = AdvancedFPOpType::CVTSQ;
                break;
            case 0x014:
                m_opType = AdvancedFPOpType::ITOFS;
                break;
            case 0x024:
                m_opType = AdvancedFPOpType::ITOFT;
                break;
            case 0x01C:
                m_opType = AdvancedFPOpType::FTOIT;
                break;
            case 0x02A:
                m_opType = AdvancedFPOpType::FCMOVEQ;
                break;
            case 0x02B:
                m_opType = AdvancedFPOpType::FCMOVNE;
                break;
            case 0x02C:
                m_opType = AdvancedFPOpType::FCMOVLT;
                break;
            case 0x02D:
                m_opType = AdvancedFPOpType::FCMOVGE;
                break;
            case 0x02E:
                m_opType = AdvancedFPOpType::FCMOVLE;
                break;
            case 0x02F:
                m_opType = AdvancedFPOpType::FCMOVGT;
                break;
            case 0x025:
                m_opType = AdvancedFPOpType::MF_FPCR;
                break;
            case 0x024:
                m_opType = AdvancedFPOpType::MT_FPCR;
                break;
            default:
                DEBUG_LOG("Unknown advanced FP function: 0x%03X", function);
                m_opType = AdvancedFPOpType::CVTQS;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown advanced FP primary opcode: 0x%02X", primaryOpcode);
            m_opType = AdvancedFPOpType::UNKNOWN;
            break;
        }

        DEBUG_LOG("Advanced FP instruction decoded - Type: 0x%04X, Dest: F%d, Src1: F%d, Src2: F%d",
                  static_cast<int>(m_opType), m_destReg, m_srcReg1, m_srcReg2);
    }



    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Fast transfer operations
        case AdvancedFPOpType::ITOFS:
        case AdvancedFPOpType::ITOFT:
        case AdvancedFPOpType::ITOFF:
        case AdvancedFPOpType::ITOFG:
        case AdvancedFPOpType::FTOIT:
        case AdvancedFPOpType::FTOIS:
        case AdvancedFPOpType::FTOIG:
        case AdvancedFPOpType::FTOIF:
            return 1;

        // Conversion operations
        case AdvancedFPOpType::CVTLQ:
        case AdvancedFPOpType::CVTQL:
            return 2;
        case AdvancedFPOpType::CVTQF:
        case AdvancedFPOpType::CVTQG:
        case AdvancedFPOpType::CVTQS:
        case AdvancedFPOpType::CVTQT:
        case AdvancedFPOpType::CVTFQ:
        case AdvancedFPOpType::CVTGQ:
        case AdvancedFPOpType::CVTSQ:
        case AdvancedFPOpType::CVTTQ:
            return 4;

        // Conditional moves (fast)
        case AdvancedFPOpType::FCMOVEQ:
        case AdvancedFPOpType::FCMOVNE:
        case AdvancedFPOpType::FCMOVLT:
        case AdvancedFPOpType::FCMOVGE:
        case AdvancedFPOpType::FCMOVLE:
        case AdvancedFPOpType::FCMOVGT:
        case AdvancedFPOpType::FCMOVUN:
        case AdvancedFPOpType::FCMOVORD:
            return 1;

        // Comparison operations
        case AdvancedFPOpType::CMPTEQL:
        case AdvancedFPOpType::CMPTUN:
        case AdvancedFPOpType::CMPTLT:
        case AdvancedFPOpType::CMPTLE:
        case AdvancedFPOpType::CMPTGT:
        case AdvancedFPOpType::CMPTGE:
            return 3;

        // Control register operations
        case AdvancedFPOpType::MF_FPCR:
        case AdvancedFPOpType::MT_FPCR:
            return 2;
        case AdvancedFPOpType::EXCB:
        case AdvancedFPOpType::TRAPB:
            return 5;

        // Integer arithmetic in FP registers
        case AdvancedFPOpType::ADDQ:
        case AdvancedFPOpType::SUBQ:
            return 1;
        case AdvancedFPOpType::MULQ:
            return 3;
        case AdvancedFPOpType::UMULH:
            return 4;

        // Special floating point operations
        case AdvancedFPOpType::FSEL:
        case AdvancedFPOpType::FMAX:
        case AdvancedFPOpType::FMIN:
        case AdvancedFPOpType::FABS:
        case AdvancedFPOpType::FNEG:
            return 1;

        // Classification operations
        case AdvancedFPOpType::FPCLASS:
        case AdvancedFPOpType::ISINF:
        case AdvancedFPOpType::ISNAN:
        case AdvancedFPOpType::ISNORMAL:
        case AdvancedFPOpType::ISFINITE:
        case AdvancedFPOpType::ISZERO:
        case AdvancedFPOpType::SIGNBIT:
            return 2;

        // Scale operations
        case AdvancedFPOpType::SCALB:
        case AdvancedFPOpType::LOGB:
        case AdvancedFPOpType::FREXP:
        case AdvancedFPOpType::LDEXP:
            return 3;

        // Rounding operations
        case AdvancedFPOpType::RINT:
        case AdvancedFPOpType::NEARBYINT:
        case AdvancedFPOpType::TRUNC:
        case AdvancedFPOpType::FLOOR:
        case AdvancedFPOpType::CEIL:
        case AdvancedFPOpType::ROUND:
            return 3;

        // Remainder operations
        case AdvancedFPOpType::FREM:
        case AdvancedFPOpType::REMAINDER:
        case AdvancedFPOpType::REMQUO:
            return 8;

        // Next/Previous operations
        case AdvancedFPOpType::NEXTAFTER:
        case AdvancedFPOpType::NEXTUP:
        case AdvancedFPOpType::NEXTDOWN:
            return 4;

        // Fused multiply-add operations
        case AdvancedFPOpType::FMADD:
        case AdvancedFPOpType::FMSUB:
        case AdvancedFPOpType::FNMADD:
        case AdvancedFPOpType::FNMSUB:
            return 4;

        // Decimal floating point operations
        case AdvancedFPOpType::DFADD:
        case AdvancedFPOpType::DFSUB:
            return 6;
        case AdvancedFPOpType::DFMUL:
            return 8;
        case AdvancedFPOpType::DFDIV:
            return 15;

        // Vector floating point operations
        case AdvancedFPOpType::VFADD:
        case AdvancedFPOpType::VFSUB:
            return 4;
        case AdvancedFPOpType::VFMUL:
            return 6;
        case AdvancedFPOpType::VFDIV:
            return 12;
        case AdvancedFPOpType::VFDOT:
            return 8;

        default:
            return 4;
        }
    }

    // Floating point classification
    bool isFloatingPoint() const override { return true; }

    // Performance-critical accessors
    inline AdvancedFPOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg1() const { return m_srcReg1; }
    inline uint8_t getSrcReg2() const { return m_srcReg2; }
    inline uint8_t getSrcReg3() const { return m_srcReg3; }
    inline FloatingPointFormat getFormat() const { return m_format; }
    inline RoundingMode getRoundingMode() const { return m_roundingMode; }
    inline double getResult() const { return m_result; }
    inline uint64_t getIntResult() const { return m_intResult; }
    inline uint64_t getFPCR() const { return m_fpcr; }
    inline FPClass getFPClass() const { return m_fpClass; }

    // Performance counters
    inline uint64_t getConversionCount() const { return m_conversionCount.load(std::memory_order_relaxed); }
    inline uint64_t getTransferCount() const { return m_transferCount.load(std::memory_order_relaxed); }
    inline uint64_t getConditionalMoveCount() const { return m_conditionalMoveCount.load(std::memory_order_relaxed); }
    inline uint64_t getComparisonCount() const { return m_comparisonCount.load(std::memory_order_relaxed); }
    inline uint64_t getControlRegisterCount() const { return m_controlRegisterCount.load(std::memory_order_relaxed); }
    inline uint64_t getSpecialOpCount() const { return m_specialOpCount.load(std::memory_order_relaxed); }
    inline uint64_t getRoundingCount() const { return m_roundingCount.load(std::memory_order_relaxed); }
    inline uint64_t getRemainderCount() const { return m_remainderCount.load(std::memory_order_relaxed); }
    inline uint64_t getFusedOpCount() const { return m_fusedOpCount.load(std::memory_order_relaxed); }
    inline uint64_t getVectorOpCount() const { return m_vectorOpCount.load(std::memory_order_relaxed); }
    inline uint64_t getClassificationCount() const { return m_classificationCount.load(std::memory_order_relaxed); }
    inline uint64_t getExceptionCount() const { return m_exceptionCount.load(std::memory_order_relaxed); }

    // Operation classification
    inline bool isConversionOperation() const
    {
        return (m_opType >= AdvancedFPOpType::CVTLQ && m_opType <= AdvancedFPOpType::CVTTQ);
    }

    inline bool isTransferOperation() const
    {
        return (m_opType >= AdvancedFPOpType::ITOFS && m_opType <= AdvancedFPOpType::FTOIF);
    }

    inline bool isConditionalMoveOperation() const
    {
        return (m_opType >= AdvancedFPOpType::FCMOVEQ && m_opType <= AdvancedFPOpType::FCMOVORD);
    }

    inline bool isFusedOperation() const
    {
        return (m_opType >= AdvancedFPOpType::FMADD && m_opType <= AdvancedFPOpType::FNMSUB);
    }

    inline bool isVectorOperation() const
    {
        return (m_opType >= AdvancedFPOpType::VFADD && m_opType <= AdvancedFPOpType::VFDOT);
    }

    // Hot path execution support
    inline void setOperands(double op1, double op2, double op3 = 0.0)
    {
        m_operand1 = op1;
        m_operand2 = op2;
        m_operand3 = op3;
    }

    inline void setIntOperands(uint64_t op1, uint64_t op2)
    {
        m_intOperand1 = op1;
        m_intOperand2 = op2;
    }

    inline void setRoundingMode(RoundingMode mode) { m_roundingMode = mode; }
    inline void setFPCR(uint64_t fpcr) { m_fpcr = fpcr; }

  private:
    FloatingPointFormat determineFormat(AdvancedFPOpType opType) const
    {
        switch (opType)
        {
        case AdvancedFPOpType::CVTQS:
        case AdvancedFPOpType::CVTSQ:
        case AdvancedFPOpType::ITOFS:
        case AdvancedFPOpType::FTOIS:
            return FloatingPointFormat::IEEE_SINGLE;

        case AdvancedFPOpType::CVTQT:
        case AdvancedFPOpType::CVTTQ:
        case AdvancedFPOpType::ITOFT:
        case AdvancedFPOpType::FTOIT:
            return FloatingPointFormat::IEEE_DOUBLE;

        case AdvancedFPOpType::CVTQF:
        case AdvancedFPOpType::CVTFQ:
        case AdvancedFPOpType::ITOFF:
        case AdvancedFPOpType::FTOIF:
            return FloatingPointFormat::VAX_F;

        case AdvancedFPOpType::CVTQG:
        case AdvancedFPOpType::CVTGQ:
        case AdvancedFPOpType::ITOFG:
        case AdvancedFPOpType::FTOIG:
            return FloatingPointFormat::VAX_G;

        case AdvancedFPOpType::DFADD:
        case AdvancedFPOpType::DFSUB:
        case AdvancedFPOpType::DFMUL:
        case AdvancedFPOpType::DFDIV:
            return FloatingPointFormat::DECIMAL64;

        case AdvancedFPOpType::ADDQ:
        case AdvancedFPOpType::SUBQ:
        case AdvancedFPOpType::MULQ:
        case AdvancedFPOpType::UMULH:
        case AdvancedFPOpType::CVTLQ:
        case AdvancedFPOpType::CVTQL:
            return FloatingPointFormat::QUADWORD_INT;

        default:
            return FloatingPointFormat::IEEE_DOUBLE;
        }
    }

    bool performAdvancedFPOperation()
    {
        switch (m_opType)
        {
        // Conversion operations
        case AdvancedFPOpType::CVTLQ:
        case AdvancedFPOpType::CVTQL:
        case AdvancedFPOpType::CVTQF:
        case AdvancedFPOpType::CVTQG:
        case AdvancedFPOpType::CVTQS:
        case AdvancedFPOpType::CVTQT:
        case AdvancedFPOpType::CVTFQ:
        case AdvancedFPOpType::CVTGQ:
        case AdvancedFPOpType::CVTSQ:
        case AdvancedFPOpType::CVTTQ:
            return performConversionOperation();

        // Transfer operations
        case AdvancedFPOpType::ITOFS:
        case AdvancedFPOpType::ITOFT:
        case AdvancedFPOpType::ITOFF:
        case AdvancedFPOpType::ITOFG:
        case AdvancedFPOpType::FTOIT:
        case AdvancedFPOpType::FTOIS:
        case AdvancedFPOpType::FTOIG:
        case AdvancedFPOpType::FTOIF:
            return performTransferOperation();

        // Conditional moves
        case AdvancedFPOpType::FCMOVEQ:
        case AdvancedFPOpType::FCMOVNE:
        case AdvancedFPOpType::FCMOVLT:
        case AdvancedFPOpType::FCMOVGE:
        case AdvancedFPOpType::FCMOVLE:
        case AdvancedFPOpType::FCMOVGT:
        case AdvancedFPOpType::FCMOVUN:
        case AdvancedFPOpType::FCMOVORD:
            return performConditionalMoveOperation();

        // Comparison operations
        case AdvancedFPOpType::CMPTEQL:
        case AdvancedFPOpType::CMPTUN:
        case AdvancedFPOpType::CMPTLT:
        case AdvancedFPOpType::CMPTLE:
        case AdvancedFPOpType::CMPTGT:
        case AdvancedFPOpType::CMPTGE:
            return performComparisonOperation();

        // Control register operations
        case AdvancedFPOpType::EXCB:
        case AdvancedFPOpType::TRAPB:
        case AdvancedFPOpType::MF_FPCR:
        case AdvancedFPOpType::MT_FPCR:
            return performControlRegisterOperation();

        // Integer arithmetic
        case AdvancedFPOpType::ADDQ:
        case AdvancedFPOpType::SUBQ:
        case AdvancedFPOpType::MULQ:
        case AdvancedFPOpType::UMULH:
            return performIntegerArithmetic();

        // Special floating point operations
        case AdvancedFPOpType::FSEL:
        case AdvancedFPOpType::FMAX:
        case AdvancedFPOpType::FMIN:
        case AdvancedFPOpType::FABS:
        case AdvancedFPOpType::FNEG:
            return performSpecialFPOperation();

        // Classification operations
        case AdvancedFPOpType::FPCLASS:
        case AdvancedFPOpType::ISINF:
        case AdvancedFPOpType::ISNAN:
        case AdvancedFPOpType::ISNORMAL:
        case AdvancedFPOpType::ISFINITE:
        case AdvancedFPOpType::ISZERO:
        case AdvancedFPOpType::SIGNBIT:
            return performClassificationOperation();

        // Scale operations
        case AdvancedFPOpType::SCALB:
        case AdvancedFPOpType::LOGB:
        case AdvancedFPOpType::FREXP:
        case AdvancedFPOpType::LDEXP:
            return performScaleOperation();

        // Rounding operations
        case AdvancedFPOpType::RINT:
        case AdvancedFPOpType::NEARBYINT:
        case AdvancedFPOpType::TRUNC:
        case AdvancedFPOpType::FLOOR:
        case AdvancedFPOpType::CEIL:
        case AdvancedFPOpType::ROUND:
            return performRoundingOperation();

        // Remainder operations
        case AdvancedFPOpType::FREM:
        case AdvancedFPOpType::REMAINDER:
        case AdvancedFPOpType::REMQUO:
            return performRemainderOperation();

        // Next/Previous operations
        case AdvancedFPOpType::NEXTAFTER:
        case AdvancedFPOpType::NEXTUP:
        case AdvancedFPOpType::NEXTDOWN:
            return performNextOperation();

        // Fused operations
        case AdvancedFPOpType::FMADD:
        case AdvancedFPOpType::FMSUB:
        case AdvancedFPOpType::FNMADD:
        case AdvancedFPOpType::FNMSUB:
            return performFusedOperation();

        // Decimal floating point operations
        case AdvancedFPOpType::DFADD:
        case AdvancedFPOpType::DFSUB:
        case AdvancedFPOpType::DFMUL:
        case AdvancedFPOpType::DFDIV:
            return performDecimalFPOperation();

        // Vector operations
        case AdvancedFPOpType::VFADD:
        case AdvancedFPOpType::VFSUB:
        case AdvancedFPOpType::VFMUL:
        case AdvancedFPOpType::VFDIV:
        case AdvancedFPOpType::VFDOT:
            return performVectorFPOperation();

        default:
            return false;
        }
    }

    bool performConversionOperation()
    {
        m_conversionCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::CVTLQ:
            // Convert 32-bit integer to 64-bit integer
            m_intResult = static_cast<int64_t>(static_cast<int32_t>(m_intOperand1));
            break;

        case AdvancedFPOpType::CVTQL:
            // Convert 64-bit integer to 32-bit integer
            m_intResult = static_cast<uint32_t>(m_intOperand1);
            break;

        case AdvancedFPOpType::CVTQS:
            // Convert 64-bit integer to single precision float
            m_result = static_cast<float>(static_cast<int64_t>(m_intOperand1));
            break;

        case AdvancedFPOpType::CVTQT:
            // Convert 64-bit integer to double precision float
            m_result = static_cast<double>(static_cast<int64_t>(m_intOperand1));
            break;

        case AdvancedFPOpType::CVTSQ:
            // Convert single precision float to 64-bit integer
            m_intResult = static_cast<int64_t>(static_cast<float>(m_operand1));
            break;

        case AdvancedFPOpType::CVTTQ:
            // Convert double precision float to 64-bit integer
            m_intResult = static_cast<int64_t>(m_operand1);
            break;

        case AdvancedFPOpType::CVTQF:
        case AdvancedFPOpType::CVTFQ:
        case AdvancedFPOpType::CVTQG:
        case AdvancedFPOpType::CVTGQ:
            // VAX format conversions (simplified)
            m_result = m_operand1;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Conversion operation: %f -> %f (int: %llu -> %llu)", m_operand1, m_result, m_intOperand1,
                  m_intResult);
        return true;
    }

    bool performTransferOperation()
    {
        m_transferCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::ITOFS:
        case AdvancedFPOpType::ITOFT:
        case AdvancedFPOpType::ITOFF:
        case AdvancedFPOpType::ITOFG:
            // Transfer integer bits to floating point register (no conversion)
            memcpy(&m_result, &m_intOperand1, sizeof(double));
            break;

        case AdvancedFPOpType::FTOIT:
        case AdvancedFPOpType::FTOIS:
        case AdvancedFPOpType::FTOIG:
        case AdvancedFPOpType::FTOIF:
            // Transfer floating point bits to integer register (no conversion)
            memcpy(&m_intResult, &m_operand1, sizeof(uint64_t));
            break;

        default:
            return false;
        }

        DEBUG_LOG("Transfer operation: transferred bits between registers");
        return true;
    }

    bool performConditionalMoveOperation()
    {
        m_conditionalMoveCount.fetch_add(1, std::memory_order_relaxed);

        bool condition = false;

        switch (m_opType)
        {
        case AdvancedFPOpType::FCMOVEQ:
            condition = (m_operand1 == 0.0);
            break;
        case AdvancedFPOpType::FCMOVNE:
            condition = (m_operand1 != 0.0);
            break;
        case AdvancedFPOpType::FCMOVLT:
            condition = (m_operand1 < 0.0);
            break;
        case AdvancedFPOpType::FCMOVGE:
            condition = (m_operand1 >= 0.0);
            break;
        case AdvancedFPOpType::FCMOVLE:
            condition = (m_operand1 <= 0.0);
            break;
        case AdvancedFPOpType::FCMOVGT:
            condition = (m_operand1 > 0.0);
            break;
        case AdvancedFPOpType::FCMOVUN:
            condition = std::isnan(m_operand1);
            break;
        case AdvancedFPOpType::FCMOVORD:
            condition = !std::isnan(m_operand1);
            break;
        default:
            return false;
        }

        if (condition)
        {
            m_result = m_operand2;
        }
        else
        {
            m_result = m_operand1; // Keep original value
        }

        DEBUG_LOG("Conditional move: condition=%s, result=%f", condition ? "true" : "false", m_result);
        return true;
    }

    bool performComparisonOperation()
    {
        m_comparisonCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::CMPTEQL:
            m_result = (m_operand1 == m_operand2) ? 1.0 : 0.0;
            if (std::isnan(m_operand1) || std::isnan(m_operand2))
            {
                m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
            }
            break;

        case AdvancedFPOpType::CMPTUN:
            m_result = (std::isnan(m_operand1) || std::isnan(m_operand2)) ? 1.0 : 0.0;
            break;

        case AdvancedFPOpType::CMPTLT:
            m_result = (m_operand1 < m_operand2) ? 1.0 : 0.0;
            break;

        case AdvancedFPOpType::CMPTLE:
            m_result = (m_operand1 <= m_operand2) ? 1.0 : 0.0;
            break;

        case AdvancedFPOpType::CMPTGT:
            m_result = (m_operand1 > m_operand2) ? 1.0 : 0.0;
            break;

        case AdvancedFPOpType::CMPTGE:
            m_result = (m_operand1 >= m_operand2) ? 1.0 : 0.0;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Comparison operation: %f vs %f = %f", m_operand1, m_operand2, m_result);
        return true;
    }

    bool performControlRegisterOperation()
    {
        m_controlRegisterCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::MF_FPCR:
            // Move from floating point control register
            m_intResult = m_fpcr;
            break;

        case AdvancedFPOpType::MT_FPCR:
            // Move to floating point control register
            m_fpcr = m_intOperand1;
            break;

        case AdvancedFPOpType::EXCB:
            // Exception barrier - ensure all pending exceptions are processed
            break;

        case AdvancedFPOpType::TRAPB:
            // Trap barrier - ensure all pending traps are processed
            break;

        default:
            return false;
        }

        DEBUG_LOG("Control register operation completed");
        return true;
    }

    bool performIntegerArithmetic()
    {
        switch (m_opType)
        {
        case AdvancedFPOpType::ADDQ:
            m_intResult = m_intOperand1 + m_intOperand2;
            break;

        case AdvancedFPOpType::SUBQ:
            m_intResult = m_intOperand1 - m_intOperand2;
            break;

        case AdvancedFPOpType::MULQ:
            m_intResult = m_intOperand1 * m_intOperand2;
            break;

        case AdvancedFPOpType::UMULH:
            // Unsigned multiply high (128-bit multiply, return high 64 bits)
            {
                __uint128_t result = static_cast<__uint128_t>(m_intOperand1) * static_cast<__uint128_t>(m_intOperand2);
                m_intResult = static_cast<uint64_t>(result >> 64);
            }
            break;

        default:
            return false;
        }

        DEBUG_LOG("Integer arithmetic: %llu op %llu = %llu", m_intOperand1, m_intOperand2, m_intResult);
        return true;
    }

    bool performSpecialFPOperation()
    {
        m_specialOpCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::FSEL:
            // Floating point select based on sign
            m_result = (m_operand1 >= 0.0) ? m_operand2 : m_operand3;
            break;

        case AdvancedFPOpType::FMAX:
            m_result = std::fmax(m_operand1, m_operand2);
            break;

        case AdvancedFPOpType::FMIN:
            m_result = std::fmin(m_operand1, m_operand2);
            break;

        case AdvancedFPOpType::FABS:
            m_result = std::fabs(m_operand1);
            break;

        case AdvancedFPOpType::FNEG:
            m_result = -m_operand1;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Special FP operation: %f -> %f", m_operand1, m_result);
        return true;
    }

    bool performClassificationOperation()
    {
        m_classificationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::FPCLASS:
            // Classify floating point number
            if (std::isnan(m_operand1))
            {
                m_fpClass = std::signbit(m_operand1) ? FPClass::SNAN : FPClass::QNAN;
            }
            else if (std::isinf(m_operand1))
            {
                m_fpClass = std::signbit(m_operand1) ? FPClass::NEG_INF : FPClass::POS_INF;
            }
            else if (m_operand1 == 0.0)
            {
                m_fpClass = std::signbit(m_operand1) ? FPClass::NEG_ZERO : FPClass::POS_ZERO;
            }
            else if (std::isnormal(m_operand1))
            {
                m_fpClass = std::signbit(m_operand1) ? FPClass::NEG_NORMAL : FPClass::POS_NORMAL;
            }
            else
            {
                m_fpClass = std::signbit(m_operand1) ? FPClass::NEG_SUBNORMAL : FPClass::POS_SUBNORMAL;
            }
            m_intResult = static_cast<uint64_t>(m_fpClass);
            break;

        case AdvancedFPOpType::ISINF:
            m_intResult = std::isinf(m_operand1) ? 1 : 0;
            break;

        case AdvancedFPOpType::ISNAN:
            m_intResult = std::isnan(m_operand1) ? 1 : 0;
            break;

        case AdvancedFPOpType::ISNORMAL:
            m_intResult = std::isnormal(m_operand1) ? 1 : 0;
            break;

        case AdvancedFPOpType::ISFINITE:
            m_intResult = std::isfinite(m_operand1) ? 1 : 0;
            break;

        case AdvancedFPOpType::ISZERO:
            m_intResult = (m_operand1 == 0.0) ? 1 : 0;
            break;

        case AdvancedFPOpType::SIGNBIT:
            m_intResult = std::signbit(m_operand1) ? 1 : 0;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Classification operation: %f -> class %llu", m_operand1, m_intResult);
        return true;
    }

    bool performScaleOperation()
    {
        switch (m_opType)
        {
        case AdvancedFPOpType::SCALB:
            // Scale by power of 2
            m_result = std::scalbn(m_operand1, static_cast<int>(m_intOperand2));
            break;

        case AdvancedFPOpType::LOGB:
            // Extract exponent
            m_result = std::logb(m_operand1);
            break;

        case AdvancedFPOpType::FREXP:
            // Extract mantissa and exponent
            {
                int exp;
                m_result = std::frexp(m_operand1, &exp);
                m_intResult = exp;
            }
            break;

        case AdvancedFPOpType::LDEXP:
            // Load exponent
            m_result = std::ldexp(m_operand1, static_cast<int>(m_intOperand2));
            break;

        default:
            return false;
        }

        DEBUG_LOG("Scale operation: %f -> %f", m_operand1, m_result);
        return true;
    }

    bool performRoundingOperation()
    {
        m_roundingCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::RINT:
            m_result = std::rint(m_operand1);
            break;

        case AdvancedFPOpType::NEARBYINT:
            m_result = std::nearbyint(m_operand1);
            break;

        case AdvancedFPOpType::TRUNC:
            m_result = std::trunc(m_operand1);
            break;

        case AdvancedFPOpType::FLOOR:
            m_result = std::floor(m_operand1);
            break;

        case AdvancedFPOpType::CEIL:
            m_result = std::ceil(m_operand1);
            break;

        case AdvancedFPOpType::ROUND:
            m_result = std::round(m_operand1);
            break;

        default:
            return false;
        }

        DEBUG_LOG("Rounding operation: %f -> %f", m_operand1, m_result);
        return true;
    }

    bool performRemainderOperation()
    {
        m_remainderCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::FREM:
            m_result = std::fmod(m_operand1, m_operand2);
            break;

        case AdvancedFPOpType::REMAINDER:
            m_result = std::remainder(m_operand1, m_operand2);
            break;

        case AdvancedFPOpType::REMQUO:
        {
            int quo;
            m_result = std::remquo(m_operand1, m_operand2, &quo);
            m_intResult = quo;
        }
        break;

        default:
            return false;
        }

        DEBUG_LOG("Remainder operation: %f mod %f = %f", m_operand1, m_operand2, m_result);
        return true;
    }

    bool performNextOperation()
    {
        switch (m_opType)
        {
        case AdvancedFPOpType::NEXTAFTER:
            m_result = std::nextafter(m_operand1, m_operand2);
            break;

        case AdvancedFPOpType::NEXTUP:
            m_result = std::nextafter(m_operand1, INFINITY);
            break;

        case AdvancedFPOpType::NEXTDOWN:
            m_result = std::nextafter(m_operand1, -INFINITY);
            break;

        default:
            return false;
        }

        DEBUG_LOG("Next operation: %f -> %f", m_operand1, m_result);
        return true;
    }

    bool performFusedOperation()
    {
        m_fusedOpCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case AdvancedFPOpType::FMADD:
            // Fused multiply-add: (a * b) + c
            m_result = std::fma(m_operand1, m_operand2, m_operand3);
            break;

        case AdvancedFPOpType::FMSUB:
            // Fused multiply-subtract: (a * b) - c
            m_result = std::fma(m_operand1, m_operand2, -m_operand3);
            break;

        case AdvancedFPOpType::FNMADD:
            // Fused negative multiply-add: -(a * b) + c
            m_result = std::fma(-m_operand1, m_operand2, m_operand3);
            break;

        case AdvancedFPOpType::FNMSUB:
            // Fused negative multiply-subtract: -(a * b) - c
            m_result = std::fma(-m_operand1, m_operand2, -m_operand3);
            break;

        default:
            return false;
        }

        DEBUG_LOG("Fused operation: (%f * %f) +/- %f = %f", m_operand1, m_operand2, m_operand3, m_result);
        return true;
    }

    bool performDecimalFPOperation()
    {
        // Simplified decimal floating point operations
        switch (m_opType)
        {
        case AdvancedFPOpType::DFADD:
            m_result = m_operand1 + m_operand2;
            break;

        case AdvancedFPOpType::DFSUB:
            m_result = m_operand1 - m_operand2;
            break;

        case AdvancedFPOpType::DFMUL:
            m_result = m_operand1 * m_operand2;
            break;

        case AdvancedFPOpType::DFDIV:
            if (m_operand2 == 0.0)
            {
                m_exceptionCount.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            m_result = m_operand1 / m_operand2;
            break;

        default:
            return false;
        }

        DEBUG_LOG("Decimal FP operation: %f op %f = %f", m_operand1, m_operand2, m_result);
        return true;
    }

    bool performVectorFPOperation()
    {
        m_vectorOpCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified vector operations (treating operands as 2-element vectors)
        union
        {
            double d;
            struct
            {
                float f1, f2;
            } f;
        } op1, op2, res;

        op1.d = m_operand1;
        op2.d = m_operand2;

        switch (m_opType)
        {
        case AdvancedFPOpType::VFADD:
            res.f.f1 = op1.f.f1 + op2.f.f1;
            res.f.f2 = op1.f.f2 + op2.f.f2;
            break;

        case AdvancedFPOpType::VFSUB:
            res.f.f1 = op1.f.f1 - op2.f.f1;
            res.f.f2 = op1.f.f2 - op2.f.f2;
            break;

        case AdvancedFPOpType::VFMUL:
            res.f.f1 = op1.f.f1 * op2.f.f1;
            res.f.f2 = op1.f.f2 * op2.f.f2;
            break;

        case AdvancedFPOpType::VFDIV:
            res.f.f1 = op1.f.f1 / op2.f.f1;
            res.f.f2 = op1.f.f2 / op2.f.f2;
            break;

        case AdvancedFPOpType::VFDOT:
            // Dot product
            res.f.f1 = op1.f.f1 * op2.f.f1 + op1.f.f2 * op2.f.f2;
            res.f.f2 = 0.0f;
            break;

        default:
            return false;
        }

        m_result = res.d;

        DEBUG_LOG("Vector FP operation: <%f,%f> op <%f,%f> = <%f,%f>", op1.f.f1, op1.f.f2, op2.f.f1, op2.f.f2, res.f.f1,
                  res.f.f2);
        return true;
    }

  private:
     AdvancedFPOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg1;
     uint8_t m_srcReg2;
     uint8_t m_srcReg3;
     FloatingPointFormat m_format;

    RoundingMode m_roundingMode;
    double m_operand1;
    double m_operand2;
    double m_operand3;
    double m_result;
    uint64_t m_intOperand1;
    uint64_t m_intOperand2;
    uint64_t m_intResult;
    uint64_t m_fpcr;
    FPClass m_fpClass;

    std::atomic<uint64_t> m_conversionCount;
    std::atomic<uint64_t> m_transferCount;
    std::atomic<uint64_t> m_conditionalMoveCount;
    std::atomic<uint64_t> m_comparisonCount;
    std::atomic<uint64_t> m_controlRegisterCount;
    std::atomic<uint64_t> m_specialOpCount;
    std::atomic<uint64_t> m_roundingCount;
    std::atomic<uint64_t> m_remainderCount;
    std::atomic<uint64_t> m_fusedOpCount;
    std::atomic<uint64_t> m_vectorOpCount;
    std::atomic<uint64_t> m_classificationCount;
    std::atomic<uint64_t> m_exceptionCount;

    // Prevent copying for performance
    alphaAdvancedFloatingPointInstruction(const alphaAdvancedFloatingPointInstruction &) = delete;
    alphaAdvancedFloatingPointInstruction &operator=(const alphaAdvancedFloatingPointInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaConditionalMoveInstruction : public alphaInstructionBase
{
  public:
    enum class ConditionalMoveOpType : uint16_t
    {
        // Basic integer conditional moves
        CMOVEQ = 0x24, // Conditional move if equal to zero
        CMOVNE = 0x26, // Conditional move if not equal to zero
        CMOVLT = 0x44, // Conditional move if less than zero
        CMOVLE = 0x64, // Conditional move if less than or equal to zero
        CMOVGT = 0x66, // Conditional move if greater than zero
        CMOVGE = 0x46, // Conditional move if greater than or equal to zero

        // Bit-based conditional moves
        CMOVLBC = 0x16, // Conditional move if low bit clear
        CMOVLBS = 0x14, // Conditional move if low bit set

        // Extended bit test conditional moves
        CMOVBC0 = 0x17, // Conditional move if bit 0 clear
        CMOVBS0 = 0x15, // Conditional move if bit 0 set
        CMOVBC1 = 0x37, // Conditional move if bit 1 clear
        CMOVBS1 = 0x35, // Conditional move if bit 1 set
        CMOVBC2 = 0x57, // Conditional move if bit 2 clear
        CMOVBS2 = 0x55, // Conditional move if bit 2 set
        CMOVBC3 = 0x77, // Conditional move if bit 3 clear
        CMOVBS3 = 0x75, // Conditional move if bit 3 set

        // Multi-bit conditional moves
        CMOVBITS = 0x80, // Conditional move based on bit pattern
        CMOVMASK = 0x81, // Conditional move based on mask

        // Signed comparison variants
        CMOVSEQ = 0x84, // Conditional move if signed equal
        CMOVSNE = 0x86, // Conditional move if signed not equal
        CMOVSLT = 0x88, // Conditional move if signed less than
        CMOVSLE = 0x8A, // Conditional move if signed less than or equal
        CMOVSGT = 0x8C, // Conditional move if signed greater than
        CMOVSGE = 0x8E, // Conditional move if signed greater than or equal

        // Unsigned comparison variants
        CMOVUEQ = 0x94, // Conditional move if unsigned equal
        CMOVUNE = 0x96, // Conditional move if unsigned not equal
        CMOVULT = 0x98, // Conditional move if unsigned less than
        CMOVULE = 0x9A, // Conditional move if unsigned less than or equal
        CMOVUGT = 0x9C, // Conditional move if unsigned greater than
        CMOVUGE = 0x9E, // Conditional move if unsigned greater than or equal

        // Range-based conditional moves
        CMOVBND = 0xA0, // Conditional move if within bounds
        CMOVOOB = 0xA2, // Conditional move if out of bounds

        // Parity-based conditional moves
        CMOVPEV = 0xB0, // Conditional move if parity even
        CMOVPOD = 0xB2, // Conditional move if parity odd

        // Immediate conditional moves
        CMOVEQI = 0xC0, // Conditional move if equal to immediate
        CMOVNEI = 0xC2, // Conditional move if not equal to immediate
        CMOVLTI = 0xC4, // Conditional move if less than immediate
        CMOVLEI = 0xC6, // Conditional move if less than or equal to immediate
        CMOVGTI = 0xC8, // Conditional move if greater than immediate
        CMOVGEI = 0xCA, // Conditional move if greater than or equal to immediate

        // Conditional swap operations
        CSWAPEQ = 0xD0, // Conditional swap if equal
        CSWAPNE = 0xD2, // Conditional swap if not equal
        CSWAPLT = 0xD4, // Conditional swap if less than
        CSWAPGT = 0xD6, // Conditional swap if greater than

        // Conditional exchange operations
        CXCHGEQ = 0xE0, // Conditional exchange if equal
        CXCHGNE = 0xE2, // Conditional exchange if not equal

        // Special conditional moves
        CMOVNULL = 0xF0,  // Conditional move if null pointer
        CMOVNNULL = 0xF2, // Conditional move if not null pointer
        CMOVZERO = 0xF4,  // Conditional move if zero (alias for CMOVEQ)
        CMOVNZERO = 0xF6, // Conditional move if non-zero (alias for CMOVNE)

        // Data size variants
        CMOVEQB = 0x124, // Conditional move byte if equal
        CMOVEQW = 0x224, // Conditional move word if equal
        CMOVEQL = 0x324, // Conditional move longword if equal
        CMOVEQQ = 0x424, // Conditional move quadword if equal

        // Unknown operation
        UNKNOWN = 0xFFFF
    };

    enum class ConditionType : uint8_t
    {
        ZERO_TEST = 0,         // Test against zero
        SIGN_TEST = 1,         // Test sign bit
        BIT_TEST = 2,          // Test specific bit
        COMPARE = 3,           // Compare two values
        RANGE_TEST = 4,        // Test within range
        PARITY_TEST = 5,       // Test parity
        IMMEDIATE_COMPARE = 6, // Compare with immediate
        POINTER_TEST = 7       // Test pointer validity
    };

    enum class ComparisonMode : uint8_t
    {
        SIGNED = 0,   // Signed comparison
        UNSIGNED = 1, // Unsigned comparison
        BITWISE = 2   // Bitwise comparison
    };

    enum class DataSize : uint8_t
    {
        BYTE = 1,     // 8-bit operations
        WORD = 2,     // 16-bit operations
        LONGWORD = 4, // 32-bit operations
        QUADWORD = 8  // 64-bit operations
    };

    explicit alphaConditionalMoveInstruction(uint32_t opcode, ConditionalMoveOpType opType, uint8_t destReg,
                                             uint8_t srcReg, uint8_t condReg)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg(srcReg), m_condReg(condReg),
          m_immediate(0), m_useImmediate(false), m_conditionType(determineConditionType(opType)),
          m_comparisonMode(determineComparisonMode(opType)), m_dataSize(determineDataSize(opType)), m_conditionValue(0),
          m_sourceValue(0), m_result(0), m_bitPosition(0), m_mask(0), m_lowerBound(0), m_upperBound(0),
          m_conditionMet(false), m_movePerformedCount(0), m_moveSkippedCount(0), m_zeroTestCount(0), m_signTestCount(0),
          m_bitTestCount(0), m_compareCount(0), m_rangeTestCount(0), m_parityTestCount(0), m_immediateCompareCount(0),
          m_swapOperationCount(0), m_exchangeOperationCount(0), m_nullPointerTestCount(0)
    {
        DEBUG_LOG("alphaConditionalMoveInstruction created - OpType: 0x%04X, Dest: R%d, Src: R%d, Cond: R%d",
                  static_cast<int>(opType), destReg, srcReg, condReg);
    }

    // Immediate mode constructor
    explicit alphaConditionalMoveInstruction(uint32_t opcode, ConditionalMoveOpType opType, uint8_t destReg,
                                             uint8_t srcReg, uint8_t condReg, int16_t immediate)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg(srcReg), m_condReg(condReg),
          m_immediate(immediate), m_useImmediate(true), m_conditionType(determineConditionType(opType)),
          m_comparisonMode(determineComparisonMode(opType)), m_dataSize(determineDataSize(opType)), m_conditionValue(0),
          m_sourceValue(0), m_result(0), m_bitPosition(0), m_mask(0), m_lowerBound(0), m_upperBound(0),
          m_conditionMet(false), m_movePerformedCount(0), m_moveSkippedCount(0), m_zeroTestCount(0), m_signTestCount(0),
          m_bitTestCount(0), m_compareCount(0), m_rangeTestCount(0), m_parityTestCount(0), m_immediateCompareCount(0),
          m_swapOperationCount(0), m_exchangeOperationCount(0), m_nullPointerTestCount(0)
    {
        DEBUG_LOG("alphaConditionalMoveInstruction created (immediate) - OpType: 0x%04X, Dest: R%d, Src: R%d, Cond: "
                  "R%d, Imm: %d",
                  static_cast<int>(opType), destReg, srcReg, condReg, immediate);
    }

    virtual ~alphaConditionalMoveInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performConditionalMoveOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }
    void decode()
    {
        DEBUG_LOG("Decoding conditional move instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        bool isLiteral = (opcode >> 12) & 0x1;
        uint8_t function = (opcode >> 5) & 0x7F;
        uint8_t rc = opcode & 0x1F;

        m_destReg = rc;
        m_srcReg = ra;  // Source value register
        m_condReg = rb; // Condition register

        if (isLiteral)
        {
            m_immediate = static_cast<int16_t>(rb);
            m_useImmediate = true;
        }
        else
        {
            m_useImmediate = false;
        }

        // Determine conditional move operation type
        switch (primaryOpcode)
        {
        case 0x11: // Integer conditional moves
            switch (function)
            {
            case 0x24:
                m_opType = ConditionalMoveOpType::CMOVEQ;
                break;
            case 0x26:
                m_opType = ConditionalMoveOpType::CMOVNE;
                break;
            case 0x44:
                m_opType = ConditionalMoveOpType::CMOVLT;
                break;
            case 0x64:
                m_opType = ConditionalMoveOpType::CMOVLE;
                break;
            case 0x66:
                m_opType = ConditionalMoveOpType::CMOVGT;
                break;
            case 0x46:
                m_opType = ConditionalMoveOpType::CMOVGE;
                break;
            case 0x16:
                m_opType = ConditionalMoveOpType::CMOVLBC;
                break;
            case 0x14:
                m_opType = ConditionalMoveOpType::CMOVLBS;
                break;
            default:
                DEBUG_LOG("Unknown conditional move function: 0x%02X", function);
                m_opType = ConditionalMoveOpType::CMOVEQ;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown conditional move primary opcode: 0x%02X", primaryOpcode);
            m_opType = ConditionalMoveOpType::UNKNOWN;
            break;
        }

        DEBUG_LOG("Conditional move instruction decoded - Type: 0x%04X, Dest: R%d, Src: R%d, Cond: R%d",
                  static_cast<int>(m_opType), m_destReg, m_srcReg, m_condReg);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Basic conditional moves (fast)
        case ConditionalMoveOpType::CMOVEQ:
        case ConditionalMoveOpType::CMOVNE:
        case ConditionalMoveOpType::CMOVLT:
        case ConditionalMoveOpType::CMOVLE:
        case ConditionalMoveOpType::CMOVGT:
        case ConditionalMoveOpType::CMOVGE:
            return 1;

        // Bit test conditional moves
        case ConditionalMoveOpType::CMOVLBC:
        case ConditionalMoveOpType::CMOVLBS:
        case ConditionalMoveOpType::CMOVBC0:
        case ConditionalMoveOpType::CMOVBS0:
        case ConditionalMoveOpType::CMOVBC1:
        case ConditionalMoveOpType::CMOVBS1:
        case ConditionalMoveOpType::CMOVBC2:
        case ConditionalMoveOpType::CMOVBS2:
        case ConditionalMoveOpType::CMOVBC3:
        case ConditionalMoveOpType::CMOVBS3:
            return 1;

        // Multi-bit operations
        case ConditionalMoveOpType::CMOVBITS:
        case ConditionalMoveOpType::CMOVMASK:
            return 2;

        // Signed/unsigned comparison variants
        case ConditionalMoveOpType::CMOVSEQ:
        case ConditionalMoveOpType::CMOVSNE:
        case ConditionalMoveOpType::CMOVSLT:
        case ConditionalMoveOpType::CMOVSLE:
        case ConditionalMoveOpType::CMOVSGT:
        case ConditionalMoveOpType::CMOVSGE:
        case ConditionalMoveOpType::CMOVUEQ:
        case ConditionalMoveOpType::CMOVUNE:
        case ConditionalMoveOpType::CMOVULT:
        case ConditionalMoveOpType::CMOVULE:
        case ConditionalMoveOpType::CMOVUGT:
        case ConditionalMoveOpType::CMOVUGE:
            return 1;

        // Range-based operations
        case ConditionalMoveOpType::CMOVBND:
        case ConditionalMoveOpType::CMOVOOB:
            return 2;

        // Parity operations
        case ConditionalMoveOpType::CMOVPEV:
        case ConditionalMoveOpType::CMOVPOD:
            return 2;

        // Immediate comparisons
        case ConditionalMoveOpType::CMOVEQI:
        case ConditionalMoveOpType::CMOVNEI:
        case ConditionalMoveOpType::CMOVLTI:
        case ConditionalMoveOpType::CMOVLEI:
        case ConditionalMoveOpType::CMOVGTI:
        case ConditionalMoveOpType::CMOVGEI:
            return 1;

        // Conditional swap operations
        case ConditionalMoveOpType::CSWAPEQ:
        case ConditionalMoveOpType::CSWAPNE:
        case ConditionalMoveOpType::CSWAPLT:
        case ConditionalMoveOpType::CSWAPGT:
            return 2;

        // Conditional exchange operations
        case ConditionalMoveOpType::CXCHGEQ:
        case ConditionalMoveOpType::CXCHGNE:
            return 3;

        // Special operations
        case ConditionalMoveOpType::CMOVNULL:
        case ConditionalMoveOpType::CMOVNNULL:
        case ConditionalMoveOpType::CMOVZERO:
        case ConditionalMoveOpType::CMOVNZERO:
            return 1;

        // Data size variants
        case ConditionalMoveOpType::CMOVEQB:
        case ConditionalMoveOpType::CMOVEQW:
        case ConditionalMoveOpType::CMOVEQL:
        case ConditionalMoveOpType::CMOVEQQ:
            return 1;

        default:
            return 1;
        }
    }

    // Performance-critical accessors
    inline ConditionalMoveOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg() const { return m_srcReg; }
    inline uint8_t getCondReg() const { return m_condReg; }
    inline int16_t getImmediate() const { return m_immediate; }
    inline bool usesImmediate() const { return m_useImmediate; }
    inline ConditionType getConditionType() const { return m_conditionType; }
    inline ComparisonMode getComparisonMode() const { return m_comparisonMode; }
    inline DataSize getDataSize() const { return m_dataSize; }
    inline uint64_t getResult() const { return m_result; }
    inline bool wasConditionMet() const { return m_conditionMet; }
    inline uint8_t getBitPosition() const { return m_bitPosition; }
    inline uint64_t getMask() const { return m_mask; }

    // Performance counters
    inline uint64_t getMovePerformedCount() const { return m_movePerformedCount.load(std::memory_order_relaxed); }
    inline uint64_t getMoveSkippedCount() const { return m_moveSkippedCount.load(std::memory_order_relaxed); }
    inline uint64_t getZeroTestCount() const { return m_zeroTestCount.load(std::memory_order_relaxed); }
    inline uint64_t getSignTestCount() const { return m_signTestCount.load(std::memory_order_relaxed); }
    inline uint64_t getBitTestCount() const { return m_bitTestCount.load(std::memory_order_relaxed); }
    inline uint64_t getCompareCount() const { return m_compareCount.load(std::memory_order_relaxed); }
    inline uint64_t getRangeTestCount() const { return m_rangeTestCount.load(std::memory_order_relaxed); }
    inline uint64_t getParityTestCount() const { return m_parityTestCount.load(std::memory_order_relaxed); }
    inline uint64_t getImmediateCompareCount() const { return m_immediateCompareCount.load(std::memory_order_relaxed); }
    inline uint64_t getSwapOperationCount() const { return m_swapOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getExchangeOperationCount() const
    {
        return m_exchangeOperationCount.load(std::memory_order_relaxed);
    }
    inline uint64_t getNullPointerTestCount() const { return m_nullPointerTestCount.load(std::memory_order_relaxed); }

    // Statistical analysis
    inline double getMovePerformanceRate() const
    {
        uint64_t total = getMovePerformedCount() + getMoveSkippedCount();
        return total > 0 ? static_cast<double>(getMovePerformedCount()) / total : 0.0;
    }

    // Operation classification
    inline bool isBasicConditionalMove() const
    {
        return (m_opType >= ConditionalMoveOpType::CMOVEQ && m_opType <= ConditionalMoveOpType::CMOVGE);
    }

    inline bool isBitTestOperation() const
    {
        return (m_opType >= ConditionalMoveOpType::CMOVLBC && m_opType <= ConditionalMoveOpType::CMOVBS3);
    }

    inline bool isComparisonOperation() const
    {
        return (m_opType >= ConditionalMoveOpType::CMOVSEQ && m_opType <= ConditionalMoveOpType::CMOVUGE);
    }

    inline bool isSwapOperation() const
    {
        return (m_opType >= ConditionalMoveOpType::CSWAPEQ && m_opType <= ConditionalMoveOpType::CSWAPGT);
    }

    inline bool isExchangeOperation() const
    {
        return (m_opType >= ConditionalMoveOpType::CXCHGEQ && m_opType <= ConditionalMoveOpType::CXCHGNE);
    }

    // Hot path execution support
    inline void setConditionValue(uint64_t value) { m_conditionValue = value; }
    inline void setSourceValue(uint64_t value) { m_sourceValue = value; }
    inline void setBitPosition(uint8_t pos) { m_bitPosition = pos; }
    inline void setMask(uint64_t mask) { m_mask = mask; }
    inline void setBounds(uint64_t lower, uint64_t upper)
    {
        m_lowerBound = lower;
        m_upperBound = upper;
    }

  private:
    ConditionType determineConditionType(ConditionalMoveOpType opType) const
    {
        switch (opType)
        {
        case ConditionalMoveOpType::CMOVEQ:
        case ConditionalMoveOpType::CMOVNE:
        case ConditionalMoveOpType::CMOVZERO:
        case ConditionalMoveOpType::CMOVNZERO:
            return ConditionType::ZERO_TEST;

        case ConditionalMoveOpType::CMOVLT:
        case ConditionalMoveOpType::CMOVLE:
        case ConditionalMoveOpType::CMOVGT:
        case ConditionalMoveOpType::CMOVGE:
            return ConditionType::SIGN_TEST;

        case ConditionalMoveOpType::CMOVLBC:
        case ConditionalMoveOpType::CMOVLBS:
        case ConditionalMoveOpType::CMOVBC0:
        case ConditionalMoveOpType::CMOVBS0:
        case ConditionalMoveOpType::CMOVBC1:
        case ConditionalMoveOpType::CMOVBS1:
        case ConditionalMoveOpType::CMOVBC2:
        case ConditionalMoveOpType::CMOVBS2:
        case ConditionalMoveOpType::CMOVBC3:
        case ConditionalMoveOpType::CMOVBS3:
        case ConditionalMoveOpType::CMOVBITS:
        case ConditionalMoveOpType::CMOVMASK:
            return ConditionType::BIT_TEST;

        case ConditionalMoveOpType::CMOVSEQ:
        case ConditionalMoveOpType::CMOVSNE:
        case ConditionalMoveOpType::CMOVSLT:
        case ConditionalMoveOpType::CMOVSLE:
        case ConditionalMoveOpType::CMOVSGT:
        case ConditionalMoveOpType::CMOVSGE:
        case ConditionalMoveOpType::CMOVUEQ:
        case ConditionalMoveOpType::CMOVUNE:
        case ConditionalMoveOpType::CMOVULT:
        case ConditionalMoveOpType::CMOVULE:
        case ConditionalMoveOpType::CMOVUGT:
        case ConditionalMoveOpType::CMOVUGE:
            return ConditionType::COMPARE;

        case ConditionalMoveOpType::CMOVBND:
        case ConditionalMoveOpType::CMOVOOB:
            return ConditionType::RANGE_TEST;

        case ConditionalMoveOpType::CMOVPEV:
        case ConditionalMoveOpType::CMOVPOD:
            return ConditionType::PARITY_TEST;

        case ConditionalMoveOpType::CMOVEQI:
        case ConditionalMoveOpType::CMOVNEI:
        case ConditionalMoveOpType::CMOVLTI:
        case ConditionalMoveOpType::CMOVLEI:
        case ConditionalMoveOpType::CMOVGTI:
        case ConditionalMoveOpType::CMOVGEI:
            return ConditionType::IMMEDIATE_COMPARE;

        case ConditionalMoveOpType::CMOVNULL:
        case ConditionalMoveOpType::CMOVNNULL:
            return ConditionType::POINTER_TEST;

        default:
            return ConditionType::ZERO_TEST;
        }
    }

    ComparisonMode determineComparisonMode(ConditionalMoveOpType opType) const
    {
        switch (opType)
        {
        case ConditionalMoveOpType::CMOVSEQ:
        case ConditionalMoveOpType::CMOVSNE:
        case ConditionalMoveOpType::CMOVSLT:
        case ConditionalMoveOpType::CMOVSLE:
        case ConditionalMoveOpType::CMOVSGT:
        case ConditionalMoveOpType::CMOVSGE:
            return ComparisonMode::SIGNED;

        case ConditionalMoveOpType::CMOVUEQ:
        case ConditionalMoveOpType::CMOVUNE:
        case ConditionalMoveOpType::CMOVULT:
        case ConditionalMoveOpType::CMOVULE:
        case ConditionalMoveOpType::CMOVUGT:
        case ConditionalMoveOpType::CMOVUGE:
            return ComparisonMode::UNSIGNED;

        case ConditionalMoveOpType::CMOVLBC:
        case ConditionalMoveOpType::CMOVLBS:
        case ConditionalMoveOpType::CMOVBITS:
        case ConditionalMoveOpType::CMOVMASK:
            return ComparisonMode::BITWISE;

        default:
            return ComparisonMode::SIGNED;
        }
    }

    DataSize determineDataSize(ConditionalMoveOpType opType) const
    {
        switch (opType)
        {
        case ConditionalMoveOpType::CMOVEQB:
            return DataSize::BYTE;
        case ConditionalMoveOpType::CMOVEQW:
            return DataSize::WORD;
        case ConditionalMoveOpType::CMOVEQL:
            return DataSize::LONGWORD;
        case ConditionalMoveOpType::CMOVEQQ:
            return DataSize::QUADWORD;
        default:
            return DataSize::QUADWORD;
        }
    }

    bool performConditionalMoveOperation()
    {
        // Evaluate the condition first
        m_conditionMet = evaluateCondition();

        if (m_conditionMet)
        {
            m_movePerformedCount.fetch_add(1, std::memory_order_relaxed);
            return performMoveOperation();
        }
        else
        {
            m_moveSkippedCount.fetch_add(1, std::memory_order_relaxed);
            // Keep the original destination value
            m_result = m_conditionValue; // Original destination value
            return true;
        }
    }

    bool evaluateCondition()
    {
        switch (m_conditionType)
        {
        case ConditionType::ZERO_TEST:
            return evaluateZeroTest();
        case ConditionType::SIGN_TEST:
            return evaluateSignTest();
        case ConditionType::BIT_TEST:
            return evaluateBitTest();
        case ConditionType::COMPARE:
            return evaluateComparison();
        case ConditionType::RANGE_TEST:
            return evaluateRangeTest();
        case ConditionType::PARITY_TEST:
            return evaluateParityTest();
        case ConditionType::IMMEDIATE_COMPARE:
            return evaluateImmediateComparison();
        case ConditionType::POINTER_TEST:
            return evaluatePointerTest();
        default:
            return false;
        }
    }

    bool evaluateZeroTest()
    {
        m_zeroTestCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVEQ:
        case ConditionalMoveOpType::CMOVZERO:
            return (m_conditionValue == 0);
        case ConditionalMoveOpType::CMOVNE:
        case ConditionalMoveOpType::CMOVNZERO:
            return (m_conditionValue != 0);
        default:
            return false;
        }
    }

    bool evaluateSignTest()
    {
        m_signTestCount.fetch_add(1, std::memory_order_relaxed);

        int64_t signedValue = static_cast<int64_t>(m_conditionValue);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVLT:
            return (signedValue < 0);
        case ConditionalMoveOpType::CMOVLE:
            return (signedValue <= 0);
        case ConditionalMoveOpType::CMOVGT:
            return (signedValue > 0);
        case ConditionalMoveOpType::CMOVGE:
            return (signedValue >= 0);
        default:
            return false;
        }
    }

    bool evaluateBitTest()
    {
        m_bitTestCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVLBC:
        case ConditionalMoveOpType::CMOVBC0:
            return ((m_conditionValue & 1) == 0);
        case ConditionalMoveOpType::CMOVLBS:
        case ConditionalMoveOpType::CMOVBS0:
            return ((m_conditionValue & 1) != 0);
        case ConditionalMoveOpType::CMOVBC1:
            return ((m_conditionValue & 2) == 0);
        case ConditionalMoveOpType::CMOVBS1:
            return ((m_conditionValue & 2) != 0);
        case ConditionalMoveOpType::CMOVBC2:
            return ((m_conditionValue & 4) == 0);
        case ConditionalMoveOpType::CMOVBS2:
            return ((m_conditionValue & 4) != 0);
        case ConditionalMoveOpType::CMOVBC3:
            return ((m_conditionValue & 8) == 0);
        case ConditionalMoveOpType::CMOVBS3:
            return ((m_conditionValue & 8) != 0);
        case ConditionalMoveOpType::CMOVBITS:
            return ((m_conditionValue & m_mask) == m_mask);
        case ConditionalMoveOpType::CMOVMASK:
            return ((m_conditionValue & m_mask) != 0);
        default:
            return false;
        }
    }

    bool evaluateComparison()
    {
        m_compareCount.fetch_add(1, std::memory_order_relaxed);

        if (m_comparisonMode == ComparisonMode::SIGNED)
        {
            int64_t val1 = static_cast<int64_t>(m_conditionValue);
            int64_t val2 = static_cast<int64_t>(m_sourceValue);

            switch (m_opType)
            {
            case ConditionalMoveOpType::CMOVSEQ:
                return (val1 == val2);
            case ConditionalMoveOpType::CMOVSNE:
                return (val1 != val2);
            case ConditionalMoveOpType::CMOVSLT:
                return (val1 < val2);
            case ConditionalMoveOpType::CMOVSLE:
                return (val1 <= val2);
            case ConditionalMoveOpType::CMOVSGT:
                return (val1 > val2);
            case ConditionalMoveOpType::CMOVSGE:
                return (val1 >= val2);
            default:
                return false;
            }
        }
        else if (m_comparisonMode == ComparisonMode::UNSIGNED)
        {
            uint64_t val1 = m_conditionValue;
            uint64_t val2 = m_sourceValue;

            switch (m_opType)
            {
            case ConditionalMoveOpType::CMOVUEQ:
                return (val1 == val2);
            case ConditionalMoveOpType::CMOVUNE:
                return (val1 != val2);
            case ConditionalMoveOpType::CMOVULT:
                return (val1 < val2);
            case ConditionalMoveOpType::CMOVULE:
                return (val1 <= val2);
            case ConditionalMoveOpType::CMOVUGT:
                return (val1 > val2);
            case ConditionalMoveOpType::CMOVUGE:
                return (val1 >= val2);
            default:
                return false;
            }
        }

        return false;
    }

    bool evaluateRangeTest()
    {
        m_rangeTestCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVBND:
            return (m_conditionValue >= m_lowerBound && m_conditionValue <= m_upperBound);
        case ConditionalMoveOpType::CMOVOOB:
            return (m_conditionValue < m_lowerBound || m_conditionValue > m_upperBound);
        default:
            return false;
        }
    }

    bool evaluateParityTest()
    {
        m_parityTestCount.fetch_add(1, std::memory_order_relaxed);

        // Count number of set bits
        uint64_t value = m_conditionValue;
        int popcount = __builtin_popcountll(value);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVPEV:
            return ((popcount & 1) == 0); // Even parity
        case ConditionalMoveOpType::CMOVPOD:
            return ((popcount & 1) == 1); // Odd parity
        default:
            return false;
        }
    }

    bool evaluateImmediateComparison()
    {
        m_immediateCompareCount.fetch_add(1, std::memory_order_relaxed);

        int64_t signedValue = static_cast<int64_t>(m_conditionValue);
        int64_t signedImmediate = static_cast<int64_t>(m_immediate);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVEQI:
            return (signedValue == signedImmediate);
        case ConditionalMoveOpType::CMOVNEI:
            return (signedValue != signedImmediate);
        case ConditionalMoveOpType::CMOVLTI:
            return (signedValue < signedImmediate);
        case ConditionalMoveOpType::CMOVLEI:
            return (signedValue <= signedImmediate);
        case ConditionalMoveOpType::CMOVGTI:
            return (signedValue > signedImmediate);
        case ConditionalMoveOpType::CMOVGEI:
            return (signedValue >= signedImmediate);
        default:
            return false;
        }
    }

    bool evaluatePointerTest()
    {
        m_nullPointerTestCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case ConditionalMoveOpType::CMOVNULL:
            return (m_conditionValue == 0);
        case ConditionalMoveOpType::CMOVNNULL:
            return (m_conditionValue != 0);
        default:
            return false;
        }
    }

    bool performMoveOperation()
    {
        switch (m_dataSize)
        {
        case DataSize::BYTE:
            m_result = m_sourceValue & 0xFF;
            break;
        case DataSize::WORD:
            m_result = m_sourceValue & 0xFFFF;
            break;
        case DataSize::LONGWORD:
            m_result = m_sourceValue & 0xFFFFFFFF;
            break;
        case DataSize::QUADWORD:
            m_result = m_sourceValue;
            break;
        }

        // Handle special operations
        if (isSwapOperation())
        {
            return performSwapOperation();
        }
        else if (isExchangeOperation())
        {
            return performExchangeOperation();
        }

        DEBUG_LOG("Conditional move performed: condition=true, src=0x%016llX -> dest=0x%016llX", m_sourceValue,
                  m_result);
        return true;
    }

    bool performSwapOperation()
    {
        m_swapOperationCount.fetch_add(1, std::memory_order_relaxed);

        // Swap source and condition values
        uint64_t temp = m_sourceValue;
        m_sourceValue = m_conditionValue;
        m_result = temp;

        DEBUG_LOG("Conditional swap performed: swapped 0x%016llX <-> 0x%016llX", m_conditionValue, temp);
        return true;
    }

    bool performExchangeOperation()
    {
        m_exchangeOperationCount.fetch_add(1, std::memory_order_relaxed);

        // Exchange operation (atomic-like behavior)
        m_result = m_conditionValue;
        // In real implementation, would update the condition register with source value

        DEBUG_LOG("Conditional exchange performed: exchanged 0x%016llX -> 0x%016llX", m_conditionValue, m_sourceValue);
        return true;
    }

  private:
     ConditionalMoveOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg;
     uint8_t m_condReg;
     int16_t m_immediate;
     bool m_useImmediate;
     ConditionType m_conditionType;
     ComparisonMode m_comparisonMode;
     DataSize m_dataSize;

    uint64_t m_conditionValue;
    uint64_t m_sourceValue;
    uint64_t m_result;
    uint8_t m_bitPosition;
    uint64_t m_mask;
    uint64_t m_lowerBound;
    uint64_t m_upperBound;
    bool m_conditionMet;

    std::atomic<uint64_t> m_movePerformedCount;
    std::atomic<uint64_t> m_moveSkippedCount;
    std::atomic<uint64_t> m_zeroTestCount;
    std::atomic<uint64_t> m_signTestCount;
    std::atomic<uint64_t> m_bitTestCount;
    std::atomic<uint64_t> m_compareCount;
    std::atomic<uint64_t> m_rangeTestCount;
    std::atomic<uint64_t> m_parityTestCount;
    std::atomic<uint64_t> m_immediateCompareCount;
    std::atomic<uint64_t> m_swapOperationCount;
    std::atomic<uint64_t> m_exchangeOperationCount;
    std::atomic<uint64_t> m_nullPointerTestCount;

    // Prevent copying for performance
    alphaConditionalMoveInstruction(const alphaConditionalMoveInstruction &) = delete;
    alphaConditionalMoveInstruction &operator=(const alphaConditionalMoveInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaMemoryOrderingInstruction : public alphaInstructionBase
{
  public:
    enum class MemoryOrderingOpType : uint16_t
    {
        // Memory barriers
        EXCB = 0x004,  // Exception barrier
        TRAPB = 0x000, // Trap barrier
        MB = 0x4000,   // Memory barrier
        WMB = 0x4400,  // Write memory barrier
        RMB = 0x4200,  // Read memory barrier
        IMB = 0x0086,  // Instruction memory barrier

        // Cache operations
        FETCH = 0xF800,   // Prefetch for read
        FETCH_M = 0xF900, // Prefetch for modify
        ECB = 0xE800,     // Evict cache block
        WH64 = 0xF600,    // Write hint 64 bytes
        WH64EN = 0xF601,  // Write hint 64 enable

        // Lock flag operations
        RS = 0xF000, // Read and set lock flag
        RC = 0xF001, // Read and clear lock flag

        // Load-locked/Store-conditional operations
        LDL_L = 0x2A, // Load locked longword
        LDQ_L = 0x2B, // Load locked quadword
        STL_C = 0x2E, // Store conditional longword
        STQ_C = 0x2F, // Store conditional quadword

        // Advanced memory fences
        MEMFENCE = 0x4800, // Full memory fence
        SFENCE = 0x4C00,   // Store fence
        LFENCE = 0x4A00,   // Load fence
        MFENCE = 0x4E00,   // Memory and I/O fence

        // Cache coherency operations
        FLUSH = 0x5000,   // Cache flush
        FLUSHI = 0x5001,  // Cache flush and invalidate
        INVAL = 0x5100,   // Cache invalidate
        WBACK = 0x5200,   // Cache writeback
        WBINVAL = 0x5300, // Cache writeback and invalidate

        // Atomic read-modify-write operations
        CAS = 0x6000,  // Compare and swap
        CAS8 = 0x6001, // Compare and swap 8-byte
        CAS4 = 0x6002, // Compare and swap 4-byte
        CAS2 = 0x6003, // Compare and swap 2-byte
        CAS1 = 0x6004, // Compare and swap 1-byte

        XCHG = 0x6100,  // Exchange
        XCHG8 = 0x6101, // Exchange 8-byte
        XCHG4 = 0x6102, // Exchange 4-byte
        XCHG2 = 0x6103, // Exchange 2-byte
        XCHG1 = 0x6104, // Exchange 1-byte

        FETCHADD = 0x6200,  // Fetch and add
        FETCHADD8 = 0x6201, // Fetch and add 8-byte
        FETCHADD4 = 0x6202, // Fetch and add 4-byte
        FETCHADD2 = 0x6203, // Fetch and add 2-byte
        FETCHADD1 = 0x6204, // Fetch and add 1-byte

        FETCHAND = 0x6300,  // Fetch and AND
        FETCHOR = 0x6400,   // Fetch and OR
        FETCHXOR = 0x6500,  // Fetch and XOR
        FETCHNAND = 0x6600, // Fetch and NAND

        // Memory ordering semantics
        ACQUIRE = 0x7000, // Acquire semantics
        RELEASE = 0x7100, // Release semantics
        ACQREL = 0x7200,  // Acquire-release semantics
        SEQCST = 0x7300,  // Sequential consistency

        // TLB operations
        TLBFLUSH = 0x8000, // TLB flush
        TLBINVAL = 0x8100, // TLB invalidate
        TLBSYNC = 0x8200,  // TLB synchronize

        // DMA coherency operations
        DMAFLUSH = 0x9000, // DMA cache flush
        DMAINVAL = 0x9100, // DMA cache invalidate
        DMASYNC = 0x9200,  // DMA synchronize

        // Performance monitoring barriers
        PMFENCE = 0xA000, // Performance monitor fence
        PMFLUSH = 0xA100, // Performance monitor flush

        // Debug and trace barriers
        DBGFENCE = 0xB000, // Debug fence
        TRCFENCE = 0xB100, // Trace fence

        // Unknown operation
        UNKNOWN = 0xFFFF
    };

    enum class MemoryOrdering : uint8_t
    {
        RELAXED = 0, // No ordering constraints
        ACQUIRE = 1, // Acquire semantics
        RELEASE = 2, // Release semantics
        ACQ_REL = 3, // Acquire-release semantics
        SEQ_CST = 4  // Sequential consistency
    };

    enum class CacheScope : uint8_t
    {
        LOCAL = 0,           // Local cache only
        SHARED = 1,          // Shared cache
        SYSTEM = 2,          // System-wide
        COHERENCY_DOMAIN = 3 // Full coherency domain
    };

    enum class BarrierType : uint8_t
    {
        LOAD_LOAD = 0,   // Load-load barrier
        LOAD_STORE = 1,  // Load-store barrier
        STORE_LOAD = 2,  // Store-load barrier
        STORE_STORE = 3, // Store-store barrier
        FULL = 4         // Full barrier
    };

    enum class AtomicOperation : uint8_t
    {
        COMPARE_SWAP = 0, // Compare and swap
        EXCHANGE = 1,     // Exchange
        FETCH_ADD = 2,    // Fetch and add
        FETCH_AND = 3,    // Fetch and AND
        FETCH_OR = 4,     // Fetch and OR
        FETCH_XOR = 5,    // Fetch and XOR
        FETCH_NAND = 6    // Fetch and NAND
    };

    explicit alphaMemoryOrderingInstruction(uint32_t opcode, MemoryOrderingOpType opType)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(0), m_srcReg(0), m_addrReg(0),
          m_memoryOrdering(determineMemoryOrdering(opType)), m_cacheScope(CacheScope::SYSTEM),
          m_barrierType(determineBarrierType(opType)), m_atomicOperation(determineAtomicOperation(opType)),
          m_address(0), m_value(0), m_compareValue(0), m_result(0), m_accessSize(8), m_cacheLineSize(64),
          m_success(false), m_memoryBarrierCount(0), m_cacheOperationCount(0), m_lockOperationCount(0),
          m_atomicOperationCount(0), m_tlbOperationCount(0), m_prefetchCount(0), m_flushCount(0), m_invalidateCount(0),
          m_fenceCount(0), m_loadLockCount(0), m_storeConditionalCount(0), m_atomicSuccessCount(0),
          m_atomicFailureCount(0)
    {
        DEBUG_LOG("alphaMemoryOrderingInstruction created - OpType: 0x%04X", static_cast<int>(opType));
    }

    // Constructor with registers
    explicit alphaMemoryOrderingInstruction(uint32_t opcode, MemoryOrderingOpType opType, uint8_t destReg,
                                            uint8_t srcReg, uint8_t addrReg)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg(srcReg), m_addrReg(addrReg),
          m_memoryOrdering(determineMemoryOrdering(opType)), m_cacheScope(CacheScope::SYSTEM),
          m_barrierType(determineBarrierType(opType)), m_atomicOperation(determineAtomicOperation(opType)),
          m_address(0), m_value(0), m_compareValue(0), m_result(0), m_accessSize(determineAccessSize(opType)),
          m_cacheLineSize(64), m_success(false), m_memoryBarrierCount(0), m_cacheOperationCount(0),
          m_lockOperationCount(0), m_atomicOperationCount(0), m_tlbOperationCount(0), m_prefetchCount(0),
          m_flushCount(0), m_invalidateCount(0), m_fenceCount(0), m_loadLockCount(0), m_storeConditionalCount(0),
          m_atomicSuccessCount(0), m_atomicFailureCount(0)
    {
        DEBUG_LOG("alphaMemoryOrderingInstruction created - OpType: 0x%04X, Dest: R%d, Src: R%d, Addr: R%d",
                  static_cast<int>(opType), destReg, srcReg, addrReg);
    }

    virtual ~alphaMemoryOrderingInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performMemoryOrderingOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding memory ordering instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        uint16_t function = opcode & 0xFFFF;

        m_destReg = ra;
        m_srcReg = rb;
        m_addrReg = rb; // Base address register

        // Determine memory ordering operation type
        switch (primaryOpcode)
        {
        case 0x18: // Memory format instructions with function codes
            switch (function)
            {
            case 0x4000:
                m_opType = MemoryOrderingOpType::MB;
                break;
            case 0x4400:
                m_opType = MemoryOrderingOpType::WMB;
                break;
            case 0x4200:
                m_opType = MemoryOrderingOpType::RMB;
                break;
            case 0xF000:
                m_opType = MemoryOrderingOpType::RS;
                break;
            case 0xF001:
                m_opType = MemoryOrderingOpType::RC;
                break;
            case 0xF800:
                m_opType = MemoryOrderingOpType::FETCH;
                break;
            case 0xF900:
                m_opType = MemoryOrderingOpType::FETCH_M;
                break;
            case 0xE800:
                m_opType = MemoryOrderingOpType::ECB;
                break;
            default:
                DEBUG_LOG("Unknown memory ordering function: 0x%04X", function);
                m_opType = MemoryOrderingOpType::MB;
                break;
            }
            break;

        case 0x2A:
            m_opType = MemoryOrderingOpType::LDL_L;
            break;
        case 0x2B:
            m_opType = MemoryOrderingOpType::LDQ_L;
            break;
        case 0x2E:
            m_opType = MemoryOrderingOpType::STL_C;
            break;
        case 0x2F:
            m_opType = MemoryOrderingOpType::STQ_C;
            break;

        case 0x00: // CALL_PAL with specific functions
            switch (function & 0xFF)
            {
            case 0x86:
                m_opType = MemoryOrderingOpType::IMB;
                break;
            case 0x004:
                m_opType = MemoryOrderingOpType::EXCB;
                break;
            case 0x000:
                m_opType = MemoryOrderingOpType::TRAPB;
                break;
            default:
                DEBUG_LOG("Unknown PAL memory function: 0x%02X", function & 0xFF);
                m_opType = MemoryOrderingOpType::MB;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown memory ordering primary opcode: 0x%02X", primaryOpcode);
            m_opType = MemoryOrderingOpType::UNKNOWN;
            break;
        }

        // Set access size based on operation
        switch (m_opType)
        {
        case MemoryOrderingOpType::LDL_L:
        case MemoryOrderingOpType::STL_C:
            m_accessSize = 4;
            break;
        case MemoryOrderingOpType::LDQ_L:
        case MemoryOrderingOpType::STQ_C:
            m_accessSize = 8;
            break;
        default:
            m_accessSize = 0; // Not applicable for barriers
            break;
        }

        DEBUG_LOG("Memory ordering instruction decoded - Type: 0x%04X, Size: %d", static_cast<int>(m_opType),
                  m_accessSize);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // Fast barriers
        case MemoryOrderingOpType::EXCB:
        case MemoryOrderingOpType::TRAPB:
            return 5;

        // Memory barriers
        case MemoryOrderingOpType::MB:
        case MemoryOrderingOpType::WMB:
        case MemoryOrderingOpType::RMB:
            return 10;
        case MemoryOrderingOpType::IMB:
            return 50;

        // Cache operations
        case MemoryOrderingOpType::FETCH:
        case MemoryOrderingOpType::FETCH_M:
            return 1; // Async operation
        case MemoryOrderingOpType::ECB:
            return 5;
        case MemoryOrderingOpType::WH64:
        case MemoryOrderingOpType::WH64EN:
            return 2;

        // Lock operations
        case MemoryOrderingOpType::RS:
        case MemoryOrderingOpType::RC:
            return 3;

        // Load-locked/Store-conditional
        case MemoryOrderingOpType::LDL_L:
        case MemoryOrderingOpType::LDQ_L:
            return 3;
        case MemoryOrderingOpType::STL_C:
        case MemoryOrderingOpType::STQ_C:
            return 5;

        // Advanced fences
        case MemoryOrderingOpType::MEMFENCE:
        case MemoryOrderingOpType::SFENCE:
        case MemoryOrderingOpType::LFENCE:
            return 8;
        case MemoryOrderingOpType::MFENCE:
            return 15;

        // Cache coherency operations
        case MemoryOrderingOpType::FLUSH:
        case MemoryOrderingOpType::FLUSHI:
            return 20;
        case MemoryOrderingOpType::INVAL:
            return 10;
        case MemoryOrderingOpType::WBACK:
        case MemoryOrderingOpType::WBINVAL:
            return 25;

        // Atomic operations
        case MemoryOrderingOpType::CAS:
        case MemoryOrderingOpType::CAS8:
        case MemoryOrderingOpType::CAS4:
        case MemoryOrderingOpType::CAS2:
        case MemoryOrderingOpType::CAS1:
            return 8;
        case MemoryOrderingOpType::XCHG:
        case MemoryOrderingOpType::XCHG8:
        case MemoryOrderingOpType::XCHG4:
        case MemoryOrderingOpType::XCHG2:
        case MemoryOrderingOpType::XCHG1:
            return 6;
        case MemoryOrderingOpType::FETCHADD:
        case MemoryOrderingOpType::FETCHADD8:
        case MemoryOrderingOpType::FETCHADD4:
        case MemoryOrderingOpType::FETCHADD2:
        case MemoryOrderingOpType::FETCHADD1:
        case MemoryOrderingOpType::FETCHAND:
        case MemoryOrderingOpType::FETCHOR:
        case MemoryOrderingOpType::FETCHXOR:
        case MemoryOrderingOpType::FETCHNAND:
            return 7;

        // Memory ordering semantics
        case MemoryOrderingOpType::ACQUIRE:
        case MemoryOrderingOpType::RELEASE:
            return 3;
        case MemoryOrderingOpType::ACQREL:
        case MemoryOrderingOpType::SEQCST:
            return 5;

        // TLB operations
        case MemoryOrderingOpType::TLBFLUSH:
        case MemoryOrderingOpType::TLBINVAL:
            return 30;
        case MemoryOrderingOpType::TLBSYNC:
            return 40;

        // DMA operations
        case MemoryOrderingOpType::DMAFLUSH:
        case MemoryOrderingOpType::DMAINVAL:
            return 50;
        case MemoryOrderingOpType::DMASYNC:
            return 100;

        // Performance monitoring
        case MemoryOrderingOpType::PMFENCE:
        case MemoryOrderingOpType::PMFLUSH:
            return 10;

        // Debug operations
        case MemoryOrderingOpType::DBGFENCE:
        case MemoryOrderingOpType::TRCFENCE:
            return 15;

        default:
            return 10;
        }
    }

    // Memory operation classification
    bool isMemoryOperation() const override
    {
        return (m_opType >= MemoryOrderingOpType::LDL_L && m_opType <= MemoryOrderingOpType::STQ_C) ||
               isAtomicOperation();
    }

    // Performance-critical accessors
    inline MemoryOrderingOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg() const { return m_srcReg; }
    inline uint8_t getAddrReg() const { return m_addrReg; }
    inline MemoryOrdering getMemoryOrdering() const { return m_memoryOrdering; }
    inline CacheScope getCacheScope() const { return m_cacheScope; }
    inline BarrierType getBarrierType() const { return m_barrierType; }
    inline AtomicOperation getAtomicOperation() const { return m_atomicOperation; }
    inline uint64_t getAddress() const { return m_address; }
    inline uint64_t getValue() const { return m_value; }
    inline uint64_t getCompareValue() const { return m_compareValue; }
    inline uint64_t getResult() const { return m_result; }
    inline uint32_t getAccessSize() const { return m_accessSize; }
    inline uint32_t getCacheLineSize() const { return m_cacheLineSize; }
    inline bool wasSuccessful() const { return m_success; }

    // Performance counters
    inline uint64_t getMemoryBarrierCount() const { return m_memoryBarrierCount.load(std::memory_order_relaxed); }
    inline uint64_t getCacheOperationCount() const { return m_cacheOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getLockOperationCount() const { return m_lockOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getAtomicOperationCount() const { return m_atomicOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getTlbOperationCount() const { return m_tlbOperationCount.load(std::memory_order_relaxed); }
    inline uint64_t getPrefetchCount() const { return m_prefetchCount.load(std::memory_order_relaxed); }
    inline uint64_t getFlushCount() const { return m_flushCount.load(std::memory_order_relaxed); }
    inline uint64_t getInvalidateCount() const { return m_invalidateCount.load(std::memory_order_relaxed); }
    inline uint64_t getFenceCount() const { return m_fenceCount.load(std::memory_order_relaxed); }
    inline uint64_t getLoadLockCount() const { return m_loadLockCount.load(std::memory_order_relaxed); }
    inline uint64_t getStoreConditionalCount() const { return m_storeConditionalCount.load(std::memory_order_relaxed); }
    inline uint64_t getAtomicSuccessCount() const { return m_atomicSuccessCount.load(std::memory_order_relaxed); }
    inline uint64_t getAtomicFailureCount() const { return m_atomicFailureCount.load(std::memory_order_relaxed); }

    // Statistical analysis
    inline double getAtomicSuccessRate() const
    {
        uint64_t total = getAtomicSuccessCount() + getAtomicFailureCount();
        return total > 0 ? static_cast<double>(getAtomicSuccessCount()) / total : 0.0;
    }

    inline double getStoreConditionalSuccessRate() const
    {
        uint64_t total = getLoadLockCount();
        uint64_t successful = getStoreConditionalCount();
        return total > 0 ? static_cast<double>(successful) / total : 0.0;
    }

    // Operation classification
    inline bool isMemoryBarrier() const
    {
        return (m_opType >= MemoryOrderingOpType::EXCB && m_opType <= MemoryOrderingOpType::IMB) ||
               (m_opType >= MemoryOrderingOpType::MEMFENCE && m_opType <= MemoryOrderingOpType::MFENCE);
    }

    inline bool isCacheOperation() const
    {
        return (m_opType >= MemoryOrderingOpType::FETCH && m_opType <= MemoryOrderingOpType::WH64EN) ||
               (m_opType >= MemoryOrderingOpType::FLUSH && m_opType <= MemoryOrderingOpType::WBINVAL);
    }

    inline bool isLockOperation() const
    {
        return (m_opType >= MemoryOrderingOpType::RS && m_opType <= MemoryOrderingOpType::RC);
    }

    inline bool isLoadLockStoreConditional() const
    {
        return (m_opType >= MemoryOrderingOpType::LDL_L && m_opType <= MemoryOrderingOpType::STQ_C);
    }

    inline bool isAtomicOperation() const
    {
        return (m_opType >= MemoryOrderingOpType::CAS && m_opType <= MemoryOrderingOpType::FETCHNAND);
    }

    inline bool isTLBOperation() const
    {
        return (m_opType >= MemoryOrderingOpType::TLBFLUSH && m_opType <= MemoryOrderingOpType::TLBSYNC);
    }

    // Hot path execution support
    inline void setAddress(uint64_t addr) { m_address = addr; }
    inline void setValue(uint64_t val) { m_value = val; }
    inline void setCompareValue(uint64_t val) { m_compareValue = val; }
    inline void setCacheScope(CacheScope scope) { m_cacheScope = scope; }
    inline void setAccessSize(uint32_t size) { m_accessSize = size; }

  private:
    MemoryOrdering determineMemoryOrdering(MemoryOrderingOpType opType) const
    {
        switch (opType)
        {
        case MemoryOrderingOpType::ACQUIRE:
            return MemoryOrdering::ACQUIRE;
        case MemoryOrderingOpType::RELEASE:
            return MemoryOrdering::RELEASE;
        case MemoryOrderingOpType::ACQREL:
            return MemoryOrdering::ACQ_REL;
        case MemoryOrderingOpType::SEQCST:
        case MemoryOrderingOpType::MB:
        case MemoryOrderingOpType::MEMFENCE:
        case MemoryOrderingOpType::MFENCE:
            return MemoryOrdering::SEQ_CST;
        default:
            return MemoryOrdering::RELAXED;
        }
    }

    BarrierType determineBarrierType(MemoryOrderingOpType opType) const
    {
        switch (opType)
        {
        case MemoryOrderingOpType::RMB:
        case MemoryOrderingOpType::LFENCE:
            return BarrierType::LOAD_LOAD;
        case MemoryOrderingOpType::WMB:
        case MemoryOrderingOpType::SFENCE:
            return BarrierType::STORE_STORE;
        case MemoryOrderingOpType::MB:
        case MemoryOrderingOpType::MEMFENCE:
        case MemoryOrderingOpType::MFENCE:
            return BarrierType::FULL;
        default:
            return BarrierType::FULL;
        }
    }

    AtomicOperation determineAtomicOperation(MemoryOrderingOpType opType) const
    {
        switch (opType)
        {
        case MemoryOrderingOpType::CAS:
        case MemoryOrderingOpType::CAS8:
        case MemoryOrderingOpType::CAS4:
        case MemoryOrderingOpType::CAS2:
        case MemoryOrderingOpType::CAS1:
            return AtomicOperation::COMPARE_SWAP;
        case MemoryOrderingOpType::XCHG:
        case MemoryOrderingOpType::XCHG8:
        case MemoryOrderingOpType::XCHG4:
        case MemoryOrderingOpType::XCHG2:
        case MemoryOrderingOpType::XCHG1:
            return AtomicOperation::EXCHANGE;
        case MemoryOrderingOpType::FETCHADD:
        case MemoryOrderingOpType::FETCHADD8:
        case MemoryOrderingOpType::FETCHADD4:
        case MemoryOrderingOpType::FETCHADD2:
        case MemoryOrderingOpType::FETCHADD1:
            return AtomicOperation::FETCH_ADD;
        case MemoryOrderingOpType::FETCHAND:
            return AtomicOperation::FETCH_AND;
        case MemoryOrderingOpType::FETCHOR:
            return AtomicOperation::FETCH_OR;
        case MemoryOrderingOpType::FETCHXOR:
            return AtomicOperation::FETCH_XOR;
        case MemoryOrderingOpType::FETCHNAND:
            return AtomicOperation::FETCH_NAND;
        default:
            return AtomicOperation::COMPARE_SWAP;
        }
    }

    uint32_t determineAccessSize(MemoryOrderingOpType opType) const
    {
        switch (opType)
        {
        case MemoryOrderingOpType::CAS1:
        case MemoryOrderingOpType::XCHG1:
        case MemoryOrderingOpType::FETCHADD1:
            return 1;
        case MemoryOrderingOpType::CAS2:
        case MemoryOrderingOpType::XCHG2:
        case MemoryOrderingOpType::FETCHADD2:
            return 2;
        case MemoryOrderingOpType::CAS4:
        case MemoryOrderingOpType::XCHG4:
        case MemoryOrderingOpType::FETCHADD4:
        case MemoryOrderingOpType::LDL_L:
        case MemoryOrderingOpType::STL_C:
            return 4;
        case MemoryOrderingOpType::CAS8:
        case MemoryOrderingOpType::XCHG8:
        case MemoryOrderingOpType::FETCHADD8:
        case MemoryOrderingOpType::LDQ_L:
        case MemoryOrderingOpType::STQ_C:
            return 8;
        default:
            return 8;
        }
    }

    bool performMemoryOrderingOperation()
    {
        switch (m_opType)
        {
        // Memory barriers
        case MemoryOrderingOpType::EXCB:
        case MemoryOrderingOpType::TRAPB:
        case MemoryOrderingOpType::MB:
        case MemoryOrderingOpType::WMB:
        case MemoryOrderingOpType::RMB:
        case MemoryOrderingOpType::IMB:
        case MemoryOrderingOpType::MEMFENCE:
        case MemoryOrderingOpType::SFENCE:
        case MemoryOrderingOpType::LFENCE:
        case MemoryOrderingOpType::MFENCE:
            return performMemoryBarrier();

        // Cache operations
        case MemoryOrderingOpType::FETCH:
        case MemoryOrderingOpType::FETCH_M:
        case MemoryOrderingOpType::ECB:
        case MemoryOrderingOpType::WH64:
        case MemoryOrderingOpType::WH64EN:
            return performCacheOperation();

        // Lock operations
        case MemoryOrderingOpType::RS:
        case MemoryOrderingOpType::RC:
            return performLockOperation();

        // Load-locked/Store-conditional
        case MemoryOrderingOpType::LDL_L:
        case MemoryOrderingOpType::LDQ_L:
        case MemoryOrderingOpType::STL_C:
        case MemoryOrderingOpType::STQ_C:
            return performLoadLockStoreConditional();

        // Cache coherency operations
        case MemoryOrderingOpType::FLUSH:
        case MemoryOrderingOpType::FLUSHI:
        case MemoryOrderingOpType::INVAL:
        case MemoryOrderingOpType::WBACK:
        case MemoryOrderingOpType::WBINVAL:
            return performCacheCoherencyOperation();

        // Atomic operations
        case MemoryOrderingOpType::CAS:
        case MemoryOrderingOpType::CAS8:
        case MemoryOrderingOpType::CAS4:
        case MemoryOrderingOpType::CAS2:
        case MemoryOrderingOpType::CAS1:
        case MemoryOrderingOpType::XCHG:
        case MemoryOrderingOpType::XCHG8:
        case MemoryOrderingOpType::XCHG4:
        case MemoryOrderingOpType::XCHG2:
        case MemoryOrderingOpType::XCHG1:
        case MemoryOrderingOpType::FETCHADD:
        case MemoryOrderingOpType::FETCHADD8:
        case MemoryOrderingOpType::FETCHADD4:
        case MemoryOrderingOpType::FETCHADD2:
        case MemoryOrderingOpType::FETCHADD1:
        case MemoryOrderingOpType::FETCHAND:
        case MemoryOrderingOpType::FETCHOR:
        case MemoryOrderingOpType::FETCHXOR:
        case MemoryOrderingOpType::FETCHNAND:
            return performAtomicOperation();

        // Memory ordering semantics
        case MemoryOrderingOpType::ACQUIRE:
        case MemoryOrderingOpType::RELEASE:
        case MemoryOrderingOpType::ACQREL:
        case MemoryOrderingOpType::SEQCST:
            return performMemoryOrderingSemantics();

        // TLB operations
        case MemoryOrderingOpType::TLBFLUSH:
        case MemoryOrderingOpType::TLBINVAL:
        case MemoryOrderingOpType::TLBSYNC:
            return performTLBOperation();

        // DMA operations
        case MemoryOrderingOpType::DMAFLUSH:
        case MemoryOrderingOpType::DMAINVAL:
        case MemoryOrderingOpType::DMASYNC:
            return performDMAOperation();

        // Performance monitoring
        case MemoryOrderingOpType::PMFENCE:
        case MemoryOrderingOpType::PMFLUSH:
            return performPerformanceMonitorOperation();

        // Debug operations
        case MemoryOrderingOpType::DBGFENCE:
        case MemoryOrderingOpType::TRCFENCE:
            return performDebugOperation();

        default:
            return false;
        }
    }

    bool performMemoryBarrier()
    {
        m_memoryBarrierCount.fetch_add(1, std::memory_order_relaxed);
        m_fenceCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MemoryOrderingOpType::EXCB:
            DEBUG_LOG("Exception barrier executed");
            break;
        case MemoryOrderingOpType::TRAPB:
            DEBUG_LOG("Trap barrier executed");
            break;
        case MemoryOrderingOpType::MB:
        case MemoryOrderingOpType::MEMFENCE:
            DEBUG_LOG("Full memory barrier executed");
            std::atomic_thread_fence(std::memory_order_seq_cst);
            break;
        case MemoryOrderingOpType::WMB:
        case MemoryOrderingOpType::SFENCE:
            DEBUG_LOG("Write/Store memory barrier executed");
            std::atomic_thread_fence(std::memory_order_release);
            break;
        case MemoryOrderingOpType::RMB:
        case MemoryOrderingOpType::LFENCE:
            DEBUG_LOG("Read/Load memory barrier executed");
            std::atomic_thread_fence(std::memory_order_acquire);
            break;
        case MemoryOrderingOpType::IMB:
            DEBUG_LOG("Instruction memory barrier executed");
            break;
        case MemoryOrderingOpType::MFENCE:
            DEBUG_LOG("Memory and I/O fence executed");
            std::atomic_thread_fence(std::memory_order_seq_cst);
            break;
        default:
            return false;
        }

        return true;
    }

    bool performCacheOperation()
    {
        m_cacheOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MemoryOrderingOpType::FETCH:
            m_prefetchCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Prefetch for read at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::FETCH_M:
            m_prefetchCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Prefetch for modify at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::ECB:
            DEBUG_LOG("Evict cache block at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::WH64:
        case MemoryOrderingOpType::WH64EN:
            DEBUG_LOG("Write hint 64 bytes at address 0x%016llX", m_address);
            break;
        default:
            return false;
        }

        return true;
    }

    bool performLockOperation()
    {
        m_lockOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MemoryOrderingOpType::RS:
            // Read and set lock flag
            m_result = 0; // Assume lock was clear
            m_success = true;
            DEBUG_LOG("Read and set lock flag");
            break;
        case MemoryOrderingOpType::RC:
            // Read and clear lock flag
            m_result = 1; // Assume lock was set
            m_success = true;
            DEBUG_LOG("Read and clear lock flag");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performLoadLockStoreConditional()
    {
        switch (m_opType)
        {
        case MemoryOrderingOpType::LDL_L:
        case MemoryOrderingOpType::LDQ_L:
            m_loadLockCount.fetch_add(1, std::memory_order_relaxed);
            // Simulate load-locked operation
            m_result = m_value; // Load current value
            m_success = true;
            DEBUG_LOG("Load-locked %d bytes at address 0x%016llX, value=0x%016llX", m_accessSize, m_address, m_result);
            break;

        case MemoryOrderingOpType::STL_C:
        case MemoryOrderingOpType::STQ_C:
            m_storeConditionalCount.fetch_add(1, std::memory_order_relaxed);
            // Simulate store-conditional operation
            // In real implementation, would check if reservation is still valid
            m_success = true; // Assume success for simulation
            m_result = m_success ? 1 : 0;
            if (m_success)
            {
                m_atomicSuccessCount.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                m_atomicFailureCount.fetch_add(1, std::memory_order_relaxed);
            }
            DEBUG_LOG("Store-conditional %d bytes at address 0x%016llX, value=0x%016llX, success=%s", m_accessSize,
                      m_address, m_value, m_success ? "true" : "false");
            break;

        default:
            return false;
        }

        return true;
    }

    bool performCacheCoherencyOperation()
    {
        m_cacheOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MemoryOrderingOpType::FLUSH:
            m_flushCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Cache flush at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::FLUSHI:
            m_flushCount.fetch_add(1, std::memory_order_relaxed);
            m_invalidateCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Cache flush and invalidate at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::INVAL:
            m_invalidateCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Cache invalidate at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::WBACK:
            DEBUG_LOG("Cache writeback at address 0x%016llX", m_address);
            break;
        case MemoryOrderingOpType::WBINVAL:
            m_invalidateCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("Cache writeback and invalidate at address 0x%016llX", m_address);
            break;
        default:
            return false;
        }

        return true;
    }

    bool performAtomicOperation()
    {
        m_atomicOperationCount.fetch_add(1, std::memory_order_relaxed);

        // Simulate atomic operation success/failure
        m_success = true; // Assume success for most operations

        switch (m_atomicOperation)
        {
        case AtomicOperation::COMPARE_SWAP:
            // Compare current value with compare value
            if (m_value == m_compareValue)
            {
                m_result = m_compareValue; // Return old value
                m_value = m_value;         // Would store new value in real implementation
                m_success = true;
            }
            else
            {
                m_result = m_value; // Return current value
                m_success = false;
            }
            DEBUG_LOG("Compare-and-swap %d bytes: addr=0x%016llX, compare=0x%016llX, new=0x%016llX, success=%s",
                      m_accessSize, m_address, m_compareValue, m_value, m_success ? "true" : "false");
            break;

        case AtomicOperation::EXCHANGE:
            m_result = m_value; // Return old value
            // Would store new value in real implementation
            DEBUG_LOG("Exchange %d bytes: addr=0x%016llX, old=0x%016llX, new=0x%016llX", m_accessSize, m_address,
                      m_result, m_value);
            break;

        case AtomicOperation::FETCH_ADD:
            m_result = m_value; // Return old value
            m_value += m_value; // Add value (simplified)
            DEBUG_LOG("Fetch-and-add %d bytes: addr=0x%016llX, old=0x%016llX, add=0x%016llX", m_accessSize, m_address,
                      m_result, m_value);
            break;

        case AtomicOperation::FETCH_AND:
            m_result = m_value; // Return old value
            m_value &= m_value; // AND value (simplified)
            DEBUG_LOG("Fetch-and-AND %d bytes: addr=0x%016llX, old=0x%016llX", m_accessSize, m_address, m_result);
            break;

        case AtomicOperation::FETCH_OR:
            m_result = m_value; // Return old value
            m_value |= m_value; // OR value (simplified)
            DEBUG_LOG("Fetch-and-OR %d bytes: addr=0x%016llX, old=0x%016llX", m_accessSize, m_address, m_result);
            break;

        case AtomicOperation::FETCH_XOR:
            m_result = m_value; // Return old value
            m_value ^= m_value; // XOR value (simplified)
            DEBUG_LOG("Fetch-and-XOR %d bytes: addr=0x%016llX, old=0x%016llX", m_accessSize, m_address, m_result);
            break;

        case AtomicOperation::FETCH_NAND:
            m_result = m_value;             // Return old value
            m_value = ~(m_value & m_value); // NAND value (simplified)
            DEBUG_LOG("Fetch-and-NAND %d bytes: addr=0x%016llX, old=0x%016llX", m_accessSize, m_address, m_result);
            break;

        default:
            return false;
        }

        if (m_success)
        {
            m_atomicSuccessCount.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            m_atomicFailureCount.fetch_add(1, std::memory_order_relaxed);
        }

        return true;
    }

    bool performMemoryOrderingSemantics()
    {
        m_fenceCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MemoryOrderingOpType::ACQUIRE:
            std::atomic_thread_fence(std::memory_order_acquire);
            DEBUG_LOG("Acquire semantics fence executed");
            break;
        case MemoryOrderingOpType::RELEASE:
            std::atomic_thread_fence(std::memory_order_release);
            DEBUG_LOG("Release semantics fence executed");
            break;
        case MemoryOrderingOpType::ACQREL:
            std::atomic_thread_fence(std::memory_order_acq_rel);
            DEBUG_LOG("Acquire-release semantics fence executed");
            break;
        case MemoryOrderingOpType::SEQCST:
            std::atomic_thread_fence(std::memory_order_seq_cst);
            DEBUG_LOG("Sequential consistency fence executed");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performTLBOperation()
    {
        m_tlbOperationCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case MemoryOrderingOpType::TLBFLUSH:
            DEBUG_LOG("TLB flush executed");
            break;
        case MemoryOrderingOpType::TLBINVAL:
            DEBUG_LOG("TLB invalidate executed");
            break;
        case MemoryOrderingOpType::TLBSYNC:
            DEBUG_LOG("TLB synchronize executed");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performDMAOperation()
    {
        switch (m_opType)
        {
        case MemoryOrderingOpType::DMAFLUSH:
            DEBUG_LOG("DMA cache flush executed");
            break;
        case MemoryOrderingOpType::DMAINVAL:
            DEBUG_LOG("DMA cache invalidate executed");
            break;
        case MemoryOrderingOpType::DMASYNC:
            DEBUG_LOG("DMA synchronize executed");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performPerformanceMonitorOperation()
    {
        switch (m_opType)
        {
        case MemoryOrderingOpType::PMFENCE:
            DEBUG_LOG("Performance monitor fence executed");
            break;
        case MemoryOrderingOpType::PMFLUSH:
            DEBUG_LOG("Performance monitor flush executed");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performDebugOperation()
    {
        switch (m_opType)
        {
        case MemoryOrderingOpType::DBGFENCE:
            DEBUG_LOG("Debug fence executed");
            break;
        case MemoryOrderingOpType::TRCFENCE:
            DEBUG_LOG("Trace fence executed");
            break;
        default:
            return false;
        }

        return true;
    }

  private:
     MemoryOrderingOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg;
     uint8_t m_addrReg;
     MemoryOrdering m_memoryOrdering;

    CacheScope m_cacheScope;
    BarrierType m_barrierType;
    AtomicOperation m_atomicOperation;
    uint64_t m_address;
    uint64_t m_value;
    uint64_t m_compareValue;
    uint64_t m_result;
    uint32_t m_accessSize;
    uint32_t m_cacheLineSize;
    bool m_success;

    std::atomic<uint64_t> m_memoryBarrierCount;
    std::atomic<uint64_t> m_cacheOperationCount;
    std::atomic<uint64_t> m_lockOperationCount;
    std::atomic<uint64_t> m_atomicOperationCount;
    std::atomic<uint64_t> m_tlbOperationCount;
    std::atomic<uint64_t> m_prefetchCount;
    std::atomic<uint64_t> m_flushCount;
    std::atomic<uint64_t> m_invalidateCount;
    std::atomic<uint64_t> m_fenceCount;
    std::atomic<uint64_t> m_loadLockCount;
    std::atomic<uint64_t> m_storeConditionalCount;
    std::atomic<uint64_t> m_atomicSuccessCount;
    std::atomic<uint64_t> m_atomicFailureCount;

    // Prevent copying for performance
    alphaMemoryOrderingInstruction(const alphaMemoryOrderingInstruction &) = delete;
    alphaMemoryOrderingInstruction &operator=(const alphaMemoryOrderingInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#if defined(_WIN32) && defined(_MSC_VER)
// Microsoft VS2022 specific optimizations
#pragma pack(push, 8)
#endif

class alphaVAXCompatibilityInstruction : public alphaInstructionBase
{
  public:
    enum class VAXCompatOpType : uint16_t
    {
        // VAX Integer Arithmetic with Overflow Detection
        ADDLV = 0x400, // Add longword with overflow trap
        SUBLV = 0x409, // Subtract longword with overflow trap
        MULLV = 0x408, // Multiply longword with overflow trap
        DIVLV = 0x40B, // Divide longword with overflow trap

        // VAX Floating Point Operations (F, D, G formats)
        ADDF = 0x080, // Add F-format (VAX single precision)
        SUBF = 0x081, // Subtract F-format
        MULF = 0x082, // Multiply F-format
        DIVF = 0x083, // Divide F-format
        NEGF = 0x085, // Negate F-format
        ABSF = 0x084, // Absolute value F-format

        ADDD = 0x0C0, // Add D-format (VAX double precision)
        SUBD = 0x0C1, // Subtract D-format
        MULD = 0x0C2, // Multiply D-format
        DIVD = 0x0C3, // Divide D-format
        NEGD = 0x0C5, // Negate D-format
        ABSD = 0x0C4, // Absolute value D-format

        ADDG = 0x0A0, // Add G-format (VAX extended double)
        SUBG = 0x0A1, // Subtract G-format
        MULG = 0x0A2, // Multiply G-format
        DIVG = 0x0A3, // Divide G-format
        NEGG = 0x0A5, // Negate G-format
        ABSG = 0x0A4, // Absolute value G-format

        // VAX Floating Point Conversions
        CVTFD = 0x0AC, // Convert F to D format
        CVTDF = 0x0AD, // Convert D to F format
        CVTFG = 0x0AE, // Convert F to G format
        CVTGF = 0x0AF, // Convert G to F format
        CVTDG = 0x0CE, // Convert D to G format
        CVTGD = 0x0CF, // Convert G to D format

        // VAX Integer/Float Conversions
        CVTFL = 0x088, // Convert F-format to longword
        CVTLF = 0x089, // Convert longword to F-format
        CVTDL = 0x0C8, // Convert D-format to longword
        CVTLD = 0x0C9, // Convert longword to D-format
        CVTGL = 0x0A8, // Convert G-format to longword
        CVTLG = 0x0A9, // Convert longword to G-format

        // VAX Condition Code Operations
        TSTF = 0x08A, // Test F-format (set condition codes)
        TSTD = 0x0CA, // Test D-format
        TSTG = 0x0AA, // Test G-format
        TSTL = 0x048, // Test longword

        // VAX Compare Operations
        CMPF = 0x08B, // Compare F-format
        CMPD = 0x0CB, // Compare D-format
        CMPG = 0x0AB, // Compare G-format
        CMPL = 0x049, // Compare longword

        // VAX Bit Field Operations
        EXTV = 0x500,  // Extract field (variable)
        EXTZV = 0x501, // Extract field zero-extended
        INSV = 0x502,  // Insert field
        FFC = 0x510,   // Find first clear bit
        FFS = 0x511,   // Find first set bit

        // VAX String Operations
        MOVC3 = 0x520, // Move character string (3 operand)
        MOVC5 = 0x521, // Move character string (5 operand)
        CMPC3 = 0x522, // Compare character string (3 operand)
        CMPC5 = 0x523, // Compare character string (5 operand)
        LOCC = 0x524,  // Locate character
        SKPC = 0x525,  // Skip character
        SCANC = 0x526, // Scan character
        SPANC = 0x527, // Span character

        // VAX Packed Decimal Operations
        ADDP4 = 0x540, // Add packed decimal (4 operand)
        ADDP6 = 0x541, // Add packed decimal (6 operand)
        SUBP4 = 0x542, // Subtract packed decimal (4 operand)
        SUBP6 = 0x543, // Subtract packed decimal (6 operand)
        MULP = 0x544,  // Multiply packed decimal
        DIVP = 0x545,  // Divide packed decimal
        CVTLP = 0x546, // Convert longword to packed
        CVTPL = 0x547, // Convert packed to longword
        CVTPT = 0x548, // Convert packed to trailing numeric
        CVTTP = 0x549, // Convert trailing numeric to packed
        CVTPS = 0x54A, // Convert packed to separate numeric
        CVTSP = 0x54B, // Convert separate numeric to packed

        // VAX Decimal String Operations
        MOVP = 0x550,   // Move packed decimal
        CMPP3 = 0x551,  // Compare packed decimal (3 operand)
        CMPP4 = 0x552,  // Compare packed decimal (4 operand)
        ASHP = 0x553,   // Arithmetic shift packed
        EDITPC = 0x554, // Edit packed to character

        // VAX Address Calculation
        MOVA = 0x560,  // Move address
        PUSHA = 0x561, // Push address

        // VAX Procedure Call/Return
        CALLS = 0x570, // Call procedure
        CALLG = 0x571, // Call procedure (general)
        RET = 0x572,   // Return from procedure

        // VAX Miscellaneous
        HALT = 0x580,   // Halt processor
        NOP = 0x581,    // No operation
        LDPCTX = 0x582, // Load process context
        SVPCTX = 0x583, // Save process context
        MTPR = 0x584,   // Move to processor register
        MFPR = 0x585,   // Move from processor register

        // VAX CRC Operations
        CRC = 0x590, // Calculate CRC

        // VAX Queue Operations
        INSQUE = 0x5A0, // Insert entry in queue
        REMQUE = 0x5A1, // Remove entry from queue

        // VAX Atomic Operations
        ADAWI = 0x5B0, // Add aligned word interlocked

        // Unknown operation
        UNKNOWN = 0xFFFF
    };

    enum class VAXDataType : uint8_t
    {
        BYTE = 1,            // 8-bit byte
        WORD = 2,            // 16-bit word
        LONGWORD = 4,        // 32-bit longword
        QUADWORD = 8,        // 64-bit quadword
        F_FLOAT = 4,         // VAX F-format floating point
        D_FLOAT = 8,         // VAX D-format floating point
        G_FLOAT = 8,         // VAX G-format floating point
        PACKED_DECIMAL = 0,  // Variable length packed decimal
        CHARACTER_STRING = 0 // Variable length character string
    };

    enum class VAXConditionCode : uint8_t
    {
        N = 8, // Negative
        Z = 4, // Zero
        V = 2, // Overflow
        C = 1  // Carry
    };

    enum class VAXFloatFormat : uint8_t
    {
        F_FORMAT = 0, // 32-bit VAX F-format
        D_FORMAT = 1, // 64-bit VAX D-format
        G_FORMAT = 2  // 64-bit VAX G-format
    };

    explicit alphaVAXCompatibilityInstruction(uint32_t opcode, VAXCompatOpType opType, uint8_t destReg, uint8_t srcReg1,
                                              uint8_t srcReg2)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_srcReg3(0), m_dataType(determineDataType(opType)), m_vaxFloatFormat(determineVAXFloatFormat(opType)),
          m_operand1(0), m_operand2(0), m_operand3(0), m_result(0), m_floatOperand1(0.0), m_floatOperand2(0.0),
          m_floatResult(0.0), m_conditionCodes(0), m_overflowFlag(false), m_zeroFlag(false), m_negativeFlag(false),
          m_carryFlag(false), m_stringLength(0), m_stringAddress(0), m_packedDecimalLength(0), m_vaxArithmeticCount(0),
          m_vaxFloatCount(0), m_vaxConversionCount(0), m_vaxStringCount(0), m_vaxDecimalCount(0), m_vaxBitFieldCount(0),
          m_vaxConditionCount(0), m_vaxOverflowCount(0), m_vaxProcedureCount(0), m_vaxQueueCount(0),
          m_vaxAtomicCount(0), m_formatConversionCount(0)
    {
        DEBUG_LOG("alphaVAXCompatibilityInstruction created - OpType: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2);
    }

    // Constructor for operations with 3+ operands
    explicit alphaVAXCompatibilityInstruction(uint32_t opcode, VAXCompatOpType opType, uint8_t destReg, uint8_t srcReg1,
                                              uint8_t srcReg2, uint8_t srcReg3)
        : alphaInstructionBase(opcode), m_opType(opType), m_destReg(destReg), m_srcReg1(srcReg1), m_srcReg2(srcReg2),
          m_srcReg3(srcReg3), m_dataType(determineDataType(opType)), m_vaxFloatFormat(determineVAXFloatFormat(opType)),
          m_operand1(0), m_operand2(0), m_operand3(0), m_result(0), m_floatOperand1(0.0), m_floatOperand2(0.0),
          m_floatResult(0.0), m_conditionCodes(0), m_overflowFlag(false), m_zeroFlag(false), m_negativeFlag(false),
          m_carryFlag(false), m_stringLength(0), m_stringAddress(0), m_packedDecimalLength(0), m_vaxArithmeticCount(0),
          m_vaxFloatCount(0), m_vaxConversionCount(0), m_vaxStringCount(0), m_vaxDecimalCount(0), m_vaxBitFieldCount(0),
          m_vaxConditionCount(0), m_vaxOverflowCount(0), m_vaxProcedureCount(0), m_vaxQueueCount(0),
          m_vaxAtomicCount(0), m_formatConversionCount(0)
    {
        DEBUG_LOG("alphaVAXCompatibilityInstruction created (3-op) - OpType: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d, "
                  "Src3: R%d",
                  static_cast<int>(opType), destReg, srcReg1, srcReg2, srcReg3);
    }

    virtual ~alphaVAXCompatibilityInstruction() = default;

    // Core execution interface implementation
    bool execute() override
    {
        incrementExecutionCount();

        bool success = performVAXCompatibilityOperation();
        if (success)
        {
            addCycles(getCycleLatency());
        }

        return success;
    }

void decode()
    {
        DEBUG_LOG("Decoding VAX compatibility instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        uint16_t function = opcode & 0x7FF;
        uint8_t rc = opcode & 0x1F;

        m_srcReg1 = ra;
        m_srcReg2 = rb;
        m_destReg = rc;
        m_srcReg3 = 0; // Will be set for multi-operand instructions

        // Determine VAX compatibility operation type
        switch (primaryOpcode)
        {
        case 0x10: // Integer arithmetic with overflow
            switch (function)
            {
            case 0x400:
                m_opType = VAXCompatOpType::ADDLV;
                break;
            case 0x409:
                m_opType = VAXCompatOpType::SUBLV;
                break;
            case 0x408:
                m_opType = VAXCompatOpType::MULLV;
                break;
            case 0x40B:
                m_opType = VAXCompatOpType::DIVLV;
                break;
            default:
                DEBUG_LOG("Unknown VAX integer function: 0x%03X", function);
                m_opType = VAXCompatOpType::ADDLV;
                break;
            }
            break;

        case 0x14: // VAX floating point
            switch (function)
            {
            case 0x080:
                m_opType = VAXCompatOpType::ADDF;
                break;
            case 0x081:
                m_opType = VAXCompatOpType::SUBF;
                break;
            case 0x082:
                m_opType = VAXCompatOpType::MULF;
                break;
            case 0x083:
                m_opType = VAXCompatOpType::DIVF;
                break;
            case 0x085:
                m_opType = VAXCompatOpType::NEGF;
                break;
            case 0x084:
                m_opType = VAXCompatOpType::ABSF;
                break;
            case 0x0C0:
                m_opType = VAXCompatOpType::ADDD;
                break;
            case 0x0C1:
                m_opType = VAXCompatOpType::SUBD;
                break;
            case 0x0C2:
                m_opType = VAXCompatOpType::MULD;
                break;
            case 0x0C3:
                m_opType = VAXCompatOpType::DIVD;
                break;
            case 0x0A0:
                m_opType = VAXCompatOpType::ADDG;
                break;
            case 0x0A1:
                m_opType = VAXCompatOpType::SUBG;
                break;
            case 0x0A2:
                m_opType = VAXCompatOpType::MULG;
                break;
            case 0x0A3:
                m_opType = VAXCompatOpType::DIVG;
                break;
            default:
                DEBUG_LOG("Unknown VAX FP function: 0x%03X", function);
                m_opType = VAXCompatOpType::ADDF;
                break;
            }
            break;

        default:
            DEBUG_LOG("Unknown VAX compatibility primary opcode: 0x%02X", primaryOpcode);
            m_opType = VAXCompatOpType::UNKNOWN;
            break;
        }

        DEBUG_LOG("VAX compatibility instruction decoded - Type: 0x%04X, Dest: R%d, Src1: R%d, Src2: R%d",
                  static_cast<int>(m_opType), m_destReg, m_srcReg1, m_srcReg2);
    }

    uint32_t getCycleLatency() const override
    {
        switch (m_opType)
        {
        // VAX integer arithmetic with overflow
        case VAXCompatOpType::ADDLV:
        case VAXCompatOpType::SUBLV:
            return 2;
        case VAXCompatOpType::MULLV:
            return 6;
        case VAXCompatOpType::DIVLV:
            return 25;

        // VAX F-format floating point
        case VAXCompatOpType::ADDF:
        case VAXCompatOpType::SUBF:
            return 4;
        case VAXCompatOpType::MULF:
            return 4;
        case VAXCompatOpType::DIVF:
            return 15;
        case VAXCompatOpType::NEGF:
        case VAXCompatOpType::ABSF:
            return 1;

        // VAX D-format floating point
        case VAXCompatOpType::ADDD:
        case VAXCompatOpType::SUBD:
            return 4;
        case VAXCompatOpType::MULD:
            return 4;
        case VAXCompatOpType::DIVD:
            return 18;
        case VAXCompatOpType::NEGD:
        case VAXCompatOpType::ABSD:
            return 1;

        // VAX G-format floating point
        case VAXCompatOpType::ADDG:
        case VAXCompatOpType::SUBG:
            return 4;
        case VAXCompatOpType::MULG:
            return 4;
        case VAXCompatOpType::DIVG:
            return 18;
        case VAXCompatOpType::NEGG:
        case VAXCompatOpType::ABSG:
            return 1;

        // VAX floating point conversions
        case VAXCompatOpType::CVTFD:
        case VAXCompatOpType::CVTDF:
        case VAXCompatOpType::CVTFG:
        case VAXCompatOpType::CVTGF:
        case VAXCompatOpType::CVTDG:
        case VAXCompatOpType::CVTGD:
            return 4;

        // VAX integer/float conversions
        case VAXCompatOpType::CVTFL:
        case VAXCompatOpType::CVTLF:
        case VAXCompatOpType::CVTDL:
        case VAXCompatOpType::CVTLD:
        case VAXCompatOpType::CVTGL:
        case VAXCompatOpType::CVTLG:
            return 4;

        // VAX condition code operations
        case VAXCompatOpType::TSTF:
        case VAXCompatOpType::TSTD:
        case VAXCompatOpType::TSTG:
        case VAXCompatOpType::TSTL:
            return 1;

        // VAX compare operations
        case VAXCompatOpType::CMPF:
        case VAXCompatOpType::CMPD:
        case VAXCompatOpType::CMPG:
        case VAXCompatOpType::CMPL:
            return 2;

        // VAX bit field operations
        case VAXCompatOpType::EXTV:
        case VAXCompatOpType::EXTZV:
        case VAXCompatOpType::INSV:
            return 3;
        case VAXCompatOpType::FFC:
        case VAXCompatOpType::FFS:
            return 4;

        // VAX string operations
        case VAXCompatOpType::MOVC3:
        case VAXCompatOpType::MOVC5:
            return 10; // Variable based on length
        case VAXCompatOpType::CMPC3:
        case VAXCompatOpType::CMPC5:
            return 8;
        case VAXCompatOpType::LOCC:
        case VAXCompatOpType::SKPC:
        case VAXCompatOpType::SCANC:
        case VAXCompatOpType::SPANC:
            return 6;

        // VAX packed decimal operations
        case VAXCompatOpType::ADDP4:
        case VAXCompatOpType::ADDP6:
        case VAXCompatOpType::SUBP4:
        case VAXCompatOpType::SUBP6:
            return 15;
        case VAXCompatOpType::MULP:
            return 25;
        case VAXCompatOpType::DIVP:
            return 40;
        case VAXCompatOpType::CVTLP:
        case VAXCompatOpType::CVTPL:
        case VAXCompatOpType::CVTPT:
        case VAXCompatOpType::CVTTP:
        case VAXCompatOpType::CVTPS:
        case VAXCompatOpType::CVTSP:
            return 12;

        // VAX decimal string operations
        case VAXCompatOpType::MOVP:
            return 8;
        case VAXCompatOpType::CMPP3:
        case VAXCompatOpType::CMPP4:
            return 10;
        case VAXCompatOpType::ASHP:
            return 15;
        case VAXCompatOpType::EDITPC:
            return 30;

        // VAX address operations
        case VAXCompatOpType::MOVA:
        case VAXCompatOpType::PUSHA:
            return 1;

        // VAX procedure operations
        case VAXCompatOpType::CALLS:
        case VAXCompatOpType::CALLG:
            return 20;
        case VAXCompatOpType::RET:
            return 15;

        // VAX miscellaneous
        case VAXCompatOpType::HALT:
            return 100;
        case VAXCompatOpType::NOP:
            return 1;
        case VAXCompatOpType::LDPCTX:
        case VAXCompatOpType::SVPCTX:
            return 50;
        case VAXCompatOpType::MTPR:
        case VAXCompatOpType::MFPR:
            return 3;

        // VAX CRC
        case VAXCompatOpType::CRC:
            return 8;

        // VAX queue operations
        case VAXCompatOpType::INSQUE:
        case VAXCompatOpType::REMQUE:
            return 5;

        // VAX atomic operations
        case VAXCompatOpType::ADAWI:
            return 8;

        default:
            return 4;
        }
    }

    // Floating point classification
    bool isFloatingPoint() const override
    {
        return (m_opType >= VAXCompatOpType::ADDF && m_opType <= VAXCompatOpType::ABSG) ||
               (m_opType >= VAXCompatOpType::CVTFD && m_opType <= VAXCompatOpType::CVTLG) ||
               (m_opType >= VAXCompatOpType::TSTF && m_opType <= VAXCompatOpType::CMPG);
    }

    // Performance-critical accessors
    inline VAXCompatOpType getOpType() const { return m_opType; }
    inline uint8_t getDestReg() const { return m_destReg; }
    inline uint8_t getSrcReg1() const { return m_srcReg1; }
    inline uint8_t getSrcReg2() const { return m_srcReg2; }
    inline uint8_t getSrcReg3() const { return m_srcReg3; }
    inline VAXDataType getDataType() const { return m_dataType; }
    inline VAXFloatFormat getVAXFloatFormat() const { return m_vaxFloatFormat; }
    inline uint64_t getResult() const { return m_result; }
    inline double getFloatResult() const { return m_floatResult; }
    inline uint8_t getConditionCodes() const { return m_conditionCodes; }
    inline bool getOverflowFlag() const { return m_overflowFlag; }
    inline bool getZeroFlag() const { return m_zeroFlag; }
    inline bool getNegativeFlag() const { return m_negativeFlag; }
    inline bool getCarryFlag() const { return m_carryFlag; }

    // Performance counters
    inline uint64_t getVAXArithmeticCount() const { return m_vaxArithmeticCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXFloatCount() const { return m_vaxFloatCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXConversionCount() const { return m_vaxConversionCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXStringCount() const { return m_vaxStringCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXDecimalCount() const { return m_vaxDecimalCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXBitFieldCount() const { return m_vaxBitFieldCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXConditionCount() const { return m_vaxConditionCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXOverflowCount() const { return m_vaxOverflowCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXProcedureCount() const { return m_vaxProcedureCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXQueueCount() const { return m_vaxQueueCount.load(std::memory_order_relaxed); }
    inline uint64_t getVAXAtomicCount() const { return m_vaxAtomicCount.load(std::memory_order_relaxed); }
    inline uint64_t getFormatConversionCount() const { return m_formatConversionCount.load(std::memory_order_relaxed); }

    // Operation classification
    inline bool isVAXArithmetic() const
    {
        return (m_opType >= VAXCompatOpType::ADDLV && m_opType <= VAXCompatOpType::DIVLV);
    }

    inline bool isVAXFloatingPoint() const
    {
        return (m_opType >= VAXCompatOpType::ADDF && m_opType <= VAXCompatOpType::ABSG);
    }

    inline bool isVAXConversion() const
    {
        return (m_opType >= VAXCompatOpType::CVTFD && m_opType <= VAXCompatOpType::CVTLG);
    }

    inline bool isVAXString() const
    {
        return (m_opType >= VAXCompatOpType::MOVC3 && m_opType <= VAXCompatOpType::SPANC);
    }

    inline bool isVAXDecimal() const
    {
        return (m_opType >= VAXCompatOpType::ADDP4 && m_opType <= VAXCompatOpType::EDITPC);
    }

    inline bool isVAXBitField() const
    {
        return (m_opType >= VAXCompatOpType::EXTV && m_opType <= VAXCompatOpType::FFS);
    }

    // Hot path execution support
    inline void setOperands(uint64_t op1, uint64_t op2, uint64_t op3 = 0)
    {
        m_operand1 = op1;
        m_operand2 = op2;
        m_operand3 = op3;
    }

    inline void setFloatOperands(double op1, double op2)
    {
        m_floatOperand1 = op1;
        m_floatOperand2 = op2;
    }

    inline void setStringParameters(uint64_t address, uint32_t length)
    {
        m_stringAddress = address;
        m_stringLength = length;
    }

  private:
    VAXDataType determineDataType(VAXCompatOpType opType) const
    {
        switch (opType)
        {
        case VAXCompatOpType::ADDLV:
        case VAXCompatOpType::SUBLV:
        case VAXCompatOpType::MULLV:
        case VAXCompatOpType::DIVLV:
        case VAXCompatOpType::TSTL:
        case VAXCompatOpType::CMPL:
            return VAXDataType::LONGWORD;

        case VAXCompatOpType::ADDF:
        case VAXCompatOpType::SUBF:
        case VAXCompatOpType::MULF:
        case VAXCompatOpType::DIVF:
        case VAXCompatOpType::NEGF:
        case VAXCompatOpType::ABSF:
        case VAXCompatOpType::TSTF:
        case VAXCompatOpType::CMPF:
            return VAXDataType::F_FLOAT;

        case VAXCompatOpType::ADDD:
        case VAXCompatOpType::SUBD:
        case VAXCompatOpType::MULD:
        case VAXCompatOpType::DIVD:
        case VAXCompatOpType::NEGD:
        case VAXCompatOpType::ABSD:
        case VAXCompatOpType::TSTD:
        case VAXCompatOpType::CMPD:
            return VAXDataType::D_FLOAT;

        case VAXCompatOpType::ADDG:
        case VAXCompatOpType::SUBG:
        case VAXCompatOpType::MULG:
        case VAXCompatOpType::DIVG:
        case VAXCompatOpType::NEGG:
        case VAXCompatOpType::ABSG:
        case VAXCompatOpType::TSTG:
        case VAXCompatOpType::CMPG:
            return VAXDataType::G_FLOAT;

        case VAXCompatOpType::MOVC3:
        case VAXCompatOpType::MOVC5:
        case VAXCompatOpType::CMPC3:
        case VAXCompatOpType::CMPC5:
        case VAXCompatOpType::LOCC:
        case VAXCompatOpType::SKPC:
        case VAXCompatOpType::SCANC:
        case VAXCompatOpType::SPANC:
            return VAXDataType::CHARACTER_STRING;

        case VAXCompatOpType::ADDP4:
        case VAXCompatOpType::ADDP6:
        case VAXCompatOpType::SUBP4:
        case VAXCompatOpType::SUBP6:
        case VAXCompatOpType::MULP:
        case VAXCompatOpType::DIVP:
        case VAXCompatOpType::MOVP:
        case VAXCompatOpType::CMPP3:
        case VAXCompatOpType::CMPP4:
        case VAXCompatOpType::ASHP:
        case VAXCompatOpType::EDITPC:
            return VAXDataType::PACKED_DECIMAL;

        default:
            return VAXDataType::LONGWORD;
        }
    }

    VAXFloatFormat determineVAXFloatFormat(VAXCompatOpType opType) const
    {
        switch (opType)
        {
        case VAXCompatOpType::ADDF:
        case VAXCompatOpType::SUBF:
        case VAXCompatOpType::MULF:
        case VAXCompatOpType::DIVF:
        case VAXCompatOpType::NEGF:
        case VAXCompatOpType::ABSF:
        case VAXCompatOpType::TSTF:
        case VAXCompatOpType::CMPF:
            return VAXFloatFormat::F_FORMAT;

        case VAXCompatOpType::ADDD:
        case VAXCompatOpType::SUBD:
        case VAXCompatOpType::MULD:
        case VAXCompatOpType::DIVD:
        case VAXCompatOpType::NEGD:
        case VAXCompatOpType::ABSD:
        case VAXCompatOpType::TSTD:
        case VAXCompatOpType::CMPD:
            return VAXFloatFormat::D_FORMAT;

        case VAXCompatOpType::ADDG:
        case VAXCompatOpType::SUBG:
        case VAXCompatOpType::MULG:
        case VAXCompatOpType::DIVG:
        case VAXCompatOpType::NEGG:
        case VAXCompatOpType::ABSG:
        case VAXCompatOpType::TSTG:
        case VAXCompatOpType::CMPG:
            return VAXFloatFormat::G_FORMAT;

        default:
            return VAXFloatFormat::F_FORMAT;
        }
    }

    bool performVAXCompatibilityOperation()
    {
        switch (m_opType)
        {
        // VAX integer arithmetic
        case VAXCompatOpType::ADDLV:
        case VAXCompatOpType::SUBLV:
        case VAXCompatOpType::MULLV:
        case VAXCompatOpType::DIVLV:
            return performVAXArithmetic();

        // VAX floating point operations
        case VAXCompatOpType::ADDF:
        case VAXCompatOpType::SUBF:
        case VAXCompatOpType::MULF:
        case VAXCompatOpType::DIVF:
        case VAXCompatOpType::NEGF:
        case VAXCompatOpType::ABSF:
        case VAXCompatOpType::ADDD:
        case VAXCompatOpType::SUBD:
        case VAXCompatOpType::MULD:
        case VAXCompatOpType::DIVD:
        case VAXCompatOpType::NEGD:
        case VAXCompatOpType::ABSD:
        case VAXCompatOpType::ADDG:
        case VAXCompatOpType::SUBG:
        case VAXCompatOpType::MULG:
        case VAXCompatOpType::DIVG:
        case VAXCompatOpType::NEGG:
        case VAXCompatOpType::ABSG:
            return performVAXFloatingPoint();

        // VAX conversions
        case VAXCompatOpType::CVTFD:
        case VAXCompatOpType::CVTDF:
        case VAXCompatOpType::CVTFG:
        case VAXCompatOpType::CVTGF:
        case VAXCompatOpType::CVTDG:
        case VAXCompatOpType::CVTGD:
        case VAXCompatOpType::CVTFL:
        case VAXCompatOpType::CVTLF:
        case VAXCompatOpType::CVTDL:
        case VAXCompatOpType::CVTLD:
        case VAXCompatOpType::CVTGL:
        case VAXCompatOpType::CVTLG:
            return performVAXConversion();

        // VAX condition code operations
        case VAXCompatOpType::TSTF:
        case VAXCompatOpType::TSTD:
        case VAXCompatOpType::TSTG:
        case VAXCompatOpType::TSTL:
        case VAXCompatOpType::CMPF:
        case VAXCompatOpType::CMPD:
        case VAXCompatOpType::CMPG:
        case VAXCompatOpType::CMPL:
            return performVAXConditionCode();

        // VAX bit field operations
        case VAXCompatOpType::EXTV:
        case VAXCompatOpType::EXTZV:
        case VAXCompatOpType::INSV:
        case VAXCompatOpType::FFC:
        case VAXCompatOpType::FFS:
            return performVAXBitField();

        // VAX string operations
        case VAXCompatOpType::MOVC3:
        case VAXCompatOpType::MOVC5:
        case VAXCompatOpType::CMPC3:
        case VAXCompatOpType::CMPC5:
        case VAXCompatOpType::LOCC:
        case VAXCompatOpType::SKPC:
        case VAXCompatOpType::SCANC:
        case VAXCompatOpType::SPANC:
            return performVAXString();

        // VAX packed decimal operations
        case VAXCompatOpType::ADDP4:
        case VAXCompatOpType::ADDP6:
        case VAXCompatOpType::SUBP4:
        case VAXCompatOpType::SUBP6:
        case VAXCompatOpType::MULP:
        case VAXCompatOpType::DIVP:
        case VAXCompatOpType::CVTLP:
        case VAXCompatOpType::CVTPL:
        case VAXCompatOpType::CVTPT:
        case VAXCompatOpType::CVTTP:
        case VAXCompatOpType::CVTPS:
        case VAXCompatOpType::CVTSP:
        case VAXCompatOpType::MOVP:
        case VAXCompatOpType::CMPP3:
        case VAXCompatOpType::CMPP4:
        case VAXCompatOpType::ASHP:
        case VAXCompatOpType::EDITPC:
            return performVAXDecimal();

        // VAX address operations
        case VAXCompatOpType::MOVA:
        case VAXCompatOpType::PUSHA:
            return performVAXAddress();

        // VAX procedure operations
        case VAXCompatOpType::CALLS:
        case VAXCompatOpType::CALLG:
        case VAXCompatOpType::RET:
            return performVAXProcedure();

        // VAX miscellaneous
        case VAXCompatOpType::HALT:
        case VAXCompatOpType::NOP:
        case VAXCompatOpType::LDPCTX:
        case VAXCompatOpType::SVPCTX:
        case VAXCompatOpType::MTPR:
        case VAXCompatOpType::MFPR:
            return performVAXMiscellaneous();

        // VAX CRC
        case VAXCompatOpType::CRC:
            return performVAXCRC();

        // VAX queue operations
        case VAXCompatOpType::INSQUE:
        case VAXCompatOpType::REMQUE:
            return performVAXQueue();

        // VAX atomic operations
        case VAXCompatOpType::ADAWI:
            return performVAXAtomic();

        default:
            return false;
        }
    }

    bool performVAXArithmetic()
    {
        m_vaxArithmeticCount.fetch_add(1, std::memory_order_relaxed);

        int32_t op1 = static_cast<int32_t>(m_operand1);
        int32_t op2 = static_cast<int32_t>(m_operand2);
        int64_t result64;

        switch (m_opType)
        {
        case VAXCompatOpType::ADDLV:
            result64 = static_cast<int64_t>(op1) + static_cast<int64_t>(op2);
            break;
        case VAXCompatOpType::SUBLV:
            result64 = static_cast<int64_t>(op1) - static_cast<int64_t>(op2);
            break;
        case VAXCompatOpType::MULLV:
            result64 = static_cast<int64_t>(op1) * static_cast<int64_t>(op2);
            break;
        case VAXCompatOpType::DIVLV:
            if (op2 == 0)
            {
                DEBUG_LOG("VAX divide by zero");
                return false;
            }
            result64 = static_cast<int64_t>(op1) / static_cast<int64_t>(op2);
            break;
        default:
            return false;
        }

        // Check for overflow
        if (result64 > 0x7FFFFFFF || result64 < -0x80000000LL)
        {
            m_overflowFlag = true;
            m_vaxOverflowCount.fetch_add(1, std::memory_order_relaxed);
            DEBUG_LOG("VAX arithmetic overflow detected");
            // In VAX, overflow typically causes a trap
            return false;
        }

        m_result = static_cast<uint64_t>(static_cast<int32_t>(result64));
        updateVAXConditionCodes(static_cast<int32_t>(result64));

        DEBUG_LOG("VAX arithmetic: %d op %d = %lld", op1, op2, result64);
        return true;
    }

    bool performVAXFloatingPoint()
    {
        m_vaxFloatCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified VAX floating point operations
        // In reality, would need to handle VAX-specific formats and rounding
        switch (m_opType)
        {
        case VAXCompatOpType::ADDF:
        case VAXCompatOpType::ADDD:
        case VAXCompatOpType::ADDG:
            m_floatResult = m_floatOperand1 + m_floatOperand2;
            break;
        case VAXCompatOpType::SUBF:
        case VAXCompatOpType::SUBD:
        case VAXCompatOpType::SUBG:
            m_floatResult = m_floatOperand1 - m_floatOperand2;
            break;
        case VAXCompatOpType::MULF:
        case VAXCompatOpType::MULD:
        case VAXCompatOpType::MULG:
            m_floatResult = m_floatOperand1 * m_floatOperand2;
            break;
        case VAXCompatOpType::DIVF:
        case VAXCompatOpType::DIVD:
        case VAXCompatOpType::DIVG:
            if (m_floatOperand2 == 0.0)
            {
                DEBUG_LOG("VAX floating point divide by zero");
                return false;
            }
            m_floatResult = m_floatOperand1 / m_floatOperand2;
            break;
        case VAXCompatOpType::NEGF:
        case VAXCompatOpType::NEGD:
        case VAXCompatOpType::NEGG:
            m_floatResult = -m_floatOperand1;
            break;
        case VAXCompatOpType::ABSF:
        case VAXCompatOpType::ABSD:
        case VAXCompatOpType::ABSG:
            m_floatResult = std::abs(m_floatOperand1);
            break;
        default:
            return false;
        }

        updateVAXFloatConditionCodes(m_floatResult);

        DEBUG_LOG("VAX floating point: %f op %f = %f", m_floatOperand1, m_floatOperand2, m_floatResult);
        return true;
    }

    bool performVAXConversion()
    {
        m_vaxConversionCount.fetch_add(1, std::memory_order_relaxed);
        m_formatConversionCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified VAX format conversions
        switch (m_opType)
        {
        case VAXCompatOpType::CVTFD:
        case VAXCompatOpType::CVTFG:
        case VAXCompatOpType::CVTDG:
            // Convert to higher precision (simplified)
            m_floatResult = m_floatOperand1;
            break;
        case VAXCompatOpType::CVTDF:
        case VAXCompatOpType::CVTGF:
        case VAXCompatOpType::CVTGD:
            // Convert to lower precision (simplified)
            m_floatResult = m_floatOperand1;
            break;
        case VAXCompatOpType::CVTFL:
        case VAXCompatOpType::CVTDL:
        case VAXCompatOpType::CVTGL:
            // Convert float to longword
            m_result = static_cast<uint64_t>(static_cast<int32_t>(m_floatOperand1));
            break;
        case VAXCompatOpType::CVTLF:
        case VAXCompatOpType::CVTLD:
        case VAXCompatOpType::CVTLG:
            // Convert longword to float
            m_floatResult = static_cast<double>(static_cast<int32_t>(m_operand1));
            break;
        default:
            return false;
        }

        DEBUG_LOG("VAX conversion performed");
        return true;
    }

    bool performVAXConditionCode()
    {
        m_vaxConditionCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case VAXCompatOpType::TSTL:
            updateVAXConditionCodes(static_cast<int32_t>(m_operand1));
            m_result = m_operand1;
            break;
        case VAXCompatOpType::TSTF:
        case VAXCompatOpType::TSTD:
        case VAXCompatOpType::TSTG:
            updateVAXFloatConditionCodes(m_floatOperand1);
            m_floatResult = m_floatOperand1;
            break;
        case VAXCompatOpType::CMPL:
        {
            int32_t op1 = static_cast<int32_t>(m_operand1);
            int32_t op2 = static_cast<int32_t>(m_operand2);
            int32_t result = op1 - op2;
            updateVAXConditionCodes(result);
            m_result = (result == 0) ? 0 : ((result < 0) ? -1 : 1);
        }
        break;
        case VAXCompatOpType::CMPF:
        case VAXCompatOpType::CMPD:
        case VAXCompatOpType::CMPG:
        {
            double result = m_floatOperand1 - m_floatOperand2;
            updateVAXFloatConditionCodes(result);
            m_floatResult = (result == 0.0) ? 0.0 : ((result < 0.0) ? -1.0 : 1.0);
        }
        break;
        default:
            return false;
        }

        DEBUG_LOG("VAX condition code operation performed");
        return true;
    }

    bool performVAXBitField()
    {
        m_vaxBitFieldCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified bit field operations
        uint8_t pos = static_cast<uint8_t>(m_operand2 & 0x3F);
        uint8_t size = static_cast<uint8_t>(m_operand3 & 0x3F);

        switch (m_opType)
        {
        case VAXCompatOpType::EXTV:
            // Extract field with sign extension
            {
                uint64_t mask = (1ULL << size) - 1;
                uint64_t field = (m_operand1 >> pos) & mask;
                if (field & (1ULL << (size - 1)))
                {
                    // Sign extend
                    field |= (~mask);
                }
                m_result = field;
            }
            break;
        case VAXCompatOpType::EXTZV:
            // Extract field with zero extension
            {
                uint64_t mask = (1ULL << size) - 1;
                m_result = (m_operand1 >> pos) & mask;
            }
            break;
        case VAXCompatOpType::INSV:
            // Insert field
            {
                uint64_t mask = (1ULL << size) - 1;
                uint64_t clearMask = ~(mask << pos);
                m_result = (m_operand1 & clearMask) | ((m_operand2 & mask) << pos);
            }
            break;
        case VAXCompatOpType::FFC:
            // Find first clear bit
            m_result = __builtin_ctzll(~m_operand1);
            break;
        case VAXCompatOpType::FFS:
            // Find first set bit
            m_result = __builtin_ctzll(m_operand1);
            break;
        default:
            return false;
        }

        DEBUG_LOG("VAX bit field operation performed");
        return true;
    }

    bool performVAXString()
    {
        m_vaxStringCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified string operations
        switch (m_opType)
        {
        case VAXCompatOpType::MOVC3:
        case VAXCompatOpType::MOVC5:
            DEBUG_LOG("VAX move character string: length=%d", m_stringLength);
            m_result = m_stringLength;
            break;
        case VAXCompatOpType::CMPC3:
        case VAXCompatOpType::CMPC5:
            DEBUG_LOG("VAX compare character string: length=%d", m_stringLength);
            m_result = 0; // Assume equal for simplification
            break;
        case VAXCompatOpType::LOCC:
        case VAXCompatOpType::SKPC:
        case VAXCompatOpType::SCANC:
        case VAXCompatOpType::SPANC:
            DEBUG_LOG("VAX character locate/scan operation");
            m_result = 0; // Character not found
            break;
        default:
            return false;
        }

        return true;
    }

    bool performVAXDecimal()
    {
        m_vaxDecimalCount.fetch_add(1, std::memory_order_relaxed);

        // Simplified packed decimal operations
        switch (m_opType)
        {
        case VAXCompatOpType::ADDP4:
        case VAXCompatOpType::ADDP6:
            DEBUG_LOG("VAX packed decimal add");
            break;
        case VAXCompatOpType::SUBP4:
        case VAXCompatOpType::SUBP6:
            DEBUG_LOG("VAX packed decimal subtract");
            break;
        case VAXCompatOpType::MULP:
            DEBUG_LOG("VAX packed decimal multiply");
            break;
        case VAXCompatOpType::DIVP:
            DEBUG_LOG("VAX packed decimal divide");
            break;
        case VAXCompatOpType::CVTLP:
        case VAXCompatOpType::CVTPL:
        case VAXCompatOpType::CVTPT:
        case VAXCompatOpType::CVTTP:
        case VAXCompatOpType::CVTPS:
        case VAXCompatOpType::CVTSP:
            DEBUG_LOG("VAX packed decimal conversion");
            break;
        case VAXCompatOpType::MOVP:
            DEBUG_LOG("VAX move packed decimal");
            break;
        case VAXCompatOpType::CMPP3:
        case VAXCompatOpType::CMPP4:
            DEBUG_LOG("VAX compare packed decimal");
            break;
        case VAXCompatOpType::ASHP:
            DEBUG_LOG("VAX arithmetic shift packed");
            break;
        case VAXCompatOpType::EDITPC:
            DEBUG_LOG("VAX edit packed to character");
            break;
        default:
            return false;
        }

        m_result = 0;
        return true;
    }

    bool performVAXAddress()
    {
        switch (m_opType)
        {
        case VAXCompatOpType::MOVA:
            m_result = m_operand1; // Move address
            DEBUG_LOG("VAX move address: 0x%016llX", m_result);
            break;
        case VAXCompatOpType::PUSHA:
            m_result = m_operand1; // Push address
            DEBUG_LOG("VAX push address: 0x%016llX", m_result);
            break;
        default:
            return false;
        }

        return true;
    }

    bool performVAXProcedure()
    {
        m_vaxProcedureCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case VAXCompatOpType::CALLS:
        case VAXCompatOpType::CALLG:
            DEBUG_LOG("VAX procedure call");
            break;
        case VAXCompatOpType::RET:
            DEBUG_LOG("VAX procedure return");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performVAXMiscellaneous()
    {
        switch (m_opType)
        {
        case VAXCompatOpType::HALT:
            DEBUG_LOG("VAX halt processor");
            break;
        case VAXCompatOpType::NOP:
            DEBUG_LOG("VAX no operation");
            break;
        case VAXCompatOpType::LDPCTX:
        case VAXCompatOpType::SVPCTX:
            DEBUG_LOG("VAX process context operation");
            break;
        case VAXCompatOpType::MTPR:
        case VAXCompatOpType::MFPR:
            DEBUG_LOG("VAX processor register operation");
            m_result = m_operand1;
            break;
        default:
            return false;
        }

        return true;
    }

    bool performVAXCRC()
    {
        // Simplified CRC calculation
        m_result = m_operand1 ^ m_operand2; // Very simplified
        DEBUG_LOG("VAX CRC calculation");
        return true;
    }

    bool performVAXQueue()
    {
        m_vaxQueueCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case VAXCompatOpType::INSQUE:
            DEBUG_LOG("VAX insert queue entry");
            break;
        case VAXCompatOpType::REMQUE:
            DEBUG_LOG("VAX remove queue entry");
            break;
        default:
            return false;
        }

        return true;
    }

    bool performVAXAtomic()
    {
        m_vaxAtomicCount.fetch_add(1, std::memory_order_relaxed);

        switch (m_opType)
        {
        case VAXCompatOpType::ADAWI:
            // Add aligned word interlocked
            m_result = m_operand1 + m_operand2;
            DEBUG_LOG("VAX atomic add word interlocked");
            break;
        default:
            return false;
        }

        return true;
    }

    void updateVAXConditionCodes(int32_t value)
    {
        m_conditionCodes = 0;
        m_zeroFlag = (value == 0);
        m_negativeFlag = (value < 0);
        m_overflowFlag = false; // Set by arithmetic operations
        m_carryFlag = false;    // Set by arithmetic operations

        if (m_zeroFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::Z);
        if (m_negativeFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::N);
        if (m_overflowFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::V);
        if (m_carryFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::C);
    }

    void updateVAXFloatConditionCodes(double value)
    {
        m_conditionCodes = 0;
        m_zeroFlag = (value == 0.0);
        m_negativeFlag = (value < 0.0);
        m_overflowFlag = std::isinf(value);
        m_carryFlag = false;

        if (m_zeroFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::Z);
        if (m_negativeFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::N);
        if (m_overflowFlag)
            m_conditionCodes |= static_cast<uint8_t>(VAXConditionCode::V);
    }

  private:
    mutable VAXCompatOpType m_opType;
     uint8_t m_destReg;
     uint8_t m_srcReg1;
     uint8_t m_srcReg2;
     uint8_t m_srcReg3;
     VAXDataType m_dataType;
     VAXFloatFormat m_vaxFloatFormat;

    uint64_t m_operand1;
    uint64_t m_operand2;
    uint64_t m_operand3;
    uint64_t m_result;
    double m_floatOperand1;
    double m_floatOperand2;
    double m_floatResult;
    uint8_t m_conditionCodes;
    bool m_overflowFlag;
    bool m_zeroFlag;
    bool m_negativeFlag;
    bool m_carryFlag;
    uint32_t m_stringLength;
    uint64_t m_stringAddress;
    uint32_t m_packedDecimalLength;

    std::atomic<uint64_t> m_vaxArithmeticCount;
    std::atomic<uint64_t> m_vaxFloatCount;
    std::atomic<uint64_t> m_vaxConversionCount;
    std::atomic<uint64_t> m_vaxStringCount;
    std::atomic<uint64_t> m_vaxDecimalCount;
    std::atomic<uint64_t> m_vaxBitFieldCount;
    std::atomic<uint64_t> m_vaxConditionCount;
    std::atomic<uint64_t> m_vaxOverflowCount;
    std::atomic<uint64_t> m_vaxProcedureCount;
    std::atomic<uint64_t> m_vaxQueueCount;
    std::atomic<uint64_t> m_vaxAtomicCount;
    std::atomic<uint64_t> m_formatConversionCount;

    // Prevent copying for performance
    alphaVAXCompatibilityInstruction(const alphaVAXCompatibilityInstruction &) = delete;
    alphaVAXCompatibilityInstruction &operator=(const alphaVAXCompatibilityInstruction &) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif


#if defined(_MSC_VER) && defined(_WIN64)
// Microsoft VS2022 host system architecture
#pragma pack(push, 8)
#endif

// Forward declarations for memory system integration

class alphaCacheCoherencyController;

// Hot Path Class - Optimized for atomic memory operations
class alphaLoadStoreConditionalInstruction : public alphaInstructionBase
{
  public:
    // Direct initialization for hot path performance
    explicit alphaLoadStoreConditionalInstruction() = default;
    ~alphaLoadStoreConditionalInstruction() = default;

    // Core pipeline methods
    void decode()
    {
        DEBUG_LOG("Decoding load-store conditional instruction opcode: 0x%08X", getOpcode());

        uint32_t opcode = getOpcode();

        // Extract instruction fields based on Alpha memory format
        uint8_t primaryOpcode = (opcode >> 26) & 0x3F;
        uint8_t ra = (opcode >> 21) & 0x1F;
        uint8_t rb = (opcode >> 16) & 0x1F;
        int16_t displacement = static_cast<int16_t>(opcode & 0xFFFF);

        // Determine operation type
        switch (primaryOpcode)
        {
        case 0x2A:
            m_operation = LSCOperation::LDL_L;
            m_accessSize = 4;
            m_memoryOrdering = MemoryOrdering::Acquire;
            break;
        case 0x2B:
            m_operation = LSCOperation::LDQ_L;
            m_accessSize = 8;
            m_memoryOrdering = MemoryOrdering::Acquire;
            break;
        case 0x2E:
            m_operation = LSCOperation::STL_C;
            m_accessSize = 4;
            m_memoryOrdering = MemoryOrdering::Release;
            break;
        case 0x2F:
            m_operation = LSCOperation::STQ_C;
            m_accessSize = 8;
            m_memoryOrdering = MemoryOrdering::Release;
            break;
        default:
            DEBUG_LOG("Unknown load-store conditional opcode: 0x%02X", primaryOpcode);
            m_operation = LSCOperation::None;
            break;
        }

        // Calculate effective address (will be refined during execution)
        m_effectiveAddress = static_cast<uint64_t>(displacement); // Base address will be added during execution

        // Initialize reservation state
        m_reservationState = ReservationState::None;
        m_reservationValid = false;

        // Check if access crosses cache line boundaries
        m_crossesCacheLine = ((m_effectiveAddress & 0x3F) + m_accessSize) > 64;

        // Determine if fast path can be used
        m_fastPath = !m_crossesCacheLine && (m_accessSize <= 8);

        DEBUG_LOG("Load-store conditional decoded - Operation: %s, Size: %d, FastPath: %s", getOperationName(),
                  m_accessSize, m_fastPath ? "Yes" : "No");
    }
    bool execute() override
    {
        // TODO
    }
    void writeback() override {
         //TODO
    }
    const char *typeName() const override { return "LoadStoreConditional"; }

    // Load-Locked/Store-Conditional operation types
    enum class LSCOperation : uint8_t
    {
        None = 0,  // Not an LSC operation
        LDL_L = 1, // Load Locked Longword (32-bit)
        LDQ_L = 2, // Load Locked Quadword (64-bit)
        STL_C = 3, // Store Conditional Longword (32-bit)
        STQ_C = 4  // Store Conditional Quadword (64-bit)
    };

    // Memory ordering semantics
    enum class MemoryOrdering : uint8_t
    {
        Relaxed = 0, // No ordering constraints
        Acquire = 1, // Acquire semantics (load-locked)
        Release = 2, // Release semantics (store-conditional)
        AcqRel = 3,  // Both acquire and release
        SeqCst = 4   // Sequential consistency
    };

    // Reservation state tracking
    enum class ReservationState : uint8_t
    {
        None = 0,        // No reservation
        Valid = 1,       // Valid reservation
        Invalidated = 2, // Reservation lost due to external write
        Expired = 3,     // Reservation expired (timeout)
        Conflict = 4     // Conflicting reservation detected
    };

  private:
    // Memory operation fields - aligned for cache efficiency
    alignas(8) LSCOperation m_operation{LSCOperation::None};
    MemoryOrdering m_memoryOrdering{MemoryOrdering::SeqCst};
    ReservationState m_reservationState{ReservationState::None};

    uint64_t m_effectiveAddress{0}; // Target memory address
    uint64_t m_loadedValue{0};      // Value loaded by load-locked
    uint64_t m_storeValue{0};       // Value to store by store-conditional
    uint32_t m_accessSize{0};       // 4 bytes (longword) or 8 bytes (quadword)

    // Reservation tracking
    uint64_t m_reservationAddress{0};   // Address of current reservation
    uint32_t m_reservationSize{0};      // Size of reserved region
    uint64_t m_reservationTimestamp{0}; // When reservation was established
    uint32_t m_reservationId{0};        // Unique reservation identifier
    uint8_t m_processorId{0};           // Processor that owns reservation

    // Cache coherency tracking
    bool m_isExclusiveAccess{false}; // Cache line in exclusive state
    bool m_reservationValid{true};   // Current reservation validity
    uint64_t m_cacheLineAddress{0};  // Aligned cache line address
    uint32_t m_coherencyState{0};    // MESI/MOESI state

    // Performance optimization flags
    bool m_fastPath{true};          // Can use optimized execution path
    bool m_needsBarrier{false};     // Requires memory barrier
    bool m_crossesCacheLine{false}; // Access spans multiple cache lines

  public:
    // Performance tracking - hot path optimized atomics
    static std::atomic<uint64_t> s_totalLoadLocked;
    static std::atomic<uint64_t> s_totalStoreConditional;
    static std::atomic<uint64_t> s_successfulStores;
    static std::atomic<uint64_t> s_failedStores;
    static std::atomic<uint64_t> s_reservationConflicts;
    static std::atomic<uint64_t> s_cacheLineEvictions;

    // Inline hot path methods for maximum performance
    inline bool isLoadLocked() const
    {
        return (m_operation == LSCOperation::LDL_L || m_operation == LSCOperation::LDQ_L);
    }

    inline bool isStoreConditional() const
    {
        return (m_operation == LSCOperation::STL_C || m_operation == LSCOperation::STQ_C);
    }

    inline bool isLongwordAccess() const
    {
        return (m_operation == LSCOperation::LDL_L || m_operation == LSCOperation::STL_C);
    }

    inline bool isQuadwordAccess() const
    {
        return (m_operation == LSCOperation::LDQ_L || m_operation == LSCOperation::STQ_C);
    }

    inline void recordLoadLocked() { s_totalLoadLocked.fetch_add(1, std::memory_order_relaxed); }

    inline void recordStoreConditional(bool success)
    {
        s_totalStoreConditional.fetch_add(1, std::memory_order_relaxed);
        if (success)
        {
            s_successfulStores.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            s_failedStores.fetch_add(1, std::memory_order_relaxed);
        }
    }

    inline void recordReservationConflict()
    {
        s_reservationConflicts.fetch_add(1, std::memory_order_relaxed);
        m_reservationState = ReservationState::Conflict;
    }

    inline double getStoreSuccessRate() const
    {
        uint64_t total = s_totalStoreConditional.load(std::memory_order_relaxed);
        return total > 0 ? static_cast<double>(s_successfulStores.load(std::memory_order_relaxed)) / total : 0.0;
    }

    // Reservation management
    inline bool establishReservation(uint64_t address, uint32_t size)
    {
        m_reservationAddress = alignToReservationGranularity(address);
        m_reservationSize = size;
        m_reservationTimestamp = getCurrentTimestamp();
        m_reservationId = generateReservationId();
        m_reservationState = ReservationState::Valid;
        m_reservationValid = true;

        return registerMemoryReservation(m_reservationAddress, m_reservationSize, m_reservationId);
    }

    inline bool checkReservation(uint64_t address, uint32_t size) const
    {
        if (!m_reservationValid || m_reservationState != ReservationState::Valid)
        {
            return false;
        }

        uint64_t alignedAddr = alignToReservationGranularity(address);
        return (alignedAddr == m_reservationAddress) && (size <= m_reservationSize) &&
               !isReservationConflicted(m_reservationId);
    }

    inline void clearReservation()
    {
        if (m_reservationValid)
        {
            unregisterMemoryReservation(m_reservationId);
            m_reservationValid = false;
            m_reservationState = ReservationState::None;
        }
    }

    // Cache coherency support
    inline void updateCoherencyState(uint32_t newState)
    {
        m_coherencyState = newState;

        // Check if reservation should be invalidated
        if (newState != 0x3 && newState != 0x2)
        { // Not Exclusive or Modified
            invalidateReservation();
        }
    }

    inline void invalidateReservation()
    {
        m_reservationValid = false;
        m_reservationState = ReservationState::Invalidated;
    }

    // Memory ordering enforcement
    inline void enforceMemoryOrdering()
    {
        switch (m_memoryOrdering)
        {
        case MemoryOrdering::Acquire:
            executeAcquireBarrier();
            break;
        case MemoryOrdering::Release:
            executeReleaseBarrier();
            break;
        case MemoryOrdering::AcqRel:
            executeAcquireBarrier();
            executeReleaseBarrier();
            break;
        case MemoryOrdering::SeqCst:
            executeFullMemoryBarrier();
            break;
        case MemoryOrdering::Relaxed:
        default:
            // No barrier needed
            break;
        }
    }

    // Accessors for monitoring and debugging
    inline LSCOperation getOperation() const { return m_operation; }
    inline MemoryOrdering getMemoryOrdering() const { return m_memoryOrdering; }
    inline ReservationState getReservationState() const { return m_reservationState; }
    inline uint64_t getEffectiveAddress() const { return m_effectiveAddress; }
    inline uint64_t getReservationAddress() const { return m_reservationAddress; }
    inline bool hasValidReservation() const { return m_reservationValid; }
    inline uint32_t getAccessSize() const { return m_accessSize; }

    // String conversion for debugging (cold path)
    const char *getOperationName() const;
    const char *getMemoryOrderingName() const;
    const char *getReservationStateName() const;

    // Advanced memory management methods
    bool executeLoadLocked();
    bool executeStoreConditional();
    void setupMemoryAccess(uint64_t address, uint32_t size, LSCOperation op);
    bool validateMemoryAlignment() const;
    void handleCacheLineEviction(uint64_t evictedAddress);

  private:
    // Internal helper methods - optimized for hot path
    inline uint64_t alignToReservationGranularity(uint64_t address) const
    {
        // Align to cache line boundary (typically 64 bytes)
        return address & ~0x3F;
    }

    inline uint64_t alignToCacheLine(uint64_t address) const
    {
        return address & ~0x3F; // 64-byte cache line alignment
    }

    inline uint32_t generateReservationId() const
    {
        return (static_cast<uint32_t>(m_pc >> 2) ^ static_cast<uint32_t>(getCurrentTimestamp())) & 0xFFFFFF;
    }

    inline uint64_t getCurrentTimestamp() const
    {
        return getCycleCounter(); // Implementation-specific cycle counter
    }

    inline bool isReservationExpired() const
    {
        uint64_t currentTime = getCurrentTimestamp();
        uint64_t maxAge = 10000; // Maximum reservation age in cycles
        return (currentTime - m_reservationTimestamp) > maxAge;
    }

    // Cache coherency protocol support
    inline void requestExclusiveAccess(uint64_t address)
    {
        m_cacheLineAddress = alignToCacheLine(address);
        m_isExclusiveAccess = requestCacheLineExclusive(m_cacheLineAddress);
    }

    inline void releaseExclusiveAccess()
    {
        if (m_isExclusiveAccess)
        {
            releaseCacheLineExclusive(m_cacheLineAddress);
            m_isExclusiveAccess = false;
        }
    }

    // Memory system interface methods (implemented elsewhere)
    bool registerMemoryReservation(uint64_t address, uint32_t size, uint32_t id);
    void unregisterMemoryReservation(uint32_t id);
    bool isReservationConflicted(uint32_t id) const;
    bool requestCacheLineExclusive(uint64_t cacheLineAddr);
    void releaseCacheLineExclusive(uint64_t cacheLineAddr);

    // Memory barrier implementations
    void executeAcquireBarrier();
    void executeReleaseBarrier();
    void executeFullMemoryBarrier();

    // System interface methods
    uint64_t getCycleCounter() const;
    uint8_t getCurrentProcessorId() const;

    // Alpha-specific instruction decoding
    void decodeLoadLocked();
    void decodeStoreConditional();
    void calculateEffectiveAddress();

    // Performance optimization methods
    bool canUseFastPath() const;
    void optimizeForSequentialAccess();
    void handleSlowPath();
};

// Memory Reservation Table for tracking system-wide reservations
class alphaMemoryReservationTable
{
  public:
    struct ReservationEntry
    {
        uint64_t address;
        uint32_t size;
        uint32_t reservationId;
        uint8_t processorId;
        uint64_t timestamp;
        bool valid;
    };

    static alphaMemoryReservationTable &instance();

    bool addReservation(uint64_t address, uint32_t size, uint32_t id, uint8_t processorId);
    void removeReservation(uint32_t id);
    bool checkConflict(uint64_t address, uint32_t size, uint32_t excludeId = 0) const;
    void invalidateReservationsAt(uint64_t address, uint32_t size);
    void clearExpiredReservations();

    // Statistics
    inline size_t getActiveReservations() const { return m_activeCount.load(std::memory_order_relaxed); }
    inline uint64_t getTotalConflicts() const { return m_totalConflicts.load(std::memory_order_relaxed); }

  private:
    static constexpr size_t MAX_RESERVATIONS = 256;
    ReservationEntry m_reservations[MAX_RESERVATIONS];
    std::atomic<size_t> m_activeCount{0};
    std::atomic<uint64_t> m_totalConflicts{0};
    mutable std::atomic_flag m_tableLock = ATOMIC_FLAG_INIT;

    alphaMemoryReservationTable() = default;

    inline void acquireTableLock() const
    {
        while (m_tableLock.test_and_set(std::memory_order_acquire))
        {
            // Busy wait with pause for better performance
            _mm_pause();
        }
    }

    inline void releaseTableLock() const { m_tableLock.clear(std::memory_order_release); }
};

// Static member initialization
std::atomic<uint64_t> alphaLoadStoreConditionalInstruction::s_totalLoadLocked{0};
std::atomic<uint64_t> alphaLoadStoreConditionalInstruction::s_totalStoreConditional{0};
std::atomic<uint64_t> alphaLoadStoreConditionalInstruction::s_successfulStores{0};
std::atomic<uint64_t> alphaLoadStoreConditionalInstruction::s_failedStores{0};
std::atomic<uint64_t> alphaLoadStoreConditionalInstruction::s_reservationConflicts{0};
std::atomic<uint64_t> alphaLoadStoreConditionalInstruction::s_cacheLineEvictions{0};

#if defined(_MSC_VER) && defined(_WIN64)
#pragma pack(pop)
#endif

#if defined(_MSC_VER) && defined(_WIN64)
// Microsoft VS2022 host system architecture
#pragma pack(push, 8)
#endif

// Forward declarations for memory system integration


// Hot Path Class - Optimized for unaligned memory operations
class alphaUnalignedMemoryInstruction : public alphaInstructionBase
{
  public:
    // Direct initialization for hot path performance
    explicit alphaUnalignedMemoryInstruction() = default;
    ~alphaUnalignedMemoryInstruction() = default;

    // Core pipeline methods
    void decode() override;
    bool execute() override;
    void writeback() override;
    const char *typeName() const override { return "UnalignedMemory"; }

    // Unaligned memory operation types
    enum class UnalignedOperation : uint8_t
    {
        None = 0, // Not an unaligned operation
        LDQU = 1, // Load Quadword Unaligned
        STQ_U = 2 // Store Quadword Unaligned
    };

    // Access pattern types for optimization
    enum class AccessPattern : uint8_t
    {
        Unknown = 0,    // Unknown access pattern
        Sequential = 1, // Sequential unaligned accesses
        Strided = 2,    // Regular stride pattern
        Random = 3,     // Random unaligned accesses
        Packed = 4,     // Packed structure access
        Streaming = 5   // Streaming/bulk data access
    };

    // Alignment characteristics
    enum class AlignmentType : uint8_t
    {
        Aligned = 0,     // Actually aligned (8-byte boundary)
        Misaligned1 = 1, // 1-byte misalignment
        Misaligned2 = 2, // 2-byte misalignment
        Misaligned3 = 3, // 3-byte misalignment
        Misaligned4 = 4, // 4-byte misalignment
        Misaligned5 = 5, // 5-byte misalignment
        Misaligned6 = 6, // 6-byte misalignment
        Misaligned7 = 7  // 7-byte misalignment
    };

    // Execution strategy for unaligned access
    enum class ExecutionStrategy : uint8_t
    {
        Auto = 0,           // Automatically determine best strategy
        SingleAccess = 1,   // Single unaligned memory operation
        DualAccess = 2,     // Two aligned accesses with masking
        ByteWise = 3,       // Byte-by-byte access for complex cases
        CacheOptimized = 4, // Cache-line-aware strategy
        Vectorized = 5      // Use vector operations if available
    };

  private:
    // Memory operation fields - aligned for cache efficiency
    alignas(8) UnalignedOperation m_operation{UnalignedOperation::None};
    AccessPattern m_accessPattern{AccessPattern::Unknown};
    AlignmentType m_alignmentType{AlignmentType::Aligned};
    ExecutionStrategy m_strategy{ExecutionStrategy::Auto};

    uint64_t m_effectiveAddress{0}; // Target memory address (unaligned)
    uint64_t m_alignedAddress{0};   // Aligned base address
    uint64_t m_dataValue{0};        // 64-bit data value
    uint8_t m_alignmentOffset{0};   // Offset from aligned boundary (0-7)

    // Cache line information
    uint64_t m_primaryCacheLine{0};   // Primary cache line address
    uint64_t m_secondaryCacheLine{0}; // Secondary cache line (if crossing)
    bool m_crossesCacheLine{false};   // Access spans two cache lines
    uint8_t m_cacheLineOffset{0};     // Offset within primary cache line

    // Performance optimization data
    bool m_fastPath{true};               // Can use optimized execution
    bool m_needsByteSwap{false};         // Requires endianness conversion
    bool m_canUseHardwareSupport{false}; // Hardware unaligned support available
    uint32_t m_accessStride{0};          // Detected stride pattern

    // Masking and shifting for dual-access strategy
    uint64_t m_lowMask{0};  // Mask for low-order bytes
    uint64_t m_highMask{0}; // Mask for high-order bytes
    uint8_t m_lowShift{0};  // Shift count for low portion
    uint8_t m_highShift{0}; // Shift count for high portion

  public:
    // Performance tracking - hot path optimized atomics
    static std::atomic<uint64_t> s_totalUnalignedLoads;
    static std::atomic<uint64_t> s_totalUnalignedStores;
    static std::atomic<uint64_t> s_cacheLineCrossings;
    static std::atomic<uint64_t> s_fastPathExecutions;
    static std::atomic<uint64_t> s_slowPathExecutions;
    static std::atomic<uint64_t> s_byteWiseAccesses;
    static std::atomic<uint64_t> s_hardwareAssisted;

    // Inline hot path methods for maximum performance
    inline bool isLoadOperation() const { return m_operation == UnalignedOperation::LDQU; }

    inline bool isStoreOperation() const { return m_operation == UnalignedOperation::STQ_U; }

    inline bool isActuallyAligned() const { return m_alignmentType == AlignmentType::Aligned; }

    inline bool requiresDualAccess() const
    {
        return m_crossesCacheLine || (m_alignmentOffset != 0 && !m_canUseHardwareSupport);
    }

    inline void recordUnalignedAccess()
    {
        if (isLoadOperation())
        {
            s_totalUnalignedLoads.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            s_totalUnalignedStores.fetch_add(1, std::memory_order_relaxed);
        }

        if (m_crossesCacheLine)
        {
            s_cacheLineCrossings.fetch_add(1, std::memory_order_relaxed);
        }
    }

    inline void recordExecutionPath()
    {
        if (m_fastPath)
        {
            s_fastPathExecutions.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            s_slowPathExecutions.fetch_add(1, std::memory_order_relaxed);
        }

        if (m_strategy == ExecutionStrategy::ByteWise)
        {
            s_byteWiseAccesses.fetch_add(1, std::memory_order_relaxed);
        }

        if (m_canUseHardwareSupport)
        {
            s_hardwareAssisted.fetch_add(1, std::memory_order_relaxed);
        }
    }

    inline uint8_t calculateAlignmentOffset(uint64_t address) const { return static_cast<uint8_t>(address & 0x7); }

    inline uint64_t alignToQuadword(uint64_t address) const { return address & ~0x7ULL; }

    inline uint64_t alignToCacheLine(uint64_t address) const
    {
        return address & ~0x3FULL; // 64-byte cache line alignment
    }

    inline bool spansCacheLines(uint64_t address) const
    {
        return alignToCacheLine(address) != alignToCacheLine(address + 7);
    }

    inline double getUnalignedAccessRatio() const
    {
        uint64_t total = s_totalUnalignedLoads.load(std::memory_order_relaxed) +
                         s_totalUnalignedStores.load(std::memory_order_relaxed);
        uint64_t crossings = s_cacheLineCrossings.load(std::memory_order_relaxed);
        return total > 0 ? static_cast<double>(crossings) / total : 0.0;
    }

    // Access pattern detection and optimization
    inline void updateAccessPattern(uint64_t currentAddress, uint64_t previousAddress)
    {
        if (previousAddress != 0)
        {
            int64_t stride = static_cast<int64_t>(currentAddress) - static_cast<int64_t>(previousAddress);

            if (stride == 8)
            {
                m_accessPattern = AccessPattern::Sequential;
            }
            else if (stride > 0 && stride < 256 && (stride % 8) != 0)
            {
                m_accessPattern = AccessPattern::Strided;
                m_accessStride = static_cast<uint32_t>(stride);
            }
            else if (stride == 0)
            {
                m_accessPattern = AccessPattern::Packed;
            }
            else
            {
                m_accessPattern = AccessPattern::Random;
            }
        }
    }

    inline void optimizeForPattern()
    {
        switch (m_accessPattern)
        {
        case AccessPattern::Sequential:
        case AccessPattern::Streaming:
            m_strategy = ExecutionStrategy::CacheOptimized;
            enablePrefetching();
            break;

        case AccessPattern::Strided:
            if (m_accessStride <= 16)
            {
                m_strategy = ExecutionStrategy::CacheOptimized;
            }
            else
            {
                m_strategy = ExecutionStrategy::Auto;
            }
            break;

        case AccessPattern::Packed:
            m_strategy = ExecutionStrategy::DualAccess;
            break;

        case AccessPattern::Random:
        default:
            m_strategy = ExecutionStrategy::Auto;
            break;
        }
    }

    // Masking and shifting calculations for dual-access strategy
    inline void calculateAccessMasks()
    {
        if (m_alignmentOffset == 0)
        {
            // Actually aligned - single access
            m_lowMask = 0xFFFFFFFFFFFFFFFFULL;
            m_highMask = 0;
            m_lowShift = 0;
            m_highShift = 0;
        }
        else
        {
            // Unaligned - dual access required
            uint8_t bytesInLow = 8 - m_alignmentOffset;
            uint8_t bytesInHigh = m_alignmentOffset;

            m_lowMask = (1ULL << (bytesInLow * 8)) - 1;
            m_highMask = (1ULL << (bytesInHigh * 8)) - 1;
            m_lowShift = m_alignmentOffset * 8;
            m_highShift = bytesInLow * 8;
        }
    }

    // Accessors for monitoring and debugging
    inline UnalignedOperation getOperation() const { return m_operation; }
    inline AccessPattern getAccessPattern() const { return m_accessPattern; }
    inline AlignmentType getAlignmentType() const { return m_alignmentType; }
    inline ExecutionStrategy getStrategy() const { return m_strategy; }
    inline uint64_t getEffectiveAddress() const { return m_effectiveAddress; }
    inline uint8_t getAlignmentOffset() const { return m_alignmentOffset; }
    inline bool crossesCacheLines() const { return m_crossesCacheLine; }

    // String conversion for debugging (cold path)
    const char *getOperationName() const;
    const char *getAccessPatternName() const;
    const char *getAlignmentTypeName() const;
    const char *getExecutionStrategyName() const;

    // Advanced memory management methods
    bool executeUnalignedLoad();
    bool executeUnalignedStore();
    void setupUnalignedAccess(uint64_t address, UnalignedOperation op);
    void detectHardwareSupport();
    void enablePrefetching();

  private:
    // Internal helper methods - optimized for hot path
    inline void classifyAlignment(uint64_t address)
    {
        m_alignmentOffset = calculateAlignmentOffset(address);
        m_alignmentType = static_cast<AlignmentType>(m_alignmentOffset);
        m_alignedAddress = alignToQuadword(address);
    }

    inline void analyzeCacheLineAccess(uint64_t address)
    {
        m_primaryCacheLine = alignToCacheLine(address);
        m_crossesCacheLine = spansCacheLines(address);
        m_cacheLineOffset = static_cast<uint8_t>(address & 0x3F);

        if (m_crossesCacheLine)
        {
            m_secondaryCacheLine = alignToCacheLine(address + 7);
        }
    }

    inline void determineExecutionStrategy()
    {
        if (m_strategy == ExecutionStrategy::Auto)
        {
            if (isActuallyAligned())
            {
                m_strategy = ExecutionStrategy::SingleAccess;
            }
            else if (m_canUseHardwareSupport)
            {
                m_strategy = ExecutionStrategy::SingleAccess;
            }
            else if (m_crossesCacheLine)
            {
                m_strategy = ExecutionStrategy::DualAccess;
            }
            else if (m_alignmentOffset <= 4)
            {
                m_strategy = ExecutionStrategy::DualAccess;
            }
            else
            {
                m_strategy = ExecutionStrategy::ByteWise;
            }
        }
    }

    inline bool canUseFastPath() const
    {
        return (m_strategy == ExecutionStrategy::SingleAccess) ||
               (m_strategy == ExecutionStrategy::DualAccess && !m_crossesCacheLine) ||
               (m_canUseHardwareSupport && m_alignmentOffset <= 4);
    }

    // Execution strategy implementations
    bool executeSingleAccess();
    bool executeDualAccess();
    bool executeByteWise();
    bool executeCacheOptimized();

    // Memory access primitives
    bool performAlignedLoad(uint64_t address, uint64_t *value);
    bool performAlignedStore(uint64_t address, uint64_t value);
    bool performUnalignedLoadHardware(uint64_t address, uint64_t *value);
    bool performUnalignedStoreHardware(uint64_t address, uint64_t value);

    // Byte manipulation helpers
    uint64_t assembleFromBytes(const uint8_t *bytes) const;
    void disassembleToBytes(uint64_t value, uint8_t *bytes) const;
    uint64_t combineAlignedAccesses(uint64_t lowValue, uint64_t highValue) const;
    void splitForAlignedAccesses(uint64_t value, uint64_t *lowValue, uint64_t *highValue) const;

    // Cache and prefetch management
    void prefetchNextAccess();
    void invalidateCacheIfNeeded();
    bool isCacheLineResident(uint64_t cacheLineAddr) const;

    // Hardware support detection
    bool detectUnalignedLoadSupport() const;
    bool detectUnalignedStoreSupport() const;
    bool hasVectorSupport() const;

    // Alpha-specific instruction decoding
    void decodeUnalignedLoad();
    void decodeUnalignedStore();
    void calculateEffectiveAddress();

    // Performance optimization
    void optimizeForSequentialAccess();
    void optimizeForStridedAccess();
    void tuneForWorkload();
};

// Unaligned Access Pattern Tracker for performance optimization
class alphaUnalignedAccessTracker
{
  public:
    struct AccessHistoryEntry
    {
        uint64_t address;
        uint64_t timestamp;
        uint8_t alignmentOffset;
        bool wasOptimized;
    };

    static alphaUnalignedAccessTracker &instance();

    void recordAccess(uint64_t address, uint8_t alignmentOffset, bool optimized);
    AccessPattern detectPattern(uint64_t address) const;
    void updateOptimizationHints(uint64_t address, ExecutionStrategy strategy);
    bool shouldOptimizeForAddress(uint64_t address) const;

    // Statistics and tuning
    double getOptimizationEffectiveness() const;
    void clearHistory();
    size_t getHistorySize() const { return m_historyIndex.load(std::memory_order_relaxed); }

  private:
    static constexpr size_t MAX_HISTORY = 1024;
    AccessHistoryEntry m_history[MAX_HISTORY];
    std::atomic<size_t> m_historyIndex{0};
    std::atomic<uint64_t> m_totalAccesses{0};
    std::atomic<uint64_t> m_optimizedAccesses{0};

    alphaUnalignedAccessTracker() = default;

    inline size_t getNextIndex() { return m_historyIndex.fetch_add(1, std::memory_order_relaxed) % MAX_HISTORY; }
};

// Static member initialization
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_totalUnalignedLoads{0};
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_totalUnalignedStores{0};
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_cacheLineCrossings{0};
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_fastPathExecutions{0};
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_slowPathExecutions{0};
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_byteWiseAccesses{0};
std::atomic<uint64_t> alphaUnalignedMemoryInstruction::s_hardwareAssisted{0};

#if defined(_MSC_VER) && defined(_WIN64)
#pragma pack(pop)
#endif



#endif // ALPHAINSTRUCTIONBASE_H


