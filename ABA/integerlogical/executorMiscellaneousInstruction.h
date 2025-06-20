#pragma once
#pragma once

// executorInteger.h
// ByteManipulationExecutor using QVector instead of std::array for its dispatch table
// Eliminates “incomplete array” errors by building the table at runtime.

// Note: requires QtCore/QVector

#pragma once

#include <QtCore/QVector>
#include "Assembler.h"
#include "structs/memoryInstruction.h"

using Assembler = assemblerSpace::Assembler;



class executorFmtByteManipulation {
public:
	using opCode12_Handler = void (executorFmtByteManipulation::*)(const OperateInstruction&);

	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	explicit executorFmtByteManipulation(Assembler& assembler)
		: assembler(assembler)
	{
	}

	/**
	 * Decode inst, look up the handler in a QVector, and dispatch.
	 */
	void execute(const OperateInstruction& inst) {
		OperateInstruction i = inst;
		i.decode();

		const auto idx = static_cast<int>(i.fnc & 0x3F);
		const QVector<opCode12_Handler>& table = dispatchTable();
		opCode12_Handler h = table.at(idx);
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
	static QVector<opCode12_Handler> createDispatchTable() {
		QVector<opCode12_Handler> tbl(64);
		tbl.fill(nullptr);

		tbl[0x06] = &MiscellaneousInstruction::emitExtBl;   // ExtBl
		
		tbl[0x39] = &LogicalAndShiftExecutor::emitSll;   // SLL
		tbl[0x3C] = &LogicalAndShiftExecutor::emitSrl;   // SRL
		tbl[0x34] = &LogicalAndShiftExecutor::emitSra;   // SRA
		tbl[0x30] = &executorFmtByteManipulation::emitZap;  // Zap
		tbl[0x31] = &executorFmtByteManipulation::emitZapNot;  // ZapNot
		// OP 10. CMPBGE

		return tbl;
	}

	/**
	 * Return the singleton dispatch table.
	 * Initialized on first call in a thread?safe manner.
	 */
	static const QVector<LogicalShiftHandler>& dispatchTable() {
		static const QVector<LogicalShiftHandler> table = createDispatchTable();
		return table;
	}





	// … declare other handlers as needed …
};

/*

Table 4–17: Miscellaneous Instructions Summary
Mnemonic Operation


ECB Evict Cache Block (mem)
EXCB Exception Barrier (mfc)
FETCH Prefetch Data (mem)
FETCH_M Prefetch Data, Modify Intent

MB Memory Barrier (mem)
PREFETCH Normal prefetch (mem)
PREFETCH_EN Prefetch Memory Data, Evict Next
PREFETCH_M Prefetch Memory Data with Modify Intent
PREFETCH_MEN Prefetch Memory Data with Modify Intent, Evict Next
RPCC Read Processor Cycle Counter (mem)
TRAPB Trap Barrier (mem)
WH64 Write Hint — 64 Bytes (mfc)
WH64EN Write Hint — 64 Bytes Evict Next (mem)
WMB Write Memory Barrier (mem)
*/

