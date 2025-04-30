#pragma once
#ifndef Helpers_h__
#define Helpers_h__


#include "FpRegisterBankcls.h"
#include "FPCRContext.h"
#include <limits>
#include <cmath>
#include <QtGlobal>
#include <QDebug>
#include <QHash>
#include <cstring>
#include <QScopedPointer>



//Macros

#define MEM_BARRIER()   std::atomic_thread_fence(std::memory_order_seq_cst)
#define MEM_WMB()       std::atomic_thread_fence(std::memory_order_release)
#define MEM_RMB()       std::atomic_thread_fence(std::memory_order_acquire)
#define TRAP_BARRIER()  std::atomic_thread_fence(std::memory_order_seq_cst)

#ifdef QT_DEBUG
#define DEBUG_LOG(msg) qDebug() << msg
#else
#define DEBUG_LOG(msg)
#endif

// Common Types



/**
 * @brief Floating-Point Control Register context (Alpha AXP FPCR)
 * Handles trap enables, sticky flags, rounding modes.
 */


/**
 * @brief Alpha AXP dt_gfloat: IEEE 754-1985 Double Precision Floating Point
 */

//class dt_gfloat {
// public:
// 	quint64 raw = 0; ///< 64-bit Alpha G floating value (raw bits)
// 
// 	static constexpr int EXP_BITS = 11;
// 	static constexpr int FRAC_BITS = 52;
// 	static constexpr int EXP_BIAS = 1024;
// 
// 	static constexpr quint64 SIGN_MASK = 0x8000000000000000ULL;
// 	static constexpr quint64 EXP_MASK = 0x7FF0000000000000ULL;
// 	static constexpr quint64 FRAC_MASK = 0x000FFFFFFFFFFFFFULL;
// 
// 	// Default constructor
// 	dt_gfloat() = default;
// 
// 	// Constructor from raw bits
// 	explicit dt_gfloat(quint64 rawBits) : raw(rawBits) {}
// 
// 	// Constructor from double
// 	explicit dt_gfloat(double value) { *this = fromDouble(value); }
// 
// 	/// Convert double → dt_gfloat (bitwise copy)
// 	static dt_gfloat fromDouble(double value) {
// 		dt_gfloat gf;
// 		std::memcpy(&gf.raw, &value, sizeof(double));
// 		return gf;
// 	}
// 
// 	/// Convert dt_gfloat → double (bitwise copy)
// 	double toDouble() const {
// 		double val;
// 		std::memcpy(&val, &raw, sizeof(double));
// 		return val;
// 	}
// 
// 	/// Returns sign bit (0=positive, 1=negative)
// 	bool sign() const { return raw & SIGN_MASK; }
// 
// 	/// Extracts exponent field (11 bits)
// 	int exponent() const { return (raw & EXP_MASK) >> FRAC_BITS; }
// 
// 	/// Extracts unbiased exponent (removes EXP_BIAS)
// 	qint64 unbiasedExponent() const { return exponent() - EXP_BIAS; }
// 
// 	/// Extracts fraction field (52 bits)
// 	quint64 fraction() const { return raw & FRAC_MASK; }
// 
// 	/// Tests special cases
// 	bool isZero() const { return (raw & ~SIGN_MASK) == 0; }
// 	bool isInf() const { return (exponent() == 0x7FF) && (fraction() == 0); }
// 	bool isNaN() const { return (exponent() == 0x7FF) && (fraction() != 0); }
// 	bool isDenormal() const { return (exponent() == 0) && (fraction() != 0); }
// 
// 	/// Arithmetic operators
// 	dt_gfloat operator+(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() + rhs.toDouble()); }
// 	dt_gfloat operator-(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() - rhs.toDouble()); }
// 	dt_gfloat operator*(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() * rhs.toDouble()); }
// 	dt_gfloat operator/(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() / rhs.toDouble()); }
// 
// 	/// Comparison operators
// 	bool operator==(const dt_gfloat& rhs) const { return toDouble() == rhs.toDouble(); }
// 	bool operator!=(const dt_gfloat& rhs) const { return toDouble() != rhs.toDouble(); }
// 	bool operator<(const dt_gfloat& rhs) const { return toDouble() < rhs.toDouble(); }
// 	bool operator>(const dt_gfloat& rhs) const { return toDouble() > rhs.toDouble(); }
// 	bool operator<=(const dt_gfloat& rhs) const { return toDouble() <= rhs.toDouble(); }
// 	bool operator>=(const dt_gfloat& rhs) const { return toDouble() >= rhs.toDouble(); }
// 
// 	/// Conversion from int64_t
// 	static dt_gfloat fromInt64(qint64 val) {
// 		return fromDouble(static_cast<double>(val));
// 	}
// 
// 	/// Conversion to int64_t with rounding based on FPCR
// 	qint64 toInt64(FPCRContext& fpcr) const {
// 		double val = toDouble();
// 		if (isNaN() && fpcr.trapInvalid())
// 			fpcr.setStickyInvalid();
// 		return static_cast<qint64>(applyRounding(val, fpcr));
// 	}
// 
// 	/// Applies rounding rules from FPCR
// 	static double applyRounding(double value, const FPCRContext& fpcr) {
// 		switch (fpcr.roundingMode()) {
// 		case 0: return std::nearbyint(value);      // round to nearest
// 		case 1: return (value > 0) ? std::floor(value) : std::ceil(value); // toward zero
// 		case 2: return std::ceil(value);           // toward +infinity
// 		case 3: return std::floor(value);          // toward -infinity
// 		default: return value;
// 		}
// 	}
// 
// 	/// Debug print helper
// 	friend QDebug operator<<(QDebug dbg, const dt_gfloat& gf) {
// 		dbg.nospace() << "GFloat("
// 			<< "raw=0x" << QString::number(gf.raw, 16)
// 			<< ", exp=" << gf.exponent()
// 			<< ", val=" << gf.toDouble()
// 			<< ")";
// 		return dbg.space();
// 	}
// };


