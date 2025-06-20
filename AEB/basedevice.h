#pragma once
#ifndef BASEDEVICE_H
#define BASEDEVICE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QReadWriteLock>
#include <QVariantMap>
#include <QDebug>
#include "BusInterface.h"
#include "IRQController.h"

/**
 * @brief Abstract base class for all memory-mapped devices in the Alpha AXP emulator.
 *
 * Provides core register access, interrupt triggering, and BusInterface integration.
 * All MMIO-capable devices should inherit from this class.
 *
 * Reference: Alpha System Architecture Manual, Bus and Interrupt Handling Sections
 */
class BaseDevice : public QObject, public BusInterface {
    Q_OBJECT

public:
    BaseDevice(IRQController* irqCtrl,
        int irqVec,
        const QString& id,
        const QString& desc,
        QObject* parent = nullptr)
        : QObject(parent),
        irqController(irqCtrl),
        irqVector(irqVec),
        deviceId(id),
        deviceDescription(desc)
    {
        reset();
    }

    virtual ~BaseDevice() override {
        qDebug() << "[BaseDevice] Destroyed:" << deviceId;
    }

    // --- BusInterface Implementation ---

    QString identifier() const override { return deviceId; }
    QString description() const override { return deviceDescription; }

    bool canInterrupt() const override { return irqVector >= 0 && irqController; }
    quint8 interruptVector()  override { return static_cast<quint8>(irqVector); }

    void connectIRQController(IRQController* irq) override {
        irqController = irq;
    }

    quint64 read(quint64 offset) override {
        QReadLocker locker(&deviceLock);
        return read(offset, 8); // Default: quadword
    }

    bool write(quint64 offset, quint64 value) override {
        QWriteLocker locker(&deviceLock);
        return write(offset, value, 8); // Default: quadword
   
    }

    bool isDeviceAddress(quint64 address) override {
        return (address >= baseAddress && address < (baseAddress + mappedSize));
    }

    quint64 getBaseAddress()  override { return baseAddress; }
    quint64 getSize() const override { return mappedSize; }

    // --- Device Interface Extensions ---

    void setMemoryMapping(quint64 base, quint64 size) {
        baseAddress = base;
        mappedSize = size;
    }

    virtual quint64 size() const { return mappedSize; }

    virtual QVariantMap metadata() const {
        return {
            {"identifier", deviceId},
            {"description", deviceDescription},
            {"baseAddress", baseAddress},
            {"size", mappedSize},
            {"irqVector", irqVector}
        };
    }

    virtual void reset() override {
        QWriteLocker locker(&deviceLock);
        registers.clear();
        initializeRegisters();
        qDebug() << "[BaseDevice] Reset:" << deviceId;
    }

    virtual quint64 read(quint64 address, int size) {
        quint64 aligned = alignAddress(address, size);
        quint64 value = registers.value(aligned, 0);

        switch (size) {
        case 1: value &= 0xFF; break;
        case 2: value &= 0xFFFF; break;
        case 4: value &= 0xFFFFFFFF; break;
        case 8: break;
        default:
            qWarning() << "[BaseDevice] Invalid read size:" << size;
            return 0;
        }

        emit deviceAccessed(false, address, value, size);
        return value;
    }

    virtual bool write(quint64 address, quint64 data, int size) {
        quint64 aligned = alignAddress(address, size);

        switch (size) {
        case 1: data &= 0xFF; break;
        case 2: data &= 0xFFFF; break;
        case 4: data &= 0xFFFFFFFF; break;
        case 8: break;
        default:
            qWarning() << "[BaseDevice] Invalid write size:" << size;
            return false;
        }

        registers[aligned] = data;
        emit deviceAccessed(true, address, data, size);
        return true;
    }

signals:
    void deviceAccessed(bool isWrite, quint64 address, quint64 data, int size);
    void interruptTriggered(int vector);
    void interruptCleared(int vector);

protected:
    IRQController* irqController = nullptr;
    int irqVector = -1;
    QString deviceId;
    QString deviceDescription;
    quint64 baseAddress = 0;
    quint64 mappedSize = 0;
    QMap<quint64, quint64> registers;
    mutable QReadWriteLock deviceLock;

    virtual void initializeRegisters() = 0;

    void triggerInterrupt() {
        if (canInterrupt()) {
            irqController->signalIRQ(0, irqVector); // CPU 0
            emit interruptTriggered(irqVector);
        }
    }

    void clearInterrupt() {
        if (canInterrupt()) {
            irqController->clearIRQ(0, irqVector);
            emit interruptCleared(irqVector);
        }
    }

    static quint64 alignAddress(quint64 addr, int size) {
        switch (size) {
        case 1: return addr;
        case 2: return addr & ~0x1ULL;
        case 4: return addr & ~0x3ULL;
        case 8: return addr & ~0x7ULL;
        default: return addr;
        }
    }
};

#endif // BASEDEVICE_H
