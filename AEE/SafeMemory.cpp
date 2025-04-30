// SafeMemory.cpp
#include "SafeMemory.h"
#include <QFile>
#include <QDebug>

SafeMemory::SafeMemory(MMIOManager* mmioManager, quint64 initialSize, QObject* parent)
	: QObject(parent), mmioManager(mmioManager)
{
	memory.resize(static_cast<int>(initialSize));
	memory.fill(0);
	qDebug() << "SafeMemory: Initialized with" << initialSize << "bytes";
}

void SafeMemory::resize(quint64 newSize)
{
	QWriteLocker locker(&memoryLock);
	if (newSize > static_cast<quint64>(memory.size())) {
		// Growing memory - keep existing contents
		int oldSize = memory.size();
		memory.resize(static_cast<int>(newSize));
		// Zero out the new memory region
		for (int i = oldSize; i < memory.size(); ++i) {
			memory[i] = 0;
		}
	}
	else {
		// Shrinking memory - truncate
		memory.resize(static_cast<int>(newSize));
	}
	qDebug() << "SafeMemory: Resized to" << newSize << "bytes";
}

quint64 SafeMemory::size() const
{
	QReadLocker locker(&memoryLock);
	return static_cast<quint64>(memory.size());
}

bool SafeMemory::isValidAddress(quint64 address, int size) const
{
	return (address + size <= static_cast<quint64>(memory.size()));
}

quint8 SafeMemory::readUInt8(quint64 address)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		quint64 value = mmioManager->readMMIO(address, 1);
		emit memoryRead(address, value, 1);
		return static_cast<quint8>(value);
	}

	// Regular memory access
	QReadLocker locker(&memoryLock);
	if (!isValidAddress(address, 1)) {
		qWarning() << "SafeMemory: Read8 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	quint8 value = memory[static_cast<int>(address)];
	emit memoryRead(address, value, 1);
	return value;
}

quint16 SafeMemory::read16(quint64 address)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		quint64 value = mmioManager->readMMIO(address, 2);
		emit memoryRead(address, value, 2);
		return static_cast<quint16>(value);
	}

	// Regular memory access
	QReadLocker locker(&memoryLock);
	if (!isValidAddress(address, 2)) {
		qWarning() << "SafeMemory: Read16 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	// Alpha uses little-endian byte order
	quint16 value = static_cast<quint16>(memory[static_cast<int>(address)]) |
		(static_cast<quint16>(memory[static_cast<int>(address + 1)]) << 8);

	emit memoryRead(address, value, 2);
	return value;
}

quint32 SafeMemory::readUInt32(quint64 address)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		quint64 value = mmioManager->readMMIO(address, 4);
		emit memoryRead(address, value, 4);
		return static_cast<quint32>(value);
	}

	// Regular memory access
	QReadLocker locker(&memoryLock);
	if (!isValidAddress(address, 4)) {
		qWarning() << "SafeMemory: Read32 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	// Alpha uses little-endian byte order
	quint32 value = static_cast<quint32>(memory[static_cast<int>(address)]) |
		(static_cast<quint32>(memory[static_cast<int>(address + 1)]) << 8) |
		(static_cast<quint32>(memory[static_cast<int>(address + 2)]) << 16) |
		(static_cast<quint32>(memory[static_cast<int>(address + 3)]) << 24);

	emit memoryRead(address, value, 4);
	return value;
}

quint64 SafeMemory::readUInt64(quint64 address)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		quint64 value = mmioManager->readMMIO(address, 8);
		emit memoryRead(address, value, 8);
		return value;
	}

	// Regular memory access
	QReadLocker locker(&memoryLock);
	if (!isValidAddress(address, 8)) {
		qWarning() << "SafeMemory: Read64 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	// Alpha uses little-endian byte order
	quint64 value = static_cast<quint64>(memory[static_cast<int>(address)]) |
		(static_cast<quint64>(memory[static_cast<int>(address + 1)]) << 8) |
		(static_cast<quint64>(memory[static_cast<int>(address + 2)]) << 16) |
		(static_cast<quint64>(memory[static_cast<int>(address + 3)]) << 24) |
		(static_cast<quint64>(memory[static_cast<int>(address + 4)]) << 32) |
		(static_cast<quint64>(memory[static_cast<int>(address + 5)]) << 40) |
		(static_cast<quint64>(memory[static_cast<int>(address + 6)]) << 48) |
		(static_cast<quint64>(memory[static_cast<int>(address + 7)]) << 56);

	emit memoryRead(address, value, 8);
	return value;
}

