#pragma once
#ifndef MMIOManager_h__
#define MMIOManager_h__

#include <QObject>
#include <QMap>
#include <QVector>
#include <QReadWriteLock>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QJsonArray>
#include <QString>
#include <functional>
#include "../AEB/BusInterface.h"
#include "../AEB/IRQController.h"
#include "../AESH/QSettingsConfigLoader.h"
#include "../AEB/DeviceInterface.h"
#include "../AEB/systembus.h"
#include "MMIODevice.h"
#include "../AEJ/GlobalMacro.h"
#include "AsaNamespaces.h"
#include "../AEB/TsunamiCSR.h"
#include "../AEB/PCISparseWindow.h"
#include "../AEB/PCIDenseWindow.h"
//#include <PCIConfigWindow.h>
#include "AlphaMMIOAddressChecker.h"
#include "./enumerations/enumCpuModel.h"
#include "TranslationResult.h"



/**
 * @brief MMIOManager - Manages memory-mapped I/O regions and dispatches accesses
 * Reference: Alpha AXP System Architecture, MMIO Dispatch Semantics
 * 
 * 
 * Addressing Rules:
 * 
 * Recommended MMIO Base Addresses and Sizes

    Device Class	        Suggested Base Address	        Size (bytes)	        Notes
    TulipNIC	            0x10000000	                    0x100 (256)	            Enough for 16 x 32-bit CSRs
    UartDevice	            0x10000100	                    0x08 (8)	            8 register slots, 1 byte each
    VirtualScsiController	0x10000200	                    0x100 (256)         	For command queues, LUN selector, DMA config
    (Reserved future use)	0x10000300	                    0x100	                Room for another MMIO device
    MMIO ROM / Config	    0x1FFF0000	                    0x10000	                ROM page, IDPROM, device summary


    Usage and Implementation: 
	mmioManager->mapDevice(tulipNic, 0x10000000, 0x100);
    mmioManager->mapDevice(uartDevice, 0x10000100, 0x08);
    mmioManager->mapDevice(scsiController, 0x10000200, 0x100);

 */
// TODO MMIOManager Checklist
 // ============================================================================
 // MMIOManager Maintenance Guidance
 // ----------------------------------------------------------------------------
 //
 // 1. ResetHandler() wrong syntax
 //    - Issue: Original code used regions.Clear();
 //    - Fix: QVector uses lowercase clear(), not Clear().
 //      ➔ Change regions.Clear() → regions.clear();
 //
 // 2. handler field unused
 //    - Issue: A 'handler' pointer was declared globally but never used.
 //    - Fix: This field is unnecessary and has been removed.
 //      ➔ Only Region::handler is used per mapped device.
 //
 // 3. Only 32-bit read/write supported
 //    - Issue: Early version only had uint32_t read(uint64_t addr).
 //    - Guidance: Expand support for 8-bit, 16-bit, 64-bit MMIO accesses
 //      if devices require it (e.g., UARTs are often 8-bit).
 //      ➔ Add overloaded read8(), read16(), read32(), read64() methods.
 //
 // 4. No isMMIOAddress() helper
 //    - Issue: No fast way to check if an address is MMIO-mapped.
 //    - Fix: Added bool isMMIOAddress(uint64_t addr) method.
 //      ➔ This is critical for SafeMemory or AlphaMemorySystem
 //         to distinguish RAM from device accesses.
 //
 // 5. No thread safety (multi-core/SMP risk)
 //    - Issue: QVector regions is not protected against concurrent modification.
 //      Alpha SMP systems (2+ CPUs) may race on MMIO lookup.
 //    - Fix: Added QMutex lock around mapDevice(), read(), write(), reset(), isMMIOAddress().
 //      ➔ Guarantees safe concurrent access from multiple AlphaCPU instances.
 //
 // ----------------------------------------------------------------------------
 // Design Principles:
 // - MMIOManager owns no devices; it maps handlers.
 // - Device handlers must be stable during system lifetime.
 // - AlphaCPU, SafeMemory, and AlphaMemorySystem must call MMIOManager
 //   for all MMIO-mapped address ranges.
 //
 // ----------------------------------------------------------------------------


