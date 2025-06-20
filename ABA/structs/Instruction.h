// Instruction.h
#pragma once
#include <cstdint>

namespace Arch {

	/// The 14 supported formats
	enum class FormatID : uint8_t {
		ALPHA_MEM,         // Alpha Memory format
		ALPHA_MEMFCT,         // Alpha Memory format
		ALPHA_BRANCH,      // Alpha Branch format
		ALPHA_OPERATE,     // Alpha Operate format
		ALPHA_OPERATE_VECTOR,     // Alpha Operate format
		ALPHA_FP_OPERATE,  // Alpha Floating-point Operate
		ALPHA_PAL,         // Alpha PALcode
		ALPHA_CONSOLE,     // Alpha Console PAL
		ALPHA_OSF1,        // Alpha OSF/1 PAL
		VAX_PAL,           // VAX PALcode
		VAX_FP,            // VAX Floating-point PAL
		ALPHA_VECTOR,      // Vector - Floating Point
		TRU64_PAL          // Tru64 PALcode
	};

	/// Base class for any decoded instruction
	struct Instruction {
		virtual ~Instruction() = default;
		/// Which dispatch table to use
		virtual FormatID format() const = 0;
		/// Index into that table (usually the 16-bit “opcode||fnc” or just opcode)
		virtual uint16_t getCode() const = 0;
	};

} // namespace Arch
