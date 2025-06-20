#include "AlphaMemorySystem_refactored.h"
#include "../AEJ/AlphaMMIOAddressChecker.h"
#include "../AEJ/GlobalLockTracker.h"
#include "../AEJ/TranslationResult.h"
#include "../AEJ/constants/constAlphaMemorySystem.h"
#include "../AEJ/structures/structProbeResult.h"
#include "../AEJ/AlphaCPU_refactored.h"
#include "../AEJ/GlobalMacro.h"
#include <QDateTime>
#include <QMutexLocker>
#include <QtGlobal>
#include "enumerations/enumMemoryFaultType.h"
#include "enumerations/enumMemoryBarrierEmulationMode.h"
#include "utilitySafeIncrement.h"
#include "enumerations/MemAccessType.h"

namespace
{
void qtMemoryBarrier()
{
    // Enhanced Qt atomic memory barrier implementation
    QAtomicInt barrier;

    // Acquire-release pair for full memory barrier
    barrier.fetchAndAddAcquire(0);   // Load with acquire semantics
    barrier.fetchAndStoreRelease(0); // Store with release semantics

    // Additional ordered operation for maximum compatibility
    barrier.fetchAndStoreOrdered(0);
}

// Helper for sequential consistency when needed
void qtSequentialBarrier()
{
    QAtomicInt seqBarrier;
    seqBarrier.fetchAndStoreOrdered(0);
}
} // namespace

AlphaMemorySystem::AlphaMemorySystem(QObject *parent)
    : QObject(parent)
    , m_nextCpuId(0)
    , m_safeMemory(nullptr)
    , m_mmioManager(nullptr)
    , m_irqController(nullptr)
    , m_translationCache(nullptr)
    , m_cpuModel(CpuModel::CPU_EV56)
    , m_level3SharedCache(nullptr)
    , m_exceptionHandler(nullptr)
    , m_currentCPU(nullptr)
    , m_currentASN(0)
    , m_physicalMemoryBase(0x0)
    , m_physicalMemorySize(0x100000000ULL) // Default 4GB
    , m_kernelMemoryBase(0xFFFFFC0000000000ULL)
    , m_kernelMemorySize(0x40000000ULL) // Default 1GB kernel space
    , m_enforceAlignment(true), m_deviceManager(nullptr)
    , reservationAddr(INVALID_RESERVATION)

{
    // High-performance TLB configuration
    const int TLB_ENTRIES_PER_CPU = 128; // Larger TLB for better hit rates
    const int MAX_CPUS = 16;             // Support up to 16 CPUs

    // Create internal TLB system - this is now owned by AlphaMemorySystem
    m_tlbSystem = new TLBSystem(TLB_ENTRIES_PER_CPU, MAX_CPUS);

    // Initialize the rest of the memory system
    initialize();

    DEBUG_LOG(QString("AlphaMemorySystem: Created with internal TLB system (%1 entries per CPU, max %2 CPUs)")
                  .arg(TLB_ENTRIES_PER_CPU)
                  .arg(MAX_CPUS));
}

AlphaMemorySystem::~AlphaMemorySystem()
{
    // Unregister all CPUs first - this will clean up their TLB entries
    {
        QWriteLocker locker(&m_cpuRegistryLock);

        // Unregister each CPU from TLB system before clearing registry
        for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
        {
            quint16 cpuId = it.key();
            if (m_tlbSystem)
            {
                m_tlbSystem->unregisterCPU(cpuId);
                DEBUG_LOG(QString("AlphaMemorySystem: Unregistered CPU %1 from TLB during cleanup").arg(cpuId));
            }
        }

        m_cpuRegistry.clear();
    }

    // Clean up internal TLB system
    if (m_tlbSystem)
    {
        delete m_tlbSystem;
        m_tlbSystem = nullptr;
        DEBUG_LOG("AlphaMemorySystem: Internal TLB system destroyed");
    }

    DEBUG_LOG("AlphaMemorySystem: Destructor completed - all resources cleaned up");
}

bool AlphaMemorySystem::readVirtualMemoryWithCache(quint16 cpuId, quint64 virtualAddr, quint64 &value, int size,
                                                   quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
    {
        return false;
    }

    safeIncrement(m_totalMemoryAccesses);

    // Try CPU's cache hierarchy first
    if (cpu->getLevel1DataCache())
    {
        if (cpu->getLevel1DataCache()->read(virtualAddr, &value, size))
        {
            DEBUG_LOG("AlphaMemorySystem: L1 cache hit for CPU %1, addr=0x%2", cpuId, QString::number(virtualAddr, 16));
            return true;
        }
    }

    // Fall back to standard virtual memory read with TLB translation
    TranslationResult result = translateInternal(cpuId, virtualAddr, 0 /* read */, false /* data */);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, 0);
            break;
        default:
            emit sigTranslationMiss(virtualAddr);
            break;
        }
        value = 0xFFFFFFFFFFFFFFFFULL;
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();
    bool success = accessPhysicalMemory(physicalAddr, value, size, false, pc, cpuId);

    // Populate cache on successful read
    if (success && cpu->getLevel1DataCache())
    {
        cpu->getLevel1DataCache()->write(virtualAddr, &value, size);
    }

    if (success)
    {
        emit sigMemoryRead(virtualAddr, value, size);
    }

    return success;
}

bool AlphaMemorySystem::writeVirtualMemoryWithCache(quint16 cpuId, quint64 virtualAddr, quint64 value, int size,
                                                    quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
    {
        return false;
    }

    safeIncrement(m_totalMemoryAccesses);

    // Perform translation first
    TranslationResult result = translateInternal(cpuId, virtualAddr, 1 /* write */, false /* data */);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, 1);
            break;
        default:
            emit sigTranslationMiss(virtualAddr);
            break;
        }
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();

    // Clear any conflicting reservations before write
    clearReservations(physicalAddr, size, cpuId);

    // Write to CPU's cache hierarchy
    bool cacheSuccess = false;
    if (cpu->getLevel1DataCache())
    {
        cacheSuccess = cpu->getLevel1DataCache()->write(virtualAddr, &value, size);
    }

    // Write to physical memory
    quint64 tempValue = value;
    bool memSuccess = accessPhysicalMemory(physicalAddr, tempValue, size, true, pc, cpuId);

    bool success = cacheSuccess || memSuccess;

    if (success)
    {
        // Notify other CPUs about the write for cache coherency
        emit sigMemoryWriteNotification(physicalAddr, size, cpuId);
        emit sigMemoryWritten(virtualAddr, value, size);

        // Send cache coherency message
        CacheCoherencyMessage msg;
        msg.type = CacheCoherencyMessage::INVALIDATE_LINE;
        msg.physicalAddress = physicalAddr;
        msg.sourceCpuId = cpuId;
        msg.targetCpuId = 0xFFFF; // Broadcast
        msg.size = size;
        msg.timestamp = getCurrentTimestamp();

        sendCacheCoherencyMessage(msg);
    }

    return success;
}
// =========================
// SMP CPU MANAGEMENT
// =========================

bool AlphaMemorySystem::readWithoutFault(quint64 address, quint64 &value, size_t size)
{
    // Try to read memory without causing faults

    // First translate to physical address
    quint64 physicalAddr;
    if (!translateAddressNonFaulting(address, physicalAddr, false))
    {
        return false; // Translation would fault
    }

    // Read from physical memory
    return readPhysicalMemory(physicalAddr, value, size);
}

bool AlphaMemorySystem::registerCPU(AlphaCPU *cpu_, quint16 cpuId)
{
    if (!cpu_)
    {
        ERROR_LOG("AlphaMemorySystem: Cannot register null CPU");
        return false;
    }

    QWriteLocker locker(&m_cpuRegistryLock);

    if (m_cpuRegistry.contains(cpuId))
    {
        WARN_LOG("AlphaMemorySystem: CPU ID %1 already registered", cpuId);
        return false;
    }

    // Create registry entry
    CPURegistryEntry entry(cpu_, cpuId);
    m_cpuRegistry.insert(cpuId, entry);

    // Register CPU with internal TLB system
    if (m_tlbSystem && !m_tlbSystem->registerCPU(cpuId))
    {
        ERROR_LOG("AlphaMemorySystem: Failed to register CPU %1 with TLB system", cpuId);
        m_cpuRegistry.remove(cpuId);
        return false;
    }

    // Connect CPU's instruction cache to L3 if available
    if (m_level3SharedCache && cpu_)
    {
        auto *instructionCache = cpu_->getInstructionCache();
        if (instructionCache && instructionCache->getUnifiedCache())
        {
            instructionCache->getUnifiedCache()->setNextLevel(m_level3SharedCache);
            DEBUG_LOG("AlphaMemorySystem: Connected CPU %1 I-cache to L3", cpuId);
        }

        // Set up CPU's data cache hierarchy
        // This requires CPU to expose its cache hierarchy
        if (cpu_->getLevel1DataCache())
        {
            cpu_->getLevel1DataCache()->setNextLevel(m_level3SharedCache);
            DEBUG_LOG("AlphaMemorySystem: Connected CPU %1 L1D-cache to L3", cpuId);
        }
    }

    // Update CPU context in TLB system
    if (m_tlbSystem)
    {
        m_tlbSystem->updateCPUContext(cpuId, cpu_->getCurrentASN());
    }

    DEBUG_LOG("AlphaMemorySystem: Successfully registered CPU %1 (total: %2)", cpuId, m_cpuRegistry.size());

    emit sigCPURegistered(cpuId);
    return true;
}

void AlphaMemorySystem::updateCPUContext(quint16 cpuId, quint64 newASN)
{
    // Update current CPU context
    if (m_currentCPU && m_currentCPU->getCpuId() == cpuId)
    {
        m_currentASN = newASN;
    }

    // Update TLB system context
    if (m_tlbSystem)
    {
        m_tlbSystem->updateCPUContext(cpuId, newASN);
    }

    DEBUG_LOG("AlphaMemorySystem: Updated CPU %1 context to ASN=%2", cpuId, newASN);
}

bool AlphaMemorySystem::unregisterCPU(quint16 cpuId)
{
    QWriteLocker locker(&m_cpuRegistryLock);

    if (!m_cpuRegistry.contains(cpuId))
    {
        WARN_LOG(QString("AlphaMemorySystem: CPU ID %1 not registered").arg(cpuId));
        return false;
    }

    // Get CPU info before removal for cleanup
    const CPURegistryEntry &entry = m_cpuRegistry[cpuId];
    AlphaCPU *cpu = entry.cpu;

    // Step 1: Clear CPU's memory reservations first
    // This prevents any pending load-locked operations from interfering
    clearCpuReservations(cpuId);

    // Step 2: Invalidate all TLB entries for this CPU
    // This ensures no stale translations remain
    if (m_tlbSystem)
    {
        // Get TLB statistics before cleanup for debugging
        TLBSystem::TLBStats stats = m_tlbSystem->getTLBStats(cpuId);
        DEBUG_LOG(
            QString("AlphaMemorySystem: CPU %1 TLB stats before cleanup - Hits: %2, Misses: %3, Valid Entries: %4")
                .arg(cpuId)
                .arg(stats.hits)
                .arg(stats.misses)
                .arg(stats.validEntries));

        // Invalidate all entries for this CPU
        m_tlbSystem->invalidateAll(cpuId);

        // Unregister CPU from TLB system - this frees the TLB memory
        if (!m_tlbSystem->unregisterCPU(cpuId))
        {
            WARN_LOG(QString("AlphaMemorySystem: Failed to unregister CPU %1 from TLB system").arg(cpuId));
            // Continue with unregistration even if TLB cleanup fails
        }
        else
        {
            DEBUG_LOG(QString("AlphaMemorySystem: Successfully cleaned up TLB for CPU %1").arg(cpuId));
        }
    }

    // Step 3: Remove from CPU registry
    m_cpuRegistry.remove(cpuId);

    // Step 4: Clean up reservation tracking
    {
        QWriteLocker reservationLocker(&m_reservationsLock);
        m_reservations.remove(cpuId);
    }

    // Step 5: Notify other components about CPU removal
    // This allows caches and other components to clean up CPU-specific state
    if (cpu)
    {
        // Send final cache coherency message to clean up any CPU-specific cache state
        CacheCoherencyMessage msg;
        msg.type = CacheCoherencyMessage::FLUSH_LINE;
        msg.physicalAddress = 0; // All addresses
        msg.sourceCpuId = cpuId;
        msg.targetCpuId = 0xFFFF; // Broadcast
        msg.size = 0;             // All sizes
        msg.timestamp = getCurrentTimestamp();

        sendCacheCoherencyMessage(msg);
    }

    DEBUG_LOG(QString("AlphaMemorySystem: Successfully unregistered CPU %1 (remaining: %2 CPUs)")
                  .arg(cpuId)
                  .arg(m_cpuRegistry.size()));

    emit sigCPUUnregistered(cpuId);
    return true;
}

AlphaCPU *AlphaMemorySystem::getCPU(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuRegistryLock);
    auto it = m_cpuRegistry.find(cpuId);
    return (it != m_cpuRegistry.end()) ? it->cpu : nullptr;
}

QVector<CPURegistryEntry> AlphaMemorySystem::getAllCPUs() const
{
    QReadLocker locker(&m_cpuRegistryLock);
    QVector<CPURegistryEntry> result;
    result.reserve(m_cpuRegistry.size());

    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        result.append(it.value());
    }

    return result;
}

quint16 AlphaMemorySystem::getCPUCount() const
{
    QReadLocker locker(&m_cpuRegistryLock);
    return static_cast<quint16>(m_cpuRegistry.size());
}

void AlphaMemorySystem::setCPUOnlineStatus(quint16 cpuId, bool isOnline)
{
    QWriteLocker locker(&m_cpuRegistryLock);
    auto it = m_cpuRegistry.find(cpuId);
    if (it != m_cpuRegistry.end())
    {
        it->isOnline = isOnline;
        emit sigCPUOnlineStatusChanged(cpuId, isOnline);
        DEBUG_LOG(QString("AlphaMemorySystem: CPU %1 %2").arg(cpuId).arg(isOnline ? "online" : "offline"));
    }
}

// =========================
// SMP-AWARE MEMORY OPERATIONS
// =========================

bool AlphaMemorySystem::readPhysicalMemory(quint64 physicalAddr, quint64 &value, size_t size)
{
    if (!m_safeMemory)
        return false;

    if (isMMIOAddress(physicalAddr))
    {
        if (m_mmioManager)
        {
            value = m_mmioManager->readMMIO(physicalAddr, static_cast<int>(size), 0);
            return true;
        }
        return false;
    }

    try
    {
        switch (size)
        {
        case 1:
            value = m_safeMemory->readUInt8(physicalAddr, 0);
            break;
        case 2:
            value = m_safeMemory->readUInt16(physicalAddr, 0);
            break;
        case 4:
            value = m_safeMemory->readUInt32(physicalAddr, 0);
            break;
        case 8:
            value = m_safeMemory->readUInt64(physicalAddr, 0);
            break;
        default:
            return false;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}


/**
 * @brief Read from virtual memory using TLB translation.
 * Performs TLB translation, protection checks, and traps on failure.
 *
 * @param cpuId      CPU initiating access
 * @param virtualAddr Virtual address to read from
 * @param value      Out parameter for read result
 * @param size       Number of bytes to read (must be 1, 2, 4, 8)
 * @param pc         Program Counter for fault context
 * @return true if successful, false if fault occurred
 */
inline bool AlphaMemorySystem::readVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 &value, int size,
                                                 quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
        return false;


    asa_utils::safeIncrement(m_totalMemoryAccesses);
 

    // Perform TLB translation
    TranslationResult result = translateInternal(cpuId, virtualAddr, MemAccessType::READ, false /*isInstr*/);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, MemAccessType::READ);
            break;
        default:
            break;
        }
        raiseMemoryAccessException(virtualAddr, size, false /*isWrite*/, pc);
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();
    return accessPhysicalMemory(physicalAddr, value, size, false /*isWrite*/, pc, cpuId);
}

/**
 * @brief Write to virtual memory using TLB translation.
 * Performs TLB translation, protection checks, and traps on failure.
 *
 * @param cpuId      CPU initiating access
 * @param virtualAddr Virtual address to write to
 * @param value      Value to write
 * @param size       Number of bytes to write (must be 1, 2, 4, 8)
 * @param pc         Program Counter for fault context
 * @return true if successful, false if fault occurred
 */
inline bool AlphaMemorySystem::writeVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 value, int size,
                                                  quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
        return false;

    asa_utils::safeIncrement(m_totalMemoryAccesses, 1);

    TranslationResult result = translateInternal(cpuId, virtualAddr, MemAccessType::WRITE, false /*isInstr*/);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, MemAccessType::WRITE);
            break;
        default:
            break;
        }
        raiseMemoryAccessException(virtualAddr, size, true /*isWrite*/, pc);
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();
    return accessPhysicalMemory(physicalAddr, value, size, true /*isWrite*/, pc, cpuId);
}
/**
 * @brief Read from virtual memory using TLB translation.
 * Performs TLB translation, protection checks, and traps on failure.
 *
 * @param cpuId      CPU initiating access
 * @param virtualAddr Virtual address to read from
 * @param value      Out parameter for read result
 * @param size       Number of bytes to read (must be 1, 2, 4, 8)
 * @param pc         Program Counter for fault context
 * @return true if successful, false if fault occurred
 */
