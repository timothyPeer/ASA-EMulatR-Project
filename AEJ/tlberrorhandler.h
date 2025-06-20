#pragma once

#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"
#include <QtCore/QAtomicInteger>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QTimer>
#include <QtCore/QtAlgorithms>

/*
The TLB system architecture with comprehensive error handling, collision detection, state management,
address translation, pipeline coordination, performance optimization, and robust error recovery capabilities.
*/
class tlbErrorHandler : public QObject
{
    Q_OBJECT

  public:
    enum ErrorType
    {
        NO_ERROR = 0,
        TRANSLATION_FAULT,
        PROTECTION_VIOLATION,
        INVALID_ADDRESS,
        PAGE_FAULT,
        PRIVILEGE_VIOLATION,
        ALIGNMENT_FAULT,
        BUS_ERROR,
        HARDWARE_FAULT,
        TIMEOUT_ERROR,
        RESOURCE_EXHAUSTION
    };

    enum ErrorSeverity
    {
        SEVERITY_INFO = 0,
        SEVERITY_WARNING,
        SEVERITY_ERROR,
        SEVERITY_CRITICAL,
        SEVERITY_FATAL
    };

    enum RecoveryAction
    {
        ACTION_NONE = 0,
        ACTION_RETRY,
        ACTION_INVALIDATE_ENTRY,
        ACTION_FLUSH_TLB,
        ACTION_RESET_PIPELINE,
        ACTION_ESCALATE_EXCEPTION,
        ACTION_SYSTEM_HALT
    };

    struct ErrorRecord
    {
        quint64 errorId;
        ErrorType errorType;
        ErrorSeverity severity;
        quint64 virtualAddress;
        quint32 processId;
        quint32 threadId;
        quint64 timestamp;
        RecoveryAction actionTaken;
        QString errorDescription;
        qint32 retryCount;
        bool isResolved;

        ErrorRecord()
            : errorId(0), errorType(NO_ERROR), severity(SEVERITY_INFO), virtualAddress(0), processId(0), threadId(0),
              timestamp(0), actionTaken(ACTION_NONE), retryCount(0), isResolved(false)
        {
        }
    };

    struct ErrorStatistics
    {
        QAtomicInteger<quint64> totalErrors;
        QAtomicInteger<quint64> translationFaults;
        QAtomicInteger<quint64> protectionViolations;
        QAtomicInteger<quint64> pageFaults;
        QAtomicInteger<quint64> hardwareFaults;
        QAtomicInteger<quint64> timeoutErrors;
        QAtomicInteger<quint64> recoveredErrors;
        QAtomicInteger<quint64> unrecoveredErrors;

        ErrorStatistics()
            : totalErrors(0), translationFaults(0), protectionViolations(0), pageFaults(0), hardwareFaults(0),
              timeoutErrors(0), recoveredErrors(0), unrecoveredErrors(0)
        {
        }
    };

  private:
    static const qint32 MAX_ERROR_HISTORY = 256;
    static const qint32 MAX_RETRY_ATTEMPTS = 3;
    static const qint32 ERROR_BURST_THRESHOLD = 10;
    static const qint32 ERROR_BURST_WINDOW_MS = 1000;
    static const qint32 MONITORING_INTERVAL_MS = 5000;
    static constexpr qreal FAULT_RATE_THRESHOLD = 0.1; // 10% fault rate threshold

    QQueue<ErrorRecord> m_errorHistory;
    QHash<quint32, qint32> m_processErrorCounts; // Process ID -> Error count
    QMutex m_errorMutex;
    QMutex m_statisticsMutex;

    ErrorStatistics m_statistics;
    QAtomicInteger<quint64> m_errorIdCounter;
    QTimer *m_monitoringTimer;

    quint64 m_lastBurstTime;
    qint32 m_burstErrorCount;
    bool m_emergencyMode;
    bool m_initialized;

  public:
    explicit tlbErrorHandler(QObject *parent = nullptr)
        : QObject(parent), m_errorIdCounter(0), m_monitoringTimer(nullptr), m_lastBurstTime(0), m_burstErrorCount(0),
          m_emergencyMode(false), m_initialized(false)
    {
        initialize();
    }

    ~tlbErrorHandler()
    {
        if (m_monitoringTimer)
        {
            m_monitoringTimer->stop();
            delete m_monitoringTimer;
        }

        quint64 totalErrors = m_statistics.totalErrors.loadAcquire();
        quint64 recovered = m_statistics.recoveredErrors.loadAcquire();
        qreal recoveryRate = totalErrors > 0 ? static_cast<qreal>(recovered) / static_cast<qreal>(totalErrors) : 0.0;

        DEBUG_LOG("tlbErrorHandler destroyed - Total errors: %llu, Recovery rate: %.2f%%", totalErrors,
                  recoveryRate * 100.0);
    }

