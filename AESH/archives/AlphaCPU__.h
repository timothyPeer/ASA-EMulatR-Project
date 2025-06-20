#pragma once
// AlphaCPU.h

#include <QObject>
#include <QAtomicInt>
#include <QTimer>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSet>
#include <QDateTime>

#include "../AEJ/fetchUnit.h"
#include "IExecutionContext.h"
#include "../AEC/RegisterBank.h"
#include "SafeMemory.h"
#include "AlphaMemorySystem.h"
#include "alphasmpManager.h"
#include "../AEJ/decodeStage.h"
#include "../AEE/FPException.h"
#include "../AEU/StackManager.h"
#include "../AEJ/executeStage.h"
#include "../AEJ/writeBackStage.h"
#include "../AEJ/PendingLoad.h"
#include "../AEJ/IprBank.h"
#include "../AEJ/InstructionPipeLine.h"
#include "../AEJ/UnifiedDataCache.h"
#include "../AEE/MemoryFaultInfo.h"
#include "../AEJ/enumerations/enumRoundingMode.h"
#include "../AEJ/enumerations/enumIPRNumbers.h"
#include "../AEJ/enumerations/enumCpuState.h"
#include "../AEJ/enumerations/enumProcessorMode.h"
#include "../AEJ/enumerations/enumIprBank.h"
#include "../AEJ/traps/trapFaultTraps.h"
#include "../AEJ/enumerations/enumRegisterType.h"
#include "../AEU/StackFrame.h"
#include "../AEJ/enumerations/enumMachineCheckType.h"
#include "../AEJ/enumerations/enumExceptionTypeArithmetic.h"
#include "../AEJ/constants/constExceptions.h"
#include "../AEJ/traps/trapFpType.h"
#include "../AEJ/enumerations/enumFPCompare.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"
#include "../AEJ/constants/constEXC_SUM.h"
#include "../AEJ/enumerations/enumFPFormat.h"
#include "../AEJ/constants/constAsaPerformance.h"
#include "../AEJ/structures/structSystemEntryPoints.h"
#include "../AEJ/structures/enumPalCodes.h"
#include "../AEJ/constants/constStatusRegister.h"
#include "../AEJ/constants/constStackConstants.h"
#
#include "../AEJ/instructionTLB.h"
#include "../AEJ/enumerations/enumDenormalMode.h"
#include "../AEE/TLBExceptionQ.h"




class AlphaSMPManager;


class AlphaCPU : public QObject, public IExecutionContext {
	Q_OBJECT



private:
	quint64	m_pc; // PC
	quint32	m_cpuId; // The zero based ordinal CPU ID
	IprBank* m_iprs; // Use the enhanced IprBank
	bool m_hasException = false;
	ProcessorMode m_currentMode = ProcessorMode::USER;
	quint64 m_palCodeBase = 0xFFFFFFFF80000000ULL;  // PAL base address

	// Pipeline stages
	QScopedPointer<FetchUnit> m_fetchUnit;					// Handles instruction fetching from memory
	QScopedPointer<DecodeStage> m_decodeStage;				// Decodes instructions into internal representation
	QScopedPointer<ExecuteStage> m_executeStage;			// Executes decoded instructions
	QScopedPointer<WritebackStage> m_writebackStage;		// Handles register file updates

	// CPU model for architecture-specific features
	CpuModel m_cpuModel;                               // EV4/EV5/EV6/EV7 model code

	// Member variables for execution control
	bool m_isShuttingDown = false;
	bool m_allowInstructionFetch = true;
//	ExecutionContext m_savedContext;
	QString m_lastStopReason;

	// Performance tracking
	QDateTime m_executionStartTime;
	qint64 m_totalExecutionTime = 0;
	quint64 m_currentInstructionCount = 0;
	double m_averageIPS = 0.0;

	quint64 m_currentASN; ///< current address-space number
	quint64 m_savedASN; ///< the save ASN - TBD how it will be consumed.

	using ProcessorStatus = quint64;
	/// Validate that the saved PS can be restored.
   /// @see Alpha AXP Architecture Reference Manual, Vol.1 §2.3.1
	bool isValidPS(ProcessorStatus newPS, ProcessorStatus oldPS) const;
	/// Switch the stack pointer (R30) based on mode bits in PS.
	/// Saves old SP to the appropriate IPR, then loads new SP.
	/// @see Alpha AXP Architecture Reference Manual, Vol.1 §6.7.4
	void switchStack(ProcessorStatus newPS, ProcessorStatus oldPS);

	/// True if any interrupt (software or hardware) is pending.
	bool interruptsPending() const;

	/// True if interrupts are enabled under the given PS.
	bool isInterruptEnabled(ProcessorStatus ps) const;

	/// Deliver a pending interrupt via the same exception path.
	/// @see Alpha AXP Architecture Reference Manual, Vol.1 §6.2
	void dispatchInterrupt();

public:

	// CPU Id: is a constant.
	explicit AlphaCPU(quint16 cpuID, QObject* parent = nullptr) 
		: QObject(parent), 
		m_cpuId(cpuID), 
		m_iprs(new IprBank(nullptr)),
		m_pc(0),
		m_hasException(false),
		m_currentMode(ProcessorMode::USER), 
		m_palCodeBase(0xFFFFFFFF80000000ULL)
	{
		initializeCpu();
	}

	void afterInstructionExecution()
	{
		// Check for pending interrupts
		checkPendingInterrupts();

		// Check for pending ASTs
		checkPendingAst();

		// Other post-instruction operations
		// ...
	}

	/**
	 * @brief Jump to the appropriate exception handler vector
	 *
	 * This function implements the Alpha exception dispatch mechanism by jumping
	 * to the appropriate exception handler based on the fault type. In Alpha
	 * architecture, exceptions are handled through PALcode vectors.
	 *
	 * @param faultType The type of memory fault that occurred
	 */
	void jumpToExceptionVector(MemoryFaultType faultType) {
		// Get the exception vector address based on fault type
		quint64 vectorAddress = getExceptionVectorAddress(faultType);

		// Save additional state required for exception handling
		prepareExceptionJump();

		// Set the program counter to the exception vector
		m_pc = vectorAddress;

		// Log the exception jump for debugging
		DEBUG_LOG(QString("Jumping to exception vector: PC=0x%1, Type=%2")
			.arg(vectorAddress, 16, 16, QChar('0'))
			.arg(static_cast<int>(faultType)));

		// Mark that we're now executing in exception context
		m_inExceptionHandler = true;

		// Update cycle counter for exception processing overhead
		m_cycleCounter += EXCEPTION_PROCESSING_CYCLES;
	}

	IprBank* iprBank() { return m_iprs; }						// IRP Internal Register 
/*	const IprBank& iprBank() const { return m_iprs; }		// IRP Internal Register*/

	void initializeCpu() {

	}

	bool readMemory64(quint64 vaddr, quint64& val, quint64 pc)
	{
		return m_memorySystem->readVirtualMemory(this, vaddr, val, 8, pc);
	}
	bool readMemory64Locked(quint64 vaddr, quint64& val, quint64 pc)
	{
		bool ok = m_memorySystem->readVirtualMemory(this, vaddr, val, 8, pc);
		if (ok) {
			m_reservationAddr = vaddr;
			m_reservationValid = true;
		}
		return ok;
	}
	bool writeMemory32Conditional(quint64 vaddr,
		quint32 value,
		quint64 pc)
	{
		// 1.  Did a valid LDL_L/LDQ_L reservation precede this store?
		if (!m_reservationValid || m_reservationAddr != vaddr) {
			m_reservationValid = false;            // failed ⇒ clear reservation
			return false;                          // “store failed” to ISA
		}

		// 2.  Forward the store through the full memory system
		bool ok = m_memorySystem->writeVirtualMemory(vaddr, value, 4, pc);

		// 3.  Success or not, the reservation is cleared afterwards
		m_reservationValid = false;
		return ok;                                 // caller writes R25 = ok ? 1 : 0
	}
	bool writeMemory32(quint64 vaddr, quint32 value, quint64 pc)
	{
		// Fast path through AMS → SafeMemory/MMIO; returns false on fault
		bool ok = m_memorySystem->writeVirtualMemory(vaddr, value, 4, pc);

		if (ok) {                       // invalidate every CPU reservation on that quadword
			m_memorySystem->clearReservations(vaddr & ~0x7ULL, 8);
		}
		return ok;
	}
	bool writeMemory64(quint64 vaddr, quint64 value, quint64 pc)
	{
		bool ok = m_memorySystem->writeVirtualMemory(vaddr, value, 8, pc);
		if (ok) {
			m_memorySystem->clearReservations(vaddr & ~0x7ULL, 8);
		}
		return ok;
	}
		bool writeMemory64Conditional(quint64 vaddr,
		quint64 value,
		quint64 pc)
	{
		if (!m_reservationValid || m_reservationAddr != vaddr) {
			m_reservationValid = false;
			return false;
		}

		bool ok = m_memorySystem->writeVirtualMemory(vaddr, value, 8, pc);
		m_reservationValid = false;
		return ok;
	}

