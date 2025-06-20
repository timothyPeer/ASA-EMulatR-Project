#pragma once

#include <QObject>
#include <QMutex>
#include "../AEC/RegisterBank.h"
#include "../AEC/FPCRContext.h"
#include "../AEC/fpRegisterBankCls.h"
#include "IExecutionContext.h"
#include "AlphaMemorySystem.h"
#include "decodeOperate.h"
#include "../AEC/dt_gfloat.h"
#include "../AESH/helpers.h"
#include "../AESH/GlobalMacro.h"


class FloatingPointExecutor : public QObject {
	Q_OBJECT

	Q_DISABLE_COPY(FloatingPointExecutor)

		IExecutionContext* ctx;


public:
    public slots:
    void setFPCR(FpcrRegister fpcr);
    void enableFloatingPoint(bool enabled);
    void resetState();

	FloatingPointExecutor(IExecutionContext* ctx,
		AlphaMemorySystem* memSystem_,
		QObject* parent)
		: ctx(ctx),
		memSystem(memSystem_),
		QObject(parent) {
	}
/*
	signals are emitted via IExecutionContext.  Simpler execution from AlphaCPU to AlphaCPU
*/
signals:
// 	void trapRaised(helpers_JIT::TrapType type);
// 	void illegalInstruction(quint64 instr, quint64 pc);

	// Floating Point Add
	void execADDF(const OperateInstruction& op) {
		double tmp_a = ctx->readFpReg(op.ra);
		dt_gfloat a = static_cast<dt_gfloat>(tmp_a);
		dt_gfloat b = fpRegs->readFpReg(op.rb);

		dt_gfloat result = a + b; // Correct operator

		fpRegs->writeFpReg(op.rc, result);
		ctx->notifyFpRegisterUpdated(op.rc, result.raw);
	}

	// Floating Point Subtract
	void execSUBF(const OperateInstruction& op) {
		dt_gfloat a = fpRegs->readFpReg(op.ra);  // Fsrc1
		dt_gfloat b = fpRegs->readFpReg(op.rb);  // Fsrc2

		double diff = a.toDouble() - b.toDouble(); // IEEE 64-bit subtraction

		// Apply rounding using FPCR context
		double rounded = dt_gfloat::applyRounding(diff, fpRegs->getFpcrContext());

		dt_gfloat result = dt_gfloat::fromDouble(rounded);

		fpRegs->writeFpReg(op.rc, result); // Save result
		ctx->notifyFpRegisterUpdated(op.rc, result.raw); // Notify
	}

	//Multiply two floating-point registers
	void execMULF(const OperateInstruction& inst) {
		dt_gfloat a = fpRegs->readFpReg(inst.ra); // Fsrc1 - Read Fsrc1 and Fsrc2
		dt_gfloat b = fpRegs->readFpReg(inst.rb); // Fsrc2 - Read Fsrc1 and Fsrc2

		double product = a.toDouble() * b.toDouble(); // IEEE double multiplication

		// Apply rounding according to FPCR context
		double rounded = dt_gfloat::applyRounding(product, fpRegs->getFpcrContext());

		dt_gfloat result = dt_gfloat::fromDouble(rounded); // Create final result

		fpRegs->writeFpReg(inst.rc, result); // Save to Fdst
		ctx->notifyFpRegisterUpdated(inst.rc, result.raw);
	}

