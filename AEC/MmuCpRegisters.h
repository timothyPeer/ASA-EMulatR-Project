// MmuCpRegisters.h
#pragma once
#include <cstdint>

/*
“MMU & interrupt register map” for every control register named in the Alpha System Architecture plus a ready‑to‑embed C++ structure that models them exactly as PALcode and the OS see them.


Register (mnemonic)		Width		Access level	Purpose / when it is used
TB_ISN / TB_ISA			64 b			PAL only		Instruction TLB Insert Number and Address. PALcode writes ISN with a virtual page number (VPN) plus ASn; writes ISA with the PTE; executes TBIS to insert into I‑TB.
TB_DSN / TB_DSA			64 b			PAL only		Same pair for the Data TLB (TBDS).
TB_TAG					64 b			PAL only		Holds the tag result of a preceding TBPT (probe) or TBI miss; PAL reads it to decide miss vs. hit.
MM_STAT (MMCSR)			64 b			kernel & PAL	Memory‑management status: fault type, faulting level, protection bits, and reserved‑instruction indicator. Written by hardware on any MMU exception.
									(read‑only)	
VA						64 b			kernel & PAL	The virtual address that caused the most recent MMU or access fault.
									(read‑only)	
DTB_PTE / ITB_PTE		64 b			PAL only		Hardware copies the faulting Page Table Entry here after a DTB/ITB miss. PALcode may patch or validate before reinsertion.
DTB_ASN / ITB_ASN		8 b in MSR	kernel & PAL	Current Address‑Space Number for Data/Instruction TLB. OS writes on context switch; hardware appends to tag comparators.
PCBB					64 b			kernel			Process Control Block Base (physical address) – OS sets this to point to a per‑process PCB; PALcode uses it during context‑switch instructions.
PTBR					64 b			kernel			Page Table Base Register – physical page containing Level‑1 page table. MMU walks page tables relative to this base.
ASN						8 b			kernel			Short alias of DTB/ITB ASN when CPU implements a single shared ASN register (EV4/EV5). EV6 splits into separate DTB_ASN/ITB_ASN.
SISR					64 b			kernel & PAL	Software‑Interrupt Summary Register – bits correspond to AST, MTPR(SIR), etc. OS sets to trigger soft IRQs; hardware copies to PS.SW after trap entry.
IER						64 b			kernel			Interrupt Enable Register – per‑device or per‑vector mask of external interrupts routed to the CPU.
IPIR					64 b			kernel			Inter‑Processor Interrupt Register – writing a bit sends an interrupt to the corresponding CPU in SMP systems.
									(MP only)

*/

struct MmuCpRegs
{
	/* --- TLB Insert / Probe --- */
	uint64_t TB_ISN = 0;   ///< Instr. TB insert number
	uint64_t TB_ISA = 0;   ///< Instr. TB insert address/PTE
	uint64_t TB_DSN = 0;   ///< Data  TB insert number
	uint64_t TB_DSA = 0;   ///< Data  TB insert address/PTE
	uint64_t TB_TAG = 0;   ///< Result tag from TB probe

	/* --- Fault status path --- */
	uint64_t MM_STAT = 0;  ///< MMCSR – fault reason bits
	uint64_t VA = 0; ///< Faulting virtual address
	uint64_t ITB_PTE = 0; ///< Auto-filled PTE on ITB miss
	uint64_t DTB_PTE = 0; ///< Auto-filled PTE on DTB miss

	/* --- Address-space / context registers --- */
	uint8_t  ITB_ASN = 0;
	uint8_t  DTB_ASN = 0;   ///< On EV4/EV5 treat these aliases of ASN
	uint64_t PCBB = 0;   ///< Process-control block base (phys)
	uint64_t PTBR = 0;   ///< Page-table base (phys)
	uint8_t  ASN = 0;   ///< Unified ASN (older cores)

	/* --- Interrupt registers --- */
	uint64_t SISR = 0;   ///< Soft-int summary / AST
	uint64_t IER = 0;   ///< External-IRQ enable mask
	uint64_t IPIR = 0;   ///< Inter-processor interrupt
};
