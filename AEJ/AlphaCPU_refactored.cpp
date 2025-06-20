#include "AlphaCPU_refactored.h"
#include "GlobalMacro.h"
#include "enumerations/enumIPRNumbers.h"
#include "UnifiedDataCache.h"
#include "enumerations/enumExceptionType.h"
#include "enumerations/enumExceptionArithmetic.h"
#include "traps/trapFpType.h"
#include "enumerations/enumCpuModel.h"
#include "enumerations/enumFPCompare.h"
#include "enumerations/enumRegisterType.h"
#include "enumerations/enumRoundingMode.h"
#include "utilitySafeIncrement.h"
#include "enumerations/enumSecurityViolationType.h"
#include <cstring>
#include "AlphaMemorySystem_refactored.h"

AlphaCPU::AlphaCPU(quint16 cpuID, AlphaMemorySystem *memorySystem, QObject *parent)
    : QObject(parent), m_cpuId(cpuID), m_memorySystem(memorySystem), m_iprs(new IprBank(this)), m_pc(0),
      m_hasException(false), m_currentMode(enumProcessorMode::USER), m_palCodeBase(0)
{
    // Don't initialize TLB here - it's handled by memory system

    // Initialize CPU and SMP features
    initializeCpu();
    initializeSMP();

    // Initialize cache instances
    m_level1DataCache.reset(new UnifiedDataCache(nullptr));
    m_level2DataCache.reset(new UnifiedDataCache(nullptr));
    m_instructionCache.reset(new AlphaInstructionCache(nullptr));

    m_processorContext->reset(new AlphaProcessorContext(nullptr));

    // Register with memory system - let it handle TLB setup
    if (m_memorySystem)
    {
        m_memorySystem->registerCPU(this, m_cpuId);
    }

    // Set CPU reference in IPR bank
    if (m_iprs)
    {
        m_iprs->setCpu(this);
    }

    DEBUG_LOG(QString("AlphaCPU: Created CPU%1 with SMP support").arg(m_cpuId));
}


 AlphaCPU::~AlphaCPU()
{
    if (m_memorySystem)
    {
        m_memorySystem->unregisterCPU(m_cpuId);
    }

    DEBUG_LOG(QString("AlphaCPU: Destroyed CPU%1").arg(m_cpuId));
}

bool AlphaCPU::readMemory64(quint64 vaddr, quint64 &val, quint64 pc)
{
    if (!m_memorySystem)
        return false;

    // Try L1 cache first
    if (m_level1DataCache && m_level1DataCache->read(vaddr, &val, sizeof(val)))
    {
        DEBUG_LOG("AlphaCPU: L1 cache hit on CPU%1 for addr=0x%2", m_cpuId, QString::number(vaddr, 16));
        return true;
    }

    // Cache miss - use memory system with TLB lookup and caching
    bool success = m_memorySystem->readVirtualMemory(m_cpuId, vaddr, val, 8, pc);

    // Populate cache on successful memory read
    if (success && m_level1DataCache)
    {
        m_level1DataCache->write(vaddr, &val, sizeof(val));
        DEBUG_LOG("AlphaCPU: Populated L1 cache on CPU%1 for addr=0x%2", m_cpuId, QString::number(vaddr, 16));
    }

    return success;
}

bool AlphaCPU::readMemory64Locked(quint64 vaddr, quint64 &val, quint64 pc)
{
    if (!m_memorySystem)
        return false;

    return m_memorySystem->loadLocked(m_cpuId, vaddr, val, 8, pc);
}

bool AlphaCPU::readMemoryWithFaultHandling(quint64 address, quint64 &value, const PALInstruction &instr)
{
    if (!m_memorySystem)
        return false;

    try
    {
        // Attempt to read memory through the memory system
        bool success = m_memorySystem->readVirtualMemory(m_cpuId, address, value, 8, m_pc);

        if (!success)
        {
            // Check what type of fault occurred
            quint64 physAddr;
            bool translationSuccess =
                m_memorySystem->translateAddress(m_cpuId, address, physAddr, getCurrentASN(), false, false);

            if (!translationSuccess)
            {
                // Translation fault
                raiseMemoryException(address, false, true, false);
            }
            else
            {
                // Access violation or other fault
                raiseMemoryException(address, false, false, false);
            }
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        // Handle any exceptions that might occur during memory access
        handleMemoryFault(address, false);
        return false;
    }
}

bool AlphaCPU::writeMemory32Conditional(quint64 vaddr, quint32 value, quint64 pc)
{
    quint64 val64 = value;
    return m_memorySystem->storeConditional(m_cpuId, vaddr, val64, 4, pc);
}

bool AlphaCPU::writeMemory32(quint64 vaddr, quint32 value, quint64 pc)
{
    return m_memorySystem->writeVirtualMemory(m_cpuId, vaddr, static_cast<quint64>(value), 4, pc);
}

bool AlphaCPU::writeMemory64(quint64 vaddr, quint64 value, quint64 pc)
{
    if (!m_memorySystem)
        return false;

    // Write to cache first (write-allocate policy)
    if (m_level1DataCache)
    {
        m_level1DataCache->write(vaddr, &value, sizeof(value));
    }

    // Write through to memory system
    bool success = m_memorySystem->writeVirtualMemory(m_cpuId, vaddr, value, 8, pc);

    if (!success && m_level1DataCache)
    {
        // If memory write failed, invalidate cache entry
        m_level1DataCache->invalidateLine(vaddr);
        DEBUG_LOG("AlphaCPU: Invalidated L1 cache entry due to write failure on CPU%1", m_cpuId);
    }

    return success;
}


bool AlphaCPU::writeMemory64Conditional(quint64 vaddr, quint64 value, quint64 pc)
{
    if (!m_memorySystem)
        return false;

    return m_memorySystem->storeConditional(m_cpuId, vaddr, value, 8, pc);
}
void AlphaCPU::handleMemoryFault(quint64 address, bool isWrite)
{
    quint64 excSum = 0;

    // Use your EXC_SUM constants
    excSum |= EXC_SUM_ACCESS_VIOLATION;

    if (!isWrite)
    {
        excSum |= EXC_SUM_FAULT_ON_READ;
    }

    // Check for translation fault through memory system
    bool tlbMiss = false;
    if (m_memorySystem)
    {
        quint64 physAddr;
        tlbMiss = !m_memorySystem->translateAddress(m_cpuId, address, physAddr, getCurrentASN(), isWrite, false);
    }

    if (tlbMiss)
    {
        excSum |= EXC_SUM_TRANS_NOT_VALID;
    }

    // Check for alignment fault
    if (address & 0x7)
    {
        excSum |= EXC_SUM_ALIGNMENT_FAULT;
    }

    // Store in exception registers
    m_iprs->write(IPRNumbers::IPR_EXC_SUM, excSum);
    m_iprs->write(IPRNumbers::IPR_EXC_ADDR, address);
    m_iprs->write(IPRNumbers::IPR_EXC_PC, m_pc);

    // Raise exception
    raiseException(0x0004, m_pc); // Memory management exception
}



void AlphaCPU::handleIPI(int vector, quint16 sourceCpuId)
{
    DEBUG_LOG(QString("CPU%1: Received IPI vector %2 from CPU%3").arg(m_cpuId).arg(vector).arg(sourceCpuId));

    // Set the appropriate bit in the IPIR register
    quint64 ipirValue = m_iprs->read(IPRNumbers::IPR_IPIR);
    ipirValue |= (1ULL << vector);
    m_iprs->write(IPRNumbers::IPR_IPIR, ipirValue);

    // Record this pending interrupt
    pendingInterrupts.insert(vector);
    interruptPending.storeRelaxed(true);

    // Check if we can handle it immediately using your constants
    if (m_isRunning && (m_processorStatus & PS_INTERRUPT_ENABLE) && !m_inExceptionHandler)
    {
        if (canTakeInterrupt(vector))
        {
            deliverPendingInterrupt();
        }
    }
}



void AlphaCPU::sendIPI(quint16 targetCpuId, int vector)
{
    if (m_memorySystem)
    {
        AlphaCPU *targetCpu = m_memorySystem->getCPU(targetCpuId);
        if (targetCpu)
        {
            targetCpu->handleIPI(vector, m_cpuId);
            emit sigIPISent(m_cpuId, targetCpuId, vector);
        }
    }
}

void AlphaCPU::setFloatRegister(quint8 regnum, quint64 unintValue)
{
    if (regnum >= 32)
    {
        DEBUG_LOG(QString("CPU%1: Invalid float register number: %2").arg(m_cpuId).arg(regnum));
        return;
    }

    ensureComponentsInitialized();

    if (m_registers)
    {
        m_registers->writeFloatReg(regnum, unintValue);
        emit sigRegisterUpdated(regnum, RegisterType::FLOATING_POINT, unintValue);
    }

    DEBUG_LOG(QString("CPU%1: Float register F%2 = 0x%3").arg(m_cpuId).arg(regnum).arg(unintValue, 0, 16));
}

void AlphaCPU::setRegister(quint8 regnum, quint64 unintValue)
{
    if (regnum >= 32)
    {
        DEBUG_LOG(QString("CPU%1: Invalid integer register number: %2").arg(m_cpuId).arg(regnum));
        return;
    }

    ensureComponentsInitialized();

    if (m_registers)
    {
        m_registers->writeIntReg(regnum, unintValue);
        emit sigRegisterUpdated(regnum, RegisterType::INTEGER, unintValue);
    }

    DEBUG_LOG(QString("CPU%1: Integer register R%2 = 0x%3").arg(m_cpuId).arg(regnum).arg(unintValue, 0, 16));
}

quint64 AlphaCPU::convertVaxGToF(quint64 raValue, RoundingMode rm_)
{
    quint64 result = 0;
    return result;
}

quint64 AlphaCPU::implVersion()
{
    // Return implementation version based on CPU model
    switch (m_cpuModel)
    {
    case static_cast<quint64>(CpuModel::CPU_EV4):
        return 0x1; // EV4 implementation
    case static_cast<quint64>(CpuModel::CPU_EV5):
        return 0x2; // EV5 implementation
    case static_cast<quint64>(CpuModel::CPU_EV56):
        return 0x3; // EV56 implementation
    case static_cast<quint64>(CpuModel::CPU_PCA56):
        return 0x4; // PCA56 implementation
    case static_cast<quint64>(CpuModel::CPU_EV6):
        return 0x5; // EV6 implementation
    case static_cast<quint64>(CpuModel::CPU_EV67):
        return 0x6; // EV67 implementation
    case static_cast<quint64>(CpuModel::CPU_EV68):
        return 0x7; // EV68 implementation
    default:
        return 0x1; // Default to EV4
    }
}

quint64 AlphaCPU::convertToVaxGUnbiased(quint64 raValue, RoundingMode rm_)
{
    // Convert IEEE double to VAX G format with unbiased rounding
    double ieeeValue;
    std::memcpy(&ieeeValue, &raValue, sizeof(double));

    // Check for special cases
    if (std::isnan(ieeeValue))
    {
        // VAX doesn't have NaN - return reserved operand fault
        triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
        return 0;
    }

    if (std::isinf(ieeeValue))
    {
        // VAX doesn't have infinity - return overflow
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    // Set unbiased rounding mode
    int oldMode = std::fegetround();
    switch (rm_)
    {
    case RoundingMode::ROUND_TO_NEAREST:
        std::fesetround(FE_TONEAREST);
        break;
    case RoundingMode::ROUND_DOWN:
        std::fesetround(FE_DOWNWARD);
        break;
    case RoundingMode::ROUND_UP:
        std::fesetround(FE_UPWARD);
        break;
    case RoundingMode::ROUND_TO_ZERO:
        std::fesetround(FE_TOWARDZERO);
        break;
    }

    // Convert to VAX G format (simplified - actual conversion is complex)
    // VAX G: sign(1) + exponent(11) + fraction(52) with bias 1024
    quint64 result = raValue; // Placeholder - needs proper VAX G conversion

    // Restore rounding mode
    std::fesetround(oldMode);

    return result;
}

quint64 AlphaCPU::convertToVaxG(quint64 raValue, RoundingMode rm_)
{
    // Convert IEEE double to VAX G format with biased rounding
    double ieeeValue;
    std::memcpy(&ieeeValue, &raValue, sizeof(double));

    // Check for special cases
    if (std::isnan(ieeeValue) || std::isinf(ieeeValue))
    {
        triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
        return 0;
    }

    // Apply biased rounding (add small bias before conversion)
    double biasedValue = ieeeValue;
    if (rm_ == RoundingMode::ROUND_TO_NEAREST)
    {
        // Add small bias for VAX-style rounding
        biasedValue += (ieeeValue > 0) ? 1e-15 : -1e-15;
    }

    // Convert to VAX G format (simplified)
    quint64 result;
    std::memcpy(&result, &biasedValue, sizeof(double));

    return result;
}


quint64 AlphaCPU::convertQuadToG(const DecodedInstruction &instruction, quint64 raValue)
{
    // Convert 64-bit integer to VAX G format
    qint64 intValue = static_cast<qint64>(raValue);

    // Check for overflow (VAX G has limited range)
    if (intValue > 9007199254740992LL || intValue < -9007199254740992LL)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    // Convert to double first
    double doubleValue = static_cast<double>(intValue);

    // Convert to VAX G format (simplified)
    quint64 result;
    std::memcpy(&result, &doubleValue, sizeof(double));

    return result;
}

quint64 AlphaCPU::convertQuadToF(const DecodedInstruction &instruction, quint64 raValue)
{
    // Convert 64-bit integer to VAX F format
    qint64 intValue = static_cast<qint64>(raValue);

    // VAX F format has limited precision (24-bit mantissa)
    if (intValue > 16777216LL || intValue < -16777216LL)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    // Convert to float
    float floatValue = static_cast<float>(intValue);

    // Convert to VAX F format (simplified - needs proper VAX F conversion)
    quint32 result;
    std::memcpy(&result, &floatValue, sizeof(float));

    return static_cast<quint64>(result);
}


quint64 AlphaCPU::convertQuadToS(quint64 faVal, quint64 rbVal)
{
    // Convert 64-bit integer to IEEE S_float (single precision)
    // This is a placeholder - implement proper conversion
    float result = static_cast<float>(static_cast<qint64>(faVal));
    quint32 resultBits;
    std::memcpy(&resultBits, &result, sizeof(float));
    return static_cast<quint64>(resultBits);
}

quint64 AlphaCPU::convertQuadToSChopped(quint64 faVal, quint64 rbVal)
{
    // Convert with chopped (truncate) rounding
    // Save current rounding mode
    int oldMode = std::fegetround();
    std::fesetround(FE_TOWARDZERO); // Chopped = toward zero

    quint64 result = convertQuadToS(faVal, rbVal);

    // Restore rounding mode
    std::fesetround(oldMode);
    return result;
}

quint64 AlphaCPU::convertQuadToT(quint64 faVal, quint64 rbVal)
{
    // Convert 64-bit integer to IEEE T_float (double precision)
    double result = static_cast<double>(static_cast<qint64>(faVal));
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

quint64 AlphaCPU::convertQuadToTChopped(quint64 faVal, quint64 rbVal)
{
    // Convert with chopped (truncate) rounding
    int oldMode = std::fegetround();
    std::fesetround(FE_TOWARDZERO);

    quint64 result = convertQuadToT(faVal, rbVal);

    std::fesetround(oldMode);
    return result;
}

quint64 AlphaCPU::convertSToT(quint64 faVal, quint64 rbVal)
{
    // Convert S_float to T_float (single to double precision)
    quint32 singleBits = static_cast<quint32>(faVal);
    float singleValue;
    std::memcpy(&singleValue, &singleBits, sizeof(float));

    double doubleValue = static_cast<double>(singleValue);
    quint64 doubleBits;
    std::memcpy(&doubleBits, &doubleValue, sizeof(double));
    return doubleBits;
}
quint64 AlphaCPU::convertTToS(quint64 faVal, quint64 rbVal)
{
    // Convert T_float to S_float (double to single precision)
    double doubleValue;
    std::memcpy(&doubleValue, &faVal, sizeof(double));

    float singleValue = static_cast<float>(doubleValue);
    quint32 singleBits;
    std::memcpy(&singleBits, &singleValue, sizeof(float));
    return static_cast<quint64>(singleBits);
}


quint64 AlphaCPU::convertTToQuad(quint64 faVal, quint64 rbVal)
{
    // Convert T_float to 64-bit integer
    double doubleValue;
    std::memcpy(&doubleValue, &faVal, sizeof(double));

    return static_cast<quint64>(static_cast<qint64>(doubleValue));
}
quint64 AlphaCPU::convertToVaxFUnbiased(quint64 raValue, RoundingMode rm_)
{
    // Convert IEEE single to VAX F format with unbiased rounding
    float ieeeValue;
    quint32 valueBits = static_cast<quint32>(raValue);
    std::memcpy(&ieeeValue, &valueBits, sizeof(float));

    if (std::isnan(ieeeValue) || std::isinf(ieeeValue))
    {
        triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
        return 0;
    }

    // Set rounding mode
    int oldMode = std::fegetround();
    switch (rm_)
    {
    case RoundingMode::ROUND_TO_NEAREST:
        std::fesetround(FE_TONEAREST);
        break;
    case RoundingMode::ROUND_DOWN:
        std::fesetround(FE_DOWNWARD);
        break;
    case RoundingMode::ROUND_UP:
        std::fesetround(FE_UPWARD);
        break;
    case RoundingMode::ROUND_TO_ZERO:
        std::fesetround(FE_TOWARDZERO);
        break;
    }

    // Convert to VAX F format (simplified)
    quint32 result = valueBits; // Placeholder

    std::fesetround(oldMode);
    return static_cast<quint64>(result);
}


quint64 AlphaCPU::convertToVaxF(quint64 raValue, RoundingMode rm_)
{
    // Convert IEEE single to VAX F format with biased rounding
    float ieeeValue;
    quint32 valueBits = static_cast<quint32>(raValue);
    std::memcpy(&ieeeValue, &valueBits, sizeof(float));

    if (std::isnan(ieeeValue) || std::isinf(ieeeValue))
    {
        triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
        return 0;
    }

    // Apply bias for VAX-style rounding
    if (rm_ == RoundingMode::ROUND_TO_NEAREST && ieeeValue != 0.0f)
    {
        ieeeValue += (ieeeValue > 0) ? 1e-7f : -1e-7f;
    }

    // Convert to VAX F format (simplified)
    quint32 result;
    std::memcpy(&result, &ieeeValue, sizeof(float));

    return static_cast<quint64>(result);
}


void AlphaCPU::handleCacheCoherencyEvent(quint64 physicalAddr, const QString &eventType)
{
    if (eventType == "INVALIDATE")
    {
        // Invalidate cache lines containing this address
        if (m_level1DataCache)
        {
            m_level1DataCache->invalidateLine(physicalAddr);
        }
        if (m_level2DataCache)
        {
            m_level2DataCache->invalidateLine(physicalAddr);
        }
        if (m_instructionCache && m_instructionCache->getUnifiedCache())
        {
            m_instructionCache->getUnifiedCache()->invalidateLine(physicalAddr);
        }

        DEBUG_LOG("AlphaCPU: Invalidated cache lines on CPU%1 for addr=0x%2", m_cpuId,
                  QString::number(physicalAddr, 16));
    }
    else if (eventType == "FLUSH")
    {
        // Flush cache lines (writeback if dirty, then invalidate)
        if (m_level1DataCache)
        {
            m_level1DataCache->flushLine(physicalAddr);
        }
        if (m_level2DataCache)
        {
            m_level2DataCache->flushLine(physicalAddr);
        }
        if (m_instructionCache && m_instructionCache->getUnifiedCache())
        {
            m_instructionCache->getUnifiedCache()->flushLine(physicalAddr);
        }

        DEBUG_LOG("AlphaCPU: Flushed cache lines on CPU%1 for addr=0x%2", m_cpuId, QString::number(physicalAddr, 16));
    }
    else if (eventType == "SNOOP_READ" || eventType == "SNOOP_WRITE")
    {
        // Handle cache snooping
        if (m_level1DataCache)
        {
            m_level1DataCache->snoop(physicalAddr, eventType);
        }
        if (m_level2DataCache)
        {
            m_level2DataCache->snoop(physicalAddr, eventType);
        }

        DEBUG_LOG("AlphaCPU: Processed snoop %1 on CPU%2 for addr=0x%3", eventType, m_cpuId,
                  QString::number(physicalAddr, 16));
    }

    // Update coherency statistics
    asa_utils::safeIncrement(m_coherencyEvents);

    emit sigCacheCoherencyHandled(physicalAddr, m_cpuId, eventType);
}


void AlphaCPU::invalidateTLBByASN(quint64 asn, quint16 sourceCpuId)
{
    // Delegate to memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateTLBByASN(asn, sourceCpuId);
    }

    DEBUG_LOG(QString("CPU%1: TLB ASN invalidation for ASN=%2").arg(m_cpuId).arg(asn));
}

void AlphaCPU::invalidateTLBEntry(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    // Delegate to memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateTLBEntry(virtualAddr, asn, sourceCpuId);
    }

    DEBUG_LOG(
        QString("CPU%1: TLB entry invalidation for VA=0x%2, ASN=%3").arg(m_cpuId).arg(virtualAddr, 0, 16).arg(asn));
}
void AlphaCPU::invalidateTlbSingleData(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    // Delegate to memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateTlbSingleData(virtualAddr, sourceCpuId);
    }

    DEBUG_LOG(QString("CPU%1: Data TLB single invalidation for VA=0x%2, ASN=%3")
                  .arg(m_cpuId)
                  .arg(virtualAddr, 0, 16)
                  .arg(asn));
}
void AlphaCPU::invalidateTlbSingleInstruction(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    // Delegate to memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateTlbSingleInstruction(virtualAddr, sourceCpuId);
    }

    DEBUG_LOG(QString("CPU%1: Instruction TLB single invalidation for VA=0x%2, ASN=%3")
                  .arg(m_cpuId)
                  .arg(virtualAddr, 0, 16)
                  .arg(asn));
}



