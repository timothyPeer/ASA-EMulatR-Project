#pragma once

#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"
#include <QtCore/QAtomicInteger>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QtAlgorithms>

// Include all TLB subsystem components
#include "tlbAddressTranslator.h"
#include "tlbCollisionDetector.h"
#include "tlbEntryStateManager.h"
#include "tlbErrorHandler.h"
#include "tlbPerformanceOptimizer.h"
#include "tlbPipelineCoordinator.h"
#include "tlbSystem.h"

class tlbSystemCoordinator : public QObject
{
    Q_OBJECT

  public:
    enum SystemState
    {
        STATE_UNINITIALIZED = 0,
        STATE_INITIALIZING,
        STATE_OPERATIONAL,
        STATE_DEGRADED,
        STATE_EMERGENCY,
        STATE_MAINTENANCE,
        STATE_SHUTDOWN
    };

    enum TlbOperation
    {
        OP_TRANSLATE = 0,
        OP_INVALIDATE,
        OP_FLUSH,
        OP_PREFETCH,
        OP_MAINTENANCE
    };

    struct TlbRequest
    {
        quint64 requestId;
        TlbOperation operation;
        quint64 virtualAddress;
        quint32 processId;
        quint32 threadId;
        bool isLoad;
        bool isStore;
        bool isExecute;
        bool isHighPriority;
        quint64 submissionTime;

        TlbRequest()
            : requestId(0), operation(OP_TRANSLATE), virtualAddress(0), processId(0), threadId(0), isLoad(false),
              isStore(false), isExecute(false), isHighPriority(false), submissionTime(0)
        {
        }
    };

    struct TlbResponse
    {
        quint64 requestId;
        bool wasSuccessful;
        quint64 physicalAddress;
        quint32 tbIndex;
        tlbAddressTranslator::TranslationResult result;
        tlbErrorHandler::ErrorType errorType;
        quint64 processingTime;

        TlbResponse()
            : requestId(0), wasSuccessful(false), physicalAddress(0), tbIndex(0),
              result(tlbAddressTranslator::TRANSLATION_FAULT), errorType(tlbErrorHandler::NO_ERROR), processingTime(0)
        {
        }
    };

    struct SystemStatistics
    {
        QAtomicInteger<quint64> totalRequests;
        QAtomicInteger<quint64> successfulTranslations;
        QAtomicInteger<quint64> failedTranslations;
        QAtomicInteger<quint64> collisionEvents;
        QAtomicInteger<quint64> optimizationEvents;
        QAtomicInteger<quint64> errorEvents;

        SystemStatistics()
            : totalRequests(0), successfulTranslations(0), failedTranslations(0), collisionEvents(0),
              optimizationEvents(0), errorEvents(0)
        {
        }
    };

  private:
    // Core TLB subsystem components
    tlbCollisionDetector *m_collisionDetector;
    tlbEntryStateManager *m_entryStateManager;
    tlbAddressTranslator *m_addressTranslator;
    tlbPipelineCoordinator *m_pipelineCoordinator;
    tlbPerformanceOptimizer *m_performanceOptimizer;
    tlbErrorHandler *m_errorHandler;
    TLBSystem *m_tlbSystem;

    // System coordination
    SystemState m_systemState;
    QMutex m_systemMutex;
    QAtomicInteger<quint64> m_requestIdCounter;
    QTimer *m_maintenanceTimer;

    // Statistics and monitoring
    SystemStatistics m_statistics;
    quint64 m_initializationTime;
    quint64 m_lastMaintenanceTime;

    // Configuration
    bool m_optimizationEnabled;
    bool m_emergencyModeActive;
    bool m_initialized;

    static const qint32 MAINTENANCE_INTERVAL_MS = 30000;     // 30 seconds
    static constexpr qreal EMERGENCY_FAULT_THRESHOLD = 0.25; // 25% fault rate triggers emergency mode

  public:
    explicit tlbSystemCoordinator(QObject *parent = nullptr)
        : QObject(parent), m_collisionDetector(nullptr), m_entryStateManager(nullptr), m_addressTranslator(nullptr),
          m_pipelineCoordinator(nullptr), m_performanceOptimizer(nullptr), m_errorHandler(nullptr),
          m_systemState(STATE_UNINITIALIZED), m_requestIdCounter(0), m_maintenanceTimer(nullptr),
          m_initializationTime(0), m_lastMaintenanceTime(0), m_optimizationEnabled(true), m_emergencyModeActive(false),
          m_initialized(false)
    {
        initialize();
    }

