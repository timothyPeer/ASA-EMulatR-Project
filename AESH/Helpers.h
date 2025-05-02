#pragma once
#ifndef Helpers_h__
#define Helpers_h__

#include <QString>

// Protection flags for virtual memory
enum ProtectionFlags {
	Read = 0x1,
	Write = 0x2,
	Execute = 0x4
};


namespace helpers_JIT {

	// JIT optimization options
	struct Options {
		int optimizationLevel = 2;
		int traceCompilationThreshold = 50;
		int blockCompilationThreshold = 10;
		bool enableTraceCompilation = true;
	};

	enum class OptimizationLevel {
		NONE = 0,
		BASIC = 1,
		ADVANCED = 2,
		AGGRESSIVE = 3
	};

	struct ExecutionResult {
		int instructionsExecuted = 0;
		quint64 finalPC = 0;
		QVector<int> registers;
		QVector<double> fpRegisters;
		int compiledBlocks = 0;
		int compiledTraces = 0;
	};

	enum class Format {
		FORMAT_OPERATE,
		FORMAT_BRANCH,
		FORMAT_MEMORY,
		FORMAT_SYSTEM,
		FORMAT_VECTOR,
		FORMAT_MEMORY_BARRIER
	};

	struct InstructionDefinition {
		QString mnemonic;
		int opcode = 0;
		int functionCode = -1;
		QString instructionClass;
		QStringList operands;
		QString description;
	};

	enum class RegisterType {
		INTEGER_REG,
		FLOAT_REG,
		FLOATING_REG,
		SPECIAL_REG
	};

	enum class CPUState {
		IDLE,
		RUNNING,
		PAUSED,
		WAITING_FOR_INTERRUPT,
		EXCEPTION_HANDLING,
		HALTED,
		TRAPPED
	};

	enum class MmuMode {
		Kernel = 0,
		Executive = 1,
		Supervisor = 2,
		User = 3
	};

	enum class TrapType {
		PrivilegeViolation,
		MMUAccessFault,
		FloatingPointDisabled,
		ReservedInstruction,
		SoftwareInterrupt,
		ArithmeticTrap,
		Breakpoint,
		DivideByZero_int,
		DivideByZero_fp
	};

	enum ExceptionBit {
		SWC = 0,
		INV = 1,
		DZE = 2,
		OVF = 3,
		UNF = 4,
		INE = 5,
		IOV = 6
	};

	enum class ExceptionType {
		ARITHMETIC_TRAP = 0,
		ILLEGAL_INSTRUCTION = 2,
		PRIVILEGED_INSTRUCTION = 3,
		ALIGNMENT_FAULT = 4,
		MEMORY_ACCESS_VIOLATION = 5,
		MEMORY_READ_FAULT,
		MEMORY_WRITE_FAULT,
		MEMORY_EXECUTE_FAULT,
		MEMORY_ALIGNMENT_FAULT,
		PAGE_FAULT,
		INTEGER_OVERFLOW,
		INTEGER_DIVIDE_BY_ZERO,
		FLOATING_POINT_OVERFLOW,
		FLOATING_POINT_UNDERFLOW,
		FLOATING_POINT_DIVIDE_BY_ZERO,
		FLOATING_POINT_INVALID,
		RESERVED_OPERAND,
		MACHINE_CHECK,
		BUS_ERROR,
		SYSTEM_CALL,
		BREAKPOINT,
		INTERRUPT,
		HALT,
		UNKNOWN_EXCEPTION
	};

	inline quint32 qHash(const ExceptionType& key, uint seed = 0) {
		return ::qHash(static_cast<int>(key), seed);
	}

	enum PALFunction {
		PAL_HALT = 0x0000,
		PAL_WRKGP = 0x002E,
		PAL_WRUSP = 0x0030,
		PAL_RDUSP = 0x0031,
		PAL_SYSTEM_CALL = 0x0083,
		PAL_MACHINE_CHECK = 0x0002,
		PAL_BUS_ERROR = 0x0003
		// Add additional PAL functions here
	};
}

#endif // Helpers_h__
