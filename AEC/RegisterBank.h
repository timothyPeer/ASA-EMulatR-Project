#pragma once
#ifndef REGISTER_BANKS_H
#define REGISTER_BANKS_H
#include "AlphaVax_DataTypes.h" // Updated: Replaces deprecated dt_gfloat
#include <QObject>
#include <QVector>
#include <QBitArray>
#include <QDebug>
#include <QMap>
#include <array>

#include "FpcrRegister.h" // Defines FPCR flag bits and trap modes
#include "FpRegisterBankcls.h"

/**
 * @brief Integer register bank for Alpha AXP architecture, with FPCR support.
 * 
 * ASA References:
   - §4.3.3 Trap and Interrupt Stack Format (PC, FP, PSR, FPCR, GPRs, FPRs)
   - §3.3 / §5.1 Integer and Floating-Point Registers
   - §6.6.2 REI restores full context
 */
class RegisterBank : public QObject {
    Q_OBJECT

public:
    static constexpr int NUM_REGS = 32;
    static constexpr int NUM_ALT_FPCR = 8;

    std::array<quint64, NUM_REGS> intRegs{};
    std::array<quint64, NUM_REGS> floatRegs{};
    std::array<std::array<quint64, 2>, NUM_REGS> vectorRegs{};
    std::array<quint64, NUM_ALT_FPCR> altFPCRs{};
    FpcrRegister fpcr;

    bool intDirty = false;
    bool floatDirty = false;
    bool vectorDirty = false;
    bool fpcrDirty = false;

    RegisterBank(QObject* parent = nullptr) : QObject(parent) {
        buildRegisterNameMap();
    }

    // === Access Helpers for Integer Registers ===

	QVector<quint64> dump() const {
		QVector<quint64> regs;
		for (int i = 0; i < 31; ++i)  // R31 always reads as zero, skip saving
			regs.append(readIntReg(i));
		return regs;
	}
	void load(const QVector<quint64>& values) {
		for (int i = 0; i < qMin(values.size(), 31); ++i)
			writeIntReg(i, values[i]);
	}


    inline quint64 readIntReg(quint8 index) const {
        return (index < NUM_REGS) ? intRegs[index] : 0;
    }

    inline void writeIntReg(quint8 index, quint64 value) {
        if (index < NUM_REGS && index != 31) {
            intRegs[index] = value;
            intDirty = true;
        }
    }

    void clear() {
        regNameMap.clear();
    }
    // Common aliases as per ASA (register usage convention)
    inline quint64 readSP() const { return readIntReg(30); } // R30 = Stack Pointer
    inline void writeSP(quint64 value) { writeIntReg(30, value); }
    inline quint64 readRA() const { return readIntReg(26); } // R26 = Return Address
    inline void writeRA(quint64 value) { writeIntReg(26, value); }

    inline quint64 readA(int index) const { return readIntReg(16 + index); } // a0-a5
    inline void writeA(int index, quint64 value) { writeIntReg(16 + index, value); }

    inline quint64 readT(int index) const { return readIntReg(1 + index); }  // t0-t7
    inline void writeT(int index, quint64 value) { writeIntReg(1 + index, value); }

    QString regName(int index) const {
        return regNameMap.value(index, QString("$r%1").arg(index));
    }

    // === Floating-Point Register Access ===
     quint64 readFloatReg(quint8 index) const {
        return (index < NUM_REGS) ? floatRegs[index] : 0;
    }

     void writeFloatReg(quint8 index, quint64 value) {
        if (index < NUM_REGS && index != 31) {
            floatRegs[index] = value;
            floatDirty = true;
        }
    }

    // === Vector Register Access ===
    inline quint64 readVectorReg(quint8 index, quint8 lane) const {
        return (index < NUM_REGS && lane < 2) ? vectorRegs[index][lane] : 0;
    }

    inline void writeVectorReg(quint8 index, quint8 lane, quint64 value) {
        if (index < NUM_REGS && lane < 2) {
            vectorRegs[index][lane] = value;
            vectorDirty = true;
        }
    }

    // === FPCR Access and Control ===
    inline quint64 getFPCR() const { return fpcr.getRaw(); }
    inline void setFPCR(quint64 value) { fpcr.setRaw(value); fpcrDirty = true; }

    inline void setFPCRFlag(FpcrRegister::FlagBit flag, bool enable) {
        fpcr.setFlag(flag, enable);
        fpcrDirty = true;
    }

    inline void setFPCRTrap(FpcrRegister::FlagBit trap, bool enable) {
        fpcr.setTrapEnabled(trap, enable);
        fpcrDirty = true;
    }

    inline bool hasFPCRFlag(FpcrRegister::FlagBit flag) const {
        return fpcr.hasFlag(flag);
    }

    inline bool isFPCRTrapEnabled(FpcrRegister::FlagBit trap) const {
        return fpcr.isTrapEnabled(trap);
    }

    inline FpcrRegister::RoundingMode roundingMode() const {
        return fpcr.roundingMode();
    }

    void clearAllDirtyFlags() {
        intDirty = floatDirty = vectorDirty = fpcrDirty = false;
    }

private:
    QMap<int, QString> regNameMap;

    void buildRegisterNameMap() {
        for (int i = 0; i < NUM_REGS; ++i) {
            regNameMap[i] = QString("$r%1").arg(i);
        }
        regNameMap[26] = "$ra";  // Return address
        regNameMap[30] = "$sp";  // Stack pointer
        for (int i = 0; i <= 5; ++i) regNameMap[16 + i] = QString("$a%1").arg(i);
        for (int i = 0; i <= 7; ++i) regNameMap[1 + i] = QString("$t%1").arg(i);
    }
};

/**
 * @brief Floating-point register bank using G_Float for internal format.
 */
#endif // REGISTER_BANKS_H