	// Floating-point addition for VAX F format values
	quint64 addFFormat(quint64 a, quint64 b) {
		// Convert VAX F format values to native double
		double aValue = convertFromVaxF(a);
		double bValue = convertFromVaxF(b);

		// Perform addition
		double result = aValue + bValue;

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			setFloatingPointFlag(FPTrapType::FP_INVALID_OPERATION);
		}
		else if (std::isinf(result)) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
		}

		// Convert back to VAX F format
		return convertToVaxF(result);
	}
	// Floating-point addition for IEEE T format values
	quint64 addTFormat(quint64 a, quint64 b) {
		// Convert raw bits to double precision values
		double aValue = std::bit_cast<double>(a);
		double bValue = std::bit_cast<double>(b);

		// Perform addition
		double result;
		try {
			result = aValue + bValue;
		}
		catch (...) {
			// Handle computation errors
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
		}
		else if (std::isinf(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
		}

		// Return the bits
		return std::bit_cast<quint64>(result);
	}

	// Floating-point subtraction for VAX F format values
	quint64 subFFormat(quint64 a, quint64 b) {
		// Convert VAX F format values to native double
		double aValue = convertFromVaxF(a);
		double bValue = convertFromVaxF(b);

		// Perform subtraction
		double result = aValue - bValue;

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			setFloatingPointFlag(FPTrapType::FP_INVALID_OPERATION);
		}
		else if (std::isinf(result)) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
		}

		// Convert back to VAX F format
		return convertToVaxF(result);
	}

	// Floating-point multiplication for VAX F format values
	quint64 mulFFormat(quint64 a, quint64 b) {
		// Convert VAX F format values to native double
		double aValue = convertFromVaxF(a);
		double bValue = convertFromVaxF(b);

		// Perform multiplication
		double result = aValue * bValue;

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			setFloatingPointFlag(FPTrapType::FP_INVALID_OPERATION);
		}
		else if (std::isinf(result)) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
		}

		// Convert back to VAX F format
		return convertToVaxF(result);
	}

	// Floating-point division for VAX F format values
	quint64 divFFormat(quint64 a, quint64 b) {
		// Convert VAX F format values to native double
		double aValue = convertFromVaxF(a);
		double bValue = convertFromVaxF(b);

		// Check for division by zero
		if (bValue == 0.0) {
			setFloatingPointFlag(FPTrapType::FP_DIVISION_BY_ZERO);
			return getFloatingPointQuietNaN();
		}

		// Perform division
		double result = aValue / bValue;

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			setFloatingPointFlag(FPTrapType::FP_INVALID_OPERATION);
		}
		else if (std::isinf(result)) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
		}

		// Convert back to VAX F format
		return convertToVaxF(result);
	}


	// Convert VAX F format value to native double
	double convertFromVaxF(quint64 value) {
		// If value is zero, return 0.0
		if (value == 0) {
			return 0.0;
		}

		// Extract the components of the VAX F format
		bool sign = (value >> 63) & 1;                // Sign bit (bit 63)
		int exponent = ((value >> 55) & 0xFF) - 128;  // Exponent (bits 55-62), remove bias of 128
		quint64 fraction = value & 0x007FFFFFFFFFFFFF; // Fraction (bits 0-54)

		// VAX format has an implied 1 before the binary point
		fraction |= 0x0080000000000000ULL;

		// Convert to IEEE 754 double precision format
		// IEEE uses a bias of 1023 for the exponent
		int ieeeExponent = exponent + 1023;

		// Check for underflow/overflow
		if (ieeeExponent < 0) {
			// Underflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0.0;
		}

		if (ieeeExponent > 2047) {
			// Overflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			return sign ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
		}

		// Shift the fraction to adjust for IEEE format (which has 52 bits of fraction)
		// VAX F has 23 bits of fraction plus 1 hidden bit
		fraction = (fraction >> 3) & 0x000FFFFFFFFFFFFFULL;

		// Assemble the IEEE 754 double
		quint64 ieeeBits = (sign ? 0x8000000000000000ULL : 0) |
			((quint64)ieeeExponent << 52) |
			fraction;

		// Convert the bit pattern to a double
		return std::bit_cast<double>(ieeeBits);
	}
	// Convert VAX F floating-point value to quadword integer
	quint64 convertFToQuad(quint64 value) {
		// Convert VAX F format to native double
		double floatValue = convertFromVaxF(value);

		// Round according to current rounding mode
		RoundingMode mode = getCurrentRoundingMode();
		switch (mode) {
		case RoundingMode::ROUND_CHOPPED:
			floatValue = std::trunc(floatValue);
			break;
		case RoundingMode::ROUND_MINUS_INFINITY:
			floatValue = std::floor(floatValue);
			break;
		case RoundingMode::ROUND_PLUS_INFINITY:
			floatValue = std::ceil(floatValue);
			break;
		default: // ROUND_NEAREST
			floatValue = std::round(floatValue);
			break;
		}

		// Convert to integer
		qint64 result;
		if (floatValue >= static_cast<double>(std::numeric_limits<qint64>::max())) {
			// Overflow
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
			result = std::numeric_limits<qint64>::max();
		}
		else if (floatValue <= static_cast<double>(std::numeric_limits<qint64>::min())) {
			// Underflow
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
			result = std::numeric_limits<qint64>::min();
		}
		else {
			result = static_cast<qint64>(floatValue);

			// Check for inexact conversion
			if (static_cast<double>(result) != floatValue) {
				setFloatingPointFlag(FPTrapType::FP_INEXACT);
			}
		}

		return static_cast<quint64>(result);
	}

	// Compare two VAX F format values
	quint64 compareFFormat(quint64 a, quint64 b, FPCompareType compareType) {
		// Convert VAX F format values to native double
		double aValue = convertFromVaxF(a);
		double bValue = convertFromVaxF(b);

		// Perform comparison
		bool result = false;
		switch (compareType) {
		case FPCompareType::FP_EQUAL:
			result = (aValue == bValue);
			break;
		case FPCompareType::FP_LESS:
			result = (aValue < bValue);
			break;
		case FPCompareType::FP_LESS_EQUAL:
			result = (aValue <= bValue);
			break;
		case FPCompareType::FP_UNORDERED:
			result = (std::isnan(aValue) || std::isnan(bValue));
			break;
		}

		// Return 1 if condition is true, 0 otherwise
		return result ? 1 : 0;
	}

	// Set the value of a floating-point register
	void setFloatRegister(quint8 regNum, double value) {
		if (regNum >= 32) {
			DEBUG_LOG(QString("AlphaCPU: Invalid FP register number: %1").arg(regNum));
			return;
		}

		// F31 is hardwired to zero in Alpha architecture
		if (regNum == 31) {
			DEBUG_LOG("AlphaCPU: Attempted to write to F31 (hardwired to zero)");
			return;
		}

		// Convert double to raw bits
		quint64 bits = std::bit_cast<quint64>(value);

		// Write register
		m_registerBank->fp().raw[regNum] = bits;

		DEBUG_LOG(QString("AlphaCPU: F%1 = %2 (0x%3)")
			.arg(regNum)
			.arg(value, 0, 'g', 17)
			.arg(bits, 16, 16, QChar('0')));
	}

	// Get the 32-bit value from a floating-point register
	quint32 getFloatRegister32(quint8 regNum) const {
		if (regNum >= 32) {
			DEBUG_LOG(QString("AlphaCPU: Invalid FP register number: %1").arg(regNum));
			return 0;
		}

		// F31 is hardwired to zero in Alpha architecture
		if (regNum == 31) {
			return 0;
		}

		// Extract lower 32 bits from the 64-bit register
		return static_cast<quint32>(m_registerBank->fp().raw[regNum] & 0xFFFFFFFF);
	}

	// Normalize (scale) a VAX F format result
	double scaleVaxFResult(double result) {
		// Check if the result is zero
		if (result == 0.0) {
			return 0.0;
		}

		// Get the components of the double
		int exponent;
		double fraction = std::frexp(result, &exponent);

		// VAX F format has a bias of 128
		int biasedExp = exponent + 127;

		// Check for overflow/underflow
		if (biasedExp > 255) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
			return std::copysign(std::numeric_limits<double>::max(), result);
		}
		if (biasedExp < 0) {
			setFloatingPointFlag(FPTrapType::FP_UNDERFLOW);
			return 0.0;
		}

		// Normalize the fraction to be in [0.5, 1)
		if (fraction < 0.5) {
			fraction *= 2.0;
			biasedExp--;
		}

		// Reassemble the scaled value
		return std::ldexp(fraction, biasedExp);
	}

	// Normalize (scale) a VAX G format result
	double scaleVaxGResult(double result) {
		// Check if the result is zero
		if (result == 0.0) {
			return 0.0;
		}

		// Get the components of the double
		int exponent;
		double fraction = std::frexp(result, &exponent);

		// VAX G format has a bias of 1024
		int biasedExp = exponent + 1023;

		// Check for overflow/underflow
		if (biasedExp > 2047) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
			return std::copysign(std::numeric_limits<double>::max(), result);
		}
		if (biasedExp < 0) {
			setFloatingPointFlag(FPTrapType::FP_UNDERFLOW);
			return 0.0;
		}

		// Normalize the fraction to be in [0.5, 1)
		if (fraction < 0.5) {
			fraction *= 2.0;
			biasedExp--;
		}

		// Reassemble the scaled value
		return std::ldexp(fraction, biasedExp);
	}

	// Normalize (scale) an IEEE T format result
	double scaleIeeeTResult(double result) {
		// IEEE T format is already in IEEE double format, so we just need to handle
		// special cases like denormals, infinities, and NaNs

		// Check if the result is zero
		if (result == 0.0) {
			return 0.0;
		}

		// Check for NaN or infinity
		if (std::isnan(result) || std::isinf(result)) {
			return result;
		}

		// Get the components of the double
		int exponent;
		double fraction = std::frexp(result, &exponent);

		// IEEE format has a bias of 1023
		int biasedExp = exponent + 1022;

		// Check for overflow
		if (biasedExp > 2046) {
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
			return std::copysign(std::numeric_limits<double>::infinity(), result);
		}

		// Check for underflow (resulting in denormal)
		if (biasedExp < 1) {
			// Denormal handling depends on CPU settings
			if (m_denormalHandlingMode == DenormalMode::FLUSH_TO_ZERO) {
				setFloatingPointFlag(FPTrapType::FP_UNDERFLOW);
				return 0.0;
			}

			// Scale denormal number
			double scaledFraction = fraction * std::pow(2.0, biasedExp);
			return std::copysign(scaledFraction * std::pow(2.0, -1022), result);
		}

		// Normal number, no scaling needed
		return result;
	}

	// Convert a double to IEEE single precision
	double convertToIeeeS(double value) {
		// Convert to float (IEEE single precision) and back to double
		float floatValue = static_cast<float>(value);

		// Check for exceptions
		if (std::isnan(value) && !std::isnan(floatValue)) {
			// NaN converted to non-NaN
			setFloatingPointFlag(FPTrapType::FP_INVALID_OPERATION);
		}
		else if (std::isinf(value) && !std::isinf(floatValue)) {
			// Infinity converted to finite
			setFloatingPointFlag(FPTrapType::FP_OVERFLOW);
		}
		else if (value != 0.0 && floatValue == 0.0) {
			// Non-zero converted to zero (underflow)
			setFloatingPointFlag(FPTrapType::FP_UNDERFLOW);
		}
		else if (value != static_cast<double>(floatValue)) {
			// Inexact conversion
			setFloatingPointFlag(FPTrapType::FP_INEXACT);
		}

		// Return the value converted to double
		return static_cast<double>(floatValue);
	}

	// Trigger a floating-point exception
	void triggerFloatingPointException(FPTrapType exception) {
		// Set the appropriate status flag
		setFloatingPointFlag(exception);

		// Check if the trap is enabled for this exception
		bool trapEnabled = false;
		switch (exception) {
		case FPTrapType::FP_INVALID_OPERATION:
			trapEnabled = m_registerBank->fp().isTrapEnabled_InvalidOp();
			break;
		case FPTrapType::FP_DIVISION_BY_ZERO:
			trapEnabled = m_registerBank->fp().isTrapEnabled_DivZero();
			break;
		case FPTrapType::FP_OVERFLOW:
			trapEnabled = m_registerBank->fp().isTrapEnabled_Overflow();
			break;
		case FPTrapType::FP_UNDERFLOW:
			trapEnabled = m_registerBank->fp().isTrapEnabled_Underflow();
			break;
		case FPTrapType::FP_INEXACT:
			trapEnabled = m_registerBank->fp().isTrapEnabled_Inexact();
			break;
		default:
			// For unknown exceptions, always trap
			trapEnabled = true;
			break;
		}

		// If trap is enabled, trigger the exception
		if (trapEnabled) {
			DEBUG_LOG(QString("AlphaCPU: Triggering FP exception: %1").arg(static_cast<int>(exception)));
			triggerException(FPTrapType::FP_EXCEPTION, getPC());
		}
	}
	void attachMemorySystem(AlphaMemorySystem* memSys) { m_memorySystem = memSys; }
	void attachSMPManager(AlphaSMPManager* smpMgr) { m_AlphaSMPManager = smpMgr; }
	void attachRegisterBank(RegisterFileWrapper* regBank_) { m_registerBank = regBank_;}
	void attachMMIOManager(MMIOManager* mmio_) { m_mmioManager = mmio_; }
	void attachIRQController(IRQController* irqController_) { m_irqController = irqController_; }
    void attachTlbSystem(TLBSystem* tlb) { m_tlbSystem = tlb; } 	// Constructor should initialize TLB system reference
	void attachUnifiedCache(UnifiedDataCache* unifiedDataCache_) { m_level2DataCache = unifiedDataCache_; }
	
	/**
	 * @brief Calculate PAL entry point for exception type
	 */
	quint64 calculatePalEntryPoint(ExceptionType exception) {
		quint64 scbb = m_iprs.read(Ipr::SCBB);

		switch (exception) {
			case ExceptionType::MACHINE_CHECK:
			return scbb + PAL_OFFSET_MACHINE_CHECK;
		case ExceptionType::ALIGNMENT_FAULT:
			return scbb + PAL_OFFSET_ALIGNMENT_FAULT;
		case ExceptionType::ILLEGAL_INSTRUCTION:
			return scbb + PAL_OFFSET_ILLEGAL_INSTR;
		case ExceptionType::INTERRUPT:
			return scbb + PAL_OFFSET_INTERRUPT;
		case ExceptionType::AST: //AST
			return scbb + PAL_OFFSET_AST;
		case ExceptionType::ARITHMETIC_TRAP:
			return scbb + PAL_OFFSET_ARITHMETIC_TRAP;
		case ExceptionType::FP_EXCEPTION:
			return scbb + PAL_OFFSET_FP_EXCEPTION;
		default:
			ERROR_LOG(QString("Unknown exception type: %1").arg(static_cast<int>(exception)));
			return scbb + PAL_OFFSET_UNKNOWN;
		}
	}
	/**
	 * @brief Check for pending hardware interrupts
	 */
	void checkHardwareInterrupts() {
		// This would check external interrupt lines
		// Implementation depends on your interrupt controller
		DEBUG_LOG("Checking hardware interrupts");
	}
	/**
	 * @brief Check for pending interrupts after IPL change
	 */
	void checkForPendingInterrupts() {
		checkSoftwareInterrupts();
		checkHardwareInterrupts();
	}

	void checkPendingAst()
	{
		auto& iprs = iprBank();

		const quint64 sirr = iprs.read(Ipr::SIRR);
		const quint64 asten = iprs.read(Ipr::ASTEN);
		const quint8  ipl = static_cast<quint8>(iprs.read(Ipr::IPL));

		// Mask the pending AST levels that are enabled
		const quint64 pending = sirr & asten;

		// Find the highest?priority AST (lowest numeric level > IPL)
		if (pending) {
			const int level = ctz64(pending);         // count?trailing?zeros ? level #
			if (level > ipl) {
				// 1. Clear SIRR bit to show we’ve accepted it
				iprs.write(Ipr::SIRR, sirr & ~(1ULL << level));

				// 2. Set AST Summary (sticky until PAL clears)
				iprs.write(Ipr::ASTSR,
					iprs.read(Ipr::ASTSR) | (1ULL << level));

				// 3. Vector to PAL AST entrypoint (e.g., SCBB + 0x80)
				deliverException(ExceptionType::AST, level);
			}
		}
	}

	void checkSoftwareInterrupts() {
		quint64 sirr = m_iprs.read(Ipr::SIRR);
		quint64 ipl = m_iprs.read(Ipr::IPL);

		// Check for pending interrupts above current IPL
		quint64 pending = sirr & ~((1ULL << ipl) - 1);

		if (pending) {
			const int level = ctz64(pending);
			if (level > ipl) {
				// 1. Clear SIRR bit
				m_iprs.write(Ipr::SIRR, sirr & ~(1ULL << level));

				// 2. Set AST Summary
				m_iprs.write(Ipr::ASTSR,
					m_iprs.read(Ipr::ASTSR) | (1ULL << level));

				// 3. Vector to PAL AST entrypoint
				deliverException(ExceptionType::AST level);
			}
		}
	}

	/**
	* @brief Clear exception state after handling
	*/
	void clearExceptionState()
	{
		m_hasException = false;
		m_exceptionPending = false;
		m_exceptionPc = 0;
		m_faultingVirtualAddress = 0;
		m_exceptionCause = 0;
		m_faultingInstruction = 0;
		m_memoryManagementStatus = 0;
	}

	// Copy sign from one floating-point value to another
	quint64 copySign(quint64 magnitude, quint64 signSource) {
		// Extract sign bit from source (bit 63)
		bool signBit = (signSource >> 63) & 1;

		// Clear the sign bit of magnitude and set it to the source sign
		quint64 result = (magnitude & 0x7FFFFFFFFFFFFFFFULL) | (signBit ? 0x8000000000000000ULL : 0);

		return result;
	}

	// Copy negated sign from one floating-point value to another
	quint64 copySignNegate(quint64 magnitude, quint64 signSource) {
		// Extract sign bit from source (bit 63) and negate it
		bool signBit = !((signSource >> 63) & 1);

		// Clear the sign bit of magnitude and set it to the negated source sign
		quint64 result = (magnitude & 0x7FFFFFFFFFFFFFFFULL) | (signBit ? 0x8000000000000000ULL : 0);

		return result;
	}

	// Convert quadword integer to IEEE S format
	quint64 convertQuadToS(quint64 value) {
		// Convert signed integer to double first
		qint64 signedValue = static_cast<qint64>(value);
		double doubleValue = static_cast<double>(signedValue);

		// Convert to single precision with rounding
		float result = static_cast<float>(doubleValue);

		// Check for inexact conversion
		if (static_cast<double>(result) != doubleValue) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Inexact();
			}
		}

		// Check for overflow/underflow
		if (std::isinf(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
		}

		// Convert result back to double precision for storage
		double storedResult = static_cast<double>(result);
		return std::bit_cast<quint64>(storedResult);
	}

	// Convert quadword integer to IEEE T format
	quint64 convertQuadToT(quint64 value) {
		// Convert signed integer to double
		qint64 signedValue = static_cast<qint64>(value);
		double result = static_cast<double>(signedValue);

		// Check for inexact conversion (very large integers may lose precision)
		if (static_cast<qint64>(result) != signedValue) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Inexact();
			}
		}

		return std::bit_cast<quint64>(result);
	}

	// Convert IEEE T floating-point value to quadword integer
	quint64 convertTToQuad(quint64 value) {
		// Convert T format to native double
		double floatValue = std::bit_cast<double>(value);

		// Apply current rounding mode
		RoundingMode mode = getCurrentRoundingMode();
		switch (mode) {
		case RoundingMode::ROUND_CHOPPED:
			floatValue = std::trunc(floatValue);
			break;
		case RoundingMode::ROUND_MINUS_INFINITY:
			floatValue = std::floor(floatValue);
			break;
		case RoundingMode::ROUND_PLUS_INFINITY:
			floatValue = std::ceil(floatValue);
			break;
		default: // ROUND_NEAREST
			floatValue = std::round(floatValue);
			break;
		}

		// Convert to integer
		qint64 result;
		if (floatValue >= static_cast<double>(std::numeric_limits<qint64>::max())) {
			// Overflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			result = std::numeric_limits<qint64>::max();
		}
		else if (floatValue <= static_cast<double>(std::numeric_limits<qint64>::min())) {
			// Underflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			result = std::numeric_limits<qint64>::min();
		}
		else {
			result = static_cast<qint64>(floatValue);

			// Check for inexact conversion
			if (static_cast<double>(result) != floatValue) {
				if (m_registerBank && m_registerBank->fp()) {
					m_registerBank->fp()->raiseStatus_Inexact();
				}
			}
		}

		return static_cast<quint64>(result);
	}




	/**
	 * @brief Count trailing zeros in a 64-bit value
	 *
	 * This function counts the number of trailing zero bits in the given value.
	 * It's commonly used to find the position of the lowest set bit.
	 *
	 * @param value The 64-bit value to count trailing zeros in
	 * @return Number of trailing zeros (0-63)
	 */
	int ctz64(quint64 value) {
		if (value == 0) {
			return 64;  // All bits are zero
		}

		// Use compiler builtin if available (more efficient)
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_ctzll(value);
#elif defined(_MSC_VER)
		unsigned long trailing_zeros;
		_BitScanForward64(&trailing_zeros, value);
		return static_cast<int>(trailing_zeros);
#else
	// Fallback implementation using bit manipulation
		int count = 0;

		// Check lower 32 bits first
		if ((value & 0xFFFFFFFFULL) == 0) {
			count += 32;
			value >>= 32;
		}

		// Check lower 16 bits
		if ((value & 0xFFFFULL) == 0) {
			count += 16;
			value >>= 16;
		}

		// Check lower 8 bits
		if ((value & 0xFFULL) == 0) {
			count += 8;
			value >>= 8;
		}

		// Check lower 4 bits
		if ((value & 0xFULL) == 0) {
			count += 4;
			value >>= 4;
		}

		// Check lower 2 bits
		if ((value & 0x3ULL) == 0) {
			count += 2;
			value >>= 2;
		}

		// Check lowest bit
		if ((value & 0x1ULL) == 0) {
			count += 1;
		}

		return count;
#endif
	}

	bool decodeAndExecute(quint32 instruction)
	{
		// Extract the primary opcode (bits 31:26)
		quint8 primaryOpcode = static_cast<quint8>((instruction >> 26) & 0x3F);

		qDebug() << "[AlphaCPU] Decoding instruction:"
			<< QString("0x%1").arg(instruction, 8, 16)
			<< " Primary opcode:"
			<< QString("0x%1").arg(primaryOpcode, 2, 16);
		return false;
	}

	/**
	 * @brief Deliver an exception to the CPU
	 *
	 * This function initiates exception processing by setting up the appropriate
	 * state and jumping to the PAL exception handler.
	 *
	 * @param exception The type of exception to deliver
	 * @param level The interrupt level or additional exception data
	 */
	void deliverException(ExceptionType exception, int level) {
		// Save current state before entering exception handler
		saveProcessorState();

		// Set up exception-specific state
		setupExceptionState(exception, level);

		// Get the PAL entry point for this exception
		quint64 palEntry = getPalEntryPoint(exception);

		// Create exception frame using the framework
		quint64 excSum = m_exceptionCause; // Already prepared in setupExceptionState
		pushExceptionFrame(m_pc, getProcessorStatus(), excSum);

		// Jump to the PAL handler
		jumpToPalHandler(palEntry, level);

		// Update performance counters
		updateExceptionCounters(exception);
	}


	/**
	 * @brief Clear instruction prefetch buffers
	 */
	void clearInstructionPrefetchBuffers() {
		for (auto& buffer : m_prefetchBuffers) {
			buffer.clear();
		}
		m_prefetchBufferValid = false;

		DEBUG_LOG("Instruction prefetch buffers cleared");
	}

	

	/**
	 * @brief Disable CPU features during exception handling
	 */
	void disableCpuFeaturesForException() {
		// Disable speculative execution
		m_speculativeExecutionEnabled = false;

		// Disable branch prediction
		m_branchPredictionEnabled = false;

		// Disable prefetching
		m_prefetchingEnabled = false;

		// Ensure memory ordering is strict during exception handling
		m_memoryOrderingStrict = true;
	}

	/**
	* @brief Deliver an exception into the CPU.
	*
	* ASA Vol 1 §6.7.3 requires that on entry to an exception:
	* 1) a hardware-saved frame (ExceptionFrame) be pushed,
	* 2) full context spilled only if scheduling occurs.
	*/
	void dispatchException(ExceptionType type, quint64 faultAddr);

		void executeNextInstruction() {
		quint64 currentPC = m_pc;

		// Use the memory system directly instead of translate()
		quint32 instruction = 0;

		// Let AlphaMemorySystem handle the complete translation and fetch
		if (m_memorySystem->readVirtualMemory(m_pc, &instruction, 4, m_pc)) {
			// Instruction fetch succeeded - decode and execute
			bool branched = decodeAndExecute(instruction);
			if (!branched) {
				m_pc += 4;
			}
			emit sigCycleExecuted(m_pc);
		}
		else {
			// Memory system will have already handled:
			// - TLB lookup/miss handling
			// - Page fault generation  
			// - Protection violations
			// - Exception delivery
			// 
			// The fault information is already set up by the memory system
			// No additional action needed here
		}
	}

	void finished()
	{
		// TODO delete this;
	}

	// Floating-point conditional move
	quint64 floatConditionalMove(quint64 condition, quint64 trueValue, quint64 falseValue) {
		// Convert condition to double to test
		double conditionValue = std::bit_cast<double>(condition);

		// In Alpha, the condition is typically:
		// - Non-zero (including NaN) = true
		// - Zero = false
		bool isTrue = (conditionValue != 0.0);

		// Return the appropriate value
		return isTrue ? trueValue : falseValue;
	}

	//alphaInstruction* getAlphaInstruction();

	UnifiedDataCache* getDataCache() { return m_level2DataCache; }

	/**
	 * @brief Get CPU ID
	 * @return CPU identifier
	 */
	quint16 getCpuId()  const {
		return m_cpuId;
	}

	/**
	* @brief Method that matches your call pattern
	* This just sets the halted state
	*/
	void halted() {
		setHalted(true);
	}
	/**
 * @brief Get the current exception PC
 *
 * @return Program counter of the faulting instruction
 */
	quint64 getExceptionPC() const { return m_exceptionPc; }
	/**
	 * @brief Get priority level for exception type
	 *
	 * @param exceptionType Exception type
	 * @return Priority level (higher number = higher priority)
	 */
	int getExceptionPriority(ExceptionType exceptionType)
	{
		switch (exceptionType) {
		case ExceptionType::MACHINE_CHECK:
			return 10;  // Highest priority
		case ExceptionType::ARITHMETIC_TRAP:
			return 8;
		case ExceptionType::ALIGNMENT_FAULT:
			return 7;
		case ExceptionType::ACCESS_CONTROL_VIOLATION:
			return 6;
		case ExceptionType::PAGE_FAULT:
			return 5;
		case ExceptionType::ILLEGAL_OPCODE:
			return 4;
		case ExceptionType::OPCODE_RESERVED:
			return 3;
		case ExceptionType::INTERRUPT:
			return 2;
		default:
			return 1;  // Lowest priority
		}
	}
	/**
	 * @brief Get the faulting virtual address
	 *
	 * @return Virtual address that caused the fault
	 */
	quint64 getFaultingVirtualAddress() const { return m_faultingVirtualAddress; }

	FpcrRegister* getFpcr() override
	{
		m_registerBank->fp();
	}
	RegisterBank* getRegisterBank() override {
		return m_registerBank;
	};


	//AlphaJITCompiler* getJITCompiler() { return m_jitCompilerPtr.data(); }
	AlphaMemorySystem* getMemorySystem() { return m_memorySystem; }

	/**
	 * @brief Get PAL entry point for an exception
	 *
	 * @param exception The exception type
	 * @return PAL entry point address
	 */
	quint64 getPalEntryPoint(ExceptionType exception) {
		// Get the System Control Block Base (SCBB) register
		quint64 scbb = m_iprs.read(Ipr::SCBB);

		// PAL entry points are offsets from SCBB
		switch (exception) {
		case ExceptionType::MACHINE_CHECK:
			return scbb + PAL_OFFSET_MACHINE_CHECK;
		case ExceptionType::ALIGNMENT_FAULT:
			return scbb + PAL_OFFSET_ALIGNMENT_FAULT;
		case ExceptionType::ILLEGAL_INSTRUCTION:
			return scbb + PAL_OFFSET_ILLEGAL_INSTR;
		case ExceptionType::INTERRUPT:
			return scbb + PAL_OFFSET_INTERRUPT;
		case ExceptionType::AST:
			return scbb + PAL_OFFSET_AST;
		case ExceptionType::ARITHMETIC_TRAP:
			return scbb + PAL_OFFSET_ARITHMETIC_TRAP;
		case ExceptionType::FP_EXCEPTION:
			return scbb + PAL_OFFSET_FP_EXCEPTION;
		case ExceptionType::PAGE_FAULT:
			return scbb + PAL_OFFSET_PAGE_FAULT;
		case ExceptionType::ACCESS_CONTROL_VIOLATION:
			return scbb + PAL_OFFSET_ACCESS_VIOLATION;
		default:
			ERROR_LOG(QString("Unknown exception type: %1").arg(static_cast<int>(exception)));
			return scbb + PAL_OFFSET_UNKNOWN;
		}
	}

	/**
	 * @brief Get current processor status
	 * @return Current processor status register value
	 */
	quint64 getProcessorStatus() const {
		return m_iprs->read(Ipr::PS);
	}
	SafeMemory* getSafeMemory() { return m_memorySystem->getSafeMemory(); }
	CPUState	getState() { return m_cpuState; }

	/**
	 * @brief Get translation info for debugging/inspection purposes
	 * This is NOT used for normal memory access - use memory system directly
	 */
	bool getTranslationInfo(quint64 vAddr, quint64& pAddr, bool& isValid,
		TLBException& exception) const {
		if (!m_memorySystem || !m_memorySystem->getTlbSystem()) {
			return false;
		}

		quint64 currentASN = m_iprs->read(Ipr::ASN);
		bool isKernelMode = (m_currentMode == ProcessorMode::KERNEL);

		auto result = m_memorySystem->getTlbSystem()->translateAddress(
			vAddr, false, false, currentASN, isKernelMode);

		pAddr = result.physicalAddress;
		isValid = (result.tlbException == TLBException::NONE);
		exception = result.tlbException;

		return true;
	}

	quint64				getUserSP() const { return m_registerBank->readIntReg(30); } // Assuming R30

	void halt() {
		DEBUG_LOG("AlphaCPU: CPU halted");
		m_halted = true;
		m_running = false;
		stopInstructionPipeline();
		notifySystemOfHalt();
		emit sigCpuHalted(m_cpuId);
	}

	void setPerformanceCounter(quint32 counterIdx, quint64 value) {
		if (counterIdx >= MAX_PERF_COUNTERS) {
			DEBUG_LOG(QString("AlphaCPU: Invalid counter index for set: %1").arg(counterIdx));
			return;
		}

		DEBUG_LOG(QString("AlphaCPU: Setting counter %1 to 0x%2")
			.arg(counterIdx)
			.arg(value, 16, 16, QChar('0')));

		m_perfCounters[counterIdx].value = value;
	}


	/**
 * @brief Check if floating-point exception should trigger a trap
 * @param exception The exception type that was just flagged
 */
	void checkFloatingPointTraps(FPTrapType exception)
	{
		if (!m_registerBank) {
			return;
		}

		auto& fpRegs = m_registerBank->fp();
		bool shouldTrap = false;

		// Check if trap is enabled for this specific exception type
		switch (exception) {
		case FPTrapType::FP_INVALID_OPERATION:
			shouldTrap = fpRegs.isTrapEnabled_InvalidOp();
			break;
		case FPTrapType::FP_DIVISION_BY_ZERO:
			shouldTrap = fpRegs.isTrapEnabled_DivZero();
			break;
		case FPTrapType::FP_OVERFLOW:
			shouldTrap = fpRegs.isTrapEnabled_Overflow();
			break;
		case FPTrapType::FP_UNDERFLOW:
			shouldTrap = fpRegs.isTrapEnabled_Underflow();
			break;
		case FPTrapType::FP_INEXACT:
			shouldTrap = fpRegs.isTrapEnabled_Inexact();
			break;
		default:
			// Unknown exceptions should always trap for safety
			shouldTrap = true;
			break;
		}

		if (shouldTrap) {
			DEBUG_LOG(QString("AlphaCPU: FP Exception %1 triggered trap")
				.arg(static_cast<int>(exception)));

			// Trigger the appropriate exception
			triggerException(ExceptionType::FP_EXCEPTION, getPC());
		}
	}

	/**
	 * @brief Clear specific floating-point status flag
	 * @param exception Exception type flag to clear
	 */
	void clearFloatingPointFlag(FPTrapType exception)
	{
		if (!m_registerBank) {
			DEBUG_LOG("AlphaCPU: No register bank available for FP flag clearing");
			return;
		}

		auto& fpRegs = m_registerBank->fp();

		switch (exception) {
		case FPTrapType::FP_INVALID_OPERATION:
			fpRegs.clearStatus_InvalidOp();
			DEBUG_LOG("AlphaCPU: FP Invalid Operation flag cleared");
			break;

		case FPTrapType::FP_DIVISION_BY_ZERO:
			fpRegs.clearStatus_DivZero();
			DEBUG_LOG("AlphaCPU: FP Division by Zero flag cleared");
			break;

		case FPTrapType::FP_OVERFLOW:
			fpRegs.clearStatus_Overflow();
			DEBUG_LOG("AlphaCPU: FP Overflow flag cleared");
			break;

		case FPTrapType::FP_UNDERFLOW:
			fpRegs.clearStatus_Underflow();
			DEBUG_LOG("AlphaCPU: FP Underflow flag cleared");
			break;

		case FPTrapType::FP_INEXACT:
			fpRegs.clearStatus_Inexact();
			DEBUG_LOG("AlphaCPU: FP Inexact flag cleared");
			break;

		default:
			DEBUG_LOG(QString("AlphaCPU: Cannot clear unknown FP exception type %1")
				.arg(static_cast<int>(exception)));
			break;
		}
	}

	/**
	 * @brief Get current floating-point status flags
	 * @return Bitmask of currently set FP status flags
	 */
	quint32 getFloatingPointStatus() const
	{
		if (!m_registerBank) {
			return 0;
		}

		const auto& fpRegs = m_registerBank->fp();
		quint32 status = 0;

		if (fpRegs.status_InvalidOp()) {
			status |= static_cast<quint32>(FPTrapType::FP_INVALID_OPERATION);
		}
		if (fpRegs.status_DivZero()) {
			status |= static_cast<quint32>(FPTrapType::FP_DIVISION_BY_ZERO);
		}
		if (fpRegs.status_Overflow()) {
			status |= static_cast<quint32>(FPTrapType::FP_OVERFLOW);
		}
		if (fpRegs.status_Underflow()) {
			status |= static_cast<quint32>(FPTrapType::FP_UNDERFLOW);
		}
		if (fpRegs.status_Inexact()) {
			status |= static_cast<quint32>(FPTrapType::FP_INEXACT);
		}

		return status;
	}

	/**
	 * @brief Check if any floating-point exceptions are pending
	 * @return True if any FP status flags are set
	 */
	bool hasFloatingPointExceptions() const
	{
		return getFloatingPointStatus() != 0;
	}
	double sqrtWithDenormalizedHandling(double input, FPFormat format, RoundingMode mode)
	{
		if (input < 0.0) {
			// Negative input - set invalid operation flag and return NaN
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return std::numeric_limits<double>::quiet_NaN();
		}

		if (input == 0.0) {
			return 0.0; // sqrt(0) = 0
		}

		// Check if input is denormalized for the target format
		if (isDenormalized(input, format)) {
			// Handle denormalized numbers based on the format
			switch (format) {
			case FPFormat::IEEE_S_FORMAT:
			case FPFormat::IEEE_T_FORMAT:
				// IEEE formats support denormals
				break;

			case FPFormat::VAX_F_FORMAT:
			case FPFormat::VAX_G_FORMAT:
				// VAX formats don't support denormals, convert to zero
				if (m_registerBank && m_registerBank->fp()) {
					m_registerBank->fp()->raiseStatus_Underflow();
				}
				return 0.0;
			}
		}

		// Compute sqrt
		double result = std::sqrt(input);

		// Apply appropriate rounding
		switch (mode) {
		case RoundingMode::ROUND_NEAREST:
			// std::sqrt already uses round-to-nearest
			break;

		case RoundingMode::ROUND_CHOPPED:
			// Implement truncation if needed
			// This would require extracting components and clearing bits
			break;

		default:
			// Use default rounding
			break;
		}

		// Check if result is denormalized for target format
		if (isDenormalized(result, format)) {
			switch (format) {
			case FPFormat::IEEE_S_FORMAT:
			case FPFormat::IEEE_T_FORMAT:
				// IEEE formats support denormals
				break;

			case FPFormat::VAX_F_FORMAT:
			case FPFormat::VAX_G_FORMAT:
				// VAX formats don't support denormals, convert to zero
				if (m_registerBank && m_registerBank->fp()) {
					m_registerBank->fp()->raiseStatus_Underflow();
				}
				return 0.0;
			}
		}

		return result;
	}

	double applyUnbiasedRounding(double value) {
		// Apply unbiased rounding to a double value
		// Unbiased rounding is "round to nearest, ties to even"

		// Get the FPU's current rounding mode
		RoundingMode currentMode = getCurrentRoundingMode();

		// If already using round-to-nearest, return as is
		if (currentMode == RoundingMode::ROUND_NEAREST) {
			return value;
		}

		// Otherwise, extract value components
		uint64_t bits = std::bit_cast<uint64_t>(value);
		bool sign = (bits >> 63) != 0;
		int exponent = ((bits >> 52) & 0x7FF);
		uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;

		// If it's a special value (0, infinity, NaN), return as is
		if (exponent == 0 || exponent == 0x7FF) {
			return value;
		}

		// For unbiased rounding, we need to analyze the fraction bits
		// This would depend on the specific rounding requirements
		// For simplicity, we're just returning the value with IEEE
		// round-to-nearest semantics

		return value;
	}
	double convertToVaxFWithUnbiasedRounding(double value, RoundingMode mode) {
		// If zero or denormalized, return zero
		if (value == 0.0 || isDenormalized(value, FPFormat::VAX_F_FORMAT)) {
			return 0.0;
		}

		// Extract components from the IEEE double
		uint64_t bits = std::bit_cast<uint64_t>(value);
		bool sign = (bits >> 63) != 0;
		int exponent = ((bits >> 52) & 0x7FF) - 1023; // Unbias the exponent
		uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;

		// Add the implicit leading bit
		fraction |= 0x0010000000000000ULL;

		// Adjust for VAX F-format
		// VAX F has 8-bit exponent with bias 128 and 23-bit fraction
		int vaxExponent = exponent + 128;

		// Check for overflow/underflow
		if (vaxExponent > 255) {
			// Overflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			return sign ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
		}

		if (vaxExponent < 0) {
			// Underflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0.0;
		}

		// Shift the fraction to match 23-bit precision of VAX F-format
		// Apply rounding based on mode
		uint64_t roundedFraction = fraction >> 29; // Keep 23 bits

		if (mode == RoundingMode::ROUND_NEAREST) {
			// Check rounding bit
			bool roundBit = ((fraction >> 28) & 1) != 0;
			bool stickyBit = (fraction & 0x0FFFFFFFULL) != 0;
			bool lsb = ((fraction >> 29) & 1) != 0;

			// Round to nearest, ties to even
			if (roundBit && (stickyBit || lsb)) {
				roundedFraction++;
				// Check for rounding overflow
				if (roundedFraction > 0x007FFFFF) {
					roundedFraction = 0;
					vaxExponent++;
				}
			}
		}

		// Convert the components back to double
		// Here we're constructing a double that represents the VAX F-format value
		uint64_t resultBits = (sign ? 0x8000000000000000ULL : 0) |
			((uint64_t)(vaxExponent & 0xFF) << 52) |
			(roundedFraction << 29);

		return std::bit_cast<double>(resultBits);
	}
	quint64 convertToVaxF(qint64 intValue, RoundingMode mode) {
		// VAX F-format (32-bit) conversion with rounding control
		if (intValue == 0) {
			return 0; // VAX format zero
		}

		// Determine sign bit
		bool negative = (intValue < 0);
		uint64_t absValue = negative ? -intValue : intValue;

		// Find the most significant bit position
		int msb = 63 - __builtin_clzll(absValue);

		// VAX F-format: 1 sign bit, 8 exponent bits, 23 fraction bits
		int biasedExp = msb + 128; // VAX F-format uses 128-bias

		// Check for overflow/underflow
		if (biasedExp > 255) {
			// Overflow - set appropriate flags and return max value
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			return negative ? 0x00008000FFFFFFFFULL : 0x0000800000000000ULL; // Maximum value with sign
		}

		if (msb < -128) {
			// Underflow - set appropriate flags and return zero
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0;
		}

		// Normalize the fraction (shift to ensure most significant bit is 1)
		uint64_t fraction = (absValue << (55 - msb)) & 0x007FFFFF00000000ULL;

		// Handle rounding
		bool roundBit = false;
		switch (mode) {
		case RoundingMode::ROUND_NEAREST: {
			// Round to nearest, ties to even
			roundBit = ((absValue << (56 - msb)) & 0x0080000000000000ULL) != 0;
			bool stickyBit = ((absValue << (57 - msb)) & 0x007FFFFFFFFFFFF0ULL) != 0;
			if (roundBit && (stickyBit || (fraction & 0x0000000100000000ULL))) {
				fraction += 0x0000000100000000ULL;
				// Check for rounding overflow
				if ((fraction & 0x0080000000000000ULL) != 0) {
					fraction = 0;
					biasedExp++;
				}
			}
			break;
		}
		case RoundingMode::ROUND_CHOPPED:
			// Just truncate - no rounding
			break;
		default:
			// Default to truncation for unsupported rounding modes
			break;
		}

		// Combine the components into VAX F-format
		// In VAX F-format, the exponent and fraction are stored in big-endian byte order
		uint64_t result = (negative ? 0x0000800000000000ULL : 0) |
			((uint64_t)(biasedExp & 0xFF) << 47) |
			((fraction >> 32) & 0x007FFFFF);

		// VAX word swapped format (bytes are reversed within 16-bit chunks)
		result = ((result & 0xFF00FF0000000000ULL) >> 8) |
			((result & 0x00FF00FF00000000ULL) << 8);

		return result;
	}
	quint64 convertToVaxG(qint64 intValue, RoundingMode mode) {
		// VAX G-format (64-bit) conversion with rounding control
		if (intValue == 0) {
			return 0; // VAX format zero
		}

		// Determine sign bit
		bool negative = (intValue < 0);
		uint64_t absValue = negative ? -intValue : intValue;

		// Find the most significant bit position
		int msb = 63 - __builtin_clzll(absValue);

		// VAX G-format: 1 sign bit, 11 exponent bits, 52 fraction bits
		int biasedExp = msb + 1024; // VAX G-format uses 1024-bias

		// Check for overflow/underflow
		if (biasedExp > 2047) {
			// Overflow - set appropriate flags and return max value
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			return negative ? 0x8000FFFFFFFFFFULL : 0x8000000000000000ULL; // Maximum value with sign
		}

		if (msb < -1023) {
			// Underflow - set appropriate flags and return zero
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0;
		}

		// Normalize the fraction (shift to ensure most significant bit is 1)
		uint64_t fraction = (absValue << (11 - msb)) & 0x000FFFFFFFFFFFFFULL;

		// Handle rounding
		bool roundBit = false;
		switch (mode) {
		case RoundingMode::ROUND_NEAREST: {
			// Round to nearest, ties to even
			roundBit = ((absValue << (12 - msb)) & 0x0800000000000000ULL) != 0;

			// FIXED: Correct sticky bit mask (16 hex digits = 64 bits)
			bool stickyBit = ((absValue << (13 - msb)) & 0x7FFFFFFFFFFFFFFFULL) != 0;

			if (roundBit && (stickyBit || (fraction & 0x0000000000000001ULL))) {
				fraction += 1;
				// Check for rounding overflow
				if ((fraction & 0x0010000000000000ULL) != 0) {
					fraction = 0;
					biasedExp++;
				}
			}
			break;
		}
		case RoundingMode::ROUND_CHOPPED:
			// Just truncate - no rounding
			break;
		default:
			// Default to truncation for unsupported rounding modes
			break;
		}

		// Combine the components into VAX G-format
		// In VAX G-format, the exponent and fraction are stored in a specific order
		uint64_t result = (negative ? 0x8000000000000000ULL : 0) |
			((uint64_t)(biasedExp & 0x7FF) << 52) |
			fraction;

		// VAX word swapped format
		result = ((result & 0xFF00FF00FF00FF00ULL) >> 8) |
			((result & 0x00FF00FF00FF00FFULL) << 8);

		return result;
	}
	double convertToVaxGWithUnbiasedRounding(double value, RoundingMode mode) {
		// If zero or denormalized, return zero
		if (value == 0.0 || isDenormalized(value, FPFormat::VAX_G_FORMAT)) {
			return 0.0;
		}

		// Extract components from the IEEE double
		uint64_t bits = std::bit_cast<uint64_t>(value);
		bool sign = (bits >> 63) != 0;
		int exponent = ((bits >> 52) & 0x7FF) - 1023; // Unbias the exponent
		uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;

		// Add the implicit leading bit
		fraction |= 0x0010000000000000ULL;

		// Adjust for VAX G-format
		// VAX G has 11-bit exponent with bias 1024 and 52-bit fraction
		int vaxExponent = exponent + 1024;

		// Check for overflow/underflow
		if (vaxExponent > 2047) {
			// Overflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			return sign ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
		}

		if (vaxExponent < 0) {
			// Underflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0.0;
		}

		// For VAX G, the fraction bits align with IEEE double
		uint64_t roundedFraction = fraction & 0x000FFFFFFFFFFFFFULL;

		// No rounding needed for G-format since it has the same precision as IEEE double

		// Convert the components back to double
		// Here we're constructing a double that represents the VAX G-format value
		uint64_t resultBits = (sign ? 0x8000000000000000ULL : 0) |
			((uint64_t)(vaxExponent & 0x7FF) << 52) |
			roundedFraction;

		return std::bit_cast<double>(resultBits);
	}
	double convertToIeeeSWithUnbiasedRounding(double value, RoundingMode mode) {
		// If zero, return zero
		if (value == 0.0) {
			return 0.0;
		}

		// Extract components from the input double
		uint64_t bits = std::bit_cast<uint64_t>(value);
		bool sign = (bits >> 63) != 0;
		int exponent = ((bits >> 52) & 0x7FF) - 1023; // Unbias the exponent
		uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;

		// Add the implicit leading bit for normalized numbers
		if (exponent != -1023) { // Not denormalized
			fraction |= 0x0010000000000000ULL;
		}

		// Adjust for IEEE single-precision
		// IEEE S has 8-bit exponent with bias 127 and 23-bit fraction
		int ieeeExponent = exponent + 127;

		// Check for overflow/underflow
		if (ieeeExponent > 254) { // 255 is reserved for infinity/NaN
			// Overflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}

			// Return infinity
			uint32_t singleBits = (sign ? 0xFF800000U : 0x7F800000U);
			float result = std::bit_cast<float>(singleBits);
			return static_cast<double>(result);
		}

		if (ieeeExponent < -22) { // Below smallest denormal
			// Underflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0.0;
		}

		// Handle denormals
		if (ieeeExponent <= 0) {
			// Shift the fraction right to create a denormalized number
			int shift = 1 - ieeeExponent;
			fraction >>= shift;
			ieeeExponent = 0;
		}

		// Shift the fraction to match 23-bit precision of IEEE single
		// Apply rounding based on mode
		uint32_t roundedFraction = (fraction >> 29) & 0x007FFFFF; // Keep 23 bits

		if (mode == RoundingMode::ROUND_NEAREST) {
			// Check rounding bit
			bool roundBit = ((fraction >> 28) & 1) != 0;
			bool stickyBit = (fraction & 0x0FFFFFFFULL) != 0;
			bool lsb = ((fraction >> 29) & 1) != 0;

			// Round to nearest, ties to even
			if (roundBit && (stickyBit || lsb)) {
				roundedFraction++;
				// Check for rounding overflow
				if (roundedFraction > 0x007FFFFF) {
					roundedFraction = 0;
					ieeeExponent++;

					// Check if rounding caused overflow to infinity
					if (ieeeExponent > 254) {
						if (m_registerBank && m_registerBank->fp()) {
							m_registerBank->fp()->raiseStatus_Overflow();
						}
						uint32_t singleBits = (sign ? 0xFF800000U : 0x7F800000U);
						float result = std::bit_cast<float>(singleBits);
						return static_cast<double>(result);
					}
				}
			}
		}

		// Construct IEEE single-precision format
		uint32_t singleBits = (sign ? 0x80000000U : 0) |
			((uint32_t)(ieeeExponent & 0xFF) << 23) |
			roundedFraction;

		// Convert to float and then to double
		float singleResult = std::bit_cast<float>(singleBits);
		return static_cast<double>(singleResult);
	}
	double convertToIeeeTWithUnbiasedRounding(double value, RoundingMode mode) {
		// IEEE T (double precision) format conversion
		// Since we're working with IEEE doubles already, this mainly involves
		// handling special cases and rounding modes

		// If value is already in IEEE format, no format conversion needed
		// Just handle rounding if necessary

		if (mode == RoundingMode::ROUND_NEAREST) {
			// IEEE double already uses round-to-nearest, ties to even
			return value;
		}
		else if (mode == RoundingMode::ROUND_CHOPPED) {
			// Implement truncation for double precision
			uint64_t bits = std::bit_cast<uint64_t>(value);
			bool sign = (bits >> 63) != 0;
			int exponent = ((bits >> 52) & 0x7FF);
			uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;

			// If it's a special value (0, infinity, NaN), return as is
			if (exponent == 0 || exponent == 0x7FF) {
				return value;
			}

			// For truncation toward zero, we just need to clear any
			// bits that would be rounded based on the intended precision
			// For T-format, we keep all bits intact

			// For chopped rounding, we don't modify anything
			return value;
		}

		// Default behavior - return unmodified value
		return value;
	}
	bool isDenormalized(double value, FPFormat format) {
		uint64_t bits = std::bit_cast<uint64_t>(value);
		int exponent = ((bits >> 52) & 0x7FF);

		switch (format) {
		case FPFormat::IEEE_S_FORMAT: // IEEE single precision
			return exponent < (127 - 1023 + 1); // Exponent too small for normalized S-format

		case FPFormat::IEEE_T_FORMAT: // IEEE double precision
			return exponent == 0; // IEEE denormal

		case FPFormat::VAX_F_FORMAT: // VAX F-format
			return exponent < (128 - 1023 + 1); // Too small for VAX F normalized

		case FPFormat::VAX_G_FORMAT: // VAX G-format
			return exponent < (1024 - 1023 + 1); // Too small for VAX G normalized

		default:
			return false;
		}
	}
	// Helper method to convert 64-bit integer to VAX F (32-bit float) format with unbiased rounding
	quint64 convertToVaxFUnbiased(qint64 intValue, RoundingMode mode) {
		// Convert to double first
		double doubleVal = static_cast<double>(intValue);

		// Apply conversion with unbiased rounding
		double vaxFVal = convertToVaxFWithUnbiasedRounding(doubleVal, mode);

		// Return raw bits
		return std::bit_cast<uint64_t>(vaxFVal);
	}
	// Helper method to convert 64-bit integer to VAX G (64-bit float) format with unbiased rounding
	quint64 convertToVaxGUnbiased(qint64 intValue, RoundingMode mode)
	{
		// Convert to double first
		double doubleVal = static_cast<double>(intValue);

		// Apply conversion with unbiased rounding
		double vaxGVal = convertToVaxGWithUnbiasedRounding(doubleVal, mode);

		// Return raw bits
		return std::bit_cast<uint64_t>(vaxGVal);
	}
	// Helper method to convert VAX G to VAX F format
	quint64 convertVaxGToF(quint64 gValue, RoundingMode mode)
	{
		// If G-format value is zero, return F-format zero
		if (gValue == 0) {
			return 0;
		}

		// Extract components from G-format
		bool sign = (gValue >> 63) != 0;
		int exponent = ((gValue >> 52) & 0x7FF) - 1024; // Unbias G-format exponent
		uint64_t fraction = gValue & 0x000FFFFFFFFFFFFFULL;

		// F-format
		int fExponent = exponent + 128;

		// Check for overflow/underflow
		if (fExponent > 255) {
			// Overflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
			return sign ? 0x00008000FFFFFFFFULL : 0x0000800000000000ULL;
		}

		if (fExponent < 0) {
			// Underflow
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Underflow();
			}
			return 0;
		}

		// Truncate G-format fraction to F-format precision
		uint64_t fFraction = fraction >> 29;

		// Apply rounding based on mode
		if (mode == RoundingMode::ROUND_NEAREST) {
			// Check rounding bit
			bool roundBit = ((fraction >> 28) & 1) != 0;
			bool stickyBit = (fraction & 0x0FFFFFFFULL) != 0;
			bool lsb = (fFraction & 1) != 0;

			// Round to nearest, ties to even
			if (roundBit && (stickyBit || lsb)) {
				fFraction++;
				// Check for rounding overflow
				if (fFraction > 0x007FFFFF) {
					fFraction = 0;
					fExponent++;
					// Check for final overflow
					if (fExponent > 255) {
						if (m_registerBank && m_registerBank->fp()) {
							m_registerBank->fp()->raiseStatus_Overflow();
						}
						return sign ? 0x00008000FFFFFFFFULL : 0x0000800000000000ULL;
					}
				}
			}
		}

		// Combine the components into VAX F-format
		uint64_t result = (sign ? 0x0000800000000000ULL : 0) |
			((uint64_t)(fExponent & 0xFF) << 47) |
			((fFraction & 0x007FFFFF) << 32);

		// VAX word swapped format
		result = ((result & 0xFF00FF0000000000ULL) >> 8) |
			((result & 0x00FF00FF00000000ULL) << 8);

		return result;
	}
	// Helper method to convert VAX G to F format with unbiased rounding
	quint64 convertVaxGToFUnbiased(quint64 gValue, RoundingMode mode)
	{
		// Convert to double
		double doubleVal = std::bit_cast<double>(gValue);

		// Apply conversion with unbiased rounding
		double fVal = convertToVaxFWithUnbiasedRounding(doubleVal, mode);

		// Return raw bits
		return std::bit_cast<uint64_t>(fVal);
	}
	void handleCounterOverflow(quint32 counterIdx) {
		DEBUG_LOG(QString("AlphaCPU: Performance counter %1 overflow")
			.arg(counterIdx));

		switch (m_perfCounters[counterIdx].overflowAction) {
		case 0x0000: // No action
			// Just reset the counter
			m_perfCounters[counterIdx].value = 0;
			break;

		case 0x0001: // Generate interrupt
			// Set pending interrupt and reset counter
			m_perfMonInterruptPending = true;
			m_perfMonInterruptVector = 0x40 + counterIdx; // Example vector assignment
			m_perfCounters[counterIdx].value = 0;

			// Trigger actual interrupt if interrupts enabled
			if (m_interruptEnable) {
				triggerPerfMonInterrupt(m_perfMonInterruptVector);
			}
			break;

		case 0x0002: // Stop counting
			// Disable the counter
			m_perfCounters[counterIdx].enabled = false;
			updateMonitoringState();
			break;

		case 0x0003: // Wrap and continue
			// Wrap around to zero
			m_perfCounters[counterIdx].value = 0;
			break;
		}
	}
	/**
	 * @brief Handle a double fault condition
	 *
	 * Called when an exception occurs while handling another exception.
	 * This typically results in a machine check or system halt.
	 */
	void handleDoubleFault() {
		ERROR_LOG("Double fault detected - system halting");

		// Set up machine check state
		m_machineCheckPending = true;
		m_machineCheckPC = m_pc;
		m_machineCheckType = MachineCheckType::DOUBLE_FAULT;

		// Jump to machine check handler
		m_pc = m_palCodeBase + PAL_VECTOR_MACHINE_CHECK;

		// If machine check handler isn't available, halt the system
		if (!machineCheckHandlerAvailable()) {
			halt();
		}
	}

	void handleInterrupts() {
		// Read SIRR and check for pending software interrupts
		quint64 sirr = m_iprs.read(AsaIpr::IprBank::SIRR);
		quint64 ipl = m_iprs.read(AsaIpr::IprBank::IPL);
	}

	/**
	 * @brief Check if CPU has a pending exception
	 *
	 * @return True if exception is pending
	 */
	bool hasException() 
	{
		return m_hasException && m_exceptionPending;
	}
	bool inPALMode() const { return false; }
	// Alternative: Keep both names for compatibility


	bool isRunning() const { return m_running; }
	/**
	 * @brief Check if machine check handler is available
	 * @return True if handler is available and callable
	 */
	bool isMachineCheckHandlerAvailable() const {
		// Check if SCBB is set up and points to valid PAL code
		quint64 scbb = m_iprs.read(Ipr::SCBB);
		if (scbb == 0) {
			return false;
		}

		// Check if we're not already in a machine check
		if (m_machineCheckPending) {
			return false;
		}

		// Verify the machine check vector is accessible
		quint64 mcVector = scbb + PAL_OFFSET_MACHINE_CHECK;
		if (!isAddressAccessible(mcVector)) {
			return false;
		}

		return true;
	}
 /**
 * @brief Invalidate instruction cache
 *
 * Invalidates all entries in the instruction cache. This is typically
 * required after TLB invalidations that affect instruction pages.
 */
