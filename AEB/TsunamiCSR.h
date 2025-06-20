#pragma once
#include <QObject>
#include "BusInterface.h"

class TsunamiCSR : public QObject, public BusInterface
{
	Q_OBJECT

public:
	TsunamiCSR(QObject* parent) : QObject(parent) {

	}






	QString identifier() const override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	QString description() const override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	quint64 read(quint64 offset, int size) override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	quint64 read(quint64 offset) override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	bool write(quint64 offset, quint64 value) override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	bool write(quint64 offset, quint64 value, int size) override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	bool isDeviceAddress(quint64 addr) override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	void reset() override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	quint64 getBaseAddress() override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	quint64 getSize() const override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	bool canInterrupt() const override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	quint64 size() const override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}


	void setMemoryMapping(quint64 base, quint64 size) override
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

};