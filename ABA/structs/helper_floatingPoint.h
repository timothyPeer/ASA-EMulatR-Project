#pragma once
// FloatingPointOps.h
// Software implementations of Alpha AXP floating-point convert and sqrt operations
// for use in JitLoadExecutor.
// All functions are inline for maximum performance.
//
// References:
//   Table 4-13: Floating-Point Operate Instructions Summary (I), §4.10 (pp. 4-90–4-96) :contentReference[oaicite:3]{index=3}
//   IEEE Floating-Point Conformance: “Other IEEE operations (SQRT)” (Appendix B-2) :contentReference[oaicite:4]{index=4}

#ifndef FLOATING_POINT_OPS_H
#define FLOATING_POINT_OPS_H

#include <cstdint>
#include <cmath>
#include <bit>    // for std::bit_cast
#include "Assembler.h"
#include "floatingPointInstruction.h"

struct FloatingPointInstruction;

//-----------------------------------------------------------------------------
// Bit-cast helpers
//-----------------------------------------------------------------------------
inline float  bitsToFloat(uint32_t bits) { return std::bit_cast<float>(bits); }
inline uint32_t floatToBits(float f) { return std::bit_cast<uint32_t>(f); }
inline double bitsToDouble(uint64_t bits) { return std::bit_cast<double>(bits); }
inline uint64_t doubleToBits(double d) { return std::bit_cast<uint64_t>(d); }

//-----------------------------------------------------------------------------
// Masks and base function-code values (IEEE conversions, rounding = nearest even)
//-----------------------------------------------------------------------------
constexpr uint16_t FP_FNC_BASE_MASK = 0xFF8;  // drop the low-3 “qualifier” bits

// Base codes for IEEE convert (ignoring /S, /U, etc. qualifiers)
constexpr uint16_t CVTQS_BASE = 0x7BC;  // Quadword ? S_floating
constexpr uint16_t CVTQT_BASE = 0x7BE;  // Quadword ? T_floating
constexpr uint16_t CVTST_BASE = 0x2AC;  // S_floating ? T_floating
constexpr uint16_t CVTTQ_BASE = 0x7AF;  // T_floating ? Quadword
constexpr uint16_t CVTTS_BASE = 0x5AC;  // T_floating ? S_floating

// (If you have a hardware sqrt extension, fill in the real base codes here; 
// otherwise JIT will trap into PALcode or call this software routine)
constexpr uint16_t SQRTS_BASE = 0x000;   // placeholder: S_floating sqrt 
constexpr uint16_t SQRTT_BASE = 0x000;   // placeholder: T_floating sqrt

//-----------------------------------------------------------------------------
// fpConvert: implement CVTxx instructions in software.
// rawA is the 64-bit register value (bits interpreted per source format).
// fnc is the 11-bit function code field from bits<15:5> of the instruction.
// Returns the new 64-bit bit-pattern to write into the destination register.
//-----------------------------------------------------------------------------
inline uint64_t fpConvert(uint64_t rawA, uint16_t fnc) {
	uint16_t op = fnc & FP_FNC_BASE_MASK;
	switch (op) {
	case CVTQS_BASE: {  // Quadword ? single (IEEE)
		int64_t iv = static_cast<int64_t>(rawA);
		float    f = static_cast<float>(iv);
		return static_cast<uint64_t>(floatToBits(f));
	}
	case CVTQT_BASE: {  // Quadword ? double (IEEE)
		int64_t iv = static_cast<int64_t>(rawA);
		double   d = static_cast<double>(iv);
		return doubleToBits(d);
	}
	case CVTST_BASE: {  // single ? double (IEEE)
		uint32_t lo = static_cast<uint32_t>(rawA);
		float    f = bitsToFloat(lo);
		double   d = static_cast<double>(f);
		return doubleToBits(d);
	}
	case CVTTQ_BASE: {  // double ? quadword (IEEE)
		double   d = bitsToDouble(rawA);
		int64_t  iv = static_cast<int64_t>(d);
		return static_cast<uint64_t>(iv);
	}
	case CVTTS_BASE: {  // double ? single (IEEE)
		double   d = bitsToDouble(rawA);
		float    f = static_cast<float>(d);
		return static_cast<uint64_t>(floatToBits(f));
	}
	default:
		// Unimplemented conversion: in real JIT, trap or delegate to PALcode
		return rawA;
	}
}

//-----------------------------------------------------------------------------
// fpSqrt: implement SQRTxx instructions in software.
// rawA is the 64-bit register value (bit-pattern of S or T float).
// fnc is the function code; low-3 qualifier bits are used.
// Returns the new 64-bit bit-pattern of the sqrt result.
//-----------------------------------------------------------------------------
// Full fnc codes (Table 4-13, §4.10)
constexpr uint16_t SQRTS_FNC = 0x3D0;    // actual SQRTS function code
constexpr uint16_t SQRTT_FNC = 0x3D8;    // actual SQRTT function code

inline uint64_t fpSqrt(uint64_t rawA, uint16_t fnc) {
	switch (fnc) {
	case SQRTS_FNC: {
		// single-precision
		uint32_t  bits = static_cast<uint32_t>(rawA);
		float     f = std::bit_cast<float>(bits);
		float     r = std::sqrtf(f);
		uint32_t  rb = std::bit_cast<uint32_t>(r);
		// zero-extend to 64 bits:
		return static_cast<uint64_t>(rb);
	}
	case SQRTT_FNC: {
		// double-precision
		double    d = std::bit_cast<double>(rawA);
		double    r2 = std::sqrt(d);
		return std::bit_cast<uint64_t>(r2);
	}
	default:
		// unimplemented — trap into PAL or return rawA
		return rawA;
	}
}

inline uint64_t executeCVT(const FloatingPointInstruction& inst, uint64_t rawA) {
	return fpConvert(rawA, inst.fnc);
}

inline uint64_t executeSQRT(const FloatingPointInstruction& inst, uint64_t rawA) {
	return fpSqrt(rawA, inst.fnc);
}

/**
 * Emit a floating-point subtract (F_/G_/S_/T) using your Assembler.
 *
 * @param as     your Assembler instance
 * @param inst   already-loaded FloatingPointInstruction
 */
 /// Emit the SSE sequence for FP subtraction:
 ///   dest = srcA; dest -= srcB;
inline void fpSub(Assembler& assembler, const FloatingPointInstruction& inst) {
	auto i = inst;
	i.decode();

	uint8_t a = i.srcA();
	uint8_t b = i.srcB();
	uint8_t c = i.dest();

	if (isSinglePrecision(i.fnc)) {
		assembler.movss(c, a);
		assembler.subss(c, b);
	}
	else {
		assembler.movsd(c, a);
		assembler.subsd(c, b);
	}
}




#endif // FLOATING_POINT_OPS_H