inline bool AlphaMemorySystem::readVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 &value, int size,
                                                 quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
        return false;

    safeIncrement(m_totalMemoryAccesses);

    // Perform TLB translation
    TranslationResult result = translateInternal(cpuId, virtualAddr, MemAccessType::READ, false /*isInstr*/);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, MemAccessType::READ);
            break;
        default:
            break;
        }
        raiseMemoryAccessException(virtualAddr, size, false /*isWrite*/, pc);
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();
    return accessPhysicalMemory(physicalAddr, value, size, false /*isWrite*/, pc, cpuId);
}


/**
 * @brief Write to virtual memory using TLB translation.
 * Performs TLB translation, protection checks, and traps on failure.
 *
 * @param cpuId      CPU initiating access
 * @param virtualAddr Virtual address to write to
 * @param value      Value to write
 * @param size       Number of bytes to write (must be 1, 2, 4, 8)
 * @param pc         Program Counter for fault context
 * @return true if successful, false if fault occurred
 */
inline bool AlphaMemorySystem::writeVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 value, int size,
                                                  quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
        return false;

    safeIncrement(m_totalMemoryAccesses);

    TranslationResult result = translateInternal(cpuId, virtualAddr, MemAccessType::WRITE, false /*isInstr*/);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, MemAccessType::WRITE);
            break;
        default:
            break;
        }
        raiseMemoryAccessException(virtualAddr, size, true /*isWrite*/, pc);
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();
    return accessPhysicalMemory(physicalAddr, value, size, true /*isWrite*/, pc, cpuId);
}


bool AlphaMemorySystem::writePhysicalMemory(quint64 physicalAddr, quint64 value, size_t size)
{
    if (!m_safeMemory)
        return false;

    if (isMMIOAddress(physicalAddr))
    {
        if (m_mmioManager)
        {
            return m_mmioManager->writeMMIO(physicalAddr, value, static_cast<int>(size), 0);
        }
        return false;
    }

    try
    {
        switch (size)
        {
        case 1:
            m_safeMemory->writeUInt8(physicalAddr, static_cast<quint8>(value), 0);
            break;
        case 2:
            m_safeMemory->writeUInt16(physicalAddr, static_cast<quint16>(value), 0);
            break;
        case 4:
            m_safeMemory->writeUInt32(physicalAddr, static_cast<quint32>(value), 0);
            break;
        case 8:
            m_safeMemory->writeUInt64(physicalAddr, value, 0);
            break;
        default:
            return false;
        }

        // Emit signal for monitoring
        emit sigMemoryWritten(physicalAddr, value, static_cast<int>(size));
        return true;
    }
    catch (...)
    {
        return false;
    }
}


bool AlphaMemorySystem::writeVirtualMemory(quint16 cpuId, quint64 virtualAddr, void *value, int size, quint64 pc)
{
    if (!value || size == 0)
    {
        return false;
    }

    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
    {
        return false;
    }

    // For larger writes, write in chunks
    const quint8 *buffer = static_cast<const quint8 *>(value);
    for (int offset = 0; offset < size; ++offset)
    {
        quint64 byteValue = static_cast<quint64>(buffer[offset]);
        if (!writeVirtualMemory(cpuId, virtualAddr + offset, byteValue, 1, pc))
        {
            return false;
        }
    }

    return true;
}

// =========================
// SMP LOAD-LOCKED/STORE-CONDITIONAL SUPPORT
// =========================

bool AlphaMemorySystem::isPageMapped(quint64 virtualAddress, quint64 asn, bool isWrite)
{
    // Check if a virtual page is mapped without faulting

    // Get page table base
    quint64 vptb = getVPTB(asn);
    if (vptb == 0)
    {
        return false; // No page table
    }

    // Calculate page table entry address
    quint64 vpn = virtualAddress >> 13; // Virtual page number (8KB pages)
    quint64 pteAddr = vptb + (vpn * 8); // Each PTE is 8 bytes

    // Read page table entry without faulting
    quint64 pte;
    if (!readPhysicalMemory(pteAddr, pte, 8))
    {
        return false; // Can't read PTE
    }

    // Check if PTE is valid
    if ((pte & 0x1) == 0)
    {
        return false; // Invalid PTE
    }

    // Check write permission if needed
    if (isWrite && (pte & 0x2) == 0)
    {
        return false; // Not writable
    }

    return true; // Page is mapped and accessible
}
bool AlphaMemorySystem::isKernelAddress(quint64 address)
{
    // Alpha kernel space is typically in upper half of address space
    return (address & 0x8000000000000000ULL) != 0;
}
bool AlphaMemorySystem::isKernelMode()
{
    if (m_currentCPU)
    {
        // Check if CPU is in kernel mode (PS register bit)
        quint64 ps = m_currentCPU->getPS();
        return (ps & 0x8) == 0; // Kernel mode when CM bit is 0
    }
    return false;
}

bool AlphaMemorySystem::isWritableAddress(quint64 address)
{
    // Check if address is in a writable region
    QReadLocker locker(&m_memoryLock);
    auto it = m_memoryMap.lowerBound(address);
    if (it != m_memoryMap.end() && address >= it.key() && address < (it.key() + it.value().size))
    {
        return (it.value().protectionFlags & 0x2) != 0; // Write permission bit
    }
    return false;
}

bool AlphaMemorySystem::loadLocked(quint16 cpuId, quint64 vaddr, quint64 &value, int size, quint64 pc)
{
    // First, perform normal load with translation
    if (!readVirtualMemory(cpuId, vaddr, value, size, pc))
    {
        return false;
    }

    // Get physical address for reservation tracking
    TranslationResult result = translateInternal(cpuId, vaddr, 0 /* read */, false /* data */);
    if (!result.isValid())
    {
        return false;
    }

    quint64 physAddr = result.getPhysicalAddress();

    // Set up reservation
    QWriteLocker locker(&m_reservationsLock);

    SMPReservationState &reservation = m_reservations[cpuId];
    reservation.physicalAddress = physAddr & ~0x7ULL; // Align to 8-byte boundary
    reservation.virtualAddress = vaddr;
    reservation.cpuId = cpuId;
    reservation.size = size;
    reservation.isValid = true;
    reservation.timestamp = getCurrentTimestamp();
    reservation.accessCount.store(1);

    DEBUG_LOG(QString("Load-locked: CPU%1, vaddr=0x%2, paddr=0x%3, size=%4")
                  .arg(cpuId)
                  .arg(vaddr, 0, 16)
                  .arg(reservation.physicalAddress, 0, 16)
                  .arg(size));

    return true;
}


void AlphaInstructionCache::setNextLevelCache(UnifiedDataCache *nextLevel)
{
    if (m_unifiedCache && nextLevel)
    {
        m_unifiedCache->setNextLevel(nextLevel);
        DEBUG_LOG("InstructionCache", "Connected to next level cache for CPU %u", m_cpuId);
    }
}


void AlphaMemorySystem::setupL3CacheIntegration()
{
    if (!m_level3SharedCache)
    {
        WARN_LOG("AlphaMemorySystem: No L3 cache available for integration setup");
        return;
    }

    // Connect L3 cache signals to memory system
    connect(m_level3SharedCache, &UnifiedDataCache::sigLineEvicted, this,
            [this](quint64 address, bool wasDirty)
            {
                if (wasDirty)
                {
                    // Notify all CPUs about dirty line eviction
                    emit sigMemoryWriteNotification(address, 64, 0xFFFF);
                }
                DEBUG_LOG("AlphaMemorySystem: L3 line evicted: addr=0x%1, dirty=%2", QString::number(address, 16),
                          wasDirty);
            });

    connect(m_level3SharedCache, &UnifiedDataCache::sigLineInvalidated, this,
            [this](quint64 address)
            {
                // Propagate invalidation to all CPU caches
                CacheCoherencyMessage msg;
                msg.type = CacheCoherencyMessage::INVALIDATE_LINE;
                msg.physicalAddress = address;
                msg.sourceCpuId = 0xFFFF;
                msg.targetCpuId = 0xFFFF;
                msg.size = 64;
                msg.timestamp = getCurrentTimestamp();
                sendCacheCoherencyMessage(msg);
            });

    connect(m_level3SharedCache, &UnifiedDataCache::sigCoherencyViolation, this,
            [this](quint64 address, const QString &operation)
            {
                ERROR_LOG("AlphaMemorySystem: L3 coherency violation: addr=0x%1, op=%2", QString::number(address, 16),
                          operation);
                emit sigCacheCoherencyEvent(address, 0xFFFF, "VIOLATION");
            });

    // Set up backing store integration
    m_level3SharedCache->setBackingRead([this](quint64 addr, void *buf, size_t size) -> bool
                                        { return readPhysicalMemory(addr, *static_cast<quint64 *>(buf), size); });

    DEBUG_LOG("AlphaMemorySystem: L3 cache integration setup complete");
}


bool AlphaMemorySystem::storeConditional(quint16 cpuId, quint64 vaddr, quint64 value, int size, quint64 pc)
{
    // Check if CPU has a valid reservation
    QReadLocker readLocker(&m_reservationsLock);
    if (!m_reservations.contains(cpuId) || !m_reservations[cpuId].isValid)
    {
        DEBUG_LOG(QString("Store-conditional failed: CPU%1, no reservation").arg(cpuId));
        return false;
    }

    // Get physical address
    TranslationResult result = translateInternal(cpuId, vaddr, 1 /* write */, false /* data */);
    if (!result.isValid())
    {
        // Clear reservation on translation failure
        QWriteLocker writeLocker(&m_reservationsLock);
        m_reservations[cpuId].clear();
        return false;
    }

    quint64 physAddr = result.getPhysicalAddress();
    SMPReservationState &reservation = m_reservations[cpuId];

    // Check if reservation matches this address
    if (!reservation.matches(physAddr, size))
    {
        QWriteLocker writeLocker(&m_reservationsLock);
        reservation.clear();
        DEBUG_LOG(QString("Store-conditional failed: CPU%1, address mismatch").arg(cpuId));
        return false;
    }
    readLocker.unlock();

    // Attempt the store
    if (!writeVirtualMemory(cpuId, vaddr, value, size, pc))
    {
        QWriteLocker writeLocker(&m_reservationsLock);
        reservation.clear();
        return false;
    }

    // Store succeeded - clear the reservation and invalidate overlapping reservations
    QWriteLocker writeLocker(&m_reservationsLock);
    reservation.clear();
    invalidateOverlappingReservations(physAddr, size, cpuId);

    DEBUG_LOG(QString("Store-conditional succeeded: CPU%1, vaddr=0x%2, paddr=0x%3")
                  .arg(cpuId)
                  .arg(vaddr, 0, 16)
                  .arg(physAddr, 0, 16));

    return true;
}

void AlphaMemorySystem::clearReservations(quint64 physAddr, int size, quint16 excludeCpuId)
{
    QWriteLocker locker(&m_reservationsLock);
    invalidateOverlappingReservations(physAddr, size, excludeCpuId);

    DEBUG_LOG(QString("Cleared reservations for paddr=0x%1, size=%2, excluding CPU%3")
                  .arg(physAddr, 0, 16)
                  .arg(size)
                  .arg(excludeCpuId));
}

void AlphaMemorySystem::clearCpuReservations(quint16 cpuId)
{
    QWriteLocker locker(&m_reservationsLock);
    if (m_reservations.contains(cpuId))
    {
        m_reservations[cpuId].clear();
        DEBUG_LOG(QString("Cleared all reservations for CPU%1").arg(cpuId));
    }
}
quint64 AlphaMemorySystem::getVPTB(quint64 asn)
{
    // Get Virtual Page Table Base for given ASN
    // This might be stored per-process or globally
    if (m_currentCPU)
    {
        return m_currentCPU->readIPR("VPTB");
    }
    return 0;
}

bool AlphaMemorySystem::hasReservation(quint16 cpuId, quint64 physAddr) const
{
    QReadLocker locker(&m_reservationsLock);
    if (!m_reservations.contains(cpuId))
    {
        return false;
    }

    const SMPReservationState &reservation = m_reservations[cpuId];
    return reservation.isValid && reservation.matches(physAddr, 1);
}

// =========================
// CACHE COHERENCY AND SMP COORDINATION
// =========================

void AlphaMemorySystem::sendCacheCoherencyMessage(const CacheCoherencyMessage &message)
{
    QMutexLocker locker(&m_coherencyLock);

    asa_utils::safeIncrement(m_cacheCoherencyEvents);

    // Route to L3 shared cache first
    if (m_level3SharedCache && message.targetCpuId == 0xFFFF)
    {
        switch (message.type)
        {
        case CacheCoherencyMessage::INVALIDATE_LINE:
            m_level3SharedCache->invalidateLine(message.physicalAddress);
            break;
        case CacheCoherencyMessage::FLUSH_LINE:
            m_level3SharedCache->flushLine(message.physicalAddress);
            break;
        case CacheCoherencyMessage::WRITE_BACK:
            if (m_level3SharedCache->isDirty(message.physicalAddress))
            {
                m_level3SharedCache->writeBackLine(message.physicalAddress);
            }
            break;
        }
    }

    // Then route to individual CPUs
    if (message.targetCpuId == 0xFFFF)
    {
        broadcastMessage(message);
    }
    else
    {
        sendMessageToCPU(message.targetCpuId, message);
    }

    DEBUG_LOG("AlphaMemorySystem: Cache coherency message sent: type=%1, addr=0x%2, target=CPU%3",
              static_cast<int>(message.type), QString::number(message.physicalAddress, 16), message.targetCpuId);
}

void AlphaMemorySystem::invalidateCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    CacheCoherencyMessage msg;
    msg.type = CacheCoherencyMessage::INVALIDATE_LINE;
    msg.physicalAddress = physicalAddr;
    msg.sourceCpuId = sourceCpuId;
    msg.targetCpuId = 0xFFFF; // Broadcast
    msg.size = size;
    msg.timestamp = getCurrentTimestamp();

    sendCacheCoherencyMessage(msg);

    emit sigCacheCoherencyEvent(physicalAddr, sourceCpuId, "INVALIDATE");
}

void AlphaMemorySystem::flushCacheLines(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    CacheCoherencyMessage msg;
    msg.type = CacheCoherencyMessage::FLUSH_LINE;
    msg.physicalAddress = physicalAddr;
    msg.sourceCpuId = sourceCpuId;
    msg.targetCpuId = 0xFFFF; // Broadcast
    msg.size = size;
    msg.timestamp = getCurrentTimestamp();

    sendCacheCoherencyMessage(msg);

    emit sigCacheCoherencyEvent(physicalAddr, sourceCpuId, "FLUSH");
}

// =========================
// TLB MANAGEMENT (SMP-aware)
// =========================

/**
 * @brief Validate TLB entry before insertion to prevent corruption
 * @param entry TLB entry to validate
 * @param cpuId CPU that will own this entry
 * @return True if entry is valid and safe
 */
bool AlphaMemorySystem::validateTLBEntry(const TLBEntry &entry, quint16 cpuId)
{
    // Check basic validity
    if (!entry.isValid())
    {
        ERROR_LOG("AlphaMemorySystem: Attempting to insert invalid TLB entry");
        return false;
    }

    // Validate virtual address alignment
    quint64 pageSize = entry.getPageSize();
    if (pageSize == 0)
    {
        ERROR_LOG("AlphaMemorySystem: TLB entry has zero page size");
        return false;
    }

    quint64 virtualAddr = entry.getVirtualAddress();
    if ((virtualAddr % pageSize) != 0)
    {
        ERROR_LOG(QString("AlphaMemorySystem: TLB entry virtual address 0x%1 not aligned to page size %2")
                      .arg(virtualAddr, 0, 16)
                      .arg(pageSize));
        return false;
    }

    // Validate physical address alignment
    quint64 physicalAddr = entry.getPhysicalAddress();
    if ((physicalAddr % pageSize) != 0)
    {
        ERROR_LOG(QString("AlphaMemorySystem: TLB entry physical address 0x%1 not aligned to page size %2")
                      .arg(physicalAddr, 0, 16)
                      .arg(pageSize));
        return false;
    }

    // Check for reasonable address ranges
    if (!isValidVirtualAddress(virtualAddr))
    {
        ERROR_LOG(QString("AlphaMemorySystem: TLB entry has invalid virtual address 0x%1").arg(virtualAddr, 0, 16));
        return false;
    }

    if (!isValidPhysicalAddress(physicalAddr))
    {
        ERROR_LOG(QString("AlphaMemorySystem: TLB entry has invalid physical address 0x%1").arg(physicalAddr, 0, 16));
        return false;
    }

    // Validate protection flags
    quint32 protection = entry.getProtection();
    if (!isValidProtectionFlags(protection))
    {
        ERROR_LOG(QString("AlphaMemorySystem: TLB entry has invalid protection flags 0x%1").arg(protection, 0, 16));
        return false;
    }

    // Check for conflicting entries (this is expensive but important for safety)
    if (m_tlbSystem && m_tlbSystem->isCPURegistered(cpuId))
    {
        TLBSystem::TLBStats stats = m_tlbSystem->getTLBStats(cpuId);
        if (stats.validEntries >= stats.entries)
        {
            // TLB is full - this will cause LRU eviction, which is okay
            DEBUG_LOG(QString("AlphaMemorySystem: TLB full for CPU %1, will evict LRU entry").arg(cpuId));
        }
    }

    return true;
}


/**
 * @brief Validate TLB system integrity
 * @return True if TLB system appears healthy
 */
