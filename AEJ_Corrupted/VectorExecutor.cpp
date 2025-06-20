#include "VectorExecutor.h"
#include "decodeOperate.h"
#include "decodeMemoryOffset.h"
#include "..\AESH\TraceManager.h"
#include "helpers.h"
#include "GlobalMacro.h"

void VectorExecutor::execVADD(const OperateInstruction& op)
{
	// Fetch operands
	const quint64 a = ctx->readIntReg(op.ra);
	const quint64 b = ctx->readIntReg(op.rb);
	// Perform vector-add (element-wise in real SIMD; here scalar)
	const quint64 result = a + b;
	// Write back
	ctx->writeIntReg(op.rc, result);	// use the IExecutionContext
	// Notify any UI/tracer
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execVSUB(const OperateInstruction& op)
{
	const quint64 a = ctx->readIntReg(op.ra);
	const quint64 b = ctx->readIntReg(op.rb);
	const quint64 result = a - b;
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execVAND(const OperateInstruction& op)
{
	const quint64 a = ctx->readIntReg(op.ra);
	const quint64 b = ctx->readIntReg(op.rb);
	const quint64 result = a & b;
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execVOR(const OperateInstruction& op)
{
	const quint64 a = ctx->readIntReg(op.ra);
	const quint64 b = ctx->readIntReg(op.rb);
	const quint64 result = a | b;
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execVXOR(const OperateInstruction& op)
{
	const quint64 a = ctx->readIntReg(op.ra);
	const quint64 b = ctx->readIntReg(op.rb);
	const quint64 result = a ^ b;
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execVMUL(const OperateInstruction& op)
{
	const quint64 a = ctx->readIntReg(op.ra);
	const quint64 b = ctx->readIntReg(op.rb);
	const quint64 result = a * b;
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execVLD(const OperateInstruction& op)
{
	// 1) compute the virtual address
	const quint64 base = ctx->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, ctx);
	const quint64 addr = base + offset;

	// 2) fetch 64-bits from the virtual address, trapping on fault
	quint64 data = 0;
	bool ok = memSystem->readVirtualMemory(
		alphaCPU,    // for reporting faults
		addr,        // virtual address
		data,       // out-param receives the 8-byte value
		8            // size in bytes
	);
	if (!ok) {
		DEBUG_LOG(QString("[VectorExecutor] VLD fault @ VA=0x%1").arg(addr, 8, 16, QChar('0')));
		return;
	}

	
	// 3) write it into the integer register file
	//ctx->writeIntReg(op.rc, data);
	vecRegs->writeVecLane(op.rc, /*lane=*/0, data);  // SIMD model
	
	// 4) notify any watchers that reg op.rc changed
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, data);
}

void VectorExecutor::execVST(const OperateInstruction& op)
{
	// 1) compute the virtual address
	const quint64 base = ctx->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, ctx);
	const quint64 addr = base + offset;

	// 2) get the value to store from the vector register (here treated as 64-bit)
	const quint64 data = regs->readIntReg(op.rc);

	// 3) perform the virtual‐memory store (8 bytes)
	bool ok = memSystem->writeVirtualMemory(
		alphaCPU,    // for exception reporting
		addr,        // virtual address
		data,       // pointer to 8-byte value
		8            // size in bytes
	);
	if (!ok) {
		// on a page-fault or MMIO trap the memory system already signaled
		return;
	}

	// 4) notify any watchers that memory was written
	ctx->notifyMemoryAccessed( addr, /*isWrite=*/true, /*size=*/8);
}

void VectorExecutor::execLDBU(const OperateInstruction& op)
{
	quint64 addr = regs->readIntReg(op.ra) + regs->readIntReg(op.rb);
	quint64 temp = 0;
	if (!memSystem->readVirtualMemory(alphaCPU, addr, temp, 1)) {
		ctx->notifyTrapRaised(TrapType::MMUAccessFault);
		return;
	}
	quint8 value = static_cast<quint8>(temp);
	regs->writeIntReg(op.rc, value);
	ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, static_cast<quint64>(value));
}

void VectorExecutor::execLDWU(const OperateInstruction& op)
{
	const quint64 base = ctx->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, ctx);
	const quint64 addr = base + offset;
	quint32 half = 0;
	if (!memSystem->readVirtualMemory(alphaCPU, addr, &half, 2)) return;
	ctx->writeIntReg(op.rc, static_cast<quint64>(half));
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, static_cast<quint64>(half));
}

void VectorExecutor::execSTB(const OperateInstruction& op)
{
	const quint64 base = ctx->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, ctx);
	const quint64 addr = base + offset;
	quint8 val = static_cast<quint8>(ctx->readIntReg(op.rc));
	if (!memSystem->writeVirtualMemory(alphaCPU, addr, val, 1)) return;
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, val);
}

void VectorExecutor::execSTW(const OperateInstruction& op)
{
	const quint64 base = ctx->readIntReg(op.ra);
	const quint64 offset = decodeMemoryOffset(op, ctx);      // <-- pass ctx, not ctx->readFpReg()
	const quint64 addr = base + offset;
	quint16       val = static_cast<quint16>(ctx->readIntReg(op.rc));
	// pass the 16-bit value, not &val
	if (!memSystem->writeVirtualMemory(alphaCPU, addr, val, 2))
		return;
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, val);
}

void VectorExecutor::execSEXTW(const OperateInstruction& op)
{
	quint32 w = static_cast<quint32>(ctx->readIntReg(op.ra));
	qint32 sw = static_cast<qint32>(w);
	ctx->writeIntReg(op.rc, static_cast<quint64>(static_cast<qint64>(sw)));
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, sw);
}