class MMIOManager : public QObject {
    Q_OBJECT


		struct MMIOWriteEntry {
		quint64 physicalAddr;
		quint64 value;
		quint64 timestamp;
		int size;
		bool pending;
		QString deviceName;
	};
	
    struct Window { quint64 base, size; };
	QVector<Window> mmioWindows; // ↑ Simple range list separate from per-device mappings
    CpuModel m_cpuModel;

public:
    explicit MMIOManager( QObject* parent = nullptr)
        : QObject(parent){
        qDebug() << "MMIOManager: Initialized";
    }
    ~MMIOManager() {}

	void registerWindow(quint64 base, quint64 size)
	{
		QWriteLocker L(&lock);
		mmioWindows.append({ base, size });
	}

	//void mapDevice(DeviceInterface* device, quint64 base, quint64 size);



    void attachSystemBus(SystemBus* sysBus) { m_systemBus = sysBus;  }
	void attachIrqController(IRQController* irq) { m_irqController = irq; }

    void initializeCpuModel(CpuModel cpuModel) { m_cpuModel = cpuModel; }
    bool remapDevice(BusInterface* device, quint64 newBase, quint64 newSize) {
        if (!unMapDevice(device)) return false;
        return mapDevice(device, newBase, newSize);
    }
    TranslationResult translateAddress()
    {
        //TODO
        return TranslationResult();
    }
    bool unMapDevice(BusInterface* device) {
        if (!device) return false;
        QWriteLocker locker(&lock);
        for (int i = 0; i < deviceMappings.size(); ++i) {
            if (deviceMappings[i].device == device) {
                deviceMappings.removeAt(i);
                emit deviceUnregistered(device);
                return true;
            }
        }
        return false;
    }

    bool unMapDeviceById(const QString& id) {
        QWriteLocker locker(&lock);
        for (int i = 0; i < deviceMappings.size(); ++i) {
            if (deviceMappings[i].device->identifier() == id) {
                emit deviceUnregistered(deviceMappings[i].device);
                deviceMappings.removeAt(i);
                return true;
            }
        }
        return false;
    }

    quint64 readMMIO(quint64 address, int size, quint64 pc = 0) {
        QReadLocker locker(&lock);
        const DeviceMapping* m = findMapping(address);
        if (!m) return 0;
        quint64 val = m->device->read(m->getRelativeAddress(address), size);
        emit deviceAccessed(m->device, m->getRelativeAddress(address), false, size, val);
        return val;
    }

	bool mapDevice(DeviceInterface* device, quint64 base, quint64 size)
	{
		QWriteLocker locker(&lock);

		// Ensure bounds are valid and aligned
		Q_ASSERT(device);
		Q_ASSERT(size > 0);
		Q_ASSERT((base & (size - 1)) == 0); // alignment

		// Let the device know its assigned mapping
		device->setMemoryMapping(base, size);

		// Register with system bus
		if (m_systemBus)
			m_systemBus->mapDevice(device, base, size);
	}
	bool mapDevice(BusInterface* device, quint64 baseAddress, quint64 size) {
		if (!device) return false;
		QWriteLocker locker(&lock);
		for (const auto& m : deviceMappings) {
			if ((baseAddress < m.baseAddress + m.size) &&
				(baseAddress + size > m.baseAddress)) {
				qWarning() << "MMIOManager: Overlap detected for device" << device->identifier();
				return false;
			}
		}
		deviceMappings.append({ device, baseAddress, size });
		emit deviceRegistered(device, baseAddress, size);
		return true;
	}

    
    bool writeMMIO(quint64 address, quint64 data, int size, quint64 pc = 0) {
        QReadLocker locker(&lock);
        const DeviceMapping* m = findMapping(address);
        if (!m) return false;
        m->device->write(m->getRelativeAddress(address), data, size);
        emit deviceAccessed(m->device, m->getRelativeAddress(address), true, size, data);
        return true;
    }

