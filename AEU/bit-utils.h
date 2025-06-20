#pragma once
#ifndef BIT_UTILS_H
#define BIT_UTILS_H

/**
 * @file bit-utils.h
 * @brief Miscellaneous bit?manipulation helpers for the Alpha AXP emulator.
 *
 * Header?only, no external dependencies except <QtGlobal> on Qt?based builds
 * (for Q_ASSERT).  All functions are constexpr/inline so they impose zero
 * linkage overhead.  They fall back to portable C implementations when the
 * compiler’s builtin intrinsics are unavailable.
 *
 * © 2025  Timothy Peer & contributors – MIT License
 */

#include <QtGlobal>
#include <cstdint>

namespace BitUtils {

    /**
     * @brief Count trailing zeros in a 64?bit value.
     * @param x  64?bit unsigned integer.
     * @return   Number of consecutive zero bits starting at LSB (0?64).
     *           Returns 64 when x == 0.
     */
    static inline int ctz64(quint64 x)
    {
#if defined(__GNUG__) || defined(__clang__)
        return x ? __builtin_ctzll(x) : 64;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned long idx;
        if (_BitScanForward64(&idx, x))
            return static_cast<int>(idx);
        return 64;
#else
        if (!x) return 64;
        int n = 0;
        while ((x & 1) == 0) {
            x >>= 1;
            ++n;
        }
        return n;
#endif
    }

    /**
     * @brief Count leading zeros in a 64?bit value.
     */
    static inline int clz64(quint64 x)
    {
#if defined(__GNUG__) || defined(__clang__)
        return x ? __builtin_clzll(x) : 64;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned long idx;
        if (_BitScanReverse64(&idx, x))
            return 63 - static_cast<int>(idx);
        return 64;
#else
        if (!x) return 64;
        int n = 0;
        quint64 mask = 1ULL << 63;
        while ((x & mask) == 0) {
            mask >>= 1;
            ++n;
        }
        return n;
#endif
    }

    /**
     * @brief Population count (Hamming weight) of a 64?bit value.
     */
    static inline int popcount64(quint64 x)
    {
#if defined(__GNUG__) || defined(__clang__)
        return __builtin_popcountll(x);
#else
        int count = 0;
        while (x) {
            x &= (x - 1);
            ++count;
        }
        return count;
#endif
    }

    /**
     * @brief True if x is a non?zero power of two.
     */
    static inline bool isPowerOfTwo(quint64 x) { return x && !(x & (x - 1)); }
   // static inline bool isPowerOfTwo(size_t x) { return x && !(x & (x - 1)); }

    /**
     * @brief Align value up to the next multiple of @p alignment (alignment must be power?of?two).
     */
    static inline quint64 alignUp(quint64 value, quint64 alignment)
    {
        Q_ASSERT(isPowerOfTwo(alignment));
        return (value + alignment - 1) & ~(alignment - 1);
    }

    /**
     * @brief Align value down to a multiple of @p alignment (alignment must be power?of?two).
     */
    static inline quint64 alignDown(quint64 value, quint64 alignment)
    {
        Q_ASSERT(isPowerOfTwo(alignment));
        return value & ~(alignment - 1);
    }

    /**
     * @brief Rotate left 64?bit value by r bits.
     */
	static inline quint64 rol64(quint64 x, unsigned int r)
	{
		const unsigned int mask = 63;
		r &= mask;
		return (x << r) | (x >> ((64 - r) & mask)); // Use subtraction instead of negation
	}

    /**
     * @brief Rotate right 64?bit value by r bits.
     */
    static inline quint64 ror64(quint64 x, unsigned int r)
    {
        const unsigned int mask = 63;
        r &= mask;
        return (x >> r) | (x << ((64-r) & mask));
    }

} // namespace BitUtils


#endif // BIT_UTILS_H
