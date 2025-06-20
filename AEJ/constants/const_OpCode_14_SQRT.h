#pragma once
#include <QtCore>

constexpr quint32 FUNC_VAX_SQRTF = 0x08A;           // Square Root F_floating (VAX F_FLOATING)
constexpr quint32 FUNC_VAX_SQRTG = 0x1AA;           // Square Root G_floating (VAX G_FLOATING)
constexpr quint32 FUNC_IEEE_SQRTS = 0x08B;           // Square Root F_floating (VAX F_FLOATING)
constexpr quint32 FUNC_IEEE_SQRTT = 0x1AB;           // Square Root G_floating (VAX G_FLOATING)
constexpr quint32 FUNC_IEEE_SQRTF_UC = 0x10A;   //   Floating-point square-root (single, UC rounding)
constexpr quint32 FUNC_IEEE_SQRTS_UC = 0x10B;   //   Floating-point square-root (single =>double, UC rounding)
constexpr quint32 FUNC_IEEE_SQRTG_UC = 0x12A;   //   Floating-point square-root (double =>quad, UC rounding)
constexpr quint32 FUNC_IEEE_SQRTT_UC = 0x12B;   //   Floating-point square-root (quad =>double, UC rounding)
constexpr quint32 FUNC_IEEE_SQRTS_UM = 0x14B;   //   Floating-point square-root (single =>double, UM rounding)
constexpr quint32 FUNC_IEEE_SQRTT_UM = 0x16B;   //   Floating-point square-root (quad =>double, UM rounding)
constexpr quint32 FUNC_IEEE_SQRTF_U = 0x18A;    //   Floating-point square-root (single, U rounding)
constexpr quint32 FUNC_IEEE_SQRTS_U = 0x18B;    //   Floating-point square-root (single =>double, U rounding)
constexpr quint32 FUNC_IEEE_SQRTG_U = 0x1AA;    //   Floating-point square-root (double =>quad, U rounding)
constexpr quint32 FUNC_IEEE_SQRTT_U = 0x1AB;    //   Floating-point square-root (quad =>double, U rounding)
constexpr quint32 FUNC_IEEE_SQRTS_UD = 0x1CB;   //   Floating-point square-root (single =>double, UD rounding)
constexpr quint32 FUNC_IEEE_SQRTT_UD = 0x1EB;   //   Floating-point square-root (quad =>double, UD rounding)
constexpr quint32 FUNC_IEEE_SQRTF_SC = 0x40A;   //   Floating-point square-root (single, SC rounding)
constexpr quint32 FUNC_IEEE_SQRTG_SC = 0x42A;   //   Floating-point square-root (double =>quad, SC rounding)
constexpr quint32 FUNC_IEEE_SQRTF_S = 0x48A;    //   Floating-point square-root (single, S rounding)
constexpr quint32 FUNC_IEEE_SQRTG_S = 0x4AA;    //   Floating-point square-root (double =>quad, S rounding)
constexpr quint32 FUNC_IEEE_SQRTF_SUC = 0x50A;  //   Floating-point square-root (single, SUC rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUC = 0x50B;  //   Floating-point square-root (single =>double, SUC rounding)
constexpr quint32 FUNC_IEEE_SQRTG_SUC = 0x52A;  //   Floating-point square-root (double =>quad, SUC rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUC = 0x52B;  //   Floating-point square-root (quad =>double, SUC rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUM = 0x54B;  //   Floating-point square-root (single =>double, SUM rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUM = 0x56B;  //   Floating-point square-root (quad =>double, SUM rounding)
constexpr quint32 FUNC_IEEE_SQRTF_SU = 0x58A;   //   Floating-point square-root (single, SU rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SU = 0x58B;   //   Floating-point square-root (single =>double, SU rounding)
constexpr quint32 FUNC_IEEE_SQRTG_SU = 0x5AA;   //   Floating-point square-root (double =>quad, SU rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SU = 0x5AB;   //   Floating-point square-root (quad =>double, SU rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUD = 0x5CB;  //   Floating-point square-root (single =>double, SUD rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUD = 0x5EB;  //   Floating-point square-root (quad =>double, SUD rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUIC = 0x70B; //   Floating-point square-root (single =>double, SUIC rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUIC = 0x72B; //   Floating-point square-root (quad =>double, SUIC rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUIM = 0x74B; //   Floating-point square-root (single =>double, SUIM rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUIM = 0x76B; //   Floating-point square-root (quad =>double, SUIM rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUI = 0x78B;  //   Floating-point square-root (single =>double, SUI rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUI = 0x7AB;  //   Floating-point square-root (quad =>double, SUI rounding)
constexpr quint32 FUNC_IEEE_SQRTS_SUID = 0x7CB; //   Floating-point square-root (single =>double, SUID rounding)
constexpr quint32 FUNC_IEEE_SQRTT_SUID = 0x7EB; //   Floating-point square-root (quad =>double, SUID rounding)   
constexpr quint32 FUNC_IEEE_SQRTT_S = FUNC_IEEE_SQRTG_SU;
    
    
// 
// /*
//     SQRT Constants
// 
// Every square-root variant in the ITFP group (primary-opcode 0x14)
// exactly as listed in the Alpha Architecture Reference Manual V4,
// Appendix C-6.
// 
// | Mnemonic   | Dotted code | **Opcode** | **Function (hex)** |
// | ---------- | ----------- | ---------- | ------------------ |
// | SQRTF/UC   | 14.10A      | 0x14       | **0x10A**          |
// | SQRTS/UC   | 14.10B      | 0x14       | **0x10B**          |
// | SQRTG/UC   | 14.12A      | 0x14       | **0x12A**          |
// | SQRTT/UC   | 14.12B      | 0x14       | **0x12B**          |
// | SQRTS/UM   | 14.14B      | 0x14       | **0x14B**          |
// | SQRTT/UM   | 14.16B      | 0x14       | **0x16B**          |
// | SQRTF/U    | 14.18A      | 0x14       | **0x18A**          |
// | SQRTS/U    | 14.18B      | 0x14       | **0x18B**          |
// | SQRTG/U    | 14.1AA      | 0x14       | **0x1AA**          |
// | SQRTT/U    | 14.1AB      | 0x14       | **0x1AB**          |
// | SQRTS/UD   | 14.1CB      | 0x14       | **0x1CB**          |
// | SQRTT/UD   | 14.1EB      | 0x14       | **0x1EB**          |
// | SQRTF/SC   | 14.40A      | 0x14       | **0x40A**          |
// | SQRTG/SC   | 14.42A      | 0x14       | **0x42A**          |
// | SQRTF/S    | 14.48A      | 0x14       | **0x48A**          |
// | SQRTG/S    | 14.4AA      | 0x14       | **0x4AA**          |
// | SQRTF/SUC  | 14.50A      | 0x14       | **0x50A**          |
// | SQRTS/SUC  | 14.50B      | 0x14       | **0x50B**          |
// | SQRTG/SUC  | 14.52A      | 0x14       | **0x52A**          |
// | SQRTT/SUC  | 14.52B      | 0x14       | **0x52B**          |
// | SQRTS/SUM  | 14.54B      | 0x14       | **0x54B**          |
// | SQRTT/SUM  | 14.56B      | 0x14       | **0x56B**          |
// | SQRTF/SU   | 14.58A      | 0x14       | **0x58A**          |
// | SQRTS/SU   | 14.58B      | 0x14       | **0x58B**          |
// | SQRTG/SU   | 14.5AA      | 0x14       | **0x5AA**          |
// | SQRTT/SU   | 14.5AB      | 0x14       | **0x5AB**          |
// | SQRTS/SUD  | 14.5CB      | 0x14       | **0x5CB**          |
// | SQRTT/SUD  | 14.5EB      | 0x14       | **0x5EB**          |
// | SQRTS/SUIC | 14.70B      | 0x14       | **0x70B**          |
// | SQRTT/SUIC | 14.72B      | 0x14       | **0x72B**          |
// | SQRTS/SUIM | 14.74B      | 0x14       | **0x74B**          |
// | SQRTT/SUIM | 14.76B      | 0x14       | **0x76B**          |
// | SQRTS/SUI  | 14.78B      | 0x14       | **0x78B**          |
// | SQRTT/SUI  | 14.7AB      | 0x14       | **0x7AB**          |
// | SQRTS/SUID | 14.7CB      | 0x14       | **0x7CB**          |
// | SQRTT/SUID | 14.7EB      | 0x14       | **0x7EB**          |
// */
// // ---------------------------------
// //  Missing ITFP SQRT rounded-mode variants
// //  Source: Alpha AXP System Reference Manual v6 (1994),
// //          Appendix C-6, ITFP Instruction Function Codes
// ”. 
// //
// //  NB:  * /C  = chopped-round (toward 0)
// //       * /M  = round toward ?
// //       * /D  = dynamic/+   (depends on FPCR, but software treats it as +)
// //  All values are the 11-bit function field that follows the 6-bit opcode 0x14.
// //  The relationship UC = C + 0x100, UM = M + 0x100, UD = D + 0x100
// //  is visible in the existing table (see lines for SQRT*_UC, SQRT*_UM, …)
// // ---------------------------------
// 
// // — Unbiased, chopped (/C)
// constexpr quint32 FUNC_SQRTF_C = 0x00A; // 14.00A  SQRTF/C
// constexpr quint32 FUNC_SQRTS_C = 0x00B; // 14.00B  SQRTS/C
// constexpr quint32 FUNC_SQRTG_C = 0x02A; // 14.02A  SQRTG/C
// constexpr quint32 FUNC_SQRTT_C = 0x02B; // 14.02B  SQRTT/C
// 
// // — Unbiased, round ?? (/M)
// constexpr quint32 FUNC_SQRTS_M = 0x04B; // 14.04B  SQRTS/M
// constexpr quint32 FUNC_SQRTT_M = 0x06B; // 14.06B  SQRTT/M
// 
// // — Unbiased, round +? (/D – “dynamic”)
// 
// constexpr quint32 FUNC_SQRTT_D = 0x0EB; // 14.0EB  SQRTT/D
// 
// //  ITFP  –  SQRT S_floating, scaled + unbiased (/SU)
// constexpr quint32 FUNC_SQRTS_SU = 0x58B; // 14.58B  SQRTS/SU
// 
// //  Compatibility alias – *not* an architecturally distinct instruction
// constexpr quint32 FUNC_SQRTS_S = FUNC_SQRTS_SU; // use /SU in place of /S
// 
// // ---------------------------------
// //   ITFP (opcode 0x14) Square-root of VAX / IEEE formats
// //   Naming:  FUNC_SQRT{F|S|G|T}_{mode}
// //            mode = U  (unbiased)   UC (unbiased + checked)
// //                 = UM (unbiased, round ??)   UD (unbiased, round +?)
// //                 = S  (scaled)     SC (scaled + checked)
// //                 = SU (scaled unbiased)      SUD  
// // ---------------------------------
// 
