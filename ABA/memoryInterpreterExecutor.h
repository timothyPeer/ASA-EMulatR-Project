#pragma once
#include "helpers/IExecutor.h"
#pragma once
// =============================================================================
// MemoryInterpreterExecutor.h
// =============================================================================
// C++-only interpreter for Alpha AXP memory-reference instructions.
// Based on Alpha AXP Architecture Reference Manual, Fourth Edition,
// Appendix C.4 (Memory-Reference Formats) and Chapter 6.

#include "../ABA/executors/IExecutor.h"
#include "structs/MemoryInstruction.h"

#include "../AEJ/AlphaProcessorContext.h"

#include <array>
#include "../AEC/RegisterBank.h"

namespace Alpha {

	/// Interpreter for all load/store memory instructions (primary opcodes 0x14–0x1B, 0x1C–0x23)
	class MemoryInterpreterExecutor  : public IExecutor {
	public:
		using Handler = void (MemoryInterpreterExecutor::*)(const MemoryInstruction&);

		/// Construct with register bank and processor context
		MemoryInterpreterExecutor(RegisterBank* regs, AlphaProcessorContext* ctx)
			: regs_(regs), ctx_(ctx), dispatchTable_(createDispatchTable()) {
		}

		/// Execute a decoded MemoryInstruction
		inline void execute(const MemoryInstruction& inst) override {
			uint8_t op = inst.opcode;
			if (op < dispatchTable_.size() && dispatchTable_[op]) {
				(this->*dispatchTable_[op])(inst);
			}
		}

	private:
		RegisterBank* regs_;
		AlphaProcessorContext* ctx_;
		std::array<Handler, 64> dispatchTable_;

		/// Build the opcode->handler table for memory ops
		static std::array<Handler, 64> createDispatchTable() {
			std::array<Handler, 64> table{};
			// --- Load Byte (signed) ---
			table[0x19] = &MemoryInterpreterExecutor::interpLdb;
			// --- Load Byte Unsigned ---
			table[0x0A] = &MemoryInterpreterExecutor::interpLdbu;
			// --- Load Halfword (signed) ---
			table[0x1B] = &MemoryInterpreterExecutor::interpLdh;
			// --- Load Halfword Unsigned ---
			table[0x1C] = &MemoryInterpreterExecutor::interpLdhu;
			// --- Load Longword (signed 32-bit) ---
			table[0x1D] = &MemoryInterpreterExecutor::interpLdW;
			// --- Load Longword Unsigned ---
			table[0x0C] = &MemoryInterpreterExecutor::interpLdWu;
			// --- Load Quadword ---
			table[0x16] = &MemoryInterpreterExecutor::interpLdQ;
			// ... add other loads (LDA, LDQ_U, LDF, etc.) as needed ...

			// --- Store Byte ---
			table[0x0B] = &MemoryInterpreterExecutor::interpStb;
			// --- Store Halfword ---
			table[0x0D] = &MemoryInterpreterExecutor::interpSth;
			// --- Store Longword ---
			table[0x0E] = &MemoryInterpreterExecutor::interpStW;
			// --- Store Quadword ---
			table[0x10] = &MemoryInterpreterExecutor::interpStQ;
			// ... add other stores as needed ...

			  // Memory-format jumps (JMP/JSR/RET/JSR_COROUTINE)
			table[0x1A] = &MemoryInterpreterExecutor::interpMemJump;
			return table;
		}

		// Helper to compute effective address pointer
		inline uint8_t* computeAddr(const MemoryInstruction& i) const {
			uint64_t base = regs_->readIntReg(i.rb);
			uint64_t addr = i.computeAddress(base);
			return regs_->basePointer() + addr;
		}

		// === Load Instructions ===
		inline void interpLdb(const MemoryInstruction& i) {
			// Compute effective address from Rb
			uint8_t* addr = regs_->basePointer()
				+ i.computeAddress(regs_->readIntReg(i.rb));
			// Load signed byte, write into Ra
			int8_t  v = *reinterpret_cast<int8_t*>(addr);
			regs_->writeIntReg(i.ra, static_cast<uint64_t>(static_cast<int64_t>(v)));
			ctx_->updateConditionCodes(static_cast<int64_t>(v), 0, 0, false);
		}

