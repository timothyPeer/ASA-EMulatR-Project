#pragma once

#include <QObject>
#include <QHash>
#include <QSet>

class AlphaJITProfiler : public QObject
{
	Q_OBJECT

public:
	explicit AlphaJITProfiler(QObject* parent = nullptr);
	~AlphaJITProfiler();

	void setHotThreshold(int threshold) { hotThreshold = threshold; }
	int getHotThreshold() const { return hotThreshold; }

	// Call on each instruction
	void recordExecution(quint64 pc);

signals:
	void hotSpotDetected(quint64 startPC, quint64 endPC, int execCount);

private:
	int hotThreshold = 100;
	QHash<quint64, int> executionCounts;
	QSet<quint64> alreadyReported;
};
