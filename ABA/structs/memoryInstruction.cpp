#include <stdafx.h>
#include "memoryInstruction.h"

#include "../AEJ/AlphaCPU_refactored.h"
#include "../AEJ/SafeMemory_refactored.h"
#include "../AEJ/IExecutionContext.h"


void Arch::MemoryInstruction::emitAlphaLDQ_L(const MemoryInstruction& i, AlphaCPU* cpu, RegisterBank* regs, AlphaProcessorContext* ctx)
{
	uint64_t va = i.computeAddress(regs->readIntReg(i.rb));
	uint64_t value = 0;
	if (!loadMem(ctx, va, 8, value)) return;

	regs->writeIntReg(i.ra, value);

	// Set reservation address in the owning CPU
	if (cpu != nullptr) {
		cpu->setReservation(va);
	}

	ctx->advancePC();
}

/**
 * [9.2.2] STQ_C - Store Quadword Conditional
 * Stores a value only if the reservation is still valid.
 * Writes 1 to rc if the store was successful, otherwise 0.
 */
void Arch::MemoryInstruction::emitAlphaSTQ_C(const MemoryInstruction& i, AlphaCPU* cpu, RegisterBank* regs, AlphaProcessorContext* ctx)
{

	uint64_t va = i.computeAddress(regs->readIntReg(i.rb));
	uint64_t data = regs->readIntReg(i.ra);

    if (cpu == nullptr) return;
	if (!cpu || !cpu->isReservationValid() || cpu->getReservationAddress() != va) {
		// Failed — write failure code back to Ra
		regs->writeIntReg(i.ra, 0);
		if (cpu) 
			cpu->clearReservation();
		ctx->advancePC();
		return;
	}
	if (!cpu || !cpu->isReservationValid() || cpu->getReservationAddress() != va) {
		// Failed — write failure code back to Ra
		regs->writeIntReg(i.ra, 0);
		if (cpu) cpu->clearReservation();
		ctx->advancePC();
		return;
	}
}