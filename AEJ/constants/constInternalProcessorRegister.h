#pragma once
#include <QtCore>


// 
// Internal Processor Register (IPR) Numbers - EV4/EV5
// 

constexpr quint32 IPR_EV4_EV5_ICSR = 0x01;     // Ibox Control/Status Register
constexpr quint32 IPR_EV4_EV5_IBOX = 0x02;     // Ibox Register
constexpr quint32 IPR_EV4_EV5_ICCSR = 0x03;    // Icache Control/Status Register
constexpr quint32 IPR_EV4_EV5_ITB_PTE = 0x04;  // ITB Page Table Entry
constexpr quint32 IPR_EV4_EV5_DTB_PTE = 0x05;  // DTB Page Table Entry
constexpr quint32 IPR_EV4_EV5_PS = 0x06;       // Processor Status
constexpr quint32 IPR_EV4_EV5_EXC_ADDR = 0x07; // Exception Address
constexpr quint32 IPR_EV4_EV5_EXC_SUM = 0x08;  // Exception Summary
constexpr quint32 IPR_EV4_EV5_PAL_BASE = 0x09; // PAL Base Address
constexpr quint32 IPR_EV4_EV5_HIRR = 0x0A;     // Hardware Interrupt Request
constexpr quint32 IPR_EV4_EV5_SIRR = 0x0B;     // Software Interrupt Request
constexpr quint32 IPR_EV4_EV5_ASTRR = 0x0C;    // AST Request Register

// 
// Internal Processor Register (IPR) Numbers - EV6
// 

constexpr quint32 IPR_EV6_IVA_FORM = 0x00;     // IVA Form Register
constexpr quint32 IPR_EV6_IER_CM = 0x01;       // Interrupt Enable Current Mode
constexpr quint32 IPR_EV6_SIRR = 0x02;         // Software Interrupt Request
constexpr quint32 IPR_EV6_ISUM = 0x03;         // Interrupt Summary
constexpr quint32 IPR_EV6_HW_INT_CLR = 0x04;   // Hardware Interrupt Clear
constexpr quint32 IPR_EV6_EXC_ADDR = 0x05;     // Exception Address
constexpr quint32 IPR_EV6_IC_PERR_STAT = 0x06; // Icache Parity Error Status
constexpr quint32 IPR_EV6_IC_PERR_ADDR = 0x07; // Icache Parity Error Address
constexpr quint32 IPR_EV6_PMCTR = 0x08;        // Performance Counter
constexpr quint32 IPR_EV6_PAL_BASE = 0x09;     // PAL Base Address
constexpr quint32 IPR_EV6_I_CTL = 0x0A;        // Istream Control
constexpr quint32 IPR_EV6_PCTR_CTL = 0x0B;     // Performance Counter Control
constexpr quint32 IPR_EV6_CLR_MAP = 0x0C;      // Clear Map Register
constexpr quint32 IPR_EV6_I_STAT = 0x0D;       // Istream Status
constexpr quint32 IPR_EV6_SLEEP = 0x0E;        // Sleep Register


// 
// Internal Processor Register (IPR) Numbers - EV7
// 

constexpr quint32 IPR_EV7_IVA_FORM = 0x00; // IVA Form Register
constexpr quint32 IPR_EV7_IER = 0x01;      // Interrupt Enable Register
constexpr quint32 IPR_EV7_SIRR = 0x02;     // Software Interrupt Request
constexpr quint32 IPR_EV7_ISUM = 0x03;     // Interrupt Summary
constexpr quint32 IPR_EV7_EXC_ADDR = 0x04; // Exception Address
constexpr quint32 IPR_EV7_EXC_SUM = 0x05;  // Exception Summary
constexpr quint32 IPR_EV7_EXC_MASK = 0x06; // Exception Mask
constexpr quint32 IPR_EV7_PAL_BASE = 0x07; // PAL Base Address
constexpr quint32 IPR_EV7_I_CTL = 0x08;    // Istream Control
constexpr quint32 IPR_EV7_I_STAT = 0x09;   // Istream Status
constexpr quint32 IPR_EV7_DC_CTL = 0x0A;   // Dcache Control
constexpr quint32 IPR_EV7_DC_STAT = 0x0B;  // Dcache Status
constexpr quint32 IPR_EV7_C_DATA = 0x0C;   // Cache Data Register
constexpr quint32 IPR_EV7_C_SHIFT = 0x0D;  // Cache Shift Register
constexpr quint32 IPR_EV7_PMCTR0 = 0x10;   // Performance Counter 0
constexpr quint32 IPR_EV7_PMCTR1 = 0x11;   // Performance Counter 1
constexpr quint32 IPR_EV7_PMCTR2 = 0x12;   // Performance Counter 2
constexpr quint32 IPR_EV7_PMCTR3 = 0x13;   // Performance Counter 3
