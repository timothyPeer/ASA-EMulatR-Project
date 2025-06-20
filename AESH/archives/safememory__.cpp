// SafeMemory.cpp
#include "SafeMemory.h"
#include <QFile>
#include <QDebug>
#include "TraceManager.h"
#include "GlobalMacro.h"
#include "MemoryAccessException.h"
#include "..\AEJ\GlobalLockTracker.h"
#include "AlphaCPU.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"
#include "../AEJ/structures/enumMemoryPerm.h"

SafeMemory::SafeMemory( QObject* parent)
	: QObject(parent)
{
	//memory.resize(static_cast<int>(initialSize)); 
	m_ram.fill(0);		// Memory should have been initialize in AlphaMemorySystem
	//qDebug() << "SafeMemory: Constructed and Initialized ";
	TRACE_LOG(QString("[SafeMemory:Ctor()] Memory Size: %1").arg(m_ram.size()));
}

void SafeMemory::resize(quint64 newSize, bool initialize)
{
	QWriteLocker locker(&m_memoryLock);
	if (initialize) {
		m_ram.resize(static_cast<int>(newSize));
		return;
	}
	// Expand Memory Boundaries
	if (newSize > static_cast<quint64>(m_ram.size())) {
		// Growing memory - keep existing contents
		int oldSize = m_ram.size();
		m_ram.resize(static_cast<int>(newSize));
		// Zero out the new memory region
		for (int i = oldSize; i < m_ram.size(); ++i) {
			m_ram[i] = 0;
		}
	}
	TRACE_LOG(QString("[SafeMemory:resize()] allocation complete :%1").arg(m_ram.size()));
}

quint64 SafeMemory::size() const
{
	QReadLocker locker(&m_memoryLock);
	return static_cast<quint64>(m_ram.size());
}
/**
 * @brief Invalidate every CPU’s LD[L/Q]_L reservation that intersects a write.
 *
 * @param physicalAddr  First byte of the written quad-word (already 8-aligned)
 * @param size          Size of the write (4 or 8)
 * TODO:  CPU Registry
 *
 * A full SMP model would walk every attached CPU and clear its reservation if
 * the address ranges overlap.  For now we just emit a debug message – you can
 * extend this later when you add a CPU-registry to AlphaMemorySystem.
 */
void SafeMemory::clearReservations(quint64 physicalAddr, int size)
{
	Q_UNUSED(size);
	DEBUG_LOG(QStringLiteral("AlphaMemorySystem: clearing reservations for PA=0x%1")
		.arg(physicalAddr, 16, 16, QChar('0')));

	// 1) Clear each CPU's own reservation bits:
	for (AlphaCPU* cpu : m_attachedCpus) {
		cpu->invalidateReservation(physicalAddr, size);
	}

	emit sigReservationCleared(physicalAddr, size);
}
bool SafeMemory::isValidAddress(quint64 address, int size) const
{
	return (address + size <= static_cast<quint64>(m_ram.size()));
}
inline quint8 SafeMemory::readUInt8(quint64 pa, quint64 pc)
{
	QReadLocker L(&m_memoryLock);
	if (!isValidAddress(pa, 1))
		throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS,
			pa, 1, false, pc);
	quint8 v = m_ram[int(pa)];
	emit sigMemoryRead(pa, v, 1);
	return v;
}

