#pragma once

/**
 * @file enumIPRNumbers.h
 * @brief Alpha AXP Internal Processor Register (IPR) Numbers
 *
 * Updated to include all necessary IPRs for PAL executor and exception handling.
 * Maintains compatibility with existing IprBank implementation.
 */

enum class IPRNumbers
{
    // === Core Processor Registers (0-63) ===
    IPR_PAL_BASE = 3,  // PALcode Base Address Register
    IPR_PS = 4,        // Processor Status Register
    IPR_FEN = 5,       // Floating-Point Enable Register
    IPR_IPIR = 6,      // Inter-Processor Interrupt Request Register
    IPR_IPL = 7,       // Interrupt Priority Level
    IPR_ASN = 8,       // Address Space Number Register
    IPR_ASTSR = 9,     // AST Status Register
    IPR_ASTEN = 10,    // AST Enable Register
    IPR_SIRR = 11,     // Software Interrupt Request Register
    IPR_IPLR = 12,     // IPL Register (duplicate of IPL for compatibility)
    IPR_VPTB = 13,     // Virtual Page Table Base
    IPR_USP = 14,      // User Stack Pointer
    IPR_KSP = 15,      // Kernel Stack Pointer
    IPR_SSP = 16,      // System Stack Pointer
    IPR_ESP = 17,      // Executive Stack Pointer
    IPR_SCBB = 18,     // System Control Block Base
    IPR_SISR = 19,     // Software Interrupt Summary Register
    IPR_PRBR = 20,     // Processor Base Register
    IPR_PTBR = 21,     // Page Table Base Register
    IPR_PCBB = 22,     // Process Control Block Base
    IPR_MCES = 23,     // Machine Check Error Summary
    IPR_TBCHK = 24,    // Translation Buffer Check
    IPR_WHAMI = 25,    // Who Am I register
    IPR_UNQ = 26,      // Unique register
    IPR_THREAD = 27,   // Thread ID register
    IPR_PAL_MODE = 28, // PAL Mode register
    IPR_IRQL = 29,     // Interrupt Request Level
    IPR_PAL_TEMP = 30, // PAL Temporary register

    // === Missing Standard IPRs ===
    IPR_AST = 31,     // AST register
    IPR_KGP = 32,     // Kernel Global Pointer
    IPR_VAL = 33,     // Value register (PAL temporary)
    IPR_PMEIPL = 34,  // PME Interrupt Priority Level
    IPR_ICCSR = 35,   // Instruction Cache Control and Status
    IPR_PCC = 36,     // Process Cycle Counter
    IPR_PERFMON = 37, // Performance Monitor Control
    IPR_PCC_CC = 38,  // Process Cycle Counter Corrected
    IPR_SP = 39,      // Stack Pointer (current mode)
    IPR_IERR = 40,    // Instruction Error register
    IPR_FPCR = 41,    // Floating-Point Control Register

