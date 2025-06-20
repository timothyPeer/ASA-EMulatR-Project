#pragma once
#include <QStringList>
#define QSL_SAFE(...)  QStringList{ __VA_ARGS__ }
/* AlphaInstrTables.h  – generated from your CSV
 *
 *  Usage
 *    #include "AlphaInstrTables.h"
 *    QVector<InstructionDefinition> core(
 *        std::begin(kCoreInstr), std::end(kCoreInstr));
 *    QVector<InstructionDefinition> pal (
 *        std::begin(kPalInstr),  std::end(kPalInstr));
 *
 *    decoder->loadInstructionDefinitions(core);
 *    decoder->loadPalDefinitions(pal);
 */

#pragma once
//#include "InstructionDefinition.h"

struct InstructionDefinition {
	const char* section;
	const char* mnemonic;
	quint16      opcode;
	quint16       func;      // –1 for "don’t‑care" in non‑operate formats
	const char* instrClass;
	QStringList operands;
	const char* description;
};
 /* ------------ Core (non‑PAL) instructions ------------------------- */
// static constexpr InstructionDefinition kCoreInstr[] = {
//     /*Section, Mnemonic,Opcode,Func,Class,Operands,Description*/
//     { "Integer","ADDL",0x10,0x00,"Operate",{"ra","rb","rc"},"Integer Add (longword)" },
//     { "Integer","ADDQ",0x10,0x20,"Operate",{"ra","rb","rc"},"Integer Add (quadword)" },
//     { "Integer","SUBL",0x10,0x09,"Operate",{"ra","rb","rc"},"Integer Subtract (longword)" },
//     { "Integer","SUBQ",0x10,0x29,"Operate",{"ra","rb","rc"},"Integer Subtract (quadword)" },
//     { "Integer","MULL",0x10,0x0C,"Operate",{"ra","rb","rc"},"Integer Multiply (longword)" },
//     { "Integer","MULQ",0x10,0x2C,"Operate",{"ra","rb","rc"},"Integer Multiply (quadword)" },
//     { "Integer","UMULH",0x10,0x30,"Operate",{"ra","rb","rc"},"Unsigned Multiply High (quadword)" },
//     { "Integer","DIVL",0x10,0x1D,"Operate",{"ra","rb","rc"},"Divide (longword)" },
//     { "Integer","DIVQ",0x10,0x3D,"Operate",{"ra","rb","rc"},"Divide (quadword)" },
//     { "Integer","CMPEQ",0x10,0x2D,"Operate",{"ra","rb","rc"},"Compare Equal" },
//     { "Integer","CMPULT",0x10,0x01,"Operate",{"ra","rb","rc"},"Compare Unsigned Less Than" },
//     { "Integer","CMPULE",0x10,0x11,"Operate",{"ra","rb","rc"},"Compare Unsigned Less Equal" },
//     { "Integer","CMPLT",0x10,0x02,"Operate",{"ra","rb","rc"},"Compare Signed Less Than" },
//     { "Integer","CMPLE",0x10,0x12,"Operate",{"ra","rb","rc"},"Compare Signed Less Equal" },
//     { "Integer","AND",0x11,0x00,"Operate",{"ra","rb","rc"},"Logical AND" },
//     { "Integer","BIC",0x11,0x08,"Operate",{"ra","rb","rc"},"Bit Clear" },
//     { "Integer","BIS",0x11,0x14,"Operate",{"ra","rb","rc"},"Bit Set" },
//     { "Integer","ORNOT",0x11,0x1C,"Operate",{"ra","rb","rc"},"OR NOT" },
//     { "Integer","XOR",0x11,0x20,"Operate",{"ra","rb","rc"},"Logical Exclusive OR" },
//     { "Integer","EQV",0x11,0x28,"Operate",{"ra","rb","rc"},"Logical Equivalence" },
//     { "Integer","SLL",0x12,0x39,"Operate",{"ra","rb","rc"},"Shift Left Logical" },
//     { "Integer","SRL",0x12,0x34,"Operate",{"ra","rb","rc"},"Shift Right Logical" },
//     { "Integer","SRA",0x12,0x3C,"Operate",{"ra","rb","rc"},"Shift Right Arithmetic" },
//     { "Integer","ZAP",0x12,0x30,"Operate",{"ra","rb","rc"},"Zero Byte Mask" },
//     { "Integer","ZAPNOT",0x12,0x31,"Operate",{"ra","rb","rc"},"Zero Byte Not Mask" },
//     { "Integer","MSKBL",0x12,0x02,"Operate",{"ra","rb","rc"},"Mask Byte Low" },
//     { "Integer","EXTBL",0x12,0x06,"Operate",{"ra","rb","rc"},"Extract Byte Low" },
//     { "Integer","INSBL",0x12,0x0B,"Operate",{"ra","rb","rc"},"Insert Byte Low" },
//     { "Integer","MSKWL",0x12,0x12,"Operate",{"ra","rb","rc"},"Mask Word Low" },
//     { "Integer","EXTWL",0x12,0x16,"Operate",{"ra","rb","rc"},"Extract Word Low" },
//     { "Integer","INSWL",0x12,0x1B,"Operate",{"ra","rb","rc"},"Insert Word Low" },
//     { "Integer","MSKLL",0x12,0x22,"Operate",{"ra","rb","rc"},"Mask Longword Low" },
//     { "Integer","EXTLL",0x12,0x26,"Operate",{"ra","rb","rc"},"Extract Longword Low" },
//     { "Integer","INSLL",0x12,0x2B,"Operate",{"ra","rb","rc"},"Insert Longword Low" },
//     { "Integer","MSKQL",0x12,0x32,"Operate",{"ra","rb","rc"},"Mask Quadword Low" },
//     { "Integer","EXTQL",0x12,0x36,"Operate",{"ra","rb","rc"},"Extract Quadword Low" },
//     { "Integer","INSQL",0x12,0x3B,"Operate",{"ra","rb","rc"},"Insert Quadword Low" },
//     { "Integer","SEXTWL",0x10,0x0E,"Operate",{"ra","rb","rc"},"Sign‑Extend Word to Longword" },
//     { "Integer","SEXTLL",0x10,0x0F,"Operate",{"ra","rb","rc"},"Sign‑Extend Longword to Quadword" },
// 
//     { "FloatingPoint","ADDF",0x16,0x00,"Operate",{"fa","fb","fc"},"Floating Add S" },
//     { "FloatingPoint","ADDD",0x16,0x01,"Operate",{"fa","fb","fc"},"Floating Add D" },
//     { "FloatingPoint","ADDG",0x16,0x02,"Operate",{"fa","fb","fc"},"Floating Add G" },
//     { "FloatingPoint","ADDT",0x16,0x03,"Operate",{"fa","fb","fc"},"Floating Add T" },
//     { "FloatingPoint","SUBF",0x16,0x20,"Operate",{"fa","fb","fc"},"Floating Sub S" },
//     { "FloatingPoint","SUBD",0x16,0x21,"Operate",{"fa","fb","fc"},"Floating Sub D" },
//     { "FloatingPoint","SUBG",0x16,0x22,"Operate",{"fa","fb","fc"},"Floating Sub G" },
//     { "FloatingPoint","SUBT",0x16,0x23,"Operate",{"fa","fb","fc"},"Floating Sub T" },
//     { "FloatingPoint","MULF",0x16,0x08,"Operate",{"fa","fb","fc"},"Floating Mul S" },
//     { "FloatingPoint","MULD",0x16,0x09,"Operate",{"fa","fb","fc"},"Floating Mul D" },
//     { "FloatingPoint","MULG",0x16,0x0A,"Operate",{"fa","fb","fc"},"Floating Mul G" },
//     { "FloatingPoint","MULT",0x16,0x0B,"Operate",{"fa","fb","fc"},"Floating Mul T" },
//     { "FloatingPoint","DIVF",0x16,0x18,"Operate",{"fa","fb","fc"},"Floating Div S" },
//     { "FloatingPoint","DIVD",0x16,0x19,"Operate",{"fa","fb","fc"},"Floating Div D" },
//     { "FloatingPoint","DIVG",0x16,0x1A,"Operate",{"fa","fb","fc"},"Floating Div G" },
//     { "FloatingPoint","DIVT",0x16,0x1B,"Operate",{"fa","fb","fc"},"Floating Div T" },
//     { "FloatingPoint","CMPTEQ",0x16,0x30,"Operate",{"fa","fb","fc"},"Compare Equal T" },
//     { "FloatingPoint","CMPTLT",0x16,0x31,"Operate",{"fa","fb","fc"},"Compare LT T" },
//     { "FloatingPoint","CMPTLE",0x16,0x32,"Operate",{"fa","fb","fc"},"Compare LE T" },
//     { "FloatingPoint","CPYS",0x17,0x20,"Operate",{"fa","fb","fc"},"Copy Sign S" },
//     { "FloatingPoint","CPYSN",0x17,0x21,"Operate",{"fa","fb","fc"},"Copy Sign Neg S" },
//     { "FloatingPoint","CPYSE",0x17,0x22,"Operate",{"fa","fb","fc"},"Copy Sign Eq S" },
//     { "FloatingPoint","MT_FPCR",0x17,0x26,"Operate",{"fa","fb","fc"},"Move to FPCR" },
//     { "FloatingPoint","MF_FPCR",0x17,0x25,"Operate",{"fa","fb","fc"},"Move from FPCR" },
//     { "FloatingPoint","CVTQL",0x17,0x2F,"Operate",{"fa","fb","fc"},"Convert Qword to L‑float" },
//     { "FloatingPoint","CVTQF",0x17,0x2B,"Operate",{"fa","fb","fc"},"Convert Qword to S‑float" },
//     { "FloatingPoint","CVTQG",0x17,0x2C,"Operate",{"fa","fb","fc"},"Convert Qword to G‑float" },
//     { "FloatingPoint","CVTQT",0x17,0x2D,"Operate",{"fa","fb","fc"},"Convert Qword to T‑float" },
// 
//     { "Control","BR",0x30,-1,"Branch",{"ra","disp"},"Branch Relative" },
//     { "Control","BSR",0x34,-1,"Branch",{"ra","disp"},"Branch to Subroutine" },
//     { "Control","BLBC",0x38,-1,"Branch",{"ra","disp"},"Branch Low Bit Clear" },
//     { "Control","BLBS",0x3C,-1,"Branch",{"ra","disp"},"Branch Low Bit Set" },
//     { "Control","BEQ",0x39,-1,"Branch",{"ra","disp"},"Branch if Equal" },
//     { "Control","BNE",0x3D,-1,"Branch",{"ra","disp"},"Branch if Not Equal" },
//     { "Control","BLT",0x3B,-1,"Branch",{"ra","disp"},"Branch if Less Than" },
//     { "Control","BGE",0x3F,-1,"Branch",{"ra","disp"},"Branch if GE" },
//     { "Control","BLLE",0x3A,-1,"Branch",{"ra","disp"},"Branch if LE" },
//     { "Control","BLGT",0x3E,-1,"Branch",{"ra","disp"},"Branch if GT" },
//     { "Control","JMP",0x1A,-1,"Branch",{"ra","rb"},"Jump Indirect" },
//     { "Control","JSR",0x1B,-1,"Branch",{"ra","rb"},"Jump to Subroutine Indirect" },
//     { "Control","RET",0x1C,-1,"Branch",{"ra","rb"},"Return from Subroutine" },
//     { "Control","JMP_COROUTINE",0x1D,-1,"Branch",{"ra","rb"},"Jump to Coroutine" },
//     { "Control","MB",0x18,-1,"MemoryBarrier",{},"Memory Barrier" },
//     { "Control","WMB",0x19,-1,"MemoryBarrier",{},"Write Memory Barrier" },
// 
//     { "Vector","VADD",0x60,0x00,"Vector",{"va","vb","vc"},"Vector Add" },
//     { "Vector","VSUB",0x60,0x01,"Vector",{"va","vb","vc"},"Vector Sub" },
//     { "Vector","VMUL",0x60,0x02,"Vector",{"va","vb","vc"},"Vector Multiply" },
//     { "Vector","VDIV",0x60,0x03,"Vector",{"va","vb","vc"},"Vector Divide" },
// 
//     { "Integer","AMASK",0x1C,0x02,"Operate",{"ra","rb","rc"},"Address Mask" },
//     { "Integer","IMPLVER",0x1C,0x06,"Operate",{"ra","rb","rc"},"Implementation Version" },
//     { "Control","FETCH",0x18,-1,"MemoryBarrier",{"ra","disp"},"Memory Fetch Hint" },
//     { "Control","FETCH_M",0x19,-1,"MemoryBarrier",{"ra","disp"},"Memory Fetch & Modify Hint" },
//     { "Control","TRAPB",0x18,-1,"MemoryBarrier",{},"Trap Barrier" },
//     { "Control","MB2",0x18,-1,"MemoryBarrier",{},"Memory Barrier Variant 2" },
//     { "Control","MB3",0x19,-1,"MemoryBarrier",{},"Memory Barrier Variant 3" },
//     { "Integer","RPCC",0x1C,0x41,"Operate",{"ra","rb","rc"},"Read Proc Cycle Cnt" }
// };

