// Assembler.h
#pragma once
#ifndef ASSEMBLER_H_
#define ASSEMBLER_H_

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include "extensions/assemblerBase.h"
#include <QVector>
#include <QMap>

namespace encodingHeader {
	static constexpr uint32_t SPR_EXCEPTION_SUMMARY = 0x11;
	static constexpr uint32_t SPR_SOFTWARE_INTERRUPT = 0x12;  // <– replace 0x12 with the canonical value from your ASA reference
	/// Machine-Check Error Summary SPR (see Alpha Architecture Ref. Man. §13.3.9)
	static constexpr uint32_t SPR_MACHINE_CHECK_SUMMARY = /* canonical SPR number from ASA */;

	// primary?opcode field is bits [31:26], so mask & shift values go here
	static constexpr uint32_t OPCODE_MTPR = 0x1E;  // the hex value per the Alpha AXP ISA
	static constexpr uint32_t OPCODE_MFPR = 0x1F;  // for the companion "move from PR" instr

	static constexpr uint32_t OPCODE_BITS = 6;
	/// Where in the instruction word does the primary opcode live?
	static constexpr uint32_t OPCODE_SHIFT = 26;
	/// Mask for a 6-bit field
	static constexpr uint32_t OPCODE_MASK = (1u << OPCODE_BITS) - 1;

	// Helpers
	/// Place a 6-bit opcode into bits 31:26
	static constexpr uint32_t OP(uint32_t code) {
		return (code & OPCODE_MASK) << OPCODE_SHIFT;
	}
}
namespace assemblerSpace {



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
	enum class HostReg : uint8_t {
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


