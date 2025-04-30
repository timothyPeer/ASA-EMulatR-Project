#pragma once

#include "..\AESH\Helpers.h"
#include <QVector>

struct ExceptionVectorEntry {
	helpers_JIT::ExceptionType exceptionType;
	quint64 vectorAddress;
	QString description;
};

// Static table - fixed at boot
static const QVector<ExceptionVectorEntry> AlphaExceptionVectorTable = {
	{ helpers_JIT::ExceptionType::ARITHMETIC_TRAP,           0x0100, "Integer arithmetic trap" },
	{ helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION,       0x0200, "Illegal instruction" },
	{ helpers_JIT::ExceptionType::PRIVILEGED_INSTRUCTION,    0x0280, "Privileged instruction fault" },
	{ helpers_JIT::ExceptionType::ALIGNMENT_FAULT,           0x0300, "Alignment fault" },
	{ helpers_JIT::ExceptionType::MEMORY_ACCESS_VIOLATION,   0x0380, "Memory access violation" },
	{ helpers_JIT::ExceptionType::MEMORY_READ_FAULT,         0x0400, "Memory read fault" },
	{ helpers_JIT::ExceptionType::MEMORY_WRITE_FAULT,        0x0480, "Memory write fault" },
	{ helpers_JIT::ExceptionType::MEMORY_EXECUTE_FAULT,      0x0500, "Memory execute fault" },
	{ helpers_JIT::ExceptionType::MEMORY_ALIGNMENT_FAULT,    0x0580, "Memory alignment fault" },
	{ helpers_JIT::ExceptionType::PAGE_FAULT,                0x0600, "Page fault" },
	{ helpers_JIT::ExceptionType::INTEGER_OVERFLOW,          0x0680, "Integer overflow" },
	{ helpers_JIT::ExceptionType::INTEGER_DIVIDE_BY_ZERO,    0x0700, "Integer division by zero" },
	{ helpers_JIT::ExceptionType::FLOATING_POINT_OVERFLOW,   0x0780, "FP overflow" },
	{ helpers_JIT::ExceptionType::FLOATING_POINT_UNDERFLOW,  0x0800, "FP underflow" },
	{ helpers_JIT::ExceptionType::FLOATING_POINT_DIVIDE_BY_ZERO, 0x0880, "FP divide by zero" },
	{ helpers_JIT::ExceptionType::FLOATING_POINT_INVALID,    0x0900, "Invalid FP operation" },
	{ helpers_JIT::ExceptionType::RESERVED_OPERAND,          0x0980, "Reserved operand fault" },
	{ helpers_JIT::ExceptionType::MACHINE_CHECK,             0x0A00, "Machine check" },
	{ helpers_JIT::ExceptionType::BUS_ERROR,                 0x0A80, "Bus error" },
	{ helpers_JIT::ExceptionType::SYSTEM_CALL,               0x0B00, "System call" },
	{ helpers_JIT::ExceptionType::BREAKPOINT,                0x0B80, "Breakpoint" },
	{ helpers_JIT::ExceptionType::INTERRUPT,                 0x0C00, "External interrupt" },
	{ helpers_JIT::ExceptionType::HALT,                      0x0C80, "Halt instruction" },
	{ helpers_JIT::ExceptionType::UNKNOWN_EXCEPTION,         0x0D00, "Unknown exception" },
};