void AlphaCPU::invalidateTlbSingle(quint64 virtualAddr, quint64 asn, quint16 sourceCpuId)
{
    // Delegate to memory system - this is the correct approach since TLB is now in memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateTlbSingle(virtualAddr, sourceCpuId);
    }

    DEBUG_LOG(
        QString("CPU%1: TLB single invalidation for VA=0x%2, ASN=%3").arg(m_cpuId).arg(virtualAddr, 0, 16).arg(asn));
}


void AlphaCPU::invalidateTBAllProcess()
{
    // Don't access TLB directly - delegate to memory system
    if (m_memorySystem)
    {
        quint64 currentASN = m_iprs->read(IPRNumbers::IPR_ASN);
        m_memorySystem->invalidateTLBByASN(currentASN, m_cpuId);
    }

    DEBUG_LOG(QString("CPU%1: TLB process invalidation for ASN=%2").arg(m_cpuId).arg(getCurrentASN()));
}


void AlphaCPU::invalidateAllCaches()
{
    if (m_level1DataCache)
    {
        m_level1DataCache->invalidateAll();
    }

    if (m_level2DataCache)
    {
        m_level2DataCache->invalidateAll();
    }

    if (m_instructionCache && m_instructionCache->getUnifiedCache())
    {
        m_instructionCache->getUnifiedCache()->invalidateAll();
    }

    DEBUG_LOG("AlphaCPU: All caches invalidated on CPU%1", m_cpuId);
}
void AlphaCPU::invalidateAllTLB(quint16 sourceCpuId)
{
    // Delegate to memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateAllTLB(sourceCpuId);
    }

    DEBUG_LOG(QString("CPU%1: TLB all invalidation").arg(m_cpuId));
}

void AlphaCPU::invalidateReservation(quint64 physicalAddr, int size)
{
    Q_UNUSED(size);

    // Clear local reservation state if it matches
    m_reservationValid = false;
    m_reservationAddr = 0;

    DEBUG_LOG(QString("CPU%1: Reservation invalidated for PA=0x%2").arg(m_cpuId).arg(physicalAddr, 0, 16));
}

void AlphaCPU::onCacheCoherencyEvent(quint64 physicalAddr, quint16 sourceCpuId, const QString &eventType)
{
    // Don't handle our own events
    if (sourceCpuId == m_cpuId)
    {
        return;
    }

    handleCacheCoherencyEvent(physicalAddr, eventType);
    emit sigCacheCoherencyHandled(physicalAddr, m_cpuId, eventType);
}
void AlphaCPU::onMemoryWriteNotification(quint64 physicalAddr, quint64 value, bool isWrite)
{
    //TODO
}
void AlphaCPU::onMemoryWriteNotification(quint64 physicalAddr, int size, quint16 sourceCpuId)
{
    // Don't handle our own writes
    if (sourceCpuId == m_cpuId)
    {
        return;
    }

    // Invalidate any reservations that overlap with this write
    if (m_reservationValid)
    {
        quint64 reservationEnd = m_reservationAddr + 8; // Assuming 8-byte reservations
        quint64 writeEnd = physicalAddr + size;

        // Check for overlap
        if (!(physicalAddr >= reservationEnd || writeEnd <= m_reservationAddr))
        {
            invalidateReservation(physicalAddr, size);
            emit sigReservationInvalidated(m_cpuId, physicalAddr);
        }
    }
}

void AlphaCPU::onNotifyMemoryAccessed(quint64 physicalAddr, quint64 value, bool isWrite) {

//TODO
}

quint64 AlphaCPU::readAndClearUniqueValue() { 
    
    //TODO
   
}



void AlphaCPU::initializeCpu()
{
    m_palCodeBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);

    // Initialize SMP-specific state
    m_ipiCount.storeRelaxed(0);
    m_coherencyEvents.storeRelaxed(0);

    // Enhanced cache initialization with proper hierarchy
    initializeCacheHierarchy();
    m_processorContext->setCpuId(this->m_cpuId);

    // Connect to memory system signals if available
    if (m_memorySystem)
    {
        connect(m_memorySystem, &AlphaMemorySystem::sigCacheCoherencyEvent, this, &AlphaCPU::onCacheCoherencyEvent);
        connect(m_memorySystem, &AlphaMemorySystem::sigMemoryWriteNotification, this,
                &AlphaCPU::onMemoryWriteNotification);

        // Let memory system know about our ASN for TLB management
        m_memorySystem->updateCPUContext(m_cpuId, getCurrentASN());

        // Register CPU with memory system for cache integration
        m_memorySystem->integrateTLBWithCaches();
    }

    DEBUG_LOG("AlphaCPU: SMP initialization complete for CPU%1", m_cpuId);
}

void AlphaCPU::initializeCacheHierarchy()
{
    // Initialize L1 Data Cache
    if (!m_level1DataCache)
    {
        UnifiedDataCache::Config l1Config;
        l1Config.numSets = 64;
        l1Config.associativity = 2;
        l1Config.lineSize = 64;
        l1Config.totalSize = l1Config.numSets * l1Config.associativity * l1Config.lineSize; // 8KB
        l1Config.enableCoherency = true;
        l1Config.enablePrefetch = true;
        l1Config.statusUpdateInterval = 500; // Faster updates for L1
        l1Config.coherencyProtocol = "MESI";

        m_level1DataCache.reset(new UnifiedDataCache(l1Config, this));

        // Connect L1 cache signals
        connect(m_level1DataCache.data(), &UnifiedDataCache::sigLineEvicted, this,
                [this](quint64 address, bool wasDirty)
                {
                    if (wasDirty)
                    {
                        DEBUG_LOG("AlphaCPU: L1D dirty line evicted on CPU%1: addr=0x%2", m_cpuId,
                                  QString::number(address, 16));
                    }
                });

        connect(m_level1DataCache.data(), &UnifiedDataCache::sigCoherencyViolation, this,
                [this](quint64 address, const QString &operation)
                {
                    ERROR_LOG("AlphaCPU: L1D coherency violation on CPU%1: addr=0x%2, op=%3", m_cpuId,
                              QString::number(address, 16), operation);
                });
    }

    // Initialize L2 Unified Cache
    if (!m_level2DataCache)
    {
        UnifiedDataCache::Config l2Config;
        l2Config.numSets = 256;
        l2Config.associativity = 4;
        l2Config.lineSize = 64;
        l2Config.totalSize = l2Config.numSets * l2Config.associativity * l2Config.lineSize; // 64KB
        l2Config.enableCoherency = true;
        l2Config.enablePrefetch = true;
        l2Config.statusUpdateInterval = 1000;
        l2Config.coherencyProtocol = "MESI";

        m_level2DataCache.reset(new UnifiedDataCache(l2Config, this));

        // Connect L2 cache signals
        connect(m_level2DataCache.data(), &UnifiedDataCache::sigLineEvicted, this,
                [this](quint64 address, bool wasDirty)
                {
                    if (wasDirty)
                    {
                        DEBUG_LOG("AlphaCPU: L2 dirty line evicted on CPU%1: addr=0x%2", m_cpuId,
                                  QString::number(address, 16));
                    }
                });
    }

    // Connect L1 -> L2 hierarchy
    if (m_level1DataCache && m_level2DataCache)
    {
        m_level1DataCache->setNextLevel(m_level2DataCache.data());
        m_level2DataCache->setPrevLevel(m_level1DataCache.data());
        DEBUG_LOG("AlphaCPU: Connected L1->L2 cache hierarchy for CPU%1", m_cpuId);
    }

    // Initialize instruction cache with UnifiedDataCache backing
    if (!m_instructionCache)
    {
        m_instructionCache.reset(new AlphaInstructionCache(this));

        // Create unified cache for instruction cache if needed
        UnifiedDataCache::Config iCacheConfig;
        iCacheConfig.numSets = 64;
        iCacheConfig.associativity = 2;
        iCacheConfig.lineSize = 64;
        iCacheConfig.totalSize = iCacheConfig.numSets * iCacheConfig.associativity * iCacheConfig.lineSize; // 8KB
        iCacheConfig.enableCoherency = true;
        iCacheConfig.enablePrefetch = true;
        iCacheConfig.statusUpdateInterval = 500;
        iCacheConfig.coherencyProtocol = "MESI";

        auto unifiedICache = new UnifiedDataCache(iCacheConfig, this);
        m_instructionCache->setUnifiedCache(unifiedICache);

        DEBUG_LOG("AlphaCPU: Initialized instruction cache for CPU%1", m_cpuId);
    }

    // Set up TLB integration if memory system is available
    if (m_memorySystem && m_memorySystem->getTlbSystem())
    {
        TLBSystem *tlbSystem = m_memorySystem->getTlbSystem();

        if (m_level1DataCache)
        {
            m_level1DataCache->setTLBSystem(tlbSystem, m_cpuId);
        }
        if (m_level2DataCache)
        {
            m_level2DataCache->setTLBSystem(tlbSystem, m_cpuId);
        }
        if (m_instructionCache && m_instructionCache->getUnifiedCache())
        {
            m_instructionCache->getUnifiedCache()->setTLBSystem(tlbSystem, m_cpuId);
        }

        DEBUG_LOG("AlphaCPU: Integrated caches with TLB system for CPU%1", m_cpuId);
    }

    DEBUG_LOG("AlphaCPU: Cache hierarchy initialization complete for CPU%1", m_cpuId);
}
void AlphaCPU::sendIPIBroadcast(int vector)
{
    if (!m_memorySystem)
        return;

    auto cpus = m_memorySystem->getAllCPUs();
    for (const auto &entry : cpus)
    {
        if (entry.cpuId != m_cpuId && entry.isOnline)
        {
            sendIPI(entry.cpuId, vector);
        }
    }

    DEBUG_LOG(QString("CPU%1: Broadcast IPI vector %2 to %3 CPUs").arg(m_cpuId).arg(vector).arg(cpus.size() - 1));
}