	// Encode ModR/M byte for register-to-register SSE: mod=11b, reg=dst, rm=src
	inline uint8_t modRm(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7));
	}

	/**
	 * @class Assembler
	 * @brief Gathers x86-64 machine-code bytes into an internal buffer,
	 *        manages labels and fixups, and provides inline methods to
	 *        emit various instruction patterns.
	 *
	 * Typical sequence for LDA (alpha):
	 *   // load RAX = GPR[rb]
	 *   as.emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, rbIndex);
	 *   // add immediate disp
	 *   as.emitAddRegImm(HostReg::RAX, disp);
	 *   // store result back to GPR[ra]
	 *   as.emitStoreRegMem(HostReg::RAX, HostReg::GPR_BASE, raIndex, 64);
	 */
	class Assembler : public AssemblerBase {

		QMap<QString, QVector<size_t>> labels;
	public:

		Assembler() = default;
		~Assembler() = default;

		// Access code buffer
		inline uint8_t* codePtr() { return codeBuffer.data(); }
		inline size_t codeSize() const { return codeBuffer.size(); }

		/**
		 * @brief Bind a label at current offset, back-patching fixups.
		 */
		inline void bindLabel(const std::string& label) {
			size_t pos = codeSize();
			labels[label] = pos;
			auto it = fixups.find(label);
			if (it != fixups.end()) {
				for (auto off : it->second) {
					int32_t disp = int32_t(pos - (off + 4));
					uint8_t* p = codeBuffer.data() + off;
					p[0] = uint8_t(disp);
					p[1] = uint8_t(disp >> 8);
					p[2] = uint8_t(disp >> 16);
					p[3] = uint8_t(disp >> 24);
				}
				fixups.erase(it);
			}
		}

		/**
		* How it works:
		* 0x66 is the mandatory prefix for vertical SSE2 packed-byte ops (MOVDQA, PCMPEQB, etc.).
		* 0x0F is the two-byte escape.
		* The third byte is the SSE2 opcode from Volume 2A.
		* modRm(dst,src) builds the ModR/M register-direct byte (mod=11, reg=dst, rm=src).
		*
		* MOVDQA dstXmm, srcXmm
		*   66 0F 6F /r
		*   copy 128-bit XMM register
		*/
		inline void movdqa(int dstXmm, int srcXmm) {
			emitByte(0x66);                // 66 = operand-size override
			emitByte(0x0F);
			emitByte(0x6F);                // MOVDQA opcode
			emitByte(modRm(dstXmm, srcXmm));
		}

		/**
		 * PCMPEQB dstXmm, srcXmm
		 *   66 0F 74 /r
		 *   compare packed bytes for equality
		 */
		inline void pcmpeqb(int dstXmm, int srcXmm) {
			emitByte(0x66);
			emitByte(0x0F);
			emitByte(0x74);                // PCMPEQB opcode
			emitByte(modRm(dstXmm, srcXmm));
		}

		/**
		 * PCMPGTB dstXmm, srcXmm
		 *   66 0F 64 /r
		 *   compare packed signed bytes for greater-than
		 */
		inline void pcmpgtb(int dstXmm, int srcXmm) {
			emitByte(0x66);
			emitByte(0x0F);
			emitByte(0x64);                // PCMPGTB opcode
			emitByte(modRm(dstXmm, srcXmm));
		}

		/**
		 * POR dstXmm, srcXmm
		 *   66 0F EB /r
		 *   bitwise OR of packed bytes
		 */
		inline void por(int dstXmm, int srcXmm) {
			emitByte(0x66);
			emitByte(0x0F);
			emitByte(0xEB);                // POR opcode
			emitByte(modRm(dstXmm, srcXmm));
		}

		/**
		 * PMOVMSKB dstReg, srcXmm
		 *   66 0F D7 /r
		 *   create a mask of the most significant bit of each byte in srcXmm,
		 *   store 8-bit mask in low byte of dstReg, zero-extend
		 */
		inline void pmovmskb(int dstReg, int srcXmm) {
			emitByte(0x66);
			emitByte(0x0F);
			emitByte(0xD7);                // PMOVMSKB opcode
			emitByte(modRm(dstReg, srcXmm));
		}


		/// Build a ModR/M byte for register-to-register operations:
		///   mod=11? (register), reg=src, rm=dst
		/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
		inline uint8_t modRmGp(int dst, int src) {
			return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
		}

		/*
		REX
		*/


		void emitMTSPR_SWI(uint32_t reg) {
			// MTSPR #SPR_SOFTWARE_INTERRUPT, reg
			emit((encodingHeader::OPCODE_MTPR << 26) |
				((encodingHeader::SPR_SOFTWARE_INTERRUPT & 0x1F) << 5) |
				(reg & 0x1F));
		}

		void emitMFPR_MCES(uint32_t rd) {
			// MFPR rd, SPR_MACHINE_CHECK_SUMMARY
			emit(OP(encodingHeader::OPCODE_MFPR)
				| ((encodingHeader::SPR_MACHINE_CHECK_SUMMARY & 0x1FF) << 5)
				| (rd & 0x1F)
			);
		}
		/// Emit a REX prefix if needed:
		///   W=1 for 64-bit operand, R=extension of reg field, B=extension of rm field
		inline void emitRex(bool w, int reg, int rm) {
			// base = 0100____b  
			uint8_t rex = 0x40
				| (w ? 0x08 : 0)  // W bit  
				| ((reg & 0x8) ? 0x04 : 0)  // R bit  
				| ((rm & 0x8) ? 0x01 : 0); // B bit  
			if (rex != 0x40)  // only emit if any bit beyond the fixed “0100” is set  
				emitByte(rex);
		}

		// Emit REX if either register ? 8, but keep W=0 for 32-bit ops:
		inline void emitRex32(int reg, int rm) {
			// 0100WRXB: W=0, R=(reg>>3), X=0, B=(rm>>3)
			uint8_t rex = 0x40
				| ((reg & 0x8) ? 0x04 : 0)
				| ((rm & 0x8) ? 0x01 : 0);
			if (rex != 0x40)
				emitByte(rex);
		}

		/**
		* ADDSS dst, src  – dst = dst + src (32-bit float)
		*
		* How it works:
		* 1) 0xF3 prefix selects the scalar single-precision variant (MOVSS/ADDSS)
		* 2) 0x0F escape byte introduces the extended SSE opcode space
		* 3) 0x58 is the opcode for ADDSS (scalar single-precision add)
		* 4) The ModR/M byte (mod=11, reg=dst, rm=src) encodes which XMM registers to operate on
		*/
		/** ADDSS dst, src  – dst = dst + src (32-bit float) */
		inline void addss(int dst, int src) {
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0x58);
			emitByte(modRm(dst, src));
		}

		/**
 * ADDSD dst, src  – dst = dst + src (64-bit float)
 *
 * 0xF2 prefix ? SD
 * 0x0F escape
 * 0x58 ADDSD opcode
 */
 /** ADDSD dst, src  – dst = dst + src (64-bit float) */
		inline void addsd(int dst, int src) {
			emitByte(0xF2);
			emitByte(0x0F);
			emitByte(0x58);
			emitByte(modRm(dst, src));
		}

		/*
		Subtract operations
		*/

		/**
* SUBSS dst, src  – dst = dst – src (32-bit float)
*
* 0xF3 prefix ? SS
* 0x0F escape
* 0x5C SUBSS opcode
*/
/** SUBSS dst, src  – dst = dst ? src (32-bit float) */
		inline void subss(int dstFpReg, int srcFpReg) {
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0x5C);
			emitByte(modRm(dstFpReg, srcFpReg));
		}

		/**
 * SUBSD dst, src  – dst = dst – src (64-bit float)
 *
 * 0xF2 prefix ? SD
 * 0x0F escape
 * 0x5C SUBSD opcode
 */

 /** SUBSD dst, src  – dst = dst ? src (64-bit float) */
		inline void subsd(int dstFpReg, int srcFpReg) {
			emitByte(0xF2);
			emitByte(0x0F);
			emitByte(0x5C);
			emitByte(modRm(dstFpReg, srcFpReg));
		}



		/*
		 MOV Operations
		*/

		/**
 * MOVSS dst, src  – copy 32-bit float
 *
 * 0xF3 prefix ? SS (scalar single)
 * 0x0F escape
 * 0x10 MOVSS opcode
 */
 /** MOVSS dst, src  – copy 32-bit float from XMM[src] to XMM[dst] */
		inline void movss(int dst, int src) {
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0x10);
			emitByte(modRm(dst, src));
		}


		inline void emitAddRegReg(int dstReg, int srcReg) {
			// [opcode][dst][src][padding]
			emitBits(0x10, 6);    // opcode (6 bits)
			emitBits(dst, 5);     // Rd
			emitBits(src, 5);     // Rn
			emitBits(0, 16);      // unused
		}


		/**
		* MOVSD dst, src  – copy 64-bit float from XMM[src] to XMM[dst]
		*
		* How it works:
		* 1) emitRex(false, dst, src) emits REX.R/B bits so dst/src ? XMM8 work.
		* 2) 0xF2 prefix selects scalar double?precision variant
		* 3) 0x0F escape byte for extended opcodes
		* 4) 0x10 opcode for MOVSD
		* 5) ModR/M byte (mod=11, reg=dst, rm=src) picks the two XMM registers
		*/
		/** MOVSD dst, src  – copy 64-bit float from XMM[src] to XMM[dst] */
		inline void movsd(int dstFp, int srcFp) {
			// SSE uses REX.R/B to extend the ModR/M fields for XMM8–XMM15  
			emitRex(/*W=*/false, /*reg=*/dstFp, /*rm=*/srcFp);
			emitByte(0xF2);
			emitByte(0x0F);
			emitByte(0x10);
			emitByte(modRm(dstFp, srcFp));

		}



		/**
		 * @brief Reserve 4 bytes for a label reference (branch displacement).
		 */
		inline void emitLabelRef(const std::string& label) {
			size_t pos = codeSize();
			codeBuffer.insert(codeBuffer.end(), 4, 0);
			fixups[label].push_back(pos);
		}

		//---------------------------------
		// Integer/Memory Helpers
		//---------------------------------

		/**
		 * @brief MOV dstReg, [GPR_BASE + srcIndex*8]
		 *        (Load a 64-bit GPR[srcIndex] into 'dstReg')
		 */
		inline void emitMovRegReg(HostReg dst, HostReg srcBase, uint8_t srcIndex) {
			codeBuffer.push_back(0x48); // REX.W
			codeBuffer.push_back(0x8B); // MOV r64, r/m64
			// TODO: full ModRM/SIB: mod=10, rm=srcBase, index=srcIndex, scale=8
		}

		/**
		 * @brief ADD dstReg, imm32
		 *        (Add 32-bit immediate to 64-bit register)
		 */
		inline void emitAddRegImm(HostReg dst, int32_t imm) {
			codeBuffer.push_back(0x48); // REX.W
			codeBuffer.push_back(0x81); // ADD r/m64, imm32 (opcode ext=0)
			codeBuffer.push_back(0xC0 | uint8_t(dst));
			append(reinterpret_cast<const uint8_t*>(&imm), 4);
		}

		/**
		 * @brief Store a 64-bit register value back to GPR array:
		 *        MOV [GPR_BASE + destIndex*8], srcReg
		 */
		inline void emitStoreRegMem(HostReg src, HostReg base, uint8_t destIndex, uint8_t bits) {
			codeBuffer.push_back(0x48); // REX.W
			codeBuffer.push_back(0x89); // MOV r/m64, r64
			// TODO: ModRM/SIB for memory operand [base + destIndex*8]
		}

		//---------------------------------
		// Floating-Point (SSE2) Helpers
		//---------------------------------

		inline void emitMovsdRegMem(HostReg dstXmm, HostReg base, int32_t disp) {
			codeBuffer.push_back(0xF2);
			codeBuffer.push_back(0x0F);
			codeBuffer.push_back(0x10);
			// TODO: ModRM/SIB for [base + disp]
		}
		inline void emitMovsdMemReg(HostReg base, int32_t disp, HostReg srcXmm) {
			codeBuffer.push_back(0xF2);
			codeBuffer.push_back(0x0F);
			codeBuffer.push_back(0x11);
			// TODO: ModRM/SIB
		}
		inline void emitAddsd(HostReg dstXmm, HostReg srcXmm) {
			codeBuffer.push_back(0xF2);
			codeBuffer.push_back(0x0F);
			codeBuffer.push_back(0x58);
			// TODO: ModRM
		}
		inline void emitSubsd(HostReg dstXmm, HostReg srcXmm) {
			codeBuffer.push_back(0xF2);
			codeBuffer.push_back(0x0F);
			codeBuffer.push_back(0x5C);
		}

		// Helpers for 64-bit SHL/SHR/SAR by imm8:
		//   REX.W + C1 /4 ib  SHL r/m64, imm8
		//   REX.W + C1 /5 ib  SHR r/m64, imm8
		//   REX.W + C1 /7 ib  SAR r/m64, imm8
		// :contentReference[oaicite:1]{index=1}
		 /**
		 * Shift Logical Left (quadword), immediate:
		 *   REX.W, 0xC1, ModR/M with /4, imm8
		 */
		inline void shlq(int dstReg, uint8_t imm) {
			emitByte(rexByte(/*W=*/true, /*reg=*/4, /*rm=*/dstReg));
			emitByte(0xC1);
			emitByte(modRm(4, dstReg));
			emitByte(imm);
		}

		/**
		 * Shift Logical Right (quadword), immediate:
		 *   REX.W, 0xC1, ModR/M with /5, imm8
		 */
		inline void shrq(int dstReg, uint8_t imm) {
			emitByte(rexByte(true, /*reg=*/5, dstReg));
			emitByte(0xC1);
			emitByte(modRm(5, dstReg));
			emitByte(imm);
		}

		/**
		 * Shift Arithmetic Right (quadword), immediate:
		 *   REX.W, 0xC1, ModR/M with /7, imm8
		 */
		inline void sarq(int dstReg, uint8_t imm) {
			emitByte(rexByte(true, /*reg=*/7, dstReg));
			emitByte(0xC1);
			emitByte(modRm(7, dstReg));
			emitByte(imm);
		}

		//----------------------------------------------------------------------------
	// Integer 64-bit register-to-register move/add/sub
	//----------------------------------------------------------------------------

	/**
	 * MOVQ dst, src
	 *   // copy full 64-bit register: dst ? src
	 *   REX.W + 0x89 /r      (MOV r/m64, r64)
	 */
		inline void movq(int dst, int src) {
			// W=1 for 64-bit, R=src, B=dst
			emitRex(/*w=*/true, /*reg=*/src, /*rm=*/dst);
			emitByte(0x89);
			emitByte(modRm(dst, src));
		}

		/**
		 * ADDQ dst, src
		 *   // dst ? dst + src, 64-bit wrap
		 *   REX.W + 0x01 /r      (ADD r/m64, r64)
		 */
		inline void addq(int dst, int src) {
			emitRex(true, src, dst);
			emitByte(0x01);
			emitByte(modRm(dst, src));
		}

		/**
		 * SUBQ dst, src
		 *   // dst ? dst ? src, 64-bit wrap
		 *   REX.W + 0x29 /r      (SUB r/m64, r64)
		 */
		inline void subq(int dst, int src) {
			emitRex(true, src, dst);
			emitByte(0x29);
			emitByte(modRm(dst, src));
		}

		// in Assembler.h, inside class Assembler : public AssemblerBase {
  /**
   * MOVL dst, src
   *   – load 32-bit register: MOV r/m32, r32 ? 0x89 /r
   */
		inline void movl(int dst, int src) {
			emitRex(/*W=*/false, /*reg=*/src, /*rm=*/dst);
			emitByte(0x89);                // MOV r/m32, r32 :contentReference[oaicite:0]{index=0}
			emitByte(modRm(dst, src));
		}

		/**
		 * SHLL dst, imm8
		 *   – 32-bit logical shift left by immediate: C1 /4 ib
		 */
		inline void shll(int dst, uint8_t imm) {
			emitRex(false, /*reg=*/4, /*rm=*/dst);
			emitByte(0xC1);                // SHIFT r/m32, imm8; /4 = SHL :contentReference[oaicite:1]{index=1}
			emitByte(modRm(4, dst));
			emitByte(imm);
		}

		/**
		 * ADDL dst, src
		 *   – add 32-bit registers: ADD r/m32, r32 ? 0x01 /r
		 */
		inline void addl(int dst, int src) {
			emitRex(false, /*reg=*/src, /*rm=*/dst);
			emitByte(0x01);                // ADD r/m32, r32 :contentReference[oaicite:2]{index=2}
			emitByte(modRm(dst, src));
		}

		/**
		 * MOVSXD dst, src
		 *   – sign-extend 32?64 bits: REX.W + 0x63 /r
		 */
		inline void movsxd(int dst, int src) {
			emitRex(/*W=*/true, /*reg=*/src, /*rm=*/dst);
			emitByte(0x63);                // MOVSXD r64, r/m32 :contentReference[oaicite:3]{index=3}
			emitByte(modRm(dst, src));

		}

		/**
		 * Compare 32-bit signed: CMP r/m32, r32  (opcode 0x39 /r)
		 * Emits a REX prefix with W=0 and R/B bits if you touch registers ?r8.
		 */
		inline void cmpl(int dstReg, int srcReg) {
			emitRex32(srcReg, dstReg);    // REX.W=0 plus R/B for regs 8–15
			emitByte(0x39);               // CMP r/m32, r32
			emitByte(modRmGp(dstReg, srcReg));
		}



		/// CMP r/m64, r64 – 64-bit compare (sets FLAGS)
		inline void cmpq(int dstReg, int srcReg) {
			emitRex(/*W=*/true, /*reg=*/srcReg, /*rm=*/dstReg);
			emitByte(0x39);               // opcode 39 /r :contentReference[oaicite:7]{index=7}
			emitByte(modRmGp(dstReg, srcReg));
		}
		/// SETE: Set byte if equal ? 0x0F 0x94 /r 
		/// Here reg-field = 0, r/m = destination register
		/// Set-byte helpers (conditional moves into a byte)
		inline void sete(int dst) {
			emitByte(0x0F);
			emitByte(0x94);
			emitByte(modRmGp(dst, /*reg=*/0));
		}

		/// SETLE: Set byte if ? (signed) ? 0x0F 0x9E /r 
		inline void setle(int dst) {
			emitByte(0x0F);
			emitByte(0x9E);
			emitByte(modRmGp(dst, /*reg=*/0));
		}

		/// SETL: Set byte if <  (signed) ? 0x0F 0x9C /r 
		inline void setl(int dst) {
			emitByte(0x0F);
			emitByte(0x9C);
			emitByte(modRmGp(dst, /*reg=*/0));
		}

		/// MOVZX r64, r/m8 ? REX.W + 0F B6 /r 
		/// Zero?extend the low 8 bits of `src` into full dst register.
		/// Zero-extend the result byte ? full 64-bit register
		inline void movzbq(int dst, int src) {
			emitRex(/*W=*/true, /*reg=*/dst, /*rm=*/src);
			emitByte(0x0F);
			emitByte(0xB6);
			emitByte(modRmGp(dst, src));
		}







		// In Assembler.h, inside class Assembler : public AssemblerBase







		/// SETB dst – set byte if below (CF=1) ? unsigned < (Unsigned variants:)
		inline void setb(int dstReg) {
			emitByte(0x0F);
			emitByte(0x92);               // 92 = SETB /r :contentReference[oaicite:8]{index=8}
			emitByte(modRmGp(dstReg, /*reg=*/0));
		}



		/// SETBE dst – set byte if below or equal (CF=1 or ZF=1) ? unsigned ?
		inline void setbe(int dstReg) {
			emitByte(0x0F);
			emitByte(0x96);               // 96 = SETBE /r 
			emitByte(modRmGp(dstReg, /*reg=*/0));
		}




		/**
		* LZCNT r64, r/m64 – count leading zero bits in a 64-bit register
		*   encoding: REX.W + 0xF3 0x0F 0xBD /r
		*   Intel SDM Vol.2A §6.2
		*/
		inline void lzcntq(int dstReg, int srcReg) {
			// 1) REX.W=1 for 64-bit operand (and any high-bit regs)
			emitRex(/*W=*/true, /*reg=*/dstReg, /*rm=*/srcReg);
			// 2) prefix/function bytes for LZCNT
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0xBD);
			// 3) ModR/M byte: reg=dst, rm=src
			emitByte(modRmGp(dstReg, srcReg));
		}

		/**
   * POPCNT r/m32 -> r32 (Alpha CTPOP ? x86 POPCNT)
   *   F3 0F B8 /r
   * See Intel® SDM, Vol. 2, “POPCNT” :contentReference[oaicite:6]{index=6}
   */
		inline void popcntl(int dstReg, int srcReg) {
			// If any reg???8, emit REX with W=0 for a 32-bit POPCNT
			if ((dstReg | srcReg) & 0x8) {
				emitByte(rexByte(false, srcReg, dstReg));
			}
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0xB8);
			emitByte(modRmGp(dstReg, srcReg));
		}

		/**
		 * POPCNT r/m64 -> r64 (Alpha CTPOP ? x86 POPCNT)
		 *   REX.W=1, F3 0F B8 /r
		 * See Intel® SDM, Vol. 2, “POPCNT” :contentReference[oaicite:7]{index=7}
		 */
		inline void popcntq(int dstReg, int srcReg) {
			// Always emit REX.W=1 for 64-bit
			emitByte(rexByte(true, srcReg, dstReg));
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0xB8);
			emitByte(modRmGp(dstReg, srcReg));
		}
		/// SUBL dst, src
		///   – 32-bit subtract: SUB r/m32, r32 ? opcode 0x29 /r
		inline void subl(int dstReg, int srcReg) {
			emitRex(/*W=*/false, /*reg=*/srcReg, /*rm=*/dstReg);
			emitByte(0x29);
			emitByte(modRm(dstReg, srcReg));
		}

		/// IMULL dst, src
		///   – signed 32-bit multiply: IMUL r32, r/m32 ? 0x0F 0xAF /r
		inline void imull(int dstReg, int srcReg) {
			emitRex(/*W=*/false, /*reg=*/srcReg, /*rm=*/dstReg);
			emitByte(0x0F);
			emitByte(0xAF);
			emitByte(modRm(dstReg, srcReg));
		}

		/// IMULQ dst, src
		///   – signed 64-bit multiply: REX.W + 0x0F 0xAF /r
		inline void imulq(int dstReg, int srcReg) {
			emitRex(/*W=*/true, /*reg=*/srcReg, /*rm=*/dstReg);
			emitByte(0x0F);
			emitByte(0xAF);
			emitByte(modRm(dstReg, srcReg));
		}

		/// TZCNTQ dst, src
		///   – count trailing zeros: TZCNT r64, r/m64 ? F3 0F BC /r
		inline void tzcntq(int dstReg, int srcReg) {
			emitRex(/*W=*/true, /*reg=*/dstReg, /*rm=*/srcReg);
			emitByte(0xF3);
			emitByte(0x0F);
			emitByte(0xBC);
			emitByte(modRm(dstReg, srcReg));
		}


		// Bitwise Boolean ops on 64-bit registers:
// AND   r/m64, r64 ? 21 /r
		inline void andq(int dst, int src) {
			emitRex(/*W=*/true, /*reg=*/src, /*rm=*/dst);
			emitByte(0x21);
			emitByte(modRmGp(dst, src));
		}

		// OR    r/m64, r64 ? 09 /r
		inline void orq(int dst, int src) {
			emitRex(true, src, dst);
			emitByte(0x09);
			emitByte(modRmGp(dst, src));
		}

		// XOR   r/m64, r64 ? 31 /r
		inline void xorq(int dst, int src) {
			emitRex(true, src, dst);
			emitByte(0x31);
			emitByte(modRmGp(dst, src));
		}

		// NOTQ  r/m64 ? F7 /2
		inline void notq(int dst) {
			// /2 in the ModR/M.reg field selects BITWISE NOT
			emitRex(true, /*reg=*/2, /*rm=*/dst);
			emitByte(0xF7);
			emitByte(modRmGp(dst, 2));
		}

		// Test  r/m64, r64 ? 85 /r
		//   Sets FLAGS for a conditional move or branch
		inline void testq(int dst, int src) {
			emitRex(true, /*reg=*/src, /*rm=*/dst);
			emitByte(0x85);
			emitByte(modRmGp(dst, src));
		}

		// Conditional Moves (64-bit):
		//   CMOVE r64, r/m64 ? 0F 44 /r  (copy if ZF=1)
		//   CMOVNE        ? 0F 45 /r
		//   CMOVL  (signed<) ? 0F 4C /r
		//   CMOVLE (signed?) ? 0F 4E /r
		//   CMOVG  (signed>) ? 0F 4F /r
		//   CMOVGE (signed?) ? 0F 4D /r
		inline void cmove(int dst, int src) { emitByte(0x0F); emitByte(0x44); emitByte(modRmGp(dst, src)); }
		inline void cmovne(int dst, int src) { emitByte(0x0F); emitByte(0x45); emitByte(modRmGp(dst, src)); }
		inline void cmovl(int dst, int src) { emitByte(0x0F); emitByte(0x4C); emitByte(modRmGp(dst, src)); }
		inline void cmovle(int dst, int src) { emitByte(0x0F); emitByte(0x4E); emitByte(modRmGp(dst, src)); }
		inline void cmovg(int dst, int src) { emitByte(0x0F); emitByte(0x4F); emitByte(modRmGp(dst, src)); }
		inline void cmovge(int dst, int src) { emitByte(0x0F); emitByte(0x4D); emitByte(modRmGp(dst, src)); }

		// Move immediate ? r64 (for IMPLVER):
		//   REX.W + B8+rd, imm64
		inline void movImm64(int dst, uint64_t imm) {
			// Emit REX.W and B bit for extended regs
			uint8_t rex = 0x48 | ((dst >> 3) & 1);
			if (rex != 0x48) emitByte(rex);
			// MOV r64, imm64 opcodes B8 + (dst & 7)
			emitByte(0xB8 | (dst & 7));
			// little-endian 64-bit immediate
			emitBytes(reinterpret_cast<const uint8_t*>(&imm), 8);
		}

		/**
 * CMOVZ dst, src   – 64-bit conditional move if zero (ZF=1)
 *   Encoding:  REX.W=1, 0F 44 /r
 */
		inline void cmovz(int dstReg, int srcReg) {
			// 64-bit mode, so W=1; REX.R/B for regs ?8
			emitRex(true, /*reg=*/srcReg, /*rm=*/dstReg);
			emitByte(0x0F);
			emitByte(0x44);               // CMOVZ opcode
			emitByte(modRmGp(dstReg, srcReg));
		}

		/**
		 * CMOVNZ dst, src  – 64-bit conditional move if not-zero (ZF=0)
		 *   Encoding:  REX.W=1, 0F 45 /r
		 */
		inline void cmovnz(int dstReg, int srcReg) {
			emitRex(true, /*reg=*/srcReg, /*rm=*/dstReg);
			emitByte(0x0F);
			emitByte(0x45);               // CMOVNZ opcode
			emitByte(modRmGp(dstReg, srcReg));
		}