void invalidateInstructionCache() {
		if (m_instructionCache) {
			m_instructionCache->invalidateAll();
			DEBUG_LOG("Instruction cache invalidated");
		}

		// Also invalidate any cached instruction translations
		if (m_instructionTlb) {
			m_instructionTlb->invalidateAll();
		}

		// Clear instruction prefetch buffers
		clearInstructionPrefetchBuffers();

		// Update performance counters
		m_icacheInvalidateCount++;
	}
	/**
	 * @brief Invalidate single instruction TLB entry
	 *
	 * Called when TBISI register is written. Invalidates the instruction TLB entry
	 * for the specified virtual address.
	 *
	 * @param va Virtual address to invalidate
	 */
	
	void invalidateReservation(quint64 physicalAddress, int size)
	{

	}



 /**
  * @brief Check if a virtual address is accessible without actually accessing it
  * Useful for prefetch decisions or speculative operations
  */
	bool isAddressAccessible(quint64 vAddr, bool isWrite = false) const {
		if (!m_memorySystem) {
			return false;
		}

		// Use the memory system's probe functionality
		return m_memorySystem->probeAddress(this, vAddr, isWrite);
	}
	/**
	* @brief Check if CPU is halted
	* @return True if CPU is in halted state
	*/
	bool isHalted() const {
		return m_halted;
	}

	bool isMemoryInstruction(quint32 instructionType) {
		// Check opcode category
		switch ((instructionType >> 26) & 0x3F) {
		case 0x08: // LDA
		case 0x09: // LDAH
		case 0x0A: // LDB
		case 0x0B: // LDW
		case 0x0C: // LDL
		case 0x0D: // LDQ
		case 0x0E: // LDL_L
		case 0x0F: // LDQ_L
		case 0x18: // FBEQ
		case 0x19: // FBLT
		case 0x1A: // FBLE
		case 0x1B: // FBGT
		case 0x1C: // STB
		case 0x1D: // STW
		case 0x1E: // STL
		case 0x1F: // STQ
		case 0x2C: // STL_C
		case 0x2D: // STQ_C
		case 0x22: // LDF
		case 0x23: // LDG
		case 0x24: // LDS
		case 0x25: // LDT
		case 0x26: // STF
		case 0x27: // STG
		case 0x28: // STS
		case 0x29: // STT
			return true;

		case 0x10: // Operate format instructions
		case 0x11: // Operate format instructions
		case 0x12: // Operate format instructions
		case 0x13: // Operate format instructions
		case 0x14: // Operate format instructions
		case 0x16: // Floating operate format
		case 0x17: // Floating operate format
			// Need to check memory format (FETCH, MB)
			if ((instructionType & 0x003F0000) == 0x00040000) {
				return true; // Memory barrier or fetch instructions
			}
			return false;

		default:
			return false;
		}
	}
	bool isMMUEnabled() const { return true; }


	/**
	 * @brief Jump to PAL handler
	 *
	 * @param palEntry PAL entry point address
	 * @param level Exception level or parameter
	 */
	void jumpToPalHandler(quint64 palEntry, int level) {
		// Save the exception return address
		m_exceptionReturnAddress = m_pc + 4;  // Next instruction

		// Set up registers for PAL handler
		// In Alpha, certain registers are set up by hardware for PAL

		// R0-R7 may contain exception-specific information
		m_integerRegisters[0] = m_exceptionCause;      // Exception cause
		m_integerRegisters[1] = m_faultingVirtualAddress;  // Faulting VA (if applicable)
		m_integerRegisters[2] = level;                 // Exception level
		m_integerRegisters[3] = m_exceptionPc;         // Faulting PC

		// Jump to PAL handler
		m_pc = palEntry;

		DEBUG_LOG(QString("Jumped to PAL handler at 0x%1, level=%2")
			.arg(palEntry, 16, 16, QChar('0'))
			.arg(level));
	}

	// get translation cache statistics:
	void logTranslationCacheStats() 
	{
// 		if (m_tlbSystem->getStatistics()) {
// 			auto stats = m_translationCache->getStatistics();
// 			qDebug() << QString("CPU%1 Translation Cache Stats: Lookups=%2, Hits=%3, Hit Rate=%4%")
// 				.arg(m_cpuId)
// 				.arg(stats.lookups)
// 				.arg(stats.hits)
// 				.arg(stats.hitRate() * 100.0, 0, 'f', 2);
// 		}
	}

	// If your code expects this exact name without "is" prefix:
	bool machineCheckHandlerAvailable() const {
		// Check if SCBB is set up and points to valid PAL code
		quint64 scbb = m_iprs.read(Ipr::SCBB);
		if (scbb == 0) {
			return false;
		}

		// Check if we're not already in a machine check
		if (m_machineCheckPending) {
			return false;
		}

		// Verify the machine check vector is accessible
		quint64 mcVector = scbb + PAL_OFFSET_MACHINE_CHECK;
		if (!isAddressAccessible(mcVector)) {
			return false;
		}

		return true;
	}

	quint64 readPerformanceCounter(quint32 counterIdx) {
		if (counterIdx >= MAX_PERF_COUNTERS) {
			DEBUG_LOG(QString("AlphaCPU: Invalid counter index for read: %1").arg(counterIdx));
			return 0;
		}

		DEBUG_LOG(QString("AlphaCPU: Reading counter %1: value=0x%2")
			.arg(counterIdx)
			.arg(m_perfCounters[counterIdx].value, 16, 16, QChar('0')));

		return m_perfCounters[counterIdx].value;
	}


	// Memory access
	bool readPhysicalMemory32(quint64 physicalAddress, quint32& value);

	// Processor state

	quint64 getCurrentASN() const;


	// Exception handling
	void triggerException(ExceptionType type, quint64 address) {
		DEBUG_LOG(QString("AlphaCPU: Triggering exception type=%1 at address=0x%2")
			.arg(static_cast<int>(type))
			.arg(address, 16, 16, QChar('0')));

		// Set up exception state
		m_hasException = true;
		m_exceptionPc = address;
		m_currentExceptionType = type;

		// Dispatch exception to PAL code
		jumpToExceptionVector(mapExceptionToFaultType(type));
	}
	void triggerException(FPTrapType type, quint64 address) {
		DEBUG_LOG(QString("AlphaCPU: Triggering exception type=%1 at address=0x%2")
			.arg(static_cast<int>(type))
			.arg(address, 16, 16, QChar('0')));

		// Set up exception state
		m_hasException = true;
		m_exceptionPc = address;
		m_currentExceptionType = type;

		// Dispatch exception to PAL code
		jumpToExceptionVector(mapExceptionToFaultType(type));
	}

	bool handleTLBMiss(quint64 virtualAddress, quint64 asn, bool isInstruction);
