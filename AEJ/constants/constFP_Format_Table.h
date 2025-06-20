#pragma once
#include <QObject>
#include "../AEJ/structures/structFPFormatInfo.h"

// Format characteristics lookup table
static const FPFormatInfo FP_FORMAT_TABLE[] = {
    // VAX formats
    {32, 8, 23, 1, 128, true, false, "VAX F_floating"},      // VAX_F_FORMAT
    {64, 11, 52, 1, 1024, true, false, "VAX G_floating"},    // VAX_G_FORMAT
    {64, 8, 55, 1, 128, true, false, "VAX D_floating"},      // VAX_D_FORMAT
    {128, 15, 112, 1, 16384, true, false, "VAX H_floating"}, // VAX_H_FORMAT

    // IEEE formats
    {32, 8, 23, 1, 127, true, true, "IEEE Single"},    // IEEE_S_FORMAT
    {64, 11, 52, 1, 1023, true, true, "IEEE Double"},  // IEEE_T_FORMAT
    {128, 15, 112, 1, 16383, true, true, "IEEE Quad"}, // IEEE_Q_FORMAT

    // Integer formats
    {32, 0, 31, 1, 0, false, false, "32-bit Integer"}, // INTEGER_LONG
    {64, 0, 63, 1, 0, false, false, "64-bit Integer"}, // INTEGER_QUAD
};