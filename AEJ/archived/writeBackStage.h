#pragma once
#include <QObject>
#include <QQueue>
#include "GlobalMacro.h"

class AlphaCPU;

/**
 * @class WritebackStage
 * @brief Register writeback pipeline stage
 *
 * This class handles writing execution results back to registers.
 * Features:
 * - Supports both integer and floating-point writebacks
 * - Maintains writeback order (FIFO)
 * - Prevents writes to hardwired zero registers (R31/F31)
 * - Provides hazard detection for pipeline stall optimization
 * - Supports batch writebacks
 */
class WritebackStage
{
  public:
    /*
    Basic Writeback: The main writeback() function handles writing values to integer registers.
    Floating-Point Support: Added writebackFloatingPoint() for floating-point register writes.
    Pipeline Order: Uses a FIFO queue to maintain the correct order of writebacks.
    Hardwired Zero Protection: Prevents writes to R31 (integer) and F31 (floating-point) which are hardwired to zero in
    Alpha architecture. Hazard Detection: Provides methods to check for register hazards, which is useful for pipeline
    stall detection. Batch Operations: Supports multiple writebacks with writebackMultiple(). Pipeline Management:
    Includes flush() for pipeline flushes and methods to check pending operations.
    */
    WritebackStage() { DEBUG_LOG("WritebackStage initialized"); }
    
    void attachAlphaCPU(AlphaCPU *cpu_) { m_cpu = cpu_; }
    /**
     * @brief Perform integer register writeback
     * @param regNum Register number (0-31)
     * @param value Value to write
     */
    void writeback(quint8 regNum, quint64 value);

    /**
     * @brief Perform floating-point register writeback
     * @param regNum Register number (0-31)
     * @param value Value to write
     */
    void writebackFloatingPoint(quint8 regNum, quint64 value);

    /**
     * @brief Flush writeback stage
     */
    void flush();

    /**
     * @brief Check if there are pending writebacks
     */
    bool hasPendingWrites() const;

    /**
     * @brief Get number of pending writebacks
     */
    quint32 getPendingWriteCount() const;

    /**
     * @brief Perform multiple writebacks in one call
     * @param writebacks Vector of register-value pairs
     */
    void writebackMultiple(const QVector<QPair<quint8, quint64>> &writebacks);

    /**
     * @brief Check for integer register hazard
     * @param regNum Register to check
     * @return True if register has pending write
     */
    bool hasRegisterHazard(quint8 regNum) const;

    /**
     * @brief Check for floating-point register hazard
     * @param regNum Register to check
     * @return True if register has pending write
     */
    bool hasFloatRegisterHazard(quint8 regNum) const;

  private:
    AlphaCPU *m_cpu;

    struct WritebackEntry
    {
        quint8 regNum = 0;
        quint64 value = 0;
        bool valid = false;
        bool isFloat = false; // true for floating-point, false for integer
    };

    QQueue<WritebackEntry> m_writebackQueue;

    /**
     * @brief Process all pending writebacks
     */
    void processWritebacks();
};


