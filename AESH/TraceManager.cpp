#include "TraceManager.h"

void TraceWorker::process()
{
	while (true) {
		QList<QString> batch;

		m_lock->lock();
		if (m_queue->isEmpty()) {
			m_waitCondition->wait(m_lock); // Wait efficiently until notified
		}
		while (!m_queue->isEmpty() && batch.size() < 100) { // Batch 100 messages
			batch.append(m_queue->dequeue());
		}
		m_lock->unlock();

		for (const QString& message : batch) {
			m_outputStream << message << Qt::endl;
		}
		m_outputStream.flush();
	}
}