bool AlphaMemorySystem::validateTLBSystemIntegrity()
{
    if (!m_tlbSystem)
    {
        ERROR_LOG("AlphaMemorySystem: TLB system is null during integrity check");
        return false;
    }

    try
    {
        // Check that all registered CPUs have valid TLB data
        QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

        for (quint16 cpuId : registeredCPUs)
        {
            if (!m_tlbSystem->isCPURegistered(cpuId))
            {
                ERROR_LOG(QString("AlphaMemorySystem: CPU %1 appears in list but not registered").arg(cpuId));
                return false;
            }

            // Get TLB statistics to verify TLB is accessible
            TLBSystem::TLBStats stats = m_tlbSystem->getTLBStats(cpuId);

            // Sanity check the statistics
            if (stats.validEntries > stats.entries)
            {
                ERROR_LOG(QString("AlphaMemorySystem: CPU %1 has more valid entries (%2) than total entries (%3)")
                              .arg(cpuId)
                              .arg(stats.validEntries)
                              .arg(stats.entries));
                return false;
            }

            if (stats.entries == 0)
            {
                ERROR_LOG(QString("AlphaMemorySystem: CPU %1 has zero TLB entries").arg(cpuId));
                return false;
            }
        }

        // Check that CPU registry matches TLB registration
        {
            QReadLocker locker(&m_cpuRegistryLock);
            for (auto it = m_cpuRegistry.constBegin(); it != m_cpuRegistry.constEnd(); ++it)
            {
                quint16 cpuId = it.key();
                if (!m_tlbSystem->isCPURegistered(cpuId))
                {
                    ERROR_LOG(QString("AlphaMemorySystem: CPU %1 in registry but not in TLB system").arg(cpuId));
                    return false;
                }
            }
        }

        DEBUG_LOG(
            QString("AlphaMemorySystem: TLB system integrity check passed for %1 CPUs").arg(registeredCPUs.size()));
        return true;
    }
    catch (const std::exception &e)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Exception during TLB integrity check: %1").arg(e.what()));
        return false;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Unknown exception during TLB integrity check");
        return false;
    }
}

/**
 * @brief Invalidate single TLB entry across all CPUs with internal TLB coordination
 * @param virtualAddr Virtual address to invalidate
 * @param sourceCpuId CPU that initiated the invalidation
 */
void AlphaMemorySystem::invalidateTlbSingle(quint64 virtualAddr, quint16 sourceCpuId)
{
    asa_utils::safeIncrement(m_tlbInvalidations);
   // m_tlbInvalidations.fetchAndAddRelaxed(1);

    // Get current ASN for invalidation
    quint64 currentASN = getCurrentASN();

    // =====================================================
    // PHASE 1: INTERNAL TLB SYSTEM INVALIDATION (New)
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            // Invalidate on source CPU's TLB first
            if (m_tlbSystem->isCPURegistered(sourceCpuId))
            {
                m_tlbSystem->invalidateEntry(sourceCpuId, virtualAddr, currentASN);
                DEBUG_LOG(QString("Internal TLB invalidated for source CPU %1, VA=0x%2")
                              .arg(sourceCpuId)
                              .arg(virtualAddr, 0, 16));
            }

            // Get list of all registered CPUs for broadcast invalidation
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

            // Invalidate on all OTHER CPUs' internal TLBs
            for (quint16 cpuId : registeredCPUs)
            {
                if (cpuId != sourceCpuId)
                {
                    m_tlbSystem->invalidateEntry(cpuId, virtualAddr, currentASN);
                    DEBUG_LOG(
                        QString("Internal TLB invalidated for CPU %1, VA=0x%2").arg(cpuId).arg(virtualAddr, 0, 16));
                }
            }
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("Exception during internal TLB invalidation: %1").arg(e.what()));
            handleTLBError(sourceCpuId, QString("Internal TLB invalidation failed: %1").arg(e.what()));
        }
        catch (...)
        {
            ERROR_LOG("Unknown exception during internal TLB invalidation");
            handleTLBError(sourceCpuId, "Internal TLB invalidation unknown error");
        }
    }
}

/**
 * @brief Invalidate data TLB entries across all CPUs
 * @param virtualAddr Virtual address to invalidate
 * @param sourceCpuId CPU that initiated the invalidation
 */
void AlphaMemorySystem::invalidateTlbSingleData(quint64 virtualAddr, quint16 sourceCpuId)
{
    asa_utils::safeIncrement(m_tlbInvalidations);
   // m_tlbInvalidations.fetchAndAddRelaxed(1);

    quint64 currentASN = getCurrentASN();

    // =====================================================
    // PHASE 1: INTERNAL TLB DATA INVALIDATION
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            // Get all registered CPUs
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

            // Invalidate data entries on all CPUs (including source)
            for (quint16 cpuId : registeredCPUs)
            {
                if (m_tlbSystem->isCPURegistered(cpuId))
                {
                    m_tlbSystem->invalidateDataEntry(cpuId, virtualAddr, currentASN);
                }
            }

            DEBUG_LOG(QString("Internal data TLB invalidated for all CPUs, VA=0x%1").arg(virtualAddr, 0, 16));
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("Exception during internal data TLB invalidation: %1").arg(e.what()));
            handleTLBError(sourceCpuId, QString("Data TLB invalidation failed: %1").arg(e.what()));
        }
        catch (...)
        {
            ERROR_LOG("Unknown exception during internal data TLB invalidation");
            handleTLBError(sourceCpuId, "Data TLB invalidation unknown error");
        }
    }

    // =====================================================
    // PHASE 2: EXTERNAL CPU DATA NOTIFICATION
    // =====================================================

    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it.key() != sourceCpuId && it->isOnline && it->cpu)
        {
            try
            {
                it->cpu->invalidateTBSingleData(virtualAddr);
            }
            catch (...)
            {
                WARN_LOG(QString("Failed to send external data TLB invalidation to CPU %1").arg(it.key()));
            }
        }
    }

    DEBUG_LOG(
        QString("Complete data TLB invalidation: VA=0x%1, source=CPU%2").arg(virtualAddr, 0, 16).arg(sourceCpuId));

    emit sigTlbInvalidated();
}


/**
 * @brief Invalidate instruction TLB entries across all CPUs
 * @param virtualAddr Virtual address to invalidate
 * @param sourceCpuId CPU that initiated the invalidation
 */
void AlphaMemorySystem::invalidateTlbSingleInstruction(quint64 virtualAddr, quint16 sourceCpuId)
{
    asa_utils::safeIncrement(m_tlbInvalidations);
    //m_tlbInvalidations.fetchAndAddRelaxed(1);

    quint64 currentASN = getCurrentASN();

    // =====================================================
    // PHASE 1: INTERNAL TLB INSTRUCTION INVALIDATION
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

            // Invalidate instruction entries on all CPUs
            for (quint16 cpuId : registeredCPUs)
            {
                if (m_tlbSystem->isCPURegistered(cpuId))
                {
                    m_tlbSystem->invalidateInstructionEntry(cpuId, virtualAddr, currentASN);
                }
            }

            DEBUG_LOG(QString("Internal instruction TLB invalidated for all CPUs, VA=0x%1").arg(virtualAddr, 0, 16));
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("Exception during internal instruction TLB invalidation: %1").arg(e.what()));
            handleTLBError(sourceCpuId, QString("Instruction TLB invalidation failed: %1").arg(e.what()));
        }
        catch (...)
        {
            ERROR_LOG("Unknown exception during internal instruction TLB invalidation");
            handleTLBError(sourceCpuId, "Instruction TLB invalidation unknown error");
        }
    }

    // =====================================================
    // PHASE 2: EXTERNAL CPU INSTRUCTION NOTIFICATION
    // =====================================================

    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it.key() != sourceCpuId && it->isOnline && it->cpu)
        {
            try
            {
                it->cpu->invalidateTBSingleInstruction(virtualAddr);
            }
            catch (...)
            {
                WARN_LOG(QString("Failed to send external instruction TLB invalidation to CPU %1").arg(it.key()));
            }
        }
    }

    DEBUG_LOG(QString("Complete instruction TLB invalidation: VA=0x%1, source=CPU%2")
                  .arg(virtualAddr, 0, 16)
                  .arg(sourceCpuId));

    emit sigTlbInvalidated();
}

void AlphaMemorySystem::invalidateTLBEntry(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    // Input validation
    if (!isValidVirtualAddress(virtualAddr))
    {
        ERROR_LOG(
            QString("AlphaMemorySystem: Invalid virtual address 0x%1 for TLB invalidation").arg(virtualAddr, 0, 16));
        return;
    }

    if (asn > 255)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Invalid ASN %1 for TLB invalidation (max 255)").arg(asn));
        return;
    }

    // Update statistics
    asa_utils::safeIncrement(m_tlbInvalidations);

    DEBUG_LOG(QString("AlphaMemorySystem: Invalidating TLB entry VA=0x%1, ASN=%2, source=CPU%3")
                  .arg(virtualAddr, 0, 16)
                  .arg(asn)
                  .arg(sourceCpuId));

    // =====================================================
    // PHASE 1: INTERNAL TLB SYSTEM INVALIDATION
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();
            int totalInvalidated = 0;

            // Track performance
            QElapsedTimer timer;
            timer.start();

            for (quint16 cpuId : registeredCPUs)
            {
                if (m_tlbSystem->isCPURegistered(cpuId))
                {
                    // Get stats before invalidation for monitoring
                    TLBSystem::TLBStats statsBefore = m_tlbSystem->getTLBStats(cpuId);

                    if (asn == 0)
                    {
                        // Global invalidation - invalidate this VA for all ASNs
                        invalidateTLBEntryAllASNs(cpuId, virtualAddr);
                    }
                    else
                    {
                        // Specific ASN invalidation
                        m_tlbSystem->invalidateEntry(cpuId, virtualAddr, asn);
                    }

                    // Calculate invalidation impact
                    TLBSystem::TLBStats statsAfter = m_tlbSystem->getTLBStats(cpuId);
                    int invalidated = statsBefore.validEntries - statsAfter.validEntries;
                    totalInvalidated += invalidated;

                    if (invalidated > 0)
                    {
                        DEBUG_LOG(
                            QString("Internal TLB invalidated %1 entries for CPU %2").arg(invalidated).arg(cpuId));
                    }
                }
            }

            qint64 elapsedMicroseconds = timer.nsecsElapsed() / 1000;

            DEBUG_LOG(QString("Internal TLB entry invalidation complete: %1 entries, %2?s, %3 CPUs")
                          .arg(totalInvalidated)
                          .arg(elapsedMicroseconds)
                          .arg(registeredCPUs.size()));
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("Exception during internal TLB entry invalidation: %1").arg(e.what()));
            handleTLBError(sourceCpuId, QString("TLB entry invalidation failed: %1").arg(e.what()));
        }
        catch (...)
        {
            ERROR_LOG("Unknown exception during internal TLB entry invalidation");
            handleTLBError(sourceCpuId, "TLB entry invalidation unknown error");
        }
    }

    // =====================================================
    // PHASE 2: EXTERNAL CPU NOTIFICATION
    // =====================================================

    notifyExternalCPUsOfTLBInvalidation(virtualAddr, asn, sourceCpuId);

    // =====================================================
    // PHASE 3: CACHE COHERENCY COORDINATION
    // =====================================================

    handleTLBInvalidationCacheCoherency(virtualAddr, asn, sourceCpuId);

    // =====================================================
    // PHASE 4: MONITORING AND CLEANUP
    // =====================================================

    // Emit monitoring signal
    emit sigTlbEntryInvalidated(virtualAddr, asn, sourceCpuId);
    emit sigTlbInvalidated(); // General TLB invalidation signal

    DEBUG_LOG(QString("Complete TLB entry invalidation: VA=0x%1, ASN=%2, source=CPU%3")
                  .arg(virtualAddr, 0, 16)
                  .arg(asn)
                  .arg(sourceCpuId));
}

/**
 * @brief Invalidate TLB entry for all ASNs on a specific CPU
 * @param cpuId CPU to invalidate on
 * @param virtualAddr Virtual address to invalidate
 */
void AlphaMemorySystem::invalidateTLBEntryAllASNs(quint16 cpuId, quint64 virtualAddr)
{
    if (!m_tlbSystem || !m_tlbSystem->isCPURegistered(cpuId))
    {
        return;
    }

    try
    {
        // Alpha supports ASN 0-255
        for (quint64 asn = 0; asn <= 255; ++asn)
        {
            m_tlbSystem->invalidateEntry(cpuId, virtualAddr, asn);
        }

        DEBUG_LOG(QString("Invalidated VA=0x%1 for all ASNs on CPU %2").arg(virtualAddr, 0, 16).arg(cpuId));
    }
    catch (const std::exception &e)
    {
        ERROR_LOG(QString("Exception invalidating all ASNs for CPU %1: %2").arg(cpuId).arg(e.what()));
    }
    catch (...)
    {
        ERROR_LOG(QString("Unknown exception invalidating all ASNs for CPU %1").arg(cpuId));
    }
}


/**
 * @brief Invalidate all TLB entries for specific ASN across all CPUs
 * @param asn Address Space Number to invalidate
 * @param sourceCpuId CPU that initiated the invalidation
 */
void AlphaMemorySystem::invalidateTLBByASN(quint64 asn, quint16 sourceCpuId)
{
    asa_utils::safeIncrement(m_tlbInvalidations);
  //  m_tlbInvalidations.fetchAndAddRelaxed(1);

    // =====================================================
    // PHASE 1: INTERNAL TLB ASN INVALIDATION
    // =====================================================
    
    if (m_tlbSystem) {
        try {
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();
            
            // Track invalidation statistics
            int totalInvalidated = 0;
            
            // Invalidate by ASN on all registered CPUs
            for (quint16 cpuId : registeredCPUs) {
                if (m_tlbSystem->isCPURegistered(cpuId)) {
                    // Get stats before invalidation for logging
                    TLBSystem::TLBStats statsBefore = m_tlbSystem->getTLBStats(cpuId);
                    
                    m_tlbSystem->invalidateByASN(cpuId, asn);
                    
                    // Get stats after invalidation
                    TLBSystem::TLBStats statsAfter = m_tlbSystem->getTLBStats(cpuId);
                    int invalidatedForCPU = statsBefore.validEntries - statsAfter.validEntries;
                    totalInvalidated += invalidatedForCPU;
                    
                    DEBUG_LOG(QString("Internal TLB ASN invalidation for CPU %1: %2 entries invalidated")
                              .arg(cpuId).arg(invalidatedForCPU));
                }
            }
            
            DEBUG_LOG(QString("Internal TLB ASN %1 invalidation complete: %2 total entries invalidated")
                      .arg(asn).arg(totalInvalidated));
            
        } catch (const std::exception &e) {
            ERROR_LOG(QString("Exception during internal TLB ASN invalidation: %1").arg(e.what()));
            handleTLBError(sourceCpuId, QString("ASN TLB invalidation failed: %1").arg(e.what()));
        } catch (...) {
            ERROR_LOG("Unknown exception during internal TLB ASN invalidation");
            handleTLBError(sourceCpuId, "ASN TLB invalidation unknown error");
        }
    }

    // =====================================================
    // PHASE 2: EXTERNAL CPU ASN NOTIFICATION
    // =====================================================
    
    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it) {
        if (it.key() != sourceCpuId && it->isOnline && it->cpu) {
            try {
                it->cpu->invalidateTBAllProcess();  // External CPU ASN invalidation
            } catch (...) {
                WARN_LOG(QString("Failed to send external ASN TLB invalidation to CPU %1").arg(it.key()));
            }
        }
    }

    DEBUG_LOG(QString("Complete ASN TLB invalidation: ASN=%1, source=CPU%2").arg(asn).arg(sourceCpuId));

    emit sigTlbInvalidated();
}

/**
 * @brief Invalidate all TLB entries for specific ASN across all CPUs
 * @param asn Address Space Number to invalidate
 * @param sourceCpuId CPU that initiated the invalidation
 */
void AlphaMemorySystem::invalidateTLBByASN(quint64 asn, quint16 sourceCpuId)
{
    asa_utils::safeIncrement(m_tlbInvalidations);
   // m_tlbInvalidations.fetchAndAddRelaxed(1);

    // =====================================================
    // PHASE 1: INTERNAL TLB ASN INVALIDATION
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

            // Track invalidation statistics
            int totalInvalidated = 0;

            // Invalidate by ASN on all registered CPUs
            for (quint16 cpuId : registeredCPUs)
            {
                if (m_tlbSystem->isCPURegistered(cpuId))
                {
                    // Get stats before invalidation for logging
                    TLBSystem::TLBStats statsBefore = m_tlbSystem->getTLBStats(cpuId);

                    m_tlbSystem->invalidateByASN(cpuId, asn);

                    // Get stats after invalidation
                    TLBSystem::TLBStats statsAfter = m_tlbSystem->getTLBStats(cpuId);
                    int invalidatedForCPU = statsBefore.validEntries - statsAfter.validEntries;
                    totalInvalidated += invalidatedForCPU;

                    DEBUG_LOG(QString("Internal TLB ASN invalidation for CPU %1: %2 entries invalidated")
                                  .arg(cpuId)
                                  .arg(invalidatedForCPU));
                }
            }

            DEBUG_LOG(QString("Internal TLB ASN %1 invalidation complete: %2 total entries invalidated")
                          .arg(asn)
                          .arg(totalInvalidated));
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("Exception during internal TLB ASN invalidation: %1").arg(e.what()));
            handleTLBError(sourceCpuId, QString("ASN TLB invalidation failed: %1").arg(e.what()));
        }
        catch (...)
        {
            ERROR_LOG("Unknown exception during internal TLB ASN invalidation");
            handleTLBError(sourceCpuId, "ASN TLB invalidation unknown error");
        }
    }

    // =====================================================
    // PHASE 2: EXTERNAL CPU ASN NOTIFICATION
    // =====================================================

    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it.key() != sourceCpuId && it->isOnline && it->cpu)
        {
            try
            {
                it->cpu->invalidateTBAllProcess(); // External CPU ASN invalidation
            }
            catch (...)
            {
                WARN_LOG(QString("Failed to send external ASN TLB invalidation to CPU %1").arg(it.key()));
            }
        }
    }

    DEBUG_LOG(QString("Complete ASN TLB invalidation: ASN=%1, source=CPU%2").arg(asn).arg(sourceCpuId));

    emit sigTlbInvalidated();
}

