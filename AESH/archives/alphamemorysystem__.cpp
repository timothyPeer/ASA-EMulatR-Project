#include "AlphaMemorySystem.h"
#include <QtGlobal>
#include <QMutexLocker>
#include <QDateTime>
#include "../AESH/AlphaCPU.h"
#include "../AEJ/TranslationResult.h"
#include "../AEJ/AlphaMemorySystemConstants.h"
#include "../AEJ/constants/constAlphaMemorySystem.h"
#include "../AEJ/structures/structProbeResult.h"
#include "../AEJ/AlphaMMIOAddressChecker.h"
#include "../AEJ/GlobalLockTracker.h"
#include "../AESH/GlobalMacro.h"


AlphaMemorySystem::AlphaMemorySystem(QObject* parent /*= nullptr*/) 
	:  QObject(parent)
{
    initialize();
}

AlphaMemorySystem::~AlphaMemorySystem()
{
}



/**
 * @brief Check if a physical address maps to MMIO space
 * @param physicalAddr Physical address to check
 * @return True if address is in MMIO range
 */
bool MMIOManager::isMMIOAddress(quint64 pa) const
{
	QReadLocker L(&lock);
	for (auto& w : mmioWindows)
		if (pa >= w.base && pa < w.base + w.size)
			return true;
	return false;
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
void AlphaMemorySystem::clearReservations(quint64 physicalAddr, int size)
{
    Q_UNUSED(size);
    DEBUG_LOG(QStringLiteral("AlphaMemorySystem: clearing reservations for PA=0x%1")
        .arg(physicalAddr, 16, 16, QChar('0')));

    // 1) Clear each CPU's own reservation bits:
    for (AlphaCPU *cpu : m_attachedCpus) {
        cpu->invalidateReservation(physicalAddr, size);
    }

    // 2) Tell the global tracker:
    GlobalLockTracker::invalidate(physicalAddr);
}



// Read from virtual memory a reinterpreted cast of value to the integer data type based on the size_t.
bool AlphaMemorySystem::readVirtualMemory( quint64 virtualAddr, void* value, quint16 size, quint64 pc )
{
	quint64 physicalAddr;

	// Translate virtual address to physical address (read access = 0)
	if (!translate(virtualAddr, physicalAddr, 0)) {
		//emit protectionFault(virtualAddr, size);
		emit sigProtectionFault(virtualAddr, 0); // read access
		emit sigTranslationMiss(virtualAddr);
		if (value) std::memset(value, 0xFF, size);  // mark as fault
		return false;
	}

	bool success = false;

	if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
		quint64 tmp = m_mmioManager->readMMIO(physicalAddr, size);
		if (value) std::memcpy(value, &tmp, size);
	}
	else {
		switch (size) {
		case 1: *reinterpret_cast<quint8*>(value) = m_safeMemory->readUInt8(physicalAddr, pc);  success = true; break;
		case 2: *reinterpret_cast<quint16*>(value) = m_safeMemory->readUInt16(physicalAddr, pc); success = true; break;
		case 4: *reinterpret_cast<quint32*>(value) = m_safeMemory->readUInt32(physicalAddr, pc); success = true; break;
		case 8: *reinterpret_cast<quint64*>(value) = m_safeMemory->readUInt64(physicalAddr, pc); success = true; break;
		default:
			qWarning() << "[AlphaMemorySystem] Invalid memory read size:" << size;
			if (value) std::memset(value, 0, size);
			return false;
		}
	}

	if (success) {
		emit sigMemoryRead(virtualAddr, physicalAddr, size);
	}

	return success;
}




// Overload with reference + signal support

//////////////////////////////////////////////////////////////////////////
/// </summary> Write Virtual Memory
/// <param name="virtualAddr"></param> Virtual Addrss
/// <param name="value"></param> 
/// <param name="size"></param> data type size 8/16/32/64

bool AlphaMemorySystem::writeVirtualMemory( quint64 virtualAddr, void* value, int size, quint64 pc )
{
	QWriteLocker locker(&memoryLock);     // in read‑only methods
	quint64 physicalAddr;
	
	// Translate virtual address to physical address (write access = 1)
	if (!translate(virtualAddr, physicalAddr, 1)) {
		//emit protectionFault(virtualAddr, size);
		emit sigProtectionFault(virtualAddr, 1); // write access
		emit sigTranslationMiss(virtualAddr);
		return false;
	}

	// Dispatch based on access target (SafeMemory handles MMIO transparently)
	switch (size) {
	case 1: {
		//-m_memorySystem->writeUInt8(physicalAddr, static_cast<quint8>(value));
		auto p = reinterpret_cast<const quint8*>(value);
		m_safeMemory->writeUInt8(physicalAddr, *p, pc);
		break;
	}
	case 2: {
		//-m_memorySystem->writeUInt16(physicalAddr, static_cast<quint16>(value));
		auto p = reinterpret_cast<const quint16*>(value);
		m_safeMemory->writeUInt16(physicalAddr, *p, pc);
		break;
	}
	case 4: {
		//-m_memorySystem->writeUInt32(physicalAddr, static_cast<quint32>(value));
		auto p = reinterpret_cast<const quint32*>(value);
		m_safeMemory->writeUInt32(physicalAddr, *p, pc);
		break;
	}
	case 8: {
		//-m_memorySystem->writeUInt64(physicalAddr, static_cast<quint64>(value));
		auto p = reinterpret_cast<const quint64*>(value);
		m_safeMemory->writeUInt64(physicalAddr, *p, pc);
		break;
	}
	default:
		qWarning() << "[AlphaMemorySystem] Invalid memory write size:" << size;
		return false;
	}
	return true;
}
bool AlphaMemorySystem::writeVirtualMemory( quint64 virtualAddr, quint64 value, int size, quint64 pc)
{
	QWriteLocker locker(&memoryLock);     // in read‑only methods
	quint64 physicalAddr;

	// Translate virtual address to physical address (write access = 1)
	if (!translate(virtualAddr, physicalAddr, 1)) {
		//emit protectionFault(virtualAddr, size);
		emit sigProtectionFault(virtualAddr, 1); // write access
		emit sigTranslationMiss(virtualAddr);
		return false;
	}

	// Dispatch based on access target (SafeMemory handles MMIO transparently)
	switch (size) {
	case 1: m_safeMemory->writeUInt8(physicalAddr, static_cast<quint8>(value), pc); break;
	case 2: m_safeMemory->writeUInt16(physicalAddr, static_cast<quint16>(value), pc); break;
	case 4: m_safeMemory->writeUInt32(physicalAddr, static_cast<quint32>(value), pc); break;
	case 8: m_safeMemory->writeUInt64(physicalAddr, static_cast<quint64>(value), pc); break;
	default:
		qWarning() << "[AlphaMemorySystem] Invalid memory write size:" << size;
		return false;
	}
	return true;
}
bool AlphaMemorySystem::readBlock(quint64 physicalAddr, void* buffer, size_t size, quint64 pc)
{

	QReadLocker locker(&memoryLock);
 

	if (!buffer || size == 0) return false;

	if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
		bool success = false;
		quint64 mmioVal = m_mmioManager->readMMIO(physicalAddr, static_cast<int>(size));
		if (success) {
			std::memcpy(buffer, &mmioVal, size);
			emit sigMemoryRead(physicalAddr, physicalAddr, static_cast<int>(size));
		}
		return success;
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			static_cast<quint8*>(buffer)[i] = m_safeMemory->readUInt8(physicalAddr + i,pc);
		}
		emit sigMemoryRead(physicalAddr, physicalAddr, static_cast<int>(size));
		return true;
	}
}

bool AlphaMemorySystem::writeBlock(quint64 physicalAddr, const void* buffer, size_t size, quint64 pc )
{
	QWriteLocker locker(&memoryLock);
	if (!buffer || size == 0) return false;

	if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
		quint64 value = 0;
		std::memcpy(&value, buffer, size);
		bool success = m_mmioManager->writeMMIO(physicalAddr, value, static_cast<int>(size));
		if (success)
			emit sigMemoryWritten(physicalAddr, physicalAddr, static_cast<int>(size));
		return success;
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			m_safeMemory->writeUInt8(physicalAddr + i, static_cast<const quint8*>(buffer)[i],  pc);
		}
		emit sigMemoryWritten(physicalAddr, physicalAddr, static_cast<int>(size));
		return true;
	}
}

