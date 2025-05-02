#pragma once


// ============================================================================
// MMIODeviceExamples.H
// -----------------------------------------------------------------------------
// ScsiBusController and TulipNIC device classes updated for MMIO-compatible
// read/write dispatch using BusInterface's extended method signatures.
// Includes unit test simulation for size-specific access.
// ============================================================================


#include <QObject>
#include "ScsiBusController.h"
#include "TulipNIC_DC21040.h"
#include <QtTest/QtTest>

class TestMMIODevices : public QObject {
	Q_OBJECT

public:
	explicit TestMMIODevices(QObject* parent = nullptr) : QObject(parent)
	{ }
	~TestMMIODevices() {};

private slots:
	void testScsiBasicRW()  {
		ScsiBusController scsi;
		scsi.setMemoryMapping(0x1000, 0x100);
		scsi.write(0x10, 0x123456789ABCDEF0);
		QCOMPARE(scsi.read(0x10), 0x123456789ABCDEF0);
	}

	void testTulipByteRW() {
		TulipNIC_DC21040 nic;
		nic.setMemoryMapping(0x2000, 256);
		nic.write(0x04, 0xAABBCCDD, 4);
		QCOMPARE(nic.read(0x04, 4), quint64(0xAABBCCDD));
	}

	void testOutOfBoundsAccess() {
		TulipNIC_DC21040 nic;
		nic.setMemoryMapping(0x3000, 256);
		nic.write(0xFF, 0x1122334455667788, 8); // this should not write full value
		QVERIFY(nic.read(0xFF, 8) != 0x1122334455667788);
	}
};

QTEST_MAIN(TestMMIODevices)