    ~tlbSystemCoordinator()
    {
        if (m_maintenanceTimer)
        {
            m_maintenanceTimer->stop();
            delete m_maintenanceTimer;
        }

        // Cleanup subsystem components
        delete m_errorHandler;
        delete m_performanceOptimizer;
        delete m_pipelineCoordinator;
        delete m_addressTranslator;
        delete m_entryStateManager;
        delete m_collisionDetector;

        quint64 totalOps = m_statistics.totalRequests.loadAcquire();
        quint64 successful = m_statistics.successfulTranslations.loadAcquire();
        qreal successRate = totalOps > 0 ? static_cast<qreal>(successful) / static_cast<qreal>(totalOps) : 0.0;

        DEBUG_LOG("tlbSystem destroyed - Total requests: %llu, Success rate: %.2f%%", totalOps, successRate * 100.0);
    }

    void initialize()
    {
        if (m_initialized)
            return;

        QMutexLocker locker(&m_systemMutex);
        m_systemState = STATE_INITIALIZING;

        m_initializationTime = QDateTime::currentMSecsSinceEpoch();

        // Create subsystem components
        m_collisionDetector = new tlbCollisionDetector(this);
        m_entryStateManager = new tlbEntryStateManager(this);
        m_addressTranslator = new tlbAddressTranslator(this);
        m_pipelineCoordinator = new tlbPipelineCoordinator(this);
        m_performanceOptimizer = new tlbPerformanceOptimizer(this);
        m_errorHandler = new tlbErrorHandler(this);

        // Initialize all subsystems
        m_collisionDetector->initialize();
        m_entryStateManager->initialize();
        m_addressTranslator->initialize();
        m_pipelineCoordinator->initialize();
        m_performanceOptimizer->initialize();
        m_errorHandler->initialize();

        // Set up maintenance timer
        m_maintenanceTimer = new QTimer(this);
        m_maintenanceTimer->start(MAINTENANCE_INTERVAL_MS);

        m_initialized = true;
        m_systemState = STATE_OPERATIONAL;
        m_lastMaintenanceTime = m_initializationTime;

        DEBUG_LOG("tlbSystem initialized successfully - State: OPERATIONAL");
        emit sigSystemInitialized();
    }

    void initialize_SignalsAndSlots()
    {
        if (!m_initialized)
            return;

        // Initialize subsystem signal/slot connections
        m_collisionDetector->initialize_SignalsAndSlots();
        m_entryStateManager->initialize_SignalsAndSlots();
        m_addressTranslator->initialize_SignalsAndSlots();
        m_pipelineCoordinator->initialize_SignalsAndSlots();
        m_performanceOptimizer->initialize_SignalsAndSlots();
        m_errorHandler->initialize_SignalsAndSlots();

        // Connect inter-subsystem signals
        connectSubsystemSignals();

        // Connect maintenance timer
        if (m_maintenanceTimer)
        {
            connect(m_maintenanceTimer, &QTimer::timeout, this, &tlbSystem::performMaintenance);
        }

        DEBUG_LOG("tlbSystem signal/slot connections established");
    }

    TlbResponse processRequest(const TlbRequest &request)
    {
        QMutexLocker locker(&m_systemMutex);

        TlbResponse response;
        response.requestId = request.requestId;
        response.processingTime = QDateTime::currentMSecsSinceEpoch();

        m_statistics.totalRequests.fetchAndAddAcquire(1);

        // Check system state
        if (m_systemState != STATE_OPERATIONAL && m_systemState != STATE_DEGRADED)
        {
            response.wasSuccessful = false;
            response.errorType = tlbErrorHandler::RESOURCE_EXHAUSTION;
            m_statistics.failedTranslations.fetchAndAddAcquire(1);
            return response;
        }

        try
        {
            switch (request.operation)
            {
            case OP_TRANSLATE:
                response = processTranslationRequest(request);
                break;
            case OP_INVALIDATE:
                response = processInvalidationRequest(request);
                break;
            case OP_FLUSH:
                response = processFlushRequest(request);
                break;
            case OP_PREFETCH:
                response = processPrefetchRequest(request);
                break;
            case OP_MAINTENANCE:
                response = processMaintenanceRequest(request);
                break;
            default:
                response.wasSuccessful = false;
                response.errorType = tlbErrorHandler::INVALID_ADDRESS;
                break;
            }
        }
        catch (...)
        {
            response.wasSuccessful = false;
            response.errorType = tlbErrorHandler::HARDWARE_FAULT;
            handleSystemError("Exception caught in processRequest", request.virtualAddress, request.processId);
        }

        // Update statistics
        if (response.wasSuccessful)
        {
            m_statistics.successfulTranslations.fetchAndAddAcquire(1);
        }
        else
        {
            m_statistics.failedTranslations.fetchAndAddAcquire(1);
            m_statistics.errorEvents.fetchAndAddAcquire(1);
        }

        response.processingTime = QDateTime::currentMSecsSinceEpoch() - response.processingTime;

        emit sigRequestProcessed(response.requestId, response.wasSuccessful, response.processingTime);
        return response;
    }

