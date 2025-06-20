// executorAlphaFloatingPoint.cpp
#include "executorAlphaFloatingPoint.h"
#include "AlphaCPU_refactored.h"
#include "AlphaInstructionCache.h"
#include "DecodedInstruction.h"
#include "UnifiedDataCache.h"
#include <QDebug>
#include <QMutexLocker>
#include <cmath>
#include <limits>
#include <QtConcurrent>
#include "utilitySafeIncrement.h"

executorAlphaFloatingPoint::executorAlphaFloatingPoint(AlphaCPU *cpu, QObject *parent)
    : QObject(parent), m_cpu(cpu), m_fpcr()
{
    // Initialize floating-point control register with default rounding mode (round to nearest)
    m_fpcr.fields.rounding_mode = 0; // Round to nearest (even)
}

bool executorAlphaFloatingPoint::executeFLTLFunction(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    m_fpInstructions++;

    // Extract function code from bits [10:5] of the instruction
    quint32 function = (instruction.raw >> 5) & 0x3F;

    // Check for additional qualifier bits
    bool isTrapping = (instruction.raw & 0x2000) != 0; // bit 13
    bool isSoftware = (instruction.raw & 0x8000) != 0; // bit 15

    // Combine function code with qualifiers for extended functions
    quint32 extendedFunction = function;
    if (isTrapping)
        extendedFunction |= 0x100;
    if (isSoftware)
        extendedFunction |= 0x400;

    bool success = false;

    switch (extendedFunction)
    {
    case 0x010: // CVTLQ
        success = executeCVTLQ(instruction);
        break;
    case 0x020: // CPYS
        success = executeCPYS(instruction);
        break;
    case 0x021: // CPYSN
        success = executeCPYSN(instruction);
        break;
    case 0x022: // CPYSE
        success = executeCPYSE(instruction);
        break;
    case 0x024: // MT_FPCR
        success = executeMT_FPCR(instruction);
        break;
    case 0x025: // MF_FPCR
        success = executeMF_FPCR(instruction);
        break;
    case 0x02A: // FCMOVEQ
        success = executeFCMOVEQ(instruction);
        break;
    case 0x02B: // FCMOVNE
        success = executeFCMOVNE(instruction);
        break;
    case 0x02C: // FCMOVLT
        success = executeFCMOVLT(instruction);
        break;
    case 0x02D: // FCMOVGE
        success = executeFCMOVGE(instruction);
        break;
    case 0x02E: // FCMOVLE
        success = executeFCMOVLE(instruction);
        break;
    case 0x02F: // FCMOVGT
        success = executeFCMOVGT(instruction);
        break;
    case 0x030: // CVTQL
        success = executeCVTQL(instruction);
        break;
    case 0x130: // CVTQLV (with overflow trap)
        success = executeCVTQLV(instruction);
        break;
    case 0x530: // CVTQLSV (software completion + overflow trap)
        success = executeCVTQLSV(instruction);
        break;
    default:
        qWarning() << "Unknown FLTL function:" << Qt::hex << extendedFunction;
        success = false;
        break;
    }

    emit fpInstructionExecuted(extendedFunction, success);
    return success;
}

bool executorAlphaFloatingPoint::executeCVTLQ(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConversions);

    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 rbValue;
    if (!readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    // Convert 32-bit longword to 64-bit quadword (sign extension)
    quint32 longword = static_cast<quint32>(rbValue & 0xFFFFFFFF);
    quint64 result = convertLongwordToQuadword(longword);

    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeCPYS(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpSignOperations);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 result = copyFloatSign(raValue, rbValue);
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeCPYSN(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpSignOperations);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 result = copyFloatSignNegate(raValue, rbValue);
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeCPYSE(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpSignOperations);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 result = copyFloatSignAndExponent(raValue, rbValue);
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeMT_FPCR(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpcrOperations);

    quint8 ra = (instruction.raw >> 21) & 0x1F;

    quint64 raValue;
    if (!readFloatRegisterWithCache(ra, raValue))
    {
        return false;
    }

    setFPCR(raValue);
    return true;
}

