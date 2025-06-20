#pragma once
#pragma once

/**
 * @file   PALFunctionConstants.h
 * @brief  Complete Alpha PAL (Privileged Architecture Library) function constants
 *
 * This file defines all PAL function codes for Alpha AXP processors.
 * PAL functions are invoked via CALL_PAL instructions (OpCode 0x00).
 * Different Alpha implementations and operating systems use different PAL codes.
 */

#include <QtGlobal>
// 
// // =============================================================================
// // COMMON PAL FUNCTIONS (used across multiple Alpha implementations)
// // =============================================================================
// 
// // System Control and Debugging
// static constexpr quint32 FUNC_Common_HALT_ = 0x0000;   // Halt processor
// static constexpr quint32 FUNC_Common_CFLUSH_ = 0x0001; // Cache flush
// static constexpr quint32 FUNC_Common_DRAINA_ = 0x0002; // Drain aborts
// static constexpr quint32 FUNC_Common_CSERVE_ = 0x0009; // Console service
// static constexpr quint32 FUNC_Common_IMB_ = 0x0086;    // Instruction memory barrier
// 
// // Context and Process Management
// static constexpr quint32 FUNC_Common_SWPCTX_ = 0x0030; // Swap context
// static constexpr quint32 FUNC_Common_REI_ = 0x003E;    // Return from exception/interrupt
// 
// // Memory Management - TLB Operations
// static constexpr quint32 FUNC_Common_TBI_ = 0x0033;        // Translation buffer invalidate
// static constexpr quint32 FUNC_Common_MTPR_TBIA_ = 0x0034;  // TLB invalidate all
// static constexpr quint32 FUNC_Common_MTPR_TBIS_ = 0x0035;  // TLB invalidate single
// static constexpr quint32 FUNC_Common_MTPR_TBISD_ = 0x0036; // TLB invalidate single data
// static constexpr quint32 FUNC_Common_MTPR_TBISI_ = 0x0037; // TLB invalidate single instruction
// static constexpr quint32 FUNC_Common_MTPR_VPTB_ = 0x002D;  // Move to virtual page table base
// static constexpr quint32 FUNC_Common_MFPR_VPTB_ = 0x002A;  // Move from virtual page table base
// 
// // IPR (Internal Processor Register) Operations
// static constexpr quint32 FUNC_Common_MFPR_ASTEN_ = 0x0026; // Move from AST enable
// static constexpr quint32 FUNC_Common_MFPR_ASTSR_ = 0x0027; // Move from AST summary
// static constexpr quint32 FUNC_Common_MFPR_FEN_ = 0x002B;   // Move from floating enable
// static constexpr quint32 FUNC_Common_WRVAL_ = 0x0031;      // Write value
// static constexpr quint32 FUNC_Common_RDVAL_ = 0x0032;      // Read value
// static constexpr quint32 FUNC_Common_WRENT_ = 0x0034;      // Write entry point
// static constexpr quint32 FUNC_Common_SWPIPL_ = 0x0035;     // Swap interrupt priority level
// static constexpr quint32 FUNC_Common_RDPS_ = 0x0036;       // Read processor status
// static constexpr quint32 FUNC_Common_WRKGP_ = 0x0037;      // Write kernel global pointer
// static constexpr quint32 FUNC_Common_WRUSP_ = 0x0038;      // Write user stack pointer
// static constexpr quint32 FUNC_Common_RDUSP_ = 0x003A;      // Read user stack pointer
// static constexpr quint32 FUNC_Common_WRPERFMON_ = 0x0039;  // Write performance monitor
// 
// // System Calls and Mode Changes
// static constexpr quint32 FUNC_Common_BPT_ = 0x0080;     // Breakpoint
// static constexpr quint32 FUNC_Common_BUGCHK_ = 0x0081;  // Bug check
// static constexpr quint32 FUNC_Common_CHME_ = 0x0082;    // Change mode to executive
// static constexpr quint32 FUNC_Common_CHMS_ = 0x0083;    // Change mode to supervisor
// static constexpr quint32 FUNC_Common_CHMU_ = 0x0084;    // Change mode to user
// static constexpr quint32 FUNC_Common_GENTRAP_ = 0x00AA; // Generate trap
// 
// // Memory Access Validation
// static constexpr quint32 FUNC_Common_PROBEW_ = 0x002E; // Probe write access
// static constexpr quint32 FUNC_Common_PROBER_ = 0x002F; // Probe read access
// 
// // Interlocked Queue Operations (Alpha-specific)
// static constexpr quint32 FUNC_Common_INSQHIL_ = 0x0087; // Insert head interlocked longword
// static constexpr quint32 FUNC_Common_INSQTIL_ = 0x0088; // Insert tail interlocked longword
// static constexpr quint32 FUNC_Common_INSQHIQ_ = 0x0089; // Insert head interlocked quadword
// static constexpr quint32 FUNC_Common_REMQHIL_ = 0x008A; // Remove head interlocked longword
// static constexpr quint32 FUNC_Common_REMQTIL_ = 0x008B; // Remove tail interlocked longword
// static constexpr quint32 FUNC_Common_REMQHIQ_ = 0x008C; // Remove head interlocked quadword
// static constexpr quint32 FUNC_Common_REMQTIQ_ = 0x008D; // Remove tail interlocked quadword
// 
// // Memory Barriers (used in MISC instruction format, but related to PAL)
// static constexpr quint32 FUNC_MB = 0x4000;    // Memory barrier
// static constexpr quint32 FUNC_TRAPB = 0x0000; // Trap barrier
// 
// // =============================================================================
// // ALPHA-SPECIFIC PAL FUNCTIONS (Digital Alpha implementations)
// // =============================================================================
// 
// // Physical Memory Access
// static constexpr quint32 FUNC_Alpha_LDQP_ = 0x0001; // Load quadword physical
// static constexpr quint32 FUNC_Alpha_STQP_ = 0x0002; // Store quadword physical
// 
// // Extended IPR Operations
// static constexpr quint32 FUNC_Alpha_MFPR_ASN_ = 0x0003;     // Move from ASN
// static constexpr quint32 FUNC_Alpha_MTPR_ASTEN_ = 0x0004;   // Move to AST enable
// static constexpr quint32 FUNC_Alpha_MTPR_ASTSR_ = 0x0005;   // Move to AST summary
// static constexpr quint32 FUNC_Alpha_MFPR_MCES_ = 0x0010;    // Move from machine check error summary
// static constexpr quint32 FUNC_Alpha_MTPR_MCES_ = 0x0011;    // Move to machine check error summary
// static constexpr quint32 FUNC_Alpha_MFPR_PCBB_ = 0x0012;    // Move from process control block base
// static constexpr quint32 FUNC_Alpha_MFPR_PRBR_ = 0x0013;    // Move from processor base register
// static constexpr quint32 FUNC_Alpha_MTPR_PRBR_ = 0x0014;    // Move to processor base register
// static constexpr quint32 FUNC_Alpha_MFPR_PTBR_ = 0x0015;    // Move from page table base register
// static constexpr quint32 FUNC_Alpha_MTPR_SCBB_ = 0x0016;    // Move to system control block base
// static constexpr quint32 FUNC_Alpha_MTPR_SIRR_ = 0x0017;    // Move to software interrupt request
// static constexpr quint32 FUNC_Alpha_MFPR_SISR_ = 0x0018;    // Move from software interrupt summary
// static constexpr quint32 FUNC_Alpha_MFPR_SSP_ = 0x0019;     // Move from system stack pointer
// static constexpr quint32 FUNC_Alpha_MTPR_SSP_ = 0x001A;     // Move to system stack pointer
// static constexpr quint32 FUNC_Alpha_MFPR_USP_ = 0x001B;     // Move from user stack pointer
// static constexpr quint32 FUNC_Alpha_MTPR_USP_ = 0x001C;     // Move to user stack pointer
// static constexpr quint32 FUNC_Alpha_MTPR_FEN_ = 0x001D;     // Move to floating enable
// static constexpr quint32 FUNC_Alpha_MTPR_IPIR_ = 0x001E;    // Move to inter-processor interrupt request
// static constexpr quint32 FUNC_Alpha_MFPR_IPL_ = 0x001F;     // Move from interrupt priority level
// static constexpr quint32 FUNC_Alpha_MTPR_IPL_ = 0x0020;     // Move to interrupt priority level
// static constexpr quint32 FUNC_Alpha_MFPR_TBCHK_ = 0x0021;   // Move from translation buffer check
// static constexpr quint32 FUNC_Alpha_MTPR_TBIAP_ = 0x0022;   // TLB invalidate all process
// static constexpr quint32 FUNC_Alpha_MFPR_ESP_ = 0x0023;     // Move from executive stack pointer
// static constexpr quint32 FUNC_Alpha_MTPR_ESP_ = 0x0024;     // Move to executive stack pointer
// static constexpr quint32 FUNC_Alpha_MTPR_PERFMON_ = 0x0025; // Move to performance monitor
// 
// // CPU Identification and Control
// static constexpr quint32 FUNC_Alpha_MFPR_WHAMI_ = 0x003F; // Move from WHO-AM-I register
// static constexpr quint32 FUNC_Alpha_READ_UNQ_ = 0x009E;   // Read unique value
// static constexpr quint32 FUNC_Alpha_WRITE_UNQ_ = 0x009F;  // Write unique value
// 
// // System Initialization and Control
// static constexpr quint32 FUNC_Alpha_INITPAL_ = 0x0000; // Initialize PAL
// static constexpr quint32 FUNC_Alpha_WRENTRY_ = 0x0001; // Write entry point
// 
// // Interrupt and Exception Control
// static constexpr quint32 FUNC_Alpha_SWPIRQL_ = 0x0006;    // Swap interrupt request level
// static constexpr quint32 FUNC_Alpha_RDIRQL_ = 0x0007;     // Read interrupt request level
// static constexpr quint32 FUNC_Alpha_DI_ = 0x0008;         // Disable interrupts
// static constexpr quint32 FUNC_Alpha_EI_ = 0x0009;         // Enable interrupts
// static constexpr quint32 FUNC_Alpha_SWPPAL_ = 0x000A;     // Swap PAL code base
// static constexpr quint32 FUNC_Alpha_SSIR_ = 0x000B;       // Set software interrupt request
// static constexpr quint32 FUNC_Alpha_CSIR_ = 0x000C;       // Clear software interrupt request
// static constexpr quint32 FUNC_Alpha_RFE_ = 0x000D;        // Return from exception
// static constexpr quint32 FUNC_Alpha_RETSYS_ = 0x000E;     // Return from system call
// static constexpr quint32 FUNC_Alpha_RESTART_ = 0x000F;    // Restart processor
// static constexpr quint32 FUNC_Alpha_SWPPROCESS_ = 0x0010; // Swap process
// 
// // Machine Check and Error Handling
// static constexpr quint32 FUNC_Alpha_RDMCES_ = 0x0011; // Read machine check error summary
// static constexpr quint32 FUNC_Alpha_WRMCES_ = 0x0012; // Write machine check error summary
// 
// // Advanced TLB Operations
// static constexpr quint32 FUNC_Alpha_TBIA_ = 0x0013;    // Translation buffer invalidate all
// static constexpr quint32 FUNC_Alpha_TBIS_ = 0x0014;    // Translation buffer invalidate single
// static constexpr quint32 FUNC_Alpha_TBISASN_ = 0x0015; // Translation buffer invalidate by ASN
// 
// // Stack Management
// static constexpr quint32 FUNC_Alpha_RDKSP_ = 0x0016;  // Read kernel stack pointer
// static constexpr quint32 FUNC_Alpha_SWPKSP_ = 0x0017; // Swap kernel stack pointer
// 
// // System Control
// static constexpr quint32 FUNC_Alpha_RDPSR_ = 0x0018;  // Read processor status register
// static constexpr quint32 FUNC_Alpha_REBOOT_ = 0x0019; // Reboot system
// 
// // Kernel Debugging and Development
// static constexpr quint32 FUNC_Alpha_CHMK_ = 0x001A;    // Change mode to kernel
// static constexpr quint32 FUNC_Alpha_CALLKD_ = 0x001B;  // Call kernel debugger
// static constexpr quint32 FUNC_Alpha_GENTRAP_ = 0x001C; // Generate trap
// static constexpr quint32 FUNC_Alpha_KBPT_ = 0x001D;    // Kernel breakpoint
// 
// // =============================================================================
// // TRU64 UNIX PAL FUNCTIONS (Digital UNIX / Tru64 UNIX specific)
// // =============================================================================

