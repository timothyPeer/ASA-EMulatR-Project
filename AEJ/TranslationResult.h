#pragma once
#include <QObject>
#include "../AEE/TLBExceptionQ.h"
#include "../AEJ/helpers/helperSignExtend.h"
#include "../AEJ/enumerations/enumTLBException.h"


class TranslationResult
{
  public:
    TranslationResult() = default;
    TranslationResult(quint64 pAddr, excTLBException ex, bool isHit)
        : m_physicalAddress(pAddr), m_tlbException(ex), m_hit(isHit)
    {
    }

    void setFault(bool bIsFault) { m_bFault = bIsFault; }
    void setFaultReason(QString faultReason_) { m_faultReason = faultReason_; }
    bool isValid() const { return m_tlbException == excTLBException::NONE; }
    bool setValid(bool bValid)
    {
        if (bValid)
        {
            m_tlbException = excTLBException::NONE;
        }
        return bValid;
    }
    bool isHit() const { return m_hit; }
    void setHit(bool bHit) { m_hit = bHit; }
    void setTLBException(excTLBException exc_type) { m_tlbException = exc_type; }
    quint64 physicalAddress() const { return m_physicalAddress; }
    void setPhysicalAddress(quint64 pa_) { m_physicalAddress = pa_; }
    excTLBException tlbException() const { return m_tlbException; }
    excTLBException getTLBException() const { return m_tlbException; }
    quint64 getPhysicalAddress() const { return m_physicalAddress; }
    QString getFaultReason() const { return m_faultReason; }
    void setExecutable(bool bExecuteable) { m_executable = bExecuteable; }

    // Add missing getException() method
    excTLBException getException() const { return m_tlbException; }

    // Factory methods that were missing
    static TranslationResult makeFault(excTLBException ex) { return TranslationResult(0, ex, false); }

    static TranslationResult makeHit(quint64 physAddr)
    {
        return TranslationResult(physAddr, excTLBException::NONE, true);
    }

    // Additional factory methods used in AlphaMemorySystem
    static TranslationResult createFault(excTLBException ex) { return TranslationResult(0, ex, false); }

    static TranslationResult createSuccess(quint64 physAddr)
    {
        return TranslationResult(physAddr, excTLBException::NONE, true);
    }

  private:
    quint64 m_physicalAddress = 0;
    excTLBException m_tlbException = excTLBException::NONE;

    bool m_hit = false;
    bool m_executable = false;
    bool m_bFault = false;
    QString m_faultReason;
};

// class TranslationResult
// {
//   public:
//     TranslationResult() = default;
//     TranslationResult(quint64 pAddr, AsaExceptions::excTLBException ex, bool isHit)
//         : m_physicalAddress(pAddr), m_tlbException(ex), m_hit(isHit)
//     {
//     }
// 
//     void setFault(bool bIsFault) { m_bFault = bIsFault;  }
//     void setFaultReason(QString faultReason_) { m_faultReason = m_faultReason;  }
//     bool isValid() { return m_tlbException == AsaExceptions::excTLBException::NONE; }
//     bool setValid(bool bValid) 
//     {
//         bValid = true;
//         m_tlbException = AsaExceptions::excTLBException::NONE;
//     }
//     bool isHit() const { return m_hit; }
//     void setHit(bool bHit) { m_hit = bHit; }
//     void setTLBException(AsaExceptions::excTLBException exc_type) { m_tlbException = exc_type; }
//     quint64 physicalAddress() const { return m_physicalAddress; }
//     void setPhysicalAddress(quint64 pa_) { m_physicalAddress = pa_;  }
//     AsaExceptions::excTLBException tlbException() const { return m_tlbException; }
//     quint64 getPhysicalAddress() { return m_physicalAddress;  }
//     QString getFaultReason() { return m_faultReason;  }
//     void setExecutable(bool bExecuteable) { m_executable = bExecuteable; }
//     
//     static TranslationResult makeFault(AsaExceptions::excTLBException ex) { return TranslationResult(0, ex, false); }
// 
//     static TranslationResult makeHit(quint64 physAddr)
//     {
//         return TranslationResult(physAddr, AsaExceptions::excTLBException::NONE, true);
//     }
// 
//   private:
//     quint64 m_physicalAddress = 0;
//     AsaExceptions::excTLBException m_tlbException = AsaExceptions::excTLBException::NONE;
//     
//     bool m_hit = false;
//     bool m_executable = false;
//     bool m_bFault = false;
//     QString m_faultReason;
// };
