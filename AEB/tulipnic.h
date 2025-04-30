/*
 the Tulip NIC (Digital 21x4x series Ethernet controller), commonly used in Alpha-based systems. 
 The emulation will focus on PCI configuration, memory-mapped I/O handling, transmit/receive descriptor ring support, 
 and interrupt signaling — consistent with the DEC 21140A specification, which is the typical "Tulip" reference.
*/
#pragma once
#ifndef TULIPNIC_H
#define TULIPNIC_H



/**
 * @brief Emulates a DEC Tulip 21x4x series NIC (21140A)
 *
 * This device supports PCI and MMIO-based network emulation,
 * including descriptor-based Tx/Rx ring buffers, CSR control/status
 * registers, and basic interrupt management.
 *
 * Reference: DEC 21140A Tulip Ethernet Controller Datasheet
 */
 /**
  * @brief Emulates a DEC 21140A (Tulip) Ethernet NIC for Alpha AXP systems
  *
  * Supports PCI MMIO, Tx/Rx descriptor polling, and interrupt signaling.
  * SRM-compatible via emulation of CSR registers and TAP/UDP extension.
  *
  * Reference: DEC 21140A Hardware Reference Manual, Digital Equipment Corp.
  */

#include <QObject>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <array>
#include <QDebug>
#include "DeviceInterface.h"
#include "mmiomanager.h"
#include "BusInterface.h"
#include "mmiohandler.h"

  //
  // TulipNIC - Emulated DEC 21140 Tulip Ethernet Controller
  // Provides MMIO-based register interface and metadata
  //

/**
 * @brief Emulates a DEC Tulip 21x4x series NIC (21140A)
 *
 * Provides MMIO access to CSR registers, basic interrupt logic,
 * and transmit/receive descriptor emulation for Alpha AXP systems.
 *
 * Reference: DEC 21140A Hardware Reference Manual
 */

 /**
  * @brief Emulates a DEC Tulip 21x4x series NIC (21140A)
  *
  * Provides MMIO access to CSR registers, basic interrupt logic,
  * and transmit/receive descriptor emulation for Alpha AXP systems.
  *
  * Reference: DEC 21140A Hardware Reference Manual
  */
class TulipNIC : public QObject, public DeviceInterface, public BusInterface, public MmioHandler {
    Q_OBJECT

public:
    explicit TulipNIC(int index = 0, QObject* parent = nullptr)
        : QObject(parent), deviceIndex(index)
    {
        csr.fill(0);
        mappedSize = 256; // 16 CSRs x 4 bytes
        macAddress = QByteArray::fromHex("08002BDEADBE");
        qDebug() << "[TulipNIC] Created instance" << identifier();
    }

	explicit TulipNIC(IRQController* irq, int irqVector, const QString& mac, QObject* parent = nullptr)
		: QObject(parent), irqLine(irqVector)
	{
		Q_UNUSED(irq); // Hook later if IRQController integration is needed

		// Default address setup — these should be explicitly configured via setMemoryMapping()
		baseAddress = 0;
		mappedSize = 256;

		if (mac.size() == 6)
			macAddress = mac.toLocal8Bit();
		else
			macAddress = QByteArray::fromHex("08002BDEADBE"); // fallback

		csr.fill(0);  // Clear CSR registers

		qDebug() << "[TulipNIC] Created with IRQ vector" << irqVector
			<< "and MAC:" << macAddress.toHex(':');
	}


    QString identifier() const override { return QString("tulip%1").arg(deviceIndex); }
    QString description() const override { return "DEC 21140A Tulip Ethernet Controller"; }

	quint64 read(quint64 offset, int size) override {
		QMutexLocker lock(&mutex);
		if (offset + size > (csr.size() * sizeof(quint32))) return 0xFFFFFFFFFFFFFFFF;
		quint32 index = static_cast<quint32>(offset >> 2);
		return csr[index];
	}

	void write(quint64 offset, quint64 value, int size) override {
		QMutexLocker lock(&mutex);
		if (offset + size > (csr.size() * sizeof(quint32))) return;
		quint32 index = static_cast<quint32>(offset >> 2);
		csr[index] = static_cast<quint32>(value);
	}
    uint32_t mmioRead(uint64_t addr) override {
        QMutexLocker lock(&mutex);
        quint32 index = static_cast<quint32>((addr - baseAddress) >> 2);
        if (index >= csr.size()) return 0xFFFFFFFF;
        return csr[index];
    }

    void mmioWrite(uint64_t addr, uint32_t value) override {
        QMutexLocker lock(&mutex);
        quint32 index = static_cast<quint32>((addr - baseAddress) >> 2);
        if (index >= csr.size()) return;
        csr[index] = value;
        handleCSRWrite(index, value);
    }

	quint64 read(quint64 offset) override {
		return read(offset, 4); // Assume 4-byte access default
	}

	void write(quint64 offset, quint64 value) override {
		write(offset, value, 4); // Assume 4-byte access default
	}
//     void write(quint64 offset, quint64 value, int size) override {
//         QMutexLocker lock(&mutex);
//         if (offset + size > (csr.size() * sizeof(quint32))) return;
//         quint32 index = static_cast<quint32>(offset >> 2);
//         csr[index] = static_cast<quint32>(value);
//     }

    bool isDeviceAddress(quint64 addr) const override {
        return (addr >= baseAddress && addr < (baseAddress + mappedSize));
    }

    void reset() override {
        QMutexLocker lock(&mutex);
        csr.fill(0);
    }

    quint64 getSize() const override { return mappedSize; }  // ✅ matches BusInterface
    quint64 getBaseAddress() const override { return baseAddress; }  // ✅ matches BusInterface
	QByteArray getMacAddress() const { return macAddress; }

	void setMemoryMapping(quint64 base, quint64 size) override {
		baseAddress = base;
		mappedSize = size;
	}

   
    void setIRQ(int irq) { this->irqLine = irq; }
    int getIRQ() const { return irqLine; }

    void setMacAddress(const QByteArray& mac) {
        if (mac.size() == 6) macAddress = mac;
    }

	quint64 size() const override {
		return mappedSize;
	}

private:
    QMutex mutex;
    quint64 baseAddress = 0;
    quint64 mappedSize = 0;

    std::array<quint32, 16> csr{}; // 16 CSR registers
    int deviceIndex = 0;
    int irqLine = -1;
    QByteArray macAddress;

    void handleCSRWrite(quint32 index, quint32 value) {
        qDebug() << "[TulipNIC] handleCSRWrite index=" << index << " value=" << value;
    }
};

#endif // TULIPNIC_H
