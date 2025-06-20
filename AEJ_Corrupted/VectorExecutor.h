#pragma once

#include <QObject>
#include "IExecutionContext.h"
#include "alphamemorysystem.h"
#include "RegisterBank.h"
#include "fpRegisterBankCls.h"

#include "VectorRegisterBank.h"

class VectorExecutor : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(VectorExecutor)

public:
	VectorExecutor(IExecutionContext* ctx,
		AlphaMemorySystem* memSystem_,
		AlphaCPU* alphaCPU_,
		RegisterBank* regs_,
		FpRegisterBankcls* fpRegs_,
		QObject* parent)
		: QObject(parent),
		ctx(ctx),
		memSystem(memSystem_),
		alphaCPU(alphaCPU_),
		regs(regs_),
		fpRegs(fpRegs_),
		vecRegs(nullptr) // You may inject/set this later if needed
	{
	}

	void execVADD(const OperateInstruction& op);
	void execVSUB(const OperateInstruction& op);
	void execVAND(const OperateInstruction& op);
	void execVOR(const OperateInstruction& op);
	void execVXOR(const OperateInstruction& op);
	void execVMUL(const OperateInstruction& op);
	void execVLD(const OperateInstruction& op);
	void execVST(const OperateInstruction& op);

	// Load/Store byte/word
	void execLDBU(const OperateInstruction& op);
	void execLDWU(const OperateInstruction& op);
	void execSTB(const OperateInstruction& op);
	void execSTW(const OperateInstruction& op);

	// Sign-extension operations
	void execSEXTW(const OperateInstruction& op);
	void execSEXTBU(const OperateInstruction& op);

	// Motion Video Instructions
	void execMINUB8(const OperateInstruction& op);
	void execMINSB8(const OperateInstruction& op);
	void execMAXSB8(const OperateInstruction& op);
	void execMAXUB8(const OperateInstruction& op);
	void execPKLB(const OperateInstruction& op);
	void execPKWB(const OperateInstruction& op);
	void execUNPKBL(const OperateInstruction& op);
	void execUNPKBW(const OperateInstruction& op);
	void execPERR(const OperateInstruction& op);

signals:
	void trapRaised(helpers_JIT::TrapType trap);
private:
	IExecutionContext* ctx;
	RegisterBank* regs;
	FpRegisterBankcls* fpRegs;
	AlphaMemorySystem* memSystem;
	AlphaCPU* alphaCPU;
	VectorRegisterBank* vecRegs; // Set externally if needed
};
