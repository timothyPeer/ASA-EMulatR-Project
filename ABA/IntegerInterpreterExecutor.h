#pragma once
#include "../ABA/executors/IExecutor.h"
#include "structs/operateInstruction.h"
#include <QVector>
#include "integerlogical/executorfmtIntegerOperate.h"
#
class IntegerInterpreterExecutor :
    public IExecutor
{
	using handler = void (executorFmtIntegerOperate::*)(const OperateInstruction&);
    IntegerInterpreterExecutor() {

    }
    ~IntegerInterpreterExecutor() = default;
	void execute(const OperateInstruction& inst) {
		OperateInstruction i = inst;

		i.decode(i.opcode);

		// Map primary opcode to subtable index
		static const QVector<quint8> primaries = { 0x10, 0x11, 0x13, 0x1C };
		int pidx = primaries.indexOf(i.opcode);
		if (pidx < 0) return;  // unsupported opcode

		// Lookup in subtable, then by function code
		const auto& sub = dispatchTable()[pidx];
		int fidx = i.fnc & 0x7F;       // lower 7 bits
		handler h = sub.at(fidx);      // safe: sub is QVector<handler>
		if (h) (this->*h)(i);
	}
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
};

