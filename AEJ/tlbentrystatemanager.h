#pragma once

#include <QObject>

#pragma once

#include "GlobalMacro.h"
#include <QtCore/QAtomicInt>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include "utilitySafeIncrement.h"

/*
Key Features:

Entry State Management: Tracks valid/invalid, clean/dirty, and reference bits for each TLB entry
Access Permission Control: Enforces read/write/execute permissions with violation detection
Per-Entry Locking: Uses individual mutexes for each entry to maximize concurrency
Process Isolation: Associates entries with process IDs for selective flushing
Entry Lifecycle: Complete management from validation through invalidation
Statistics Tracking: Monitors valid entries, dirty entries, violations, and invalidations

Core Methods:

setEntryValid()/invalidateEntry(): Manages entry validity lifecycle
checkAccessPermission(): Validates access rights before operations
markEntryDirty()/updateReferenceStatus(): Tracks entry usage patterns
lockEntry()/unlockEntry(): Prevents entry replacement during critical operations
flushAllEntries()/flushEntriesByProcessId(): Bulk invalidation operations
*/
class tlbEntryStateManager : public QObject
{
    Q_OBJECT

  public:
    enum AccessPermission
    {
        NO_ACCESS = 0x00,
        READ_ONLY = 0x01,
        WRITE_ONLY = 0x02,
        READ_WRITE = 0x03,
        EXECUTE_ONLY = 0x04,
        READ_EXECUTE = 0x05,
        WRITE_EXECUTE = 0x06,
        FULL_ACCESS = 0x07
    };

    enum EntryState
    {
        INVALID = 0,
        VALID_CLEAN,
        VALID_DIRTY,
        PENDING_INVALIDATE,
        LOCKED
    };

    struct TlbEntryState
    {
        bool validBit;
        bool dirtyBit;
        bool referenceBit;
        AccessPermission permissions;
        EntryState state;
        QAtomicInt virtualTag;
        QAtomicInt physicalAddress;
        QAtomicInt lastAccessTime;
        QAtomicInt accessCount;
        QAtomicInt processId;

        TlbEntryState()
            : validBit(false), dirtyBit(false), referenceBit(false), permissions(NO_ACCESS), state(INVALID),
              virtualTag(0), physicalAddress(0), lastAccessTime(0), accessCount(0), processId(0)
        {
        }
    };

  private:
    static const int MAX_TLB_ENTRIES = 64;
    TlbEntryState m_entries[MAX_TLB_ENTRIES];
    QMutex m_stateMutex[MAX_TLB_ENTRIES]; // Per-entry locking for better concurrency
    QAtomicInt m_validEntryCount;
    QAtomicInt m_dirtyEntryCount;
    QAtomicInt m_accessViolationCount;
    QAtomicInt m_invalidationCount;
    bool m_initialized;

  public:
    explicit tlbEntryStateManager(QObject *parent = nullptr)
        : QObject(parent), m_validEntryCount(0), m_dirtyEntryCount(0), m_accessViolationCount(0),
          m_invalidationCount(0), m_initialized(false)
    {
        initialize();
    }

    ~tlbEntryStateManager()
    {
        DEBUG_LOG("tlbEntryStateManager destroyed - Valid entries: %d, Dirty entries: %d",
                  m_validEntryCount.loadAcquire(), m_dirtyEntryCount.loadAcquire());
    }

    void initialize()
    {
        if (m_initialized)
            return;

        for (int i = 0; i < MAX_TLB_ENTRIES; ++i)
        {
            QMutexLocker locker(&m_stateMutex[i]);
            m_entries[i] = TlbEntryState();
        }
        m_initialized = true;
        DEBUG_LOG("tlbEntryStateManager initialized with %d entries", MAX_TLB_ENTRIES);
    }

    void initialize_SignalsAndSlots()
    {
        // Connect internal signals for state change notifications
    }

