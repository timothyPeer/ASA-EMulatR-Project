#include "AlphaInstruction.h"

std::unique_ptr<AlphaInstruction> AlphaInstruction::Create(quint32 rawInstr) {
	quint32 opcode = (rawInstr >> 26) & 0x3F;
	quint32 function = 0;

	// Extract function based on format
	if (opcode >= 0x10 && opcode <= 0x1F) {
		function = (rawInstr >> 5) & 0x7F;
	}
	else if (opcode == 0x16 || opcode == 0x17) {
		function = (rawInstr >> 5) & 0x7FF;
	}

	// First check for memory operations
	if (opcode == OPCODE_LDQ) {
		return std::make_unique<LdqInstruction>(rawInstr);
	}
	else if (opcode == OPCODE_LDQ_U) {
		return std::make_unique<LdqUnalignedInstruction>(rawInstr);
	}
	else if (opcode == OPCODE_STQ) {
		return std::make_unique<StqInstruction>(rawInstr);
	}

	// Integer operations
	if (opcode == 0x10) {
		switch (function) {
		case FUNC_ADDQ: return std::make_unique<AddqInstruction>(rawInstr);
		case FUNC_SUBQ: return std::make_unique<SubqInstruction>(rawInstr);
			// Other integer operations...
		}
	}

	// Logical operations
	if (opcode == 0x11) {
		switch (function) {
		case FUNC_AND: return std::make_unique<AndInstruction>(rawInstr);
		case FUNC_BIS: return std::make_unique<BisInstruction>(rawInstr);
			// Other logical operations...
		}
	}

	// FP operations
	if (opcode == 0x16) {
		switch (function) {
		case FUNC_ADDT: return std::make_unique<FPAddTInstruction>(rawInstr);
		case FUNC_SUBT: return std::make_unique<FPSubTInstruction>(rawInstr);
			// Other FP operations...
		}
	}

	// Branch operations
	if (opcode >= 0x30 && opcode <= 0x3F) {
		return std::make_unique<ConditionalBranchInstruction>(rawInstr);
	}

	// PAL operations
	if (opcode == 0x00) {
		return std::make_unique<PALInstruction>(rawInstr);
	}

	// Fallback for unhandled instructions
	return std::make_unique<FallbackInstruction>(rawInstr);
}