bool executorAlphaFloatingPoint::executeMF_FPCR(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpcrOperations);

    quint8 rc = instruction.raw & 0x1F;

    quint64 fpcrValue = getFPCR();
    return writeFloatRegisterWithCache(rc, fpcrValue);
}

bool executorAlphaFloatingPoint::executeFCMOVEQ(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConditionalMoves);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 result = isFloatZero(raValue) ? rbValue : raValue;
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeFCMOVNE(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConditionalMoves);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 result = !isFloatZero(raValue) ? rbValue : raValue;
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeFCMOVLT(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConditionalMoves);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 zeroValue = 0;
    quint64 result = isFloatLessThan(raValue, zeroValue) ? rbValue : raValue;
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeFCMOVGE(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConditionalMoves);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 zeroValue = 0;
    quint64 result = isFloatGreaterOrEqual(raValue, zeroValue) ? rbValue : raValue;
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeFCMOVLE(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConditionalMoves);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 zeroValue = 0;
    quint64 result = isFloatLessOrEqual(raValue, zeroValue) ? rbValue : raValue;
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeFCMOVGT(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConditionalMoves);

    quint8 ra = (instruction.raw >> 21) & 0x1F;
    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 raValue, rbValue;
    if (!readFloatRegisterWithCache(ra, raValue) || !readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    quint64 zeroValue = 0;
    quint64 result = isFloatGreaterThan(raValue, zeroValue) ? rbValue : raValue;
    return writeFloatRegisterWithCache(rc, result);
}

bool executorAlphaFloatingPoint::executeCVTQL(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConversions);

    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 rbValue;
    if (!readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    // Convert 64-bit quadword to 32-bit longword (truncation)
    quint32 result = convertQuadwordToLongword(rbValue, false);

    return writeFloatRegisterWithCache(rc, static_cast<quint64>(result));
}

bool executorAlphaFloatingPoint::executeCVTQLV(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConversions);

    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 rbValue;
    if (!readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    // Convert with overflow checking
    quint32 result = convertQuadwordToLongword(rbValue, true);

    return writeFloatRegisterWithCache(rc, static_cast<quint64>(result));
}

bool executorAlphaFloatingPoint::executeCVTQLSV(const DecodedInstruction &instruction)
{
    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_fpConversions);

    quint8 rb = (instruction.raw >> 16) & 0x1F;
    quint8 rc = instruction.raw & 0x1F;

    quint64 rbValue;
    if (!readFloatRegisterWithCache(rb, rbValue))
    {
        return false;
    }

    // Convert with software completion and overflow checking
    quint32 result = convertQuadwordToLongword(rbValue, true);

    return writeFloatRegisterWithCache(rc, static_cast<quint64>(result));
}

quint64 executorAlphaFloatingPoint::getFPCR() const
{
    QMutexLocker locker(&m_fpcrMutex);
    return m_fpcr.raw;
}

void executorAlphaFloatingPoint::setFPCR(quint64 value)
{
    QMutexLocker locker(&m_fpcrMutex);
    m_fpcr.raw = value;

    // Apply rounding mode to FPU
    switch (m_fpcr.fields.rounding_mode)
    {
    case 0: // Round to nearest (even)
        std::fesetround(FE_TONEAREST);
        break;
    case 1: // Round toward zero (truncate)
        std::fesetround(FE_TOWARDZERO);
        break;
    case 2: // Round toward positive infinity
        std::fesetround(FE_UPWARD);
        break;
    case 3: // Round toward negative infinity
        std::fesetround(FE_DOWNWARD);
        break;
    }
}

