#pragma once

/*
Feature	Bits	Behavior
Trap enables	0–4	If set, traps occur on that exception
Sticky flags	5–9	Set whenever corresponding exception occurs
Rounding mode	57–58	Controls rounding behavior (nearest, zero, +inf, −inf)

*/

#ifndef FPCRCONTEXT_H
#define FPCRCONTEXT_H

#include <QtGlobal>
#include "..\AEJ\enumerations\enumRoundingMode.h"

/**
 * @brief Alpha AXP FPCR context representation.
 * Supports trap enables, sticky flags, and rounding mode.
 * Based on Alpha AXP Architecture Reference Manual §4.9.6–4.9.7.
 */
struct FPCRContext {
    quint64 raw = 0; ///< Full 64-bit FPCR image.

    // ===== Trap Enable Accessors =====

    bool trapInexact() const { return raw & (1ULL << 0); }
    bool trapUnderflow() const { return raw & (1ULL << 1); }
    bool trapOverflow() const { return raw & (1ULL << 2); }
    bool trapDivZero() const { return raw & (1ULL << 3); }
    bool trapInvalid() const { return raw & (1ULL << 4); }

    // ===== Rounding Mode Accessor =====

    /**
     * @brief Get rounding mode (bits 57-58).
     * 0 = Round to nearest
     * 1 = Round toward zero
     * 2 = Round toward +infinity
     * 3 = Round toward -infinity
     */
    quint8 roundingMode() const {
        return static_cast<quint8>((raw >> 57) & 0x3);
    }

    // ===== Sticky Flag Checkers =====

    bool stickyInexact() const { return raw & (1ULL << 5); }
    bool stickyUnderflow() const { return raw & (1ULL << 6); }
    bool stickyOverflow() const { return raw & (1ULL << 7); }
    bool stickyDivZero() const { return raw & (1ULL << 8); }
    bool stickyInvalid() const { return raw & (1ULL << 9); }

    // ===== Sticky Flag Setters =====

    void setStickyInexact() { raw |= (1ULL << 5); }
    void setStickyUnderflow() { raw |= (1ULL << 6); }
    void setStickyOverflow() { raw |= (1ULL << 7); }
    void setStickyDivZero() { raw |= (1ULL << 8); }
    void setStickyInvalid() { raw |= (1ULL << 9); }

    // ===== Helper Methods =====

    void clearStickyFlags() {
        raw &= ~(0x3E0ULL); // Clear bits 5–9 (sticky bits)
    }

    void setRoundingMode(quint8 mode) {
        raw &= ~(0x3ULL << 57);         // Clear bits 57–58
        raw |= (quint64(mode & 0x3) << 57); // Set new rounding mode
    }
};

#endif // FPCRCONTEXT_H
