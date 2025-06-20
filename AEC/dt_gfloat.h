#pragma once
#ifndef dt_gfloat_h__
#define dt_gfloat_h__


#include <QObject>
#include <QDebug>
#include "FPCRContext.h"
#include "..\AEJ\enumerations\enumRoundingMode.h"

class dt_gfloat {
public:
	quint64 raw = 0; ///< 64-bit Alpha G floating value (raw bits)

	static constexpr int EXP_BITS = 11;
	static constexpr int FRAC_BITS = 52;
	static constexpr int EXP_BIAS = 1024;

	static constexpr quint64 SIGN_MASK = 0x8000000000000000ULL;
	static constexpr quint64 EXP_MASK = 0x7FF0000000000000ULL;
	static constexpr quint64 FRAC_MASK = 0x000FFFFFFFFFFFFFULL;

	// Default constructor
	dt_gfloat() = default;

	// Constructor from raw bits
	explicit dt_gfloat(quint64 rawBits) : raw(rawBits) {}

	// Constructor from double
	explicit dt_gfloat(double value) { *this = fromDouble(value); }

	/// Convert double → dt_gfloat (bitwise copy)
	static dt_gfloat fromDouble(double value) {
		dt_gfloat gf;
		std::memcpy(&gf.raw, &value, sizeof(double));
		return gf;
	}

	/// Convert dt_gfloat → double (bitwise copy)
	double toDouble() const {
		double val;
		std::memcpy(&val, &raw, sizeof(double));
		return val;
	}

	/// Returns sign bit (0=positive, 1=negative)
	bool sign() const { return raw & SIGN_MASK; }

	/// Extracts exponent field (11 bits)
	int exponent() const { return (raw & EXP_MASK) >> FRAC_BITS; }

	/// Extracts unbiased exponent (removes EXP_BIAS)
	qint64 unbiasedExponent() const { return exponent() - EXP_BIAS; }

	/// Extracts fraction field (52 bits)
	quint64 fraction() const { return raw & FRAC_MASK; }

	/// Tests special cases
	bool isZero() const { return (raw & ~SIGN_MASK) == 0; }
	bool isInf() const { return (exponent() == 0x7FF) && (fraction() == 0); }
	bool isNaN() const { return (exponent() == 0x7FF) && (fraction() != 0); }
	bool isDenormal() const { return (exponent() == 0) && (fraction() != 0); }

	/// Arithmetic operators
	dt_gfloat operator+(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() + rhs.toDouble()); }
	dt_gfloat operator-(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() - rhs.toDouble()); }
	dt_gfloat operator*(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() * rhs.toDouble()); }
	dt_gfloat operator/(const dt_gfloat& rhs) const { return fromDouble(this->toDouble() / rhs.toDouble()); }

	/// Comparison operators
	bool operator==(const dt_gfloat& rhs) const { return toDouble() == rhs.toDouble(); }
	bool operator!=(const dt_gfloat& rhs) const { return toDouble() != rhs.toDouble(); }
	bool operator<(const dt_gfloat& rhs) const { return toDouble() < rhs.toDouble(); }
	bool operator>(const dt_gfloat& rhs) const { return toDouble() > rhs.toDouble(); }
	bool operator<=(const dt_gfloat& rhs) const { return toDouble() <= rhs.toDouble(); }
	bool operator>=(const dt_gfloat& rhs) const { return toDouble() >= rhs.toDouble(); }

	/// Conversion from int64_t
	static dt_gfloat fromInt64(qint64 val) {
		return fromDouble(static_cast<double>(val));
	}

	/// Conversion to int64_t with rounding based on FPCR
	qint64 toInt64(FPCRContext& fpcr) const {
		double val = toDouble();
		if (isNaN() && fpcr.trapInvalid())
			fpcr.setStickyInvalid();
		return static_cast<qint64>(applyRounding(val, fpcr));
	}

	/// Applies rounding rules from FPCR
	static double applyRounding(double value, const FPCRContext& fpcr) {
		switch (fpcr.roundingMode()) {
		case 0: return std::nearbyint(value);      // round to nearest
		case 1: return (value > 0) ? std::floor(value) : std::ceil(value); // toward zero
		case 2: return std::ceil(value);           // toward +infinity
		case 3: return std::floor(value);          // toward -infinity
		default: return value;
		}
	}

	/// Debug print helper
	friend QDebug operator<<(QDebug dbg, const dt_gfloat& gf) {
		dbg.nospace() << "GFloat("
			<< "raw=0x" << QString::number(gf.raw, 16)
			<< ", exp=" << gf.exponent()
			<< ", val=" << gf.toDouble()
			<< ")";
		return dbg.space();
	}
};
#endif // dt_gfloat_h__
