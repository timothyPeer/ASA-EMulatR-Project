#pragma once
#ifndef ALPHAVAX_DATATYPES_H
#define ALPHAVAX_DATATYPES_H

#include <QtGlobal>
#include <QByteArray>
#include <QString>
#include <QDebug>
#include <cmath>
#include <cstring>

// ============================================================================
// AlphaVAX_DataTypes.h
// ----------------------------------------------------------------------------
// Emulation-compatible definitions of Alpha AXP and VAX data types.
// Based on Alpha AXP System Reference Manual v6, Section 2.2
// Supports Byte, Word, Longword, Quadword, and all floating-point formats:
// F_Float, G_Float, D_Float, S_Float, T_Float, X_Float.
// ============================================================================

// -----------------------------------
// Helper Namespace for Conversions
// -----------------------------------

namespace FloatUtils {

	inline QByteArray floatToFBytes(float value) {
		quint32 bits;
		std::memcpy(&bits, &value, sizeof(bits));
		QByteArray out(4, 0);
		out[0] = bits >> 16;
		out[1] = bits >> 24;
		out[2] = bits >> 0;
		out[3] = bits >> 8;
		return out;
	}

	inline float fBytesToFloat(const QByteArray& bytes) {
		quint32 bits = (quint8)bytes[1] << 24 | (quint8)bytes[0] << 16 | (quint8)bytes[3] << 8 | (quint8)bytes[2];
		float value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	inline QByteArray doubleToDBytes(double value) {
		quint64 bits;
		std::memcpy(&bits, &value, sizeof(bits));
		QByteArray out(8, 0);
		for (int i = 0; i < 8; ++i)
			out[i ^ 1] = bits >> (i * 8);
		return out;
	}

	inline double dBytesToDouble(const QByteArray& bytes) {
		quint64 bits = 0;
		for (int i = 0; i < 8; ++i)
			bits |= quint64(quint8(bytes[i ^ 1])) << (i * 8);
		double value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	inline QByteArray doubleToGBytes(double value) {
		quint64 bits;
		std::memcpy(&bits, &value, sizeof(bits));
		QByteArray out(8, 0);
		out[0] = bits >> 16;
		out[1] = bits >> 24;
		out[2] = bits >> 32;
		out[3] = bits >> 40;
		out[4] = bits >> 0;
		out[5] = bits >> 8;
		out[6] = bits >> 48;
		out[7] = bits >> 56;
		return out;
	}

	inline double gBytesToDouble(const QByteArray& bytes) {
		quint64 bits = 0;
		bits |= quint64(quint8(bytes[1])) << 56;
		bits |= quint64(quint8(bytes[0])) << 48;
		bits |= quint64(quint8(bytes[3])) << 40;
		bits |= quint64(quint8(bytes[2])) << 32;
		bits |= quint64(quint8(bytes[5])) << 8;
		bits |= quint64(quint8(bytes[4])) << 0;
		bits |= quint64(quint8(bytes[7])) << 24;
		bits |= quint64(quint8(bytes[6])) << 16;
		double value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	inline QByteArray floatToSBytes(float value) {
		quint32 bits;
		std::memcpy(&bits, &value, sizeof(bits));
		QByteArray out(4, 0);
		for (int i = 0; i < 4; ++i)
			out[i] = bits >> ((3 - i) * 8);
		return out;
	}

	inline float sBytesToFloat(const QByteArray& bytes) {
		quint32 bits = 0;
		for (int i = 0; i < 4; ++i)
			bits |= quint32(quint8(bytes[i])) << ((3 - i) * 8);
		float value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	inline QByteArray doubleToTBytes(double value) {
		quint64 bits;
		std::memcpy(&bits, &value, sizeof(bits));
		QByteArray out(8, 0);
		for (int i = 0; i < 8; ++i)
			out[i] = bits >> ((7 - i) * 8);
		return out;
	}

	inline double tBytesToDouble(const QByteArray& bytes) {
		quint64 bits = 0;
		for (int i = 0; i < 8; ++i)
			bits |= quint64(quint8(bytes[i])) << ((7 - i) * 8);
		double value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	inline QByteArray longDoubleToXBytes(long double value) {
		QByteArray out(16, 0);
		std::memcpy(out.data(), &value, 16);
		return out;
	}

	inline long double xBytesToLongDouble(const QByteArray& bytes) {
		long double val = 0.0;
		std::memcpy(&val, bytes.constData(), 16);
		return val;
	}

} // namespace FloatUtils

namespace AlphaVAX {

	// -------------------
	// Fixed Integer Types
	// -------------------

	struct Byte {
		quint8 value = 0;
		Byte() = default;
		explicit Byte(quint8 val) : value(val) {}
	};

	struct Word {
		quint16 value = 0;
		Word() = default;
		explicit Word(quint16 val) : value(val) {}
	};

	struct Longword {
		quint32 value = 0;
		Longword() = default;
		explicit Longword(quint32 val) : value(val) {}
		void setBits(int start, int count, quint32 bits) {
			quint32 mask = ((1u << count) - 1) << start;
			value = (value & ~mask) | ((bits << start) & mask);
		}
	};

	struct Quadword {
		quint64 value = 0;
		Quadword() = default;
		explicit Quadword(quint64 val) : value(val) {}
	};

	// ---------------------
	// VAX Floating Formats
	// ---------------------

	struct F_Float {
		QByteArray data;
		F_Float() : data(4, 0) {}
		explicit F_Float(float value) : data(FloatUtils::floatToFBytes(value)) {}
		float toFloat() const { return FloatUtils::fBytesToFloat(data); }
	};

	struct D_Float {
		QByteArray data;
		D_Float() : data(8, 0) {}
		explicit D_Float(double value) : data(FloatUtils::doubleToDBytes(value)) {}
		double toDouble() const { return FloatUtils::dBytesToDouble(data); }
	};

	struct G_Float {
		QByteArray data;
		G_Float() : data(8, 0) {}
		explicit G_Float(double value) : data(FloatUtils::doubleToGBytes(value)) {}
		double toDouble() const { return FloatUtils::gBytesToDouble(data); }
	};

	// --------------------------
	// IEEE/Alpha Floating Formats
	// --------------------------

	struct S_Float {
		QByteArray data;
		S_Float() : data(4, 0) {}
		explicit S_Float(float value) : data(FloatUtils::floatToSBytes(value)) {}
		float toFloat() const { return FloatUtils::sBytesToFloat(data); }
	};

	struct T_Float {
		QByteArray data;
		T_Float() : data(8, 0) {}
		explicit T_Float(double value) : data(FloatUtils::doubleToTBytes(value)) {}
		double toDouble() const { return FloatUtils::tBytesToDouble(data); }
	};

	struct X_Float {
		QByteArray data;
		X_Float() : data(16, 0) {}
		explicit X_Float(long double value) : data(FloatUtils::longDoubleToXBytes(value)) {}
		long double toLongDouble() const { return FloatUtils::xBytesToLongDouble(data); }
	};

} // namespace AlphaVAX

#endif // ALPHAVAX_DATATYPES_H