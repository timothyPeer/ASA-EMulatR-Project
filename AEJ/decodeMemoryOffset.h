#pragma once
#include <QObject>
#include "IExecutionContext.h"

inline quint64 decodeMemoryOffset(const OperateInstruction& op, IExecutionContext* ctx) {
	// If your instruction format uses the "function" field as a literal immediate:
	if (op.function /* or op.isLiteral */) {
		return static_cast<quint64>(static_cast<qint8>(op.function));
	}
	else {
		return ctx->readIntReg(op.rb);
	}
}