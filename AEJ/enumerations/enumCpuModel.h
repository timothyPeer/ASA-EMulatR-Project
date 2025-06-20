#pragma once
// CPU model constants
enum class CpuModel
{
    CPU_UNKNOWN = 0x0, 
    CPU_EV4 = 0x1,   // 21064
    CPU_EV5 = 0x2,   // 21164
    CPU_EV56 = 0x3,  // 21164A
    CPU_EV6 = 0x4,   // 21264
    CPU_EV67 = 0x5,  // 21264A
    CPU_EV68 = 0x6,  // 21264B
    CPU_EV7 = 0x7,   // 21364
    CPU_EV78 = 0x8,   // 21364
    CPU_PCA56 = 0x9
    // CPU_EV79 = 0x9   // 21364A - Not supported
};
