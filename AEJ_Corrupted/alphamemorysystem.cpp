#include "AlphaMemorySystem.h"
#include "Helpers.h"
/*
?? Dispatch based on access target (SafeMemory handles MMIO transparently)
??Extend memory model with page-granular mapping?
*/

AlphaMemorySystem::AlphaMemorySystem(QObject* parent)
	: QObject(parent)
{
	m_memorySystem = new SafeMemory(this);
	mmioManager = new MMIOManager();
}

AlphaMemorySystem::~AlphaMemorySystem()
{
}
//deprecated: // VA-> PA Translation Mapping
// bool AlphaMemorySystem::translate(quint64 virtualAddr, quint64& physicalAddr, int accessType)
// {
// 	QMutexLocker locker(&memoryLock);
// 	auto it = memoryMap.lowerBound(virtualAddr);
// 	if (it == memoryMap.end() || virtualAddr < it.key()) {
// 		emit translationMiss(virtualAddr);
// 		return false;
// 	}
// 
// 	physicalAddr = it.value().first + (virtualAddr - it.key());
// 	return true;
// }

bool AlphaMemorySystem::isMMIOAddress(quint64 physicalAddr) 
{
	return mmioManager && mmioManager->isMMIOAddress(physicalAddr);
}

bool AlphaMemorySystem::readVirtualMemory(AlphaCPU* alphaCPU,
	quint64    virtualAddr,
	void* value,
	size_t     size)
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (read access = 0)
	if (!translate(alphaCPU, virtualAddr, physicalAddr, 0)) {
		emit protectionFault(virtualAddr, size);
		value = (void*) 0xFFFFFFFFFFFFFFFFULL; // Fault marker
	}

	// Determine if MMIO or RAM
	if (isMMIOAddress(physicalAddr)) {
		switch (size) {
		case 1: return m_memorySystem->readUInt8(physicalAddr);
		case 2: return m_memorySystem->readUInt16(physicalAddr);
		case 4: return m_memorySystem->readUInt32(physicalAddr);
		case 8: return m_memorySystem->readUInt64(physicalAddr);
		default:
			qWarning() << "[AlphaMemorySystem] Invalid MMIO read size:" << size;
			value = 0;
		}
	}
	else {
		switch (size) {
		case 1: return m_memorySystem->readUInt8(physicalAddr);
		case 2: return m_memorySystem->readUInt16(physicalAddr);
		case 4: return m_memorySystem->readUInt32(physicalAddr);
		case 8: return m_memorySystem->readUInt64(physicalAddr);
		default:
			qWarning() << "[AlphaMemorySystem] Invalid memory read size:" << size;
			value = 0;
		}
	}
	emit memoryRead(virtualAddr, physicalAddr, size);
}
/**
 * @brief Reads a value from virtual memory after MMU translation and protection checks.
 *
 * This function translates a virtual address to a physical address using the current
 * MMU context provided by the specified AlphaCPU. If translation is successful and
 * access is allowed, it reads a value of the requested size from either system RAM
 * or MMIO space and places the result into the referenced output parameter.
 *
 * Emits protection faults or translation misses as needed, based on MMU rules.
 *
 * @param alphaCPU       Pointer to the calling AlphaCPU instance (provides MMU mode, PSR, etc.)
 * @param virtualAddr    The virtual address to be translated and read
 * @param value          Output parameter receiving the loaded value (zero-extended)
 * @param size           Size in bytes of the read (must be 1, 2, 4, or 8)
 * @return true          If the read was successful and data is valid
 * @return false         If a translation or protection fault occurred
 * 
 * Size					Cast Type	    Representing
 *    1					quint8			1-Byte
 *    2					quint16			2-Bytes
 *    2					qint16			2-Bytes
 *    1					quint8			1-Bytes
 *    2					quint16			2-Bytes
 *    8					quint64			8-Bytes
 * 
 * @see AlphaCPU::isMMUEnabled()
 * @see AlphaMemorySystem::translate()
 * @see SafeMemory::readUInt64()
 */
bool AlphaMemorySystem::readVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, quint64& value, int size)
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (read access = 0)
	if (!translate(alphaCPU, virtualAddr, physicalAddr, 0)) {
		emit protectionFault(virtualAddr, size);
		value = 0xFFFFFFFFFFFFFFFFULL; // Fault marker
	}

	// Determine if MMIO or RAM
	if (isMMIOAddress(physicalAddr)) {
		switch (size) {
		case 1: return m_memorySystem->readUInt8(physicalAddr);
		case 2: return m_memorySystem->readUInt16(physicalAddr);
		case 4: return m_memorySystem->readUInt32(physicalAddr);
		case 8: return m_memorySystem->readUInt64(physicalAddr);
		default:
			qWarning() << "[AlphaMemorySystem] Invalid MMIO read size:" << size;
			value = 0;
		}
	}
	else {
		switch (size) {
		case 1: return m_memorySystem->readUInt8(physicalAddr);
		case 2: return m_memorySystem->readUInt16(physicalAddr);
		case 4: return m_memorySystem->readUInt32(physicalAddr);
		case 8: return m_memorySystem->readUInt64(physicalAddr);
		default:
			qWarning() << "[AlphaMemorySystem] Invalid memory read size:" << size;
			value = 0;
		}
	}
	emit memoryRead(virtualAddr, physicalAddr, size);
}

// Clear all memory mappings (virtual-to-physical)
void AlphaMemorySystem::clearMappings()
{
	QWriteLocker locker(&memoryLock);
	memoryMap.clear();
	emit mappingsCleared();
}

// Overload with reference + signal support

