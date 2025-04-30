#include "uartdevice.h"



quint64 UartDevice::read(quint64 address, int size)
{
	QMutexLocker locker(&m_mutex);

	if (size != 1) {
		qWarning() << "UartDevice: Non-byte access attempted:" << size;
		// Handle as byte access anyway
	}

	// Get the register being accessed
	Register reg = static_cast<Register>(address & 0x7);

	quint8 value = 0;

	// Check if DLAB is set (Divisor Latch Access Bit)
	bool dlab = (regs.lcr & 0x80) != 0;

	// Handle register read based on DLAB state
	if (dlab) {
		switch (reg) {
		case Register::DLL:
			value = regs.dll;
			break;
		case Register::DLM:
			value = regs.dlm;
			break;
		default:
			// Other registers are accessed normally even with DLAB set
			break;
		}
	}

	// Process normal register access
	switch (reg) {
	case Register::RBR:
		if (!dlab) {
			// Read from receive buffer
			if (!rxFifo.isEmpty()) {
				value = rxFifo.dequeue();

				// Update Line Status Register
				if (rxFifo.isEmpty()) {
					regs.lsr &= ~0x01; // Clear Data Ready bit
				}

				// Update interrupt status
				updateInterrupts();
			}
			else {
				value = 0;
			}
		}
		break;

	case Register::IER:
		if (!dlab) {
			value = regs.ier;
		}
		break;

	case Register::IIR:
		value = regs.iir;
		break;

	case Register::LCR:
		value = regs.lcr;
		break;

	case Register::MCR:
		value = regs.mcr;
		break;

	case Register::LSR:
		value = regs.lsr;
		break;

	case Register::MSR:
		value = regs.msr;
		break;

	case Register::SCR:
		value = regs.scr;
		break;

	default:
		value = 0;
		qWarning() << "UartDevice: Read from unknown register:" << static_cast<int>(reg);
		break;
	}

	qDebug() << "UartDevice: Read from register" << static_cast<int>(reg)
		<< "=" << QString("0x%1").arg(value, 2, 16, QChar('0'));

	// Update the register map
	registers[static_cast<quint64>(reg)] = value;

	emit deviceAccessed(false, address, value, size);
	return value;
}

void UartDevice::write(quint64 address, quint64 data, int size)
{
	QMutexLocker locker(&m_mutex);

	if (size != 1) {
		qWarning() << "UartDevice: Non-byte access attempted:" << size;
		// Handle as byte access anyway
	}

	// Get the register being accessed
	Register reg = static_cast<Register>(address & 0x7);
	quint8 value = static_cast<quint8>(data & 0xFF);

	// Check if DLAB is set (Divisor Latch Access Bit)
	bool dlab = (regs.lcr & 0x80) != 0;

	// Handle register write based on DLAB state
	if (dlab) {
		switch (reg) {
		case Register::DLL:
			regs.dll = value;
			registers[static_cast<quint64>(reg)] = value;
			return;

		case Register::DLM:
			regs.dlm = value;
			registers[static_cast<quint64>(reg)] = value;
			return;

		default:
			// Other registers are accessed normally even with DLAB set
			break;
		}
	}

	// Process normal register access
	switch (reg) {
	case Register::THR:
		if (!dlab) {
			// Write to transmit holding register
			regs.thr = value;

			// Add to transmit FIFO
			txFifo.enqueue(value);

			// Update Line Status Register
			regs.lsr &= ~0x20; // Clear THR Empty bit

			// Start the transmit timer if not already running
			if (!txTimer->isActive()) {
				txTimer->start();
			}

			// Emit the character for console integration
			emit dataTransmitted(value);
		}
		break;

	case Register::IER:
		if (!dlab) {
			regs.ier = value & 0x0F; // Only lower 4 bits are used
			updateInterrupts();
		}
		break;

	case Register::FCR:
		regs.fcr = value;

		// Handle FIFO control operations
		if (value & 0x01) {
			// FIFO enable
			if (value & 0x02) {
				// Clear receive FIFO
				rxFifo.clear();
				regs.lsr &= ~0x01; // Clear Data Ready bit
			}

			if (value & 0x04) {
				// Clear transmit FIFO
				txFifo.clear();
				regs.lsr |= 0x20; // Set THR Empty bit
			}
		}

		updateInterrupts();
		break;

	case Register::LCR:
		regs.lcr = value;
		break;

	case Register::MCR:
		regs.mcr = value;
		updateInterrupts();
		break;

	case Register::SCR:
		regs.scr = value;
		break;

	default:
		qWarning() << "UartDevice: Write to unknown register:" << static_cast<int>(reg);
		break;
	}

	qDebug() << "UartDevice: Write to register" << static_cast<int>(reg)
		<< "=" << QString("0x%1").arg(value, 2, 16, QChar('0'));

	// Update the register map
	registers[static_cast<quint64>(reg)] = value;

	// Update interrupt status
	updateInterrupts();

	emit deviceAccessed(true, address, data, size);
}

