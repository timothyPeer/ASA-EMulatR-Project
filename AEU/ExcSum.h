#pragma once

#ifndef ExcSum_h__
#define ExcSum_h__

// ExcSum.h
#include <QtGlobal>
#include "../AEJ/JITFaultInfoStructures.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"

class ExcSum {
public:
	// Exception summary register bit definitions for memory faults
	static constexpr quint64 ACCESS_VIOLATION = 0x0000000000000001ULL;  // Insufficient access rights
	static constexpr quint64 FAULT_ON_READ = 0x0000000000000002ULL;  // Hardware error during read
	static constexpr quint64 TRANS_NOT_VALID = 0x0000000000000004ULL;  // Address translation failed
	static constexpr quint64 ALIGNMENT_FAULT = 0x0000000000000008ULL;  // Misaligned memory access
	static constexpr quint64 INSTRUCTION_FAULT = 0x0000000000000010ULL;  // Instruction fetch fault

	// Additional exception types (commonly in Alpha architecture)
	static constexpr quint64 ILLEGAL_INSTRUCTION = 0x0000000000000020ULL;  // Invalid opcode
	static constexpr quint64 ARITHMETIC_TRAP = 0x0000000000000040ULL;  // Arithmetic exception
	static constexpr quint64 FP_EXCEPTION = 0x0000000000000080ULL;  // Floating-point exception
	static constexpr quint64 INTERRUPT = 0x0000000000000100ULL;  // External interrupt
	static constexpr quint64 MACHINE_CHECK = 0x0000000000000200ULL;  // Hardware error
	static constexpr quint64 BREAKPOINT = 0x0000000000000400ULL;  // Software breakpoint
	static constexpr quint64 SYSCALL = 0x0000000000000800ULL;  // System call

	// Constructor with default of no exceptions
	ExcSum(quint64 value = 0) : m_value(value) {}

	// Get/set the raw value
	quint64 getValue() const { return m_value; }
	void setValue(quint64 value) { m_value = value; }

	// Check if a specific exception is set
	bool isSet(quint64 exceptionBit) const { return (m_value & exceptionBit) != 0; }

	// Set a specific exception
	void set(quint64 exceptionBit) { m_value |= exceptionBit; }

	// Clear a specific exception
	void clear(quint64 exceptionBit) { m_value &= ~exceptionBit; }

	// Clear all exceptions
	void clearAll() { m_value = 0; }

	// Get the exception type for a memory fault
	static quint64 getExceptionBitForFault(MemoryFaultType faultType) {
		switch (faultType) {
          case MemoryFaultType::ACCESS_VIOLATION:
             return ACCESS_VIOLATION;
          case MemoryFaultType::READ_ERROR:
		
		  case MemoryFaultType::FAULT_ON_READ:
			return FAULT_ON_READ;
	      case MemoryFaultType::TRANSLATION_NOT_VALID:
			return TRANS_NOT_VALID;
		  case MemoryFaultType::ALIGNMENT_FAULT:
			return ALIGNMENT_FAULT;
		  case MemoryFaultType::INSTRUCTION_ACCESS_FAULT:
			return INSTRUCTION_FAULT;
                case MemoryFaultType::INVALID_ADDRESS:
			return TRANS_NOT_VALID; // Most similar

		  case MemoryFaultType::FAULT_ON_WRITE:
			return FAULT_ON_READ; // Most similar
          case MemoryFaultType::WRITE_ERROR:
            return FAULT_ON_READ; // Most similar
		  case MemoryFaultType::MMIO_ERROR:
			return FAULT_ON_READ; // Most similar
		default:
			return 0;
		}
	}

private:
	quint64 m_value;  // Raw exception summary value
};
#endif // ExcSum_h__

