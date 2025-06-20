#pragma once

#include <QtGlobal>
#include <cstring>

// Alpha AXP FPCR (Floating-point Control Register)
// Architecture Reference: Alpha AXP Architecture Handbook, Chapter 11
struct FpcrRegister
{
    union
    {
        quint64 raw;
        struct
        {
            quint64 rnd : 2;          // Rounding mode (00=Chopped, 01=-Inf, 10=+Inf, 11=Nearest)
            quint64 dynRND : 2;       // Dynamic rounding mode (used in CVT operations)
            quint64 underflow : 1;    // Underflow trap enable
            quint64 overflow : 1;     // Overflow trap enable
            quint64 divideByZero : 1; // Divide-by-zero trap enable
            quint64 inexact : 1;      // Inexact trap enable
            quint64 invalidOp : 1;    // Invalid operation trap enable
            quint64 unused1 : 3;      // Reserved / unused
            quint64 statusInv : 1;    // Invalid operation occurred
            quint64 statusDze : 1;    // Divide-by-zero occurred
            quint64 statusOfl : 1;    // Overflow occurred
            quint64 statusUfl : 1;    // Underflow occurred
            quint64 statusIox : 1;    // Inexact occurred
            quint64 unused2 : 47;     // Reserved
        } bits;
    };

    FpcrRegister() : raw(0) {}


    bool bitTest(unsigned b) const { return ((raw >> b) & 1ULL) != 0; }
    void clearBit(unsigned b) { raw &= ~(1ULL << b); }
    void setBit(unsigned b) { raw |= (1ULL << b); }

    // Read individual flags
    bool isTrapEnabled_InvalidOp() const { return bits.invalidOp; }
    bool isTrapEnabled_DivZero() const { return bits.divideByZero; }
    bool isTrapEnabled_Overflow() const { return bits.overflow; }
    bool isTrapEnabled_Underflow() const { return bits.underflow; }
    bool isTrapEnabled_Inexact() const { return bits.inexact; }

    // Set traps
    void setTrapEnabled_InvalidOp(bool enable) { bits.invalidOp = enable; }
    void setTrapEnabled_DivZero(bool enable) { bits.divideByZero = enable; }
    void setTrapEnabled_Overflow(bool enable) { bits.overflow = enable; }
    void setTrapEnabled_Underflow(bool enable) { bits.underflow = enable; }
    void setTrapEnabled_Inexact(bool enable) { bits.inexact = enable; }

    // Status flags
    bool status_InvalidOp() const { return bits.statusInv; }
    bool status_DivZero() const { return bits.statusDze; }
    bool status_Overflow() const { return bits.statusOfl; }
    bool status_Underflow() const { return bits.statusUfl; }
    bool status_Inexact() const { return bits.statusIox; }

    // Modify status
    void clearStatusFlags() { bits.statusInv = bits.statusDze = bits.statusOfl = bits.statusUfl = bits.statusIox = 0; }
    void raiseStatus_InvalidOp() { bits.statusInv = 1; }
    void raiseStatus_DivZero() { bits.statusDze = 1; }
    void raiseStatus_Overflow() { bits.statusOfl = 1; }
    void raiseStatus_Underflow() { bits.statusUfl = 1; }
    void raiseStatus_Inexact() { bits.statusIox = 1; }

    // Encoding/decoding helpers
    static FpcrRegister fromRaw(quint64 value)
    {
        FpcrRegister f;
        f.raw = value;
        return f;
    }

    quint64 toRaw() const { return raw; }

    static quint64 fromDouble(double value)
    {
        quint64 rawBits = 0;
        static_assert(sizeof(double) == sizeof(quint64), "Size mismatch for double and quint64");
        std::memcpy(&rawBits, &value, sizeof(double));
        return rawBits;
    }
    static double toDouble(quint64 rawBits)
    {
        double value = 0;
        std::memcpy(&value, &rawBits, sizeof(double));
        return value;
    }
};