void AlphaCPU::flushAllCaches()
{
    if (m_level1DataCache)
    {
        m_level1DataCache->flush();
        DEBUG_LOG("AlphaCPU: Flushed L1 data cache on CPU%1", m_cpuId);
    }

    if (m_level2DataCache)
    {
        m_level2DataCache->flush();
        DEBUG_LOG("AlphaCPU: Flushed L2 cache on CPU%1", m_cpuId);
    }

    if (m_instructionCache && m_instructionCache->getUnifiedCache())
    {
        m_instructionCache->getUnifiedCache()->flush();
        DEBUG_LOG("AlphaCPU: Flushed instruction cache on CPU%1", m_cpuId);
    }

    DEBUG_LOG("AlphaCPU: All caches flushed on CPU%1", m_cpuId);
}
void AlphaCPU::flushCPUTLBCache(quint16 cpuId)
{
    // Delegate to memory system
    if (m_memorySystem)
    {
        m_memorySystem->invalidateAllTLB(cpuId);
    }

    DEBUG_LOG(QString("CPU%1: TLB cache flushed").arg(cpuId));
}
void AlphaCPU::flushTLBAndNotify(int scope, quint64 virtualAddr /*= 0*/)
{
    switch (scope)
    {
    case 0: // Single entry
        if (m_memorySystem)
        {
            m_memorySystem->invalidateTLBEntry(virtualAddr, m_iprs->read(IPRNumbers::IPR_ASN), m_cpuId);
        }
        break;
    case 1: // Process (current ASN)
        if (m_memorySystem)
        {
            m_memorySystem->invalidateTLBByASN(m_iprs->read(IPRNumbers::IPR_ASN), m_cpuId);
        }
        break;
    case 2: // All
        if (m_memorySystem)
        {
            m_memorySystem->invalidateAllTLB(m_cpuId);
        }
        break;
    }
}

void AlphaCPU::memoryBarrierSMP(int type)
{
    // Local memory barrier
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Coordinate with other CPUs if needed
    if (type == 2)
    { // Full barrier
        // Send IPI to all other CPUs to ensure they also barrier
        sendIPIBroadcast(0x10); // Use vector 0x10 for memory barriers
    }

    DEBUG_LOG(QString("CPU%1: SMP memory barrier type %2").arg(m_cpuId).arg(type));
}

void AlphaCPU::handlePALBaseChange(quint64 newPALBase)
{
    // Called when PAL_BASE register changes
    DEBUG_LOG(QString("CPU%1: PAL Base changed to 0x%2").arg(m_cpuId).arg(newPALBase, 0, 16));

    m_palCodeBase = newPALBase;

    // May need to invalidate instruction cache when PAL base changes
    if (m_instructionCache)
    {
        // Invalidate cached PAL instructions
        m_instructionCache->invalidateRange(newPALBase, newPALBase + 0x10000); // 64KB PAL space
    }
}

void AlphaCPU::handleSMPException(ExceptionType exceptionType, quint64 faultAddr, bool needsCoordination /*= false*/)
{
    // Handle local exception first
    dispatchException(exceptionType, faultAddr);

    // Coordinate with other CPUs if needed
    if (needsCoordination && m_memorySystem)
    {
        switch (exceptionType)
        {
        case ExceptionType::PAGE_FAULT:
            // Notify other CPUs about page fault for shared pages
            sendIPIBroadcast(0x20); // Page fault notification vector
            break;
        case ExceptionType::MACHINE_CHECK:
            // Machine check may require system-wide response
            sendIPIBroadcast(0x21); // Machine check notification vector
            break;
        default:
            break;
        }
    }
}

void AlphaCPU::updateSMPPerformanceCounters(int eventType, quint64 count /*= 1*/)
{
    switch (eventType)
    {
    case 0x100: // IPI sent
        m_ipisSent += count;
        break;
    case 0x101: // IPI received
        m_ipisReceived += count;
        break;
    case 0x102: // Cache coherency event
        m_coherencyEvents.fetchAndAddRelaxed(static_cast<int>(count));
        break;
    case 0x103: // Reservation invalidation
        m_reservationInvalidations += count;
        break;
    case 0x104: // TLB invalidation received
        m_tlbInvalidationsReceived += count;
        break;
    default:
        break;
    }
}

void AlphaCPU::incrementPC()
{
    m_pc += 4; // Alpha instructions are 4 bytes
    m_currentPC = m_pc;
}

quint64 AlphaCPU::getPS() const { return m_iprs ? m_iprs->read(IPRNumbers::IPR_PS) : 0; }

void AlphaCPU::incrementPerformanceCounter(enumInstructionPerformance cnt_r)
{
    switch (cnt_r)
    {
    case enumInstructionPerformance::BRANCH_INSTRUCTIONS:
    asa_utils::safeIncrement(m_performanceCounters[0]);
        break;
    case enumInstructionPerformance::BRANCHES_TAKEN:
        asa_utils::safeIncrement(m_performanceCounters[1]);
        break;
    case enumInstructionPerformance::BRANCHES_NOT_TAKEN:
        asa_utils::safeIncrement(m_performanceCounters[2]);
        break;
    case enumInstructionPerformance::BRANCHNES_MISPREDICTIONS:
        asa_utils::safeIncrement(m_performanceCounters[3]);
        break;
    default:
        // Unknown performance counter type
        break;
    }

    // Check for overflow and emit signal if needed
    for (int i = 0; i < 8; ++i)
    {
        if (m_performanceCounters[i] == 0xFFFFFFFFFFFFFFFFULL)
        {
            emit sigPerformanceCounterOverflow(m_cpuId, i);
        }
    }
}

void AlphaCPU::attachMemorySystem(AlphaMemorySystem *memorySystem) { m_memorySystem = memorySystem; }
bool AlphaCPU::atomicCompareAndSwap(quint64 address, quint64 expectedValue, quint64 newValue, int size)
{
    if (!m_memorySystem)
        return false;

    // Use load-locked/store-conditional for atomic CAS
    quint64 currentValue;
    if (!m_memorySystem->loadLocked(m_cpuId, address, currentValue, size, m_pc))
    {
        return false;
    }

    if (currentValue != expectedValue)
    {
        // Clear reservation and return false
        m_memorySystem->clearCpuReservations(m_cpuId);
        return false;
    }

    return m_memorySystem->storeConditional(m_cpuId, address, newValue, size, m_pc);
}

quint64 AlphaCPU::atomicFetchAndAdd(quint64 address, quint64 addValue, int size)
{
    if (!m_memorySystem)
        return 0;

    // Retry loop for atomic fetch-and-add
    const int maxRetries = 100;
    for (int retry = 0; retry < maxRetries; ++retry)
    {
        quint64 currentValue;
        if (!m_memorySystem->loadLocked(m_cpuId, address, currentValue, size, m_pc))
        {
            return 0;
        }

        quint64 newValue = currentValue + addValue;
        if (m_memorySystem->storeConditional(m_cpuId, address, newValue, size, m_pc))
        {
            return currentValue; // Success - return original value
        }

        // Store-conditional failed, retry
    }

    // Failed after max retries
    WARN_LOG(QString("CPU%1: Atomic fetch-and-add failed after %2 retries").arg(m_cpuId).arg(maxRetries));
    return 0;
}

quint64 AlphaCPU::swppalSMP(quint64 newPalBase, bool coordinated /*= true*/)
{
    quint64 oldPalBase = m_palCodeBase;
    m_palCodeBase = newPalBase;

    if (coordinated)
    {
        // Notify other CPUs about PAL switch for coordination
        sendIPIBroadcast(0x30); // PAL switch notification vector

        // Invalidate instruction caches on all CPUs
        if (m_memorySystem)
        {
            m_memorySystem->invalidateCacheLines(newPalBase, 0x10000, m_cpuId); // 64KB PAL code
        }
    }

    return oldPalBase;
}

void AlphaCPU::drainaSMP()
{
    // Local drain first
    drainAborts();

    // Coordinate with other CPUs
    executeMemoryBarrier(2); // Full memory barrier

    // Send drain completion IPI
    sendIPIBroadcast(0x31); // Drain completion vector
}

bool AlphaCPU::translateAddress(quint16 cpuId, quint64 virtualAddr, quint64 &physicalAddr, quint64 asn, bool isWrite,
                                bool isInstruction)
{
    if (!m_memorySystem)
        return false;

    // Use memory system's translation - it handles the TLB internally
    return m_memorySystem->translate(virtualAddr, physicalAddr, isWrite ? 1 : (isInstruction ? 2 : 0));
}
bool AlphaCPU::translateVirtualAddress(quint64 virtualAddr, quint64 &physicalAddr, bool isWrite, bool isInstruction)
{
    if (!m_memorySystem)
        return false;

    quint64 currentASN = getCurrentASN();
    return m_memorySystem->translateAddress(m_cpuId, virtualAddr, physicalAddr, currentASN, isWrite, isInstruction);
}

void AlphaCPU::triggerException(ExceptionType eType, quint64 targetPC)
{
    // TODO
}

AlphaCPU::CPUTopology AlphaCPU::getCPUTopology() const
{
    CPUTopology topo;
    topo.cpuId = m_cpuId;
    topo.coreId = m_cpuId;        // Simple 1:1 mapping for now
    topo.packageId = m_cpuId / 4; // 4 CPUs per package
    topo.threadId = 0;
    topo.isHyperthreaded = false;
    return topo;
}

quint64 AlphaCPU::readWHAMI() const
{
    // Encode CPU ID and other identification in WHAMI format
    quint64 whami = 0;
    whami |= (m_cpuId & 0xFF);                          // CPU ID (bits 7:0)
    whami |= ((getCPUTopology().packageId & 0xF) << 8); // Package ID (bits 11:8)
    whami |= (static_cast<quint64>(m_cpuModel) << 16);  // CPU model (bits 23:16)
    return whami;
}

void AlphaCPU::initializeRegisters()
{
    if (!m_registers)
    {
        m_registers = new RegisterBank(this);
    }

//     // Connect RegisterBank exception signals
//     connect(m_registers.data(), &RegisterBank::sigArithmeticExceptionRaised, this,
//             [this](ExceptionTypeArithmetic type)
//             {
//                 // Convert to CPU exception type and handle
//                 ExceptionType cpuType = convertArithmeticException(type);
//                 raiseException(cpuType, getPC());
//             });
// 
//     connect(m_registers.data(), &RegisterBank::sigExceptionRaised, this,
//             [this](FPTrapType type)
//             {
//                 // Handle floating-point exceptions
//                 handleFloatingPointException(type);
//             });
}

void AlphaCPU::initializeSMP()
{
    // Initialize SMP-specific state
    m_ipiCount= 0;
    m_coherencyEvents = 0;

    // Connect to memory system signals if available
    if (m_memorySystem)
    {
        connect(m_memorySystem, &AlphaMemorySystem::sigCacheCoherencyEvent, this, &AlphaCPU::onCacheCoherencyEvent);
        connect(m_memorySystem, &AlphaMemorySystem::sigMemoryWriteNotification, this,
                &AlphaCPU::onMemoryWriteNotification);
        connect(m_memorySystem, &AlphaMemorySystem::sigReservationCleared, this, &AlphaCPU::onReservationCleared);
    }

    DEBUG_LOG(QString("CPU%1: SMP initialization complete").arg(m_cpuId));
}

void AlphaCPU::onReservationCleared(quint16 cpuId, quint64 physicalAddr, int size)
{
    if (cpuId == m_cpuId)
    {
        invalidateReservation(physicalAddr, size);
        emit sigReservationInvalidated(m_cpuId, physicalAddr);
    }
}


void AlphaCPU::ensureComponentsInitialized()
{
    if (!m_registers)
    {
        m_registers = new RegisterBank(this);
    }
}

void AlphaCPU::vectorToExceptionHandler(quint64 exceptionCode, quint64 faultingPC)
{
    // Get the appropriate exception vector based on exception type
    quint64 vectorAddress = 0;

    switch (exceptionCode & 0xFF00)
    {
    case 0x0000: // System exceptions
        quint64 palBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);
        quint64 vectorAddress = palBase + getExceptionVector(0);
       // vectorAddress = getExceptionVector(0);
        break;
    case 0x0100: // Arithmetic exceptions
        quint64 palBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);
        quint64 vectorAddress = palBase + getExceptionVector(1);
       // vectorAddress = getExceptionVector(1);
        break;
    case 0x0200: // Interrupt exceptions
        quint64 palBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);
        quint64 vectorAddress = palBase + getExceptionVector(2);
       // vectorAddress = getExceptionVector(2);
        break;
    case 0x0400: // Memory management exceptions
        quint64 palBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);
        quint64 vectorAddress = palBase + getExceptionVector(3);
        //vectorAddress = getExceptionVector(3);
        break;
    default:
        quint64 palBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);
        quint64 vectorAddress = palBase + getExceptionVector(0);
        //vectorAddress = getExceptionVector(0); // Default to system exception
        break;
    }

    if (vectorAddress != 0)
    {
        // Switch to kernel mode
        setPrivilegeMode(0);

        // Disable interrupts
        disableInterrupts();

        // Jump to exception handler
        setPC(vectorAddress);

        DEBUG_LOG(QString("CPU%1: Vectoring to exception handler at 0x%2").arg(m_cpuId).arg(vectorAddress, 0, 16));
    }
}

quint64 AlphaCPU::getProcessCycleCounter() { 
    
    //TODO
    // 
    throw std::logic_error("The method or operation is not implemented."); 
}

quint64 AlphaCPU::readAndSetUniqueValue() { 
    
    //TODO 
    throw std::logic_error("The method or operation is not implemented."); 
}

// Get exception vector address
quint64 AlphaCPU::getExceptionVector(int vectorNumber)
{
    if (m_iprs && vectorNumber >= 0 && vectorNumber < 8)
    {
        return m_iprs->read(static_cast<Ipr>(static_cast<int>(IPRNumbers::IPR_ENTRY_0) + vectorNumber));
    }
    return 0;
}

quint64 AlphaCPU::readIPRByName(const QString &name) const
{
    if (!m_iprs)
        return 0;

    // Extended IPR name mapping for PAL functions
    if (name == "ASN")
        return m_iprs->read(IPRNumbers::IPR_ASN);
    if (name == "MCES")
        return m_iprs->read(IPRNumbers::IPR_MCES);
    if (name == "PCBB")
        return m_iprs->read(IPRNumbers::IPR_PCBB);
    if (name == "PRBR")
        return m_iprs->read(IPRNumbers::IPR_PRBR);
    if (name == "PTBR")
        return m_iprs->read(IPRNumbers::IPR_PTBR);
    if (name == "SCBB")
        return m_iprs->read(IPRNumbers::IPR_SCBB);
    if (name == "SIRR")
        return m_iprs->read(IPRNumbers::IPR_SIRR);
    if (name == "SISR")
        return m_iprs->read(IPRNumbers::IPR_SISR);
    if (name == "SSP")
        return m_iprs->read(IPRNumbers::IPR_SSP);
    if (name == "ESP")
        return m_iprs->read(IPRNumbers::IPR_ESP);
    if (name == "KSP")
        return m_iprs->read(IPRNumbers::IPR_KSP);
    if (name == "IRQL")
        return m_iprs->read(IPRNumbers::IPR_IRQL);
    if (name == "TBCHK")
        return m_iprs->read(IPRNumbers::IPR_TBCHK);
    if (name == "UNQ")
        return m_iprs->read(IPRNumbers::IPR_UNQ);
    if (name == "THREAD")
        return m_iprs->read(IPRNumbers::IPR_THREAD);
    if (name == "PAL_MODE")
        return m_iprs->read(IPRNumbers::IPR_PAL_MODE);
    if (name == "RESTART_VECTOR")
        return m_iprs->read(IPRNumbers::IPR_RESTART_VECTOR);
    if (name == "DEBUGGER_VECTOR")
        return m_iprs->read(IPRNumbers::IPR_DEBUGGER_VECTOR);
    if (name == "PROCESS")
        return m_iprs->read(IPRNumbers::IPR_PROCESS);

    return readIPR(name); // Fall back to existing method
}

