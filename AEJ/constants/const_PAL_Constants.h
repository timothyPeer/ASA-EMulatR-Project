#include <QtCore>
#include <QtCore>
#pragma once
#pragma once

constexpr quint32 BARRIER_TYPE_EXCB = 0x0400;  // Exception barrier
constexpr quint32 BARRIER_TYPE_MB = 0x4000;    // Standard memory barrier
constexpr quint32 BARRIER_TYPE_TRAPB = 0x0000; // Trap barrier
constexpr quint32 BARRIER_TYPE_WMB = 0x4400;   // Write memory barrier
constexpr quint32 FUNC_AMASK = 0x61;   // AMASK  (Ra & ~Rb) -> Rc
constexpr quint32 FUNC_ECB = 0xE800; // Evict Cache Block
constexpr quint32 FUNC_EXCB = 0x0400; // Exception Barrier
constexpr quint32 FUNC_FETCH = 0x8000;   // Prefetch data
constexpr quint32 FUNC_FETCH_M = 0xA000; // Prefetch data with modify intent
constexpr quint32 FUNC_IMPLVER = 0x6C; // IMPLVER – return CPU implementation version
constexpr quint32 FUNC_MB = 0x4000;    // Memory Barrier
constexpr quint32 FUNC_RC = 0xE000;      // Read and Clear
constexpr quint32 FUNC_RDPERF = 0x9000; // Read Performance Counter (implementation specific)
constexpr quint32 FUNC_RPCC = 0xC000;    // Read Process Cycle Counter
constexpr quint32 FUNC_RS = 0xF000; // Read and Set Lock Flag
constexpr quint32 FUNC_TRAPB = 0x4200; // Trap Barrier
constexpr quint32 FUNC_WMB = 0x4400;   // Write Memory Barrier
constexpr quint64 PAL_OFFSET_ACCESS_VIOLATION = 0x680; // Access control violation
constexpr quint64 PAL_OFFSET_ALIGNMENT_FAULT = 0x280;
constexpr quint64 PAL_OFFSET_ARITHMETIC_TRAP = 0x500;
constexpr quint64 PAL_OFFSET_AST = 0x480;
constexpr quint64 PAL_OFFSET_FP_EXCEPTION = 0x580;
constexpr quint64 PAL_OFFSET_ILLEGAL_INSTR = 0x300;
constexpr quint64 PAL_OFFSET_INTERRUPT = 0x400;
constexpr quint64 PAL_OFFSET_MACHINE_CHECK = 0x200;
constexpr quint64 PAL_OFFSET_PAGE_FAULT = 0x600;       // Translation fault
constexpr quint64 PAL_OFFSET_UNKNOWN = 0x700;