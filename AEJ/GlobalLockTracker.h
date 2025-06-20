// GlobalLockTracker.H


#pragma once
#include <QReadWriteLock>
#include <QSet>
#include <QtGlobal>

class GlobalLockTracker
{
  public:
    //! Invalidate the 16-byte block at 'base'.  All LDx_L reservations there are lost.
    static void invalidate(quint64 base)
    {
        QWriteLocker locker(&lock());
        invalidatedBases().insert(base);
    }

    //! Returns true if 'base' was invalidated since the last reservation.
    static bool wasInvalidated(quint64 base)
    {
        QReadLocker locker(&lock());
        return invalidatedBases().contains(base);
    }

  private:
    GlobalLockTracker() = delete;
    ~GlobalLockTracker() = delete;

    //! Singleton lock protecting the invalidation set.
    static QReadWriteLock &lock()
    {
        static QReadWriteLock s_lock;
        return s_lock;
    }

    //! Set of 16-byte-aligned bases that have been invalidated.
    static QSet<quint64> &invalidatedBases()
    {
        static QSet<quint64> s_invalidated;
        return s_invalidated;
    }
};
