#pragma once

// executorFmtBranch.h
// executorFmtBranch using QVector instead of std::array for its dispatch table
// Eliminates “incomplete array” errors by building the table at runtime.

// Note: requires QtCore/QVector

#pragma once

#include <QtCore/QVector>
#include "Assembler.h"
#include "structs/memoryInstruction.h"
#include "structs/operateInstruction.h"

using Assembler = assemblerSpace::Assembler;



class executorFmtBranch {
public:
	using opCodeMem_Handler = void (executorFmtBranch::*)(const MemoryInstruction&);


	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	explicit executorFmtBranch(Assembler& assembler)
		: assembler(assembler)
	{
	}

	/**
	 * Decode inst, look up the handler in a QVector, and dispatch.
	 */
	void execute(const MemoryInstruction& inst) {
		MemoryInstruction i = inst;
		i.decode();

		const auto idx = static_cast<int>(i.fnc & 0x3F);
		const QVector<opCodeMem_Handler>& table = dispatchTable();
		opCodeMem_Handler h = table.at(idx);
		if (h) {
			(this->*h)(i);
		}
		else {
			// Unimplemented opcode: raise trap or fallback
		}
	}

private:
	Assembler& assembler;

	/**
	 * Build the 64-entry dispatch table once at startup.
	 * Entries default to nullptr, with only the implemented opcodes set.
	 */
	static QVector<opCodeMem_Handler> createDispatchTable() {
		QVector<opCodeMem_Handler> tbl(64);
		tbl.fill(nullptr);
		tbl[0x08] = &executorFmtBranch::emitLDA;    // LDA
		tbl[0x09] = &executorFmtBranch::emitLDAH;    // LDAH
		tbl[0x0A] = &executorFmtBranch::emitLDBU;    // LDBU
		tbl[0x0C] = &executorFmtBranch::emitLDWU;    // LDWU
		tbl[0x20] = &executorFmtBranch::emitLDF;    // LDF
		tbl[0x21] = &executorFmtBranch::emitLDG;    // LDG
		tbl[0x28] = &executorFmtBranch::emitPREFETCH    // shared instruction... execute prefetch, then ldl
		tbl[0x28] = &executorFmtBranch::emitLDL;    // LDL
		tbl[0x2A] = &executorFmtBranch::emitLDL_L;    // LDL
		tbl[0x29] = &executorFmtBranch::emitPREFETCH_EN;    // shared instruction... execute prefetch_en, then ldq
		tbl[0x29] = &executorFmtBranch::emitLDQ;    // LDL
		tbl[0x2B] = &executorFmtBranch::emitLDQ_L;    // LDL
		tbl[0x0B] = &executorFmtBranch::emitLDQ_U    // LDL
		tbl[0x22] = &executorFmtBranch::emitPREFETCH_M;    // shared instruction... execute prefetch_m, then lds
		tbl[0x22] = &executorFmtBranch::emitLDS;    // LDL
		tbl[0x23] = &executorFmtBranch::emitPREFETCH_MEN;    // shared instruction... execute prefetch_men, then ldt
		tbl[0x23] = &executorFmtBranch::emitLDT;    // LDL
		tbl[0x0E] = &executorFmtBranch::emitSTB;    // LDL
		tbl[0x24] = &executorFmtBranch::emitSTF;    // LDL
		tbl[0x25] = &executorFmtBranch::emitSTG;    // LDL
		tbl[0x26] = &executorFmtBranch::emitSTS;    // LDL
		tbl[0x2C] = &executorFmtBranch::emitSTL;    // LDL
		tbl[0x2E] = &executorFmtBranch::emitSTL_C;    // LDL
		tbl[0x2D] = &executorFmtBranch::emitSTQ;    // LDL
		tbl[0x2F] = &executorFmtBranch::emitSTQ_C;    // LDL
		tbl[0x0F] = &executorFmtBranch::emitSTQ_U;    // LDL
		tbl[0x27] = &executorFmtBranch::emitSTT;    // LDL
		tbl[0x0D] = &executorFmtBranch::emitSTW;    // LDL

		return tbl;
	}

	/**
	 * Return the singleton dispatch table.
	 * Initialized on first call in a thread?safe manner.
	 */
	static const QVector<opCodeMem_Handler>& dispatchTable() {
		static const QVector<opCodeMem_Handler> table = createDispatchTable();
		return table;
	}


	




	// … declare other handlers as needed …
};




