#pragma once
#pragma once

#include "AlphaPlatformGuards.h"
#include <QtGlobal>

//==============================================================================
// WINDOWS NT PAL OPCODES
//==============================================================================
#ifdef ALPHA_PLATFORM_WINDOWS
namespace WindowsNT
{
// System operations
constexpr quint32 SWPCTX = 0x04; // Swap context
constexpr quint32 SWPPAL = 0x10; // Swap PAL code
constexpr quint32 RDMCES = 0x18; // Read machine check error summary
constexpr quint32 WRMCES = 0x19; // Write machine check error summary

// Memory operations
constexpr quint32 IMB = 0x26;    // Instruction memory barrier
///constexpr quint32 DRAINA = 0x2F; // Drain aborts (Common)

// CPU Control
constexpr quint32 RDIRQL = 0x29;   // Read interrupt request level
constexpr quint32 SWPIRQL = 0x2A;  // Swap interrupt request level
constexpr quint32 RDTHREAD = 0x2E; // Read thread ID

// Floating point operations
constexpr quint32 WRFEN = 0x2B; // Write floating point enable

// Memory management
constexpr quint32 TBIA = 0x40; // Translation buffer invalidate all
constexpr quint32 TBIM = 0x41; // Translation buffer invalidate multiple
constexpr quint32 PAL_TBIS = 0x42; // Translation buffer invalidate single

// System calls
constexpr quint32 GENTRAP = 0x9B; // Generate trap
constexpr quint32 DBGSTOP = 0x98; // Debug stop
constexpr quint32 LDQP = 0x86;    // Load quadword physical
constexpr quint32 STQP = 0x87;    // Store quadword physical

// Multiprocessor operations
constexpr quint32 CALLKD = 0xF0;     // Call kernel debugger
constexpr quint32 RDCOUNTERS = 0x80; // Read performance counters
constexpr quint32 RDSTATE = 0x7C;    // Read processor state
constexpr quint32 WRPERFMON = 0x39;  // Write performance monitor
}
#endif

//==============================================================================
// SRM CONSOLE/LINUX PAL OPCODES
//==============================================================================
#ifdef ALPHA_PLATFORM_SRM

#endif

//==============================================================================
// CUSTOM PLATFORM PAL OPCODES
//==============================================================================
#ifdef ALPHA_PLATFORM_CUSTOM
namespace Custom
{
// Define your custom PAL opcodes here
// This section can be modified for custom Alpha emulation targets

// Example PAL codes (replace with your own as needed)
constexpr quint32 SWPCTX = 0x00;  // Swap context
constexpr quint32 CALLSYS = 0x80; // System call
constexpr quint32 BPT = 0x81;     // Breakpoint

// Add more custom PAL codes as needed...
}
#endif