#include "AlphaCPU.h"
#include "../AEU/StackManager.h"
#include "../AEJ/enumerations/enumExceptionTypeArithmetic.h"
#include "../AEJ/enumerations/enumProcessorMode.h"
#include "../AEJ/enumerations/enumCpuState.h"





/**
	* @brief Deliver an exception into the CPU.
	*
	* ASA Vol 1 §6.7.3 requires that on entry to an exception:
	* 1) a hardware-saved frame (ExceptionFrame) be pushed,
	* 2) full context spilled only if scheduling occurs.
	*/
void AlphaCPU::dispatchException(ExceptionType type, quint64 faultAddr) {
	// Build & push the ExceptionFrame per ASA §6.7.3
	FrameHelpers::pushTrapFrame(
		*m_stackManager,
		/*pc=*/m_pc,
		/*ps=*/getProcessorStatus(),
		/*excSum=*/static_cast<quint64>(type),
		/*gpr=*/m_registerBank->rawInt(),
		/*fpcr=*/getFPCR()
	);

	// … now jump into PAL vector …
}


// Inline implementations
inline void AlphaCPU::returnFromException() {
	// 1) Get the top frame from the StackManager
	auto maybeFrame = m_stackManager->top();             // std::optional<StackFrame>
	if (!maybeFrame) {
		handleDoubleFault();
		return;
	}
	StackFrame frame = *maybeFrame;

	// 2) Pop the frame
	m_stackManager->popFrame();

	// 3) Validate the saved PS
	ProcessorStatus oldPS = getProcessorStatus();
	ProcessorStatus newPS = frame.hwFrame.ps;
	if (!isValidPS(newPS, oldPS)) {
		// Illegal PS transition ⇒ re‐trap
		dispatchException(ExceptionType::ILLEGAL_OPERAND, m_pc);
		return;
	}

	// 4) Switch the user/kernel/etc. stack pointers
	switchStack(newPS, oldPS);

	// 5) If a full SavedContext was allocated, restore registers
	if (frame.savedCtx) {
		SavedContext& ctx = *frame.savedCtx;
		for (int i = 0; i < 32; ++i)
			m_registerBank->writeIntReg(i, ctx.intRegs[i]);
		// (Floating‐point registers restored elsewhere if needed)
	}

	// 6) Restore PS and PC
	setProcessorStatus(newPS);
	m_pc = frame.hwFrame.pc;

	// 7) Before next fetch, check for any now‐enabled interrupts
	if (interruptsPending() && isInterruptEnabled(newPS)) {
		dispatchInterrupt();
		return;
	}

	// 8) Clear exception flags so normal execution resumes
	m_inExceptionHandler = false;
	clearExceptionState();
}


inline bool AlphaCPU::isValidPS(ProcessorStatus newPS, ProcessorStatus /*oldPS*/) const {
	// TODO: enforce IPL restrictions, mode‐change rules
	return true;
}


inline void AlphaCPU::switchStack(ProcessorStatus newPS, ProcessorStatus oldPS) {
	// Extract mode bits (CM fields are bits [4:3] of PS)
	auto modeOf = [](ProcessorStatus ps) {
		return static_cast<ProcessorMode>((ps >> 3) & 0x3);
		};
	ProcessorMode oldMode = modeOf(oldPS);
	ProcessorMode newMode = modeOf(newPS);

	// Save R30 to the old mode's IPR
	quint64 sp = m_registerBank->readIntReg(30);
	switch (oldMode) {
	case ProcessorMode::USER:       m_iprs->write(Ipr::USP, sp); break;
	case ProcessorMode::SUPERVISOR: m_iprs->write(Ipr::SSP, sp); break;
	case ProcessorMode::EXECUTIVE:  m_iprs->write(Ipr::ESP, sp); break;
	case ProcessorMode::KERNEL:     m_iprs->write(Ipr::KSP, sp); break;
	}

	// Load the new SP from its IPR
	quint64 newSP = 0;
	switch (newMode) {
	case ProcessorMode::USER:       newSP = m_iprs->read(Ipr::USP); break;
	case ProcessorMode::SUPERVISOR: newSP = m_iprs->read(Ipr::SSP); break;
	case ProcessorMode::EXECUTIVE:  newSP = m_iprs->read(Ipr::ESP); break;
	case ProcessorMode::KERNEL:     newSP = m_iprs->read(Ipr::KSP); break;
	}
	m_registerBank->writeIntReg(30, newSP);
}


