// RegisterBank.h
#pragma once

#include <QVector>
#include <QObject>
#include <QtGlobal>
#include <QScopedPointer>
#include <QVector>
#include "FpRegisterFile.h"
#include "RegisterFileWrapper.h"
#include "../AEJ/traps/trapFpType.h"
#include "../AEJ/enumerations/enumExceptionType.h"

/**
 * @brief Unified register bank for Integer and Floating-Point registers.
 *        This class manages the integer GPRs (R0–R30) and floating-point FPRs (F0–F30),
 *        as well as architectural zero-registers (R31, F31).
 */
class RegisterBank : public QObject {
	Q_OBJECT

public:
	RegisterBank(QObject* parent = nullptr)
		: QObject(parent), intRegs(32, 0), fpRegs(new RegisterFileWrapper(this)) {
	}


	// Underflow trap enable: UNFD bit (FPCR<61>) clear => trap enabled
	inline bool isUnderflowTrapEnabled() const {
		return !fpRegs->fpcr().bitTest(61);
	}
	// Set Underflow summary flag: UNF bit (FPCR<55>)
	inline void setUnderflowFlag() {
		fpRegs->fpcr().setBit(55);
	}

	// Overflow trap enable: OVFD bit (FPCR<51>) clear => trap enabled
	inline bool isOverflowTrapEnabled() const {
		return !fpRegs->fpcr().bitTest(51);
	}
	// Set Overflow summary flag: OVF bit (FPCR<54>)
	inline void setOverflowFlag() {
		fpRegs->fpcr().setBit(54);
	}

	// Inexact trap enable: INED bit (FPCR<62>) clear => trap enabled
	inline bool isInexactTrapEnabled() const {
		return !fpRegs->fpcr().bitTest(62);
	}
	// Set Inexact summary flag: CINE bit (FPCR<56>)
	inline void setInexactFlag() {
		fpRegs->fpcr().setBit(56);
	}
	double readFpReg(quint8 reg) const {
		return getFpBank()->readFpReg(reg);
	}

	void writeFpReg(quint8 reg, double value) {
		getFpBank()->writeFpReg(reg, value);
	}
	/* Direct access to the underlying FP register array ------------------- */
	FpRegs& fp() { return fpRegs->fp(); }        // non-const
	const FpRegs& fp() const { return fpRegs->fp(); }        // const

	/**
	 * @brief Get direct pointer to integer register array
	 *
	 * This method is primarily used for exception handling,
	 * where the entire register state needs to be saved.
	 *
	 * @return quint64* Pointer to the 32-element integer register array
	 */
	QVector<quint64> getIntRegisterArray() { return intRegs; }

	/**
 * @brief Handle arithmetic exceptions for integer operations
 */
	void handleArithmeticException(ExceptionType type) {
		// Handle integer arithmetic exceptions
		emit sigArithmeticExceptionRaised(type);
	}

	/**
 * @brief Handle floating-point exceptions based on type
 */
	void handleFloatingPointException(FPTrapType type) {
		// This would typically:
		// 1. Save current state
		// 2. Jump to exception handler
		// 3. Set appropriate processor state

		switch (type) {
		case FPTrapType::FP_INVALID_OPERATION:
			// Handle invalid operation exception
			break;
		case FPTrapType::FP_OVERFLOW:
			// Handle overflow exception  
			break;
		case FPTrapType::FP_UNDERFLOW:
			// Handle underflow exception
			break;
		default:
			break;
		}

		emit sigExceptionRaised(type);
	}

	/**
	 * @brief Set the base pointer to system memory for load/store access.
	 * @param base Pointer to the start of the memory region (byte-addressable).
	 */
	inline void setMemoryBasePointer(uint8_t* base) {
		m_memoryBase = base;
	}

	/**
	 * @brief Get the base pointer to system memory for load/store access.
	 * @return Pointer to the start of the memory region.
	 */
	inline uint8_t* basePointer() const {
		return m_memoryBase;
	}
	quint64 readInt(int reg) const { return intRegs.value(reg); }
	void writeInt(int reg, quint64 val) {
		if (reg != 31) intRegs[reg] = val;
		emit sigIntRegisterUpdated(reg, val);
	}

