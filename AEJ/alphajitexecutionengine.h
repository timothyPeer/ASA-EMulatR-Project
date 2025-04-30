#pragma once

#include <QObject>
#include <QVector>
#include <QMap>
#include "alphajitcompiler.h"
#include "AlphaBasicBlock.h"
#include "alphatrace.h"
#include "alphajitprofiler.h"
#include "..\AESH\Helpers.h"

AlphaJITCompiler compiler;

/**
 * Alpha JIT Execution Engine - manages execution of Alpha code
 */
class AlphaJITExecutionEngine : public QObject
{
    Q_OBJECT

public:

    explicit AlphaJITExecutionEngine(QObject* parent = nullptr) : QObject(parent)
    {
        // Initialize registers
        registers.fill(0, 32);
        fpRegisters.fill(0.0, 32);
    }

    ~AlphaJITExecutionEngine()
    {
		delete   alphaProfiler;
        delete   alphaCompiler;
    }
    void loadCode(const QVector<quint32>& code, quint64 baseAddress = 0) {
        // Implementation
    }

    helpers_JIT::ExecutionResult execute(quint64 startAddress, int maxInstructions = 1000000) {
        // Placeholder implementation
        helpers_JIT::ExecutionResult result;
        result.instructionsExecuted = 0;
        result.finalPC = startAddress;
        result.registers = getRegisters();
        result.fpRegisters = getFpRegisters();
        result.compiledBlocks = 0;
        result.compiledTraces = 0;
        return result;
    }

    void setTraceThreshold(int threshold) { traceThreshold = threshold; }
    int getTraceThreshold() const { return traceThreshold; }

    QVector<int> getRegisters() const { return registers; }
    QVector<double> getFpRegisters() const { return fpRegisters; }
    quint64 getPC() const { return pc; }

    QMap<quint64, AlphaBasicBlock*>& getBasicBlocks() ;
    QMap<QString, AlphaTrace*>& getTraces() ;
    void setAlphaProfiler(AlphaJITProfiler* profiler) { alphaProfiler = profiler; }
    void setAlphaCompiler(AlphaJITCompiler* compiler) { alphaCompiler = compiler;  }
    AlphaJITProfiler* getAlphaJITProfiler() { return alphaProfiler; }
    AlphaJITCompiler* getAlphaJITCompiler() { return alphaCompiler; }
private:
    QVector<int> registers = QVector<int>(32, 0);  // 32 Alpha registers
    QVector<double> fpRegisters = QVector<double>(32, 0.0);  // 32 Alpha FP registers
    quint64 pc = 0;  // Program counter
    int traceThreshold = 50;  // Threshold for trace formation

    QMap<quint64, AlphaBasicBlock*> basicBlocks;
    QMap<QString, AlphaTrace*> traces;
    AlphaJITProfiler* alphaProfiler;
    AlphaJITCompiler* alphaCompiler;
};
