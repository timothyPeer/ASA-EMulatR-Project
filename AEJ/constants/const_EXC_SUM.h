#pragma once
#include <QtCore>

// Exception registers that will be part of the ExcSum
static constexpr quint64 EXC_SUM_ACCESS_VIOLATION = 0x0001;
static constexpr quint64 EXC_SUM_FAULT_ON_READ = 0x0002;
static constexpr quint64 EXC_SUM_TRANS_NOT_VALID = 0x0004;
static constexpr quint64 EXC_SUM_ALIGNMENT_FAULT = 0x0008;

// Processor Status register bits
constexpr quint64 PS_INTERRUPT_ENABLE = 0x0001; // Bit 0 - IE
constexpr quint64 PS_PAL_MODE = 0x0002;         // Bit 1 - PAL
constexpr quint64 PS_CURRENT_MODE = 0x0018;     // Bits 3-4 - CM

// Current Mode field values (bits 3-4)
constexpr quint64 PS_MODE_KERNEL = 0x0000;     // 00 - Kernel mode
constexpr quint64 PS_MODE_EXECUTIVE = 0x0008;  // 01 - Executive mode
constexpr quint64 PS_MODE_SUPERVISOR = 0x0010; // 10 - Supervisor mode
constexpr quint64 PS_MODE_USER = 0x0018;       // 11 - User mode

// Utility masks and shifts
constexpr int PS_MODE_SHIFT = 3;
constexpr quint64 PS_MODE_MASK = 0x3; // 2-bit mask after shifting