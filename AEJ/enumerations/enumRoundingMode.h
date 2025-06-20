#pragma once
/**
 * @enum RoundingMode
 * @brief IEEE 754 and Alpha-specific floating-point rounding modes
 *
 * Alpha supports all IEEE rounding modes plus some extensions.
 * These modes control how intermediate results are rounded to fit
 * in the target floating-point format.
 */
enum class RoundingMode : quint8
{
    // Standard IEEE 754 rounding modes
    ROUND_NEAREST_EVEN = 0,    ///< Round to nearest, ties to even (IEEE default)
    ROUND_TOWARD_ZERO = 1,     ///< Round toward zero (truncate/chop)
    ROUND_TOWARD_POSITIVE = 2, ///< Round toward +infinity (ceiling)
    ROUND_TOWARD_NEGATIVE = 3, ///< Round toward -infinity (floor)

    // Alpha-specific extensions
    ROUND_NEAREST_UP = 4, ///< Round to nearest, ties away from zero (unbiased)
    ROUND_DYNAMIC = 5,    ///< Use dynamic rounding mode from FPCR

    // Aliases for clarity
    ROUND_CHOPPED = ROUND_TOWARD_ZERO,
    ROUND_CEILING = ROUND_TOWARD_POSITIVE,
    ROUND_FLOOR = ROUND_TOWARD_NEGATIVE,
    ROUND_PLUS_INFINITY = ROUND_TOWARD_POSITIVE,
    ROUND_MINUS_INFINITY = ROUND_TOWARD_NEGATIVE,
    ROUND_NEAREST = ROUND_NEAREST_EVEN,
    ROUND_UNBIASED = ROUND_NEAREST_UP,
    EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    UNORDERED,
    NOT_EQUAL,
    NOT_LESS_THAN,
    NOT_LESS_EQUAL,
    ORDERED
};