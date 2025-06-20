#pragma once

#ifndef FpRegisterBank_h__
#define FpRegisterBank_h__

#include "dt_gfloat.h"
#include <QObject>
#include <QVector>
#include <QBitArray>
#include <QDebug>
#include <QMap>
#include <array>

#include "FpRegisterFile.h" // Defines FPCR flag bits and trap modes


/*

Considerations:
	- All double precision types are treated as 64-bit, even S-Float,
*/



class FpRegisterBankcls : public QObject {
	Q_OBJECT

public:
	explicit FpRegisterBankcls(QObject* parent = nullptr);

	dt_gfloat readFpReg(quint8 index) const;

	void writeFpReg(quint8 index, const dt_gfloat& value);

	void load(const QVector<quint64>& values);

	QVector<quint64> dump() const;
	void clear()
	{
		fpRegisters.clear();
	}
	void clearDirtyFlags();

	// FPCR accessors
	void setFpcr(quint64 value);
	quint64 getFpcr() const;
	QVector<dt_gfloat> getVectorFpRegister() { return fpRegisters; }
	FPCRContext& getFpcrContext();
	const FPCRContext& getFpcrContext() const;

private:
	QVector<dt_gfloat> fpRegisters;  // ✅ Now using QVector
	QBitArray dirtyFlags;
	FPCRContext fpcr;
};

#endif // FpRegisterBank_h__