    void enableOptimization(bool enable)
    {
        QMutexLocker locker(&m_systemMutex);
        m_optimizationEnabled = enable;

        if (m_performanceOptimizer)
        {
            if (enable)
            {
                m_performanceOptimizer->enableOptimization(tlbPerformanceOptimizer::STRATEGY_ADAPTIVE_REPLACEMENT);
            }
            else
            {
                m_performanceOptimizer->disableOptimization();
            }
        }

        DEBUG_LOG("TLB optimization %s", enable ? "enabled" : "disabled");
        emit sigOptimizationToggled(enable);
    }

    void flushAllEntries()
    {
        QMutexLocker locker(&m_systemMutex);

        if (m_entryStateManager)
        {
            m_entryStateManager->flushAllEntries();
        }

        if (m_pipelineCoordinator)
        {
            m_pipelineCoordinator->drainPipeline();
        }

        DEBUG_LOG("All TLB entries flushed");
        emit sigSystemFlushed();
    }

    void flushProcessEntries(quint32 processId)
    {
        QMutexLocker locker(&m_systemMutex);

        if (m_entryStateManager)
        {
            m_entryStateManager->flushEntriesByProcessId(processId);
        }

        DEBUG_LOG("TLB entries flushed for process ID: %u", processId);
        emit sigProcessFlushed(processId);
    }

    SystemState getSystemState() const { return m_systemState; }
    bool isOptimizationEnabled() const { return m_optimizationEnabled; }
    bool isEmergencyModeActive() const { return m_emergencyModeActive; }

    // Performance metrics
    qreal getOverallHitRatio() const
    {
        if (!m_addressTranslator)
            return 0.0;
        return m_addressTranslator->getHitRatio();
    }

    qreal getSystemEfficiency() const
    {
        quint64 total = m_statistics.totalRequests.loadAcquire();
        if (total == 0)
            return 0.0;
        return static_cast<qreal>(m_statistics.successfulTranslations.loadAcquire()) / static_cast<qreal>(total);
    }

    qreal getCurrentFaultRate() const
    {
        if (!m_errorHandler)
            return 0.0;
        return m_errorHandler->calculateFaultRate();
    }

    // Statistics accessors
    quint64 getTotalRequests() const { return m_statistics.totalRequests.loadAcquire(); }
    quint64 getSuccessfulTranslations() const { return m_statistics.successfulTranslations.loadAcquire(); }
    quint64 getFailedTranslations() const { return m_statistics.failedTranslations.loadAcquire(); }
    quint64 getCollisionEvents() const { return m_statistics.collisionEvents.loadAcquire(); }
    quint64 getOptimizationEvents() const { return m_statistics.optimizationEvents.loadAcquire(); }
    quint64 getErrorEvents() const { return m_statistics.errorEvents.loadAcquire(); }

    // Subsystem access (for advanced configuration)
    tlbCollisionDetector *getCollisionDetector() const { return m_collisionDetector; }
    tlbEntryStateManager *getEntryStateManager() const { return m_entryStateManager; }
    tlbAddressTranslator *getAddressTranslator() const { return m_addressTranslator; }
    tlbPipelineCoordinator *getPipelineCoordinator() const { return m_pipelineCoordinator; }
    tlbPerformanceOptimizer *getPerformanceOptimizer() const { return m_performanceOptimizer; }
    tlbErrorHandler *getErrorHandler() const { return m_errorHandler; }

    void resetAllStatistics()
    {
        QMutexLocker locker(&m_systemMutex);

        m_statistics = SystemStatistics();

        if (m_collisionDetector)
            m_collisionDetector->resetStatistics();
        if (m_entryStateManager)
            m_entryStateManager->resetStatistics();
        if (m_addressTranslator)
            m_addressTranslator->resetStatistics();
        if (m_pipelineCoordinator)
            m_pipelineCoordinator->resetStatistics();
        if (m_performanceOptimizer)
            m_performanceOptimizer->resetStatistics();
        if (m_errorHandler)
            m_errorHandler->resetStatistics();

        DEBUG_LOG("All TLB system statistics reset");
        emit sigStatisticsReset();
    }

