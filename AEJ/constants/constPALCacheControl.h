#pragma once
#include <QtCore>
#include <QObject>
#include "AlphaPlatformGuards.h"

//==============================================================================
// ALPHA PROCESSOR IDENTIFICATION
//==============================================================================

// Processor implementation values (from IMPLVER instruction)
constexpr quint32 ALPHA_IMPLVER_EV4 = 0; // Alpha 21064 (EV4)
constexpr quint32 ALPHA_IMPLVER_EV5 = 1; // Alpha 21164 (EV5)
constexpr quint32 ALPHA_IMPLVER_EV6 = 2; // Alpha 21264 (EV6)
constexpr quint32 ALPHA_IMPLVER_EV7 = 3; // Alpha 21364 (EV7)

//==============================================================================
// CACHE CONTROL CONSTANTS
//==============================================================================

// Prefetch hints and cache operations
constexpr quint32 PREFETCH_LOAD = 0x0;       // Prefetch for load
constexpr quint32 PREFETCH_MODIFY = 0x2000;  // Prefetch for modify
constexpr quint32 CACHE_INVALIDATE = 0x1400; // Cache invalidate

// Cache control operations
constexpr quint32 FUNC_ECBI = 0x1400; // Evict cache block invalidated
constexpr quint32 FUNC_WH64 = 0xF800; // Write Hint 64 bytes


constexpr quint32 PAL_HALT = 0x00;   // Halt processor /
constexpr quint32 PAL_CFLUSH = 0x01; // Cache flush /
constexpr quint32 PAL_DRAINA = 0x02; // Drain aborts /


namespace OpenVMS
{
// System and privileged instructions
constexpr quint32 SWPCTX = 0x05;         // Swap context ??
constexpr quint32 PAL_MFPR_ASN = 0x06;   // Move from processor register - ASN
constexpr quint32 PAL_MTPR_ASTEN = 0x07; // Move to processor register - AST enable
constexpr quint32 PAL_MTPR_ASTSR = 0x08; // Move to processor register - AST summary

// CPU control
constexpr quint32 PAL_MFPR_FEN = 0x0B;  // Move from processor register - FP enable
constexpr quint32 PAL_MTPR_FEN = 0x0C;  // Move to processor register - FP enable
constexpr quint32 PAL_MTPR_IPIR = 0x0D; // Move to processor register - IPI request

// Interrupt control
constexpr quint32 PAL_MFPR_IPL = 0x0E; // Move from processor register - IPL
constexpr quint32 PAL_MTPR_IPL = 0x0F; // Move to processor register - IPL

// Machine check handling
constexpr quint32 PAL_MFPR_MCES = 0x10; // Move from processor register - MCES
constexpr quint32 PAL_MTPR_MCES = 0x11; // Move to processor register - MCES

// Process control
constexpr quint32 PAL_MFPR_PCBB = 0x12; // Move from processor register - PCBB
constexpr quint32 PAL_MFPR_PRBR = 0x13; // Move from processor register - PRBR
constexpr quint32 PAL_MTPR_PRBR = 0x14; // Move to processor register - PRBR

// Memory management
constexpr quint32 PAL_MFPR_PTBR = 0x15; // Move from processor register - PTBR
constexpr quint32 PAL_MFPR_SCBB = 0x16; // Move from processor register - SCBB
constexpr quint32 PAL_MTPR_SCBB = 0x17; // Move to processor register - SCBB
constexpr quint32 PAL_TBIS = 0x42;      // Translation buffer invalidate single

// Interrupt handling
constexpr quint32 PAL_MTPR_SIRR = 0x18; // Move to processor register - SIRR
constexpr quint32 PAL_MFPR_SISR = 0x19; // Move from processor register - SISR

// TLB management
constexpr quint32 PAL_MFPR_TBCHK = 0x1A; // Move from processor register - TBCHK
constexpr quint32 PAL_MTPR_TBIA = 0x1B;  // Move to processor register - TBIA
constexpr quint32 PAL_MTPR_TBIAP = 0x1C; // Move to processor register - TBIAP
constexpr quint32 PAL_MTPR_TBIS = 0x1D;  // Move to processor register - TBIS

// Stack pointer management
constexpr quint32 PAL_MFPR_ESP = 0x1E; // Move from processor register - ESP
constexpr quint32 PAL_MTPR_ESP = 0x1F; // Move to processor register - ESP
constexpr quint32 PAL_MFPR_SSP = 0x20; // Move from processor register - SSP
constexpr quint32 PAL_MTPR_SSP = 0x21; // Move to processor register - SSP
constexpr quint32 PAL_MFPR_USP = 0x22; // Move from processor register - USP
constexpr quint32 PAL_MTPR_USP = 0x23; // Move to processor register - USP

// Advanced TLB operations
constexpr quint32 PAL_MTPR_TBISD = 0x24; // Move to processor register - TBISD
constexpr quint32 PAL_MTPR_TBISI = 0x25; // Move to processor register - TBISI

// AST operations
constexpr quint32 MFPR_ASTEN = 0x26; // Move from processor register - ASTEN
constexpr quint32 MFPR_ASTSR = 0x27; // Move from processor register - ASTSR

// Virtual page table
constexpr quint32 PAL_MFPR_VPTB = 0x29; // Move from processor register - VPTB
constexpr quint32 PAL_MTPR_VPTB = 0x2A; // Move to processor register - VPTB

// Performance monitoring
constexpr quint32 PAL_MTPR_PERFMON = 0x2B; // Move to processor register - PERFMON

// Processor identification
constexpr quint32 PAL_MFPR_WHAMI = 0x3F; // Move from processor register - WHAMI

// Mode changes
constexpr quint32 PAL_CHME = 0x82; // Change mode to executive
constexpr quint32 PAL_CHMS = 0x83; // Change mode to supervisor
constexpr quint32 PAL_CHMU = 0x84; // Change mode to user

// Queue operations
constexpr quint32 PAL_INSQHIL = 0x85; // Insert into queue head, longword, interlocked
constexpr quint32 PAL_INSQTIL = 0x86; // Insert into queue tail, longword, interlocked
constexpr quint32 PAL_INSQHIQ = 0x87; // Insert into queue head, quadword, interlocked
constexpr quint32 PAL_INSQTIQ = 0x88; // Insert into queue tail, quadword, interlocked
constexpr quint32 PAL_REMQHIL = 0x89; // Remove from queue head, longword, interlocked
constexpr quint32 PAL_REMQTIL = 0x8A; // Remove from queue tail, longword, interlocked
constexpr quint32 PAL_REMQHIQ = 0x8B; // Remove from queue head, quadword, interlocked
constexpr quint32 PAL_REMQTIQ = 0x8C; // Remove from queue tail, quadword, interlocked

} // namespace OpenVMS

