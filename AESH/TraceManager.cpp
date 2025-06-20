#include "TraceManager.h"
#include <QMutexLocker>
#include <QDateTime>
#include <QIODevice>
#include <QDebug>
#include "GlobalMacro.h"



TraceManager::TraceManager()
	: currentLevel(0), logStream(&logFile), flushTimer(nullptr), exitRequested(false) {
	startWorker();
}

TraceManager& TraceManager::instance()
{
	static TraceManager* instance = new TraceManager();
	return *instance;
}


TraceManager::~TraceManager() {
	{
		QMutexLocker locker(&queueMutex);
		exitRequested = true;
		queueNotEmpty.wakeAll();
	}
	workerThread.quit();
	workerThread.wait();
}

void TraceManager::trace(const QString& msg) { enqueueLog("TRACE", msg); }
void TraceManager::debug(const QString& msg) { enqueueLog("DEBUG", msg); }
void TraceManager::info(const QString& msg) { enqueueLog("INFO", msg); }
void TraceManager::error(const QString& msg) { enqueueLog("ERROR", msg); }
void TraceManager::critical(const QString& msg) { enqueueLog("CRITICAL", msg); }
void TraceManager::warn(const QString& msg) { enqueueLog("WARN", msg); }
void TraceManager::setLogLevel(int level)
{
	QMutexLocker locker(&mutex);  // Protect access to `currentLevel`
	currentLevel = level;
}

bool TraceManager::isLevelEnabled(int level) const
{
	QMutexLocker locker(&mutex);  // Ensure safe concurrent access


	return level >= currentLevel;
}

void TraceManager::enableFileLogging(const QString& path)
{
	if (logFile.isOpen()) {
		logFile.flush();
		logFile.close();
	}

	logFile.setFileName(path);
	if (logFile.open(QIODevice::Append | QIODevice::Text)) {
		logStream.setDevice(&logFile);
		qDebug() << "[TraceManager] Logging to file:" << path;
	}
	else {
		qWarning() << "[TraceManager] Failed to open log file:" << path;
	}
}

void TraceManager::disableFileLogging()
{
	QMutexLocker locker(&mutex);

	if (logFile.isOpen()) {
		logStream.flush();
		logFile.close();
		logStream.setDevice(nullptr);
		qDebug() << "[TraceManager] File logging disabled.";
	}
}

void TraceManager::enqueueLog(const QString& level, const QString& message) {
	QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
	QString line = QString("[%1] [%2] %3").arg(timestamp, level, message);

	{
		QMutexLocker locker(&queueMutex);
		messageQueue.enqueue(line);
	}

	emit messageLogged(level, message);
	queueNotEmpty.wakeOne();
}

void TraceManager::startWorker() {
	QObject* worker = new QObject;
	flushTimer = new QTimer;

	worker->moveToThread(&workerThread);
	flushTimer->moveToThread(&workerThread);

	QObject::connect(flushTimer, &QTimer::timeout, this, &TraceManager::flushQueuedMessages);

	QObject::connect(&workerThread, &QThread::started, [=]() {
		flushTimer->start(100); // flush every 100ms
		});

	QObject::connect(&workerThread, &QThread::finished, [=]() {
		flushTimer->stop();
		flushQueuedMessages(); // flush any remaining messages
		delete flushTimer;
		delete worker;
		});

	workerThread.start();
}

void TraceManager::flushQueuedMessages() {
	QQueue<QString> batch;
	{
		QMutexLocker locker(&queueMutex);
		while (!messageQueue.isEmpty())
			batch.enqueue(messageQueue.dequeue());
	}

	QMutexLocker locker(&mutex);
	for (const QString& line : batch) {
		qDebug().noquote() << line;
		if (logFile.isOpen()) {
			logStream << line << '\n';
		}
	}

	if (logFile.isOpen())
		logStream.flush();
}


// void TraceWorker::process()
// {
// 	while (true) {
// 		QList<QString> batch;
// 
// 		m_lock->lock();
// 		if (m_queue->isEmpty()) {
// 			m_waitCondition->wait(m_lock); // Wait efficiently until notified
// 		}
// 		while (!m_queue->isEmpty() && batch.size() < 100) { // Batch 100 messages
// 			batch.append(m_queue->dequeue());
// 		}
// 		m_lock->unlock();
// 
// 		for (const QString& message : batch) {
// 			m_outputStream << message << Qt::endl;
// 		}
// 		m_outputStream.flush();
// 	}
// }
