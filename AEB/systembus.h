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
#include "IRQController.h"


// Mapping between address range and device
struct DeviceMapping {
	quint64 startAddr;
	quint64 endAddr;
	BusInterface* device;

	quint64 getRelativeAddress(quint64 addr) const {
		return addr - startAddr;
	}

	bool contains(quint64 addr) const {
		return addr >= startAddr && addr <= endAddr;
	}
};

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
	explicit SystemBus(QObject* parent = nullptr)
		: QObject(parent) {
	}

	void attachIrqController(IRQController* irqController) { m_irqController = irqController;  }

	void mapDevice(BusInterface* device, quint64 startAddr, quint64 size) {
		QMutexLocker locker(&mutex);
		DeviceMapping mapping{ startAddr, startAddr + size - 1, device };
		mappings.append(mapping);
		qDebug() << "[SystemBus] Mapped device from" << QString("0x%1").arg(startAddr, 0, 16)
			<< "to" << QString("0x%1").arg(startAddr + size - 1, 0, 16);
	}

	BusInterface* findDevice(quint64 addr, quint64& relative) const {
		for (const auto& m : mappings) {
			if (m.contains(addr)) {
				relative = m.getRelativeAddress(addr);
				return m.device;
			}
		}
		return nullptr;
	}




private:
	QVector<DeviceMapping> mappings;
	mutable QMutex mutex;
	IRQController* m_irqController; 
};

#endif // SYSTEMBUS_H