// System Control
static constexpr quint32 FUNC_Tru64_REBOOT = 0x0000;  // System reboot
static constexpr quint32 FUNC_Tru64_INITPAL = 0x0001; // Initialize PAL for Tru64

// Interrupt Control
static constexpr quint32 FUNC_Tru64_SWPIRQL = 0x0002; // Swap interrupt request level
static constexpr quint32 FUNC_Tru64_RDIRQL_ = 0x0003;  // Read interrupt request level
static constexpr quint32 FUNC_Tru64_DI = 0x0004;      // Disable interrupts

// Machine Check Error Handling
static constexpr quint32 FUNC_Tru64_RDMCES_ = 0x0005; // Read machine check error summary
static constexpr quint32 FUNC_Tru64_WRMCES_ = 0x0006; // Write machine check error summary

// Process Control Block
static constexpr quint32 FUNC_Tru64_RDPCBB_ = 0x0007; // Read process control block base

// System Registers
static constexpr quint32 FUNC_Tru64_WRPRBR_ = 0x0008; // Write processor base register

// TLB Management
static constexpr quint32 FUNC_Tru64_TBIA_ = 0x0009;    // Translation buffer invalidate all
static constexpr quint32 FUNC_Tru64_TBIS_ = 0x000A;    // Translation buffer invalidate single (instruction)
static constexpr quint32 FUNC_Tru64_DTBIS_ = 0x000B;   // Data translation buffer invalidate single
static constexpr quint32 FUNC_Tru64_TBISASN_ = 0x000C; // Translation buffer invalidate by ASN