#pragma region ByteManipulation Helper Functions
		// Scalar single-precision move/add/sub/mul/div/sqrt
		inline void movss(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x10); emitByte(modRm(dst, src));
		}
		inline void addss(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x58); emitByte(modRm(dst, src));
		}
		inline void subss(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x5C); emitByte(modRm(dst, src));
		}
		inline void mulss(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x59); emitByte(modRm(dst, src));
		}
		inline void divss(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x5E); emitByte(modRm(dst, src));
		}
		inline void sqrtss(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x51); emitByte(modRm(dst, src));
		}

		// Scalar double-precision
		inline void movsd(int dst, int src) {
			emitRex(false, dst, src);
			emitByte(0xF2); emitByte(0x0F); emitByte(0x10); emitByte(modRm(dst, src));
		}
		inline void addsd(int dst, int src) {
			emitRex(false, dst, src);
			emitByte(0xF2); emitByte(0x0F); emitByte(0x58); emitByte(modRm(dst, src));
		}
		inline void subsd(int dst, int src) {
			emitRex(false, dst, src);
			emitByte(0xF2); emitByte(0x0F); emitByte(0x5C); emitByte(modRm(dst, src));
		}
		inline void mulsd(int dst, int src) {
			emitRex(false, dst, src);
			emitByte(0xF2); emitByte(0x0F); emitByte(0x59); emitByte(modRm(dst, src));
		}
		inline void divsd(int dst, int src) {
			emitRex(false, dst, src);
			emitByte(0xF2); emitByte(0x0F); emitByte(0x5E); emitByte(modRm(dst, src));
		}
		inline void sqrtsd(int dst, int src) {
			emitRex(false, dst, src);
			emitByte(0xF2); emitByte(0x0F); emitByte(0x51); emitByte(modRm(dst, src));
		}

		// Conversions
		inline void cvtss2sd(int dst, int src) {
			emitByte(0xF3); emitByte(0x0F); emitByte(0x5A); emitByte(modRm(dst, src));
		}
		inline void cvtsd2ss(int dst, int src) {
			emitByte(0xF2); emitByte(0x0F); emitByte(0x5A); emitByte(modRm(dst, src));
		}

#pragma endregion ByteManipulation Helper Functions

#pragma region System Masks
		static constexpr uint32_t FLOAT_REGISTER_BITS = 5;
		static constexpr uint32_t FLOAT_REGISTER_MASK = (1u << FLOAT_REGISTER_BITS) - 1;  // 0x1F
		//(1<<5)-1 = 31, i.e. 0x1F. You can use it to mask out any of the RA/RB/RC fields in a 32-bit instruction word.
		static constexpr uint32_t INTEGER_REGISTER_BITS = 5;
		static constexpr uint32_t INTEGER_REGISTER_MASK = (1u << INTEGER_REGISTER_BITS) - 1;  // 0x1F

#pragma endregion System Masks

	private:
		std::vector<uint8_t> codeBuffer;
		std::unordered_map<std::string, size_t> labels;
		std::unordered_map<std::string, std::vector<size_t>> fixups;
		inline void append(const uint8_t* data, size_t len) {
			codeBuffer.insert(codeBuffer.end(), data, data + len);
		}
	};
}

#endif // ASSEMBLER_H_
