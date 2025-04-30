#pragma once
#ifndef SCSIBUSCONTROLLER_H
#define SCSIBUSCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QFile>
#include <QMutex>
#include <QTimer>
#include <QDebug>
#include "..\AEB\BusInterface.h"

// ============================================================================
// ScsiBusController.H
// -----------------------------------------------------------------------------
// Simulates a SCSI bus controller with up to 256 devices (extended from 8).
// Handles block-level operations (Read, Write, Identify, Reset, etc.)
// Emulates SCSI disk behavior using QFile-backed disk images.
//
// Reference: ANSI SCSI-2 Command Set, DEC Alpha SRM Console Services
// ============================================================================

class ScsiBusController : public QObject, public BusInterface {
	Q_OBJECT

public:
	enum class Register : quint64 {
		Status = 0x00, // Read-only status register
		Command = 0x08, // Write-only command register
		Data = 0x10, // Bidirectional data register
		Block = 0x18, // Block address
		DeviceId = 0x20, // Target device ID (0-255)
		InterruptEnable = 0x28 // Enable IRQ
	};

	enum class Command : quint8 {
		TestUnitReady = 0x00,    // SCSI opcode for TEST UNIT READY
		Inquiry = 0x12,          // SCSI opcode for INQUIRY
		RequestSense = 0x03,     // SCSI opcode for REQUEST SENSE
		FormatUnit = 0x04,       // SCSI opcode for FORMAT UNIT
		ReadBlock = 0x28,        // Aligned with SCSI READ(10) opcode
		WriteBlock = 0x2A,       // Aligned with SCSI WRITE(10) opcode
		Identify = 0xDE,         // Emulator-specific IDENTIFY command
		Reset = 0xFF             // Emulator-only RESET command
	};

	enum class Status : quint8 {
		Idle = 0x00,
		Busy = 0x01,
		DataReady = 0x02,
		Error = 0xFF
	};

	explicit ScsiBusController(QObject* parent = nullptr);
	~ScsiBusController();

	// Register accessors
	quint64 read(quint64 offset);
	void write(quint64 offset, quint64 value);

	// Disk management
	bool attachDiskImage(quint8 deviceId, const QString& path, bool readOnly = false);
	void detachDiskImage(quint8 deviceId);
	bool createDiskImage(const QString& path, int sizeInMB);

	void setIRQVector(quint8 vector) { irqVector = vector; }
	bool canInterrupt() const { return interruptEnabled; }

signals:
	void irqRaised(quint8 vector);

private slots:
	void onOperationComplete();

private:
	QMap<quint8, QFile*> attachedDisks;  // Extended: support up to 256 SCSI devices
	QMutex mutex;

	quint8 irqVector = 0;
	bool interruptEnabled = false;
	quint64 dataReg = 0;
	quint64 blockAddr = 0;
	quint8 currentDeviceId = 0;
	Status statusReg = Status::Idle;
	QTimer* operationTimer = nullptr;
	QByteArray senseData;

	// Command execution logic
	void executeCommand(Command cmd);

	void triggerInterrupt()
	{
		if (interruptEnabled) emit irqRaised(irqVector);
	}

	// Command Handlers
	void processReadBlock();
	void processWriteBlock();
	void processIdentify();
	void processReset();
	void cmdInquiry();
	void cmdRequestSense();
	void cmdTestReady();
	void cmdFormatUnit();

	// Helpers
	void setErrorStatus(const QString& reason);
	bool writeToFile(QFile* file, quint64 offset, quint64 value);
	bool readFromFile(QFile* file, quint64 offset, quint64& result);
	bool isValidDevice(quint8 id) const { return attachedDisks.contains(id); }
};

#endif // SCSIBUSCONTROLLER_H
