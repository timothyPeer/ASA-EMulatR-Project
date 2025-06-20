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
#include <functional>

#include "BusInterface.h"
#include "..\AEE\MMIOManager.h"
#include "ScsiBusController.h"
#include "TulipNIC_DC21040.h"
#include "UartDevice.h"
#include "IRQController.h"
#include "SystemBus.h"

/**
 * @brief DeviceManager (refactored) - Coordinates MMIO, IRQ, SystemBus, and lifecycle
 *
 * This class no longer tracks its own MMIO mappings. All memory-mapped operations are
 * delegated to MMIOManager. It retains control over device registration, instantiation,
 * IRQ mapping, and JSON-based configuration loading.
 */
class DeviceManager : public QObject {
    Q_OBJECT

public:
    explicit DeviceManager(SystemBus* sbus, IRQController* ictr, QObject* parent = nullptr)
        : QObject(parent), m_systemBus(sbus), m_irqController(ictr), m_mmioManager(nullptr) {
    }

    void attachIrqController(IRQController* irqController) { m_irqController = irqController;  }
    void attachSystemBus(SystemBus* systemBus) { m_systemBus = systemBus;  }
    void attachMmioManager(MMIOManager* mmioManager) { m_mmioManager = mmioManager;  }

    void setLoggingCallback(std::function<void(const QString&)> cb) {
        loggingCallback = std::move(cb);
    }

    // ====================================================================================
    // DEVICE REGISTRATION, CONFIGURATION, LIFECYCLE
    // ====================================================================================

    bool addDevice(BusInterface* device) {
        QWriteLocker locker(&deviceLock);
        const QString id = device->identifier();
        if (devices.contains(id)) {
            qWarning() << "DeviceManager: Device already exists:" << id;
            return false;
        }

        if (m_mmioManager && !m_mmioManager->mapDevice(device, device->getBaseAddress(), device->getSize())) {
            qWarning() << "DeviceManager: Failed to map device into MMIOManager:" << id;
            return false;
        }

        if (m_systemBus) {
            m_systemBus->mapDevice(device, device->getBaseAddress(), device->getSize());
        }

        if (m_irqController && device->canInterrupt()) {
            m_irqController->registerHandler(0, [device](int vec) { Q_UNUSED(vec); Q_UNUSED(device); });
        }

        devices.insert(id, device);
        emit deviceAdded(id);
        return true;
    }

    bool removeDevice(const QString& id) {
        QWriteLocker locker(&deviceLock);
        if (!devices.contains(id)) return false;
        BusInterface* dev = devices.take(id);
        if (m_mmioManager) m_mmioManager->unMapDevice(dev);
       // if (bus) bus->dumpMappings();
        delete dev;
        emit deviceRemoved(id);
        return true;
    }

    bool loadFromJson(const QJsonObject& config) {
        if (!config.contains("devices") || !config["devices"].isArray()) return false;
        QJsonArray arr = config["devices"].toArray();
        for (const auto& item : arr) {
            if (!item.isObject()) continue;
            QJsonObject obj = item.toObject();
            QString type = obj["type"].toString();
            QString id = obj["id"].toString();
            quint64 addr = obj["base"].toString().toULongLong(nullptr, 0);
            quint32 size = obj["size"].toInt();
            int irqVec = obj.contains("irq") ? obj["irq"].toInt() : allocateIRQ();

            if (irqVec < 0) {
                qWarning() << "DeviceManager: No available IRQ vectors for" << id;
                continue;
            }

            BusInterface* dev = nullptr;
            if (type == "UART") {
                dev = new UartDevice(m_irqController, irqVec);
            }
            else if (type == "SCSI") {
                dev = new ScsiBusController(m_irqController, irqVec);
            }
            else if (type == "NIC") {
                QString mac = obj["mac"].toString();
                dev = new TulipNIC_DC21040(m_irqController, irqVec, mac);
            }
            else {
                qWarning() << "Unknown device type in config:" << type;
                continue;
            }

            dev->setMemoryMapping(addr, size);
            addDevice(dev);
        }
        return true;
    }

    // ====================================================================================
    // DEVICE INSPECTION / ACCESS
    // ====================================================================================

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
        for (auto* device : devices) {
            summaries << QString("%1: %2 [%3-%4]")
                .arg(device->identifier())
                .arg(device->description())
                .arg(device->getBaseAddress(), 0, 16)
                .arg(device->getBaseAddress() + device->getSize() - 1, 0, 16);
        }
        return summaries;
    }

    QVector<BusInterface*> getDevices() const {
        QReadLocker locker(&deviceLock);
        return devices.values().toVector();
    }

   void resetAllDevices() {
        QReadLocker locker(&deviceLock);
        for (auto* device : devices) device->reset();
    }
	QString dumpSystemBus() const {
		if (!m_systemBus)
			return "SystemBus not attached.";

		//bus->dumpMappings();
		return "System bus dump complete.";
	}

signals:
    void deviceAdded(const QString& id);
    void deviceRemoved(const QString& id);

private:
    IRQController* m_irqController = nullptr;
    SystemBus* m_systemBus = nullptr;
    MMIOManager* m_mmioManager = nullptr;
    QHash<QString, BusInterface*> devices;
    mutable QReadWriteLock deviceLock;
    std::function<void(const QString&)> loggingCallback;

    int allocateIRQ() {
        static QSet<int> used;
        for (int i = 32; i < 255; ++i) {
            if (!used.contains(i)) {
                used.insert(i);
                return i;
            }
        }
        return -1;
    }
};

#endif // DEVICEMANAGER_H