  signals:
    void sigSystemInitialized();
    void sigRequestProcessed(quint64 requestId, bool wasSuccessful, quint64 processingTime);
    void sigOptimizationToggled(bool enabled);
    void sigSystemFlushed();
    void sigProcessFlushed(quint32 processId);
    void sigStatisticsReset();
    void sigEmergencyModeActivated();
    void sigEmergencyModeDeactivated();
    void sigSystemStateChanged(SystemState oldState, SystemState newState);

  private slots:
    void performMaintenance()
    {
        m_lastMaintenanceTime = QDateTime::currentMSecsSinceEpoch();

        // Check system health
        qreal faultRate = getCurrentFaultRate();
        if (faultRate > EMERGENCY_FAULT_THRESHOLD && !m_emergencyModeActive)
        {
            activateEmergencyMode();
        }
        else if (faultRate < (EMERGENCY_FAULT_THRESHOLD * 0.5) && m_emergencyModeActive)
        {
            deactivateEmergencyMode();
        }

        // Performance optimization cycle
        if (m_optimizationEnabled && m_performanceOptimizer)
        {
            m_statistics.optimizationEvents.fetchAndAddAcquire(1);
        }

        DEBUG_LOG("Maintenance cycle completed - Fault rate: %.2f%%", faultRate * 100.0);
    }

    void onCollisionDetected(tlbCollisionDetector::CollisionType type, quint32 tbIndex, quint64 virtualAddress)
    {
        m_statistics.collisionEvents.fetchAndAddAcquire(1);

        if (m_performanceOptimizer && m_optimizationEnabled)
        {
            // Trigger optimization response to collision
            qint32 optimalBank = m_performanceOptimizer->calculateOptimalBank(
                virtualAddress, type == tlbCollisionDetector::LOAD_LOAD_COLLISION);
            m_performanceOptimizer->recordCollisionReduction(virtualAddress, optimalBank);
        }
    }

    void onErrorReported(quint64 errorId, tlbErrorHandler::ErrorType errorType, tlbErrorHandler::ErrorSeverity severity,
                         quint64 virtualAddress, quint32 processId)
    {
        m_statistics.errorEvents.fetchAndAddAcquire(1);

        // Handle critical errors that may require system state changes
        if (severity >= tlbErrorHandler::SEVERITY_CRITICAL)
        {
            if (m_systemState == STATE_OPERATIONAL)
            {
                changeSystemState(STATE_DEGRADED);
            }
        }
    }

  private:
    TlbResponse processTranslationRequest(const TlbRequest &request)
    {
        TlbResponse response;
        response.requestId = request.requestId;

        // Step 1: Calculate TLB index and check for collisions
        quint32 tbIndex = m_addressTranslator->calculateTlbIndex(request.virtualAddress);
        response.tbIndex = tbIndex;

        tlbCollisionDetector::CollisionType collision =
            m_collisionDetector->detectCollision(request.virtualAddress, tbIndex, request.isLoad, request.threadId);

        if (collision != tlbCollisionDetector::NO_COLLISION)
        {
            // Handle collision through pipeline coordination
            bool shouldStall = m_collisionDetector->shouldStallOperation(collision, request.isLoad);
            if (shouldStall)
            {
                m_pipelineCoordinator->stallOperation(request.requestId, tlbPipelineCoordinator::COLLISION_STALL);
                response.wasSuccessful = false;
                response.errorType = tlbErrorHandler::RESOURCE_EXHAUSTION;
                return response;
            }
        }

        // Step 2: Check TLB entry validity and permissions
        if (!m_entryStateManager->isEntryValid(tbIndex))
        {
            response.result = tlbAddressTranslator::TRANSLATION_MISS;
            m_addressTranslator->recordTranslationMiss(tbIndex, request.virtualAddress);
            response.wasSuccessful = false;
            return response;
        }

        tlbEntryStateManager::AccessPermission requiredPermission = request.isLoad ? tlbEntryStateManager::READ_ONLY
                                                                    : request.isStore
                                                                        ? tlbEntryStateManager::WRITE_ONLY
                                                                        : tlbEntryStateManager::EXECUTE_ONLY;

        if (!m_entryStateManager->checkAccessPermission(tbIndex, requiredPermission))
        {
            response.result = tlbAddressTranslator::TRANSLATION_PROTECTION_VIOLATION;
            response.errorType = tlbErrorHandler::PROTECTION_VIOLATION;
            response.wasSuccessful = false;
            return response;
        }

        // Step 3: Perform successful translation
        tlbEntryStateManager::TlbEntryState entryState = m_entryStateManager->getEntryState(tbIndex);
        response.physicalAddress =
            m_addressTranslator->constructPhysicalAddress(entryState.physicalAddress, request.virtualAddress);

        // Update entry state
        m_entryStateManager->updateReferenceStatus(tbIndex);
        if (request.isStore)
        {
            m_entryStateManager->markEntryDirty(tbIndex);
        }

        // Record successful translation
        m_addressTranslator->recordTranslationHit(tbIndex, request.virtualAddress);
        response.result = tlbAddressTranslator::TRANSLATION_HIT;
        response.wasSuccessful = true;

        // Trigger prefetch if enabled
        if (m_optimizationEnabled && m_performanceOptimizer)
        {
            m_performanceOptimizer->shouldPrefetch(request.virtualAddress, request.processId);
        }

        return response;
    }