	bool isMMIOAddress(quint64 address) const  {
		// Check manually mapped devices first
        QReadLocker locker(&lock);
		if (findMapping(address) != nullptr) {
			return true;
		}

		// Check chipset-specific MMIO regions
		return AlphaMMIOAddressChecker::isMMIOAddress(address, m_cpuModel);
	}
    
    bool hasDevice(quint64 address) const {
        return isMMIOAddress(address);
    }

    QVector<BusInterface*> getDevices() const {
        QReadLocker locker(&lock);
        QVector<BusInterface*> list;
        for (const auto& m : deviceMappings) list.append(m.device);
        return list;
    }

    BusInterface* getDeviceByIdentifier(const QString& id) const {
        QReadLocker locker(&lock);
        for (const auto& m : deviceMappings)
            if (m.device->identifier() == id) return m.device;
        return nullptr;
    }

    BusInterface* getDeviceInfo(quint64 address, quint64& base, quint64& size) const {
        QReadLocker locker(&lock);
        const DeviceMapping* m = findMapping(address);
        if (!m) return base = size = 0, nullptr;
        base = m->baseAddress; size = m->size;
        return m->device;
    }

    QVector<QPair<quint64, quint64>> getMappingRanges() const {
        QReadLocker locker(&lock);
        QVector<QPair<quint64, quint64>> ranges;
        for (const auto& m : deviceMappings)
            ranges.append({ m.baseAddress, m.size });
        return ranges;
    }

    QString getDeviceSummary() const {
        QReadLocker locker(&lock);
        QString result;
        for (const auto& m : deviceMappings) {
            result += QString("[MMIO] %1 @ 0x%2 (%3 bytes)\n")
                .arg(m.device->identifier())
                .arg(m.baseAddress, 0, 16)
                .arg(m.size);
        }
        return result.isEmpty() ? "No MMIO devices registered." : result;
    }

    void resetAllDevices() {
        QReadLocker locker(&lock);
        for (const auto& m : deviceMappings) m.device->reset();
    }

    void setLoggingCallback(std::function<void(const QString&)> cb) {
        loggingCallback = std::move(cb);
    }

    bool loadFromConfig(const QJsonArray& arr) {
        QWriteLocker locker(&lock);
        for (const auto& item : arr) {
            if (!item.isObject()) continue;
            QJsonObject obj = item.toObject();
            QString id = obj["id"].toString();
            quint64 base = obj["base"].toString().toULongLong(nullptr, 0);
            quint64 size = obj["size"].toString().toULongLong(nullptr, 0);
            BusInterface* iface = getDeviceByIdentifier(id);
            if (iface) mapDevice(iface, base, size);
        }
        return true;
    }

    QString dumpMMIOMap() const {
        QReadLocker locker(&lock);
        QString out;
        for (const auto& m : deviceMappings) {
            out += QString("0x%1 - 0x%2 : %3\n")
                .arg(m.baseAddress, 0, 16)
                .arg(m.baseAddress + m.size - 1, 0, 16)
                .arg(m.device->identifier());
        }
        return out;
    }