bool executorAlphaFloatingPoint::fetchInstructionWithCache(quint64 pc, quint32 &instruction)
{
    QMutexLocker locker(&m_statsMutex);

    if (m_instructionCache)
    {
        bool hit = m_instructionCache->read(pc, reinterpret_cast<quint8 *>(&instruction), 4);
        if (hit)
        {
           asa_utils::safeIncrement( m_l1ICacheHits);
            updateCacheStatistics("L1I", true);
        }
        else
        {
            asa_utils::safeIncrement(m_l1ICacheMisses);
            updateCacheStatistics("L1I", false);

            // Try L2 cache
            if (m_level2Cache)
            {
                hit = m_level2Cache->read(pc, reinterpret_cast<quint8 *>(&instruction), 4);
                if (hit)
                {
                    asa_utils::safeIncrement(m_l2CacheHits);
                    updateCacheStatistics("L2", true);
                }
                else
                {
                    asa_utils::safeIncrement(m_l2CacheMisses);
                    updateCacheStatistics("L2", false);

                    // Try L3 cache
                    if (m_level3Cache)
                    {
                        hit = m_level3Cache->read(pc, reinterpret_cast<quint8 *>(&instruction), 4);
                        if (hit)
                        {
                            asa_utils::safeIncrement(m_l3CacheHits);
                            updateCacheStatistics("L3", true);
                        }
                        else
                        {
                            asa_utils::safeIncrement(m_l3CacheMisses);
                            updateCacheStatistics("L3", false);
                        }
                    }
                }
            }
        }
        return hit;
    }

    // Fallback to CPU memory access
    return m_cpu ? m_cpu->readMemory(pc, reinterpret_cast<quint8 *>(&instruction), 4) : false;
}

bool executorAlphaFloatingPoint::readFloatRegisterWithCache(quint8 reg, quint64 &value)
{
    if (!m_cpu)
        return false;

    // Read from floating-point register file
    value = m_cpu->getFloatRegister(reg);

    QMutexLocker locker(&m_statsMutex);
   asa_utils::safeIncrement( m_l1DCacheHits); // Register access is always a hit
    updateCacheStatistics("L1D", true);

    return true;
}

bool executorAlphaFloatingPoint::writeFloatRegisterWithCache(quint8 reg, quint64 value)
{
    if (!m_cpu)
        return false;

    // Write to floating-point register file
    m_cpu->setFloatRegister(reg, value);

    QMutexLocker locker(&m_statsMutex);
    asa_utils::safeIncrement(m_l1DCacheHits); // Register access is always a hit
    updateCacheStatistics("L1D", true);

    return true;
}

bool executorAlphaFloatingPoint::isFloatZero(quint64 fpValue) const
{
    // Check for IEEE 754 zero (both +0.0 and -0.0)
    return (fpValue & 0x7FFFFFFFFFFFFFFFULL) == 0;
}

bool executorAlphaFloatingPoint::isFloatNegative(quint64 fpValue) const
{
    // Check sign bit (bit 63)
    return (fpValue & 0x8000000000000000ULL) != 0;
}

