#pragma once

#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"
#include <QtCore/QAtomicInteger>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QtAlgorithms>

/*

tlbPerformanceOptimizer uses QTimer for periodic optimization cycles and maintains detailed statistics to measure the
effectiveness of each strategy. It can dynamically switch between strategies based on collision reduction targets and prefetch efficiency
thresholds.
*/
class tlbPerformanceOptimizer : public QObject
{
    Q_OBJECT

  public:
    enum OptimizationStrategy
    {
        STRATEGY_DISABLED = 0,
        STRATEGY_BANKING,
        STRATEGY_PREFETCH,
        STRATEGY_VICTIM_CACHE,
        STRATEGY_ADAPTIVE_REPLACEMENT,
        STRATEGY_PROCESS_PARTITIONING
    };

    enum BankingMode
    {
        NO_BANKING = 0,
        DUAL_BANK,
        QUAD_BANK,
        OCTAL_BANK
    };

    enum PrefetchPattern
    {
        NO_PREFETCH = 0,
        SEQUENTIAL_PREFETCH,
        STRIDE_PREFETCH,
        PATTERN_PREFETCH,
        ADAPTIVE_PREFETCH
    };

    struct BankConfiguration
    {
        qint32 bankCount;
        qint32 entriesPerBank;
        BankingMode mode;
        qint32 loadBankMask;
        qint32 storeBankMask;

        BankConfiguration() : bankCount(1), entriesPerBank(64), mode(NO_BANKING), loadBankMask(0), storeBankMask(0) {}
    };

    struct PrefetchEntry
    {
        quint64 virtualAddress;
        quint32 processId;
        quint64 prefetchTime;
        qint32 confidence;
        bool isActive;

        PrefetchEntry() : virtualAddress(0), processId(0), prefetchTime(0), confidence(0), isActive(false) {}
    };

    struct AccessPattern
    {
        quint64 lastAddress;
        qint64 stride;
        qint32 sequentialCount;
        qint32 confidence;
        quint64 lastAccessTime;

        AccessPattern() : lastAddress(0), stride(0), sequentialCount(0), confidence(0), lastAccessTime(0) {}
    };

  private:
    static const qint32 MAX_PREFETCH_ENTRIES = 16;
    static const qint32 MAX_PATTERN_HISTORY = 8;
    static const qint32 PREFETCH_CONFIDENCE_THRESHOLD = 3;
    static const qint32 OPTIMIZATION_INTERVAL_MS = 100;
    //static const qreal COLLISION_REDUCTION_TARGET = 0.5; // 50% reduction goal
    static constexpr qreal COLLISION_REDUCTION_TARGET = 0.5; // 50% reduction goal

    BankConfiguration m_bankConfig;
    PrefetchPattern m_prefetchPattern;
    OptimizationStrategy m_activeStrategy;

    PrefetchEntry m_prefetchTable[MAX_PREFETCH_ENTRIES];
    QHash<quint32, AccessPattern> m_processPatterns; // Process ID -> Pattern
    QMutex m_optimizationMutex;
    QMutex m_prefetchMutex;
    QMutex m_patternMutex;

    QTimer *m_optimizationTimer;

    QAtomicInteger<quint64> m_collisionReductions;
    QAtomicInteger<quint64> m_successfulPrefetches;
    QAtomicInteger<quint64> m_wastedPrefetches;
    QAtomicInteger<quint64> m_bankSwitches;
    QAtomicInteger<quint64> m_optimizationCycles;

    qreal m_baselineCollisionRate;
    qreal m_currentCollisionRate;
    bool m_initialized;

  public:
    explicit tlbPerformanceOptimizer(QObject *parent = nullptr)
        : QObject(parent), m_prefetchPattern(NO_PREFETCH), m_activeStrategy(STRATEGY_DISABLED),
          m_optimizationTimer(nullptr), m_collisionReductions(0), m_successfulPrefetches(0), m_wastedPrefetches(0),
          m_bankSwitches(0), m_optimizationCycles(0), m_baselineCollisionRate(0.0), m_currentCollisionRate(0.0),
          m_initialized(false)
    {
        initialize();
    }

