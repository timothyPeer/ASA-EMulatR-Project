#pragma once
#include <QObject>
#include <QString>



/**
 * @class PendingLoad
 * @brief Represents a pending memory load operation
 *
 * This class encapsulates all information about a memory load that hasn't completed yet.
 * It's used to handle cache misses, memory latency, and out-of-order completion.
 *
 * Features:
 * - Tracks load address, size, destination register
 * - Handles different Alpha load types (byte, word, longword, quadword)
 * - Supports locked loads and floating-point loads
 * - Provides error handling and completion status
 * - Measures load latency for performance analysis
 */
class PendingLoad
{
  public:
    /**
     * @brief Default constructor - creates invalid load
     */
    PendingLoad();

    /**
     * @brief Constructor
     * @param addr Memory address to load from
     * @param reg Destination register number
     * @param pc Program counter when load was issued
     * @param size Size of load in bytes (1, 2, 4, or 8)
     */
    PendingLoad(quint64 addr, quint8 reg, quint64 pc, quint32 size = 8);

    // Status queries
    bool isValid() const;
    bool isComplete() const;
    bool hasError() const;

    // Accessors
    quint64 getAddress() const;
    quint8 getDestReg() const;
    quint64 getValue() const;
    quint64 getPC() const;
    quint32 getLoadSize() const;
    quint32 getErrorCode() const;
    QString getErrorMessage() const;
    quint64 getLatency() const;

    // Load type queries
    bool isByteLoad() const;
    bool isWordLoad() const;
    bool isLongwordLoad() const;
    bool isQuadwordLoad() const;
    bool isUnalignedLoad() const;
    bool isFloatingPointLoad() const;
    bool isLocked() const;

    // Load type setters
    void setUnaligned(bool unaligned);
    void setFloatingPoint(bool fp);
    void setLocked(bool locked);

    // Completion control
    void waitForCompletion();
    void markComplete();
    void markComplete(quint64 value);
    void markError(quint32 errorCode, const QString &errorMessage);

    // Static factory methods for common Alpha load types
    static PendingLoad createByteLoad(quint64 addr, quint8 reg, quint64 pc);
    static PendingLoad createWordLoad(quint64 addr, quint8 reg, quint64 pc);
    static PendingLoad createLongwordLoad(quint64 addr, quint8 reg, quint64 pc);
    static PendingLoad createQuadwordLoad(quint64 addr, quint8 reg, quint64 pc);
    static PendingLoad createLockedLoad(quint64 addr, quint8 reg, quint64 pc, quint32 size);
    static PendingLoad createFloatingPointLoad(quint64 addr, quint8 reg, quint64 pc, quint32 size);

  private:
    quint64 m_address = 0;          ///< Memory address to load from
    quint8 m_destReg = 0;           ///< Destination register number
    quint64 m_pc = 0;               ///< PC when load was issued
    quint64 m_value = 0;            ///< Loaded value (when complete)
    quint32 m_loadSize = 8;         ///< Size of load in bytes
    bool m_valid = false;           ///< Is this a valid load request?
    bool m_complete = false;        ///< Has the load completed?
    bool m_isLocked = false;        ///< Is this a locked load (LDx_L)?
    bool m_isFloatingPoint = false; ///< Is this a floating-point load?
    bool m_isUnaligned = false;     ///< Is this an unaligned load?

    // Error handling
    quint32 m_errorCode = 0; ///< Error code (0 = no error)
    QString m_errorMessage;  ///< Human-readable error message

    // Performance tracking
    quint64 m_startTime = 0;      ///< When the load was started (ms)
    quint64 m_completionTime = 0; ///< When the load completed (ms)
};

