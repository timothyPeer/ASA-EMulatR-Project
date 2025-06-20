#include "alphajitprofiler.h"
// #include <QDebug>
// 
// AlphaJITProfiler::AlphaJITProfiler(QObject* parent)
// 	: QObject(parent)
// {
// }
// 
// AlphaJITProfiler::~AlphaJITProfiler()
// {
// }
// 
// void AlphaJITProfiler::recordExecution(quint64 pc)
// {
// 	int& count = executionCounts[pc];
// 	count++;
// 
// 	if (count >= hotThreshold && !alreadyReported.contains(pc)) {
// 		alreadyReported.insert(pc);
// 
// 		quint64 blockEnd = pc + 16;  // Placeholder for block boundary
// 		emit hotSpotDetected(pc, blockEnd, count);
// 	}
// 
// 	// Optional: clear old entries to manage memory
// 	if (executionCounts.size() > 10000) {
// 		executionCounts.clear();
// 		alreadyReported.clear();
// 	}
// }
// void AlphaJITProfiler::recordInstructionExecution(quint32 opcode, quint32 function) {
// 	// Create a composite key from opcode and function
// 	quint64 key = ((quint64)opcode << 32) | function;
// 	instructionFrequency[key]++;
// 
// 	// Periodically analyze to update hotpath cache
// 	if (instructionFrequency.size() > 100 &&
// 		(instructionFrequency.size() % 1000 == 0)) {
// 		emit instructionHotspotUpdated();
// 	}
// }
// 
// QList<QPair<quint32, quint32>> AlphaJITProfiler::getTopInstructions(int count) {
// 	// Get the most frequently executed instructions
// 	QList<QPair<quint64, int>> sorted;
// 
// 	for (auto it = instructionFrequency.begin(); it != instructionFrequency.end(); ++it) {
// 		sorted.append(qMakePair(it.key(), it.value()));
// 	}
// 
// 	std::sort(sorted.begin(), sorted.end(),
// 		[](const QPair<quint64, int>& a, const QPair<quint64, int>& b) {
// 			return a.second > b.second; // Sort in descending order
// 		});
// 
// 	// Convert back to opcode/function pairs
// 	QList<QPair<quint32, quint32>> result;
// 	for (int i = 0; i < qMin(count, sorted.size()); i++) {
// 		quint64 key = sorted[i].first;
// 		quint32 opcode = key >> 32;
// 		quint32 function = key & 0xFFFFFFFF;
// 		result.append(qMakePair(opcode, function));
// 	}
// 
// 	return result;
// }
// // void AlphaJITProfiler::recordInstructionExecution(quint32 opcode, quint32 function) {
// // 	// Create a composite key from opcode and function
// // 	quint64 key = ((quint64)opcode << 32) | function;
// // 	instructionFrequency[key]++;
// // 
// // 	// Update overall instruction count
// // 	m_totalInstructionCount++;
// // 
// // 	// Track specific types of instructions based on opcode
// // 	if (m_monitoringEnabled) {
// // 		// Memory operations (load/store)
// // 		if ((opcode >= 0x08 && opcode <= 0x0F) || // Load format
// // 			(opcode >= 0x20 && opcode <= 0x27) || // Float load format
// // 			(opcode >= 0x28 && opcode <= 0x2F)) { // Store format
// // 			recordMemoryOperation();
// // 		}
// // 
// // 		// Branch operations
// // 		if (opcode >= 0x30 && opcode <= 0x3F) {
// // 			recordBranchInstruction();
// // 		}
// // 	}
// // 
// // 	// Periodically analyze to update hotpath cache
// // 	if (instructionFrequency.size() > 100 &&
// // 		(instructionFrequency.size() % 1000 == 0)) {
// // 		emit instructionHotspotUpdated();
// // 	}
// // }
// 
// void AlphaJITProfiler::recordPrefetch(bool withEvictNext)
// {
// 	m_prefetchCount++;
// 	if (withEvictNext) evictNextCount++;
// 	// Optionally emit a signal for monitoring
//     emit prefetchRecorded(0, 0, false, withEvictNext);
// }
// 
// void AlphaJITProfiler::recordPrefetch(quint64 address, int size, bool modifyIntent, bool evictNext)
// {
// 	// Increment prefetch counter
// 	m_prefetchCount++;
// 
// 	// Track prefetch patterns if monitoring is enabled
// 	if (m_monitoringEnabled) {
// 		// Track prefetch by page
// 		quint64 page = address & ~0xFFFULL; // Get 4KB page address
// 		m_prefetchByPage[page]++;
// 
// 		// Track modify intent prefetches if relevant
// 		if (modifyIntent) {
// 			m_modifyIntentPrefetchCount++;
// 		}
// 
// 		// Track evict-next prefetches if relevant
// 		if (evictNext) {
// 			m_evictNextPrefetchCount++;
// 		}
// 
// 		// Optional: Track spatial locality (prefetch distance from last memory access)
// 		if (m_lastMemoryAccessAddr != 0) {
// 			quint64 distance = (address > m_lastMemoryAccessAddr) ?
// 				(address - m_lastMemoryAccessAddr) :
// 				(m_lastMemoryAccessAddr - address);
// 
// 			// Add to histogram (customize bins as needed)
// 			if (distance < 64) {
// 				m_prefetchDistanceHistogram[0]++;
// 			}
// 			else if (distance < 256) {
// 				m_prefetchDistanceHistogram[1]++;
// 			}
// 			else if (distance < 1024) {
// 				m_prefetchDistanceHistogram[2]++;
// 			}
// 			else if (distance < 4096) {
// 				m_prefetchDistanceHistogram[3]++;
// 			}
// 			else {
// 				m_prefetchDistanceHistogram[4]++;
// 			}
// 		}
// 	}
// 
// 	// Emit the signal with prefetch details
// 	emit prefetchRecorded(address, size, modifyIntent, evictNext);
// }
// 
// void AlphaJITProfiler::recordLockedOperation(quint64 address, bool isWrite, bool success /*= false*/)
// {
// 	if (!m_monitoringEnabled) return;
// 
// 	// Increment general counter
// 	m_lockedOperationCount++;
// 
// 	// Track read vs write operations
// 	if (isWrite) {
// 		m_lockedStoreCount++;
// 		// For writes, track success rate
// 		if (success) {
// 			m_successfulLockedStoreCount++;
// 		}
// 	}
// 	else {
// 		m_lockedLoadCount++;
// 	}
// 
// 	// Track by address (e.g., which synchronization variables are most contended)
// 	m_lockedOpsByAddress[address]++;
// }
// 
// void AlphaJITProfiler::recordMemoryOperation(quint64 address, int size, bool isWrite)
// {
// 	m_memoryOperationCount++;
// 	// Record this address for prefetch spatial locality analysis
// 	m_lastMemoryAccessAddr = address;
// }
// 
// void AlphaJITProfiler::recordMemoryOperation()
// {
// 	m_memoryOperationCount++;
// }
// 
// void AlphaJITProfiler::resetPerformanceCounters() {
// 	// Reset all performance counters to zero
// 	m_totalInstructionCount = 0;
// 	m_memoryOperationCount = 0;
// 	m_branchInstructionCount = 0;
// 	m_branchMispredictionCount = 0;
// 	m_cacheMissCount = 0;
// 	m_customEventCount = 0;
// 
// 	// Optionally reset instruction frequency data too
// 	if (!m_perfConfig.trackInstructionMix) {
// 		instructionFrequency.clear();
// 	}
// 
// 	// Reset hotspot tracking if not enabled
// 	if (!m_perfConfig.trackHotspots) {
// 		executionCounts.clear();
// 		alreadyReported.clear();
// 	}
// 
// 	qDebug() << "Performance counters have been reset";
// }
// 
// void AlphaJITProfiler::configureEventTracking(int eventType) {
// 	// Store the event type
// 	m_perfConfig.eventType = eventType;
// 
// 	// Set up tracking based on the event type
// 	switch (eventType) {
// 	case 0: // Cycle count - no special setup needed
// 		qDebug() << "Configured to track cycle count";
// 		break;
// 
// 	case 1: // Instruction count
// 		qDebug() << "Configured to track total instruction count";
// 		// Enable basic execution tracking
// 		m_perfConfig.trackInstructionMix = true;
// 		break;
// 
// 	case 2: // Memory operations
// 		qDebug() << "Configured to track memory operations";
// 		m_perfConfig.trackMemoryStats = true;
// 		break;
// 
// 	case 3: // Branch instructions
// 		qDebug() << "Configured to track branch instructions";
// 		m_perfConfig.trackBranchStats = true;
// 		break;
// 
// 	case 4: // Branch mispredictions
// 		qDebug() << "Configured to track branch mispredictions";
// 		m_perfConfig.trackBranchStats = true;
// 		break;
// 
// 	case 5: // Cache misses
// 		qDebug() << "Configured to track cache misses";
// 		m_perfConfig.trackMemoryStats = true;
// 		break;
// 
// 	case 6: // TLB misses (handled by TLBSystem)
// 		qDebug() << "Configured to track TLB misses";
// 		// No special setup needed as TLB misses are tracked externally
// 		break;
// 
// 	case 7: // Custom event count - can be used for application-specific events
// 		qDebug() << "Configured to track custom events";
// 		break;
// 
// 	default:
// 		qWarning() << "Unknown event type:" << eventType;
// 		break;
// 	}
// 
// 	// Make sure the appropriate tracking is enabled for the event type
// 	if (m_perfConfig.eventType == 1 || m_perfConfig.eventType == 2 ||
// 		m_perfConfig.eventType == 3 || m_perfConfig.eventType == 4) {
// 		// These event types require instruction tracking to be useful
// 		m_perfConfig.trackInstructionMix = true;
// 	}
// 
// 	  m_trackedEventType = eventType;
// 
//         // Reset counters when changing event type
//         m_totalInstructionCount = 0;
//         m_memoryOperationCount = 0;
//         m_branchInstructionCount = 0;
//         m_branchMispredictionCount = 0;
//         m_cacheMissCount = 0;
//         m_customEventCount = 0;
// }
// 
