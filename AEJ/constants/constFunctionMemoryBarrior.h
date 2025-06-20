#pragma once
#include <QtCore>

// Memory and Exception Barriers
// constexpr quint32 FUNC_TRAPB = 0x0000; // Trap Barrier
constexpr quint32 FUNC_TRAPB = 0x4200; // Trap Barrier
// Memory barriers
constexpr quint32 FUNC_MB = 0x4000;  // Memory Barrier
constexpr quint32 FUNC_WMB = 0x4400; // Write Memory Barrier

/*AlphaBarrierExecutor.h*/