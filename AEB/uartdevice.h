// UartDevice.h
#pragma once
#ifndef UartDevice_h__
#define UartDevice_h__

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QTimer>
#include "BaseDevice.h"
#include "mmiomanager.h"
#include "mmiohandler.h"
#include "Helpers.h"

struct Region {
	quint64 start;
	quint64 end;
	MmioHandler* handler;
};
/**
 * @brief Emulates a simple 16550A-compatible UART
 *
 * Provides serial communication capabilities with:
 * - Transmit and receive buffers
 * - Programmable baud rate
 * - Hardware flow control
 * - Interrupt generation
 */
class UartDevice : public BaseDevice,  public MmioHandler {
    Q_OBJECT

public:
    enum class Register : quint8 {
        RBR = 0x00,  // Receive Buffer Register (read-only)
        THR = 0x00,  // Transmit Holding Register (write-only)
        IER = 0x01,  // Interrupt Enable Register
        IIR = 0x02,  // Interrupt Identification Register (read-only)
        FCR = 0x02,  // FIFO Control Register (write-only)
        LCR = 0x03,  // Line Control Register
        MCR = 0x04,  // Modem Control Register
        LSR = 0x05,  // Line Status Register
        MSR = 0x06,  // Modem Status Register
        SCR = 0x07,  // Scratch Register
        DLL = 0x00,  // Divisor Latch LSB (when DLAB=1)
        DLM = 0x01   // Divisor Latch MSB (when DLAB=1)
    };

    /**
     * @brief Construct a new UART
     * @param irqController IRQ controller to use
     * @param irqVector Interrupt vector to use
     * @param parent Optional QObject parent
     */

	UartDevice(IRQController* irqController, int irqVector, QObject* parent = nullptr) : BaseDevice(irqController, irqVector, "uart", "16550A UART Controller", parent)
	{
		// Create the transmit timer
		txTimer = new QTimer(this);
		connect(txTimer, &QTimer::timeout, this, &UartDevice::onTransmitTimer);
		txTimer->setInterval(10); // 10ms for ~9600 baud simulation

		// Initialize device state
		reset();

		qDebug() << "UartDevice: Initialized with IRQ vector" << irqVector;
	}

    /**
     * @brief Destroy the UART
     */

	~UartDevice()
	{
		txTimer->stop();
		qDebug() << "UartDevice: Destroyed";
	}


	// MMIOHandler Interfaces
// 	// MMIO interface for 32-bit register access (default 8 registers)
// 	uint32_t mmioRead(uint64_t addr) override {
// 		return static_cast<uint32_t>(read(addr & 0x7, 1));
// 	}
// 
// 	void mmioWrite(uint64_t addr, uint32_t value) override {
// 		write(addr & 0x7, value & 0xFF, 1);
// 	}
///////////////////////////////////////////////////////////////////////
// 
    // BusInterface implementation

	quint64 read(quint64 address, int size) override;

	void write(quint64 address, quint64 data, int size) override;

	bool isDeviceAddress(quint64 address) const override
	{
		// UART registers are within the first 8 bytes
		return address < 8;
	}

	void reset() override;

    /**
     * @brief Send data to the UART (from external source)
     * @param data The data byte to send
     * @return True if data was accepted
     */

	bool sendData(quint8 data);

	quint64 size() const override {
		return mappedSize;
	}

    /**
     * @brief Get received data from the UART (to external sink)
     * @param data Output parameter to store received byte
     * @return True if data was available
     */

	bool receiveData(quint8& data);

    /**
     * @brief Check if the UART has data to receive
     * @return True if data is available
     */

	bool hasDataToReceive() const;

    /**
     * @brief Attach a console to this UART for I/O
     * @param console The console object
     */

	void attachConsole(QObject* console)
	{
		// This should be implemented to connect signals/slots with a console object
		qDebug() << "UartDevice: Console attached";
	}

	/// Read an 8-bit value from MMIO or return 0xFF if unmapped.
	uint8_t mmioReadUInt8(quint64 addr) override;

	/// Read a 16-bit value from MMIO or return 0xFFFF if unmapped.
	uint16_t mmioReadUInt16(quint64 addr) override;

	/// Read a 32-bit value from MMIO or return 0xFFFFFFFF if unmapped.
	uint32_t mmioReadUInt32(quint64 addr) override;

	/// Read a 64-bit value from MMIO or return all-ones if unmapped.
	quint64 mmioReadUInt64(quint64 addr) override;

    /// Write an 8-bit value to MMIO if mapped.
	void mmioWriteUInt8(quint64 addr, uint8_t val) override;

	/// Write a 16-bit value to MMIO if mapped.
	void mmioWriteUInt16(quint64 addr, uint16_t val) override;

	/// Write a 32-bit value to MMIO if mapped.
	void mmioWriteUInt32(quint64 addr, uint32_t val) override;

	/// Write a 64-bit value to MMIO if mapped.
	void mmioWriteUInt64(quint64 addr, quint64 val) override;

signals:
    /**
     * @brief Signal emitted when data is transmitted
     * @param data The transmitted byte
     */
    void dataTransmitted(quint8 data);