void AlphaMemorySystem::integrateTLBWithCaches()
{
    if (!m_tlbSystem)
    {
        WARN_LOG("AlphaMemorySystem: No TLB system available for cache integration");
        return;
    }

    // Integrate L3 shared cache with TLB system
    if (m_level3SharedCache)
    {
        m_level3SharedCache->setTLBSystem(m_tlbSystem, 0xFFFF); // Shared across all CPUs
        DEBUG_LOG("AlphaMemorySystem: Integrated L3 cache with TLB system");
    }

    // Integrate per-CPU caches with TLB system
    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it->cpu && it->isOnline)
        {
            quint16 cpuId = it.key();
            AlphaCPU *cpu = it->cpu;

            // Integrate CPU's data caches with TLB
            if (cpu->getLevel1DataCache())
            {
                cpu->getLevel1DataCache()->setTLBSystem(m_tlbSystem, cpuId);
            }
            if (cpu->getLevel2DataCache())
            {
                cpu->getLevel2DataCache()->setTLBSystem(m_tlbSystem, cpuId);
            }

            // Integrate instruction cache
            if (cpu->getInstructionCache())
            {
                auto *unifiedICache = cpu->getInstructionCache()->getUnifiedCache();
                if (unifiedICache)
                {
                    unifiedICache->setTLBSystem(m_tlbSystem, cpuId);
                }
            }

            DEBUG_LOG("AlphaMemorySystem: Integrated TLB with caches for CPU %1", cpuId);
        }
    }
}

/**
 * @brief Global TLB flush across all CPUs
 * @param sourceCpuId CPU that initiated the invalidation
 */
void AlphaMemorySystem::invalidateAllTLB(quint16 sourceCpuId)
{
    asa_utils::safeIncrement(m_tlbInvalidations);
    //m_tlbInvalidations.fetchAndAddRelaxed(1);

    // =====================================================
    // PHASE 1: INTERNAL TLB GLOBAL FLUSH
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

            // Get total statistics before flush
            int totalEntriesBefore = 0;
            for (quint16 cpuId : registeredCPUs)
            {
                if (m_tlbSystem->isCPURegistered(cpuId))
                {
                    TLBSystem::TLBStats stats = m_tlbSystem->getTLBStats(cpuId);
                    totalEntriesBefore += stats.validEntries;
                }
            }

            // Perform global flush on all CPUs
            for (quint16 cpuId : registeredCPUs)
            {
                if (m_tlbSystem->isCPURegistered(cpuId))
                {
                    m_tlbSystem->invalidateAll(cpuId);
                    DEBUG_LOG(QString("Internal TLB flushed for CPU %1").arg(cpuId));
                }
            }

            DEBUG_LOG(QString("Internal global TLB flush complete: %1 entries invalidated across %2 CPUs")
                          .arg(totalEntriesBefore)
                          .arg(registeredCPUs.size()));

            // Also invalidate translation cache globally
            if (m_translationCache)
            {
                m_translationCache->invalidateAll();
                DEBUG_LOG("Translation cache globally invalidated");
            }
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("Exception during internal global TLB flush: %1").arg(e.what()));
            handleTLBError(0xFFFF, QString("Global TLB flush failed: %1").arg(e.what()));
        }
        catch (...)
        {
            ERROR_LOG("Unknown exception during internal global TLB flush");
            handleTLBError(0xFFFF, "Global TLB flush unknown error");
        }
    }

    // =====================================================
    // PHASE 2: EXTERNAL CPU GLOBAL NOTIFICATION
    // =====================================================

    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it.key() != sourceCpuId && it->isOnline && it->cpu)
        {
            try
            {
                it->cpu->invalidateTBAll(); // External CPU global flush
            }
            catch (...)
            {
                WARN_LOG(QString("Failed to send external global TLB flush to CPU %1").arg(it.key()));
            }
        }
    }

    DEBUG_LOG(QString("Complete global TLB flush: source=CPU%1").arg(sourceCpuId));

    emit sigTlbInvalidated();
}


// =========================
// MEMORY MAPPING (unchanged interface but with SMP coordination)
// =========================

void AlphaMemorySystem::mapMemory(quint64 virtualAddr, quint64 physicalAddr, quint64 size, int protectionFlags)
{
    QWriteLocker locker(&m_memoryLock);
    MappingEntry entry;
    entry.physicalBase = physicalAddr;
    entry.size = size;
    entry.protectionFlags = protectionFlags;
    m_memoryMap.insert(virtualAddr, entry);

    // Invalidate TLB entries that might be affected by this mapping
    invalidateAllTLB(0xFFFF);
}

bool AlphaMemorySystem::translateAddressNonFaulting(quint64 virtualAddress, quint64 &physicalAddress, bool isWrite)
{
    // Try to translate address without causing exceptions

    // Check TLB first
    if (m_tlbSystem)
    {
        if (m_tlbSystem->lookup(virtualAddress, getCurrentASN(), &physicalAddress))
        {
            return true; // TLB hit
        }
    }

    // TLB miss - check page table without faulting
    return translateViaPageTable(virtualAddress, physicalAddress, false); // false = don't fault
}

bool AlphaMemorySystem::translateViaPageTable(quint64 virtualAddress, quint64 &physicalAddress, bool allowFault)
{
    // Translate address using page table

    // Get current ASN and VPTB
    quint64 asn = getCurrentASN();
    quint64 vptb = getVPTB(asn);

    if (vptb == 0)
    {
        if (allowFault)
        {
            // Raise page table fault
            raiseTLBMiss(virtualAddress, asn);
        }
        return false;
    }

    // Calculate PTE address
    quint64 vpn = virtualAddress >> 13;
    quint64 pteAddr = vptb + (vpn * 8);

    // Read PTE
    quint64 pte;
    if (!readPhysicalMemory(pteAddr, pte, 8))
    {
        if (allowFault)
        {
            raiseTLBMiss(virtualAddress, asn);
        }
        return false;
    }

    // Check validity
    if ((pte & 0x1) == 0)
    {
        if (allowFault)
        {
            raiseTLBMiss(virtualAddress, asn);
        }
        return false;
    }

    // Extract physical address
    quint64 pfn = (pte >> 13) & 0x1FFFFFF;    // Physical frame number
    quint64 offset = virtualAddress & 0x1FFF; // Page offset (13 bits)
    physicalAddress = (pfn << 13) | offset;

    return true;
}


void AlphaMemorySystem::unmapMemory(quint64 virtualAddr)
{
    QWriteLocker locker(&m_memoryLock);
    m_memoryMap.remove(virtualAddr);

    // Invalidate TLB entries that might be affected
    invalidateAllTLB(0xFFFF);
}

void AlphaMemorySystem::clearMappings()
{
    QWriteLocker locker(&m_memoryLock);
    m_memoryMap.clear();

    // Invalidate all TLB entries on all CPUs
    invalidateAllTLB(0xFFFF);

    emit sigMappingsCleared();
}

bool AlphaMemorySystem::checkAccess(quint64 vaddr, int accessType) const
{
    QReadLocker locker(&m_memoryLock);

    auto it = m_memoryMap.lowerBound(vaddr);
    if (it == m_memoryMap.constEnd() || vaddr < it.key() || vaddr >= (it.key() + it.value().size))
    {
        return false;
    }

    const MappingEntry &entry = it.value();
    return (entry.protectionFlags & accessType) == accessType;
}

bool AlphaMemorySystem::translate(quint64 virtualAddr, quint64 &physicalAddr, int accessType)
{
    // Use CPU 0 as default for legacy compatibility
    TranslationResult result = translateInternal(0, virtualAddr, accessType, false);
    if (result.isValid())
    {
        physicalAddr = result.getPhysicalAddress();
        return true;
    }
    return false;
}

QVector<QPair<quint64, MappingEntry>> AlphaMemorySystem::getMappedRegions() const
{
    QReadLocker locker(&m_memoryLock);

    QVector<QPair<quint64, MappingEntry>> regions;
    for (auto it = m_memoryMap.constBegin(); it != m_memoryMap.constEnd(); ++it)
    {
        regions.append(qMakePair(it.key(), it.value()));
    }
    return regions;
}

// =========================
// UTILITY METHODS
// =========================

void AlphaMemorySystem::initialize()
{
    // Initialize atomic counters using Qt atomic operations
    m_totalMemoryAccesses.storeRelaxed(0);
    m_cacheCoherencyEvents.storeRelaxed(0);
    m_reservationConflicts.storeRelaxed(0);
    m_tlbInvalidations.storeRelaxed(0);
    m_totalTranslations.storeRelaxed(0);
    m_pageFaults.storeRelaxed(0);
    m_protectionFaults.storeRelaxed(0);

    DEBUG_LOG("AlphaMemorySystem: SMP-aware memory system initialized with Qt atomics");
}


void AlphaMemorySystem::raiseMemoryAccessException(quint64 address, int size, bool isWrite, quint64 pc)
{
    // Use your MemoryAccessException class

    // Determine the specific memory fault type
    MemoryFaultType faultType = determineMemoryFaultType(address, size, isWrite);

    // Create and throw your MemoryAccessException
    MemoryAccessException memException(faultType, address, size, isWrite, pc);

    qDebug() << QString("Raising Memory Access Exception: Type=%1, Addr=0x%2, Size=%3, Write=%4, PC=0x%5")
                    .arg(static_cast<int>(faultType))
                    .arg(address, 0, 16)
                    .arg(size)
                    .arg(isWrite)
                    .arg(pc, 0, 16);

    throw memException;
}

void AlphaMemorySystem::raiseTLBMiss(quint64 virtualAddress, quint64 asn)
{
    // Use your TLBExceptionQ class
    quint64 currentPC = getCurrentPC();

    // Determine the specific TLB exception type
    excTLBException tlbType = determineTLBExceptionType(virtualAddress, asn);

    // Create and throw your TLBExceptionQ
    TLBExceptionQ tlbException(tlbType, virtualAddress, currentPC);

    // Add exception frame if needed
    ExceptionFrame frame;
    frame.pc = currentPC;
    frame.ps = getCurrentPS();
    frame.virtualAddress = virtualAddress;
    tlbException.pushFrame(frame);

    qDebug() << QString("Raising TLB Exception: %1 at VA=0x%2, ASN=%3")
                    .arg(static_cast<int>(tlbType))
                    .arg(virtualAddress, 0, 16)
                    .arg(asn);

    throw tlbException;
}

bool AlphaMemorySystem::readBlock(quint64 physicalAddr, void *buffer, size_t size, quint64 pc)
{
    QReadLocker locker(&m_memoryLock);

    if (!buffer || size == 0)
        return false;

    if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr))
    {
        bool success = false;
        quint64 mmioVal = m_mmioManager->readMMIO(physicalAddr, static_cast<int>(size), pc);
        if (success)
        {
            std::memcpy(buffer, &mmioVal, size);
            emit sigMemoryRead(physicalAddr, physicalAddr, static_cast<int>(size));
        }
        return success;
    }
    else
    {
        for (size_t i = 0; i < size; ++i)
        {
            static_cast<quint8 *>(buffer)[i] = m_safeMemory->readUInt8(physicalAddr + i, pc);
        }
        emit sigMemoryRead(physicalAddr, physicalAddr, static_cast<int>(size));
        return true;
    }
}

bool AlphaMemorySystem::wouldCauseTLBMiss(quint64 virtualAddress, quint64 asn, bool isWrite)
{
    // Check if this virtual address would cause a TLB miss using your TLB system

    if (m_tlbSystem)
    {
        quint64 physicalAddr;
        bool tlbHit = m_tlbSystem->lookup(virtualAddress, asn, &physicalAddr);
        return !tlbHit;
    }

    // Check page tables if no TLB system
    return !isPageMapped(virtualAddress, asn, isWrite);
}

bool AlphaMemorySystem::wouldCauseTLBMissSimple(quint64 virtualAddress, quint64 asn, bool isWrite)
{
    // Simple heuristic-based TLB miss detection

    // Check if address is in unmapped regions
    if (virtualAddress < 0x1000)
    {
        return true; // Very low addresses usually unmapped
    }

    if (virtualAddress >= 0x7FFFFFFFFFFF && virtualAddress < 0xFFFFFFFF80000000ULL)
    {
        return true; // Gap in Alpha address space
    }

    // For now, assume most addresses are mapped
    // This is conservative but safe
    return false;
}
bool AlphaMemorySystem::writeBlock(quint64 physicalAddr, const void *buffer, size_t size, quint64 pc)
{
    QWriteLocker locker(&m_memoryLock);
    if (!buffer || size == 0)
        return false;

    if (m_mmioManager && m_mmioManager->isMMIOAddress(physicalAddr))
    {
        quint64 value = 0;
        std::memcpy(&value, buffer, size);
        bool success = m_mmioManager->writeMMIO(physicalAddr, value, static_cast<int>(size), pc);
        if (success)
            emit sigMemoryWritten(physicalAddr, physicalAddr, static_cast<int>(size));
        return success;
    }
    else
    {
        for (size_t i = 0; i < size; ++i)
        {
            m_safeMemory->writeUInt8(physicalAddr + i, static_cast<const quint8 *>(buffer)[i], pc);
        }
        emit sigMemoryWritten(physicalAddr, physicalAddr, static_cast<int>(size));
        return true;
    }
}

void AlphaMemorySystem::resetMappingStatistics()
{
    m_totalTranslations.storeRelaxed(0);
    m_pageFaults.storeRelaxed(0);
    m_protectionFaults.storeRelaxed(0);
    m_totalMemoryAccesses.storeRelaxed(0);
    m_cacheCoherencyEvents.storeRelaxed(0);
    m_reservationConflicts.storeRelaxed(0);
    m_tlbInvalidations.storeRelaxed(0);
}

quint64 AlphaMemorySystem::getCurrentASN()
{
    // Get current Address Space Number from the current CPU context
    if (m_currentCPU)
    {
        return m_currentCPU->getCurrentASN();
    }

    // Fallback: try to get from any online CPU
    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it->isOnline && it->cpu)
        {
            return it->cpu->getCurrentASN();
        }
    }

    return 0; // Default ASN if no CPU available
}

quint64 AlphaMemorySystem::getCurrentPC()
{
    if (m_currentCPU)
    {
        return m_currentCPU->getPC();
    }
    return 0;
}

quint64 AlphaMemorySystem::getCurrentPS()
{
    if (m_currentCPU)
    {
        return m_currentCPU->getPS();
    }
    return 0;
}


quint64 AlphaMemorySystem::getCurrentTimestamp() const { return QDateTime::currentMSecsSinceEpoch(); }

// =========================
// PRIVATE HELPER METHODS
// =========================

AlphaCPU *AlphaMemorySystem::validateAndGetCPU(quint16 cpuId) const
{
    QReadLocker locker(&m_cpuRegistryLock);
    auto it = m_cpuRegistry.find(cpuId);
    if (it == m_cpuRegistry.end())
    {
        ERROR_LOG(QString("AlphaMemorySystem: Invalid CPU ID %1").arg(cpuId));
        return nullptr;
    }

    if (!it->isOnline)
    {
        WARN_LOG(QString("AlphaMemorySystem: CPU %1 is offline").arg(cpuId));
        return nullptr;
    }

    return it->cpu;
}

void AlphaMemorySystem::sendMessageToCPU(quint16 cpuId, const CacheCoherencyMessage &message)
{
    AlphaCPU *cpu = getCPU(cpuId);
    if (!cpu)
    {
        return;
    }

    switch (message.type)
    {
    case CacheCoherencyMessage::INVALIDATE_LINE:
        cpu->handleCacheCoherencyEvent(message.physicalAddress, "INVALIDATE");
        break;
    case CacheCoherencyMessage::FLUSH_LINE:
        cpu->handleCacheCoherencyEvent(message.physicalAddress, "FLUSH");
        break;
    case CacheCoherencyMessage::RESERVATION_CLEAR:
        cpu->invalidateReservation(message.physicalAddress, message.size);
        break;
    default:
        break;
    }
}

void AlphaMemorySystem::attachL3Cache(UnifiedDataCache *l3Cache)
{
    if (!l3Cache)
    {
        WARN_LOG("AlphaMemorySystem: Null L3 cache provided");
        return;
    }

    m_level3SharedCache = l3Cache;

    // Configure L3 cache with TLB system
    if (m_tlbSystem)
    {
        m_level3SharedCache->setTLBSystem(m_tlbSystem, 0xFFFF); // Shared across all CPUs
        DEBUG_LOG("AlphaMemorySystem: L3 cache integrated with TLB system");
    }

    // Set up backing store integration
    m_level3SharedCache->setBackingRead(
        [this](quint64 addr, void *buf, size_t size) -> bool
        {
            quint64 value;
            if (this->readPhysicalMemory(addr, value, size))
            {
                std::memcpy(buf, &value, size);
                return true;
            }
            return false;
        });

    // Connect coherency signals
    connect(m_level3SharedCache, &UnifiedDataCache::sigLineEvicted, this,
            [this](quint64 address, bool wasDirty)
            {
                if (wasDirty)
                {
                    emit sigMemoryWriteNotification(address, 64, 0xFFFF);
                }
            });

    DEBUG_LOG("AlphaMemorySystem: L3 shared cache attached and configured");
}

