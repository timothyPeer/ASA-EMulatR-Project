#include "writeBackStage.h"
#include "GlobalMacro.h"
#include <QVector>
#include "AlphaCPU_refactored.h"
void WritebackStage::writeback(quint8 regNum, quint64 value)
{
    // Don't write to R31 (hardwired zero register)
    if (regNum == 31)
    {
        DEBUG_LOG("WritebackStage: Attempted to write to R31 (hardwired zero), ignoring");
        return;
    }

    // Create writeback entry
    WritebackEntry entry;
    entry.regNum = regNum;
    entry.value = value;
    entry.valid = true;

    // Add to writeback queue
    m_writebackQueue.enqueue(entry);

    DEBUG_LOG(QString("WritebackStage: Queued writeback R%1 = 0x%2").arg(regNum).arg(value, 16, 16, QChar('0')));

    // Process writebacks immediately in order
    while (!m_writebackQueue.isEmpty())
    {
        WritebackEntry currentEntry = m_writebackQueue.dequeue();

        if (currentEntry.valid)
        {
            // Perform the actual writeback
            m_cpu->setRegister(currentEntry.regNum, currentEntry.value);

            DEBUG_LOG(QString("WritebackStage: Completed writeback R%1 = 0x%2")
                          .arg(currentEntry.regNum)
                          .arg(currentEntry.value, 16, 16, QChar('0')));
        }
    }
}
void WritebackStage::flush()
{
    if (!m_writebackQueue.isEmpty())
    {
        DEBUG_LOG(QString("WritebackStage: Flushing %1 pending writebacks").arg(m_writebackQueue.size()));

        // Clear the queue without performing writebacks
        m_writebackQueue.clear();
    }

    DEBUG_LOG("WritebackStage: Pipeline flushed");
}
// Enhanced implementation with better pipeline management
void WritebackStage::writebackFloatingPoint(quint8 regNum, quint64 value)
{
    // Don't write to F31 (hardwired zero register for floating point)
    if (regNum == 31)
    {
        DEBUG_LOG("WritebackStage: Attempted to write to F31 (hardwired zero), ignoring");
        return;
    }

    // Create floating-point writeback entry
    WritebackEntry entry;
    entry.regNum = regNum;
    entry.value = value;
    entry.valid = true;
    entry.isFloat = true;

    m_writebackQueue.enqueue(entry);

    DEBUG_LOG(QString("WritebackStage: Queued FP writeback F%1 = 0x%2").arg(regNum).arg(value, 16, 16, QChar('0')));

    // Process floating-point writebacks
    processWritebacks();
}
void WritebackStage::processWritebacks()
{
    // Process all pending writebacks in FIFO order
    while (!m_writebackQueue.isEmpty())
    {
        WritebackEntry entry = m_writebackQueue.dequeue();

        if (!entry.valid)
            continue;

        if (entry.isFloat)
        {
            m_cpu->setFloatRegister(entry.regNum, entry.value);
            DEBUG_LOG(QString("WritebackStage: Completed FP writeback F%1 = 0x%2")
                          .arg(entry.regNum)
                          .arg(entry.value, 16, 16, QChar('0')));
        }
        else
        {
            m_cpu->setRegister(entry.regNum, entry.value);
            DEBUG_LOG(QString("WritebackStage: Completed INT writeback R%1 = 0x%2")
                          .arg(entry.regNum)
                          .arg(entry.value, 16, 16, QChar('0')));
        }
    }
}
bool WritebackStage::hasPendingWrites() const { 
    return !m_writebackQueue.isEmpty();
}
quint32 WritebackStage::getPendingWriteCount() const { return m_writebackQueue.size(); }
void WritebackStage::writebackMultiple(const QVector<QPair<quint8, quint64>> &writebacks)
{
    for (const auto &wb : writebacks)
    {
        writeback(wb.first, wb.second);
    }
}
// Check for register hazards (useful for pipeline stall detection)
bool WritebackStage::hasRegisterHazard(quint8 regNum) const
{
    for (const auto &entry : m_writebackQueue)
    {
        if (entry.valid && entry.regNum == regNum && !entry.isFloat)
        {
            return true;
        }
    }
    return false;
}
bool WritebackStage::hasFloatRegisterHazard(quint8 regNum) const
{
    for (const auto &entry : m_writebackQueue)
    {
        if (entry.valid && entry.regNum == regNum && entry.isFloat)
        {
            return true;
        }
    }
    return false;
}