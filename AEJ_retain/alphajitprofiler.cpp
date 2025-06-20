#include "alphajitprofiler.h"

AlphaJITProfiler::AlphaJITProfiler(QObject* parent)
	: QObject(parent)
{
}

AlphaJITProfiler::~AlphaJITProfiler()
{
}

void AlphaJITProfiler::recordExecution(quint64 pc)
{
	int& count = executionCounts[pc];
	count++;

	if (count >= hotThreshold && !alreadyReported.contains(pc)) {
		alreadyReported.insert(pc);

		quint64 blockEnd = pc + 16;  // Placeholder for block boundary
		emit hotSpotDetected(pc, blockEnd, count);
	}

	// Optional: clear old entries to manage memory
	if (executionCounts.size() > 10000) {
		executionCounts.clear();
		alreadyReported.clear();
	}
}