quint64 AlphaCPU::readPerformanceCounter(quint64 counterNum) const { return getPerformanceCounter(counterNum); }

bool AlphaCPU::writeIPRByName(const QString &name, quint64 value)
{
    if (!m_iprs)
        return false;

    // Extended IPR name mapping for PAL functions
    if (name == "ASN")
    {
        m_iprs->write(IPRNumbers::IPR_ASN, value);
        return true;
    }
    if (name == "MCES")
    {
        m_iprs->write(IPRNumbers::IPR_MCES, value);
        return true;
    }
    if (name == "PCBB")
    {
        m_iprs->write(IPRNumbers::IPR_PCBB, value);
        return true;
    }
    if (name == "PRBR")
    {
        m_iprs->write(IPRNumbers::IPR_PRBR, value);
        return true;
    }
    if (name == "PTBR")
    {
        m_iprs->write(IPRNumbers::IPR_PTBR, value);
        return true;
    }
    if (name == "SCBB")
    {
        m_iprs->write(IPRNumbers::IPR_SCBB, value);
        return true;
    }
    if (name == "SIRR")
    {
        m_iprs->write(IPRNumbers::IPR_SIRR, value);
        return true;
    }
    if (name == "SISR")
    {
        m_iprs->write(IPRNumbers::IPR_SISR, value);
        return true;
    }
    if (name == "SSP")
    {
        m_iprs->write(IPRNumbers::IPR_SSP, value);
        return true;
    }
    if (name == "ESP")
    {
        m_iprs->write(IPRNumbers::IPR_ESP, value);
        return true;
    }
    if (name == "KSP")
    {
        m_iprs->write(IPRNumbers::IPR_KSP, value);
        return true;
    }
    if (name == "IRQL")
    {
        m_iprs->write(IPRNumbers::IPR_IRQL, value);
        return true;
    }
    if (name == "TBCHK")
    {
        m_iprs->write(IPRNumbers::IPR_TBCHK, value);
        return true;
    }
    if (name == "UNQ")
    {
        m_iprs->write(IPRNumbers::IPR_UNQ, value);
        return true;
    }
    if (name == "THREAD")
    {
        m_iprs->write(IPRNumbers::IPR_THREAD, value);
        return true;
    }
    if (name == "PAL_MODE")
    {
        m_iprs->write(IPRNumbers::IPR_PAL_MODE, value);
        return true;
    }
    if (name == "RESTART_VECTOR")
    {
        m_iprs->write(IPRNumbers::IPR_RESTART_VECTOR, value);
        return true;
    }
    if (name == "DEBUGGER_VECTOR")
    {
        m_iprs->write(IPRNumbers::IPR_DEBUGGER_VECTOR, value);
        return true;
    }
    if (name == "PROCESS")
    {
        m_iprs->write(IPRNumbers::IPR_PROCESS, value);
        return true;
    }

    return writeIPR(name, value); // Fall back to existing method
}

void AlphaCPU::setPerformanceCounter(int counterNum, quint64 value)
{
    if (counterNum >= 0 && counterNum < 8)
    {
        m_performanceCounters[counterNum] = value;
    }
}

void AlphaCPU::checkSoftwareInterrupts()
{
    if (!m_iprs)
        return;

    // Check Software Interrupt Summary Register (SISR)
    quint64 sisr = m_iprs->read(IPRNumbers::IPR_SISR);
    if (sisr == 0)
        return; // No software interrupts pending

    // Check which software interrupt levels are pending
    for (int level = 1; level <= 15; ++level)
    {
        if (sisr & (1ULL << level))
        {
            // Check if this interrupt can be delivered
            if (canTakeInterrupt(level))
            {
                // Clear the bit in SISR
                sisr &= ~(1ULL << level);
                m_iprs->write(IPRNumbers::IPR_SISR, sisr);

                // Deliver the software interrupt
                deliverInterrupt(level);

                DEBUG_LOG(QString("CPU%1: Software interrupt level %2 delivered").arg(m_cpuId).arg(level));
                return; // Only deliver highest priority interrupt
            }
        }
    }
}

void AlphaCPU::updateCPUContext(quint16 cpuId, quint64 newASN)
{
    // Update our ASN
    if (m_iprs)
    {
        m_iprs->write(IPRNumbers::IPR_ASN, newASN);
    }

    DEBUG_LOG(QString("CPU%1: Context updated to ASN=%2").arg(cpuId).arg(newASN));
}

void AlphaCPU::updateInterruptPriority()
{
    if (!m_iprs)
        return;

    // Get current processor status
    quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);

    // Extract current IPL (Interrupt Priority Level) from PS register
    quint64 currentIPL = (ps >> 3) & 0x7; // Bits 5:3 contain IPL

    // Update the standalone IPL register for compatibility
    m_iprs->write(IPRNumbers::IPR_IPLR, currentIPL);

    // Check if any pending interrupts can now be delivered
    if (interruptPending.loadRelaxed() && (ps & PS_INTERRUPT_ENABLE))
    {
        // Check hardware interrupts (IPIs)
        deliverPendingInterrupt();

        // Check software interrupts
        checkSoftwareInterrupts();
    }

    DEBUG_LOG(QString("CPU%1: Interrupt priority updated to %2, interrupts %3")
                  .arg(m_cpuId)
                  .arg(currentIPL)
                  .arg((ps & PS_INTERRUPT_ENABLE) ? "enabled" : "disabled"));
}

void AlphaCPU::updateProcessorStatus(quint64 newPS)
{
    if (!m_iprs)
        return;

    quint64 oldPS = m_iprs->read(IPRNumbers::IPR_PS);

    // Store the new processor status
    m_iprs->write(IPRNumbers::IPR_PS, newPS);

    // Extract and update privilege mode
    quint64 newMode = newPS & PS_CURRENT_MODE; // Bits 1:0
    switch (newMode)
    {
    case PS_MODE_KERNEL:
        m_currentMode = enumProcessorMode::KERNEL;
        break;
    case PS_MODE_EXECUTIVE:
        m_currentMode = enumProcessorMode::EXECUTIVE;
        break;
    case PS_MODE_SUPERVISOR:
        m_currentMode = enumProcessorMode::SUPERVISOR;
        break;
    case PS_MODE_USER:
        m_currentMode = enumProcessorMode::USER;
        break;
    }

    // Update interrupt enable flag
    m_interruptEnable = (newPS & PS_INTERRUPT_ENABLE) != 0;

    // Update IPL if it changed
    quint64 oldIPL = (oldPS >> 3) & 0x7;
    quint64 newIPL = (newPS >> 3) & 0x7;
    if (oldIPL != newIPL)
    {
        updateInterruptPriority();
    }

    // Handle PAL mode transition
    bool oldPALMode = (oldPS & PS_PAL_MODE) != 0;
    bool newPALMode = (newPS & PS_PAL_MODE) != 0;
    if (oldPALMode != newPALMode)
    {
        if (newPALMode)
        {
            DEBUG_LOG(QString("CPU%1: Entered PAL mode").arg(m_cpuId));
        }
        else
        {
            DEBUG_LOG(QString("CPU%1: Exited PAL mode").arg(m_cpuId));
        }
    }

    // Emit signal for status change
    emit sigCpuStatusUpdate(m_cpuId);

    DEBUG_LOG(
        QString("CPU%1: Processor status updated from 0x%2 to 0x%3").arg(m_cpuId).arg(oldPS, 0, 16).arg(newPS, 0, 16));
}

// Additional helper methods that might be called

void AlphaCPU::handleFloatingPointException(FPTrapType type)
{
    // Handle floating-point exceptions based on type
    switch (type)
    {
    case FPTrapType::INVALID_OPERATION:
        raiseException(ExceptionType::ARITHMETIC, getPC());
        break;
    case FPTrapType::DIVISION_BY_ZERO:
        raiseException(ExceptionType::ARITHMETIC, getPC());
        break;
    case FPTrapType::OVERFLOW:
        raiseException(ExceptionType::ARITHMETIC, getPC());
        break;
    case FPTrapType::UNDERFLOW:
        raiseException(ExceptionType::ARITHMETIC, getPC());
        break;
    default:
        raiseException(ExceptionType::ARITHMETIC, getPC());
        break;
    }
}

void AlphaCPU::handleInterruptPriorityChange(quint64 newIPL)
{
    if (!m_iprs)
        return;

    // Update the IPL field in the PS register
    quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
    ps &= ~(0x7ULL << 3);        // Clear IPL field (bits 5:3)
    ps |= ((newIPL & 0x7) << 3); // Set new IPL

    updateProcessorStatus(ps);
}

void AlphaCPU::maskInterrupt(int level)
{
    if (level < 0 || level > 31)
        return;

    if (m_iprs)
    {
        quint64 mask = m_iprs->read(IPRNumbers::IPR_EXC_MASK);
        mask |= (1ULL << level); // Set bit to mask this interrupt level
        m_iprs->write(IPRNumbers::IPR_EXC_MASK, mask);
    }
}

void AlphaCPU::unmaskInterrupt(int level)
{
    if (level < 0 || level > 31)
        return;

    if (m_iprs)
    {
        quint64 mask = m_iprs->read(IPRNumbers::IPR_EXC_MASK);
        mask &= ~(1ULL << level); // Clear bit to unmask this interrupt level
        m_iprs->write(IPRNumbers::IPR_EXC_MASK, mask);
    }
}

bool AlphaCPU::isInterruptMasked(int level) const
{
    if (level < 0 || level > 31 || !m_iprs)
        return true; // Default to masked if invalid

    quint64 mask = m_iprs->read(IPRNumbers::IPR_EXC_MASK);
    return (mask & (1ULL << level)) != 0;
}

void AlphaCPU::checkPendingInterrupts()
{
    if (!m_iprs || !m_interruptEnable)
        return;

    // Check hardware interrupts first (higher priority)
    if (!pendingInterrupts.isEmpty())
    {
        deliverPendingInterrupt();
    }

    // Then check software interrupts
    checkSoftwareInterrupts();
}

void AlphaCPU::setInterruptPriorityLevel(quint64 newIPL)
{
    if (newIPL > 7) // IPL is 3 bits (0-7)
        return;

    handleInterruptPriorityChange(newIPL);
}

quint64 AlphaCPU::getInterruptPriorityLevel() const
{
    if (!m_iprs)
        return 0;

    quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
    return (ps >> 3) & 0x7; // Extract IPL from bits 5:3
}


bool AlphaCPU::hasBranchPredictor()
{
    // Return true if this CPU model has a branch predictor
    switch (m_cpuModel)
    {
    case CpuModel::CPU_EV4:
    case CpuModel::CPU_EV5:
        return false; // Early Alpha CPUs had no branch predictor
    case CpuModel::CPU_EV56:
    case CpuModel::CPU_PCA56:
    case CpuModel::CPU_EV6:
    case CpuModel::CPU_EV67:
    case CpuModel::CPU_EV68:
        return true; // Later CPUs have branch predictors
    default:
        return true; // Default to having one
    }
}

bool AlphaCPU::fetchInstructionWithCache(quint64 pc, quint32 &instruction)
{
    if (!m_memorySystem)
        return false;

    // Let memory system handle TLB translation and caching
    quint64 instruction64;
    if (m_memorySystem->readVirtualMemory(m_cpuId, pc, instruction64, 4, pc))
    {
        instruction = static_cast<quint32>(instruction64 & 0xFFFFFFFF);
        return true;
    }

    return false;
}
void AlphaCPU::flushPipeline()
{
    // Flush instruction pipeline
    // In a real implementation, this would:
    // 1. Cancel pending instructions
    // 2. Clear prediction buffers
    // 3. Invalidate instruction cache lines

    if (m_instructionCache)
    {
        m_instructionCache->flushPipeline();
    }

    // Clear any pending loads
    // Reset branch prediction state
    // Clear decode buffers

    DEBUG_LOG(QString("CPU%1: Pipeline flushed").arg(m_cpuId));
}

quint64 AlphaCPU::getFloatRegister32(quint64 reg_)
{
    if (reg_ >= 32)
        return 0;

    ensureComponentsInitialized();

    if (m_registers)
    {
        quint64 fullValue = m_registers->readFloatReg(static_cast<quint8>(reg_));
        return fullValue & 0xFFFFFFFF; // Return lower 32 bits
    }
    return 0;
}

quint64 AlphaCPU::getFloatRegister64(quint64 reg_)
{
    if (reg_ >= 32)
        return 0;

    ensureComponentsInitialized();

    if (m_registers)
    {
        return m_registers->readFloatReg(static_cast<quint8>(reg_));
    }
    return 0;
}


quint64 AlphaCPU::getFloatRegister(quint64 reg_) { return getFloatRegister64(reg_); }

void AlphaCPU::applyUnbiasedRounding(quint64 aur_)
{
    // Apply unbiased rounding to floating point operations
    // This affects the FPCR rounding mode temporarily

    if (!m_iprs)
        return;

    // Read current FPCR
    quint64 fpcr = m_iprs->read(IPRNumbers::IPR_FPCR);

    // Set unbiased rounding mode (clear bias bit)
    fpcr &= ~(1ULL << 58); // Clear UNBIASED bit

    // Write back FPCR
    m_iprs->write(IPRNumbers::IPR_FPCR, fpcr);

    DEBUG_LOG(QString("CPU%1: Applied unbiased rounding").arg(m_cpuId));
}

quint64 AlphaCPU::scaleVaxFResult(quint64 addr_)
{
    // Scale VAX F format result based on scale factor
    float value;
    quint32 bits = static_cast<quint32>(addr_);
    std::memcpy(&value, &bits, sizeof(float));

    // Apply VAX F format scaling (exponent bias = 128)
    if (value != 0.0f)
    {
        // VAX F has different exponent bias than IEEE
        // This is a simplified scaling
        value *= 1.0f; // Placeholder for actual scaling
    }

    quint32 result;
    std::memcpy(&result, &value, sizeof(float));
    return static_cast<quint64>(result);
}

quint64 AlphaCPU::scaleVaxGResult(quint64 addr_)
{
    // Scale VAX G format result
    double value;
    std::memcpy(&value, &addr_, sizeof(double));

    // Apply VAX G format scaling (exponent bias = 1024)
    if (value != 0.0)
    {
        // VAX G has different exponent bias than IEEE
        value *= 1.0; // Placeholder for actual scaling
    }

    quint64 result;
    std::memcpy(&result, &value, sizeof(double));
    return result;
}

quint64 AlphaCPU::scaleIeeeTResult(quint64 addr_)
{
    // Scale IEEE T format (double precision) result
    double value;
    std::memcpy(&value, &addr_, sizeof(double));

    // IEEE T format doesn't need special scaling
    return addr_;
}

quint64 AlphaCPU::scaleIeeeSResult(quint64 addr_)
{
    // Scale IEEE S format (single precision) result
    quint32 bits = static_cast<quint32>(addr_);

    // IEEE S format doesn't need special scaling
    return static_cast<quint64>(bits);
}


bool AlphaCPU::handleMemoryFault(quint64 faultingAddress, bool isWrite, const PALInstruction &instr)
{
    try
    {
        // Determine fault type
        bool isAlignment = (faultingAddress & 0x7) != 0; // Check 8-byte alignment
        bool isTranslation = false;

        // Check if translation is valid
        quint64 physAddr;
        if (m_memorySystem)
        {
            isTranslation =
                !m_memorySystem->translateAddress(m_cpuId, faultingAddress, physAddr, getCurrentASN(), isWrite, false);
        }

        // Raise appropriate memory exception
        raiseMemoryException(faultingAddress, isWrite, isTranslation, isAlignment);

        return false; // Fault handled by exception
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG(QString("CPU%1: Exception during fault handling: %2").arg(m_cpuId).arg(e.what()));
        return false;
    }
}

