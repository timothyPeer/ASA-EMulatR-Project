// SafeMemory.h
#pragma once
#ifndef SafeMemory_h__
#define SafeMemory_h__

#include <QObject>
#include <QVector>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>
#include <QMutex>
#include <QByteArray>
#include "../AEE/MMIOManager.h"
#include "../AESH/QSettingsConfigLoader.h"
#include "alphajitprofiler.h"
#include "../intelhexloader.h"
#include "../AEJ/enumerations/enumMemoryPerm.h"


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
    explicit SafeMemory(QObject* parent = nullptr);

	// SafeMemory.cpp
	void flushWrites()
	{
		QMutexLocker locker(&m_mutex);

#ifdef USE_MEMORY_MAPPED_FILES
		// Memory-mapped file implementation
		if (m_mappedFile && m_mappedFileData) {
			// Ensure all writes are committed to the file
			if (msync(m_mappedFileData, m_size, MS_SYNC) != 0) {
				qWarning() << "Failed to sync memory-mapped file:" << strerror(errno);
			}
			DEBUG_LOG("SafeMemory: Memory-mapped file synchronized");
		}
#else
		// Regular memory implementation
		// In normal mode, writes are immediately committed to RAM
		// so flushing is essentially a no-op, but we can:
		// 1. Clear any internal write coalescing buffers
		// 2. Ensure cache coherency if using special memory
		// 3. Log the operation for debugging

		DEBUG_LOG("SafeMemory: Write buffers flushed (in-memory mode)");
#endif

		// Common operations regardless of memory type
		emit sigWritesFlushed();
	}

	// Passed in from AlphaSMPManager
	void attachIRQController(IRQController* irQController_) { m_irqController = irQController_; }
    /**
    * @brief Connect a profiler to track memory operations
    * @param profiler Pointer to the profiler instance
    */
    //void attachProfiler(AlphaJITProfiler* profiler) { m_profiler = profiler; }
	void attachSystemLoader(ConfigLoader* systemLoader_) { m_configLoader = systemLoader_; }
	void clear(quint64 startAddress, quint64 size, quint8 value);
	void copyMemory(quint64 destination, quint64 source, quint64 size);
    void dumpMemory(quint64 startAddr, quint64 length);
    /**
    * @brief Fetch an instruction from memory (used by CPU)
    * This handles instruction caching and alignment
    * @param address Physical address to fetch from
    * @return The 32-bit instruction word
    */
    quint32 fetchInstruction(quint64 address);

   
	// Get a direct pointer to physical memory (dangerous, use with caution!)
	quint8* getPhysicalPointer(quint64 physicalAddress) {
		if (!isValidAddress(physicalAddress, 1)) {
			return nullptr;
		}
		return &m_ram[static_cast<int>(physicalAddress)];
	}
    
    /**
    * @brief Load a binary file into memory
    * @param filename Path to the binary file
    * @param loadAddress Physical address to load at
    * @return True if successful
    */

    bool loadBinary(const QString& filename, quint64 loadAddress);

    /**
 * @brief Thread-safe memory subsystem with region mapping support
 *
 * SafeMemory with mapRegion extends the internal RAM buffer to ensure
 * that any write or execute region at a given physical address is backed
 * by allocated memory. New bytes are zero-initialized if the region
 * extends past the current memory size.
 *
 * @param address Physical start address of the region to map
 * @param size    Size of the region in bytes
 * @param perm    Memory permissions (Read/Write/Execute)
 *
 * @note This method wraps resize(end, false) to preserve existing contents
 *       and zero-initialize only the newly allocated bytes.
 * @see Alpha AXP System Reference Manual Version 6, Part One, Chapter 2
 *      "Basic Architecture", Section 2.1 Addressing, p.2-1 for memory
 *      addressing fundamentals.
 */
    void mapRegion(quint64 address, quint64 size, MemoryPerm perm);
    /**
    * @brief Provide a prefetch hint to memory subsystem
    * @param address Physical address to prefetch
    * @param size Size of data to prefetch in bytes
    * @param evictNext Hint that next sequential block should be evicted
    * @return True if prefetch hint was accepted
    */
    bool prefetchHint(quint64 address, int size, bool evictNext = false);
    /**
   * @brief Read a byte from memory or MMIO device
   * @param address Physical address to read from
   * @return The byte value
   */
    quint8 readUInt8(quint64 address,  quint64 pc = 0);
    /**
     * @brief Resize the physical memory
     * @param newSize New size in bytes
     * @param initialize  Initialize memory only 
     */

     /**
     * @brief Read a 16-bit word from memory or MMIO device
     * @param address Physical address to read from
     * @return The word value
     */
    quint16 readUInt16(quint64 address, quint64 pc = 0);

    /**
     * @brief Read a 32-bit longword from memory or MMIO device
     * @param address Physical address to read from
     * @return The longword value
     */
    quint32 readUInt32(quint64 address, quint64 pc = 0);
    /**
     * @brief Read a 64-bit quadword from memory or MMIO device
     * @param address Physical address to read from
     * @return The quadword value
     */
    quint64 readUInt64(quint64 address, quint64 pc = 0);
    void resize(quint64 newSize, bool initialize=true);
    /**
     * @brief Get current physical memory size
     * @return Size in bytes
     */
    quint64 size() const;


    /**
 * @brief Write a block of bytes to memory
 * @param address Physical address to write to
 * @param data Pointer to source data
 * @param size Number of bytes to write
 * @param pc Program counter (for error reporting)
 */
    void writeBytes(quint64 address, const quint8* data, quint64 size, quint64 pc = 0);

    /**
     * @brief Write a QByteArray to memory (convenience overload)
     * @param address Physical address to write to
     * @param data QByteArray containing the data to write
     * @param pc Program counter (for error reporting)
     */
    void writeBytes(quint64 address, const QByteArray& data, quint64 pc = 0);
    /**
     * @brief Write a byte to memory or MMIO device
     * @param address Physical address to write to
     * @param value The byte value to write
     */
    void writeUInt8(quint64 address, quint8 value, quint64 pc = 0);

    /**
     * @brief Write a 16-bit word to memory or MMIO device
     * @param address Physical address to write to
     * @param value The word value to write
     */
    void writeUInt16(quint64 address, quint16 value, quint64 pc = 0);

    /**
     * @brief Write a 32-bit longword to memory or MMIO device
     * @param address Physical address to write to
     * @param value The longword value to write
     */
    void writeUInt32(quint64 address, quint32 value, quint64 pc = 0);

    /**
     * @brief Write a 64-bit quadword to memory or MMIO device
     * @param address Physical address to write to
     * @param value The quadword value to write
     */
    void writeUInt64(quint64 address, quint64 value, quint64 pc = 0);

   
