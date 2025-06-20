#pragma once
#include <QtCore>

//---------------------------------
// Sign & Zero-Extend Helpers
//---------------------------------

/**
 * @brief Sign-extend an 8-bit value to 64 bits.
 */
static inline int64_t signExtend8(uint8_t v) {
    return static_cast<int64_t>(static_cast<int8_t>(v));
}

/**
 * @brief Sign-extend a 16-bit value to 64 bits.
 */
static inline int64_t signExtend16(uint16_t v) {
    return static_cast<int64_t>(static_cast<int16_t>(v));
}

/**
 * @brief Sign-extend a 32-bit value to 64 bits.
 */
static inline int64_t signExtend32(uint32_t v) {
    return static_cast<int64_t>(static_cast<int32_t>(v));
}

/**
 * @brief Zero-extend an 8-bit value to 64 bits.
 */
static inline uint64_t zeroExtend8(uint8_t v) {
    return static_cast<uint64_t>(v);
}

/**
 * @brief Zero-extend a 16-bit value to 64 bits.
 */
static inline uint64_t zeroExtend16(uint16_t v) {
    return static_cast<uint64_t>(v);
}

/**
 * @brief Zero-extend a 32-bit value to 64 bits.
 */
static inline uint64_t zeroExtend32(uint32_t v) {
    return static_cast<uint64_t>(v);
}

