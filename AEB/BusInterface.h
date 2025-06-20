// BusInterface.h
#pragma once
#ifndef BusInterface_h__
#define BusInterface_h__

#include <QObject>
#include <QString>


// ============================================================================
// UnifiedBusInterface.H
// -----------------------------------------------------------------------------
// Integration header unifying BusInterface, SystemBus, IRQController, DeviceManager
// Compatible with Qt-based Alpha AXP Emulator architecture
// ============================================================================

#include <QVector>
#include <QHash>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonObject>
#include <QJsonArray>

class IRQController;
class SystemBus;
class DeviceManager;

// -----------------------------------------------------------------------------
// BusInterface: Abstract interface for all bus-mappable devices
// -----------------------------------------------------------------------------
/**
 * @brief Interface for memory-mapped devices on the Alpha AXP system bus
 *
 * Devices implementing this interface can be attached to the system bus
 * and will respond to memory reads/writes in their assigned address range.
 * This interface is used by both the SystemBus and MMIOManager to
 * correctly route memory requests to devices.
 */
class BusInterface {
public:
	virtual ~BusInterface() {}

	/**
	* @brief Get the device identifier
	* @return Device identifier string
	*/
	virtual QString identifier() const = 0;

	/**
	* @brief Get a human-readable description of the device
	* @return Device description string
	*/
	virtual QString description() const = 0;
	/**
	* @brief Read data from device at specified offset
	* @param address The device-relative address to read from
	* @param size The size of the read (1, 2, 4, or 8 bytes)
	* @return The data read from the device
	*/
	virtual quint64 read(quint64 offset, int size) = 0;
	virtual quint64 read(quint64 offset) = 0;
	// NEW privileged methods with default implementations:
	virtual quint64 readPrivileged(quint64 offset, int size) {
		// Default: privileged access same as normal access
		return read(offset, size);
	}
	/**
	 * @brief Write data to device at specified offset
	 * @param address The device-relative address (offset) to write to
	 * @param data The data (value) to write
	 * @param size The size of the write (1, 2, 4, or 8 bytes)
	 */
	virtual bool write(quint64 offset, quint64 value) = 0;
	virtual bool write(quint64 offset, quint64 value, int size) = 0;
	



	virtual bool writePrivileged(quint64 offset, quint64 value, int size) {
		// Default: privileged access same as normal access
		return write(offset, value, size);
	}

	virtual bool supportsPrivilegedAccess() const {
		return false; // Most devices don't need special privilege handling
	}

	virtual bool supportsWriteBuffering() const {
		return false; // Most devices don't buffer writes
	}
	/**
	* @brief Check if the physical address is handled by this device
	* @param address The physical memory address to check
	* @return True if the address belongs to this device
	*/
	virtual bool isDeviceAddress(quint64 addr)  = 0;

	/**
	* @brief Reset the device to its initial state
	*/
	virtual void reset() = 0;

	virtual quint64 getBaseAddress()  = 0;
	virtual quint64 getSize() const = 0;

	/**
	 * @brief Check if the device can generate interrupts
	 * @return True if the device can generate interrupts
	 */
	virtual bool canInterrupt() const { return false; }

	/**
	 * @brief Get the interrupt vector for this device
	 * @return The IRQ vector number (0 if not applicable)
	 */
	virtual quint8 interruptVector()  { return 0; }
	virtual void connectIRQController(IRQController* irq) { Q_UNUSED(irq); }
	virtual quint64 size() const = 0;

	/*
	 @brief perform mapping from the Json based configuration.
	*/
	virtual void setMemoryMapping(quint64 base, quint64 size) = 0; // 🆕 Added for JSON-based config


};






#endif // BusInterface_h__