// AlphaJITCompiler.cpp
#include "AlphaJITCompiler.h"
#include <QMutexLocker>
#include <QDebug>
#include "decodeOperate.h"

AlphaJITCompiler::AlphaJITCompiler(RegisterBank* intRegs,
	FpRegisterFileWrapper* fpRegs,
	SafeMemory* mem,
	QObject* parent)
	: QObject(parent)
	, integerRegs(intRegs)
	, floatingRegs(fpRegs)
	, memory(mem)
	, profiler(this)
{
	profiler.setHotThreshold(100);
}

AlphaJITCompiler::BlockFunc AlphaJITCompiler::compileOrGetBlock(quint64 pc) {
	quint64 hits = ++hitCounters[pc];
	{
		QMutexLocker locker(&cacheMutex);
		if (cache.contains(pc)) return cache.value(pc);
	}
	if (hits >= profiler.getHotThreshold()) {
		auto func = createJITBlock(pc);
		QMutexLocker locker(&cacheMutex);
		cache.insert(pc, func);
		return func;
	}
	return createInterpreterBlock(pc);
}

quint64 AlphaJITCompiler::executeBlock(quint64 pc) {
	return compileOrGetBlock(pc)();
}

AlphaJITCompiler::BlockFunc AlphaJITCompiler::createJITBlock(quint64 pc) {
	return [this, pc]() -> quint64 {
		quint64 currentPC = pc;
		while (true) {
			quint32 rawInstr = memory->readUInt32(currentPC);
			auto op = decodeOperate(rawInstr);
			currentPC += 4;

			switch (static_cast<Section>(op.opcode)) {
			case Section::SECTION_INTEGER:
				if (op.function == 0x20) {
					quint64 a = integerRegs->readIntReg(op.ra);
					quint64 b = integerRegs->readIntReg(op.rb);
					integerRegs->writeIntReg(op.rc, a + b);
				}
				else return interpretBlock(pc);
				break;

			case Section::SECTION_FLOATING_POINT: {
				auto fa = floatingRegs->readFpReg(op.ra);
				auto fb = floatingRegs->readFpReg(op.rb);
				floatingRegs->writeFpReg(op.rc, fa + fb);
				break;
			}

			case Section::SECTION_VECTOR:
			case Section::SECTION_CONTROL:
			case Section::SECTION_PAL:
			default:
				return interpretBlock(pc);
			}
		}
		return currentPC;
		};
}


AlphaJITCompiler::BlockFunc AlphaJITCompiler::createInterpreterBlock(quint64 pc) {
	return [this, pc]() -> quint64 {
		qDebug() << "Interpreting block at PC=" << QString::number(pc, 16);
		return interpretBlock(pc);
		};
}

quint64 AlphaJITCompiler::interpretBlock(quint64 pc) {
	qWarning() << "Interpreter fallback at PC=" << QString::number(pc, 16);
	return pc + 4;
}

void AlphaJITCompiler::updateBranchPredictor(quint64 pc, quint64 actualTarget) {
	branchPredictor[pc] = actualTarget;
}