    // === Machine Check and Error Registers (60-79) ===
    IPR_MCSR = 60,         // Machine Check Status Register
    IPR_DC_CTL = 61,       // Data Cache Control
    IPR_DC_STAT = 62,      // Data Cache Status
    IPR_IC_PERR_STAT = 63, // I-Cache Parity Error Status
    IPR_PMCTR = 64,        // Performance Monitor Counter
    IPR_IPR0 = 80,
    IPR_IPR1 = 81,
    IPR_IPR2 = 82,
    IPR_IPR3 = 83,
    IPR_IPR4 = 84,
    IPR_IPR5 = 85,
    IPR_IPR6 = 86,
    IPR_IPR7 = 87,
    IPR_IPR8 = 88,
    IPR_IPR9 = 89,
    IPR_IPR10 = 90,
    IPR_IPR11 = 91,
    IPR_IPR12 = 92,
    IPR_IPR13 = 93,
    IPR_IPR14 = 94,
    IPR_IPR15 = 95,
    IPR_IPR16 = 96,
    IPR_IPR17 = 97,
    IPR_IPR18 = 98,
    IPR_IPR19 = 99,
    IPR_IPR20 = 100,
    IPR_IPR21 = 101,
    IPR_IPR22 = 102,
    IPR_IPR23 = 103,
    IPR_IPR24 = 104,
    IPR_IPR25 = 105,
    IPR_IPR26 = 106,
    IPR_IPR27 = 107,
    IPR_IPR28 = 108,
    IPR_IPR29 = 109,
    IPR_IPR30 = 110,
    IPR_IPR31 = 111,
    IPR_IPR32 = 112,
    IPR_IPR33 = 113,
    IPR_IPR34 = 114,
    IPR_IPR35 = 115,
    IPR_IPR36 = 116,
    IPR_IPR37 = 117,
    IPR_IPR38 = 118,
    IPR_IPR39 = 119,
    IPR_IPR40 = 120,
    IPR_IPR41 = 121,
    IPR_IPR42 = 122,
    IPR_IPR43 = 123,
    IPR_IPR44 = 124,
    IPR_IPR45 = 125,
    IPR_IPR46 = 126,
    IPR_IPR47 = 127,
    IPR_IPR48 = 128,
    IPR_IPR49 = 129,
    IPR_IPR50 = 130,
    IPR_IPR51 = 131,
    IPR_IPR52 = 132,
    IPR_IPR53 = 133,
    IPR_IPR54 = 134,
    IPR_IPR55 = 135,
    IPR_IPR56 = 136,
    IPR_IPR57 = 137,
    IPR_IPR58 = 138,
    IPR_IPR59 = 139,
    IPR_IPR60 = 140,
    IPR_IPR61 = 141,
    IPR_IPR62 = 142,
    IPR_IPR63 = 143,
    IPR_IPR64 = 144,
    IPR_IPR65 = 145,
    IPR_IPR66 = 146,
    IPR_IPR67 = 147,
    IPR_IPR68 = 148,
    IPR_IPR69 = 149,
    IPR_IPR70 = 150,
    IPR_IPR71 = 151,
    IPR_IPR72 = 152,
    IPR_IPR73 = 153,
    IPR_IPR74 = 154,
    IPR_IPR75 = 155,
    IPR_IPR76 = 156,
    IPR_IPR77 = 157,
    IPR_IPR78 = 158,
    IPR_IPR79 = 159,
    IPR_IPR80 = 160,
    IPR_IPR81 = 161,
    IPR_IPR82 = 162,
    IPR_IPR83 = 163,
    IPR_IPR84 = 164,
    IPR_IPR85 = 165,
    IPR_IPR86 = 166,
    IPR_IPR87 = 167,
    IPR_IPR88 = 168,
    IPR_IPR89 = 169,
    IPR_IPR90 = 170,
    IPR_IPR91 = 171,
    IPR_IPR92 = 172,
    IPR_IPR93 = 173,
    IPR_IPR94 = 174,
    IPR_IPR95 = 175,
    IPR_IPR96 = 176,
    IPR_IPR97 = 177,
    IPR_IPR98 = 178,
    IPR_IPR99 = 179,
    IPR_IPR100 = 180,
    IPR_IPR101 = 181,
    IPR_IPR102 = 182,
    IPR_IPR103 = 183,
    IPR_IPR104 = 184,
    IPR_IPR105 = 185,
    IPR_IPR106 = 186,
    IPR_IPR107 = 187,
    IPR_IPR108 = 188,
    IPR_IPR109 = 189,
    IPR_IPR110 = 190,
    IPR_IPR111 = 191,
    IPR_IPR112 = 192,
    IPR_IPR113 = 193,
    IPR_IPR114 = 194,
    IPR_IPR115 = 195,
    IPR_IPR116 = 196,
    IPR_IPR117 = 197,
    IPR_IPR118 = 198,
    IPR_IPR119 = 199,
    IPR_IPR120 = 200,
    IPR_IPR121 = 201,
    IPR_IPR122 = 202,
    IPR_IPR123 = 203,
    IPR_IPR124 = 204,
    IPR_IPR125 = 205,
    IPR_IPR126 = 206,
    IPR_IPR127 = 207,
    // === Exception Registers (0x100-0x11F) ===
    IPR_EXC_ADDR = 0x100,     // Exception address register
    IPR_EXC_SUM = 0x101,      // Exception summary register
    IPR_EXC_MASK = 0x102,     // Exception mask register
    IPR_EXC_SYNDROME = 0x103, // Exception syndrome register
    IPR_EXC_PC = 0x104,       // Exception Program Counter
    IPR_EXC_PS = 0x105,       // Exception Processor Status