// class dt_gfloat {
// public:
// 	quint64 raw = 0;
// 	
// 	FpRegisterBank* fpRegs;
// 
// 	static constexpr int EXP_BITS = 11;
// 	static constexpr int FRAC_BITS = 52;
// 	static constexpr int EXP_BIAS = 1024;
// 
// 	static constexpr quint64 SIGN_MASK = 0x8000000000000000ULL;
// 	static constexpr quint64 EXP_MASK = 0x7FF0000000000000ULL;
// 	static constexpr quint64 FRAC_MASK = 0x000FFFFFFFFFFFFFULL;
// 
// 	dt_gfloat()
// 	{
// 		fpRegs = new FpRegisterBank;
// 	}
// 	~dt_gfloat()
// 	{
// 		delete fpRegs;
// 	}
// 	explicit dt_gfloat(quint64 rawBits) : raw(rawBits) {}
// 	explicit dt_gfloat(double value) { *this = fromDouble(value); }
// 
// 	static dt_gfloat fromDouble(double value) {
// 		dt_gfloat gf;
// 		std::memcpy(&gf.raw, &value, sizeof(double));
// 		return gf;
// 	}
// 	// Math Functions: 
// 	void execADDF(const helpers_JIT::OperateInstruction& op) {
// 		dt_gfloat a = fpRegs->readFpReg(op.ra);
// 		dt_gfloat b = fpRegs->readFpReg(op.rb);
// 
// 		dt_gfloat result = a + b; // use operator+, NOT .add()
// 
// 		fpRegs->writeFpReg(op.rc, result);
// 	//	emit registerUpdated(op.rc, result.raw);
// 	}
// 
// 	double toDouble() const {
// 		double val;
// 		std::memcpy(&val, &raw, sizeof(double));
// 		return val;
// 	}
// 	/*                                                              */
// 	bool sign() const { return raw & SIGN_MASK; }
// 	int exponent() const { return (raw & EXP_MASK) >> FRAC_BITS; }
// 	qint64 unbiasedExponent() const { return exponent() - EXP_BIAS; }
// 	quint64 fraction() const { return raw & FRAC_MASK; }
// 
// 	bool isZero() const { return (raw & ~SIGN_MASK) == 0; }
// 	bool isInf() const { return (exponent() == 0x7FF) && (fraction() == 0); }
// 	bool isNaN() const { return (exponent() == 0x7FF) && (fraction() != 0); }
// 	bool isDenormal() const { return (exponent() == 0) && (fraction() != 0); }
// 
// 	dt_gfloat operator+(const dt_gfloat& rhs) const {
// 		return fromDouble(this->toDouble() + rhs.toDouble());
// 	}
// 	dt_gfloat operator-(const dt_gfloat& rhs) const {
// 		return fromDouble(this->toDouble() - rhs.toDouble());
// 	}
// 	dt_gfloat operator*(const dt_gfloat& rhs) const {
// 		return fromDouble(this->toDouble() * rhs.toDouble());
// 	}
// 	dt_gfloat operator/(const dt_gfloat& rhs) const {
// 		return fromDouble(this->toDouble() / rhs.toDouble());
// 	}
// 
// 	bool operator==(const dt_gfloat& rhs) const { return toDouble() == rhs.toDouble(); }
// 	bool operator!=(const dt_gfloat& rhs) const { return toDouble() != rhs.toDouble(); }
// 	bool operator<(const dt_gfloat& rhs) const { return toDouble() < rhs.toDouble(); }
// 	bool operator>(const dt_gfloat& rhs) const { return toDouble() > rhs.toDouble(); }
// 	bool operator<=(const dt_gfloat& rhs) const { return toDouble() <= rhs.toDouble(); }
// 	bool operator>=(const dt_gfloat& rhs) const { return toDouble() >= rhs.toDouble(); }
// 
// 	static dt_gfloat fromInt64(qint64 val) {
// 		return fromDouble(static_cast<double>(val));
// 	}
// 
// 	qint64 toInt64(FPCRContext& fpcr) const {
// 		double val = toDouble();
// 		if (isNaN() && fpcr.trapInvalid()) fpcr.setStickyInvalid();
// 		return static_cast<qint64>(applyRounding(val, fpcr));
// 	}
// 
// 	static double applyRounding(double value, const FPCRContext& fpcr) {
// 		switch (fpcr.roundingMode()) {
// 		case 0: return std::nearbyint(value); // round to nearest
// 		case 1: return (value > 0) ? std::floor(value) : std::ceil(value); // toward 0
// 		case 2: return std::ceil(value); // toward +inf
// 		case 3: return std::floor(value); // toward -inf
// 		default: return value;
// 		}
// 	}
// 
// 	friend QDebug operator<<(QDebug dbg, const dt_gfloat& gf) {
// 		dbg.nospace() << "GFloat("
// 			<< "raw=0x" << QString::number(gf.raw, 16)
// 			<< ", exp=" << gf.exponent()
// 			<< ", val=" << gf.toDouble()
// 			<< ")";
// 		return dbg.space();
// 	}
// };


	// Virtual Address → MappingEntry for AlphaMemorySystem
