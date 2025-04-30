#pragma once
#ifndef SYSTEMBUS_H
#define SYSTEMBUS_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QDebug>
#include <QtGlobal> // For qFormatHex
#include "BusInterface.h"



// ============================================================================
// SystemBus.h
// -----------------------------------------------------------------------------
// In-memory bus interconnect model for Alpha AXP emulation.
// Maps physical addresses to connected devices.
// Supports multiple devices with non-overlapping memory regions.
// ============================================================================

class SystemBus : public QObject {
	Q_OBJECT

public:
	struct DeviceMapping {
		quint64 start;           // Start of physical memory range
		quint64 end;             // End of physical memory range (inclusive)
		BusInterface* device;    // Device to handle accesses in this range
	};

	explicit SystemBus(QObject* parent = nullptr)
		: QObject(parent) {
		qDebug() << "[SystemBus] Initialized.";
	}

	// Adds a device to the bus at a specific address range.
	void mapDevice(BusInterface* device, quint64 start, quint64 size) {
		QMutexLocker locker(&mutex);
		DeviceMapping mapping{ start, start + size - 1, device };
		mappings.append(mapping);
		qDebug() << "[SystemBus] Mapped device from" << QString("0x%1").arg(start, 0, 16)
			<< "to" << QString("0x%1").arg(start + size - 1, 0, 16);
	}

	// Returns the device responsible for a given physical address.
	BusInterface* resolveDevice(quint64 address) const {
		for (const auto& mapping : mappings) {
			if (address >= mapping.start && address <= mapping.end) {
				return mapping.device;
			}
		}
		return nullptr; // No device found
	}

	// Debug function to dump all mappings
	void dumpMappings() const {
		qDebug() << "[SystemBus] Device mappings:";
		for (const auto& mapping : mappings) {
			qDebug() << " -" << QString("0x%1").arg(mapping.start, 0, 16)
				<< "to" << QString("0x%1").arg(mapping.end, 0, 16)
				<< "=>" << mapping.device;
		}
	}

private:
	QVector<DeviceMapping> mappings; // List of device address mappings
	mutable QMutex mutex;           // Protects mapping changes
};

#endif // SYSTEMBUS_H
