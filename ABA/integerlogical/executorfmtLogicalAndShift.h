// executorInteger.h
// LogicalAndShiftExecutor using QVector instead of std::array for its dispatch table
// Eliminates “incomplete array” errors by building the table at runtime.

// Note: requires QtCore/QVector

#pragma once

#include <QtCore/QVector>
#include "Assembler.h"
#include "structs/operateInstruction.h"

using Assembler = assemblerSpace::Assembler;



class LogicalAndShiftExecutor {
public:
	using Handler_ = void (LogicalAndShiftExecutor::*)(const OperateInstruction&);

	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	explicit LogicalAndShiftExecutor(Assembler& assembler)
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
		auto h = dispatchTable().at(idx);
		if (h) (this->*h)(i);
		else /* trap or fallback */;
		if (h) {
			(this->*h)(i);
		}
		else {
			// Unimplemented opcode: raise trap or fallback
		}
	}


private:
	Assembler& assembler;


	
	
	


	// … declare other handlers as needed …
};

