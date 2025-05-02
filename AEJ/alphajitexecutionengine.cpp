// AlphaJITExecutionEngine.cpp
#include "AlphaJITExecutionEngine.h"
#include <QDebug>

AlphaJITExecutionEngine::AlphaJITExecutionEngine(RegisterBank* regs,
	FpRegisterBankcls* fpRegs,
	SafeMemory* mem,
	QObject* parent)
	: QObject(parent)
	, registerBank(regs)
	, fpRegisterBank(fpRegs)
	, memory(mem)
	, registers(32, 0)
	, fpRegisters(32, 0.0)
	, pc(0)
	, traceThreshold(50)
{
	alphaProfiler = new AlphaJITProfiler(this);
	alphaCompiler = new AlphaJITCompiler(registerBank, fpRegisterBank, memory, this);
}

AlphaJITExecutionEngine::~AlphaJITExecutionEngine() {
	// QObject hierarchy auto-cleans children
}

void AlphaJITExecutionEngine::loadCode(const QVector<quint32>& code, quint64 baseAddress) {
	Q_UNUSED(code)
	Q_UNUSED(baseAddress)
		// TODO : LoadCode -- is this function needed?
}

helpers_JIT::ExecutionResult AlphaJITExecutionEngine::execute(quint64 startAddress,
	int maxInstructions) {
	Q_UNUSED(maxInstructions)
		pc = startAddress;
	helpers_JIT::ExecutionResult result;
	result.instructionsExecuted = 0;
	result.finalPC = alphaCompiler->executeBlock(pc);
	result.registers = registers;
	result.fpRegisters = fpRegisters;
	result.compiledBlocks = 0;
	result.compiledTraces = 0;
	return result;
}

QMap<quint64, AlphaBasicBlock*>& AlphaJITExecutionEngine::getBasicBlocks() {
	return basicBlocks;
}

QMap<QString, AlphaTrace*>& AlphaJITExecutionEngine::getTraces() {
	return traces;
}

FpRegisterBankcls* AlphaJITExecutionEngine::getFpRegisterBank()
{
	return fpRegisterBank;
}

quint64 AlphaJITExecutionEngine::getPC()
{
	return pc;
}

