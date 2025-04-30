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


quint64 AlphaMemorySystem::readVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, int size)
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (read access = 0)
	if (!translate(alphaCPU, virtualAddr, physicalAddr, 0)) {
		emit protectionFault(virtualAddr, size);
		return 0xFFFFFFFFFFFFFFFFULL; // Fault marker
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
			return 0;
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
			return 0;
		}
	}
	emit memoryRead(virtualAddr,physicalAddr,size);
}
bool AlphaMemorySystem::readVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, quint32& value, int size)
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

// JSON output of mapped regions for debugging
QJsonDocument AlphaMemorySystem::getMappedRegionsJson() const
{
	QReadLocker locker(&memoryLock);
	QJsonArray regionArray;

	for (auto it = memoryMap.constBegin(); it != memoryMap.constEnd(); ++it) {
		QJsonObject region;
		region["virtual"] = QString("0x%1").arg(it.key(), 0, 16);
		region["physical"] = QString("0x%1").arg(it.value().physicalBase, 0, 16);
		region["size"] = it.value().size;
		region["permissions"] = it.value().protectionFlags;
		regionArray.append(region);
	}

	QJsonObject result;
	result["mappings"] = regionArray;
	return QJsonDocument(result);
}

// Overload with reference + signal support
bool AlphaMemorySystem::readVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, quint32& value, int size)
{
	quint64 physicalAddr;
	if (!translate(alphaCPU, virtualAddr, physicalAddr, 0)) {
		emit translationMiss(virtualAddr);
		return false;
	}

	value = 0;
	switch (size) {
	case 1: value = m_memorySystem->readUInt8(physicalAddr); break;
	case 2: value = m_memorySystem->readUInt16(physicalAddr); break;
	case 4: value = m_memorySystem->readUInt32(physicalAddr); break;
	default:
		qWarning() << "[AlphaMemorySystem] Invalid read size:" << size;
		return false;
	}

	emit memoryRead(physicalAddr, value, size);
	return true;
}

//////////////////////////////////////////////////////////////////////////
/// </summary> Write Virtual Memory
/// <param name="virtualAddr"></param> Virtual Addrss
/// <param name="value"></param> 
/// <param name="size"></param> data type size 8/16/32/64

void AlphaMemorySystem::writeVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, quint64 value, int size)
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (write access = 1)
	if (!translate(alphaCPU,virtualAddr, physicalAddr, 1)) {
		emit protectionFault(virtualAddr, size, /*isWrite=*/true);
		return;
	}

	// Dispatch based on access target (SafeMemory handles MMIO transparently)
	switch (size) {
	case 1: m_memorySystem->writeUInt8(physicalAddr, static_cast<quint8>(value)); break;
	case 2: m_memorySystem->writeUInt16(physicalAddr, static_cast<quint16>(value)); break;
	case 4: m_memorySystem->writeUInt32(physicalAddr, static_cast<quint32>(value)); break;
	case 8: m_memorySystem->writeUInt64(physicalAddr, static_cast<quint64>(value)); break;
	default:
		qWarning() << "[AlphaMemorySystem] Invalid memory write size:" << size;
		return;
	}
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

bool AlphaMemorySystem::translate(AlphaCPU* cpu, quint64 virtualAddr, quint64& physicalAddr, int accessType)
{
	QMutexLocker locker(&memoryLock);

	if (!cpu || !cpu->isMMUEnabled()) {
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
