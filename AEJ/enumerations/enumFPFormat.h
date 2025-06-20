#pragma once
#include <QtCore>
/**
 * @enum FPFormat
 * @brief Floating-point number formats supported by Alpha
 *
 * Alpha supports both VAX and IEEE floating-point formats.
 * Each format has different precision, range, and bit layouts.
 */
enum class FPFormat : quint8
{
    // VAX floating-point formats
    VAX_F_FORMAT = 0, ///< VAX F_floating (32-bit, proprietary)
    VAX_G_FORMAT = 1, ///< VAX G_floating (64-bit, proprietary)
    VAX_D_FORMAT = 2, ///< VAX D_floating (64-bit, different layout than G)
    VAX_H_FORMAT = 3, ///< VAX H_floating (128-bit, rarely used)

    // IEEE 754 formats
    IEEE_S_FORMAT = 4, ///< IEEE single precision (32-bit)
    IEEE_T_FORMAT = 5, ///< IEEE double precision (64-bit)
    IEEE_Q_FORMAT = 6, ///< IEEE quad precision (128-bit, limited support)

    // Integer formats (for conversions)
    INTEGER_LONG = 7, ///< 32-bit signed integer
    INTEGER_QUAD = 8, ///< 64-bit signed integer

    // Special values
    UNKNOWN_FORMAT = 255
};