void SafeMemory::mapRegion(quint64 address, quint64 size, MemoryPerm perm)
{
	Q_UNUSED(perm); // For now, we don't enforce permissions - just ensure memory exists

	// Calculate the end address we need to support
	quint64 endAddress = address + size;

	// If this region extends beyond current memory, resize to accommodate it
	if (endAddress > static_cast<quint64>(m_ram.size())) {
		resize(endAddress, false); // false = don't reinitialize, preserve existing content
		TRACE_LOG(QString("[SafeMemory::mapRegion] Extended memory to 0x%1 for region at 0x%2, size %3")
			.arg(endAddress, 16, 16, QChar('0'))
			.arg(address, 16, 16, QChar('0'))
			.arg(size));
	}

	// Optionally emit a signal that a region was mapped
	emit sigRegionMapped(address, size, static_cast<quint8>(perm));
}
bool SafeMemory::prefetchHint(quint64 address, int size, bool evictNext) {
	// In a real implementation, this would communicate with the cache subsystem
	// For now, we'll just log the hint and treat it as a successful operation

	// Calculate the cache line address (typically 64-byte aligned in modern systems)
	quint64 cacheLine = address & ~0x3F;  // 64-byte cache line alignment

	// Next sequential cache line that might be evicted
	quint64 nextCacheLine = cacheLine + 64;

	TRACE_LOG(QString("[SafeMemory:prefetchHint()] Hint to prefetch from 0x%1, %2 bytes")
		.arg(address, 0, 16)
		.arg(size));

	if (evictNext) {
		TRACE_LOG(QString("[SafeMemory:prefetchHint()] With EVICT_NEXT modifier for line 0x%1")
			.arg(nextCacheLine, 0, 16));
	}

	// Optional: Emit signal for monitoring (similar to normal reads, but with type indication)
	emit memoryRead(address, evictNext ? 0xFFFFFFFFFFFFFFFF : 0, size);

	// If we have a profiler linked, record the prefetch
	if (m_profiler) {
		m_profiler->recordPrefetch(evictNext);
	}

	return true;
}
inline quint16 SafeMemory::readUInt16(quint64 pa, quint64 pc)
{
	if (pa & 1)                         /* Alpha requires aligned half-words */
		throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT,
			pa, 2, false, pc);

	QReadLocker L(&m_memoryLock);
	if (!isValidAddress(pa, 2))
		throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS,
			pa, 2, false, pc);

	quint16 v = qFromLittleEndian<quint16>(&m_ram[int(pa)]);
	emit sigMemoryRead(pa, v, 2);
	return v;
}
// --- drop-in replacement for the whole function (.H only) ---
inline quint32 SafeMemory::readUInt32(quint64 pa, quint64 pc)
{
	if (pa & 3)
		throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT,
			pa, 4, /*isWrite*/false, pc);

	QReadLocker L(&m_memoryLock);
	if (!isValidAddress(pa, 4))
		throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS,
			pa, 4, /*isWrite*/false, pc);

	quint32 v = qFromLittleEndian<quint32>(&m_ram[int(pa)]);
	emit sigMemoryRead(pa, v, 4);
	return v;
}


/*
Address		Byte		Shift
address + 0	bits 7:0	direct
address + 1	bits 15:8	<< 8
address + 2	bits 23:16	<< 16
address + 3	bits 31:24	<< 24
address + 4	bits 39:32	<< 32
address + 5	bits 47:40	<< 40
address + 6	bits 55:48	<< 48
address + 7	bits 63:56	<< 56

*/

inline quint64 SafeMemory::readUInt64(quint64 pa, quint64 pc)
{
	if (pa & 0x7)
		throw MemoryAccessException(MemoryFaultType::ALIGNMENT_FAULT,
			pa, 8, /*isWrite*/false, pc);

	QReadLocker L(&m_memoryLock);
	if (!isValidAddress(pa, 8))
		throw MemoryAccessException(MemoryFaultType::INVALID_ADDRESS,
			pa, 8, false, pc);

	quint64 v = qFromLittleEndian<quint64>(&m_ram[int(pa)]);
	emit sigMemoryRead(pa, v, 8);
	return v;
}


void SafeMemory::writeUInt8(quint64 address, quint8 value, quint64 pc)
{
	QWriteLocker locker(&m_memoryLock);

	if (!isValidAddress(address, 1)) {
		WARN_LOG(QString("[SafeMemory:writeUInt8()] out of bounds: 0x%1  RAM Size: %2 bytes")
			.arg(address, 0, 16)
			.arg(m_ram.size())
		);
		return;
	}

	try
	{
		m_ram[static_cast<int>(address)] = static_cast<quint8>(value);
	}
	catch (const std::exception& e)
	{
		throw MemoryAccessException(MemoryFaultType::WRITE_ERROR,
			/*isWrite*/ true, 8, address, pc);
	}

	emit sigMemoryWritten(address, value, 1);
}