/**
 * @brief Increment counter for return address mispredictions
 *
 * This function is called when a branch prediction based on the return stack
 * is found to be incorrect. It updates performance counters that track
 * branch prediction accuracy.
 */
	void incrementReturnMispredictions() {
		// Increment the misprediction counter
		m_returnStackMispredictions++;

		// Log the misprediction
		DEBUG_LOG(QString("AlphaCPU: Return address misprediction detected (total: %1)")
			.arg(m_returnStackMispredictions));

		// Update overall branch statistics
		if (m_executeStage) {
			m_executeStage->updateBranchStatistics(true); // true = mispredicted
		}
	}
	/**
 * @brief Get the value of a specific integer register
 *
 * This function retrieves the current value from the specified integer register.
 * It handles the special case of register 31 (R31), which is hardwired to zero
 * in the Alpha architecture.
 *
 * @param reg Register number (0-31)
 * @return The register value, or 0 if the register is invalid or R31
 */
	quint64 getRegister(quint8 reg) const {
		// Check for valid register number
		if (reg > 31) {
			DEBUG_LOG(QString("AlphaCPU: Invalid register number for read: R%1").arg(reg));
			return 0;
		}

		// R31 is hardwired to zero in Alpha architecture
		if (reg == 31) {
			return 0;
		}

		// Read register value from register bank
		if (m_registerBank) {
			quint64 value = m_registerBank->readIntReg(reg);

			// Log the register read (if debug level is appropriate)
// 			if (TraceManager::instance().getLevel() >= TraceLevel::TRACE_DEBUG) {
// 				DEBUG_LOG(QString("AlphaCPU: Read R%1 = 0x%2").arg(reg).arg(value, 16, 16, QChar('0')));
// 			}

			return value;
		}
		else {
			ERROR_LOG(QString("AlphaCPU: Register bank not available for read from R%1").arg(reg));
			return 0;
		}
	}

	/**
 * @brief Set a specific integer register to a new value
 *
 * This function updates the specified integer register with a new value.
 * It handles the special case of register 31 (R31), which is hardwired to zero
 * in the Alpha architecture and ignores writes.
 *
 * @param reg Register number (0-31)
 * @param value Value to store in the register
 */
	void setRegister(quint8 reg, quint64 value) {
		// Check for valid register number
		if (reg > 31) {
			DEBUG_LOG(QString("AlphaCPU: Invalid register number for write: R%1").arg(reg));
			return;
		}

		// R31 is hardwired to zero in Alpha architecture - ignore writes
		if (reg == 31) {
			DEBUG_LOG("AlphaCPU: Attempted write to R31 (hardwired zero), ignoring");
			return;
		}

		// Update register value in register bank
		if (m_registerBank) {
			m_registerBank->writeIntReg(reg, value);

			// Log the register update
			DEBUG_LOG(QString("AlphaCPU: R%1 = 0x%2").arg(reg).arg(value, 16, 16, QChar('0')));

			// Emit signal to notify observers of register change
			emit sigRegisterUpdated(reg, RegisterType::INTEGER_REG, value);
		}
		else {
			ERROR_LOG(QString("AlphaCPU: Register bank not available for write to R%1").arg(reg));
		}
	}

	/**
	 * @brief Push a return address onto the return address stack
	 *
	 * The Alpha architecture uses a return address stack to predict return
	 * addresses for subroutine calls. This function pushes a new return
	 * address onto this stack when a subroutine call is executed.
	 *
	 * @param address The return address to push onto the stack
	 */
	void pushReturnStack(quint64 address) {
		// Check if the return stack is initialized
		if (m_returnAddressStack.size() != RETURN_STACK_SIZE) {
			m_returnAddressStack.resize(RETURN_STACK_SIZE);
			m_returnStackIndex = 0;
			m_returnStackCount = 0;
		}

		// Store the return address in the stack
		m_returnAddressStack[m_returnStackIndex] = address;

		// Update stack pointer (wrap around if necessary)
		m_returnStackIndex = (m_returnStackIndex + 1) % RETURN_STACK_SIZE;

		// Update count of valid entries (up to the stack size)
		if (m_returnStackCount < RETURN_STACK_SIZE) {
			m_returnStackCount++;
		}

		DEBUG_LOG(QString("AlphaCPU: Pushed return address 0x%1 to return stack (entries: %2)")
			.arg(address, 16, 16, QChar('0'))
			.arg(m_returnStackCount));

		// Update statistics
		m_returnStackPushes++;
	}

	/**
	 * @brief Pop a return address from the return address stack
	 *
	 * This function retrieves the most recently pushed return address from
	 * the return address stack. It is used during subroutine returns to
	 * predict the target address.
	 *
	 * @return The predicted return address, or 0 if the stack is empty
	 */
	quint64 popReturnStack() {
		// Check if there are any entries in the stack
		if (m_returnStackCount == 0) {
			DEBUG_LOG(QString("AlphaCPU: Attempted to pop from empty return stack"));
			m_returnStackUnderflows++;
			return 0;
		}

		// Adjust the index to point to the previous entry
		m_returnStackIndex = (m_returnStackIndex == 0) ?
			RETURN_STACK_SIZE - 1 :
			m_returnStackIndex - 1;

		// Decrement the count of valid entries
		m_returnStackCount--;

		// Get the return address
		quint64 returnAddress = m_returnAddressStack[m_returnStackIndex];

		DEBUG_LOG(QString("AlphaCPU: Popped return address 0x%1 from return stack (remaining: %2)")
			.arg(returnAddress, 16, 16, QChar('0'))
			.arg(m_returnStackCount));

		// Update statistics
		m_returnStackPops++;

		return returnAddress;
	}
	quint64 getPC()  { return m_pc; }
	void setPC(quint64 newPC) { m_pc = newPC; }
	void flushPipeline() {
	}

	// Processor state
	bool isKernelMode() const;
	/**
	 * @brief Check if floating-point is enabled
	 * @return True if FP operations are enabled
	 */
	bool isFloatingPointEnabled() const
	{
		// Check if floating-point is enabled in processor status
		return (getProcessorStatus() & PS_FP_ENABLE) != 0;
	}

	/**
	 * @brief Check if value is floating-point zero
	 * @param value Value to check (raw register value)
	 * @return True if value represents FP zero
	 */
	bool isFloatZero(quint64 value) const
	{
		double val = std::bit_cast<double>(value);
		return val == 0.0 || val == -0.0;
	}
	/**
 * @brief Check if value is floating-point negative
 * @param value Value to check (raw register value)
 * @return True if value is negative
 */
	bool isFloatNegative(quint64 value) const {
		double val = std::bit_cast<double>(value);
		return std::signbit(val);
	}
	/**
 * @brief Get const access to RegisterFileWrapper FP register file
 * @return Const reference to FP register file
 */
	FpRegs& getFpRegs()
	{
		if (!m_registerBank.data()) {
			throw std::runtime_error("AlphaCPU: Register bank not initialized");
		}
		return m_registerBank->fp();
	}
	
	void start() {
		stopRequested.storeRelaxed(false);
	}

	// PAL operations

	
	

	bool isKernelMode() { return m_currentMode == ProcessorMode::KERNEL; }

	// Compare two IEEE T format values
	quint64 compareTFormat(quint64 a, quint64 b, FPCompareType compareType) {
		// Convert raw bits to double precision values
		double aValue = std::bit_cast<double>(a);
		double bValue = std::bit_cast<double>(b);

		// Perform comparison based on compareType
		bool result = false;

		switch (compareType) {
		case FPCompareType::FP_EQUAL:
			result = (aValue == bValue);
			break;
		case FPCompareType::FP_LESS:
			result = (aValue < bValue);
			break;
		case FPCompareType::FP_LESS_EQUAL:
			result = (aValue <= bValue);
			break;
		case FPCompareType::FP_UNORDERED:
			result = (std::isnan(aValue) || std::isnan(bValue));
			break;
		default:
			// Unknown comparison type
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return 0;
		}

		// If either operand is NaN and we're not testing for unordered,
		// set invalid operation flag
		if ((std::isnan(aValue) || std::isnan(bValue)) &&
			compareType != FPCompareType::FP_UNORDERED) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			// For unordered comparisons with NaN, Alpha architecture returns 0
			return 0;
		}

		// Return 1 if condition is true, 0 otherwise
		return result ? 1 : 0;
	}
	// Floating-point division for IEEE S format values
	quint64 divSFormat(quint64 a, quint64 b) {
		// Convert to native double precision values
		float aValue = std::bit_cast<float>(static_cast<quint32>(a));
		float bValue = std::bit_cast<float>(static_cast<quint32>(b));

		// Check for division by zero
		if (bValue == 0.0f) {
			// Set division by zero flag
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_DivZero();
			}
			// Return appropriate NaN value
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Perform division
		float result;
		try {
			result = aValue / bValue;
		}
		catch (...) {
			// Handle computation errors (shouldn't happen in normal floating-point)
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
		}
		else if (std::isinf(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
		}

		// Convert result to double precision (IEEE T format)
		double doubleResult = static_cast<double>(result);

		// Return the bits
		return std::bit_cast<quint64>(doubleResult);
	}

	// Floating-point division for IEEE T format values
	quint64 divTFormat(quint64 a, quint64 b) {
		// Convert raw bits to double precision values
		double aValue = std::bit_cast<double>(a);
		double bValue = std::bit_cast<double>(b);

		// Check for division by zero
		if (bValue == 0.0) {
			// Set division by zero flag
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_DivZero();
			}
			// Return appropriate NaN value
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Perform division
		double result;
		try {
			result = aValue / bValue;
		}
		catch (...) {
			// Handle computation errors
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
		}
		else if (std::isinf(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
		}

		// Return the bits
		return std::bit_cast<quint64>(result);
	}

	void drainAborts() {
		DEBUG_LOG("AlphaCPU: Draining aborts");

		// Complete all pending memory operations
		flushPendingMemoryOperations();

		// Ensure memory barrier
		if (m_memorySystem) {
			m_memorySystem->flushWriteBuffers(this);
		}
	}
	// TODO: executeConsoleService
	void executeConsoleService() {
		// Implementation dependent on console service codes
		DEBUG_LOG("AlphaCPU: Console service requested");
		// Would typically interact with simulated console device
	}

	void disableInterrupts() {
		m_interruptEnable = false;

		// Update processor status register
		quint64 ps = getProcessorStatus();
		ps &= ~PS_INTERRUPT_ENABLE;
		setProcessorStatus(ps);
	}

	void enableInterrupts() {
		m_interruptEnable = true;

		// Update processor status register
		quint64 ps = getProcessorStatus();
		ps |= PS_INTERRUPT_ENABLE;
		setProcessorStatus(ps);

		// Check for pending interrupts now that interrupts are enabled
		checkPendingInterrupts();
	}
	void flushCaches() {
		DEBUG_LOG("AlphaCPU: Flushing all caches");

		// Flush instruction cache
		if (m_instructionCache) {
			m_instructionCache->invalidateAll();
		}

		// Flush data caches
		if (m_level1DataCache) {
			m_level1DataCache->invalidateAll();
		}

		if (m_level2DataCache) {
			m_level2DataCache->invalidateAll();
		}
		if (m_memorySystem)
		{
			m_memorySystem->getTlbSystem()->invalidateByASN(m_currentASN);
		}
	}
	void incrementReturnMispredictions();
	quint64 insertQueueHeadLW(quint64 queueAddr, quint64 entryAddr) {
		// Insert at head of longword queue with interlocked operation
		quint64 result = 0;
		quint32 queueHeader = 0;
		quint32 headOffset = 0;

		// Read queue header atomically
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, &queueHeader, 4)) {
			return 2; // Queue header read failed
		}

		// Get head pointer from queue header
		headOffset = queueHeader;

		// Set forward link in new entry to point to current head
		if (!m_memorySystem->writeVirtualMemory(this, entryAddr, &headOffset, 4)) {
			return 2; // Entry write failed
		}

		// Update queue header to point to new entry
		quint32 entryOffset = static_cast<quint32>(entryAddr - queueAddr);

		// Write queue header atomically, checking if it was changed
		if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, &entryOffset, 4, queueHeader)) {
			return 1; // Queue was modified - retry needed
		}

		return 0; // Success
	}

	quint64 insertQueueTailLW(quint64 queueAddr, quint64 entryAddr) {
		// Insert at tail of longword queue with interlocked operation
		quint64 result = 0;
		quint32 queueHeader[2]; // [0] = head, [1] = tail

		// Read queue header atomically (both head and tail)
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, queueHeader, 4)) {
			return 2; // Queue header read failed
		}

		// Get tail pointer
		quint32 tailOffset = queueHeader[1];

		if (tailOffset == 0) {
			// Empty queue, head and tail will point to new entry
			quint32 entryOffset = static_cast<quint32>(entryAddr - queueAddr);

			// Clear forward link in new entry
			quint32 zero = 0;
			if (!m_memorySystem->writeVirtualMemory(this, entryAddr, &zero, 4)) {
				return 2; // Entry write failed
			}

			// Update both head and tail pointers atomically
			queueHeader[0] = entryOffset;
			queueHeader[1] = entryOffset;

			if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, queueHeader, 4, *reinterpret_cast<quint64*>(queueHeader))) {
				return 1; // Queue was modified - retry needed
			}
		}
		else {
			// Non-empty queue, append to tail
			quint64 tailEntryAddr = queueAddr + tailOffset;
			quint32 entryOffset = static_cast<quint32>(entryAddr - queueAddr);

			// Clear forward link in new entry
			quint32 zero = 0;
			if (!m_memorySystem->writeVirtualMemory(this, entryAddr, &zero, 4)) {
				return 2; // Entry write failed
			}

			// Update forward link in current tail to point to new entry
			if (!m_memorySystem->writeVirtualMemoryConditional(this, tailEntryAddr, &entryOffset, 4, 0)) {
				return 1; // Tail entry was modified - retry needed
			}

			// Update tail pointer in queue header
			if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr + 4, &entryOffset, 4, tailOffset)) {
				return 1; // Queue tail was modified - retry needed
			}
		}

		return 0; // Success
	}
	quint64 insertQueueTailQW(quint64 queueAddr, quint64 entryAddr) {
		// Insert at tail of longword queue with interlocked operation
		quint64 result = 0;
		quint32 queueHeader[2]; // [0] = head, [1] = tail

		// Read queue header atomically (both head and tail)
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, queueHeader, 8)) {
			return 2; // Queue header read failed
		}

		// Get tail pointer
		quint32 tailOffset = queueHeader[1];

		if (tailOffset == 0) {
			// Empty queue, head and tail will point to new entry
			quint32 entryOffset = static_cast<quint32>(entryAddr - queueAddr);

			// Clear forward link in new entry
			quint32 zero = 0;
			if (!m_memorySystem->writeVirtualMemory(this, entryAddr, &zero, 8)) {
				return 2; // Entry write failed
			}

			// Update both head and tail pointers atomically
			queueHeader[0] = entryOffset;
			queueHeader[1] = entryOffset;

			if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, queueHeader, 8, *reinterpret_cast<quint64*>(queueHeader))) {
				return 1; // Queue was modified - retry needed
			}
		}
		else {
			// Non-empty queue, append to tail
			quint64 tailEntryAddr = queueAddr + tailOffset;
			quint32 entryOffset = static_cast<quint32>(entryAddr - queueAddr);

			// Clear forward link in new entry
			quint32 zero = 0;
			if (!m_memorySystem->writeVirtualMemory(this, entryAddr, &zero, 4)) {
				return 2; // Entry write failed
			}

			// Update forward link in current tail to point to new entry
			if (!m_memorySystem->writeVirtualMemoryConditional(this, tailEntryAddr, &entryOffset, 4, 0)) {
				return 1; // Tail entry was modified - retry needed
			}

			// Update tail pointer in queue header
			if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr + 4, &entryOffset, 4, tailOffset)) {
				return 1; // Queue tail was modified - retry needed
			}
		}

		return 0; // Success
	}
	// Similar implementations for quadword queue operations
	quint64 insertQueueHeadQW(quint64 queueAddr, quint64 entryAddr) {
		// Insert at head of quadword queue with interlocked operation
		// Implementation similar to insertQueueHeadLW but using 8-byte operations
		quint64 result = 0;
		quint64 queueHeader = 0;

		// Read queue header atomically
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, &queueHeader, 8)) {
			return 2; // Queue header read failed
		}

		// Get head pointer from queue header
		quint64 headOffset = queueHeader;

		// Set forward link in new entry to point to current head
		if (!m_memorySystem->writeVirtualMemory(this, entryAddr, &headOffset, 8)) {
			return 2; // Entry write failed
		}

		// Update queue header to point to new entry
		quint64 entryOffset = entryAddr - queueAddr;

		// Write queue header atomically, checking if it was changed
		if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, &entryOffset, 8, queueHeader)) {
			return 1; // Queue was modified - retry needed
		}

		return 0; // Success
	}

	/**
 * @brief Notifies other CPUs in the system about a critical entry point change
 *
 * In a multiprocessor system, changes to critical system entry points
 * need to be propagated to all CPUs.
 *
 * @param type The entry point type that changed
 * @param address The new address for the entry point
 */
	void notifySystemEntryPointChange(quint64 type, quint64 address) {
		if (m_AlphaSMPManager) {
			// Notify the SMP manager about the change
			m_AlphaSMPManager->notifySystemEntryChange(type, address);

			DEBUG_LOG(QString("AlphaCPU: Notified all CPUs of entry point change: type=%1, address=0x%2")
				.arg(type)
				.arg(address, 16, 16, QChar('0')));
		}
	}
	// Control flow

	/**
     * @brief Pop a return address from the return address stack
     *
     * This function retrieves the most recently pushed return address from
     * the return address stack. It is used during subroutine returns to
     * predict the target address.
     *
     * @return The predicted return address, or 0 if the stack is empty
     */
	quint64 popReturnStack() {
		// Check if there are any entries in the stack
		if (m_returnStackCount == 0) {
			DEBUG_LOG(QString("CPU%1: Attempted to pop from empty return stack")
				.arg(m_cpuId));
			m_returnStackUnderflows++;
			return 0;
		}

		// Adjust the index to point to the previous entry
		m_returnStackIndex = (m_returnStackIndex == 0) ?
			RETURN_STACK_SIZE - 1 :
			m_returnStackIndex - 1;

		// Decrement the count of valid entries
		m_returnStackCount--;

		// Get the return address
		quint64 returnAddress = m_returnAddressStack[m_returnStackIndex];

		DEBUG_LOG(QString("CPU%1: Popped return address 0x%2 from return stack (remaining: %3)")
			.arg(m_cpuId)
			.arg(returnAddress, 16, 16, QChar('0'))
			.arg(m_returnStackCount));

		// Update statistics
		m_returnStackPops++;

		return returnAddress;
	}
	/**
     * @brief Push an address onto the return address stack
     *
     * The Alpha architecture uses a return address stack to predict return
     * addresses for subroutine calls. This function pushes a new return
     * address onto this stack when a subroutine call is executed.
     *
     * @param address The return address to push onto the stack
     */
	void pushReturnStack(quint64 address) {
		// Check if the return stack is initialized
		if (m_returnAddressStack.size() != RETURN_STACK_SIZE) {
			m_returnAddressStack.resize(RETURN_STACK_SIZE);
			m_returnStackIndex = 0;
			m_returnStackCount = 0;
		}

		// Store the return address in the stack
		m_returnAddressStack[m_returnStackIndex] = address;

		// Update stack pointer (wrap around if necessary)
		m_returnStackIndex = (m_returnStackIndex + 1) % RETURN_STACK_SIZE;

		// Update count of valid entries (up to the stack size)
		if (m_returnStackCount < RETURN_STACK_SIZE) {
			m_returnStackCount++;
		}

		DEBUG_LOG(QString("CPU%1: Pushed return address 0x%2 to return stack (entries: %3)")
			.arg(m_cpuId)
			.arg(address, 16, 16, QChar('0'))
			.arg(m_returnStackCount));

		// Update statistics
		m_returnStackPushes++;
	}

	quint64 readASN() {
		return m_iprs.read(IprBank::ASN);
	}
	quint64 readASTEN() {
		return m_iprs.read(IprBank::ASTEN);
	}
	quint64 readASTSR() {
		return m_iprs.read(IprBank::ASTSR);
	}

	quint64 readESP() {
		return m_iprs.read(IprBank::ESP);
	}
	quint64 readFEN() {
		return m_iprs.read(IprBank::FEN);
	}
	uint64_t readIntReg(unsigned idx) { return m_registerBank->readInt(idx); }


	quint64 readIRQL() {
		return m_iprs.read(IprBank::IPL);
	}
	quint64 readMCES() {
		return m_iprs.read(IprBank::MCES);
	}
	quint64 readPCBB() {
		return m_iprs.read(IprBank::PCBB);
	}
	quint64 readPRBR() {
		return m_iprs.read(IprBank::PRBR);
	}
	quint64 readProcessorStatus() {
		return getProcessorStatus();
	}
	quint64 readPTBR() {
		return m_iprs.read(IprBank::PTBR);
	}
	quint64 readSCBB() {
		return m_iprs.read(IprBank::SCBB);
	}

	quint64 readSISR() {
		return m_iprs.read(IprBank::SISR);
	}
	quint64 readSSP() {
		return m_iprs.read(IprBank::SSP);
	}
	quint64 readUSP() {
		return m_iprs.read(IprBank::USP);
	}
	quint64 readVAL() {
		// Implementation would depend on what VAL is used for
		return m_iprs.read(IprBank::VAL);
	}
	quint64 readVPTB() {
		return m_iprs.read(IprBank::VPTB);
	}
	quint64 readWHAMI() {
		// Returns the processor ID
		return m_cpuId;
	}
	quint64 removeQueueHeadLW(quint64 queueAddr, quint64* removedEntryAddr) {
		// Remove entry from head of longword queue with interlocked operation
		quint32 queueHeader = 0;

		// Read queue header atomically
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, &queueHeader, 4)) {
			return 2; // Queue header read failed
		}

		// Check if queue is empty
		if (queueHeader == 0) {
			*removedEntryAddr = 0;
			return 1; // Queue empty
		}

		// Calculate address of first entry
		quint64 headEntryAddr = queueAddr + queueHeader;
		*removedEntryAddr = headEntryAddr;

		// Read forward link of head entry
		quint32 nextOffset = 0;
		if (!m_memorySystem->readVirtualMemory(this, headEntryAddr, &nextOffset, 4)) {
			return 2; // Head entry read failed
		}

		// Update queue header to point to next entry
		if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, &nextOffset, 4, queueHeader)) {
			return 1; // Queue was modified - retry needed
		}

		return 0; // Success
	}

	quint64 removeQueueHeadQW(quint64 queueAddr, quint64* removedEntryAddr) {
		// Remove entry from head of longword queue with interlocked operation
		quint32 queueHeader = 0;

		// Read queue header atomically
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, &queueHeader, 8)) {
			return 2; // Queue header read failed
		}

		// Check if queue is empty
		if (queueHeader == 0) {
			*removedEntryAddr = 0;
			return 1; // Queue empty
		}

		// Calculate address of first entry
		quint64 headEntryAddr = queueAddr + queueHeader;
		*removedEntryAddr = headEntryAddr;

		// Read forward link of head entry
		quint32 nextOffset = 0;
		if (!m_memorySystem->readVirtualMemory(this, headEntryAddr, &nextOffset, 8)) {
			return 2; // Head entry read failed
		}

		// Update queue header to point to next entry
		if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, &nextOffset, 8, queueHeader)) {
			return 1; // Queue was modified - retry needed
		}

		return 0; // Success
	}
	quint64 removeQueueHeadQW(quint64 queueAddr, quint64* removedEntryAddr) {
		// Implementation for quadword queue head removal
		// Similar to removeQueueHeadLW but with 8-byte operations
		return 0;
	}

	quint64 removeQueueTailLW(quint64 queueAddr, quint64* removedEntryAddr) {
		// Remove entry from tail of longword queue with interlocked operation
		// This operation requires traversing the queue to find the tail

		// Queue header structure: [0] = head offset, [1] = tail offset
		quint32 queueHeader[2];

		// Read queue header atomically (both head and tail fields)
		if (!m_memorySystem->readVirtualMemoryAtomic(this, queueAddr, queueHeader, 8)) {
			return 2; // Queue header read failed - fatal error
		}

		// Check if queue is empty
		if (queueHeader[0] == 0) {
			*removedEntryAddr = 0;
			return 1; // Queue empty
		}

		// Calculate tail entry address from tail offset in header
		quint64 tailEntryAddr = queueAddr + queueHeader[1];
		*removedEntryAddr = tailEntryAddr;

		// Special case: If head and tail are the same, there's only one entry
		if (queueHeader[0] == queueHeader[1]) {
			// Queue will be empty after removal
			quint32 zero = 0;

			// Update both head and tail to zero (atomic 8-byte write)
			if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr, &zero, 8,
				*reinterpret_cast<quint64*>(queueHeader))) {
				return 3; // Queue was modified - retry needed
			}

			return 0; // Success
		}

		// If we reach here, we need to find the entry before the tail
		quint64 currentAddr = queueAddr + queueHeader[0]; // Start at head
		quint64 prevAddr = 0;
		quint32 nextOffset = 0;

		// Loop protection - we'll limit traversal to a reasonable value
		const int MAX_TRAVERSAL = 1000;
		int count = 0;

		// Traverse the queue until we find the entry that points to the tail
		while (currentAddr != tailEntryAddr && count < MAX_TRAVERSAL) {
			prevAddr = currentAddr;

			// Read next link from current entry
			if (!m_memorySystem->readVirtualMemory(this, currentAddr, &nextOffset, 4)) {
				return 2; // Read failed - fatal error
			}

			// If next link is 0, we've reached end of queue 
			// This shouldn't happen if the queue is consistent
			if (nextOffset == 0) {
				DEBUG_LOG("AlphaCPU: Inconsistent queue structure detected");
				return 2; // Queue structure error
			}

			// Move to next entry
			currentAddr = queueAddr + nextOffset;
			count++;
		}

		// If we hit our traversal limit, the queue might be corrupted
		if (count >= MAX_TRAVERSAL) {
			DEBUG_LOG("AlphaCPU: Queue traversal limit exceeded");
			return 2; // Queue structure error
		}

		// At this point, prevAddr contains the address of the entry before the tail

		// Clear the link in the entry before tail
		quint32 zero = 0;
		if (!m_memorySystem->writeVirtualMemoryConditional(this, prevAddr, &zero, 4,
			static_cast<quint32>(tailEntryAddr - queueAddr))) {
			return 3; // Entry was modified - retry needed
		}

		// Update tail pointer in queue header to point to the new tail
		quint32 newTailOffset = static_cast<quint32>(prevAddr - queueAddr);
		if (!m_memorySystem->writeVirtualMemoryConditional(this, queueAddr + 4, &newTailOffset, 4,
			queueHeader[1])) {
			return 3; // Queue tail was modified - retry needed
		}

		return 0; // Success
	}
	quint64 removeQueueTailQW(quint64 queueAddr, quint64* removedEntryAddr) {
		// Implementation for quadword queue tail removal
		return 1; // Placeholder - similar complexity to LW version
	}
	void reset() {
		m_pc = 0;

		emit sigHandleReset();
	}
	void resetPerformanceCounters() {
		DEBUG_LOG("AlphaCPU: Resetting all performance counters");

		// Reset all counter values to zero
		for (int i = 0; i < MAX_PERF_COUNTERS; i++) {
			m_perfCounters[i].value = 0;
			// Don't change configuration, just clear values
		}

		// Reset profiling state
		m_profilingActive = false;
		m_profilingSamplingRate = 0;

		// Clear any pending performance monitor interrupts
		m_perfMonInterruptPending = false;
	}


	quint64 swapContext(quint64 newContext) {
		// This would typically save/restore register state and address space
		quint64 oldContext = m_iprs.read(IprBank::PCBB);
		m_iprs.write(IprBank::PCBB, newContext);

		// Actual context switch would involve more state management
		// including potentially changing address space and loading registers

		// Invalidate TLB for context switch
		invalidateTBAllProcess();

		return oldContext;
	}
	quint64 swapIRQL(quint64 newLevel) {
		quint64 oldLevel = m_iprs.read(IprBank::IPL);
		m_iprs.write(IprBank::IPL, newLevel);

		// If lowering IRQL, check for pending interrupts
		if (newLevel < oldLevel) {
			checkPendingInterrupts();
		}

		return oldLevel;
	}
	quint64 swapPALBase(quint64 newBase) {
		quint64 oldBase = m_palCodeBase;
		m_palCodeBase = newBase;
		return oldBase;
	}


	/**
 * @brief Updates a jump target in memory at the specified address
 *
 * This helper function updates a memory location to contain a jump
 * instruction (BR) to the target address. This is how entry points
 * are typically implemented in Alpha systems.
 *
 * @param address Address where the jump instruction should be stored
 * @param target Target address to jump to
 */
	void updateMemoryJumpTarget(quint64 address, quint64 target) {
		// Convert target address to a BR (branch) instruction
		// BR instruction format: opcode 0x30, with displacement in 21-bit signed value

		// Calculate the branch displacement (in instructions, not bytes)
		qint64 displacement = ((qint64)target - (qint64)address) / 4;

		// Check if displacement is within range for BR instruction (21-bit signed)
		if (displacement < -1048576 || displacement > 1048575) {
			DEBUG_LOG(QString("AlphaCPU: Branch displacement out of range: %1").arg(displacement));
			// In a real system, this would be a fatal error
			// For our emulator, we'll use a workaround

			// Create a sequence that loads the address and jumps to it
			// 1. LDA R0, <lower 16 bits of address>
			quint32 instr1 = 0x20000000 | (0 << 21) | (31 << 16) | (target & 0xFFFF);

			// 2. LDAH R0, <upper 16 bits of address>
			quint32 instr2 = 0x24000000 | (0 << 21) | (0 << 16) | ((target >> 16) & 0xFFFF);

			// 3. JMP R31, (R0)
			quint32 instr3 = 0x6BFC0000 | (31 << 21) | (0 << 16);

			// Write the instruction sequence
			m_safeMemory->writeUInt32(address, instr1);
			m_safeMemory->writeUInt32(address + 4, instr2);
			m_safeMemory->writeUInt32(address + 8, instr3);
		}
		else {
			// Create a simple branch instruction
			quint32 brInstruction = 0x30000000 | (displacement & 0x1FFFFF);

			// Write the branch instruction to memory
			m_safeMemory->writeUInt32(address, brInstruction);
		}
	}
	/**
 * @brief Updates system control blocks after entry point changes
 *
 * This internal method ensures that system control blocks like the SCBB
 * (System Control Block Base) are properly updated to reflect changes
 * in system entry points.
 */
	void updateSystemControlBlocks() {
		// Get the current SCBB value
		quint64 scbb = m_iprs.read(IprBank::SCBB);

		// If SCBB is zero, we need to initialize it first
		if (scbb == 0) {
			// In a real system, this would allocate memory for the SCB
			// In our emulator, we'll use a predefined region
			scbb = 0xFFFFFE0000000000ULL;  // Typical value in Alpha systems
			m_iprs.write(IprBank::SCBB, scbb);

			DEBUG_LOG(QString("AlphaCPU: Initialized SCBB to 0x%1")
				.arg(scbb, 16, 16, QChar('0')));
		}

		// For PALcode implementations that use memory-based control blocks,
		// we would update the control blocks in memory here.

		// Different PALcode implementations handle this differently:
		switch (m_palcodeType) {
		case PAL_TYPE_VMS:
			// OpenVMS directly uses the system entry table in memory
			updateVMSSystemControlBlock(scbb);
			break;

		case PAL_TYPE_UNIX:
			// Tru64/Digital UNIX uses a different layout
			updateUNIXSystemControlBlock(scbb);
			break;

		case PAL_TYPE_NT:
			// Windows NT uses yet another approach
			updateNTSystemControlBlock(scbb);
			break;

		default:
			// Generic approach - just store entries in our table
			break;
		}
	}

	/**
 * @brief Updates OpenVMS system control block in memory
 *
 * @param scbb System Control Block Base address
 */
	void updateVMSSystemControlBlock(quint64 scbb) {
		// OpenVMS maintains a table of exception vectors at SCBB
		// Each vector is 8 bytes apart and contains a jump instruction

		// Example for key vectors:
		updateMemoryJumpTarget(scbb + 0x0000, m_systemEntryPoints.reset);
		updateMemoryJumpTarget(scbb + 0x0080, m_systemEntryPoints.machineCheck);
		updateMemoryJumpTarget(scbb + 0x0100, m_systemEntryPoints.kernelStackNotValid);
		updateMemoryJumpTarget(scbb + 0x0180, m_systemEntryPoints.powerFail);
		updateMemoryJumpTarget(scbb + 0x0200, m_systemEntryPoints.memoryFault);
		updateMemoryJumpTarget(scbb + 0x0280, m_systemEntryPoints.arithmeticTrap);
		updateMemoryJumpTarget(scbb + 0x0300, m_systemEntryPoints.interrupt);
		updateMemoryJumpTarget(scbb + 0x0380, m_systemEntryPoints.astEntry);

		// And so on for other vectors...
	}


	void writeASTEN(quint64 value) {
		m_iprs.write(IprBank::ASTEN, value);
		// Check for pending ASTs that might be enabled now
		checkPendingAst();
	}
	void writeASTSR(quint64 value) {
		m_iprs.write(IprBank::ASTSR, value);
	}
	void writeESP(quint64 value) {
		m_iprs.write(IprBank::ESP, value);
	}
	void writeFEN(quint64 value) {
		m_iprs.write(IprBank::FEN, value & 1);
		m_fpEnable = (value & 1) != 0;
	}
	void writeKGP(quint64 value) {
		// Write kernel global pointer
		m_iprs.write(IprBank::KGP, value);
	}

	void writeIPIR(quint64 value) {
		m_iprs.write(IprBank::IPIR, value);
		// Check for pending interrupts
		checkPendingInterrupts();
	}
	void writeMCES(quint64 value) {
		m_iprs.write(IprBank::MCES, value);
	}

	void writePerfMon(quint64 function, quint64 value) {
		DEBUG_LOG(QString("AlphaCPU: Write to performance monitor: function=0x%1, value=0x%2")
			.arg(function, 16, 16, QChar('0'))
			.arg(value, 16, 16, QChar('0')));

		// The function code determines which performance monitoring operation to perform
		switch (function) {
		case 0x0000: // Reset all counters
			resetPerformanceCounters();
			break;

		case 0x0001: // Enable specific counter
			enablePerformanceCounter(value);
			break;

		case 0x0002: // Disable specific counter
			disablePerformanceCounter(value);
			break;

		case 0x0003: // Configure counter
		{
			quint32 counterIdx = (value >> 48) & 0xFFFF;    // Extract counter index
			quint32 eventType = (value >> 32) & 0xFFFF;     // Extract event type
			quint32 counterCtrl = value & 0xFFFFFFFF;       // Extract control bits
			configurePerformanceCounter(counterIdx, eventType, counterCtrl);
		}
		break;

		case 0x0004: // Read counter value
		{
			quint32 counterIdx = value & 0xFFFF;
			quint64 counterValue = readPerformanceCounter(counterIdx);
			m_registerBank->writeIntReg(0, counterValue); // Return in R0
		}
		break;

		case 0x0005: // Set counter value (for testing or initialization)
		{
			quint32 counterIdx = (value >> 48) & 0xFFFF;
			quint64 counterValue = value & 0xFFFFFFFFFFFFULL;
			setPerformanceCounter(counterIdx, counterValue);
		}
		break;

		case 0x0006: // Configure counter overflow handling
		{
			quint32 counterIdx = (value >> 48) & 0xFFFF;
			quint32 overflowAction = (value >> 32) & 0xFFFF;
			quint32 overflowThreshold = value & 0xFFFFFFFF;
			configureCounterOverflow(counterIdx, overflowAction, overflowThreshold);
		}
		break;

		case 0x0007: // Start profiling session
		{
			quint64 samplingRate = value;
			startProfilingSession(samplingRate);
		}
		break;

		case 0x0008: // Stop profiling session
			stopProfilingSession();
			break;

		case 0x0009: // Configure enhanced features (EV6 and later)
			configureEnhancedMonitoring(value);
			break;

		case 0x000A: // Set filtering parameters
		{
			quint32 filterType = (value >> 48) & 0xFFFF;
			quint64 filterValue = value & 0xFFFFFFFFFFFFULL;
			setMonitoringFilter(filterType, filterValue);
		}
		break;

		default:
			DEBUG_LOG(QString("AlphaCPU: Unknown performance monitor function: 0x%1")
				.arg(function, 16, 16, QChar('0')));
			break;
		}
	}


	void writePRBR(quint64 value) {
		m_iprs.write(IprBank::PRBR, value);
	}
	void writeSCBB(quint64 value) {
		m_iprs.write(IprBank::SCBB, value);
	}
	
	void writeSIRR(quint64 value) {
		m_iprs.write(IprBank::SIRR, value);
		// Check for pending software interrupts
		checkSoftwareInterrupts();
	}
	void writeSSP(quint64 value) {
		m_iprs.write(IprBank::SSP, value);
	}
	/**
 * @brief Sets up a system entry point for a specific exception type
 *
 * This function configures the entry point address that the CPU will jump to
 * when a specific type of exception occurs. Alpha CPUs maintain a table of
 * these entry points as part of their exception handling architecture.
 *
 * @param address The virtual address to jump to when the exception occurs
 * @param type The exception type ID (determines which entry to modify)
 */
	void writeSystemEntry(quint64 address, quint64 type) {
		DEBUG_LOG(QString("AlphaCPU: System entry point set: type=%1, address=0x%2")
			.arg(type)
			.arg(address, 16, 16, QChar('0')));

		// Validate the entry type is in valid range
		if (type >= MAX_SYSTEM_ENTRY_POINTS) {
			DEBUG_LOG(QString("AlphaCPU: Invalid system entry type: %1").arg(type));
			return;
		}

		// Make sure address is properly aligned (must be quadword aligned)
		if (address & 0x7) {
			DEBUG_LOG(QString("AlphaCPU: Misaligned system entry address: 0x%1")
				.arg(address, 16, 16, QChar('0')));
			// In real hardware this might trigger an exception, but we'll just fix it
			address = address & ~0x7;
		}

		// Alpha system entry types (these vary by PALcode implementation)
		switch (type) {
		case 0:  // RESET entry point
			DEBUG_LOG("AlphaCPU: Setting RESET entry point");
			m_systemEntryPoints.reset = address;
			break;

		case 1:  // MACHINE_CHECK entry point
			DEBUG_LOG("AlphaCPU: Setting MACHINE_CHECK entry point");
			m_systemEntryPoints.machineCheck = address;
			break;

		case 2:  // KERNEL_STACK_NOT_VALID entry point
			DEBUG_LOG("AlphaCPU: Setting KERNEL_STACK_NOT_VALID entry point");
			m_systemEntryPoints.kernelStackNotValid = address;
			break;

		case 3:  // POWER_FAIL entry point
			DEBUG_LOG("AlphaCPU: Setting POWER_FAIL entry point");
			m_systemEntryPoints.powerFail = address;
			break;

		case 4:  // MEMORY_FAULT entry point
			DEBUG_LOG("AlphaCPU: Setting MEMORY_FAULT entry point");
			m_systemEntryPoints.memoryFault = address;
			break;

		case 5:  // ARITHMETIC_TRAP entry point
			DEBUG_LOG("AlphaCPU: Setting ARITHMETIC_TRAP entry point");
			m_systemEntryPoints.arithmeticTrap = address;
			break;

		case 6:  // INTERRUPT entry point
			DEBUG_LOG("AlphaCPU: Setting INTERRUPT entry point");
			m_systemEntryPoints.interrupt = address;
			break;

		case 7:  // AST_ENTRY entry point
			DEBUG_LOG("AlphaCPU: Setting AST_ENTRY entry point");
			m_systemEntryPoints.astEntry = address;
			break;

		case 8:  // ALIGNMENT_FAULT entry point
			DEBUG_LOG("AlphaCPU: Setting ALIGNMENT_FAULT entry point");
			m_systemEntryPoints.alignmentFault = address;
			break;

		case 9:  // TRANSLATION_INVALID entry point
			DEBUG_LOG("AlphaCPU: Setting TRANSLATION_INVALID entry point");
			m_systemEntryPoints.translationInvalid = address;
			break;

		case 10: // ACCESS_VIOLATION entry point
			DEBUG_LOG("AlphaCPU: Setting ACCESS_VIOLATION entry point");
			m_systemEntryPoints.accessViolation = address;
			break;

		case 11: // OPCODE_INVALID entry point
			DEBUG_LOG("AlphaCPU: Setting OPCODE_INVALID entry point");
			m_systemEntryPoints.opcodeInvalid = address;
			break;

		case 12: // FLOATING_POINT_EXCEPTION entry point
			DEBUG_LOG("AlphaCPU: Setting FLOATING_POINT_EXCEPTION entry point");
			m_systemEntryPoints.floatingPointException = address;
			break;

		case 13: // DEVICE_INTERRUPT entry point
			DEBUG_LOG("AlphaCPU: Setting DEVICE_INTERRUPT entry point");
			m_systemEntryPoints.deviceInterrupt = address;
			break;

		case 14: // SYSTEM_CALL entry point
			DEBUG_LOG("AlphaCPU: Setting SYSTEM_CALL entry point");
			m_systemEntryPoints.systemCall = address;
			break;

			// OpenVMS-specific entry points
		case 20: // CHANGE_MODE_TO_KERNEL entry point
			DEBUG_LOG("AlphaCPU: Setting CHANGE_MODE_TO_KERNEL entry point");
			m_systemEntryPoints.changeModeKernel = address;
			break;

		case 21: // CHANGE_MODE_TO_EXEC entry point
			DEBUG_LOG("AlphaCPU: Setting CHANGE_MODE_TO_EXEC entry point");
			m_systemEntryPoints.changeModeExec = address;
			break;

		case 22: // CHANGE_MODE_TO_SUPER entry point
			DEBUG_LOG("AlphaCPU: Setting CHANGE_MODE_TO_SUPER entry point");
			m_systemEntryPoints.changeModeSuper = address;
			break;

		case 23: // CHANGE_MODE_TO_USER entry point
			DEBUG_LOG("AlphaCPU: Setting CHANGE_MODE_TO_USER entry point");
			m_systemEntryPoints.changeModeUser = address;
			break;

			// Tru64/Digital UNIX specific entry points
		case 30: // UNIX_SYSTEM_CALL entry point
			DEBUG_LOG("AlphaCPU: Setting UNIX_SYSTEM_CALL entry point");
			m_systemEntryPoints.unixSystemCall = address;
			break;

		case 31: // UNIX_USER_SIGNAL entry point
			DEBUG_LOG("AlphaCPU: Setting UNIX_USER_SIGNAL entry point");
			m_systemEntryPoints.unixUserSignal = address;
			break;

			// Windows NT specific entry points
		case 40: // WINDOWS_SYSTEM_SERVICE entry point
			DEBUG_LOG("AlphaCPU: Setting WINDOWS_SYSTEM_SERVICE entry point");
			m_systemEntryPoints.windowsSystemService = address;
			break;

		case 41: // WINDOWS_DISPATCH_EXCEPTION entry point
			DEBUG_LOG("AlphaCPU: Setting WINDOWS_DISPATCH_EXCEPTION entry point");
			m_systemEntryPoints.windowsDispatchException = address;
			break;

		default:
			// Custom entry points - stored in an array
			if (type >= 100 && type < 100 + MAX_CUSTOM_ENTRIES) {
				int index = type - 100;
				DEBUG_LOG(QString("AlphaCPU: Setting custom entry point %1").arg(index));
				m_systemEntryPoints.customEntries[index] = address;
			}
			else {
				DEBUG_LOG(QString("AlphaCPU: Unknown system entry type: %1").arg(type));
			}
			break;
		}

		// Update system control blocks if needed
		updateSystemControlBlocks();

		// If this is a critical system vector, need to update all CPUs in the system
		if (type < 5) {  // The most critical vectors
			notifySystemEntryPointChange(type, address);
		}
	}
	void writeUSP(quint64 value) {
		m_iprs.write(IprBank::USP, value);
	}

	void writeVAL(quint64 value) {
		// Implementation would depend on what VAL is used for
		// in your specific system
		DEBUG_LOG(QString("AlphaCPU: VAL register set to 0x%1")
			.arg(value, 16, 16, QChar('0')));
	}
	void writeVPTB(quint64 value) {
		m_iprs.write(IprBank::VPTB, value);
	}

