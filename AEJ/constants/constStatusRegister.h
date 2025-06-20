#pragma once
#include <QtCore>

// Processor status register bits
constexpr quint64 PS_USER_MODE = 0x1;
constexpr quint64 PS_KERNEL_MODE = 0x0;
//constexpr quint64 PS_INTERRUPT_ENABLE = 0x2;
constexpr quint64 PS_EXCEPTION_MODE = 0x4;
constexpr quint64 PS_ARITHMETIC_TRAP_ENABLE = 0x8;
constexpr quint64 PS_FP_TRAP_ENABLE = 0x10;
constexpr quint64 PS_IE = 0x11; // AlphaCPU

