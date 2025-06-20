#pragma once

// executorFmtIntegerLoadAndStore.h
// executorFmtIntegerLoadAndStore using QVector instead of std::array for its dispatch table
// Eliminates “incomplete array” errors by building the table at runtime.

// Note: requires QtCore/QVector

#pragma once

#include <QtCore/QVector>
#include "Assembler.h"
#include "structs/memoryInstruction.h"
#include "structs/operateInstruction.h"

using Assembler = assemblerSpace::Assembler;



class executorFmtIntegerLoadAndStore {
public:
	using opCode12_Handler = void (executorFmtIntegerLoadAndStore::*)(const OperateInstruction&);
	using opCode13_Handler = void (executorFmtIntegerLoadAndStore::*)(const OperateInstruction&);

	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	explicit executorFmtIntegerLoadAndStore(Assembler& assembler)
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



			return tbl;
	}

	/**
	 * Return the singleton dispatch table.
	 * Initialized on first call in a thread?safe manner.
	 */
	static const QVector<opCode13_Handler>& dispatchTable() {
		static const QVector<opCode13_Handler> table = createDispatchTable();
		return table;
	}





	// … declare other handlers as needed …
};