#pragma endregion ExecuteStage Implementation


	/**
	 * @brief Map memory fault type to Alpha exception type
	 *
	 * @param faultType Memory fault type
	 * @return Corresponding Alpha exception type
	 */

	AsaExceptions::ExceptionType mapMemoryFaultToExceptionType(AsaExceptions::MemoryFaultType faultType)
	{
		switch (faultType) {
		case AsaExceptions::MemoryFaultType::PAGE_FAULT:
			return AsaExceptions::ExceptionType::PAGE_FAULT;
		case AsaExceptions::MemoryFaultType::ACCESS_VIOLATION:
			return AsaExceptions::ExceptionType::ACCESS_CONTROL_VIOLATION;
		case AsaExceptions::MemoryFaultType::ALIGNMENT_FAULT:
			return AsaExceptions::ExceptionType::ALIGNMENT_FAULT;
		case AsaExceptions::MemoryFaultType::PROTECTION_VIOLATION:
			return AsaExceptions::ExceptionType::ACCESS_CONTROL_VIOLATION;
		case AsaExceptions::MemoryFaultType::GENERAL_PROTECTION_FAULT:
			return AsaExceptions::ExceptionType::ILLEGAL_OPCODE;
		default:
			return AsaExceptions::ExceptionType::MACHINE_CHECK;
		}
	}

	// Floating-point multiplication for IEEE S format values
	quint64 mulSFormat(quint64 a, quint64 b) {
		// Convert to native single precision values
		float aValue = std::bit_cast<float>(static_cast<quint32>(a));
		float bValue = std::bit_cast<float>(static_cast<quint32>(b));

		// Perform multiplication
		float result;
		try {
			result = aValue * bValue;
		}
		catch (...) {
			// Handle computation errors
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
		}
		else if (std::isinf(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
		}

		// Convert result to double precision for storage
		double doubleResult = static_cast<double>(result);
		return std::bit_cast<quint64>(doubleResult);
	}
	// Floating-point multiplication for IEEE T format values
	quint64 mulTFormat(quint64 a, quint64 b) {
		// Convert raw bits to double precision values
		double aValue = std::bit_cast<double>(a);
		double bValue = std::bit_cast<double>(b);

		// Perform multiplication
		double result;
		try {
			result = aValue * bValue;
		}
		catch (...) {
			// Handle computation errors
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
			return std::bit_cast<quint64>(std::numeric_limits<double>::quiet_NaN());
		}

		// Check for floating-point exceptions
		if (std::isnan(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_InvalidOp();
			}
		}
		else if (std::isinf(result)) {
			if (m_registerBank && m_registerBank->fp()) {
				m_registerBank->fp()->raiseStatus_Overflow();
			}
		}

		// Return the bits
		return std::bit_cast<quint64>(result);
	}



	/**
	 * @brief Check if an exception frame is needed
	 */
	bool needsExceptionFrame()  {   // TODO
		// Exception frames are typically needed for:
		// - Page faults
		// - Access violations 
		// - Any exception that may need to be reflected to user mode
		return true;  // For simplicity, always create a frame
	}
	void notifyExecutionStopped() override;

	void notifyExecutionStopped()
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void notifyFpRegisterUpdated(unsigned idx, double value) override;

	void notifyFpRegisterUpdated(unsigned idx, double value)
	{
		m_registerBank->getFpBank()->writeFpReg(idx, value);
	}

	void notifyIllegalInstruction(quint64 instructionWord, quint64 pc) override;

	void notifyIllegalInstruction(quint64 instr, quint64 pc)
	{
		emit illegalInstruction(pc, instr);
	}

	void notifyMemoryAccessed(quint64 addr, quint64 value, bool isWrite) override;

	void notifyMemoryAccessed(quint64 addr, quint64 val, bool isWrite)
	{
		emit memoryAccessed(addr, val, isWrite);
	}

	void notifyRaiseException(AsaExceptions::ExceptionType except_, quint64 pc_) override
	{
		//TODO notifyRaiseException
	}
	void notifyRegisterUpdate(bool isFp, quint64 registerIndex, quint64 value);

	void notifyRegisterUpdate(bool isFp, quint64 registerIndex, quint64 value)
	{
		if (isFp) {
			// Write to FP register
			if (registerIndex < 31) {
				m_registerBank->getFpBank()->writeFpReg(static_cast<quint8>(registerIndex), *reinterpret_cast<double*>(&value));
			}
			else if (registerIndex == 31) {
				FpcrRegister fpcr = FpcrRegister::fromRaw(value);
				m_registerBank->getFpBank()->writeFpcr(fpcr);
				emit sigFpcrChanged(fpcr.raw);
			}

			DEBUG_LOG(QString("[AlphaCPU] FP Register R %1 updated to 0x%2").arg(registerIndex).arg(QString::number(value, 16)));
		}
		else {
			// Write to Integer register
			if (registerIndex < 31) {
				m_registerBank->writeIntReg(static_cast<quint8>(registerIndex), value);
			}

			DEBUG_LOG(QString("[AlphaCPU] Integer Register R%1 updated to 0x%2").arg(registerIndex).arg(QString::number(value, 16)));

			emit sigRegisterUpdated(static_cast<int>(registerIndex), isFp ? helpers_JIT::RegisterType::FLOAT_REG : helpers_JIT::RegisterType::INTEGER_REG, value);
		}
	}

	void writeIPR(int iprNumber, quint64 value)
	{
		switch (iprNumber) {
		case static_cast<int>(AsaIpr::IPRNumbers::IPR_EXC_ADDR):
			m_exceptionAddress = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_EXC_SUM):
			m_exceptionSummary = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_EXC_MASK):
			m_exceptionMask = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_PAL_BASE):
			m_palBaseAddress = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_PS):
			m_processorStatus = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_FEN):
			m_fpEnable = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_IPIR):
			m_ipInterruptRequest = value;
			// Check if new interrupts need immediate handling
			checkPendingInterrupts();
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_IPIR_PRIORITY):
			m_ipInterruptPriority = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_ASN):
			m_memorySystem->getTlbSystem()->setCurrentASN(value);
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_ASTSR):
			m_astStatus = value;
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_ASTEN):
			m_astEnable = value;
			// Check if enabling ASTs results in pending ASTs
			checkPendingAst();
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_SIRR):
			m_softwareInterruptRequest = value;
			// Check if new software interrupts need handling
			checkPendingInterrupts();
			break;

		case static_cast<int>(AsaIpr::IPRNumbers::IPR_IPLR):
			m_interruptPriorityLevel = value;
			// Check if lowering IPL enables pending interrupts
			checkPendingInterrupts();
			break;

		default:
		}
	}

	void notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t value) override;

	void notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t val)
	{
		emit sigRegisterUpdated(idx, isFp ? helpers_JIT::RegisterType::FLOAT_REG : helpers_JIT::RegisterType::INTEGER_REG, val);
	}

	void notifyReturnFromTrap() override;

	void notifyReturnFromTrap()
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void notifySetKernelSP(quint64 gpVal_) override
	{
		// R30 is the stack pointer (SP) in kernel mode
		m_registerBank->writeIntReg(30, gpVal_);
		emit sigRegisterUpdated(30, helpers_JIT::RegisterType::INTEGER_REG, gpVal_);
		qInfo() << "[AlphaCPU] Kernel Stack Pointer (R30) set to:" << QString("0x%1").arg(gpVal_, 0, 16);
	}
	void notifySetRunning(bool bIsRunning /*= false*/) override
	{
		m_isRunning = bIsRunning;
	} 
	void notifySetState(AsaTypesAndStates::CPUState state_) ;

	void notifySetState(AsaTypesAndStates::CPUState state_)
	{
		m_cpuState = state_;
	}

	void notifySetUserSP(quint64 usp_) override;

	void notifySetUserSP(quint64 usp_)
	{
		constexpr int R30 = 30; // Stack Pointer (SP) in Alpha is R30

		// Set SP register (R30) to new value
		m_registerBank->writeIntReg(R30, usp_);

		// Optionally emit signal for observers
		emit userStackPointerChanged(usp_);

		qInfo() << "[AlphaCPU] User SP (R30) updated to" << QString("0x%1").arg(usp_, 0, 16);
	}
	void notifyStateChanged(AsaTypesAndStates::CPUState newState_) override;
	void notifyStateChanged(AsaTypesAndStates::CPUState newState_)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}
	void notifyTrapRaised(quint64 type) override;
	void notifyTrapRaised(quint64 type)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}
	//void		raiseTrap(helpers_JIT::Fault_TrapType trapType);
   void raiseTrap(int trapCode) override;
   double readFpReg(unsigned idx) { return m_registerBank->getFpBank()->readFpReg(idx); }
   void writeFpReg(unsigned idx, double val) { m_registerBank->getFpBank()->writeFpReg(idx, val); }
   bool readMemory(uint64_t addr, void* buf, size_t size) { return m_memorySystem->readBlock(addr, buf, size); }
   bool writeMemory(uint64_t addr, void* buf, size_t size) { return m_memorySystem->writeBlock(addr, buf, size); }