// Stack Management
static constexpr quint32 FUNC_Tru64_RDKSP_ = 0x000D;  // Read kernel stack pointer
static constexpr quint32 FUNC_Tru64_SWPKSP_ = 0x000E; // Swap kernel stack pointer

// Performance Monitoring
static constexpr quint32 FUNC_Tru64_WRPERFMON_ = 0x000F;  // Write performance monitor
static constexpr quint32 FUNC_Tru64_RDCOUNTERS_ = 0x0013; // Read performance counters
static constexpr quint32 FUNC_Tru64_RDPER_ = 0x0019;      // Read performance counter

// IPL and Priority Management
static constexpr quint32 FUNC_Tru64_SWPIPL_ = 0x0010; // Swap interrupt priority level

// User Stack Management
static constexpr quint32 FUNC_Tru64_RDUSP_ = 0x0011; // Read user stack pointer
static constexpr quint32 FUNC_Tru64_WRUSP_ = 0x0012; // Write user stack pointer

// System Calls
static constexpr quint32 FUNC_Tru64_CALLSYS_ = 0x0014; // Call system service

// Software Interrupts
static constexpr quint32 FUNC_Tru64_SSIR_ = 0x0015; // Set software interrupt request

// Inter-Processor Communication
static constexpr quint32 FUNC_Tru64_WRIPIR_ = 0x0016; // Write inter-processor interrupt request

