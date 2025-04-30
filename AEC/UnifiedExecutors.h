// UnifiedExecutors.h - Simplified Alpha AXP Emulator Executors
// Includes Integer, FloatingPoint, Vector, and Control Executors
// Reference: Alpha AXP Architecture, Vol. I & II (Digital Equipment Corp.)

#pragma once

#include "dt_gfloat.h"
#include <QObject>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QSharedPointer>
#include <QtEndian>
#include "AlphaCPUInterface.h"
#include "SafeMemory.h"
#include "RegisterBank.h"
#include "FpcrRegister.h"
#include "Helpers.h"
#include "TraceManager.h"
#include "FpRegisterBankcls.h"

quint64 decodeMemoryOffset(const helpers_JIT::OperateInstruction& op, RegisterBank* regs)
{
	if ((op.rawInstruction >> 12) & 0x1) {
		quint64 offset = (op.rawInstruction >> 13) & 0x7FFF;
		if (offset & 0x4000)
			offset |= 0xFFFFFFFFFFFF8000ULL;
		return offset;
	}
	else {
		return regs->readIntReg(op.rb);
	}

}

// Instruction decode format for Alpha AXP FP and Integer instructions
// struct OperateInstruction {
// 	quint8 opcode;     // bits 31:26
// 	quint8 ra;         // bits 25:21
// 	quint8 rb;         // bits 20:16
// 	quint8 rc;         // bits 4:0
// 	quint8 function;   // bits 5:0
// };
// 
// inline OperateInstruction decodeOperate(quint32 instr) {
// 	return {
// 		static_cast<quint8>((instr >> 26) & 0x3F),
// 		static_cast<quint8>((instr >> 21) & 0x1F),
// 		static_cast<quint8>((instr >> 16) & 0x1F),
// 		static_cast<quint8>(instr & 0x1F),
// 		static_cast<quint8>(instr & 0x3F)
// 	};
// }

// ===========================================================================
// Base Executor Class - All Executors Inherit from This
// ===========================================================================
class AlphaExecutor : public QObject {
	Q_OBJECT
public:
	AlphaExecutor(AlphaCPUInterface* cpu, SafeMemory* mem, RegisterBank* regs, FpRegisterBankcls* fpRegs_, QObject* parent = nullptr)
		: QObject(parent), alphaCPU(cpu), memory(mem), regs(regs), fpRegs_alphaExecutor(fpRegs_){
	}

	virtual ~AlphaExecutor() {

	
	}

	virtual void execute(quint32 instruction) = 0;
	virtual void buildDispatchTable() = 0;

signals:
	void instructionExecuted(quint32 instr);
	void registerUpdated(int regIndex, quint64 value);
	void memoryAccessed(quint64 addr, quint64 value, bool isWrite);

protected:
	AlphaCPUInterface* alphaCPU;
	SafeMemory* memory;
	RegisterBank* regs;
	FpRegisterBankcls* fpRegs_alphaExecutor;
};

// ===========================================================================
// IntegerExecutor
// ===========================================================================

// IntegerExecutor.H

/**
 * @brief Executes Alpha AXP integer instructions
 *
 * Reference: Alpha AXP System Reference Manual (1994)
 * - Chapter 3: Instruction Formats
 * - Chapter 4: Instruction Descriptions
 * - Appendix C: Opcode Summary
 */
class IntegerExecutor : public AlphaExecutor {
	Q_OBJECT


	enum IntegerSubType {
		ARITHMETIC,
		COMPARISON,
		CONVERSION,
		UNKNOWN
	};

signals:
	void trapRaised(helpers_JIT::TrapType type);
	void illegalInstruction(quint64 instructionWord, quint64 pc);
public:
	explicit IntegerExecutor(AlphaCPUInterface* cpuptr, SafeMemory* mem, RegisterBank* regs, FpRegisterBankcls* fpRegs, QObject* parent = nullptr)
		: AlphaExecutor(cpuptr, mem, regs, fpRegs, parent) {
		buildDispatchTable();
	}

	/**
	 * @brief Decodes and executes an instruction using the dispatch table.
	 */
	void execute(quint32 instr) override {
		QMutexLocker locker(&mutex);

		// Decode top 6 bits (bits 31:26) for opcode
		quint8 opcode = (instr >> 26) & 0x3F;

		auto handler = dispatchTable.value(opcode, nullptr);

		if (handler) {
			handler(instr); // 🔥 Execute instruction handler
		}
		else {
			emit illegalInstruction(instr, alphaCPU->getPC()); // 🔥 Emit illegalInstruction signal
		}

		emit instructionExecuted(instr); // 🔥 Emit executed signal
	}


