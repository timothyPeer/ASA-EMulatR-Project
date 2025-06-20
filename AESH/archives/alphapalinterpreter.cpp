#include "AlphaPALInterpreter.h"
#include "AlphaCPU.h"
#include "JitPALConstants.h"
#include "helpers.h"

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
		ctx->raiseTrap(static_cast<int>(helpers_JIT::Fault_TrapType::Breakpoint));  // or a custom TrapType::Halt
		break;

	case PAL_MCHK:
		qCritical() << "[PALInterpreter] Machine Check Exception (PAL_MCHK) at PC:"
			<< QString("0x%1").arg(ctx->getPC(), 8, 16, QLatin1Char('0'));
		ctx->raiseTrap(static_cast<int>(helpers_JIT::Fault_TrapType::MachineCheck));
		system.fatalBugCheck(op.rand0());   // etc.
		enterMachineCheckVector();
		break;

	case PAL_BPT:
		qInfo() << "[PALInterpreter] Breakpoint (PAL_BPT) at PC:"
			<< QString("0x%1").arg(ctx->getPC(), 8, 16, QLatin1Char('0'));
		ctx->raiseTrap(static_cast<int>(helpers_JIT::Fault_TrapType::Breakpoint));
		break;

	//case PAL_BUGCHK:
	

	case PAL_WRKGP: {
		quint64 value = ctx->readIntReg(0);  // Typically, PAL arguments are in R0
		qInfo() << "[PALInterpreter] WRKGP – Writing Kernel GP:"
			<< QString("0x%1").arg(value, 8, 16, QLatin1Char('0'));
		ctx->writeIntReg(ctx->getRegisterBank()->getKernelGP(), value);  // Replace with actual kernel GP index

		quint64 tmpVal = ctx->getRegisterBank()->getKernelGP();

		ctx->notifyRegisterUpdated(false, tmpVal, value);
		break;
	}

	case PAL_WRUSP:
		handleWriteUserSP();
		break;

	case PAL_RDUSP:
		handleReadUserSP();
		break;

	case PAL_UNIX_CALLSYS:
		handleSystemCall();
		break;

		// Placeholders for future implementation
	case PAL_WRPERFMON:
	case PAL_RDDPERFMON:
	case PAL_IMB:
	case PAL_REI:
	case PAL_RET:
	case PAL_CALLPRIV:
	case PAL_READ_UNQ:
	case PAL_WRITE_UNQ:
	case PAL_TBI:
	{
		const uint64_t tbiMode = m_cpu->r16();   // R16 carries the “which” value
		if (tbiMode == 0) {          // TBIA
			m_tlbSystem->invalidateAll();
		}
		else if (tbiMode == 1) {   // TBIS
			m_tlbSystem->invalidatePage(a0 /*virtual address in R16*/);
		}
		break;
	}
	case PAL_TBIM:
	case PAL_TBIE:
	case PAL_SWPPAL:
	case PAL_SWPIPL:
	case PAL_WR_PS_SW:
	case PAL_WRVPTPTR:
	case PAL_SWASTEN:
	
	case PAL_MFPR_ASTEN:
	


	case PAL_RDPS:            // Read Processor Status (PS)
		result = m_cpu->getProcessorStatus();
		break;

	case PAL_DRAINA:          // Drain abort queues
		m_tlbSystem->waitForPendingAborts();
		break;

	case PAL_MTPR_ASTEN:      // Enable ASTs (priv-reg write)
		m_cpu->setASTEnable(a0 & 1);
		break;

	case PAL_EXCB:            // Lightweight ordering barrier
		m_cpu->memoryBarrier(Barrier::Excb);
		qInfo() << "[PALInterpreter] PAL function 0x"
			<< QString::number(palFunctionCode, 16)
			<< " is unimplemented.";
		ctx->raiseTrap(static_cast<int>(ReservedInstruction));
		break;


	default:
		qWarning() << "[PALInterpreter] Unknown PAL function:"
			<< QString::number(palFunctionCode, 16);
		ctx->notifyIllegalInstruction(palFunctionCode, ctx->getPC());
		break;
	}
}


void AlphaPALInterpreter::raiseException(ExceptionType, quint64 pc_)
{
	// TODO
}

// CPU Specific handlers for halt conditions
void AlphaPALInterpreter::handleHalt()
{
   ctx->notifySetState(CPUState::HALTED);
    emit ctx->notifyExecutionStopped();
    emit ctx->notifyStateChanged(CPUState::HALTED);
}

void AlphaPALInterpreter::handlePrivilegedContextSwitch()
{
    // Placeholder - In real Alpha, switch process context
    // Here you can just log or simulate a dummy switch
}

void AlphaPALInterpreter::handleSystemCall()
{
    // Raise a system call exception
    ctx->notifyRaiseException(ExceptionType::SYSTEM_CALL, ctx->getPC());
}

void AlphaPALInterpreter::handleUnknownPAL( quint32 palFunction)
{
    // Raise Illegal Instruction or Privileged Instruction Exception
    ctx->notifyRaiseException(ExceptionType::ILLEGAL_INSTRUCTION, ctx->getPC());
}

void AlphaPALInterpreter::handleWriteKernelGP()
{
	quint64 gpValue = ctx->readIntReg(0); // In Alpha, R0 carries arguments
	ctx->notifySetKernelSP(gpValue);
}

void AlphaPALInterpreter::handleWriteUserSP()
{
	quint64 spValue = ctx->readRegister(0); // Assume R0 carries new USP
	ctx->notifySetUserSP(spValue);
}

void AlphaPALInterpreter::handleReadUserSP()
{
	ctx->writeRegister(0, ctx->getUserSP()); // Write current USP into R0
}

void AlphaPALInterpreter::handleMachineCheck()
{
	raiseException(ExceptionType::MACHINE_CHECK, ctx->getPC());
}

void AlphaPALInterpreter::handleBusError()
{
	raiseException(ExceptionType::BUS_ERROR, ctx->getPC());
}