    /**
 * @brief Read from MMIO device with privileged access
 * @param physicalAddr Physical address of MMIO register
 * @param size Size of read (1, 2, 4, or 8 bytes)
 * @return Value read from MMIO device
 *
 * Privileged MMIO reads bypass any access control mechanisms that devices
 * might implement. This is used by PAL code, kernel drivers, and system
 * initialization routines that need unrestricted hardware access.
 */
//     quint64 readMMIOPrivileged(quint64 physicalAddr, int size, quint64 pc = 0)
//     {
//         QMutexLocker locker(&m_deviceMutex);
// 
//         // Find the device responsible for this address
//         MMIODevice* device = findDeviceForAddress(physicalAddr);
// 
//         if (!device) {
//             qWarning() << "[MMIOManager] No device found for privileged read at 0x"
//                 << QString::number(physicalAddr, 16);
//             return 0xFFFFFFFFFFFFFFFFULL;  // Bus error value
//         }
// 
//         // Calculate device-relative offset
//         quint64 deviceOffset = physicalAddr - device->getBaseAddress();
// 
//         // Perform privileged read
//         quint64 value = 0;
//         bool success = false;
// 
//         // Check if device supports privileged access
//         if (device->supportsPrivilegedAccess()) {
//             value = device->readPrivileged(deviceOffset, size);
//             success = true;
//         }
//         else {
//             // Fall back to normal read for devices without privilege support
//             value = device->read(deviceOffset, size);
//             success = true;
//         }
// 
//         if (success) {
//             emit mmioRead(physicalAddr, value, size, device->getName());
//             DEBUG_LOG(QString("MMIOManager: Privileged read 0x%1 from %2 at 0x%3")
//                 .arg(value, size * 2, 16, QChar('0'))
//                 .arg(device->getName())
//                 .arg(physicalAddr, 16, 16, QChar('0')));
//         }
// 
//         return value;
//     }

	quint64 readMMIOPrivileged(quint64 address, int size, quint64 pc) {
		// Bypass normal access checks
		QReadLocker locker(&lock);
		const DeviceMapping* m = findMapping(address);
		if (!m) return 0xFFFFFFFFFFFFFFFFULL;

		// Call device with privileged access if supported
		return m->device->readPrivileged(m->getRelativeAddress(address), size);
	}

    /**
     * @brief Write to MMIO device with privileged access
     * @param physicalAddr Physical address of MMIO register
     * @param value Value to write
     * @param size Size of write (1, 2, 4, or 8 bytes)
     * @return True if write succeeded, false otherwise
     *
     * Privileged MMIO writes bypass access control and can modify any device
     * register regardless of current privilege level or protection mechanisms.
     */
//     bool writeMMIOPrivileged(quint64 physicalAddr, quint64 value, int size, quint64 pc = 0)
//     {
//         QMutexLocker locker(&m_deviceMutex);
// 
//         // Find the device responsible for this address
//         MMIODevice* device = findDeviceForAddress(physicalAddr);
// 
//         if (!device) {
//             qWarning() << "[MMIOManager] No device found for privileged write at 0x"
//                 << QString::number(physicalAddr, 16);
//             return false;
//         }
// 
//         // Calculate device-relative offset
//         quint64 deviceOffset = physicalAddr - device->getBaseAddress();
// 
//         DEBUG_LOG(QString("MMIOManager: Privileged write 0x%1 to %2 at 0x%3")
//             .arg(value, size * 2, 16, QChar('0'))
//             .arg(device->getName())
//             .arg(physicalAddr, 16, 16, QChar('0')));
// 
//         // Perform privileged write
//         bool success = false;
// 
//         // Check if device supports privileged access
//         if (device->supportsPrivilegedAccess()) {
//             success = device->writePrivileged(deviceOffset, value, size);
//         }
//         else {
//             // Fall back to normal write for devices without privilege support
//             success = device->write(deviceOffset, value, size);
//         }
// 
//         if (success) {
//             emit mmioWritten(physicalAddr, value, size, device->getName());
// 
//             // Add to write buffer if device supports write buffering
//             if (device->supportsWriteBuffering()) {
//                 addToMMIOWriteBuffer(physicalAddr, value, size, device->getName());
//             }
//         }
// 
//         return success;
//     }