bool executorAlphaFloatingPoint::isFloatEqual(quint64 fp1, quint64 fp2) const
{
    // Handle NaN cases
    if (((fp1 & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL && (fp1 & 0x000FFFFFFFFFFFFFULL) != 0) ||
        ((fp2 & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL && (fp2 & 0x000FFFFFFFFFFFFFULL) != 0))
    {
        return false; // NaN is never equal to anything
    }

    // Handle zero cases (+0.0 == -0.0)
    if (isFloatZero(fp1) && isFloatZero(fp2))
    {
        return true;
    }

    return fp1 == fp2;
}

bool executorAlphaFloatingPoint::isFloatLessThan(quint64 fp1, quint64 fp2) const
{
    double d1, d2;
    std::memcpy(&d1, &fp1, sizeof(double));
    std::memcpy(&d2, &fp2, sizeof(double));
    return d1 < d2;
}

bool executorAlphaFloatingPoint::isFloatLessOrEqual(quint64 fp1, quint64 fp2) const
{
    double d1, d2;
    std::memcpy(&d1, &fp1, sizeof(double));
    std::memcpy(&d2, &fp2, sizeof(double));
    return d1 <= d2;
}

bool executorAlphaFloatingPoint::isFloatGreaterThan(quint64 fp1, quint64 fp2) const
{
    double d1, d2;
    std::memcpy(&d1, &fp1, sizeof(double));
    std::memcpy(&d2, &fp2, sizeof(double));
    return d1 > d2;
}

bool executorAlphaFloatingPoint::isFloatGreaterOrEqual(quint64 fp1, quint64 fp2) const
{
    double d1, d2;
    std::memcpy(&d1, &fp1, sizeof(double));
    std::memcpy(&d2, &fp2, sizeof(double));
    return d1 >= d2;
}

quint64 executorAlphaFloatingPoint::copyFloatSign(quint64 source, quint64 target) const
{
    // Copy sign bit from source to target
    return (target & 0x7FFFFFFFFFFFFFFFULL) | (source & 0x8000000000000000ULL);
}

quint64 executorAlphaFloatingPoint::copyFloatSignNegate(quint64 source, quint64 target) const
{
    // Copy negated sign bit from source to target
    return (target & 0x7FFFFFFFFFFFFFFFULL) | ((~source) & 0x8000000000000000ULL);
}

quint64 executorAlphaFloatingPoint::copyFloatSignAndExponent(quint64 source, quint64 target) const
{
    // Copy sign and exponent bits from source to target, keep mantissa from target
    return (target & 0x000FFFFFFFFFFFFFULL) | (source & 0xFFF0000000000000ULL);
}

quint64 executorAlphaFloatingPoint::convertLongwordToQuadword(quint32 longword) const
{
    // Sign-extend 32-bit to 64-bit
    qint32 signedLongword = static_cast<qint32>(longword);
    return static_cast<quint64>(static_cast<qint64>(signedLongword));
}

quint32 executorAlphaFloatingPoint::convertQuadwordToLongword(quint64 quadword, bool checkOverflow) const
{
    qint64 signedQuadword = static_cast<qint64>(quadword);

    if (checkOverflow)
    {
        if (signedQuadword > std::numeric_limits<qint32>::max() || signedQuadword < std::numeric_limits<qint32>::min())
        {
            raiseFloatingPointException(0x10); // Overflow exception
        }
    }

    return static_cast<quint32>(signedQuadword & 0xFFFFFFFF);
}

void executorAlphaFloatingPoint::raiseFloatingPointException(quint32 exceptionType)
{
    QMutexLocker locker(&m_fpcrMutex);

    // Set appropriate exception flag in FPCR
    switch (exceptionType)
    {
    case 0x01: // Inexact
        m_fpcr.fields.inexact_result = 1;
        if (m_fpcr.fields.trap_enable_inexact)
        {
            emit fpExceptionRaised(exceptionType, m_cpu ? m_cpu->getPC() : 0);
        }
        break;
    case 0x02: // Underflow
        m_fpcr.fields.underflow_result = 1;
        if (m_fpcr.fields.trap_enable_underflow)
        {
            emit fpExceptionRaised(exceptionType, m_cpu ? m_cpu->getPC() : 0);
        }
        break;
    case 0x04: // Overflow
        m_fpcr.fields.overflow_result = 1;
        if (m_fpcr.fields.trap_enable_overflow)
        {
            emit fpExceptionRaised(exceptionType, m_cpu ? m_cpu->getPC() : 0);
        }
        break;
    case 0x08: // Division by zero
        m_fpcr.fields.div_zero_result = 1;
        if (m_fpcr.fields.trap_enable_div_zero)
        {
            emit fpExceptionRaised(exceptionType, m_cpu ? m_cpu->getPC() : 0);
        }
        break;
    case 0x10: // Invalid operation
        m_fpcr.fields.invalid_result = 1;
        if (m_fpcr.fields.trap_enable_invalid)
        {
            emit fpExceptionRaised(exceptionType, m_cpu ? m_cpu->getPC() : 0);
        }
        break;
    }
}

bool executorAlphaFloatingPoint::checkFloatingPointTraps(quint64 fpResult)
{
    // Check for NaN, infinity, or other exceptional values
    quint64 exponent = (fpResult >> 52) & 0x7FF;
    quint64 mantissa = fpResult & 0x000FFFFFFFFFFFFFULL;

    if (exponent == 0x7FF)
    {
        if (mantissa != 0)
        {
            // NaN
            raiseFloatingPointException(0x10);
            return false;
        }
        else
        {
            // Infinity
            raiseFloatingPointException(0x04);
            return false;
        }
    }

    if (exponent == 0 && mantissa != 0)
    {
        // Denormalized number
        raiseFloatingPointException(0x02);
    }

    return true;
}

void executorAlphaFloatingPoint::updateCacheStatistics(const QString &level, bool hit)
{
    emit cachePerformanceUpdate(level, hit);
}

void executorAlphaFloatingPoint::printStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    qDebug() << "=== Alpha Floating-Point Executor Statistics ===";
    qDebug() << "FP Instructions:" << m_fpInstructions;
    qDebug() << "FP Conditional Moves:" << m_fpConditionalMoves;
    DEBUG_LOG(QString( "FP Conversions: %1").arg( m_fpConversions));
    qDebug() << "FP Sign Operations:" << m_fpSignOperations;
    qDebug() << "FPCR Operations:" << m_fpcrOperations;

    qDebug() << "\n=== Cache Performance ===";
    qDebug() << "L1 I-Cache: Hits=" << m_l1ICacheHits << ", Misses=" << m_l1ICacheMisses;
    qDebug() << "L1 D-Cache: Hits=" << m_l1DCacheHits << ", Misses=" << m_l1DCacheMisses;
    qDebug() << "L2 Cache: Hits=" << m_l2CacheHits << ", Misses=" << m_l2CacheMisses;
    qDebug() << "L3 Cache: Hits=" << m_l3CacheHits << ", Misses=" << m_l3CacheMisses;

    // Calculate hit rates
    quint64 totalL1IAccess = m_l1ICacheHits + m_l1ICacheMisses;
    quint64 totalL1DAccess = m_l1DCacheHits + m_l1DCacheMisses;
    quint64 totalL2Access = m_l2CacheHits + m_l2CacheMisses;
    quint64 totalL3Access = m_l3CacheHits + m_l3CacheMisses;

    if (totalL1IAccess > 0)
    {
        double hitRate = (static_cast<double>(m_l1ICacheHits) / totalL1IAccess) * 100.0;
        qDebug() << "L1 I-Cache Hit Rate:" << QString::number(hitRate, 'f', 2) << "%";
    }

    if (totalL1DAccess > 0)
    {
        double hitRate = (static_cast<double>(m_l1DCacheHits) / totalL1DAccess) * 100.0;
        qDebug() << "L1 D-Cache Hit Rate:" << QString::number(hitRate, 'f', 2) << "%";
    }

    if (totalL2Access > 0)
    {
        double hitRate = (static_cast<double>(m_l2CacheHits) / totalL2Access) * 100.0;
        qDebug() << "L2 Cache Hit Rate:" << QString::number(hitRate, 'f', 2) << "%";
    }

    if (totalL3Access > 0)
    {
        double hitRate = (static_cast<double>(m_l3CacheHits) / totalL3Access) * 100.0;
        qDebug() << "L3 Cache Hit Rate:" << QString::number(hitRate, 'f', 2) << "%";
    }
}

void executorAlphaFloatingPoint::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);

    m_fpInstructions = 0;
    m_fpConditionalMoves = 0;
    m_fpConversions = 0;
    m_fpSignOperations = 0;
    m_fpcrOperations = 0;

    m_l1ICacheHits = 0;
    m_l1ICacheMisses = 0;
    m_l1DCacheHits = 0;
    m_l1DCacheMisses = 0;
    m_l2CacheHits = 0;
    m_l2CacheMisses = 0;
    m_l3CacheHits = 0;
    m_l3CacheMisses = 0;
}


