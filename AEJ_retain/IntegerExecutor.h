#pragma once
#ifndef IntegerExecutor2_h__
#define IntegerExecutor2_h__

#include <QObject>
#include "IExecutionContext.h"
#include "decodeOperate.h"
#include <cstring>
#include "alphamemorysystem.h"
/*#include "AlphaCPU.h"*/

class IntegerExecutor : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(IntegerExecutor)

	enum IntegerSubType {
		ARITHMETIC,
		COMPARISON,
		CONVERSION,
		UNKNOWN
	};

public:
	IntegerExecutor(IExecutionContext* ctx,
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


	// Arithmetic
	void execADDL(const OperateInstruction& op) {
		quint32 result = static_cast<quint32>(ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb));
		ctx->writeIntReg(op.rc, static_cast<quint64>(result));
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execADDQ(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execCMPEQ(const OperateInstruction& op) {
		quint64 result = (ctx->readIntReg(op.ra) == ctx->readIntReg(op.rb)) ? 1 : 0;
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execCMPLT(const OperateInstruction& op) {
		qint64 a = static_cast<qint64>(ctx->readIntReg(op.ra));
		qint64 b = static_cast<qint64>(ctx->readIntReg(op.rb));
		ctx->writeIntReg(op.rc, a < b ? 1 : 0);
		ctx->notifyRegisterUpdated(/*isFp=*/false,op.rc, a < b ? 1 : 0);
	}

	void execCMPLE(const OperateInstruction& op) {
		qint64 a = static_cast<qint64>(ctx->readIntReg(op.ra));
		qint64 b = static_cast<qint64>(ctx->readIntReg(op.rb));
		ctx->writeIntReg(op.rc, a <= b ? 1 : 0);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, a < b ? 1 : 0);
	}
	//Sign-extend 32-bit source integer to 64-bit destination
	void execCVTLQ(const OperateInstruction& op) {
		quint32 src = static_cast<quint32>(ctx->readIntReg(op.ra) & 0xFFFFFFFF); // Read as 32-bit
		qint64 result = static_cast<qint64>(static_cast<qint32>(src)); // Sign-extend 32→64 bits
		ctx->writeIntReg(op.rc, static_cast<quint64>(result)); // Write back to 64-bit register
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, static_cast<quint64>(result));
		
	}
	//Truncate 64-bit source integer to lower 32 bits, sign-extend to 64 bits
	void execCVTQL(const OperateInstruction& op) {
		qint64 src = static_cast<qint64>(ctx->readIntReg(op.ra)); // Read full 64-bit source
		quint32 lower32 = static_cast<quint32>(src & 0xFFFFFFFF); // Take low 32 bits
		qint64 result = static_cast<qint64>(static_cast<qint32>(lower32)); // Sign-extend 32→64 bits
		ctx->writeIntReg(op.rc, static_cast<quint64>(result)); // Write back
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, static_cast<quint64>(result));
	}

	// Logical
	void execAND(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) & ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execBIC(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) & ~ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execBIS(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) | ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execEQV(const OperateInstruction& op) {
		quint64 result = ~(ctx->readIntReg(op.ra) ^ ctx->readIntReg(op.rb));
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execSUB(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) - ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execMUL(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) * ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execDIV(const OperateInstruction& op) {
		quint64 divisor = ctx->readIntReg(op.rb);
		if (divisor == 0) {
			ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::DivideByZero_int));
			return;
		}
		quint64 result = ctx->readIntReg(op.ra) / divisor;
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execMOD(const OperateInstruction& op) {
		quint64 divisor = ctx->readIntReg(op.rb);
		if (divisor == 0) {
			ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::DivideByZero_int));
			return;
		}
		quint64 result = ctx->readIntReg(op.ra) % divisor;
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}
	void execOR(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) | ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execXOR(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) ^ ctx->readIntReg(op.rb);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execNOT(const OperateInstruction& op) {
		quint64 result = ~ctx->readIntReg(op.ra);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execSLL(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) << (ctx->readIntReg(op.rb) & 0x3F);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execSRL(const OperateInstruction& op) {
		quint64 result = ctx->readIntReg(op.ra) >> (ctx->readIntReg(op.rb) & 0x3F);
		ctx->writeIntReg(op.rc, result);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, result);
	}

	void execSRA(const OperateInstruction& op) {
		qint64 result = static_cast<qint64>(ctx->readIntReg(op.ra)) >> (ctx->readIntReg(op.rb) & 0x3F);
		ctx->writeIntReg(op.rc, static_cast<quint64>(result));
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc,result);
	}

	void execLDB(const OperateInstruction& op) {
		quint64 addr = ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb);
		quint64 temp = 0;
		if (!memSystem->readVirtualMemory(alphaCPU, addr, temp, 1)) {
			ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::MMUAccessFault));
			return;
		}
		qint8 value = static_cast<qint8>(temp);
		regs->writeIntReg(op.rc, static_cast<qint64>(value));
		ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, static_cast<qint64>(value));
	}

	void execLDBU(const OperateInstruction& op) {
		quint64 addr = ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb);
		quint64 temp = 0;
		if (!memSystem->readVirtualMemory(alphaCPU, addr, temp, 1)) {
			ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::MMUAccessFault));
			return;
		}
		quint8 value = static_cast<quint8>(temp);
		ctx->writeIntReg(op.rc, value);
		
		ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, value);
	}

	void execLDW(const OperateInstruction& op) {
		quint64 addr = ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb);
		quint64 temp = 0;
		if (!memSystem->readVirtualMemory(alphaCPU, addr, temp, 2)) {
			ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::MMUAccessFault));
			return;
		}
		qint16 value = static_cast<qint16>(temp);
		ctx->writeIntReg(op.rc, static_cast<qint64>(value));
		ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc,value);
	}

	void execLDWU(const OperateInstruction& op) {
		quint64 addr = ctx->readIntReg(op.ra) + cxt->readIntReg(op.rb);
		quint64 temp = 0;
		if (!memSystem->readVirtualMemory(alphaCPU, addr, temp, 2)) {
			ctx->notifyTrapRaised(static_cast<quint64>(helpers_JIT::TrapType::MMUAccessFault));
			return;
		}
		quint16 value = static_cast<quint16>(temp);
		ctx->writeIntReg(op.rc, value);
		ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
		ctx->notifyRegisterUpdated(/*isFp=*/false, op.rc, value);
	}

	void execSTB(const OperateInstruction& op) {
		quint64 addr = ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb);
		quint8 value = static_cast<quint8>(ctx->readIntReg(op.rc));
		memSystem->writeVirtualMemory(alphaCPU, addr, static_cast<quint64>(value), 1);
		ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
	}

	void execSTW(const OperateInstruction& op) {
		quint64 addr = ctx->readIntReg(op.ra) + ctx->readIntReg(op.rb);
		quint16 value = static_cast<quint16>(ctx->readIntReg(op.rc));
		memSystem->writeVirtualMemory(alphaCPU, addr, static_cast<quint64>(value), 2);
		ctx->notifyMemoryAccessed(addr, value, /*isWrite=*/false);
	}
		private:
			IExecutionContext* ctx;
			RegisterBank* regs;
			FpRegisterBankcls* fpRegs;
			AlphaMemorySystem* memSystem;
			AlphaCPU* alphaCPU;

};
#endif // IntegerExecutor2_h__