#pragma once

#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"
#include "tlbSystemCoordinator.h"
#include <QtCore/QAtomicInteger>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QtAlgorithms>

class tlbCacheIntegrator : public QObject
{
    Q_OBJECT

  public:
    enum CacheLevel
    {
        CACHE_L1_INSTRUCTION = 0,
        CACHE_L1_DATA,
        CACHE_L2_UNIFIED,
        CACHE_L3_UNIFIED,
        CACHE_LLC // Last Level Cache
    };

    enum CacheOperation
    {
        CACHE_OP_READ = 0,
        CACHE_OP_WRITE,
        CACHE_OP_PREFETCH,
        CACHE_OP_INVALIDATE,
        CACHE_OP_FLUSH,
        CACHE_OP_WRITEBACK
    };

    enum CoherencyState
    {
        COHERENCY_INVALID = 0,
        COHERENCY_SHARED,
        COHERENCY_EXCLUSIVE,
        COHERENCY_MODIFIED,
        COHERENCY_OWNED
    };

    struct CacheRequest
    {
        quint64 requestId;
        CacheLevel targetLevel;
        CacheOperation operation;
        quint64 virtualAddress;
        quint64 physicalAddress;
        quint32 processId;
        quint32 threadId;
        quint32 dataSize;
        bool isTlbDriven;
        bool isCoherent;
        quint64 submissionTime;

        CacheRequest()
            : requestId(0), targetLevel(CACHE_L1_DATA), operation(CACHE_OP_READ), virtualAddress(0), physicalAddress(0),
              processId(0), threadId(0), dataSize(0), isTlbDriven(false), isCoherent(true), submissionTime(0)
        {
        }
    };

    struct CacheResponse
    {
        quint64 requestId;
        bool wasHit;
        bool wasSuccessful;
        CacheLevel hitLevel;
        CoherencyState coherencyState;
        quint64 responseData;
        quint64 processingTime;
        bool triggeredTlbMiss;

        CacheResponse()
            : requestId(0), wasHit(false), wasSuccessful(false), hitLevel(CACHE_L1_DATA),
              coherencyState(COHERENCY_INVALID), responseData(0), processingTime(0), triggeredTlbMiss(false)
        {
        }
    };

    struct TlbCacheStatistics
    {
        QAtomicInteger<quint64> totalCacheRequests;
        QAtomicInteger<quint64> tlbDrivenRequests;
        QAtomicInteger<quint64> cacheHits;
        QAtomicInteger<quint64> cacheMisses;
        QAtomicInteger<quint64> tlbMissesFromCache;
        QAtomicInteger<quint64> coherencyOperations;
        QAtomicInteger<quint64> writebacks;
        QAtomicInteger<quint64> prefetchHits;

        TlbCacheStatistics()
            : totalCacheRequests(0), tlbDrivenRequests(0), cacheHits(0), cacheMisses(0), tlbMissesFromCache(0),
              coherencyOperations(0), writebacks(0), prefetchHits(0)
        {
        }
    };

  private:
    // Core integration components
    tlbSystemCoordinator *m_tlbCoordinator;
    QMutex m_integrationMutex;
    QAtomicInteger<quint64> m_requestIdCounter;

    // CPU-specific cache interface mapping
    QVector<QHash<CacheLevel, QObject *>> m_cpuCacheControllers; // CPU ID -> CacheLevel -> Cache Controller
    QHash<quint64, quint64> m_virtualToPhysicalMap;              // VA -> PA mapping cache
    QMutex m_mappingMutex;
    QMutex m_cpuCacheMutex;
    quint16 m_maxCpuCount;

    // Statistics and monitoring
    TlbCacheStatistics m_statistics;
    QHash<CacheLevel, QAtomicInteger<quint64> *> m_perLevelHits;
    QHash<CacheLevel, QAtomicInteger<quint64> *> m_perLevelMisses;

    // Configuration parameters (loaded from settings or cache controllers)
    quint32 m_cacheLineSize;
    quint32 m_cacheLineMask;
    quint32 m_pageSize;
    qreal m_efficiencyTarget;
    quint32 m_prefetchDepth;    // Number of cache lines to prefetch ahead
    quint32 m_prefetchDistance; // Distance in bytes for prefetch lookahead
    bool m_coherencyEnabled;
    bool m_prefetchEnabled;
    bool m_writebackEnabled;
    bool m_initialized;

