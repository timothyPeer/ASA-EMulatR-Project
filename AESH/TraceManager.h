#pragma once
#ifndef TRACEMANAGER_H
#define TRACEMANAGER_H

#include <QObject>
#include <QDebug>
#include <QString>
#include <QFile>

/**
 * @brief TraceManager provides centralized runtime tracing control for the emulator.
 * Allows dynamic enabling/disabling of debug output based on trace levels.


Main Emulator Threads
(AlphaCPUs, IOManager, SMPManager, Devices)
	|
	|  --> TraceManager::logXXX("message")
	|
	|  --> [ QMutex + QQueue<QString> ] buffer
		  |
		  |  (background thread)
		  v
[TraceWorker QThread]
	|
	|---> Write to File
	|---> Or Console
	|---> (Future: network UDP)



 */


#include <QTextStream>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QThread>

 /**
  * @brief TraceWorker processes log messages asynchronously in a background thread.
  */
class TraceWorker : public QObject
{
    Q_OBJECT

public:
    explicit TraceWorker(QQueue<QString>* sharedQueue, QMutex* lock, QWaitCondition* waitCond, QObject* parent = nullptr)
        : QObject(parent), m_queue(sharedQueue), m_lock(lock), m_waitCondition(waitCond)
    {
        m_logFile.setFileName("trace_output.log");
        m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        m_outputStream.setDevice(&m_logFile);
    }

public slots:
    void process();
   

private:
    QQueue<QString>* m_queue;
    QMutex* m_lock;
    QWaitCondition* m_waitCondition;
    QFile m_logFile;
    QTextStream m_outputStream;
};

/**
 * @brief TraceManager provides centralized, thread-safe tracing with async background worker.
 */
class TraceManager : public QObject
{
    Q_OBJECT

public:
    static TraceManager& instance()
    {
        static TraceManager instance;
        return instance;
    }

    static void setTraceLevel(int level) { instance().m_traceLevel = level; }
    static int getTraceLevel() { return instance().m_traceLevel; }

    static void logInfo(const QString& message)
    {
        if (instance().m_traceLevel >= 1) {
            instance().enqueueMessage("[INFO] " + message);
        }
    }

    static void logDebug(const QString& message)
    {
        if (instance().m_traceLevel >= 2) {
            instance().enqueueMessage("[DEBUG] " + message);
        }
    }

    static void logVerbose(const QString& message)
    {
        if (instance().m_traceLevel >= 3) {
            instance().enqueueMessage("[VERBOSE] " + message);
        }
    }

    void startWorker()
    {
        m_worker = new TraceWorker(&m_logQueue, &m_queueLock, &m_waitCondition);
        m_workerThread = new QThread(this);
        m_worker->moveToThread(m_workerThread);

        connect(m_workerThread, &QThread::started, m_worker, &TraceWorker::process);

        m_workerThread->start();
    }

    void stopWorker()
    {
        if (m_workerThread) {
            m_workerThread->quit();
            m_workerThread->wait();
            delete m_worker;
            delete m_workerThread;
        }
    }

private:
    explicit TraceManager(QObject* parent = nullptr)
        : QObject(parent), m_traceLevel(0), m_worker(nullptr), m_workerThread(nullptr)
    {
    }

    void enqueueMessage(const QString& message)
    {
        QMutexLocker locker(&m_queueLock);
        m_logQueue.enqueue(message);
        m_waitCondition.wakeOne(); // Wake the worker thread
    }

    int m_traceLevel;
    QQueue<QString> m_logQueue;
    QMutex m_queueLock;
    QWaitCondition m_waitCondition;

    TraceWorker* m_worker;
    QThread* m_workerThread;
};

#endif // TRACEMANAGER_H





