#pragma once
#include <QtCore>

constexpr quint32 FUNC_Alpha_AND = 0x0;      //   AND
constexpr quint32 FUNC_Alpha_BIC = 0x8;      //   BIC
constexpr quint32 FUNC_Alpha_CMOVLBS = 0x14; //   CMOVLBS
constexpr quint32 FUNC_Alpha_CMOVLBC = 0x16; //   CMOVLBC
constexpr quint32 FUNC_Alpha_BIS = 0x20;     //   BIS
constexpr quint32 FUNC_Alpha_CMOVEQ = 0x24;  //   CMOVEQ
constexpr quint32 FUNC_Alpha_CMOVNE = 0x26;  //   CMOVNE
constexpr quint32 FUNC_Alpha_ORNOT = 0x28;   //   ORNOT
constexpr quint32 FUNC_Alpha_XOR = 0x40;     //   XOR
constexpr quint32 FUNC_Alpha_CMOVLT = 0x44;  //   CMOVLT
constexpr quint32 FUNC_Alpha_CMOVGE = 0x46;  //   CMOVGE
constexpr quint32 FUNC_Alpha_EQV = 0x48;     //   EQV
constexpr quint32 FUNC_Alpha_CMOVLE = 0x64;  //   CMOVLE
constexpr quint32 FUNC_Alpha_CMOVGT = 0x66;  //   CMOVGT
