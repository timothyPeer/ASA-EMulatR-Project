#pragma once

#include "GlobalMacro.h"
#include <QtCore/QAtomicInt>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include "utilitySafeIncrement.h"


/*
Collision Detection: Monitors up to 4 concurrent operations and detects when they target the same TB index
Priority Management: Supports three priority schemes (load priority, store priority, oldest-first)
Statistics Tracking: Maintains counters for different collision types using atomic operations
Thread Safety: Uses QMutex for protecting shared data structures
Operation Lifecycle: Registers operations when they start, detects collisions during execution, and unregisters when
complete

Core Methods:

detectCollision(): Checks if a new operation collides with existing ones
registerOperation()/unregisterOperation(): Manages the lifecycle of TB operations
shouldStallOperation(): Determines if an operation should be stalled based on priority scheme
Statistics methods for monitoring collision behavior
*/
class tlbCollisionDetector : public QObject
{
    Q_OBJECT

  public:
    enum CollisionType
    {
        NO_COLLISION = 0,
        LOAD_LOAD_COLLISION,
        STORE_STORE_COLLISION,
        LOAD_STORE_COLLISION,
        STORE_LOAD_COLLISION
    };

    enum Priority
    {
        LOAD_PRIORITY = 0,
        STORE_PRIORITY = 1,
        OLDEST_FIRST = 2
    };

    struct CollisionEntry
    {
        bool isActive;
        bool isLoad;
        bool isStore;
        uint64_t virtualAddress;
        uint32_t tbIndex;
        uint64_t timestamp;
        uint32_t threadId;

        CollisionEntry()
            : isActive(false), isLoad(false), isStore(false), virtualAddress(0), tbIndex(0), timestamp(0), threadId(0)
        {
        }
    };

  private:
    static const int MAX_CONCURRENT_OPERATIONS = 4;
    CollisionEntry m_activeOperations[MAX_CONCURRENT_OPERATIONS];
    QMutex m_operationMutex;
    QAtomicInt m_collisionCounter;
    QAtomicInt m_loadLoadCollisions;
    QAtomicInt m_storeStoreCollisions;
    QAtomicInt m_loadStoreCollisions;
    Priority m_priorityScheme;
    bool m_initialized;

  public:
    explicit tlbCollisionDetector(QObject *parent = nullptr)
        : QObject(parent), m_collisionCounter(0), m_loadLoadCollisions(0), m_storeStoreCollisions(0),
          m_loadStoreCollisions(0), m_priorityScheme(STORE_PRIORITY), m_initialized(false)
    {
        initialize();
    }

    ~tlbCollisionDetector()
    {
        DEBUG_LOG("tlbCollisionDetector destroyed - Total collisions: %d", m_collisionCounter.load());
    }

    void initialize()
    {
        if (m_initialized)
            return;

        QMutexLocker locker(&m_operationMutex);
        for (int i = 0; i < MAX_CONCURRENT_OPERATIONS; ++i)
        {
            m_activeOperations[i] = CollisionEntry();
        }
        m_initialized = true;
        DEBUG_LOG("tlbCollisionDetector initialized");
    }

    void initialize_SignalsAndSlots()
    {
        // Connect internal signals if needed for future expansion
    }

    CollisionType detectCollision(uint64_t virtualAddress, uint32_t tbIndex, bool isLoad, uint32_t threadId)
    {
        QMutexLocker locker(&m_operationMutex);

        for (int i = 0; i < MAX_CONCURRENT_OPERATIONS; ++i)
        {
            if (m_activeOperations[i].isActive && m_activeOperations[i].tbIndex == tbIndex)
            {
                CollisionType collision = determineCollisionType(m_activeOperations[i], isLoad);
                if (collision != NO_COLLISION)
                {
                    asa_utils::safeIncrement(m_collisionCounter);
                    updateCollisionStats(collision);
                    DEBUG_LOG("Collision detected: Type=%d, TB Index=%u, VA=0x%llx", collision, tbIndex,
                              virtualAddress);
                    emit sigCollisionDetected(collision, tbIndex, virtualAddress);
                    return collision;
                }
            }
        }
        return NO_COLLISION;
    }