    // === Memory Management (0x110-0x11F) ===
    IPR_MM_FAULT_ADDR = 0x110, // Memory management fault address
    IPR_MM_STAT = 0x111,       // Memory management status
    IPR_DTB_CTL = 0x112,       // Data Translation Buffer Control
    IPR_ITB_CTL = 0x113,       // Instruction Translation Buffer Control

    // === Performance Monitoring (0x120-0x13F) ===
    IPR_PERFMON_CTRL = 0x120, // Performance monitor control
    IPR_PERFMON_MASK = 0x121, // Performance monitor mask
    IPR_PERFMON_0 = 0x130,    // Performance Monitor Counter 0
    IPR_PERFMON_1 = 0x131,    // Performance Monitor Counter 1
    IPR_PERFMON_2 = 0x132,    // Performance Monitor Counter 2
    IPR_PERFMON_3 = 0x133,    // Performance Monitor Counter 3
    IPR_PERFMON_4 = 0x134,    // Performance Monitor Counter 4
    IPR_PERFMON_5 = 0x135,    // Performance Monitor Counter 5
    IPR_PERFMON_6 = 0x136,    // Performance Monitor Counter 6
    IPR_PERFMON_7 = 0x137,    // Performance Monitor Counter 7

    // === Exception Entry Points (0x140-0x14F) ===
    IPR_ENTRY_0 = 0x140, // Exception Entry Point 0
    IPR_ENTRY_1 = 0x141, // Exception Entry Point 1
    IPR_ENTRY_2 = 0x142, // Exception Entry Point 2
    IPR_ENTRY_3 = 0x143, // Exception Entry Point 3
    IPR_ENTRY_4 = 0x144, // Exception Entry Point 4
    IPR_ENTRY_5 = 0x145, // Exception Entry Point 5
    IPR_ENTRY_6 = 0x146, // Exception Entry Point 6
    IPR_ENTRY_7 = 0x147, // Exception Entry Point 7

    // === System Vectors (0x150-0x15F) ===
    IPR_RESTART_VECTOR = 0x150,  // Restart vector
    IPR_DEBUGGER_VECTOR = 0x151, // Debugger vector
    IPR_PROCESS = 0x152,         // Process context register

    // === TLB Control (Write-Only Triggers) (0x160-0x16F) ===
    IPR_TBIA = 0x160,  // TLB Invalidate All
    IPR_TBIAP = 0x161, // TLB Invalidate All Process
    IPR_TBIS = 0x162,  // TLB Invalidate Single
    IPR_TBISD = 0x163, // TLB Invalidate Single Data
    IPR_TBISI = 0x164, // TLB Invalidate Single Instruction

    // Total count for array sizing
    IPR_COUNT = 0x200 // 512 total IPR slots
};

// Alias to match your IprBank's expected 'Ipr' enum name
using Ipr = IPRNumbers;