    ~tlbPerformanceOptimizer()
    {
        if (m_optimizationTimer)
        {
            m_optimizationTimer->stop();
            delete m_optimizationTimer;
        }

        quint64 totalPrefetches = m_successfulPrefetches.loadAcquire() + m_wastedPrefetches.loadAcquire();
        qreal prefetchEfficiency = totalPrefetches > 0 ? static_cast<qreal>(m_successfulPrefetches.loadAcquire()) /
                                                             static_cast<qreal>(totalPrefetches)
                                                       : 0.0;

        DEBUG_LOG(QString("tlbPerformanceOptimizer destroyed - Prefetch efficiency: %1%%, Collision reductions: %2")
                      .arg(prefetchEfficiency * 100.0)
                      .arg(             m_collisionReductions.loadAcquire()));
    }

    void initialize()
    {
        if (m_initialized)
            return;

        // Initialize banking configuration
        m_bankConfig = BankConfiguration();

        // Clear prefetch table
        for (qint32 i = 0; i < MAX_PREFETCH_ENTRIES; ++i)
        {
            m_prefetchTable[i] = PrefetchEntry();
        }

        // Initialize optimization timer
        m_optimizationTimer = new QTimer(this);
        connect(m_optimizationTimer, &QTimer::timeout, this, &tlbPerformanceOptimizer::performOptimizationCycle);

        m_initialized = true;
        DEBUG_LOG("tlbPerformanceOptimizer initialized");
    }

    void initialize_SignalsAndSlots()
    {
        // Connect optimization strategy signals
        if (m_optimizationTimer)
        {
            connect(m_optimizationTimer, &QTimer::timeout, this, &tlbPerformanceOptimizer::performOptimizationCycle);
        }
    }

    void enableOptimization(OptimizationStrategy strategy)
    {
        QMutexLocker locker(&m_optimizationMutex);

        m_activeStrategy = strategy;

        switch (strategy)
        {
        case STRATEGY_BANKING:
            configureBanking(DUAL_BANK);
            break;
        case STRATEGY_PREFETCH:
            m_prefetchPattern = SEQUENTIAL_PREFETCH;
            break;
        case STRATEGY_ADAPTIVE_REPLACEMENT:
            startOptimizationTimer();
            break;
        default:
            break;
        }

        DEBUG_LOG("Optimization strategy enabled: %d", strategy);
        emit sigOptimizationEnabled(strategy);
    }

    void disableOptimization()
    {
        QMutexLocker locker(&m_optimizationMutex);

        m_activeStrategy = STRATEGY_DISABLED;
        stopOptimizationTimer();

        DEBUG_LOG("Optimization disabled");
        emit sigOptimizationDisabled();
    }

    qint32 calculateOptimalBank(quint64 virtualAddress, bool isLoad) const
    {
        if (m_bankConfig.mode == NO_BANKING)
        {
            return 0;
        }

        // Hash virtual address to distribute across banks
        quint32 addressHash = qHash(virtualAddress >> 12); // Page-aligned hashing
        qint32 baseBank = addressHash % m_bankConfig.bankCount;

        // Apply load/store bank preferences to reduce collisions
        if (isLoad && m_bankConfig.loadBankMask != 0)
        {
            return baseBank & m_bankConfig.loadBankMask;
        }
        else if (!isLoad && m_bankConfig.storeBankMask != 0)
        {
            return baseBank & m_bankConfig.storeBankMask;
        }

        return baseBank;
    }

    bool shouldPrefetch(quint64 virtualAddress, quint32 processId)
    {
        if (m_prefetchPattern == NO_PREFETCH)
        {
            return false;
        }

        QMutexLocker patternLock(&m_patternMutex);

        if (!m_processPatterns.contains(processId))
        {
            // Initialize pattern tracking for new process
            m_processPatterns[processId] = AccessPattern();
            m_processPatterns[processId].lastAddress = virtualAddress;
            m_processPatterns[processId].lastAccessTime = QDateTime::currentMSecsSinceEpoch();
            return false;
        }

        AccessPattern &pattern = m_processPatterns[processId];
        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        // Calculate stride and update pattern
        qint64 newStride = static_cast<qint64>(virtualAddress) - static_cast<qint64>(pattern.lastAddress);

        if (pattern.stride == newStride && pattern.stride != 0)
        {
            pattern.sequentialCount++;
            pattern.confidence = qMin(pattern.confidence + 1, 10);
        }
        else
        {
            pattern.stride = newStride;
            pattern.sequentialCount = 1;
            pattern.confidence = qMax(pattern.confidence - 1, 0);
        }

        pattern.lastAddress = virtualAddress;
        pattern.lastAccessTime = currentTime;

        // Decide whether to prefetch based on confidence
        bool shouldPrefetchNext = pattern.confidence >= PREFETCH_CONFIDENCE_THRESHOLD;

        if (shouldPrefetchNext)
        {
            quint64 prefetchAddress = virtualAddress + static_cast<quint64>(pattern.stride);
            issuePrefetch(prefetchAddress, processId);
        }

        return shouldPrefetchNext;
    }

