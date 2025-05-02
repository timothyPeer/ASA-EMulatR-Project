#pragma once
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QQueue>
#include <QTimer>
#include <QFile>
#include <QTextStream>



class TraceManager : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(TraceManager)

public:
	static TraceManager& instance();
	~TraceManager();

	enum LogLevel {
		TRACE = 0,
		DEBUG = 1,
		INFO = 2,
		WARN = 3,  // ← NEW
		ERROR = 4,
		CRITICAL = 5
	};

	void trace(const QString& msg);
	void debug(const QString& msg);
	void info(const QString& msg);
	void error(const QString& msg);
	void critical(const QString& msg);
	void warn(const QString& msg);

	void setLogLevel(int level);
	bool isLevelEnabled(int level) const;

	void enableFileLogging(const QString& path);
	void disableFileLogging();

signals:
	void messageLogged(QString level, QString message);

private slots:
	void flushQueuedMessages();

private:
	TraceManager();
	void enqueueLog(const QString& level, const QString& message);
	void startWorker();

	mutable QMutex mutex;
	int currentLevel;
	QFile logFile;
	QTextStream logStream;

	// Background queue and thread
	QQueue<QString> messageQueue;
	QMutex queueMutex;
	QWaitCondition queueNotEmpty;
	QThread workerThread;
	QTimer* flushTimer;
	bool exitRequested;
};
