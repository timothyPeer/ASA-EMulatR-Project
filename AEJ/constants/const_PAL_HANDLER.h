#pragma once
#include <QtCore>
// PAL handler entry points
static constexpr quint64 PAL_HANDLER_ACCESS_VIOLATION = 0x100;
static constexpr quint64 PAL_HANDLER_FAULT_ON_READ = 0x200;
static constexpr quint64 PAL_HANDLER_TRANS_NOT_VALID = 0x300;
static constexpr quint64 PAL_HANDLER_ALIGNMENT_FAULT = 0x400;