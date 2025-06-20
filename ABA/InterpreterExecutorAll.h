#pragma once
// =============================================================================
// InterpreterExecutor.h
// =============================================================================
// Generic Interpreter for multiple PAL and instruction formats on Alpha, VAX, and Tru64.
// Supports 10 dispatch tables: 7 Alpha, 2 VAX, 1 Tru64.

#include "../executors/IExecutor.h"
#include "../ABA/structs/Instruction.h"
#include "../AEC/RegisterBank.h"
#include "../AEJ/AlphaProcessorContext.h"

#include <array>
#include <cstdint>
#include <functional>
#include "../AEJ/structures/structPALInstruction.h"
#include "structs/branchInstruction.h"
#include "structs/operateInstruction.h"
#include "structs/floatingPointInstruction_Vax.h"
#include "structs/floatingPointInstruction_alpha.h"
#include "executors/IExecutor.h"
#include "structs/memoryInstruction.h"
#include "structs/MemoryBarrierInstruction.h"
#include "structs/MemoryFuncCode.h"
#include "structs/palInstruction_Alpha.h"
#include "structs/palInstruction_Tru64.h"
#include "structs/VectorInstruction.h"


namespace Arch {

	

	/// InterpreterExecutor dispatches by format and opcode/function code
	class InterpreterExecutor : public IExecutor {
	public:
		using Handler = std::function<void(const Instruction&)>;

		InterpreterExecutor(RegisterBank* regs, AlphaProcessorContext* ctx)
			: regs_(regs), ctx_(ctx) {
			initDispatch();
		}

		/// Execute instruction by format and code
		inline void execute(const Instruction& instr) override {
			uint16_t code = instr.getCode();
			auto& table = dispatch_[static_cast<size_t>(instr.format())];
			if (code < table.size() && table[code]) {
				table[code](instr);
			}
		}

	private:
		RegisterBank* regs;
		AlphaProcessorContext* ctx;

		// Ten tables of 65536 handlers each
		std::array<std::array<Handler, 65536>, 10> dispatch_;