struct MappingEntry {
	quint64 physicalBase;
	quint64 size;
	quint32 protectionFlags; // Bitmask: READ=1, WRITE=2, EXECUTE=4
};

// Bitmask for AlphaMemory System Protection Flags
enum ProtectionFlags {
	Read = 0x1,
	Write = 0x2,
	Execute = 0x4
};

// Vector SMP Thread Struct

class AlphaCPU;
class QThread;
class CpuThreadBundle {
public:
	AlphaCPU* cpu;
	QThread* thread;
	CpuThreadBundle() = default;
	~CpuThreadBundle() {}
};

namespace helpers_JIT
{

	struct Options {
		int optimizationLevel = 2;
		int traceCompilationThreshold = 50;
		int blockCompilationThreshold = 10;
		bool enableTraceCompilation = true;
	};
	/**
   * Helper structure for instruction definitions
   */
// 	struct InstructionDefinition_str {
// 		QString mnemonic;
// 		int opcode;
// 		int functionCode;
// 		QString instructionClass;
// 		QStringList operands;
// 		QString description;
// 	};

	// Optimization levels
	enum class OptimizationLevel {
		NONE = 0,        // Direct translation, no optimization
		BASIC = 1,       // Basic optimizations (constant folding, etc.)
		ADVANCED = 2,    // Advanced optimizations (instruction scheduling, etc.)
		AGGRESSIVE = 3   // Aggressive optimizations (may be slower to compile)
	};

// 	struct Options {
// 		int optimizationLevel = 2;
// 		int traceCompilationThreshold = 50;
// 		int blockCompilationThreshold = 10;
// 		bool enableTraceCompilation = true;
// 	};

// 	struct ExecutionResult {
// 		int instructionsExecuted;
// 		quint64 finalPC;
// 		QVector<int> registers;
// 		QVector<double> fpRegisters;
// 		int compiledBlocks;
// 		int compiledTraces;
// 	};
	/**
	 * Helper structure for execution results
	 */
	struct  ExecutionResult {
		int instructionsExecuted;
		quint64 finalPC;
		QVector<int> registers;
		QVector<double> fpRegisters;
		int compiledBlocks;
		int compiledTraces;
	};

