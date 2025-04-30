#pragma once
// AlphaJITCompiler.h - JIT compiler header
#ifndef ALPHAJITCOMPILER_H
#define ALPHAJITCOMPILER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QAtomicInt>
#include <QPair>
#include <QByteArray>
#include "..\AESH\Helpers.h"

/**
 * @brief AlphaJITCompiler - Just-In-Time compiler for Alpha instructions
 *
 * This class translates Alpha instructions to native code for efficient execution.
 * It runs in a separate thread to avoid blocking CPU execution.
 */
// class AlphaJITCompiler : public QObject
// {
//     Q_OBJECT
// 
// public:
//    
// 
//     explicit AlphaJITCompiler(QObject* parent = nullptr);
//     ~AlphaJITCompiler();
// 
//     void initialize();
//     void shutdown();
// 
//     // Configuration - provides compatibility with both class versions
// 
// 	/// Returns true if this PC has been compiled to native code
// 	bool hasBlock(quint64 pc) const;
// 
// 	/// Executes a compiled block
// 	void runBlock(quint64 pc, AlphaCPU* cpu);
// 
// 	/// Compile a block if needed (e.g., hot path)
// 	void compileBlock(quint64 pc);
// 
// 
// 
// 	void setOptimizationLevel(int level) {
// 		m_optimizationLevel = static_cast<helpers_JIT::OptimizationLevel>(level);
// 	}
//     void setOptimizationLevel(helpers_JIT::OptimizationLevel level);
//     //void setOptimizationLevel(helpers_JIT::OptimizationLevel level) { (m_optimizationLevel = level); }
//     helpers_JIT::OptimizationLevel getOptimizationLevel_toEnum() const { return m_optimizationLevel; }
//     int getOptimizationLevel_toInt() const { return static_cast<int>(m_optimizationLevel); }
// 
// public slots:
//     // Compilation requests
//     void compileBlock(quint64 startAddr, const QByteArray& instructions);
//     bool compileInstruction(quint64 address, quint32 instruction);
//     void invalidateBlock(quint64 startAddr);
//     void prioritizeCompilation(quint64 startAddr);
// 
//     // Control
//     void startCompiler();
//     void stopCompiler();
//     void pauseCompiler();
//     void resumeCompiler();
//     void trapRaised(helpers_JIT::TrapType trapType);
//     void handleTrap(helpers_JIT::TrapType trapType);
// 
// signals:
//     // Compilation status
//     void blockCompiled(quint64 startAddr, const QByteArray& nativeCode);
//     void compilationProgress(int percentComplete);
//     void compilationError(quint64 startAddr, const QString& errorMessage);
//     void compilerStatusChanged(bool running);
// 
// private:
//     // Thread control
//     QThread* m_compilerThread;
//     QAtomicInt m_running;
//     QAtomicInt m_paused;
// 
//     // Compilation queue and synchronization
//     QMutex m_compilerLock;
//     QQueue<QPair<quint64, QByteArray>> m_compilationQueue;
//     QQueue<quint64> m_priorityQueue;
//     QWaitCondition m_workAvailable;
// 
//     // Configuration
//     helpers_JIT::OptimizationLevel m_optimizationLevel;
// 
//     // Compiler execution
//     void compilerThreadMain();
// 
// 	// Stores a mapping from PC to function pointer or code blob
// 	QHash<quint64, std::function<void(AlphaCPU*)>> m_compiledBlocks;
// 
//     // Compilation steps
//     QByteArray generateNativeCode(quint64 startAddr, const QByteArray& alphaCode);
//     QByteArray decodeToIntermediateRepresentation(const QByteArray& alphaCode);
//     void applyOptimizations(QByteArray& ir);
//     QByteArray generateMachineCode(const QByteArray& ir);
// 
//     // Optimization passes
//     void applyConstantFolding(QByteArray& ir);
//     void eliminateDeadCode(QByteArray& ir);
//     void emitFallbackCall(quint64 address, quint32 instruction);
//     void applyCommonSubexpressionElimination(QByteArray& ir);
//     void applyRegisterAllocation(QByteArray& ir);
//     void applyInstructionScheduling(QByteArray& ir);
// 
//     // Helper methods
//     QPair<quint64, QByteArray> getNextCompilationJob();
// };


#include <QHash>
#include <functional>

class AlphaCPU;

class AlphaJITCompiler {
public:
	AlphaJITCompiler();

	void setJitThreshold(int threshold_) {
		m_threshold = threshold_;
	}

	bool hasBlock(quint64 pc) const;
	void runBlock(quint64 pc, AlphaCPU* cpu);

	void recordHit(quint64 pc);
	bool shouldCompile(quint64 pc) const;

	void compileBlock(quint64 pc);
	void installStub(quint64 pc, std::function<void(AlphaCPU*)> handler);

	void clear();					// Clear the HitCounters
	void clearAll();				// Clear the JIT Block and HitCounters
	void setOptimizationLevel(int optimizationLevel);
private:
	QHash<quint64, std::function<void(AlphaCPU*)>> m_blocks;
	QHash<quint64, int> m_hitCount;
	int m_threshold = 50;
	
};


#endif // ALPHAJITCOMPILER_H