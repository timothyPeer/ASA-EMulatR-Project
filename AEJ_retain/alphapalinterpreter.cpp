#include "AlphaPALInterpreter.h"
#include "AlphaCPU_fixed.h"

AlphaPALInterpreter::AlphaPALInterpreter(IExecutionContext* context, QObject* parent)
    : ctx(context), QObject(parent)
{
}

/**
 * Dispatch the PAL instruction
 */
void AlphaPALInterpreter::processPALInstruction(quint64 palFunctionCode)
{
	switch (palFunctionCode)
	{
	case PAL_HALT:
		qInfo() << "[PALInterpreter] Executing PAL_HALT (0x0000)";
		// Notify via trap, shutdown signal, or dedicated handler
		ctx->raiseTrap(static_cast<int>(helpers_JIT::TrapType::Breakpoint));  // or a custom TrapType::Halt
		break;
	case PAL_MCHK:
		qCritical() << "[PALInterpreter] Machine Check Exception (PAL_MCHK) triggered at PC:"
			<< QString("0x%1").arg(ctx->getPC(), 8, 16, QLatin1Char('0'));

		// Raise a machine check trap – this may halt or enter recovery depending on system policy
		ctx->raiseTrap(static_cast<int>(helpers_JIT::TrapType::MachineCheck));
		break;
	case PAL_BPT:
		qInfo() << "[PALInterpreter] Breakpoint trap (PAL_BPT) triggered at PC:"
			<< QString("0x%1").arg(ctx->getPC(), 8, 16, QLatin1Char('0'));

		// Raise a trap to the emulator core for breakpoint handling
		ctx->raiseTrap(static_cast<int>(helpers_JIT::TrapType::Breakpoint));
		break;
	case PAL_BUGCHK:
		qCritical() << "[PALInterpreter] BUGCHK triggered – Fatal system condition at PC:"
			<< QString("0x%1").arg(ctx->getPC(), 8, 16, QLatin1Char('0'));

		// Simulate a fatal kernel condition or internal firmware panic
		ctx->raiseTrap(static_cast<int>(helpers_JIT::TrapType::ReservedInstruction));

		// Optionally log full CPU register state or raise fatal event to host system
		// Example: emit fatalBugCheck(); if integrated with debugger or GUI
		break;
	case PAL_WRKGP:
		qInfo() << "[PALInterpreter] WRKGP (Write Kernel Global Pointer):"
			<< QString("0x%1").arg(value, 8, 16, QLatin1Char('0'));

		// In a full OS environment, this would write to the kernel GP register or shadow
		// For emulation, you might store it in a context field or reserved register
		ctx->writeIntReg(kernelGpIndex, value);  // Replace with your emulator's kernel GP index
		ctx->notifyRegisterUpdated(false, kernelGpIndex, value);
		break;
	case PAL_WRUSP:
		break;
	case PAL_RDUSP:
		break;
	case PAL_WRPERFMON:
		break;
	case PAL_RDDPERFMON:
		break;
	case PAL_IMB:
		break;
	case PAL_REI:
		break;
	case PAL_SWPCTX:
		break;
	case PAL_CALLSYS:
		break;
	case PAL_RET:
		break;
	case PAL_CALLPRIV:
		break;
	case PAL_RDUNIQUE:
		break;
	case PAL_WRUNIQUE:
		break;
	case PAL_TBIA:
		break;
	case PAL_TBIS:
		break;
	case PAL_TBIM:
		break;
	case PAL_TBIE:
		break;
	case PAL_DRAINA:
		break;
	case PAL_SWPPAL:
		break;
	case PAL_SWPIPL:
		break;
	case PAL_RDPS:
		break;
	case PAL_WRPS:
		break;
	case PAL_WRVPTPTR:
		break;
	case PAL_SWASTEN:
		break;
	case PAL_WRASTEN:
		break;
	case PAL_RDASTEN:
		break;
	case PAL_EXCB:
		break;
	default:
		qWarning() << "[PALInterpreter] Unknown PAL function:" << QString::number(palFunctionCode, 16);
		ctx->notifyIllegalInstruction(static_cast<quint64>(palFunctionCode), ctx->getPC());
		break;
	}
}

void AlphaPALInterpreter::raiseException(helpers_JIT::ExceptionType, quint64 pc_)
{
	// TODO
}

// CPU Specific handlers for halt conditions
void AlphaPALInterpreter::handleHalt()
{
    cpu->setRunning(false);
    cpu->setState(helpers_JIT::CPUState::HALTED);
    emit cpu->executionStopped();
    emit cpu->stateChanged(helpers_JIT::CPUState::HALTED);
}

void AlphaPALInterpreter::handlePrivilegedContextSwitch()
{
    // Placeholder - In real Alpha, switch process context
    // Here you can just log or simulate a dummy switch
}

void AlphaPALInterpreter::handleSystemCall()
{
    // Raise a system call exception
    cpu->raiseException(helpers_JIT::ExceptionType::SYSTEM_CALL, cpu->getPC());
}

void AlphaPALInterpreter::handleUnknownPAL( quint32 palFunction)
{
    // Raise Illegal Instruction or Privileged Instruction Exception
    cpu->raiseException(helpers_JIT::ExceptionType::ILLEGAL_INSTRUCTION, cpu->getPC());
}

void AlphaPALInterpreter::handleWriteKernelGP()
{
	quint64 gpValue = cpu->readRegister(0); // In Alpha, R0 carries arguments
	cpu->setKernelGP(gpValue);
}

void AlphaPALInterpreter::handleWriteUserSP()
{
	quint64 spValue = cpu->readRegister(0); // Assume R0 carries new USP
	ctx.->setUserSP(spValue);
}

void AlphaPALInterpreter::handleReadUserSP()
{
	cpu->writeRegister(0, cpu->getUserSP()); // Write current USP into R0
}

void AlphaPALInterpreter::handleMachineCheck()
{
	raiseException(helpers_JIT::ExceptionType::MACHINE_CHECK, cpu->getPC());
}

void AlphaPALInterpreter::handleBusError()
{
	raiseException(helpers_JIT::ExceptionType::BUS_ERROR, cpu->getPC());
}