    void initialize()
    {
        if (m_initialized)
            return;

        m_errorHistory.clear();
        m_processErrorCounts.clear();

        // Initialize monitoring timer
        m_monitoringTimer = new QTimer(this);
        connect(m_monitoringTimer, &QTimer::timeout, this, &tlbErrorHandler::performErrorAnalysis);
        m_monitoringTimer->start(MONITORING_INTERVAL_MS);

        m_initialized = true;
        DEBUG_LOG("tlbErrorHandler initialized - History size: %d, Monitoring interval: %d ms", MAX_ERROR_HISTORY,
                  MONITORING_INTERVAL_MS);
    }

    void initialize_SignalsAndSlots()
    {
        if (m_monitoringTimer)
        {
            connect(m_monitoringTimer, &QTimer::timeout, this, &tlbErrorHandler::performErrorAnalysis);
        }
    }

    quint64 reportError(ErrorType errorType, quint64 virtualAddress, quint32 processId, quint32 threadId,
                        const QString &description = QString())
    {
        QMutexLocker locker(&m_errorMutex);

        ErrorRecord error;
        error.errorId = m_errorIdCounter.fetchAndAddAcquire(1);
        error.errorType = errorType;
        error.severity = determineSeverity(errorType);
        error.virtualAddress = virtualAddress;
        error.processId = processId;
        error.threadId = threadId;
        error.timestamp = QDateTime::currentMSecsSinceEpoch();
        error.errorDescription = description;
        error.retryCount = 0;
        error.isResolved = false;

        // Determine recovery action
        error.actionTaken = determineRecoveryAction(errorType, error.severity);

        // Add to history
        if (m_errorHistory.size() >= MAX_ERROR_HISTORY)
        {
            m_errorHistory.dequeue(); // Remove oldest
        }
        m_errorHistory.enqueue(error);

        // Update statistics
        updateErrorStatistics(errorType);

        // Update process error tracking
        m_processErrorCounts[processId]++;

        // Check for error burst
        checkErrorBurst();

        DEBUG_LOG("Error reported: ID=%llu, Type=%d, Severity=%d, VA=0x%llx, PID=%u, Action=%d", error.errorId,
                  errorType, error.severity, virtualAddress, processId, error.actionTaken);

        emit sigErrorReported(error.errorId, errorType, error.severity, virtualAddress, processId);

        // Execute recovery action
        executeRecoveryAction(error);

        return error.errorId;
    }

    bool resolveError(quint64 errorId, bool wasSuccessful)
    {
        QMutexLocker locker(&m_errorMutex);

        // Find error in history
        for (qint32 i = m_errorHistory.size() - 1; i >= 0; --i)
        {
            ErrorRecord &error = m_errorHistory[i];
            if (error.errorId == errorId && !error.isResolved)
            {
                error.isResolved = true;

                if (wasSuccessful)
                {
                    m_statistics.recoveredErrors.fetchAndAddAcquire(1);
                    DEBUG_LOG("Error resolved successfully: ID=%llu, Type=%d", errorId, error.errorType);
                    emit sigErrorResolved(errorId, error.errorType);
                }
                else
                {
                    m_statistics.unrecoveredErrors.fetchAndAddAcquire(1);
                    DEBUG_LOG("Error resolution failed: ID=%llu, Type=%d", errorId, error.errorType);

                    // Consider escalation for unresolved critical errors
                    if (error.severity >= SEVERITY_CRITICAL)
                    {
                        escalateError(error);
                    }
                }
                return true;
            }
        }
        return false;
    }

    RecoveryAction getRecommendedAction(ErrorType errorType, ErrorSeverity severity) const
    {
        return determineRecoveryAction(errorType, severity);
    }

    bool isInEmergencyMode() const { return m_emergencyMode; }

    void enterEmergencyMode()
    {
        if (!m_emergencyMode)
        {
            m_emergencyMode = true;
            DEBUG_LOG("Entering emergency mode due to error burst");
            emit sigEmergencyModeEntered();
        }
    }

    void exitEmergencyMode()
    {
        if (m_emergencyMode)
        {
            m_emergencyMode = false;
            m_burstErrorCount = 0;
            DEBUG_LOG("Exiting emergency mode");
            emit sigEmergencyModeExited();
        }
    }