	// Instruction formats
	enum class Format {
		FORMAT_OPERATE,      // Register-based operate format
		FORMAT_BRANCH,       // Branch format
		FORMAT_MEMORY,       // Memory access format
		FORMAT_SYSTEM,       // System call format
		FORMAT_VECTOR,       // Vector operation format
		FORMAT_MEMORY_BARRIER // Memory barrier format
	};

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
	/**
  * Helper structure for instruction definitions
  */
	struct  InstructionDefinition {
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
		SPECIAL_REG
	};

	// TODO CPU_STATE:: Waiting for an interrupt, Exception Handling
	enum class CPUState {
		IDLE,						// CPU is (set to) Idle
		RUNNING,					// CPU is (set to) running
		PAUSED,						// CPU is (/was) paused
		WAITING_FOR_INTERRUPT,		// CPU is waiting for an interrupt TODO
		EXCEPTION_HANDLING,			// TODO
		HALTED,						// CPU was halted
		TRAPPED						// CPU state was changed due to a trap.
	};

	/**
	* @brief Memory Management Unit (MMU) mode (Ref: ASA I, 5-1)
	*/
		enum class MmuMode {
		Kernel = 0,      ///< Highest privilege level, full system access
		Executive = 1,   ///< High privilege, access to executive data
		Supervisor = 2,  ///< Intermediate privilege, OS services
		User = 3         ///< Lowest privilege, application code only
	};

	/**
	 * @brief Types of traps/exceptions that can occur during execution
	 */
	enum class TrapType {
		PrivilegeViolation,    ///< Access violation due to privilege level
		MMUAccessFault,        ///< Memory management unit fault
		FloatingPointDisabled, ///< FP instruction when FP disabled
		ReservedInstruction,   ///< Unimplemented instruction
		SoftwareInterrupt,
		ArithmeticTrap,
		Breakpoint
	};

	/**
	 * @brief Exception flag bits for tracking error states
	 */
	enum ExceptionBit {
		SWC = 0,  ///< Software completion
		INV = 1,  ///< Invalid operation
		DZE = 2,  ///< Division by zero
		OVF = 3,  ///< Overflow
		UNF = 4,  ///< Underflow
		INE = 5,  ///< Inexact result
		IOV = 6   ///< Integer overflow
	};
	

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
	/**
 * CPU Exception Types for Alpha Architecture
 *
 * These represent the various hardware exceptions that can be raised
 * during instruction execution in an Alpha processor.
 */
	enum class ExceptionType {
		ARITHMETIC_TRAP = 0,
		ILLEGAL_INSTRUCTION = 2, // Unimplemented instruction
		PRIVILEGED_INSTRUCTION = 3,// Instruction requires higher privilege
		ALIGNMENT_FAULT = 4,
		// Memory exceptions
		MEMORY_ACCESS_VIOLATION = 5,
		MEMORY_READ_FAULT,       // Read from inaccessible memory
		MEMORY_WRITE_FAULT,      // Write to inaccessible memory
		MEMORY_EXECUTE_FAULT,    // Execute from inaccessible memory
		MEMORY_ALIGNMENT_FAULT,  // Unaligned memory access
		PAGE_FAULT,              // Access to unmapped page

		// Arithmetic exceptions
		INTEGER_OVERFLOW,        // Integer overflow
		INTEGER_DIVIDE_BY_ZERO,  // Division by zero
		FLOATING_POINT_OVERFLOW, // FP overflow
		FLOATING_POINT_UNDERFLOW,// FP underflow
		FLOATING_POINT_DIVIDE_BY_ZERO, // FP division by zero
		FLOATING_POINT_INVALID,  // Invalid FP operation

		// Instruction exceptions
		RESERVED_OPERAND,        // Reserved operand fault

		// System exceptions
		MACHINE_CHECK,           // Hardware failure
		BUS_ERROR,               // System bus error
		SYSTEM_CALL,             // System call exception
		BREAKPOINT,              // Breakpoint exception

		// External exceptions
		INTERRUPT,               // External interrupt
		HALT,                    // HALT instruction

		// Other
		UNKNOWN_EXCEPTION        // Unknown/unclassified exception
	};
	inline uint qHash(const ExceptionType& key, uint seed = 0)
	{
		return ::qHash(static_cast<int>(key), seed);
	}

	enum PALFunction {
		PAL_HALT = 0x0000,
		PAL_WRKGP = 0x002E,
		PAL_WRUSP = 0x0030,
		PAL_RDUSP = 0x0031,
		PAL_SYSTEM_CALL = 0x0083,
		PAL_MACHINE_CHECK = 0x0002,
		PAL_BUS_ERROR = 0x0003,
		// Add more PAL functions as needed
	};

}


#endif // Helpers_h__