bool AlphaMemorySystem::isMMIOAddress(quint64 physicalAddr) const
{
	QReadLocker locker(&m_memoryLock);

	// Fast path: Use inline chipset-specific check first
	// This covers standard Alpha MMIO regions (I/O hose 0x4-0x7)
	if (isMMIO(physicalAddr)) {
		return true;
	}

	// Detailed path: Check with MMIO manager for device-specific mappings
	if (m_mmioManager) {
		if (m_mmioManager->isMMIOAddress(physicalAddr)) {
			return true;
		}
	}

	// CPU model-specific MMIO regions
	if (AlphaMMIOAddressChecker::isMMIOAddress(physicalAddr, m_cpuModel)) {
		return true;
	}

	return false;
}

bool AlphaMemorySystem::isMapped(quint64 vaddr) const
{
	QReadLocker locker(&memoryLock);
	return memoryMap.contains(vaddr);
}

bool AlphaMemorySystem::checkAccess(quint64 vaddr, int accessType) const
{
	QReadLocker locker(&memoryLock);

	auto it = memoryMap.lowerBound(vaddr);
	if (it == memoryMap.constEnd() || vaddr < it.key() || vaddr >= (it.key() + it.value().size)) {
		return false;
	}

	const MappingEntry& entry = it.value();
	return (entry.protectionFlags & accessType) == accessType;
}
void AlphaMemorySystem::mapMemory(quint64 virtualAddr, quint64 physicalAddr, quint64 size, int protectionFlags)
{
	QReadLocker locker(&memoryLock);
	MappingEntry entry;
	entry.physicalBase = physicalAddr;
	entry.size = size;
	entry.protectionFlags = protectionFlags;
	memoryMap.insert(virtualAddr, entry);
}

void AlphaMemorySystem::unmapMemory(quint64 virtualAddr)
{
	QReadLocker locker(&memoryLock);
	memoryMap.remove(virtualAddr);
}

QVector<QPair<quint64, MappingEntry>> AlphaMemorySystem::getMappedRegions() const
{
	QReadLocker locker(&memoryLock);

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

bool AlphaMemorySystem::translate( quint64 virtualAddr, quint64& physicalAddr, int accessType)
{
	QReadLocker locker(&memoryLock);

	// Step 1: PAL Mode short-circuit (1:1 mapping, restricted range)
	if (m_cpu && m_cpu->inPALMode()) {
		physicalAddr = virtualAddr;
		if (!isPALVisible(physicalAddr)) {
			emit sigProtectionFault(virtualAddr, accessType);
			return false;
		}
		return true;
	}

	// Step 2: MMU disabled => 1:1 mapping
	if (!m_cpu || !m_cpu->isMMUEnabled()) {
		physicalAddr = virtualAddr;
		return true;
	}

	// Step 3: Lookup translation from memory map
	auto it = memoryMap.lowerBound(virtualAddr);
	if (it == memoryMap.end() || virtualAddr >= (it.key() + it.value().size)) {
		emit sigTranslationMiss(virtualAddr);
		return false;
	}

	const MappingEntry& entry = it.value();

	// Step 4: Access protection
	if ((accessType == 0 && !(entry.protectionFlags & 0x1)) ||  // Read
		(accessType == 1 && !(entry.protectionFlags & 0x2)) ||  // Write
		(accessType == 2 && !(entry.protectionFlags & 0x4))) {  // Execute
		emit sigProtectionFault(virtualAddr, accessType);
		return false;
	}

	// Step 5: Calculate final physical address
	physicalAddr = entry.physicalBase + (virtualAddr - it.key());
	return true;
}

void AlphaMemorySystem::initialize() {

    m_mmioManager->initializeCpuModel(m_cpuModel);
	m_attachedCpus.reserve(4);
}
void AlphaMemorySystem::initialize_AlphaCPUsignalsAndSlots()
{
	Q_ASSERT(m_cpu);

	connect(this, &AlphaMemorySystem::sigMemoryRead,
		m_cpu, &AlphaCPU::onMemoryRead);

	connect(this, &AlphaMemorySystem::sigMemoryWritten,
		m_cpu, &AlphaCPU::onMemoryWritten);

	connect(this, &AlphaMemorySystem::sigProtectionFault,
		m_cpu, &AlphaCPU::onProtectionFault);

	connect(this, &AlphaMemorySystem::sigTranslationMiss,
		m_cpu, &AlphaCPU::onTranslationMiss);

	connect(this, &AlphaMemorySystem::mappingsCleared,
		m_cpu, &AlphaCPU::onMappingsCleared);

	connect(this, &AlphaMemorySystem::sigAllCPUsPaused,
		m_cpu, &AlphaCPU::onAllCPUsPaused);

	connect(this, &AlphaMemorySystem::sigAllCPUsStarted,
		m_cpu, &AlphaCPU::onAllCPUsStarted);

	connect(this, &AlphaMemorySystem::sigAllCPUsStopped,
		m_cpu, &AlphaCPU::onAllCPUsStopped);

// 	connect(this, &AlphaMemorySystem::cpuProgress,
// 		m_cpu, &AlphaCPU::onCpuProgress);
}

/**
	* @brief Check TLB without causing exceptions (delegated to TLBSystem)
	* @param virtualAddr Virtual address to check
	* @param asn Address Space Number
	* @param isKernelMode True if kernel mode check
	* @return Encoded TLB check result
	*/
quint64 AlphaMemorySystem::checkTB(quint64 virtualAddr, quint64 asn, bool isKernelMode);
// Clear all memory mappings (virtual-to-physical)
void AlphaMemorySystem::clearMappings()
{
	QWriteLocker locker(&memoryLock);
	memoryMap.clear();
	emit sigMappingsCleared();
}

/*
from Alpha CPU
*/

void AlphaMemorySystem::onAllCPUsPaused() {
	qDebug() << "[AlphaCPU] All CPUs paused.";
}

void AlphaMemorySystem::onAllCPUsStarted() {
	qDebug() << "[AlphaCPU] All CPUs started.";
}

void AlphaMemorySystem::onAllCPUsStopped() {
	qDebug() << "[AlphaCPU] All CPUs stopped.";
}

void AlphaMemorySystem::sigCpuProgress(int cpuId, QString _txt)
{
	//TODO cpuProgress(int cpuId, QString _txt)
}

// Add proper memory barrier implementation
void AlphaMemorySystem::sigExecuteMemoryBarrier(int type) {
	// Implement full Alpha memory barrier semantics
	// ...
}

// Add cache protocol support
void AlphaMemorySystem::sigHandleCacheState(quint64 physicalAddr, int state, int cpuId) {
	// Track cache state for coherency
	// ...
}
#pragma region Helpers Buffer Support


/*
// Add these method declarations to the public section of AlphaMemorySystem class:

public:
    // Write buffer management
    void flushWriteBuffers();
    void flushWriteBuffers(quint64 startAddr, quint64 endAddr);

    // Privileged memory operations
    bool writeVirtualMemoryPrivileged(AlphaCPU* alphaCPU, quint64 virtualAddr, quint64 value, int size);
    bool writeVirtualMemoryPrivileged(AlphaCPU* alphaCPU, quint64 virtualAddr, void* value, int size);

    // Additional privileged operations
    bool readVirtualMemoryPrivileged(AlphaCPU* alphaCPU, quint64 virtualAddr, quint64& value, int size);
    bool readVirtualMemoryPrivileged(AlphaCPU* alphaCPU, quint64 virtualAddr, void* value, int size);

private:
    // Write buffer management (if implementing store buffers)
    struct WriteBufferEntry {
        quint64 physicalAddr;
        quint64 value;
        quint64 timestamp;
        int size;
        bool pending;
    };

    QVector<WriteBufferEntry> m_writeBuffer;
    QMutex m_writeBufferMutex;
    quint64 m_writeBufferTimestamp = 0;

    // Write buffer helper methods
    void addToWriteBuffer(quint64 physicalAddr, quint64 value, int size);
    void processWriteBuffer();
    bool isWriteBufferFull() const;
    void drainWriteBuffer();
*/

/**
 * @brief Flush all pending write buffers to memory
 *
 * Alpha processors may buffer writes for performance. This method ensures
 * all pending writes are committed to physical memory. This is essential
 * for memory barrier operations (MB, WMB instructions) and certain
 * cache operations.
 */
void AlphaMemorySystem::flushWriteBuffers()
{
    QMutexLocker bufferLocker(&m_writeBufferMutex);
    QWriteLocker memoryLocker(&memoryLock);

    DEBUG_LOG("AlphaMemorySystem: Flushing all write buffers");

    // If we're implementing store buffers, drain them now
    if (!m_writeBuffer.isEmpty()) {
        drainWriteBuffer();
    }

    // Ensure all MMIO writes are flushed
    if (m_mmioManager) {
        m_mmioManager->flushWrites();
    }

    // Ensure all memory writes are committed
    if (m_safeMemory) {
        m_safeMemory->flushWrites();
    }

    emit sigMemoryBarrierComplete();
    DEBUG_LOG("AlphaMemorySystem: Write buffer flush completed");
}

/**
 * @brief Flush write buffers for a specific address range
 * @param startAddr Starting virtual address of range to flush
 * @param endAddr Ending virtual address of range to flush
 *
 * This allows selective flushing for performance optimization.
 * Useful for cache line-specific operations.
 */
void AlphaMemorySystem::flushWriteBuffers(quint64 startAddr, quint64 endAddr)
{
    QMutexLocker bufferLocker(&m_writeBufferMutex);
    QWriteLocker memoryLocker(&memoryLock);

    DEBUG_LOG(QString("AlphaMemorySystem: Flushing write buffers for range 0x%1-0x%2")
        .arg(startAddr, 16, 16, QChar('0'))
        .arg(endAddr, 16, 16, QChar('0')));

    // Flush writes in the specified virtual address range
    for (auto it = m_writeBuffer.begin(); it != m_writeBuffer.end();) {
        // Convert physical address back to virtual for range check
        // This is a simplified approach - real implementation would need
        // reverse translation or range tracking
        if (it->physicalAddr >= startAddr && it->physicalAddr <= endAddr) {
            if (it->pending) {
                // Commit this write immediately
                commitWriteEntry(*it);
                it = m_writeBuffer.erase(it);
            }
            else {
                ++it;
            }
        }
        else {
            ++it;
        }
    }

    emit sigMemoryBarrierComplete();
    DEBUG_LOG("AlphaMemorySystem: Range write buffer flush completed");
}

/**
 * @brief Write to virtual memory with privileged access
 * @param alphaCPU CPU context for MMU and privilege checking
 * @param virtualAddr Virtual address to write to
 * @param value Value to write
 * @param size Size of write (1, 2, 4, or 8 bytes)
 * @return True if write succeeded, false on fault
 *
 * This method bypasses normal user/kernel privilege checks and allows
 * writing to any mapped virtual address. Used by PAL code, kernel
 * operations, and system initialization.
 */
bool AlphaMemorySystem::writeVirtualMemoryPrivileged( quint64 virtualAddr, quint64 value, int size, quint64 pc )
{
    QWriteLocker locker(&memoryLock);
    quint64 physicalAddr;

    DEBUG_LOG(QString("AlphaMemorySystem: Privileged write VA=0x%1, value=0x%2, size=%3")
        .arg(virtualAddr, 16, 16, QChar('0'))
        .arg(value, 16, 16, QChar('0'))
        .arg(size));

    // Privileged translation - bypasses protection checks
    if (!translatePrivileged( virtualAddr, physicalAddr)) {
        DEBUG_LOG(QString("AlphaMemorySystem: Privileged translation failed for VA=0x%1")
            .arg(virtualAddr, 16, 16, QChar('0')));
        emit sigTranslationMiss(virtualAddr);
        return false;
    }

    // Perform the privileged write
    bool success = false;

    if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
        // MMIO write with privileged access
        success = m_mmioManager->writeMMIOPrivileged(physicalAddr, value, size);
        DEBUG_LOG(QString("AlphaMemorySystem: Privileged MMIO write PA=0x%1")
            .arg(physicalAddr, 16, 16, QChar('0')));
    }
    else {
        // Regular memory write
        switch (size) {
        case 1:
            m_safeMemory->writeUInt8(physicalAddr, static_cast<quint8>(value), pc);
            success = true;
            break;
        case 2:
            m_safeMemory->writeUInt16(physicalAddr, static_cast<quint16>(value), pc);
            success = true;
            break;
        case 4:
            m_safeMemory->writeUInt32(physicalAddr, static_cast<quint32>(value), pc);
            success = true;
            break;
        case 8:
            m_safeMemory->writeUInt64(physicalAddr, static_cast<quint64>(value), pc);
            success = true;
            break;
        default:
            qWarning() << "[AlphaMemorySystem] Invalid privileged write size:" << size;
            return false;
        }
    }

    if (success) {
        emit sigMemoryWritten(virtualAddr, physicalAddr, size);
        DEBUG_LOG(QString("AlphaMemorySystem: Privileged write completed PA=0x%1")
            .arg(physicalAddr, 16, 16, QChar('0')));
    }

    return success;
}

