#pragma once
#include <QtCore>

//
// Hardware Instruction Format Notes
// 

/*
Hardware Instruction Formats:

HW_MFPR/HW_MTPR Format:
   31    26 25    21 20    16 15             5 4      0
  +--------+--------+--------+-----------------+--------+
  | Opcode |   Ra   |   --   |   IPR Number    |   --   |
  +--------+--------+--------+-----------------+--------+

HW_LD/HW_ST Format:
   31    26 25    21 20    16 15             5 4      0
  +--------+--------+--------+-----------------+--------+
  | Opcode |   Ra   |   Rb   |   Load/Store    |   --   |
  |        |        |(addr)  |     Type        |        |
  +--------+--------+--------+-----------------+--------+

HW_REI Format:
   31    26 25                              0
  +--------+----------------------------------+
  | Opcode |           Reserved               |
  +--------+----------------------------------+

Field Definitions:
- Ra: Destination register (for loads/MFPR) or source register (for stores/MTPR)
- Rb: Address source register (for loads/stores)
- IPR Number: Internal Processor Register number (8 bits)
- Load/Store Type: Hardware-specific operation type (varies by CPU model)
*/

//
// Helper Macros for Hardware Instruction Decoding
//

#define EXTRACT_IPR_NUMBER(instr) ((instr >> 5) & 0xFF)
#define EXTRACT_HW_TYPE(instr) ((instr >> 5) & 0xF)
#define EXTRACT_HW_FUNCTION(instr) ((instr >> 5) & 0xFFFF)


// Constants
constexpr int HALT_PROCESSING_CYCLES = 10;

// 
// Hardware Instruction Capability Masks
// 

constexpr quint32 HW_MASK_EV4_EV5 = 0x3F; // All hardware instructions supported
constexpr quint32 HW_MASK_EV6 = 0x3F;     // All hardware instructions supported
constexpr quint32 HW_MASK_EV7 = 0x3F;     // All hardware instructions supported

// Individual instruction support flags
constexpr quint32 HW_SUPPORT_MFPR = 0x01; // HW_MFPR support
constexpr quint32 HW_SUPPORT_LD = 0x02;   // HW_LD support
constexpr quint32 HW_SUPPORT_MTPR = 0x04; // HW_MTPR support
constexpr quint32 HW_SUPPORT_REI = 0x08;  // HW_REI support
constexpr quint32 HW_SUPPORT_ST = 0x10;   // HW_ST support
constexpr quint32 HW_SUPPORT_ST_C = 0x20; // HW_ST_C support

constexpr quint32 OPCODE_INT_OP = 0x10;
constexpr quint32 OPCODE_INT_CMOV = 0x11;
constexpr quint32 OPCODE_INT_MSK = 0x12;
constexpr quint32 OPCODE_INT_MISC = 0x13;
constexpr quint32 OPCODE_MISC = 0x18; // Miscellaneous Operations

//
// Hardware Load/Store Types - EV6
// 

constexpr quint32 HW_LD_EV6_PHYSICAL = 0x0;     // Physical memory load
constexpr quint32 HW_LD_EV6_VIRTUAL = 0x1;      // Virtual memory load
constexpr quint32 HW_LD_EV6_IO_SPACE = 0x2;     // I/O space load
constexpr quint32 HW_LD_EV6_CONFIG_SPACE = 0x3; // Configuration space load
constexpr quint32 HW_LD_EV6_LOCK = 0x4;         // Locked load
constexpr quint32 HW_LD_EV6_PREFETCH = 0x5;     // Prefetch load

constexpr quint32 HW_ST_EV6_PHYSICAL = 0x0;     // Physical memory store
constexpr quint32 HW_ST_EV6_VIRTUAL = 0x1;      // Virtual memory store
constexpr quint32 HW_ST_EV6_IO_SPACE = 0x2;     // I/O space store
constexpr quint32 HW_ST_EV6_CONFIG_SPACE = 0x3; // Configuration space store
constexpr quint32 HW_ST_EV6_CONDITIONAL = 0x4;  // Conditional store
constexpr quint32 HW_ST_EV6_WRITETHROUGH = 0x5; // Write-through store

//
// Hardware Load/Store Types - EV7
//

constexpr quint32 HW_LD_EV7_PHYSICAL = 0x0;     // Physical memory load
constexpr quint32 HW_LD_EV7_VIRTUAL = 0x1;      // Virtual memory load
constexpr quint32 HW_LD_EV7_IO_SPACE = 0x2;     // I/O space load
constexpr quint32 HW_LD_EV7_CONFIG_SPACE = 0x3; // Configuration space load
constexpr quint32 HW_LD_EV7_LOCK = 0x4;         // Locked load
constexpr quint32 HW_LD_EV7_PREFETCH = 0x5;     // Prefetch load
constexpr quint32 HW_LD_EV7_SPECULATIVE = 0x6;  // Speculative load
constexpr quint32 HW_LD_EV7_COHERENT = 0x7;     // Coherent load

constexpr quint32 HW_ST_EV7_PHYSICAL = 0x0;     // Physical memory store
constexpr quint32 HW_ST_EV7_VIRTUAL = 0x1;      // Virtual memory store
constexpr quint32 HW_ST_EV7_IO_SPACE = 0x2;     // I/O space store
constexpr quint32 HW_ST_EV7_CONFIG_SPACE = 0x3; // Configuration space store
constexpr quint32 HW_ST_EV7_CONDITIONAL = 0x4;  // Conditional store
constexpr quint32 HW_ST_EV7_WRITETHROUGH = 0x5; // Write-through store
constexpr quint32 HW_ST_EV7_WRITEBACK = 0x6;    // Write-back store
constexpr quint32 HW_ST_EV7_COHERENT = 0x7;     // Coherent store

// 
// Hardware Load/Store Types - EV4/EV5
// 

constexpr quint32 HW_LD_EV4_EV5_PHYSICAL = 0x0;    // Physical memory load
constexpr quint32 HW_LD_EV4_EV5_VIRTUAL_ITB = 0x1; // Virtual load via ITB
constexpr quint32 HW_LD_EV4_EV5_VIRTUAL_DTB = 0x2; // Virtual load via DTB
constexpr quint32 HW_LD_EV4_EV5_ALTERNATE = 0x3;   // Alternate space load

constexpr quint32 HW_ST_EV4_EV5_PHYSICAL = 0x0;    // Physical memory store
constexpr quint32 HW_ST_EV4_EV5_VIRTUAL_ITB = 0x1; // Virtual store via ITB
constexpr quint32 HW_ST_EV4_EV5_VIRTUAL_DTB = 0x2; // Virtual store via DTB
constexpr quint32 HW_ST_EV4_EV5_ALTERNATE = 0x3;   // Alternate space store