    // Default values (can be overridden)
    static const quint32 DEFAULT_CACHE_LINE_SIZE = 64;
    static const quint32 DEFAULT_PAGE_SIZE = 4096;
    static const quint32 DEFAULT_PREFETCH_DEPTH = 2;      // Prefetch 2 cache lines ahead
    static const quint32 DEFAULT_PREFETCH_DISTANCE = 128; // 128 bytes ahead
    static constexpr qreal DEFAULT_EFFICIENCY_TARGET = 0.95;

  public:
    explicit tlbCacheIntegrator(tlbSystemCoordinator *tlbCoordinator, quint16 maxCpuCount = 64,
                                QObject *parent = nullptr)
        : QObject(parent), m_tlbCoordinator(tlbCoordinator), m_requestIdCounter(0), m_maxCpuCount(maxCpuCount),
          m_cacheLineSize(DEFAULT_CACHE_LINE_SIZE), m_cacheLineMask(DEFAULT_CACHE_LINE_SIZE - 1),
          m_pageSize(DEFAULT_PAGE_SIZE), m_efficiencyTarget(DEFAULT_EFFICIENCY_TARGET),
          m_prefetchDepth(DEFAULT_PREFETCH_DEPTH), m_prefetchDistance(DEFAULT_PREFETCH_DISTANCE),
          m_coherencyEnabled(true), m_prefetchEnabled(true), m_writebackEnabled(true), m_initialized(false)
    {
        initialize();
    }

    ~tlbCacheIntegrator()
    {
        // Cleanup per-level statistics
        for (auto *counter : m_perLevelHits.values())
        {
            delete counter;
        }
        for (auto *counter : m_perLevelMisses.values())
        {
            delete counter;
        }

        quint64 totalRequests = m_statistics.totalCacheRequests.loadAcquire();
        quint64 hits = m_statistics.cacheHits.loadAcquire();
        qreal hitRatio = totalRequests > 0 ? static_cast<qreal>(hits) / static_cast<qreal>(totalRequests) : 0.0;

        DEBUG_LOG("tlbCacheIntegrator destroyed - Cache hit ratio: %.2f%%, TLB-driven: %llu", hitRatio * 100.0,
                  m_statistics.tlbDrivenRequests.loadAcquire());
    }

    void initialize()
    {
        if (m_initialized)
            return;

        // Initialize per-CPU cache controller vectors
        m_cpuCacheControllers.resize(m_maxCpuCount);
        for (quint16 cpuId = 0; cpuId < m_maxCpuCount; ++cpuId)
        {
            m_cpuCacheControllers[cpuId].clear();
        }

        // Initialize per-level statistics
        initializePerLevelStatistics();

        // Clear mapping cache
        m_virtualToPhysicalMap.clear();

        m_initialized = true;
        DEBUG_LOG("tlbCacheIntegrator initialized for %d CPUs with TLB coordinator integration", m_maxCpuCount);
    }

    void initialize_SignalsAndSlots()
    {
        if (!m_initialized || !m_tlbCoordinator)
            return;

        // Connect to TLB coordinator signals
        connect(m_tlbCoordinator, &tlbSystemCoordinator::sigRequestProcessed, this,
                &tlbCacheIntegrator::onTlbRequestProcessed);
        connect(m_tlbCoordinator, &tlbSystemCoordinator::sigSystemFlushed, this, &tlbCacheIntegrator::onTlbFlushed);
        connect(m_tlbCoordinator, &tlbSystemCoordinator::sigProcessFlushed, this,
                &tlbCacheIntegrator::onTlbProcessFlushed);

        DEBUG_LOG("TLB-Cache integration signals connected");
    }

    void attachCacheController(quint16 cpuId, CacheLevel level, QObject *controller)
    {
        QMutexLocker locker(&m_cpuCacheMutex);

        if (cpuId >= m_maxCpuCount)
        {
            DEBUG_LOG("Invalid CPU ID %d (max: %d) for cache controller attachment", cpuId, m_maxCpuCount - 1);
            return;
        }

        m_cpuCacheControllers[cpuId][level] = controller;

        // Auto-detect cache configuration from attached controller
        syncCacheConfiguration(controller, level);

        DEBUG_LOG("Cache controller attached for CPU %d, level %d", cpuId, level);
        emit sigCacheControllerAttached(cpuId, level);
    }