/**
 * @brief Write to virtual memory with privileged access (void* overload)
 * @param alphaCPU CPU context for MMU
 * @param virtualAddr Virtual address to write to
 * @param value Pointer to data to write
 * @param size Size of data to write
 * @return True if write succeeded, false on fault
 */
bool AlphaMemorySystem::writeVirtualMemoryPrivileged(quint64 virtualAddr, void* value, int size, quint64 pc )
{
    if (!value) {
        qWarning() << "[AlphaMemorySystem] Null pointer in privileged write";
        return false;
    }

    QWriteLocker locker(&memoryLock);
    quint64 physicalAddr;

    // Privileged translation - bypasses protection checks
    if (!translatePrivileged( virtualAddr, physicalAddr)) {
        emit sigTranslationMiss(virtualAddr);
        return false;
    }

    // Perform the privileged write with typed access
    bool success = false;

    if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
        // Convert to value for MMIO
        quint64 mmioValue = 0;
        std::memcpy(&mmioValue, value, qMin(size, 8));
        success = m_mmioManager->writeMMIOPrivileged(physicalAddr, mmioValue, size);
    }
    else {
        // Direct memory write
        switch (size) {
        case 1: {
            const quint8* p = reinterpret_cast<const quint8*>(value);
            m_safeMemory->writeUInt8(physicalAddr, *p, pc);
            success = true;
            break;
        }
        case 2: {
            const quint16* p = reinterpret_cast<const quint16*>(value);
            m_safeMemory->writeUInt16(physicalAddr, *p, pc);
            success = true;
            break;
        }
        case 4: {
            const quint32* p = reinterpret_cast<const quint32*>(value);
            m_safeMemory->writeUInt32(physicalAddr, *p, pc);
            success = true;
            break;
        }
        case 8: {
            const quint64* p = reinterpret_cast<const quint64*>(value);
            m_safeMemory->writeUInt64(physicalAddr, *p, pc);
            success = true;
            break;
        }
        default:
            qWarning() << "[AlphaMemorySystem] Invalid privileged write size:" << size;
            return false;
        }
    }

    if (success) {
        emit sigMemoryWritten(virtualAddr, physicalAddr, size);
    }

    return success;
}



/**
 * @brief Read from virtual memory with privileged access (void* overload)
 * @param alphaCPU CPU context for MMU
 * @param virtualAddr Virtual address to read from
 * @param value Pointer to buffer for read data
 * @param size Size of data to read
 * @return True if read succeeded, false on fault
 */
bool AlphaMemorySystem::readVirtualMemoryPrivileged(quint64 virtualAddr, void* value, int size, quint64 pc)
{
    if (!value) {
        qWarning() << "[AlphaMemorySystem] Null pointer in privileged read";
        return false;
    }

    QReadLocker locker(&memoryLock);
    quint64 physicalAddr;

    // Privileged translation - bypasses protection checks
    if (!translatePrivileged(m_cpu, virtualAddr, physicalAddr)) {
        emit translationMiss(virtualAddr);
        std::memset(value, 0xFF, size);  // Mark as fault
        return false;
    }

    bool success = false;

    if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
        quint64 mmioValue = m_mmioManager->readMMIOPrivileged(physicalAddr, size);
        std::memcpy(value, &mmioValue, size);
        success = true;
    }
    else {
        switch (size) {
        case 1: *reinterpret_cast<quint8*>(value) = m_safeMemory->readUInt8(physicalAddr, pc); success = true; break;
        case 2: *reinterpret_cast<quint16*>(value) = m_safeMemory->readUInt16(physicalAddr, pc); success = true; break;
        case 4: *reinterpret_cast<quint32*>(value) = m_safeMemory->readUInt32(physicalAddr, pc); success = true; break;
        case 8: *reinterpret_cast<quint64*>(value) = m_safeMemory->readUInt64(physicalAddr, pc); success = true; break;
        default:
            qWarning() << "[AlphaMemorySystem] Invalid privileged read size:" << size;
            std::memset(value, 0, size);
            return false;
        }
    }

    if (success) {
        emit memoryRead(virtualAddr, physicalAddr, size);
    }

    return success;
}

/**
 * @brief Read from virtual memory with privileged access
 * @param alphaCPU CPU context for MMU
 * @param virtualAddr Virtual address to read from
 * @param value Reference to store read value
 * @param size Size of read (1, 2, 4, or 8 bytes)
 * @return True if read succeeded, false on fault
 */
