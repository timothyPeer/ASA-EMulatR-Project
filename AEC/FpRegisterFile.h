#pragma once
// FpRegisterFile.h  (header-only)
#pragma once
#include <cstdint>
#include <type_traits>

/*

a fully-typed floating-point register file for Alpha AXP:

FReg           - the architectural numbers (F0 – F31).
FAlias         - the ABI nick-names fv0, ft0, fs0, fa0, ...  exactly as specified in the Alpha calling convention.
FpcrRegister   - the 64-bit FP control/status register (same code I gave earlier, included here for convenience).
FpRegs         - a union overlay that lets you access registers by ABI alias, by architectural index, or through a raw
                 uint64_t[32] array. The last element (fpcr) is bound to raw[31], matching the 
                 architecture (F31 doubles as FPCR).
*
FPCRRegister that models every field the Alpha AXP architecture defines in the 64-bit FPCR (Floating-Point Control / Status Register):

2-bit rounding mode (RM)
1-bit underflow trap enable (UNF)
1-bit overflow trap enable (OVF)
1-bit divide-by-zero trap enable (DZE)
1-bit invalid-op trap enable (INV)
1-bit inexact trap enable (INE)
Sticky status flags: UNF/OVF/DZE/INV/INE
Condition-code (COND) and 'flush-to-zero in sub-normals' (SIS) bits used by PALcode/SRM

You get:
uint64_t value() / void setValue(uint64_t) raw access
Field-specific getters/setters (roundingMode(), setRoundingMode(), isOverflowSticky(), clearStatus(), )
Enum RoundingMode (RoundNearest, RoundMinusInf, ...)

*/


/* -----------------------------------------------------------------------
 * 1. Architectural FP register numbers (F0-F31)
 * ------------------------------------------------------------------- */
enum FReg : uint8_t {
    F0, F1, F2, F3, F4, F5, F6, F7,
    F8, F9, F10, F11, F12, F13, F14, F15,
    F16, F17, F18, F19, F20, F21, F22, F23,
    F24, F25, F26, F27, F28, F29, F30, F31
};

/* -----------------------------------------------------------------------
 * 2. ABI aliases (OSF/1 & Tru64 calling convention)
 * ------------------------------------------------------------------- */
enum FAlias : uint8_t {
    fv0 = F0,
    fv1 = F1,

    ft0 = F2, ft1 = F3, ft2 = F4, ft3 = F5,
    ft4 = F6, ft5 = F7, ft6 = F8, ft7 = F9,

    fs0 = F10, fs1 = F11, fs2 = F12, fs3 = F13, fs4 = F14, fs5 = F15,

    fa0 = F16, fa1 = F17, fa2 = F18, fa3 = F19, fa4 = F20, fa5 = F21,

    ft8 = F22, ft9 = F23, ft10 = F24, ft11 = F25,
    ft12 = F26, ft13 = F27, ft14 = F28, ft15 = F29,

    /* F30 is temporary / scratch (no ABI alias) */
    fpcr = F31          ///< Alias for FP control register
};





