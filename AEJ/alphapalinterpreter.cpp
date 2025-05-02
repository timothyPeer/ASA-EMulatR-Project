#include "AlphaPALInterpreter.h"
#include "AlphaCPU.h"

AlphaPALInterpreter::AlphaPALInterpreter(QObject* parent)
    : QObject(parent)
{
}

/**
 * Dispatch the PAL instruction
 */
void AlphaPALInterpreter::processPALInstruction(AlphaCPU* cpu, quint32 palFunction)
{
	if (!cpu)
		return;

	switch (palFunction) {
	case helpers_JIT::PAL_HALT:
		handleHalt(cpu);
		break;

	case helpers_JIT::PAL_SYSTEM_CALL:
		handleSystemCall(cpu);
		break;

	case helpers_JIT::PAL_WRKGP:
		handleWriteKernelGP(cpu);
		break;

	case helpers_JIT::PAL_WRUSP:
		handleWriteUserSP(cpu);
		break;

	case helpers_JIT::PAL_RDUSP:
		handleReadUserSP(cpu);
		break;

	case helpers_JIT::PAL_MACHINE_CHECK:
		handleMachineCheck(cpu);
		break;

	case helpers_JIT::PAL_BUS_ERROR:
		handleBusError(cpu);
		break;

	default:
		handleUnknownPAL(cpu, palFunction);
		break;
	}
}

// CPU Specific handlers for halt conditions
void AlphaPALInterpreter::handleHalt(AlphaCPU* cpu)
{
    cpu->setRunning(false);
    cpu->setState(helpers_JIT::CPUState::HALTED);
    emit cpu->executionStopped();
    emit cpu->stateChanged(helpers_JIT::CPUState::HALTED);
}

void AlphaPALInterpreter::handlePrivilegedContextSwitch(AlphaCPU* cpu)
{
    // Placeholder - In real Alpha, switch process context
    // Here you can just log or simulate a dummy switch
}

void AlphaPALInterpreter::handleSystemCall(AlphaCPU* cpu)
{
    // Raise a system call exception
    cpu->raiseException(helpers_JIT::ExceptionType::SYSTEM_CALL, cpu->getPC());
}

void AlphaPALInterpreter::handleUnknownPAL(AlphaCPU* cpu, quint32 palFunction)
{
    // Raise Illegal Instruction or Privileged Instruction Exception
    cpu->raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, cpu->getPC());
}

void AlphaPALInterpreter::handleWriteKernelGP(AlphaCPU* cpu)
{
	quint64 gpValue = cpu->readRegister(0); // In Alpha, R0 carries arguments
	cpu->setKernelGP(gpValue);
}

void AlphaPALInterpreter::handleWriteUserSP(AlphaCPU* cpu)
{
	quint64 spValue = cpu->readRegister(0); // Assume R0 carries new USP
	cpu->setUserSP(spValue);
}

void AlphaPALInterpreter::handleReadUserSP(AlphaCPU* cpu)
{
	cpu->writeRegister(0, cpu->getUserSP()); // Write current USP into R0
}

void AlphaPALInterpreter::handleMachineCheck(AlphaCPU* cpu)
{
	raiseException(helpers_JIT::ExceptionType::MACHINE_CHECK, cpu->getPC());
}

void AlphaPALInterpreter::handleBusError(AlphaCPU* cpu)
{
	raiseException(helpers_JIT::ExceptionType::BUS_ERROR, cpu->getPC());
}