	/**
	 * @brief Builds the instruction dispatch table for all integer opcodes.
	 *
	 * - Opcode 0x10 = Integer arithmetic group
	 * - Opcode 0x11 = Integer logical group
	 */
	void buildDispatchTable() override {
		dispatchTable[0x10] = [this](quint32 instr) { dispatchArithmetic(instr); };
		dispatchTable[0x11] = [this](quint32 instr) { dispatchLogical(instr); };
	}

private:
	QHash<quint8, std::function<void(quint32)>> dispatchTable;
	QMutex mutex;

	/* Step	What it Does
		If function field = 0x6D	Possible CMPLE or CVTQL
		Check bit 12	If bit12 == 0 → CMPLE, else CVTQL
		Otherwise	Assume ARITHMETIC
	*/

	// Decode Integer DispatchArithmetic Subtypes
	inline IntegerSubType decodeIntegerSubtype(quint32 instr) {
		quint8 func = instr & 0x3F;
		quint8 minor = (instr >> 5) & 0x7;

		if (func == 0x6D) {
			if (minor == 0b001) return COMPARISON; // CMPLE minor pattern
			else return CONVERSION; // CVTQL pattern
		}
		// More cases can be added here later.

		return ARITHMETIC; // Default
	}


	/**
	 * @brief Dispatches integer arithmetic instructions (Opcode 0x10).
	 */
	void dispatchArithmetic(quint32 instr) {
		quint8 opcode = (instr >> 26) & 0x3F;
		quint8 func = instr & 0x3F; // lower 6 bits
		quint8 minorFunc = (instr >> 5) & 0x7; // minor subfields if needed

		auto op = helpers_JIT::decodeOperate(instr);

		// 🆕 Add decoding for CMPLE vs CVTQL disambiguation
		IntegerSubType subtype = decodeIntegerSubtype(instr);

		switch (op.function) {
		case 0x00: execADDL(op); break;  // ADDL
		case 0x20: execADDQ(op); break;  // ADDQ
		case 0x2D: execCMPEQ(op); break; // CMPEQ
		case 0x4D: execCMPLT(op); break; // CMPLT
		case 0x6D:
			if (subtype == COMPARISON) {
				execCMPLE(op);
			}
			else if (subtype == CONVERSION) {
				execCVTQL(op);
			}
			else {
				emit illegalInstruction(instr, alphaCPU->getPC());
			}
			break;
		case 0x0D: execCVTLQ(op); break; // CVTLQ
		default:
			emit illegalInstruction(instr, alphaCPU->getPC());
			break;
		}
	}


	/**
	 * @brief Dispatches integer logical instructions (Opcode 0x11).
	 */
	void dispatchLogical(quint32 instr) {
		auto op = helpers_JIT::decodeOperate(instr);
		switch (op.function) {
		case 0x00: execAND(op); break; // AND
		case 0x08: execBIC(op); break; // BIC
		case 0x20: execBIS(op); break; // BIS (Logical OR)
		case 0x40: execXOR(op); break; // XOR
		case 0x48: execEQV(op); break; // EQV
		default: emit illegalInstruction(instr, this->alphaCPU->getPC()); break;
		}
	}

	// Arithmetic
	void execADDL(const helpers_JIT::OperateInstruction& op) {
		quint32 result = static_cast<quint32>(regs->readIntReg(op.ra) + regs->readIntReg(op.rb));
		regs->writeIntReg(op.rc, static_cast<quint64>(result));
		emit registerUpdated(op.rc, result);
	}