// Exception Handling
static constexpr quint32 FUNC_Tru64_RFE_ = 0x0017;    // Return from exception
static constexpr quint32 FUNC_Tru64_RETSYS_ = 0x0018; // Return from system call

// Status and Control
static constexpr quint32 FUNC_Tru64_RDPSR_ = 0x0019; // Read processor status register

// Thread Management
static constexpr quint32 FUNC_Tru64_RDTHREAD_ = 0x001A; // Read thread ID
static constexpr quint32 FUNC_Tru64_SWPCTX_ = 0x001B;   // Swap context

// Floating Point Control
static constexpr quint32 FUNC_Tru64_WRFEN_ = 0x001C; // Write floating enable

// Interrupt Return
static constexpr quint32 FUNC_Tru64_RTI_ = 0x001D; // Return from interrupt

// Unique Value Management
static constexpr quint32 FUNC_Tru64_RDUNIQUE_ = 0x001E; // Read unique value
static constexpr quint32 FUNC_Tru64_WRUNIQUE_ = 0x001F; // Write unique value

// =============================================================================
// OPENVMS PAL FUNCTIONS (would be defined here if needed)
// =============================================================================
// OpenVMS uses its own set of PAL codes - could be added if supporting VMS

// =============================================================================
// WINDOWS NT ALPHA PAL FUNCTIONS (would be defined here if needed)
// =============================================================================
// Windows NT Alpha used different PAL codes - could be added if needed

// =============================================================================
// UTILITY MACROS AND HELPERS
// =============================================================================

// Extract PAL function from CALL_PAL instruction
#define EXTRACT_PAL_FUNCTION(instruction) ((instruction) & 0x3FFFFFF)

// Create CALL_PAL instruction from function code
#define CREATE_CALL_PAL(function) (0x00000000 | ((function) & 0x3FFFFFF))

// Check if instruction is CALL_PAL
#define IS_CALL_PAL(instruction) (((instruction) >> 26) == 0x00)

// PAL function classification helpers
inline bool isPALFunctionCommon(quint32 function) { return (function >= 0x0000 && function <= 0x00FF); }

inline bool isPALFunctionAlpha(quint32 function)
{
    return (function >= 0x0000 && function <= 0x001F) || (function >= 0x0080 && function <= 0x00BF);
}

inline bool isPALFunctionTru64(quint32 function) { return (function >= 0x0000 && function <= 0x003F); }

// PAL function name lookup (for debugging)
const char *getPALFunctionName(quint32 function);

// =============================================================================
// IMPLEMENTATION NOTES
// =============================================================================
/*
 * PAL Function Numbering:
 * - Each Alpha implementation defines its own PAL function codes
 * - Some codes overlap between implementations but have different meanings
 * - CALL_PAL instruction format: bits 25:0 contain the function code
 * - Function codes 0x00-0x3F are typically reserved for privileged functions
 * - Function codes 0x80-0xBF are often used for system calls
 * - Function codes 0x40-0x7F and 0xC0-0xFF may be implementation-specific
 *
 * Usage in Emulator:
 * - Use these constants in PAL instruction decoding
 * - Switch statements in PAL executors should use these constants
 * - Function classification helps with optimization and scheduling
 * - SMP coordination may require different handling per function type
 */