#pragma once
#include <QtCore>

/**
 * @struct FPFormatInfo
 * @brief Information about floating-point format characteristics
 */
struct FPFormatInfo
{
    quint8 totalBits;     ///< Total bits in format
    quint8 exponentBits;  ///< Number of exponent bits
    quint8 mantissaBits;  ///< Number of mantissa bits (excluding hidden bit)
    quint8 signBits;      ///< Number of sign bits (always 1)
    quint16 exponentBias; ///< Exponent bias value
    bool hasHiddenBit;    ///< True if format has hidden/implicit bit
    bool isIEEE;          ///< True for IEEE formats, false for VAX
    const char *name;     ///< Human-readable format name
};