	void execADDQ(const helpers_JIT::OperateInstruction& op) {
		quint64 result = regs->readIntReg(op.ra) + regs->readIntReg(op.rb);
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execCMPEQ(const helpers_JIT::OperateInstruction& op) {
		quint64 result = (regs->readIntReg(op.ra) == regs->readIntReg(op.rb)) ? 1 : 0;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execCMPLT(const helpers_JIT::OperateInstruction& op) {
		qint64 a = static_cast<qint64>(regs->readIntReg(op.ra));
		qint64 b = static_cast<qint64>(regs->readIntReg(op.rb));
		regs->writeIntReg(op.rc, a < b ? 1 : 0);
		emit registerUpdated(op.rc, a < b ? 1 : 0);
	}

	void execCMPLE(const helpers_JIT::OperateInstruction& op) {
		qint64 a = static_cast<qint64>(regs->readIntReg(op.ra));
		qint64 b = static_cast<qint64>(regs->readIntReg(op.rb));
		regs->writeIntReg(op.rc, a <= b ? 1 : 0);
		emit registerUpdated(op.rc, a <= b ? 1 : 0);
	}
	//Sign-extend 32-bit source integer to 64-bit destination
	void execCVTLQ(const helpers_JIT::OperateInstruction& op) {
		quint32 src = static_cast<quint32>(regs->readIntReg(op.ra) & 0xFFFFFFFF); // Read as 32-bit
		qint64 result = static_cast<qint64>(static_cast<qint32>(src)); // Sign-extend 32→64 bits
		regs->writeIntReg(op.rc, static_cast<quint64>(result)); // Write back to 64-bit register
		emit registerUpdated(op.rc, static_cast<quint64>(result));
	}
	//Truncate 64-bit source integer to lower 32 bits, sign-extend to 64 bits
	void execCVTQL(const helpers_JIT::OperateInstruction& op) {
		qint64 src = static_cast<qint64>(regs->readIntReg(op.ra)); // Read full 64-bit source
		quint32 lower32 = static_cast<quint32>(src & 0xFFFFFFFF); // Take low 32 bits
		qint64 result = static_cast<qint64>(static_cast<qint32>(lower32)); // Sign-extend 32→64 bits
		regs->writeIntReg(op.rc, static_cast<quint64>(result)); // Write back
		emit registerUpdated(op.rc, static_cast<quint64>(result));
	}

	// Logical
	void execAND(const helpers_JIT::OperateInstruction& op) {
		quint64 result = regs->readIntReg(op.ra) & regs->readIntReg(op.rb);
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execBIC(const helpers_JIT::OperateInstruction& op) {
		quint64 result = regs->readIntReg(op.ra) & ~regs->readIntReg(op.rb);
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execBIS(const helpers_JIT::OperateInstruction& op) {
		quint64 result = regs->readIntReg(op.ra) | regs->readIntReg(op.rb);
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execXOR(const helpers_JIT::OperateInstruction& op) {
		quint64 result = regs->readIntReg(op.ra) ^ regs->readIntReg(op.rb);
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execEQV(const helpers_JIT::OperateInstruction& op) {
		quint64 result = ~(regs->readIntReg(op.ra) ^ regs->readIntReg(op.rb));
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}
};


// ===========================================================================
// FloatingPointExecutor
// ===========================================================================
class FloatingPointExecutor : public AlphaExecutor {
	Q_OBJECT

signals:
	void trapRaised(const QString& reason);
	void illegalInstruction(quint64 instructionWord, quint64 pc);
public:
	explicit FloatingPointExecutor(AlphaCPUInterface* cpuptr, SafeMemory* mem, RegisterBank* regs, FpRegisterBankcls* fpRegs__, QObject* parent = nullptr)
		: AlphaExecutor(cpuptr, mem, regs, nullptr, parent) {
		buildDispatchTable();
		fpRegs = fpRegs__;
	}

	void execute(quint32 instr) override {
		QMutexLocker locker(&mutex);
		auto op = helpers_JIT::decodeOperate(instr);
		auto handler = dispatchTable.value(op.function & 0x3F, nullptr);
		if (handler) {
			handler(op);
		}
		else {
			emit illegalInstruction(instr, alphaCPU->getPC()); // 🔥 Raise illegal instruction!
		}
		emit instructionExecuted(instr);
	}

	void buildDispatchTable() override {
		dispatchTable[0x00] = [this](const helpers_JIT::OperateInstruction& op) { execADDF(op); };
		dispatchTable[0x01] = [this](const helpers_JIT::OperateInstruction& op) { execSUBF(op); };
		dispatchTable[0x02] = [this](const helpers_JIT::OperateInstruction& op) { execMULF(op); };
		dispatchTable[0x03] = [this](const helpers_JIT::OperateInstruction& op) { execDIVF(op); };
		dispatchTable[0x06] = [this](const helpers_JIT::OperateInstruction& op) { execCVTQS(op); };
		dispatchTable[0x07] = [this](const helpers_JIT::OperateInstruction& op) { execCVTTQ(op); };
		dispatchTable[0x1E] = [this](const helpers_JIT::OperateInstruction& op) { execCPYS(op); };
		dispatchTable[0x1F] = [this](const helpers_JIT::OperateInstruction& op) { execCPYSN(op); };
		dispatchTable[0x20] = [this](const helpers_JIT::OperateInstruction& op) { execCPYSE(op); };
		dispatchTable[0x23] = [this](const helpers_JIT::OperateInstruction& op) { execFCMOVEQ(op); };
		dispatchTable[0x24] = [this](const helpers_JIT::OperateInstruction& op) { execFCMOVNE(op); };
		dispatchTable[0x25] = [this](const helpers_JIT::OperateInstruction& op) { execFCMOVLT(op); };
		dispatchTable[0x26] = [this](const helpers_JIT::OperateInstruction& op) { execFCMOVLE(op); };
		dispatchTable[0x27] = [this](const helpers_JIT::OperateInstruction& op) { execFCMOVGT(op); };
		dispatchTable[0x28] = [this](const helpers_JIT::OperateInstruction& op) { execFCMOVGE(op); };
		dispatchTable[0x2C] = [this](const helpers_JIT::OperateInstruction& op) { execMT_FPCR(op); }; // MT_FPCR
		dispatchTable[0x2D] = [this](const helpers_JIT::OperateInstruction& op) { execMF_FPCR(op); }; // MF_FPCR


	}


private:
	// Trap Handling
	void raiseFpTrap(const QString& reason) {
		emit trapRaised(reason);
	}
	QHash<quint8, std::function<void(const helpers_JIT::OperateInstruction&)>> dispatchTable;
	QMutex mutex;
	FpcrRegister fpcrRegister;

	FpRegisterBankcls* fpRegs = nullptr;

	/*
	Core FP arithmetic (ADDF, SUBF, MULF, DIVF)
	*/
	// Floating Point Add
	void execADDF(const helpers_JIT::OperateInstruction& op) {
		dt_gfloat a = fpRegs->readFpReg(op.ra);
		dt_gfloat b = fpRegs->readFpReg(op.rb);

		dt_gfloat result = a + b; // Correct operator

		fpRegs->writeFpReg(op.rc, result);
		emit registerUpdated(op.rc, result.raw);
	}

	// Floating Point Subtract
	void execSUBF(const helpers_JIT::OperateInstruction& op) {
		dt_gfloat a = fpRegs->readFpReg(op.ra);  // Fsrc1
		dt_gfloat b = fpRegs->readFpReg(op.rb);  // Fsrc2

		double diff = a.toDouble() - b.toDouble(); // IEEE 64-bit subtraction

		// Apply rounding using FPCR context
		double rounded = dt_gfloat::applyRounding(diff, fpRegs->getFpcrContext());

		dt_gfloat result = dt_gfloat::fromDouble(rounded);

		fpRegs->writeFpReg(op.rc, result); // Save result
		emit registerUpdated(op.rc, result.raw); // Notify
	}

	//Multiply two floating-point registers
	void execMULF(const helpers_JIT::OperateInstruction& inst) {
		dt_gfloat a = fpRegs->readFpReg(inst.ra); // Fsrc1 - Read Fsrc1 and Fsrc2
		dt_gfloat b = fpRegs->readFpReg(inst.rb); // Fsrc2 - Read Fsrc1 and Fsrc2

		double product = a.toDouble() * b.toDouble(); // IEEE double multiplication

		// Apply rounding according to FPCR context
		double rounded = dt_gfloat::applyRounding(product, fpRegs->getFpcrContext());

		dt_gfloat result = dt_gfloat::fromDouble(rounded); // Create final result

		fpRegs->writeFpReg(inst.rc, result); // Save to Fdst
		emit registerUpdated(inst.rc, result.raw);
	}

	// Divide two floating-point registers
	void execDIVF(const helpers_JIT::OperateInstruction& inst) {
		dt_gfloat numerator = fpRegs->readFpReg(inst.ra); // Fsrc1
		dt_gfloat denominator = fpRegs->readFpReg(inst.rb); // Fsrc2

		FPCRContext& ctx = fpRegs->getFpcrContext(); // Grab FPCR context

		if (denominator.isZero()) {
			if (ctx.trapDivZero()) {
				ctx.setStickyDivZero(); // Set sticky bit
				raiseFpTrap("DIVF divide by zero");
				return;
			}
		}

		double quotient = numerator.toDouble() / denominator.toDouble(); // IEEE 64-bit division
		double rounded = dt_gfloat::applyRounding(quotient, ctx);

		dt_gfloat result = dt_gfloat::fromDouble(rounded);

		fpRegs->writeFpReg(inst.rc, result);
		emit registerUpdated(inst.rc, result.raw);
	}

	
	/*
	IEEE conversion and trap-sensitive ops (CVTQS, CVTTQ)
	*/
	//Convert Quadword Integer to S_Float
	void execCVTQS(const helpers_JIT::OperateInstruction& op) {
		qint64 intVal = regs->readIntReg(op.ra); // Read source integer

		float sVal = static_cast<float>(intVal); // Convert to 32-bit float

		// Promote to double for dt_gfloat construction
		double promoted = static_cast<double>(sVal);

		// Apply FPCR rounding before storing
		double rounded = dt_gfloat::applyRounding(promoted, fpRegs->getFpcrContext());

		dt_gfloat result = dt_gfloat::fromDouble(rounded);

		fpRegs->writeFpReg(op.rc, result);
		emit registerUpdated(op.rc, result.raw);
	}

	//Convert S_Float (32-bit float) → Quadword Integer (signed 64-bit integer)
	void execCVTTQ(const helpers_JIT::OperateInstruction& op) {
		dt_gfloat val = fpRegs->readFpReg(op.ra); // Read Fsrc (S_Float promoted)

		FPCRContext& ctx = fpRegs->getFpcrContext(); // Grab FPCR context

		// Convert to integer according to Alpha rounding rules
		qint64 result = val.toInt64(ctx);

		regs->writeIntReg(op.rc, static_cast<quint64>(result)); // Save to integer register
		emit registerUpdated(op.rc, static_cast<quint64>(result));
	}

	/*
	Sign manipulation (CPYS, CPYSN, CPYSE)
	*/
	
	// Copy Sign (Floating-Point Operate Format)
	void execCPYS(const helpers_JIT::OperateInstruction& op) {
		dt_gfloat mag = fpRegs->readFpReg(op.ra);     // Magnitude source
		dt_gfloat signSrc = fpRegs->readFpReg(op.rb); // Sign source

		quint64 rawResult = mag.raw & ~dt_gfloat::SIGN_MASK; // Clear sign bit
		if (signSrc.sign()) {
			rawResult |= dt_gfloat::SIGN_MASK; // Set sign if negative
		}

		dt_gfloat result(rawResult); // Construct properly
		fpRegs->writeFpReg(op.rc, result);
		emit registerUpdated(op.rc, result.raw); // Always use .raw explicitly
	}

	// Copy Sign and Exponent (Floating-Point Format)
	void execCPYSE(const helpers_JIT::OperateInstruction& op) {
		dt_gfloat mag = fpRegs->readFpReg(op.ra);     // Magnitude source (Ra)
		dt_gfloat signSrc = fpRegs->readFpReg(op.rb); // Sign source (Rb)

		quint64 rawResult = mag.raw & ~dt_gfloat::SIGN_MASK; // Clear magnitude sign bit

		bool useInvertedSign = mag.isZero(); // If magnitude is zero, invert sign

		bool signBit;
		if (useInvertedSign) {
			signBit = !signSrc.sign(); // Invert sign
		}
		else {
			signBit = signSrc.sign(); // Normal sign
		}

		if (signBit) {
			rawResult |= dt_gfloat::SIGN_MASK; // Set sign bit if negative
		}

		dt_gfloat result(rawResult);
		fpRegs->writeFpReg(op.rc, result);
		emit registerUpdated(op.rc, result.raw);
	}

	// Copy Sign Negate - Floating Point Format
	void execCPYSN(const helpers_JIT::OperateInstruction& op) {
		dt_gfloat mag = fpRegs->readFpReg(op.ra);     // Magnitude source (Ra)
		dt_gfloat signSrc = fpRegs->readFpReg(op.rb); // Sign source (Rb)

		quint64 rawResult = mag.raw & ~dt_gfloat::SIGN_MASK; // Clear magnitude sign bit

		bool signBit = !signSrc.sign(); // Invert the sign source

		if (signBit) {
			rawResult |= dt_gfloat::SIGN_MASK; // Set sign bit if negative
		}

		dt_gfloat result(rawResult);
		fpRegs->writeFpReg(op.rc, result);
		emit registerUpdated(op.rc, result.raw);
	}
	


	/*
	Conditional moves (FCMOVEQ, FCMOVNE, FCMOVLT, FCMOVLE, FCMOVGT, FCMOVGE)
	Instruction			Condition			Status
	FCMOVEQ				Ra == 0	
	FCMOVNE				Ra ≠ 0	
	FCMOVLT				Ra < 0	
	FCMOVLE				Ra ≤ 0	
	FCMOVGT				Ra > 0	
	FCMOVGE				Ra ≥ 0	

	*/
	//Floating-Point Conditional Move Equal Zero
	void execFCMOVEQ(const helpers_JIT::OperateInstruction& op) {
		if (regs->readIntReg(op.ra) == 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			emit registerUpdated(op.rc, value.raw);
		}
	}

	// Register Not Equal to Zero
	void execFCMOVNE(const helpers_JIT::OperateInstruction& op) {
		if (regs->readIntReg(op.ra) != 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			emit registerUpdated(op.rc, value.raw);
		}
	}

	// Register Not Less Than to Zero
	void execFCMOVLT(const helpers_JIT::OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) < 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			emit registerUpdated(op.rc, value.raw);
		}
	}

	// Register Less Than or Equal to Zero
	void execFCMOVLE(const helpers_JIT::OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) <= 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			emit registerUpdated(op.rc, value.raw);
		}
	}

	// Register Greater than Zero
	void execFCMOVGT(const helpers_JIT::OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) > 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			emit registerUpdated(op.rc, value.raw);
		}
	}

