#pragma once
#include <QObject>
#include <QString>

/**
 * @brief Abstract base for every memory-mapped I/O device.
 *
 *  • All offsets are **device–relative** (physicalAddr-base).
 *  • `size` is 1, 2, 4, or 8 and denotes the transfer width in bytes.
 *  • `read()` returns the raw value; `write()` returns *true* on success.
 *    Returning *false* lets the bus raise an error/status bit if you wish.
 */
class MMIODevice
{
public:
    // ──────────────────────────────── Mandatory I/O API ───────────────────────────────
    virtual quint64 read(quint64 offset, int size) = 0;
    virtual bool    write(quint64 offset, quint64 value, int size) = 0;

    // ───────────────────── Optional privileged / buffered variants ────────────────────
    virtual bool supportsPrivilegedAccess()     const { return false; }
    virtual quint64 readPrivileged(quint64 offset, int size)
    {
        return read(offset, size);
    }
    virtual bool writePrivileged(quint64 offset, quint64 value, int size)
    {
        return write(offset, value, size);
    }

    virtual bool supportsWriteBuffering() const { return false; }
    virtual void flushWriteBuffer() { /* default: no-op */ }

    // ───────────────────────────── Helper utilities (unchanged) ───────────────────────
    bool containsAddress(quint64 phys) const
    {
        return phys >= getBaseAddress() && phys < getBaseAddress() + getSize();
    }

    virtual QString getName()       const = 0;
    virtual quint64 getBaseAddress() const = 0;
    virtual quint64 getSize()        const = 0;

    virtual ~MMIODevice() = default;
};
