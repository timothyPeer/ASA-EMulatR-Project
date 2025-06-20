#pragma once
#pragma once

// executorFmtMbr.h
// executorFmtMbr using QVector instead of std::array for its dispatch table
// Eliminates “incomplete array” errors by building the table at runtime.

// Note: requires QtCore/QVector

#pragma once

#include <QtCore/QVector>
#include "Assembler.h"
#include "structs/memoryInstruction.h"
#include "structs/operateInstruction.h"
#include "executorFmtMbr.h"

using Assembler = assemblerSpace::Assembler;



class executorFmtMfc {
public:
	using opCode18_Handler = void (executorFmtMfc::*)(const MemoryInstruction&);


	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	explicit executorFmtMfc(Assembler& assembler)
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
		const QVector<opCode18_Handler>& table = dispatchTable();
		opCode18_Handler h = table.at(idx);
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
	static QVector<opCode18_Handler> createDispatchTable() {
		QVector<opCode18_Handler> tbl(64);
		tbl.fill(nullptr);
		tbl[0x0] = &executorFmtMbr::emitJMP;  // LDA
		tbl[0x1] = &executorFmtMbr::emitJSR;    // LDAH
		tbl[0x3] = &executorFmtMbr::emitJSR_COROUTINE;    // LDBU
		tbl[0x2] = &executorFmtMbr::emitRET;    // LDBU

		tbl[0xE800] = &executorFmtMbr::emitECB;    // 
		tbl[0x0400] = &executorFmtMbr::emitEXCB;

		tbl[0x8000] = &executorFmtMbr::emitFETCH;    // 
		tbl[0xA000] = &executorFmtMbr::emitFETCH_M;
		tbl[0xA000] = &executorFmtMbr::emitFETCH_M;
		tbl[0x4000] = &executorFmtMbr::emitMB;
		tbl[0xE000] = &executorFmtMbr::emitRC;
		tbl[0xC000] = &executorFmtMbr::emitRPCC;
		tbl[0xF000] = &executorFmtMbr::emitRS;
		tbl[0x0000] = &executorFmtMbr::emitTRAPB;
		tbl[0xF800] = &executorFmtMbr::emitWH64;
		tbl[0xFC00] = &executorFmtMbr::emitWH64EM;
		tbl[0x4400] = &executorFmtMbr::emitWMB;

		return tbl;
	}

	/**
	 * Return the singleton dispatch table.
	 * Initialized on first call in a thread?safe manner.
	 */
	static const QVector<opCode18_Handler>& dispatchTable() {
		static const QVector<opCode18_Handler> table = createDispatchTable();
		return table;
	}







	// … declare other handlers as needed …
};




