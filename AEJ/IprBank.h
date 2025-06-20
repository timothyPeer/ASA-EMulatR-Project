#pragma once
/**
 * @file   IprBank.h
 * @brief  Thread safe bank of Alpha AXP Internal Processor Registers (IPRs).
 *
 * (c) 2025  Timothy Peer & contributors – MIT License
 *
 * The class covers the full PAL visible IPR set typically required by
 * an ES40 class system.  Add or remove enum entries as your target variant
 * (EV5/6/7, 21164, 21264, Titan, etc.) demands.
 */

#include "JITFaultInfoStructures.h"
#include <QObject>
#include <QReadWriteLock>
#include <QVector>
#include <QtGlobal>
#include "../AEJ/constants/const_OpCode_0_PAL.h"
#include "../AEJ/enumerations/enumIPRNumbers.h"
#include "GlobalMacro.h"  

class AlphaCPU;

class IprBank final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(IprBank)

  public:
    /* ========= Architectural enumeration ========= */
   

    /* ========= Ctor ========= */
    explicit IprBank(QObject *parent = nullptr)
        : QObject(parent), regs_(static_cast<int>(IPRNumbers::IPR_COUNT), 0), // Use IPR_COUNT for sizing
          m_cpu(nullptr)
    {
        initializeDefaults();
    }

    /* ========= Public API ========= */

    /// Read an IPR (inline for speed).
    quint64 read(IPRNumbers id) const
    {
        QReadLocker l(&lock_);
        return regs_.at(static_cast<int>(id));
    }

    /// Write an IPR; emits registerChanged if the value differs.
    void write(IPRNumbers id, quint64 value);

    /// Shorthand for PAL generic window  IPR[n]
    /// Accepts 
    Q_INVOKABLE quint64 readIpr(quint8 n) const;
    Q_INVOKABLE void writeIpr(quint8 n, quint64 v);

    /// Reset all IPRs to zero (useful on power?up / warm reset).
    Q_INVOKABLE void clear();
    /// Set the CPU reference
    void setCpu(AlphaCPU *cpu_);

	QString getRegisterName(IPRNumbers id);
    void initializeDefaults();
    /// Get register name for debugging

 
    
  signals:
    /// Emitted after a successful write() that changed the stored value.
    void sigRegisterChanged(IPRNumbers id, quint64 newValue);

  private:
    /* ========= State ========= */
    mutable QReadWriteLock lock_;
    QVector<quint64> regs_;
    // quint64                 Ipr0;
    AlphaCPU *m_cpu; // Back-reference to owning CPU
    /// Handle special write behaviors that don't store the value
	bool handleSpecialWrite(IPRNumbers id, quint64 value);
    /// Handle post-write notifications
    void handlePostWrite(IPRNumbers  id, quint64 value);

};

/* ================== Usage snippet ==================
IprBank iprs;
iprs.write(IprBank::ASN, 0x1234);
auto asn = iprs.read(IprBank::ASN);
connect(&iprs, &IprBank::registerChanged,
        [](IprBank::Ipr id, quint64 v){ qDebug() << id << '?' << v; });
========================================================= */
