#pragma once
#include <QtCore>

constexpr quint32 EXCEPTION_CAUSE_AST = 0x01;
constexpr quint32 EXCEPTION_CAUSE_INTERRUPT = 0x02;
constexpr quint32 EXCEPTION_CAUSE_MACHINE_CHECK = 0x03;
constexpr quint32 EXCEPTION_CAUSE_ALIGNMENT = 0x04;
constexpr quint32 EXCEPTION_CAUSE_ILLEGAL_INSTR = 0x05;
constexpr quint32 EXCEPTION_CAUSE_UNKNOWN = 0xFF;