		/// Initialize all dispatch tables
		void initDispatch() {
			// Build each table
			buildAlphaMemTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_MEM)]);//
			buildAlphaMemFuncTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_MEMFCT)]);//
			buildAlphaBranchTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_BRANCH)]);//
			buildAlphaOperateTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_OPERATE)]);//
			buildAlphaFpOperateTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_FP_OPERATE)]);//
			buildAlphaPalTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_PAL)]);//
			//buildAlphaVectorTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_OPERATE_VECTOR)]); //
			//TODO
			//buildAlphaConsoleTable(dispatch_[static_cast<size_t>(FormatID::ALPHA_CONSOLE)]); 	//TODO
			//buildVaxPalTable(dispatch_[static_cast<size_t>(FormatID::VAX_PAL)]); 	//TODO
			buildVaxFpTable(dispatch_[static_cast<size_t>(FormatID::VAX_FP)]);//
			/**/
			buildTru64PalTable(dispatch_[static_cast<size_t>(FormatID::TRU64_PAL)]); //
			
		}

		//TODO
		//tbl[0x003] = &PALInstruction::emitAlpha_REBOOT; // TODO VERIFY NEEDED
		//tbl[0x ? ? 17] = &PALInstruction::emitTru64_TBIMSASN;
		//tbl[0x ? ? 13] = &PALInstruction::emitTru64_WRPRBR_TOCHECK;
		//tbl[0x ? ? 14] = &PALInstruction::emitTru64_TBIA_to_verify;

		// Stubs for table builders

		/**/
		static void buildAlphaMemTable(std::array<Handler, 65536>& tbl) {
			
			tbl[0x34] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_SRL(inst);
				};
			tbl[0x8000] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_FETCH(inst);
				};
			tbl[0xA000] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_FETCH_M(inst);
				};
			tbl[0x20] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDF(inst);
				};
			tbl[0x21] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDG(inst);
				};
			tbl[0x22] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDS(inst);
				};
			tbl[0x23] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDT(inst);
				};
			tbl[0x24] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STF(inst);
				};
			tbl[0x25] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STG(inst);
				};
			tbl[0x26] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STS(inst);
				};
			tbl[0x27] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STT(inst);
				};
			tbl[0x28] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDL(inst);
				};
			tbl[0x29] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDQ(inst);
				};
			tbl[0x08] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDA(inst);
				};
			tbl[0x09] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDAH(inst);
				};
			tbl[0x0B] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDQ_U(inst);
				};
			tbl[0x0F] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STQ_U(inst);
				};
			tbl[0x2A] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDL_L(inst);
				};
			tbl[0x2B] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_LDQ_L(inst);
				};
			tbl[0x2C] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STL(inst);
				};
			tbl[0x2D] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STQ(inst);
				};
			tbl[0x2E] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STL_C(inst);
				};
			tbl[0x2F] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryInstruction&>(base);
				MemoryInstruction::emitAlpha_STQ_C(inst);
				};
			
			tbl[0x4000] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryBarrierInstruction&>(base);
				MemoryBarrierInstruction::emitAlpha_MB(inst);
				};
			tbl[0x34] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryBarrierInstruction&>(base);
				MemoryBarrierInstruction::emitAlpha_BSR(inst);
				};
			tbl[0x00] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryBarrierInstruction&>(base);
				MemoryBarrierInstruction::emitAlpha_JMP(inst);
				};
			tbl[0x01] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryBarrierInstruction&>(base);
				MemoryBarrierInstruction::emitAlpha_JSR(inst);
				};
			tbl[0x03] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryBarrierInstruction&>(base);
				MemoryBarrierInstruction::emitAlpha_JSR_COROUTINE(inst);
				};


		}
		/**/
		static void buildAlphaMemFuncTable(std::array<Handler, 65536>& tbl) {
			

			tbl[0x0] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_TRAPB(inst);
				};
			tbl[0x400] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_EXCB(inst);
				};
			tbl[0x4400] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_WMB(inst);
				};
			tbl[0xC000] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_RPCC(inst);
				};
			tbl[0xE000] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_RC(inst);
				};
			tbl[0xE800] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_ECB(inst);
				};
			tbl[0xF000] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_RS(inst);
				};
			tbl[0x02] = [](const Instruction& base) {
				auto& inst = static_cast<const MemoryFuncCode&>(base);
				MemoryFuncCode::emitAlpha_RET(inst);
				};



		}
		/**/
		static void buildAlphaBranchTable(std::array<Handler, 65536>& tbl) {
		
			tbl[0x30] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BR(inst, regs, ctx);
				};
			tbl[0x31] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_FBEQ(inst, regs, ctx);
				};
			tbl[0x32] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_FBLT(inst, regs, ctx);
				};
			tbl[0x33] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_FBLE(inst, regs, ctx);
				};
			tbl[0x35] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_FBNE(inst, regs, ctx);
				};
			tbl[0x36] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_FBGE(inst, regs, ctx);
				};
			tbl[0x37] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_FBGT(inst, regs, ctx);
				};
			tbl[0x38] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BLBC(inst, regs, ctx);
				};
			tbl[0x39] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BEQ(inst, regs, ctx);
				};
			tbl[0x3A] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BLT(inst, regs, ctx);
				};
			tbl[0x3B] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BLE(inst, regs, ctx);
				};
			tbl[0x3C] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BLBS(inst, regs, ctx);
				};
			tbl[0x3D] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BNE(inst, regs, ctx);
				};
			tbl[0x3E] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BGE(inst, regs, ctx);
				};
			tbl[0x3F] = [](const Instruction& base) {
				auto& inst = static_cast<const BranchInstruction&>(base);
				BranchInstruction::emitAlpha_BGT(inst, regs, ctx);
				};
		}
		/**/
		static void buildAlphaOperateTable(std::array<Handler, 65536>& tbl) {

			tbl[0x00] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_ADDL(inst);
				};
			tbl[0x02] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S4ADDL(inst);
				};
			tbl[0x12] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S8ADDL(inst);
				};
			tbl[0x20] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_ADDQ(inst);
				};
			tbl[0x22] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S4ADDQ(inst);
				};
			tbl[0x32] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S8ADDQ(inst);
				};
			tbl[0x0B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S4SUBL(inst);
				};
			tbl[0x1B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S8SUBL(inst);
				};
			tbl[0x1D] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMPULT(inst);
				};
			tbl[0x2B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S4SUBQ(inst);
				};
			tbl[0x3B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_S8SUBQ(inst);
				};
			tbl[0x3D] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMPULE(inst);
				};
			tbl[0x0] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_AND(inst);
				};
			tbl[0x8] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_BIC(inst);
				};
			tbl[0x14] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVLBS(inst);
				};
			tbl[0x16] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVLBC(inst);
				};
			tbl[0x20] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_BIS(inst);
				};
			tbl[0x24] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVEQ(inst);
				};
			tbl[0x28] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_ORNOT(inst);
				};
			tbl[0x40] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_XOR(inst);
				};
			tbl[0x44] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVLT(inst);
				};
			tbl[0x46] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVGE(inst);
				};
			tbl[0x48] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EQV(inst);
				};
			tbl[0x64] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVLE(inst);
				};
			tbl[0x66] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_CMOVGT(inst);
				};
			tbl[0x02] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKBL(inst);
				};
			tbl[0x06] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTBL(inst);
				};
			tbl[0x12] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKWL(inst);
				};
			tbl[0x16] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTWL(inst);
				};
			tbl[0x22] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKLL(inst);
				};
			tbl[0x26] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTLL(inst);
				};
			tbl[0x30] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_ZAP(inst);
				};
			tbl[0x31] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_ZAPNOT(inst);
				};
			tbl[0x32] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKQL(inst);
				};
			tbl[0x36] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTQL(inst);
				};
			tbl[0x39] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_SLL(inst);
				};
			tbl[0x52] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKWH(inst);
				};
			tbl[0x57] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSWH(inst);
				};
			tbl[0x62] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKLH(inst);
				};
			tbl[0x67] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSLH(inst);
				};
			tbl[0x72] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MSKQH(inst);
				};
			tbl[0x77] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSQH(inst);
				};
			tbl[0x0B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSBL(inst);
				};
			tbl[0x1B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSWL(inst);
				};
			tbl[0x2B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSLL(inst);
				};
			tbl[0x3B] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_INSQL(inst);
				};
			tbl[0x3C] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_SRA(inst);
				};
			tbl[0x5A] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTWH(inst);
				};
			tbl[0x6A] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTLH(inst);
				};
			tbl[0x7A] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_EXTQH(inst);
				};
			tbl[0x00] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MULL(inst);
				};
			tbl[0x20] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MULQ(inst);
				};
			tbl[0x30] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_UMULH(inst);
				};
 
			static void buildAlphaVectorTable(std::array<Handler, 65536>& tbl) {
				/* [10.40] Shift right arithmetic  */
				tbl[0x60] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				VectorInstruction::emitAlpha_ADDL_V(inst);
			};

			/* [10.60] Extract longword high  */
			tbl[0x60] = [](const VectorInstruction& base) {
				auto& inst = static_cast<const VectorInstruction&>(base);
				VectorInstruction::emitAlpha_ADDQ_V(inst);
			};

		
		}
		

			/* [13.40] Floating-point multiply (round toward zero)  */
			tbl[0x40] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MULL_V(inst);
				};
			/* [13.60] Floating-point divide (round toward zero)  */
			tbl[0x60] = [](const Instruction& base) {
				auto& inst = static_cast<const OperateInstruction&>(base);
				OperateInstruction::emitAlpha_MULQ_V(inst);
				};


		}
		/**/
		static void buildAlphaFpOperateTable(std::array<Handler, 65536>& tbl) {
	
		
			tbl[0x09] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBL(inst);
				};
			tbl[0x29] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBQ(inst);
				};
			tbl[0x49] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBL_V(inst);
				};
			tbl[0x69] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBQ_V(inst);
				};
			tbl[0x0F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPBGE(inst);
				};
			tbl[0x2D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPEQ(inst);
				};
			tbl[0x4D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPLT(inst);
				};
			tbl[0x6D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPLE(inst);
				};
			tbl[0x26] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMOVNE(inst);
				};
			
			tbl[0x10A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTF_UC(inst);
				};
			tbl[0x10B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_UC(inst);
				};
			tbl[0x12A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTG_UC(inst);
				};
			tbl[0x12B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_UC(inst);
				};
			tbl[0x14B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_UM(inst);
				};
			tbl[0x16B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_UM(inst);
				};
			tbl[0x18A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTF_U(inst);
				};
			tbl[0x18B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_U(inst);
				};
			tbl[0x1AA] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTG_U(inst);
				};
			tbl[0x1AB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_U(inst);
				};
			tbl[0x1CB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_UD(inst);
				};
			tbl[0x1EB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_UD(inst);
				};
			tbl[0x40A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTF_SC(inst);
				};
			tbl[0x42A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTG_SC(inst);
				};
			tbl[0x48A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTF_S(inst);
				};
			tbl[0x4AA] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTG_S(inst);
				};
			tbl[0x50A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTF_SUC(inst);
				};
			tbl[0x50B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUC(inst);
				};
			tbl[0x52A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTG_SUC(inst);
				};
			tbl[0x52B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUC(inst);
				};
			tbl[0x54B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUM(inst);
				};
			tbl[0x56B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUM(inst);
				};
			tbl[0x58A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTF_SU(inst);
				};
			tbl[0x58B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SU(inst);
				};
			tbl[0x5AA] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTG_SU(inst);
				};
			tbl[0x5AB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SU(inst);
				};
			tbl[0x5CB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUD(inst);
				};
			tbl[0x5EB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUD(inst);
				};
			tbl[0x70B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUIC(inst);
				};
			tbl[0x72B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUIC(inst);
				};
			tbl[0x74B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUIM(inst);
				};
			tbl[0x76B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUIM(inst);
				};
			tbl[0x78B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUI(inst);
				};
			tbl[0x7AB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUI(inst);
				};
			tbl[0x7CB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTS_SUID(inst);
				};
			tbl[0x7EB] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SQRTT_SUID(inst);
				};
			
			
			tbl[0x80] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDF(inst);
				};
			
			tbl[0x81] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBF(inst);
				};
			
			tbl[0x82] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULF(inst);
				};
			
			

			tbl[0x09E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTDG(inst);
				};
			tbl[0x0A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDG(inst);
				};
			tbl[0x0A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBG(inst);
				};
			tbl[0x0A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULG(inst);
				};
			tbl[0x0A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVG(inst);
				};
			tbl[0x0A5] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPGEQ(inst);
				};
			tbl[0x0A6] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPGLT(inst);
				};
			
			tbl[0x0AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTGF(inst);
				};
			tbl[0x0AD] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTGD(inst);
				};
			tbl[0x0AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTGQ(inst);
				};
			tbl[0x0BC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQF(inst);
				};
			tbl[0x0BE] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQG(inst);
				};
			
			
			tbl[0x000] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_C(inst);
				};
			tbl[0x0E0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_D(inst);
				};
			tbl[0x0E0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_D(inst);
				};
			tbl[0x0E3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_D(inst);
				};
			tbl[0x0E3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVTID(inst);
				};
			tbl[0x0E2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_D(inst);
				};
			tbl[0x0E2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULTID(inst);
				};
			tbl[0x0E1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_D(inst);
				};
			tbl[0x0E1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBTID(inst);
				};
			tbl[0x1E0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_UD(inst);
				};
			tbl[0x01] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_C(inst);
				};
			tbl[0x02] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_C(inst);
				};
			tbl[0x03] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_C(inst);
				};
			tbl[0x5E0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUD(inst);
				};
			tbl[0x7E0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUID(inst);
				};
			tbl[0x1E1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_UD(inst);
				};
			tbl[0x020] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_C(inst);
				};
			tbl[0x21] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_C(inst);
				};
			tbl[0x22] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_C(inst);
				};
			tbl[0x23] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_C(inst);
				};
			tbl[0x40] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_M(inst);
				};
			tbl[0x41] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_M(inst);
				};
			tbl[0x42] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_M(inst);
				};
			tbl[0x43] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_M(inst);
				};
			tbl[0x5E1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUD(inst);
				};
			tbl[0x60] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_M(inst);
				};
			tbl[0x61] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_M(inst);
				};
			tbl[0x62] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_M(inst);
				};
			tbl[0x63] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_M(inst);
				};
			tbl[0x7E1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUID(inst);
				};
			tbl[0x80] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS(inst);
				};
			tbl[0x81] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS(inst);
				};
			tbl[0x82] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS(inst);
				};
			tbl[0x83] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS(inst);
				};
			tbl[0x100] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_UC(inst);
				};
			tbl[0x1E2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULTIUD(inst);
				};
			tbl[0x101] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_UC(inst);
				};
			tbl[0x102] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_UC(inst);
				};
			tbl[0x103] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_UC(inst);
				};
			tbl[0x120] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_UC(inst);
				};
			tbl[0x121] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_UC(inst);
				};
			tbl[0x122] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_UC(inst);
				};
			tbl[0x123] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_UC(inst);
				};
			tbl[0x140] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_UM(inst);
				};
			tbl[0x141] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_UM(inst);
				};
			tbl[0x142] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_UM(inst);
				};
			tbl[0x143] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_UM(inst);
				};
			tbl[0x160] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_UM(inst);
				};
			tbl[0x161] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_UM(inst);
				};
			tbl[0x162] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_UM(inst);
				};
			tbl[0x163] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_UM(inst);
				};
			tbl[0x180] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_U(inst);
				};
			tbl[0x181] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_U(inst);
				};
			tbl[0x182] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_U(inst);
				};
			tbl[0x183] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_U(inst);
				};
			tbl[0x500] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUC(inst);
				};
			tbl[0x5E2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUD(inst);
				};
			tbl[0x501] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUC(inst);
				};
			tbl[0x502] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUC(inst);
				};
			tbl[0x503] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUC(inst);
				};
			tbl[0x520] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUC(inst);
				};
			tbl[0x521] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUC(inst);
				};
			tbl[0x522] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUC(inst);
				};
			tbl[0x523] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUC(inst);
				};
			tbl[0x540] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUM(inst);
				};
			tbl[0x541] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUM(inst);
				};
			tbl[0x542] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUM(inst);
				};
			tbl[0x543] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUM(inst);
				};
			tbl[0x560] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUM(inst);
				};
			tbl[0x561] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUM(inst);
				};
			tbl[0x562] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUM(inst);
				};
			tbl[0x563] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUM(inst);
				};
			tbl[0x580] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SU(inst);
				};
			tbl[0x580] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SU(inst);
				};
			tbl[0x581] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SU(inst);
				};
			tbl[0x582] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SU(inst);
				};
			tbl[0x583] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SU(inst);
				};
			tbl[0x700] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUIC(inst);
				};
			tbl[0x7E2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUID(inst);
				};
			tbl[0x701] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUIC(inst);
				};
			tbl[0x702] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUIC(inst);
				};
			tbl[0x703] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUIC(inst);
				};
			tbl[0x720] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUIC(inst);
				};
			tbl[0x721] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUIC(inst);
				};
			tbl[0x722] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUIC(inst);
				};
			tbl[0x723] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUIC(inst);
				};
			tbl[0x740] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUIM(inst);
				};
			tbl[0x740] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUIM(inst);
				};
			tbl[0x741] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUIM(inst);
				};
			tbl[0x742] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUIM(inst);
				};
			tbl[0x743] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUIM(inst);
				};
			tbl[0x760] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUIM(inst);
				};
			tbl[0x761] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUIM(inst);
				};
			tbl[0x762] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUIM(inst);
				};
			tbl[0x763] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUIM(inst);
				};
			tbl[0x780] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUI(inst);
				};
			tbl[0x780] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUI(inst);
				};
			tbl[0x781] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUI(inst);
				};
			tbl[0x782] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUI(inst);
				};
			tbl[0x783] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUI(inst);
				};
			tbl[0x1E3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_UD(inst);
				};
			tbl[0x5E3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUD(inst);
				};
			tbl[0x7E3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUID(inst);
				};
			tbl[0x02C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_C(inst);
				};
			tbl[0x02F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_C(inst);
				};
			tbl[0x03C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_C(inst);
				};
			tbl[0x03E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_C(inst);
				};
			tbl[0x06C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_M(inst);
				};
			tbl[0x06F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_M(inst);
				};
			tbl[0x07C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_M(inst);
				};
			tbl[0x07E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_M(inst);
				};
			tbl[0x0A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT(inst);
				};
			tbl[0x0A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT(inst);
				};
			tbl[0x0A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT(inst);
				};
			tbl[0x0A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT(inst);
				};
			tbl[0x0A4] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTUN(inst);
				};
			tbl[0x0A5] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTEQ(inst);
				};
			tbl[0x0A6] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTLT(inst);
				};
			tbl[0x0A7] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTLE(inst);
				};
			tbl[0x0AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS(inst);
				};
			tbl[0x0AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ(inst);
				};
			tbl[0x0BC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS(inst);
				};
			tbl[0x0BE] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT(inst);
				};
			tbl[0x0C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_D(inst);
				};
			tbl[0x0C1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBSID(inst);
				};
			tbl[0x0C2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULSID(inst);
				};
			tbl[0x0C3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVSID(inst);
				};
			tbl[0x0EC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTSID(inst);
				};
			tbl[0x0EF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQD(inst);
				};
			tbl[0x0FC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_D(inst);
				};
			tbl[0x0FE] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_D(inst);
				};
			tbl[0x12C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_UC(inst);
				};
			tbl[0x12F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_VC(inst);
				};
			tbl[0x16C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_UM(inst);
				};
			tbl[0x16F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_VM(inst);
				};
			tbl[0x1A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_U(inst);
				};
			tbl[0x1A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_U(inst);
				};
			tbl[0x1A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_U(inst);
				};
			tbl[0x1A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_U(inst);
				};
			tbl[0x1AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_U(inst);
				};
			tbl[0x1AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_V(inst);
				};
			tbl[0x1C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_UD(inst);
				};
			tbl[0x1C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_UD(inst);
				};
			tbl[0x1C1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBSIUD(inst);
				};
			tbl[0x1C2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_UD(inst);
				};
			tbl[0x1C3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_UD(inst);
				};
			tbl[0x1EC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTSIUD(inst);
				};
			tbl[0x1EF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_VD(inst);
				};
			tbl[0x2AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTST(inst);
				};
			tbl[0x52C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUC(inst);
				};
			tbl[0x52F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVC(inst);
				};
			tbl[0x56C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUM(inst);
				};
			tbl[0x56F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVM(inst);
				};
			tbl[0x5A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SU(inst);
				};
			tbl[0x5A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SU(inst);
				};
			tbl[0x5A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SU(inst);
				};
			tbl[0x5A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SU(inst);
				};
			tbl[0x5A4] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTUN_SU(inst);
				};
			tbl[0x5A5] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTEQ_SU(inst);
				};
			tbl[0x5A6] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTLT_SU(inst);
				};
			tbl[0x5A7] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CMPTLE_SU(inst);
				};
			tbl[0x5AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SU(inst);
				};
			tbl[0x5AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SV(inst);
				};
			tbl[0x5C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUD(inst);
				};
			tbl[0x5C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUD(inst);
				};
			tbl[0x5C1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUD(inst);
				};
			tbl[0x5C2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUD(inst);
				};
			tbl[0x5C3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUD(inst);
				};
			tbl[0x5EC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUD(inst);
				};
			tbl[0x5EF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVD(inst);
				};
			tbl[0x6AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTST_S(inst);
				};
			tbl[0x72C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUIC(inst);
				};
			tbl[0x72F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVIC(inst);
				};
			tbl[0x73C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_SUC(inst);
				};
			tbl[0x73E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_SUC(inst);
				};
			tbl[0x76C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUIM(inst);
				};
			tbl[0x76F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVIM(inst);
				};
			tbl[0x77C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_SUM(inst);
				};
			tbl[0x77E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_SUM(inst);
				};
			tbl[0x7A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDT_SUI(inst);
				};
			tbl[0x7A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBT_SUI(inst);
				};
			tbl[0x7A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULT_SUI(inst);
				};
			tbl[0x7A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVT_SUI(inst);
				};
			tbl[0x7AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUI(inst);
				};
			tbl[0x7AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVI(inst);
				};
			tbl[0x7BC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_SU(inst);
				};
			tbl[0x7BE] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_SUI(inst);
				};
			tbl[0x7C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUID(inst);
				};
			tbl[0x7C0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_ADDS_SUID(inst);
				};
			tbl[0x7C1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_SUBS_SUID(inst);
				};
			tbl[0x7C2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MULS_SUID(inst);
				};
			tbl[0x7C3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_DIVS_SUID(inst);
				};
			tbl[0x7EC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTS_SUID(inst);
				};
			tbl[0x7EF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTTQ_SVID(inst);
				};
			tbl[0x7FC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQS_SUD(inst);
				};
			tbl[0x7FE] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQT_SUD(inst);
				};
			tbl[0x10] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTLQ(inst);
				};
			tbl[0x20] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CPYS(inst);
				};
			tbl[0x21] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CPYSN(inst);
				};
			tbl[0x22] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CPYSE(inst);
				};
			tbl[0x24] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MT_FPCR(inst);
				};
			tbl[0x25] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_MF_FPCR(inst);
				};
			tbl[0x30] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_CVTQL(inst);
				};
			tbl[0x02A] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_FCMOVEQ(inst);
				};
			tbl[0x02B] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_FCMOVNE(inst);
				};
			tbl[0x02C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_FCMOVLT(inst);
				};
			tbl[0x02D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_FCMOVGE(inst);
				};
			tbl[0x02E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_FCMOVLE(inst);
				};
			tbl[0x02F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_Alpha&>(base);
				FloatingPointInstruction_Alpha::emitAlpha_FCMOVGT(inst);
				};





		}
		static void buildAlphaPalTable(std::array<Handler, 65536>& tbl) {

			tbl[0x0000] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_HALT(inst);
				};
			tbl[0x0001] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CFLUSH(inst);
				};
			tbl[0x0002] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_DRAINA(inst);
				};
			tbl[0x0003] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_LDQP(inst);
				};
			tbl[0x0004] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_STQP(inst);
				};
			tbl[0x0006] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_ASN(inst);
				};
			tbl[0x0007] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_ASTEN(inst);
				};
			tbl[0x0008] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_ASTSR(inst);
				};
			tbl[0x0009] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CSERVE(inst);
				};
			tbl[0x0010] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_MCES(inst);
				};
			tbl[0x0011] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_MCES(inst);
				};
			tbl[0x0012] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_PCBB(inst);
				};
			tbl[0x0013] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_PRBR(inst);
				};
			tbl[0x0014] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_PRBR(inst);
				};
			tbl[0x0015] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_PTBR(inst);
				};
			tbl[0x0016] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_SCBB(inst);
				};
			tbl[0x0017] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_SCBB(inst);
				};
			tbl[0x0018] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_SIRR(inst);
				};
			tbl[0x0019] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_SISR(inst);
				};
			tbl[0x0020] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_SSP(inst);
				};
			tbl[0x0021] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_SSP(inst);
				};
			tbl[0x0022] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_USP(inst);
				};
			tbl[0x0023] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_USP(inst);
				};
			tbl[0x0024] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_TBISD(inst);
				};
			tbl[0x0025] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_TBISI(inst);
				};
			tbl[0x0026] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_ASTEN(inst);
				};
			tbl[0x0027] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_ASTSR(inst);
				};
			tbl[0x0029] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_VPTB(inst);
				};
			tbl[0x0030] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_SWPCTX(inst);
				};
			tbl[0x0031] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRVAL(inst);
				};
			tbl[0x0032] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RDVAL(inst);
				};
			tbl[0x0033] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_TBI(inst);
				};
			tbl[0x0034] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRENT(inst);
				};
			tbl[0x0035] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_SWPIPL(inst);
				};
			tbl[0x0036] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RDPS(inst);
				};
			tbl[0x0037] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRKGP(inst);
				};
			tbl[0x0038] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRUSP(inst);
				};
			tbl[0x0039] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRPERFMON(inst);
				};
			tbl[0x0080] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_BPT(inst);
				};
			tbl[0x0081] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_BUGCHK(inst);
				};
			tbl[0x0082] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CHME(inst);
				};
			tbl[0x0083] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CHMK(inst);
				};
			tbl[0x0084] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CHMS(inst);
				};
			tbl[0x0085] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CHMU(inst);
				};
			tbl[0x0086] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_IMB(inst);
				};
			tbl[0x0087] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQHIL(inst);
				};
			tbl[0x0088] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQTIL(inst);
				};
			tbl[0x0089] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQHIQ(inst);
				};
			tbl[0x0090] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_PROBEW(inst);
				};
			tbl[0x0091] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RD_PS(inst);
				};
			tbl[0x0092] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REI(inst);
				};
			tbl[0x0093] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQHIL(inst);
				};
			tbl[0x0094] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQTIL(inst);
				};
			tbl[0x0095] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQHIQ(inst);
				};
			tbl[0x0096] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQTIQ(inst);
				};
			tbl[0x0097] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQUEL(inst);
				};
			tbl[0x0098] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQUEQ(inst);
				};
			tbl[0x0099] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQUEL / D(inst);
				};
			tbl[0x ? ? ? 3] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REBOOT(inst);
				};
			tbl[0x000A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_SWPPAL(inst);
				};
			tbl[0x000B] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_FEN(inst);
				};
			tbl[0x000C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_FEN(inst);
				};
			tbl[0x000D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_IPIR(inst);
				};
			tbl[0x000E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_IPL(inst);
				};
			tbl[0x000F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_IPL(inst);
				};
			tbl[0x001A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_TBCHK(inst);
				};
			tbl[0x001B] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_TBIA(inst);
				};
			tbl[0x001C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_TBIAP(inst);
				};
			tbl[0x001D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_TBIS(inst);
				};
			tbl[0x001E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_ESP(inst);
				};
			tbl[0x001F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_ESP(inst);
				};
			tbl[0x002A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_VPTB(inst);
				};
			tbl[0x002B] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_PERFMON(inst);
				};
			tbl[0x002D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRVPTPTR(inst);
				};
			tbl[0x002E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MTPR_DATFX(inst);
				};
			tbl[0x003A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RDUSP(inst);
				};
			tbl[0x003C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WHAMI(inst);
				};
			tbl[0x003D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RETSYS(inst);
				};
			tbl[0x003E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WTINT(inst);
				};
			tbl[0x003F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_MFPR_WHAMI(inst);
				};
			tbl[0x008A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQTIQ(inst);
				};
			tbl[0x008B] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQUEL(inst);
				};
			tbl[0x008C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQUEQ(inst);
				};
			tbl[0x008D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQUEUL / D(inst);
				};
			tbl[0x008E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQUEQ / D(inst);
				};
			tbl[0x008F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_PROBER(inst);
				};
			tbl[0x009A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQUEQ / D(inst);
				};
			tbl[0x009B] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_SWASTEN(inst);
				};
			tbl[0x009C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WR_PS_SW(inst);
				};
			tbl[0x009D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RSCC(inst);
				};
			tbl[0x009E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_READ_UNQ(inst);
				};
			tbl[0x009F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_WRITE_UNQ(inst);
				};
			tbl[0x00A0] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_AMOVRR(inst);
				};
			tbl[0x00A1] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_AMOVRM(inst);
				};
			tbl[0x00A2] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQHILR(inst);
				};
			tbl[0x00A3] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQTILR(inst);
				};
			tbl[0x00A4] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQHIQR(inst);
				};
			tbl[0x00A5] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_INSQTIQR(inst);
				};
			tbl[0x00A6] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQHILR(inst);
				};
			tbl[0x00A7] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQTILR(inst);
				};
			tbl[0x00A8] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQHIQR(inst);
				};
			tbl[0x00A9] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_REMQTIQR(inst);
				};
			tbl[0x00AA] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_GENTRAP(inst);
				};
			tbl[0x00AB] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RDTEB(inst);
				};
			tbl[0x00AC] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_KBPT(inst);
				};
			tbl[0x00AD] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CALLKD(inst);
				};
			tbl[0x00AE] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CLRFEN(inst);
				};
			tbl[0x0E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_RFE(inst);
				};
			tbl[0xAB] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_KBPT(inst);
				};
			tbl[0x9998] = [](const Instruction& base) { //TODO - Verification Required
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_CALL_PAL(inst);
				};
			tbl[0x9999 ] = [](const Instruction& base) { //TODO - Verification Required
				auto& inst = static_cast<const PalInstruction_Alpha&>(base);
				PalInstruction_Alpha::emitAlpha_SSW(inst);
				};

		}
