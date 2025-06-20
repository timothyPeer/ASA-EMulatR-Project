// MemoryInstruction.h
// Header for Alpha AXP “Mem”-format memory instructions  
// Format: opcode[31:26], Ra[25:21], Rb[20:16], disp[15:0]  
// See Alpha AXP System Reference Manual v6, §3.3.1, Fig. 3-1 (p. 3-9) :contentReference[oaicite:0]{index=0}

#ifndef MEMORY_INSTRUCTION_H
#define MEMORY_INSTRUCTION_H

#include <cstdint>
#include "../ABA/structs/Instruction.h"
#include <stdexcept>
#include "../AEJ/AlphaProcessorContext.h"


class AlphaCPU;
namespace Arch {
    // -----------------------------------------------------------------------------
    // Standard memory?reference format (load/store, address?compute, jumps):
    //   opcode[31:26], Ra[25:21], Rb[20:16], disp[15:0]
    //   Effective address = Rb + SEXT(disp)
    // -----------------------------------------------------------------------------
    struct MemoryInstruction : public Arch::Instruction {
        uint32_t raw;    ///< Raw 32-bit instruction word
        uint8_t  opcode; ///< Major opcode bits <31:26>
        uint8_t  ra;     ///< Base/destination register bits <25:21>
        uint8_t  rb;     ///< Index/source register bits <20:16>
        int16_t  disp;   ///< 16-bit signed displacement bits <15:0>
        uint16_t fnc;          ///< bits <12:5> (extended opcode)

        /// Decode raw instruction into fields
        inline void decode() {
            opcode = static_cast<uint8_t>((raw >> 26) & 0x3F);
            ra = static_cast<uint8_t>((raw >> 21) & 0x1F);
            rb = static_cast<uint8_t>((raw >> 16) & 0x1F);
            disp = static_cast<int16_t>(raw & 0xFFFF);
        }

		/**
 * @brief Load a value from virtual memory using processor context.
 *
 * This wrapper automatically extracts the CPU ID and PC from the context,
 * and invokes the AlphaMemorySystem read path.
 *
 * @param ctx   Alpha processor context (provides cpuId, PC, memSystem)
 * @param addr  Virtual address to read from
 * @param size  Size in bytes (must be 1, 2, 4, or 8)
 * @param out   Output parameter for the loaded value
 * @return true if load succeeded, false if a memory trap occurred
 */
		static inline bool loadMem(AlphaProcessorContext* ctx,
			uint64_t addr, size_t size, uint64_t& out)
		{
			AlphaMemorySystem* memSys = ctx->memSystem();
			quint16 cpuId = ctx->cpuId();
			quint64 pc = ctx->getProgramCounter();

			return memSys->readVirtualMemory(cpuId, addr, out, static_cast<int>(size), pc);
		}

		/**
 * @brief Store a value to virtual memory using processor context.
 *
 * This wrapper automatically extracts the CPU ID and PC from the context,
 * and invokes the AlphaMemorySystem write path.
 *
 * @param ctx   Alpha processor context (provides cpuId, PC, memSystem)
 * @param addr  Virtual address to write to
 * @param size  Size in bytes (must be 1, 2, 4, or 8)
 * @param val   Value to store
 * @return true if store succeeded, false if a memory trap occurred
 */

		static inline bool storeMem(AlphaProcessorContext* ctx,
			uint64_t addr, size_t size, uint64_t val)
		{
			AlphaMemorySystem* memSys = ctx->memSystem();
			quint16 cpuId = ctx->cpuId();
			quint64 pc = ctx->getProgramCounter();
			return memSys->writeVirtualMemory(cpuId, addr, val, static_cast<int>(size), pc);
		}


		FormatID format() const override { return FormatID::ALPHA_MEM; }
		uint16_t getCode() const override { return opcode; }
        /**
         * Compute the virtual address for a memory access:
         *   va = Rb_val + sign-extended displacement
         * @param Rb_val  Value read from integer register Rb
         * @return        64-bit effective address
         */
        inline uint64_t computeAddress(uint64_t Rb_val) const {
            return Rb_val + static_cast<int64_t>(disp);
        }

		
		// [18.8000] FETCH: atomic fetch (8-byte) and lock
		static inline void emitAlpha_FETCH(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t tmp = 0;
			if (!ctx->memSystem()->atomicFetch(ctx, va, tmp)) return;
			regs->writeIntReg(inst.ra, tmp);
			ctx->advancePC();
		}

		// [18.A000] FETCH_M: atomic fetch and modify (8-byte)
		static inline void emitAlpha_FETCH_M(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t tmp = 0;
			if (!ctx->memSystem()->atomicFetchModify(ctx, va, tmp)) return;
			regs->writeIntReg(inst.ra, tmp);
			ctx->advancePC();
		}

		// [20] LDF: load byte (signed)
		static inline void emitAlpha_LDF(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t tmp = 0;
			if (!loadMem(ctx, va, 1, tmp)) return;
			int8_t v = static_cast<int8_t>(tmp);
			regs->writeIntReg(inst.ra, static_cast<uint64_t>(v));
			ctx->advancePC();
		}

		// [21] LDG: load 4-byte word (signed)
		static inline void emitAlpha_LDG(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t tmp = 0;
			if (!loadMem(ctx, va, 4, tmp)) return;
			int32_t v = static_cast<int32_t>(tmp);
			regs->writeIntReg(inst.ra, static_cast<uint64_t>(v));
			ctx->advancePC();
		}