	// Register Greater than or Equal to Zero
	void execFCMOVGE(const helpers_JIT::OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) >= 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			emit registerUpdated(op.rc, value.raw);
		}
	}



	/*
	FPCR access (MT_FPCR, MF_FPCR)
	*/
	//// Defined in Alpha AXP Architecture Reference Manual, Volume I, Section 4.10.5.
	//Writes an integer value from a general-purpose register (e.g., Rn) into the floating-point control register (FPCR).
	void execMT_FPCR(const helpers_JIT::OperateInstruction& inst) {
		fpRegs->setFpcr(regs->readIntReg(inst.ra));					// Safer future-proof way
		DEBUG_LOG("[FPCR] MT_FPCR set to " + QString("0x%1").arg(fpcrRegister.getRaw(), 0, 16));
	}

	//Reads the current FPCR value and stores it into a general-purpose register (e.g., Rn).
	// 
	//Move to Floating-Point Control Register
	void execMF_FPCR(const helpers_JIT::OperateInstruction& inst) {
		regs->writeIntReg(inst.rc, fpcrRegister.getRaw());
		DEBUG_LOG("[FPCR] MF_FPCR read as " + QString("0x%1").arg(fpcrRegister.getRaw(), 0, 16));
	}

	/*
	* Traps should handle
		Trap Type				Examples
		Divide-by-zero			e.g., during execDIVF, execDIVG
		Overflow				during MULF, ADDF with large results
		Underflow				small denormalized results
		Inexact					rounding loss during conversion (CVTxx ops)
		Invalid operation		0/0, inf - inf, NaN propagation
	
	*/
};

