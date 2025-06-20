#pragma once
#include <QObject>

static constexpr quint64 JIT_FAULT_SENTINEL = ~0ULL;    // 0xFFFF…FFFF  -- needed in interpret block otherwise 
														//will endless loop with unsupported instructions.

