#pragma once
#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <functional>
#include "helpers.h"
#include "TraceManager.h"
#include "IExecutionContext.h"
#include "alphamemorysystem.h"
// #include "fpRegisterBankCls.h"
// #include "RegisterBank.h"
// #include "GlobalMacro.h"



struct BranchInstruction {
	quint8 opcode;         ///< Primary opcode (bits 31:26)
	quint8 ra;             ///< Source register (bits 25:21)
	qint32 displacement;   ///< Signed 21-bit immediate, left-shifted by 2
};

/**
 * @brief Decodes a 32-bit Alpha branch instruction
 * @param instruction 32-bit instruction word
 * @return A decoded BranchInstruction structure
 *
 * Reference: Alpha AXP Architecture Handbook, Vol. I, §4.3.2
 */
inline BranchInstruction decodeBranch(quint32 instruction) {
	BranchInstruction br;

	br.opcode = static_cast<quint8>((instruction >> 26) & 0x3F);  // bits 31:26
	br.ra = static_cast<quint8>((instruction >> 21) & 0x1F);  // bits 25:21

	// bits 20:0 = 21-bit signed displacement, sign-extend and left-shifted by 2
	qint32 disp = static_cast<qint32>(instruction << 11) >> 11;  // sign-extend 21 bits
	br.displacement = disp;

	return br;
}

class ControlExecutor : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(ControlExecutor)

/*
	signals are emitted via IExecutionContext.  Simpler execution from AlphaCPU to AlphaCPU
*/
signals:
// 	void trapRaised(const QString& reason);
// 	void illegalInstruction(quint64 instructionWord, quint64 pc);

public:
	explicit ControlExecutor(IExecutionContext* ctx,
		AlphaMemorySystem* memSystem_,
		QObject* parent)
		: ctx(ctx),
		memSystem(memSystem_),
		QObject(parent) {}

	void execConditionalBranch(const BranchInstruction& br, std::function<bool(qint64)> condition) {
		qint64 raVal = static_cast<qint64>(ctx->readIntReg(br.ra));
		quint64 target = ctx->getPC() + static_cast<qint64>(br.displacement << 2);

		if (condition(raVal)) {
			ctx->setPC(target);
		}

		TRACE_LOG(QString("ControlExecutor::execConditionalBranch to PC=0x%1").arg(target, 8, 16, QChar('0')));
	}

	void execBR(const BranchInstruction& br) {
		quint64 newPC = ctx->getPC() + static_cast<qint64>(br.displacement << 2);
		ctx->writeIntReg(br.ra, ctx->getPC());
		ctx->setPC(newPC);
	}

	void execBSR(const BranchInstruction& br) {
		execBR(br);
	}

	void execBEQ(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return v == 0; });
	}

	void execBNE(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return v != 0; });
	}

	void execBGE(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return v >= 0; });
	}

	void execBGT(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return v > 0; });
	}

	void execBLE(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return v <= 0; });
	}

	void execBLT(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return v < 0; });
	}

	void execBLBC(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return (v & 1) == 0; });
	}

	void execBLBS(const BranchInstruction& br) {
		execConditionalBranch(br, [](qint64 v) { return (v & 1) != 0; });
	}

	void execREI() {
		qDebug() << "ControlExecutor: Executing REI (Return from Exception)";
		ctx->notifyReturnFromTrap();
	}


// 	void execute(quint32 instr) override {
// 		QMutexLocker locker(&mutex);
// 
// 		quint8 opcode = (instr >> 26) & 0x3F;
// 		auto handler = dispatchTable.value(opcode, nullptr);
// 
// 		if (handler) {
// 			handler(instr);
// 		}
// 		else {
// 			emit illegalInstruction(instr, alphaCPU->getPC());
// 		}
// 
// 		emit instructionExecuted(instr);
// 	}

// 	void buildDispatchTable()  {
// 		dispatchTable[0x30] = [this](quint32 instr) { execBR(decodeBranch(instr)); };
// 		dispatchTable[0x34] = [this](quint32 instr) { execBSR(decodeBranch(instr)); };
// 		dispatchTable[0x39] = [this](quint32 instr) { execBEQ(decodeBranch(instr)); };
// 		dispatchTable[0x3E] = [this](quint32 instr) { execBGE(decodeBranch(instr)); };
// 		dispatchTable[0x3F] = [this](quint32 instr) { execBGT(decodeBranch(instr)); };
// 		dispatchTable[0x38] = [this](quint32 instr) { execBLBC(decodeBranch(instr)); };
// 		dispatchTable[0x3C] = [this](quint32 instr) { execBLBS(decodeBranch(instr)); };
// 		dispatchTable[0x3B] = [this](quint32 instr) { execBLE(decodeBranch(instr)); };
// 		dispatchTable[0x3A] = [this](quint32 instr) { execBLT(decodeBranch(instr)); };
// 		dispatchTable[0x3D] = [this](quint32 instr) { execBNE(decodeBranch(instr)); };
// 		dispatchTable[0x1F] = [this](quint32 instr) { execREI(); };
// 	}

private:
	IExecutionContext* ctx;
	AlphaMemorySystem* memSystem;
	AlphaCPU* alphaCPU;
	

	
};