/// Trap  handling needs to execute here
   void raiseTrap(int trapCode)
{
	emit trapOccurred(static_cast<helpers_JIT::Fault_TrapType>(trapCode), m_pc, m_cpuId);
}
   double readFpReg(unsigned idx) override;
   uint64_t	readIntReg(unsigned idx) override;
    quint64 readIPR(AsaIpr::IPRNumbers iprNumber)
	{
		switch (iprNumber) {
		case AsaIpr::IPRNumbers::IPR_EXC_ADDR:
			return m_exceptionAddress;

		case AsaIpr::IPRNumbers::IPR_EXC_SUM:
			return m_exceptionSummary;

		case AsaIpr::IPRNumbers::IPR_EXC_MASK:
			return m_exceptionMask;

		case AsaIpr::IPRNumbers::IPR_PAL_BASE:
			return m_palBaseAddress;

		case AsaIpr::IPRNumbers::IPR_PS:
			return m_processorStatus;

		case AsaIpr::IPRNumbers::IPR_FEN:
			return m_fpEnable;

		case AsaIpr::IPRNumbers::IPR_IPIR:
			return m_ipInterruptRequest;

		case AsaIpr::IPRNumbers::IPR_IPIR_PRIORITY:
			return m_ipInterruptPriority;

		case AsaIpr::IPRNumbers::IPR_ASN:
			return m_memorySystem->getTlbSystem()->getCurrentASN();

		case (AsaIpr::IPRNumbers::IPR_ASTSR:
			return m_astStatus;

		case AsaIpr::IPRNumbers::IPR_ASTEN:
			return m_astEnable;

		case AsaIpr::IPRNumbers::IPR_SIRR:
			return m_softwareInterruptRequest;

		case AsaIpr::IPRNumbers::IPR_IPLR:
			return m_interruptPriorityLevel;

		default:
			// Log invalid register access
			TraceManager::instance().warn(QString("Invalid IPR read: %1").arg(iprNumber));
			return 0;
		}
	}
	bool readMemory(uint64_t addr, void* buf, size_t size) override;
	quint64 readRegister(quint8 index) const
	{
		return m_registerBank->readIntReg(index);
	}
	/**
	 * @brief Check if I-cache invalidation is required
	 */
	bool requiresICacheInvalidation()  {
		// I-cache invalidation may be needed for:
		// - Page faults that involve instruction pages
		// - Self-modifying code scenarios
		// - TLB-related faults
		return m_faultType == AsaExceptions::MemoryFaultType::PAGE_FAULT;
	}
	void reset();
	QString resolveSymbol(quint64 address) {
		// This would typically use a symbol table provided by the OS/debugger
		// For the emulator, we could maintain a map of known symbols

		// Example implementation - check a map of known addresses
		if (m_symbolTable.contains(address)) {
			return m_symbolTable[address];
		}

		// Check if address falls within any known modules
		for (const auto& module : m_moduleTable) {
			if (address >= module.baseAddress && address < (module.baseAddress + module.size)) {
				quint64 offset = address - module.baseAddress;
				return QString("%1+0x%2").arg(module.name).arg(offset, 0, 16);
			}
		}

		return ""; // No symbol found
	}
	/**
	 * @brief Restore CPU features after exception handling
	 *
	 * Re-enables CPU features that were disabled during exception processing
	 * such as speculative execution, branch prediction, and prefetching.
	 */
	void restoreCpuFeatures() {
		// Re-enable features that were disabled during exception handling
		m_speculativeExecutionEnabled = true;
		m_branchPredictionEnabled = true;
		m_prefetchingEnabled = true;
		m_memoryOrderingStrict = false;  // Return to relaxed ordering

		// Re-enable any other performance features
		m_outOfOrderExecution = true;
		m_superscalarDispatch = true;

		DEBUG_LOG("CPU performance features restored");
	}
	/**
	 * @brief Restore stack pointer after exception handling
	 *
	 * This method restores the appropriate stack pointer based on the
	 * current processor mode after returning from an exception.
	 */
	void restoreStackPointer() {
		switch (m_currentMode) {
		case ProcessorMode::USER:
			m_currentStackPointer = m_iprs.read(IprBank::USP);
			break;
		case ProcessorMode::SUPERVISOR:
			m_currentStackPointer = m_iprs.read(IprBank::SSP);
			break;
		case ProcessorMode::KERNEL:
			m_currentStackPointer = m_iprs.read(IprBank::KSP);
			break;
		case ProcessorMode::PAL:
			// PAL uses kernel stack
			m_currentStackPointer = m_iprs.read(IprBank::KSP);
			break;
		}

		DEBUG_LOG(QString("Stack pointer restored: mode=%1, SP=0x%2")
			.arg(static_cast<int>(m_currentMode))
			.arg(m_currentStackPointer, 16, 16, QChar('0')));
	}
	/**
	 * @brief Save current processor state before entering exception handler
	 */
	void saveProcessorState()
	{
	}
	/**
	* @brief Set CPU exception state from memory fault information
	*
	* This function updates the CPU's internal state to reflect an exception
	* that has occurred during instruction execution. It sets up the necessary
	* registers and state so that exception handling can proceed correctly.
	*
	* @param faultInfo Information about the memory fault that occurred
	*/
	void setExceptionState(const MemoryFaultInfo& faultInfo)
	{
		// Save the current processor status
		// In Alpha, this typically involves setting up exception registers

		// 1. Save the faulting PC (Program Counter)
		// This is the PC of the instruction that caused the fault
		m_exceptionPc = faultInfo.pc;

		// 2. Save the faulting virtual address
		// This is stored in a special register for page fault handling
		m_faultingVirtualAddress = faultInfo.faultAddress;

		// 3. Set up exception cause information
		// Encode the exception type and additional details
		m_exceptionCause = static_cast<quint32>(faultInfo.faultType);

		// Add additional fault information to the cause register
		if (faultInfo.isWrite) {
			m_exceptionCause |= 0x1;  // Bit 0: Write fault
		}

		// Encode access size in bits 3-2
		switch (faultInfo.accessSize) {
		case 1: m_exceptionCause |= (0x0 << 2); break;  // Byte
		case 2: m_exceptionCause |= (0x1 << 2); break;  // Word
		case 4: m_exceptionCause |= (0x2 << 2); break;  // Longword
		case 8: m_exceptionCause |= (0x3 << 2); break;  // Quadword
		}

		// 4. Save the faulting instruction word
		m_faultingInstruction = faultInfo.instruction;

		// 5. Set exception flags
		m_hasException = true;
		m_exceptionPending = true;

		// 6. Update processor status register (PSR) or similar
		// Mark that we're entering exception mode
		saveProcessorState();

		// 7. For Alpha-specific handling, set up MM_STAT register equivalent
		// This register contains detailed information about memory management faults
		setupMemoryManagementStatus(faultInfo);

		// 8. Set up exception priority and type
		m_currentExceptionType = mapMemoryFaultToExceptionType(faultInfo.faultType);
		m_exceptionPriority = getExceptionPriority(m_currentExceptionType);

		DEBUG_LOG(QString("Exception state set: PC=0x%1, VA=0x%2, Type=%3, %4")
			.arg(m_exceptionPc, 16, 16, QChar('0'))
			.arg(m_faultingVirtualAddress, 16, 16, QChar('0'))
			.arg(static_cast<int>(faultInfo.faultType))
			.arg(faultInfo.isWrite ? "Write" : "Read"));
	}
	/**
	* @brief Set the halted state without performing halt operations
	* This might be what you actually want if the halt happened externally
	*/
	void setHalted(bool halted = true) {
		m_halted = halted;
		if (halted) {
			m_running = false;
		}
	}
	/**
	 * @brief Set the kernel stack pointer
	 * @param sp New kernel stack pointer value
	 */
	quint64 getKernelStackPointer() {
		return m_iprs.read(IprBank::KSP);
	}
	void setKernelStackPointer(quint64 sp) {
		m_iprs.write(IprBank::KSP, sp);

		// If we're currently in kernel mode, update SP register directly
		if (m_currentMode == ProcessorMode::KERNEL) {
			m_registerBank->writeIntReg(30, sp);  // R30 is SP in Alpha
		}
	}
	/**
	 * @brief Set CPU ID
	 * @param id CPU identifier
	 */
	void setCpuId(int id_) {
		m_cpuId = id_;
	}
	/**
 * @brief Set up exception state for the given exception type
 *
 * @param exception The exception type
 * @param level Additional exception data (e.g., interrupt level)
 */
	void setupExceptionState(AsaExceptions::ExceptionCause exception, int level) {
		// Clear any pending exception state
		clearExceptionState();

		// Set current exception type
		m_currentExceptionType = mapExceptionToType(exception);
		m_exceptionLevel = level;

		// Update processor status for exception handling
		m_savedProcessorStatus = m_processorStatus;

		// Disable interrupts and set kernel mode
		m_processorStatus &= ~PS_INTERRUPT_ENABLE;
		m_processorStatus &= ~PS_USER_MODE;
		m_processorStatus |= PS_KERNEL_MODE;

		// Set exception mode flag
		m_processorStatus |= PS_EXCEPTION_MODE;

		// Save current PC as exception PC
		m_exceptionPc = m_pc;

		// Set up exception cause register
		switch (exception) {
		case AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_AST:
			m_exceptionCause = AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_AST | (level << 8);
			break;
		case AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_INTERRUPT:
			m_exceptionCause = AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_INTERRUPT | (level << 8);
			break;
		case AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_MACHINE_CHECK:
			m_exceptionCause = AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_MACHINE_CHECK;
			break;
		case AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_ALIGNMENT:
			m_exceptionCause = AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_ALIGNMENT;
			break;
		case AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_ILLEGAL_INSTR:
			m_exceptionCause = AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_ILLEGAL_INSTR;
			break;
		default:
			m_exceptionCause = AsaExceptions::ExceptionCause::EXCEPTION_CAUSE_UNKNOWN;
			break;
		}

		// Mark that we're processing an exception
		m_inExceptionHandler = true;
		m_exceptionPending = true;
	}
	/**
     * @brief Clear all floating-point status flags using RegisterFileWrapper
     */
	void clearFloatingPointStatus()
	{
		if (!m_registerBank.data()) {
			return;
		}

		m_registerBank->fp().clearStatusFlags();
		DEBUG_LOG("AlphaCPU: Cleared all FP status flags");
	}
	/**
	 * @brief Set all floating-point trap enables using RegisterFileWrapper
	 * @param enable True to enable all traps, false to disable
	 */
	void setAllFloatingPointTraps(bool enable)
	{
		if (!m_registerBank.data()) {
			return;
		}

		FpcrRegister& fpcr = m_registerBank->fp();
		fpcr.setTrapEnabled_InvalidOp(enable);
		fpcr.setTrapEnabled_DivZero(enable);
		fpcr.setTrapEnabled_Overflow(enable);
		fpcr.setTrapEnabled_Underflow(enable);
		fpcr.setTrapEnabled_Inexact(enable);

		DEBUG_LOG(QString("AlphaCPU: %1 all FP traps").arg(enable ? "Enabled" : "Disabled"));
	}
	/**
	 * @brief Get floating-point quiet NaN
	 * @return IEEE quiet NaN value
	 */
	double getFloatingPointQuietNaN() const
	{
		return std::numeric_limits<double>::quiet_NaN();
	}
	/**
	 * @brief Get floating-point register value as double
	 * @param regNum Register number (0-31, F31 is hardwired to 0.0)
	 * @return Register value as double
	 */
	double getFloatRegister(quint8 regNum) const
	{
		if (!m_registerBank.data() || regNum >= 32) {
			DEBUG_LOG(QString("AlphaCPU: Invalid FP register access F%1").arg(regNum));
			return 0.0;
		}

		// F31 is hardwired to +0.0 in Alpha architecture
		if (regNum == 31) {
			return 0.0;
		}
	}
	/**
	 * @brief Get floating-point register raw 64-bit value
	 * @param regNum Register number
	 * @return Raw 64-bit register value
	 */
	quint64 getFloatRegister64(quint8 regNum) const
	{
		if (!m_registerBank.data() || regNum >= 32 || regNum == 31) {
			return 0;
		}

		return m_registerBank->fp().raw[regNum];
	}
	/**
	 * @brief Get floating-point register value as float (32-bit)
	 * @param regNum Register number
	 * @return Register value as single-precision float
	 */
	quint32 getFloatRegister32(quint8 regNum) const
	{
		if (!m_registerBank.data() || regNum >= 32 || regNum == 31) {
			return 0.0f;
		}

		// Extract lower 32 bits and interpret as float
		quint32 bits = static_cast<quint32>(m_registerBank->fp().raw[regNum] & 0xFFFFFFFF);
		return std::bit_cast<float>(bits);
	}
	/**
	 * @brief Set floating-point register from double value
	 * @param regNum Register number (0-31)
	 * @param value Double value to store
	 */
	void setFloatRegister(quint8 regNum, double value)
	{
		if (!m_registerBank || regNum >= 32) {
			DEBUG_LOG(QString("AlphaCPU: Invalid FP register write F%1").arg(regNum));
			return;
		}

		// F31 is hardwired to zero - ignore writes
		if (regNum == 31) {
			DEBUG_LOG("AlphaCPU: Attempted write to F31 (hardwired zero), ignoring");
			return;
		}

		// Use the register file wrapper's double view for clean storage
		m_registerBank->fp().asDouble[regNum] = value;

		DEBUG_LOG(QString("AlphaCPU: F%1 = %2 (0x%3)")
			.arg(regNum)
			.arg(value, 0, 'g', 17)
			.arg(m_registerBank->fp().raw[regNum], 16, 16, QChar('0')));
	}
	/**
	 * @brief Set floating-point register from float value
	 * @param regNum Register number
	 * @param value Float value to store
	 */
	void setFloatRegister(quint8 regNum, float value)
	{
		if (!m_registerBank.data() || regNum >= 32 || regNum == 31) {
			return;
		}

		// Store as 32-bit float in lower bits, preserve upper bits
		quint32 bits = std::bit_cast<quint32>(value);
		m_registerBank->fp().raw[regNum] = (m_registerBank->fp().raw[regNum] & 0xFFFFFFFF00000000ULL) | bits;
	}
	/**
 * @brief Set floating-point exception flag using RegisterFileWrapper FPCR
 * @param exception Exception type to set
 */
	void setFloatingPointFlag(AsaExceptions::FPTrapType exception)
	{
		if (!m_registerBank.data()) {
			DEBUG_LOG("AlphaCPU: No register bank available for FP flag setting");
			return;
		}

		// Access FPCR through the register file wrapper
		FpcrRegister& fpcr = m_registerBank->fp();

		switch (exception) {
		case AsaExceptions::FPTrapType::FP_INVALID_OPERATION:
			fpcr.raiseStatus_InvalidOp();
			DEBUG_LOG("AlphaCPU: FP Invalid Operation flag set");
			break;

		case AsaExceptions::FPTrapType::FP_DIVISION_BY_ZERO:
			fpcr.raiseStatus_DivZero();
			DEBUG_LOG("AlphaCPU: FP Division by Zero flag set");
			break;

		case AsaExceptions::FPTrapType::FP_OVERFLOW:
			fpcr.raiseStatus_Overflow();
			DEBUG_LOG("AlphaCPU: FP Overflow flag set");
			break;

		case AsaExceptions::FPTrapType::FP_UNDERFLOW:
			fpcr.raiseStatus_Underflow();
			DEBUG_LOG("AlphaCPU: FP Underflow flag set");
			break;

		case AsaExceptions::FPTrapType::FP_INEXACT:
			fpcr.raiseStatus_Inexact();
			DEBUG_LOG("AlphaCPU: FP Inexact flag set");
			break;

		case AsaExceptions::FPTrapType::FP_ARITHMETIC_TRAP:
			// Handle as arithmetic trap
			triggerException(AsaExceptions::FPTrapType::ARITHMETIC_TRAP, getPC());
			return;

		default:
			DEBUG_LOG(QString("AlphaCPU: Unknown FP exception %1").arg(static_cast<int>(exception)));
			break;
		}
	}
	/**
	 * @brief Trigger floating-point exception using RegisterFileWrapper FPCR
	 * @param exception Exception type
	 */
	void triggerFloatingPointException(FPException exception)
	{
		if (!m_registerBank.data()) {
			DEBUG_LOG("AlphaCPU: No register bank available for FP exception");
			return;
		}

		// Set the status flag first
		setFloatingPointFlag(exception);

		// Check if this exception should trigger a trap using RegisterFileWrapper FPCR
		FpcrRegister& fpcr = m_registerBank->fp();
		bool shouldTrap = false;

		switch (exception) {
		case FPException::FP_INVALID_OPERATION:
			shouldTrap = fpcr.isTrapEnabled_InvalidOp();
			break;
		case FPException::FP_DIVISION_BY_ZERO:
			shouldTrap = fpcr.isTrapEnabled_DivZero();
			break;
		case FPException::FP_OVERFLOW:
			shouldTrap = fpcr.isTrapEnabled_Overflow();
			break;
		case FPException::FP_UNDERFLOW:
			shouldTrap = fpcr.isTrapEnabled_Underflow();
			break;
		case FPException::FP_INEXACT:
			shouldTrap = fpcr.isTrapEnabled_Inexact();
			break;
		default:
			shouldTrap = true;  // Unknown exceptions always trap
			break;
		}

		if (shouldTrap) {
			DEBUG_LOG(QString("AlphaCPU: FP Exception %1 triggered trap").arg(static_cast<int>(exception)));
			triggerException(FLOATING_POINT_EXCEPTION, getPC());
		}
	}
	/**
	 * @brief Check for pending floating-point exceptions using RegisterFileWrapper
	 * @return True if any exceptions are pending
	 */
	bool checkFloatingPointExceptions() const
	{
		if (!m_registerBank.data()) {
			return false;
		}

		// Check any status flags are set using RegisterFileWrapper FPCR
		const FpcrRegister& fpcr = m_registerBank->fp();
		return fpcr.status_InvalidOp() ||
			fpcr.status_DivZero() ||
			fpcr.status_Overflow() ||
			fpcr.status_Underflow() ||
			fpcr.status_Inexact();
	}
	/**
 * @brief Get current rounding mode from RegisterFileWrapper FPCR
 * @return Current rounding mode
 */
	RoundingMode getCurrentRoundingMode()
	{
		if (!m_registerBank.data()) {
			return RoundingMode::ROUND_NEAREST_EVEN;
		}

		// Extract rounding mode from FPCR (bits 1:0)
		const FpcrRegister& fpcr = m_registerBank->fp();
		quint8 roundBits = fpcr.raw & 0x3;
		return static_cast<RoundingMode>(roundBits);
	}
	/**
	 * @brief Get FPCR register value using RegisterFileWrapper
	 * @return Raw FPCR value
	 */
	quint64 getFPCR() const
	{
		if (!m_registerBank.data()) {
			return 0;
		}
		return m_registerBank->fp().fpcr.toRaw();
	}
	/**
	 * @brief Set FPCR register value using RegisterFileWrapper
	 * @param value New FPCR value
	 */
	void setFPCR(quint64 value)
	{
		if (!m_registerBank.data()) {
			return;
		}

		m_registerBank->fp().fpcr = FpcrRegister::fromRaw(value);

		DEBUG_LOG(QString("AlphaCPU: Set FPCR to 0x%1").arg(value, 16, 16, QChar('0')));
	}
	/**
	 * @brief Access registers by ABI name using RegisterFileWrapper
	 * @param alias ABI alias for register
	 * @return Register value
	 */
	double getFloatRegisterByAlias(FAlias alias) const
	{
		return getFloatRegister(static_cast<quint8>(alias));
	}
	/**
	 * @brief Set register by ABI name using RegisterFileWrapper
	 * @param alias ABI alias for register
	 * @param value Value to set
	 */
	void setFloatRegisterByAlias(FAlias alias, double value)
	{
		setFloatRegister(static_cast<quint8>(alias), value);
	}
	void setRegister(quint8 reg, quint64 value);
	// Memory access
	bool readMemory8(quint64 address, quint8& value);
	bool readMemory16(quint64 address, quint16& value);
	bool readMemory32(quint64 address, quint32& value);
	bool readMemory64(quint64 address, quint64& value);
	bool writeMemory8(quint64 address, quint8 value);
	bool writeMemory16(quint64 address, quint16 value);
	bool writeMemory32(quint64 address, quint32 value);
	bool writeMemory64(quint64 address, quint64 value);

	// Locked memory operations
	bool readMemory32Locked(quint64 address, quint32& value);
	bool readMemory64Locked(quint64 address, quint64& value);
	bool writeMemory32Conditional(quint64 address, quint32 value);
	bool writeMemory64Conditional(quint64 address, quint64 value);
	/**
	 * @brief Switch processor mode
	 * TODO: Stack Processing During processor context mode changes
	 */
	void switchProcessorMode(ProcessorMode newMode) {
		ProcessorMode oldMode = m_currentMode;
		m_currentMode = newMode;

		// Save the current register R30 (SP) for the old mode
		quint64 currentSP = m_registerBank->readIntReg(30);

		// Save current SP into the appropriate IPR for the old mode
		switch (oldMode) {
		case ProcessorMode::USER:
			m_iprs.write(IprBank::USP, currentSP);
			break;
		case ProcessorMode::SUPERVISOR:
			m_iprs.write(IprBank::SSP, currentSP);
			break;
		case ProcessorMode::KERNEL:
			m_iprs.write(IprBank::KSP, currentSP);
			break;
		case ProcessorMode::PAL:
			// PAL uses kernel stack
			m_iprs.write(IprBank::KSP, currentSP);
			break;
		}

		// Load the appropriate SP from IPR for the new mode
		quint64 newSP = 0;
		switch (newMode) {
		case ProcessorMode::USER:
			newSP = m_iprs.read(IprBank::USP);
			break;
		case ProcessorMode::SUPERVISOR:
			newSP = m_iprs.read(IprBank::SSP);
			break;
		case ProcessorMode::KERNEL:
			newSP = m_iprs.read(IprBank::KSP);
			break;
		case ProcessorMode::PAL:
			// PAL uses kernel stack
			newSP = m_iprs.read(IprBank::KSP);
			break;
		}

		// Update the R30 register with the new SP
		m_registerBank->writeIntReg(30, newSP);

		// If we're switching to/from PAL mode, additional context might need to be saved/restored
		// For example, we might need to preserve more registers or update the StackManager
		if (oldMode == ProcessorMode::PAL || newMode == ProcessorMode::PAL) {
			// If needed, save or restore additional PAL context using StackManager
			// For example, if entering PAL mode, we might need to create a new frame
			if (newMode == ProcessorMode::PAL && oldMode != ProcessorMode::PAL) {
				// When entering PAL mode, we might want to save a minimal frame
				// This depends on your specific PAL implementation requirements
				ExceptionFrame palFrame;
				palFrame.pc = m_pc;
				palFrame.ps = m_iprs.read(IprBank::PS);
				// We'd populate other fields as needed

				// This is optional and depends on your PAL implementation
				// m_stackManager->push(palFrame);
			}
		}

		DEBUG_LOG(QString("Mode switch: %1 -> %2, SP=0x%3")
			.arg(static_cast<int>(oldMode))
			.arg(static_cast<int>(newMode))
			.arg(newSP, 16, 16, QChar('0')));
	}
	
	/**
	 * @brief Set up memory management status register information
	 *
	 * @param faultInfo Memory fault information to encode
	 */
	void setupMemoryManagementStatus(const MemoryFaultInfo& faultInfo)
	{
		m_memoryManagementStatus = 0;

		// Set fault type
		switch (faultInfo.faultType) {
		case MemoryFaultType::PAGE_FAULT:
			m_memoryManagementStatus |= 0x1;  // Page not present
			break;
		case MemoryFaultType::ACCESS_VIOLATION:
			m_memoryManagementStatus |= 0x2;  // Access violation
			break;
		case MemoryFaultType::ALIGNMENT_FAULT:
			m_memoryManagementStatus |= 0x4;  // Alignment fault
			break;
		case MemoryFaultType::GENERAL_PROTECTION_FAULT:
			m_memoryManagementStatus |= 0x8;  // General protection fault
			break;
		default:
			break;
		}

		// Set read/write flag
		if (faultInfo.isWrite) {
			m_memoryManagementStatus |= 0x10;  // Write fault
		}

		// Set user/supervisor flag
		if (m_savedProcessorMode == ProcessorMode::USER) {
			m_memoryManagementStatus |= 0x20;  // User mode fault
		}
	}
	void setMMUEnabled(bool bEnabled) {
		//TODO setMMUEnabled(bool bEnabled
	}
	/**
	 * @brief Set processor status register
	 * @param status New processor status value
	 */
	void setProcessorStatus(quint64 status) {
		m_iprs.write(Ipr::PS, status);
		// Note: IprBank will call updateProcessorStatus() which handles the logic
	}

	bool supportsIPRAccess() const { return true; }  // Flag for IPR support
	void takeProbeSample() {
		quint64 currentPC = getPC();
		m_profileSampleCount++;

		// Find if we already have an entry for this PC
		bool found = false;
		for (int i = 0; i < PROFILE_BUFFER_SIZE; i++) {
			if (m_profileBuffer[i].pc == currentPC) {
				m_profileBuffer[i].count++;
				found = true;
				break;
			}

			// If we find an empty slot, we can use it for a new entry
			if (m_profileBuffer[i].pc == 0) {
				m_profileBuffer[i].pc = currentPC;
				m_profileBuffer[i].count = 1;
				found = true;
				break;
			}
		}

		// If buffer is full and we didn't find a match, replace least frequent entry
		if (!found) {
			int minCount = INT_MAX;
			int minIndex = 0;

			for (int i = 0; i < PROFILE_BUFFER_SIZE; i++) {
				if (m_profileBuffer[i].count < minCount) {
					minCount = m_profileBuffer[i].count;
					minIndex = i;
				}
			}

			// Replace the least frequent entry
			m_profileBuffer[minIndex].pc = currentPC;
			m_profileBuffer[minIndex].count = 1;
		}
	}

	void triggerPerfMonInterrupt(quint32 vector) {
		DEBUG_LOG(QString("AlphaCPU: Triggering performance monitor interrupt, vector=0x%1")
			.arg(vector, 2, 16, QChar('0')));

		// Save current PC and other state
		quint64 savedPC = getPC();

		// Jump to interrupt vector
		quint64 scbb = m_iprs.read(Ipr::SCBB);
		quint64 vectorAddr = scbb + (vector * 0x80);  // 0x80 bytes per vector
		setPC(vectorAddr);



		// Save processor state for interrupt return
		m_exceptionReturnAddress = savedPC;
		m_savedProcessorStatus = getProcessorStatus();

		// Trigger the exception based on CPU model
		switch (m_cpuModel) {
		case CpuModel::CPU_EV4:
		case CpuModel::CPU_EV5:
			// Older models use simple vectoring
			break;

		case CpuModel::CPU_EV6:
		case CpuModel::CPU_EV7:
			// Newer models save additional state
			m_iprs->write(IPRNumbers::IPR_EXC_ADDR, savedPC);
			m_iprs->write(IPRNumbers::IPR_EXC_SUM, 0x1000 | vector); // Mark as perf mon interrupt

			// Save specific counter info in another register
			quint64 perfInfo = (m_perfCounters[vector & 0xF].eventType << 16) |
				(m_perfCounters[vector & 0xF].value & 0xFFFF);
			m_iprs->write(IPRNumbers::IPR_MASK, perfInfo);
			break;
		}
	}
	void updateBlockStatistics(quint64 startAddr) {}
	/**
 * @brief Update interrupt priority level
 *
 * Called when the IPL register is written. Updates the current interrupt
 * priority level and checks for pending interrupts that can now be delivered.
 *
 * @param level New interrupt priority level (0-31)
 */
	void updateInterruptPriority(quint64 level) {
		quint64 oldIpl = m_currentIpl;
		m_currentIpl = level & 0x1F;  // Only 5 bits used

		DEBUG_LOG(QString("IPL updated: %1 -> %2").arg(oldIpl).arg(m_currentIpl));

		// If IPL decreased, check for pending interrupts
		if (m_currentIpl < oldIpl) {
			checkSoftwareInterrupts();
			checkHardwareInterrupts();
		}
	}
	void updateMonitoringState() {
		bool anyCounterEnabled = false;

		for (int i = 0; i < MAX_PERF_COUNTERS; i++) {
			if (m_perfCounters[i].enabled) {
				anyCounterEnabled = true;
				break;
			}
		}

		m_performanceMonitoringActive = anyCounterEnabled || m_profilingActive;

		DEBUG_LOG(QString("AlphaCPU: Performance monitoring %1")
			.arg(m_performanceMonitoringActive ? "active" : "inactive"));
	}
	void updatePerformanceCounters(quint32 completedInstructionType) {
		if (!m_performanceMonitoringActive) {
			return;
		}

		// Get current mode
		bool isKernelMode = (m_currentMode == ProcessorMode::KERNEL);
		bool isUserMode = (m_currentMode == ProcessorMode::USER);
		bool isSupervisorMode = (m_currentMode == ProcessorMode::SUPERVISOR);
		bool isPalMode = inPALMode();

		// Update counters based on their configuration
		for (int i = 0; i < MAX_PERF_COUNTERS; i++) {
			if (!m_perfCounters[i].enabled) {
				continue;
			}

			// Check if this counter should count in the current mode
			bool shouldCount = false;

			if (isKernelMode && m_perfCounters[i].countInKernelMode) shouldCount = true;
			if (isUserMode && m_perfCounters[i].countInUserMode) shouldCount = true;
			if (isSupervisorMode && m_perfCounters[i].countInSupervisorMode) shouldCount = true;
			if (isPalMode && m_perfCounters[i].countPalMode) shouldCount = true;

			// Apply inversion if configured
			if (m_perfCounters[i].invertMode) {
				shouldCount = !shouldCount;
			}

			// Apply filters if enabled
			if (shouldCount && m_monitoringFilters.addrRangeEnabled) {
				quint64 pc = getPC();
				if (pc < m_monitoringFilters.addrRangeStart || pc > m_monitoringFilters.addrRangeEnd) {
					shouldCount = false;
				}
			}

			if (shouldCount && m_monitoringFilters.instructionTypeEnabled) {
				if (completedInstructionType != m_monitoringFilters.instructionType) {
					shouldCount = false;
				}
			}

			// If all checks pass, increment counter
			if (shouldCount) {
				bool eventOccurred = false;

				// Check if the event we're monitoring has occurred
				switch (m_perfCounters[i].eventType) {
				case 0x0001: // Cycles
					eventOccurred = true; // Always count cycles
					break;

				case 0x0002: // Instructions retired
					eventOccurred = true; // We're called after each completed instruction
					break;

				case 0x0003: // Memory references
					// Check if instruction accessed memory
					eventOccurred = isMemoryInstruction(completedInstructionType);
					break;

				case 0x0004: // D-cache misses
					// This would be set by cache system
					eventOccurred = m_lastInstructionDCacheMiss;
					break;

				case 0x0005: // I-cache misses
					// This would be set by cache system
					eventOccurred = m_lastInstructionICacheMiss;
					break;

					// Further event types would be handled similarly
				}

				// If event occurred, increment counter
				if (eventOccurred) {
					m_perfCounters[i].value++;

					// Check for overflow
					if (m_perfCounters[i].value >= m_perfCounters[i].overflowThreshold) {
						handleCounterOverflow(i);
					}
				}
			}
		}

		// Handle profiling if active
		if (m_profilingActive && m_cycleCounter >= m_profileNextSample) {
			takeProbeSample();
			m_profileNextSample = m_cycleCounter + m_profilingSamplingRate;
		}
	}
	/**
	 * @brief Update processor status register for exception handling
	 */
	void updateProcessorStatusForException() {
		// Save current PS
		m_savedProcessorStatus = getProcessorStatus();

		// Clear user mode, set kernel mode
		m_processorStatus &= ~PS_USER_MODE;
		m_processorStatus |= PS_KERNEL_MODE;

		// Disable interrupts
		m_processorStatus &= ~PS_INTERRUPT_ENABLE;

		// Set exception mode flag
		m_processorStatus |= PS_EXCEPTION_MODE;

		// Clear certain traps that should not fire during exception handling
		m_processorStatus &= ~(PS_ARITHMETIC_TRAP_ENABLE | PS_FP_TRAP_ENABLE);
	}
	/**
	 * @brief Update processor status register
	 *
	 * Called when the PS register is written to handle mode changes,
	 * interrupt enable/disable, and other status updates.
	 *
	 * @param status New processor status value
	 */
	void updateProcessorStatus(quint64 status) {
		quint64 oldStatus = m_processorStatus;
		m_processorStatus = status;

		// Extract and handle mode change
		ProcessorMode newMode = static_cast<ProcessorMode>((status >> 3) & 0x3);
		if (newMode != m_currentMode) {
			switchProcessorMode(newMode);
		}

		// Handle interrupt enable/disable
		bool newInterruptEnable = (status & PS_INTERRUPT_ENABLE) != 0;
		if (newInterruptEnable != m_interruptEnable) {
			m_interruptEnable = newInterruptEnable;
			if (newInterruptEnable) {
				// Interrupts re-enabled - check for pending interrupts
				checkForPendingInterrupts();
			}
		}

		// Handle FP enable/disable
		bool newFpEnable = (status & PS_FP_ENABLE) != 0;
		if (newFpEnable != m_fpEnable) {
			m_fpEnable = newFpEnable;
		}

		DEBUG_LOG(QString("PS updated: 0x%1 -> 0x%2, mode=%3, IE=%4")
			.arg(oldStatus, 16, 16, QChar('0'))
			.arg(status, 16, 16, QChar('0'))
			.arg(static_cast<int>(m_currentMode))
			.arg(m_interruptEnable));
	}
	void writeFpReg(unsigned idx, double value) override;

	void writeIntReg(unsigned idx, uint64_t val) override { m_registerBank->writeInt(idx, val); }
	void writeIPR(int iprNumber, quint64 value);
	/**
	 * @brief Write to kernel memory
	 *
	 * This method performs a privileged memory write that bypasses normal
	 * protection checks, used primarily during exception handling.
	 *
	 * @param address Virtual address to write to
	 * @param value Value to write
	 */
	void writeKernelMemory(quint64 address, quint64 value) {
		try {
			if (!m_memorySystem) {
				ERROR_LOG("Memory system not available for kernel write");
				throw std::runtime_error("Memory system not available");
			}

			// Perform privileged write (bypasses normal protection checks)
			bool success = m_memorySystem->writeVirtualMemoryPrivileged(address, &value, sizeof(value));

			if (!success) {
				ERROR_LOG(QString("Kernel memory write failed: addr=0x%1, value=0x%2")
					.arg(address, 16, 16, QChar('0'))
					.arg(value, 16, 16, QChar('0')));
				throw std::runtime_error("Kernel memory write failed");
			}

			DEBUG_LOG(QString("Kernel write: addr=0x%1, value=0x%2")
				.arg(address, 16, 16, QChar('0'))
				.arg(value, 16, 16, QChar('0')));
		}
		catch (const std::exception& e) {
			ERROR_LOG(QString("Exception during kernel write: %1").arg(e.what()));
			// For kernel writes during exception handling, we can't throw
			// Just log the error and continue
		}
	}
	bool writeMemory(uint64_t addr, void* buf, size_t size) override;
	void writeRegister(unsigned idx, uint64_t value) override { m_registerBank->writeIntReg(idx, value); }




	quint64 getPC() const override;


	


	void writeIntReg(unsigned idx, uint64_t value) override;


	double readFpReg(unsigned idx) override;


	void writeFpReg(unsigned idx, double value) override;


	quint64 readRegister(quint8 index) const override;


	void writeRegister(unsigned idx, uint64_t value) override;


	bool readMemory(uint64_t addr, void* buf, size_t size) override;


	bool writeMemory(uint64_t addr, void* buf, size_t size) override;


	void raiseTrap(int trapCode) override;


	void notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t rawValue) override;


	void notifyTrapRaised(quint64 type) override;


	void notifyReturnFromTrap() override;


	void notifyStateChanged(AsaTypesAndStates::CPUState newState_) override;


	void notifyRaiseException(AsaExceptions::ExceptionType eType_, quint64 pc) override;


	void notifySetState(AsaTypesAndStates::CPUState state_) override;


	void notifySetRunning(bool bIsRunning = false) override;


	void notifySetKernelSP(quint64 gpVal) override;


	void notifySetUserSP(quint64 usp_) override;