bool AlphaCPU::hasPerformanceCounters() { return true; }

bool AlphaCPU::isFloatingPointEnabled()
{
    // Check if floating-point operations are enabled
    if (m_iprs)
    {
        quint64 fen = m_iprs->read(IPRNumbers::IPR_FEN);
        return (fen & 0x1) != 0; // Check FEN enable bit
    }
    return true; // Default to enabled
}

bool AlphaCPU::isKernelMode() const { return (m_currentMode == enumProcessorMode::KERNEL); }

bool AlphaCPU::canTakeInterrupt(int vector) const
{
    if (!m_iprs)
        return false;

    // Check if interrupts are enabled using your constant
    quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
    if ((ps & PS_INTERRUPT_ENABLE) == 0) // Check bit 0
        return false;

    // Check interrupt priority level
    quint64 ipl = m_iprs->read(IPRNumbers::IPR_IPL);
    quint64 irql = m_iprs->read(IPRNumbers::IPR_IRQL);

    // Can take interrupt if vector priority > current IPL and >= IRQL
    return (vector > ipl) && (vector >= irql);
}


bool AlphaCPU::checkLockFlag()
{
    // Check if load-locked reservation is still valid
    return m_reservationValid;
}
void AlphaCPU::clearLockFlag()
{
    // Clear load-locked reservation
    m_reservationValid = false;
    m_reservationAddr = 0;

    DEBUG_LOG(QString("CPU%1: Lock flag cleared").arg(m_cpuId));
}




void AlphaCPU::triggerFloatingPointException(FPTrapType fpTrap)
{
    if (!m_iprs)
        return;

    // Set appropriate bits in FPCR exception summary
    quint64 fpcr = m_iprs->read(IPRNumbers::IPR_FPCR);

    switch (fpTrap)
    {
    case FPTrapType::FP_INVALID_OPERATION:
        fpcr |= (1ULL << 1); // INV bit
        break;
    case FPTrapType::FP_DIVISION_BY_ZERO:
        fpcr |= (1ULL << 2); // DZE bit
        break;
    case FPTrapType::FP_OVERFLOW:
        fpcr |= (1ULL << 3); // OVF bit
        break;
    case FPTrapType::FP_UNDERFLOW:
        fpcr |= (1ULL << 4); // UNF bit
        break;
    case FPTrapType::FP_INEXACT:
        fpcr |= (1ULL << 5); // INE bit
        break;
    }

    m_iprs->write(IPRNumbers::IPR_FPCR, fpcr);

    // Raise arithmetic exception
    raiseException(ExceptionType::ARITHMETIC, m_pc);

    DEBUG_LOG(QString("CPU%1: Floating point exception: %2").arg(m_cpuId).arg(static_cast<int>(fpTrap)));
}

quint64 AlphaCPU::getFloatingPointNaN()
{
    // Return IEEE double precision quiet NaN
    return 0x7FF8000000000000ULL;
}

bool AlphaCPU::writeVirtualMemory(quint64 addr_, quint64 val_)
{
    if (!m_memorySystem)
        return false;

    return m_memorySystem->writeVirtualMemory(m_cpuId, addr_, val_, 8, m_pc);
}

bool AlphaCPU::writeIOSpace(quint64 addr_, quint64 val_)
{
    if (!m_mmIoManager)
        return false;

    try
    {
        m_mmIoManager->writeIO(addr_, val_, 8);
        return true;
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG(QString("CPU%1: IO write failed at 0x%2: %3").arg(m_cpuId).arg(addr_, 0, 16).arg(e.what()));
        return false;
    }
}

bool AlphaCPU::writeConfigSpace(quint64 addr_, quint64 val_)
{
    if (!m_mmIoManager)
        return false;

    try
    {
        m_mmIoManager->writeConfig(addr_, val_, 8);
        return true;
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG(QString("CPU%1: Config write failed at 0x%2: %3").arg(m_cpuId).arg(addr_, 0, 16).arg(e.what()));
        return false;
    }
}

bool AlphaCPU::writeMemoryConditional(quint64 addr_, quint64 val_)
{
    if (!m_memorySystem)
        return false;

    return m_memorySystem->storeConditional(m_cpuId, addr_, val_, 8, m_pc);
}


quint64 AlphaCPU::addFFormat(quint64 faVal, quint64 rbVal)
{
    // Add two VAX F_floating values
    // VAX F format: sign(1) + exponent(8) + fraction(23) with bias 128

    // Extract VAX F components (simplified - real implementation needs full VAX F handling)
    float a = static_cast<float>(faVal); // Placeholder conversion
    float b = static_cast<float>(rbVal); // Placeholder conversion

    // Check for VAX F special cases
    if (faVal == 0 || rbVal == 0)
    {
        return (faVal == 0) ? rbVal : faVal;
    }

    float result = a + b;

    // Check for overflow/underflow in VAX F range
    if (std::isinf(result) || std::abs(result) > 1.7e38f)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 2.9e-39f && result != 0.0f)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}

quint64 AlphaCPU::addGFormat(quint64 faVal, quint64 rbVal)
{
    // Add two VAX G_floating values
    // VAX G format: sign(1) + exponent(11) + fraction(52) with bias 1024

    if (faVal == 0)
        return rbVal;
    if (rbVal == 0)
        return faVal;

    double a = static_cast<double>(faVal); // Placeholder conversion
    double b = static_cast<double>(rbVal); // Placeholder conversion

    double result = a + b;

    // Check for VAX G overflow/underflow
    if (std::isinf(result) || std::abs(result) > 8.9e307)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 5.6e-309 && result != 0.0)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}


quint64 AlphaCPU::subDFormat(quint64 faVal, quint64 rbVal)
{
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a - b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}
quint64 AlphaCPU::subFFormat(quint64 faVal, quint64 rbVal)
{
    // Subtract two VAX F_floating values
    float a = static_cast<float>(faVal); // Placeholder conversion
    float b = static_cast<float>(rbVal); // Placeholder conversion

    if (faVal == 0)
        return static_cast<quint64>(-static_cast<float>(rbVal));
    if (rbVal == 0)
        return faVal;

    float result = a - b;

    // Check for overflow/underflow
    if (std::isinf(result) || std::abs(result) > 1.7e38f)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 2.9e-39f && result != 0.0f)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}

quint64 AlphaCPU::mulDFormat(quint64 faVal, quint64 rbVal)
{
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a * b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

quint64 AlphaCPU::mulGFormat(quint64 faVal, quint64 rbVal)
{
    // Multiply two VAX G_floating values
    if (faVal == 0 || rbVal == 0)
        return 0;

    double a = static_cast<double>(faVal); // Placeholder conversion
    double b = static_cast<double>(rbVal); // Placeholder conversion

    double result = a * b;

    // Check for overflow/underflow
    if (std::isinf(result) || std::abs(result) > 8.9e307)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 5.6e-309 && result != 0.0)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}
quint64 AlphaCPU::mulSFormat(quint64 faVal, quint64 rbVal)
{
    quint32 aBits = static_cast<quint32>(faVal);
    quint32 bBits = static_cast<quint32>(rbVal);
    float a, b;
    std::memcpy(&a, &aBits, sizeof(float));
    std::memcpy(&b, &bBits, sizeof(float));
    float result = a * b;
    quint32 resultBits;
    std::memcpy(&resultBits, &result, sizeof(float));
    return static_cast<quint64>(resultBits);
}

quint64 AlphaCPU::mulTFormat(quint64 faVal, quint64 rbVal)
{
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a * b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

quint64 AlphaCPU::mulFFormat(quint64 faVal, quint64 rbVal)
{
    // Multiply two VAX F_floating values
    if (faVal == 0 || rbVal == 0)
        return 0;

    float a = static_cast<float>(faVal); // Placeholder conversion
    float b = static_cast<float>(rbVal); // Placeholder conversion

    float result = a * b;

    // Check for overflow/underflow
    if (std::isinf(result) || std::abs(result) > 1.7e38f)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 2.9e-39f && result != 0.0f)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}

quint64 AlphaCPU::divFFormat(quint64 faVal, quint64 rbVal)
{
    // Divide two VAX F_floating values
    if (rbVal == 0)
    {
        triggerFloatingPointException(FPTrapType::FP_DIVISION_BY_ZERO);
        return 0;
    }

    if (faVal == 0)
        return 0;

    float a = static_cast<float>(faVal); // Placeholder conversion
    float b = static_cast<float>(rbVal); // Placeholder conversion

    float result = a / b;

    // Check for overflow/underflow
    if (std::isinf(result) || std::abs(result) > 1.7e38f)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 2.9e-39f && result != 0.0f)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}


quint64 AlphaCPU::divGFormat(quint64 faVal, quint64 rbVal)
{
    // Divide two VAX G_floating values
    if (rbVal == 0)
    {
        triggerFloatingPointException(FPTrapType::FP_DIVISION_BY_ZERO);
        return 0;
    }

    if (faVal == 0)
        return 0;

    double a = static_cast<double>(faVal); // Placeholder conversion
    double b = static_cast<double>(rbVal); // Placeholder conversion

    double result = a / b;

    // Check for overflow/underflow in VAX G range
    if (std::isinf(result) || std::abs(result) > 8.9e307)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 5.6e-309 && result != 0.0)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}
quint64 AlphaCPU::addSFormat(quint64 faVal, quint64 rbVal)
{
    // Add two IEEE S_floating values
    quint32 aBits = static_cast<quint32>(faVal);
    quint32 bBits = static_cast<quint32>(rbVal);
    float a, b;
    std::memcpy(&a, &aBits, sizeof(float));
    std::memcpy(&b, &bBits, sizeof(float));
    float result = a + b;
    quint32 resultBits;
    std::memcpy(&resultBits, &result, sizeof(float));
    return static_cast<quint64>(resultBits);
}


