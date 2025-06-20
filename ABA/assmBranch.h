#pragma once
#include "Assembler.h"

namespace assemblerSpace {
    class assmBranch :
        public Assembler
    {
        assmBranch() = default;
        ~assmBranch() = default;

		//---------------------------------
		// Conditional Branch Helpers
		//---------------------------------

		inline void emitJmp(const std::string& label) {
			// emit the 0xE9 opcode for a 32-bit rel32 jump
			emitByte(0xE9);
			// reserve the 4-byte displacement and record the fixup
			emitLabelRef(label);
		}

		inline void emitJcc(Condition cc, const std::string& label) {
			static const uint8_t opMap[6] = { 0x84, 0x85, 0x8C, 0x8E, 0x8F, 0x8D };
			// two-byte conditional-jump opcode: 0F xx
			emitByte(0x0F);
			emitByte(opMap[int(cc)]);
			emitLabelRef(label);
		}


    };

}