// ===========================================================================
// VectorExecutor
// ===========================================================================
// VectorExecutor.h
#ifndef VECTOREXECUTOR_H
#define VECTOREXECUTOR_H


#include <functional>

/**
 * @brief Handles Alpha AXP Vector (SIMD-style) instruction execution.
 *
 * Reference: Alpha Architecture Reference Manual, Version 6
 * - Vector extensions are implementation-dependent and minimal in base ASA.
 * - This executor serves custom SIMD-style instruction sets for extended Alpha emulation.
 */
class VectorExecutor : public AlphaExecutor {
	Q_OBJECT

signals:
	void trapRaised(const QString& reason);
	void illegalInstruction(quint64 instructionWord, quint64 pc);

public:
	explicit VectorExecutor(AlphaCPUInterface* cpuptr, SafeMemory* mem, RegisterBank* regs,FpRegisterBankcls* fpRegs, QObject* parent = nullptr)
		: AlphaExecutor(cpuptr, mem, regs, fpRegs, parent) {
		buildDispatchTable();
	}

	/**
	 * @brief Decodes and dispatches a vector instruction.
	 * The opcode and function field select the operation to invoke.
	 */
	void execute(quint32 instr) override {
		QMutexLocker locker(&mutex);
		const helpers_JIT::OperateInstruction op = helpers_JIT::decodeOperate(instr);
		auto handler = dispatchTable.value(op.function & 0x3F, nullptr);  // Only low 6 bits are relevant (ASA v6 §4.8.3)
		if (handler) {
			handler(op);
		}
		else {
			emit illegalInstruction(instr, alphaCPU->getPC()); // 🔥 Raise illegal instruction!
		}
		emit instructionExecuted(instr);
	}