    void attachUnifiedDataCache(const QVector<quint16> &cpuIds, QObject *unifiedCache)
    {
        QMutexLocker locker(&m_cpuCacheMutex);

        for (quint16 cpuId : cpuIds)
        {
            if (cpuId >= m_maxCpuCount)
            {
                DEBUG_LOG("Invalid CPU ID %d (max: %d) for unified cache attachment", cpuId, m_maxCpuCount - 1);
                continue;
            }

            // Attach unified cache as L3 for specified CPUs
            m_cpuCacheControllers[cpuId][CACHE_L3_UNIFIED] = unifiedCache;
        }

        // Sync configuration with the unified cache
        syncCacheConfiguration(unifiedCache, CACHE_L3_UNIFIED);

        DEBUG_LOG("Unified data cache attached for %d CPUs", cpuIds.size());
        emit sigUnifiedCacheAttached(cpuIds, CACHE_L3_UNIFIED);
    }

    CacheResponse processMemoryRequest(quint16 cpuId, quint64 virtualAddress, CacheOperation operation,
                                       quint32 processId, quint32 threadId, quint32 dataSize = 4)
    {
        QMutexLocker locker(&m_integrationMutex);

        CacheResponse response;
        response.requestId = m_requestIdCounter.fetchAndAddAcquire(1);
        response.processingTime = QDateTime::currentMSecsSinceEpoch();

        if (cpuId >= m_maxCpuCount)
        {
            DEBUG_LOG("Invalid CPU ID %d for memory request", cpuId);
            response.wasSuccessful = false;
            return response;
        }

        m_statistics.totalCacheRequests.fetchAndAddAcquire(1);

        // Step 1: Check if we have cached VA->PA mapping
        quint64 physicalAddress = 0;
        bool haveCachedMapping = false;

        {
            QMutexLocker mappingLock(&m_mappingMutex);
            if (m_virtualToPhysicalMap.contains(virtualAddress & ~(m_pageSize - 1)))
            {
                physicalAddress =
                    m_virtualToPhysicalMap[virtualAddress & ~(m_pageSize - 1)] | (virtualAddress & (m_pageSize - 1));
                haveCachedMapping = true;
            }
        }

        // Step 2: If no cached mapping, request TLB translation
        if (!haveCachedMapping)
        {
            tlbSystemCoordinator::TlbRequest tlbRequest;
            tlbRequest.requestId = response.requestId;
            tlbRequest.operation = tlbSystemCoordinator::OP_TRANSLATE;
            tlbRequest.virtualAddress = virtualAddress;
            tlbRequest.processId = processId;
            tlbRequest.threadId = threadId;
            tlbRequest.isLoad = (operation == CACHE_OP_READ);
            tlbRequest.isStore = (operation == CACHE_OP_WRITE);
            tlbRequest.submissionTime = QDateTime::currentMSecsSinceEpoch();

            tlbSystemCoordinator::TlbResponse tlbResponse = m_tlbCoordinator->processRequest(tlbRequest);

            if (!tlbResponse.wasSuccessful)
            {
                response.wasSuccessful = false;
                response.triggeredTlbMiss = true;
                m_statistics.tlbMissesFromCache.fetchAndAddAcquire(1);
                DEBUG_LOG("TLB miss for cache request: VA=0x%llx, PID=%u", virtualAddress, processId);
                return response;
            }

            physicalAddress = tlbResponse.physicalAddress;

            // Cache the VA->PA mapping for future requests
            cacheAddressMapping(virtualAddress, physicalAddress);
            m_statistics.tlbDrivenRequests.fetchAndAddAcquire(1);
        }

        // Step 3: Process cache request with physical address using CPU-specific caches
        response =
            processCacheHierarchy(cpuId, virtualAddress, physicalAddress, operation, processId, threadId, dataSize);

        response.processingTime = QDateTime::currentMSecsSinceEpoch() - response.processingTime;

        emit sigCacheRequestProcessed(response.requestId, response.wasHit, response.hitLevel, response.processingTime);
        return response;
    }