// Legacy compatibility - map old names to new enum
// Legacy compatibility - map old names to new enum
namespace IPR
{
// === Core Processor Registers ===
static constexpr auto PAL_BASE = IPRNumbers::IPR_PAL_BASE;
static constexpr auto PS = IPRNumbers::IPR_PS;
static constexpr auto FEN = IPRNumbers::IPR_FEN;
static constexpr auto IPIR = IPRNumbers::IPR_IPIR;
static constexpr auto IPL = IPRNumbers::IPR_IPL;
static constexpr auto ASN = IPRNumbers::IPR_ASN;
static constexpr auto ASTSR = IPRNumbers::IPR_ASTSR;
static constexpr auto ASTEN = IPRNumbers::IPR_ASTEN;
static constexpr auto SIRR = IPRNumbers::IPR_SIRR;
static constexpr auto IPLR = IPRNumbers::IPR_IPLR;
static constexpr auto VPTB = IPRNumbers::IPR_VPTB;
static constexpr auto USP = IPRNumbers::IPR_USP;
static constexpr auto KSP = IPRNumbers::IPR_KSP;
static constexpr auto SSP = IPRNumbers::IPR_SSP;
static constexpr auto ESP = IPRNumbers::IPR_ESP;
static constexpr auto SCBB = IPRNumbers::IPR_SCBB;
static constexpr auto SISR = IPRNumbers::IPR_SISR;
static constexpr auto PRBR = IPRNumbers::IPR_PRBR;
static constexpr auto PTBR = IPRNumbers::IPR_PTBR;
static constexpr auto PCBB = IPRNumbers::IPR_PCBB;
static constexpr auto MCES = IPRNumbers::IPR_MCES;
static constexpr auto TBCHK = IPRNumbers::IPR_TBCHK;
static constexpr auto WHAMI = IPRNumbers::IPR_WHAMI;
static constexpr auto UNQ = IPRNumbers::IPR_UNQ;
static constexpr auto THREAD = IPRNumbers::IPR_THREAD;
static constexpr auto PAL_MODE = IPRNumbers::IPR_PAL_MODE;
static constexpr auto IRQL = IPRNumbers::IPR_IRQL;
static constexpr auto PAL_TEMP = IPRNumbers::IPR_PAL_TEMP;

// === Missing Standard IPRs ===
static constexpr auto AST = IPRNumbers::IPR_AST;
static constexpr auto KGP = IPRNumbers::IPR_KGP;
static constexpr auto VAL = IPRNumbers::IPR_VAL;
static constexpr auto PMEIPL = IPRNumbers::IPR_PMEIPL;
static constexpr auto ICCSR = IPRNumbers::IPR_ICCSR;
static constexpr auto PCC = IPRNumbers::IPR_PCC;
static constexpr auto PERFMON = IPRNumbers::IPR_PERFMON;
static constexpr auto PCC_CC = IPRNumbers::IPR_PCC_CC;
static constexpr auto SP = IPRNumbers::IPR_SP;
static constexpr auto IERR = IPRNumbers::IPR_IERR;
static constexpr auto FPCR = IPRNumbers::IPR_FPCR;

// === Machine Check and Error Registers ===
static constexpr auto MCSR = IPRNumbers::IPR_MCSR;
static constexpr auto DC_CTL = IPRNumbers::IPR_DC_CTL;
static constexpr auto DC_STAT = IPRNumbers::IPR_DC_STAT;
static constexpr auto IC_PERR_STAT = IPRNumbers::IPR_IC_PERR_STAT;
static constexpr auto PMCTR = IPRNumbers::IPR_PMCTR;

// === Generic IPR Window (IPR0-IPR127) ===
static constexpr auto IPR0 = IPRNumbers::IPR_IPR0;
static constexpr auto IPR1 = IPRNumbers::IPR_IPR1;
static constexpr auto IPR2 = IPRNumbers::IPR_IPR2;
static constexpr auto IPR3 = IPRNumbers::IPR_IPR3;
static constexpr auto IPR4 = IPRNumbers::IPR_IPR4;
static constexpr auto IPR5 = IPRNumbers::IPR_IPR5;
static constexpr auto IPR6 = IPRNumbers::IPR_IPR6;
static constexpr auto IPR7 = IPRNumbers::IPR_IPR7;
static constexpr auto IPR8 = IPRNumbers::IPR_IPR8;
static constexpr auto IPR9 = IPRNumbers::IPR_IPR9;
static constexpr auto IPR10 = IPRNumbers::IPR_IPR10;
static constexpr auto IPR11 = IPRNumbers::IPR_IPR11;
static constexpr auto IPR12 = IPRNumbers::IPR_IPR12;
static constexpr auto IPR13 = IPRNumbers::IPR_IPR13;
static constexpr auto IPR14 = IPRNumbers::IPR_IPR14;
static constexpr auto IPR15 = IPRNumbers::IPR_IPR15;
static constexpr auto IPR16 = IPRNumbers::IPR_IPR16;
static constexpr auto IPR17 = IPRNumbers::IPR_IPR17;
static constexpr auto IPR18 = IPRNumbers::IPR_IPR18;
static constexpr auto IPR19 = IPRNumbers::IPR_IPR19;
static constexpr auto IPR20 = IPRNumbers::IPR_IPR20;
static constexpr auto IPR21 = IPRNumbers::IPR_IPR21;
static constexpr auto IPR22 = IPRNumbers::IPR_IPR22;
static constexpr auto IPR23 = IPRNumbers::IPR_IPR23;
static constexpr auto IPR24 = IPRNumbers::IPR_IPR24;
static constexpr auto IPR25 = IPRNumbers::IPR_IPR25;
static constexpr auto IPR26 = IPRNumbers::IPR_IPR26;
static constexpr auto IPR27 = IPRNumbers::IPR_IPR27;
static constexpr auto IPR28 = IPRNumbers::IPR_IPR28;
static constexpr auto IPR29 = IPRNumbers::IPR_IPR29;
static constexpr auto IPR30 = IPRNumbers::IPR_IPR30;
static constexpr auto IPR31 = IPRNumbers::IPR_IPR31;
static constexpr auto IPR32 = IPRNumbers::IPR_IPR32;
static constexpr auto IPR33 = IPRNumbers::IPR_IPR33;
static constexpr auto IPR34 = IPRNumbers::IPR_IPR34;
static constexpr auto IPR35 = IPRNumbers::IPR_IPR35;
static constexpr auto IPR36 = IPRNumbers::IPR_IPR36;
static constexpr auto IPR37 = IPRNumbers::IPR_IPR37;
static constexpr auto IPR38 = IPRNumbers::IPR_IPR38;
static constexpr auto IPR39 = IPRNumbers::IPR_IPR39;
static constexpr auto IPR40 = IPRNumbers::IPR_IPR40;
static constexpr auto IPR41 = IPRNumbers::IPR_IPR41;
static constexpr auto IPR42 = IPRNumbers::IPR_IPR42;
static constexpr auto IPR43 = IPRNumbers::IPR_IPR43;
static constexpr auto IPR44 = IPRNumbers::IPR_IPR44;
static constexpr auto IPR45 = IPRNumbers::IPR_IPR45;
static constexpr auto IPR46 = IPRNumbers::IPR_IPR46;
static constexpr auto IPR47 = IPRNumbers::IPR_IPR47;
static constexpr auto IPR48 = IPRNumbers::IPR_IPR48;
static constexpr auto IPR49 = IPRNumbers::IPR_IPR49;
static constexpr auto IPR50 = IPRNumbers::IPR_IPR50;
static constexpr auto IPR51 = IPRNumbers::IPR_IPR51;
static constexpr auto IPR52 = IPRNumbers::IPR_IPR52;
static constexpr auto IPR53 = IPRNumbers::IPR_IPR53;
static constexpr auto IPR54 = IPRNumbers::IPR_IPR54;
static constexpr auto IPR55 = IPRNumbers::IPR_IPR55;
static constexpr auto IPR56 = IPRNumbers::IPR_IPR56;
static constexpr auto IPR57 = IPRNumbers::IPR_IPR57;
static constexpr auto IPR58 = IPRNumbers::IPR_IPR58;
static constexpr auto IPR59 = IPRNumbers::IPR_IPR59;
static constexpr auto IPR60 = IPRNumbers::IPR_IPR60;
static constexpr auto IPR61 = IPRNumbers::IPR_IPR61;
static constexpr auto IPR62 = IPRNumbers::IPR_IPR62;
static constexpr auto IPR63 = IPRNumbers::IPR_IPR63;
static constexpr auto IPR64 = IPRNumbers::IPR_IPR64;
static constexpr auto IPR65 = IPRNumbers::IPR_IPR65;
static constexpr auto IPR66 = IPRNumbers::IPR_IPR66;
static constexpr auto IPR67 = IPRNumbers::IPR_IPR67;
static constexpr auto IPR68 = IPRNumbers::IPR_IPR68;
static constexpr auto IPR69 = IPRNumbers::IPR_IPR69;
static constexpr auto IPR70 = IPRNumbers::IPR_IPR70;
static constexpr auto IPR71 = IPRNumbers::IPR_IPR71;
static constexpr auto IPR72 = IPRNumbers::IPR_IPR72;
static constexpr auto IPR73 = IPRNumbers::IPR_IPR73;
static constexpr auto IPR74 = IPRNumbers::IPR_IPR74;
static constexpr auto IPR75 = IPRNumbers::IPR_IPR75;
static constexpr auto IPR76 = IPRNumbers::IPR_IPR76;
static constexpr auto IPR77 = IPRNumbers::IPR_IPR77;
static constexpr auto IPR78 = IPRNumbers::IPR_IPR78;
static constexpr auto IPR79 = IPRNumbers::IPR_IPR79;
static constexpr auto IPR80 = IPRNumbers::IPR_IPR80;
static constexpr auto IPR81 = IPRNumbers::IPR_IPR81;
static constexpr auto IPR82 = IPRNumbers::IPR_IPR82;
static constexpr auto IPR83 = IPRNumbers::IPR_IPR83;
static constexpr auto IPR84 = IPRNumbers::IPR_IPR84;
static constexpr auto IPR85 = IPRNumbers::IPR_IPR85;
static constexpr auto IPR86 = IPRNumbers::IPR_IPR86;
static constexpr auto IPR87 = IPRNumbers::IPR_IPR87;
static constexpr auto IPR88 = IPRNumbers::IPR_IPR88;
static constexpr auto IPR89 = IPRNumbers::IPR_IPR89;
static constexpr auto IPR90 = IPRNumbers::IPR_IPR90;
static constexpr auto IPR91 = IPRNumbers::IPR_IPR91;
static constexpr auto IPR92 = IPRNumbers::IPR_IPR92;
static constexpr auto IPR93 = IPRNumbers::IPR_IPR93;
static constexpr auto IPR94 = IPRNumbers::IPR_IPR94;
static constexpr auto IPR95 = IPRNumbers::IPR_IPR95;
static constexpr auto IPR96 = IPRNumbers::IPR_IPR96;
static constexpr auto IPR97 = IPRNumbers::IPR_IPR97;
static constexpr auto IPR98 = IPRNumbers::IPR_IPR98;
static constexpr auto IPR99 = IPRNumbers::IPR_IPR99;
static constexpr auto IPR100 = IPRNumbers::IPR_IPR100;
static constexpr auto IPR101 = IPRNumbers::IPR_IPR101;
static constexpr auto IPR102 = IPRNumbers::IPR_IPR102;
static constexpr auto IPR103 = IPRNumbers::IPR_IPR103;
static constexpr auto IPR104 = IPRNumbers::IPR_IPR104;
static constexpr auto IPR105 = IPRNumbers::IPR_IPR105;
static constexpr auto IPR106 = IPRNumbers::IPR_IPR106;
static constexpr auto IPR107 = IPRNumbers::IPR_IPR107;
static constexpr auto IPR108 = IPRNumbers::IPR_IPR108;
static constexpr auto IPR109 = IPRNumbers::IPR_IPR109;
static constexpr auto IPR110 = IPRNumbers::IPR_IPR110;
static constexpr auto IPR111 = IPRNumbers::IPR_IPR111;
static constexpr auto IPR112 = IPRNumbers::IPR_IPR112;
static constexpr auto IPR113 = IPRNumbers::IPR_IPR113;
static constexpr auto IPR114 = IPRNumbers::IPR_IPR114;
static constexpr auto IPR115 = IPRNumbers::IPR_IPR115;
static constexpr auto IPR116 = IPRNumbers::IPR_IPR116;
static constexpr auto IPR117 = IPRNumbers::IPR_IPR117;
static constexpr auto IPR118 = IPRNumbers::IPR_IPR118;
static constexpr auto IPR119 = IPRNumbers::IPR_IPR119;
static constexpr auto IPR120 = IPRNumbers::IPR_IPR120;
static constexpr auto IPR121 = IPRNumbers::IPR_IPR121;
static constexpr auto IPR122 = IPRNumbers::IPR_IPR122;
static constexpr auto IPR123 = IPRNumbers::IPR_IPR123;
static constexpr auto IPR124 = IPRNumbers::IPR_IPR124;
static constexpr auto IPR125 = IPRNumbers::IPR_IPR125;
static constexpr auto IPR126 = IPRNumbers::IPR_IPR126;
static constexpr auto IPR127 = IPRNumbers::IPR_IPR127;

// === Exception Registers ===
static constexpr auto EXC_ADDR = IPRNumbers::IPR_EXC_ADDR;
static constexpr auto EXC_SUM = IPRNumbers::IPR_EXC_SUM;
static constexpr auto EXC_MASK = IPRNumbers::IPR_EXC_MASK;
static constexpr auto EXC_SYNDROME = IPRNumbers::IPR_EXC_SYNDROME;
static constexpr auto EXC_PC = IPRNumbers::IPR_EXC_PC;
static constexpr auto EXC_PS = IPRNumbers::IPR_EXC_PS;

// === Memory Management ===
static constexpr auto MM_FAULT_ADDR = IPRNumbers::IPR_MM_FAULT_ADDR;
static constexpr auto MM_STAT = IPRNumbers::IPR_MM_STAT;
static constexpr auto DTB_CTL = IPRNumbers::IPR_DTB_CTL;
static constexpr auto ITB_CTL = IPRNumbers::IPR_ITB_CTL;

// === Performance Monitoring ===
static constexpr auto PERFMON_CTRL = IPRNumbers::IPR_PERFMON_CTRL;
static constexpr auto PERFMON_MASK = IPRNumbers::IPR_PERFMON_MASK;
static constexpr auto PERFMON_0 = IPRNumbers::IPR_PERFMON_0;
static constexpr auto PERFMON_1 = IPRNumbers::IPR_PERFMON_1;
static constexpr auto PERFMON_2 = IPRNumbers::IPR_PERFMON_2;
static constexpr auto PERFMON_3 = IPRNumbers::IPR_PERFMON_3;
static constexpr auto PERFMON_4 = IPRNumbers::IPR_PERFMON_4;
static constexpr auto PERFMON_5 = IPRNumbers::IPR_PERFMON_5;
static constexpr auto PERFMON_6 = IPRNumbers::IPR_PERFMON_6;
static constexpr auto PERFMON_7 = IPRNumbers::IPR_PERFMON_7;

// === Exception Entry Points ===
static constexpr auto ENTRY_0 = IPRNumbers::IPR_ENTRY_0;
static constexpr auto ENTRY_1 = IPRNumbers::IPR_ENTRY_1;
static constexpr auto ENTRY_2 = IPRNumbers::IPR_ENTRY_2;
static constexpr auto ENTRY_3 = IPRNumbers::IPR_ENTRY_3;
static constexpr auto ENTRY_4 = IPRNumbers::IPR_ENTRY_4;
static constexpr auto ENTRY_5 = IPRNumbers::IPR_ENTRY_5;
static constexpr auto ENTRY_6 = IPRNumbers::IPR_ENTRY_6;
static constexpr auto ENTRY_7 = IPRNumbers::IPR_ENTRY_7;

// === System Vectors ===
static constexpr auto RESTART_VECTOR = IPRNumbers::IPR_RESTART_VECTOR;
static constexpr auto DEBUGGER_VECTOR = IPRNumbers::IPR_DEBUGGER_VECTOR;
static constexpr auto PROCESS = IPRNumbers::IPR_PROCESS;

// === TLB Control Registers ===
static constexpr auto TBIA = IPRNumbers::IPR_TBIA;
static constexpr auto TBIAP = IPRNumbers::IPR_TBIAP;
static constexpr auto TBIS = IPRNumbers::IPR_TBIS;
static constexpr auto TBISD = IPRNumbers::IPR_TBISD;
static constexpr auto TBISI = IPRNumbers::IPR_TBISI;

} // namespace IPR