    void recordAccess(quint64 virtualAddress, quint32 processId, bool wasHit, bool wasPrefetched)
    {
        if (wasPrefetched)
        {
            if (wasHit)
            {
                m_successfulPrefetches.fetchAndAddAcquire(1);
                DEBUG_LOG(QString("Successful prefetch: VA=0x%1, PID=%2").arg(virtualAddress).arg(processId));
            }
            else
            {
                m_wastedPrefetches.fetchAndAddAcquire(1);
            }
        }

        // Update access patterns for future prefetch decisions
        shouldPrefetch(virtualAddress, processId);
    }

    void recordCollisionReduction(quint64 virtualAddress, qint32 bankId)
    {
        m_collisionReductions.fetchAndAddAcquire(1);
        m_bankSwitches.fetchAndAddAcquire(1);

        DEBUG_LOG("Collision reduced via banking: VA=0x%llx, Bank=%d", virtualAddress, bankId);
        emit sigCollisionReduced(virtualAddress, bankId);
    }

    void updateCollisionRate(qreal newRate)
    {
        QMutexLocker locker(&m_optimizationMutex);

        if (m_baselineCollisionRate == 0.0)
        {
            m_baselineCollisionRate = newRate;
        }

        m_currentCollisionRate = newRate;

        // Adaptive strategy adjustment
        if (m_activeStrategy == STRATEGY_ADAPTIVE_REPLACEMENT)
        {
            adaptStrategy();
        }
    }

    BankConfiguration getBankConfiguration() const
    {
        QMutexLocker locker(&m_optimizationMutex);
        return m_bankConfig;
    }

    PrefetchPattern getPrefetchPattern() const { return m_prefetchPattern; }
    OptimizationStrategy getActiveStrategy() const { return m_activeStrategy; }

    // Performance metrics
    qreal getCollisionReductionRatio() const
    {
        if (m_baselineCollisionRate == 0.0)
            return 0.0;
        return 1.0 - (m_currentCollisionRate / m_baselineCollisionRate);
    }

    qreal getPrefetchEfficiency() const
    {
        quint64 total = m_successfulPrefetches.loadAcquire() + m_wastedPrefetches.loadAcquire();
        if (total == 0)
            return 0.0;
        return static_cast<qreal>(m_successfulPrefetches.loadAcquire()) / static_cast<qreal>(total);
    }

    // Statistics accessors
    quint64 getCollisionReductions() const { return m_collisionReductions.loadAcquire(); }
    quint64 getSuccessfulPrefetches() const { return m_successfulPrefetches.loadAcquire(); }
    quint64 getWastedPrefetches() const { return m_wastedPrefetches.loadAcquire(); }
    quint64 getBankSwitches() const { return m_bankSwitches.loadAcquire(); }
    quint64 getOptimizationCycles() const { return m_optimizationCycles.loadAcquire(); }

    void resetStatistics()
    {
        m_collisionReductions.storeRelease(0);
        m_successfulPrefetches.storeRelease(0);
        m_wastedPrefetches.storeRelease(0);
        m_bankSwitches.storeRelease(0);
        m_optimizationCycles.storeRelease(0);
        m_baselineCollisionRate = 0.0;
        m_currentCollisionRate = 0.0;
        DEBUG_LOG("Performance optimization statistics reset");
    }

  signals:
    void sigOptimizationEnabled(OptimizationStrategy strategy);
    void sigOptimizationDisabled();
    void sigPrefetchIssued(quint64 virtualAddress, quint32 processId);
    void sigCollisionReduced(quint64 virtualAddress, qint32 bankId);
    void sigStrategyAdapted(OptimizationStrategy oldStrategy, OptimizationStrategy newStrategy);

  private slots:
    void performOptimizationCycle()
    {
        m_optimizationCycles.fetchAndAddAcquire(1);

        // Periodic optimization analysis
        qreal reductionRatio = getCollisionReductionRatio();

        if (reductionRatio < COLLISION_REDUCTION_TARGET)
        {
            DEBUG_LOG(QString("Optimization cycle: Current reduction %1%% below target %2%%")
                          .arg(reductionRatio * 100.0)
                          .arg(                   COLLISION_REDUCTION_TARGET * 100.0));

            if (m_activeStrategy == STRATEGY_BANKING)
            {
                // Consider upgrading banking mode
                if (m_bankConfig.mode < OCTAL_BANK)
                {
                    configureBanking(static_cast<BankingMode>(m_bankConfig.mode + 1));
                }
            }
        }
    }

