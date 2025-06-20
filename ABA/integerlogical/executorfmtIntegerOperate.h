// executorInteger.h
// IntegerExecutor using QVector instead of std::array for its dispatch table
// Eliminates “incomplete array” errors by building the table at runtime.
// Note: requires QtCore/QVector
#pragma once
#include "../AEJ/globalmacro.h"
#include "Assembler.h"
#include "executorfmtLogicalAndShift.h"
#include "structs/floatingPointInstruction.h"
#include "structs/operateInstruction.h"
#include <QtCore/QVector>
#include <structs/helper_floatingPoint.h>
#include "structs/ByteManipulationInstruction.h"
#include "../AEC/RegisterBank.h"
#include "extensions/assemblerBase.h"
#include "structs/memoryInstruction.h"
#include "helpers/executorFmtMFormat.h"

using Assembler = assemblerSpace::Assembler;
class executorFmtIntegerOperate : public IExecutor
{
public:
	using handler = void (executorFmtIntegerOperate::*)(const OperateInstruction&);


	/// Build a ModR/M byte for register-to-register operations:
	///   mod=11? (register), reg=src, rm=dst
	/// See Intel® SDM, “ModR/M Byte” :contentReference[oaicite:2]{index=2}
	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

    explicit executorFmtIntegerOperate(Assembler& assembler)
        : assembler(assembler)
    {

    }
	
	void attachRegisterBank(RegisterBank* regBank) {
		m_registerBank = regBank;
	}
	void execute(const OperateInstruction& inst) {
			OperateInstruction i = inst;

			i.decode(i.opcode);

			// Map primary opcode to subtable index
			static const QVector<quint8> primaries = { 0x10, 0x11, 0x13, 0x1C};
			int pidx = primaries.indexOf(i.opcode);
			if (pidx < 0) return;  // unsupported opcode

			// Lookup in subtable, then by function code
			const auto& sub = dispatchTable()[pidx];
			int fidx = i.fnc & 0x7F;       // lower 7 bits
			handler h = sub.at(fidx);      // safe: sub is QVector<handler>
			if (h) (this->*h)(i);
		}

private:
	RegisterBank* m_registerBank;
    Assembler& assembler;



