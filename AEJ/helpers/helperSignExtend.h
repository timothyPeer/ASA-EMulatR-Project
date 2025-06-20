#pragma once

#include <QtGlobal>  // For quint8, qint8, quint16, qint16, etc.

// Forward declarations to fix missing type specifier issue

#include "../AEJ/JITFaultInfoStructures.h"
#include "../AEE/MemoryAccessException.h"
#include "../AEE/TLBExceptionQ.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"
#include "../AEJ/enumerations/enumTLBException.h"

inline quint64 signExtend8(quint8 value) {
	return static_cast<quint64>(static_cast<qint64>(static_cast<qint8>(value)));
}

inline quint64 signExtend16(quint16 value) {
	return static_cast<quint64>(static_cast<qint64>(static_cast<qint16>(value)));
}

inline quint64 signExtend32(quint32 value) {
	return static_cast<quint64>(static_cast<qint64>(static_cast<qint32>(value)));
}

inline quint64 signExtend21(quint32 value) {
	constexpr quint32 signBit = 1u << 20;
	constexpr quint32 mask = ~((1u << 21) - 1);

	quint64 extended = (value & signBit) ? (value | mask) : value;

	return static_cast<quint64>(static_cast<qint64>(static_cast<qint32>(extended)));
}


