#pragma once

// RegisterFileWrapper.h

#include <QObject>
#include <QtGlobal>
#include <QVector>
#include <cstring>
#include <QDebug>
#include "FpRegisterFile.h"
#include "../AEJ/GlobalMacro.h"
#include "../AEJ/JITFaultInfoStructures.h"
#include "../AEJ/traps/trapFpType.h"
#include "../AEJ/constants/const_PAL_Constants.h"
#include "../AEJ/constants/const_FPCR_AMASK.h"
#include "../AEJ/structures/structFpRegister.h"
#include "../AEJ/structures/structFpRegs.h"


class AlphaCPU;		// we need a forward declaration

/**
 * @brief Qt-compatible wrapper for FpRegs structure from FpRegisterFile.h.
 * Provides signal-slot integration, value read/write access to Int and FP, and access to FPCR.
 * Manages: 
     Floating-point registers: via regs (already present)
     FPCR (via regs.fpcr, mapped to raw[31])
	 Integer registers: newly introduced intRegs in the wrapper only
*/


class RegisterFileWrapper : public QObject {
	Q_OBJECT

public:


	explicit RegisterFileWrapper(QObject* parent = nullptr)
		: QObject(parent), intRegs(32, 0)  // initialize 32 elements
	{
		intRegs[31] = 0; 		// R31 is hardwired to 0 and never changes
	}

	void attachAlphaCPU(AlphaCPU* cpu_) { 
			m_alphaCPUPtr = cpu_;  
	}
	// =================== Integer Registers ===================

	void clearAllExceptionFlags() {
		m_fpRegs.fpcr.raw &= ~(AlphaFPCR::FPCR_INV |
			AlphaFPCR::FPCR_DZE |
			AlphaFPCR::FPCR_OVF |
			AlphaFPCR::FPCR_UNF |
			AlphaFPCR::FPCR_INE |
			AlphaFPCR::FPCR_IOV |
			AlphaFPCR::FPCR_SUM);
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}
	quint64 readIntReg(quint8 index) const {
		if (index < 32)
			return intRegs[index];
		return 0;  // R31 is always 0
	}



	void writeIntReg(quint8 index, quint64 value) {
		if (index < 31) // R0-R30 are writable
			intRegs[index] = value;
		// R31 is always 0
	}

	// =================== Floating-Point Registers ===================

	double readFp(FReg index) const {
		if (index < 32)
			return m_fpRegs.asDouble[index];
		return 0.0;
	}

	/**
	*@brief Set the Inexact Result flag in FPCR
	* This sets the INE bit in the FPCR status field
	*/
		void setInexactFlag(bool enable) {
		if (enable) {
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_INE;
			// Also set the summary bit when any exception occurs
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_SUM;
		}
		else {
			m_fpRegs.fpcr.raw &= ~AlphaFPCR::FPCR_INE;
		}
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}

	/**
	 * @brief Set the Integer Overflow flag in FPCR
	 * This sets the IOV bit in the FPCR status field
	 */
	void setIntegerOverflowFlag(bool enable) {
		if (enable) {
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_IOV;
			// Also set the summary bit when any exception occurs
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_SUM;
		}
		else {
			m_fpRegs.fpcr.raw &= ~AlphaFPCR::FPCR_IOV;
		}
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}
	/**
 * @brief Set a general arithmetic exception flag (for integer operations)
 * This is a convenience method that sets the Integer Overflow flag
 */
	void setArithmeticExceptionFlag(bool enable) {
		setIntegerOverflowFlag(enable);
	}

	// =================== Trap Enable Checking Methods ===================

