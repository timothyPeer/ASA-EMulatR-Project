#pragma once

#pragma once
/*--------------------------------------------------------------------
 *  EV-level selector for PAL/opcode constants
 *
 *  Usage:
 *      // compile with -DCPU_EV=5  (for EV5 / 21164)
 *      constexpr quint32 PAL_SWPCTX = EV_SELECT(0x04, 0x05, 0x04, 0x04, 0x04, 0x04);
 *
 *  If CPU_EV is not supplied on the command line or build system,
 *  you can set a default below.
 *------------------------------------------------------------------*/

//------------------------------------------------------------------
// 1) Define which EV we are building for
// 
//------------------------------------------------------------------

#ifndef CPU_EV
#define CPU_EV 6 /* default to EV6 if caller forgot */
#endif

#if (CPU_EV == 4)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV4)
#elif (CPU_EV == 5)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV5)
#elif (CPU_EV == 6)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV6)
#elif (CPU_EV == 67)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV67)
#elif (CPU_EV == 68)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV68)
#elif (CPU_EV == 7)
#define EV_SELECT(EV4, EV5, EV6, EV67, EV68, EV7) (EV7)
#else
#error "Unknown CPU_EV value (valid: 4,5,6,67,68,7)"
#endif

/* Selects a function constant only if the target EV level supports it.
   Example:
       case EV_SELECT(FUNC_ADDQV, EV6): …    // emitted only when CPU ≥ EV6
*/
//#define EV_SELECT(CONST, MIN_EV) ((CPU_EV >= (MIN_EV)) ? (CONST) : 0xFFFFFFFFu)
