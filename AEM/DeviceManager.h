#pragma once
#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QReadWriteLock>
#include <QScopedPointer>
#include <QDebug>

#include "..\AEB\BusInterface.h"
#include "..\AEM\MMIOManager.h"
#include "..\AES\ScsiBusController.h"
#include "..\AES\tulipnic.h"
#include "..\AEB\UartDevice.h"

/*
DeviceManager unified header has been created and includes:

Full device registration and lookup with MMIOManager integration

Signal support for deviceAdded and deviceRemoved

Thread-safe access with QReadWriteLock

JSON-based configuration loader via loadFromJson()

Metadata accessors: getDeviceIds() and getDeviceSummaries()

Default device initializers for UART, SCSI, and NIC
*/
class DeviceManager : public QObject {
	Q_OBJECT

public:

	DeviceManager(SystemBus* sbus, IRQController* ictr) : bus(sbus), irq(ictr) {}

	void registerDevice(BusInterface* dev, quint64 base, quint64 size) {
		bus->attach(dev, base, size);
		if (dev->canInterrupt())
			irq->registerDevice(dev->interruptVector(), dev);
	}

	void resetAll() {
		for (auto& entry : busMap)
			entry.device->reset();
	}
	void setMMIOManager(MMIOManager* manager) {
		mmioManager = manager;
	}

	bool addDevice(BusInterface* device) {
		QWriteLocker locker(&deviceLock);
		const QString id = device->identifier();
		if (devices.contains(id)) {
			qWarning() << "Device already exists:" << id;
			return false;
		}
		devices.insert(id, device);
		if (mmioManager)
			mmioManager->mapDevice(device, device->baseAddress(), device->size());
		emit deviceAdded(id);
		return true;
	}

	bool removeDevice(const QString& id) {
		QWriteLocker locker(&deviceLock);
		if (!devices.contains(id)) return false;
		BusInterface* dev = devices.take(id);
		if (mmioManager)
			mmioManager->unmapDevice(dev);
		delete dev;
		emit deviceRemoved(id);
		return true;
	}

	BusInterface* getDevice(const QString& id) const {
		QReadLocker locker(&deviceLock);
		return devices.value(id, nullptr);
	}

	QList<QString> getDeviceIds() const {
		QReadLocker locker(&deviceLock);
		return devices.keys();
	}

	QStringList getDeviceSummaries() const {
		QReadLocker locker(&deviceLock);
		QStringList summaries;
		for (auto* device : devices)
			summaries << QString("%1: %2 [%3-%4]")
			.arg(device->identifier())
			.arg(device->description())
			.arg(device->baseAddress(), 0, 16)
			.arg(device->baseAddress() + device->size() - 1, 0, 16);
		return summaries;
	}

	void resetAllDevices() {
		QReadLocker locker(&deviceLock);
		for (auto* device : devices)
			device->reset();
	}

	void initializeDefaultDevices() {
		addDevice(new UartDevice("OPA0", 0x10000000, 0x1000));
		addDevice(new ScsiBusController("PKA0", 0x20000000, 0x1000));
		addDevice(new TulipNIC("EWA0", 0x30000000, 0x1000));
	}

	bool loadFromJson(const QJsonObject& config) {
		if (!config.contains("devices") || !config["devices"].isArray())
			return false;

		QJsonArray arr = config["devices"].toArray();
		for (const auto& item : arr) {
			if (!item.isObject()) continue;
			QJsonObject obj = item.toObject();
			QString type = obj["type"].toString();
			QString id = obj["id"].toString();
			quint64 addr = obj["base"].toString().toULongLong(nullptr, 0);
			quint32 size = obj["size"].toInt();

			if (type == "UART") {
				addDevice(new UartDevice(id, addr, size));
			}
			else if (type == "SCSI") {
				addDevice(new ScsiBusController(id, addr, size));
			}
			else if (type == "NIC") {
				addDevice(new TulipNIC(id, addr, size));
			}
			else {
				qWarning() << "Unknown device type in config:" << type;
			}
		}
		return true;
	}

signals:
	void deviceAdded(const QString& id);
	void deviceRemoved(const QString& id);

private:
	QHash<QString, BusInterface*> devices;
	MMIOManager* mmioManager;
	mutable QReadWriteLock deviceLock;
	SystemBus* bus;
	IRQController* irq;
	QVector<SystemBus::Mapping> busMap;
};

#endif // DEVICEMANAGER_H