void executorAlphaFloatingPoint::startAsyncPipeline()
{
    if (m_pipelineActive.exchange(true))
    {
        return; // Already running
    }

    // Clear pipeline state
    {
        QMutexLocker locker(&m_pipelineMutex);
        m_fetchQueue.clear();
        m_decodeQueue.clear();
        m_executeQueue.clear();
        m_writebackQueue.clear();
        m_registerLastWriter.clear();
        m_dependencyGraph.clear();
        m_sequenceCounter.store(0);
    }

    // Start worker threads
    m_fetchWorker = QtConcurrent::run([this]() { fetchWorker(); });
    m_decodeWorker = QtConcurrent::run([this]() { decodeWorker(); });
    m_executeWorker = QtConcurrent::run([this]() { executeWorker(); });
    m_writebackWorker = QtConcurrent::run([this]() { writebackWorker(); });
    m_cacheWorker = QtConcurrent::run([this]() { cacheWorker(); });

    qDebug() << "Asynchronous FP pipeline started";
}

void executorAlphaFloatingPoint::stopAsyncPipeline()
{
    if (!m_pipelineActive.exchange(false))
    {
        return; // Already stopped
    }

    // Wake up all workers
    m_pipelineCondition.wakeAll();
    m_cacheQueueCondition.wakeAll();
    m_cacheRequestSemaphore.release(MAX_CACHE_REQUESTS);

    // Wait for workers to complete
    m_fetchWorker.waitForFinished();
    m_decodeWorker.waitForFinished();
    m_executeWorker.waitForFinished();
    m_writebackWorker.waitForFinished();
    m_cacheWorker.waitForFinished();

    qDebug() << "Asynchronous FP pipeline stopped";
}

