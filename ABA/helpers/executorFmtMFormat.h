#pragma once
// =============================================================================
// executorFmtMFormat Emit Helper
// =============================================================================
// Unified emitter for all M-format (byte/bit manipulation) instructions in
// the Alpha AXP (opcode group 0x12).
// This file assumes MFormatInstruction is defined in its own header.
// M-format layout: [opcode:6][rd:5][ra:5][width:6][position:6]
// Based on Alpha AXP Architecture Reference Manual, Fourth Edition,
// Appendix C.7 (Byte/Bit Manipulation Instructions) cite ASA-C7

#include "../Assembler.h"


/// MFormatInstruction holds the decoded fields for all "M-format" instructions
/// such as EXT, INS, and MSK variants.
struct MFormatInstruction {
	uint8_t opcode; ///< Primary opcode (6 bits, should be 0x12)
	uint8_t rd;     ///< Destination register index (5 bits)
	uint8_t ra;     ///< Source register index      (5 bits)
	uint8_t width;  ///< Field width in bits        (6 bits)
	uint8_t pos;    ///< Starting bit position      (6 bits)
};

	/// Emit M-format instructions (Ext, Ins, Msk) using decoded fields.
	class executorFmtMFormat {
	private:
		Assembler& assembler;  ///< Reference to shared Assembler instance

	public:
		/// Construct with an existing Assembler reference
		explicit executorFmtMFormat(Assembler& a)
			: assembler(a) {
		}

		/// Emit a decoded M-format instruction.
		/// The MFormatInstruction struct should be defined in MFormatInstruction.h:
		///   struct MFormatInstruction {
		///       uint8_t opcode;    // primary opcode (6 bits)
		///       uint8_t rd;        // destination register (5 bits)
		///       uint8_t ra;        // source register (5 bits)
		///       uint8_t width;     // field width in bits (6 bits)
		///       uint8_t pos;       // start bit position (6 bits)
		///   };
		inline void emitMFormat(const MFormatInstruction& op) {
			assembler.emitBits(op.opcode, 6);  // opcode
			assembler.emitBits(op.rd & 0x1F, 5);  // rd
			assembler.emitBits(op.ra & 0x1F, 5);  // ra
			assembler.emitBits(op.width & 0x3F, 6);  // width
			assembler.emitBits(op.pos & 0x3F, 6);  // position
			assembler.flushBits();                 // align to next instruction
		}
	};


