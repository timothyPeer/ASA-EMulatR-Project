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
#include "../AEC/RegisterBank.h"

using Assembler = assemblerSpace::Assembler;



class executorFmtMbr {
public:
	using handler = void (executorFmtMbr::*)(const MemoryInstruction&);

	void attachRegisterBank(RegisterBank* regBank) { m_registerBank = regBank;  }

	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	explicit executorFmtMbr(Assembler& assembler)
		: assembler(assembler)
	{
	}

	/**
	 * Decode inst, look up the handler in a QVector, and dispatch.
	 */
	void execute(const MemoryInstruction& inst) {
		MemoryInstruction i = inst;
		i.decode();

		// Map primary opcode to subtable index
		static const QVector<quint8> primaries = { 0x1A };
		int pidx = primaries.indexOf(i.opcode);
		if (pidx < 0) return;  // unsupported opcode

		// Lookup in subtable, then by function code
		const auto& sub = dispatchTable()[pidx];
		int fidx = i.fnc & 0x7F;       // lower 7 bits
		handler h = sub.at(fidx);      // safe: sub is QVector<handler>
		if (h) (this->*h)(i);
	}
	// Returns the singleton 2-D dispatch table
	static const QVector<QVector<handler>>& dispatchTable() {
		static const QVector<QVector<handler>> table = createDispatchTable();
		return table;
	}

private:
	Assembler& assembler;
	RegisterBank* m_registerBank = nullptr;

	/**
	 * Build the 64-entry dispatch table once at startup.
	 * Entries default to nullptr, with only the implemented opcodes set.
	 */
	static QVector<QVector<handler>>  createDispatchTable() {
		QVector<QVector<handler>> all(1);

		auto& t1A = all[0]; // primary opcode 0x10
		t1A[0x0] = &executorFmtMbr::emitJMP;  // LDA
		t1A[0x1] = &executorFmtMbr::emitJSR;    // LDAH
		t1A[0x3] = &executorFmtMbr::emitJSR_COROUTINE;    // LDBU
		t1A[0x2] = &executorFmtMbr::emitRET;    // LDBU
		

		return all;
	}



#pragma region opCode 1A

	inline void emitJMP(const MemoryInstruction& inst) {
		// 1) compute updated PC (address of next instruction)
		quint64 nextPC = getPC() + 4;
		// 2) write return address
		m_registerBank->writeIntReg(inst.ra, nextPC);
		// 3) jump target: contents of Rb with low 2 bits ignored
		quint64 target = m_registerBank->readIntReg(inst.rb) & ~0x3ull;
		setPC(target);
	}

	// JSR: identical semantics to JMP, but hint bits differ :contentReference[oaicite:10]{index=10}
	inline void emitJSR(const MemoryInstruction& inst) {
		emitJMP(inst);
	}

	// RET: return from subroutine; same as JMP/JSR, hint bits indicate “pop” :contentReference[oaicite:11]{index=11}
	inline void emitRET(const MemoryInstruction& inst) {
		emitJMP(inst);
	}
	// JSR_COROUTINE: “pop then push” hint variant, identical execution :contentReference[oaicite:12]{index=12}
	inline void emitJSR_COROUTINE(const MemoryInstruction& inst) {
		emitJMP(inst);
	}


#pragma endregion opCode 1A







	// … declare other handlers as needed …
};




