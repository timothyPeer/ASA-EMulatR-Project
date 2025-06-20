#include "fpRegisterFileWrapper.h"
#include <limits>
#include <cmath>    // 

FpRegisterBankcls::FpRegisterBankcls(QObject* parent)
	: QObject(parent), fpRegisters(32), dirtyFlags(32)
{
	fpRegisters.fill(dt_gfloat(0.0));
}


dt_gfloat FpRegisterBankcls::readFpReg(quint8 index) const {
	if (index >= fpRegisters.size()) {
		qWarning() << "[FpRegisterBank] Invalid FP read index:" << index;
		return dt_gfloat(std::numeric_limits<double>::quiet_NaN());
	}
	return fpRegisters[index];
}

void FpRegisterBankcls::writeFpReg(quint8 index, const dt_gfloat& value) {
	if (index < fpRegisters.size() && index != 31) {
		fpRegisters[index] = value;
		dirtyFlags.setBit(index);
	}
}

void FpRegisterBankcls::load(const QVector<quint64>& values) {
	for (int i = 0; i < qMin(values.size(), 31); ++i)
		writeFpReg(i, dt_gfloat(values[i]));
}

QVector<quint64> FpRegisterBankcls::dump() const {
	QVector<quint64> fprs;
	for (int i = 0; i < 31; ++i)
		fprs.append(fpRegisters[i].toDouble());
	return fprs;
}

void FpRegisterBankcls::clearDirtyFlags() {
	dirtyFlags.fill(false);
}

void FpRegisterBankcls::setFpcr(quint64 value) {
	fpcr.raw = value;
}

quint64 FpRegisterBankcls::getFpcr() const {
	return fpcr.raw;
}

FPCRContext& FpRegisterBankcls::getFpcrContext() {
	return fpcr;
}

const FPCRContext& FpRegisterBankcls::getFpcrContext() const {
	return fpcr;
}