namespace SRM
{
// SRM/Linux specific PAL codes
constexpr quint32 SWPCTX = 0x00; // Swap context
constexpr quint32 WRKGP = 0x0D;  // Write kernel global pointer
constexpr quint32 WRUSP = 0x0E;  // Write user stack pointer
constexpr quint32 RDUSP = 0x0F;  // Read user stack pointer
constexpr quint32 TBI = 0x33;    // Translation buffer invalidate

// Console operations
constexpr quint32 CSERVE = 0x09; // Console service
constexpr quint32 SWPPAL = 0x0A; // Swap PAL code

// Interrupt handling
constexpr quint32 RDIRQL = 0x06;  // Read IRQ level
constexpr quint32 SWPIRQL = 0x01; // Swap IRQ level
constexpr quint32 DI = 0x08;      // Disable interrupts
constexpr quint32 EI = 0x09;      // Enable interrupts

// Machine check handling
constexpr quint32 RDMCES = 0x13; // Read machine check error summary
constexpr quint32 WRMCES = 0x14; // Write machine check error summary

// Linux-specific
constexpr quint32 LDQP = 0x03;    // Load quadword physical
constexpr quint32 STQP = 0x04;    // Store quadword physical
constexpr quint32 BPT = 0x80;     // Breakpoint
constexpr quint32 BUGCHK = 0x81;  // Bug check
constexpr quint32 CALLSYS = 0x83; // System call
constexpr quint32 IMB = 0x86;     // Instruction memory barrier
} // namespace SRM
#ifdef ALPHA_PLATFORM_TRU64
namespace Tru64
{
// System call and process control
constexpr quint32 CALLSYS = 0x83;      // System call
constexpr quint32 PAL_SWPCTX = 0x30;   // Swap context
constexpr quint32 PAL_SWPPAL = 0x0A;   // Swap PALcode
constexpr quint32 PAL_TODO_IMB = 0x86; // Instruction memory barrier

// CPU control and diagnostics
constexpr quint32 PAL_RDPS = 0x36;           // Read processor status
constexpr quint32 PAL_WRKGP = 0x0D;          // Write kernel global pointer
constexpr quint32 PAL_WRUSP = 0x0E;          // Write user stack pointer
constexpr quint32 PAL_RDUSP = 0x0F;          // Read user stack pointer
constexpr quint32 PAL_WRPERFMON = 0x91;      // Write performance monitor
constexpr quint32 PAL_TODO_RDPERFMON = 0x90; // Read performance monitor

// Interrupt control
constexpr quint32 PAL_RDIRQL = 0x06;    // Read interrupt request level
constexpr quint32 PAL_SWPIRQL = 0x01;   // Swap interrupt request level
constexpr quint32 PAL_DI = 0x08;        // Disable interrupts
constexpr quint32 PAL_EI = 0x09;        // Enable interrupts
constexpr quint32 PAL_MTPR_IPIR = 0x0D; // Write interprocessor interrupt request

// Machine check handling
constexpr quint32 PAL_RDMCES = 0x10; // Read machine check error summary
constexpr quint32 PAL_WRMCES = 0x11; // Write machine check error summary

// Floating point control
constexpr quint32 PAL_WRFEN = 0x0C; // Write floating-point enable

// MMU/TLB control
constexpr quint32 PAL_WRVPTPTR = 0x0B; // Write virtual page table pointer
constexpr quint32 PAL_WTKTRP = 0x12;   // Write kernel trap entry
constexpr quint32 PAL_WRENT = 0x34;    // Write entry point
constexpr quint32 PAL_TBI = 0x33;      // Translation buffer invalidate

// Diagnostic operations
constexpr quint32 PAL_WRVAL = 0x07; // Write system value
constexpr quint32 PAL_RDVAL = 0x08; // Read system value

// Console operations
constexpr quint32 PAL_CSERVE = 0x09; // Console service
} // namespace Tru64
#endif