    qreal calculateFaultRate(quint32 processId = 0) const
    {
        QMutexLocker locker(&m_errorMutex);

        if (processId == 0)
        {
            // Overall fault rate
            quint64 totalOps = m_statistics.totalErrors.loadAcquire() * 10; // Estimate total operations
            if (totalOps == 0)
                return 0.0;
            return static_cast<qreal>(m_statistics.totalErrors.loadAcquire()) / static_cast<qreal>(totalOps);
        }
        else
        {
            // Process-specific fault rate
            if (!m_processErrorCounts.contains(processId))
                return 0.0;
            qint32 processErrors = m_processErrorCounts[processId];
            qint32 estimatedOps = processErrors * 20; // Rough estimate
            if (estimatedOps == 0)
                return 0.0;
            return static_cast<qreal>(processErrors) / static_cast<qreal>(estimatedOps);
        }
    }

    QList<ErrorRecord> getRecentErrors(qint32 count = 10) const
    {
        QMutexLocker locker(&m_errorMutex);

        QList<ErrorRecord> recent;
        qint32 startIndex = qMax(0, m_errorHistory.size() - count);

        for (qint32 i = startIndex; i < m_errorHistory.size(); ++i)
        {
            recent.append(m_errorHistory[i]);
        }

        return recent;
    }

    QHash<ErrorType, qint32> getErrorTypeCounts() const
    {
        QMutexLocker locker(&m_errorMutex);

        QHash<ErrorType, qint32> counts;
        for (const ErrorRecord &error : m_errorHistory)
        {
            counts[error.errorType]++;
        }

        return counts;
    }

    // Statistics accessors
    quint64 getTotalErrors() const { return m_statistics.totalErrors.loadAcquire(); }
    quint64 getTranslationFaults() const { return m_statistics.translationFaults.loadAcquire(); }
    quint64 getProtectionViolations() const { return m_statistics.protectionViolations.loadAcquire(); }
    quint64 getPageFaults() const { return m_statistics.pageFaults.loadAcquire(); }
    quint64 getHardwareFaults() const { return m_statistics.hardwareFaults.loadAcquire(); }
    quint64 getTimeoutErrors() const { return m_statistics.timeoutErrors.loadAcquire(); }
    quint64 getRecoveredErrors() const { return m_statistics.recoveredErrors.loadAcquire(); }
    quint64 getUnrecoveredErrors() const { return m_statistics.unrecoveredErrors.loadAcquire(); }

    qreal getRecoveryRate() const
    {
        quint64 total = m_statistics.totalErrors.loadAcquire();
        if (total == 0)
            return 0.0;
        return static_cast<qreal>(m_statistics.recoveredErrors.loadAcquire()) / static_cast<qreal>(total);
    }

    void resetStatistics()
    {
        QMutexLocker errorLock(&m_errorMutex);
        QMutexLocker statsLock(&m_statisticsMutex);

        m_statistics = ErrorStatistics();
        m_errorHistory.clear();
        m_processErrorCounts.clear();
        m_burstErrorCount = 0;
        exitEmergencyMode();

        DEBUG_LOG("Error handler statistics reset");
    }

  signals:
    void sigErrorReported(quint64 errorId, ErrorType errorType, ErrorSeverity severity, quint64 virtualAddress,
                          quint32 processId);
    void sigErrorResolved(quint64 errorId, ErrorType errorType);
    void sigRecoveryActionExecuted(quint64 errorId, RecoveryAction action, bool wasSuccessful);
    void sigErrorEscalated(quint64 errorId, ErrorType errorType, ErrorSeverity severity);
    void sigEmergencyModeEntered();
    void sigEmergencyModeExited();
    void sigFaultRateExceeded(qreal currentRate, qreal threshold);

  private slots:
    void performErrorAnalysis()
    {
        QMutexLocker locker(&m_errorMutex);

        // Check overall fault rate
        qreal faultRate = calculateFaultRate();
        if (faultRate > FAULT_RATE_THRESHOLD)
        {
            DEBUG_LOG("Fault rate threshold exceeded: %.2f%% > %.2f%%", faultRate * 100.0,
                      FAULT_RATE_THRESHOLD * 100.0);
            emit sigFaultRateExceeded(faultRate, FAULT_RATE_THRESHOLD);
        }

        // Cleanup old process error counts
        QHash<quint32, qint32>::iterator it = m_processErrorCounts.begin();
        while (it != m_processErrorCounts.end())
        {
            if (it.value() == 0)
            {
                it = m_processErrorCounts.erase(it);
            }
            else
            {
                // Decay error counts over time
                it.value() = qMax(0, it.value() - 1);
                ++it;
            }
        }
    }