void SafeMemory::writeUInt8(quint64 address, quint8 value)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		mmioManager->writeMMIO(address, value, 1);
		emit memoryWritten(address, value, 1);
		return;
	}

	// Regular memory access
	QWriteLocker locker(&memoryLock);
	if (!isValidAddress(address, 1)) {
		qWarning() << "SafeMemory: Write8 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return;
	}

	memory[static_cast<int>(address)] = value;
	emit memoryWritten(address, value, 1);
}

void SafeMemory::writeUInt16(quint64 address, quint16 value)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		mmioManager->writeMMIO(address, value, 2);
		emit memoryWritten(address, value, 2);
		return;
	}

	// Regular memory access
	QWriteLocker locker(&memoryLock);
	if (!isValidAddress(address, 2)) {
		qWarning() << "SafeMemory: Write16 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return;
	}

	// Alpha uses little-endian byte order
	memory[static_cast<int>(address)] = static_cast<quint8>(value);
	memory[static_cast<int>(address + 1)] = static_cast<quint8>(value >> 8);

	emit memoryWritten(address, value, 2);
}

void SafeMemory::writeUInt32(quint64 address, quint32 value)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		mmioManager->writeMMIO(address, value, 4);
		emit memoryWritten(address, value, 4);
		return;
	}

	// Regular memory access
	QWriteLocker locker(&memoryLock);
	if (!isValidAddress(address, 4)) {
		qWarning() << "SafeMemory: Write32 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return;
	}

	// Alpha uses little-endian byte order
	memory[static_cast<int>(address)] = static_cast<quint8>(value);
	memory[static_cast<int>(address + 1)] = static_cast<quint8>(value >> 8);
	memory[static_cast<int>(address + 2)] = static_cast<quint8>(value >> 16);
	memory[static_cast<int>(address + 3)] = static_cast<quint8>(value >> 24);

	emit memoryWritten(address, value, 4);
}

void SafeMemory::writeUInt64(quint64 address, quint64 value)
{
	// Check MMIO first
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		mmioManager->writeMMIO(address, value, 8);
		emit memoryWritten(address, value, 8);
		return;
	}

	// Regular memory access
	QWriteLocker locker(&memoryLock);
	if (!isValidAddress(address, 8)) {
		qWarning() << "SafeMemory: Write64 out of bounds:" << QString("0x%1").arg(address, 0, 16);
		return;
	}

	// Alpha uses little-endian byte order
	memory[static_cast<int>(address)] = static_cast<quint8>(value);
	memory[static_cast<int>(address + 1)] = static_cast<quint8>(value >> 8);
	memory[static_cast<int>(address + 2)] = static_cast<quint8>(value >> 16);
	memory[static_cast<int>(address + 3)] = static_cast<quint8>(value >> 24);
	memory[static_cast<int>(address + 4)] = static_cast<quint8>(value >> 32);
	memory[static_cast<int>(address + 5)] = static_cast<quint8>(value >> 40);
	memory[static_cast<int>(address + 6)] = static_cast<quint8>(value >> 48);
	memory[static_cast<int>(address + 7)] = static_cast<quint8>(value >> 56);

	emit memoryWritten(address, value, 8);
}

