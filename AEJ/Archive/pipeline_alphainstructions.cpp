// Assembler.h
#pragma once
#ifndef ASSEMBLER_H_
#define ASSEMBLER_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file Assembler.h
 * @brief Header-only JIT emission DSL for translating Alpha AXP instructions
 *        into x86-64 machine code. Provides inline helpers for integer,
 *        memory, branch, address (LDA/LDAH), and floating-point operations.
 * @note  Entirely emulator-specific; not part of VS2022 or any standard library.
 * @see   Alpha AXP System Reference Manual, Version 6
 */

//-----------------------------------------------------------------------------
// HostReg: x86-64 physical registers available to JIT emission.
//   - RAX, RBX, RCX, RDX, RSI, RDI, RSP, RBP: standard integer registers.
//   - XMM0–XMM3: SSE2 registers for FP.
//   - GPR_BASE: pointer to the Alpha CPU's general-register array in memory.
enum class HostReg : uint8_t
{
    RAX,     ///< Scratch & accumulator register for address/ALU results
    RBX,     ///< Callee-saved register; can hold long-lived pointers
    RCX,     ///< 1st integer argument (SysV ABI); used for call targets
    RDX,     ///< 2nd integer argument; used as temp for loads
    RSI,     ///< Scratch register
    RDI,     ///< 3rd integer argument; used for pointer parameters
    RSP,     ///< Stack pointer (must be maintained properly)
    RBP,     ///< Frame/base pointer (optional use)
    XMM0,    ///< SSE2 FP register for double-precision ops
    XMM1,    ///< SSE2 FP register
    XMM2,    ///< SSE2 FP register
    XMM3,    ///< SSE2 FP register
    GPR_BASE ///< Base pointer to Alpha CPU GPR array in host memory
};

// Condition codes for conditional branches (0F 8x / 0F 8x opcodes)
enum class Condition : uint8_t
{
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE
};

/**
 * @class Assembler
 * @brief Gathers x86-64 machine-code bytes into an internal buffer,
 *        manages labels and fixups, and provides inline methods to
 *        emit various instruction patterns.
 */
class Assembler
{
  public:
    Assembler() = default;
    ~Assembler() = default;

    // Access generated code
    inline uint8_t *codePtr() { return codeBuffer.data(); }
    inline size_t codeSize() const { return codeBuffer.size(); }

    /**
     * @brief Define a label at current offset, back-patching fixups.
     */
    inline void bindLabel(const std::string &label)
    {
        size_t pos = codeSize();
        labels[label] = pos;
        auto it = fixups.find(label);
        if (it != fixups.end())
        {
            for (auto off : it->second)
            {
                int32_t disp = int32_t(pos - (off + 4));
                uint8_t *p = codeBuffer.data() + off;
                p[0] = uint8_t(disp);
                p[1] = uint8_t(disp >> 8);
                p[2] = uint8_t(disp >> 16);
                p[3] = uint8_t(disp >> 24);
            }
            fixups.erase(it);
        }
    }

    /**
     * @brief Reserve 4 bytes for a label reference (branch displacement).
     */
    inline void emitLabelRef(const std::string &label)
    {
        size_t pos = codeSize();
        codeBuffer.insert(codeBuffer.end(), 4, 0);
        fixups[label].push_back(pos);
    }

    //---------------------------------
    // Integer/Memory Helpers
    //---------------------------------

    /**
     * @brief MOV dstReg, [GPR_BASE + srcIndex*8]
     */
    inline void emitMovRegReg(HostReg dst, HostReg srcBase, uint8_t srcIndex)
    {
        codeBuffer.push_back(0x48); // REX.W
        codeBuffer.push_back(0x8B); // MOV r64, r/m64
        // TODO: full ModRM/SIB: mod=10, rm=srcBase, index=srcIndex, scale=8
    }

    /**
     * @brief ADD dstReg, imm32
     */
    inline void emitAddRegImm(HostReg dst, int32_t imm)
    {
        codeBuffer.push_back(0x48); // REX.W
        codeBuffer.push_back(0x81); // ADD r/m64, imm32
        codeBuffer.push_back(0xC0 | uint8_t(dst));
        append(reinterpret_cast<const uint8_t *>(&imm), 4);
    }