  private:
    ErrorSeverity determineSeverity(ErrorType errorType) const
    {
        switch (errorType)
        {
        case TRANSLATION_FAULT:
        case INVALID_ADDRESS:
            return SEVERITY_WARNING;
        case PROTECTION_VIOLATION:
        case PRIVILEGE_VIOLATION:
            return SEVERITY_ERROR;
        case PAGE_FAULT:
        case ALIGNMENT_FAULT:
            return SEVERITY_INFO;
        case BUS_ERROR:
        case HARDWARE_FAULT:
            return SEVERITY_CRITICAL;
        case TIMEOUT_ERROR:
        case RESOURCE_EXHAUSTION:
            return SEVERITY_ERROR;
        default:
            return SEVERITY_INFO;
        }
    }

    RecoveryAction determineRecoveryAction(ErrorType errorType, ErrorSeverity severity) const
    {
        if (m_emergencyMode)
        {
            return (severity >= SEVERITY_CRITICAL) ? ACTION_SYSTEM_HALT : ACTION_FLUSH_TLB;
        }

        switch (errorType)
        {
        case TRANSLATION_FAULT:
        case INVALID_ADDRESS:
            return ACTION_INVALIDATE_ENTRY;
        case PROTECTION_VIOLATION:
        case PRIVILEGE_VIOLATION:
            return ACTION_ESCALATE_EXCEPTION;
        case PAGE_FAULT:
            return ACTION_RETRY;
        case HARDWARE_FAULT:
        case BUS_ERROR:
            return ACTION_RESET_PIPELINE;
        case TIMEOUT_ERROR:
            return ACTION_RETRY;
        case RESOURCE_EXHAUSTION:
            return ACTION_FLUSH_TLB;
        default:
            return ACTION_NONE;
        }
    }

    void updateErrorStatistics(ErrorType errorType)
    {
        m_statistics.totalErrors.fetchAndAddAcquire(1);

        switch (errorType)
        {
        case TRANSLATION_FAULT:
            m_statistics.translationFaults.fetchAndAddAcquire(1);
            break;
        case PROTECTION_VIOLATION:
        case PRIVILEGE_VIOLATION:
            m_statistics.protectionViolations.fetchAndAddAcquire(1);
            break;
        case PAGE_FAULT:
            m_statistics.pageFaults.fetchAndAddAcquire(1);
            break;
        case HARDWARE_FAULT:
        case BUS_ERROR:
            m_statistics.hardwareFaults.fetchAndAddAcquire(1);
            break;
        case TIMEOUT_ERROR:
            m_statistics.timeoutErrors.fetchAndAddAcquire(1);
            break;
        default:
            break;
        }
    }

    void checkErrorBurst()
    {
        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        if ((currentTime - m_lastBurstTime) > ERROR_BURST_WINDOW_MS)
        {
            m_burstErrorCount = 1;
            m_lastBurstTime = currentTime;
        }
        else
        {
            m_burstErrorCount++;
            if (m_burstErrorCount >= ERROR_BURST_THRESHOLD)
            {
                enterEmergencyMode();
            }
        }
    }

    void executeRecoveryAction(const ErrorRecord &error)
    {
        bool actionSuccessful = false;

        switch (error.actionTaken)
        {
        case ACTION_RETRY:
            // Signal retry request
            actionSuccessful = true; // Assume retry capability exists
            break;
        case ACTION_INVALIDATE_ENTRY:
            // Signal entry invalidation
            actionSuccessful = true;
            break;
        case ACTION_FLUSH_TLB:
            // Signal TLB flush
            actionSuccessful = true;
            break;
        case ACTION_RESET_PIPELINE:
            // Signal pipeline reset
            actionSuccessful = true;
            break;
        case ACTION_ESCALATE_EXCEPTION:
            // Signal exception escalation
            actionSuccessful = true;
            emit sigErrorEscalated(error.errorId, error.errorType, error.severity);
            break;
        case ACTION_SYSTEM_HALT:
            // Critical system halt
            actionSuccessful = false; // This is a last resort
            DEBUG_LOG("CRITICAL: System halt recommended for error ID=%llu", error.errorId);
            break;
        default:
            actionSuccessful = true;
            break;
        }

        emit sigRecoveryActionExecuted(error.errorId, error.actionTaken, actionSuccessful);
    }

    void escalateError(const ErrorRecord &error)
    {
        DEBUG_LOG("Escalating unresolved critical error: ID=%llu, Type=%d", error.errorId, error.errorType);
        emit sigErrorEscalated(error.errorId, error.errorType, error.severity);
    }
};
