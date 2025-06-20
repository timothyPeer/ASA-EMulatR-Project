#pragma once
// IntRegisterFile.h
#pragma once
#include <cstdint>

/**
 * Architectural register numbers 0-31.
 */
enum Reg : uint8_t {
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13, R14, R15,
    R16, R17, R18, R19, R20, R21, R22, R23,
    R24, R25, R26, R27, R28, R29, R30, R31
};

/**
 * ABI names (OpenVMS/Tru64 calling convention).
 *
 * R27 = gp, R28 = sp, R26 = pv, etc.
 */
enum Alias : uint8_t {
    v0 = R0,

    t0 = R1,
    t1 = R2, t2 = R3, t3 = R4,
    t4 = R5, t5 = R6, t6 = R7,

    s0 = R9, s1 = R10, s2 = R11,
    s3 = R12, s4 = R13, s5 = R14,
    fp = R15,          // frame pointer

    a0 = R16,
    a1 = R17, a2 = R18, a3 = R19,
    a4 = R20, a5 = R21,

    t8 = R22, t9 = R23,
    t10 = R24, t11 = R25,

    pv = R26,          // procedure value / return address
    gp = R27,          // global pointer
    sp = R28,          // stack pointer

    fp_alt = R29,       // optional alt-fp
    zero = R30,       // reads 0, writes ignored
    ra = R31        // return address (JSR/BSR)
};

/**
 * Register file view that lets you use aliases *and*
 * raw indices against the same storage.
 */
struct IntRegs {
    // contiguous backing array
    uint64_t raw[32]{};

    // overlay with references for convenience
    union {
        struct {
            uint64_t v0;

            uint64_t t0, t1, t2, t3, t4, t5, t6;

            uint64_t s0, s1, s2, s3, s4, s5, fp;

            uint64_t a0, a1, a2, a3, a4, a5;

            uint64_t t8, t9, t10, t11;

            uint64_t pv, gp, sp, fp_alt, zero, ra;
        };
        uint64_t byIndex[32];   // same as raw
    };

    // constructor wires the alias refs onto raw[]
    constexpr IntRegs() : raw{},
        v0(raw[R0]),
        t0(raw[R1]), t1(raw[R2]), t2(raw[R3]), t3(raw[R4]), t4(raw[R5]), t5(raw[R6]), t6(raw[R7]),
        s0(raw[R9]), s1(raw[R10]), s2(raw[R11]), s3(raw[R12]), s4(raw[R13]), s5(raw[R14]), fp(raw[R15]),
        a0(raw[R16]), a1(raw[R17]), a2(raw[R18]), a3(raw[R19]), a4(raw[R20]), a5(raw[R21]),
        t8(raw[R22]), t9(raw[R23]), t10(raw[R24]), t11(raw[R25]),
        pv(raw[R26]), gp(raw[R27]), sp(raw[R28]), fp_alt(raw[R29]), zero(raw[R30]), ra(raw[R31])
    {
    }
};

