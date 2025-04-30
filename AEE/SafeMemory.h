// SafeMemory.h
#pragma once
#ifndef SafeMemory_h__
#define SafeMemory_h__

#include <QObject>
#include <QVector>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include "MMIOManager.h"

/**
 * @brief Thread-safe memory subsystem with MMIO support
 *
 * SafeMemory provides a thread-safe interface to system memory,
 * including support for memory-mapped I/O through MMIOManager.
 * It serves as the main memory interface for the Alpha CPU.
 */
class SafeMemory : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new SafeMemory system
     * @param mmioManager Optional MMIO manager for device access
     * @param initialSize Initial memory size in bytes
     * @param parent Optional QObject parent
     */
    explicit SafeMemory(MMIOManager* mmioManager = nullptr, quint64 initialSize = 1024 * 1024, QObject* parent = nullptr);

    /**
     * @brief Resize the physical memory
     * @param newSize New size in bytes
     */
    void resize(quint64 newSize);

    /**
     * @brief Get current physical memory size
     * @return Size in bytes
     */
    quint64 size() const;

    /**
     * @brief Read a byte from memory or MMIO device
     * @param address Physical address to read from
     * @return The byte value
     */
    quint8 readUInt8(quint64 address);

    /**
     * @brief Read a 16-bit word from memory or MMIO device
     * @param address Physical address to read from
     * @return The word value
     */
    quint16 readUInt16(quint64 address);

    /**
     * @brief Read a 32-bit longword from memory or MMIO device
     * @param address Physical address to read from
     * @return The longword value
     */
    quint32 readUInt32(quint64 address);

    /**
     * @brief Read a 64-bit quadword from memory or MMIO device
     * @param address Physical address to read from
     * @return The quadword value
     */
    quint64 readUInt64(quint64 address);

    /**
     * @brief Write a byte to memory or MMIO device
     * @param address Physical address to write to
     * @param value The byte value to write
     */
    void writeUInt8(quint64 address, quint8 value);

    /**
     * @brief Write a 16-bit word to memory or MMIO device
     * @param address Physical address to write to
     * @param value The word value to write
     */
    void writeUInt16(quint64 address, quint16 value);

    /**
     * @brief Write a 32-bit longword to memory or MMIO device
     * @param address Physical address to write to
     * @param value The longword value to write
     */
    void writeUInt32(quint64 address, quint32 value);

    /**
     * @brief Write a 64-bit quadword to memory or MMIO device
     * @param address Physical address to write to
     * @param value The quadword value to write
     */
    void writeUInt64(quint64 address, quint64 value);

    /**
     * @brief Fetch an instruction from memory (used by CPU)
     * This handles instruction caching and alignment
     * @param address Physical address to fetch from
     * @return The 32-bit instruction word
     */
    quint32 fetchInstruction(quint64 address);

    /**
     * @brief Set the MMIO manager
     * @param manager Pointer to MMIO manager
     */
    void setMMIOManager(MMIOManager* manager);

    /**
     * @brief Load a binary file into memory
     * @param filename Path to the binary file
     * @param loadAddress Physical address to load at
     * @return True if successful
     */
    bool loadBinary(const QString& filename, quint64 loadAddress);

    bool dumpMemory(const QString& filename, quint64 startAddress, quint64 size);
    void clear(quint64 startAddress, quint64 size, quint8 value);
    void copyMemory(quint64 destination, quint64 source, quint64 size);
signals:
    /**
     * @brief Signal emitted when memory is read
     * @param address Address that was read
     * @param value Value that was read
     * @param size Size of the read in bytes
     */
    void memoryRead(quint64 address, quint64 value, int size);

    /**
     * @brief Signal emitted when memory is written
     * @param address Address that was written
     * @param value Value that was written
     * @param size Size of the write in bytes
     */
    void memoryWritten(quint64 address, quint64 value, int size);

private:
    QVector<quint8> memory;
    mutable QReadWriteLock memoryLock;
    MMIOManager* mmioManager;

    /**
     * @brief Check if address is valid physical memory
     * @param address Address to check
     * @param size Size of the access
     * @return True if address is valid
     */
    bool isValidAddress(quint64 address, int size) const;
};

#endif // SafeMemory_h__