    /**
     * @brief Signal emitted when data is received
     * @param data The received byte
     */
    void dataReceived(quint8 data);

private:
    struct UartRegisters {
        quint8 rbr = 0;     // Receive Buffer Register
        quint8 thr = 0;     // Transmit Holding Register
        quint8 ier = 0;     // Interrupt Enable Register
        quint8 iir = 0;     // Interrupt Identification Register
        quint8 fcr = 0;     // FIFO Control Register
        quint8 lcr = 0;     // Line Control Register
        quint8 mcr = 0;     // Modem Control Register
        quint8 lsr = 0;     // Line Status Register
        quint8 msr = 0;     // Modem Status Register
        quint8 scr = 0;     // Scratch Register
        quint8 dll = 0;     // Divisor Latch LSB
        quint8 dlm = 0;     // Divisor Latch MSB
    };

    UartRegisters regs;
    QQueue<quint8> rxFifo;  // Receive FIFO
    QQueue<quint8> txFifo;  // Transmit FIFO
    QTimer* txTimer;        // Transmit timer
    mutable QMutex m_mutex;           // Protect state
	QVector<Region> m_regions;

	
	// Generic reader: locks, finds region, invokes correct handler or returns default

	template<typename T>
	T readGeneric(quint64 addr, T defaultVal) const
	{
		QMutexLocker locker(&m_mutex);
		for (const auto& r : m_regions) {
			if (addr >= r.start && addr <= r.end) {
				auto off = addr - r.start;
				if constexpr (std::is_same_v<T, uint8_t >) return r.handler->mmioReadUInt8(off);
				if constexpr (std::is_same_v<T, uint16_t>) return r.handler->mmioReadUInt16(off);
				if constexpr (std::is_same_v<T, uint32_t>) return r.handler->mmioReadUInt32(off);
				if constexpr (std::is_same_v<T, quint64>) return r.handler->mmioReadUInt64(off);
			}
		}
		return defaultVal;
	}

	// Generic writer: locks, finds region, invokes correct handler
	template<typename V>
	void writeGeneric(quint64 addr, V val) {
		QMutexLocker locker(&m_mutex);
		for (const auto& r : m_regions) {
			if (addr >= r.start && addr <= r.end) {
				auto off = addr - r.start;
				if constexpr (std::is_same_v<V, uint8_t >) r.handler->mmioWriteUInt8(off, val);
				if constexpr (std::is_same_v<V, uint16_t>) r.handler->mmioWriteUInt16(off, val);
				if constexpr (std::is_same_v<V, uint32_t>) r.handler->mmioWriteUInt32(off, val);
				if constexpr (std::is_same_v<V, quint64>) r.handler->mmioWriteUInt64(off, val);
				return;
			}
		}
	}
	
	/**
     * @brief Initialize registers with default values
     */

	void initializeRegisters() override
	{
		// Initialize 16550A registers to default values
		regs.rbr = 0;     // Receive Buffer Register
		regs.thr = 0;     // Transmit Holding Register
		regs.ier = 0;     // Interrupt Enable Register
		regs.iir = 0x01;  // Interrupt Identification Register (no interrupt pending)
		regs.fcr = 0;     // FIFO Control Register
		regs.lcr = 0;     // Line Control Register
		regs.mcr = 0;     // Modem Control Register
		regs.lsr = 0x60;  // Line Status Register (THR empty, TX Shift Register empty)
		regs.msr = 0;     // Modem Status Register
		regs.scr = 0;     // Scratch Register
		regs.dll = 0;     // Divisor Latch LSB
		regs.dlm = 0;     // Divisor Latch MSB

		// Map registers to register map
		registers[static_cast<quint64>(Register::RBR)] = regs.rbr;
		registers[static_cast<quint64>(Register::IER)] = regs.ier;
		registers[static_cast<quint64>(Register::IIR)] = regs.iir;
		registers[static_cast<quint64>(Register::LCR)] = regs.lcr;
		registers[static_cast<quint64>(Register::MCR)] = regs.mcr;
		registers[static_cast<quint64>(Register::LSR)] = regs.lsr;
		registers[static_cast<quint64>(Register::MSR)] = regs.msr;
		registers[static_cast<quint64>(Register::SCR)] = regs.scr;
	}

    /**
     * @brief Update UART state after register change
     */
    void updateState();

    /**
     * @brief Update interrupt status
     */

	void updateInterrupts()
	{
		// Determine the highest priority interrupt
		quint8 intPriority = 0;

		// Check for interrupts in priority order (see 16550A documentation)

		// 1. Line Status (highest priority)
		if ((regs.ier & 0x04) && (regs.lsr & 0x1E)) { // LSR bit 1-4 errors
			intPriority = 3;
		}
		// 2. Receive Data Available
		else if ((regs.ier & 0x01) && (regs.lsr & 0x01)) {
			intPriority = 2;
		}
		// 3. Transmit Holding Register Empty
		else if ((regs.ier & 0x02) && (regs.lsr & 0x20)) {
			intPriority = 1;
		}
		// 4. Modem Status (lowest priority)
		else if ((regs.ier & 0x08) && (regs.msr & 0x0F)) {
			intPriority = 0;
		}

		// Update the Interrupt Identification Register
		if (intPriority > 0) {
			// Interrupt pending
			regs.iir = (intPriority << 1);

			// Trigger the interrupt
			triggerInterrupt();
		}
		else {
			// No interrupt pending
			regs.iir = 0x01;

			// Clear the interrupt
			clearInterrupt();
		}

		// Update the IIR in the register map
		registers[static_cast<quint64>(Register::IIR)] = regs.iir;
	}

    /**
     * @brief Process transmit data
     */
    void processTransmit();

private slots:
    /**
     * @brief Handle transmit timer timeout
     */

	void onTransmitTimer();

};

#endif // UartDevice_h__