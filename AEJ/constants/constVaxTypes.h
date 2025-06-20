#pragma once
#include <QtCore>



//==============================================================================
// VAX FLOATING-POINT (Opcode 0x15) - 11-bit function codes
//==============================================================================

// VAX F_floating operations


// constexpr quint32 FUNC_CMPFEQ = 0x125; // Compare F_floating Equal
// constexpr quint32 FUNC_CMPFLT = 0x126; // Compare F_floating Less Than
// constexpr quint32 FUNC_CMPFLE = 0x127; // Compare F_floating Less Than or Equal


// VAX D_floating operations (21164 and later)
constexpr quint32 FUNC_ADDD = 0x140;  // Add D_floating
constexpr quint32 FUNC_SUBD = 0x141;  // Subtract D_floating
constexpr quint32 FUNC_MULD = 0x142;  // Multiply D_floating
constexpr quint32 FUNC_DIVD = 0x143;  // Divide D_floating
constexpr quint32 FUNC_SQRTD = 0x14B; // Square Root (VAX D_floating)

/*
Below is the authoritative map for every VAX-format convert instruction in
the FLTV group (primary-opcode 0x15). The low-order 11-bit function field
is shown in hexadecimal; these values come straight from Appendix C-6 of the
Alpha Architecture Reference Manual (V4).

1?Quad - F / G-float converts
Mnemonic	Dotted code	Function	Notes
CVTQF /C	15.03C	    0x03C	    chopped (round-toward-0)
CVTQF	    15.0BC	    0x0BC	    round-to-nearest
CVTQF /UC	15.13C	    0x13C	    unbiased chopped (scaled)
CVTQG /C	15.03E	    0x03E	    chopped
CVTQG	    15.0BE	    0x0BE	    round-to-nearest
CVTQG /UC	15.13E	    0x13E	    scaled chopped

2?G-float - F / D / Q converts
Mnemonic	Dotted code	Function
CVTGF /C	15.02C	    0x02C
CVTGF	    15.0AC	    0x0AC
CVTGF /UC	15.12C	    0x12C
CVTGD /C	15.02D	    0x02D
CVTGD	    15.0AD	    0x0AD
CVTGD /UC	15.12D	    0x12D
CVTGQ /C	15.02F	    0x02F
CVTGQ	    15.0AF	    0x0AF
CVTGQ /VC   15.12F	    0x12F

The manual calls the scaled-and-checked variant '/VC' for this single
instruction; the value is still 0x100 + 0x02F.

3?F-float -> G-float / Quad converts
Mnemonic	Dotted code	    Function
CVTFG	    15.1AC	        0x1AC
CVTFQ	    15.1AF	        0x1AF
CVTGQ /V	15.15F	        0x15F
CVTFQ /V	15.1EF	        0x1EF

*/

// Quad ->  F,G
constexpr quint32 FUNC_CVTQF_C = 0x03C;
constexpr quint32 FUNC_CVTQF = 0x0BC;
constexpr quint32 FUNC_CVTQF_UC = 0x13C;

constexpr quint32 FUNC_CVTQG_C = 0x03E;
constexpr quint32 FUNC_CVTQG = 0x0BE;
constexpr quint32 FUNC_CVTQG_UC = 0x13E;

// G ? F,D,Q
constexpr quint32 FUNC_CVTGF_C = 0x02C;
constexpr quint32 FUNC_CVTGF = 0x0AC;
constexpr quint32 FUNC_CVTGF_UC = 0x12C;

constexpr quint32 FUNC_CVTGD_C = 0x02D;
constexpr quint32 FUNC_CVTGD = 0x0AD;
constexpr quint32 FUNC_CVTGD_UC = 0x12D;

constexpr quint32 FUNC_CVTGQ_C = 0x02F;
constexpr quint32 FUNC_CVTGQ = 0x0AF;
constexpr quint32 FUNC_CVTGQ_VC = 0x12F; // scaled-checked

// F ? G,Q
constexpr quint32 FUNC_CVTFG = 0x1AC;
constexpr quint32 FUNC_CVTFQ = 0x1AF;
constexpr quint32 FUNC_CVTGQ_V = 0x15F; // G?Q with /V
constexpr quint32 FUNC_CVTFQ_V = 0x1EF; // F?Q with /V

constexpr quint32 FUNC_CVTDG = 0x19E;  // Convert D_floating to G_floating
constexpr quint32 FUNC_CVTGQV = 0x15F; // Convert G_floating to Quadword with overflow
constexpr quint32 FUNC_CVTFQV = 0x1EF; // Convert F_floating to Quadword with overflow