bool executorAlphaFloatingPoint::submitInstruction(const DecodedInstruction &instruction, quint64 pc)
{
    if (!m_pipelineActive.load())
    {
        return false;
    }

    QMutexLocker locker(&m_pipelineMutex);

    if (m_fetchQueue.size() >= MAX_PIPELINE_DEPTH)
    {
        m_pipelineStalls.fetch_add(1);
        return false; // Pipeline full
    }

    quint64 seqNum = m_sequenceCounter.fetch_add(1);
    FPInstruction fpInstr(instruction, pc, seqNum);
    analyzeDependencies(fpInstr);

    m_fetchQueue.enqueue(fpInstr);
    m_pipelineCondition.wakeOne();

    return true;
}

void executorAlphaFloatingPoint::fetchWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_fetchQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_fetchQueue.isEmpty() && canAdvanceStage(m_fetchQueue, m_decodeQueue))
        {
            FPInstruction instr = m_fetchQueue.dequeue();

            // Asynchronously fetch instruction from cache
            auto future = asyncCacheRead(instr.pc, nullptr, 4);
            if (future.result())
            {
                instr.isReady = true;
                m_decodeQueue.enqueue(instr);
                m_pipelineCondition.wakeOne();
            }
            else
            {
                // Cache miss - requeue for retry
                m_fetchQueue.enqueue(instr);
                m_cacheQueueStalls.fetch_add(1);
            }
        }
    }
}

