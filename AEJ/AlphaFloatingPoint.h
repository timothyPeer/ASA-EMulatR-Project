#pragma once// ═══════════════════════════════════════════════════════════════════════════
// MISSING STRUCTURES FOR ALPHA CPU FLOATING-POINT OPERATIONS
// These should be added to InstructionPipeLine.h or a new AlphaFloatingPoint.h
// ═══════════════════════════════════════════════════════════════════════════

#pragma once
#include <QtGlobal>
#include "AsaNamespaces.h"
#include "../AEJ/enumerations/enumFPFormat.h"
#include "../AEJ/enumerations/enumRoundingMode.h"
#include "../AEJ/constants/constVaxTypes.h"
#include "../AEJ/constants/constFP_Format_Table.h"
#include "../AEJ/structures/structFPFormatInfo.h"


//═══════════════════════════════════════════════════════════════════════════
//FLOATING-POINT EXCEPTION TYPES
//═══════════════════════════════════════════════════════════════════════════

/**
 * @enum FPException
 * @brief Floating-point exception types
 */
// enum class FPException : quint8
// {
//     FP_NO_EXCEPTION = 0,
//     FP_INVALID_OPERATION = 1, ///< Invalid operation (e.g., sqrt of negative)
//     FP_DIVISION_BY_ZERO = 2,  ///< Division by zero
//     FP_OVERFLOW = 3,          ///< Result too large for format
//     FP_UNDERFLOW = 4,         ///< Result too small for format
//     FP_INEXACT = 5,           ///< Result not exactly representable
//     FP_DENORMALIZED = 6,      ///< Denormalized operand or result
//     FP_UNIMPLEMENTED = 7,     ///< Unimplemented operation
// 
//     // Alpha-specific exceptions
//     FP_ARITHMETIC_TRAP = 8,    ///< Arithmetic trap
//     FP_SOFTWARE_COMPLETION = 9 ///< Requires software completion
// };

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS FOR FORMAT HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Get format information for a floating-point format
 * @param format The floating-point format
 * @return Pointer to format information structure
 */
inline const FPFormatInfo *getFPFormatInfo(FPFormat format)
{
    if (static_cast<quint8>(format) < sizeof(FP_FORMAT_TABLE) / sizeof(FP_FORMAT_TABLE[0]))
    {
        return &FP_FORMAT_TABLE[static_cast<quint8>(format)];
    }
    return nullptr;
}

/**
 * @brief Check if a format is a VAX floating-point format
 * @param format The floating-point format to check
 * @return True if VAX format, false otherwise
 */
inline bool isVAXFormat(FPFormat format)
{
    return format >= FPFormat::VAX_F_FORMAT && format <= FPFormat::VAX_H_FORMAT;
}

/**
 * @brief Check if a format is an IEEE floating-point format
 * @param format The floating-point format to check
 * @return True if IEEE format, false otherwise
 */
inline bool isIEEEFormat(FPFormat format)
{
    return format >= FPFormat::IEEE_S_FORMAT && format <= FPFormat::IEEE_Q_FORMAT;
}

/**
 * @brief Get the precision (number of significant bits) for a format
 * @param format The floating-point format
 * @return Number of significant bits including hidden bit
 */
inline quint8 getFPPrecision(FPFormat format)
{
    const FPFormatInfo *info = getFPFormatInfo(format);
    if (info && info->hasHiddenBit)
    {
        return info->mantissaBits + 1; // Add 1 for hidden bit
    }
    return info ? info->mantissaBits : 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// ROUNDING MODE UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Convert Alpha function code rounding mode to enum
 * @param functionCode The instruction function code
 * @return Corresponding rounding mode
 */
inline RoundingMode extractRoundingMode(quint32 functionCode)
{
    // Alpha encodes rounding mode in bits 7-6 of function code
    quint8 roundBits = (functionCode >> 6) & 0x3;

    switch (roundBits)
    {
    case 0x0:
        return RoundingMode::ROUND_CHOPPED; // /C
    case 0x1:
        return RoundingMode::ROUND_MINUS_INFINITY; // /M
    case 0x2:
        return RoundingMode::ROUND_NEAREST_EVEN; // default
    case 0x3:
        return RoundingMode::ROUND_PLUS_INFINITY; // /D
    default:
        return RoundingMode::ROUND_NEAREST_EVEN;
    }
}

/**
 * @brief Get string name for rounding mode
 * @param mode The rounding mode
 * @return Human-readable name
 */
inline const char *getRoundingModeName(RoundingMode mode)
{
    switch (mode)
    {
    case RoundingMode::ROUND_NEAREST_EVEN:
        return "Round to Nearest Even";
    case RoundingMode::ROUND_TOWARD_ZERO:
        return "Round Toward Zero";
    case RoundingMode::ROUND_TOWARD_POSITIVE:
        return "Round Toward +∞";
    case RoundingMode::ROUND_TOWARD_NEGATIVE:
        return "Round Toward -∞";
    case RoundingMode::ROUND_NEAREST_UP:
        return "Round to Nearest (Unbiased)";
    case RoundingMode::ROUND_DYNAMIC:
        return "Dynamic Rounding";
    default:
        return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// EXAMPLE USAGE IN ALPHACPU CLASS
// ═══════════════════════════════════════════════════════════════════════════

/*
// These methods would be added to the AlphaCPU class:

class AlphaCPU {
public:
    // Convert using specific format and rounding mode
    double convertWithRounding(double value, FPFormat targetFormat, RoundingMode mode);

    // Check if a value is denormalized in a specific format
    bool isDenormalized(double value, FPFormat format) const;

    // Get format-specific NaN value
    double getFormatNaN(FPFormat format) const;

    // Apply rounding mode to a value
    double applyRounding(double value, RoundingMode mode) const;

    // Format conversion functions with unbiased rounding support
    double convertToVaxFWithUnbiasedRounding(double value, RoundingMode mode);
    double convertToVaxGWithUnbiasedRounding(double value, RoundingMode mode);
    double convertToIeeeSWithUnbiasedRounding(double value, RoundingMode mode);
    double convertToIeeeTWithUnbiasedRounding(double value, RoundingMode mode);

private:
    // Current floating-point control register
    quint64 m_fpcr;

    // Extract rounding mode from FPCR
    RoundingMode getCurrentRoundingMode() const;

    // Set rounding mode in FPCR
    void setCurrentRoundingMode(RoundingMode mode);
};
*/

