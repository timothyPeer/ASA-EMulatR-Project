// AlphaJITExecutionEngine.h
#pragma once

#include <QObject>
#include <QMap>
#include <QVariantList>
#include <QVector>
#include "AlphaJITCompiler.h"
#include "AlphaBasicBlock.h"
#include "AlphaTrace.h"
#include "AlphaJITProfiler.h"
#include "RegisterBank.h"
#include "FpRegisterBankcls.h"
#include "SafeMemory.h"

/**
 * @brief Manages execution of Alpha code with JIT and profiling.
 */
class AlphaJITExecutionEngine : public QObject {
    Q_OBJECT

public:
    explicit AlphaJITExecutionEngine(RegisterBank* regs,
        FpRegisterBankcls* fpRegs,
        SafeMemory* memory,
        QObject* parent = nullptr);
    ~AlphaJITExecutionEngine() override;

    void loadCode(const QVector<quint32>& code, quint64 baseAddress = 0);
    helpers_JIT::ExecutionResult execute(quint64 startAddress,
        int maxInstructions = 1000000);

	AlphaJITCompiler* getAlphaJITCompiler() const { return alphaCompiler; }
	AlphaJITProfiler* getAlphaJITProfiler() const { return alphaProfiler; }

	/// Return the integer-register snapshot for UI/serialization
	QVector<quint64> getRegisters() const { return registers; }
	/// Return the floating-point register snapshot
	QVector<double> getFpRegisters() const { return fpRegisters; }

	void setTraceThreshold(int threshold) { traceThreshold = threshold; }
	int getTraceThreshold() const { return traceThreshold; }

    QMap<quint64, AlphaBasicBlock*>& getBasicBlocks();
    QMap<QString, AlphaTrace*>& getTraces();

    FpRegisterBankcls* getFpRegisterBank();
    quint64 getPC();
private:
    RegisterBank* registerBank;
    FpRegisterBankcls* fpRegisterBank;
    SafeMemory* memory;

    AlphaJITProfiler* alphaProfiler;
    AlphaJITCompiler* alphaCompiler;

    QVariantList registersList;
    QVector<quint64> registers;
    QVector<double> fpRegisters;
    quint64 pc;
    int traceThreshold;

    QMap<quint64, AlphaBasicBlock*> basicBlocks;
    QMap<QString, AlphaTrace*> traces;
};
