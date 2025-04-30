#pragma once
#ifndef FPCRREGISTER_H
#define FPCRREGISTER_H

#include <QtGlobal>
#include <QString>
#include <QDebug>

class FpcrRegister {
public:
	FpcrRegister() = default;



	void load(quint64 value) { fpcr = value; }
	quint64 value() const { return fpcr; }
	quint64 getRaw() const { return fpcr; }
	void setRaw(quint64 value) { fpcr = value; }

	void setBit(int bitIndex) { if (bitIndex >= 0 && bitIndex < 64) fpcr |= (1ULL << bitIndex); }
	void clearBit(int bitIndex) { if (bitIndex >= 0 && bitIndex < 64) fpcr &= ~(1ULL << bitIndex); }
	bool isBitSet(int bitIndex) const { return (bitIndex >= 0 && bitIndex < 64) ? (fpcr & (1ULL << bitIndex)) != 0 : false; }

	enum FlagBit : quint64 {
		STATUS_UNDERFLOW = 1ULL << 0,
		STATUS_OVERFLOW = 1ULL << 1,
		STATUS_DIVZERO = 1ULL << 2,
		STATUS_INVALID = 1ULL << 3,
		STICKY_INEXACT = 1ULL << 5,
		STICKY_UNDERFLOW = 1ULL << 6,
		STICKY_OVERFLOW = 1ULL << 7,
		STICKY_INVALID = 1ULL << 8,
		FLAG_INEXACT = 1ULL << 49,
		FLAG_UNDERFLOW = 1ULL << 50,
		FLAG_OVERFLOW = 1ULL << 51,
		TRAP_ENABLE_UNDERFLOW = 1ULL << 52,
		TRAP_ENABLE_OVERFLOW = 1ULL << 53,
		TRAP_ENABLE_DIVZERO = 1ULL << 54,
		TRAP_ENABLE_INVALID = 1ULL << 55,
		TRAP_ENABLE_INEXACT = 1ULL << 56,
		UNDERFLOW_TO_ZERO = 1ULL << 57,
		ROUNDING_CONTROL_MASK = 3ULL << 58,
		ROUND_TO_NEAREST = 0ULL << 58,
		ROUND_TO_MINUS_INF = 1ULL << 58,
		ROUND_TO_PLUS_INF = 2ULL << 58,
		ROUND_TO_ZERO = 3ULL << 58
	};

	enum RoundingMode : quint8 {
		RoundToNearest = 0b000000,
		RoundTowardZero = 0b000001,
		RoundTowardPlusInf = 0b000010,
		RoundTowardMinusInf = 0b000011
	};

	void setTrapEnabled(FlagBit trap, bool enable) { modifyBit(getBitIndex(trap), enable); }
	bool isTrapEnabled(FlagBit trap) const { return isBitSet(getBitIndex(trap)); }
	void setFlag(FlagBit flag) { fpcr |= static_cast<quint64>(flag); }


	void modifyFlag(FlagBit flag, bool enable) { enable ? setFlag(flag) : clearBit(getBitIndex(flag)); }
	bool hasFlag(FlagBit flag) const { return (fpcr & static_cast<quint64>(flag)) != 0; }

	RoundingMode roundingMode() const { return static_cast<RoundingMode>((fpcr >> 58) & 0x3F); }
	void setRoundingMode(RoundingMode mode) {
		fpcr &= ~(0x3FULL << 58);
		fpcr |= (static_cast<quint64>(mode) << 58);
	}
	//This mirrors the Alpha Architecture Reference Manual (ASA) Vol. I, Section 4.10.5 Floating-point Control Register (FPCR), which specifies individual bit flags for exception enable, trap disable, and rounding control.
	// Set Flag Bit
	void setFlag(FlagBit flag, bool enable) {
		if (enable)
			flags |= (1 << static_cast<int>(flag));
		else
			flags &= ~(1 << static_cast<int>(flag));
	}

	bool isUnderflowTrapEnabled() const { return isTrapEnabled(TRAP_ENABLE_UNDERFLOW); }
	bool isOverflowTrapEnabled() const { return isTrapEnabled(TRAP_ENABLE_OVERFLOW); }
	bool isDivideByZeroTrapEnabled() const { return isTrapEnabled(TRAP_ENABLE_DIVZERO); }
	bool isInvalidOpTrapEnabled() const { return isTrapEnabled(TRAP_ENABLE_INVALID); }

	QString describe() const {
		return QString("FPCR=0x%1 | RM=%2 | UX=%3 OV=%4 DZ=%5 IV=%6")
			.arg(fpcr, 0, 16)
			.arg(static_cast<int>(roundingMode()))
			.arg(isUnderflowTrapEnabled())
			.arg(isOverflowTrapEnabled())
			.arg(isDivideByZeroTrapEnabled())
			.arg(isInvalidOpTrapEnabled());
	}

private:
	quint64 fpcr = 0;
	quint32 flags = 0; // Adjust type based on spec: Alpha uses 64-bit FPCR
	void modifyBit(int bitIndex, bool enable) { enable ? setBit(bitIndex) : clearBit(bitIndex); }
	static int getBitIndex(FlagBit flag) {
		for (int i = 0; i < 64; ++i)
			if ((static_cast<quint64>(flag) >> i) & 1)
				return i;
		return -1;
	}
};

#endif // FPCRREGISTER_H
