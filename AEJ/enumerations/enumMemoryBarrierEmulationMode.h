#pragma once

/*
Implementation Strategy

Instruction-Specific Mapping: Each Alpha instruction that requires memory ordering should explicitly call the
appropriate barrier method based on the Alpha Architecture Reference Manual specifications. Performance vs Accuracy
Trade-off: Qt atomics provide good ordering for most cases with better performance, while strict barriers ensure
hardware-accurate behavior when needed. Configuration-Based Selection: You could add an emulation mode setting:
*/

enum memoryBarrierEmulationMode
{
    PERFORMANCE_MODE,  // Prefer Qt atomics // AlphaMemorySystem::executeMemoryBarrier
    ACCURACY_MODE,     // Always use strict barriers // AlphaMemorySystem::executeStrictMemoryBarrier
    COMPATIBILITY_MODE // Auto-select based on instruction
};
/*
  READ_BARRIER -: 0 :- READ
  WRITE_BARRIER -: 1 :- WRITE
  FULL_BARRIER -: 2 :- FULL (Strict) 
  */
enum memoryBarrierEmulationModeType {

    READ_BARRIER =0,  // 
    WRITE_BARRIER = 1, // 
    FULL_BARRIER = 2  //
};

/*

Programmer Determination Approach
When to use executeMemoryBarrier (Qt atomics):

Alpha WMB (Write Memory Barrier) - only needs write ordering
Load-locked/Store-conditional implicit barriers - acquire/release semantics
High-performance emulation where exact hardware timing isn't critical
Most general memory ordering requirements

When to use executeStrictMemoryBarrier (hardware-level):

Alpha MB (Memory Barrier) - requires full sequential consistency
PAL code transitions - must match hardware behavior exactly
Interrupt handling and exception processing
DMA coherency operations
Cycle-accurate emulation for validation/certification

*/