#pragma once

#include <bit>
#include <cstdint>
#include <limits>

static inline uint32_t countTrailingZeros(uint32_t value)
{
#if defined(_MSC_VER)
    unsigned long index;
    if (_BitScanForward(&index, value))
        return index;
    else
        return std::numeric_limits<uint32_t>::digits; // undefined; all 0
#elif defined(__GNUC__) || defined(__clang__)
    return (value == 0) ? std::numeric_limits<uint32_t>::digits : __builtin_ctz(value);
#else
    // Portable fallback
    uint32_t count = 0;
    while ((value & 1) == 0 && count < 32)
    {
        value >>= 1;
        count++;
    }
    return count;
#endif
}