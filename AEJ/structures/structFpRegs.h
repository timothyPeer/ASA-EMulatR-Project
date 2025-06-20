#pragma once
#include <QtCore>
#include "FpRegisterFile.h"

/**
 * @file FpcrRegister.H
 * @brief Emulates the Alpha AXP Floating Point Control Register (FPCR).
 *
 * Contains exception control, rounding mode, and condition codes.
 * Reference: Alpha AXP Architecture Reference Manual §4.7.3
 */

  // IEEE FP Compare Condition Code Bits (bits 21–24)
static constexpr quint64 FPCC_LT_BIT = (1ull << 21); ///< Less Than
static constexpr quint64 FPCC_EQ_BIT = (1ull << 22); ///< Equal
static constexpr quint64 FPCC_GT_BIT = (1ull << 23); ///< Greater Than
static constexpr quint64 FPCC_UN_BIT = (1ull << 24); ///< Unordered
static constexpr quint64 FPCC_MASK = FPCC_LT_BIT | FPCC_EQ_BIT | FPCC_GT_BIT | FPCC_UN_BIT;

struct FpRegs
{
  public:

    union
    {
        quint64 raw[32];     // Raw integer backing
        double asDouble[32]; // FP view of registers
    };


     // Backing store for FPCR contents
    quint64 fpcrWord_ = 0;

  

    // Getter
    inline void getFPConditionFlags(bool &lt, bool &eq, bool &gt, bool &un) const
    {
        lt = (fpcrWord_ & FPCC_LT_BIT) != 0;
        eq = (fpcrWord_ & FPCC_EQ_BIT) != 0;
        gt = (fpcrWord_ & FPCC_GT_BIT) != 0;
        un = (fpcrWord_ & FPCC_UN_BIT) != 0;
    }

    // Setter
    inline void setFPConditionFlags(bool lt, bool eq, bool gt, bool un)
    {
        fpcrWord_ &= ~FPCC_MASK; // clear
        if (lt)
            fpcrWord_ |= FPCC_LT_BIT;
        if (eq)
            fpcrWord_ |= FPCC_EQ_BIT;
        if (gt)
            fpcrWord_ |= FPCC_GT_BIT;
        if (un)
            fpcrWord_ |= FPCC_UN_BIT;
    }

    // Raw accessor (if needed)
    quint64 rawWord() const { return fpcrWord_; }
    void setRawWord(quint64 val) { fpcrWord_ = val; }
    // ABI-named references
    quint64 &fv0 = raw[0];
    quint64 &fv1 = raw[1];

    quint64 &ft0 = raw[2];
    quint64 &ft1 = raw[3];
    quint64 &ft2 = raw[4];
    quint64 &ft3 = raw[5];
    quint64 &ft4 = raw[6];
    quint64 &ft5 = raw[7];
    quint64 &ft6 = raw[8];
    quint64 &ft7 = raw[9];

    quint64 &fs0 = raw[10];
    quint64 &fs1 = raw[11];
    quint64 &fs2 = raw[12];
    quint64 &fs3 = raw[13];
    quint64 &fs4 = raw[14];
    quint64 &fs5 = raw[15];

    quint64 &fa0 = raw[16];
    quint64 &fa1 = raw[17];
    quint64 &fa2 = raw[18];
    quint64 &fa3 = raw[19];
    quint64 &fa4 = raw[20];
    quint64 &fa5 = raw[21];

    quint64 &ft8 = raw[22];
    quint64 &ft9 = raw[23];
    quint64 &ft10 = raw[24];
    quint64 &ft11 = raw[25];
    quint64 &ft12 = raw[26];
    quint64 &ft13 = raw[27];
    quint64 &ft14 = raw[28];
    quint64 &ft15 = raw[29];

    quint64 &scratch = raw[30];

   	FpcrRegister &fpcr = *reinterpret_cast<FpcrRegister *>(&raw[31]);

    // Constructor to zero all values
    FpRegs()
    {
        for (int i = 0; i < 32; ++i)
            raw[i] = 0;
    }
};
