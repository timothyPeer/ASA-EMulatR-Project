#pragma once
#ifndef MMIOManager_h__
#define MMIOManager_h__

#include <QObject>
#include <QMap>
#include <QVector>
#include <QReadWriteLock>
#include <QJsonArray>
#include <QString>
#include <functional>
#include "BusInterface.h"
#include "IRQController.h"

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
class MMIOManager : public QObject {
    Q_OBJECT

public:
    explicit MMIOManager(IRQController* irqCtrl, QObject* parent = nullptr)
        : QObject(parent), irqController(irqCtrl) {
        qDebug() << "MMIOManager: Initialized";
    }
    ~MMIOManager() {}

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

    bool remapDevice(BusInterface* device, quint64 newBase, quint64 newSize) {
        if (!unMapDevice(device)) return false;
        return mapDevice(device, newBase, newSize);
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

    quint64 readMMIO(quint64 address, int size) {
        QReadLocker locker(&lock);
        const DeviceMapping* m = findMapping(address);
        if (!m) return 0;
        quint64 val = m->device->read(m->getRelativeAddress(address), size);
        emit deviceAccessed(m->device, m->getRelativeAddress(address), false, size, val);
        return val;
    }

    bool writeMMIO(quint64 address, quint64 data, int size) {
        QReadLocker locker(&lock);
        const DeviceMapping* m = findMapping(address);
        if (!m) return false;
        m->device->write(m->getRelativeAddress(address), data, size);
        emit deviceAccessed(m->device, m->getRelativeAddress(address), true, size, data);
        return true;
    }

    bool isMMIOAddress(quint64 address) const {
        QReadLocker locker(&lock);
        return findMapping(address) != nullptr;
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

    void setIRQController(IRQController* controller) {
        irqController = controller;
    }

    IRQController* getIRQController() const {
        return irqController;
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

signals:
    void deviceAccessed(BusInterface* device, quint64 address, bool isWrite, int size, quint64 data);
    void deviceRegistered(BusInterface* device, quint64 baseAddress, quint64 size);
    void deviceUnregistered(BusInterface* device);

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

    QVector<DeviceMapping> deviceMappings;
    IRQController* irqController = nullptr;
    mutable QReadWriteLock lock;
    std::function<void(const QString&)> loggingCallback;
};

#endif // MMIOManager_h__
