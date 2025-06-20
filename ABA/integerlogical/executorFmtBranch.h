// executorFmtBranch.h
#pragma once
#include <QtCore/QVector>
#include "Assembler.h"
#include "../structs/BranchInstruction.h"

using Assembler = assemblerSpace::Assembler;
using Condition = assemblerSpace::Condition;
using FPCondition = assemblerSpace::FPCondition;

class executorFmtBranch {
public:
	using Handler = void(executorFmtBranch::*)(const BranchInstruction&, uint64_t pc);
	executorFmtBranch(Assembler& a) : as(a) {}

	// Top-level decode + dispatch
	uint64_t execute(const BranchInstruction& inst, uint64_t pc) {
		auto i = inst;
		i.decode();
		int idx = i.opcode & 0x3F;
		Handler h = dispatchTable().at(idx);
		if (!h) return pc + 4;                // unimplemented ? fall-through
		return (this->*h)(i, pc), 0;          // handlers must update PC via as.emitJmp/Jcc
	}

private:
	Assembler& as;

	static QVector<Handler> createDispatchTable() {
		QVector<Handler> t(64, nullptr);
		// Conditional branches (fnc = raw<5:0>):
		t[0x38] = &executorFmtBranch::emitBLBC;  // BLBC  if (Ra & 1)==0
		t[0x39] = &executorFmtBranch::emitBEQ;   // BEQ   if Ra==0
		t[0x3A] = &executorFmtBranch::emitBLT;   // BLT   if Ra<0
		t[0x3B] = &executorFmtBranch::emitBNE;   // BNE   if Ra!=0
		t[0x3C] = &executorFmtBranch::emitBLBS;  // BLBS  if (Ra & 1)==1
		t[0x3C] = &executorFmtBranch::emitBLE;   // BLE   if Ra<=0
		t[0x3E] = &executorFmtBranch::emitBGE;   // BGE   if Ra>=0
		// Unconditional
		t[0x2D] = &executorFmtBranch::emitBR;    // BR    always
		t[0x30] = &executorFmtBranch::emitBSR;   // BSR   subroutine
		// Floating-point branches (fnc=bitpattern):
		t[0x31] = &executorFmtBranch::emitFBEQ;  // FBEQ  if Fp = 0
		t[0x32] = &executorFmtBranch::emitFBLT;  // FBLT  if Fp <  0
		t[0x33] = &executorFmtBranch::emitFBLE;  // FBLE  if Fp ? 0
		t[0x35] = &executorFmtBranch::emitFBNE;  // FBNE  if Fp ? 0
		t[0x36] = &executorFmtBranch::emitFBGE;  // FBGE  if Fp ? 0
		t[0x37] = &executorFmtBranch::emitFBGT;  // FBGT  if Fp >  0
		return t;
	}

	static const QVector<Handler>& dispatchTable() {
		static const QVector<Handler> tbl = createDispatchTable();
		return tbl;
	}

	// ————————————————————————————————
	// Handler prototypes:
	// Each emits host code with as.emitTest()/emitJcc()/emitJmp()
	// and must bind the BranchInstruction’s target.
	// Returns nothing—JIT driver will gen PC from the emitted code.
	// ————————————————————————————————

	void emitBEQ(const BranchInstruction& i, uint64_t pc) {
		as.testq(i.ra, i.ra);
		as.emitJcc(Condition::EQ, i.computeTarget(pc));
	}
	void emitBNE(const BranchInstruction& i, uint64_t pc) {
		as.testq(i.ra, i.ra);
		as.emitJcc(Condition::NE, i.computeTarget(pc));
	}
	void emitBGE(const BranchInstruction& i, uint64_t pc) {
		as.cmpq(i.ra, 0);
		as.emitJcc(Condition::GE, i.computeTarget(pc));
	}
	void emitBLT(const BranchInstruction& i, uint64_t pc) {
		as.cmpq(i.ra, 0);
		as.emitJcc(Condition::L, i.computeTarget(pc));
	}
	void emitBLE(const BranchInstruction& i, uint64_t pc) {
		as.cmpq(i.ra, 0);
		as.emitJcc(Condition::LE, i.computeTarget(pc));
	}
	void emitBLBC(const BranchInstruction& i, uint64_t pc) {
		as.testq(i.ra, 1);
		as.emitJcc(Condition::EQ, i.computeTarget(pc));
	}
	void emitBLBS(const BranchInstruction& i, uint64_t pc) {
		as.testq(i.ra, 1);
		as.emitJcc(Condition::NE, i.computeTarget(pc));
	}
	void emitBR(const BranchInstruction& i, uint64_t pc) {
		as.emitJmp(i.computeTarget(pc));
	}
	void emitBSR(const BranchInstruction& i, uint64_t pc) {
		// save return addr in R31, then branch
		as.movImm64(/*R31*/, pc + 4);
		as.emitJmp(i.computeTarget(pc));
	}

	// Floating?point conditional branches use FPSCR flags:
	void emitFBGE(const BranchInstruction& i, uint64_t pc) {
		as.emitFpJcc(FPCondition::GE, i.computeTarget(pc));
	}
	void emitFBGT(const BranchInstruction& i, uint64_t pc) {
		as.emitFpJcc(FPCondition::G, i.computeTarget(pc));
	}
	void emitFBLE(const BranchInstruction& i, uint64_t pc) {
		as.emitFpJcc(FPCondition::LE, i.computeTarget(pc));
	}
	void emitFBLT(const BranchInstruction& i, uint64_t pc) {
		as.emitFpJcc(FPCondition::L, i.computeTarget(pc));
	}
	void emitFBNE(const BranchInstruction& i, uint64_t pc) {
		as.emitFpJcc(FPCondition::NE, i.computeTarget(pc));
	}
	void emitFBEQ(const BranchInstruction& i, uint64_t pc) {
		as.emitFpJcc(FPCondition::E, i.computeTarget(pc));
	}
};
