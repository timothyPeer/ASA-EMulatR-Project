#pragma once
#ifndef ALPHACPUSTATE_H
#define ALPHACPUSTATE_H

// ============================================================================
// AlphaCPUState.h
// ----------------------------------------------------------------------------
// Lightweight structure used to serialize or snapshot the state of AlphaCPU
// for purposes such as debugging, checkpointing, or restoring a trapped state.
// Used in conjunction with StackFrame and AlphaCoreContext.
//
// Reference: Alpha Architecture Reference Manual v6, Chapters 3–6
// ----------------------------------------------------------------------------

#include <QtGlobal>
#include <QVector>
#include <QBitArray>
#include <QDebug>

struct AlphaCPUState {
	// --------------------------------
	// Core Register State
	// --------------------------------
	quint64 pc = 0;                  // Program Counter - ASA I, 3-1
	quint64 fp = 0;                  // Frame Pointer - ASA I, 3-1
	qint32 psr = 0;                  // Processor Status Register - ASA I, 3-1
	quint64 usp = 0;                 // User Stack Pointer - ASA I, 5-1
	quint64 vptptr = 0;              // Virtual Page Table Pointer - ASA I, 5-1
	quint64 asn = 0;                 // Address Space Number - ASA I, 5-1
	quint64 unique = 0;              // Unique Register - ASA I, 6-5
	quint64 processorStatus = 0;     // Processor Status (PALmode) - ASA I, 5-1
	quint64 savedPsr = 0;			 // Saved Processor Status Register
	// --------------------------------
	// Floating-Point Control State
	// --------------------------------
	quint64 fpcr_raw = 0;			 // FPCR raw register contents - ASA I, 4-68

	// LDx_L/STx_C reservation state:
	bool    lockFlag = false;    ///< is there an active reservation?
	quint64 lockedPhysicalAddress = 0ULL;     ///< base of the 16-byte block reserved
	// --------------------------------
	// 
	// --------------------------------
	quint64 uniqueValue = 0;
	bool astEnable = false;
	bool palMode = false;
	bool lockFlag = false;
	quint64 lockedPhysicalAddress = 0;
	QVector<quint64> intRegs;  // R0–R30 (R31 is always zero)
	QVector<quint64> fpRegs;   // F0–F30 (F31 is always zero) — IEEE 754 double format

	// --------------------------------
	// Exception Summary & Trap Info
	// --------------------------------
	QBitArray excSum = QBitArray(64, false); // Exception Summary Register - ASA I, 4-66
	bool exceptionPending = false;           // Pending exception trap flag
	quint64 exceptionVector = 0;             // Trap vector address - ASA I, 6-4

	
	// --------------------------------
	// Saved Register Banks
	// --------------------------------


	// --------------------------------
	// Dump for Debugging
	// --------------------------------
	void dump() const {
		qDebug() << "[AlphaCPUState] PC:" << QString("0x%1").arg(pc, 8, 16)
			<< " PSR:" << QString("0x%1").arg(psr, 8, 16)
			<< " FP:" << QString("0x%1").arg(fp, 8, 16)
			<< " USP:" << QString("0x%1").arg(usp, 8, 16);
		qDebug() << " Unique:" << unique << " ASN:" << asn << " EXC Vector:" << exceptionVector;
		qDebug() << " GPRs:" << intRegs.size() << ", FPRs:" << fpRegs.size();
	}

	// --------------------------------
	// Integrity Checks
	// --------------------------------
	bool hasValidRegisterState() const {
		return intRegs.size() == 31 && fpRegs.size() == 31;
	}
	QJsonObject toJson() const {
		QJsonObject obj;
		obj["pc"] = QString::number(pc);
		obj["fp"] = QString::number(fp);
		obj["usp"] = QString::number(usp);
		obj["psr"] = psr;
		obj["savedPsr"] = QString::number(savedPsr);
		obj["fpcr_raw"] = QString::number(fpcr_raw);
		obj["asn"] = QString::number(asn);
		obj["uniqueValue"] = QString::number(uniqueValue);
		obj["processorStatus"] = QString::number(processorStatus);
		obj["vptptr"] = QString::number(vptptr);
		obj["astEnable"] = astEnable;
		obj["palMode"] = palMode;
		obj["exceptionPending"] = exceptionPending;
		obj["exceptionVector"] = QString::number(exceptionVector);
		obj["lockFlag"] = lockFlag;
		obj["lockedPhysicalAddress"] = QString::number(lockedPhysicalAddress);

		QJsonArray excArray;
		for (int i = 0; i < excSum.size(); ++i)
			excArray.append(excSum.testBit(i));
		obj["excSum"] = excArray;

		QJsonArray intArray;
		for (auto r : intRegs) intArray.append(QString::number(r));
		obj["intRegs"] = intArray;

		QJsonArray fpArray;
		for (auto f : fpRegs) fpArray.append(QString::number(f));
		obj["fpRegs"] = fpArray;

		return obj;
	}

	static AlphaCPUState fromJson(const QJsonObject& obj) {
		AlphaCPUState state;
		state.pc = obj["pc"].toString().toULongLong();
		state.fp = obj["fp"].toString().toULongLong();
		state.usp = obj["usp"].toString().toULongLong();
		state.psr = obj["psr"].toInt();
		state.savedPsr = obj["savedPsr"].toInt();
		state.fpcr_raw = obj["fpcr_raw"].toString().toULongLong();
		state.asn = obj["asn"].toString().toULongLong();
		state.uniqueValue = obj["uniqueValue"].toString().toULongLong();
		state.processorStatus = obj["processorStatus"].toString().toULongLong();
		state.vptptr = obj["vptptr"].toString().toULongLong();
		state.astEnable = obj["astEnable"].toBool();
		state.palMode = obj["palMode"].toBool();
		state.exceptionPending = obj["exceptionPending"].toBool();
		state.exceptionVector = obj["exceptionVector"].toString().toULongLong();
		state.lockFlag = obj["lockFlag"].toBool();
		state.lockedPhysicalAddress = obj["lockedPhysicalAddress"].toString().toULongLong();

		QJsonArray excArray = obj["excSum"].toArray();
		state.excSum.fill(false, excArray.size());
		for (int i = 0; i < excArray.size(); ++i)
			if (excArray[i].toBool()) state.excSum.setBit(i);

		QJsonArray intArray = obj["intRegs"].toArray();
		for (const auto& v : intArray)
			state.intRegs.append(v.toString().toULongLong());

		QJsonArray fpArray = obj["fpRegs"].toArray();
		for (const auto& v : fpArray)
			state.fpRegs.append(v.toString().toDouble());

		return state;
	}
};

#endif // ALPHACPUSTATE_H