#pragma once
enum class Fault_TrapType
{
    PrivilegeViolation,
    MMUAccessFault,
    FloatingPointDisabled,
    ReservedInstruction,
    SoftwareInterrupt,
    ArithmeticTrap,
    Breakpoint,
    DivideByZero_int,
    DivideByZero_fp,
    OverFlow_fp,
    UnderFlow_fp,
    Inexact_fp,
    Invalid_fp,
    MachineCheck,
    IllegalInstruction,    // executeNextInstruction could not match the instruction to JIT or Decode Fallback
    MemoryAccessException, //
    UnsupportedOp
};
