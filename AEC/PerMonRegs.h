
// PerfMonRegs.h
#pragma once
#include <cstdint>

/*

 performance‑monitor / miscellaneous‑control registers that appeared in the EV5/EV6 Alpha implementations, 
 together with concise architectural notes and a ready‑to‑embed C++ struct so your emulator can model them.


Register		Width				Access privilege	Purpose
PERFMON_CTL0	64 bit				kernel / PAL		Select event source and qualifiers for counter 0 (monitor unit 0).
PERFMON_CTL1	64 bit				kernel / PAL		Same for counter 1 (monitor unit 1).
PERFMON_CNT0	48 bit				kernel / PAL		Event counter #0. Increments every cycle in which the chosen event occurs; saturates at 0x FFFFFFFFFFFF.
				active field in a	(read / write)		
				64‑bit register		
PERFMON_CNT1	48 bit				kernel / PAL		Event counter #1.
MISC_CTL		64 bit				kernel / PAL		Miscellaneous control bits: enable/disable I‑cache parity, BTB, branch‐stack, modify fill‑buffer behaviour, etc. Some bits are implementation‑specific.
On EV5/21164 the counters are 32 bit; EV6 (21264) widened them to 48 bit but still map in a 64‑bit MSR.

Writing PERFMON_CNTn with any value clears (zeroes) the counter.
*/

struct PerfMonRegs
{
	uint64_t CTL0 = 0;   ///< event‑select for counter 0
	uint64_t CTL1 = 0;   ///< event‑select for counter 1
	uint64_t CNT0 = 0;   ///< 48‑bit up‑counter
	uint64_t CNT1 = 0;   ///< 48‑bit up‑counter
	uint64_t MISC = 0;   ///< miscellaneous CPU‑control bits

	/* ---- helper functions ------------------------------------------ */
	void reset() { CNT0 = CNT1 = 0; }

	/* Increment an event counter with saturation to 48 bits */
	void incrCnt0(uint64_t val = 1)
	{
		CNT0 = (CNT0 + val) & 0x0000FFFFFFFFFFFFULL;
	}
	void incrCnt1(uint64_t val = 1)
	{
		CNT1 = (CNT1 + val) & 0x0000FFFFFFFFFFFFULL;
	}
};