    // Configuration methods
    void setCacheLineSize(quint32 lineSize)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_cacheLineSize = lineSize;
        m_cacheLineMask = lineSize - 1;
        DEBUG_LOG("Cache line size set to %d bytes", lineSize);
    }

    void setPageSize(quint32 pageSize)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_pageSize = pageSize;
        DEBUG_LOG("Page size set to %d bytes", pageSize);
    }

    void setEfficiencyTarget(qreal target)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_efficiencyTarget = target;
        DEBUG_LOG("Efficiency target set to %.2f%%", target * 100.0);
    }

    void setPrefetchDepth(quint32 depth)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_prefetchDepth = depth;
        DEBUG_LOG("Prefetch depth set to %d cache lines", depth);
    }

    void setPrefetchDistance(quint32 distance)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_prefetchDistance = distance;
        DEBUG_LOG("Prefetch distance set to %d bytes", distance);
    }

    void enableCoherency(bool enable)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_coherencyEnabled = enable;
        DEBUG_LOG("Cache coherency %s", enable ? "enabled" : "disabled");
    }

    void enablePrefetch(bool enable)
    {
        QMutexLocker locker(&m_integrationMutex);
        m_prefetchEnabled = enable;
        DEBUG_LOG("Cache prefetch %s", enable ? "enabled" : "disabled");
    }

    void invalidateAddressMapping(quint64 virtualAddress)
    {
        QMutexLocker locker(&m_mappingMutex);

        quint64 pageAddress = virtualAddress & ~(m_pageSize - 1);
        if (m_virtualToPhysicalMap.contains(pageAddress))
        {
            m_virtualToPhysicalMap.remove(pageAddress);
            DEBUG_LOG("Address mapping invalidated: VA=0x%llx", virtualAddress);
        }

        // Also trigger cache invalidation for coherency
        if (m_coherencyEnabled)
        {
            invalidateCacheLines(virtualAddress);
        }
    }

    void flushAllCachedMappings()
    {
        QMutexLocker locker(&m_mappingMutex);

        qint32 mappingCount = m_virtualToPhysicalMap.size();
        m_virtualToPhysicalMap.clear();

        DEBUG_LOG("All cached address mappings flushed: %d mappings", mappingCount);
        emit sigAllMappingsFlushed(mappingCount);
    }

    void flushProcessMappings(quint32 processId)
    {
        // For now, flush all mappings - could be enhanced with per-process tracking
        flushAllCachedMappings();

        DEBUG_LOG("Process mappings flushed for PID: %u", processId);
        emit sigProcessMappingsFlushed(processId);
    }

    // Configuration accessors
    quint32 getCacheLineSize() const { return m_cacheLineSize; }
    quint32 getPageSize() const { return m_pageSize; }
    qreal getEfficiencyTarget() const { return m_efficiencyTarget; }
    quint32 getPrefetchDepth() const { return m_prefetchDepth; }
    quint32 getPrefetchDistance() const { return m_prefetchDistance; }

    bool isCacheAttached(quint16 cpuId, CacheLevel level) const
    {
        QMutexLocker locker(&m_cpuCacheMutex);

        if (cpuId >= m_maxCpuCount)
            return false;
        return m_cpuCacheControllers[cpuId].contains(level);
    }

    QVector<CacheLevel> getAttachedCacheLevels(quint16 cpuId) const
    {
        QMutexLocker locker(&m_cpuCacheMutex);

        QVector<CacheLevel> levels;
        if (cpuId < m_maxCpuCount)
        {
            levels = m_cpuCacheControllers[cpuId].keys().toVector();
        }
        return levels;
    }

    // Performance metrics
    qreal getCacheHitRatio() const
    {
        quint64 total = m_statistics.totalCacheRequests.loadAcquire();
        if (total == 0)
            return 0.0;
        return static_cast<qreal>(m_statistics.cacheHits.loadAcquire()) / static_cast<qreal>(total);
    }

    qreal getTlbCacheEfficiency() const
    {
        quint64 totalRequests = m_statistics.totalCacheRequests.loadAcquire();
        quint64 tlbMisses = m_statistics.tlbMissesFromCache.loadAcquire();
        if (totalRequests == 0)
            return 0.0;
        return 1.0 - (static_cast<qreal>(tlbMisses) / static_cast<qreal>(totalRequests));
    }

    qreal getCacheHitRatioForLevel(CacheLevel level) const
    {
        if (!m_perLevelHits.contains(level) || !m_perLevelMisses.contains(level))
        {
            return 0.0;
        }

        quint64 hits = m_perLevelHits[level]->loadAcquire();
        quint64 misses = m_perLevelMisses[level]->loadAcquire();
        quint64 total = hits + misses;

        if (total == 0)
            return 0.0;
        return static_cast<qreal>(hits) / static_cast<qreal>(total);
    }

    // Statistics accessors
    quint64 getTotalCacheRequests() const { return m_statistics.totalCacheRequests.loadAcquire(); }
    quint64 getTlbDrivenRequests() const { return m_statistics.tlbDrivenRequests.loadAcquire(); }
    quint64 getCacheHits() const { return m_statistics.cacheHits.loadAcquire(); }
    quint64 getCacheMisses() const { return m_statistics.cacheMisses.loadAcquire(); }
    quint64 getTlbMissesFromCache() const { return m_statistics.tlbMissesFromCache.loadAcquire(); }
    quint64 getCoherencyOperations() const { return m_statistics.coherencyOperations.loadAcquire(); }
    quint64 getWritebacks() const { return m_statistics.writebacks.loadAcquire(); }
    quint64 getPrefetchHits() const { return m_statistics.prefetchHits.loadAcquire(); }

    qint32 getCachedMappingCount() const
    {
        QMutexLocker locker(&m_mappingMutex);
        return m_virtualToPhysicalMap.size();
    }

    void resetStatistics()
    {
        QMutexLocker locker(&m_integrationMutex);

        m_statistics = TlbCacheStatistics();

        // Reset per-level statistics
        for (auto *counter : m_perLevelHits.values())
        {
            counter->storeRelease(0);
        }
        for (auto *counter : m_perLevelMisses.values())
        {
            counter->storeRelease(0);
        }

        DEBUG_LOG("TLB-Cache integration statistics reset");
    }

  signals:
    void sigCacheControllerAttached(quint16 cpuId, CacheLevel level);
    void sigUnifiedCacheAttached(QVector<quint16> cpuIds, CacheLevel level);
    void sigCacheRequestProcessed(quint64 requestId, bool wasHit, CacheLevel hitLevel, quint64 processingTime);
    void sigTlbMissTriggered(quint64 virtualAddress, quint32 processId);
    void sigAllMappingsFlushed(qint32 mappingCount);
    void sigProcessMappingsFlushed(quint32 processId);
    void sigCoherencyOperation(quint64 physicalAddress, CoherencyState newState);

  private slots:
    void onTlbRequestProcessed(quint64 requestId, bool wasSuccessful, quint64 processingTime)
    {
        if (wasSuccessful)
        {
            // TLB translation successful - cache performance should improve
            DEBUG_LOG("TLB translation completed for request ID: %llu", requestId);
        }
    }

    void onTlbFlushed()
    {
        // When TLB is flushed, invalidate all our cached mappings
        flushAllCachedMappings();
        DEBUG_LOG("TLB flush triggered mapping cache flush");
    }

    void onTlbProcessFlushed(quint32 processId)
    {
        // When specific process TLB entries are flushed, invalidate process mappings
        flushProcessMappings(processId);
        DEBUG_LOG("TLB process flush triggered mapping cache flush for PID: %u", processId);
    }

  private:
    void initializePerLevelStatistics()
    {
        // Initialize hit/miss counters for each cache level
        QList<CacheLevel> levels = {CACHE_L1_INSTRUCTION, CACHE_L1_DATA, CACHE_L2_UNIFIED, CACHE_L3_UNIFIED, CACHE_LLC};

        for (CacheLevel level : levels)
        {
            m_perLevelHits[level] = new QAtomicInteger<quint64>(0);
            m_perLevelMisses[level] = new QAtomicInteger<quint64>(0);
        }
    }

    void cacheAddressMapping(quint64 virtualAddress, quint64 physicalAddress)
    {
        QMutexLocker locker(&m_mappingMutex);

        quint64 virtualPage = virtualAddress & ~(m_pageSize - 1);
        quint64 physicalPage = physicalAddress & ~(m_pageSize - 1);

        m_virtualToPhysicalMap[virtualPage] = physicalPage;
    }

    CacheResponse processCacheHierarchy(quint16 cpuId, quint64 virtualAddress, quint64 physicalAddress,
                                        CacheOperation operation, quint32 processId, quint32 threadId, quint32 dataSize)
    {
        CacheResponse response;
        response.requestId = m_requestIdCounter.loadAcquire();

        QMutexLocker cpuLock(&m_cpuCacheMutex);
        const QHash<CacheLevel, QObject *> &cpuCaches = m_cpuCacheControllers[cpuId];
        cpuLock.unlock();

        // Try CPU-private L1 cache first
        if (cpuCaches.contains(CACHE_L1_DATA) &&
            performActualCacheAccess(cpuId, CACHE_L1_DATA, physicalAddress, operation))
        {
            response.wasHit = true;
            response.hitLevel = CACHE_L1_DATA;
            m_perLevelHits[CACHE_L1_DATA]->fetchAndAddAcquire(1);
            m_statistics.cacheHits.fetchAndAddAcquire(1);
        }
        // Try CPU-private L2 cache
        else if (cpuCaches.contains(CACHE_L2_UNIFIED) &&
                 performActualCacheAccess(cpuId, CACHE_L2_UNIFIED, physicalAddress, operation))
        {
            response.wasHit = true;
            response.hitLevel = CACHE_L2_UNIFIED;
            m_perLevelHits[CACHE_L2_UNIFIED]->fetchAndAddAcquire(1);
            m_perLevelMisses[CACHE_L1_DATA]->fetchAndAddAcquire(1);
            m_statistics.cacheHits.fetchAndAddAcquire(1);
        }
        // Try shared L3 cache (unified data cache)
        else if (cpuCaches.contains(CACHE_L3_UNIFIED) &&
                 performActualCacheAccess(cpuId, CACHE_L3_UNIFIED, physicalAddress, operation))
        {
            response.wasHit = true;
            response.hitLevel = CACHE_L3_UNIFIED;
            m_perLevelHits[CACHE_L3_UNIFIED]->fetchAndAddAcquire(1);
            m_perLevelMisses[CACHE_L1_DATA]->fetchAndAddAcquire(1);
            m_perLevelMisses[CACHE_L2_UNIFIED]->fetchAndAddAcquire(1);
            m_statistics.cacheHits.fetchAndAddAcquire(1);
        }
        // Memory access required
        else
        {
            response.wasHit = false;
            response.hitLevel = CACHE_LLC; // Indicates memory access
            m_perLevelMisses[CACHE_L1_DATA]->fetchAndAddAcquire(1);
            m_perLevelMisses[CACHE_L2_UNIFIED]->fetchAndAddAcquire(1);
            m_perLevelMisses[CACHE_L3_UNIFIED]->fetchAndAddAcquire(1);
            m_statistics.cacheMisses.fetchAndAddAcquire(1);
        }

        response.wasSuccessful = true;
        response.coherencyState = COHERENCY_SHARED; // Default coherency state

        // Handle coherency operations if enabled
        if (m_coherencyEnabled && operation == CACHE_OP_WRITE)
        {
            handleCoherencyOperation(physicalAddress, COHERENCY_MODIFIED);
        }

        return response;
    }

    bool performActualCacheAccess(quint16 cpuId, CacheLevel level, quint64 physicalAddress, CacheOperation operation)
    {
        QMutexLocker cpuLock(&m_cpuCacheMutex);

        if (cpuId >= m_maxCpuCount || !m_cpuCacheControllers[cpuId].contains(level))
        {
            return false; // Cache not attached
        }

        QObject *cacheController = m_cpuCacheControllers[cpuId][level];
        cpuLock.unlock();

        if (!cacheController)
        {
            return false;
        }

        // Call actual cache controller methods using Qt's meta-object system
        bool wasHit = false;

        switch (operation)
        {
        case CACHE_OP_READ:
        {
            // Invoke cache read method - adjust method name to match your cache API
            QMetaObject::invokeMethod(cacheController, "lookup", Qt::DirectConnection, Q_RETURN_ARG(bool, wasHit),
                                      Q_ARG(quint64, physicalAddress));
            break;
        }
        case CACHE_OP_WRITE:
        {
            // Invoke cache write method
            QMetaObject::invokeMethod(cacheController, "write", Qt::DirectConnection, Q_RETURN_ARG(bool, wasHit),
                                      Q_ARG(quint64, physicalAddress));
            break;
        }
        case CACHE_OP_PREFETCH:
        {
            // Perform prefetch operation with depth
            bool prefetchSuccess = true;
            for (quint32 i = 1; i <= m_prefetchDepth; ++i)
            {
                quint64 prefetchAddress = physicalAddress + (i * m_cacheLineSize);
                bool result = false;
                QMetaObject::invokeMethod(cacheController, "prefetch", Qt::DirectConnection, Q_RETURN_ARG(bool, result),
                                          Q_ARG(quint64, prefetchAddress));
                prefetchSuccess &= result;
            }
            wasHit = prefetchSuccess;
            break;
        }
        case CACHE_OP_INVALIDATE:
        {
            // Invoke cache invalidate method
            QMetaObject::invokeMethod(cacheController, "invalidate", Qt::DirectConnection,
                                      Q_ARG(quint64, physicalAddress));
            wasHit = false; // Invalidation doesn't count as hit
            break;
        }
        default:
            wasHit = false;
            break;
        }

        return wasHit;
    }

    bool simulateCacheAccess(CacheLevel level, quint64 physicalAddress, CacheOperation operation)
    {
        // This is now only used as fallback if actual cache controller method fails
        // Simple simulation based on address patterns
        quint32 hash = qHash(physicalAddress >> 6); // Cache line aligned

        switch (level)
        {
        case CACHE_L1_DATA:
        case CACHE_L1_INSTRUCTION:
            return (hash % 100) < 85; // 85% L1 hit rate
        case CACHE_L2_UNIFIED:
            return (hash % 100) < 70; // 70% L2 hit rate
        case CACHE_L3_UNIFIED:
            return (hash % 100) < 50; // 50% L3 hit rate
        default:
            return false;
        }
    }

    void invalidateCacheLines(quint64 virtualAddress)
    {
        // Trigger cache line invalidation across all levels
        quint64 cacheLineAddress = virtualAddress & ~m_cacheLineMask;

        DEBUG_LOG("Invalidating cache lines for address: 0x%llx", cacheLineAddress);
        m_statistics.coherencyOperations.fetchAndAddAcquire(1);

        // In real implementation, this would call cache controller invalidation methods
    }

    void syncCacheConfiguration(QObject *cacheController, CacheLevel level)
    {
        // Auto-detect configuration from cache controller
        // This assumes your cache controllers have standard property names

        if (cacheController->property("lineSize").isValid())
        {
            quint32 lineSize = cacheController->property("lineSize").toUInt();
            if (lineSize > 0 && lineSize != m_cacheLineSize)
            {
                DEBUG_LOG("Auto-detected cache line size: %d bytes from level %d", lineSize, level);
                setCacheLineSize(lineSize);
            }
        }

        if (cacheController->property("enableCoherency").isValid())
        {
            bool coherency = cacheController->property("enableCoherency").toBool();
            if (coherency != m_coherencyEnabled)
            {
                enableCoherency(coherency);
            }
        }

        if (cacheController->property("enablePrefetch").isValid())
        {
            bool prefetch = cacheController->property("enablePrefetch").toBool();
            if (prefetch != m_prefetchEnabled)
            {
                enablePrefetch(prefetch);
            }
        }

        if (cacheController->property("prefetchDepth").isValid())
        {
            quint32 depth = cacheController->property("prefetchDepth").toUInt();
            if (depth > 0 && depth != m_prefetchDepth)
            {
                DEBUG_LOG("Auto-detected prefetch depth: %d from level %d", depth, level);
                setPrefetchDepth(depth);
            }
        }

        if (cacheController->property("prefetchDistance").isValid())
        {
            quint32 distance = cacheController->property("prefetchDistance").toUInt();
            if (distance > 0 && distance != m_prefetchDistance)
            {
                DEBUG_LOG("Auto-detected prefetch distance: %d bytes from level %d", distance, level);
                setPrefetchDistance(distance);
            }
        }
    }

    void handleCoherencyOperation(quint64 physicalAddress, CoherencyState newState)
    {
        m_statistics.coherencyOperations.fetchAndAddAcquire(1);

        DEBUG_LOG("Coherency operation: PA=0x%llx, State=%d", physicalAddress, newState);
        emit sigCoherencyOperation(physicalAddress, newState);
    }
};

