#pragma once
#pragma once
/**
 * @file AlphaFpcrFlags.h
 * @brief Alpha AXP Floating Point Control Register (FPCR) and FPCC Flags
 *
 * This header defines the bit positions of the FPCR, particularly the
 * floating-point condition code (FPCC) bits and associated helpers for use
 * in instruction implementations (e.g., CMPTEQ, CMPTEQ, FCMOVxx).
 *
 * Reference: Alpha Architecture Reference Manual, §4.7.9 (IEEE FPCR)
 */

#include <cstdint>

// ====== FPCR Bit Definitions ======
// Full 64-bit FPCR layout: [0–63]
// Bits 21–24: FPCC — Floating-Point Condition Codes
// static constexpr uint32_t FPCC_LT_BIT = (1u << 21); ///< Less Than
// static constexpr uint32_t FPCC_EQ_BIT = (1u << 22); ///< Equal
// static constexpr uint32_t FPCC_GT_BIT = (1u << 23); ///< Greater Than
// static constexpr uint32_t FPCC_UN_BIT = (1u << 24); ///< Unordered (NaN)

// IEEE FP compare flags: bits 21–24 in FPCR
static constexpr uint32_t FPCC_LT_BIT = (1u << 21); // Less Than
static constexpr uint32_t FPCC_EQ_BIT = (1u << 22); // Equal
static constexpr uint32_t FPCC_GT_BIT = (1u << 23); // Greater Than
static constexpr uint32_t FPCC_UN_BIT = (1u << 24); // Unordered (e.g., NaN)
static constexpr uint32_t FPCC_MASK = FPCC_LT_BIT | FPCC_EQ_BIT | FPCC_GT_BIT | FPCC_UN_BIT;

// ====== Composite FPCC Mask ======
static constexpr uint32_t FPCC_MASK = FPCC_LT_BIT | FPCC_EQ_BIT | FPCC_GT_BIT | FPCC_UN_BIT;

// ====== Utility Functions ======

/**
 * @brief Extract the current FPCC bits from FPCR.
 * @param fpcr 64-bit FPCR register value
 * @return 32-bit FPCC bits (21–24 masked)
 */
static inline uint32_t getFPConditionCodes(uint64_t fpcr)
{
    return static_cast<uint32_t>((fpcr >> 21) & 0xF); // Extract bits 21–24
}

/**
 * @brief Clear FPCC bits in FPCR.
 * @param fpcr 64-bit FPCR reference (modified in-place)
 */
static inline void clearFPConditionCodes(uint64_t &fpcr) { fpcr &= ~(static_cast<uint64_t>(FPCC_MASK)); }

/**
 * @brief Set FPCC flags in FPCR.
 * @param fpcr 64-bit FPCR reference
 * @param flags 32-bit FPCC bits (must be within FPCC_MASK)
 */
static inline void setFPConditionCodes(uint64_t &fpcr, uint32_t flags)
{
    clearFPConditionCodes(fpcr);
    fpcr |= (static_cast<uint64_t>(flags) & FPCC_MASK);
}

/**
 * @brief Check if current FPCC flags match "greater than or equal".
 * @param fpcr 64-bit FPCR value
 * @return true if FPCC indicates GT or EQ
 */
static inline bool fpccIsGE(uint64_t fpcr) { return (fpcr & (FPCC_GT_BIT | FPCC_EQ_BIT)) != 0; }