void SafeMemory::writeBytes(quint64 address, const quint8* data, quint64 size, quint64 pc)
{
	QWriteLocker locker(&m_memoryLock);

	// Ensure the target region is mapped
	if (!isValidAddress(address, static_cast<int>(size))) {
		WARN_LOG(QString("[SafeMemory::writeBytes] out of bounds: 0x%1, size %2, RAM Size: %3 bytes")
			.arg(address, 0, 16)
			.arg(size)
			.arg(m_ram.size()));
		return;
	}

	try {
		// Copy bytes into memory
		for (quint64 i = 0; i < size; ++i) {
			m_ram[static_cast<int>(address + i)] = data[i];
		}

		TRACE_LOG(QString("[SafeMemory::writeBytes] Wrote %1 bytes to 0x%2")
			.arg(size)
			.arg(address, 16, 16, QChar('0')));
	}
	catch (const std::exception& e) {
		throw MemoryAccessException(MemoryFaultType::WRITE_ERROR,
			address, static_cast<int>(size), true, pc);
	}

	// Emit signal for the block write
	emit sigMemoryWritten(address, size, static_cast<int>(size)); // Note: using size as 'value' for block writes
}

// Convenience overload for QByteArray
void SafeMemory::writeBytes(quint64 address, const QByteArray& data, quint64 pc)
{
	writeBytes(address, reinterpret_cast<const quint8*>(data.constData()),
		static_cast<quint64>(data.size()), pc);
}
void SafeMemory::writeUInt16(quint64 address, quint16 value, quint64 pc)
{
	QWriteLocker locker(&m_memoryLock);

	if (!isValidAddress(address, 2)) {
		qWarning() << "[SafeMemory] Write16 out of bounds:"
			<< QString("0x%1").arg(address, 0, 16)
			<< "(RAM Size:" << m_ram.size() << "bytes)";
		return;
	}


	try
	{
		m_ram[static_cast<int>(address)] = static_cast<quint8>(value);
		m_ram[static_cast<int>(address + 1)] = static_cast<quint8>(value >> 8);
	}
	catch (const std::exception& e)
	{
		throw MemoryAccessException(MemoryFaultType::WRITE_ERROR,
			/*isWrite*/ true, 16, address, pc);
	}
	emit sigMemoryWritten(address, value, 2);
}


/*
Address Offset	Data Stored	Notes
+0				bits 7:0	value lowest byte
+1				bits 15:8	shifted 8 bits
+2				bits 23:16	shifted 16 bits
+3				bits 31:24	shifted 24 bits

*/

void SafeMemory::writeUInt32(quint64 address, quint32 value, quint64 pc)
{
	QWriteLocker locker(&m_memoryLock);

	if (!isValidAddress(address, 4)) {
		qWarning() << "[SafeMemory] Write32 out of bounds:"
			<< QString("0x%1").arg(address, 0, 16)
			<< "(RAM Size:" << m_ram.size() << "bytes)";
		return;
	}



	try
	{
		m_ram[static_cast<int>(address)] = static_cast<quint8>(value);
		m_ram[static_cast<int>(address + 1)] = static_cast<quint8>(value >> 8);
		m_ram[static_cast<int>(address + 2)] = static_cast<quint8>(value >> 16);
		m_ram[static_cast<int>(address + 3)] = static_cast<quint8>(value >> 24);

		TRACE_LOG(QString("SafeMemory: Write32 to 0x%1 = 0x%2").arg(address, 8, 16).arg(value, 8, 16));
	}
	catch (const std::exception& e)
	{
		throw MemoryAccessException(MemoryFaultType::WRITE_ERROR,
			/*isWrite*/ true, 32, address, pc);
	}

	emit sigMemoryWritten(address, value, 4);
}


/*
Aspect								Explanation
Locks memory for thread safety	
Bounds checks address	
Reads 4 bytes in Little-Endian order	
Returns 32-bit instruction	
No signal emitted	
*/
quint32 SafeMemory::fetchInstruction(quint64 address)
{
	QReadLocker locker(&m_memoryLock);

	if (!isValidAddress(address, 4)) {
		qWarning() << "[SafeMemory] Instruction fetch out of bounds:"
			<< QString("0x%1").arg(address, 0, 16);
		return 0;
	}

	quint32 instruction = static_cast<quint32>(m_ram[static_cast<int>(address)]) |
		(static_cast<quint32>(m_ram[static_cast<int>(address + 1)]) << 8) |
		(static_cast<quint32>(m_ram[static_cast<int>(address + 2)]) << 16) |
		(static_cast<quint32>(m_ram[static_cast<int>(address + 3)]) << 24);

	return instruction;
}

bool SafeMemory::loadBinary(const QString& filename, quint64 loadAddress)
{
	//TODO: SafeMemory::LoadBinary
	return false;
}

