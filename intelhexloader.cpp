// IntelHexLoader.cpp
#include "IntelHexLoader.h"
#include <QFile>
#include <QTextStream>
#include "..\AEJ\SafeMemory_refactored.h"

bool IntelHexLoader::loadHexFile(const QString& filePath,
	SafeMemory* memory,
	quint64       loadBase)
{
	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	QTextStream in(&f);
	quint64 upperAddr = 0;

	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if (line.isEmpty() || !line.startsWith(':'))
			continue;

		quint8  len;
		quint16 addr;
		quint8  type;
		QByteArray data;
		if (!parseLine(line, len, addr, type, data))
			return false;

		switch (type) {
		case 0x00: {  // Data record
			quint64 target = loadBase + upperAddr + addr;
			// map & write this chunk
			memory->mapRegion(target, data.size(), enumMemoryPerm::RWExec);
			memory->writeBytes(target,
				reinterpret_cast<const uint8_t*>(data.constData()),
				data.size());
			
			break;
		}
		case 0x01:    // End-of-file
			return true;
		case 0x02: {  // Extended segment address
			// addr = (data[0]<<8 | data[1]) << 4
			upperAddr = ((uint16_t)((uint8_t)data[0] << 8 |
				(uint8_t)data[1])) << 4;
			break;
		}
		case 0x04: {  // Extended linear address
			// addr = (data[0]<<8 | data[1]) << 16
			upperAddr = ((uint16_t)((uint8_t)data[0] << 8 |
				(uint8_t)data[1])) << 16;
			break;
		}
		default:
			// ignore other record types (start-segment, start-linear, etc.)
			break;
		}
	}

	return true;
}

bool IntelHexLoader::parseLine(const QString& line,
	quint8& byteCount,
	quint16& address,
	quint8& recordType,
	QByteArray& data)
{
	// Minimal length is 11 chars: :LLAAAATTCC
	if (line.size() < 11) return false;

	auto hexToU8 = [&](const QString& s) {
		bool ok; return (uint8_t)s.toUInt(&ok, 16);
		};
	auto hexToU16 = [&](const QString& s) {
		bool ok; return (uint16_t)s.toUInt(&ok, 16);
		};

	data.clear();
	byteCount = hexToU8(line.mid(1, 2));
	address = hexToU16(line.mid(3, 4));
	recordType = hexToU8(line.mid(7, 2));

	// read data bytes
	for (int i = 0; i < byteCount; ++i) {
		data.append((char)hexToU8(line.mid(9 + i * 2, 2)));
	}

	// verify checksum
	quint8 sum = 0;
	sum += byteCount;
	sum += uint8_t(address >> 8);
	sum += uint8_t(address & 0xFF);
	sum += recordType;
	for (auto ch : data) sum += uint8_t(ch);
	quint8 chk = hexToU8(line.mid(9 + byteCount * 2, 2));
	if (quint8(sum + chk) != 0)
		return false;

	return true;
}