void executorAlphaFloatingPoint::decodeWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_decodeQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_decodeQueue.isEmpty() && canAdvanceStage(m_decodeQueue, m_executeQueue))
        {
            FPInstruction instr = m_decodeQueue.dequeue();

            // Decode is fast - just mark as ready
            instr.isReady = true;
            m_executeQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

void executorAlphaFloatingPoint::executeWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_executeQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_executeQueue.isEmpty() && canAdvanceStage(m_executeQueue, m_writebackQueue))
        {
            FPInstruction instr = m_executeQueue.dequeue();

            // Check dependencies before execution
            if (!checkDependencies(instr))
            {
                m_executeQueue.enqueue(instr); // Requeue
                m_dependencyStalls.fetch_add(1);
                continue;
            }

            locker.unlock(); // Release lock during execution

            // Execute the instruction
            bool success = executeFLTLFunction(instr.instruction);

            locker.relock();
            instr.isCompleted = true;
            if (!success)
            {
                instr.hasException = true;
                instr.exceptionType = 0x10; // Generic FP exception
            }

            m_writebackQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

void executorAlphaFloatingPoint::writebackWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_writebackQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_writebackQueue.isEmpty())
        {
            FPInstruction instr = m_writebackQueue.dequeue();

            // Update dependency tracking
            updateDependencies(instr);

            // Handle exceptions
            if (instr.hasException)
            {
                emit fpExceptionRaised(instr.exceptionType, instr.pc);
            }

            emit fpInstructionExecuted((instr.instruction.raw >> 5) & 0x3F, instr.isCompleted && !instr.hasException);
        }
    }
}

void executorAlphaFloatingPoint::cacheWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_cacheQueueMutex);

        while (m_cacheRequestQueue.isEmpty() && m_pipelineActive.load())
        {
            m_cacheQueueCondition.wait(&m_cacheQueueMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_cacheRequestQueue.isEmpty())
        {
            CacheRequest request = m_cacheRequestQueue.dequeue();
            locker.unlock();

            bool result = false;
            switch (request.type)
            {
            case CacheRequest::InstructionFetch:
                result = fetchInstructionWithCache(request.address, *reinterpret_cast<quint32 *>(&request.data));
                break;
            case CacheRequest::RegisterRead:
                result = readFloatRegisterWithCache(request.registerNum, request.data);
                break;
            case CacheRequest::RegisterWrite:
                result = writeFloatRegisterWithCache(request.registerNum, request.data);
                break;
            }

            request.promise->addResult(result);
            request.promise->finish();
        }
    }
}

void executorAlphaFloatingPoint::analyzeDependencies(FPInstruction &instr)
{
    quint32 raw = instr.instruction.raw;
    quint8 ra = (raw >> 21) & 0x1F;
    quint8 rb = (raw >> 16) & 0x1F;
    quint8 rc = raw & 0x1F;
    quint32 function = (raw >> 5) & 0x3F;

    // Determine source and destination registers
    switch (function)
    {
    case 0x010: // CVTLQ
    case 0x030: // CVTQL
    case 0x025: // MF_FPCR
        instr.srcRegisters.insert(rb);
        instr.dstRegisters.insert(rc);
        break;
    case 0x020: // CPYS
    case 0x021: // CPYSN
    case 0x022: // CPYSE
    case 0x02A: // FCMOVEQ
    case 0x02B: // FCMOVNE
    case 0x02C: // FCMOVLT
    case 0x02D: // FCMOVGE
    case 0x02E: // FCMOVLE
    case 0x02F: // FCMOVGT
        instr.srcRegisters.insert(ra);
        instr.srcRegisters.insert(rb);
        instr.dstRegisters.insert(rc);
        break;
    case 0x024: // MT_FPCR
        instr.srcRegisters.insert(ra);
        instr.touchesFPCR = true;
        break;
    }
}

bool executorAlphaFloatingPoint::checkDependencies(const FPInstruction &instr) const
{
    // Check register dependencies
    for (quint8 reg : instr.srcRegisters)
    {
        auto it = m_registerLastWriter.find(reg);
        if (it != m_registerLastWriter.end() && it.value() > instr.sequenceNumber)
        {
            return false; // WAR hazard
        }
    }

    // Check FPCR dependencies
    if (instr.touchesFPCR && m_lastFPCRWriter > instr.sequenceNumber)
    {
        return false;
    }

    return true;
}