//////////////////////////////////////////////////////////////////////////
/// </summary> Write Virtual Memory
/// <param name="virtualAddr"></param> Virtual Addrss
/// <param name="value"></param> 
/// <param name="size"></param> data type size 8/16/32/64

bool AlphaMemorySystem::writeVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, void* value, int size)
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (write access = 1)
	if (!translate(alphaCPU, virtualAddr, physicalAddr, 1)) {
		emit protectionFault(virtualAddr, size);
		return false;
	}

	// Dispatch based on access target (SafeMemory handles MMIO transparently)
	switch (size) {
	case 1: {
		//-m_memorySystem->writeUInt8(physicalAddr, static_cast<quint8>(value));
		auto p = reinterpret_cast<const quint8*>(value);
		m_memorySystem->writeUInt8(physicalAddr, *p);
		break;
	}
	case 2: {
		//-m_memorySystem->writeUInt16(physicalAddr, static_cast<quint16>(value));
		auto p = reinterpret_cast<const quint16*>(value);
		m_memorySystem->writeUInt16(physicalAddr, *p);
		break;
	}
	case 4: {
		//-m_memorySystem->writeUInt32(physicalAddr, static_cast<quint32>(value));
		auto p = reinterpret_cast<const quint32*>(value);
		m_memorySystem->writeUInt32(physicalAddr, *p);
		break;
	}
	case 8: {
		//-m_memorySystem->writeUInt64(physicalAddr, static_cast<quint64>(value));
		auto p = reinterpret_cast<const quint64*>(value);
		m_memorySystem->writeUInt64(physicalAddr, *p);
		break;
	}
	default:
		qWarning() << "[AlphaMemorySystem] Invalid memory write size:" << size;
		return false;
	}
	return true;
}
bool AlphaMemorySystem::writeVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, quint64 value, int size)
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (write access = 1)
	if (!translate(alphaCPU,virtualAddr, physicalAddr, 1)) {
		emit protectionFault(virtualAddr, size);
		return false;
	}

	// Dispatch based on access target (SafeMemory handles MMIO transparently)
	switch (size) {
	case 1: m_memorySystem->writeUInt8(physicalAddr, static_cast<quint8>(value)); break;
	case 2: m_memorySystem->writeUInt16(physicalAddr, static_cast<quint16>(value)); break;
	case 4: m_memorySystem->writeUInt32(physicalAddr, static_cast<quint32>(value)); break;
	case 8: m_memorySystem->writeUInt64(physicalAddr, static_cast<quint64>(value)); break;
	default:
		qWarning() << "[AlphaMemorySystem] Invalid memory write size:" << size;
		return false;
	}
	return true;
}

bool AlphaMemorySystem::isMapped(quint64 vaddr) const
{
	QMutexLocker locker(&memoryLock);
	return memoryMap.contains(vaddr);
}

bool AlphaMemorySystem::checkAccess(quint64 vaddr, int accessType) const
{
	QMutexLocker locker(&memoryLock);

	auto it = memoryMap.lowerBound(vaddr);
	if (it == memoryMap.constEnd() || vaddr < it.key() || vaddr >= (it.key() + it.value().size)) {
		return false;
	}

	const MappingEntry& entry = it.value();
	return (entry.protectionFlags & accessType) == accessType;
}
void AlphaMemorySystem::mapMemory(quint64 virtualAddr, quint64 physicalAddr, quint64 size, int protectionFlags)
{
	QMutexLocker locker(&memoryLock);
	MappingEntry entry;
	entry.physicalBase = physicalAddr;
	entry.size = size;
	entry.protectionFlags = protectionFlags;
	memoryMap.insert(virtualAddr, entry);
}

void AlphaMemorySystem::unmapMemory(quint64 virtualAddr)
{
	QMutexLocker locker(&memoryLock);
	memoryMap.remove(virtualAddr);
}

QVector<QPair<quint64, MappingEntry>> AlphaMemorySystem::getMappedRegions() const
{
	QMutexLocker locker(&memoryLock);

	QVector<QPair<quint64, MappingEntry>> regions;
	for (auto it = memoryMap.constBegin(); it != memoryMap.constEnd(); ++it) {
		regions.append(qMakePair(it.key(), it.value()));
	}
	return regions;
}




/*
* Permissions BitMask
	Bit	Meaning
	0x1	Readable
	0x2	Writable
	0x4	Executable
*/
// VA-> PA Translation Mapping

bool AlphaMemorySystem::translate(AlphaCPU* alphaCpu, quint64 virtualAddr, quint64& physicalAddr, int accessType)
{
	QMutexLocker locker(&memoryLock);

	if (!alphaCpu || !alphaCpu->isMMUEnabled()) {
		physicalAddr = virtualAddr;  // 1:1 mapping
		return true;
	}

	auto it = memoryMap.lowerBound(virtualAddr);
	if (it == memoryMap.end() || virtualAddr >= (it.key() + it.value().size)) {
		emit translationMiss(virtualAddr);
		return false;
	}

	const MappingEntry& entry = it.value();

	if (accessType == 0 && !(entry.protectionFlags & 0x1)) {
		emit protectionFault(virtualAddr, accessType);
		return false;
	}
	if (accessType == 1 && !(entry.protectionFlags & 0x2)) {
		emit protectionFault(virtualAddr, accessType);
		return false;
	}
	if (accessType == 2 && !(entry.protectionFlags & 0x4)) {
		emit protectionFault(virtualAddr, accessType);
		return false;
	}

	physicalAddr = entry.physicalBase + (virtualAddr - it.key());
	return true;
}



void AlphaMemorySystem::initialize_signalsAndSlots()
{

}
