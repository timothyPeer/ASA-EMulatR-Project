// ============================================================================
// ScsiBusController.cpp
// Implementation of extended SCSI Bus Controller with 256 device support.
// Simulates disk-backed block read/write and device query functions.
// Reference: ANSI SCSI-2 and DEC SRM I/O services.
// ============================================================================

#include "ScsiBusController.h"

ScsiBusController::ScsiBusController(QObject* parent) : QObject(parent) {
	operationTimer = new QTimer(this);
	operationTimer->setSingleShot(true);
	connect(operationTimer, &QTimer::timeout, this, &ScsiBusController::onOperationComplete);
	senseData.fill(0x00, 32); // Initialize sense buffer
}

ScsiBusController::ScsiBusController(bool enableIRQ, quint8 irqVec, QObject* parent)
	: QObject(parent), irqVector(irqVec), interruptEnabled(enableIRQ) {
	operationTimer = new QTimer(this);
	operationTimer->setSingleShot(true);
	connect(operationTimer, &QTimer::timeout, this, &ScsiBusController::onOperationComplete);
	senseData.fill(0x00, 32);
}

ScsiBusController::~ScsiBusController() {
	for (auto* file : attachedDisks.values()) {
		if (file) {
			if (file->isOpen()) file->close();
			delete file;
		}
	}
}

quint64 ScsiBusController::read(quint64 offset) {
	QMutexLocker locker(&mutex);
	switch (static_cast<Register>(offset)) {
	case Register::Status: return static_cast<quint8>(statusReg);
	case Register::Data: return dataReg;
	default: return 0;
	}
}

// Correct BusInterface override for size()
quint64 ScsiBusController::size() const {
	return 0x30; // 0x00..0x28 registers mapped, total 48 bytes
}


bool ScsiBusController::attachDiskImage(quint8 id, const QString& path, bool readOnly) {
	QMutexLocker locker(&mutex);
	if (attachedDisks.contains(id)) return false;

	QFile* file = new QFile(path);
	if (!file->open(readOnly ? QFile::ReadOnly : QFile::ReadWrite)) {
		qWarning() << "SCSI: Failed to open disk image for ID" << id << path;
		delete file;
		return false;
	}
	attachedDisks[id] = file;
	return true;
}

void ScsiBusController::detachDiskImage(quint8 id) {
	QMutexLocker locker(&mutex);
	if (attachedDisks.contains(id)) {
		QFile* file = attachedDisks[id];
		if (file) {
			if (file->isOpen()) file->close();
			delete file;
		}
		attachedDisks.remove(id);
	}
}

bool ScsiBusController::createDiskImage(const QString& path, int sizeInMB) {
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) return false;
	file.resize(static_cast<qint64>(sizeInMB) * 1024 * 1024);
	file.close();
	return true;
}

void ScsiBusController::executeCommand(Command cmd) {
	switch (cmd) {
	case Command::Identify: processIdentify(); break;
	case Command::ReadBlock: processReadBlock(); break;
	case Command::WriteBlock: processWriteBlock(); break;
	case Command::Reset: processReset(); break;
	case Command::Inquiry: cmdInquiry(); break;
	case Command::RequestSense: cmdRequestSense(); break;
	case Command::TestUnitReady: cmdTestReady(); break;
	case Command::FormatUnit: cmdFormatUnit(); break;
	default:
		setErrorStatus("Invalid SCSI Command");
		break;
	}
}

void ScsiBusController::onOperationComplete() {
	statusReg = Status::DataReady;
	triggerInterrupt();
}

void ScsiBusController::processIdentify() {
	dataReg = 0x53435349; // 'SCSI' ASCII
	statusReg = Status::DataReady;
	triggerInterrupt();
}

void ScsiBusController::processReadBlock() {
	if (!isValidDevice(currentDeviceId)) return setErrorStatus("Read: No such device");
	QFile* file = attachedDisks[currentDeviceId];
	quint64 offset = blockAddr * 512;
	if (!readFromFile(file, offset, dataReg)) return setErrorStatus("Read: Failed");

	operationTimer->start(1); // Simulate delay
}

void ScsiBusController::processWriteBlock() {
	if (!isValidDevice(currentDeviceId)) return setErrorStatus("Write: No such device");
	QFile* file = attachedDisks[currentDeviceId];
	quint64 offset = blockAddr * 512;
	if (!writeToFile(file, offset, dataReg)) return setErrorStatus("Write: Failed");

	operationTimer->start(1);
}

void ScsiBusController::processReset() {
	dataReg = 0;
	statusReg = Status::Idle;
	triggerInterrupt();
}

void ScsiBusController::cmdInquiry() {
	dataReg = 0x51444543; // 'CDEQ' - Vendor Code
	statusReg = Status::DataReady;
	triggerInterrupt();
}

void ScsiBusController::cmdRequestSense() {
	dataReg = 0x00000000; // Dummy sense code
	statusReg = Status::DataReady;
	triggerInterrupt();
}

void ScsiBusController::cmdTestReady() {
	statusReg = isValidDevice(currentDeviceId) ? Status::Idle : Status::Error;
	triggerInterrupt();
}

void ScsiBusController::cmdFormatUnit() {
	// Simulated NOP
	statusReg = Status::Idle;
	triggerInterrupt();
}

void ScsiBusController::setErrorStatus(const QString& reason) {
	qWarning() << "SCSI Error:" << reason;
	statusReg = Status::Error;
	triggerInterrupt();
}

bool ScsiBusController::writeToFile(QFile* file, quint64 offset, quint64 value) {
	if (!file || !file->isOpen()) return false;
	if (!file->seek(offset)) return false;
	QByteArray buf(reinterpret_cast<const char*>(&value), sizeof(value));
	return file->write(buf) == sizeof(value);
}

bool ScsiBusController::readFromFile(QFile* file, quint64 offset, quint64& result) {
	if (!file || !file->isOpen()) return false;
	if (!file->seek(offset)) return false;
	QByteArray buf = file->read(sizeof(quint64));
	if (buf.size() != sizeof(quint64)) return false;
	result = *reinterpret_cast<const quint64*>(buf.constData());
	return true;
}