// 		static void buildAlphaConsoleTable(std::array<Handler, 65536>& tbl) {
// 			// TODO
// 		}
		/**/
		static void buildVaxFpTable(std::array<Handler, 65536>& tbl) {

			tbl[0x082] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF(inst);
				};
			tbl[0x083] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF(inst);
				};
			tbl[0x100] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_UC(inst);
				};
			tbl[0x101] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_UC(inst);
				};
			tbl[0x102] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_UC(inst);
				};
			tbl[0x103] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_UC(inst);
				};
			tbl[0x120] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_UC(inst);
				};
			tbl[0x121] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_UC(inst);
				};
			tbl[0x122] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_UC(inst);
				};
			tbl[0x123] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_UC(inst);
				};
			tbl[0x180] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_U(inst);
				};
			tbl[0x181] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_U(inst);
				};
			tbl[0x182] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_U(inst);
				};
			tbl[0x183] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_U(inst);
				};
			tbl[0x400] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_SC(inst);
				};
			tbl[0x401] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_SC(inst);
				};
			tbl[0x402] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_SC(inst);
				};
			tbl[0x403] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_SC(inst);
				};
			tbl[0x420] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_SC(inst);
				};
			tbl[0x421] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_SC(inst);
				};
			tbl[0x422] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_SC(inst);
				};
			tbl[0x423] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_SC(inst);
				};
			tbl[0x480] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_IS(inst);
				};
			tbl[0x481] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_S(inst);
				};
			tbl[0x482] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_S(inst);
				};
			tbl[0x483] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_S(inst);
				};
			tbl[0x500] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_SUC(inst);
				};
			tbl[0x501] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_SUC(inst);
				};
			tbl[0x502] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_SUC(inst);
				};
			tbl[0x503] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_SUC(inst);
				};
			tbl[0x520] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_SUC(inst);
				};
			tbl[0x521] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_SUC(inst);
				};
			tbl[0x522] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_SUC(inst);
				};
			tbl[0x523] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_SUC(inst);
				};
			tbl[0x580] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_SU(inst);
				};
			tbl[0x581] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_SU(inst);
				};
			tbl[0x582] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_SU(inst);
				};
			tbl[0x583] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_SU(inst);
				};
			tbl[0x0A7] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CMPGLE(inst);
				};

			tbl[0x1A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_U(inst);
				};
			tbl[0x1AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_U(inst);
				};
			tbl[0x1AD] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_U(inst);
				};
			tbl[0x1AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ(inst);
				};
			tbl[0x41E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_SC(inst);
				};
			tbl[0x42C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_SC(inst);
				};
			tbl[0x42D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_SC(inst);
				};
			tbl[0x42F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ_SC(inst);
				};
			tbl[0x49E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_S(inst);
				};
			tbl[0x4A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_S(inst);
				};
			tbl[0x4A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_S(inst);
				};
			tbl[0x4A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_S(inst);
				};
			tbl[0x4A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_S(inst);
				};
			tbl[0x4A5] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CMPGEQ_C(inst);
				};
			tbl[0x4A6] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CMPGLT_C(inst);
				};
			tbl[0x4A7] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CMPGLE_C(inst);
				};
			tbl[0x4AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_S(inst);
				};
			tbl[0x4AD] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_S(inst);
				};
			tbl[0x4AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ_S(inst);
				};
			tbl[0x51E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_SUC(inst);
				};
			tbl[0x52C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_SUC(inst);
				};
			tbl[0x52D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_SUC(inst);
				};
			tbl[0x52F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ_SVC(inst);
				};
			tbl[0x59E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_SU(inst);
				};
			tbl[0x5A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_SU(inst);
				};
			tbl[0x5A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_SU(inst);
				};
			tbl[0x5A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_SU(inst);
				};
			tbl[0x5A3] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_SU(inst);
				};
			tbl[0x5AC] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_SU(inst);
				};
			tbl[0x5AD] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_SU(inst);
				};
			tbl[0x5AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ_SV(inst);
				};

			tbl[0x01E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_C(inst);
				};
			tbl[0x02C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_C(inst);
				};
			tbl[0x02D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_C(inst);
				};
			tbl[0x02F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ_C(inst);
				};
			tbl[0x02F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTBQ(inst);
				};
			tbl[0x03C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTQF_C(inst);
				};
			tbl[0x03E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTQG_C(inst);
				};

			tbl[0x081] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF(inst);
				};

			tbl[0x080] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF(inst);
				};
			tbl[0x000] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDF_C(inst);
				};
			tbl[0x001] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBF_C(inst);
				};
			tbl[0x002] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULF_C(inst);
				};
			tbl[0x003] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVF_C(inst);
				};
			tbl[0x020] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_C(inst);
				};
			tbl[0x021] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_C(inst);
				};
			tbl[0x022] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_C(inst);
				};
			tbl[0x023] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_DIVG_C(inst);
				};
			tbl[0x0AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ(inst);
				};
			tbl[0x12F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTBQ(inst);
				};
			tbl[0x4A4] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTBQ(inst);
				};
			tbl[0x5AF] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTBQ(inst);
				};
			tbl[0x52F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTBQ(inst);
				};
			tbl[0x11E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_UC(inst);
				};
			tbl[0x12C] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGF_UC(inst);
				};
			tbl[0x12D] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGD_UC(inst);
				};
			tbl[0x12F] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTGQ_NC(inst);
				};
			tbl[0x19E] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_CVTDG_U(inst);
				};
			tbl[0x1A0] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_ADDG_U(inst);
				};
			tbl[0x1A1] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_SUBG_U(inst);
				};
			tbl[0x1A2] = [](const Instruction& base) {
				auto& inst = static_cast<const FloatingPointInstruction_VAX&>(base);
				FloatingPointInstruction_VAX::emitVAX_MULG_U(inst);
				};
		}
		static void buildTru64PalTable(std::array<Handler, 65536>& tbl) {


			tbl[0x0000] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_HALT(inst);
				};
			tbl[0x0001] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_CFLUSH(inst);
				};
			tbl[0x0002] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_DRAINA(inst);
				};
			tbl[0x0004] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_INITPAL(inst);
				};
			tbl[0x0006] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_SWPIRQL(inst);
				};
			tbl[0x0007] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDIRQL(inst);
				};
			tbl[0x0008] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_DI(inst);
				};
			tbl[0x0009] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_CSERVE(inst);
				};
			tbl[0x0010] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDMCES(inst);
				};
			tbl[0x0010] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_SWPCTX(inst);
				};
			tbl[0x0011] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRMCES(inst);
				};
			tbl[0x12] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDPCBB(inst);
				};
			tbl[0x0013] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRVIRBBND(inst);
				};
			tbl[0x0014] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_wrsysptb(inst);
				};
			tbl[0x15] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_THIS(inst);
				};
			tbl[0x0016] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_DTBIS(inst);
				};
			tbl[0x0018] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDKSP(inst);
				};
			tbl[0x0019] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_SWPKSP(inst);
				};
			tbl[0x0030] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDCOUNTERS(inst);
				};
			tbl[0x0031] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRVAL(inst);
				};
			tbl[0x0032] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDVAL(inst);
				};
			tbl[0x0033] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_TBI(inst);
				};
			tbl[0x0034] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRENT(inst);
				};
			tbl[0x0035] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_SWPIPL(inst);
				};
			tbl[0x0036] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDPS(inst);
				};
			tbl[0x0037] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRKGP(inst);
				};
			tbl[0x0038] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRUSP(inst);
				};
			tbl[0x0039] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRPERFMON(inst);
				};
			tbl[0x0080] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_BPT(inst);
				};
			tbl[0x0081] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_BUGCHK(inst);
				};
			tbl[0x0083] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_CALLSYS(inst);
				};
			tbl[0x0086] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_IMB(inst);
				};
			tbl[0x0092] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_URTI(inst);
				};
			tbl[0x ? ? 13] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRPRBR_TOCHECK(inst);
				};
			tbl[0x ? ? 14] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_TBIA_to_verify(inst);
				};
			tbl[0x ? ? 17] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_TBIMSASN(inst);
				};
			tbl[0x000A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_SWPPAL(inst);
				};
			tbl[0x000C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_SSIR(inst);
				};
			tbl[0x000D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRIPIR(inst);
				};
			tbl[0x000E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RFE(inst);
				};
			tbl[0x001A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDPSR(inst);
				};
			tbl[0x001C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDPER(inst);
				};
			tbl[0x001E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDTHREAD(inst);
				};
			tbl[0x002B] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRFEN(inst);
				};
			tbl[0x002D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRVPTPTR(inst);
				};
			tbl[0x002E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRASN(inst);
				};
			tbl[0x003A] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDUSP(inst);
				};
			tbl[0x003C] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WHAMI(inst);
				};
			tbl[0x003D] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RETSYS(inst);
				};
			tbl[0x003E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WTINT(inst);
				};
			tbl[0x003F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RTI(inst);
				};
			tbl[0x009E] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDUNIQUE(inst);
				};
			tbl[0x009F] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_WRUNIQUE(inst);
				};
			tbl[0x00AA] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_GENTRAP(inst);
				};
			tbl[0x00AB] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_RDTEB(inst);
				};
			tbl[0x00AC] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_KBPT(inst);
				};
			tbl[0x00AD] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_CALLKD(inst);
				};
			tbl[0x00AE] = [](const Instruction& base) {
				auto& inst = static_cast<const PalInstruction_Tru64&>(base);
				PalInstruction_Tru64::emitTru64_CLRFEN(inst);
				};

		}
	};

} // namespace Arch
