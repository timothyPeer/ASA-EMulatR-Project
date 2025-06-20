#pragma once
#include <QObject>
#include <QString>
#include <QThread>
#include <QException>
#include <QByteArray>
#include <QtGlobal>
#include "../AEJ/helpers//helperSignExtend.h"
#include "../AEE/MemoryFaultInfo.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"


/**
 * @brief Exception for memory access faults, fully Qt-compliant.
 */
class MemoryAccessException : public QException
{
public:
	MemoryAccessException(MemoryFaultType type_, quint64 address_, int size, bool isWrite, quint64 pc)
		: m_type(type_), m_address(address_), m_size(size), m_isWrite(isWrite), m_programCounter(pc)
	{
		buildMessage();
	}

// 	explicit MemoryAccessException( MemoryFaultInfo* info)
// 		: m_type(info.faultType), m_address(info.faultAddress), m_size(info.accessSize),
// 		m_isWrite(info.isWrite), m_programCounter(info.pc)
// 	{
// 		buildMessage();
// 	}

	void raise() const override { throw* this; }
	MemoryAccessException* clone() const override { return new MemoryAccessException(*this); }

	QString message() const { return m_msg; }
	const char* what() const noexcept override {
		static thread_local QByteArray utf8Data;
		utf8Data = m_msg.toUtf8();
		return utf8Data.constData();
	}

	MemoryFaultType getType() const { return m_type; }
	quint64 getAddress() const { return m_address; }
	int getSize() const { return m_size; }
	bool isWrite() const { return m_isWrite; }
	quint64 getPC() const { return m_programCounter; }

	MemoryFaultInfo getFaultInfo() const {
		return MemoryFaultInfo{ m_type, m_address, 0, m_size, m_isWrite, false, m_programCounter, 0 };
	}

private:
	void buildMessage() {
		m_msg = QString("Memory access error: %1 at address 0x%2 (size %3, %4, PC: 0x%5)")
			.arg(QString::number(static_cast<int>(m_type)))
			.arg(QString::number(m_address, 16))
			.arg(m_size)
			.arg(m_isWrite ? "write" : "read")
			.arg(QString::number(m_programCounter, 16));
	}

private:
	MemoryFaultType m_type;
	quint64 m_address;
	int m_size;
	bool m_isWrite;
	quint64 m_programCounter;
	QString m_msg;
};


