#pragma once
#ifndef STACKFRAME_H
#define STACKFRAME_H

// ============================================================================
// StackFrame.h
// ----------------------------------------------------------------------------
// Represents a saved CPU context frame used during trap handling or context
// switches in the Alpha AXP architecture. This is used by AlphaCPU and
// AlphaCoreContext to preserve execution state.
//
// Reference: Alpha Architecture Reference Manual v6, Chapter 6 (Traps)
// ----------------------------------------------------------------------------

#include <QtGlobal>
#include <QVector>
#include <QDebug>

class StackFrame {
public:
	// -------------------------------
	// Public members
	// -------------------------------

	quint64 pc = 0;               // Saved PC (return address) - ASA I, 6-4
	quint64 framePointer = 0;     // Saved FP - ASA I, 6-4
	quint32 psr = 0;              // Saved PSR (Processor Status Register) - ASA I, 6-4
	quint64 returnAddress = 0;    // Address to return to after trap - ASA I, 6-4

	QVector<quint64> savedGPRs;   // General-purpose register state (R0–R30) - ASA I, 3-1
	QVector<quint64> savedFPRs;   // Floating-point register state (F0–F30) - ASA I, 4-66
	quint64 usp = 0;             // User Stack Pointer (for mode switches)
	quint64 asn = 0;             // Address Space Number (MMU context)
	quint64 vptptr = 0;          // Virtual Page Table Pointer
	quint64 uniqueValue = 0;     // Unique value for AST/deliverability context
	bool astEnable = false;      // AST delivery enabled
	quint64 processorStatus = 0; // Optional extended processor state

	// -------------------------------
	// Constructors
	// -------------------------------

	StackFrame() = default;

	StackFrame(quint64 _pc, quint64 _fp, quint32 _psr, quint64 _ret)
		: pc(_pc), framePointer(_fp), psr(_psr), returnAddress(_ret) {
	}
	StackFrame(quint64 _pc, quint64 _fp, quint32 _psr, quint64 _ret,
		quint64 _usp, quint64 _asn, quint64 _vpt, quint64 _unique, bool _ast)
		: pc(_pc), framePointer(_fp), psr(_psr), returnAddress(_ret),
		usp(_usp), asn(_asn), vptptr(_vpt), uniqueValue(_unique), astEnable(_ast) {
	}

	// -------------------------------
	// Validation Helpers
	// -------------------------------

	bool isGPRValid() const { return savedGPRs.size() == 31; }
	bool isFPRValid() const { return savedFPRs.size() == 31; }

	// -------------------------------
	// Debug Dump
	// -------------------------------

	void dump() const {
		qDebug() << " USP =" << QString("0x%1").arg(usp, 8, 16)
			<< " ASN =" << asn
			<< " AST =" << astEnable
			<< " VPT =" << QString("0x%1").arg(vptptr, 8, 16)
			<< " Unique =" << uniqueValue;
		qDebug() << " GPR Count:" << savedGPRs.size() << ", FPR Count:" << savedFPRs.size();
	}
};

#endif // STACKFRAME_H