	/**
 * @brief Raise Invalid Operation exception status
 * This is triggered by operations like sqrt(-1), 0/0, inf-inf, etc.
 */
	void raiseStatus_InvalidOP() {
		// Set the Invalid Operation flag in the floating-point status register
		if (fpRegs) {
			fpRegs->setInvalidOperationFlag(true);

			// Optionally emit a signal for debugging/monitoring
			emit sigFpStatusUpdated("Invalid Operation");

			// In a real processor, this might also trigger an exception
			// depending on the exception enable bits
			if (fpRegs->isInvalidOperationTrapEnabled()) {
				// Trigger floating-point exception handling
				handleFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
			}
		}
	}


 /**
  * @brief Raise Invalid Overflow exception status (typically for integer operations)
  * This handles cases where integer arithmetic results exceed representable range
  */
	void raiseStatus_InvalidOverflow() {
		// This might be for integer overflow in arithmetic operations
		// Set appropriate status flags

		// For integer overflow, you might want to set a general arithmetic exception flag
		if (fpRegs) {
			fpRegs->setArithmeticExceptionFlag(true);
		}

		emit sigIntStatusUpdated("Invalid Overflow");

		// Handle the exception based on processor configuration
		// Fixed: Use correct arithmetic exception type instead of FP trap type
		handleArithmeticException(ExceptionType::ARITHMETIC_OVERFLOW);
	}

	/**
 * @brief Raise Overflow exception status for floating-point operations
 * This occurs when the result of an operation is too large to represent
 */
	void raiseStatus_OverFlow() {
		if (fpRegs) {
			// Set the Overflow flag in the floating-point status register
			fpRegs->setOverflowFlag(true);

			emit sigFpStatusUpdated("Floating-Point Overflow");

			// Check if overflow traps are enabled
			if (fpRegs->isOverflowTrapEnabled()) {
				handleFloatingPointException(FPTrapType::FP_OVERFLOW);
			}
		}
	}


	/**
	 * @brief Raise Underflow exception status for floating-point operations
	 * This occurs when the result of an operation is too small to represent accurately
	 */
	void raiseStatus_UnderFlow() {
		if (fpRegs) {
			// Set the Underflow flag in the floating-point status register
			fpRegs->setUnderflowFlag(true);

			emit sigFpStatusUpdated("Floating-Point Underflow");

			// Check if underflow traps are enabled
			if (fpRegs->isUnderflowTrapEnabled()) {
				handleFloatingPointException(FPTrapType::FP_UNDERFLOW);
			}
		}
	}

	// Read integer register (R0–R30); R31 is always zero
	quint64 readIntReg(quint8 reg) const {
		if (reg >= 31) return 0;
		return intRegs[reg];
	}

	// Write integer register (R0–R30); writing R31 is ignored
	void writeIntReg(quint8 reg, quint64 value) {
		if (reg < 31) intRegs[reg] = value;
	}

	quint64 getKernelGP() const {
		return intRegs[getKernelGP()];
	}

	void setKernelGP(quint64 value) {
		intRegs[getKernelGP()] = value;
		emit sigRegisterUpdated(getKernelGP(), value);  // if signal connected
	}
	RegisterFileWrapper* getFpBank() const { return fpRegs; }

signals:
	void sigIntRegisterUpdated(int reg, quint64 val);
	void sigRegisterUpdated(int reg, quint64 val);
	void sigExceptionRaised(FPTrapType except_);
	void sigFpStatusUpdated(const QString &status_);
	void sigIntStatusUpdated(const QString& status);
	void sigArithmeticExceptionRaised(ExceptionType type_);

private:
	QVector<quint64> intRegs;
	RegisterFileWrapper* fpRegs;
	uint8_t* m_memoryBase;              ///< Pointer to base of system memory
};