bool AlphaMemorySystem::readVirtualMemoryPrivileged(quint64 virtualAddr, quint64& value, int size, quint64 pc)
{
    QReadLocker locker(&memoryLock);
    quint64 physicalAddr;

    // Privileged translation - bypasses protection checks
    if (!translatePrivileged( virtualAddr, physicalAddr)) {
        emit sigTranslationMiss(virtualAddr);
        value = 0xFFFFFFFFFFFFFFFFULL;
        return false;
    }

    bool success = false;

    if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr)) {
        value = m_mmioManager->readMMIOPrivileged(physicalAddr, size);
        success = true;
    }
    else {
        switch (size) {
        case 1: value = m_safeMemory->readUInt8(physicalAddr, pc); success = true; break;
        case 2: value = m_safeMemory->readUInt16(physicalAddr, pc); success = true; break;
        case 4: value = m_safeMemory->readUInt32(physicalAddr, pc); success = true; break;
        case 8: value = m_safeMemory->readUInt64(physicalAddr, pc); success = true; break;
        default:
            qWarning() << "[AlphaMemorySystem] Invalid privileged read size:" << size;
            value = 0;
            return false;
        }
    }

    if (success) {
        emit sigMemoryRead(virtualAddr, physicalAddr, size);
    }

    return success;
}

// ═══════════════════════════════════════════════════════════════════════════
// WRITE BUFFER IMPLEMENTATION (OPTIONAL ADVANCED FEATURE)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Add entry to write buffer for deferred processing
 * @param physicalAddr Physical address of write
 * @param value Value to write
 * @param size Size of write
 */
void AlphaMemorySystem::addToWriteBuffer(quint64 physicalAddr, quint64 value, int size)
{
    QMutexLocker locker(&m_writeBufferMutex);

    // Check if buffer is full
    if (isWriteBufferFull()) {
        // Force drain oldest entries
        drainWriteBuffer();
    }

    WriteBufferEntry entry;
    entry.physicalAddr = physicalAddr;
    entry.value = value;
    entry.timestamp = ++m_writeBufferTimestamp;
    entry.size = size;
    entry.pending = true;

    m_writeBuffer.append(entry);

    DEBUG_LOG(QString("AlphaMemorySystem: Added to write buffer PA=0x%1, value=0x%2")
        .arg(physicalAddr, 16, 16, QChar('0'))
        .arg(value, 16, 16, QChar('0')));
}



/**
 * @brief Process all entries in write buffer
 */
void AlphaMemorySystem::processWriteBuffer()
{
    QMutexLocker locker(&m_writeBufferMutex);

    for (auto& entry : m_writeBuffer) {
        if (entry.pending) {
            commitWriteEntry(entry);
            entry.pending = false;
        }
    }

    // Remove completed entries
    m_writeBuffer.erase(
        std::remove_if(m_writeBuffer.begin(), m_writeBuffer.end(),
            [](const WriteBufferEntry& e) { return !e.pending; }),
        m_writeBuffer.end());
}

/**
 * @brief Check if write buffer is full
 * @return True if buffer should be drained
 */
bool AlphaMemorySystem::isWriteBufferFull() const
{
    const int MAX_WRITE_BUFFER_SIZE = 32;  // Configurable
    return m_writeBuffer.size() >= MAX_WRITE_BUFFER_SIZE;
}

/**
 * @brief Drain write buffer immediately
 */
void AlphaMemorySystem::drainWriteBuffer()
{
    // Note: Caller must hold m_writeBufferMutex

    for (auto& entry : m_writeBuffer) {
        if (entry.pending) {
            commitWriteEntry(entry);
        }
    }

    m_writeBuffer.clear();
    DEBUG_LOG("AlphaMemorySystem: Write buffer drained");
}

/**
 * @brief Commit a single write buffer entry to memory
 * @param entry Write buffer entry to commit
 */