/*
Index	Stored Byte	Explanation
i=0		bits 7:0	Lowest byte
i=1		bits 15:8	Next byte
i=2		bits 23:16	etc.
i=3		bits 31:24
i=4		bits 39:32
i=5		bits 47:40
i=6		bits 55:48
i=7		bits 63:56	Highest byte
*/

void SafeMemory::writeUInt64(quint64 address, quint64 value, quint64 pc)
{
	QWriteLocker locker(&m_memoryLock);

	if (!isValidAddress(address, 8)) {
		qWarning() << "[SafeMemory] Write64 out of bounds:"
			<< QString("0x%1").arg(address, 0, 16)
			<< "(RAM Size:" << m_ram.size() << "bytes)";
		return;
	}

	try
	{
		for (int i = 0; i < 8; ++i) {
			m_ram[static_cast<int>(address) + i] = static_cast<quint8>(value >> (i * 8));
		}

		TRACE_LOG(QString("SafeMemory: Write32 to 0x%1 = 0x%2").arg(address, 8, 16).arg(value, 8, 16));
	}
	catch (const std::exception& e)
	{
		throw MemoryAccessException(MemoryFaultType::WRITE_ERROR,
			/*isWrite*/ true, 64, address, pc);
	}
	
	emit memoryWritten(address, value, 8);
}


void SafeMemory::dumpMemory(quint64 startAddr, quint64 length)  {
	DEBUG_LOG(QString("=== Memory Dump from 0x%1 to 0x%2 ===")
		.arg(QString::number(startAddr, 16))
		.arg(QString::number(startAddr + length - 1, 16)));

	constexpr int BYTES_PER_LINE = 16;

	for (quint64 addr = startAddr; addr < startAddr + length; addr += BYTES_PER_LINE) {
		QString line = QString("0x%1:")
			.arg(QString::number(addr, 16).rightJustified(16, '0'));
		QString ascii;

		for (int i = 0; i < BYTES_PER_LINE && addr + i < startAddr + length; ++i) {
			try {
				quint8 byte = readUInt8(addr + i);
				line += QString(" %1").arg(QString::number(byte, 16).rightJustified(2, '0'));

				// Add ASCII representation
				if (byte >= 32 && byte <= 126) { // Printable ASCII
					ascii += QChar(byte);
				}
				else {
					ascii += '.';
				}
			}
			catch (const std::exception& e) {
				line += " ??";
				ascii += '?';
			}
		}

		// Pad line to ensure consistent formatting
		while (line.length() < 16 + 3 * BYTES_PER_LINE) {
			line += " ";
		}

		line += "  " + ascii;
		DEBUG_LOG(line);
	}
}

void SafeMemory::clear(quint64 startAddress, quint64 size, quint8 value)
{
	QWriteLocker locker(&m_memoryLock);

	quint64 endAddress = startAddress + size;
	if (endAddress > static_cast<quint64>(m_ram.size())) {
		qWarning() << "SafeMemory: Clear region exceeds memory bounds";
		endAddress = static_cast<quint64>(m_ram.size());
	}

	for (quint64 i = startAddress; i < endAddress; ++i) {
		m_ram[static_cast<int>(i)] = value;
	}

	qDebug() << "SafeMemory: Cleared" << (endAddress - startAddress) << "bytes to value"
		<< static_cast<int>(value) << "starting at" << QString("0x%1").arg(startAddress, 0, 16);
}

void SafeMemory::copyMemory(quint64 destination, quint64 source, quint64 size)
{
	QWriteLocker locker(&m_memoryLock);

	if (source + size > static_cast<quint64>(m_ram.size()) ||
		destination + size > static_cast<quint64>(m_ram.size())) {
		qWarning() << "SafeMemory: Copy exceeds memory bounds";
		return;
	}

	// Handle overlapping regions by determining copy direction
	if (destination <= source || destination >= source + size) {
		// Non-overlapping or destination before source - copy forward
		for (quint64 i = 0; i < size; ++i) {
			m_ram[static_cast<int>(destination + i)] = m_ram[static_cast<int>(source + i)];
		}
	}
	else {
		// Overlapping with destination after source - copy backward
		for (quint64 i = size; i > 0; --i) {
			m_ram[static_cast<int>(destination + i - 1)] = m_ram[static_cast<int>(source + i - 1)];
		}
	}

	qDebug() << "SafeMemory: Copied" << size << "bytes from"
		<< QString("0x%1").arg(source, 0, 16) << "to"
		<< QString("0x%1").arg(destination, 0, 16);
}