signals:
	void sigCacheCoherencyEvent(quint64 physicalAddress, int cpuId, const QString& eventType); //represents a notification of a cache-related synchronization event (e.g., invalidate, flush, writeback) across CPUs.

	void sigCpuHalted(int cpuId);				// Qt signal for CPU halt notification
	/**
	* @brief Emitted when CPU state changes
	* @param newState The new CPU state
	*/
	void sigCpuStateChanged(CPUState newState);
	void sigCpuStatusUpdate(quint8 cpuid);
	
	
	void sigCycleExecuted(quint64 cycle);
	// Add to AlphaCPU.h in the public section
	void sigDeliverPendingInterrupt();
	/**
	 * @brief Emitted when an error occurs during execution stop
	 * @param errorMessage Description of the error
	 */
	void sigExecutionError(const QString& errorMessage);
	void sigExecutionPaused(quint16 cpuId);
	void sigExecutionStarted(quint16 cpuId);
	void sigExecutionStopped(quint16 cpuId);
	/**
	 * @brief Emitted when execution stops
	 * @param finalPC The final program counter value
	 * @param totalInstructions Total instructions executed
	 */
	//void sigExecutionStopped(quint64 finalPC, quint64 totalInstructions);

	void sigFpcrChanged(quint64 changedFprc);
	
	
	void sigHandleReset();
	void sigIllegalInstruction(quint64 pc, quint64 opcode);
	
	void sigMappingsCleared();
	void sigMemoryAccessed(quint64 address, quint64 value, bool isWrite);
	void sigOperationCompleted();
	void sigOperationStatus(const QString& message);
	void sigProcessingProgress(int percentComplete);
	void sigRegisterUpdated(int regNum, RegisterType type, quint64 value);

	void sigStateChanged();
	void sigTranslationMiss(quint64 virtualAddress);
	void sigTrapOccurred(Fault_TrapType type, quint64 pc, int cpuId);
	void sigTrapRaised(Fault_TrapType trap);

	

	/*
		int regNum — the register number (0–30 for integer, 0–30 for FP, etc.)
	helpers_JIT::RegisterType type — enum to distinguish Integer, FloatingPoint, Vector, etc.
	quint64 value — the new value of the register
	*/

	void sigUserStackPointerChanged(quint64 newSP);  // for UI/logging


private slots:

	void onSystemStarted() {
		qDebug() << "[AlphaCPU] System started.";
	}
	void onSystemStopped() {
		qDebug() << "[AlphaCPU] System stopped.";
	}
	void onMemoryAccessed(quint64 addr, quint64 value, bool isWrite) {
		qDebug() << "[AlphaCPU] Memory accessed:" << (isWrite ? "WRITE" : "READ") << "Addr:" << addr << "Value:" << value;
	}
	void onSystemPaused()
	{

	}
	void onSystemResumed() {
		qDebug() << "[AlphaCPU] System resumed.";
	}
	void deliverPendingInterrupt()
	{
	}
	void onDeliverPendingInterrupt()
	{
		// If no interrupts are pending, nothing to do
		if (pendingInterrupts.isEmpty()) {
			return;
		}

		// Get current IPL
		quint64 currentIPL = readIPR(static_cast<int>(IPRNumbers::IPR_IPLR));

		// Track highest priority pending interrupt
		int highestVector = -1;
		int highestPriority = -1;

		// Find the highest priority pending interrupt
		for (int vector : pendingInterrupts) {
			// Get priority of this interrupt
			int priority = 0;
			if (interruptPriorities.contains(vector)) {
				priority = interruptPriorities[vector];
			}
			else {
				// Extract from IPIR_PRIORITY register
				quint64 priorityValue = readIPR(static_cast<int>(IPRNumbers::IPR_IPIR_PRIORITY));
				priority = (priorityValue >> (vector * 4)) & 0xF;
			}

			// Check if this is higher priority than what we've found so far
			if (priority > highestPriority) {
				highestPriority = priority;
				highestVector = vector;
			}
		}

		// If we found a high-priority interrupt, process it
		if (highestVector >= 0 && highestPriority > currentIPL) {
			deliverInterrupt(highestVector, highestPriority);

			// Remove from pending set
			pendingInterrupts.remove(highestVector);
			interruptPriorities.remove(highestVector);

			// Check if we still have pending interrupts
			if (pendingInterrupts.isEmpty()) {
				interruptPending.storeRelaxed(false);
			}
		}
	}
	void onTrapOccurred(Fault_TrapType type, quint64 pc, int cpuId) {
		qDebug() << "[AlphaCPU] Trap occurred on CPU" << cpuId << "PC:" << pc << "TrapType:" << static_cast<int>(type);
	}
	//void onConfigureSystem();
	void onInterprocessorInterruptSent(int from, int to, int vector) {
		qDebug() << "[AlphaCPU] IPI from CPU" << from << "to CPU" << to << "vector:" << vector;
	}
	void onIllegalInstruction(quint64 instr, quint64 pc) {
		qWarning() << "[AlphaCPU] Illegal instruction 0x" << instr << " at PC:" << pc;
	}
	void onSignalStartAll() {
		qDebug() << "[AlphaCPU] Signal to start all CPUs.";
	}
	void onSignalStopAll() {
		qDebug() << "[AlphaCPU] Signal to stop all CPUs.";
	}
	void onSystemInitialized() {
		qDebug() << "[AlphaCPU] System initialized.";
	}
	void onSignalPauseAll() {
		qDebug() << "[AlphaCPU] Signal to pause all CPUs.";
	}
	void onSignalResumeAll() {
		qDebug() << "[AlphaCPU] Signal to resume all CPUs.";
	}
	void onSignalResetAll() {
		qDebug() << "[AlphaCPU] Signal to reset all CPUs.";
	}
	void onSignalSendInterrupt(int cpuId, quint64 vector) {
		qDebug() << "[AlphaCPU] Send interrupt to CPU" << cpuId << "vector:" << vector;
	}
	void onExecutionFinished()
	{
		//TODO executionFinished()
	}
	void onHandleInterrupt(int vector)
	{
		// Set the appropriate bit in the IPIR
		quint64 ipirValue = readIPR(static_cast<int>(IPRNumbers::IPR_IPIR));
		ipirValue |= (1ULL << vector);
		writeIPR(static_cast<int>(IPRNumbers::IPR_IPIR), ipirValue);

		// Record this pending interrupt
		pendingInterrupts.insert(vector);
		interruptPending.storeRelaxed(true);

		// Log the interrupt
		TraceManager::instance().debug(QString("CPU%1: Received interrupt vector %2")
			.arg(m_cpuId)
			.arg(vector));

		// Check if we can handle it immediately
		if (canTakeInterrupt(vector)) {
			deliverPendingInterrupt();
		}
	}
	void onHandleInterruptWithPriority(int vector, int priority)
	{
		// Set the appropriate bit in the IPIR
		quint64 ipirValue = readIPR(static_cast<int>(IPRNumbers::IPR_IPIR));
		ipirValue |= (1ULL << vector);
		writeIPR(static_cast<int>(IPRNumbers::IPR_IPIR), ipirValue);

		// Set the priority in the IPIR_PRIORITY register
		quint64 priorityValue = readIPR(static_cast<int>(IPRNumbers::IPR_IPIR_PRIORITY));
		// Clear existing priority bits for this vector (4 bits per vector)
		priorityValue &= ~(0xFULL << (vector * 4));
		// Set the new priority (value 0-15)
		priorityValue |= (static_cast<quint64>(priority & 0xF) << (vector * 4));
		writeIPR(static_cast<int>(IPRNumbers::IPR_IPIR_PRIORITY), priorityValue);

		// Record this pending interrupt with its priority
		pendingInterrupts.insert(vector);
		interruptPriorities[vector] = priority;
		interruptPending.storeRelaxed(true);

		// Log the interrupt
		TraceManager::instance().debug(QString("CPU%1: Received interrupt vector %2 with priority %3")
			.arg(m_cpuId)
			.arg(vector)
			.arg(priority));

		// Check if we can handle it immediately
		if (canTakeInterrupt(vector)) {
			deliverPendingInterrupt();
		}
	}

public slots:
	void onExecutionFinished() {}

	/**
     * @brief Handle a system interrupt
     *
     * This slot is triggered when an interrupt needs to be delivered to the CPU.
     * It sets up the appropriate interrupt state and triggers handling.
     *
     * @param vector The interrupt vector number
     */
	void onHandleInterrupt(int vector) {
		DEBUG_LOG(QString("CPU%1: Handling interrupt vector %2")
			.arg(m_cpuId)
			.arg(vector));

		// Set the appropriate bit in the IPIR register
		quint64 ipirValue = m_iprs.read(Ipr::IPIR);
		ipirValue |= (1ULL << vector);
		m_iprs.write(Ipr::IPIR, ipirValue);

		// Record this pending interrupt
		pendingInterrupts.insert(vector);
		interruptPending.storeRelaxed(true);

		// Default priority is medium (8)
		interruptPriorities[vector] = 8;

		// Log the interrupt
		DEBUG_LOG(QString("CPU%1: Registered interrupt vector %2 with priority %3")
			.arg(m_cpuId)
			.arg(vector)
			.arg(interruptPriorities[vector]));

		// Check if we can handle it immediately
		if (m_isRunning && (m_processorStatus & PS_INTERRUPT_ENABLE) && !m_inExceptionHandler) {
			if (canTakeInterrupt(vector)) {
				deliverPendingInterrupt();
			}
		}
	}
/**
 * @brief Handle a system interrupt with explicit priority
 *
 * This slot is triggered when an interrupt with a specific priority
 * needs to be delivered to the CPU.
 *
 * @param vector The interrupt vector number
 * @param priority The interrupt priority level (0-15)
 */
	void onHandleInterruptWithPriority(int vector, int priority) {
		DEBUG_LOG(QString("CPU%1: Handling interrupt vector %2 with priority %3")
			.arg(m_cpuId)
			.arg(vector)
			.arg(priority));

		// Set the appropriate bit in the IPIR register
		quint64 ipirValue = m_iprs.read(IprBank::IPIR);
		ipirValue |= (1ULL << vector);
		m_iprs.write(IprBank::IPIR, ipirValue);

		// Set the priority in the IPIR_PRIORITY register
		quint64 priorityValue = m_iprs.read(Ipr::IPIR_PRIORITY);
		// Clear existing priority bits for this vector (4 bits per vector)
		priorityValue &= ~(0xFULL << (vector * 4));
		// Set the new priority (value 0-15)
		priorityValue |= (static_cast<quint64>(priority & 0xF) << (vector * 4));
		m_iprs.write(Ipr::IPIR_PRIORITY, priorityValue);

		// Record this pending interrupt with its priority
		pendingInterrupts.insert(vector);
		interruptPriorities[vector] = priority;
		interruptPending.storeRelaxed(true);

		// Log the interrupt
		DEBUG_LOG(QString("CPU%1: Registered interrupt vector %2 with priority %3")
			.arg(m_cpuId)
			.arg(vector)
			.arg(priority));

		// Check if we can handle it immediately
		if (m_isRunning && (m_processorStatus & PS_INTERRUPT_ENABLE) && !m_inExceptionHandler) {
			if (canTakeInterrupt(vector)) {
				deliverPendingInterrupt();
			}
		}
	}
	void checkPendingInterrupts()
	{
		// Check if we have any pending interrupts
		if (!interruptPending.loadRelaxed()) {
			return;
		}

		// Check if interrupts are enabled and we're not already in an exception handler
		if (!m_inExceptionHandler && (m_processorStatus & PS_IE)) {
			deliverPendingInterrupt();
		}
	}
	void deliverInterrupt(int vector, int priority)
	{
		// Save current context
		quint64 savedPC = m_pc;
		quint64 savedPS = readIPR(static_cast<int>(IPRNumbers::IPR_PS));

		// Set new IPL to mask lower-priority interrupts
		writeIPR(static_cast<int>(IPRNumbers::IPR_IPLR), priority);

		// Flag that we're in exception handler
		m_inExceptionHandler = true;

		// Calculate the exception summary value
		quint64 excSum = (1ULL << vector); // Set bit corresponding to this vector

		// Build exception frame using the helper
		bool success = FrameHelpers::pushTrapFrame(
			*m_stackManager,     // Your StackManager instance
			savedPC,               // PC at exception
			savedPS,               // PS at exception
			excSum,                // Exception summary
			m_registerBank->getIntRegisterArray(), // Pointer to register array
			m_registerBank->fp() // FPCR value
		);

		if (!success) {
			// Handle stack overflow
			TraceManager::instance().error(QString("CPU%1: Exception frame stack overflow!").arg(m_cpuId));
			// Emergency handling (maybe reset CPU)
			return;
		}

		// Clear the interrupt bit
		quint64 ipirValue = readIPR(static_cast<int>(IPRNumbers::IPR_IPIR));
		ipirValue &= ~(1ULL << vector);
		writeIPR(static_cast<int>(IPRNumbers::IPR_IPIR), ipirValue);

		// Jump to PAL interrupt handler
		quint64 palBase = readIPR(static_cast<int>(IPRNumbers::IPR_PAL_BASE));
		m_pc = palBase + (vector * 0x80);  // Typically 0x80 bytes per vector

		// Log the interrupt delivery
		TraceManager::instance().debug(QString("CPU%1: Delivered interrupt vector %2 (priority %3), PC=0x%4")
			.arg(m_cpuId)
			.arg(vector)
			.arg(priority)
			.arg(m_pc, 0, 16));
	}
	void onAllCPUsStarted() {

	}
	void stop() {
		stopRequested.storeRelaxed(true);
		emit halted();
	}
	void onAllCPUsStopped() {

	}
	void onAllCPUsPaused() {

	}
	void onMappingsCleared() {
		qDebug() << "[AlphaCPU] Memory mappings cleared.";
	}
	void onMemoryWritten(quint64 address, quint64 value, int size)
	{

	}
	void onMemoryRead(quint64 address, quint64 value, int size)
	{

	}
	void onCacheCoherencyEvent(quint64 addr) {
		qDebug() << "[AlphaCPU] Cache coherency event at address:" << QString("0x%1").arg(addr, 0, 16);

		if (m_level2DataCache->contains(addr)) {
			m_level2DataCache->remove(addr); // Simulate invalidation
			qDebug() << "[AlphaCPU] Invalidated D-cache line for address:" << QString("0x%1").arg(addr, 0, 16);
		}
	}
	void onCPUProgress(int cpuId, QString _text)
	{
		qDebug() << "[AlphaCPU] CPU" << cpuId << "progress:" << _text << "%";
	}
	void onCpuStatusUpdate(int cpuId, QString& status)
	{
		qDebug() << "[AlphaCPU] CPU" << cpuId << "status update:" << status;
	}
	void onProtectionFault(quint64 vaddr, int accessType) {
		qWarning() << "[AlphaCPU] Protection fault at virtual address:" << vaddr << "Access type:" << accessType;
	}

	void onTranslationMiss(quint64 vaddr) {
		qWarning() << "[AlphaCPU] Translation miss at virtual address:" << vaddr;
	}

	/**
 * @brief Pause CPU execution
 *
 * This slot is triggered when a pause request is received from the system.
 * It stops the current execution without resetting the CPU state.
 */
	Q_INVOKABLE void onPauseExecution() {
		if (!m_isRunning || m_halted) {
			// Already paused or halted
			return;
		}

		DEBUG_LOG(QString("CPU%1: Execution paused at PC=0x%2")
			.arg(m_cpuId)
			.arg(m_pc, 16, 16, QChar('0')));

		// Store that we're no longer running
		m_isRunning = false;

		// Stop the instruction pipeline
		if (m_fetchUnit) {
			m_fetchUnit->pause();
		}

		// Cancel any pending memory operations that can be cancelled
		for (auto& pendingLoad : m_pendingLoads) {
			if (pendingLoad.isValid() && !pendingLoad.isComplete() && pendingLoad.isCancellable()) {
				pendingLoad.cancel();
			}
		}

		for (auto& pendingStore : m_pendingStores) {
			if (pendingStore.isValid() && !pendingStore.isComplete() && pendingStore.isCancellable()) {
				pendingStore.cancel();
			}
		}

		// Save current state for resumption
		m_pausedState.programCounter = m_pc;
		m_pausedState.processorStatus = m_processorStatus;
		m_pausedState.currentMode = m_currentMode;

		// Notify the system that we've paused
		emit sigExecutionPaused(m_cpuId);
	}
	/**
	* @brief Handle an incoming interrupt from the system
	*
	* This slot is triggered when an interrupt signal is received.
	* It sets up the appropriate interrupt flags and triggers handling
	* if interrupts are enabled.
	*
	* @param irqLine The interrupt line number (0-31)
	*/
	void onReceiveInterrupt(int irqLine) {
		DEBUG_LOG(QString("CPU%1: Received interrupt on line %2")
			.arg(m_cpuId)
			.arg(irqLine));

		// Record the pending interrupt
		pendingInterrupts.insert(irqLine);
		interruptPending.storeRelaxed(true);

		// Set the appropriate bit in the IPIR register
		quint64 ipirValue = m_iprs.read(IprBank::IPIR);
		ipirValue |= (1ULL << irqLine);
		m_iprs.write(IprBank::IPIR, ipirValue);

		// Set default priority if not specified (half of maximum priority)
		if (!interruptPriorities.contains(irqLine)) {
			interruptPriorities[irqLine] = 8; // Default priority - middle of range (0-15)
		}

		// If interrupts are enabled and we're running, deliver immediately
		if (m_isRunning && (m_processorStatus & PS_INTERRUPT_ENABLE) && !m_inExceptionHandler) {
			if (canTakeInterrupt(irqLine)) {
				deliverPendingInterrupt();
			}
		}
	}
	/**
	*@brief Reset the CPU to initial state
	*
	* This slot is triggered when a reset signal is received from the system.
	* It resets all CPU registers and state to their initial values.
	*/

	void onResetCPU() {
		DEBUG_LOG(QString("CPU%1: Reset initiated")
			.arg(m_cpuId));

		// Stop current execution if running
		m_isRunning = false;
		m_halted = false;

		// Reset all pipeline stages
		if (m_fetchUnit) {
			m_fetchUnit->reset();
		}

		if (m_decodeStage) {
			m_decodeStage->reset();
		}

		if (m_executeStage) {
			m_executeStage->reset();
		}

		if (m_writebackStage) {
			m_writebackStage->reset();
		}

		// Clear pending memory operations
		m_pendingLoads.clear();
		m_pendingStores.clear();
		m_memoryBarrierPending = false;

		// Reset program counter to reset vector
		m_pc = 0;

		// Reset processor mode to kernel for initialization
		m_currentMode = ProcessorMode::KERNEL;

		// Reset processor status register - kernel mode, interrupts disabled
		m_processorStatus = PS_KERNEL_MODE;
		m_interruptEnable = false;
		m_fpEnable = false;

		// Reset system control registers
		m_iprs.write(IprBank::SCBB, 0); // System Control Block Base
		m_iprs.write(IprBank::PCBB, 0); // Process Control Block Base
		m_iprs.write(IprBank::ASN, 0);  // Address Space Number
		m_iprs.write(IprBank::IPL, 0);  // Interrupt Priority Level

		// Clear exception state
		m_inExceptionHandler = false;
		m_hasException = false;
		m_exceptionPending = false;
		m_exceptionPc = 0;
		m_exceptionReturnAddress = 0;
		m_exceptionCause = 0;

		// Clear interrupt state
		pendingInterrupts.clear();
		interruptPriorities.clear();
		interruptPending.storeRelaxed(false);


		// Reset TLB and caches
		if (m_memorySystem) {
			m_memorySystem->reset();   // Todo
		}

		if (m_instructionCache) {
			m_instructionCache->invalidateAll();
		}

		if (m_level1DataCache) {
			m_level1DataCache->invalidateAll();
		}

		if (m_level2DataCache) {
			m_level2DataCache->invalidateAll();
		}

		if (m_translationCache) {
			m_translationCache->invalidateAll();
		}

		// Reset performance counters
		resetPerformanceCounters();

		// Set PAL base address (typically provided by firmware)
		m_palCodeBase = 0xFFFFFFFF80000000ULL;

		// Send reset signal
		emit sigHandleReset();

		DEBUG_LOG(QString("CPU%1: Reset complete, PC=0x%2")
			.arg(m_cpuId)
			.arg(m_pc, 16, 16, QChar('0')));
	}

	/**
	*@brief Resume CPU execution after a pause
	*
	* This slot is triggered when a resume signal is received.
	* It restores CPU state and continues execution from where it was paused.
	*/
	void onResumeExecution()
	{
		if (m_isRunning || m_halted) {
			// Already running or halted
			return;
		}

		DEBUG_LOG(QString("CPU%1: Resuming execution at PC=0x%2")
			.arg(m_cpuId)
			.arg(m_pc, 16, 16, QChar('0')));

		// Restore state from pause
		m_pc = m_pausedState.programCounter;
		m_processorStatus = m_pausedState.processorStatus;
		m_currentMode = m_pausedState.currentMode;

		// Mark CPU as running
		m_isRunning = true;

		// Resume pipeline
		if (m_fetchUnit) {
			m_fetchUnit->resume();
		}

		// Check for pending interrupts that may have occurred while paused
		if ((m_processorStatus & PS_INTERRUPT_ENABLE) && !m_inExceptionHandler && interruptPending.loadRelaxed()) {
			deliverPendingInterrupt();
		}

		// Notify system that execution has resumed
		emit sigExecutionStarted(m_cpuId);
	}


	/**
	 * @brief Start CPU execution
	 *
	 * This slot is triggered when a start execution request is received.
	 * It begins CPU execution from the current program counter.
	 */
	void onStartExecution() {
		if (m_isRunning || m_halted) {
			// Already running or halted
			return;
		}

		DEBUG_LOG(QString("CPU%1: Starting execution from PC=0x%2")
			.arg(m_cpuId)
			.arg(m_pc, 16, 16, QChar('0')));

		// Mark CPU as running
		m_isRunning = true;

		// Start the instruction pipeline
		if (m_fetchUnit) {
			m_fetchUnit->start();
		}

		// Reset stop requested flag
		stopRequested.storeRelaxed(false);

		// Notify the system that execution has started
		emit sigExecutionStarted(m_cpuId);
	}

