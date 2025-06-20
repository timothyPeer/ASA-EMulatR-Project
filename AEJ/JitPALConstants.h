#pragma once

#pragma once
/*--------------------------------------------------------------------
 *  EvPalSelect.h  •  per-EV PAL function number selection helper
 *
 *  Build flag:
 *      -DCPU_EV={4|5|6|67|68|7}
 *
 *  If CPU_EV is omitted, EV6 is used as a default (21264 / Clipper PAL).
 *------------------------------------------------------------------*/

// ─── 1. EV selector ------------------------------------------------
#ifndef CPU_EV
#define CPU_EV 6
#endif

#if (CPU_EV == 4)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV4)
#elif (CPU_EV == 5)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV5)
#elif (CPU_EV == 6)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV6)
#elif (CPU_EV == 67)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV67)
#elif (CPU_EV == 68)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV68)
#elif (CPU_EV == 7)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV7)
#else
#error "Unknown CPU_EV value; use 4,5,6,67,68, or 7."
#endif


// ─── 2. Helper macro to declare a PAL constant --------------------
#define PALCONST(name, EV4, EV5, EV6, EV67, EV68, EV7)                                                                 \
    constexpr quint32 name = EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7)

// ─── 3.  OSF/Tru64 PAL routines whose numbers moved ---------------
// (values are from DEC/Compaq reference PALs: OSF V5.x EV4/5,
//  Clipper V6.x EV6/67/68, BWX PAL for EV56, Marvel V7.x)

// Machine-Check / Memory-Error
PALCONST(PAL_RDMCES, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13);
PALCONST(PAL_WRMCES, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14);

// Interrupt-level services
PALCONST(PAL_RDIRQL, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06);
PALCONST(PAL_SWPIRQL, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01);
PALCONST(PAL_DI, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08);
PALCONST(PAL_EI, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09);