#define QSL_SAFE(...) ([]() -> QStringList { return QStringList{__VA_ARGS__}; }())

static constexpr InstructionDefinition kCoreInstr[] = {
	/* Section,        Mnemonic, Opcode, Func,  Class,            Operands,                              Description */
	{ "Integer",       "ADDL",        0x10, 0x00, "Operate",        QSL_SAFE("ra","rb","rc"),             "Integer Add (longword)" },
	{ "Integer",       "ADDQ",        0x10, 0x20, "Operate",        QSL_SAFE("ra","rb","rc"),             "Integer Add (quadword)" },
	{ "Integer",       "SUBL",        0x10, 0x09, "Operate",        QSL_SAFE("ra","rb","rc"),             "Integer Subtract (longword)" },
	{ "Integer",       "SUBQ",        0x10, 0x29, "Operate",        QSL_SAFE("ra","rb","rc"),             "Integer Subtract (quadword)" },
	{ "Integer",       "MULL",        0x10, 0x0C, "Operate",        QSL_SAFE("ra","rb","rc"),             "Integer Multiply (longword)" },
	{ "Integer",       "MULQ",        0x10, 0x2C, "Operate",        QSL_SAFE("ra","rb","rc"),             "Integer Multiply (quadword)" },
	{ "Integer",       "UMULH",       0x10, 0x30, "Operate",        QSL_SAFE("ra","rb","rc"),             "Unsigned Multiply High (quadword)" },
	{ "Integer",       "DIVL",        0x10, 0x1D, "Operate",        QSL_SAFE("ra","rb","rc"),             "Divide (longword)" },
	{ "Integer",       "DIVQ",        0x10, 0x3D, "Operate",        QSL_SAFE("ra","rb","rc"),             "Divide (quadword)" },
	{ "Integer",       "CMPEQ",       0x10, 0x2D, "Operate",        QSL_SAFE("ra","rb","rc"),             "Compare Equal" },
	{ "Integer",       "CMPULT",      0x10, 0x01, "Operate",        QSL_SAFE("ra","rb","rc"),             "Compare Unsigned Less Than" },
	{ "Integer",       "CMPULE",      0x10, 0x11, "Operate",        QSL_SAFE("ra","rb","rc"),             "Compare Unsigned Less Equal" },
	{ "Integer",       "CMPLT",       0x10, 0x02, "Operate",        QSL_SAFE("ra","rb","rc"),             "Compare Signed Less Than" },
	{ "Integer",       "CMPLE",       0x10, 0x12, "Operate",        QSL_SAFE("ra","rb","rc"),             "Compare Signed Less Equal" },
	{ "Integer",       "AND",         0x11, 0x00, "Operate",        QSL_SAFE("ra","rb","rc"),             "Logical AND" },
	{ "Integer",       "BIC",         0x11, 0x08, "Operate",        QSL_SAFE("ra","rb","rc"),             "Bit Clear" },
	{ "Integer",       "BIS",         0x11, 0x14, "Operate",        QSL_SAFE("ra","rb","rc"),             "Bit Set" },
	{ "Integer",       "ORNOT",       0x11, 0x1C, "Operate",        QSL_SAFE("ra","rb","rc"),             "OR NOT" },
	{ "Integer",       "XOR",         0x11, 0x20, "Operate",        QSL_SAFE("ra","rb","rc"),             "Logical Exclusive OR" },
	{ "Integer",       "EQV",         0x11, 0x28, "Operate",        QSL_SAFE("ra","rb","rc"),             "Logical Equivalence" },
	{ "Integer",       "SLL",         0x12, 0x39, "Operate",        QSL_SAFE("ra","rb","rc"),             "Shift Left Logical" },
	{ "Integer",       "SRL",         0x12, 0x34, "Operate",        QSL_SAFE("ra","rb","rc"),             "Shift Right Logical" },
	{ "Integer",       "SRA",         0x12, 0x3C, "Operate",        QSL_SAFE("ra","rb","rc"),             "Shift Right Arithmetic" },
	{ "Integer",       "ZAP",         0x12, 0x30, "Operate",        QSL_SAFE("ra","rb","rc"),             "Zero Byte Mask" },
	{ "Integer",       "ZAPNOT",      0x12, 0x31, "Operate",        QSL_SAFE("ra","rb","rc"),             "Zero Byte Not Mask" },
	{ "Integer",       "MSKBL",       0x12, 0x02, "Operate",        QSL_SAFE("ra","rb","rc"),             "Mask Byte Low" },
	{ "Integer",       "EXTBL",       0x12, 0x06, "Operate",        QSL_SAFE("ra","rb","rc"),             "Extract Byte Low" },
	{ "Integer",       "INSBL",       0x12, 0x0B, "Operate",        QSL_SAFE("ra","rb","rc"),             "Insert Byte Low" },
	{ "Integer",       "MSKWL",       0x12, 0x12, "Operate",        QSL_SAFE("ra","rb","rc"),             "Mask Word Low" },
	{ "Integer",       "EXTWL",       0x12, 0x16, "Operate",        QSL_SAFE("ra","rb","rc"),             "Extract Word Low" },
	{ "Integer",       "INSWL",       0x12, 0x1B, "Operate",        QSL_SAFE("ra","rb","rc"),             "Insert Word Low" },
	{ "Integer",       "MSKLL",       0x12, 0x22, "Operate",        QSL_SAFE("ra","rb","rc"),             "Mask Longword Low" },
	{ "Integer",       "EXTLL",       0x12, 0x26, "Operate",        QSL_SAFE("ra","rb","rc"),             "Extract Longword Low" },
	{ "Integer",       "INSLL",       0x12, 0x2B, "Operate",        QSL_SAFE("ra","rb","rc"),             "Insert Longword Low" },
	{ "Integer",       "MSKQL",       0x12, 0x32, "Operate",        QSL_SAFE("ra","rb","rc"),             "Mask Quadword Low" },
	{ "Integer",       "EXTQL",       0x12, 0x36, "Operate",        QSL_SAFE("ra","rb","rc"),             "Extract Quadword Low" },
	{ "Integer",       "INSQL",       0x12, 0x3B, "Operate",        QSL_SAFE("ra","rb","rc"),             "Insert Quadword Low" },
	{ "Integer",       "SEXTWL",      0x10, 0x0E, "Operate",        QSL_SAFE("ra","rb","rc"),             "Sign‑Extend Word to Longword" },
	{ "Integer",       "SEXTLL",      0x10, 0x0F, "Operate",        QSL_SAFE("ra","rb","rc"),             "Sign‑Extend Longword to Quadword" },
	{ "FloatingPoint", "ADDF",        0x16, 0x00, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Add S" }, 	/* Floating‑point operate */
	{ "FloatingPoint", "ADDD",        0x16, 0x01, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Add D" },
	{ "FloatingPoint", "ADDG",        0x16, 0x02, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Add G" },
	{ "FloatingPoint", "ADDT",        0x16, 0x03, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Add T" },
	{ "FloatingPoint", "SUBF",        0x16, 0x20, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Sub S" },
	{ "FloatingPoint", "SUBD",        0x16, 0x21, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Sub D" },
	{ "FloatingPoint", "SUBG",        0x16, 0x22, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Sub G" },
	{ "FloatingPoint", "SUBT",        0x16, 0x23, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Sub T" },
	{ "FloatingPoint", "MULF",        0x16, 0x08, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Mul S" },
	{ "FloatingPoint", "MULD",        0x16, 0x09, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Mul D" },
	{ "FloatingPoint", "MULG",        0x16, 0x0A, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Mul G" },
	{ "FloatingPoint", "MULT",        0x16, 0x0B, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Mul T" },
	{ "FloatingPoint", "DIVF",        0x16, 0x18, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Div S" },
	{ "FloatingPoint", "DIVD",        0x16, 0x19, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Div D" },
	{ "FloatingPoint", "DIVG",        0x16, 0x1A, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Div G" },
	{ "FloatingPoint", "DIVT",        0x16, 0x1B, "Operate",        QSL_SAFE("fa","fb","fc"),             "Floating Div T" },
	{ "FloatingPoint", "CMPTEQ",      0x16, 0x30, "Operate",        QSL_SAFE("fa","fb","fc"),             "Compare Equal T" },
	{ "FloatingPoint", "CMPTLT",      0x16, 0x31, "Operate",        QSL_SAFE("fa","fb","fc"),             "Compare LT T" },
	{ "FloatingPoint", "CMPTLE",      0x16, 0x32, "Operate",        QSL_SAFE("fa","fb","fc"),             "Compare LE T" },
	{ "FloatingPoint", "CPYS",        0x17, 0x20, "Operate",        QSL_SAFE("fa","fb","fc"),             "Copy Sign S" },
	{ "FloatingPoint", "CPYSN",       0x17, 0x21, "Operate",        QSL_SAFE("fa","fb","fc"),             "Copy Sign Neg S" },
	{ "FloatingPoint", "CPYSE",       0x17, 0x22, "Operate",        QSL_SAFE("fa","fb","fc"),             "Copy Sign Eq S" },
	{ "FloatingPoint", "MT_FPCR",     0x17, 0x26, "Operate",        QSL_SAFE("fa","fb","fc"),             "Move to FPCR" },
	{ "FloatingPoint", "MF_FPCR",     0x17, 0x25, "Operate",        QSL_SAFE("fa","fb","fc"),             "Move from FPCR" },
	{ "FloatingPoint", "CVTQL",       0x17, 0x2F, "Operate",        QSL_SAFE("fa","fb","fc"),             "Convert Qword to L‑float" },
	{ "FloatingPoint", "CVTQF",       0x17, 0x2B, "Operate",        QSL_SAFE("fa","fb","fc"),             "Convert Qword to S‑float" },
	{ "FloatingPoint", "CVTQG",       0x17, 0x2C, "Operate",        QSL_SAFE("fa","fb","fc"),             "Convert Qword to G‑float" },
	{ "FloatingPoint", "CVTQT",       0x17, 0x2D, "Operate",        QSL_SAFE("fa","fb","fc"),             "Convert Qword to T‑float" },

	/* Control & branch */
	{ "Control",       "BR",          0x30, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch Relative" },
	{ "Control",       "BSR",         0x34, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch to Subroutine" },
	{ "Control",       "BLBC",        0x38, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch Low Bit Clear" },
	{ "Control",       "BLBS",        0x3C, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch Low Bit Set" },
	{ "Control",       "BEQ",         0x39, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch if Equal" },
	{ "Control",       "BNE",         0x3D, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch if Not Equal" },
	{ "Control",       "BLT",         0x3B, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch if Less Than" },
	{ "Control",       "BGE",         0x3F, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch if GE" },
	{ "Control",       "BLLE",        0x3A, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch if LE" },
	{ "Control",       "BLGT",        0x3E, -1,  "Branch",         QSL_SAFE("ra","disp"),                "Branch if GT" },
	{ "Control",       "JMP",         0x1A, -1,  "Branch",         QSL_SAFE("ra","rb"),                 "Jump Indirect" },
	{ "Control",       "JSR",         0x1B, -1,  "Branch",         QSL_SAFE("ra","rb"),                 "Jump to Subroutine Indirect" },
	{ "Control",       "RET",         0x1C, -1,  "Branch",         QSL_SAFE("ra","rb"),                 "Return from Subroutine" },
	{ "Control",       "JMP_COROUTINE",0x1D,-1,  "Branch",         QSL_SAFE("ra","rb"),                 "Jump to Coroutine" },
	{ "Control",       "MB",          0x18, -1,  "MemoryBarrier",   QSL_SAFE(),                          "Memory Barrier" },
	{ "Control",       "WMB",         0x19, -1,  "MemoryBarrier",   QSL_SAFE(),                          "Write Memory Barrier" },

	/* Vector */
	{ "Vector",        "VADD",        0x60, 0x00, "Vector",         QSL_SAFE("va","vb","vc"),           "Vector Add" },
	{ "Vector",        "VSUB",        0x60, 0x01, "Vector",         QSL_SAFE("va","vb","vc"),           "Vector Sub" },
	{ "Vector",        "VMUL",        0x60, 0x02, "Vector",         QSL_SAFE("va","vb","vc"),           "Vector Multiply" },
	{ "Vector",        "VDIV",        0x60, 0x03, "Vector",         QSL_SAFE("va","vb","vc"),           "Vector Divide" },

	/* Misc integer & barriers */
	{ "Integer",       "AMASK",       0x1C, 0x02, "Operate",        QSL_SAFE("ra","rb","rc"),             "Address Mask" },
	{ "Integer",       "IMPLVER",     0x1C, 0x06, "Operate",        QSL_SAFE("ra","rb","rc"),             "Implementation Version" },
	{ "Control",       "FETCH",       0x18, -1,  "MemoryBarrier",   QSL_SAFE("ra","disp"),                "Memory Fetch Hint" },
	{ "Control",       "FETCH_M",     0x19, -1,  "MemoryBarrier",   QSL_SAFE("ra","disp"),                "Memory Fetch & Modify Hint" },
	{ "Control",       "TRAPB",       0x18, -1,  "MemoryBarrier",   QSL_SAFE(),                          "Trap Barrier" },
	{ "Control",       "MB2",         0x18, -1,  "MemoryBarrier",   QSL_SAFE(),                          "Memory Barrier Variant 2" },
	{ "Control",       "MB3",         0x19, -1,  "MemoryBarrier",   QSL_SAFE(),                          "Memory Barrier Variant 3" },
	{ "Integer",       "RPCC",        0x1C, 0x41, "Operate",        QSL_SAFE("ra","rb","rc"),             "Read Proc Cycle Cnt" }
};

/* ------------ PAL (CALL_PAL) instructions ------------------------- */
// static constexpr InstructionDefinition kPalInstr[] = {
//     { "PAL","CALL_PAL",0x00,-1,"PAL",{"palcode_entry"},"Call PAL Routine" },
//     { "PAL","REI",0x00,0x86,"PAL",{},"Return from exception" },
//     { "PAL","HALT",0x00,0x00,"PAL",{},"Processor Halt" },
//     { "PAL","WRVPTPTR",0x00,0x9B,"PAL",{},"Write VPT Pointer" },
//     { "PAL","MFPR",0x00,0x9C,"PAL",{"pr"},"Move from Processor Reg" },
//     { "PAL","MTPR",0x00,0x9D,"PAL",{"pr","val"},"Move to Processor Reg" },
//     { "PAL","SWPCTX",0x00,0x9E,"PAL",{},"Swap Context" },
//     { "PAL","SIRR",0x00,0xA4,"PAL",{"mask"},"Set Interrupt Req" },
//     { "PAL","CSIR",0x00,0xA5,"PAL",{"mask"},"Clear Interrupt Req" },
//     { "PAL","RD_PS",0x00,0x9C,"PAL",{},"Read PS" },
//     { "PAL","WR_PS",0x00,0x9D,"PAL",{"val"},"Write PS" },
// 
//     /* Extra PALs from second CSV list */
//     { "PAL","BPT",0x00,0x80,"PAL",{},"Breakpoint Trap" },
//     { "PAL","BPT_ALT",0x00,0x81,"PAL",{},"Breakpoint alt" },
//     { "PAL","BUGCHK",0x00,0x82,"PAL",{},"Kernel bug check" },
//     { "PAL","CHMK",0x00,0x83,"PAL",{},"Change‑mode to kernel" },
//     { "PAL","IMB",0x00,0x86,"PAL",{},"Instruction MB" },
//     { "PAL","SWPIPL",0x00,0x9E,"PAL",{},"Swap IPL" },
//     { "PAL","RDPS",0x00,0x9C,"PAL",{},"Read PS" },
//     { "PAL","WRPS",0x00,0x9D,"PAL",{},"Write PS" },
//     { "PAL","RDUNIQUE",0x00,0x9F,"PAL",{},"Read UNIQUE" },
//     { "PAL","WRUNIQUE",0x00,0xA0,"PAL",{},"Write UNIQUE" },
//     { "PAL","RDVAL",0x00,0xA1,"PAL",{},"Read VA" },
//     { "PAL","WRVAL",0x00,0xA2,"PAL",{},"Write VA" },
//     { "PAL","RDLOCK",0x00,0xA6,"PAL",{},"Read LOCK_FLAG" },
//     { "PAL","WRLOCK",0x00,0xA7,"PAL",{},"Write LOCK_FLAG" },
//     { "PAL","TBIS",0x00,0xAF,"PAL",{},"Insert I‑TB entry" },
//     { "PAL","TBAS",0x00,0xB0,"PAL",{},"Insert D‑TB entry" },
//     { "PAL","TBIA",0x00,0xB1,"PAL",{},"Invalidate all TLB" },
//     { "PAL","TBISD",0x00,0xB2,"PAL",{},"Invalidate single D‑TB" },
//     { "PAL","TBISI",0x00,0xB3,"PAL",{},"Invalidate single I‑TB" }
// };

static constexpr InstructionDefinition kPalInstr[] = {
    /* Section, Mnemonic,    Opcode, Func,  Class, Operands,             Description */
    { "PAL", "CALL_PAL",   0x00,  -1,   "PAL", QSL_SAFE("palcode_entry"), "Call PAL Routine" },
    { "PAL", "REI",        0x00,  0x86, "PAL", QSL_SAFE(),               "Return from Exception" },
    { "PAL", "HALT",       0x00,  0x00, "PAL", QSL_SAFE(),               "Processor Halt" },
    { "PAL", "WRVPTPTR",   0x00,  0x9B, "PAL", QSL_SAFE(),               "Write VPT Pointer" },
    { "PAL", "MFPR",       0x00,  0x9C, "PAL", QSL_SAFE("pr"),           "Move from Processor Reg" },
    { "PAL", "MTPR",       0x00,  0x9D, "PAL", QSL_SAFE("pr","val"),    "Move to Processor Reg" },
    { "PAL", "SWPCTX",     0x00,  0x9E, "PAL", QSL_SAFE(),               "Swap Context" },
    { "PAL", "SIRR",       0x00,  0xA4, "PAL", QSL_SAFE("mask"),        "Set Interrupt Request" },
    { "PAL", "CSIR",       0x00,  0xA5, "PAL", QSL_SAFE("mask"),        "Clear Interrupt Request" },
    { "PAL", "RD_PS",      0x00,  0x9C, "PAL", QSL_SAFE(),               "Read PS" },
    { "PAL", "WR_PS",      0x00,  0x9D, "PAL", QSL_SAFE("val"),         "Write PS" },

    /* Additional PAL opcodes */
    { "PAL", "BPT",        0x00,  0x80, "PAL", QSL_SAFE(),               "Breakpoint Trap" },
    { "PAL", "BPT_ALT",    0x00,  0x81, "PAL", QSL_SAFE(),               "Alternate Breakpoint" },
    { "PAL", "BUGCHK",     0x00,  0x82, "PAL", QSL_SAFE(),               "Kernel Bug Check" },
    { "PAL", "CHMK",       0x00,  0x83, "PAL", QSL_SAFE(),               "Change‑mode to Kernel" },
    { "PAL", "IMB",        0x00,  0x86, "PAL", QSL_SAFE(),               "Instruction Memory Barrier" },
    { "PAL", "SWPIPL",     0x00,  0x9E, "PAL", QSL_SAFE(),               "Swap IPL" },
    { "PAL", "RDPS",       0x00,  0x9C, "PAL", QSL_SAFE(),               "Read PS (alias)" },
    { "PAL", "WRPS",       0x00,  0x9D, "PAL", QSL_SAFE(),               "Write PS (alias)" },
    { "PAL", "RDUNIQUE",   0x00,  0x9F, "PAL", QSL_SAFE(),               "Read UNIQUE" },
    { "PAL", "WRUNIQUE",   0x00,  0xA0, "PAL", QSL_SAFE(),               "Write UNIQUE" },
    { "PAL", "RDVAL",      0x00,  0xA1, "PAL", QSL_SAFE(),               "Read VA" },
    { "PAL", "WRVAL",      0x00,  0xA2, "PAL", QSL_SAFE(),               "Write VA" },
    { "PAL", "RDLOCK",     0x00,  0xA6, "PAL", QSL_SAFE(),               "Read LOCK_FLAG" },
    { "PAL", "WRLOCK",     0x00,  0xA7, "PAL", QSL_SAFE(),               "Write LOCK_FLAG" },
    { "PAL", "TBIS",       0x00,  0xAF, "PAL", QSL_SAFE(),               "Insert I‑TB Entry" },
    { "PAL", "TBAS",       0x00,  0xB0, "PAL", QSL_SAFE(),               "Insert D‑TB Entry" },
    { "PAL", "TBIA",       0x00,  0xB1, "PAL", QSL_SAFE(),               "Invalidate All TLB" },
    { "PAL", "TBISD",      0x00,  0xB2, "PAL", QSL_SAFE(),               "Invalidate Single D‑TB" },
    { "PAL", "TBISI",      0x00,  0xB3, "PAL", QSL_SAFE(),               "Invalidate Single I‑TB" }
};