	bool writeMMIOPrivileged(quint64 address, quint64 value, int size, quint64 pc) {
		QWriteLocker locker(&lock);
		const DeviceMapping* m = findMapping(address);
		if (!m) return false;

		return m->device->writePrivileged(m->getRelativeAddress(address), value, size);
	}
    /**
     * @brief Flush all pending MMIO writes
     *
     * Some MMIO devices support write buffering for performance. This method
     * ensures all buffered writes are committed to the actual device registers.
     * Essential for memory barrier operations and device synchronization.
     */
    void flushWrites()
    {
        QMutexLocker bufferLocker(&m_mmioWriteBufferMutex);
        QMutexLocker deviceLocker(&m_deviceMutex);

        DEBUG_LOG("MMIOManager: Flushing all MMIO write buffers");

        // Process all pending MMIO writes
        if (!m_mmioWriteBuffer.isEmpty()) {
            drainWriteBuffers();
        }

        // Flush write buffers in all devices
        for (auto& device : m_devices) {
            if (device && device->supportsWriteBuffering()) {
                device->flushWriteBuffer();
            }
        }

        DEBUG_LOG("MMIOManager: MMIO write flush completed");
    }

    /**
     * @brief Flush MMIO writes for a specific address range
     * @param startAddr Starting physical address of range
     * @param endAddr Ending physical address of range
     *
     * Flushes only the writes within the specified address range.
     * Useful for device-specific or register-specific flushing.
     */
    void flushWrites(quint64 startAddr, quint64 endAddr)
    {
        QMutexLocker bufferLocker(&m_mmioWriteBufferMutex);
        QMutexLocker deviceLocker(&m_deviceMutex);

        DEBUG_LOG(QString("MMIOManager: Flushing MMIO writes for range 0x%1-0x%2")
            .arg(startAddr, 16, 16, QChar('0'))
            .arg(endAddr, 16, 16, QChar('0')));

        // Flush writes in the specified address range
        for (auto it = m_mmioWriteBuffer.begin(); it != m_mmioWriteBuffer.end();) {
            if (it->physicalAddr >= startAddr && it->physicalAddr <= endAddr) {
                if (it->pending) {
                    commitMMIOWrite(*it);
                    it = m_mmioWriteBuffer.erase(it);
                }
                else {
                    ++it;
                }
            }
            else {
                ++it;
            }
        }

        // Flush device write buffers for devices in range
        for (auto& device : m_devices) {
            if (device && device->supportsWriteBuffering()) {
                quint64 deviceBase = device->getBaseAddress();
                quint64 deviceEnd = deviceBase + device->getSize();

                // Check if device overlaps with flush range
                if (!(deviceEnd <= startAddr || deviceBase >= endAddr)) {
                    device->flushWriteBuffer();
                }
            }
        }

        DEBUG_LOG("MMIOManager: Range MMIO write flush completed");
    }

    /**
     * @brief Check if MMIO address supports write buffering
     * @param physicalAddr Physical address to check
     * @return True if writes to this address are buffered
     */
    bool isMMIOWriteBuffered(quint64 physicalAddr) const
    {
        QMutexLocker locker(&m_deviceMutex);

        MMIODevice* device = findDeviceForAddress(physicalAddr);
        return device ? device->supportsWriteBuffering() : false;
    }