// Context / PAL swap
PALCONST(PAL_SWPCTX, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
PALCONST(PAL_SWPPAL, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A);

// Privileged-register services
PALCONST(PAL_WRFEN, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C);
PALCONST(PAL_WRVPTPTR, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B);

// Trap-vector / TBI
PALCONST(PAL_WRENT, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03);
PALCONST(PAL_WTKTRP, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12);
PALCONST(PAL_TBI, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33);
PALCONST(PAL_TBIA, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09);
PALCONST(PAL_TBIS, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A);
//constexpr quint32 PAL_TBI = 0x33; // Translation-buffer op

// Diagnostic-value services
PALCONST(PAL_WRVAL, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07);
PALCONST(PAL_RDVAL, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08);

// UNIX/OSF extensions/PALCONST(PAL_RDPS, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98);
PALCONST(PAL_WRKGP, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D);
PALCONST(PAL_WRUSP, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E);
PALCONST(PAL_RDUSP, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F);
PALCONST(PAL_WRPERFMON, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91);
PALCONST(PAL_RDPERFMON, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90);

// …add more PALCONST lines here as you discover EV-dependent numbers …


//==============================================================================
// PAL OPERATIONS (Opcode 0x00) - 26-bit function codes
//==============================================================================

// Basic PAL operations
//constexpr quint32 PAL_HALT = 0x00;   // *Halt processor
//constexpr quint32 PAL_CFLUSH = 0x01; // *Cache flush
//constexpr quint32 PAL_DRAINA = 0x02; // *Drain aborts

// ---------- VMS-specific ----------


// ---------- OSF/1-specific (what SRM/Linux use) ----------
constexpr quint32 PAL_CSERVE = 0x09; // *Console service

constexpr quint32 PAL_RDPS = 0x36;   // Read Processor Status
constexpr quint32 PAL_WRKGPR = 0x37; // etc…

// ––– Named aliases that your code (InstructionPipeLine) expects –––
// constexpr quint32 PAL_TBIA = PAL_TBI;    // “Invalidate All” ⇢ arg = 0
// constexpr quint32 PAL_TBIS = PAL_TBI;    // “Invalidate Single” ⇢ arg = 1
constexpr quint32 PAL_BUGCHK = 0x81;     // Bug check
constexpr quint32 PAL_EXCB = 0x83;       // memory-ordering barrier (EXCB instr)
constexpr quint32 PAL_MCHK = PAL_BUGCHK; // Machine Check
constexpr quint32 PAL_LDQP = 0x03;   // Load quadword physical
constexpr quint32 PAL_STQP = 0x04;   // Store quadword physical

//

// constexpr quint32 PAL_SWPCTX =  0x04;       // Swap context
/*constexpr quint32 PAL_MFPR_ASN = 0x06;   // Move from PR address space number*/
//
/*constexpr quint32 PAL_MTPR_ASTSR = 0x08; // Move to PR AST summary*/

// constexpr quint32 PAL_SWPPAL = 0x0A;       // Swap PALcode
/*constexpr quint32 PAL_MFPR_FEN = 0x0B;     // Move from PR floating enable*/
/*constexpr quint32 PAL_MTPR_FEN = 0x0C;     // Move to PR floating enable*/
/*constexpr quint32 PAL_MTPR_IPIR = 0x0D;    // *Move to PR inter-processor interrupt (WRIPIR)*/
// constexpr quint32 PAL_MFPR_IPL = 0x0E;     // Move from PR interrupt priority level
/*constexpr quint32 PAL_MTPR_IPL = 0x0F;     // Move to PR interrupt priority level*/
/*constexpr quint32 PAL_MFPR_MCES = 0x10;    // Move from PR machine check error summary*/
/*constexpr quint32 PAL_MTPR_MCES = 0x11;    // Move to PR machine check error summary*/
/*constexpr quint32 PAL_MFPR_PCBB = 0x12;    // Move from PR process control block base*/
/*constexpr quint32 PAL_MFPR_PRBR = 0x13;    // Move from PR processor base register*/
/*constexpr quint32 PAL_MTPR_PRBR = 0x14;    // Move to PR processor base register*/
/*constexpr quint32 PAL_MFPR_PTBR = 0x15;    // Move from PR page table base register*/
/*constexpr quint32 PAL_MFPR_SCBB = 0x16;    // Move from PR system control block base*/
// constexpr quint32 PAL_MTPR_SCBB = 0x17;    // Move to PR system control block base
// constexpr quint32 PAL_MTPR_SIRR = 0x18;    // Move to PR software interrupt request
/*constexpr quint32 PAL_MFPR_SISR = 0x19;    // Move from PR software interrupt summary*/
// constexpr quint32 PAL_MFPR_TBCHK = 0x1A;   // Move from PR translation buffer check
/*constexpr quint32 PAL_MTPR_TBIA = 0x1B;    // Move to PR translation buffer invalidate all*/
/*constexpr quint32 PAL_MTPR_TBIAP = 0x1C;   // Move to PR TB invalidate all process*/
/*constexpr quint32 PAL_MTPR_TBIS = 0x1D;    // Move to PR TB invalidate single*/
/*constexpr quint32 PAL_MFPR_ESP = 0x1E;     // Move from PR executive stack pointer*/
/*constexpr quint32 PAL_MTPR_ESP = 0x1F;     // Move to PR executive stack pointer*/
/*constexpr quint32 PAL_MFPR_SSP = 0x20;     // Move from PR supervisor stack pointer*/
/*constexpr quint32 PAL_MTPR_SSP = 0x21;     // Move to PR supervisor stack pointer*/
/*constexpr quint32 PAL_MFPR_USP = 0x22;     // Move from PR user stack pointer*/
/*constexpr quint32 PAL_MTPR_USP = 0x23;     // Move to PR user stack pointer*/
/*constexpr quint32 PAL_MTPR_TBISD = 0x24;   // Move to PR TB invalidate single data*/
/*constexpr quint32 PAL_MTPR_TBISI = 0x25;   // Move to PR TB invalidate single instruction*/
/*constexpr quint32 PAL_MFPR_ASTEN = 0x26;   // Move from PR AST enable*/
/*constexpr quint32 PAL_MFPR_ASTSR = 0x27;   // Move from PR AST summary*/
/*constexpr quint32 PAL_MFPR_VPTB = 0x29;    // Move from PR virtual page table base*/
/*constexpr quint32 PAL_MTPR_VPTB = 0x2A;    // Move to PR virtual page table base*/
/*constexpr quint32 PAL_MTPR_PERFMON = 0x2B; // Move to PR performance monitor*/
/*constexpr quint32 PAL_MFPR_WHAMI = 0x3F;   // Move from PR who am I*/
constexpr quint32 PAL_TBIE = 0x3F;         // TLB Invalidate Entry
constexpr quint32 PAL_TBIM = 0x38;         // TLB Invalidate Multiple
constexpr quint32 PAL_RET = 0x6C;          // Return from PAL code

constexpr quint32 PAL_BPT = 0x80;      // Breakpoint trap

/*constexpr quint32 PAL_CHME = 0x82;     // Change mode to executive*/
/*constexpr quint32 PAL_CHMS = 0x83;     // Change mode to supervisor*/
/*constexpr quint32 PAL_CHMU = 0x84;     // Change mode to user*/
constexpr quint32 PAL_IMB = 0x86;      // I-stream memory barrier
constexpr quint32 PAL_OPCDEC = 0xB7;   // Opcode reserved for Digital
/*constexpr quint32 PAL_INSQHIL = 0x85;  // Insert entry into head interlocked longword queue*/
constexpr quint32 PAL_CALLPRIV = 0x85; // Call Privileged Instruction
/*constexpr quint32 PAL_INSQTIL = 0x86;  // Insert entry into tail interlocked longword queue*/
/*constexpr quint32 PAL_INSQHIQ = 0x87;  // Insert entry into head interlocked quadword queue*/
/*constexpr quint32 PAL_INSQTIQ = 0x88;  // Insert entry into tail interlocked quadword queue*/
/*constexpr quint32 PAL_REMQHIL = 0x89;  // Remove entry from head interlocked longword queue*/
/*constexpr quint32 PAL_REMQTIL = 0x8A;  // Remove entry from tail interlocked longword queue*/
/*constexpr quint32 PAL_REMQHIQ = 0x8B;  // Remove entry from head interlocked quadword queue*/
/*constexpr quint32 PAL_REMQTIQ = 0x8C;  // Remove entry from tail interlocked quadword queue*/
constexpr quint32 PAL_INSQHILE = 0x8D; // Insert entry into head interlocked longword queue, exit
constexpr quint32 PAL_INSQTILE = 0x8E; // Insert entry into tail interlocked longword queue, exit
constexpr quint32 PAL_INSQHIQE = 0x8F; // Insert entry into head interlocked quadword queue, exit
constexpr quint32 PAL_INSQTIQE = 0x90; // Insert entry into tail interlocked quadword queue, exit
constexpr quint32 PAL_REMQHILE = 0x91; // Remove entry from head interlocked longword queue, exit
constexpr quint32 PAL_RDDPERFMON = 0x92; // Read Detailed Performance Monitor
constexpr quint32 PAL_REMQTILE = 0x92; // Remove entry from tail interlocked longword queue, exit
constexpr quint32 PAL_REMQHIQE = 0x93; // Remove entry from head interlocked quadword queue, exit
constexpr quint32 PAL_REMQTIQE = 0x94; // Remove entry from tail interlocked quadword queue, exit
constexpr quint32 PAL_PROBEW = 0x95;   // Probe write access
constexpr quint32 PAL_PROBER = 0x96;   // Probe read access
constexpr quint32 PAL_PRIV = 0x97;     // Privileged instruction
// constexpr quint32 PAL_RDPS = 0x98;      // Read processor status
// constexpr quint32 PAL_REI = 0x99;       // Return from exception or interrupt
constexpr quint32 PAL_SWASTEN = 0x9A;   // Swap AST enable
constexpr quint32 PAL_SWPIPL = PAL_SWPIRQL; // Alias for SWPIRQL (for compatibility)
constexpr quint32 PAL_WR_PS_SW = 0x9B;  // Write processor status software field
constexpr quint32 PAL_RSCC = 0x9C;      // Read system cycle counter
constexpr quint32 PAL_READ_UNQ = 0x9E;  // Read unique value
constexpr quint32 PAL_WRITE_UNQ = 0x9F; // Write unique value
constexpr quint32 PAL_AMOVRR = 0xA0;    // Atomic move register to register
constexpr quint32 PAL_AMOVRM = 0xA1;    // Atomic move register to memory
constexpr quint32 PAL_INSQHIL_D = 0xA2; // Insert entry into head interlocked longword queue, deferred
constexpr quint32 PAL_INSQTIL_D = 0xA3; // Insert entry into tail interlocked longword queue, deferred
constexpr quint32 PAL_INSQHIQ_D = 0xA4; // Insert entry into head interlocked quadword queue, deferred
constexpr quint32 PAL_INSQTIQ_D = 0xA5; // Insert entry into tail interlocked quadword queue, deferred
constexpr quint32 PAL_REMQHIL_D = 0xA6; // Remove entry from head interlocked longword queue, deferred
constexpr quint32 PAL_REMQTIL_D = 0xA7; // Remove entry from tail interlocked longword queue, deferred
constexpr quint32 PAL_REMQHIQ_D = 0xA8; // Remove entry from head interlocked quadword queue, deferred
constexpr quint32 PAL_REMQTIQ_D = 0xA9; // Remove entry from tail interlocked quadword queue, deferred

// Console PAL operations
constexpr quint32 PAL_CONSHALT = 0xB8;    // Console halt
constexpr quint32 PAL_CONSENV = 0xB9;     // Console environment
constexpr quint32 PAL_CONSINIT = 0xBA;    // Console initialization
constexpr quint32 PAL_CONSRESTART = 0xBB; // Console restart
constexpr quint32 PAL_CONSOUT = 0xBC;     // Console output
constexpr quint32 PAL_CONSIN = 0xBD;      // Console input

// Quad/Octaword operations
constexpr quint32 PAL_LDQP_L = 0xBE; // Load quadword physical locked
constexpr quint32 PAL_STQP_C = 0xBF; // Store quadword physical conditional
constexpr quint32 PAL_LDQP_U = 0xC0; // Load quadword physical unaligned
constexpr quint32 PAL_STQP_U = 0xC1; // Store quadword physical unaligned

// PAL vector offset constants (in bytes from PAL base)
constexpr quint64 PAL_VECTOR_TRANSLATION_NOT_VALID = 0x100;
constexpr quint64 PAL_VECTOR_ACCESS_CONTROL_VIOLATION = 0x200;
constexpr quint64 PAL_VECTOR_DATA_ALIGNMENT_FAULT = 0x300;
constexpr quint64 PAL_VECTOR_FAULT_ON_READ_WRITE = 0x400;
constexpr quint64 PAL_VECTOR_MACHINE_CHECK = 0x500;
constexpr quint64 PAL_VECTOR_GENERIC_EXCEPTION = 0x600;

//==============================================================================
// PAL CONSTANTS AND DEFINITIONS
//==============================================================================

// PAL special function codes for machine-specific operations
constexpr quint32 PAL_VMS_HALT = 0x9A; // VMS specific halt
constexpr quint32 PAL_VMS_CHMK = 0x83; // VMS change mode to kernel
constexpr quint32 PAL_VMS_CHMX = 0x82; // VMS change mode to executive
constexpr quint32 PAL_VMS_CHMS = 0x83; // VMS change mode to supervisor
constexpr quint32 PAL_VMS_CHMU = 0x84; // VMS change mode to user

// UNIX/Tru64 specific PAL codes
constexpr quint32 PAL_UNIX_CALLSYS = 0x83;   // System call
constexpr quint32 PAL_UNIX_IMB = 0x86;       // Instruction memory barrier
constexpr quint32 PAL_UNIX_RDPERFMON = 0x90; // Read performance monitor
constexpr quint32 PAL_UNIX_WRPERFMON = 0x91; // Write performance monitor