    TlbResponse processInvalidationRequest(const TlbRequest &request)
    {
        TlbResponse response;
        response.requestId = request.requestId;

        quint32 tbIndex = m_addressTranslator->calculateTlbIndex(request.virtualAddress);
        response.tbIndex = tbIndex;

        bool success = m_entryStateManager->invalidateEntry(tbIndex);
        response.wasSuccessful = success;

        return response;
    }

    TlbResponse processFlushRequest(const TlbRequest &request)
    {
        TlbResponse response;
        response.requestId = request.requestId;

        if (request.processId == 0)
        {
            flushAllEntries();
        }
        else
        {
            flushProcessEntries(request.processId);
        }

        response.wasSuccessful = true;
        return response;
    }

    TlbResponse processPrefetchRequest(const TlbRequest &request)
    {
        TlbResponse response;
        response.requestId = request.requestId;

        // Prefetch implementation would go here
        // For now, just indicate success
        response.wasSuccessful = true;

        return response;
    }

    TlbResponse processMaintenanceRequest(const TlbRequest &request)
    {
        TlbResponse response;
        response.requestId = request.requestId;

        performMaintenance();
        response.wasSuccessful = true;

        return response;
    }

    void connectSubsystemSignals()
    {
        // Connect collision detector signals
        connect(m_collisionDetector, &tlbCollisionDetector::sigCollisionDetected, this,
                &tlbSystem::onCollisionDetected);

        // Connect error handler signals
        connect(m_errorHandler, &tlbErrorHandler::sigErrorReported, this, &tlbSystem::onErrorReported);

        // Connect performance optimizer to error handler for emergency mode
        connect(m_errorHandler, &tlbErrorHandler::sigEmergencyModeEntered, this, &tlbSystem::activateEmergencyMode);
        connect(m_errorHandler, &tlbErrorHandler::sigEmergencyModeExited, this, &tlbSystem::deactivateEmergencyMode);
    }

    void changeSystemState(SystemState newState)
    {
        SystemState oldState = m_systemState;
        m_systemState = newState;

        DEBUG_LOG("System state changed: %d -> %d", oldState, newState);
        emit sigSystemStateChanged(oldState, newState);
    }

    void activateEmergencyMode()
    {
        if (!m_emergencyModeActive)
        {
            m_emergencyModeActive = true;
            changeSystemState(STATE_EMERGENCY);

            // Reduce system performance to stabilize
            if (m_performanceOptimizer)
            {
                m_performanceOptimizer->disableOptimization();
            }

            DEBUG_LOG("Emergency mode activated");
            emit sigEmergencyModeActivated();
        }
    }

    void deactivateEmergencyMode()
    {
        if (m_emergencyModeActive)
        {
            m_emergencyModeActive = false;
            changeSystemState(STATE_OPERATIONAL);

            // Re-enable optimizations
            if (m_optimizationEnabled && m_performanceOptimizer)
            {
                m_performanceOptimizer->enableOptimization(tlbPerformanceOptimizer::STRATEGY_ADAPTIVE_REPLACEMENT);
            }

            DEBUG_LOG("Emergency mode deactivated");
            emit sigEmergencyModeDeactivated();
        }
    }

    void handleSystemError(const QString &errorMessage, quint64 virtualAddress, quint32 processId)
    {
        DEBUG_LOG("System error: %s (VA=0x%llx, PID=%u)", errorMessage.toUtf8().constData(), virtualAddress, processId);

        if (m_errorHandler)
        {
            m_errorHandler->reportError(tlbErrorHandler::HARDWARE_FAULT, virtualAddress, processId, 0, errorMessage);
        }
    }
};