void AlphaMemorySystem::attachTLBCacheIntegrator(tlbCacheIntegrator *integrator)
{
    if (!integrator)
    {
        WARN_LOG("AlphaMemorySystem: Cannot attach null TLB cache integrator");
        return;
    }

    // Connect integrator to L3 shared cache
    if (m_level3SharedCache)
    {
        QVector<quint16> allCpuIds;
        {
            QReadLocker locker(&m_cpuRegistryLock);
            for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
            {
                if (it->isOnline)
                {
                    allCpuIds.append(it.key());
                }
            }
        }

        integrator->attachUnifiedDataCache(allCpuIds, m_level3SharedCache);
        DEBUG_LOG("AlphaMemorySystem: Attached L3 cache to TLB integrator for %1 CPUs", allCpuIds.size());
    }

    // Connect integrator to per-CPU caches
    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it->cpu && it->isOnline)
        {
            quint16 cpuId = it.key();
            AlphaCPU *cpu = it->cpu;

            // Attach L1 data cache
            if (cpu->getLevel1DataCache())
            {
                integrator->attachCacheController(cpuId, tlbCacheIntegrator::CACHE_L1_DATA, cpu->getLevel1DataCache());
            }

            // Attach L2 cache
            if (cpu->getLevel2DataCache())
            {
                integrator->attachCacheController(cpuId, tlbCacheIntegrator::CACHE_L2_UNIFIED,
                                                  cpu->getLevel2DataCache());
            }

            // Attach instruction cache
            if (cpu->getInstructionCache())
            {
                auto *unifiedICache = cpu->getInstructionCache()->getUnifiedCache();
                if (unifiedICache)
                {
                    integrator->attachCacheController(cpuId, tlbCacheIntegrator::CACHE_L1_INSTRUCTION, unifiedICache);
                }
            }

            DEBUG_LOG("AlphaMemorySystem: Attached CPU %1 caches to TLB integrator", cpuId);
        }
    }

    // Initialize signals and slots
    integrator->initialize_SignalsAndSlots();

    DEBUG_LOG("AlphaMemorySystem: TLB cache integrator attachment complete");
}


void AlphaMemorySystem::broadcastMessage(memoryBarrierEmulationModeType type, quint16 cpuId)
{
    quint16 cpuId = m_processorContext->cpuId();
    quint64 pc = ctx->getProgramCounter();
    if (!readVirtualMemory(cpuId, addr, out, 8, pc))
    {
        return false; // fault captured by ctx
    }
    // Clear reservation for this CPU
    reservationAddr = INVALID_RESERVATION;
    return true;
}

void AlphaMemorySystem::broadcastMessage(memoryBarrierEmulationModeType type, quint16 cputId)
{

}
void AlphaMemorySystem::invalidateOverlappingReservations(quint64 physAddr, int size, quint16 excludeCpuId)
{
    // Note: Caller must hold m_reservationsLock

    for (auto it = m_reservations.begin(); it != m_reservations.end(); ++it)
    {
        if (it.key() != excludeCpuId && it.value().isValid)
        {
            if (it.value().matches(physAddr, size))
            {
                DEBUG_LOG(QString("Invalidating reservation for CPU%1 due to overlapping access").arg(it.key()));
                it.value().clear();

                // Send reservation clear message to the CPU
                CacheCoherencyMessage msg;
                msg.type = CacheCoherencyMessage::RESERVATION_CLEAR;
                msg.physicalAddress = physAddr;
                msg.sourceCpuId = excludeCpuId;
                msg.targetCpuId = it.key();
                msg.size = size;
                msg.timestamp = getCurrentTimestamp();

                sendMessageToCPU(it.key(), msg);
            }
        }
    }
}

// Populate TLB on every successful translation
// This maximizes hit rates for subsequent accesses
TranslationResult AlphaMemorySystem::translateInternal(quint16 cpuId, quint64 virtualAddr, int accessType,
                                                       bool isInstruction)
{
    // Input validation first - fail fast on invalid inputs
    AlphaCPU *cpu = getCPU(cpuId);
    if (!cpu)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Invalid CPU ID %1 during translation").arg(cpuId));
        return TranslationResult::createFault(excTLBException::INVALID_ENTRY);
    }

    // Validate virtual address
    if (!isValidVirtualAddress(virtualAddr))
    {
        ERROR_LOG(
            QString("AlphaMemorySystem: Invalid virtual address 0x%1 for CPU %2").arg(virtualAddr, 0, 16).arg(cpuId));
        asa_utils::safeIncrement(m_pageFaults);
        //m_pageFaults.fetchAndAddRelaxed(1);
        return TranslationResult::createFault(excTLBException::INVALID_ADDRESS);
    }

    // Validate access type
    if (accessType < 0 || accessType > 2)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Invalid access type %1 for translation").arg(accessType));
        return TranslationResult::createFault(excTLBException::INVALID_ENTRY);
    }

    // Safe counter increment
    asa_utils::safeIncrement(m_totalTranslations);
    //m_totalTranslations.fetchAndAddRelaxed(1);

    // Check MMU status safely
    bool mmuEnabled = false;
    try
    {
        mmuEnabled = cpu->isMMUEnabled();
    }
    catch (...)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Exception checking MMU status for CPU %1").arg(cpuId));
        return TranslationResult::createFault(excTLBException::INVALID_ENTRY);
    }

    if (!mmuEnabled)
    {
        // Direct physical addressing - validate the address is reasonable
        if (!isValidPhysicalAddress(virtualAddr))
        {
            ERROR_LOG(
                QString("AlphaMemorySystem: Invalid physical address 0x%1 in direct mode").arg(virtualAddr, 0, 16));
            return TranslationResult::createFault(excTLBException::INVALID_ADDRESS);
        }
        return TranslationResult::createSuccess(virtualAddr);
    }

    // Get ASN safely
    quint64 currentASN = 0;
    bool isKernel = false;
    try
    {
        currentASN = getCurrentASN();
        isKernel = isKernelMode();
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Exception getting CPU state during translation");
        return TranslationResult::createFault(excTLBException::INVALID_ENTRY);
    }

    // =====================================================
    // PHASE 1: SAFE TLB LOOKUP (Fast Path)
    // =====================================================

    if (m_tlbSystem)
    {
        try
        {
            if (m_tlbSystem->isCPURegistered(cpuId))
            {
                quint64 physicalAddr = m_tlbSystem->checkTB(cpuId, virtualAddr, currentASN, isKernel);

                if (physicalAddr != 0)
                {
                    // TLB HIT - validate the result before returning
                    if (!isValidPhysicalAddress(physicalAddr))
                    {
                        ERROR_LOG(QString("AlphaMemorySystem: TLB returned invalid physical address 0x%1")
                                      .arg(physicalAddr, 0, 16));
                        // Invalidate the corrupted TLB entry
                        m_tlbSystem->invalidateEntry(cpuId, virtualAddr, currentASN);
                        // Continue to page table walk
                    }
                    else
                    {
                        // Valid TLB hit
                        return TranslationResult::createSuccess(physicalAddr);
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            ERROR_LOG(QString("AlphaMemorySystem: Exception during TLB lookup: %1").arg(e.what()));
            // TLB may be corrupted - continue without it
        }
        catch (...)
        {
            ERROR_LOG("AlphaMemorySystem: Unknown exception during TLB lookup");
            // Continue to page table walk
        }
    }

    // =====================================================
    // PHASE 2: SAFE PAGE TABLE WALK (Slow Path)
    // =====================================================

    quint64 physicalAddr = 0;
    bool pageTableSuccess = false;

    try
    {
        pageTableSuccess = translateViaPageTable(virtualAddr, physicalAddr, true);

        if (pageTableSuccess)
        {
            // Validate page table result
            if (!isValidPhysicalAddress(physicalAddr))
            {
                ERROR_LOG(QString("AlphaMemorySystem: Page table returned invalid physical address 0x%1")
                              .arg(physicalAddr, 0, 16));
                pageTableSuccess = false;
            }
        }
    }
    catch (const std::exception &e)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Exception during page table walk: %1").arg(e.what()));
        pageTableSuccess = false;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Unknown exception during page table walk");
        pageTableSuccess = false;
    }

    if (pageTableSuccess)
    {
        // Safe TLB population
        if (!safeTLBPopulation(cpuId, virtualAddr, physicalAddr, currentASN, accessType, isInstruction))
        {
            WARN_LOG(QString("AlphaMemorySystem: Failed to populate TLB after successful page table walk for VA=0x%1")
                         .arg(virtualAddr, 0, 16));
            // Continue anyway - translation succeeded even if TLB population failed
        }

        return TranslationResult::createSuccess(physicalAddr);
    }

    // =====================================================
    // PHASE 3: SAFE MEMORY MAP FALLBACK (Legacy Path)
    // =====================================================

    try
    {
        QReadLocker locker(&m_memoryLock);
        auto it = m_memoryMap.lowerBound(virtualAddr);

        if (it != m_memoryMap.end() && virtualAddr >= it.key() && virtualAddr < (it.key() + it.value().size))
        {
            const MappingEntry &entry = it.value();

            // Validate mapping entry
            if (entry.size == 0)
            {
                ERROR_LOG(QString("AlphaMemorySystem: Zero-size memory mapping at 0x%1").arg(it.key(), 0, 16));
                asa_utils::safeIncrement(m_pageFaults);
                //m_pageFaults.fetchAndAddRelaxed(1);
                return TranslationResult::createFault(excTLBException::INVALID_ENTRY);
            }

            // Check permissions
            if ((accessType == 0 && !(entry.protectionFlags & 0x1)) || // Read
                (accessType == 1 && !(entry.protectionFlags & 0x2)) || // Write
                (accessType == 2 && !(entry.protectionFlags & 0x4)))
            { // Execute

               // m_protectionFaults.fetchAndAddRelaxed(1);
                asa_utils::safeIncrement(m_protectionFaults);
                return TranslationResult::createFault(excTLBException::PROTECTION_FAULT);
            }

            // Calculate physical address with overflow protection
            quint64 offset = virtualAddr - it.key();
            if (offset > entry.size)
            {
                ERROR_LOG(QString("AlphaMemorySystem: Virtual address offset %1 exceeds mapping size %2")
                              .arg(offset)
                              .arg(entry.size));
                //m_pageFaults.fetchAndAddRelaxed(1);
                asa_utils::safeIncrement(m_pageFaults);
                return TranslationResult::createFault(excTLBException::INVALID_ADDRESS);
            }

            physicalAddr = entry.physicalBase + offset;

            // Validate result
            if (!isValidPhysicalAddress(physicalAddr))
            {
                ERROR_LOG(QString("AlphaMemorySystem: Memory map produced invalid physical address 0x%1")
                              .arg(physicalAddr, 0, 16));
                //m_pageFaults.fetchAndAddRelaxed(1);
                asa_utils::safeIncrement(m_pageFaults);
                return TranslationResult::createFault(excTLBException::INVALID_ADDRESS);
            }

            // Safe TLB population for memory map result
            if (!safeTLBPopulationFromMemoryMap(cpuId, virtualAddr, physicalAddr, currentASN, entry, isInstruction))
            {
                WARN_LOG(QString("AlphaMemorySystem: Failed to populate TLB from memory map for VA=0x%1")
                             .arg(virtualAddr, 0, 16));
                // Continue anyway - translation succeeded
            }

            return TranslationResult::createSuccess(physicalAddr);
        }
    }
    catch (const std::exception &e)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Exception during memory map lookup: %1").arg(e.what()));
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Unknown exception during memory map lookup");
    }

    // =====================================================
    // PHASE 4: TRANSLATION FAILURE WITH CLEANUP
    // =====================================================

    // All translation methods failed
    //m_pageFaults.fetchAndAddRelaxed(1);
    asa_utils::safeIncrement(m_pageFaults);

    // Log detailed failure information for debugging
    DEBUG_LOG(QString("AlphaMemorySystem: Translation failed for CPU %1, VA=0x%2, ASN=%3, AccessType=%4")
                  .arg(cpuId)
                  .arg(virtualAddr, 0, 16)
                  .arg(currentASN)
                  .arg(accessType));

    // Check if we should trigger emergency cleanup due to repeated failures
    static QAtomicInt consecutiveFailures(0);
    
    asa_utils::safeIncrement(consecutiveFailures);
  //  = consecutiveFailures.fetchAndAddRelaxed(1);
    int failures = consecutiveFailures;

    if (failures > 100)
    { // Too many consecutive failures
        WARN_LOG(QString("AlphaMemorySystem: %1 consecutive translation failures, triggering emergency TLB cleanup")
                     .arg(failures));
        emergencyTLBCleanup(cpuId);
        consecutiveFailures.store(0); // Reset counter after cleanup
    }

    return TranslationResult::createFault(excTLBException::TLB_MISS);
}

bool AlphaMemorySystem::isValidVirtualAddress(quint64 virtualAddress) const
{
    // Alpha virtual address space validation for memory system

    // Check for canonical Alpha address format (43-bit virtual addresses)
    // Bits 63:43 must be sign extension of bit 42
    quint64 bit42 = (virtualAddress >> 42) & 0x1;
    quint64 highBits = (virtualAddress >> 43) & 0x1FFFFF; // Extract bits 63:43

    quint64 expectedHighBits = bit42 ? 0x1FFFFF : 0x0;
    if (highBits != expectedHighBits)
    {
        DEBUG_LOG("AlphaMemorySystem: Non-canonical virtual address 0x%016llX", virtualAddress);
        return false;
    }

    // Check against configured memory regions if available
    if (m_physicalMemorySize > 0)
    {
        // Basic physical memory bounds check
        quint64 physicalLimit = m_physicalMemoryBase + m_physicalMemorySize;
        if (virtualAddress >= m_physicalMemoryBase && virtualAddress < physicalLimit)
        {
            return true; // Within physical memory bounds
        }
    }

    // Check special Alpha address ranges

    // User space (0x0000000000000000 - 0x000003FFFFFFFFFF)
    if (virtualAddress <= 0x000003FFFFFFFFFFULL)
    {
        // Standard user space - basic validation
        // Process-specific limits would be enforced at higher levels
        return true;
    }

    // System space (0xFFFFFC0000000000 - 0xFFFFFFFFFFFFFFFF)
    if (virtualAddress >= 0xFFFFFC0000000000ULL)
    {
        // Kernel/system virtual address space

        // PAL code space (0xFFFFFFFF80000000 - 0xFFFFFFFFFFFFFFFF)
        if (virtualAddress >= 0xFFFFFFFF80000000ULL)
        {
            // PAL code region - always valid if in range
            return true;
        }

        // Kernel space (0xFFFFFC0000000000 - 0xFFFFFFFF7FFFFFFF)
        if (virtualAddress >= 0xFFFFFC0000000000ULL && virtualAddress <= 0xFFFFFFFF7FFFFFFFULL)
        {
            // Check against kernel memory allocation
            if (m_kernelMemoryBase > 0)
            {
                return (virtualAddress >= m_kernelMemoryBase &&
                        virtualAddress < (m_kernelMemoryBase + m_kernelMemorySize));
            }
            return true; // Default to valid if no specific limits set
        }

        return true; // Other system space addresses
    }

    // I/O space and special regions (0x0000040000000000 - 0xFFFFFBFFFFFFFFFF)
    if (virtualAddress >= 0x0000040000000000ULL && virtualAddress < 0xFFFFFC0000000000ULL)
    {
        // Check for memory-mapped device regions
        if (m_deviceManager)
        {
            return m_deviceManager->isValidDeviceAddress(virtualAddress);
        }

        // Default I/O space handling - basic range validation
        // Specific I/O regions would be validated by device managers
        return true;
    }

    // Check for basic alignment requirements
    if (m_enforceAlignment)
    {
        // Check 8-byte alignment for normal memory operations
        if ((virtualAddress & 0x7) != 0)
        {
            // Allow unaligned access but log warning for performance
            DEBUG_LOG("AlphaMemorySystem: Unaligned access at 0x%016llX may impact performance", virtualAddress);
        }
    }

    // Check against TLB validity if available
    if (m_tlbSystem)
    {
        // Quick TLB validity check - don't translate, just verify entry exists
        return m_tlbSystem->hasValidMapping(virtualAddress);
    }

    // Default case - if no specific validation failed, consider it potentially valid
    // The actual translation and access will be handled by the TLB system
    return true;
}
bool AlphaMemorySystem::isMMIOAddress(quint64 physicalAddr) const
{
    // Fast path: Use inline chipset-specific check first
    if ((physicalAddr >> 31) >= 0x4 && (physicalAddr >> 31) <= 0x7)
    {
        return true;
    }

    // Detailed path: Check with MMIO manager
    if (m_mmioManager)
    {
        return m_mmioManager->isMMIOAddress(physicalAddr);
    }

    return false;
}

