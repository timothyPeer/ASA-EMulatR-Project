#include "VectorExecutor.h"
#include "VectorExecutor_p.h"
#include <QDebug>

VectorExecutor::VectorExecutor(AlphaCPUInterface* cpu,
	AlphaMemorySystem* memory,
	RegisterBank* regs,
	FpRegisterBankcls* fpRegs,
	QObject* parent)
	: QObject(parent)
	, d_ptr(new VectorExecutorPrivate)
{
	Q_D(VectorExecutor);
	d->cpu = cpu;
	d->memory = memory;
	d->regs = regs;
	d->fpRegs = fpRegs;
}

VectorExecutor::~VectorExecutor() = default;

// Forward all exec calls to the private implementation
void VectorExecutor::execVLD(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVLD(op); }
void VectorExecutor::execVST(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVST(op); }
void VectorExecutor::execVADD(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVADD(op); }
void VectorExecutor::execVSUB(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVSUB(op); }
void VectorExecutor::execVAND(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVAND(op); }
void VectorExecutor::execVOR(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVOR(op); }
void VectorExecutor::execVXOR(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVXOR(op); }
void VectorExecutor::execVMUL(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execVMUL(op); }

void VectorExecutor::execLDBU(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execLDBU(op); }
void VectorExecutor::execLDWU(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execLDWU(op); }
void VectorExecutor::execSTB(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execSTB(op); }
void VectorExecutor::execSTW(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execSTW(op); }
void VectorExecutor::execSEXTW(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execSEXTW(op); }
void VectorExecutor::execSEXTBU(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execSEXTBU(op); }

void VectorExecutor::execMAXSB8(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execMAXSB8(op); }
void VectorExecutor::execMINUB8(const helpers_JIT::OperateInstruction& op) { Q_D(VectorExecutor); d->execMINUB8(op); }
// ... forward other MVI ops similarly