	// Returns the singleton 2-D dispatch table
	static const QVector<QVector<handler>>& dispatchTable() {
		static const QVector<QVector<handler>> table = createDispatchTable();
		return table;
	}
	static QVector<QVector<handler>> createDispatchTable() 
	{
		QVector<QVector<handler>> all(4);

		auto& t10 = all[0]; // primary opcode 0x10
		auto& t11 = all[1]; // primary opcode 0x11
		auto& t13 = all[2]; // primary opcode 0x11
		auto& t1C = all[3]; // primary opcode 0x1C

		t10[0x00] = &executorFmtIntegerOperate::emitAddl;    // ADDL
		t10[0x40] = &executorFmtIntegerOperate::emitAddL_V;  // AddL/V 
		t10[0x20] = &executorFmtIntegerOperate::emitAddQ;    // ADDQ
		t10[0x60] = &executorFmtIntegerOperate::emitAddQ_V;  // AddQ/V 
		t10[0x0F] = &executorFmtIntegerOperate::emitCmpBge;  // CMPBGE
		t10[0x2D] = &executorFmtIntegerOperate::emitCmpeq;   // CMPEQ (Opr 10.2D) 
		t10[0x6D] = &executorFmtIntegerOperate::emitCmple;   // CMPLE (Opr 10.6D) 
		t10[0x4D] = &executorFmtIntegerOperate::emitCmplt;   // CMPLT (Opr 10.4D) 
		t10[0x3D] = &executorFmtIntegerOperate::emitCmpule;  // CMPULE (Opr 10.3D) 
		t10[0x1D] = &executorFmtIntegerOperate::emitCmpult;  // CMPULT (Opr 10.1D) 
		t10[0x02] = &executorFmtIntegerOperate::emitS4Addl;  // S4ADDL
		t10[0x22] = &executorFmtIntegerOperate::emitS4Addq;  // S4ADDQ
		t10[0x0B] = &executorFmtIntegerOperate::emitS4Subl;  // S4Subl, Scaled (4-bit) Longword Sub   (Opr 10.0B) 
		t10[0x2B] = &executorFmtIntegerOperate::emitS4Subq;  // S4Subq, Scaled (4-bit) Quadword Sub   (Opr 10.2B) 
		t10[0x12] = &executorFmtIntegerOperate::emitS8Addl;  // S8ADDL
		t10[0x32] = &executorFmtIntegerOperate::emitS8Addq;  // S8ADDQ
		t10[0x1B] = &executorFmtIntegerOperate::emitS8SubL;    // S8SUBL (Opr 10.09) 
		t10[0x3B] = &executorFmtIntegerOperate::emitS8Subq;    // S8SUBQ
		t10[0x09] = &executorFmtIntegerOperate::emitSubL;    // SUBL
		t10[0x49] = &executorFmtIntegerOperate::emitSubL_V;  // SUBL/V
		t10[0x29] = &executorFmtIntegerOperate::emitSubQ;    // SUBQ
		t10[0x69] = &executorFmtIntegerOperate::emitSubQ_V;    // SUBQ/V

		t11[0x61] = &executorFmtIntegerOperate::emitAMask;   // AMASK
		t11[0x6C] = &executorFmtIntegerOperate::emitIMPLVER; // IMPLVER
		t11[0x00] = &executorFmtIntegerOperate::emitAnd;   // AND
		t11[0x08] = &executorFmtIntegerOperate::emitBic;   // BIC
		t11[0x20] = &executorFmtIntegerOperate::emitBis;   // BIS
		t11[0x24] = &executorFmtIntegerOperate::emitCMoveQ;  // CMOVEQ
		t11[0x46] = &executorFmtIntegerOperate::emitCMovGe;  // CMOVGE
		t11[0x66] = &executorFmtIntegerOperate::emitCMovGt;   // CMOVGT
		t11[0x16] = &executorFmtIntegerOperate::emitCMovlBc;   // CMOVLBC
		t11[0x14] = &executorFmtIntegerOperate::emitCMovLbs;  // CMOVLBS
		t11[0x64] = &executorFmtIntegerOperate::emitCMovLe;  // CMOVLE
		t11[0x44] = &executorFmtIntegerOperate::emitCMovLt;  // CMOVLT
		t11[0x26] = &executorFmtIntegerOperate::emitCMovNe;  // CMOVNE
		t11[0x48] = &executorFmtIntegerOperate::emitEqv;   // EQV
		t11[0x28] = &executorFmtIntegerOperate::emitOrNot;   // OrNot
	
		t11[0x40] = &executorFmtIntegerOperate::emitXor;   // XOR



		// Multiply instructions (operate format, opcode=0x13)
		t13[0x00] = &executorFmtIntegerOperate::emitMull;   // MULL, Longword Multiply        (Opr 13.00) :contentReference[oaicite:21]{index=21}
		t13[0x40] = &executorFmtIntegerOperate::emitMull_V;   // MULL/V, Longword Multiply        (Opr 13.00) :contentReference[oaicite:21]{index=21}
		t13[0x20] = &executorFmtIntegerOperate::emitMulq;   // MULQ, Quadword Multiply        (Opr 13.20) :contentReference[oaicite:22]{index=22}
		t13[0x60] = &executorFmtIntegerOperate::emitMulq_V;   // MULQ/V, Quadword Multiply        (Opr 13.20) :contentReference[oaicite:22]{index=22}
		t13[0x30] = &executorFmtIntegerOperate::emitUmulh;  // UMULH, Unsigned Multiply High  (Opr 13.30) :contentReference[oaicite:23]{index=23}

		t1C[0x78] = &executorFmtIntegerOperate::emitFtois;   // 
		t1C[0x70] = &executorFmtIntegerOperate::emitFtoit;   //
		t1C[0x32] = &executorFmtIntegerOperate::emitCtlz;   // CTLZ, Count Leading Zero (Opr 1C.32) 
		t1C[0x30] = &executorFmtIntegerOperate::emitCtpop;  // CTPOP, Count Population     (Opr 1C.30) 
		t1C[0x33] = &executorFmtIntegerOperate::emitCttz;   // CTTZ, Count Trailing Zero  (Opr 1C.33) 
		t1C[0x3E] = &executorFmtIntegerOperate::emitMaxsB8;   // MAXSB8 
		t1C[0x3F] = &executorFmtIntegerOperate::emitMaxsW4;   // MAXSW4
		t1C[0x3C] = &executorFmtIntegerOperate::emitMaxsUB8;   // MAXSUB8 
		t1C[0x3D] = &executorFmtIntegerOperate::emitMaxsUW4;   // MAXSUW4
		t1C[0x38] = &executorFmtIntegerOperate::emitMinsB8;   // MinSB8 
		t1C[0x39] = &executorFmtIntegerOperate::emitMinsW4;   // MinSW4
		t1C[0x3A] = &executorFmtIntegerOperate::emitMinsUB8;   // MinSUB8 
		t1C[0x3B] = &executorFmtIntegerOperate::emitMinsUW4;   // MinSUW4
		t1C[0x31] = &executorFmtIntegerOperate::emitPerr;   // PERR 
		t1C[0x37] = &executorFmtIntegerOperate::emitPKLB;   // PKLB
		t1C[0x36] = &executorFmtIntegerOperate::emitPKWB;   // PKWB 
		t1C[0x00] = &executorFmtIntegerOperate::emitSEXTB;  // SEXTB
		t1C[0x01] = &executorFmtIntegerOperate::emitSEXTW;  // SEXTW 
		t1C[0x35] = &executorFmtIntegerOperate::emitUNPKBL;  // UNPKBL
		t1C[0x34] = &executorFmtIntegerOperate::emitUNPKBW;  // UNPKBW



		return all;
	}

// 	/// executorFmtMFormat provides a single entry point for emitting
//     /// any M-format instruction based on the MFormatInstruction struct.
// 	class executorFmtMFormat {
// 	public:
// 		/// Emit an M-format instruction from its decoded fields.
// 		/// @param a   Assembler reference
// 		/// @param op  Decoded M-format instruction containing:
// 		///            - opcode: primary opcode (6 bits)
// 		///            - rd    : destination register (5 bits)
// 		///            - ra    : source register      (5 bits)
// 		///            - width : field width          (6 bits)
// 		///            - pos   : start bit position    (6 bits)
// 		inline static void emitAll(Assembler& a, const MFormatInstruction& op) {
// 			a.emitBits(op.opcode, 6);
// 			a.emitBits(op.rd & 0x1F, 5);
// 			a.emitBits(op.ra & 0x1F, 5);
// 			a.emitBits(op.width & 0x3F, 6);
// 			a.emitBits(op.pos & 0x3F, 6);
// 			a.flushBits();
// 		}
// 
// 	/// Emit a generic operate-format instruction.
//    ///
//    /// 
//    /// @param a      Assembler instance for bit emission
//    /// @param opcode 6-bit opcode (0x10 for arithmetic, 0x11 for logical)
//    /// @param ra     Source register A (0..31)
//    /// @param rb     Source register B (0..31)
//    /// @param func   6-bit function selector
//    /// @param rc     Destination register (0..31)
// 	inline static void emitOperateField(
// 		Assembler& a,
// 		const OperateInstruction& op
// 	) {
// 		a.emitBits(op.opcode, 6);       // primary opcode (0x10 or 0x11)
// 		a.emitBits(op.ra & 0x1F, 5);       // ra
// 		a.emitBits(op.rb & 0x1F, 5);       // rb
// 		a.emitBits(op.fnc & 0x3F, 6);       // function code
// 		a.emitBits(op.rc & 0x1F, 5);       // rc
// 		a.emitBits(0, 5);       // unused
// 		a.flushBits();                     // align for next instruction
// 	}

	inline void emitMGeneric(const OperateInstruction& op) {
		// pull out the five bit-fields:
		MFormatInstruction m{
		  static_cast<uint8_t>((op.raw >> 26) & 0x3F),  // opcode=0x12
		  static_cast<uint8_t>((op.raw >> 21) & 0x1F),  // rd
		  static_cast<uint8_t>((op.raw >> 16) & 0x1F),  // ra
		  static_cast<uint8_t>((op.raw >> 10) & 0x3F),  // width
		  static_cast<uint8_t>((op.raw >> 4) & 0x3F)   // pos
		};
		m_fmtMFormat->emitMFormat(m);
	}

#pragma  region OpCode 10

	// ——————————————————————————————————————————
	// Operate?format (opcode 0x10) scaled?subtract ×8 longword: S8SUBL (fnc=0x1B)
	// Rd = sign_extend32( (Ra<<3) – Rb )
	// 	
    // S8SUBL: Scaled Subtract Longword by 8 (fnc=0x1B under opcode 0x10)
    //   Rc ? SEXT( ((Ra<<3) - Rb)<31:0> )
    // Host: movl?shll?subl?movsxd  :contentReference[oaicite:23]{index=23}
    //
	// ——————————————————————————————————————————
	inline void emitS8SubL(const OperateInstruction& i) {
		uint8_t rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movl(rd, ra);    // load Ra<31:0>             :contentReference[oaicite:11]{index=11}
		assembler.shll(rd, 3);     // rd <<= 3 (×8)            
		assembler.subl(rd, rb);    // rd = rd – Rb<31:0>        :contentReference[oaicite:12]{index=12}
		assembler.movsxd(rd, rd);    // sign?extend 32?64 bits    :contentReference[oaicite:13]{index=13}
	}

	// Subtract longword with overflow?trap qualifier: SUBL/V (fnc=0x49)
	inline void emitSubL_V(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movl(rd, ra);
		assembler.subl(rd, rb);
		assembler.movsxd(rd, rd);

		assembler.subl(i.dest(), i.srcA(), i.srcB());

		// TODO: check & trap on overflow if Alpha’s flag?enable bit is set
		assembler.jo("alpha_trap_overflow");
	}