  private:
    void configureBanking(BankingMode mode)
    {
        m_bankConfig.mode = mode;

        switch (mode)
        {
        case DUAL_BANK:
            m_bankConfig.bankCount = 2;
            m_bankConfig.entriesPerBank = 32;
            m_bankConfig.loadBankMask = 0x1;  // Even banks for loads
            m_bankConfig.storeBankMask = 0x0; // Odd banks for stores
            break;
        case QUAD_BANK:
            m_bankConfig.bankCount = 4;
            m_bankConfig.entriesPerBank = 16;
            m_bankConfig.loadBankMask = 0x3;  // Banks 0,1 prefer loads
            m_bankConfig.storeBankMask = 0x2; // Banks 2,3 prefer stores
            break;
        case OCTAL_BANK:
            m_bankConfig.bankCount = 8;
            m_bankConfig.entriesPerBank = 8;
            m_bankConfig.loadBankMask = 0x7;  // Banks 0-3 prefer loads
            m_bankConfig.storeBankMask = 0x4; // Banks 4-7 prefer stores
            break;
        default:
            m_bankConfig.bankCount = 1;
            m_bankConfig.entriesPerBank = 64;
            m_bankConfig.loadBankMask = 0;
            m_bankConfig.storeBankMask = 0;
            break;
        }

        DEBUG_LOG(QString("Banking configured: Mode=%1, Banks=%2 Entries/Bank=%3")
                      .arg(mode)
                      .arg(m_bankConfig.bankCount)
                      .arg(               m_bankConfig.entriesPerBank));
        emit sigStrategyAdapted(m_activeStrategy, STRATEGY_BANKING);
    }

    void issuePrefetch(quint64 virtualAddress, quint32 processId)
    {
        QMutexLocker locker(&m_prefetchMutex);

        // Find free prefetch slot
        for (qint32 i = 0; i < MAX_PREFETCH_ENTRIES; ++i)
        {
            if (!m_prefetchTable[i].isActive)
            {
                m_prefetchTable[i].virtualAddress = virtualAddress;
                m_prefetchTable[i].processId = processId;
                m_prefetchTable[i].prefetchTime = QDateTime::currentMSecsSinceEpoch();
                m_prefetchTable[i].confidence = PREFETCH_CONFIDENCE_THRESHOLD;
                m_prefetchTable[i].isActive = true;

                DEBUG_LOG(
                    QString("Prefetch issued: VA=0x%1, PID=%2, Slot=%3").arg(virtualAddress).arg(processId).arg( i));
                emit sigPrefetchIssued(virtualAddress, processId);
                break;
            }
        }
    }

    void adaptStrategy()
    {
        // Adaptive strategy logic based on current performance
        qreal efficiency = getPrefetchEfficiency();
        qreal reductionRatio = getCollisionReductionRatio();

        if (reductionRatio < 0.3 && efficiency < 0.5)
        {
            // Poor performance - try different strategy
            OptimizationStrategy newStrategy =
                (m_activeStrategy == STRATEGY_BANKING) ? STRATEGY_PREFETCH : STRATEGY_BANKING;

            OptimizationStrategy oldStrategy = m_activeStrategy;
            enableOptimization(newStrategy);

            DEBUG_LOG(QString("Strategy adapted: %1 -> %2 (Reduction: %.3%%, Efficiency: %.4%%)")
                              .arg(oldStrategy)
                              .arg(newStrategy)
                          .arg(reductionRatio * 100.0)
                          .arg(               efficiency * 100.0));
            emit sigStrategyAdapted(oldStrategy, newStrategy);
        }
    }

    void startOptimizationTimer()
    {
        if (m_optimizationTimer && !m_optimizationTimer->isActive())
        {
            m_optimizationTimer->start(OPTIMIZATION_INTERVAL_MS);
            DEBUG_LOG(QString("Optimization timer started (%1 ms interval)").arg( OPTIMIZATION_INTERVAL_MS));
        }
    }

    void stopOptimizationTimer()
    {
        if (m_optimizationTimer && m_optimizationTimer->isActive())
        {
            m_optimizationTimer->stop();
            DEBUG_LOG("Optimization timer stopped");
        }
    }
};