	// Divide two floating-point registers
	void execDIVF(const OperateInstruction& inst) {
		dt_gfloat numerator = fpRegs->readFpReg(inst.ra); // Fsrc1
		dt_gfloat denominator = fpRegs->readFpReg(inst.rb); // Fsrc2

		FPCRContext& ctx_ = fpRegs->getFpcrContext(); // Grab FPCR context

		if (denominator.isZero()) {
			if (ctx_.trapDivZero()) {
				ctx_.setStickyDivZero(); // Set sticky bit
				ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::DivideByZero_fp));
				return;
			}
		}

		double quotient = numerator.toDouble() / denominator.toDouble(); // IEEE 64-bit division
		double rounded = dt_gfloat::applyRounding(quotient, ctx_);

		dt_gfloat result = dt_gfloat::fromDouble(rounded);

		fpRegs->writeFpReg(inst.rc, result);
		ctx->notifyFpRegisterUpdated(inst.rc, result.raw);
	}


	/*
	IEEE conversion and trap-sensitive ops (CVTQS, CVTTQ)
	*/
	//Convert Quadword Integer to S_Float
	void execCVTQS(const OperateInstruction& op) {
		qint64 intVal = regs->readIntReg(op.ra); // Read source integer

		float sVal = static_cast<float>(intVal); // Convert to 32-bit float

		// Promote to double for dt_gfloat construction
		double promoted = static_cast<double>(sVal);

		// Apply FPCR rounding before storing
		double rounded = dt_gfloat::applyRounding(promoted, fpRegs->getFpcrContext());

		dt_gfloat result = dt_gfloat::fromDouble(rounded);

		fpRegs->writeFpReg(op.rc, result);
		ctx->notifyFpRegisterUpdated(op.rc, result.raw);
	}

	//Convert S_Float (32-bit float) → Quadword Integer (signed 64-bit integer)
	void execCVTTQ(const OperateInstruction& op) {
		dt_gfloat val = fpRegs->readFpReg(op.ra); // Read Fsrc (S_Float promoted)

		FPCRContext& ctx_ = fpRegs->getFpcrContext(); // Grab FPCR context

		// Convert to integer according to Alpha rounding rules
		qint64 result = val.toInt64(ctx_);

		regs->writeIntReg(op.rc, static_cast<quint64>(result)); // Save to integer register
		ctx->notifyFpRegisterUpdated(op.rc, static_cast<quint64>(result));
	}

	/*
	Sign manipulation (CPYS, CPYSN, CPYSE)
	*/

	// Copy Sign (Floating-Point Operate Format)
	void execCPYS(const OperateInstruction& op) {
		dt_gfloat mag = fpRegs->readFpReg(op.ra);     // Magnitude source
		dt_gfloat signSrc = fpRegs->readFpReg(op.rb); // Sign source

		quint64 rawResult = mag.raw & ~dt_gfloat::SIGN_MASK; // Clear sign bit
		if (signSrc.sign()) {
			rawResult |= dt_gfloat::SIGN_MASK; // Set sign if negative
		}

		dt_gfloat result(rawResult); // Construct properly
		fpRegs->writeFpReg(op.rc, result);
		ctx->notifyFpRegisterUpdated(op.rc, result.raw); // Always use .raw explicitly
	}

	// Copy Sign and Exponent (Floating-Point Format)
	void execCPYSE(const OperateInstruction& op) {
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
		ctx->notifyFpRegisterUpdated(op.rc, result.raw);
	}

	// Copy Sign Negate - Floating Point Format
	void execCPYSN(const OperateInstruction& op) {
		dt_gfloat mag = fpRegs->readFpReg(op.ra);     // Magnitude source (Ra)
		dt_gfloat signSrc = fpRegs->readFpReg(op.rb); // Sign source (Rb)

		quint64 rawResult = mag.raw & ~dt_gfloat::SIGN_MASK; // Clear magnitude sign bit

		bool signBit = !signSrc.sign(); // Invert the sign source

		if (signBit) {
			rawResult |= dt_gfloat::SIGN_MASK; // Set sign bit if negative
		}

		dt_gfloat result(rawResult);
		fpRegs->writeFpReg(op.rc, result);
		ctx->notifyFpRegisterUpdated(op.rc, result.raw);
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
	void execFCMOVEQ(const OperateInstruction& op) {
		if (regs->readIntReg(op.ra) == 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			ctx->notifyFpRegisterUpdated(op.rc, value.raw);
		}
	}

	// Register Not Equal to Zero
	void execFCMOVNE(const OperateInstruction& op) {
		if (regs->readIntReg(op.ra) != 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			ctx->notifyFpRegisterUpdated(op.rc, value.raw);
		}
	}

	// Register Not Less Than to Zero
	void execFCMOVLT(const OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) < 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			ctx->notifyFpRegisterUpdated(op.rc, value.raw);
		}
	}

	// Register Less Than or Equal to Zero
	void execFCMOVLE(const OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) <= 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			ctx->notifyFpRegisterUpdated(op.rc, value.raw);
		}
	}

	// Register Greater than Zero
	void execFCMOVGT(const OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) > 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			ctx->notifyFpRegisterUpdated(op.rc, value.raw);
		}
	}

	// Register Greater than or Equal to Zero
	void execFCMOVGE(const OperateInstruction& op) {
		if (static_cast<qint64>(regs->readIntReg(op.ra)) >= 0) {
			dt_gfloat value = fpRegs->readFpReg(op.rb);
			fpRegs->writeFpReg(op.rc, value);
			ctx->notifyFpRegisterUpdated(op.rc, value.raw);
		}
	}



	/*
	FPCR access (MT_FPCR, MF_FPCR)
	*/
	//// Defined in Alpha AXP Architecture Reference Manual, Volume I, Section 4.10.5.
	//Writes an integer value from a general-purpose register (e.g., Rn) into the floating-point control register (FPCR).
	void execMT_FPCR(const OperateInstruction& inst) {
		fpRegs->setFpcr(regs->readIntReg(inst.ra));					// Safer future-proof way
		DEBUG_LOG("[FPCR] MT_FPCR set to " + QString("0x%1").arg(fpcrRegister.getRaw(), 0, 16));
	}

	//Reads the current FPCR value and stores it into a general-purpose register (e.g., Rn).
	// 
	//Move to Floating-Point Control Register
	void execMF_FPCR(const OperateInstruction& inst) {
		regs->writeIntReg(inst.rc, fpcrRegister.getRaw());
		DEBUG_LOG("[FPCR] MF_FPCR read as " + QString("0x%1").arg(fpcrRegister.getRaw(), 0, 16));
	}

private:
	IExecutionContext* ctx;
	RegisterBank* regs;
	FpRegisterBankcls* fpRegs;
	AlphaMemorySystem* memSystem;
	AlphaCPU* alphaCPU;
	QMutex mutex;
	FpcrRegister fpcrRegister;

};