#pragma region StopExecution Block
	/**
	 * @brief Slot to handle stopping CPU execution
	 * This method gracefully stops the CPU execution, saves state, and notifies observers
	 */
	void onStopExecution() {
		// Prevent recursive calls if already stopping
		if (m_cpuState == CPUState::HALTED || m_isShuttingDown) {
			return;
		}

		// Set shutdown flag to prevent new operations
		m_isShuttingDown = true;

		// Save current CPU state before stopping
		CPUState previousState = m_cpuState;
		m_cpuState = CPUState::HALTED;

		try {
			// Stop instruction fetch and execution pipeline
			stopInstructionPipeline();

			// Flush any pending operations
			flushPendingOperations();

			// Save current execution context
			saveExecutionContext();

			// Stop any running timers or periodic operations
			if (m_executionTimer && m_executionTimer->isActive()) {
				m_executionTimer->stop();
			}

			// Clear any pending interrupts that won't be processed
			clearPendingInterrupts();

			// Update performance counters
			updatePerformanceCounters();

			// Log the stop event
			DEBUG_LOG(QString("CPU execution stopped. Previous state: %1, Current PC: 0x%2")
				.arg(static_cast<int>(previousState))
				.arg(QString::number(m_pc, 16).rightJustified(16, '0')));

			// Emit signals to notify other components
			emit sigCpuStateChanged(m_cpuState);
			emit sigExecutionStopped(m_pc, m_totalInstructionsExecuted);

			// If we were in an exception state, handle cleanup
			if (previousState == CPUState::EXCEPTION_HANDLING) {
				handleExceptionCleanup();
			}

			// Reset execution statistics
			m_currentInstructionCount = 0;
			m_lastStopReason = "Manual stop requested";

		}
		catch (const std::exception& e) {
			// Handle any exceptions during shutdown gracefully
			DEBUG_LOG(QString("Exception during CPU stop: %1").arg(e.what()));

			// Force stop even if cleanup failed
			m_cpuState = CPUState::HALTED;
			emit sigCpuStateChanged(m_cpuState);
			emit sigExecutionError(QString("Error during stop: %1").arg(e.what()));
		}

		// Clear the shutdown flag
		m_isShuttingDown = false;

		DEBUG_LOG("CPU execution stopped successfully");
	}

	/**
	 * @brief Stop the instruction pipeline and flush any in-flight instructions
	 */
	void stopInstructionPipeline() {
		// Stop fetching new instructions
		m_allowInstructionFetch = false;

		// Mark any in-flight instructions as cancelled
		if (m_currentInstruction.isValid()) {
			m_currentInstruction.setState(InstructionState::CANCELLED);
		}

		// Clear instruction fetch queue
		m_instructionQueue.clear();

		// Stop any background instruction decoding
		if (m_decoderThread && m_decoderThread->isRunning()) {
			m_decoderThread->requestInterruption();
			m_decoderThread->wait(1000); // Wait up to 1 second for clean shutdown
		}
	}

	/**
	 * @brief Flush any pending memory operations or I/O requests
	 */
	void flushPendingOperations() {
		// Flush any pending memory writes
		if (m_memoryInterface) {
			m_memoryInterface->flushPendingWrites();
		}

		// Cancel any pending DMA operations
		if (m_dmaController) {
			m_dmaController->cancelPendingOperations();
		}

		// Flush any pending I/O operations
		flushPendingIO();

		// Wait for critical operations to complete
		waitForCriticalOperations();
	}

	/**
	 * @brief Save the current execution context for potential resume
	 */
	void saveExecutionContext() {
		if (!m_registerBank) {
			return;
		}

		// Save current register state
		m_savedContext.pc = m_pc;
		m_savedContext.sp = m_registerBank->readIntReg(30); // R30 is typically stack pointer
		m_savedContext.gp = m_registerBank->readIntReg(29); // R29 is typically global pointer

		// Save integer registers
		for (int i = 0; i < 31; ++i) {
			m_savedContext.intRegs[i] = m_registerBank->readIntReg(i);
		}

		// Save floating-point registers and FPCR
		RegisterFileWrapper* fpBank = m_registerBank->getFpBank();
		if (fpBank) {
			m_savedContext.fpcr = fpBank->readFpcrRaw();
			for (int i = 0; i < 31; ++i) {
				m_savedContext.fpRegs[i] = fpBank->readRaw(static_cast<FReg>(i));
			}
		}

		// Save processor status and control registers
		saveControlRegisters();

		// Mark context as valid
		m_savedContext.isValid = true;
		m_savedContext.saveTime = QDateTime::currentDateTime();

		DEBUG_LOG(QString("Execution context saved at PC: 0x%1")
			.arg(QString::number(m_pc, 16).rightJustified(16, '0')));
	}

	/**
	 * @brief Clear any pending interrupts that won't be processed
	 */
	void clearPendingInterrupts() {
		// Clear software interrupt requests
		m_pendingSoftwareInterrupts.clear();

		// Clear hardware interrupt requests that haven't been acknowledged
		m_pendingHardwareInterrupts.clear();

		// Reset interrupt priority level to default
		m_currentIPL = 0;

		// Notify interrupt controller
		if (m_interruptController) {
			m_interruptController->clearPendingInterrupts();
		}

		DEBUG_LOG("Pending interrupts cleared");
	}

	/**
	 * @brief Update performance counters before stopping
	 */
// 	void updatePerformanceCounters() {
// 		// Update total execution time
// 		m_totalExecutionTime += m_executionStartTime.msecsTo(QDateTime::currentDateTime());
// 
// 		// Calculate final performance metrics
// 		if (m_totalExecutionTime > 0) {
// 			m_averageIPS = (m_totalInstructionsExecuted * 1000.0) / m_totalExecutionTime;
// 		}
// 
// 		// Update cache hit rates if cache simulation is enabled
// 		updateCacheStatistics();
// 
// 		// Log final performance statistics
// 		DEBUG_LOG(QString("Performance Summary - Instructions: %1, Time: %2ms, Avg IPS: %3")
// 			.arg(m_totalInstructionsExecuted)
// 			.arg(m_totalExecutionTime)
// 			.arg(m_averageIPS, 0, 'f', 2));
// 	}

	/**
	 * @brief Handle cleanup for any active exceptions
	 */
	void handleExceptionCleanup() {
		// If we were processing an exception, clean up the exception state
		if (m_currentException.type != ExceptionType::NONE) {
			DEBUG_LOG(QString("Cleaning up active exception: %1")
				.arg(static_cast<int>(m_currentException.type)));

			// Reset exception state
			m_currentException.type = ExceptionType::NONE;
			m_currentException.pc = 0;
			m_currentException.badVaddr = 0;

			// Restore any saved state if needed
			if (m_exceptionStackDepth > 0) {
				// Pop exception stack
				m_exceptionStackDepth--;
			}
		}

		// Clear any exception-related flags
		m_inExceptionHandler = false;
		m_exceptionPending = false;
	}

	/**
	 * @brief Wait for critical operations to complete before stopping
	 */
	void waitForCriticalOperations() {
		const int maxWaitTime = 100; // Maximum wait time in ms
		QElapsedTimer timer;
		timer.start();

		while (timer.elapsed() < maxWaitTime) {
			// Check if any critical operations are still pending
			if (!hasCriticalOperationsPending()) {
				break;
			}

			// Process events to allow operations to complete
			//QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
		}

		if (timer.elapsed() >= maxWaitTime) {
			DEBUG_LOG("Warning: Critical operations did not complete within timeout");
		}
	}

	/**
	 * @brief Check if there are any critical operations still pending
	 * @return true if critical operations are pending
	 */
	bool hasCriticalOperationsPending() const {
		// Check for pending memory operations
		if (m_memoryInterface && m_memoryInterface->hasPendingOperations()) {
			return true;
		}

		// Check for pending I/O operations
		if (m_ioController && m_ioController->hasPendingOperations()) {
			return true;
		}

		// Check for active DMA transfers
		if (m_dmaController && m_dmaController->hasActiveTransfers()) {
			return true;
		}

		return false;
	}
#pragma endregion StopExecution Block
private:

	// Execution context structure for saving/restoring state
	struct ExecutionContext {
		bool isValid = false;
		QDateTime saveTime;
		quint64 pc = 0;
		quint64 sp = 0;
		quint64 gp = 0;
		quint64 intRegs[31] = { 0 };
		quint64 fpRegs[31] = { 0 };
		quint64 fpcr = 0;
		// Add other control registers as needed
	};

	void initialize()
	{
		m_stackManager.reset(new StackManager(DEFAULT_STACK_SIZE));

		m_fetchUnit.reset(new FetchUnit(nullptr));
		m_decodeStage.reset(new DecodeStage(nullptr));
		m_executeStage.reset(new ExecuteStage(nullptr));
		m_writebackStage.reset(new WritebackStage);
		m_fetchUnit->attachAlphaCPU(this);

		m_registerBank.reset(new RegisterBank(nullptr));

		// Initialize prefetch buffers
		m_prefetchBuffers.resize(4);  // 4 prefetch buffers
	

		m_memorySystem->initializeCpuModel(m_cpuModel);
		initializeSignalsAndSlots();
	}

	/// Expose SP via StackManager
	quint64 getStackPointer() const {
		return m_stackManager->currentSP();
	}

	/// Example call‐stack push (e.g. on CALL_PAL)
	void pushCallFrame(const CallFrame& frame) {
		m_stackManager->pushFrame(frame);
	}

	/// Example pop (e.g. on REI)
	CallFrame popCallFrame() {
		return m_stackManager->popFrame();
	}
	void initializeSignalsAndSlots() {
		connect(m_AlphaSMPManager, &AlphaSMPManager::systemStarted, this, &AlphaCPU::onSystemStarted);
		connect(m_AlphaSMPManager, &AlphaSMPManager::systemStopped, this, &AlphaCPU::onSystemStopped);
		connect(m_AlphaSMPManager, &AlphaSMPManager::systemPaused, this, &AlphaCPU::onSystemPaused);
		connect(m_AlphaSMPManager, &AlphaSMPManager::systemResumed, this, &AlphaCPU::onSystemResumed);
		//connect(m_AlphaSMPManager, &AlphaSMPManager::configureSystem, this, &AlphaCPU::onConfigureSystem);
		connect(m_AlphaSMPManager, &AlphaSMPManager::signalStartAll, this, &AlphaCPU::onSignalStartAll);
		connect(m_AlphaSMPManager, &AlphaSMPManager::signalStopAll, this, &AlphaCPU::onSignalStopAll);
		connect(m_AlphaSMPManager, &AlphaSMPManager::signalPauseAll, this, &AlphaCPU::onSignalPauseAll);
		connect(m_AlphaSMPManager, &AlphaSMPManager::signalResumeAll, this, &AlphaCPU::onSignalResumeAll);
		connect(m_AlphaSMPManager, &AlphaSMPManager::signalResetAll, this, &AlphaCPU::onSignalResetAll);
		connect(m_AlphaSMPManager, &AlphaSMPManager::signalSendInterrupt, this, &AlphaCPU::onSignalSendInterrupt);
		connect(this, &AlphaCPU::sigCacheCoherencyEvent, this, &AlphaCPU::onCacheCoherencyEvent);
		connect(m_AlphaSMPManager, &AlphaSMPManager::cacheCoherencyEvent, this, &AlphaCPU::onCacheCoherencyEvent);
		connect(this, &AlphaCPU::sigCpuStatusUpdate, m_AlphaSMPManager, &AlphaSMPManager::onCpuStatusUpdate);
		// 	connect(m_AlphaSMPManager, &AlphaSMPManager::cpuProgress, this, &AlphaCPU::onCpuProgress);
		// 	connect(m_AlphaSMPManager, &AlphaSMPManager::allCPUsStarted, this, &AlphaCPU::onAllCPUsStarted);
		// 	connect(m_AlphaSMPManager, &AlphaSMPManager::allCPUsStopped, this, &AlphaCPU::onAllCPUsStopped);
		// 	connect(m_AlphaSMPManager, &AlphaSMPManager::allCPUsPaused, this, &AlphaCPU::onAllCPUsPaused);
	}




	/**
	 * @brief Return from exception handler
	 *
	 * This function is called by the REI (Return from Exception/Interrupt)
	 * instruction to return from exception handling.
	 */
	void returnFromException();

	void pushExceptionFrame(quint64 pc, quint64 ps, quint64 excSum)
	{
		// Use FrameHelpers instead of direct frame manipulation
		bool success = FrameHelpers::pushTrapFrame(
			*m_stackManager,
			pc,
			ps,
			excSum,
			m_registerBank->getIntRegisterArray(),
			m_registerBank->fp().fpcr.toRaw()
		);

		if (!success) {
			// Handle stack overflow
			DEBUG_LOG("Exception frame stack overflow!");
			handleDoubleFault();
		}
	}


	int getCurrentIPL() 
	{
		return static_cast<int>(readIPR(IPRNumbers::IPR_IPLR));
	}
	//internal Register Support

	// IPR interface for this CPU

	///////////////////////////////////////////////////////////////



	bool m_isRunning=false; // is the CPU running.
	bool m_bMMUEnabled = true; // Uses AlphaMemorySystem::translate()  
	bool m_halted = false; // is the CPU halted.

	// Processor state

	ProcessorMode m_savedProcessorMode;
	bool m_savedInterruptEnable;
	bool m_interruptEnable = true;
	bool m_fpEnable = true;
	quint64 m_currentIpl = 0;

	bool m_notificationEnabled;  // Set interrupt to other CPUs if needed
	// This would be system-specific implementation
	// TLB system reference
	TLBSystem* m_tlbSystem = nullptr;

	// Tracking for interrupt priorities
	QHash<int, int> interruptPriorities;  // vector -> priority


	// State tracking
	/*bool		m_inExceptionHandler = false;*/

	quint64	m_faultingVirtualAddress = 0;
	quint8 m_exceptionCause = static_cast<quint8>(EXCEPTION_CAUSE_UNKNOWN);
	quint32	m_faultingInstruction = 0;
	quint32	m_memoryManagementStatus = 0;
	MemoryFaultType	m_faultType;

	// Pipeline components
// 	FetchUnit* m_fetchUnit = nullptr;
// 	DecodeStage* m_decodeStage = nullptr;
// 	ExecuteStage* m_executeStage = nullptr;
// 	WritebackStage* m_writebackStage = nullptr;

	// Pending operations
// 	QVector<PendingLoad> m_pendingLoads;
// 	QVector<PendingStore> m_pendingStores;
	bool m_memoryBarrierPending = false;

	// CPU features state
	bool m_speculativeExecutionEnabled = true;
	bool m_branchPredictionEnabled = true;
	bool m_prefetchingEnabled = true;
	bool m_memoryOrderingStrict = false;
	bool m_outOfOrderExecution = true;
	bool m_superscalarDispatch = true;



	// Halt state
	bool m_halted = false;
	bool m_running = true;
	bool m_machineCheckPending = false;
	bool m_doubleFaultDetected = false;
	bool m_criticalError = false;

	// Stack pointers
	quint64 m_currentStackPointer = 0;

	// Exception state
	quint64 m_exceptionReturnAddress = 0;




	// Exception counters
	quint64 m_machineCheckCount = 0;
	quint64 m_alignmentFaultCount = 0;
	quint64 m_illegalInstructionCount = 0;
	quint64 m_interruptCount = 0;
	quint64 m_astCount = 0;
	quint64 m_otherExceptionCount = 0;
	quint64 m_cycleCounter = 0;

	// Processor status
	quint64 m_savedProcessorStatus = 0;


	// Machine check state
/*	bool m_machineCheckPending = false;*/
	quint64 m_machineCheckPC = 0;
	MachineCheckType m_machineCheckType;

	// Exception handling
	int m_exceptionPriority = 0;

	QAtomicInt stopRequested;
	QAtomicInt interruptPending;
	QSet<int> pendingInterrupts;     // Set of currently pending IRQs

	// Performance counters
	quint64 m_tlbInvalidateAllCount = 0;
	quint64 m_tlbInvalidateProcessCount = 0;
	quint64 m_tlbInvalidateSingleCount = 0;
	quint64 m_tlbInvalidateDataCount = 0;
	quint64 m_tlbInvalidateInstructionCount = 0;


	// System Entry Points and PALCodes
	SystemEntryPoints m_systemEntryPoints;  // System entry point table
	PalcodeType m_palcodeType;             // Type of PALcode being used

#pragma region Performance Counters
	// Performance counters
	quint64 m_icacheInvalidateCount = 0;
#pragma endregion Performance Counters


	/*
	* 
	void attachTlbSystem(TLBSystem* tlb) { m_tlbSystem = tlb; } 	// Constructor should initialize TLB system reference
	
	*/


	InstructionCache* m_instructionCache;	//Simulates L1 instruction cache with configurable size and associativity
	IRQController* m_irqController;
	MMIOManager* m_mmioManager;     // the virtual memory MMIOManager
	AlphaSMPManager* m_AlphaSMPManager;
	QScopedPointer<RegisterBank> m_registerBank; // for floating point registerBank.fp().fpcr
	InstructionTLB* m_instructionTlb;		//Separate TLB for instruction address translations
	AlphaMemorySystem* m_memorySystem;  // Comprises the memory system, dispatcher to physical ram (safeMemory), virtual memory (MMIO)
	UnifiedDataCache* m_level2DataCache;  // Inherited from AlphaSMPManager


#pragma region Cache Properties

	// Performance monitoring state
	PerfCounter m_perfCounters[MAX_PERF_COUNTERS];     // Array of performance counters
	bool m_performanceMonitoringActive;                // Is monitoring currently active?
	bool m_perfMonInterruptPending;                    // Is there a pending performance interrupt?
	quint32 m_perfMonInterruptVector;                  // Interrupt vector for pending interrupt
	bool m_profilingActive;                           // Is profiling currently active?
	quint64 m_profilingSamplingRate;                  // Cycles between samples
	quint64 m_profileNextSample;                      // Cycle count for next sample
	int m_profileSampleCount;                         // Total samples collected
	bool m_profileTimerActive;                        // Is profile timer active?
	QVector<ProfileEntry> m_profileBuffer;            // Buffer for profile samples
	// Monitoring filters
	MonitoringFilters m_monitoringFilters;            // Active monitoring filters
	CPUState m_cpuState;
	// Symbol resolution
	QHash<quint64, QString> m_symbolTable;            // Map of addresses to symbol names
	QVector<ModuleInfo> m_moduleTable;                // Table of loaded modules

	// Last instruction state for event detection
	bool m_lastInstructionDCacheMiss;                 // Did last instruction cause D-cache miss?
	bool m_lastInstructionICacheMiss;                 // Did last instruction cause I-cache miss?
	
	//QHash<quint64, quint64> m_translationCache;// translation cache this is a shared cache for all processors


#pragma endregion Cache Properties
	QScopedPointer<UnifiedDataCache> m_level1DataCache;         // this is a local (this) data cache	
	QScopedPointer<StackFrame> m_stackFrame;
	QScopedPointer<StackManager> m_stackManager;
	QVector<PendingLoad> m_pendingLoads;					// Tracks in-flight memory load operations
	QVector<PendingStore> m_pendingStores;					// Tracks in - flight memory store operations

	// Prefetch buffers
	QVector<InstructionBuffer> m_prefetchBuffers;			// Prefetch buffer for instructions
	bool m_prefetchBufferValid = false;

	// Instruction queue
	QQueue<DecodedInstruction> m_instructionQueue;

	bool canTakeInterrupt(int vector) 
	{
		// Can't take interrupts if we're already handling an exception
		if (m_inExceptionHandler) {
			return false;
		}

		// Check if interrupts are enabled
		if (!(m_processorStatus & PS_IE)) {
			return false;
		}

		// Get current IPL
		quint64 currentIPL = readIPR(IPRNumbers::IPR_IPLR);

		// Get priority of this interrupt
		int priority = 0;

		// Check stored priorities first
		if (interruptPriorities.contains(vector)) {
			priority = interruptPriorities[vector];
		}
		else {
			// Extract from IPIR_PRIORITY register (4 bits per vector)
			quint64 priorityValue = readIPR(IPRNumbers::IPR_IPIR_PRIORITY);
			priority = (priorityValue >> (vector * 4)) & 0xF;
		}

		// Can take interrupt if its priority is higher than current IPL
		return (priority > currentIPL);
	}

	/**
	 * @brief Stop the instruction execution pipeline
	 */
	void stopInstructionPipeline() {
		// Stop fetching new instructions
		m_fetchUnit->stop();

		// Flush all pipeline stages
		m_decodeStage->flush();
		m_executeStage->flush();
		m_writebackStage->flush();

		// Cancel any pending instructions
		m_instructionQueue.clear();

		DEBUG_LOG("Instruction pipeline stopped and flushed");
	}
	
	/**
	 * @brief Flush all pending memory operations
	 *
	 * Ensures that all pending loads and stores complete before continuing.
	 * This is used before exception handling and during memory barriers.
	 */
	void flushPendingMemoryOperations() {
		// Ensure all pending loads complete
		for (auto& pendingLoad : m_pendingLoads) {
			if (pendingLoad.isValid() && !pendingLoad.isComplete()) {
				pendingLoad.waitForCompletion();
			}
		}
		m_pendingLoads.clear();

		// Ensure all pending stores complete
		for (auto& pendingStore : m_pendingStores) {
			if (pendingStore.isValid() && !pendingStore.isComplete()) {
				pendingStore.waitForCompletion();
			}
		}
		m_pendingStores.clear();

		// Flush write buffers
		if (m_memorySystem) {
			m_memorySystem->flushWriteBuffers(this);
		}

		// Ensure memory ordering consistency
		m_memoryBarrierPending = false;

		DEBUG_LOG("All pending memory operations flushed");
	}

	/**
	 * @brief Get halt reason for logging
	 * @return String describing why CPU halted
	 */
	QString getHaltReason() const {
		if (m_machineCheckPending) {
			return QString("Machine Check (type=%1)").arg(static_cast<int>(m_machineCheckType));
		}

		if (m_doubleFaultDetected) {
			return "Double Fault";
		}

		if (m_criticalError) {
			return "Critical Error";
		}

		return "Unknown/Requested Halt";
	}
	/**
	 * @brief Notify system of CPU halt
	 */
	void notifySystemOfHalt() {
		// Notify SMP manager if available
		if (m_AlphaSMPManager) {
			m_AlphaSMPManager->notifyCpuHalted(m_cpuId);
		}

		// Set interrupt to other CPUs if needed
		// This would be system-specific implementation

		// Emit Qt signal if available
		if (m_notificationEnabled) {
			emit cpuHalted(m_cpuId);
		}
	}


};



