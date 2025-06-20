#pragma once
#ifndef decodeOperate_h__
#define decodeOperate_h__

#include <QObject>

// Instruction decode format for Alpha AXP FP and Integer instructions
struct OperateInstruction
{
	quint8 opcode;
	quint8 ra;
	quint8 rb;
	quint8 rc;
	quint8 function;
	quint32 rawInstruction;
};
inline OperateInstruction decodeOperate(quint32 instr) {
	return {
		static_cast<quint8>((instr >> 26) & 0x3F),
		static_cast<quint8>((instr >> 21) & 0x1F),
		static_cast<quint8>((instr >> 16) & 0x1F),
		static_cast<quint8>(instr & 0x1F),
		static_cast<quint8>(instr & 0x3F)
	};
}

// Instruction sections/categories
enum class Section {
	SECTION_INTEGER,       // Integer operations
	SECTION_FLOATING_POINT, // Floating point operations
	SECTION_CONTROL,        // Control flow operations
	SECTION_PAL,            // PAL operations
	SECTION_VECTOR,         // Vector operations
	SECTION_MEMORY,         // Memory operations
	SECTION_OTHER           // Other operations
};

enum class Format {
	FORMAT_OPERATE,
	FORMAT_BRANCH,
	FORMAT_MEMORY,
	FORMAT_SYSTEM,
	FORMAT_VECTOR,
	FORMAT_MEMORY_BARRIER
};
#endif // decodeOperate_h__