// #pragma once
// 
// #include <QObject>
// #include <QHash>
// #include <QSet>
// #include <QList>
// #include <QMap>
// 
// class AlphaJITProfiler : public QObject
// {
// 	Q_OBJECT
// 
// private:
// 	// Performance counter configuration
// 	struct PerformanceCounterConfig {
// 		bool enabled = false;              // Is performance counting enabled
// 		int eventType = 0;                 // Type of event being tracked (0-7)
// 		bool trackInstructionMix = false;  // Track instruction type distribution
// 		bool trackBranchStats = false;     // Track branch statistics
// 		bool trackMemoryStats = false;     // Track memory access patterns
// 		bool trackHotspots = true;         // Track execution hotspots
// 		uint64_t samplingRate = 1;         // Sample every N instructions (1 = all)
// 		uint64_t counterThreshold = 0;     // Threshold for triggering alerts
// 	};
// public:
// 	explicit AlphaJITProfiler(QObject* parent = nullptr);
// 	~AlphaJITProfiler();
// 
// 	// Event tracking methods
// 	void configureEventTracking(int eventType);
// 
// 	// Performance alert types
// 	enum class PerformanceAlertType {
// 		InstructionCountExceeded,
// 		MemoryOperationsExceeded,
// 		BranchMispredictionsExceeded,
// 		CacheMissesExceeded,
// 		TLBMissesExceeded,
// 		CustomEventExceeded
// 	};
// 	// Configure the performance counter settings
// 	//void configurePerformanceCounter(quint64 configValue);
// 	// Performance counter getters
// 	quint64 getBranchInstructionCount() const { return m_branchInstructionCount; }
// 	quint64 getBranchMispredictionCount() const { return m_branchMispredictionCount; }
// 	quint64 getCacheMissCount() const { return m_cacheMissCount; }
// 	quint64 getCustomEventCount() const { return m_customEventCount; }
// 
// 	quint64 getMemoryOperationCount() const { return m_memoryOperationCount; }
// 	quint64 getTotalInstructionCount() const { return m_totalInstructionCount; }
// 
// 	// Prefetch statistics accessors
// 	quint64 getEvictNextCount() const { return m_evictNextCount; }
// 	quint64 getEvictNextPrefetchCount() const { return m_evictNextPrefetchCount; }
// 	quint64 getPrefetchCount() const { return m_prefetchCount; }
// 
// 	quint64 getModifyIntentPrefetchCount() const { return m_modifyIntentPrefetchCount; }
// 
// 	/**
// 	 * @brief Gets prefetch histogram data for visualization
// 	 * @return Map of distance ranges to counts
// 	 */
// 	QMap<QString, int> getPrefetchDistanceHistogram() const {
// 		QMap<QString, int> result;
// 		result["0-64 bytes"] = m_prefetchDistanceHistogram[0];
// 		result["64-256 bytes"] = m_prefetchDistanceHistogram[1];
// 		result["256-1K bytes"] = m_prefetchDistanceHistogram[2];
// 		result["1K-4K bytes"] = m_prefetchDistanceHistogram[3];
// 		result[">4K bytes"] = m_prefetchDistanceHistogram[4];
// 		return result;
// 	}
// 
// 
// 
// 	bool isMonitoringEnabled() const { return m_monitoringEnabled; }
// 	// Set monitoring state
// 	/*void setMonitoringEnabled(bool enabled) { m_monitoringEnabled = enabled; }*/
// 
// 	// Record branch instruction execution
// 	void recordBranchInstruction() { m_branchInstructionCount++; }
// 
// 	// Record instruction execution with opcode and function
// /*	void recordInstructionExecution(quint32 opcode, quint32 function);*/
// 
// 
// 	// Record specific events
// 	void recordInstructionCount() { m_totalInstructionCount++; }
// 
// 	
// 	void recordBranchMisprediction() { m_branchMispredictionCount++; }
// 	void recordCacheMiss() { m_cacheMissCount++; }
// 	void recordCustomEvent() { m_customEventCount++; }
// 	/**
//  * @brief Records a locked memory operation (LL/SC)
//  * @param address The memory address
//  * @param isWrite True if this is a write operation (SC)
//  * @param success For SC operations, indicates if the operation succeeded
//  */
// 	void recordLockedOperation(quint64 address, bool isWrite, bool success = false);
// 	void recordMemoryOperation(quint64 address, int size, bool isWrite);
// 	void recordMemoryOperation();
// 	void recordPrefetch(bool withEvictNext);
// 	/**
//  * @brief Records a prefetch operation and emits prefetchRecorded signal
//  * @param address The virtual address being prefetched
//  * @param size Size of the prefetch operation in bytes
//  * @param modifyIntent Whether the prefetch has modify intent
//  * @param evictNext Whether the prefetch has evict-next hint
//  */
// 	void recordPrefetch(quint64 address, int size, bool modifyIntent, bool evictNext);
// 	// Helper method to reset counters
// 	void resetPerformanceCounters();
// 
// 	void setMonitoringEnabled(bool enabled) { m_monitoringEnabled = enabled; }
// 
// 	void setHotThreshold(int threshold) { hotThreshold = threshold; }
// 	int getHotThreshold() const { return hotThreshold; }
// 	/**
// 	 * @brief Gets statistics about locked operations
// 	 *
// 	 * @return QPair<quint64, quint64> First is total LL ops, second is successful SC ops
// 	 */
// 	QPair<quint64, quint64> getLockedOperationStats() const;
// 
// 	/**
// 	 * @brief Gets the success rate of Store-Conditional operations
// 	 *
// 	 * @return double Percentage of SC operations that succeeded
// 	 */
// 	double getStoreConditionalSuccessRate() const;
// 
// 	// Call on each instruction
// 	void recordExecution(quint64 pc);
// 	void recordInstructionExecution(quint32 opcode, quint32 function);
// 
// 	QList<QPair<quint32, quint32>> getTopInstructions(int count = 10);
// 
// 	
// signals:
// 	void hotSpotDetected(quint64 startPC, quint64 endPC, int execCount);
// 	void instructionHotspotUpdated();
// 	void performanceAlert(PerformanceAlertType alertType, quint64 counterValue);
// 	/**
// 		 * @brief Signal emitted when a prefetch operation is recorded
// 		 * @param address The virtual address being prefetched
// 		 * @param size Size of the prefetch operation in bytes
// 		 * @param modifyIntent Whether the prefetch has modify intent
// 		 * @param evictNext Whether the prefetch has evict-next hint
// 		 */
// 	void prefetchRecorded(quint64 address, int size, bool modifyIntent, bool evictNext);
// 
// private:
// 	int hotThreshold = 100;
// 	QHash<quint64, int> executionCounts;
// 	QHash<quint32, int> instructionFrequency; // Maps opcode to execution count
// 	QSet<quint64> alreadyReported;
// 	bool m_monitoringEnabled = false;					// Enable Monitor
// 	PerformanceCounterConfig	m_perfConfig;
// 	int m_trackedEventType = 0;
// 	quint64 m_totalInstructionCount = 0;
// 	quint64 m_memoryOperationCount = 0;
// 	quint64 m_branchInstructionCount = 0;
// 	quint64 m_branchMispredictionCount = 0;
// 	quint64 m_cacheMissCount = 0;
// 	quint64 m_customEventCount = 0;
// 	//quint64 prefetchCount = 0;
// 	quint64 evictNextCount = 0;
// 	quint64 m_prefetchCount = 0;
// 	quint64 m_evictNextCount = 0;
// 	quint64 m_modifyIntentPrefetchCount = 0;
// 	quint64 m_evictNextPrefetchCount = 0;
// 	QHash<quint64, int> m_prefetchByPage;
// 	quint64 m_lastMemoryAccessAddr = 0;
// 	QVector<int> m_prefetchDistanceHistogram = { 0, 0, 0, 0, 0 }; // Initialize with 5 bins
// 	// Locked operation statistics
// 	quint64 m_lockedOperationCount = 0;
// 	quint64 m_lockedLoadCount = 0;
// 	quint64 m_lockedStoreCount = 0;
// 	quint64 m_successfulLockedStoreCount = 0;
// 	QHash<quint64, int> m_lockedOpsByAddress;
// };
// 
// /*
// Implementation Notes: 
// 
// External classes like UI components connect to performanceAlertTriggered to display or log the alerts
// 
// This connection chain allows decoupling between the profiler logic, the JIT compiler, and the UI
// components that display the performance data.
// 
// The performanceAlertTriggered signal is not directly connected to anything in the AlphaJITCompiler
// itself - it's meant to be connected to by external classes that want to respond to these performance
// alerts. That's why you didn't see a direct connection for this signal within the AlphaJITCompiler code.
// 
// 
// Signal: 
// 
//    performanceAlert(PerformanceAlertType, quint64);
// 
//    --------------------------------------------------------------------
//    Inside AlphaJITProfiler::recordInstructionExecution or other monitoring methods, when a threshold is exceeded:
//      emit performanceAlert(PerformanceAlertType::InstructionCountExceeded, count);
// 
//    The AlphaJITCompiler receives this in handlePerformanceAlert and emits its own signal:
//      emit performanceAlertTriggered(alertMessage, value);
// 
// 
// Example Usage: 
// // In some other class's constructor or setup method
// void SomeClass::setupConnections() {
// 	// Connect to the AlphaJITCompiler's performance alert signal
// 	connect(m_jitCompiler, &AlphaJITCompiler::performanceAlertTriggered,
// 			this, &SomeClass::onPerformanceAlert);
// }
// 
// // The handler in the receiving class
// void SomeClass::onPerformanceAlert(const QString& message, quint64 value) {
// 	// Display the alert in the UI
// 	ui->statusBar->showMessage(QString("Performance Alert: %1 (%2)").arg(message).arg(value));
// 
// 	// Maybe log to a file
// 	m_logFile.write(QString("ALERT: %1 - %2\n").arg(message).arg(value).toUtf8());
// 
// 	// Or update a performance graph
// 	m_performanceGraph->addDataPoint(value);
// }
// */