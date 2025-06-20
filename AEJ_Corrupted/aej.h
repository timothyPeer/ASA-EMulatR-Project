#pragma once

/*
* References:
	- Alpha 21164 Technical Overview, Sections 5–9
	- Alpha 21264 Out-of-Order Engine Documentation

Fields:
-------
Section           - Instruction category (Integer, FloatingPoint, Control, Vector, PAL)
Mnemonic          - Alpha assembly mnemonic (e.g., ADDL, MULQ, BR)
Opcode (hex)      - Primary 6-bit opcode value in hexadecimal (bits 31:26)
Function (hex)    - Secondary function code for 'Operate' format (bits 5:0), blank if not needed
Class             - Instruction format class: Operate, Memory, Branch, Control, System
Operands          - Logical operands (e.g., ra, rb, rc)
Description       - Short human-readable explanation
Implementation Status - Implemented, Reserved, or TODO

Sections:
---------
- Integer: Basic integer operations (ADDL, MULL, CMPEQ, etc.)
- FloatingPoint: Floating-point operations (ADDF, MULG, DIVT, etc.)
- Control: Branching, jumps, traps (BR, JMP, JSR, RET, MB, WMB)
- Vector: Vector instruction examples (VADD, VSUB)
- PAL: Privileged Architecture Library (CALL_PAL, REI, HALT)

Usage:
------
This CSV can be parsed to:
- Auto-generate Dispatch Tables
- Auto-generate DIT (Dynamic Instruction Translation) handlers
- Auto-generate JIT code emission templates
- Create documentation or assembler references

Example CSV Row:
----------------
Integer,ADDL,0x10,0x00,Operate,ra,rb,rc,Integer Add (longword),Implemented

Notes:
------
- All opcodes and function codes are hexadecimal
- Empty function fields mean function is not used (direct opcode execution)
- CSV is UTF-8 encoded
Sent from my iPhone

Section,Mnemonic,Opcode (hex),Function (hex),Class,Operands,Description,Implementation Status Integer,
ADDL,0x10,0x00,Operate,ra,rb,rc,Integer Add (longword),Implemented Integer,
ADDQ,0x10,0x20,Operate,ra,rb,rc,Integer Add (quadword),Implemented Integer,
SUBL,0x10,0x09,Operate,ra,rb,rc,Integer Subtract (longword),Implemented Integer,
SUBQ,0x10,0x29,Operate,ra,rb,rc,Integer Subtract (quadword),Implemented Integer,
MULL,0x10,0x0C,Operate,ra,rb,rc,Integer Multiply (longword),Implemented Integer,
MULQ,0x10,0x2C,Operate,ra,rb,rc,Integer Multiply (quadword),Implemented Integer,
UMULH,0x10,0x30,Operate,ra,rb,rc,Unsigned Multiply High (quadword),Implemented Integer,
DIVL,0x10,0x1D,Operate,ra,rb,rc,Divide (longword),Implemented Integer,
DIVQ,0x10,0x3D,Operate,ra,rb,rc,Divide (quadword),Implemented Integer,
CMPEQ,0x10,0x2D,Operate,ra,rb,rc,Compare Equal,Implemented FloatingPoint,
ADDF,0x16,0x00,Operate,fa,fb,fc,Add Floating-point S,Implemented FloatingPoint,
ADDG,0x16,0x01,Operate,fa,fb,fc,Add Floating-point G,Implemented FloatingPoint,
ADDT,0x16,0x02,Operate,fa,fb,fc,Add Floating-point T,Implemented Control,
BR,0x30,,Branch,ra,disp,Branch Relative,Implemented Control,
BSR,0x34,,Branch,ra,disp,Branch to Subroutine,Implemented Control,
JMP,0x1A,,Branch,ra,rb,Jump Indirect,Implemented 
PAL,CALL_PAL,0x00,,System,palcode_entry,Call PAL Routine,Implemented 
PAL,REI,0x1E,,System,,Return from Exception or Interrupt,Implemented


*/