void AlphaMemorySystem::commitWriteEntry(const WriteBufferEntry& entry)
{
    if (m_mmioManager && m_mmioManager->isMMIOAddress(entry.physicalAddr)) {
        m_mmioManager->writeMMIO(entry.physicalAddr, entry.value, entry.size);
    }
    else {
        switch (entry.size) {
        case 1: m_safeMemory->writeUInt8(entry.physicalAddr, static_cast<quint8>(entry.value)); break;
        case 2: m_safeMemory->writeUInt16(entry.physicalAddr, static_cast<quint16>(entry.value)); break;
        case 4: m_safeMemory->writeUInt32(entry.physicalAddr, static_cast<quint32>(entry.value)); break;
        case 8: m_safeMemory->writeUInt64(entry.physicalAddr, entry.value); break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVILEGED TRANSLATION HELPER
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Translate virtual address with privileged access
 * @param alphaCPU CPU context for MMU
 * @param virtualAddr Virtual address to translate
 * @param physicalAddr Output physical address
 * @return True if translation succeeded
 *
 * This bypasses protection checks but still requires valid mappings.
 * Used for PAL code and kernel operations.
 */
bool AlphaMemorySystem::translatePrivileged(quint64 virtualAddr, quint64& physicalAddr)
{
    // Similar to regular translate but skips protection checks

    // Step 1: PAL Mode short-circuit (1:1 mapping)
    if (m_cpu && m_cpu->inPALMode()) {
        physicalAddr = virtualAddr;
        return true;  // PAL mode has full access
    }

    // Step 2: MMU disabled => 1:1 mapping
    if (!m_cpu || !m_cpu->isMMUEnabled()) {
        physicalAddr = virtualAddr;
        return true;
    }

    // Step 3: Lookup translation from memory map
    auto it = memoryMap.lowerBound(virtualAddr);
    if (it == memoryMap.end() || virtualAddr >= (it.key() + it.value().size)) {
        return false;  // No mapping exists
    }

    const MappingEntry& entry = it.value();

    // Step 4: Calculate final physical address (skip protection check)
    physicalAddr = entry.physicalBase + (virtualAddr - it.key());
    return true;
}


// ═══════════════════════════════════════════════════════════════════════════
// MMIOMANAGER MISSING METHODS IMPLEMENTATION
// Add these methods to MMIOManager class
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// HEADER ADDITIONS (MMIOManager.h)
// ═══════════════════════════════════════════════════════════════════════════




// ═══════════════════════════════════════════════════════════════════════════
// SAFEMEMORY FLUSHWRITES METHOD (if needed)
// ═══════════════════════════════════════════════════════════════════════════

/*
// If SafeMemory doesn't have flushWrites(), add this to SafeMemory class:

void SafeMemory::flushWrites()
{
    QMutexLocker locker(&m_mutex);

    // In a real implementation, this would:
    // 1. Ensure all pending writes reach physical memory
    // 2. Flush any internal write buffers
    // 3. Synchronize with hardware if needed

    // For simulation, we might:
    // - Flush any write coalescing buffers
    // - Ensure memory content is synchronized
    // - Log the operation for debugging

    DEBUG_LOG("SafeMemory: Write buffers flushed");

    // If using memory-mapped files or special hardware interfaces,
    // synchronization calls would go here
#ifdef USE_MEMORY_MAPPED_FILES
    if (m_mappedFile) {
        m_mappedFile->sync();
    }
#endif
}
*/

// ═══════════════════════════════════════════════════════════════════════════
// ADDITIONAL SIGNALS FOR MMIOMANAGER (add to header)
// ═══════════════════════════════════════════════════════════════════════════

/*
// Add these signals to MMIOManager.h if not present:

signals:
    void mmioRead(quint64 address, quint64 value, int size, const QString& deviceName);
    void mmioWritten(quint64 address, quint64 value, int size, const QString& deviceName);
*/


#pragma endregion Helpers Buffer Support

#pragma region tlb System high-performance inline implementations





void AlphaMemorySystem::InternalTLB::invalidateTB(quint64 type, quint64 address) {
	switch (type) {
	case 0: // Invalidate all TB entries
		invalidateTBAll();
		break;
	case 1: // Invalidate all TB entries for current process
		invalidateTBAllProcess();
		break;
	case 2: // Invalidate single TB entry
		invalidateTBSingle(address);
		break;
	case 3: // Invalidate all TB entries for given address space
		// This would need address space ID from address
		invalidateTBAllProcess();
		break;
	default:
		DEBUG_LOG(QString("AlphaCPU: Unknown TB invalidate type %1").arg(type));
		break;
	}
}

void AlphaMemorySystem::InternalTLB::invalidateTBAll() {
	if (m_tlbSystemPtr) {
		m_tlbSystemPtr->invalidateAll();
		DEBUG_LOG("AlphaCPU: Invalidated all TLB entries");
	}

	// Clear translation cache
	if (m_translationCache) {
		m_translationCache->invalidateAll();
	}
}
void AlphaMemorySystem::InternalTLB::invalidateTBAllProcess() {
	if (m_tlbSystemPtr) {
		quint64 currentASN = m_iprs.read(IprBank::ASN);
		m_tlbSystemPtr->invalidateProcessEntries(currentASN);

		if (m_translationCache) {
			m_translationCache->invalidateASN(currentASN);
		}

		DEBUG_LOG(QString("AlphaCPU: Invalidated TLB entries for ASN=%1").arg(currentASN));
	}
}
void AlphaMemorySystem::InternalTLB::invalidateTBSingle(quint64 address) {
	if (m_tlbSystemPtr) {
		quint64 currentASN = m_iprs.read(IprBank::ASN);
		bool isKernelMode = (m_currentMode == ProcessorMode::KERNEL);

		m_tlbSystemPtr->invalidateEntry(address, currentASN, isKernelMode);

		if (m_translationCache) {
			m_translationCache->invalidateAddress(address, currentASN);
		}

		DEBUG_LOG(QString("AlphaCPU: Invalidated TLB entry for address=0x%1, ASN=%2")
			.arg(address, 16, 16, QChar('0'))
			.arg(currentASN));
	}
}

void AlphaMemorySystem::InternalTLB::invalidateTBSingleData(quint64 address) {
	if (m_tlbSystemPtr) {
		quint64 currentASN = m_iprs.read(IprBank::ASN);
		bool isKernelMode = (m_currentMode == ProcessorMode::KERNEL);

		m_tlbSystemPtr->invalidateDataEntry(address, currentASN, isKernelMode);

		DEBUG_LOG(QString("AlphaCPU: Invalidated data TLB entry for address=0x%1, ASN=%2")
			.arg(address, 16, 16, QChar('0'))
			.arg(currentASN));
	}
}
void AlphaMemorySystem::InternalTLB::invalidateTBSingleInst(quint64 address) {
	if (m_tlbSystemPtr) {
		quint64 currentASN = m_iprs.read(IprBank::ASN);
		bool isKernelMode = (m_currentMode == ProcessorMode::KERNEL);

		m_tlbSystemPtr->invalidateInstructionEntry(address, currentASN, isKernelMode);

		DEBUG_LOG(QString("AlphaCPU: Invalidated instruction TLB entry for address=0x%1, ASN=%2")
			.arg(address, 16, 16, QChar('0'))
			.arg(currentASN));
	}
}
inline AlphaMemorySystem::InternalTLB::TLBEntry*
AlphaMemorySystem::InternalTLB::findEntryFast(quint64 virtualAddr, quint64 asn, bool isInstruction) const
{
	const quint64 virtualPage = virtualAddr & ~(AlphaMemoryConstants::PAGE_SIZE - 1);
	auto& tlb = isInstruction ? m_itlb : m_dtlb;

	// Linear search optimized for cache performance
	for (auto& entry : tlb) {
		if (entry.valid &&
			entry.virtualPage == virtualPage &&
			(entry.asn == asn || (entry.protection & AlphaMemoryConstants::TLB_GLOBAL))) {
			return const_cast<TLBEntry*>(&entry);
		}
	}
	return nullptr;
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
bool AlphaMemorySystem::readVirtualMemory(quint64 virtualAddr,
    quint64& value, int size, quint64 pc)
{
    QReadLocker locker(&m_memoryLock);

    // Step 1: Fast path translation with integrated TLB lookup
    quint64 physicalAddr;
    TranslationResult result = translateInternal(virtualAddr, m_cpu->getCurrentASN(),
        0 /* read */, false /* data access */);

    if (!result.isValid()) {
        // Handle translation fault
        switch (result.getException()) {
        case TLBException::TLB_MISS:
            emit tlbMiss(virtualAddr, false);
            break;
        case TLBException::PROTECTION_FAULT:
            emit protectionFault(virtualAddr, 0);
            break;
        case TLBException::INVALID_ENTRY:
            emit translationMiss(virtualAddr);
            break;
        }
        value = 0xFFFFFFFFFFFFFFFFULL;
        return false;
    }

    physicalAddr = result.getPhysicalAddress();

    // Step 2: Access physical memory through appropriate subsystem
    bool success = accessPhysicalMemory(physicalAddr, value, size, false, pc);

    if (success) {
        emit memoryRead(virtualAddr, physicalAddr, size);
    }

    return success;
}


TranslationResult AlphaMemorySystem::translateInternal(quint64 virtualAddr, quint64 currentASN,
	int accessType, bool isInstruction)
{
	m_totalTranslations.fetch_add(1, std::memory_order_relaxed);

	// Step 1: Try TLB lookup first (fast path)
	InternalTLB::TLBEntry* entry = m_tlb.findEntryFast(virtualAddr, currentASN, isInstruction);

	if (entry) {
		// TLB hit - validate permissions
		if ((accessType == 0 && !(entry->protection & TLB_READ)) ||
			(accessType == 1 && !(entry->protection & TLB_WRITE)) ||
			(accessType == 2 && !(entry->protection & TLB_EXEC))) {

			m_protectionFaults.fetch_add(1, std::memory_order_relaxed);
			return TranslationResult::createFault(TLBException::PROTECTION_FAULT);
		}

		// Update access tracking
		entry->referenced = true;
		if (accessType == 1) entry->dirty = true;

		// Update statistics
		m_tlb.updateStats(true, isInstruction);

		// Return successful translation
		quint64 physicalAddr = entry->physicalPage | (virtualAddr & PAGE_OFFSET_MASK);
		return TranslationResult::createSuccess(physicalAddr);
	}

	// Step 2: TLB miss - handle via page table walk
	m_tlb.updateStats(false, isInstruction);
	return handleTLBMiss(virtualAddr, currentASN, accessType, isInstruction);
}

TranslationResult AlphaMemorySystem::handleTLBMiss(quint64 virtualAddr, quint64 asn,
	int accessType, bool isInstruction)
{
	// Step 1: Perform page table walk
	quint64 physicalAddr;
	quint8 protection;

	if (!walkPageTable(virtualAddr, asn, physicalAddr, protection)) {
		m_pageFaults.fetch_add(1, std::memory_order_relaxed);
		return TranslationResult::createFault(AsaExceptions::TLBException::INVALID_ENTRY);
	}

	// Step 2: Check permissions
	if ((accessType == 0 && !(protection & AlphaMemoryConstants::TLB_READ)) ||
		(accessType == 1 && !(protection & AlphaMemoryConstants::TLB_WRITE)) ||
		(accessType == 2 && !(protection & AlphaMemoryConstants::TLB_EXEC))) {

		m_protectionFaults.fetch_add(1, std::memory_order_relaxed);
		return TranslationResult::createFault(AsaExceptions::TLBException::PROTECTION_FAULT);
	}

	// Step 3: Insert new TLB entry
	m_tlb.insertEntry(virtualAddr, physicalAddr, asn, protection, isInstruction);

	// Step 4: Return successful translation
	return TranslationResult::createSuccess(physicalAddr);
}

bool AlphaMemorySystem::walkPageTable(quint64 virtualAddr, quint64 asn,
	quint64& physicalAddr, quint8& protection)
{
	// For simulation purposes, use memory mapping
	// In real implementation, this would walk hardware page tables

	auto it = m_memoryMap.lowerBound(virtualAddr);
	if (it == m_memoryMap.end() || virtualAddr >= (it.key() + it.value().size)) {
		return false; // No mapping found
	}

	const MappingEntry& entry = it.value();
	physicalAddr = entry.physicalBase + (virtualAddr - it.key());
	protection = static_cast<quint8>(entry.protectionFlags);

	return true;
}

bool isMMIOAddress(quint64 address) const {

}

bool AlphaMemorySystem::accessPhysicalMemory(quint64 physicalAddr, quint64& value,
	int size, bool isWrite, quint64 pc)
{
	// Route to appropriate memory subsystem
	if (isMMIOAddress(physicalAddr)) {
		// MMIO access
		if (isWrite) {
			return m_mmioManager->writeMMIO(physicalAddr, value, size,pc);
		}
		else {
			value = m_mmioManager->readMMIO(physicalAddr, size,pc);
			return true;
		}
	}
	else {
		// Regular physical memory access
		if (isWrite) {
			switch (size) {
			case 1: m_safeMemory->writeUInt8(physicalAddr, static_cast<quint8>(value), pc); break;
			case 2: m_safeMemory->writeUInt16(physicalAddr, static_cast<quint16>(value), pc); break;
			case 4: m_safeMemory->writeUInt32(physicalAddr, static_cast<quint32>(value), pc); break;
			case 8: m_safeMemory->writeUInt64(physicalAddr, value, pc); break;
			default: return false;
			}
		}
		else {
			switch (size) {
			case 1: value = m_safeMemory->readUInt8(physicalAddr, pc); break;
			case 2: value = m_safeMemory->readUInt16(physicalAddr, pc); break;
			case 4: value = m_safeMemory->readUInt32(physicalAddr, pc); break;
			case 8: value = m_safeMemory->readUInt64(physicalAddr, pc); break;
			default: return false;
			}
		}
		return true;
	}
}


// ═══════════════════════════════════════════════════════════════════════════
// TLB MANAGEMENT OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════



void AlphaMemorySystem::flushTLB()
{
	QWriteLocker locker(&m_memoryLock);
	m_tlbSystemPtr.invalidateAll();

	// Coordinate with translation cache
	if (m_translationCache) {
		m_translationCache->invalidateAll();
	}

	emit sigTlbFlushed();
}



// ═══════════════════════════════════════════════════════════════════════════
// INTERNAL TLB IMPLEMENTATION DETAILS
// ═══════════════════════════════════════════════════════════════════════════

void AlphaMemorySystem::insertEntry(quint64 va, quint64 pa, quint64 asn,
	quint8 protection, bool isInstruction)
{
	auto& tlb = isInstruction ? m_itlb : m_dtlb;
	auto& clock = isInstruction ? m_iTLBClock : m_dTLBClock;
	const int tlbSize = isInstruction ? 48 : 64;

	quint64 virtualPage = va & ~(AlphaMemoryConstants::PAGE_SIZE - 1);
	quint64 physicalPage = pa & ~(AlphaMemoryConstants::PAGE_SIZE - 1);

	// Look for existing entry first
	for (auto& entry : tlb) {
		if (entry.valid && entry.virtualPage == virtualPage && entry.asn == asn) {
			// Update existing entry
			entry.physicalPage = physicalPage;
			entry.protection = protection;
			entry.referenced = true;
			entry.dirty = false;
			return;
		}
	}

	// Find replacement victim using clock algorithm
	int startClock = clock.load(std::memory_order_relaxed);
	for (int i = 0; i < tlbSize; i++) {
		int index = (startClock + i) % tlbSize;
		auto& entry = tlb[index];

		if (!entry.valid) {
			// Found invalid entry
			entry.virtualPage = virtualPage;
			entry.physicalPage = physicalPage;
			entry.asn = asn;
			entry.protection = protection;
			entry.valid = true;
			entry.referenced = true;
			entry.dirty = false;

			clock.store((index + 1) % tlbSize, std::memory_order_relaxed);
			return;
		}

		if (!entry.referenced) {
			// Found unreferenced entry - replace it
			entry.virtualPage = virtualPage;
			entry.physicalPage = physicalPage;
			entry.asn = asn;
			entry.protection = protection;
			entry.valid = true;
			entry.referenced = true;
			entry.dirty = false;

			clock.store((index + 1) % tlbSize, std::memory_order_relaxed);
			return;
		}

		// Clear reference bit for second chance
		entry.referenced = false;
	}

	// All entries were referenced - replace at clock position
	int index = startClock;
	auto& entry = tlb[index];
	entry.virtualPage = virtualPage;
	entry.physicalPage = physicalPage;
	entry.asn = asn;
	entry.protection = protection;
	entry.valid = true;
	entry.referenced = true;
	entry.dirty = false;

	clock.store((index + 1) % tlbSize, std::memory_order_relaxed);
}

void AlphaMemorySystem::InternalTLB::updateStats(bool hit, bool isInstruction)
{
	if (isInstruction) {
		if (hit) {
			m_iTLBHits.fetch_add(1, std::memory_order_relaxed);
		}
		else {
			m_iTLBMisses.fetch_add(1, std::memory_order_relaxed);
		}
	}
	else {
		if (hit) {
			m_dTLBHits.fetch_add(1, std::memory_order_relaxed);
		}
		else {
			m_dTLBMisses.fetch_add(1, std::memory_order_relaxed);
		}
	}
}


// Add these methods to AlphaMemorySystem::InternalTLB class

/**
 * @brief Invalidate specific TLB entry by virtual address and ASN
 */
void AlphaMemorySystem::InternalTLB::invalidateEntry(quint64 va, quint64 asn)
{
    const quint64 virtualPage = va & ~(AlphaMemoryConstants::PAGE_SIZE - 1);

    // Invalidate in instruction TLB
    for (auto& entry : m_itlb) {
        if (entry.valid && entry.virtualPage == virtualPage &&
            (entry.asn == asn || asn == 0)) {
            entry.valid = false;
        }
    }

    // Invalidate in data TLB
    for (auto& entry : m_dtlb) {
        if (entry.valid && entry.virtualPage == virtualPage &&
            (entry.asn == asn || asn == 0)) {
            entry.valid = false;
        }
    }
}

/**
 * @brief Invalidate all TLB entries for specific ASN
 */
void AlphaMemorySystem::InternalTLB::invalidateByASN(quint64 asn)
{
    // Invalidate all instruction TLB entries for this ASN
    for (auto& entry : m_itlb) {
        if (entry.valid && entry.asn == asn) {
            entry.valid = false;
        }
    }

    // Invalidate all data TLB entries for this ASN
    for (auto& entry : m_dtlb) {
        if (entry.valid && entry.asn == asn) {
            entry.valid = false;
        }
    }
}

/**
 * @brief Invalidate all TLB entries (both instruction and data)
 */
void AlphaMemorySystem::InternalTLB::invalidateAll()
{
    // Invalidate all instruction TLB entries
    for (auto& entry : m_itlb) {
        entry.valid = false;
    }

    // Invalidate all data TLB entries
    for (auto& entry : m_dtlb) {
        entry.valid = false;
    }
}

/**
 * @brief Get comprehensive TLB statistics
 */
TLBStatistics AlphaMemorySystem::InternalTLB::getStatistics() const
{
    TLBStatistics stats;

    // Load atomic counters
    stats.iTLBHits = m_iTLBHits.load(std::memory_order_relaxed);
    stats.iTLBMisses = m_iTLBMisses.load(std::memory_order_relaxed);
    stats.dTLBHits = m_dTLBHits.load(std::memory_order_relaxed);
    stats.dTLBMisses = m_dTLBMisses.load(std::memory_order_relaxed);

    // Calculate derived statistics
    stats.totalTranslations = stats.iTLBHits + stats.iTLBMisses +
        stats.dTLBHits + stats.dTLBMisses;

    // Count valid entries
    int validITLBEntries = 0;
    int validDTLBEntries = 0;

    for (const auto& entry : m_itlb) {
        if (entry.valid) validITLBEntries++;
    }

    for (const auto& entry : m_dtlb) {
        if (entry.valid) validDTLBEntries++;
    }

    return stats;
}
#pragma endregion tlb System high-performance inline implementations

// Implementation in AlphaMemorySystem.cpp

bool AlphaMemorySystem::probeAddress(const IExecutionContext* context,
	quint64 virtualAddress,
	bool isWrite,
	int size) const {
	return probeAddressInternal(context, virtualAddress, isWrite, size);
}

bool AlphaMemorySystem::probeAddressDetailed(const IExecutionContext* context,
	quint64 virtualAddress,
	bool isWrite,
	int size,
	ProbeResult& result) const {
	return probeAddressInternal(context, virtualAddress, isWrite, size, &result);
}

bool AlphaMemorySystem::probeAddressInternal(const IExecutionContext* context,
	quint64 virtualAddress,
	bool isWrite,
	int size,
	ProbeResult* result) const {
	// Initialize result if provided
	if (result) {
		*result = ProbeResult();
		result->faultAddress = virtualAddress;
	}

	// Validate parameters
	if (!context || size <= 0 || size > 8) {
		if (result) {
			result->status = ProbeResult::Status::INVALID_ADDRESS;
			result->description = "Invalid parameters";
		}
		return false;
	}

	// Check for alignment requirements
	if (!isAlignmentValid(virtualAddress, size)) {
		if (result) {
			result->status = ProbeResult::Status::ALIGNMENT_FAULT;
			result->description = QString("Misaligned access: addr=0x%1, size=%2")
				.arg(virtualAddress, 0, 16).arg(size);
		}
		return false;
	}

	try {
		// Get current processor context
		const AlphaCPU* cpu = dynamic_cast<const AlphaCPU*>(context);
		if (!cpu) {
			if (result) {
				result->status = ProbeResult::Status::INVALID_ADDRESS;
				result->description = "Invalid execution context";
			}
			return false;
		}

		// Get current ASN and mode
		quint64 currentASN = 0;
		bool isKernelMode = cpu->isKernelMode();

		if (cpu->iprBank()) {
			currentASN = cpu->iprBank()->read(Ipr::ASN);
		}

		// Check if MMU is enabled
		if (!cpu->isMMUEnabled()) {
			// Direct physical addressing - just check if address is valid
			if (m_safeMemory && m_safeMemory->isValidPhysicalAddress(virtualAddress)) {
				if (result) {
					result->status = ProbeResult::Status::SUCCESS;
					result->physicalAddress = virtualAddress;
					result->description = "Direct physical access";
				}
				return true;
			}
			else {
				if (result) {
					result->status = ProbeResult::Status::INVALID_ADDRESS;
					result->description = "Invalid physical address";
				}
				return false;
			}
		}

		// Perform TLB translation probe
		if (!m_tlbSystem) {
			if (result) {
				result->status = ProbeResult::Status::INVALID_ADDRESS;
				result->description = "TLB system not available";
			}
			return false;
		}

		// Try translation without causing side effects
		auto tlbResult = m_tlbSystem->translateAddress(
			virtualAddress, isWrite, false, currentASN, isKernelMode);

		if (result) {
			result->tlbException = tlbResult.tlbException;
			result->physicalAddress = tlbResult.physicalAddress;
		}

		// Check translation result
		switch (tlbResult.tlbException) {
		case TLBException::NONE:
			// Translation succeeded
			break;

		case TLBException::TLB_MISS:
			if (result) {
				result->status = ProbeResult::Status::TLB_MISS;
				result->description = "TLB miss would occur";
			}
			return false;

		case TLBException::PAGE_FAULT:
			if (result) {
				result->status = ProbeResult::Status::PAGE_FAULT;
				result->description = "Page not present";
				result->requiresPageFault = true;
			}
			return false;

		case TLBException::ACCESS_VIOLATION:
			if (result) {
				result->status = ProbeResult::Status::PROTECTION_VIOLATION;
				result->description = isWrite ? "Write protection violation" :
					"Read protection violation";
			}
			return false;

		case TLBException::ALIGNMENT_FAULT:
			if (result) {
				result->status = ProbeResult::Status::ALIGNMENT_FAULT;
				result->description = "Alignment fault";
			}
			return false;

		default:
			if (result) {
				result->status = ProbeResult::Status::INVALID_ADDRESS;
				result->description = QString("TLB exception: %1")
					.arg(static_cast<int>(tlbResult.tlbException));
			}
			return false;
		}

		// Check if address maps to MMIO
		bool isMMIOAddress = false;
		if (m_mmioManager) {
			isMMIOAddress = m_mmioManager->isMMIOAddress(tlbResult.physicalAddress);
			if (result) {
				result->isMMIO = isMMIOAddress;
			}
		}

		// For MMIO addresses, check if the device supports the operation
		if (isMMIOAddress) {
			if (m_mmioManager && !m_mmioManager->isAccessSupported(
				tlbResult.physicalAddress, isWrite, size)) {
				if (result) {
					result->status = ProbeResult::Status::MMIO_REGION;
					result->description = "MMIO device doesn't support this access";
				}
				return false;
			}
		}
		else {
			// For regular memory, check if physical address is valid
			if (m_safeMemory && !m_safeMemory->isValidPhysicalAddress(
				tlbResult.physicalAddress)) {
				if (result) {
					result->status = ProbeResult::Status::INVALID_ADDRESS;
					result->description = "Invalid physical address";
				}
				return false;
			}
		}

		// All checks passed
		if (result) {
			result->status = ProbeResult::Status::SUCCESS;
			result->description = isMMIOAddress ? "MMIO access OK" : "Memory access OK";
		}
		return true;

	}
	catch (const std::exception& e) {
		if (result) {
			result->status = ProbeResult::Status::INVALID_ADDRESS;
			result->description = QString("Exception during probe: %1").arg(e.what());
		}
		return false;
	}
}

// Helper method for alignment checking
bool AlphaMemorySystem::isAlignmentValid(quint64 address, int size) const {
	switch (size) {
	case 1:  return true;                    // Byte access - always aligned
	case 2:  return (address & 0x1) == 0;    // Word - must be 2-byte aligned
	case 4:  return (address & 0x3) == 0;    // Longword - must be 4-byte aligned
	case 8:  return (address & 0x7) == 0;    // Quadword - must be 8-byte aligned
	default: return false;                   // Invalid size
	}
}
void AlphaMemorySystem::onMappingsCleared() {
	DEBUG_LOG("AlphaMemorySystem: All memory mappings cleared");

	try {
		// 1. Invalidate all TLB entries
		if (m_tlbSystem) {
			m_tlbSystem->invalidateAll();
			DEBUG_LOG("AlphaMemorySystem: TLB completely invalidated");
		}

		// 2. Clear translation caches
		if (m_translationCache) {
			m_translationCache->clear();
			DEBUG_LOG("AlphaMemorySystem: Translation cache cleared");
		}

		// 3. Reset any cached page table information
		m_cachedPageTableBase = 0;
		m_cachedASN = 0;

		// 4. Clear any pending memory operations that depend on translations
		clearPendingTranslationDependentOperations();

		// 5. Reset memory mapping statistics
		resetMappingStatistics();

		// 6. Notify MMIO manager if mappings affect device regions
		if (m_mmioManager) {
			m_mmioManager->onMappingsChanged();
		}

		// 7. Invalidate any cached memory protection information
		clearProtectionCache();

		// 8. Update performance counters
		incrementMappingClearCount();

		// 9. Emit signal to notify other components
		emit sigMappingsCleared();
		emit sigTlbInvalidated();

		DEBUG_LOG("AlphaMemorySystem: Mapping clear handling completed");

	}
	catch (const std::exception& e) {
		ERROR_LOG(QString("AlphaMemorySystem: Exception during mapping clear: %1")
			.arg(e.what()));

		// Force a complete reset on error
		forceMemorySystemReset();
	}
}
void AlphaMemorySystem::onMappingRangeCleared(quint64 startAddr, quint64 endAddr, quint64 asn) {
	DEBUG_LOG(QString("AlphaMemorySystem: Mapping range cleared: 0x%1-0x%2, ASN=%3")
		.arg(startAddr, 0, 16)
		.arg(endAddr, 0, 16)
		.arg(asn));

	try {
		// 1. Invalidate TLB entries for the specified range
		if (m_tlbSystem) {
			if (asn == 0) {
				// Clear range for all ASNs
				m_tlbSystem->invalidateRange(startAddr, endAddr);
			}
			else {
				// Clear range for specific ASN
				m_tlbSystem->invalidateRangeByASN(startAddr, endAddr, asn);
			}
		}

		// 2. Clear translation cache entries for this range
		if (m_translationCache) {
			m_translationCache->invalidateRange(startAddr, endAddr, asn);
		}

		// 3. Check if any pending operations are affected
		cancelPendingOperationsInRange(startAddr, endAddr, asn);

		// 4. Update statistics
		incrementRangeClearCount();

		// 5. Emit signal
		emit sigMappingRangeCleared(startAddr, endAddr, asn);

	}
	catch (const std::exception& e) {
		ERROR_LOG(QString("AlphaMemorySystem: Exception during range clear: %1")
			.arg(e.what()));
	}
}
void AlphaMemorySystem::onASNMappingsCleared(quint64 asn) {
	DEBUG_LOG(QString("AlphaMemorySystem: ASN %1 mappings cleared").arg(asn));

	try {
		// 1. Invalidate TLB entries for the specified ASN
		if (m_tlbSystem) {
			if (asn == 0) {
				// ASN 0 typically means "all ASNs"
				m_tlbSystem->invalidateAll();
			}
			else {
				m_tlbSystem->invalidateByASN(asn);
			}
		}

		// 2. Clear translation cache entries for this ASN
		if (m_translationCache) {
			m_translationCache->invalidateByASN(asn);
		}

		// 3. Cancel pending operations for this ASN
		onCancelPendingOperationsByASN(asn);

		// 4. Update statistics
		onIncrementASNClearCount();

		// 5. Emit signal (using the range version with full address space)
		emit sigMappingRangeCleared(0, 0xFFFFFFFFFFFFFFFFULL, asn);

	}
	catch (const std::exception& e) {
		ERROR_LOG(QString("AlphaMemorySystem: Exception during ASN clear: %1")
			.arg(e.what()));
	}
}

// Helper methods (add to private section of AlphaMemorySystem)

void AlphaMemorySystem::clearPendingTranslationDependentOperations() {
	// Clear any operations that depend on specific virtual->physical mappings
	// This might include:
	// - Pending DMA operations with virtual addresses
	// - Cached translation results
	// - Prefetch operations

	if (m_pendingVirtualOperations.size() > 0) {
		DEBUG_LOG(QString("AlphaMemorySystem: Cancelling %1 pending virtual operations")
			.arg(m_pendingVirtualOperations.size()));

		for (auto& operation : m_pendingVirtualOperations) {
			operation.cancel("Virtual mappings cleared");
		}
		m_pendingVirtualOperations.clear();
	}
}
void AlphaMemorySystem::resetMappingStatistics() {
	m_mappingStats.totalMappingClears++;
	m_mappingStats.lastClearTime = QDateTime::currentDateTime();

	// Reset counters that are no longer valid
	m_mappingStats.tlbHits = 0;
	m_mappingStats.tlbMisses = 0;
	m_mappingStats.translationCacheHits = 0;
	m_mappingStats.translationCacheMisses = 0;
}
void AlphaMemorySystem::clearProtectionCache() {
	// Clear any cached memory protection information
	if (m_protectionCache) {
		m_protectionCache->clear();
	}

	// Reset protection-related flags
	m_lastProtectionCheck.address = 0;
	m_lastProtectionCheck.isValid = false;
}
void AlphaMemorySystem::cancelPendingOperationsInRange(quint64 startAddr, quint64 endAddr, quint64 asn) {
	auto it = m_pendingVirtualOperations.begin();
	while (it != m_pendingVirtualOperations.end()) {
		bool shouldCancel = false;

		// Check if operation is in the cleared range
		if (it->virtualAddress >= startAddr && it->virtualAddress <= endAddr) {
			// Check ASN if specified
			if (asn == 0 || it->asn == asn) {
				shouldCancel = true;
			}
		}

		if (shouldCancel) {
			it->cancel(QString("Mapping cleared for range 0x%1-0x%2")
				.arg(startAddr, 0, 16).arg(endAddr, 0, 16));
			it = m_pendingVirtualOperations.erase(it);
		}
		else {
			++it;
		}
	}
}
void AlphaMemorySystem::cancelPendingOperationsByASN(quint64 asn) {
	auto it = m_pendingVirtualOperations.begin();
	while (it != m_pendingVirtualOperations.end()) {
		if (asn == 0 || it->asn == asn) {
			it->cancel(QString("Mappings cleared for ASN %1").arg(asn));
			it = m_pendingVirtualOperations.erase(it);
		}
		else {
			++it;
		}
	}
}
void AlphaMemorySystem::forceMemorySystemReset() {
	ERROR_LOG("AlphaMemorySystem: Forcing complete memory system reset");

	// This is a last-resort recovery method
	try {
		if (m_tlbSystem) {
			m_tlbSystem->reset();
		}
		if (m_translationCache) {
			m_translationCache->reset();
		}
		if (m_mmioManager) {
			m_mmioManager->reset();
		}

		// Clear all pending operations
		m_pendingVirtualOperations.clear();

		// Reset all cached state
		m_cachedPageTableBase = 0;
		m_cachedASN = 0;

		// Emit signals
		emit mappingsCleared();
		emit tlbInvalidated();

	}
	catch (...) {
		FATAL_LOG("AlphaMemorySystem: Failed to reset memory system - system may be unstable");
	}
}
// Performance counter helpers
void AlphaMemorySystem::incrementMappingClearCount() {
	m_mappingStats.totalMappingClears++;
}
void AlphaMemorySystem::incrementRangeClearCount() {
	m_mappingStats.rangeMappingClears++;
}
void AlphaMemorySystem::incrementASNClearCount() {
	m_mappingStats.asnMappingClears++;
}

bool AlphaMemorySystem::loadLocked(AlphaCPU* cpu, quint64 vaddr, quint64& value, int size, quint64 pc) {
	// First, perform normal load with translation
	if (!readVirtualMemory(cpu, vaddr, &value, size, pc)) {
		// Load failed (page fault, protection violation, etc.)
		return false;
	}

	// Get physical address for reservation tracking
	quint64 physAddr = 0;
	if (!translateVirtualToPhysical(cpu, vaddr, physAddr, false)) {
		// Translation failed - shouldn't happen since load succeeded
		ERROR_LOG("Translation failed after successful load in loadLocked");
		return false;
	}

	// Clear any existing reservation for this CPU
	if (m_reservations.contains(cpu)) {
		m_reservations[cpu].clear();
	}

	// Create new reservation
	ReservationState& reservation = m_reservations[cpu];
	reservation.isValid = true;
	reservation.physicalAddress = physAddr & ~0x7ULL;  // Align to 8-byte boundary
	reservation.virtualAddress = vaddr;
	reservation.size = size;
	reservation.timestamp = getCurrentTimestamp();
	reservation.cpu = cpu;

	m_loadLockedCount++;

	DEBUG_LOG(QString("Load-locked: CPU%1, vaddr=0x%2, paddr=0x%3, size=%4")
		.arg(cpu->getCpuId())
		.arg(vaddr, 0, 16)
		.arg(reservation.physicalAddress, 0, 16)
		.arg(size));

	return true;
}

bool AlphaMemorySystem::storeConditional(AlphaCPU* cpu_, quint64 vaddr, quint64 value, int size, quint64 pc) {
	// Check if CPU has a valid reservation
	if (!m_reservations.contains(cpu_) || !m_reservations[cpu_].isValid) {
		// No reservation - store fails
		m_storeConditionalFailureCount++;
		DEBUG_LOG(QString("Store-conditional failed: CPU%1, no reservation").arg(cpu_->getCpuId()));
		return false;
	}

	// Get physical address
	quint64 physAddr = 0;
	if (!translateVirtualToPhysical(cpu_, vaddr, physAddr, true)) {
		// Translation failed
		m_reservations[cpu_].clear();
		m_storeConditionalFailureCount++;
		return false;
	}

	ReservationState& reservation = m_reservations[cpu_];

	// Check if reservation matches this address
	if (!reservation.matches(physAddr, size)) {
		// Address doesn't match reservation
		reservation.clear();
		m_storeConditionalFailureCount++;
		DEBUG_LOG(QString("Store-conditional failed: CPU%1, address mismatch").arg(cpu_->getCpuId()));
		return false;
	}

	// Attempt the store
	if (!writeVirtualMemory(cpu_, vaddr, &value, size, pc)) {
		// Store failed (page fault, protection violation, etc.)
		reservation.clear();
		m_storeConditionalFailureCount++;
		return false;
	}

	// Store succeeded - clear the reservation and invalidate overlapping reservations
	reservation.clear();
	invalidateOverlappingReservations(physAddr, size, cpu_);

	m_storeConditionalSuccessCount++;

	DEBUG_LOG(QString("Store-conditional succeeded: CPU%1, vaddr=0x%2, paddr=0x%3")
		.arg(cpu->getCpuId())
		.arg(vaddr, 0, 16)
		.arg(physAddr, 0, 16));

	return true;
}

void AlphaMemorySystem::clearReservations(quint64 physAddr, int size) {
	invalidateOverlappingReservations(physAddr, size, nullptr);
	m_reservationClearCount++;

	DEBUG_LOG(QString("Cleared reservations for paddr=0x%1, size=%2")
		.arg(physAddr, 0, 16)
		.arg(size));
}

void AlphaMemorySystem::clearCpuReservations(AlphaCPU* cpu_) {
	if (m_reservations.contains(cpu)) {
		m_reservations[cpu].clear();
		DEBUG_LOG(QString("Cleared all reservations for CPU%1").arg(cpu->getCpuId()));
	}
}

bool AlphaMemorySystem::hasReservation(AlphaCPU* cpu_, quint64 physAddr) const {
	if (!m_reservations.contains(cpu)) {
		return false;
	}

	const ReservationState& reservation = m_reservations[cpu];
	return reservation.isValid && reservation.matches(physAddr, 1);
}

void AlphaMemorySystem::invalidateOverlappingReservations(quint64 physAddr, int size, AlphaCPU* excludeCpu) {
	auto it = m_reservations.begin();
	while (it != m_reservations.end()) {
		if (it.key() != excludeCpu && it.value().isValid) {
			if (it.value().matches(physAddr, size)) {
				DEBUG_LOG(QString("Invalidating reservation for CPU%1 due to overlapping access")
					.arg(it.key()->getCpuId()));
				it.value().clear();
			}
		}
		++it;
	}
}

quint64 AlphaMemorySystem::getCurrentTimestamp() const {
	// Use cycle counter or system time
	return QDateTime::currentMSecsSinceEpoch();
}

bool AlphaMemorySystem::translateVirtualToPhysical(AlphaCPU* cpu_, quint64 vaddr, quint64& paddr, bool isWrite) {
	if (!m_tlbSystem || !cpu_) {
		return false;
	}

	quint64 currentASN = cpu->iprBank()->read(Ipr::ASN);
	bool isKernelMode = cpu->isKernelMode();

	auto result = m_tlbSystem->translateAddress(vaddr, isWrite, false, currentASN, isKernelMode);

	if (result.tlbException == TLBException::NONE) {
		paddr = result.physicalAddress;
		return true;
	}

	return false;
}