signals:

    /**
 * @brief Signal emitted when a memory region is mapped
 * @param address Start address of the mapped region
 * @param size Size of the mapped region
 * @param permissions Memory permissions for the region
 */
    void sigRegionMapped(quint64 address, quint64 size, quint8 permissions);
    /**
     * @brief Signal emitted when memory is read
     * @param address Address that was read
     * @param value Value that was read
     * @param size Size of the read in bytes
     */
    void sigMemoryRead(quint64 address, quint64 value, int size);

    /**
     * @brief Signal emitted when memory is written
     * @param address Address that was written
     * @param value Value that was written
     * @param size Size of the write in bytes
     */
    void sigMemoryWritten(quint64 address, quint64 value, int size);

    void sigReservationCleared(quint64 physicalAddr, int size);
    void sigWritesFlushed();   

private:
    //QVector<quint8> m_safeMemory;
    mutable QReadWriteLock m_memoryLock;
    QMutex m_mutex;
    IRQController* m_irqController;         // Passed in from AlphaSMPManager
    ConfigLoader* m_configLoader;           // Passed in from AlphaSMPManager
    //AlphaJITProfiler* m_profiler = nullptr;  // Profiler for tracking memory operations

    QVector<quint8>      m_ram;
    void clearReservations(quint64 physicalAddress, int size);
    /**
     * @brief Check if address is valid physical memory
     * @param address Address to check
     * @param size Size of the access
     * @return True if address is valid
     */
    bool isValidAddress(quint64 address, int size) const;
};

#endif // SafeMemory_h__