quint64 AlphaCPU::addTFormat(quint64 faVal, quint64 rbVal)
{
    // Add two IEEE T_floating values
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a + b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

quint64 AlphaCPU::subGFormat(quint64 faVal, quint64 rbVal)
{
    // Subtract two VAX G_floating values
    if (faVal == 0)
        return static_cast<quint64>(-static_cast<double>(rbVal));
    if (rbVal == 0)
        return faVal;

    double a = static_cast<double>(faVal); // Placeholder conversion
    double b = static_cast<double>(rbVal); // Placeholder conversion

    double result = a - b;

    // Check for overflow/underflow
    if (std::isinf(result) || std::abs(result) > 8.9e307)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    if (std::abs(result) < 5.6e-309 && result != 0.0)
    {
        triggerFloatingPointException(FPTrapType::FP_UNDERFLOW);
        return 0;
    }

    return static_cast<quint64>(result); // Placeholder conversion
}
quint64 AlphaCPU::subSFormat(quint64 faVal, quint64 rbVal)
{
    quint32 aBits = static_cast<quint32>(faVal);
    quint32 bBits = static_cast<quint32>(rbVal);
    float a, b;
    std::memcpy(&a, &aBits, sizeof(float));
    std::memcpy(&b, &bBits, sizeof(float));
    float result = a - b;
    quint32 resultBits;
    std::memcpy(&resultBits, &result, sizeof(float));
    return static_cast<quint64>(resultBits);
}

quint64 AlphaCPU::subTFormat(quint64 faVal, quint64 rbVal)
{
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a - b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

quint64 AlphaCPU::getPerformanceCounter(int counterNum) const
{
    if (counterNum >= 0 && counterNum < 8)
    {
        return m_performanceCounters[counterNum];
    }
    return 0;
}

quint64 AlphaCPU::readDetailedPerformanceCounter(quint64 counterNum) const
{
    // Return detailed performance counter with additional metadata
    if (counterNum < 8)
    {
        return m_performanceCounters[counterNum] | (counterNum << 56); // Embed counter ID
    }
    return 0;
}

void AlphaCPU::writePerformanceCounter(quint64 counterNum, quint64 value)
{
    if (counterNum < 8)
    {
        m_performanceCounters[counterNum] = value;
    }
}

bool AlphaCPU::writeMemoryWithFaultHandling(quint64 address, quint64 value, const PALInstruction &instr)
{
    if (!m_memorySystem)
        return false;

    try
    {
        // Attempt to write memory through the memory system
        bool success = m_memorySystem->writeVirtualMemory(m_cpuId, address, value, 8, m_pc);

        if (!success)
        {
            // Check what type of fault occurred
            quint64 physAddr;
            bool translationSuccess =
                m_memorySystem->translateAddress(m_cpuId, address, physAddr, getCurrentASN(), true, false);

            if (!translationSuccess)
            {
                // Translation fault
                raiseMemoryException(address, true, true, false);
            }
            else
            {
                // Access violation or other fault
                raiseMemoryException(address, true, false, false);
            }
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        // Handle any exceptions that might occur during memory access
        handleMemoryFault(address, true);
        return false;
    }
}
void AlphaCPU::triggerSoftwareInterrupt(quint64 vector)
{
    // Set software interrupt bit
    if (m_iprs)
    {
        quint64 sisr = m_iprs->read(IPRNumbers::IPR_SISR);
        sisr |= (1ULL << (vector & 0x3F));
        m_iprs->write(IPRNumbers::IPR_SISR, sisr);
    }

    // Signal pending interrupt
    interruptPending.storeRelaxed(true);
}

void AlphaCPU::debugIPRMappings() const
{
    if (!m_iprs)
        return;

    qDebug() << "=== IPR Register Mappings ===";
    qDebug() << "EXC_SUM:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_EXC_SUM);
    qDebug() << "EXC_PC:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_EXC_PC);
    qDebug() << "EXC_ADDR:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_EXC_ADDR);
    qDebug() << "PAL_BASE:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_PAL_BASE);
    qDebug() << "PS:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_PS);
    qDebug() << "ASN:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_ASN);
    qDebug() << "VPTB:" << Qt::hex << m_iprs->read(IPRNumbers::IPR_VPTB);

    // Check exception entry points
    for (int i = 0; i < 8; ++i)
    {
        quint64 entry = m_iprs->read(static_cast<IPRNumbers>(static_cast<int>(IPRNumbers::IPR_ENTRY_0) + i));
        qDebug() << QString("ENTRY_%1:").arg(i) << Qt::hex << entry;
    }
}

void AlphaCPU::deliverInterrupt(int vector)
{
    if (!m_iprs)
        return;

    // Save current state
    quint64 currentPS = m_iprs->read(IPRNumbers::IPR_PS);
    m_iprs->write(IPRNumbers::IPR_EXC_PS, currentPS);
    m_iprs->write(IPRNumbers::IPR_EXC_PC, m_pc);

    // Set interrupt state
    quint64 newPS = currentPS;
    newPS &= ~0x1;                  // Clear interrupt enable
    newPS |= ((vector & 0x7) << 3); // Set new IPL
    m_iprs->write(IPRNumbers::IPR_PS, newPS);

    // Vector to interrupt handler
    quint64 scbb = m_iprs->read(IPRNumbers::IPR_SCBB);     // System Control Block Base
    quint64 handlerAddr = scbb + (vector * 16); // Each vector is 16 bytes

    setPC(handlerAddr);

    m_inExceptionHandler = true;

    DEBUG_LOG(QString("CPU%1: Interrupt delivered: vector=%2, handler=0x%3")
                  .arg(m_cpuId)
                  .arg(vector)
                  .arg(handlerAddr, 0, 16));
}

void AlphaCPU::deliverPendingInterrupt()
{
    if (pendingInterrupts.isEmpty())
        return;

    // Find highest priority pending interrupt
    int highestVector = 0;
    for (int vector : pendingInterrupts)
    {
        if (vector > highestVector && canTakeInterrupt(vector))
        {
            highestVector = vector;
        }
    }

    if (highestVector > 0)
    {
        // Remove from pending list
        pendingInterrupts.remove(highestVector);

        // Clear corresponding IPIR bit
        if (m_iprs)
        {
            quint64 ipir = m_iprs->read(IPRNumbers::IPR_IPIR);
            ipir &= ~(1ULL << highestVector);
            m_iprs->write(IPRNumbers::IPR_IPIR, ipir);
        }

        // Deliver the interrupt
        deliverInterrupt(highestVector);

        DEBUG_LOG(QString("CPU%1: Delivered interrupt vector %2").arg(m_cpuId).arg(highestVector));
    }

    // Update interrupt pending flag
    interruptPending.storeRelaxed(!pendingInterrupts.isEmpty());
}

void AlphaCPU::disableInterrupts()
{
    m_interruptEnable = false;
    if (m_iprs)
    {
        quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
        ps &= ~PS_INTERRUPT_ENABLE; // Use your constant
        m_iprs->write(IPRNumbers::IPR_PS, ps);
    }
}
void AlphaCPU::enableInterrupts()
{
    m_interruptEnable = true;
    if (m_iprs)
    {
        quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
        ps |= PS_INTERRUPT_ENABLE; // Use your constant
        m_iprs->write(IPRNumbers::IPR_PS, ps);
    }
}

bool AlphaCPU::readPhysicalMemory(quint64 physAddr, quint64 &value)
{
    if (!m_memorySystem)
        return false;
    return m_memorySystem->readPhysicalMemory(physAddr, value, 8);
}

bool AlphaCPU::writePhysicalMemory(quint64 physAddr, quint64 value)
{
    if (!m_memorySystem)
        return false;
    return m_memorySystem->writePhysicalMemory(physAddr, value, 8);
}

bool AlphaCPU::readMemory(quint64 address, quint8 *buffer, size_t size)
{
    if (!m_memorySystem)
        return false;

    // Use the existing memory system
    quint64 value;
    bool success = m_memorySystem->readVirtualMemory(m_cpuId, address, value, size, m_pc);
    if (success && buffer)
    {
        memcpy(buffer, &value, size);
    }
    return success;
}

quint64 AlphaCPU::getIntegerRegister(quint8 regNum) const
{
    if (regNum >= 32)
        return 0;

    if (m_registers)
    {
        return m_registers->readIntReg(regNum);
    }
    return 0;
}

AlphaInstructionCache *AlphaCPU::getInstructionCache() const { return m_instructionCache.data(); }

double AlphaCPU::getL1HitRate() const
{
    if (m_level1DataCache)
    {
        return m_level1DataCache->getStatistics().getHitRate();
    }
    return 0.0;
}

double AlphaCPU::getL2HitRate() const
{
    if (m_level2DataCache)
    {
        return m_level2DataCache->getStatistics().getHitRate();
    }
    return 0.0;
}

quint64 AlphaCPU::getPALBase() const { return m_palCodeBase; }

quint64 AlphaCPU::getPC() const { return m_pc; }

void AlphaCPU::setIntegerRegister(quint8 regNum, quint64 value)
{
    if (regNum >= 32)
        return;

    if (m_registers)
    {
        m_registers->writeIntReg(regNum, value);
    }
}

// Enhanced IPR read/write with proper exception register support
quint64 AlphaCPU::readIPR( QString &iprName)
{
    if (!m_iprs)
        return 0;

    // Convert string name to your IPR enum and read from IprBank
    if (iprName == "EXC_SUM")
        return m_iprs->read(IPRNumbers::IPR_EXC_SUM);
    if (iprName == "EXC_PC")
        return m_iprs->read(IPRNumbers::IPR_EXC_PC);
    if (iprName == "EXC_PS")
        return m_iprs->read(IPRNumbers::IPR_EXC_PS);
    if (iprName == "EXC_ADDR")
        return m_iprs->read(IPRNumbers::IPR_EXC_ADDR);
    if (iprName == "EXC_MASK")
        return m_iprs->read(IPRNumbers::IPR_EXC_MASK);
    if (iprName == "ASTEN")
        return m_iprs->read(IPRNumbers::IPR_ASTEN);
    if (iprName == "ASTSR")
        return m_iprs->read(IPRNumbers::IPR_ASTSR);
    if (iprName == "VPTB")
        return m_iprs->read(IPRNumbers::IPR_VPTB);
    if (iprName == "FEN")
        return m_iprs->read(IPRNumbers::IPR_FEN);
    if (iprName == "IPL")
        return m_iprs->read(IPRNumbers::IPR_IPLR); // Map to your IPLR
    if (iprName == "PS")
        return m_iprs->read(IPRNumbers::IPR_PS);
    if (iprName == "USP")
        return m_iprs->read(IPRNumbers::IPR_USP);
    if (iprName == "ESP")
        return m_iprs->read(IPRNumbers::IPR_ESP);
    if (iprName == "SSP")
        return m_iprs->read(IPRNumbers::IPR_SSP);
    if (iprName == "KSP")
        return m_iprs->read(IPRNumbers::IPR_KSP);
    if (iprName == "ASN")
        return m_iprs->read(IPRNumbers::IPR_ASN);
    if (iprName == "IPIR")
        return m_iprs->read(IPRNumbers::IPR_IPIR);
    if (iprName == "PAL_TEMP")
        return m_iprs->read(IPRNumbers::IPR_PAL_TEMP);
    if (iprName == "MCES")
        return m_iprs->read(IPRNumbers::IPR_MCES);
    if (iprName == "PCBB")
        return m_iprs->read(IPRNumbers::IPR_PCBB);
    if (iprName == "PRBR")
        return m_iprs->read(IPRNumbers::IPR_PRBR);
    if (iprName == "PTBR")
        return m_iprs->read(IPRNumbers::IPR_PTBR);
    if (iprName == "SCBB")
        return m_iprs->read(IPRNumbers::IPR_SCBB);
    if (iprName == "SIRR")
        return m_iprs->read(IPRNumbers::IPR_SIRR);
    if (iprName == "SISR")
        return m_iprs->read(IPRNumbers::IPR_SISR);
    if (iprName == "IRQL")
        return m_iprs->read(IPRNumbers::IPR_IRQL);
    if (iprName == "UNQ")
        return m_iprs->read(IPRNumbers::IPR_UNQ);
    if (iprName == "THREAD")
        return m_iprs->read(IPRNumbers::IPR_THREAD);
    if (iprName == "PAL_MODE")
        return m_iprs->read(IPRNumbers::IPR_PAL_MODE);
    if (iprName == "PAL_BASE")
        return m_iprs->read(IPRNumbers::IPR_PAL_BASE);
    if (iprName == "RESTART_VECTOR")
        return m_iprs->read(IPRNumbers::IPR_RESTART_VECTOR);
    if (iprName == "DEBUGGER_VECTOR")
        return m_iprs->read(IPRNumbers::IPR_DEBUGGER_VECTOR);
    if (iprName == "PROCESS")
        return m_iprs->read(IPRNumbers::IPR_PROCESS);
    if (iprName == "TBCHK")
        return m_iprs->read(IPRNumbers::IPR_TBCHK);

    // Performance monitor registers
    if (iprName.startsWith("PERFMON_"))
    {
        bool ok;
        int counterNum = iprName.mid(8).toInt(&ok);
        if (ok && counterNum >= 0 && counterNum < 8)
        {
            return m_iprs->read(static_cast<Ipr>(static_cast<int>(IPRNumbers::IPR_PERFMON_0) + counterNum));
        }
    }

    // Entry point registers
    if (iprName.startsWith("ENTRY_"))
    {
        bool ok;
        int entryNum = iprName.mid(6).toInt(&ok);
        if (ok && entryNum >= 0 && entryNum < 8)
        {
            return m_iprs->read(static_cast<Ipr>(static_cast<int>(IPRNumbers::IPR_ENTRY_0) + entryNum));
        }
    }

    return 0;
}


bool AlphaCPU::writeIPR(const QString &iprName, quint64 value)
{
    if (!m_iprs)
        return false;

    // Convert string name to your IPR enum and write to IprBank
    if (iprName == "EXC_SUM")
    {
        m_iprs->write(IPRNumbers::IPR_EXC_SUM, value);
        return true;
    }
    if (iprName == "EXC_PC")
    {
        m_iprs->write(IPRNumbers::IPR_EXC_PC, value);
        return true;
    }
    if (iprName == "EXC_PS")
    {
        m_iprs->write(IPRNumbers::IPR_EXC_PS, value);
        return true;
    }
    if (iprName == "EXC_ADDR")
    {
        m_iprs->write(IPRNumbers::IPR_EXC_ADDR, value);
        return true;
    }
    if (iprName == "EXC_MASK")
    {
        m_iprs->write(IPRNumbers::IPR_EXC_MASK, value);
        return true;
    }
    if (iprName == "ASTEN")
    {
        m_iprs->write(IPRNumbers::IPR_ASTEN, value);
        return true;
    }
    if (iprName == "ASTSR")
    {
        m_iprs->write(IPRNumbers::IPR_ASTSR, value);
        return true;
    }
    if (iprName == "VPTB")
    {
        m_iprs->write(IPRNumbers::IPR_VPTB, value);
        return true;
    }
    if (iprName == "FEN")
    {
        m_iprs->write(IPRNumbers::IPR_FEN, value);
        return true;
    }
    if (iprName == "IPL")
    {
        m_iprs->write(IPRNumbers::IPR_IPLR, value);
        return true;
    } // Map to your IPLR
    if (iprName == "PS")
    {
        m_iprs->write(IPRNumbers::IPR_PS, value);
        return true;
    }
    if (iprName == "USP")
    {
        m_iprs->write(IPRNumbers::IPR_USP, value);
        return true;
    }
    if (iprName == "ESP")
    {
        m_iprs->write(IPRNumbers::IPR_ESP, value);
        return true;
    }
    if (iprName == "SSP")
    {
        m_iprs->write(IPRNumbers::IPR_SSP, value);
        return true;
    }
    if (iprName == "KSP")
    {
        m_iprs->write(IPRNumbers::IPR_KSP, value);
        return true;
    }
    if (iprName == "ASN")
    {
        m_iprs->write(IPRNumbers::IPR_ASN, value);
        return true;
    }
    if (iprName == "IPIR")
    {
        m_iprs->write(IPRNumbers::IPR_IPIR, value);
        return true;
    }
    if (iprName == "PAL_TEMP")
    {
        m_iprs->write(IPRNumbers::IPR_PAL_TEMP, value);
        return true;
    }
    if (iprName == "MCES")
    {
        m_iprs->write(IPRNumbers::IPR_MCES, value);
        return true;
    }
    if (iprName == "PCBB")
    {
        m_iprs->write(IPRNumbers::IPR_PCBB, value);
        return true;
    }
    if (iprName == "PRBR")
    {
        m_iprs->write(IPRNumbers::IPR_PRBR, value);
        return true;
    }
    if (iprName == "PTBR")
    {
        m_iprs->write(IPRNumbers::IPR_PTBR, value);
        return true;
    }
    if (iprName == "SCBB")
    {
        m_iprs->write(IPRNumbers::IPR_SCBB, value);
        return true;
    }
    if (iprName == "SIRR")
    {
        m_iprs->write(IPRNumbers::IPR_SIRR, value);
        return true;
    }
    if (iprName == "SISR")
    {
        m_iprs->write(IPRNumbers::IPR_SISR, value);
        return true;
    }
    if (iprName == "IRQL")
    {
        m_iprs->write(IPRNumbers::IPR_IRQL, value);
        return true;
    }
    if (iprName == "UNQ")
    {
        m_iprs->write(IPRNumbers::IPR_UNQ, value);
        return true;
    }
    if (iprName == "THREAD")
    {
        m_iprs->write(IPRNumbers::IPR_THREAD, value);
        return true;
    }
    if (iprName == "PAL_MODE")
    {
        m_iprs->write(IPRNumbers::IPR_PAL_MODE, value);
        return true;
    }
    if (iprName == "PAL_BASE")
    {
        m_iprs->write(IPRNumbers::IPR_PAL_BASE, value);
        return true;
    }
    if (iprName == "RESTART_VECTOR")
    {
        m_iprs->write(IPRNumbers::IPR_RESTART_VECTOR, value);
        return true;
    }
    if (iprName == "DEBUGGER_VECTOR")
    {
        m_iprs->write(IPRNumbers::IPR_DEBUGGER_VECTOR, value);
        return true;
    }
    if (iprName == "PROCESS")
    {
        m_iprs->write(IPRNumbers::IPR_PROCESS, value);
        return true;
    }
    if (iprName == "TBCHK")
    {
        m_iprs->write(IPRNumbers::IPR_TBCHK, value);
        return true;
    }

    // Performance monitor registers
    if (iprName.startsWith("PERFMON_"))
    {
        bool ok;
        int counterNum = iprName.mid(8).toInt(&ok);
        if (ok && counterNum >= 0 && counterNum < 8)
        {
            m_iprs->write(static_cast<Ipr>(static_cast<int>(IPRNumbers::IPR_PERFMON_0) + counterNum), value);
            return true;
        }
    }

    // Entry point registers
    if (iprName.startsWith("ENTRY_"))
    {
        bool ok;
        int entryNum = iprName.mid(6).toInt(&ok);
        if (ok && entryNum >= 0 && entryNum < 8)
        {
            m_iprs->write(static_cast<Ipr>(static_cast<int>(IPRNumbers::IPR_ENTRY_0) + entryNum), value);
            return true;
        }
    }

    return false;
}


void AlphaCPU::setPrivilegeMode(int mode)
{
    if (m_iprs)
    {
        quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
        ps &= ~PS_CURRENT_MODE; // Clear current mode bits

        switch (mode)
        {
        case 0:
            ps |= PS_MODE_KERNEL;
            m_currentMode = enumProcessorMode::KERNEL;
            break;
        case 1:
            ps |= PS_MODE_EXECUTIVE;
            m_currentMode = enumProcessorMode::EXECUTIVE;
            break;
        case 2:
            ps |= PS_MODE_SUPERVISOR;
            m_currentMode = enumProcessorMode::SUPERVISOR;
            break;
        case 3:
            ps |= PS_MODE_USER;
            m_currentMode = enumProcessorMode::USER;
            break;
        default:
            ps |= PS_MODE_KERNEL; // Default to kernel
            break;
        }

        m_iprs->write(IPRNumbers::IPR_PS, ps);
    }
}
void AlphaCPU::raiseException(ExceptionType exceptionType, quint64 faultingPC)
{
    if (!m_iprs)
        return;

    // Convert ExceptionType to exception code
    quint64 exceptionCode = 0;
    switch (exceptionType)
    {
    case ExceptionType::MEMORY_MANAGEMENT: // Use your enum value
        exceptionCode = 0x0004;            // Memory management exception
        break;
    case ExceptionType::PAGE_FAULT:
        exceptionCode = 0x0004; // Memory management exception
        break;
    case ExceptionType::MACHINE_CHECK:
        exceptionCode = 0x0008; // Machine check exception
        break;
    case ExceptionType::ARITHMETIC: // Use your enum value
        exceptionCode = 0x0001;     // Arithmetic exception
        break;
    case ExceptionType::INTERRUPT:
        exceptionCode = 0x0002; // Interrupt exception
        break;
    case ExceptionType::ILLEGAL_INSTRUCTION:
        exceptionCode = 0x0003; // Illegal instruction
        break;
    case ExceptionType::PRIVILEGE_VIOLATION:
        exceptionCode = 0x0005; // Privilege violation
        break;
    case ExceptionType::BREAKPOINT:
        exceptionCode = 0x0006; // Breakpoint
        break;
    case ExceptionType::SYSTEM_CALL:
        exceptionCode = 0x0007; // System call
        break;
    default:
        exceptionCode = 0x0000; // System exception
        break;
    }

    // Set exception state
    m_hasException = true;

    // Store exception information - FIXED: Use correct IPR references
    m_iprs->write(IPRNumbers::IPR_EXC_PC, faultingPC);
    quint64 currentPS = m_iprs->read(IPRNumbers::IPR_PS);
    m_iprs->write(IPRNumbers::IPR_EXC_PS, currentPS);

    // Vector to appropriate exception handler
    vectorToExceptionHandler(exceptionCode, faultingPC);

    DEBUG_LOG(QString("CPU%1: Exception raised: type=%2, code=0x%3, PC=0x%4")
                  .arg(m_cpuId)
                  .arg(static_cast<int>(exceptionType))
                  .arg(exceptionCode, 0, 16)
                  .arg(faultingPC, 0, 16));
}

void AlphaCPU::raiseMemoryException(quint64 faultingAddress, bool isWrite, bool isTranslationFault,
                                    bool isAlignmentFault)
{
    if (!m_iprs)
        return;

    // Build exception summary using your EXC_SUM constants
    quint64 excSum = EXC_SUM_ACCESS_VIOLATION;

    if (!isWrite)
    {
        excSum |= EXC_SUM_FAULT_ON_READ;
    }

    if (isTranslationFault)
    {
        excSum |= EXC_SUM_TRANS_NOT_VALID;
    }

    if (isAlignmentFault)
    {
        excSum |= EXC_SUM_ALIGNMENT_FAULT;
    }

    // Store exception information in your IPR system
    m_iprs->write(IPRNumbers::IPR_EXC_ADDR, faultingAddress);
    m_iprs->write(IPRNumbers::IPR_EXC_SUM, excSum);
    m_iprs->write(IPRNumbers::IPR_EXC_PC, m_pc);

    // Store current PS
    quint64 currentPS = m_iprs->read(IPRNumbers::IPR_PS);
    m_iprs->write(IPRNumbers::IPR_EXC_PS, currentPS);

    m_hasException = true;

    DEBUG_LOG(QString("CPU%1: Memory Exception - Addr=0x%2, Write=%3, TransFault=%4, AlignFault=%5, Sum=0x%6")
                  .arg(m_cpuId)
                  .arg(faultingAddress, 0, 16)
                  .arg(isWrite)
                  .arg(isTranslationFault)
                  .arg(isAlignmentFault)
                  .arg(excSum, 0, 16));

    // Vector to memory management exception handler
    vectorToExceptionHandler(0x0004, m_pc);

    // Optionally, you could also throw your MemoryAccessException here
    // for consistency with the rest of your exception system:
    /*
    MemoryFaultType faultType = isAlignmentFault ? MemoryFaultType::ALIGNMENT_FAULT :
                                                    MemoryFaultType::ACCESS_VIOLATION;
    throw MemoryAccessException(faultType, faultingAddress, 8, isWrite, m_pc);
    */
}

void AlphaCPU::halt()
{
    m_isRunning = false;
    // TODO: Implement proper halt state
    DEBUG_LOG(QString("CPU%1: Halted").arg(m_cpuId));
}

void AlphaCPU::setPC(quint64 newPC)
{
    m_pc = newPC;
    m_currentPC = newPC;
}

quint64 AlphaCPU::getCurrentContext() const
{
    // Return current ASN as context identifier
    return m_iprs ? m_iprs->read(IPRNumbers::IPR_ASN) : 0;
}

void AlphaCPU::saveContext(quint64 contextId)
{
    // TODO: Save current processor state to context area
    DEBUG_LOG(QString("CPU%1: Saving context %2").arg(m_cpuId).arg(contextId));
}

bool AlphaCPU::isMMUEnabled() const
{
    // Check if MMU is enabled via processor status register
    if (m_iprs)
    {
        quint64 ps = m_iprs->read(IPRNumbers::IPR_PS);
        // Assume MMU is enabled if not in PAL mode (bit 1)
        return (ps & PS_PAL_MODE) == 0; // MMU enabled when PAL mode bit is clear
    }
    return true; // Default to MMU enabled
}

bool AlphaCPU::isValidMemoryAddress(quint64 address) const
{
    // Alpha architecture uses 64-bit virtual addresses but not all bits are valid

    // Check for canonical address format (Alpha uses 43-bit virtual addresses)
    // Valid addresses have bits 63:43 all zeros or all ones
    quint64 highBits = (address >> 43) & 0x1FFFFF; // Extract bits 63:43
    if (highBits != 0 && highBits != 0x1FFFFF)
    {
        DEBUG_LOG("AlphaCPU: Invalid address format 0x%016llX - non-canonical", address);
        return false;
    }

    // Check alignment requirements based on current processor mode
    bool isKernel = (m_currentMode == enumProcessorMode::KERNEL);

    // Kernel space addresses (high bit set)
    if (address & 0x8000000000000000ULL)
    {
        if (!isKernel)
        {
            DEBUG_LOG("AlphaCPU: User mode access to kernel address 0x%016llX denied", address);
            return false;
        }

        // PAL code region (highest addresses)
        if (address >= 0xFFFFFFFF80000000ULL)
        {
            // Only allow PAL code access in PAL mode
            quint64 ps = m_iprs ? m_iprs->read(IPRNumbers::IPR_PS) : 0;
            if ((ps & PS_PAL_MODE) == 0)
            {
                DEBUG_LOG("AlphaCPU: Non-PAL access to PAL region 0x%016llX denied", address);
                return false;
            }
        }
    }

    // Check against memory system bounds if available
    if (m_memorySystem)
    {
        // Query memory system for valid address ranges
        if (!m_memorySystem->isValidVirtualAddress(address))
        {
            DEBUG_LOG("AlphaCPU: Address 0x%016llX outside valid memory ranges", address);
            return false;
        }
    }

    // Check for reserved/unmapped regions
    // I/O space (typically 0x8000000000000000 to 0x87FFFFFFFFFFFFFF on some Alpha systems)
    if ((address >= 0x8000000000000000ULL) && (address <= 0x87FFFFFFFFFFFFFFULL))
    {
        // I/O space access requires proper privilege checking
        if (!isKernel)
        {
            DEBUG_LOG("AlphaCPU: User mode I/O space access 0x%016llX denied", address);
            return false;
        }
    }

    // Memory-mapped register regions (platform specific)
    // This would typically be handled by the memory system, but basic checks here

    // Check for stack overflow protection
    if (m_memorySystem)
    {
        // Get current stack pointer based on mode
        quint64 currentSP = 0;
        if (m_iprs)
        {
            switch (m_currentMode)
            {
            case enumProcessorMode::KERNEL:
                currentSP = m_iprs->read(IPRNumbers::IPR_KSP);
                break;
            case enumProcessorMode::EXECUTIVE:
                currentSP = m_iprs->read(IPRNumbers::IPR_ESP);
                break;
            case enumProcessorMode::SUPERVISOR:
                currentSP = m_iprs->read(IPRNumbers::IPR_SSP);
                break;
            case enumProcessorMode::USER:
                currentSP = m_iprs->read(IPRNumbers::IPR_USP);
                break;
            }
        }

        // Basic stack bounds checking (stack grows down on Alpha)
        if (currentSP > 0)
        {
            // Check if address is suspiciously far below stack pointer (potential underflow)
            if (address < currentSP && (currentSP - address) > STACK_MAX_SIZE)
            {
                DEBUG_LOG("AlphaCPU: Potential stack underflow at 0x%016llX (SP=0x%016llX)", address, currentSP);
                return false;
            }
        }
    }

    // Address passed all validation checks
    return true;
}
void AlphaCPU::loadContext(quint64 contextId)
{
    // TODO: Load processor state from context area
    if (m_iprs)
    {
        m_iprs->write(IPRNumbers::IPR_ASN, contextId & 0xFF);
    }
    DEBUG_LOG(QString("CPU%1: Loading context %2").arg(m_cpuId).arg(contextId));
}

void AlphaCPU::notifySystemStateChange()
{
    // TODO: Notify other system components of state change
    emit sigStateChanged();
}

void AlphaCPU::flushTLBCache()
{
    // Delegate TLB cache flushing to memory system
    if (m_memorySystem)
    {
        m_memorySystem->flushCPUTLBCache(m_cpuId);
    }

    DEBUG_LOG(QString("CPU%1: TLB cache flush requested").arg(m_cpuId));
}

void AlphaCPU::executeMemoryBarrier(int type)
{
    // Execute local memory barrier
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // For SMP systems, coordinate with other CPUs
    if (type >= 2)
    { // Full barrier
        memoryBarrierSMP(type);
    }
}


/**
 * @brief Emulate LDQ_L (Load Quadword Locked)
 * Opcode: 2B (LDQ_L Ra, disp(Rb))
 */
void AlphaCPU::executeLDQ_L(quint8 ra, qint16 displacement, quint8 rb)
{
    quint64 address = m_registers[rb] + displacement;
    quint64 value;

    // Implicit acquire barrier before load-locked
    m_memorySystem->executeLoadLockedBarrier(m_cpuId);

    if (m_memorySystem->loadLocked(m_cpuId, address, value, 8, getPC()))
    {
        m_registers[ra] = value;
    }
    else
    {
        // Handle load-locked failure
        raiseException(ExceptionType::MEMORY_MANAGEMENT,getPC());
        return;
    }

    incrementPC();
}
void AlphaCPU::executeMB()
{
    // Alpha hardware requires full memory barrier
    m_memorySystem->executeAlphaMB(m_cpuId);

    // Update instruction counter
    incrementPC();
}

void AlphaCPU::enterPALMode(quint32 function)
{
    if (!m_iprs)
        return;

    // Save current state
    quint64 currentPS = m_iprs->read(IPRNumbers::IPR_PS);
    m_iprs->write(IPRNumbers::IPR_EXC_PS, currentPS);
    m_iprs->write(IPRNumbers::IPR_EXC_PC, m_pc);

    // Set PAL mode and disable interrupts using your constants
    quint64 newPS = currentPS;
    newPS |= PS_PAL_MODE;          // Set bit 1 (PAL mode)
    newPS &= ~PS_INTERRUPT_ENABLE; // Clear bit 0 (disable interrupts)
    newPS &= ~PS_CURRENT_MODE;     // Clear current mode bits
    newPS |= PS_MODE_KERNEL;       // Set kernel mode
    m_iprs->write(IPRNumbers::IPR_PS, newPS);

    // Calculate and jump to PAL entry point
    quint64 palBase = m_iprs->read(IPRNumbers::IPR_PAL_BASE);
    quint64 palEntry = palBase + (function * 64);
    setPC(palEntry);

    DEBUG_LOG(QString("CPU%1: Entered PAL mode, function=0x%2, entry=0x%3")
                  .arg(m_cpuId)
                  .arg(function, 0, 16)
                  .arg(palEntry, 0, 16));
}
/**
 * @brief Emulate CALL_PAL instruction
 * Opcode: 00 (CALL_PAL function)
 */
void AlphaCPU::executeCALL_PAL(quint32 function)
{
    // PAL code requires strict memory ordering
    m_memorySystem->executePALBarrier(m_cpuId);

    // Save state and transfer to PAL code
    enterPALMode(function);
}

/**
 * @brief Emulate STQ_C (Store Quadword Conditional)
 * Opcode: 2F (STQ_C Ra, disp(Rb))
 */
void AlphaCPU::executeSTQ_C(quint8 ra, qint16 displacement, quint8 rb)
{
    // Use RegisterBank methods for proper R31 handling
    quint64 baseAddr = m_registers ? m_registers->readIntReg(rb) : 0;
    quint64 address = baseAddr + static_cast<qint64>(displacement);
    quint64 value = m_registers ? m_registers->readIntReg(ra) : 0;

    // Proper memory barrier
    m_memorySystem->executeStoreConditionalBarrier(m_cpuId);

    // Perform store-conditional
    if (m_memorySystem->storeConditional(m_cpuId, address, value, 8, getPC()))
    {
        //  Use RegisterBank method for proper handling
        if (m_registers)
        {
            m_registers->writeIntReg(ra, 1); // Success
        }
    }
    else
    {
        //  Use RegisterBank method for proper handling
        if (m_registers)
        {
            m_registers->writeIntReg(ra, 0); // Failure
        }
    }

    incrementPC();
}
/**
 * @brief Emulate Alpha WMB instruction
 * Opcode: 18.4400 (WMB)
 */
void AlphaCPU::executeWMB()
{
    // Write memory barrier - Qt atomics sufficient
    m_memorySystem->executeAlphaWMB(m_cpuId);

    incrementPC();
}

void AlphaCPU::dispatchException(ExceptionType exceptionType, quint64 faultAddr)
{
    // This is a more detailed exception dispatcher
    switch (exceptionType)
    {
    case ExceptionType::MEMORY_MANAGEMENT:
        raiseMemoryException(faultAddr, false, true, false);
        break;
    case ExceptionType::PAGE_FAULT:
        raiseMemoryException(faultAddr, false, true, false);
        break;
    case ExceptionType::MACHINE_CHECK:
        // Handle machine check - could be hardware failure
        if (m_iprs)
        {
            m_iprs->write(IPRNumbers::IPR_MCES, 0x1); // Set machine check flag
            m_iprs->write(IPRNumbers::IPR_EXC_ADDR, faultAddr);
        }
        raiseException(ExceptionType::MACHINE_CHECK, m_pc);
        break;
    default:
        raiseException(exceptionType, m_pc);
        break;
    }
}


quint64 AlphaCPU::divDFormat(quint64 faVal, quint64 rbVal)
{
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a / b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}


quint64 AlphaCPU::divSFormat(quint64 faVal, quint64 rbVal)
{
    quint32 aBits = static_cast<quint32>(faVal);
    quint32 bBits = static_cast<quint32>(rbVal);
    float a, b;
    std::memcpy(&a, &aBits, sizeof(float));
    std::memcpy(&b, &bBits, sizeof(float));
    float result = a / b;
    quint32 resultBits;
    std::memcpy(&resultBits, &result, sizeof(float));
    return static_cast<quint64>(resultBits);
}

quint64 AlphaCPU::divTFormat(quint64 faVal, quint64 rbVal)
{
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a / b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

void AlphaCPU::drainAborts()
{
    // TODO: Wait for all pending aborts to complete
    // This is a no-op in simulation but would wait for hardware in real system
    DEBUG_LOG(QString("CPU%1: Aborts drained").arg(m_cpuId));
}

// Utility method to check exception status
bool AlphaCPU::hasException() const { return m_hasException; }

// Clear exception state
void AlphaCPU::clearException()
{
    m_hasException = false;
    if (m_iprs)
    {
        m_iprs->write(IPRNumbers::IPR_EXC_SUM, 0);
    }
}

quint64 AlphaCPU::compareFFormat(quint64 faVal, quint64 rbVal, FPCompareType typ_)
{
    // Compare two VAX F_floating values
    // Return comparison result (typically 0 or 1)
    // This is a placeholder - implement proper VAX F_floating comparison
    //TODO
    return 0;
}

quint64 AlphaCPU::compareGFormat(quint64 faVal, quint64 rbVal)
{
    // Compare two VAX G_floating values
    // This is a placeholder - implement proper VAX G_floating comparison

    //TODO
    return 0;
}

quint64 AlphaCPU::compareTFormat(quint64 faValue, quint64 fbValue, FPCompareType c_Type)
{
    // Compare two IEEE T_floating values
    double a, b;
    std::memcpy(&a, &faValue, sizeof(double));
    std::memcpy(&b, &fbValue, sizeof(double));

    switch (c_Type)
    {
    case FPCompareType::EQUAL:
        return (a == b) ? 1 : 0;
    case FPCompareType::LESS_THAN:
        return (a < b) ? 1 : 0;
    case FPCompareType::LESS_EQUAL:
        return (a <= b) ? 1 : 0;
    case FPCompareType::UNORDERED:
        return (std::isnan(a) || std::isnan(b)) ? 1 : 0;
    default:
        return 0;
    }
}

quint64 AlphaCPU::compareTFormatSignaling(quint64 faValue, quint64 fbValue, FPCompareType c_Type)
{
    // Signaling comparison - raises exception on NaN
    double a, b;
    std::memcpy(&a, &faValue, sizeof(double));
    std::memcpy(&b, &fbValue, sizeof(double));

    if (std::isnan(a) || std::isnan(b))
    {
        triggerFloatingPointException(FPTrapType::FP_INVALID_OPERATION);
        return 0;
    }

    return compareTFormat(faValue, fbValue, c_Type);
}

void AlphaCPU::connectToL3SharedCache(UnifiedDataCache *l3Cache)
{
    if (!l3Cache)
    {
        WARN_LOG("AlphaCPU: Cannot connect to null L3 cache on CPU%1", m_cpuId);
        return;
    }

    // Connect L2 to L3
    if (m_level2DataCache)
    {
        m_level2DataCache->setNextLevel(l3Cache);
        l3Cache->setPrevLevel(m_level2DataCache.data());
        DEBUG_LOG("AlphaCPU: Connected L2->L3 for CPU%1", m_cpuId);
    }

    // Connect instruction cache to L3 if no L2 connection
    if (m_instructionCache && m_instructionCache->getUnifiedCache())
    {
        m_instructionCache->getUnifiedCache()->setNextLevel(l3Cache);
        DEBUG_LOG("AlphaCPU: Connected I-cache->L3 for CPU%1", m_cpuId);
    }

    DEBUG_LOG("AlphaCPU: L3 shared cache connection complete for CPU%1", m_cpuId);
}

ExceptionType AlphaCPU::convertArithmeticException(ExceptionTypeArithmetic type)
{
    // Convert from ExceptionTypeArithmetic to general ExceptionType
    switch (type)
    {
    case ExceptionTypeArithmetic::INTEGER_OVERFLOW:
        return ExceptionType::ARITHMETIC;
    case ExceptionTypeArithmetic::DIVIDE_BY_ZERO:
        return ExceptionType::ARITHMETIC;
    case ExceptionTypeArithmetic::FLOATING_POINT_EXCEPTION:
        return ExceptionType::ARITHMETIC;
    default:
        return ExceptionType::ARITHMETIC;
    }
}

quint64 AlphaCPU::convertDToG(quint64 val_)
{
    // Convert VAX D_floating to G_floating
    // This is a placeholder - implement proper VAX conversion
    //TODO
    return val_;
}

quint64 AlphaCPU::convertGToD(quint64 val_)
{
    // Convert VAX G_floating to D_floating
    // This is a placeholder - implement proper VAX conversion
    // TODO
    return val_;
}
quint64 AlphaCPU::convertFromVaxD(quint64 val_)
{
    // Convert from VAX D_floating to IEEE
    // This is a placeholder - implement proper conversion
    // TODO
    return val_;
}
quint64 AlphaCPU::convertFromVaxG(quint64 val_)
{
    // Convert from VAX G_floating to IEEE
    // This is a placeholder - implement proper conversion
    // TODO
    return val_;
}

quint64 AlphaCPU::convertFToOther(quint64 val_)
{
    // Convert VAX F format to IEEE single precision
    // This is a complex conversion involving different exponent bias and format

    if (val_ == 0)
        return 0;

    // VAX F: sign(1) + exponent(8, bias 128) + fraction(23)
    // IEEE: sign(1) + exponent(8, bias 127) + fraction(23)

    quint32 vaxF = static_cast<quint32>(val_);

    // Extract components
    quint32 sign = (vaxF >> 15) & 0x1;
    quint32 exponent = (vaxF >> 7) & 0xFF;
    quint32 fraction = vaxF & 0x7F;

    // Convert exponent bias (VAX 128 -> IEEE 127)
    if (exponent != 0)
    {
        qint32 ieeeExp = static_cast<qint32>(exponent) - 128 + 127;
        if (ieeeExp <= 0)
        {
            // Underflow
            return (sign << 31); // Return signed zero
        }
        if (ieeeExp >= 255)
        {
            // Overflow
            return (sign << 31) | 0x7F800000; // Return signed infinity
        }
        exponent = static_cast<quint32>(ieeeExp);
    }

    // Reconstruct IEEE format
    quint32 ieee = (sign << 31) | (exponent << 23) | fraction;

    return static_cast<quint64>(ieee);
}

quint64 AlphaCPU::convertGToQuad(quint64 val_)
{
    // Convert VAX G format to 64-bit integer
    double vaxG = static_cast<double>(val_); // Placeholder conversion

    // Check for overflow
    if (vaxG > 9223372036854775807.0 || vaxG < -9223372036854775808.0)
    {
        triggerFloatingPointException(FPTrapType::FP_OVERFLOW);
        return 0;
    }

    return static_cast<quint64>(static_cast<qint64>(vaxG));
}

quint64 AlphaCPU::convertToIeeeT(quint64 val_)
{
    // Convert to IEEE T format (double precision)
    // If input is already IEEE T, return as-is
    // If VAX format, convert appropriately
    //TODO

    return val_; // Placeholder - needs format detection and conversion
}


quint64 AlphaCPU::convertToVaxD(quint64 val_)
{
    // Convert to VAX D format
    // VAX D: sign(1) + exponent(8, bias 128) + fraction(55)

    double ieee = static_cast<double>(val_);

    if (ieee == 0.0)
        return 0;

    // This is a complex conversion requiring bit manipulation
    // Placeholder implementation
    return val_;
}

void AlphaCPU::writeMemoryWriteCoherent(quint64 addr_, quint64 val_)
{
    // Write with cache coherency
    if (m_memorySystem)
    {
        m_memorySystem->writeVirtualMemory(m_cpuId, addr_, val_, 8, m_pc);
    }
}
bool AlphaCPU::writeMemoryWriteThrough(quint64 addr_, quint64 val_)
{
    if (!m_memorySystem)
        return false;

    // Write-through: write to cache and memory simultaneously
    bool success = m_memorySystem->writeVirtualMemory(m_cpuId, addr_, val_, 8, m_pc);

    if (success)
    {
        // Force write to main memory (bypass cache)
        quint64 physAddr;
        if (m_memorySystem->translateAddress(m_cpuId, addr_, physAddr, getCurrentASN(), true, false))
        {
            m_memorySystem->writePhysicalMemory(physAddr, val_, 8);
        }
    }

    return success;
}

bool AlphaCPU::writeMemoryWriteBack(quint64 addr_, quint64 val_)
{
    if (!m_memorySystem)
        return false;

    // Write-back: write to cache only, mark as dirty
    return m_memorySystem->writeVirtualMemory(m_cpuId, addr_, val_, 8, m_pc);
}

void AlphaCPU::writeVirtualMemoryITB(quint64 addr_, quint64 val_)
{
    // Write to instruction TLB entry
    if (m_memorySystem)
    {
        m_memorySystem->updateInstructionTLB(m_cpuId, addr_, val_);
    }

    DEBUG_LOG(QString("CPU%1: ITB write at 0x%2 = 0x%3").arg(m_cpuId).arg(addr_, 0, 16).arg(val_, 0, 16));
}

void AlphaCPU::writeVirtualMemoryDTB(quint64 addr_, quint64 val_)
{
    // Write to data TLB entry
    if (m_memorySystem)
    {
        m_memorySystem->updateDataTLB(m_cpuId, addr_, val_);
    }

    DEBUG_LOG(QString("CPU%1: DTB write at 0x%2 = 0x%3").arg(m_cpuId).arg(addr_, 0, 16).arg(val_, 0, 16));
}

quint64 AlphaCPU::readVirtualMemoryITB(quint64 addr_, quint64 val_)
{
    // Read from instruction TLB
    if (m_memorySystem)
    {
        quint64 result;
        if (m_memorySystem->readInstructionTLB(m_cpuId, addr_, result))
        {
            return result;
        }
    }

    DEBUG_LOG(QString("CPU%1: ITB read miss at 0x%2").arg(m_cpuId).arg(addr_, 0, 16));
    return 0;
}

quint64 AlphaCPU::readVirtualMemoryDTB(quint64 addr_, quint64 val_)
{
    // Read from data TLB
    if (m_memorySystem)
    {
        quint64 result;
        if (m_memorySystem->readDataTLB(m_cpuId, addr_, result))
        {
            return result;
        }
    }

    DEBUG_LOG(QString("CPU%1: DTB read miss at 0x%2").arg(m_cpuId).arg(addr_, 0, 16));
    return 0;
}

quint64 AlphaCPU::readVirtualMemory(quint64 addr_, quint64 val_)
{
    // Read virtual memory through normal translation
    if (m_memorySystem)
    {
        quint64 result;
        if (m_memorySystem->readVirtualMemory(m_cpuId, addr_, result, 8, m_pc))
        {
            return result;
        }
    }

    // Handle read fault
    handleMemoryFault(addr_, false);
    return 0;
}

CpuModel AlphaCPU::getCpuModel()
{
    return static_cast<CpuModel>(m_cpuModel);
}

QString AlphaCPU::formatExceptionInfo() const
{
    if (!m_iprs || !m_hasException)
        return "No exception";

    quint64 excSum = m_iprs->read(IPRNumbers::IPR_EXC_SUM);
    quint64 excAddr = m_iprs->read(IPRNumbers::IPR_EXC_ADDR);
    quint64 excPC = m_iprs->read(IPRNumbers::IPR_EXC_PC);

    QString info = QString("Exception Summary: 0x%1\n").arg(excSum, 0, 16);
    info += QString("Fault Address: 0x%1\n").arg(excAddr, 0, 16);
    info += QString("Fault PC: 0x%1\n").arg(excPC, 0, 16);

    // Decode exception bits
    if (excSum & EXC_SUM_ACCESS_VIOLATION)
        info += "- Access Violation\n";
    if (excSum & EXC_SUM_FAULT_ON_READ)
        info += "- Fault on Read\n";
    if (excSum & EXC_SUM_TRANS_NOT_VALID)
        info += "- Translation Not Valid\n";
    if (excSum & EXC_SUM_ALIGNMENT_FAULT)
        info += "- Alignment Fault\n";

    return info;
}

QString AlphaCPU::getCacheStatistics() const
{
    QString stats;

    if (m_level1DataCache)
    {
        auto l1Stats = m_level1DataCache->getStatistics();
        stats += QString("CPU%1 L1D: Hits=%2, Misses=%3, Hit Rate=%.2f%%\n")
                     .arg(m_cpuId)
                     .arg(l1Stats.hits)
                     .arg(l1Stats.misses)
                     .arg(l1Stats.getHitRate());
    }

    if (m_level2DataCache)
    {
        auto l2Stats = m_level2DataCache->getStatistics();
        stats += QString("CPU%1 L2: Hits=%2, Misses=%3, Hit Rate=%.2f%%\n")
                     .arg(m_cpuId)
                     .arg(l2Stats.hits)
                     .arg(l2Stats.misses)
                     .arg(l2Stats.getHitRate());
    }

    return stats;
}
quint16 AlphaCPU::getCpuId() const { return m_cpuId; }

quint64 AlphaCPU::addDFormat(quint64 faVal, quint64 rbVal)
{
    // Add two VAX D_floating values
    // This is a placeholder - implement proper VAX D_floating arithmetic
    double a, b;
    std::memcpy(&a, &faVal, sizeof(double));
    std::memcpy(&b, &rbVal, sizeof(double));
    double result = a + b;
    quint64 resultBits;
    std::memcpy(&resultBits, &result, sizeof(double));
    return resultBits;
}

void AlphaCPU::logSecurityViolation(SecurityViolationType svType, quint64 rawInstruction)
{
    QString violationType;
    switch (svType)
    {
    case SecurityViolationType::PRIVILEGE_VIOLATION:
        violationType = "Privilege Violation";
        break;
    case SecurityViolationType::INVALID_MEMORY_ACCESS:
        violationType = "Invalid Memory Access";
        break;
    case SecurityViolationType::UNAUTHORIZED_INSTRUCTION:
        violationType = "Unauthorized Instruction";
        break;
    case SecurityViolationType::STACK_OVERFLOW:
        violationType = "Stack Overflow";
        break;
    case SecurityViolationType::BUFFER_OVERFLOW:
        violationType = "Buffer Overflow";
        break;
    default:
        violationType = "Unknown Violation";
        break;
    }

    DEBUG_LOG(QString("CPU%1: SECURITY VIOLATION - Type: %2, Instruction: 0x%3, PC: 0x%4")
                  .arg(m_cpuId)
                  .arg(violationType)
                  .arg(rawInstruction, 0, 16)
                  .arg(m_pc, 0, 16));

    // Log to security audit trail
    QDateTime timestamp = QDateTime::currentDateTime();
    QString logEntry = QString("%1 - CPU%2: %3 at PC=0x%4, Instr=0x%5")
                           .arg(timestamp.toString())
                           .arg(m_cpuId)
                           .arg(violationType)
                           .arg(m_pc, 0, 16)
                           .arg(rawInstruction, 0, 16);

    // In a real system, this would write to a security log file
    qWarning() << "SECURITY:" << logEntry;

    // Raise privilege violation exception
    raiseException(ExceptionType::PRIVILEGE_VIOLATION, m_pc);
}

// Get current exception summary
quint64 AlphaCPU::getExceptionSummary() const { return m_iprs ? m_iprs->read(IPRNumbers::IPR_EXC_SUM) : 0; }


void AlphaCPU::handleTLBMiss(quint64 virtualAddr, bool isWrite, bool isInstruction)
{
    if (m_memorySystem)
    {
        quint64 currentASN = getCurrentASN();
        m_memorySystem->handleTLBMiss(m_cpuId, virtualAddr, currentASN, isWrite, isInstruction);
    }
}

void AlphaCPU::handleTLBMiss(quint16 cpuId, quint64 virtualAddr, quint64 asn, bool isWrite, bool isInstruction)
{
    // Log the TLB miss
    DEBUG_LOG(QString("CPU%1: TLB miss for VA=0x%2, ASN=%3, Write=%4, Instruction=%5")
                  .arg(cpuId)
                  .arg(virtualAddr, 0, 16)
                  .arg(asn)
                  .arg(isWrite)
                  .arg(isInstruction));

    // The memory system will handle TLB miss recovery automatically during translation
    // For now, we can just emit a signal
    emit sigTranslationMiss(virtualAddr);
}
void AlphaCPU::handleTLBInvalidation(quint64 virtualAddr, quint64 asn)
{
    // Check if this invalidation affects our current ASN
    quint64 currentASN = getCurrentASN();
    if (asn == currentASN || asn == 0) // ASN 0 = global invalidation
    {
        // The memory system handles the actual TLB invalidation
        // We just need to acknowledge it
        updateSMPPerformanceCounters(0x104); // TLB invalidation received

        DEBUG_LOG(
            QString("CPU%1: Processed TLB invalidation VA=0x%2, ASN=%3").arg(m_cpuId).arg(virtualAddr, 0, 16).arg(asn));
    }
}

// Check specific exception flags using your constants
bool AlphaCPU::hasAccessViolation() const
{
    quint64 excSum = getExceptionSummary();
    return (excSum & EXC_SUM_ACCESS_VIOLATION) != 0;
}

bool AlphaCPU::hasFaultOnRead() const
{
    quint64 excSum = getExceptionSummary();
    return (excSum & EXC_SUM_FAULT_ON_READ) != 0;
}

bool AlphaCPU::hasTranslationFault() const
{
    quint64 excSum = getExceptionSummary();
    return (excSum & EXC_SUM_TRANS_NOT_VALID) != 0;
}

bool AlphaCPU::hasAlignmentFault() const
{
    quint64 excSum = getExceptionSummary();
    return (excSum & EXC_SUM_ALIGNMENT_FAULT) != 0;
}