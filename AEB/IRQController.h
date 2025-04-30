#pragma once
#ifndef IRQController_h__
#define IRQController_h__

#include <QObject>
#include <QVector>
#include <QBitArray>
#include <QDebug>
#include <QMutex>
#include <functional>

/**
 * @brief Controller for handling hardware interrupts
 *
 * The IRQController manages interrupt requests (IRQs) from devices
 * and delivers them to the appropriate CPU. It tracks which IRQs
 * are pending and supports masking through CPU interrupt priority levels.
 *
 * Reference: Alpha System Architecture (1994), section 4.6.2 - Interrupt Handling
 */
class IRQController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new IRQController
     * @param parent Optional QObject parent
     */
	explicit IRQController(QObject* parent = nullptr) : QObject(parent) {
			qDebug() << "IRQController: Created";
	}

    /**
     * @brief Initialize the controller for specified number of CPUs
     * @param cpuCount Number of CPUs to handle
     */

	void initialize(int cpuCount)
	{
		QMutexLocker locker(&mutex);

		irqLines.resize(cpuCount);
		callbacks.resize(cpuCount);

		// Initialize IRQ lines for each CPU (256 possible vectors)
		for (int i = 0; i < cpuCount; ++i) {
			irqLines[i].resize(256);
			irqLines[i].fill(false);
		}

		qDebug() << "IRQController: Initialized for" << cpuCount << "CPUs";
	}

    /**
     * @brief Register an interrupt handler for a CPU
     * @param cpuIndex CPU ID (0-based)
     * @param handler Callback function to handle interrupts
     */

	void registerHandler(int cpuIndex, std::function<void(int)> handler)
	{
		QMutexLocker locker(&mutex);

		if (cpuIndex < callbacks.size()) {
			callbacks[cpuIndex] = handler;
			qDebug() << "IRQController: Registered handler for CPU" << cpuIndex;
		}
		else {
			qWarning() << "IRQController: Invalid CPU index" << cpuIndex;
		}
	}

    /**
     * @brief Signal an interrupt to a specific CPU
     * @param cpuIndex CPU ID to signal
     * @param irqVector Interrupt vector number
     */

	void signalIRQ(int cpuIndex, int irqVector)
	{
		QMutexLocker locker(&mutex);

		if (cpuIndex >= irqLines.size()) {
			qWarning() << "IRQController: Invalid CPU index" << cpuIndex;
			return;
		}

		if (irqVector < 0 || irqVector >= irqLines[cpuIndex].size()) {
			qWarning() << "IRQController: Invalid IRQ vector" << irqVector;
			return;
		}

		// Set the corresponding bit in the IRQ lines
		irqLines[cpuIndex].setBit(irqVector, true);

		qDebug() << "IRQController: IRQ" << irqVector << "signaled to CPU" << cpuIndex;

		// Invoke the registered callback if available
		if (callbacks[cpuIndex]) {
			// Release the lock before calling the callback to avoid deadlock
			locker.unlock();
			callbacks[cpuIndex](irqVector);
			emit irqDelivered(cpuIndex, irqVector);
		}
	}

    /**
     * @brief Clear a pending interrupt
     * @param cpuIndex CPU ID to clear IRQ for
     * @param irqVector Interrupt vector to clear
     */

	void clearIRQ(int cpuIndex, int irqVector)
	{
		QMutexLocker locker(&mutex);

		if (cpuIndex < irqLines.size() && irqVector < irqLines[cpuIndex].size()) {
			irqLines[cpuIndex].setBit(irqVector, false);
			qDebug() << "IRQController: IRQ" << irqVector << "cleared for CPU" << cpuIndex;
			emit irqCleared(cpuIndex, irqVector);
		}
	}

    /**
     * @brief Check if an interrupt is pending
     * @param cpuIndex CPU ID to check
     * @param irqVector Interrupt vector to check
     * @return True if IRQ is pending
     */

	bool isIRQPending(int cpuIndex, int irqVector) const
	{
		QMutexLocker locker(&mutex);

		if (cpuIndex >= irqLines.size() || irqVector >= irqLines[cpuIndex].size()) {
			return false;
		}

		return irqLines[cpuIndex].testBit(irqVector);
	}

    /**
     * @brief Get bitmap of pending interrupts for a CPU
     * @param cpuIndex CPU ID to check
     * @return BitArray with set bits for pending IRQs
     */

	QBitArray getPendingIRQs(int cpuIndex) const
	{
		QMutexLocker locker(&mutex);

		if (cpuIndex >= irqLines.size()) {
			qWarning() << "IRQController: Invalid CPU index" << cpuIndex;
			return QBitArray();
		}

		return irqLines[cpuIndex];
	}

    /**
     * @brief Reset the controller, clearing all pending IRQs
     */

	void reset()
	{
		QMutexLocker locker(&mutex);

		for (auto& irqLine : irqLines) {
			irqLine.fill(false);
		}

		qDebug() << "IRQController: Reset all IRQ lines";
	}

signals:
    /**
     * @brief Signal emitted when an IRQ is delivered to a CPU
     * @param cpuIndex CPU that received the IRQ
     * @param irqVector Vector that was delivered
     */
    void irqDelivered(int cpuIndex, int irqVector);

    /**
     * @brief Signal emitted when an IRQ is cleared
     * @param cpuIndex CPU the IRQ was cleared for
     * @param irqVector Vector that was cleared
     */
    void irqCleared(int cpuIndex, int irqVector);

private:
    QVector<QBitArray> irqLines;                      // IRQ bitmap per CPU
    QVector<std::function<void(int)>> callbacks;      // CPU interrupt handlers
    mutable QMutex mutex;                             // For thread safety
};

#endif // IRQController_h__