	// Subtract quadword with overflow?trap qualifier: SUBQ/V (fnc=0x69)
	inline void emitSubQ_V(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);
		assembler.subq(rd, rb);
		// TODO: overflow check+trap
		assembler.jo("alpha_trap_overflow");
	}

	// C++ interpreter fallback for SUBQ/V
	inline void interpSubQ_V(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA());
		uint64_t b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = a - b;
		m_registerBank->writeIntReg(i.dest(), r);

		// Detect two's-complement overflow for subtraction:
		// Overflow happens if a and b have opposite signs and result sign differs from a
		bool overflow = (((int64_t)a ^ (int64_t)b) & ((int64_t)a ^ (int64_t)r)) < 0;
		if (overflow) {
			ctx->notifyTrapRaised(helpers_JIT::TrapType::ARITHMETIC);
		}
	}


	
	// 	inline void emitAddL_V(const OperateInstruction& i) {
	// 		// ADDL/V – same as ADDL semantics, but with overflow?trap enabled by the “V” qualifier.
	// 		// For now, we emit exactly ADDL (longword add + sign?extend).
	// 		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
	// 		assembler.movl(rd, ra);    // load Ra<31:0>
	// 		assembler.addl(rd, rb);    // 32-bit add
	// 		assembler.movsxd(rd, rd);  // sign?extend result to 64 bits
	// 		// TODO: insert overflow check+trap here if overflow?enable is set in FPCR
	// 	}

	inline void emitAddQ_V(const OperateInstruction& i) {
		// ADDQ/V – quadword add with overflow?trap qualifier.
		// For now, identical to ADDQ.
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);   // copy full 64-bit Ra ? Rd
		assembler.addq(rd, rb);   // 64-bit add
		// TODO: insert overflow check+trap if needed
	}

	/*
*
Alpha semantics: for each byte lane, set corresponding byte of RD to 1 if RA_byte ? RB_byte (unsigned), else 0. We can implement this with SSE2 pcmpeqb/pcmpgtb/por+pmovmskb:
*/
//emitCmpBge (fnc 0x0F, CMPBGE)
	inline void emitCmpBge(const OperateInstruction& i) {
		// 1) copy RA into XMMdest
		assembler.movdqa(i.dest(), i.srcA());       // MOVDQA XMMdest, XMMra

		// 2) XMMtmp = (RA == RB) ? 0xFF : 0x00 per byte
		assembler.pcmpeqb(i.dest(), i.srcB());      // PCMPEQB XMMdest, XMMrb

		// 3) XMMgt = (RB > RA) ? 0xFF : 0x00 per byte
		//    we need a *second* XMM register here; let's use srcB as our temp
		assembler.movdqa(i.srcB(), i.srcA());       // MOVDQA XMMtmp, XMMra
		assembler.pcmpgtb(i.srcB(), i.dest());      // PCMPGTB XMMtmp, XMMdest

		// 4) OR equal||greater into XMMdest
		assembler.por(i.dest(), i.srcB());          // POR XMMdest, XMMtmp

		// 5) pack the high-bit of each byte into a GPR and zero-extend
		assembler.pmovmskb(i.dest(), i.dest());     // PMOVMSKB Rd, XMMdest
		// high bits of Rd are already zero
	}


	//emitAddL_V (fnc 0x40, ADDL/V)
	inline void emitAddL_V(const OperateInstruction& i) {
		// 1) RD = RA<31:0>
		assembler.movl(i.dest(), i.srcA());
		// 2) RD += RB<31:0>
		assembler.addl(i.dest(), i.srcB());
		// 3) Sign?extend 32?64 bits
		assembler.movsxd(i.dest(), i.dest());
		// TODO: check Alpha overflow flag and trap if needed
		// if OF=1, jump to your overflow-trap routine
		assembler.jo("alpha_trap_overflow");


	}
	/* 	Alpha semantics : quadword add with overflow?trap qualifier.*/
	//emitAddQ_V (fnc 0x60, ADDQ/V)
	inline void emitAddQ_V(const OperateInstruction& i) {
		// 1) RD = RA (full 64 bits)
		assembler.movq(i.dest(), i.srcA());
		// 2) RD += RB (64 bits, wrap)
		assembler.addq(i.dest(), i.srcB());
		// TODO: check overflow and trap
		assembler.jo("alpha_trap_overflow");
	}

	inline void emitExtBl(const OperateInstruction& op) { 
		
		
	}



	inline void emitAddl(const OperateInstruction& i) {
		int rd = i.dest();
		int ra = i.srcA();
		int rb = i.srcB();

		// 1) RD = RA (32-bit)
		assembler.movl(rd, ra);

		// 2) RD += RB (32-bit)
		assembler.addl(rd, rb);

		// 3) Sign-extend low 32 bits ? full 64 bits
		assembler.movsxd(rd, rd);
	}
	inline  void emitAddl(Assembler& a, int rc, int ra, int rb) { emitOperateField(a, 0x10, ra, rb, 0x00, rc); }

	// ---- Per?opcode handler methods ----
	//
	// 64-bit ADDQ: RD = RA + RB
	// -------------------
	// Intel ISA: REX.W=1 + 0x01 /r (ADD r/m64, r64) 
	//
	inline void emitAddQ(const OperateInstruction& i) {
		// REX.W + ADD r/m64, r64
		assembler.emitRex(/*w=*/true, /*reg=*/i.srcB(), /*rm=*/i.dest());
		assembler.emitByte(0x01);
		assembler.emitByte(modRmGp(i.dest(), i.srcB()));
	}

	//
	// 64-bit SUBQ: RD = RA – RB
	// -----------------
	// Intel ISA: REX.W=1 + 0x29 /r (SUB r/m64, r64) 
	//
	inline void emitSubq(const OperateInstruction& i) {
		assembler.emitRex(/*w=*/true, /*reg=*/i.srcB(), /*rm=*/i.dest());
		assembler.emitByte(0x29);
		assembler.emitByte(modRmGp(i.dest(), i.srcB()));
	}
	//
	// 32-bit S4ADDL: RD = sign_extend32( (RA<<2) + RB )
	// ----------------
	//   1) Left?shift RA by 2 bits
	//   2) Add RB (32-bit wrap)
	//   3) Sign-extend result into 64 bits
	//
	inline void emitS4Addl(const OperateInstruction& i) {
		// 1) MOV EAX, [GPR+RA*8]
		assembler.emitRex(/*w=*/false, /*reg=*/i.srcA(), /*rm=*/i.srcA());
		assembler.emitByte(0x8B);                // MOV r32, r/m32
		assembler.emitByte(modRmGp(i.srcA(), i.srcA()));

		// 2) SHL EAX, imm=2
		assembler.emitByte(0xC1);                // SHIFT r/m32 by imm8
		assembler.emitByte(modRmGp(i.srcA(), /*srcA*/ 0)); // modRm(reg=RA, rm=000 for EAX)
		assembler.emitByte(2);

		// 3) ADD EAX, [GPR+RB*8]
		assembler.emitByte(0x03);                // ADD r32, r/m32
		assembler.emitByte(modRmGp(i.srcA(), i.srcB()));

		// 4) CWDE: sign-extend EAX?RAX
		assembler.emitByte(0x98);                // CWDE – Convert Word to Double?word

		// 5) MOV [GPR+RD*8], EAX
		assembler.emitByte(0x89);                // MOV r/m32, r32
		assembler.emitByte(modRmGp(i.dest(), i.srcA()));
	}
	// executorIntegerAddq.h
	// Adds 64-bit scaled?add opcode handlers to IntegerExecutor:
	//   S4ADDQ (fnc=0x22): RD = (RA << 2) + RB
	//   S8ADDQ (fnc=0x32): RD = (RA << 3) + RB
	//
	// Relies on your Assembler extensions:
	//   movq(dst, src) – MOV r64, r64    (REX.W + 8B /r)            
	//   shlq(dst, imm) – SHL r/m64, imm8  (REX.W + C1 /4 ib)         
	//   addq(dst, src) – ADD r/m64, r64    (REX.W + 01 /r)            


	inline void emitS4Addq(const OperateInstruction& inst) {
		// fnc==0x22
		auto rd = inst.dest();
		auto ra = inst.srcA();
		auto rb = inst.srcB();

		// rd = ra
		assembler.movq(rd, ra);
		// rd <<= 2
		assembler.shlq(rd, 2);
		// rd += rb
		assembler.addq(rd, rb);
	}

	inline void emitS8Addq(const OperateInstruction& inst) {
		// fnc==0x32
		auto rd = inst.dest();
		auto ra = inst.srcA();
		auto rb = inst.srcB();

		// rd = ra
		assembler.movq(rd, ra);
		// rd <<= 3
		assembler.shlq(rd, 3);
		// rd += rb
		assembler.addq(rd, rb);
	}

	/// S8ADDL (fnc=0x12): Scaled (×8) longword add and sign-extend  (= Opr 10.12) 
	void emitS8Addl(const OperateInstruction& i) {
		// S8ADDL: RD = sign_extend32( (RA << 3) + RB )
		auto rd = i.dest();
		auto ra = i.srcA();
		auto rb = i.srcB();

		// 1) rd = ra
		assembler.movl(rd, ra);

		// 2) rd <<= 3
		assembler.shll(rd, 3);

		// 3) rd += rb
		assembler.addl(rd, rb);

		// 4) sign-extend low 32 bits ? full 64 bits
		assembler.movsxd(rd, rd);
	}


	/// CMP EQ -> 1 if Ra == Rb, else 0
	inline void emitCmpeq(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);      // compare Ra,Rb
		assembler.sete(rd);          // set low-byte rd = (ZF ? 1 : 0)
		assembler.movzbq(rd, rd);     // zero-extend byte?64-bit
	}

	/// CMP LE -> 1 if (int64)Ra <= (int64)Rb, else 0
	inline void emitCmple(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);      // compare Ra,Rb
		assembler.setle(rd);         // set if SF?OF or ZF
		assembler.movzbq(rd, rd);     // zero-extend
	}

	/// CMP LT -> 1 if (int64)Ra <  (int64)Rb, else 0
	inline void emitCmplt(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);      // compare Ra,Rb
		assembler.setl(rd);          // set if SF?OF
		assembler.movzbq(rd, rd);     // zero-extend
	}

	// CMPULT: rd = (uint64_t)Ra < (uint64_t)Rb ? 1 : 0
	inline void emitCmpult(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);       // compare Ra,Rb
		assembler.setb(rd);           // set if CF=1 (unsigned below)
		assembler.movzbq(rd, rd);     // zero?extend byte?64-bit
	}

	// CMPULE: rd = (uint64_t)Ra ? (uint64_t)Rb ? 1 : 0
	inline void emitCmpule(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);       // compare Ra,Rb
		assembler.setbe(rd);          // set if CF=1 or ZF=1 (unsigned ?)
		assembler.movzbq(rd, rd);     // zero?extend
	}

	inline void emitCmpeqL(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpl(ra, rb);
		assembler.sete(rd);
		assembler.movzbq(rd, rd);
	}
	inline void emitCmpltL(const OperateInstruction& i) {
		assembler.cmpl(i.srcA(), i.srcB());
		assembler.setl(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpleL(const OperateInstruction& i) {
		assembler.cmpl(i.srcA(), i.srcB());
		assembler.setle(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpultL(const OperateInstruction& i) {
		assembler.cmpl(i.srcA(), i.srcB());
		assembler.setb(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpuleL(const OperateInstruction& i) {
		assembler.cmpl(i.srcA(), i.srcB());
		assembler.setbe(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}

	// ——————————————————————————————————————————
// Compare signed “quadword” (64-bit) ? result in Rd
// ——————————————————————————————————————————
	inline void emitCmpeqQ(const OperateInstruction& i) {
		assembler.cmpq(i.srcA(), i.srcB());
		assembler.sete(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpltQ(const OperateInstruction& i) {
		assembler.cmpq(i.srcA(), i.srcB());
		assembler.setl(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpleQ(const OperateInstruction& i) {
		assembler.cmpq(i.srcA(), i.srcB());
		assembler.setle(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpultQ(const OperateInstruction& i) {
		assembler.cmpq(i.srcA(), i.srcB());
		assembler.setb(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}
	inline void emitCmpuleQ(const OperateInstruction& i) {
		assembler.cmpq(i.srcA(), i.srcB());
		assembler.setbe(i.dest());
		assembler.movzbq(i.dest(), i.dest());
	}

	// Count Leading Zeros
	inline 	void emitCtlz(const OperateInstruction& i) {
		// Alpha CTLZ semantics: Rd ? number of leading zero bits in Ra
		assembler.lzcntq(i.dest(), i.srcA());
	}




	//
	// CTTZ: Count Trailing Zero (fnc=0x33)
	//   Rc ? cttz(Rb)  (7-bit result, zero in high bits)
	// Host: TZCNT r64, r/m64   (F3 0F BC /r) :contentReference[oaicite:17]{index=17}
	//
	inline void emitCttz(const OperateInstruction& i) {
		assembler.tzcntq(i.dest(), i.srcB());
	}

	//
	// MULL: Longword Multiply (fnc=0x00 under opcode 0x13)
	//   Rc ? SEXT( (Ra<31:0> * Rb<31:0>)<31:0> )
	// Host (32?64): movl?imull?movsxd  :contentReference[oaicite:18]{index=18}
	//
	inline void emitMull(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movl(rd, ra);        // load 32-bit Ra
		assembler.imull(rd, rb);       // signed 32-bit multiply into rd
		assembler.movsxd(rd, rd);      // sign-extend to 64 bits
	}

	//
	// MULQ: Quadword Multiply (fnc=0x20 under opcode 0x13)
	//   Rc ? Rav * Rbv  (signed 64×64?low 64 bits)
	// Host: IMUL r64, r/m64   (REX.W 0F AF /r) :contentReference[oaicite:19]{index=19}
	//
	inline void emitMulq(const OperateInstruction& i) {
		assembler.movq(i.dest(), i.srcA());   // copy Ra?Rd
		assembler.imulq(i.dest(), i.srcB());  // signed 64-bit multiply
	}

	//
	// UMULH: Unsigned Quadword Multiply High (fnc=0x30 under opcode 0x13)
	//   Rc ? high-64(Ra * Rb)  (unsigned 64×64 ? 128 bits)
	// Host: MUL r/m64 (implicit RAX?RDX:RAX), mov RDX?Rc :contentReference[oaicite:20]{index=20}
	//
	 inline void emitUmulh(const OperateInstruction& i) {
		// Unsigned Multiply High (fnc=0x30)
		// Alpha: Rc = high_64( Ra * Rb )
		//
		// Host (x86-64):
		//   RAX ? Ra
		//   MUL  Rb            ; unsigned multiply ? RDX:RAX
		//   Rd  ? RDX

		constexpr int HOST_RAX = 0;  // x86-64 RAX
		constexpr int HOST_RDX = 2;  // x86-64 RDX

		// 1) move Ra into RAX
		assembler.movq(HOST_RAX, i.srcA());

		// 2) unsigned multiply RAX * Rb ? RDX:RAX
		//    F7 /4 with REX.W=1
		assembler.emitRex(/*W=*/true, /*reg=*/HOST_RAX, /*rm=*/i.srcB());
		assembler.emitByte(0xF7);
		assembler.emitByte(modRmGp(/*dst=*/HOST_RAX, /*src=*/i.srcB()));

		// 3) move the high half (RDX) into Rd
		assembler.movq(i.dest(), HOST_RDX);
	}

	//
	// SUBL: Subtract Longword (fnc=0x09 under opcode 0x10)
	//   Rc ? SEXT( (Ra<31:0> - Rb<31:0>)<31:0> )
	// Host: movl?subl?movsxd  :contentReference[oaicite:21]{index=21}
	//
	 inline void emitSubL(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movl(rd, ra);
		assembler.subl(rd, rb);
		assembler.movsxd(rd, rd);
	}

	//
	// S4Subl: Scaled Subtract Longword by 4 (fnc=0x0B under opcode 0x10)
	//   Rc ? SEXT( ((Ra<<2) - Rb)<31:0> )
	// Host: movl?shll?subl?movsxd  :contentReference[oaicite:22]{index=22}
	//
	 inline void emitS4Subl(const OperateInstruction& i) {
		int rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movl(rd, ra);
		assembler.shll(rd, 2);
		assembler.subl(rd, rb);
		assembler.movsxd(rd, rd);
	}



	//
	// SUBQ: Subtract Quadword (fnc=0x29 under opcode 0x10)
	//   Rc ? Ra - Rb (64-bit wrap)
	// Host: subq  :contentReference[oaicite:24]{index=24}
	//
	void emitSubQ(const OperateInstruction& i) {
		assembler.subq(i.dest(), i.srcB());
	}


	//
	// S4SUBQ: Scaled Subtract Quadword by 4 (fnc=0x2B under opcode 0x10)
	//   Rc ? (Ra<<2) - Rb
	// Host: movq?shlq?subq :contentReference[oaicite:25]{index=25}
	//
	inline void emitS4Subq(const OperateInstruction& i) {
		assembler.movq(i.dest(), i.srcA());
		assembler.shlq(i.dest(), 2);
		assembler.subq(i.dest(), i.srcB());
	}

	//
	// S8SUBQ: Scaled Subtract Quadword by 8 (fnc=0x3B under opcode 0x10)
	//   Rc ? (Ra<<3) - Rb
	// Host: movq?shlq?subq :contentReference[oaicite:26]{index=26}
	//
	inline void emitS8Subq(const OperateInstruction& i) {
		assembler.movq(i.dest(), i.srcA());
		assembler.shlq(i.dest(), 3);
		assembler.subq(i.dest(), i.srcB());
	}

#pragma endregion opCode 10
#pragma region OpCode 11

	inline void emitIMPLVER(const OperateInstruction& i)
	{

	}

	// ——————————————————————————————————————————
// MULL/V: Longword Multiply with overflow?trap qualifier (fnc=0x40)
//   Rc = sign_extend32( (Ra<31:0> * Rb<31:0>)<31:0> )
//   alpha spec: Opr13.00 + V bit :contentReference[oaicite:4]{index=4}
// ——————————————————————————————————————————
	inline void emitMull_V(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();

		// 1) MOVL Rd, Ra  (load Ra<31:0>)
		assembler.movl(rd, ra);

		// 2) IMULL Rd, Rb (signed 32×32?32 low bits)
		assembler.imull(rd, rb);

		// 3) MOVSXD Rd, Rd (sign?extend low 32 bits ? 64)
		assembler.movsxd(rd, rd);

		// TODO: check overflow (via CF/OF) and trap if V?bit enabled in FPCR
	}

	// ——————————————————————————————————————————
	// MULQ/V: Quadword Multiply with overflow?trap qualifier (fnc=0x60)
	//   Rc = (int64)Ra * (int64)Rb  (low 64 bits), with overflow semantics
	//   alpha spec: Opr13.20 + V bit :contentReference[oaicite:5]{index=5}
	// ——————————————————————————————————————————
	inline void emitMulq_V(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();

		// 1) MOVQ Rd, Ra  (copy full 64-bit Ra)
		assembler.movq(rd, ra);

		// 2) IMULQ Rd, Rb (signed 64×64?low 64 bits)
		assembler.imulq(rd, rb);

		// TODO: detect signed?overflow if needed and trap
	}

	// ——————————————————————————————————————————
	// UMULH: Unsigned Multiply High (fnc=0x30)
	//   Rc = high_64( (uint64)Ra * (uint64)Rb )  (upper half of 128-bit product)
	//   alpha spec: Opr13.30 :contentReference[oaicite:6]{index=6}
	// ——————————————————————————————————————————
	inline void emitUmulh(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();

		// On x86-64 unsigned `MUL` uses RAX?RDX:RAX:
		//   MOVQ RAX, Ra
		assembler.movq(/*raxHostReg*/, ra);

		//   MUL  Rb  (unsigned multiply, implicit RAX*Rb ? RDX:RAX)
		assembler.emitByte(0x48);                       // REX.W for RAX/RDX
		assembler.emitByte(0xF7);                       // MUL r/m64
		assembler.emitByte(modRmGp(/*raxHostReg*/, rb));

		//   MOVQ Rd, RDX
		assembler.movq(rd, /*rdxHostReg*/);
	}

	// ——————————————————————————————————————————
	// Logical/Shift?format (opcode 0x11) AMASK (fnc=0x61)
	// Rd = byte?mask generated from Ra and Rb as per Alpha AMASK spec
	// ——————————————————————————————————————————
	//inline void LogicalAndShiftExecutor::emitAMASK(const OperateInstruction& i) {//
	// In LogicalAndShiftExecutor (handles Opr 0x11 instructions)

	/// AMASK register variant (fnc=0x61)
	///   Rd = byte-mask: for each byte lane b < (Rb & 0x3F),
	///                  if Ra<8*b+7> == 1 then Rd_byte[b]=0xFF else 0x00
	inline void emitAMask(const OperateInstruction& i) {
		auto rd = i.dest();
		auto ra = i.srcA();
		auto rb = i.srcB();
		// decode literal vs register form:
		bool isLiteral = i.isLiteral;   // inst.decode() must run before
		uint8_t count = isLiteral ? i.rb : /* read GPR Rb */ i.registers[rb] & 0x3F;
		uint64_t mask = 0;
		uint64_t val = /* read GPR Ra */;
		for (int b = 0; b < count; ++b) {
			if (val & (0x80ull << (b * 8))) mask |= (0xFFull << (b * 8));
		}
		assembler.movImm64(rd, mask);  // load constant mask into Rd
	}
	// ——————————————————————————————————————————
    // Logical/Shift?format (opcode 0x11) CMOVLBC (fnc=0x16)
    // Conditional move if low bit of Ra is clear:
    //   if ((Ra & 1) == 0) Rd = Ra; else Rd = Rd (unchanged)
    // ——————————————————————————————————————————
	inline void emitCMovlBc(const OperateInstruction& i) {
		uint8_t rd = i.dest(), ra = i.srcA();
		// copy Ra ? Rd unconditionally
		assembler.movq(rd, ra);               // 64-bit copy  :contentReference[oaicite:14]{index=14}
		// test low bit of Ra (mask = 1)
		assembler.movImm64(/*tmpReg=*/RCX, 1);  // load constant 1 into a temp GPR
		assembler.testq(ra, /*tmpReg=*/RCX);     // ZF=1 if (Ra&1)==0 :contentReference[oaicite:15]{index=15}
		assembler.cmovz(rd, ra);                // if ZF, copy Ra?Rd  
	}
	
	/// AMASK literal variant (#b.ib,Rc.wq) is handled the same way—
	/// the literal is already captured in inst.rb when inst.isLiteral==true.


	// Bitwise AND: AND (fnc=0x00)
	inline void emitAnd(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);
		assembler.andq(rd, rb);
	}
	//
	// Bit Clear: BIC (fnc=0x08) — rd = ra & ~rb
	inline void emitBic(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();

		// rd = ra
		assembler.movq(rd, ra);

		// tmp = ~rb
		// we’ll use, say, RCX as a temporary—pick whatever GPR you reserve
		assembler.movq(6, rb);   // 6 is RCX in x86-64 encoding
		assembler.notq(6);       // RCX = ~RB

		// rd &= tmp  (i.e. ra & ~rb)
		assembler.andq(rd, 6);
	}

	// Bitwise OR: BIS (fnc=0x20)
	inline void emitBis(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);
		assembler.orq(rd, rb);
	}

	// Conditional Move Equal: CMOVEQ (fnc=0x24)
	inline void emitCMoveQ(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);
		assembler.cmove(rd, ra);   // if equal copy ra?rd
	}

	// Conditional Move GE: CMOVGE (fnc=0x46)
	inline void emitCMovGe(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);
		assembler.cmovge(rd, ra);
	}

	// Conditional Move GT: CMOVGT (fnc=0x66)
	inline void emitCMovGt(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);
		assembler.cmovg(rd, ra);
	}

	// Conditional Move LBC (low?bit clear): CMOVLBC (fnc=0x16)
	inline void emitCMovLbc(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA();
		// test low bit of ra:
		assembler.test(ra, 1);
		assembler.cmovz(rd, ra);  // if zero, copy
	}

	// Conditional Move LBS (low?bit set): CMOVLBS (fnc=0x14)
	inline void emitCMovLbs(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA();
		assembler.test(ra, 1);
		assembler.cmovnz(rd, ra);
	}

	// Conditional Move LE: CMOVLE (fnc=0x64)
	inline void emitCMovLe(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);
		assembler.cmovle(rd, ra);
	}

	// Conditional Move LT: CMOVLT (fnc=0x44)
	inline void emitCMovLt(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);
		assembler.cmovl(rd, ra);
	}

	// Conditional Move NE: CMOVNE (fnc=0x26)
	inline void emitCMovNe(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.cmpq(ra, rb);
		assembler.cmovne(rd, ra);
	}

	// Bitwise EQV (equivalent): EQV (fnc=0x48) — rd = ~(ra ^ rb)
	inline void emitEqv(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);
		assembler.xorq(rd, rb);
		assembler.notq(rd);
	}

	// OR NOT: ORNOT (fnc=0x28) — rd = ra | ~rb
	inline void emitOrNot(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);
		assembler.movq(/*tmp*/, rb);
		assembler.notq(/*tmp*/);
		assembler.orq(rd, /*tmp*/);
	}

	// Implementation Version: IMPLVER (fnc=0x6C)
	inline void emitImplVer(const OperateInstruction& i) {
		// typically returns a constant version code in R0; stub:
		assembler.movq(i.dest(), /*immediate version constant*/);
	}

	// Bitwise XOR: XOR (fnc=0x40)
	inline void emitXor(const OperateInstruction& i) {
		auto rd = i.dest(), ra = i.srcA(), rb = i.srcB();
		assembler.movq(rd, ra);
		assembler.xorq(rd, rb);
	}


#pragma endregion OpCode 11
#pragma region Opcode 1C

	// Floating?Integer conversions (fnc 0x70/0x78):
	inline void emitFtoit(const OperateInstruction& i) {
		// FTOIT: single-precision ? integer (round toward zero)
		float  f = std::bit_cast<float>(static_cast<uint32_t>(m_registerBank->readIntReg(i.srcA())));
		m_registerBank->writeIntReg(i.dest(), static_cast<uint64_t>(static_cast<int32_t>(f)));
	}
	inline void emitFtois(const OperateInstruction& i) {
		// FTOIS: single-precision ? integer (round to nearest even)
		float  f = std::bit_cast<float>(static_cast<uint32_t>(m_registerBank->readIntReg(i.srcA())));
		int32_t r = std::nearbyintf(f);
		m_registerBank->writeIntReg(i.dest(), static_cast<uint64_t>(r));
	}




	// ------------------------------------------------------------------------
   // Extract fixed-width fields
   // ------------------------------------------------------------------------

// 	//Extract fixed-width fields
// 	inline  void emitExtLh(OperateInstruction& i) {
// 		m_fmtMFormat->emitMFormat();
// 	}

// 	inline  void emitExtLl(Assembler& a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 16, pos); }
// 	inline  void emitExtQh(Assembler& a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 64, pos); }
// 	inline  void emitExtQl(Assembler& a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 64, pos); }
// 	inline  void emitExtWh(Assembler& a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 32, pos); }
// 	inline  void emitExtWl(Assembler& a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 32, pos); }
// 
// 	// ------------------------------------------------------------------------
// 	// Insert fixed-width fields
// 	// ------------------------------------------------------------------------
// 		inline  void emitInsBl(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 8, pos); }
// 		inline  void emitInsBh(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 8, pos); }
// 		inline  void emitInsLh(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 16, pos); }
// 		inline  void emitInsLl(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 16, pos); }
// 		inline  void emitInsQh(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 64, pos); }
// 		inline  void emitInsQl(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 64, pos); }
// 		inline  void emitInsWh(Assembler & a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 32, pos); }

	//------------------------------------------------------------------------
	// Mask fixed-width fields (Mask bits above `pos`)
	// ------------------------------------------------------------------------

	//inline  void emitInsWl(Assembler& a, int rd, int ra, int pos) { emitMFormatField(a, rd, ra, 32, pos); }

	// 0x00: SEXTB  (sign?extend byte)
	inline void emitSEXTB(const OperateInstruction& i) {
		m_registerBank->writeIntReg(i.dest(),
			static_cast<uint64_t>(
				static_cast<int64_t>(
					static_cast<int8_t>(m_registerBank->readIntReg(i.srcA()) & 0xFF)
					)
				)
		);
	}

	// 0x01: SEXTW  (sign?extend word)
	inline void emitSEXTW(const OperateInstruction& i) {
		m_registerBank->writeIntReg(i.dest(),
			static_cast<uint64_t>(
				static_cast<int64_t>(
					static_cast<int16_t>(m_registerBank->readIntReg(i.srcA()) & 0xFFFF)
					)
				)
		);
	}

	// 0x02: MSKBL  (mask byte leftmost)
	inline void emitMskBl(const OperateInstruction& i) {
		uint64_t v = m_registerBank->readIntReg(i.srcA());
		uint8_t n = static_cast<uint8_t>(v & 0x3F);
		uint64_t m = 0;
		for (int b = 0; b < n && b < 8; ++b) m |= (0xFFull << ((7 - b) * 8));
		m_registerBank->writeIntReg(i.dest(), m);
	}

	// 0x12: MSKWL  (mask byte low)
	inline void emitMskWl(const OperateInstruction& i) {
		uint64_t v = m_registerBank->readIntReg(i.srcA());
		uint8_t n = static_cast<uint8_t>(v & 0x3F);
		uint64_t m = (n >= 8 ? 0xFFFFFFFFFFFFFFFFull : ((1ull << (n * 8)) - 1));
		m_registerBank->writeIntReg(i.dest(), m);
	}

	// 0x22: MSKLH  (mask halfword high)
	inline void emitMskLh(const OperateInstruction& i) {
		uint64_t v = m_registerBank->readIntReg(i.srcA());
		uint8_t n = static_cast<uint8_t>(v & 0x3F);
		uint64_t m = 0;
		for (int h = 0; h < n && h < 4; ++h) m |= (0xFFFFull << ((3 - h) * 16));
		m_registerBank->writeIntReg(i.dest(), m);
	}

	// 0x32: MSKLW  (mask halfword low)
	inline void emitMskLl(const OperateInstruction& i) {
		uint64_t v = m_registerBank->readIntReg(i.srcA());
		uint8_t n = static_cast<uint8_t>(v & 0x3F);
		uint64_t m = (n >= 4 ? 0xFFFFFFFFFFFFFFFFull : ((1ull << (n * 16)) - 1));
		m_registerBank->writeIntReg(i.dest(), m);
	}

	// 0x52: MSKWH  (mask word high)
	inline void emitMskWh(const OperateInstruction& i) {
		uint64_t v = m_registerBank->readIntReg(i.srcA());
		uint8_t n = static_cast<uint8_t>(v & 0x3F);
		uint64_t m = 0;
		for (int w = 0; w < n && w < 2; ++w) m |= (0xFFFFFFFFull << ((1 - w) * 32));
		m_registerBank->writeIntReg(i.dest(), m);
	}

	// 0x62: MSKWL  (mask word low)
	inline void emitMskQl(const OperateInstruction& i) {
		uint64_t v = m_registerBank->readIntReg(i.srcA());
		uint8_t n = static_cast<uint8_t>(v & 0x3F);
		uint64_t m = (n >= 2 ? 0xFFFFFFFFFFFFFFFFull : ((1ull << (n * 32)) - 1));
		m_registerBank->writeIntReg(i.dest(), m);
	}
	// 0x30: CTPOP
 	inline void emitCtpop(const OperateInstruction& i) {
		uint64_t vb = m_registerBank->readIntReg(i.srcB());
		uint64_t count = __builtin_popcountll(vb);   // counts bits set in vb
		m_registerBank->writeIntReg(i.dest(), count);
 	}
	//
// CTPOP: Count Population (fnc=0x30)
//   Rc ? popcount(Rb)  (7-bit result, zero in high bits)
// Host: POPCNT r64, r/m64   (F3 0F B8 /r) :contentReference[oaicite:16]{index=16}
//
//	inline void emitCtpop(const OperateInstruction& i) {
//		assembler.popcntq(i.dest(), i.srcB());
//	}

#include <bit>
	// 0x31: PERR (parity?error = !even parity per byte ? 1 in dest if any byte odd)
	inline void emitPerr(const OperateInstruction& i) {
		auto x = m_registerBank->readIntReg(i.srcA());
		bool anyOdd = false;
 		for (int b = 0; b < 8; ++b) {
 			uint8_t vb = (x >> (b * 8)) & 0xFF;
 			if (__builtin_parity(vb)) { anyOdd = true; break; }
 		}
		uint64_t x = m_registerBank->readIntReg(i.srcA());
		uint64_t pop = std::popcount(x);
		uint64_t lz = x ? std::countl_zero(x) : 64;
		uint64_t tz = x ? std::countr_zero(x) : 64;
		m_registerBank->writeIntReg(i.dest(), anyOdd ? 1ull : 0ull);
	}

	// 0x32: CTLZ
	inline void emitCtlz(const OperateInstruction& i) {
		auto x = m_registerBank->readIntReg(i.srcA());
		m_registerBank->writeIntReg(i.dest(),
			x == 0 ? 64ull : static_cast<uint64_t>(__builtin_clzll(x))
		);
	}

	// 0x33: CTTZ
	inline void emitCttz(const OperateInstruction& i) {
		auto x = m_registerBank->readIntReg(i.srcA());
		m_registerBank->writeIntReg(i.dest(),
			x == 0 ? 64ull : static_cast<uint64_t>(__builtin_ctzll(x))
		);
	}

#ifndef Q_MOC_RUN

	template<int Bits, typename T, T(*Cmp)(T, T)>
	inline void emitMinMaxBW(const OperateInstruction& i) {
		static_assert(Bits == 8 || Bits == 16, "Supported widths only");
		uint64_t a = m_registerBank->readIntReg(i.srcA());
		uint64_t b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		constexpr int Lanes = 64 / Bits;
		for (int k = 0; k < Lanes; ++k) {
			T va = static_cast<T>((a >> (k * Bits)) & ((1ull << Bits) - 1));
			T vb = static_cast<T>((b >> (k * Bits)) & ((1ull << Bits) - 1));
			T m = Cmp(va, vb);
			uint64_t mm = static_cast<std::make_unsigned_t<T>>(m);
			r |= (mm & ((1ull << Bits) - 1)) << (k * Bits);
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}



	template<int Bits, typename T, T(*Cmp)(T, T)>
	inline void emitMinMaxBW(const OperateInstruction& i) {
		// … template code …
	}
#endif


	

		

		

	
	//PKLB combines low bytes of Ra and Rb into alternating bytes.
	inline void emitPKLB(const OperateInstruction& i) {
		// Pack Low Bytes (PKLB): Rd = { Ra<7:0>, Rb<7:0>, Ra<15:8>, Rb<15:8>, … } 
		uint64_t a = m_registerBank->readIntReg(i.srcA());
		uint64_t b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		// 4 pairs ? 8 bytes total
		for (int j = 0; j < 4; ++j) {
			uint8_t va = static_cast<uint8_t>(a >> (j * 8));
			uint8_t vb = static_cast<uint8_t>(b >> (j * 8));
			r |= uint64_t(va) << (j * 16);
			r |= uint64_t(vb) << (j * 16 + 8);
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}
	// PKWB combines low words of Ra and Rb into alternating halfwords.
	inline void emitPKWB(const OperateInstruction& i) {
		// Pack Low Words (PKWB): Rd = { Ra<15:0>, Rb<15:0>, Ra<31:16>, Rb<31:16> }
		uint64_t a = m_registerBank->readIntReg(i.srcA());
		uint64_t b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int j = 0; j < 2; ++j) {
			uint16_t wa = static_cast<uint16_t>(a >> (j * 16));
			uint16_t wb = static_cast<uint16_t>(b >> (j * 16));
			r |= uint64_t(wa) << (j * 32);
			r |= uint64_t(wb) << (j * 32 + 16);
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	//UNPKBL zero - extends each byte of Ra into a 16 - bit halfword.
	inline void emitUNPKBL(const OperateInstruction& i) {
		// Unpack Low Bytes into Halfwords (UNPKBL):
		// Rd<15:0>   = Ra<7:0>,   Rd<31:16> = Ra<15:8>, etc.
		uint64_t a = m_registerBank->readIntReg(i.srcA());
		uint64_t r = 0;
		// 4 halfwords ? 8 bytes total
		for (int j = 0; j < 4; ++j) {
			// extract the j-th byte
			uint8_t vb = static_cast<uint8_t>((a >> (j * 8)) & 0xFF);
			// place it into the lower byte of the j-th 16-bit halfword
			r |= uint64_t(vb) << (j * 16);
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	//	UNPKBW zero - extends each 16 - bit word of Ra into a 32 - bit longword.
	inline void emitUNPKBW(const OperateInstruction& i) {
		// Unpack Low Words into Longwords (UNPKBW):
		// Rd<31:0>   = Ra<15:0>,   Rd<63:32> = Ra<31:16>
		uint64_t a = m_registerBank->readIntReg(i.srcA());
		uint16_t w0 = static_cast<uint16_t>(a & 0xFFFF);
		uint16_t w1 = static_cast<uint16_t>((a >> 16) & 0xFFFF);
		uint64_t r = uint64_t(w0) | (uint64_t(w1) << 32);
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// Saturating/MAX/MIN byte/halfword variants:

	// 0x3E: MAXSB8  (max signed byte, 8 lanes)
	inline void emitMaxsB8(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 8; ++lane) {
			int8_t va = int8_t(a >> (lane * 8));
			int8_t vb = int8_t(b >> (lane * 8));
			int8_t m = std::max(va, vb);
			r |= (uint64_t(uint8_t(m)) << (lane * 8));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x3F: MAXSW4  (max signed halfword, 4 lanes)
	inline void emitMaxsW4(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 4; ++lane) {
			int16_t va = int16_t(a >> (lane * 16));
			int16_t vb = int16_t(b >> (lane * 16));
			int16_t m = std::max(va, vb);
			r |= (uint64_t(uint16_t(m)) << (lane * 16));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x3C: MAXSUB8 (max unsigned byte, 8 lanes)
	inline void emitMaxsUB8(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 8; ++lane) {
			uint8_t va = uint8_t(a >> (lane * 8));
			uint8_t vb = uint8_t(b >> (lane * 8));
			uint8_t m = std::max(va, vb);
			r |= (uint64_t(m) << (lane * 8));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x3D: MAXSUW4 (max unsigned halfword, 4 lanes)
	inline void emitMaxsUW4(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 4; ++lane) {
			uint16_t va = uint16_t(a >> (lane * 16));
			uint16_t vb = uint16_t(b >> (lane * 16));
			uint16_t m = std::max(va, vb);
			r |= (uint64_t(m) << (lane * 16));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x38: MINSB8 (min signed byte, 8 lanes)
	inline void emitMinsB8(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 8; ++lane) {
			int8_t va = int8_t(a >> (lane * 8));
			int8_t vb = int8_t(b >> (lane * 8));
			int8_t m = std::min(va, vb);
			r |= (uint64_t(uint8_t(m)) << (lane * 8));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x39: MINSW4 (min signed halfword, 4 lanes)
	inline void emitMinsW4(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 4; ++lane) {
			int16_t va = int16_t(a >> (lane * 16));
			int16_t vb = int16_t(b >> (lane * 16));
			int16_t m = std::min(va, vb);
			r |= (uint64_t(uint16_t(m)) << (lane * 16));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x3A: MINSUB8 (min unsigned byte, 8 lanes)
	inline void emitMinsUB8(const OperateInstruction& i) {
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 8; ++lane) {
			uint8_t va = uint8_t(a >> (lane * 8));
			uint8_t vb = uint8_t(b >> (lane * 8));
			uint8_t m = std::min(va, vb);
			r |= (uint64_t(m) << (lane * 8));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

	// 0x3B: MINSUW4 (min unsigned halfword, 4 lanes)
	inline void emitMinsUW4(const OperateInstruction& i) 
	{
		uint64_t a = m_registerBank->readIntReg(i.srcA()), b = m_registerBank->readIntReg(i.srcB());
		uint64_t r = 0;
		for (int lane = 0; lane < 4; ++lane) {
			uint16_t va = uint16_t(a >> (lane * 16));
			uint16_t vb = uint16_t(b >> (lane * 16));
			uint16_t m = std::min(va, vb);
			r |= (uint64_t(m) << (lane * 16));
		}
		m_registerBank->writeIntReg(i.dest(), r);
	}

#pragma endregion Opcode 1C


};