    /**
     * @brief MOV [GPR_BASE + destIndex*8], srcReg
     */
    inline void emitStoreRegMem(HostReg src, HostReg base, uint8_t destIndex, uint8_t /*bits*/)
    {
        codeBuffer.push_back(0x48); // REX.W
        codeBuffer.push_back(0x89); // MOV r/m64, r64
        // TODO: ModRM/SIB for [base + destIndex*8]
    }

    //---------------------------------
    // Address Operations (LDA, LDAH)
    //---------------------------------

    /**
     * @brief LDA: R[ra] = R[rb] + sext(disp)
     */
    inline void emitLda(uint8_t ra, uint8_t rb, int32_t disp)
    {
        emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, rb);
        emitAddRegImm(HostReg::RAX, disp);
        emitStoreRegMem(HostReg::RAX, HostReg::GPR_BASE, ra, 64);
    }

    /**
     * @brief LDAH: R[ra] = R[rb] + (disp << 16)
     */
    inline void emitLdah(uint8_t ra, uint8_t rb, int16_t disp)
    {
        emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, rb);
        emitAddRegImm(HostReg::RAX, int32_t(disp) << 16);
        emitStoreRegMem(HostReg::RAX, HostReg::GPR_BASE, ra, 64);
    }

    //---------------------------------
    // Big-endian Data Helpers
    //---------------------------------

    /**
     * @brief Retrieve a big-endian 16-bit value from a byte buffer.
     */
    static inline uint16_t getBigEndian16(const uint8_t *p) { return (uint16_t(p[0]) << 8) | uint16_t(p[1]); }

    /**
     * @brief Retrieve a big-endian 32-bit value from a byte buffer.
     */
    static inline uint32_t getBigEndian32(const uint8_t *p)
    {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    }

    /**
     * @brief Retrieve a big-endian 64-bit value from a byte buffer.
     */
    static inline uint64_t getBigEndian64(const uint8_t *p)
    {
        return (uint64_t(p[0]) << 56) | (uint64_t(p[1]) << 48) | (uint64_t(p[2]) << 40) | (uint64_t(p[3]) << 32) |
               (uint64_t(p[4]) << 24) | (uint64_t(p[5]) << 16) | (uint64_t(p[6]) << 8) | uint64_t(p[7]);
    }

    //---------------------------------
    // Conditional Branch Helpers
    //---------------------------------

    inline void emitJmp(const std::string &label)
    {
        codeBuffer.push_back(0xE9); // JMP rel32
        emitLabelRef(label);
    }
    inline void emitJcc(Condition cc, const std::string &label)
    {
        static const uint8_t opMap[6] = {0x84, 0x85, 0x8C, 0x8E, 0x8F, 0x8D};
        codeBuffer.push_back(0x0F);
        codeBuffer.push_back(opMap[int(cc)]);
        emitLabelRef(label);
    }

    //---------------------------------
    // Floating-Point (SSE2) Helpers
    //---------------------------------

    inline void emitMovsdRegMem(HostReg dstXmm, HostReg base, int32_t disp)
    {
        codeBuffer.push_back(0xF2);
        codeBuffer.push_back(0x0F);
        codeBuffer.push_back(0x10);
        // TODO: ModRM/SIB for [base + disp]
    }
    inline void emitMovsdMemReg(HostReg base, int32_t disp, HostReg srcXmm)
    {
        codeBuffer.push_back(0xF2);
        codeBuffer.push_back(0x0F);
        codeBuffer.push_back(0x11);
        // TODO: ModRM/SIB
    }
    inline void emitAddsd(HostReg dstXmm, HostReg srcXmm)
    {
        codeBuffer.push_back(0xF2);
        codeBuffer.push_back(0x0F);
        codeBuffer.push_back(0x58);
        // TODO: ModRM
    }
    inline void emitSubsd(HostReg dstXmm, HostReg srcXmm)
    {
        codeBuffer.push_back(0xF2);
        codeBuffer.push_back(0x0F);
        codeBuffer.push_back(0x5C);
    }

  private:
    std::vector<uint8_t> codeBuffer;
    std::unordered_map<std::string, size_t> labels;
    std::unordered_map<std::string, std::vector<size_t>> fixups;
    inline void append(const uint8_t *data, size_t len) { codeBuffer.insert(codeBuffer.end(), data, data + len); }
};

#endif // ASSEMBLER_H_