void VectorExecutor::execSEXTBU(const OperateInstruction& op)
{
	quint8 b = static_cast<quint8>(ctx->readIntReg(op.ra));
	qint8 sb = static_cast<qint8>(b);
	ctx->writeIntReg(op.rc, static_cast<quint64>(static_cast<qint64>(sb)));
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, static_cast<quint64>(static_cast<qint64>(sb)));
}

void VectorExecutor::execMAXSB8(const OperateInstruction& op)
{
	// placeholder: perform signed byte-wise max on 8 bytes
	quint64 a = ctx->readIntReg(op.ra);
	quint64 b = ctx->readIntReg(op.rb);
	quint64 result = 0;
	for (int i = 0; i < 8; ++i) {
		qint8 va = (a >> (i * 8)) & 0xFF;
		qint8 vb = (b >> (i * 8)) & 0xFF;
		qint8 vr = va > vb ? va : vb;
		result |= (static_cast<quint64>(static_cast<uint8_t>(vr)) << (i * 8));
	}
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execMINUB8(const OperateInstruction& op)
{
	// example unsigned byte-wise min
	quint64 a = ctx->readIntReg(op.ra);
	quint64 b = ctx->readIntReg(op.rb);
	quint64 result = 0;
	for (int i = 0; i < 8; ++i) {
		quint8 va = (a >> (i * 8)) & 0xFF;
		quint8 vb = (b >> (i * 8)) & 0xFF;
		quint8 vr = va < vb ? va : vb;
		result |= (static_cast<quint64>(vr) << (i * 8));
	}
	ctx->writeIntReg(op.rc, result);
	ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
}

void VectorExecutor::execMINSB8(const OperateInstruction& op)
{
	uint64_t a = ctx->readIntReg(op.ra);
	uint64_t b = ctx->readIntReg(op.rb);
	uint64_t r = 0;
	for (int lane = 0; lane < 8; ++lane) {
		int8_t va = static_cast<int8_t>((a >> (lane * 8)) & 0xFF);
		int8_t vb = static_cast<int8_t>((b >> (lane * 8)) & 0xFF);
		int8_t vr = std::min(va, vb);
		r |= (uint64_t)(static_cast<uint8_t>(vr)) << (lane * 8);
	}
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

void VectorExecutor::execMAXUB8(const OperateInstruction& op)
{
	uint64_t a = ctx->readIntReg(op.ra);
	uint64_t b = ctx->readIntReg(op.rb);
	uint64_t r = 0;
	for (int lane = 0; lane < 8; ++lane) {
		uint8_t va = static_cast<uint8_t>((a >> (lane * 8)) & 0xFF);
		uint8_t vb = static_cast<uint8_t>((b >> (lane * 8)) & 0xFF);
		uint8_t vr = std::max(va, vb);
		r |= (uint64_t)vr << (lane * 8);
	}
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

void VectorExecutor::execPKLB(const OperateInstruction& op)
{
	// Pack the low byte of RB into bits[15:8], low byte of RA into bits[7:0]
	uint8_t lowA = static_cast<uint8_t>(ctx->readIntReg(op.ra) & 0xFF);
	uint8_t lowB = static_cast<uint8_t>(ctx->readIntReg(op.rb) & 0xFF);
	uint64_t r = (uint64_t(lowB) << 8) | uint64_t(lowA);
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

void VectorExecutor::execPKWB(const OperateInstruction& op)
{
	// Pack the low 16-bit of RB into bits[31:16], low 16-bit of RA into bits[15:0]
	uint64_t a = ctx->readIntReg(op.ra) & 0xFFFF;
	uint64_t b = ctx->readIntReg(op.rb) & 0xFFFF;
	uint64_t r = (b << 16) | a;
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

void VectorExecutor::execUNPKBL(const OperateInstruction& op)
{
	// Unpack each 8-bit lane into 16-bit words: [ byte0 → bits[15:0], byte1→ bits[31:16] ]
	uint64_t v = ctx->readIntReg(op.ra);
	uint64_t r = 0;
	// lane 0 → word0, lane1 → word1, etc.
	for (int lane = 0; lane < 4; ++lane) {
		uint8_t byte = static_cast<uint8_t>((v >> (lane * 8)) & 0xFF);
		r |= uint64_t(byte) << (lane * 16);
	}
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

void VectorExecutor::execUNPKBW(const OperateInstruction& op)
{
	// Unpack each 16-bit word into 32-bit double-words:
	uint64_t v = ctx->readIntReg(op.ra);
	uint64_t r = 0;
	for (int lane = 0; lane < 2; ++lane) {
		uint16_t word = static_cast<uint16_t>((v >> (lane * 16)) & 0xFFFF);
		r |= uint64_t(word) << (lane * 32);
	}
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

void VectorExecutor::execPERR(const OperateInstruction& op)
{
	// Parallel error: set each byte to 0xFF if corresponding bytes of RA and RB differ
	uint64_t a = ctx->readIntReg(op.ra);
	uint64_t b = ctx->readIntReg(op.rb);
	uint64_t r = 0;
	for (int lane = 0; lane < 8; ++lane) {
		uint8_t va = static_cast<uint8_t>((a >> (lane * 8)) & 0xFF);
		uint8_t vb = static_cast<uint8_t>((b >> (lane * 8)) & 0xFF);
		uint8_t vr = (va != vb) ? 0xFF : 0x00;
		r |= uint64_t(vr) << (lane * 8);
	}
	ctx->writeIntReg(op.rc, r);
	ctx->notifyRegisterUpdated(false, op.rc, r);
}