    bool isEntryValid(uint32_t index) const
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;
        QMutexLocker locker(&m_stateMutex[index]);
        return m_entries[index].validBit && m_entries[index].state != INVALID;
    }

    bool isEntryDirty(uint32_t index) const
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;
        QMutexLocker locker(&m_stateMutex[index]);
        return m_entries[index].dirtyBit;
    }

    bool checkAccessPermission(uint32_t index, AccessPermission requestedAccess) const
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;
        QMutexLocker locker(&m_stateMutex[index]);

        if (!m_entries[index].validBit || m_entries[index].state == INVALID)
        {
            return false;
        }

        return (m_entries[index].permissions & requestedAccess) == requestedAccess;
    }

    bool setEntryValid(uint32_t index, uint64_t virtualTag, uint64_t physicalAddress, AccessPermission permissions,
                       uint32_t processId)
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;

        QMutexLocker locker(&m_stateMutex[index]);

        bool wasValid = m_entries[index].validBit;

        m_entries[index].validBit = true;
        m_entries[index].dirtyBit = false;
        m_entries[index].referenceBit = true;
        m_entries[index].permissions = permissions;
        m_entries[index].state = VALID_CLEAN;
        m_entries[index].virtualTag = virtualTag;
        m_entries[index].physicalAddress = physicalAddress;
        m_entries[index].lastAccessTime = QDateTime::currentMSecsSinceEpoch();
        m_entries[index].accessCount = 1;
        m_entries[index].processId = processId;

        if (!wasValid)
        {
            asa_utils::safeIncrement(m_validEntryCount);
        }

        DEBUG_LOG("TLB Entry %d set valid: VA=0x%llx, PA=0x%llx, PID=%u", index, virtualTag, physicalAddress,
                  processId);
        emit sigEntryValidated(index, virtualTag, physicalAddress);
        return true;
    }

    bool invalidateEntry(uint32_t index)
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;

        QMutexLocker locker(&m_stateMutex[index]);

        if (!m_entries[index].validBit)
            return false;

        bool wasDirty = m_entries[index].dirtyBit;
        uint64_t virtualTag = m_entries[index].virtualTag;

        m_entries[index].validBit = false;
        m_entries[index].state = INVALID;
        m_entries[index].referenceBit = false;

        m_validEntryCount.fetchAndAddAcquire(-1);
        if (wasDirty)
        {
            m_dirtyEntryCount.fetchAndAddAcquire(-1);
        }
        asa_utils::safeIncrement(m_invalidationCount);

        DEBUG_LOG("TLB Entry %d invalidated: VA=0x%llx", index, virtualTag);
        emit sigEntryInvalidated(index, virtualTag);
        return true;
    }

    bool markEntryDirty(uint32_t index)
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;

        QMutexLocker locker(&m_stateMutex[index]);

        if (!m_entries[index].validBit || m_entries[index].state == INVALID)
        {
            return false;
        }

        bool wasDirty = m_entries[index].dirtyBit;
        m_entries[index].dirtyBit = true;
        m_entries[index].state = VALID_DIRTY;
        m_entries[index].lastAccessTime = QDateTime::currentMSecsSinceEpoch();
        asa_utils::safeIncrement(m_entries[index].accessCount);

        if (!wasDirty)
        {
            asa_utils::safeIncrement(m_dirtyEntryCount);
        }

        DEBUG_LOG("TLB Entry %d marked dirty: VA=0x%llx", index, m_entries[index].virtualTag);
        emit sigEntryMarkedDirty(index, m_entries[index].virtualTag);
        return true;
    }

    bool updateReferenceStatus(uint32_t index)
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;

        QMutexLocker locker(&m_stateMutex[index]);

        if (!m_entries[index].validBit || m_entries[index].state == INVALID)
        {
            return false;
        }

        m_entries[index].referenceBit = true;
        m_entries[index].lastAccessTime = QDateTime::currentMSecsSinceEpoch();
        asa_utils::safeIncrement(m_entries[index].accessCount);

        return true;
    }

    bool lockEntry(uint32_t index)
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;

        QMutexLocker locker(&m_stateMutex[index]);

        if (!m_entries[index].validBit || m_entries[index].state == INVALID)
        {
            return false;
        }

        m_entries[index].state = LOCKED;
        DEBUG_LOG("TLB Entry %d locked: VA=0x%llx", index, m_entries[index].virtualTag);
        return true;
    }

    bool unlockEntry(uint32_t index)
    {
        if (index >= MAX_TLB_ENTRIES)
            return false;

        QMutexLocker locker(&m_stateMutex[index]);

        if (m_entries[index].state != LOCKED)
        {
            return false;
        }

        m_entries[index].state = m_entries[index].dirtyBit ? VALID_DIRTY : VALID_CLEAN;
        DEBUG_LOG("TLB Entry %d unlocked: VA=0x%llx", index, m_entries[index].virtualTag);
        return true;
    }

    TlbEntryState getEntryState(uint32_t index) const
    {
        if (index >= MAX_TLB_ENTRIES)
            return TlbEntryState();

        QMutexLocker locker(&m_stateMutex[index]);
        return m_entries[index];
    }

    void flushAllEntries()
    {
        int flushedCount = 0;
        for (int i = 0; i < MAX_TLB_ENTRIES; ++i)
        {
            QMutexLocker locker(&m_stateMutex[i]);
            if (m_entries[i].validBit && m_entries[i].state != LOCKED)
            {
                m_entries[i] = TlbEntryState();
                flushedCount++;
            }
        }

        m_validEntryCount.storeRelease(0);
        m_dirtyEntryCount.storeRelease(0);

        DEBUG_LOG("Flushed %d TLB entries", flushedCount);
        emit sigAllEntriesFlushed(flushedCount);
    }

    void flushEntriesByProcessId(uint32_t processId)
    {
        int flushedCount = 0;
        for (int i = 0; i < MAX_TLB_ENTRIES; ++i)
        {
            QMutexLocker locker(&m_stateMutex[i]);
            if (m_entries[i].validBit && m_entries[i].processId == processId && m_entries[i].state != LOCKED)
            {

                bool wasDirty = m_entries[i].dirtyBit;
                m_entries[i] = TlbEntryState();
                flushedCount++;

                m_validEntryCount.fetchAndAddAcquire(-1);
                if (wasDirty)
                {
                    m_dirtyEntryCount.fetchAndAddAcquire(-1);
                }
            }
        }

        DEBUG_LOG("Flushed %d TLB entries for process ID %u", flushedCount, processId);
        emit sigProcessEntriesFlushed(processId, flushedCount);
    }

    // Statistics accessors
    int getValidEntryCount() const { return m_validEntryCount.loadAcquire(); }
    int getDirtyEntryCount() const { return m_dirtyEntryCount.loadAcquire(); }
    int getAccessViolationCount() const { return m_accessViolationCount.loadAcquire(); }
    int getInvalidationCount() const { return m_invalidationCount.loadAcquire(); }
    int getMaxEntries() const { return MAX_TLB_ENTRIES; }

    void resetStatistics()
    {
        m_accessViolationCount.storeRelease(0);
        m_invalidationCount.storeRelease(0);
        DEBUG_LOG("TLB entry state statistics reset");
    }

  signals:
    void sigEntryValidated(uint32_t index, uint64_t virtualTag, uint64_t physicalAddress);
    void sigEntryInvalidated(uint32_t index, uint64_t virtualTag);
    void sigEntryMarkedDirty(uint32_t index, uint64_t virtualTag);
    void sigAccessViolation(uint32_t index, uint64_t virtualTag, AccessPermission requested);
    void sigAllEntriesFlushed(int count);
    void sigProcessEntriesFlushed(uint32_t processId, int count);
};