    /**
     * @brief Drain all MMIO write buffers immediately
     *
     * Forces immediate commit of all buffered MMIO writes.
     * Note: Caller must hold m_mmioWriteBufferMutex
     */
    void drainWriteBuffers()
    {
        // Process all pending writes
        for (auto& entry : m_mmioWriteBuffer) {
            if (entry.pending) {
                commitMMIOWrite(entry);
            }
        }

        // Clear the buffer
        m_mmioWriteBuffer.clear();
        DEBUG_LOG("MMIOManager: MMIO write buffers drained");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // MMIO WRITE BUFFER IMPLEMENTATION
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Add MMIO write to buffer for deferred processing
     * @param physicalAddr Physical address of write
     * @param value Value written
     * @param size Size of write
     * @param deviceName Name of device being written to
     */
    void addToMMIOWriteBuffer(quint64 physicalAddr, quint64 value, int size, const QString& deviceName)
    {
        QMutexLocker locker(&m_mmioWriteBufferMutex);

        // Check if buffer is full
        if (isMMIOWriteBufferFull()) {
            // Force drain some entries
            drainWriteBuffers();
        }

        MMIOWriteEntry entry;
        entry.physicalAddr = physicalAddr;
        entry.value = value;
        entry.timestamp = ++m_mmioWriteTimestamp;
        entry.size = size;
        entry.pending = true;
        entry.deviceName = deviceName;

        m_mmioWriteBuffer.append(entry);

        DEBUG_LOG(QString("MMIOManager: Added to MMIO write buffer: %1 at 0x%2")
            .arg(deviceName)
            .arg(physicalAddr, 16, 16, QChar('0')));
    }

    /**
     * @brief Commit a single MMIO write buffer entry
     * @param entry MMIO write entry to commit
     */
    void commitMMIOWrite(const MMIOWriteEntry& entry)
    {
        MMIODevice* device = findDeviceForAddress(entry.physicalAddr);

        if (device) {
            quint64 deviceOffset = entry.physicalAddr - device->getBaseAddress();

            if (device->supportsPrivilegedAccess()) {
                device->writePrivileged(deviceOffset, entry.value, entry.size);
            }
            else {
                device->write(deviceOffset, entry.value, entry.size);
            }

            DEBUG_LOG(QString("MMIOManager: Committed buffered write to %1 at 0x%2")
                .arg(entry.deviceName)
                .arg(entry.physicalAddr, 16, 16, QChar('0')));
        }
    }

    /**
     * @brief Check if MMIO write buffer is full
     * @return True if buffer should be drained
     */
    bool isMMIOWriteBufferFull() const
    {
        const int MAX_MMIO_WRITE_BUFFER_SIZE = 16;  // Smaller than memory buffer
        return m_mmioWriteBuffer.size() >= MAX_MMIO_WRITE_BUFFER_SIZE;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // HELPER METHOD (may need to be added if not present)
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Find device responsible for a given physical address
     * @param physicalAddr Physical address to look up
     * @return Pointer to device, or nullptr if not found
     */
    MMIODevice* findDeviceForAddress(quint64 physicalAddr) const
    {
        // This method might already exist - if not, here's the implementation
        for (auto& device : m_devices) {
            if (device && device->containsAddress(physicalAddr)) {
                return device.get();
            }
        }
        return nullptr;
    }

signals:
    void deviceAccessed(BusInterface* device, quint64 address, bool isWrite, int size, quint64 data);
    void deviceRegistered(BusInterface* device, quint64 baseAddress, quint64 size);
    void deviceUnregistered(BusInterface* device);
	void mmioRead(quint64 address, quint64 value, int size, const QString& deviceName);
	void mmioWritten(quint64 address, quint64 value, int size, const QString& deviceName);

private:
    struct DeviceMapping {
        BusInterface* device;
        quint64 baseAddress;
        quint64 size;
        bool containsAddress(quint64 addr) const {
            return addr >= baseAddress && addr < baseAddress + size;
        }
        quint64 getRelativeAddress(quint64 addr) const {
            return addr - baseAddress;
        }
    };

    const DeviceMapping* findMapping(quint64 addr) const {
        for (const auto& m : deviceMappings)
            if (m.containsAddress(addr)) return &m;
        return nullptr;
    }

    QVector<QSharedPointer<MMIODevice>> m_devices;
    QVector<DeviceMapping> deviceMappings;
    QVector<MMIOWriteEntry> m_mmioWriteBuffer;

    IRQController*  m_irqController = nullptr;
    ConfigLoader* m_configLoader = nullptr;
    SystemBus* m_systemBus = nullptr;
    mutable QReadWriteLock lock;
    mutable QMutex m_mmioWriteBufferMutex;
    mutable QMutex m_deviceMutex;
    quint64 m_mmioWriteTimestamp = 0;

    std::function<void(const QString&)> loggingCallback;
};

#endif // MMIOManager_h__