bool AlphaMemorySystem::accessPhysicalMemory(quint64 physicalAddr, quint64 &value, int size, bool isWrite, quint64 pc, quint16 cpuId)
{
    if (isMMIOAddress(physicalAddr))
    {
        // MMIO access
        if (isWrite)
        {
            return m_mmioManager->writeMMIO(physicalAddr, value, size, pc);
        }
        else
        {
            value = m_mmioManager->readMMIO(physicalAddr, size, pc);
            return true;
        }
    }
    else
    {
        // Regular physical memory access
        if (!m_safeMemory)
        {
            return false;
        }

        try
        {
            if (isWrite)
            {
                switch (size)
                {
                case 1:
                    m_safeMemory->writeUInt8(physicalAddr, static_cast<quint8>(value), pc);
                    break;
                case 2:
                    m_safeMemory->writeUInt16(physicalAddr, static_cast<quint16>(value), pc);
                    break;
                case 4:
                    m_safeMemory->writeUInt32(physicalAddr, static_cast<quint32>(value), pc);
                    break;
                case 8:
                    m_safeMemory->writeUInt64(physicalAddr, value, pc);
                    break;
                default:
                    return false;
                }
            }
            else
            {
                switch (size)
                {
                case 1:
                    value = m_safeMemory->readUInt8(physicalAddr, pc);
                    break;
                case 2:
                    value = m_safeMemory->readUInt16(physicalAddr, pc);
                    break;
                case 4:
                    value = m_safeMemory->readUInt32(physicalAddr, pc);
                    break;
                case 8:
                    value = m_safeMemory->readUInt64(physicalAddr, pc);
                    break;
                default:
                    return false;
                }
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

// =========================
// LEGACY COMPATIBILITY METHODS
// =========================



bool AlphaMemorySystem::readVirtualMemory(quint16 cpuId, quint64 virtualAddr, quint64 &value, int size, quint64 pc)
{
    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
    {
        return false;
    }

    safeIncrement(m_totalMemoryAccesses, 1);
    // m_totalMemoryAccesses.fetchAndAddRelaxed(1);

    // Perform translation
    TranslationResult result = translateInternal(cpuId, virtualAddr, 0 /* read */, false /* data */);
    if (!result.isValid())
    {
        switch (result.getException())
        {
        case excTLBException::TLB_MISS:
            emit sigTranslationMiss(virtualAddr);
            break;
        case excTLBException::PROTECTION_FAULT:
            emit sigProtectionFault(virtualAddr, 0);
            break;
        default:
            emit sigTranslationMiss(virtualAddr);
            break;
        }
        value = 0xFFFFFFFFFFFFFFFFULL;
        return false;
    }

    quint64 physicalAddr = result.getPhysicalAddress();

    // Access physical memory
    bool success = accessPhysicalMemory(physicalAddr, value, size, false, pc, cpuId);

    if (success)
    {
        emit sigMemoryRead(virtualAddr, value, size);
    }

    return success;
}


bool AlphaMemorySystem::readVirtualMemory(quint16 cpuId, quint64 virtualAddr, void *value, quint16 size, quint64 pc)
{
    if (!value || size == 0)
    {
        return false;
    }

    AlphaCPU *cpu = validateAndGetCPU(cpuId);
    if (!cpu)
    {
        return false;
    }

    // For larger reads, read in chunks
    quint8 *buffer = static_cast<quint8 *>(value);
    for (quint16 offset = 0; offset < size; ++offset)
    {
        quint64 byteValue = 0;
        if (!readVirtualMemory(cpuId, virtualAddr + offset, byteValue, 1, pc))
        {
            return false;
        }
        buffer[offset] = static_cast<quint8>(byteValue);
    }

    return true;
}
bool AlphaMemorySystem::readVirtualMemory(quint64 virtualAddr, quint64 &value, int size, quint64 pc)
{
    // Legacy method - use CPU 0 as default
    return readVirtualMemory(0, virtualAddr, value, size, pc);
}

bool AlphaMemorySystem::readVirtualMemory(quint64 virtualAddr, void *value, quint16 size, quint64 pc)
{
    // Legacy method - use CPU 0 as default
    return readVirtualMemory(0, virtualAddr, value, size, pc);
}

bool AlphaMemorySystem::writeVirtualMemory(quint64 virtualAddr, quint64 value, int size, quint64 pc)
{
    // Legacy method - use CPU 0 as default
    return writeVirtualMemory(0, virtualAddr, value, size, pc);
}

bool AlphaMemorySystem::writeVirtualMemory(quint64 virtualAddr, void *value, int size, quint64 pc)
{
    // Legacy method - use CPU 0 as default
    return writeVirtualMemory(0, virtualAddr, value, size, pc);
}

bool AlphaMemorySystem::loadLocked(quint64 vaddr, quint64 &value, int size, quint64 pc)
{
    // Legacy method - use CPU 0 as default
    return loadLocked(0, vaddr, value, size, pc);
}

// =========================
// ADVANCED SMP FEATURES
// =========================

bool AlphaMemorySystem::readVirtualMemoryAtomic(quint16 cpuId, quint64 virtualAddr, void *value, int size, quint64 pc)
{
    if (!value || size == 0)
    {
        return false;
    }

    // For atomic operations, we need to ensure no other CPU can interfere
    QMutexLocker atomicLocker(&m_coherencyLock);

    // Perform the read
    bool success = readVirtualMemory(cpuId, virtualAddr, value, size, pc);

    if (success)
    {
        // Send cache coherency message to ensure other CPUs see this atomic access
        CacheCoherencyMessage msg;
        msg.type = CacheCoherencyMessage::INVALIDATE_LINE;
        msg.physicalAddress = virtualAddr; // Will be translated internally
        msg.sourceCpuId = cpuId;
        msg.targetCpuId = 0xFFFF; // Broadcast
        msg.size = size;
        msg.timestamp = getCurrentTimestamp();

        sendCacheCoherencyMessage(msg);
    }

    return success;
}

bool AlphaMemorySystem::writeVirtualMemoryConditional(quint16 cpuId, quint64 virtualAddr, void *value, int size,
                                                      quint64 expectedValue, quint64 pc)
{
    if (!value || size == 0)
    {
        return false;
    }

    QMutexLocker atomicLocker(&m_coherencyLock);

    // First read the current value
    quint64 currentValue = 0;
    if (!readVirtualMemory(cpuId, virtualAddr, currentValue, size, pc))
    {
        return false;
    }

    // Check if it matches expected value
    if (currentValue != expectedValue)
    {
        return false; // Conditional write failed
    }

    // Perform the conditional write
    return writeVirtualMemory(cpuId, virtualAddr, value, size, pc);
}

void AlphaMemorySystem::flushWriteBuffers(quint16 cpuId)
{
    // Flush write buffers for specific CPU using Qt atomic operations
    if (m_safeMemory)
    {
        // Use Qt atomic operations to ensure proper memory ordering
        QAtomicInt memoryOrdering;

        // Ensure all pending writes are committed with release semantics
        memoryOrdering.fetchAndStoreRelease(1);

        // Then ensure visibility with acquire semantics
        memoryOrdering.fetchAndAddAcquire(0);

        // Use ordered operation for strongest guarantee
        memoryOrdering.fetchAndStoreOrdered(0);
    }

    // Send flush message to other CPUs
    CacheCoherencyMessage msg;
    msg.type = CacheCoherencyMessage::FLUSH_LINE;
    msg.physicalAddress = 0; // All addresses
    msg.sourceCpuId = cpuId;
    msg.targetCpuId = 0xFFFF; // Broadcast
    msg.size = 0;             // All sizes
    msg.timestamp = getCurrentTimestamp();

    sendCacheCoherencyMessage(msg);

    DEBUG_LOG(QString("Write buffers flushed for CPU%1").arg(cpuId));
}

/**
 * @brief Execute Alpha MB (Memory Barrier) instruction
 * Alpha MB: Orders all memory operations (loads and stores)
 * This is the strongest Alpha memory barrier
 */
void AlphaMemorySystem::executeAlphaMB(quint16 cpuId)
{
    // Alpha MB requires full sequential consistency
    // Use strict barriers for hardware accuracy
    executeStrictMemoryBarrier(memoryBarrierEmulationModeType::FULL_BARRIER, cpuId); // Type 2 = full barrier

    DEBUG_LOG(QString("Alpha MB executed on CPU%1").arg(cpuId));
}

/**
 * @brief Execute Alpha WMB (Write Memory Barrier) instruction
 * Alpha WMB: Orders write operations only
 */
void AlphaMemorySystem::executeAlphaWMB(quint16 cpuId)
{
    // WMB only needs write ordering - Qt atomics sufficient
    executeMemoryBarrier(memoryBarrierEmulationModeType::WRITE_BARRIER, cpuId); // Type 1 = write barrier

    DEBUG_LOG(QString("Alpha WMB executed on CPU%1").arg(cpuId));
}

/**
 * @brief Execute implicit barriers for load-locked/store-conditional
 * Alpha LDx_L/STx_C have implicit acquire/release semantics
 */
void AlphaMemorySystem::executeLoadLockedBarrier(quint16 cpuId)
{
    // Load-locked needs acquire semantics - Qt atomics sufficient
    executeMemoryBarrier(memoryBarrierEmulationModeType::READ_BARRIER, cpuId); // Type 0 = read barrier
}

void AlphaMemorySystem::executeMemoryBarrier(memoryBarrierEmulationModeType type, quint16 cpuId)
{
    // Execute memory barrier using Qt atomic operations
    // Use a more comprehensive approach for memory ordering

    switch (type)
    {
    case memoryBarrierEmulationModeType::READ_BARRIER: // Read barrier - ensure all prior reads complete before subsequent reads
    {
        QAtomicInt readBarrier;
        // Force acquire semantics - all prior memory operations complete
        readBarrier.fetchAndAddAcquire(0);
    }
    break;

    case memoryBarrierEmulationModeType::WRITE_BARRIER: // Write barrier - ensure all prior writes complete before subsequent writes
    {
        QAtomicInt writeBarrier;
        // Force release semantics - all prior memory operations visible
        writeBarrier.fetchAndStoreRelease(0);
    }
    break;

    case memoryBarrierEmulationModeType::FULL_BARRIER: // Full barrier - complete ordering of all memory operations
    {
        QAtomicInt fullBarrier;
        // Combine acquire and release for full barrier effect
        fullBarrier.fetchAndAddAcquire(0);   // Acquire barrier
        fullBarrier.fetchAndStoreRelease(0); // Release barrier

        // Additional ordered operation for stronger guarantees
        fullBarrier.fetchAndAddOrdered(0);
    }
    break;

    default:
        // Unknown barrier type - default to full barrier
        {
            QAtomicInt defaultBarrier;
            defaultBarrier.fetchAndAddOrdered(0);
        }
        break;
    }

    // Flush write buffers after barrier
    flushWriteBuffers(cpuId);

    // Send barrier message to other CPUs
    CacheCoherencyMessage msg;
    msg.type = CacheCoherencyMessage::WRITE_BACK;
    msg.physicalAddress = type; // Use this field to encode barrier type
    msg.sourceCpuId = cpuId;
    msg.targetCpuId = 0xFFFF; // Broadcast
    msg.size = 0;
    msg.timestamp = getCurrentTimestamp();

    sendCacheCoherencyMessage(msg);

    DEBUG_LOG(QString("Memory barrier executed: CPU%1, type=%2").arg(cpuId).arg(type));
}

void AlphaMemorySystem::executeStoreConditionalBarrier(quint16 cpuId)
{
    // Store-conditional needs release semantics - Qt atomics sufficient
    executeMemoryBarrier(memoryBarrierEmulationModeType::WRITE_BARRIER, cpuId); // Type 1 = write barrier

    DEBUG_LOG(QString("Store-conditional barrier executed on CPU%1").arg(cpuId));
}

void AlphaMemorySystem::executeStrictMemoryBarrier(memoryBarrierEmulationModeType type, quint16 cpuId)
{

    // Try Qt atomics first for consistency
    executeMemoryBarrier(type, cpuId);

    // Add compiler barrier to prevent reordering
    asm volatile("" ::: "memory");

    #if defined(Q_PROCESSOR_X86_64)
    // x86-64 has strong ordering, but ensure no compiler reordering
    asm volatile("mfence" ::: "memory");
#elif defined(Q_PROCESSOR_ARM)
    // ARM needs explicit memory barriers
    asm volatile("dmb sy" ::: "memory");
#elif defined(Q_PROCESSOR_ALPHA)
    // If running on actual Alpha hardware (rare!)
    asm volatile("mb" ::: "memory");
#else
    // Fallback for other architectures
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif

    // If Qt barriers are insufficient for your use case, you can fall back to:
    // std::atomic_thread_fence(std::memory_order_seq_cst);
    // But document why this is necessary

    flushWriteBuffers(cpuId);

    // Send barrier message to other CPUs
    CacheCoherencyMessage msg;
    msg.type = CacheCoherencyMessage::WRITE_BACK;
    msg.physicalAddress = type;
    msg.sourceCpuId = cpuId;
    msg.targetCpuId = 0xFFFF;
    msg.size = 0;
    msg.timestamp = getCurrentTimestamp();

    sendCacheCoherencyMessage(msg);

    DEBUG_LOG(QString("Strict memory barrier executed: CPU%1, type=%2").arg(cpuId).arg(type));
}

/**
 * @brief Execute barriers for Alpha CALL_PAL instructions
 * PAL code transitions require strict ordering for hardware compatibility
 */
void AlphaMemorySystem::executePALBarrier(quint16 cpuId)
{
    // PAL transitions need hardware-accurate ordering
    executeStrictMemoryBarrier(memoryBarrierEmulationModeType::FULL_BARRIER, cpuId); // Full strict barrier

    DEBUG_LOG(QString("PAL barrier executed on CPU%1").arg(cpuId));
}

// =========================
// SLOTS IMPLEMENTATION
// =========================

void AlphaMemorySystem::onCacheCoherencyEvent(quint64 physicalAddr, quint16 sourceCpuId, const QString &eventType)
{
    DEBUG_LOG(QString("AlphaMemorySystem: Cache coherency event from CPU%1: %2 at 0x%3")
                  .arg(sourceCpuId)
                  .arg(eventType)
                  .arg(physicalAddr, 0, 16));

    // Handle the event based on type
    if (eventType == "INVALIDATE")
    {
        invalidateCacheLines(physicalAddr, 64, sourceCpuId); // Assume 64-byte cache lines
    }
    else if (eventType == "FLUSH")
    {
        flushCacheLines(physicalAddr, 64, sourceCpuId);
    }
}

void AlphaMemorySystem::onCPUStateChanged(quint16 cpuId, int newState)
{
    DEBUG_LOG(QString("AlphaMemorySystem: CPU%1 state changed to %2").arg(cpuId).arg(newState));

    // Handle CPU state changes that affect memory system
    // For example, if CPU goes offline, clear its reservations
    if (newState == 0)
    { // Assuming 0 means offline/halted
        clearCpuReservations(cpuId);
        setCPUOnlineStatus(cpuId, false);
    }
}

void AlphaMemorySystem::onMappingsCleared()
{
    DEBUG_LOG("AlphaMemorySystem: All memory mappings cleared - invalidating all TLBs");
    invalidateAllTLB(0xFFFF);
}

void AlphaMemorySystem::onMappingRangeCleared(quint64 startAddr, quint64 endAddr, quint64 asn)
{
    DEBUG_LOG(QString("AlphaMemorySystem: Mapping range cleared: 0x%1-0x%2, ASN=%3")
                  .arg(startAddr, 0, 16)
                  .arg(endAddr, 0, 16)
                  .arg(asn));

    // Invalidate affected TLB entries on all CPUs
    if (asn == 0)
    {
        invalidateAllTLB(0xFFFF);
    }
    else
    {
        invalidateTLBByASN(asn, 0xFFFF);
    }
}

void AlphaMemorySystem::onASNMappingsCleared(quint64 asn)
{
    DEBUG_LOG(QString("AlphaMemorySystem: ASN %1 mappings cleared").arg(asn));
    invalidateTLBByASN(asn, 0xFFFF);
}

excTLBException AlphaMemorySystem::determineTLBExceptionType(quint64 virtualAddress, quint64 asn)
{
    // Check various conditions to determine the specific TLB exception type

    // Check for invalid addresses first
    if (virtualAddress < 0x1000)
    {
        return excTLBException::INVALID_ADDRESS;
    }

    // Check if page table exists
    quint64 vptb = getVPTB(asn);
    if (vptb == 0)
    {
        return excTLBException::PAGE_FAULT;
    }

    // Try to read the page table entry
    quint64 vpn = virtualAddress >> 13;
    quint64 pteAddr = vptb + (vpn * 8);
    quint64 pte;

    if (!readPhysicalMemory(pteAddr, pte, 8))
    {
        return excTLBException::PAGE_FAULT;
    }

    // Check if PTE is valid
    if ((pte & 0x1) == 0)
    {
        return excTLBException::TRANSLATION_NOT_VALID;
    }

    // If we get here, it's likely a simple TLB miss on a valid page
    return excTLBException::TLB_MISS;
}

MemoryFaultType AlphaMemorySystem::determineMemoryFaultType(quint64 address, int size, bool isWrite)
{
    // Determine specific memory fault type based on your enumMemoryFaultType

    // Check for alignment faults
    if ((address & (size - 1)) != 0)
    {
        return MemoryFaultType::ALIGNMENT_FAULT; // Assuming this exists in your enum
    }

    // Check for protection violations
    if (isWrite && !isWritableAddress(address))
    {
        return MemoryFaultType::PROTECTION_VIOLATION; // Assuming this exists
    }

    // Check for privilege violations
    if (isKernelAddress(address) && !isKernelMode())
    {
        return MemoryFaultType::PRIVILEGE_VIOLATION; // Assuming this exists
    }

    // Default to access violation
    return MemoryFaultType::ACCESS_VIOLATION; // Assuming this exists in your enum
}

// =========================
// DEBUGGING AND DIAGNOSTICS
// =========================

void AlphaMemorySystem::dumpSystemState() const
{
    QReadLocker cpuLocker(&m_cpuRegistryLock);
    QReadLocker reservationLocker(&m_reservationsLock);

    DEBUG_LOG("=== AlphaMemorySystem State Dump ===");
    DEBUG_LOG(QString("Registered CPUs: %1").arg(m_cpuRegistry.size()));

    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        DEBUG_LOG(QString("  CPU%1: %2, %3")
                      .arg(it.key())
                      .arg(it->isActive ? "Active" : "Inactive")
                      .arg(it->isOnline ? "Online" : "Offline"));
    }

    DEBUG_LOG(QString("Active reservations: %1").arg(m_reservations.size()));
    for (auto it = m_reservations.begin(); it != m_reservations.end(); ++it)
    {
        if (it->isValid)
        {
            DEBUG_LOG(QString("  CPU%1: PA=0x%2, VA=0x%3, Size=%4")
                          .arg(it.key())
                          .arg(it->physicalAddress, 0, 16)
                          .arg(it->virtualAddress, 0, 16)
                          .arg(it->size));
        }
    }

    DEBUG_LOG(QString("Statistics:"));
    DEBUG_LOG(QString("  Total memory accesses: %1").arg(m_totalMemoryAccesses.loadRelaxed()));
    DEBUG_LOG(QString("  Cache coherency events: %1").arg(m_cacheCoherencyEvents.loadRelaxed()));
    DEBUG_LOG(QString("  TLB invalidations: %1").arg(m_tlbInvalidations.loadRelaxed()));
    DEBUG_LOG(QString("  Total translations: %1").arg(m_totalTranslations.loadRelaxed()));
    DEBUG_LOG(QString("  Page faults: %1").arg(m_pageFaults.loadRelaxed()));
    DEBUG_LOG(QString("  Protection faults: %1").arg(m_protectionFaults.loadRelaxed()));
}

QString AlphaMemorySystem::getSystemStatus() const
{
    QReadLocker locker(&m_cpuRegistryLock);

    QString status;
    status += QString("CPUs: %1 registered, %2 online\n").arg(m_cpuRegistry.size()).arg(getOnlineCPUCount());

    // Use Qt atomic load operations
    status += QString("Memory: %1 translations, %2 accesses\n")
                  .arg(m_totalTranslations.loadRelaxed())
                  .arg(m_totalMemoryAccesses.loadRelaxed());

    status += QString("Coherency: %1 events, %2 TLB invalidations\n")
                  .arg(m_cacheCoherencyEvents.loadRelaxed())
                  .arg(m_tlbInvalidations.loadRelaxed());

    status += QString("Faults: %1 page faults, %2 protection faults\n")
                  .arg(m_pageFaults.loadRelaxed())
                  .arg(m_protectionFaults.loadRelaxed());

    return status;
}



quint16 AlphaMemorySystem::getOnlineCPUCount() const
{
    // Note: Caller should hold m_cpuRegistryLock
    quint16 count = 0;
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it->isOnline)
        {
            count++;
        }
    }
    return count;
}

CPURegistryEntry *AlphaMemorySystem::findCPUEntry(quint16 cpuId)
{
    // Note: Caller must hold m_cpuRegistryLock for writing
    auto it = m_cpuRegistry.find(cpuId);
    return (it != m_cpuRegistry.end()) ? &it.value() : nullptr;
}

const CPURegistryEntry *AlphaMemorySystem::findCPUEntry(quint16 cpuId) const
{
    // Note: Caller must hold m_cpuRegistryLock for reading
    auto it = m_cpuRegistry.find(cpuId);
    return (it != m_cpuRegistry.end()) ? &it.value() : nullptr;
}

void AlphaMemorySystem::processPendingCoherencyMessages()
{
    QMutexLocker locker(&m_coherencyLock);

    while (!m_pendingCoherencyMessages.isEmpty())
    {
        CacheCoherencyMessage msg = m_pendingCoherencyMessages.dequeue();

        // Process the message based on its type
        switch (msg.type)
        {
        case CacheCoherencyMessage::INVALIDATE_LINE:
            // Broadcast invalidation to target CPUs
            if (msg.targetCpuId == 0xFFFF)
            {
                broadcastMessage(msg);
            }
            else
            {
                sendMessageToCPU(msg.targetCpuId, msg);
            }
            break;

        case CacheCoherencyMessage::FLUSH_LINE:
            // Broadcast flush to target CPUs
            if (msg.targetCpuId == 0xFFFF)
            {
                broadcastMessage(msg);
            }
            else
            {
                sendMessageToCPU(msg.targetCpuId, msg);
            }
            break;

        case CacheCoherencyMessage::WRITE_BACK:
            // Handle write-back operations
            if (msg.targetCpuId == 0xFFFF)
            {
                broadcastMessage(msg);
            }
            else
            {
                sendMessageToCPU(msg.targetCpuId, msg);
            }
            break;

        case CacheCoherencyMessage::RESERVATION_CLEAR:
            // Handle reservation clearing
            sendMessageToCPU(msg.targetCpuId, msg);
            break;

        default:
            WARN_LOG(QString("Unknown cache coherency message type: %1").arg(static_cast<int>(msg.type)));
            break;
        }
    }
}

bool AlphaMemorySystem::storeConditional(quint64 vaddr, quint64 value, int size, quint64 pc)
{
    // Legacy method - use CPU 0 as default
    return storeConditional(0, vaddr, value, size, pc);
}

// Helper functions for intelligent TLB entry population

/**
 * @brief Create TLB entry from successful page table translation
 * @param entry Output TLB entry to populate
 * @param virtualAddr Source virtual address
 * @param physicalAddr Translated physical address
 * @param asn Current Address Space Number
 * @param accessType Access type (0=read, 1=write, 2=execute)
 * @param isInstruction True if this was an instruction fetch
 */
void AlphaMemorySystem::populateTLBEntryFromTranslation(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr,
                                                        quint64 asn, int accessType, bool isInstruction)
{
    // Calculate page-aligned addresses
    quint64 pageSize = AlphaMemoryConstants::PAGE_SIZE_8KB; // Default Alpha page size
    quint64 virtualPageBase = (virtualAddr / pageSize) * pageSize;
    quint64 physicalPageBase = (physicalAddr / pageSize) * pageSize;

    // Set basic address information
    entry.setVirtualPage(virtualPageBase);
    entry.setPhysicalPage(physicalPageBase);
    entry.setAsn(asn);
    entry.setGranularity(0); // 0 = 8KB pages

    // Set entry as valid
    entry.setValid(true);
    entry.setReferenced(true); // Mark as recently accessed

    // Determine protection flags based on access pattern and Alpha memory regions
    quint32 protectionFlags = determineProtectionFlags(virtualAddr, accessType, isInstruction);
    entry.setProtection(protectionFlags);

    // Use Alpha-specific logic to determine if this is an instruction entry
    bool isInstructionEntry = determineInstructionEntry(virtualAddr, isInstruction, accessType);
    entry.setIsInstructionEntry(isInstructionEntry);

    // Set dirty bit if this was a write access
    if (accessType == 1)
    { // Write access
        entry.setDirty(true);
    }

    DEBUG_LOG(QString("Created TLB entry: VA=0x%1->PA=0x%2, ASN=%3, %4, Protection=0x%5")
                  .arg(virtualPageBase, 0, 16)
                  .arg(physicalPageBase, 0, 16)
                  .arg(asn)
                  .arg(isInstructionEntry ? "INSTRUCTION" : "DATA")
                  .arg(protectionFlags, 0, 16));
}


/**
 * @brief Create TLB entry from memory map translation
 * @param entry Output TLB entry to populate
 * @param virtualAddr Source virtual address
 * @param physicalAddr Translated physical address
 * @param asn Current Address Space Number
 * @param mapEntry Memory map entry with protection info
 * @param isInstruction True if this was an instruction fetch
 */
void AlphaMemorySystem::populateTLBEntryFromMemoryMap(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr,
                                                      quint64 asn, const MappingEntry &mapEntry, bool isInstruction)
{
    // Calculate page-aligned addresses
    quint64 pageSize = AlphaMemoryConstants::PAGE_SIZE_8KB;
    quint64 virtualPageBase = (virtualAddr / pageSize) * pageSize;
    quint64 physicalPageBase = (physicalAddr / pageSize) * pageSize;

    // Set basic address information
    entry.setVirtualPage(virtualPageBase);
    entry.setPhysicalPage(physicalPageBase);
    entry.setAsn(asn);
    entry.setGranularity(0); // 8KB pages

    // Set entry as valid
    entry.setValid(true);
    entry.setReferenced(true);

    // Use memory map protection flags directly
    entry.setProtection(mapEntry.protectionFlags);

    // Determine instruction entry based on memory map and access pattern
    bool isInstructionEntry = (mapEntry.protectionFlags & 0x4) && // Executable
                              (isInstruction || determineInstructionEntry(virtualAddr, isInstruction, 2));
    entry.setIsInstructionEntry(isInstructionEntry);

    DEBUG_LOG(QString("Created TLB entry from memory map: VA=0x%1->PA=0x%2, Protection=0x%3")
                  .arg(virtualPageBase, 0, 16)
                  .arg(physicalPageBase, 0, 16)
                  .arg(mapEntry.protectionFlags, 0, 16));
}


/**
 * @brief Determine protection flags based on Alpha memory layout and access patterns
 * @param virtualAddr Virtual address being accessed
 * @param accessType Access type (0=read, 1=write, 2=execute)
 * @param isInstruction True if this was an instruction fetch
 * @return Alpha TLB protection flags
 */
quint32 AlphaMemorySystem::determineProtectionFlags(quint64 virtualAddr, int accessType, bool isInstruction)
{
    quint32 flags = AlphaMemoryConstants::TLB_VALID; // Bit 0: Always valid

    // Note: Your constants don't have explicit TLB_READ - reads are allowed by default when valid
    // Only write and execute have explicit enable bits

    if (accessType == 1)
    {                                             // Write access
        flags |= AlphaMemoryConstants::TLB_WRITE; // Bit 1: Enable writes (inverse of FOW)
    }

    if (accessType == 2 || isInstruction)
    {                                            // Execute access or instruction fetch
        flags |= AlphaMemoryConstants::TLB_EXEC; // Bit 2: Execute enable
    }

    // Set privilege level based on virtual address
    if (isKernelAddress(virtualAddr))
    {
        flags |= AlphaMemoryConstants::TLB_KERNEL; // Bit 3: Kernel-mode only
    }
    else
    {
        flags |= AlphaMemoryConstants::TLB_USER; // Bit 4: User-mode allowed
    }

    // Check if this should be a global mapping (shared across address spaces)
    if (isGlobalMapping(virtualAddr))
    {
        flags |= AlphaMemoryConstants::TLB_GLOBAL; // Bit 5: Global across ASNs
    }

    return flags;
}

void AlphaMemorySystem::configureL3CacheHierarchy()
{
    if (!m_level3SharedCache)
    {
        return;
    }

    // Configure L3 as backing store for all CPU L2 caches
    QReadLocker locker(&m_cpuRegistryLock);
    for (auto it = m_cpuRegistry.begin(); it != m_cpuRegistry.end(); ++it)
    {
        if (it->cpu && it->isOnline)
        {
            // Get CPU's L2 cache and connect to L3
            // This requires CPU cache hierarchy to be established first
            DEBUG_LOG("AlphaMemorySystem: Configuring L3 hierarchy for CPU %1", it.key());
        }
    }
}

UnifiedDataCache *AlphaMemorySystem::createL3Cache(const UnifiedDataCache::Config &config)
{
    auto l3Cache = new UnifiedDataCache(config, this);
    attachL3Cache(l3Cache);
    return l3Cache;
}


/**
 * @brief Use Alpha-specific heuristics to determine if TLB entry is for instructions
 * @param virtualAddr Virtual address being accessed
 * @param isInstruction True if this was explicitly an instruction fetch
 * @param accessType Access type (0=read, 1=write, 2=execute)
 * @return True if this should be marked as an instruction entry
 */
bool AlphaMemorySystem::determineInstructionEntry(quint64 virtualAddr, bool isInstruction, int accessType)
{
    // If explicitly marked as instruction fetch, it's definitely an instruction entry
    if (isInstruction || accessType == 2)
    {
        return true;
    }

    // Use Alpha memory layout heuristics
#ifdef ALPHA_BUILD
    // Alpha-specific memory region analysis

    // Check if address is in known instruction regions
    if (virtualAddr < 0x10000000)
    {
        // Lower 256MB often contains main executable text
        return true;
    }

    // Check for shared library regions (Alpha-specific ranges)
    if ((virtualAddr >= 0x20000000 && virtualAddr < 0x80000000))
    {
        // Shared library region - could be instruction code
        return true;
    }

    // System space executable regions
    if (isKernelAddress(virtualAddr))
    {
        return true; // Kernel code is instruction space
    }

#else
    // VAX-specific memory region analysis

    // Extract top 2 bits to determine VAX memory space
    quint32 topBits = (virtualAddr >> 30) & 0x3;

    switch (topBits)
    {
    case 0:          // P0 space - program region
        return true; // P0 typically contains user program code

    case 1: // P1 space - control region
        // P1 usually stack/control, but executable pages are likely trampolines
        return true;

    case 2:          // S0 space - system space
        return true; // System space executable pages are definitely instructions

    case 3:           // S1 space - reserved
        return false; // S1 shouldn't have executable pages
    }
#endif

    // Default: if we can't determine, assume data entry for safety
    return false;
}

/**
 * @brief Safe protection flag determination with error checking
 * @param virtualAddr Virtual address
 * @param accessType Access type
 * @param isInstruction True if instruction fetch
 * @param protectionFlags Output protection flags
 * @return True if flags were safely determined
 */
bool AlphaMemorySystem::determineProtectionFlagsSafe(quint64 virtualAddr, int accessType, bool isInstruction,
                                                     quint32 &protectionFlags)
{
    try
    {
        protectionFlags = AlphaMemoryConstants::TLB_VALID;

        // Validate access type
        if (accessType < 0 || accessType > 2)
        {
            ERROR_LOG(QString("AlphaMemorySystem: Invalid access type %1").arg(accessType));
            return false;
        }

        // Set write permission for write accesses
        if (accessType == 1)
        {
            protectionFlags |= AlphaMemoryConstants::TLB_WRITE;
        }

        // Set execute permission for execute accesses or instruction fetches
        if (accessType == 2 || isInstruction)
        {
            protectionFlags |= AlphaMemoryConstants::TLB_EXEC;
        }

        // Determine privilege level safely
        bool isKernel = false;
        try
        {
            isKernel = isKernelAddress(virtualAddr);
        }
        catch (...)
        {
            WARN_LOG(
                QString("AlphaMemorySystem: Exception determining kernel address for 0x%1").arg(virtualAddr, 0, 16));
            // Default to user mode for safety
            isKernel = false;
        }

        if (isKernel)
        {
            protectionFlags |= AlphaMemoryConstants::TLB_KERNEL;
        }
        else
        {
            protectionFlags |= AlphaMemoryConstants::TLB_USER;
        }

        // Check for global mapping safely
        bool isGlobal = false;
        try
        {
            isGlobal = isGlobalMapping(virtualAddr);
        }
        catch (...)
        {
            WARN_LOG(
                QString("AlphaMemorySystem: Exception determining global mapping for 0x%1").arg(virtualAddr, 0, 16));
            isGlobal = false; // Default to non-global for safety
        }

        if (isGlobal)
        {
            protectionFlags |= AlphaMemoryConstants::TLB_GLOBAL;
        }

        return true;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Exception in safe protection flag determination");
        return false;
    }
}

/**
 * @brief Safe instruction entry determination with fallback
 * @param virtualAddr Virtual address
 * @param isInstruction True if instruction fetch
 * @param accessType Access type
 * @return True if this should be an instruction entry (safe fallback)
 */
bool AlphaMemorySystem::determineInstructionEntrySafe(quint64 virtualAddr, bool isInstruction, int accessType)
{
    try
    {
        // If explicitly marked as instruction, trust it
        if (isInstruction || accessType == 2)
        {
            return true;
        }

        // Use safe memory region analysis
        return determineInstructionEntry(virtualAddr, isInstruction, accessType);
    }
    catch (...)
    {
        WARN_LOG(QString("AlphaMemorySystem: Exception in instruction entry determination for 0x%1")
                     .arg(virtualAddr, 0, 16));
        // Safe fallback: assume data entry to prevent instruction cache pollution
        return false;
    }
}


/**
 * @brief Enhanced TLB invalidation with performance monitoring
 * @param virtualAddr Virtual address to invalidate (0 = all addresses)
 * @param asn Address Space Number (0 = all ASNs)
 * @param sourceCpuId Source CPU
 * @param invalidationType Type of invalidation for statistics
 */
void AlphaMemorySystem::invalidateTLBWithMonitoring(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId,
                                                    const QString &invalidationType)
{
    if (!m_tlbSystem)
    {
        WARN_LOG("AlphaMemorySystem: TLB invalidation requested but no TLB system available");
        return;
    }

    // Measure invalidation performance
    QElapsedTimer timer;
    timer.start();

    try
    {
        // Get pre-invalidation statistics for monitoring
        QHash<quint16, TLBSystem::TLBStats> statsBefore;
        QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();

        for (quint16 cpuId : registeredCPUs)
        {
            if (m_tlbSystem->isCPURegistered(cpuId))
            {
                statsBefore[cpuId] = m_tlbSystem->getTLBStats(cpuId);
            }
        }

        // Perform the appropriate invalidation
        if (virtualAddr == 0 && asn == 0)
        {
            // Global flush
            invalidateAllTLB(sourceCpuId);
        }
        else if (virtualAddr == 0)
        {
            // ASN flush
            invalidateTLBByASN(asn, sourceCpuId);
        }
        else
        {
            // Single address invalidation
            invalidateTlbSingle(virtualAddr, sourceCpuId);
        }

        // Measure and log performance
        qint64 elapsedMicroseconds = timer.nsecsElapsed() / 1000;

        // Calculate invalidation impact
        int totalInvalidated = 0;
        for (quint16 cpuId : registeredCPUs)
        {
            if (m_tlbSystem->isCPURegistered(cpuId) && statsBefore.contains(cpuId))
            {
                TLBSystem::TLBStats statsAfter = m_tlbSystem->getTLBStats(cpuId);
                totalInvalidated += statsBefore[cpuId].validEntries - statsAfter.validEntries;
            }
        }

        DEBUG_LOG(QString("TLB invalidation performance: %1, %2 entries, %3?s, %4 CPUs")
                      .arg(invalidationType)
                      .arg(totalInvalidated)
                      .arg(elapsedMicroseconds)
                      .arg(registeredCPUs.size()));

        // Emit performance signal for monitoring
        emit sigTLBInvalidationPerformance(invalidationType, totalInvalidated, elapsedMicroseconds,
                                           registeredCPUs.size());
    }
    catch (const std::exception &e)
    {
        ERROR_LOG(QString("Exception during monitored TLB invalidation: %1").arg(e.what()));
        handleTLBError(sourceCpuId, QString("Monitored invalidation failed: %1").arg(e.what()));
    }
    catch (...)
    {
        ERROR_LOG("Unknown exception during monitored TLB invalidation");
        handleTLBError(sourceCpuId, "Monitored invalidation unknown error");
    }
}

/**
 * @brief Check if virtual address should have global TLB mapping
 * @param virtualAddr Virtual address to check
 * @return True if this should be a global mapping
 */
bool AlphaMemorySystem::isGlobalMapping(quint64 virtualAddr)
{
    // Kernel space is typically global
    if (isKernelAddress(virtualAddr))
    {
        return true;
    }

    // Shared library regions might be global
    if (virtualAddr >= 0x20000000 && virtualAddr < 0x80000000)
    {
        return true; // Shared libraries often global
    }

    // Most user space is not global
    return false;
}

// Memory Safety

/**
 * @brief Safe TLB entry population with comprehensive error checking
 * @param cpuId CPU identifier
 * @param virtualAddr Virtual address
 * @param physicalAddr Physical address
 * @param asn Address Space Number
 * @param accessType Access type (0=read, 1=write, 2=execute)
 * @param isInstruction True if instruction fetch
 * @return True if TLB entry was safely created and inserted
 */
bool AlphaMemorySystem::safeTLBPopulation(quint16 cpuId, quint64 virtualAddr, quint64 physicalAddr, quint64 asn,
                                          int accessType, bool isInstruction)
{
    // Validate input parameters first
    if (!m_tlbSystem)
    {
        ERROR_LOG("AlphaMemorySystem: TLB system not initialized during population attempt");
        return false;
    }

    if (!m_tlbSystem->isCPURegistered(cpuId))
    {
        ERROR_LOG(QString("AlphaMemorySystem: CPU %1 not registered with TLB during population").arg(cpuId));
        return false;
    }

    // Validate address ranges
    if (!isValidVirtualAddress(virtualAddr))
    {
        WARN_LOG(QString("AlphaMemorySystem: Invalid virtual address 0x%1 for TLB population").arg(virtualAddr, 0, 16));
        return false;
    }

    if (!isValidPhysicalAddress(physicalAddr))
    {
        WARN_LOG(
            QString("AlphaMemorySystem: Invalid physical address 0x%1 for TLB population").arg(physicalAddr, 0, 16));
        return false;
    }

    // Create TLB entry with exception safety
    try
    {
        TLBEntry newEntry;

        // Use safe helper that won't throw exceptions
        if (!populateTLBEntrySafe(newEntry, virtualAddr, physicalAddr, asn, accessType, isInstruction))
        {
            WARN_LOG(QString("AlphaMemorySystem: Failed to create TLB entry for VA=0x%1").arg(virtualAddr, 0, 16));
            return false;
        }

        // Validate entry before insertion
        if (!validateTLBEntry(newEntry, cpuId))
        {
            ERROR_LOG(QString("AlphaMemorySystem: TLB entry validation failed for CPU %1, VA=0x%2")
                          .arg(cpuId)
                          .arg(virtualAddr, 0, 16));
            return false;
        }

        // Safe insertion with error checking
        m_tlbSystem->insertTLBEntry(cpuId, newEntry);

        DEBUG_LOG(QString("AlphaMemorySystem: Safely populated TLB for CPU %1, VA=0x%2->PA=0x%3")
                      .arg(cpuId)
                      .arg(virtualAddr, 0, 16)
                      .arg(physicalAddr, 0, 16));

        return true;
    }
    catch (const std::exception &e)
    {
        ERROR_LOG(QString("AlphaMemorySystem: Exception during TLB population: %1").arg(e.what()));
        return false;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Unknown exception during TLB population");
        return false;
    }
}


/**
 * @brief Safe TLB population from memory map with validation
 * @param cpuId CPU identifier
 * @param virtualAddr Virtual address
 * @param physicalAddr Physical address
 * @param asn Address Space Number
 * @param mapEntry Memory map entry
 * @param isInstruction True if instruction fetch
 * @return True if successfully populated
 */
bool AlphaMemorySystem::safeTLBPopulationFromMemoryMap(quint16 cpuId, quint64 virtualAddr, quint64 physicalAddr,
                                                       quint64 asn, const MappingEntry &mapEntry, bool isInstruction)
{
    try
    {
        if (!m_tlbSystem || !m_tlbSystem->isCPURegistered(cpuId))
        {
            return false;
        }

        TLBEntry newEntry;
        if (!populateTLBEntryFromMemoryMapSafe(newEntry, virtualAddr, physicalAddr, asn, mapEntry, isInstruction))
        {
            return false;
        }

        if (!validateTLBEntry(newEntry, cpuId))
        {
            return false;
        }

        m_tlbSystem->insertTLBEntry(cpuId, newEntry);
        return true;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Exception in safe TLB population from memory map");
        return false;
    }
}

/**
 * @brief Exception-safe memory map TLB entry creation
 * @param entry Output TLB entry
 * @param virtualAddr Virtual address
 * @param physicalAddr Physical address
 * @param asn Address Space Number
 * @param mapEntry Memory map entry
 * @param isInstruction True if instruction fetch
 * @return True if entry was safely created
 */
bool AlphaMemorySystem::populateTLBEntryFromMemoryMapSafe(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr,
                                                          quint64 asn, const MappingEntry &mapEntry, bool isInstruction)
{
    try
    {
        // Validate memory map entry
        if (mapEntry.size == 0)
        {
            ERROR_LOG("AlphaMemorySystem: Cannot create TLB entry from zero-size memory mapping");
            return false;
        }

        // Calculate page-aligned addresses with overflow protection
        quint64 pageSize = AlphaMemoryConstants::PAGE_SIZE_8KB;

        if (virtualAddr > (UINT64_MAX - pageSize) || physicalAddr > (UINT64_MAX - pageSize))
        {
            ERROR_LOG("AlphaMemorySystem: Address overflow in memory map TLB entry creation");
            return false;
        }

        quint64 virtualPageBase = (virtualAddr / pageSize) * pageSize;
        quint64 physicalPageBase = (physicalAddr / pageSize) * pageSize;

        // Set basic address information
        entry.setVirtualPage(virtualPageBase);
        entry.setPhysicalPage(physicalPageBase);
        entry.setAsn(asn);
        entry.setGranularity(0); // 8KB pages

        // Set entry as valid
        entry.setValid(true);
        entry.setReferenced(true);

        // Validate and use memory map protection flags
        if (!isValidProtectionFlags(mapEntry.protectionFlags))
        {
            WARN_LOG(QString("AlphaMemorySystem: Invalid protection flags 0x%1 in memory map, using safe defaults")
                         .arg(mapEntry.protectionFlags, 0, 16));

            // Use safe default protection
            quint32 safeFlags = AlphaMemoryConstants::TLB_VALID | AlphaMemoryConstants::TLB_USER;
            entry.setProtection(safeFlags);
        }
        else
        {
            entry.setProtection(mapEntry.protectionFlags);
        }

        // Determine instruction entry safely
        bool isInstructionEntry = false;
        try
        {
            isInstructionEntry = (mapEntry.protectionFlags & 0x4) && // Executable
                                 (isInstruction || determineInstructionEntrySafe(virtualAddr, isInstruction, 2));
        }
        catch (...)
        {
            WARN_LOG("AlphaMemorySystem: Exception determining instruction entry, defaulting to data");
            isInstructionEntry = false; // Safe default
        }
        entry.setIsInstructionEntry(isInstructionEntry);

        return true;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Exception in safe memory map TLB entry creation");
        return false;
    }
}


/**
 * @brief Exception-safe TLB entry creation
 * @param entry Output TLB entry
 * @param virtualAddr Virtual address
 * @param physicalAddr Physical address
 * @param asn Address Space Number
 * @param accessType Access type
 * @param isInstruction True if instruction fetch
 * @return True if entry was safely created
 */
bool AlphaMemorySystem::populateTLBEntrySafe(TLBEntry &entry, quint64 virtualAddr, quint64 physicalAddr, quint64 asn,
                                             int accessType, bool isInstruction)
{
    try
    {
        // Calculate page-aligned addresses with overflow protection
        quint64 pageSize = AlphaMemoryConstants::PAGE_SIZE_8KB;

        if (virtualAddr > (UINT64_MAX - pageSize))
        {
            ERROR_LOG("AlphaMemorySystem: Virtual address overflow during page alignment");
            return false;
        }

        if (physicalAddr > (UINT64_MAX - pageSize))
        {
            ERROR_LOG("AlphaMemorySystem: Physical address overflow during page alignment");
            return false;
        }

        quint64 virtualPageBase = (virtualAddr / pageSize) * pageSize;
        quint64 physicalPageBase = (physicalAddr / pageSize) * pageSize;

        // Set basic address information with bounds checking
        entry.setVirtualPage(virtualPageBase);
        entry.setPhysicalPage(physicalPageBase);
        entry.setAsn(asn);
        entry.setGranularity(0); // 8KB pages

        // Set entry as valid
        entry.setValid(true);
        entry.setReferenced(true);

        // Determine protection flags safely
        quint32 protectionFlags = 0;
        if (!determineProtectionFlagsSafe(virtualAddr, accessType, isInstruction, protectionFlags))
        {
            ERROR_LOG("AlphaMemorySystem: Failed to determine protection flags safely");
            return false;
        }
        entry.setProtection(protectionFlags);

        // Determine instruction entry classification safely
        bool isInstructionEntry = determineInstructionEntrySafe(virtualAddr, isInstruction, accessType);
        entry.setIsInstructionEntry(isInstructionEntry);

        // Set dirty bit for write accesses
        if (accessType == 1)
        {
            entry.setDirty(true);
        }

        return true;
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Exception in safe TLB entry population");
        return false;
    }
}


/**
 * @brief Validate physical address ranges
 * @param physicalAddr Physical address to validate
 * @return True if address is in valid range
 */
bool AlphaMemorySystem::isValidPhysicalAddress(quint64 physicalAddr) const
{
    // Check for null physical address
    if (physicalAddr == 0)
    {
        return false; // Physical address 0 is typically reserved
    }

    // Check against maximum physical memory (system dependent)
    // For Alpha systems, typically 44-bit physical addresses
    const quint64 MAX_PHYSICAL_ADDRESS = 0xFFFFFFFFFFFULL; // 44-bit physical space

    if (physicalAddr > MAX_PHYSICAL_ADDRESS)
    {
        return false;
    }

    // Check if address is in MMIO range (these are valid but special)
    if (isMMIOAddress(physicalAddr))
    {
        return true; // MMIO addresses are valid physical addresses
    }

    return true;
}


/**
 * @brief Validate TLB protection flag combinations
 * @param protectionFlags Protection flags to validate
 * @return True if flag combination is valid
 */
bool AlphaMemorySystem::isValidProtectionFlags(quint32 protectionFlags) const
{
    // Must have valid bit set
    if (!(protectionFlags & AlphaMemoryConstants::TLB_VALID))
    {
        return false;
    }

    // Cannot have both kernel and user bits set
    if ((protectionFlags & AlphaMemoryConstants::TLB_KERNEL) && (protectionFlags & AlphaMemoryConstants::TLB_USER))
    {
        return false; // Conflicting privilege levels
    }

    // Must have at least one privilege bit set
    if (!(protectionFlags & AlphaMemoryConstants::TLB_KERNEL) && !(protectionFlags & AlphaMemoryConstants::TLB_USER))
    {
        return false; // No privilege level specified
    }

    // Check for reserved bits (assuming bits 6-31 are reserved)
    const quint32 RESERVED_BITS =
        ~(AlphaMemoryConstants::TLB_VALID | AlphaMemoryConstants::TLB_WRITE | AlphaMemoryConstants::TLB_EXEC |
          AlphaMemoryConstants::TLB_KERNEL | AlphaMemoryConstants::TLB_USER | AlphaMemoryConstants::TLB_GLOBAL);

    if (protectionFlags & RESERVED_BITS)
    {
        return false; // Reserved bits are set
    }

    return true;
}




/**
 * @brief Emergency TLB cleanup for error recovery
 * @param cpuId CPU to clean up (0xFFFF = all CPUs)
 */
void AlphaMemorySystem::emergencyTLBCleanup(quint16 cpuId)
{
    try
    {
        if (!m_tlbSystem)
        {
            ERROR_LOG("AlphaMemorySystem: No TLB system during emergency cleanup");
            return;
        }

        if (cpuId == 0xFFFF)
        {
            // Clean up all CPUs
            QVector<quint16> registeredCPUs = m_tlbSystem->getRegisteredCPUs();
            for (quint16 id : registeredCPUs)
            {
                m_tlbSystem->invalidateAll(id);
                WARN_LOG(QString("AlphaMemorySystem: Emergency TLB cleanup for CPU %1").arg(id));
            }
        }
        else
        {
            // Clean up specific CPU
            if (m_tlbSystem->isCPURegistered(cpuId))
            {
                m_tlbSystem->invalidateAll(cpuId);
                WARN_LOG(QString("AlphaMemorySystem: Emergency TLB cleanup for CPU %1").arg(cpuId));
            }
        }

        // Clear translation cache as well
        if (m_translationCache)
        {
            m_translationCache->invalidateAll();
        }
    }
    catch (...)
    {
        ERROR_LOG("AlphaMemorySystem: Exception during emergency TLB cleanup");
        // Last resort: try to reset TLB system
        if (m_tlbSystem)
        {
            try
            {
                // This is a last-ditch effort - may not be safe but better than corruption
                delete m_tlbSystem;
                m_tlbSystem = new TLBSystem(128, 16); // Recreate with default settings
                ERROR_LOG("AlphaMemorySystem: TLB system recreated during emergency cleanup");
            }
            catch (...)
            {
                ERROR_LOG("AlphaMemorySystem: Failed to recreate TLB system during emergency cleanup");
                m_tlbSystem = nullptr; // Give up on TLB
            }
        }
    }
}

/**
 * @brief Global error recovery function for TLB-related issues
 * @param cpuId CPU that experienced the error (0xFFFF = all CPUs)
 * @param errorType Type of error encountered
 */
void AlphaMemorySystem::handleTLBError(quint16 cpuId, const QString &errorType)
{
    ERROR_LOG(QString("AlphaMemorySystem: TLB error on CPU %1: %2").arg(cpuId).arg(errorType));

    // Increment error counter for monitoring
    static QAtomicInt tlbErrorCount(0);
    asa_utils::safeIncrement(tlbErrorCount); 
    int errorCount = tlbErrorCount; // tlbErrorCount.fetchAndAddRelaxed(1);

    // Take progressively more drastic action based on error frequency
    if (errorCount < 10)
    {
        // Low error count - just invalidate problematic entries
        if (cpuId != 0xFFFF && m_tlbSystem && m_tlbSystem->isCPURegistered(cpuId))
        {
            m_tlbSystem->invalidateAll(cpuId);
            DEBUG_LOG(QString("AlphaMemorySystem: Invalidated TLB for CPU %1 due to error").arg(cpuId));
        }
    }
    else if (errorCount < 50)
    {
        // Moderate error count - broader cleanup
        WARN_LOG(QString("AlphaMemorySystem: %1 TLB errors, performing broader cleanup").arg(errorCount));
        emergencyTLBCleanup(cpuId);
    }
    else
    {
        // High error count - system may be unstable
        ERROR_LOG(QString("AlphaMemorySystem: %1 TLB errors indicates system instability").arg(errorCount));
        emergencyTLBCleanup(0xFFFF); // Clean up all CPUs

        // Emit signal for higher-level error handling
        emit sigTLBSystemError(errorCount, errorType);

        // Reset counter to prevent spam
        if (errorCount > 100)
        {
            tlbErrorCount.store(0);
        }
    }
}