		// [22] LDS: load 2-byte halfword (signed)
		static inline void emitAlpha_LDS(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t tmp = 0;
			if (!loadMem(ctx, va, 2, tmp)) return;
			int16_t v = static_cast<int16_t>(tmp);
			regs->writeIntReg(inst.ra, static_cast<uint64_t>(v));
			ctx->advancePC();
		}

		// [23] LDT: load doubleword (8-byte)
		static inline void emitAlpha_LDT(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t tmp = 0;
			if (!loadMem(ctx, va, 8, tmp)) return;
			regs->writeIntReg(inst.ra, tmp);
			ctx->advancePC();
		}

		// [24] STF: store byte (8-bit low)
		static inline void emitAlpha_STF(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint8_t v = static_cast<uint8_t>(regs->readIntReg(inst.ra));
			if (!storeMem(ctx, va, 1, v)) return;
			ctx->advancePC();
		}

		// [25] STG: store 4-byte word
		static inline void emitAlpha_STG(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint32_t v = static_cast<uint32_t>(regs->readIntReg(inst.ra));
			if (!storeMem(ctx, va, 4, v)) return;
			ctx->advancePC();
		}

		// [26] STS: store 2-byte halfword
		static inline void emitAlpha_STS(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint16_t v = static_cast<uint16_t>(regs->readIntReg(inst.ra));
			if (!storeMem(ctx, va, 2, v)) return;
			ctx->advancePC();
		}

		// [27] STT: store doubleword (8-byte)
		static inline void emitAlpha_STT(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t va = inst.computeAddress(regs->readIntReg(inst.rb));
			uint64_t v = regs->readIntReg(inst.ra);
			if (!storeMem(ctx, va, 8, v)) return;
			ctx->advancePC();
		}

		// [08] LDA: load address (effective address)
		static inline void emitAlpha_LDA(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t v = inst.computeAddress(regs->readIntReg(inst.rb));
			regs->writeIntReg(inst.ra, v);
			ctx->advancePC();
		}

		// [09] LDAH: load address high (disp<<16)
		static inline void emitAlpha_LDAH(const MemoryInstruction& inst,
			RegisterBank* regs,
			AlphaProcessorContext* ctx)
		{
			uint64_t v = static_cast<uint64_t>(int64_t(inst.disp) << 16);
			regs->writeIntReg(inst.ra, v);
			ctx->advancePC();
		}








		/** [28.] Load floating?point register (Alpha), */
		static void emitAlpha_LDL(MemoryInstruction inst)
		{
			//TODO
		}


		/** [29.] Load globally resident register (Alpha), */
		static void emitAlpha_LDQ(MemoryInstruction inst)
		{
			//TODO
		}



		/** [0B.] Read processor status register (Alpha), */
		static void emitAlpha_LDQ_U(MemoryInstruction inst)
		{
			//TODO
		}


		/** [0F.] Reboot system (Alpha), */
		static void emitAlpha_STQ_U(MemoryInstruction inst)
		{
			//TODO
		}


		/** [2A.] Add double float, */
		static void emitAlpha_LDL_L(MemoryInstruction inst)
		{
			//TODO
		}


		/** [2B.] Add double float, */
		static void emitAlpha_LDQ_L(MemoryInstruction inst)
		{
			//TODO
		}


		/** [2C.] Add double float, */
		static void emitAlpha_STL(MemoryInstruction inst)
		{
			//TODO
		}


		/** [2D.] External call barrier (Alpha), */
		static void emitAlpha_STQ(MemoryInstruction inst)
		{
			//TODO
		}


		/** [2E.] Jump (Alpha), */
		static void emitAlpha_STL_C(MemoryInstruction inst)
		{
			//TODO
		}


		/** [2F.] Jump to subroutine (Alpha), */
		static void emitAlpha_STQ_C(MemoryInstruction inst)
		{
			//TODO
		}

		/**
        * [9.2.1] LDQ_L - Load Quadword and Reserve
        * This instruction loads a 64-bit value and sets a reservation
        * on the physical address for a later conditional store.
        */
		void emitAlphaLDQ_L(const MemoryInstruction& i, AlphaCPU* cpu, RegisterBank* regs, AlphaProcessorContext* ctx);
		void emitAlphaSTQ_C(const MemoryInstruction& i, AlphaCPU* cpu, RegisterBank* regs, AlphaProcessorContext* ctx);

		// Remaining instruction stubs (uncommon or model-specific) are left as no-ops
// to be implemented as needed:
#define NOOP_MEM(instr) \
static inline void instr(const MemoryInstruction&, RegisterBank*, AlphaProcessorContext*) { }

		NOOP_MEM(emitAlpha_LDQ)    // global load
			NOOP_MEM(emitAlpha_LDQ_U)  // global unaligned load
			NOOP_MEM(emitAlpha_STQ_U)  // store unaligned
			NOOP_MEM(emitAlpha_LDL)    // load floating
		/*	NOOP_MEM(emitAlpha_LDQ_L)  // load long*/
			NOOP_MEM(emitAlpha_LDL_L)  // load double-long
			NOOP_MEM(emitAlpha_STL)    // store floating
			NOOP_MEM(emitAlpha_STQ)    // store long
			NOOP_MEM(emitAlpha_STL_C)  // external call barrier
/*			NOOP_MEM(emitAlpha_STQ_C)  // subroutine barrier*/
			NOOP_MEM(emitAlpha_FETCH_M) // already defined above
			NOOP_MEM(emitAlpha_FETCH)
			NOOP_MEM(emitAlpha_SRL)

#undef NOOP_MEM
		


    };
}
#endif // MEMORY_INSTRUCTION_H

