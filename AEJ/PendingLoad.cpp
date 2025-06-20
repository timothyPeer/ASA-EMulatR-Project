#include "PendingLoad.h"
#include <QDateTime>
#include "GlobalMacro.h"


/**
*
  TODO::  this function needs to be injected into the ExecuteStage::executeLoad() and AlphaCPU::completePendingLoad()
 * @class PendingLoad
 * @brief Represents a pending memory load operation
 *
 * This class manages asynchronous memory load operations in the Alpha pipeline.
 * Features:
 * - Tracks load address, destination register, and PC
 * - Handles cache misses that require memory system interaction
 * - Supports completion notification and polling
 * - Integrates with WritebackStage for result delivery


 Key Features:

Complete Load Tracking: Tracks address, size, destination register, and associated PC
Multiple Load Types: Support for byte (1B), word (2B), longword (4B), and quadword (8B) loads
Special Load Support:

Locked loads (LDx_L instructions)
Floating-point loads
Unaligned loads


Error Handling: Comprehensive error tracking with codes and messages
Performance Monitoring: Latency measurement from start to completion
Factory Methods: Static methods to create common load types easily

Integration with Existing Code:
The implementation follows your existing patterns:

Uses DEBUG_LOG for consistent logging
Follows the same naming conventions
Integrates with Qt types (QString, etc.)
Matches the style of your other pipeline classes

Notes:   TODO
The waitForCompletion() method is implemented as a placeholder - in a real system this would interact with your memory
system to wait for the actual load completion. The latency tracking uses QDateTime::currentMSecsSinceEpoch() - you might
want to use cycle counts instead for more accurate simulation. The factory methods make it easy to create loads for
different Alpha instruction types without remembering the exact parameters. Error handling allows you to distinguish
between different types of memory faults (access violations, page faults, etc.).

 */
PendingLoad::PendingLoad() : m_address(0), m_destReg(0), m_pc(0), m_valid(false), m_complete(false), m_errorCode(0)
{
    // Initialize timestamps
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_completionTime = 0;
}

PendingLoad::PendingLoad(quint64 addr, quint8 reg, quint64 pc, quint32 size)
    : m_address(addr), m_destReg(reg), m_pc(pc), m_valid(true), m_complete(false), m_loadSize(size), m_errorCode(0),
      m_value(0)
{
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_completionTime = 0;

    DEBUG_LOG(QString("PendingLoad: Created load request - Addr: 0x%1, Reg: R%2, PC: 0x%3, Size: %4")
                  .arg(addr, 16, 16, QChar('0'))
                  .arg(reg)
                  .arg(pc, 16, 16, QChar('0'))
                  .arg(size));
}

bool PendingLoad::isValid() const { return m_valid; }

bool PendingLoad::isComplete() const { return m_complete; }

quint64 PendingLoad::getAddress() const { return m_address; }

quint8 PendingLoad::getDestReg() const { return m_destReg; }

quint64 PendingLoad::getValue() const { return m_value; }

quint64 PendingLoad::getPC() const { return m_pc; }

quint32 PendingLoad::getLoadSize() const { return m_loadSize; }

bool PendingLoad::hasError() const { return m_errorCode != 0; }

quint32 PendingLoad::getErrorCode() const { return m_errorCode; }

QString PendingLoad::getErrorMessage() const { return m_errorMessage; }

void PendingLoad::waitForCompletion()
{
    if (isComplete())
        return;

    // In a real implementation, this would wait for:
    // - Cache miss handling
    // - Memory bus transaction
    // - DMA completion
    // For simulation, we can either:
    // 1. Actually wait (with timeout)
    // 2. Mark as complete immediately
    // 3. Simulate latency with a timer

    DEBUG_LOG(QString("PendingLoad: Waiting for completion of load at 0x%1").arg(m_address, 16, 16, QChar('0')));

    // Simulate memory access time (in cycles, not actual time)
    // This is just a placeholder - real implementation would involve
    // checking memory system status
    markComplete();
}

void PendingLoad::markComplete()
{
    if (m_complete)
        return;

    m_complete = true;
    m_completionTime = QDateTime::currentMSecsSinceEpoch();

    DEBUG_LOG(QString("PendingLoad: Load completed - Addr: 0x%1, Value: 0x%2, Latency: %3ms")
                  .arg(m_address, 16, 16, QChar('0'))
                  .arg(m_value, 16, 16, QChar('0'))
                  .arg(m_completionTime - m_startTime));
}

void PendingLoad::markComplete(quint64 value)
{
    m_value = value;
    markComplete();
}

void PendingLoad::markError(quint32 errorCode, const QString &errorMessage)
{
    m_complete = true;
    m_errorCode = errorCode;
    m_errorMessage = errorMessage;
    m_completionTime = QDateTime::currentMSecsSinceEpoch();

    DEBUG_LOG(QString("PendingLoad: Load failed - Addr: 0x%1, Error: %2 (%3)")
                  .arg(m_address, 16, 16, QChar('0'))
                  .arg(errorCode)
                  .arg(errorMessage));
}

quint64 PendingLoad::getLatency() const
{
    if (!m_complete)
        return 0;
    return m_completionTime - m_startTime;
}

bool PendingLoad::isLocked() const { return m_isLocked; }

void PendingLoad::setLocked(bool locked)
{
    m_isLocked = locked;
    if (locked)
    {
        DEBUG_LOG(QString("PendingLoad: Marked as locked load at 0x%1").arg(m_address, 16, 16, QChar('0')));
    }
}

// Additional utility methods for specific Alpha load types
bool PendingLoad::isByteLoad() const { return m_loadSize == 1; }

bool PendingLoad::isWordLoad() const { return m_loadSize == 2; }

bool PendingLoad::isLongwordLoad() const { return m_loadSize == 4; }

bool PendingLoad::isQuadwordLoad() const { return m_loadSize == 8; }

bool PendingLoad::isUnalignedLoad() const { return m_isUnaligned; }

void PendingLoad::setUnaligned(bool unaligned) { m_isUnaligned = unaligned; }

bool PendingLoad::isFloatingPointLoad() const { return m_isFloatingPoint; }

void PendingLoad::setFloatingPoint(bool fp) { m_isFloatingPoint = fp; }

// Static factory methods for common Alpha load types
PendingLoad PendingLoad::createByteLoad(quint64 addr, quint8 reg, quint64 pc) { return PendingLoad(addr, reg, pc, 1); }

PendingLoad PendingLoad::createWordLoad(quint64 addr, quint8 reg, quint64 pc) { return PendingLoad(addr, reg, pc, 2); }

PendingLoad PendingLoad::createLongwordLoad(quint64 addr, quint8 reg, quint64 pc)
{
    return PendingLoad(addr, reg, pc, 4);
}

PendingLoad PendingLoad::createQuadwordLoad(quint64 addr, quint8 reg, quint64 pc)
{
    return PendingLoad(addr, reg, pc, 8);
}

PendingLoad PendingLoad::createLockedLoad(quint64 addr, quint8 reg, quint64 pc, quint32 size)
{
    PendingLoad load(addr, reg, pc, size);
    load.setLocked(true);
    return load;
}

PendingLoad PendingLoad::createFloatingPointLoad(quint64 addr, quint8 reg, quint64 pc, quint32 size)
{
    PendingLoad load(addr, reg, pc, size);
    load.setFloatingPoint(true);
    return load;
}