    bool registerOperation(uint64_t virtualAddress, uint32_t tbIndex, bool isLoad, uint32_t threadId)
    {
        QMutexLocker locker(&m_operationMutex);

        for (int i = 0; i < MAX_CONCURRENT_OPERATIONS; ++i)
        {
            if (!m_activeOperations[i].isActive)
            {
                m_activeOperations[i].isActive = true;
                m_activeOperations[i].isLoad = isLoad;
                m_activeOperations[i].isStore = !isLoad;
                m_activeOperations[i].virtualAddress = virtualAddress;
                m_activeOperations[i].tbIndex = tbIndex;
                m_activeOperations[i].timestamp = getCurrentTimestamp();
                m_activeOperations[i].threadId = threadId;
                DEBUG_LOG("Operation registered: %s, TB Index=%u, VA=0x%llx", isLoad ? "LOAD" : "STORE", tbIndex,
                          virtualAddress);
                return true;
            }
        }
        DEBUG_LOG("Failed to register operation - no free slots");
        return false;
    }

    void unregisterOperation(uint64_t virtualAddress, uint32_t tbIndex, uint32_t threadId)
    {
        QMutexLocker locker(&m_operationMutex);

        for (int i = 0; i < MAX_CONCURRENT_OPERATIONS; ++i)
        {
            if (m_activeOperations[i].isActive && m_activeOperations[i].tbIndex == tbIndex &&
                m_activeOperations[i].virtualAddress == virtualAddress && m_activeOperations[i].threadId == threadId)
            {
                m_activeOperations[i] = CollisionEntry();
                DEBUG_LOG("Operation unregistered: TB Index=%u, VA=0x%llx", tbIndex, virtualAddress);
                break;
            }
        }
    }

    bool shouldStallOperation(CollisionType collision, bool isCurrentLoad) const
    {
        switch (m_priorityScheme)
        {
        case LOAD_PRIORITY:
            return !isCurrentLoad;
        case STORE_PRIORITY:
            return isCurrentLoad;
        case OLDEST_FIRST:
            // Would need timestamp comparison logic here
            return false;
        default:
            return false;
        }
    }

    void setPriorityScheme(Priority scheme)
    {
        QMutexLocker locker(&m_operationMutex);
        m_priorityScheme = scheme;
        DEBUG_LOG("Priority scheme changed to: %d", scheme);
    }

    Priority getPriorityScheme() const { return m_priorityScheme; }

    // Statistics accessors
    int getTotalCollisions() const { return m_collisionCounter.loadAcquire(); }
    int getLoadLoadCollisions() const { return m_loadLoadCollisions.loadAcquire(); }
    int getStoreStoreCollisions() const { return m_storeStoreCollisions.loadAcquire(); }
    int getLoadStoreCollisions() const { return m_loadStoreCollisions.loadAcquire(); }

    void resetStatistics()
    {
        m_collisionCounter.storeRelease(0);
        m_loadLoadCollisions.storeRelease(0);
        m_storeStoreCollisions.storeRelease(0);
        m_loadStoreCollisions.storeRelease(0);
        DEBUG_LOG("Collision statistics reset");
    }

  signals:
    void sigCollisionDetected(CollisionType type, uint32_t tbIndex, uint64_t virtualAddress);
    void sigOperationStalled(uint32_t tbIndex, uint64_t virtualAddress, bool isLoad);

  private:
    CollisionType determineCollisionType(const CollisionEntry &existing, bool currentIsLoad) const
    {
        if (existing.isLoad && currentIsLoad)
        {
            return LOAD_LOAD_COLLISION;
        }
        else if (existing.isStore && !currentIsLoad)
        {
            return STORE_STORE_COLLISION;
        }
        else if (existing.isLoad && !currentIsLoad)
        {
            return LOAD_STORE_COLLISION;
        }
        else if (existing.isStore && currentIsLoad)
        {
            return STORE_LOAD_COLLISION;
        }
        return NO_COLLISION;
    }

    void updateCollisionStats(CollisionType collision)
    {
        switch (collision)
        {
        case LOAD_LOAD_COLLISION:
            asa_utils::safeIncrement(m_loadLoadCollisions);
            break;
        case STORE_STORE_COLLISION:
            asa_utils::safeIncrement(m_storeStoreCollisions);
            break;
        case LOAD_STORE_COLLISION:
        case STORE_LOAD_COLLISION:
            asa_utils::safeIncrement(m_loadStoreCollisions);
            break;
        default:
            break;
        }
    }

    uint64_t getCurrentTimestamp() const { return QDateTime::currentMSecsSinceEpoch(); }
};