		inline void interpLdbu(const MemoryInstruction& i) {
			uint8_t v = *reinterpret_cast<uint8_t*>(computeAddr(i));
			regs_->writeIntReg(i.ra, v);
			ctx_->updateConditionCodes(static_cast<int64_t>(v), 0, 0, false);
		}

		inline void interpLdh(const MemoryInstruction& i) {
			int16_t v = *reinterpret_cast<int16_t*>(computeAddr(i));
			regs_->writeIntReg(i.ra, static_cast<uint64_t>(static_cast<int64_t>(v)));
			ctx_->updateConditionCodes(static_cast<int64_t>(v), 0, 0, false);
		}

		inline void interpLdhu(const MemoryInstruction& i) {
			uint16_t v = *reinterpret_cast<uint16_t*>(computeAddr(i));
			regs_->writeIntReg(i.ra, v);
			ctx_->updateConditionCodes(static_cast<int64_t>(v), 0, 0, false);
		}

		inline void interpLdW(const MemoryInstruction& i) {
			int32_t v = *reinterpret_cast<int32_t*>(computeAddr(i));
			regs_->writeIntReg(i.ra, static_cast<uint64_t>(static_cast<int64_t>(v)));
			ctx_->updateConditionCodes(static_cast<int64_t>(v), 0, 0, false);
		}

		inline void interpLdWu(const MemoryInstruction& i) {
			uint32_t v = *reinterpret_cast<uint32_t*>(computeAddr(i));
			regs_->writeIntReg(i.ra, v);
			ctx_->updateConditionCodes(static_cast<int64_t>(v), 0, 0, false);
		}

		inline void interpLdQ(const MemoryInstruction& i) {
			int64_t v = *reinterpret_cast<int64_t*>(computeAddr(i));
			regs_->writeIntReg(i.ra, static_cast<uint64_t>(v));
			ctx_->updateConditionCodes(v, 0, 0, false);
		}

		// === Store Instructions ===
		inline void interpStb(const MemoryInstruction& i) {
			int8_t  v = static_cast<int8_t>(regs_->readIntReg(i.rb));
			*reinterpret_cast<int8_t*>(computeAddr(i)) = v;
		}

		inline void interpSth(const MemoryInstruction& i) {
			int16_t v = static_cast<int16_t>(regs_->readIntReg(i.rb));
			*reinterpret_cast<int16_t*>(computeAddr(i)) = v;
		}

		inline void interpStW(const MemoryInstruction& i) {
			uint8_t* addr = regs_->basePointer()
				+ i.computeAddress(regs_->readIntReg(i.rb));
			int32_t v = static_cast<int32_t>(regs_->readIntReg(i.ra));
			*reinterpret_cast<int32_t*>(addr) = v;
		}

		inline void interpStQ(const MemoryInstruction& i) {
			int64_t v = static_cast<int64_t>(regs_->readIntReg(i.rb));
			*reinterpret_cast<int64_t*>(computeAddr(i)) = v;
		}
		// --- Computed Jump Handler ---
		inline void interpMemJump(const MemoryInstruction& i) {
			uint64_t currentPC = ctx_->getProgramCounter();
			uint64_t target;
			switch (i.fnc) {
			case 0x00: // JMP
				target = regs_->readIntReg(i.rb) & ~0x3ULL;
				ctx_->setProgramCounter(target);
				break;
			case 0x01: // JSR
				regs_->writeIntReg(i.ra, currentPC + 4);
				target = regs_->readIntReg(i.rb) & ~0x3ULL;
				ctx_->setProgramCounter(target);
				break;
			case 0x02: // RET
				target = regs_->readIntReg(i.ra);
				ctx_->setProgramCounter(target);
				break;
			case 0x03: // JSR_COROUTINE
				regs_->writeIntReg(i.ra, currentPC + 4);
				target = regs_->readIntReg(i.rb) & ~0x3ULL;
				ctx_->setProgramCounter(target);
				break;
			default:
				break;
			}
		}
	};

} // namespace Alpha