inline bool AlphaCPU::interruptsPending() const {
	// TODO: read SIRR/IPIR from m_iprs to detect software/hardware interrupts
	return false;
}


inline bool AlphaCPU::isInterruptEnabled(ProcessorStatus ps) const {
	return (ps & PS_INTERRUPT_ENABLE) != 0;
}


inline void AlphaCPU::dispatchInterrupt() {
	dispatchException(ExceptionType::INTERRUPT, m_pc);
}
quint64 AlphaCPU::getPC() const
{
	throw std::logic_error("The method or operation is not implemented.");
}



void AlphaCPU::writeIntReg(unsigned idx, uint64_t value)
{
	throw std::logic_error("The method or operation is not implemented.");
}

double AlphaCPU::readFpReg(unsigned idx)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::writeFpReg(unsigned idx, double value)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::writeRegister(unsigned idx, uint64_t value)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool AlphaCPU::readMemory(uint64_t addr, void* buf, size_t size)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool AlphaCPU::writeMemory(uint64_t addr, void* buf, size_t size)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::raiseTrap(int trapCode)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t rawValue)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::notifyTrapRaised(quint64 type)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::notifyReturnFromTrap()
{
	throw std::logic_error("The method or operation is not implemented.");
}
void AlphaCPU::notifyRaiseException(ExceptionType eType_, quint64 pc)
{
	throw std::logic_error("The method or operation is not implemented.");
}
void AlphaCPU::notifySetState(CPUState state_)
{
	throw std::logic_error("The method or operation is not implemented.");
}
void AlphaCPU::notifySetRunning(bool bIsRunning /*= false*/)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::notifySetKernelSP(quint64 gpVal)
{
	throw std::logic_error("The method or operation is not implemented.");
}
void AlphaCPU::notifySetUserSP(quint64 usp_)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::notifyStateChanged(CPUState newState_)
{
	throw std::logic_error("The method or operation is not implemented.");
}

quint64 AlphaCPU::readRegister(quint8 index) const
{
	throw std::logic_error("The method or operation is not implemented.");
}

void AlphaCPU::returnFromException()
 {
// 	// Get the top frame from the stack manager
// 	auto topFrame = m_stackManager->top();
// 
// 	if (!topFrame) {
// 		// Error handling - no frame to return from
// 		DEBUG_LOG("Error: Attempted to return from exception with no frame");
// 		handleDoubleFault();
// 		return;
// 	}
// 
// 	// Pop the frame from the stack
// 	m_stackManager->pop();
// 
// 	// Restore processor status from the frame
// 	setProcessorStatus(topFrame->hwFrame.ps);
// 
// 	// Re-enable CPU features if appropriate
// 	restoreCpuFeatures();
// 
// 	// Jump back to interrupted instruction (PC + 4 since the exception happened at the current instruction)
// 	m_pc = topFrame->hwFrame.pc + 4;
// 
// 	// Clear exception state
// 	m_inExceptionHandler = false;
// 	clearExceptionState();
// 
// 	DEBUG_LOG(QString("Returned from exception handler, PC=0x%1")
// 		.arg(m_pc, 16, 16, QChar('0')));
	
    // 1) Pop the trap frame
	auto maybeFrame = m_stackManager->top();
	if (!maybeFrame)
	{
		handleDoubleFault();
		return;
	}
	auto frame = *maybeFrame;
	m_stackManager->popFrame();

	// 2) Validate the popped PS
	ProcessorStatus newPS = frame.hwFrame.ps;
	if (!isValidPS(newPS, /*oldPS=*/getProcessorStatus())) {
		// re-trap as Illegal Operand
		dispatchException(ExceptionType::ILLEGAL_OPERAND, /*faultAddr=*/m_pc);
		return;
	}


	// 3) Switch stacks via IPRs

	switchStack(frame.hwFrame.ps, getProcessorStatus());

	// 4) Restore R2–R7
	for (int r = 2; r <= 7; ++r)
		m_registerBank->writeIntReg(r, frame.savedRegs[r]);

	// 5) Restore PS & PC
	setProcessorStatus(newPS);
	m_pc = frame.hwFrame.pc;          // or +4 if your frame saved pre-increment PC


	// 6) Handle any pending interrupts/AST before next fetch
	if (interruptsPending() && isInterruptEnabled(newPS)) {
		dispatchInterrupt();
		return;
	}

	// 7) Clear in-exception flags
	m_inExceptionHandler = false;
	clearExceptionState();

	
}
