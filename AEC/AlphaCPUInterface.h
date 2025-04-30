#pragma once
#include <QObject>
#include "..\AESH\Helpers.h"

class AlphaCPUInterface
{
public:
	virtual ~AlphaCPUInterface() = default;

	virtual quint64 getPC() const = 0;
	virtual void setPC(quint64 pc) = 0;
	virtual void raiseException(helpers_JIT::ExceptionType type, quint64 faultAddress) = 0;
	virtual bool isKernelMode() const = 0;
	virtual void writeRegister(int regNum, quint64 value) = 0;
	virtual quint64 readRegister(int regNum) const = 0;

	void returnFromTrap()
	{
		throw std::logic_error("The method or operation is not implemented.");
	TODO: returnFromTrap();
	}


};
