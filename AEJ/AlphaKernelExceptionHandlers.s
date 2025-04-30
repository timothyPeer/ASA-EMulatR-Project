# Alpha Exception Handler Stubs
# Each handler entry point corresponds to a vector address
# Base Address: 0x100
# Spacing: 0x80 (128 bytes)

.section .text

# --------------------------
# Arithmetic Trap Handler
# Vector Address: 0x100
.globl pal_arithmetic_trap
pal_arithmetic_trap:
    # Save context
    # Do simple recovery or escalate
    br   $31, kernel_trap_dispatch

# --------------------------
# Illegal Instruction Handler
# Vector Address: 0x200
.globl pal_illegal_instruction
pal_illegal_instruction:
    br   $31, kernel_trap_dispatch

# --------------------------
# Privileged Instruction Fault Handler
# Vector Address: 0x280
.globl pal_privileged_instruction
pal_privileged_instruction:
    br   $31, kernel_trap_dispatch

# --------------------------
# Alignment Fault Handler
# Vector Address: 0x300
.globl pal_alignment_fault
pal_alignment_fault:
    br   $31, kernel_trap_dispatch

# --------------------------
# Memory Access Violation Handler
# Vector Address: 0x380
.globl pal_memory_access_violation
pal_memory_access_violation:
    br   $31, kernel_trap_dispatch

# --------------------------
# Page Fault Handler
# Vector Address: 0x600
.globl pal_page_fault
pal_page_fault:
    br   $31, kernel_trap_dispatch

# --------------------------
# Floating Point Exception Handler
# Vector Address: 0x800
.globl pal_fp_exception
pal_fp_exception:
    br   $31, kernel_trap_dispatch

# --------------------------
# Reserved Operand Handler
# Vector Address: 0x980
.globl pal_reserved_operand
pal_reserved_operand:
    br   $31, kernel_trap_dispatch

# --------------------------
# Machine Check Handler
# Vector Address: 0xA00
.globl pal_machine_check
pal_machine_check:
    br   $31, kernel_trap_dispatch

# --------------------------
# System Call Handler
# Vector Address: 0xB00
.globl pal_system_call
pal_system_call:
    br   $31, kernel_trap_dispatch

# --------------------------
# Breakpoint Handler
# Vector Address: 0xB80
.globl pal_breakpoint
pal_breakpoint:
    br   $31, kernel_trap_dispatch

# --------------------------
# Interrupt Handler
# Vector Address: 0xC00
.globl pal_interrupt
pal_interrupt:
    br   $31, kernel_trap_dispatch

# --------------------------
# Halt Handler
# Vector Address: 0xC80
.globl pal_halt
pal_halt:
    br   $31, kernel_trap_dispatch

# --------------------------
# Unknown Exception Handler
# Vector Address: 0xD00
.globl pal_unknown_exception
pal_unknown_exception:
    br   $31, kernel_trap_dispatch

# --------------------------
# Common Trap Dispatcher
.globl kernel_trap_dispatch
kernel_trap_dispatch:
    # Would invoke OS handler routines based on trap cause
    ret $31, ($26), 1
