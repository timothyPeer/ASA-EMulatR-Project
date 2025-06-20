// VectorExecutorExtensions.cpp

#include "helpers.h"
#include <cstring>
#include <QObject>
#include <QMetaObject>
#include "vectorExecutor.h"


class VectorExecutorPrivate {
	Q_DECLARE_PUBLIC(VectorExecutor)
public:
	VectorExecutorPrivate(VectorExecutor*);
	~VectorExecutorPrivate();
	// fields:
	AlphaCPUInterface* cpu;
	AlphaMemorySystem* memory;
	RegisterBank* regs;
	FpRegisterBankcls* fpRegs;
	VectorExecutor* q_ptr;  // via Q_DECLARE_PUBLIC
	// methods:






// Core vector operations
void execVLD(const helpers_JIT::OperateInstruction& op) {
	const quint64 base = regs->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, regs);
	const quint64 addr = base + offset;
	quint64 data = 0;
	if (!memory->readVirtualMemory(cpu, addr, &data, 8)) return;
	regs->writeIntReg(op.rc, data);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, data);
}

void execVST(const helpers_JIT::OperateInstruction& op) {
	const quint64 base = regs->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, regs);
	const quint64 addr = base + offset;
	quint64 data = regs->readIntReg(op.rc);
	if (!memory->writeVirtualMemory(cpu, addr, &data, 8)) return;
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->memoryAccessed(addr, true, 8);
}

void execVADD(const helpers_JIT::OperateInstruction& op) {
	const quint64 a = regs->readIntReg(op.ra);
	const quint64 b = regs->readIntReg(op.rb);
	const quint64 result = a + b;
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

void execVSUB(const helpers_JIT::OperateInstruction& op) {
	const quint64 a = regs->readIntReg(op.ra);
	const quint64 b = regs->readIntReg(op.rb);
	const quint64 result = a - b;
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

void execVAND(const helpers_JIT::OperateInstruction& op) {
	const quint64 a = regs->readIntReg(op.ra);
	const quint64 b = regs->readIntReg(op.rb);
	const quint64 result = a & b;
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

void execVOR(const helpers_JIT::OperateInstruction& op) {
	const quint64 a = regs->readIntReg(op.ra);
	const quint64 b = regs->readIntReg(op.rb);
	const quint64 result = a | b;
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

void execVXOR(const helpers_JIT::OperateInstruction& op) {
	const quint64 a = regs->readIntReg(op.ra);
	const quint64 b = regs->readIntReg(op.rb);
	const quint64 result = a ^ b;
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

void execVMUL(const helpers_JIT::OperateInstruction& op) {
	const quint64 a = regs->readIntReg(op.ra);
	const quint64 b = regs->readIntReg(op.rb);
	const quint64 result = a * b;
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

// BWX extensions
void execLDBU(const helpers_JIT::OperateInstruction& op) {
	const quint64 base = regs->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, regs);
	const quint64 addr = base + offset;
	quint8 val = 0;
	if (!memory->readVirtualMemory(cpu, addr, &val, 1)) return;
	regs->writeIntReg(op.rc, val);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, val);
}

void execLDWU(const helpers_JIT::OperateInstruction& op) {
	const quint64 base = regs->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, regs);
	const quint64 addr = base + offset;
	quint16 val = 0;
	if (!memory->readVirtualMemory(cpu, addr, &val, 2)) return;
	regs->writeIntReg(op.rc, val);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, val);
}

void execSTB(const helpers_JIT::OperateInstruction& op) {
	const quint64 base = regs->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, regs);
	const quint64 addr = base + offset;
	quint8 val = static_cast<quint8>(regs->readIntReg(op.rc));
	if (!memory->writeVirtualMemory(cpu, addr, &val, 1)) return;
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->memoryAccessed(addr, true, 1);
}

void execSTW(const helpers_JIT::OperateInstruction& op) {
	const quint64 base = regs->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, regs);
	const quint64 addr = base + offset;
	quint16 val = static_cast<quint16>(regs->readIntReg(op.rc));
	if (!memory->writeVirtualMemory(cpu, addr, &val, 2)) return;
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->memoryAccessed(addr, true, 2);
}

void execSEXTW(const helpers_JIT::OperateInstruction& op) {
	quint32 w = static_cast<quint32>(regs->readIntReg(op.ra));
	qint32 sw = static_cast<qint32>(w);
	regs->writeIntReg(op.rc, static_cast<quint64>(static_cast<qint64>(sw)));
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, sw);
}

void execSEXTBU(const helpers_JIT::OperateInstruction& op) {
	quint8 b = static_cast<quint8>(regs->readIntReg(op.ra));
	qint8 sb = static_cast<qint8>(b);
	regs->writeIntReg(op.rc, static_cast<quint64>(static_cast<qint64>(sb)));
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, sb);
}

// MVI extensions
void execMAXSB8(const helpers_JIT::OperateInstruction& op) {
	quint64 a = regs->readIntReg(op.ra);
	quint64 b = regs->readIntReg(op.rb);
	quint64 result = 0;
	for (int i = 0; i < 8; ++i) {
		qint8 va = (a >> (i * 8)) & 0xFF;
		qint8 vb = (b >> (i * 8)) & 0xFF;
		qint8 vr = va > vb ? va : vb;
		result |= (static_cast<quint64>(static_cast<uint8_t>(vr)) << (i * 8));
	}
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

void execMINUB8(const helpers_JIT::OperateInstruction& op) {
	quint64 a = regs->readIntReg(op.ra);
	quint64 b = regs->readIntReg(op.rb);
	quint64 result = 0;
	for (int i = 0; i < 8; ++i) {
		quint8 va = (a >> (i * 8)) & 0xFF;
		quint8 vb = (b >> (i * 8)) & 0xFF;
		quint8 vr = va < vb ? va : vb;
		result |= (static_cast<quint64>(vr) << (i * 8));
	}
	regs->writeIntReg(op.rc, result);
	Q_EMIT reinterpret_cast<VectorExecutor*>(qobject_cast<QObject*>(cpu))->registerUpdated(op.rc, result);
}

// TODO: implement MAXUB8, MAXSW4, MINSW4, MINUW4, PKLB, PKWB, UNPKBL, UNPKBW, PERR

};

VectorExecutorPrivate::VectorExecutorPrivate(VectorExecutor*)
{

}

VectorExecutorPrivate::~VectorExecutorPrivate()
{

}
