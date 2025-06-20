

// enum PalFunction
// {
//     PAL_HALT = 0x0000,       // Stop processor *
//     PAL_MCHK = 0x0002,       // Machine check
//     PAL_BPT = 0x0003,        // Breakpoint  (Unprivileged) *
//     PAL_BUGCHK = 0x0004,     // Bug check
//     PAL_WRKGP = 0x002E,      // Write Kernel Global Pointer
//     PAL_WRUSP = 0x0030,      // Write User Stack Pointer
//     PAL_RDUSP = 0x0031,      // Read User Stack Pointer
//     PAL_WRPERFMON = 0x0032,  // Write Performance Monitor Control
//     PAL_RDDPERFMON = 0x0033, // Read Performance Monitor Data
//     PAL_IMB = 0x0080,        // Instruction Memory Barrier
//     PAL_REI = 0x0081,        // Return from Exception
//     PAL_SWPCTX = 0x0082,     // Swap Execution Context
//     PAL_CALLSYS = 0x0083,    // System Call Handler
//     PAL_RET = 0x0084,        // Return from System Call
//     PAL_CALLPRIV = 0x0085,   // Call Privileged Service
//     PAL_RDUNIQUE = 0x0090,   // Read Unique Value
//     PAL_WRUNIQUE = 0x0091,   // Write Unique Value
//     PAL_TBIA = 0x0092,       // TLB Invalidate All
//     PAL_TBIS = 0x0093,       // TLB Invalidate Single
//     PAL_TBIM = 0x0094,       // TLB Invalidate Matching ASN
//     PAL_TBIE = 0x0095,       // TLB Invalidate by VA & ASN
//     PAL_DRAINA = 0x0096,     // Drain Write Buffer
//     PAL_SWPPAL = 0x0097,     // Switch to PAL mode
//     PAL_SWPIPL = 0x0098,     // Swap IPL
//     PAL_RDPS = 0x0099,       // Read Processor Status
//     PAL_WRPS = 0x009A,       // Write Processor Status
//     PAL_WRVPTPTR = 0x009B,   // Write Virtual Page Table Pointer
//     PAL_SWASTEN = 0x009C,    // Software AST Enable
//     PAL_WRASTEN = 0x009D,    // Write AST Enable Mask
//     PAL_RDASTEN = 0x009E,    // Read AST Enable Mask
//     PAL_EXCB = 0x009F        // Exception Barrier
// };