quint32 SafeMemory::fetchInstruction(quint64 address)
{
	// Instructions should never be fetched from MMIO regions
	if (mmioManager && mmioManager->isMMIOAddress(address)) {
		qWarning() << "SafeMemory: Attempt to fetch instruction from MMIO region at"
			<< QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	QReadLocker locker(&memoryLock);
	if (!isValidAddress(address, 4)) {
		qWarning() << "SafeMemory: Instruction fetch out of bounds:"
			<< QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	// Alpha instructions are always 32-bit
	quint32 instruction = static_cast<quint32>(memory[static_cast<int>(address)]) |
		(static_cast<quint32>(memory[static_cast<int>(address + 1)]) << 8) |
		(static_cast<quint32>(memory[static_cast<int>(address + 2)]) << 16) |
		(static_cast<quint32>(memory[static_cast<int>(address + 3)]) << 24);

	// We don't emit a signal for instruction fetches to reduce noise
	return instruction;
}

void SafeMemory::setMMIOManager(MMIOManager* manager)
{
	mmioManager = manager;
}

bool SafeMemory::loadBinary(const QString& filename, quint64 loadAddress)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning() << "SafeMemory: Failed to open file:" << filename;
		return false;
	}

	QByteArray data = file.readAll();
	file.close();

	QWriteLocker locker(&memoryLock);
	if (loadAddress + static_cast<quint64>(data.size()) > static_cast<quint64>(memory.size())) {
		qWarning() << "SafeMemory: Binary file size exceeds available memory";
		return false;
	}

	// Copy the binary data into memory
	for (int i = 0; i < data.size(); ++i) {
		memory[static_cast<int>(loadAddress) + i] = static_cast<quint8>(data[i]);
	}

	qDebug() << "SafeMemory: Loaded" << data.size() << "bytes from" << filename
		<< "to address" << QString("0x%1").arg(loadAddress, 0, 16);
	return true;
}

bool SafeMemory::dumpMemory(const QString& filename, quint64 startAddress, quint64 size)
{
	QReadLocker locker(&memoryLock);

	if (startAddress + size > static_cast<quint64>(memory.size())) {
		qWarning() << "SafeMemory: Dump region exceeds memory bounds";
		return false;
	}

	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "SafeMemory: Failed to open file for writing:" << filename;
		return false;
	}

	// Create a byte array from the memory region
	QByteArray data;
	data.reserve(static_cast<int>(size));
	for (quint64 i = 0; i < size; ++i) {
		data.append(memory[static_cast<int>(startAddress) + i]);
	}

	// Write to file
	qint64 written = file.write(data);
	file.close();

	if (written != data.size()) {
		qWarning() << "SafeMemory: Failed to write all data to file";
		return false;
	}

	qDebug() << "SafeMemory: Dumped" << size << "bytes to" << filename
		<< "from address" << QString("0x%1").arg(startAddress, 0, 16);
	return true;
}

void SafeMemory::clear(quint64 startAddress, quint64 size, quint8 value)
{
	QWriteLocker locker(&memoryLock);

	quint64 endAddress = startAddress + size;
	if (endAddress > static_cast<quint64>(memory.size())) {
		qWarning() << "SafeMemory: Clear region exceeds memory bounds";
		endAddress = static_cast<quint64>(memory.size());
	}

	for (quint64 i = startAddress; i < endAddress; ++i) {
		memory[static_cast<int>(i)] = value;
	}

	qDebug() << "SafeMemory: Cleared" << (endAddress - startAddress) << "bytes to value"
		<< static_cast<int>(value) << "starting at" << QString("0x%1").arg(startAddress, 0, 16);
}

void SafeMemory::copyMemory(quint64 destination, quint64 source, quint64 size)
{
	QWriteLocker locker(&memoryLock);

	if (source + size > static_cast<quint64>(memory.size()) ||
		destination + size > static_cast<quint64>(memory.size())) {
		qWarning() << "SafeMemory: Copy exceeds memory bounds";
		return;
	}

	// Handle overlapping regions by determining copy direction
	if (destination <= source || destination >= source + size) {
		// Non-overlapping or destination before source - copy forward
		for (quint64 i = 0; i < size; ++i) {
			memory[static_cast<int>(destination + i)] = memory[static_cast<int>(source + i)];
		}
	}
	else {
		// Overlapping with destination after source - copy backward
		for (quint64 i = size; i > 0; --i) {
			memory[static_cast<int>(destination + i - 1)] = memory[static_cast<int>(source + i - 1)];
		}
	}

	qDebug() << "SafeMemory: Copied" << size << "bytes from"
		<< QString("0x%1").arg(source, 0, 16) << "to"
		<< QString("0x%1").arg(destination, 0, 16);
}