	/**
	 * @brief Check if Invalid Operation traps are enabled
	 * @return true if INVD bit is set in FPCR
	 */
	bool isInvalidOperationTrapEnabled() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_INVD) != 0;
	}

	/**
 * @brief Check if Overflow traps are enabled
 * @return true if OVFD bit is set in FPCR
 */
	bool isOverflowTrapEnabled() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_OVFD) != 0;
	}

	/**
	 * @brief Check if Underflow traps are enabled
	 * @return true if UNFD bit is set in FPCR
	 */
	bool isUnderflowTrapEnabled() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_UNFD) != 0;
	}

	/**
	 * @brief Check if Divide by Zero traps are enabled
	 * @return true if DZED bit is set in FPCR
	 */
	bool isDivideByZeroTrapEnabled() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_DZED) != 0;
	}

	/**
 * @brief Check if Inexact traps are enabled
 * @return true if INED bit is set in FPCR
 */
	bool isInexactTrapEnabled() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_INED) != 0;
	}

	// =================== Status Flag Query Methods ===================
	/**
	 * @brief Set the Invalid Operation flag in FPCR
	 * This sets the INV bit in the FPCR status field
	 */
	void setInvalidOperationFlag(bool enable) {
		if (enable) {
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_INV;
			// Also set the summary bit when any exception occurs
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_SUM;
		}
		else {
			m_fpRegs.fpcr.raw &= ~AlphaFPCR::FPCR_INV;
		}
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}
	/**
 * @brief Check if Invalid Operation flag is set
 * @return true if INV bit is set in FPCR
 */
	bool isInvalidOperationFlagSet() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_INV) != 0;
	}

	/**
	 * @brief Check if Overflow flag is set
	 * @return true if OVF bit is set in FPCR
	 */
	bool isOverflowFlagSet() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_OVF) != 0;
	}

	/**
	 * @brief Check if Underflow flag is set
	 * @return true if UNF bit is set in FPCR
	 */
	bool isUnderflowFlagSet() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_UNF) != 0;
	}

	/**
	 * @brief Check if Divide by Zero flag is set
	 * @return true if DZE bit is set in FPCR
	 */
	bool isDivideByZeroFlagSet() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_DZE) != 0;
	}

	/**
	 * @brief Check if Inexact flag is set
	 * @return true if INE bit is set in FPCR
	 */
	bool isInexactFlagSet() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_INE) != 0;
	}

	/**
	 * @brief Check if Integer Overflow flag is set
	 * @return true if IOV bit is set in FPCR
	 */
	bool isIntegerOverflowFlagSet() const {
		return (m_fpRegs.fpcr.raw & AlphaFPCR::FPCR_IOV) != 0;
	}

	/**
	 * @brief Set the Overflow flag in FPCR
	 * This sets the OVF bit in the FPCR status field
	 */
	void setOverflowFlag(bool enable) {
		if (enable) {
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_OVF;
			// Also set the summary bit when any exception occurs
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_SUM;
		}
		else {
			m_fpRegs.fpcr.raw &= ~AlphaFPCR::FPCR_OVF;
		}
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}

	/**
 * @brief Set the Underflow flag in FPCR
 * This sets the UNF bit in the FPCR status field
 */
	void setUnderflowFlag(bool enable) {
		if (enable) {
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_UNF;
			// Also set the summary bit when any exception occurs
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_SUM;
		}
		else {
			m_fpRegs.fpcr.raw &= ~AlphaFPCR::FPCR_UNF;
		}
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}

	/**
	 * @brief Set the Divide by Zero flag in FPCR
	 * This sets the DZE bit in the FPCR status field
	 */
	void setDivideByZeroFlag(bool enable) {
		if (enable) {
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_DZE;
			// Also set the summary bit when any exception occurs
			m_fpRegs.fpcr.raw |= AlphaFPCR::FPCR_SUM;
		}
		else {
			m_fpRegs.fpcr.raw &= ~AlphaFPCR::FPCR_DZE;
		}
		emit sigFpcrUpdated(m_fpRegs.fpcr.raw);
	}
	void writeFp(FReg reg, double value) {
		this->m_fpRegs.asDouble[static_cast<int>(reg)] = value;
	}

	// Direct access for exception handling
	quint64* getIntRegisterArray() {
		return intRegs.data();
	}
	
	
	quint64 readRaw(FReg reg) const {
		return m_fpRegs.raw[static_cast<int>(reg)];
	}

	void writeRaw(FReg reg, quint64 value) {
		m_fpRegs.raw[static_cast<int>(reg)] = value;
	}

	double readFpReg(quint8 reg) const {
		if (reg >= 32) return 0.0;
		return m_fpRegs.asDouble[reg];
	}

	void writeFpReg(quint8 reg, double value) {
		if (reg < 31) {
			m_fpRegs.asDouble[reg] = value;
		}
		else if (reg == 31) {
			quint64 rawBits;
			static_assert(sizeof(rawBits) == sizeof(double), "Size mismatch");
			std::memcpy(&rawBits, &value, sizeof(double));
			m_fpRegs.raw[31] = rawBits;
		}
	}

	// =================== FPCR Access ===================

	quint64 getFPCR() const {
		return m_fpRegs.fpcr.raw;
	}

	FpcrRegister& getFpcrRegister() {
		return m_fpRegs.fpcr;
	}

	FpRegs& fp() { return m_fpRegs; }
	void writeFpcr(const FpcrRegister& newFpcr) {
		m_fpRegs.fpcr = newFpcr;
	}

	FpcrRegister readFpcr() const {
		return m_fpRegs.fpcr;
	}

	void writeFpcrRaw(quint64 raw) {
		m_fpRegs.fpcr = FpcrRegister::fromRaw(raw);
	}

	quint64 readFpcrRaw() const {
		return m_fpRegs.fpcr.toRaw();
	}

	FpcrRegister& fpcr() {
		return m_fpRegs.fpcr;
	}

	const FpcrRegister& fpcr() const {
		return m_fpRegs.fpcr;
	}

	AlphaCPU* getCurrentCPU() { 
		Q_ASSERT(m_alphaCPUPtr);
		return m_alphaCPUPtr; }

	// Get current processor mode (0=kernel, 1=executive, 2=supervisor, 3=user)
	quint64 getCurrentMode() const {
		// This is typically stored in the processor status register
		// For Alpha, it's often in the PS (Processor Status) register
		// For simplicity, let's assume it's extracted from a private member
		return m_currentMode;
	}
	// =================== Debug Dump ===================

	void dump() const {
		DEBUG_LOG("=== Integer Registers ===");
		for (int i = 0; i < 32; ++i)
		{
			DEBUG_LOG(QString("R%1: 0x%2")
				.arg(i)
				.arg(QString::number(readIntReg(i), 16).rightJustified(16, '0')));
		}
		DEBUG_LOG("\n=== Floating-Point Registers ===");
		for (int i = 0; i < 32; ++i) {
// 			DEBUG_LOG(
// 				QString("F%1: 0x%2 (%3)")
// 				.arg(i)
// 				.arg(QString::number(readFpReg(i), 16).rightJustified(16, '0'))
// 				.arg(static_cast<double>(readFpReg(i)))        // ← cast added
// 			);

		}
		DEBUG_LOG(QString("\nFPCR = 0x%1")
			.arg(QString::number(readFpcrRaw(), 16).rightJustified(16, '0')));

	}

	// Lock reservation methods for LL/SC instructions
	void setLockReservation(quint64 addr, int size) {
		m_lockReservationAddr = addr;
		m_lockReservationSize = size;
		m_lockValid = true;
	}

	bool checkLockReservation(quint64 addr, int size) const {
		return m_lockValid &&
			m_lockReservationAddr == addr &&
			m_lockReservationSize == size;
	}

	void invalidateLockReservation() {
		m_lockValid = false;
	}
	signals: 
		/**
	 * @brief Emitted when FPCR (Floating-Point Control Register) is updated
	 * @param fpcrValue The new raw value of FPCR
	 */
		void sigFpcrUpdated(quint64 fpcrValue);

		/**
		 * @brief Emitted when a floating-point exception status flag is set
		 * @param exceptionType The type of floating-point exception
		 * @param enabled Whether the exception was enabled or disabled
		 */
		void sigFpExceptionFlagChanged(FPTrapType exceptionType, bool enabled);

		/**
		 * @brief Emitted when any register value changes (for debugging/monitoring)
		 * @param regType Type of register ("INT" or "FP")
		 * @param regIndex Register index
		 * @param newValue New register value
		 */
		void sigRegisterChanged(const QString& regType, int regIndex, quint64 newValue);
	private: 
		QVector<quint64> intRegs;  // Integer registers
		FpRegs m_fpRegs;             // FP registers and FPCR
		//FpRegs regs;
		//QVector<quint64> intRegs;  // R0–R30, R31 hardwired to zero
		quint64 m_currentMode = 0; // Default to kernel mode
		quint64 m_lockReservationAddr = 0;
		int m_lockReservationSize = 0;
		bool m_lockValid = false;
		AlphaCPU* m_alphaCPUPtr;
};