void UartDevice::reset()
{
	QMutexLocker locker(&m_mutex);

	// Call base class reset
	BaseDevice::reset();

	// Clear FIFOs
	rxFifo.clear();
	txFifo.clear();

	// Stop timer
	txTimer->stop();

	qDebug() << "UartDevice: Reset complete";
}

bool UartDevice::sendData(quint8 data)
{
	QMutexLocker locker(&m_mutex);

	// Check if receive FIFO is full (assume 16-byte FIFO)
	if (rxFifo.size() >= 16) {
		qWarning() << "UartDevice: Receive FIFO full, data lost:" << data;
		return false;
	}

	// Add data to receive FIFO
	rxFifo.enqueue(data);

	// Update Line Status Register
	regs.lsr |= 0x01; // Set Data Ready bit

	// Update interrupt status
	updateInterrupts();

	// Emit signal for received data
	emit dataReceived(data);

	return true;
}

bool UartDevice::receiveData(quint8& data)
{
	QMutexLocker locker(&m_mutex);

	if (rxFifo.isEmpty()) {
		return false;
	}

	data = rxFifo.dequeue();

	// Update Line Status Register
	if (rxFifo.isEmpty()) {
		regs.lsr &= ~0x01; // Clear Data Ready bit
	}

	// Update interrupt status
	updateInterrupts();

	return true;
}

bool UartDevice::hasDataToReceive() const
{
	QMutexLocker locker(&m_mutex);
	return !rxFifo.isEmpty();
}

uint8_t UartDevice::mmioReadIUnt8(quint64 addr)
{
	return readGeneric<uint8_t >(addr, 0xFFu);
}

uint16_t UartDevice::mmioReadIUnt16(quint64 addr)
{
	return readGeneric<uint16_t>(addr, 0xFFFFu);
}

uint32_t UartDevice::mmioReadIUnt32(quint64 addr)
{
	return readGeneric<uint32_t>(addr, 0xFFFFFFFFu);
}

quint64 UartDevice::mmioReadIUnt64(quint64 addr)
{
	return readGeneric<quint64 >(addr, 0xFFFFFFFFFFFFFFFFULL);
}

void UartDevice::mmioWriteIUnt8(quint64 addr, uint8_t val)
{
	writeGeneric(addr, val);
}

void UartDevice::mmioWriteIUnt16(quint64 addr, uint16_t val)
{
	writeGeneric(addr, val);
}

void UartDevice::mmioWriteIUnt32(quint64 addr, uint32_t val)
{
	writeGeneric(addr, val);
}

void UartDevice::mmioWriteIUnt64(quint64 addr, quint64 val)
{
	writeGeneric(addr, val);
}

void UartDevice::onTransmitTimer()
{
	QMutexLocker locker(&m_mutex);

	// Process a byte from the transmit FIFO
	if (!txFifo.isEmpty()) {
		// Dequeue a byte - in a real device this would be shifted out
		quint8 txByte = txFifo.dequeue();

		// For simulation purposes, we can log the byte
		qDebug() << "UartDevice: Transmitted byte:"
			<< QString("0x%1").arg(txByte, 2, 16, QChar('0'))
			<< "(" << QChar(txByte) << ")";
	}

	// If FIFO is now empty, update status
	if (txFifo.isEmpty()) {
		// Set THR Empty bit
		regs.lsr |= 0x20;

		// Set Transmitter Empty bit
		regs.lsr |= 0x40;

		// Stop the timer
		txTimer->stop();

		// Update the LSR in the register map
		registers[static_cast<quint64>(Register::LSR)] = regs.lsr;

		// Update interrupt status
		updateInterrupts();
	}
}
