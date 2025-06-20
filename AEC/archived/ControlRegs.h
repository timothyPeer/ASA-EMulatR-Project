#pragma once

/*
six privileged control-processor registers every Alpha AXP implementation exposes, together with (1) a short architectural description and (2) a ready-to-drop-into-your-emulator C++ struct that models them.

Mnemonic		Size	Privilege		Architectural purpose
PC				64 b	all modes		Program Counter – holds the virtual address of the instruction being fetched. Hardware increments by 4 each cycle; PALcode can read/write it directly for traps and returns.
PS				64 b	kernel / PAL	Processor-Status Register – controls current mode (user/supervisor/kernel), interrupt priority level (IPL), interrupt enable bits, FEN (floating-point enable), Software-Interrupt Summary, etc.
FPCR			64 b	all modes		Floating-Point Control / Status – rounding mode, exception enables, sticky flags. Present in the F31 register bank for user code; PAL can also access it via M[TF]PR.
UNIQUE			64 b	all modes		Per-process unique value register. OS sets to a thread-local base (e.g. pthread self) so user code can obtain TLS via LDQ Rn,0(UNIQUE). No hardware meaning.
LOCK_FLAG		64 b	PAL only		Scratch register used by the Load-Locked / Store-Conditional sequence in PALcode to track memory reservation. Not architecturally visible to user mode.
CYCLE_COUNTER	64 b	PAL only (read-only to kernel on EV6)	
										Free-running cycle counter; increments every core clock. Used for profiling, scheduling, delays.


*/

// ControlRegs.h
#pragma once
#include <cstdint>
#include "FpRegisterFile.h"    // fpcr struct we built earlier

struct ControlRegs {
	uint64_t PC = 0;
	uint64_t PS = 0x0400000000000000ULL;   // reset: IPL=31, kernel
	FpcrRegister FPCR;                             // overlay as helper
	uint64_t UNIQUE = 0;
	uint64_t LOCK_FLAG = 0;
	uint64_t CYCLE_CNT = 0;

	// helpers ----------------------------------------------------------
	void advancePC() { PC += 4; }

	enum Mode { User = 0, Super = 1, Kernel = 2, PAL = 3 };
	Mode mode() const { return Mode((PS >> 3) & 0b11); }
	void setMode(Mode m) { PS = (PS & ~(0b11ULL << 3)) | (uint64_t(m) << 3); }

	uint8_t  IPL() const { return uint8_t((PS >> 8) & 0x1F); }
	void setIPL(uint8_t l) { PS = (PS & ~(0x1FULL << 8)) | (uint64_t(l & 0x1F) << 8); }

	bool intsEnabled() const { return !(PS & (1ULL << 7)); }
	void setIntsEnabled(bool e) { if (e) PS &= ~(1ULL << 7); else PS |= (1ULL << 7); }
};