	/**
	 * @brief Builds the function dispatch table for vector operations.
	 * These opcodes are hypothetical and aligned for extended Alpha-like instructions.
	 */
	void buildDispatchTable() override {
		dispatchTable[0x20] = [this](const helpers_JIT::OperateInstruction& op) { execVADD(op); }; // Vector Add
		dispatchTable[0x21] = [this](const helpers_JIT::OperateInstruction& op) { execVSUB(op); }; // Vector Subtract
		dispatchTable[0x22] = [this](const helpers_JIT::OperateInstruction& op) { execVAND(op); }; // Vector AND
		dispatchTable[0x23] = [this](const helpers_JIT::OperateInstruction& op) { execVOR(op); };  // Vector OR
		dispatchTable[0x24] = [this](const helpers_JIT::OperateInstruction& op) { execVXOR(op); }; // Vector XOR
		dispatchTable[0x25] = [this](const helpers_JIT::OperateInstruction& op) { execVMUL(op); }; // Vector Multiply
		dispatchTable[0x26] = [this](const helpers_JIT::OperateInstruction& op) { execVLD(op); };  // Vector Load
		dispatchTable[0x27] = [this](const helpers_JIT::OperateInstruction& op) { execVST(op); };  // Vector Store
	}

private:
	QHash<quint8, std::function<void(const helpers_JIT::OperateInstruction&)>> dispatchTable;
	QMutex mutex;

