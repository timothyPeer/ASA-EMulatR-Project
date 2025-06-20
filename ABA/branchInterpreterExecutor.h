#pragma once
// =============================================================================
// BranchInterpreterExecutor.h
// =============================================================================
// C++-only interpreter for Alpha AXP branch instructions (Fmt2/3/4/5)
// Primary opcodes 0x04–0x07 (conditional, JSR, BR, BSR).
// Based on Alpha AXP Architecture Reference Manual, Fourth Edition,
// Appendix C.3 (Branch Instruction Formats) ?cite?ASA-C3?

#include "../ABA/executors/IExecutor.h"
#include "structs/BranchInstruction.h"
#include "../AEC/RegisterBank.h"
#include "../AEJ/AlphaProcessorContext.h"

#include <array>
#include <cstdint>

namespace Alpha {

	/// Interpreter for branch-format instructions (primary opcodes 0x04–0x07)
	class BranchInterpreterExecutor : public IExecutor {
	public:
		using Handler = void (BranchInterpreterExecutor::*)(const BranchInstruction&);

		/// Construct with references to register bank and processor context
		explicit BranchInterpreterExecutor(RegisterBank* regs, AlphaProcessorContext* ctx)
			: regs_(regs), ctx_(ctx), dispatchTable_(createDispatchTable()) {
		}

		/// Execute a decoded BranchInstruction
		inline void execute(const BranchInstruction& i) override {
			uint8_t primary = i.opcode;
			if (primary < dispatchTable_.size()) {
				Handler h = dispatchTable_[primary];
				if (h) (this->*h)(i);
			}
		}

	private:
		RegisterBank* regs_;              ///< integer register file
		AlphaProcessorContext* ctx_;           ///< processor status & PC
		std::array<Handler, 64> dispatchTable_;

		/// Build primary-opcode -> handler table
		static std::array<Handler, 64> createDispatchTable() {
			std::array<Handler, 64> table{};
			table[0x04] = &BranchInterpreterExecutor::interpCond; // conditional branches
			table[0x05] = &BranchInterpreterExecutor::interpJsr;  // JSR
			table[0x06] = &BranchInterpreterExecutor::interpBr;   // BR (unconditional)
			table[0x07] = &BranchInterpreterExecutor::interpBsr;  // BSR (subroutine)
			return table;
		}



		// ------------------------------------------------------------------------
		// Handlers for primary opcodes
		// ------------------------------------------------------------------------

		/// Fmt2: conditional branch
		inline void interpCond(const BranchInstruction& inst) {
			uint64_t ra_val = regs_->readIntReg(inst.ra);
			if (inst.isTaken(ra_val)) {
				uint64_t target = inst.computeTarget(ctx_->getProgramCounter());
				ctx_->setProgramCounter(target);
			}
		}


		/// JSR: jump to subroutine (Fmt3)
		inline void interpJsr(const BranchInstruction& i) {
			uint64_t returnPc = ctx_->getProgramCounter() + 4;
			regs_->writeIntReg(i.ra, returnPc);      // save return address
			ctx_->setProgramCounter(returnPc + i.disp);
		}

		/// BR: unconditional branch (Fmt4)
		inline void interpBr(const BranchInstruction& i) {
			ctx_->setProgramCounter(ctx_->getProgramCounter() + i.disp);
		}

		/// BSR: branch to subroutine (Fmt5)
		inline void interpBsr(const BranchInstruction& i) {
			uint64_t returnPc = ctx_->getProgramCounter() + 4;
			regs_->writeIntReg(i.ra, returnPc);
			ctx_->setProgramCounter(returnPc + i.disp);
		}
	};

} // namespace Alpha