void executorAlphaFloatingPoint::updateDependencies(const FPInstruction &instr)
{
    // Update register writers
    for (quint8 reg : instr.dstRegisters)
    {
        m_registerLastWriter[reg] = instr.sequenceNumber;
    }

    // Update FPCR writer
    if (instr.touchesFPCR)
    {
        m_lastFPCRWriter = instr.sequenceNumber;
    }
}

QFuture<bool> executorAlphaFloatingPoint::asyncCacheRead(quint64 address, quint8 *data, int size)
{
    QMutexLocker locker(&m_cacheQueueMutex);

    CacheRequest request(CacheRequest::InstructionFetch, address);
    auto future = request.promise->future();

    m_cacheRequestQueue.enqueue(request);
    m_cacheQueueCondition.wakeOne();

    return future;
}

QFuture<bool> executorAlphaFloatingPoint::asyncRegisterRead(quint8 reg)
{
    QMutexLocker locker(&m_cacheQueueMutex);

    CacheRequest request(CacheRequest::RegisterRead, 0);
    request.registerNum = reg;
    auto future = request.promise->future();

    m_cacheRequestQueue.enqueue(request);
    m_cacheQueueCondition.wakeOne();

    return future;
}

QFuture<bool> executorAlphaFloatingPoint::asyncRegisterWrite(quint8 reg, quint64 value)
{
    QMutexLocker locker(&m_cacheQueueMutex);

    CacheRequest request(CacheRequest::RegisterWrite, 0);
    request.registerNum = reg;
    request.data = value;
    auto future = request.promise->future();

    m_cacheRequestQueue.enqueue(request);
    m_cacheQueueCondition.wakeOne();

    return future;
}

bool executorAlphaFloatingPoint::canAdvanceStage(const QQueue<FPInstruction> &from,
                                                 const QQueue<FPInstruction> &to) const
{
    return !from.isEmpty() && to.size() < MAX_PIPELINE_DEPTH;
}

void executorAlphaFloatingPoint::printPipelineStatistics() const
{
    qDebug() << "\n=== Asynchronous Pipeline Statistics ===";
    qDebug() << "Pipeline Stalls:" << m_pipelineStalls.load();
    qDebug() << "Cache Queue Stalls:" << m_cacheQueueStalls.load();
    qDebug() << "Dependency Stalls:" << m_dependencyStalls.load();
    qDebug() << "Current Pipeline Depth:";
    qDebug() << "  Fetch Queue:" << m_fetchQueue.size();
    qDebug() << "  Decode Queue:" << m_decodeQueue.size();
    qDebug() << "  Execute Queue:" << m_executeQueue.size();
    qDebug() << "  Writeback Queue:" << m_writebackQueue.size();
}
void executorAlphaFloatingPoint::cacheWorker()
{
    while (m_pipelineActive.load())
    {
        // Batch TLB requests to reduce lock contention
        QVector<CacheRequest> batch;

        {
            QMutexLocker locker(&m_cacheQueueMutex);
            // Collect multiple requests
            while (!m_cacheRequestQueue.isEmpty() && batch.size() < 8)
            {
                batch.append(m_cacheRequestQueue.dequeue());
            }
        }

        // Process batch with minimal TLB contention
        for (auto &request : batch)
        {
            // Use async TLB lookup
            auto future = m_translationCache->lookupAsync(request.address, getCurrentASN(), false,
                                                          request.type == CacheRequest::InstructionFetch);

            request.promise->addResult(future.result());
            request.promise->finish();
        }
    }
}
// Add to destructor:
executorAlphaFloatingPoint::~executorAlphaFloatingPoint() { stopAsyncPipeline(); }
/*//include "executorAlphaFloatingPoint.moc"*/