	// ----------------------------
	// Vector instruction handlers
	// ----------------------------

	void execVADD(const helpers_JIT::OperateInstruction& op) {
		const quint64 a = regs->readIntReg(op.ra);
		const quint64 b = regs->readIntReg(op.rb);
		const quint64 result = a + b;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execVSUB(const helpers_JIT::OperateInstruction& op) {
		const quint64 a = regs->readIntReg(op.ra);
		const quint64 b = regs->readIntReg(op.rb);
		const quint64 result = a - b;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execVAND(const helpers_JIT::OperateInstruction& op) {
		const quint64 a = regs->readIntReg(op.ra);
		const quint64 b = regs->readIntReg(op.rb);
		const quint64 result = a & b;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execVOR(const helpers_JIT::OperateInstruction& op) {
		const quint64 a = regs->readIntReg(op.ra);
		const quint64 b = regs->readIntReg(op.rb);
		const quint64 result = a | b;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execVXOR(const helpers_JIT::OperateInstruction& op) {
		const quint64 a = regs->readIntReg(op.ra);
		const quint64 b = regs->readIntReg(op.rb);
		const quint64 result = a ^ b;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execVMUL(const helpers_JIT::OperateInstruction& op) {
		const quint64 a = regs->readIntReg(op.ra);
		const quint64 b = regs->readIntReg(op.rb);
		const quint64 result = a * b;
		regs->writeIntReg(op.rc, result);
		emit registerUpdated(op.rc, result);
	}

	void execVLD(const helpers_JIT::OperateInstruction& op)
	{
		const quint64 base = regs->readIntReg(op.ra);
		const quint64 offset = decodeMemoryOffset(op, regs);
		const quint64 addr = base + offset;
		const quint64 data = memory->readUInt64(addr);
		regs->writeIntReg(op.rc, data);
		emit registerUpdated(op.rc, data);
	}
	//Vector Store
	void execVST(const helpers_JIT::OperateInstruction& op)
	{
		const quint64 base = regs->readIntReg(op.ra);
		const quint64 offset = decodeMemoryOffset(op, regs);
		const quint64 addr = base + offset;
		const quint64 data = regs->readIntReg(op.rc);
		memory->writeUInt64(addr, data);
		emit memoryAccessed(addr, data, /*isWrite=*/true);
	}


};

#endif // VECTOREXECUTOR_H


// ===========================================================================
// ControlExecutor
// ===========================================================================
#pragma once
#ifndef CONTROLEXECUTOR_H
#define CONTROLEXECUTOR_H


/**
 * @brief Executes control flow instructions such as branches, jumps, and returns.
 * Reference: Alpha AXP Architecture Reference Manual, Chapter 4.3, Table 4-3.
 */
class ControlExecutor : public AlphaExecutor {
	Q_OBJECT

signals:
	void trapRaised(const QString& reason);
	void illegalInstruction(quint64 instructionWord, quint64 pc);

public:
	explicit ControlExecutor(AlphaCPUInterface* cpuPtr, SafeMemory* mem, RegisterBank* regs, FpRegisterBankcls* fpRegs, QObject *parent = nullptr)
		: AlphaExecutor(cpuPtr, mem, regs,fpRegs, parent)
	{
		buildDispatchTable();
	}

	/**
	 * @brief Decodes and executes a control instruction.
	 */
	void execute(quint32 instr) override {
		QMutexLocker locker(&mutex);

		// Decode top 6 bits for opcode
		quint8 opcode = (instr >> 26) & 0x3F;

		auto handler = dispatchTable.value(opcode, nullptr);

		if (handler) {
			handler(instr); // 🔥 Execute instruction handler
		}
		else {
			emit illegalInstruction(instr, alphaCPU->getPC()); // 🔥 Illegal instruction handling
		}

		emit instructionExecuted(instr); // 🔥 Always emit executed signal
	}



	/**
	 * @brief Populates dispatch table for control opcodes (0x30–0x3F).
	 */
	void buildDispatchTable() override {
		dispatchTable[0x30] = [this](quint32 instr) { execBR(instr); };
		dispatchTable[0x34] = [this](quint32 instr) { execBSR(instr); };
		dispatchTable[0x39] = [this](quint32 instr) { execBEQ(instr); };
		dispatchTable[0x3E] = [this](quint32 instr) { execBGE(instr); };
		dispatchTable[0x3F] = [this](quint32 instr) { execBGT(instr); };
		dispatchTable[0x38] = [this](quint32 instr) { execBLBC(instr); };
		dispatchTable[0x3C] = [this](quint32 instr) { execBLBS(instr); };
		dispatchTable[0x3B] = [this](quint32 instr) { execBLE(instr); };
		dispatchTable[0x3A] = [this](quint32 instr) { execBLT(instr); };
		dispatchTable[0x3D] = [this](quint32 instr) { execBNE(instr); };
		dispatchTable[0x1F] = [this](quint32 instr) { execREI(instr); };
	}

private:
	QHash<quint8, std::function<void(quint32)>> dispatchTable;
	QMutex mutex;

	/**
	 * @brief Generic conditional branch executor
	 */
	void execConditionalBranch(quint32 instr, std::function<bool(qint64)> condition) {
		quint32 ra = (instr >> 21) & 0x1F;
		qint64 rav = static_cast<qint64>(regs->readIntReg(ra));
		qint32 disp = static_cast<qint32>(instr << 11) >> 11;  // Sign-extend 21-bit disp
		quint64 target = alphaCPU->getPC() + static_cast<qint64>(disp << 2);
		if (condition(rav)) {
			alphaCPU->setPC(target);
		}
		TraceManager::logDebug(QString("Control Executor::execConditionalBranch: %1").arg(static_cast<int>(target)));

	}

	// === Branch Implementations ===

	void execBR(quint32 instr) {
		qint32 disp = static_cast<qint32>(instr << 11) >> 11;
		quint64 newPC = alphaCPU->getPC() + static_cast<qint64>(disp << 2);
		regs->writeIntReg((instr >> 21) & 0x1F, alphaCPU->getPC()); // Save updated PC
		alphaCPU->setPC(newPC);
		TraceManager::logDebug(QString("Control Executor::execBR: Instr/NewPC: %1  : %2")
			.arg(instr)
			.arg(newPC));
	}

	void execBSR(quint32 instr) {
		execBR(instr); // Same as BR, but hint is for subroutine
		TraceManager::logDebug(QString("Control Executor::execBSR: Instr: %1  ")
			.arg(instr));
	}

	void execBEQ(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return v == 0; });
	}

	void execBNE(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return v != 0; });
	}

	void execBGE(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return v >= 0; });
	}

	void execBGT(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return v > 0; });
	}

	void execBLE(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return v <= 0; });
	}

	void execBLT(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return v < 0; });
	}

	void execBLBC(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return (v & 1) == 0; });
	}

	void execBLBS(quint32 instr) {
		execConditionalBranch(instr, [](qint64 v) { return (v & 1) != 0; });
	}

	//Reference: Alpha System Architecture Vol I §6.6.2, “Return from Exception or Interrupt (REI)”
	//The REI instruction performs a return from exception by restoring CPU state from the trap stack —
	void execREI(quint32 /*instr*/) {
		qDebug() << "ControlExecutor: Executing REI (Return from Exception)";
		alphaCPU->returnFromTrap();
	}


};

#endif // CONTROLEXECUTOR_H

