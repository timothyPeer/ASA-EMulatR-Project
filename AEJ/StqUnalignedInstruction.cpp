#include "StqUnalignedInstruction.h"

#include "StqUnalignedInstruction.h"
#include "TraceManager.h"
#include "GlobalMacro.h"
#include "MemoryAccessException.h"
#include "InsertMaskInstruction.h" // For fused operations with INSERT/MASK
#include "JitFunctionConstants.h"
#include "InsertInstruction.h"

StqUnalignedInstruction::StqUnalignedInstruction(quint32 rawInstr)
	: StoreInstruction(rawInstr) {
	ParseOperands();
	TRACE_LOG(QString("Created STQ_U instruction: ra=%1, disp=%2, rc=%3")
		.arg(m_ra).arg(m_immediate).arg(m_rc));
}

void StqUnalignedInstruction::Execute(RegisterFileWrapper* regs, SafeMemory* mem, TLBSystem* tlb) {
	// Get address from base register + displacement
	quint64 baseAddr = regs->readIntReg(m_ra);
	quint64 address = baseAddr + SEXT16(m_immediate);

	// Get value to store from source register
	quint64 value = regs->readIntReg(m_rc);

	// For STQ_U, align the address down to nearest quadword boundary
	quint64 alignedAddr = address & ~0x7ULL;

	DEBUG_LOG(QString("STQ_U: Storing 0x%1 to aligned address 0x%2 (original=0x%3)")
		.arg(value, 16, 16, QChar('0'))
		.arg(alignedAddr, 16, 16, QChar('0'))
		.arg(address, 16, 16, QChar('0')));

	try {
		// STQ_U requires a read-modify-write sequence
		// First, read the current value at the aligned address
		quint64 currentValue = mem->readUInt64(alignedAddr);

		// Calculate the byte offset from the aligned address
		int byteOffset = address & 0x7;

		// Create a mask for the bytes we need to preserve
		quint64 preserveMask;
		if (byteOffset == 0) {
			// If perfectly aligned, replace entire quadword
			preserveMask = 0;
		}
		else {
			// Otherwise, preserve bytes that would extend past the addressed location
			// This creates a mask with 1's for bytes to preserve
			preserveMask = ~0ULL << ((8 - byteOffset) * 8);
		}

		// Combine the current value (preserved bytes) with the new value
		quint64 newValue = (currentValue & preserveMask) | (value & ~preserveMask);

		// Write the combined value back to memory
		mem->writeUInt64(alignedAddr, newValue);
	}
	catch (const MemoryAccessException& e) {
		// Let the exception propagate upward for handling by AlphaJITCompiler
		ERROR_LOG(QString("STQ_U access exception: %1 at address 0x%2")
			.arg(e.what()).arg(e.getAddress(), 16, 16, QChar('0')));
		throw;
	}
}

void StqUnalignedInstruction::ParseOperands() {
	// Extract the register fields and immediate from the raw instruction
	m_ra = (m_rawInstr >> 21) & 0x1F;      // Base register (bits 25-21)
	m_rc = (m_rawInstr >> 0) & 0x1F;       // Value register (bits 4-0)
	m_immediate = m_rawInstr & 0xFFFF;     // Displacement (bits 15-0)

	// Sign extend the 16-bit displacement to 64 bits
	if (m_immediate & 0x8000) {
		m_immediate |= 0xFFFFFFFFFFFF0000ULL;
	}
}

void StqUnalignedInstruction::HandleExceptions(RegisterFileWrapper* regs, quint64 pc) {
	// STQ_U can throw:
	// - Access violations
	// - Translation not valid
	// But NOT alignment faults (it handles unaligned addresses)
	// Exception handling is done at the JIT compiler level
}

bool StqUnalignedInstruction::CanFuseWith(const AlphaInstruction* next) const {
	// STQ_U is commonly used in unaligned memory access sequences
	// Often followed by insert/mask operations for byte/word operations
	if (const InsertInstruction* insertOp = dynamic_cast<const InsertInstruction*>(next)) {
		// If insert uses same registers, can be fused
		if (insertOp->GetRa() == m_rc) {
			return true;
		}
	}
	else if (const MaskInstruction* maskOp = dynamic_cast<const MaskInstruction*>(next)) {
		// If mask uses registers related to our operation, can be fused
		if (maskOp->GetRa() == m_rc) {
			return true;
		}
	}
	return false;
}

AlphaInstruction* StqUnalignedInstruction::CreateFused(const AlphaInstruction* next) const {
	// We could create a FusedUnalignedStoreInstruction here
	// But for now, return nullptr to indicate fusion isn't implemented yet
	return nullptr;
}