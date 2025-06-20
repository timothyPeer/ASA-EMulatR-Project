#pragma once

#include <QException>
#include <QString>
#include <QByteArray>
#include <QScopedPointer>
#include <QtGlobal>
#include "JITFaultInfoStructures.h"    // For excTLBException enum
#include "../AEJ/helpers/helperSignExtend.h"
#include "../AEJ/enumerations/enumTLBException.h"
#include "../AEJ/constants/constExceptionConstants.h"
#include "../AEU/StackManager.h"

/**
 * @brief Exception class for TLB-related faults, Qt-compliant.
 * @sa Alpha AXP System Reference Manual Version 6 (1994),
 *     Section 5.4.1 'Stack Frame Layout' (p.5?3) 
 */
class TLBExceptionQ : public QException
{
public:
    /**
     * @brief Deep-copy constructor restores internal exception stack.
     * @details Allocates a fresh StackManager and deep-copies all
     *          StackFrame entries from the source via snapshot().
     * @sa StackManager::snapshot() and pushFrame() 
     */
    TLBExceptionQ(const TLBExceptionQ& other)
        : QException(other)
        , m_type(other.m_type)
        , m_virtualAddress(other.m_virtualAddress)
        , m_programCounter(other.m_programCounter)
        , m_msg(other.m_msg)
    {
        // Allocate a new stack and replay each frame
        m_excStack.reset(new StackManager(EXC_STACK_SIZE));
        auto frames = other.m_excStack->snapshot(); // deep copy under lock
        for (const auto& frame : frames) {
            m_excStack->pushFrame(frame);
        }
    }

    /**
     * @brief Primary constructor for fresh exceptions.
     */
    TLBExceptionQ(excTLBException type_,
        quint64 virtualAddress_,
        quint64 programCounter_)
        : m_type(type_)
        , m_virtualAddress(virtualAddress_)
        , m_programCounter(programCounter_)
    {
        m_excStack.reset(new StackManager(EXC_STACK_SIZE));
        buildMessage();
    }

    void raise() const override { throw* this; }
    TLBExceptionQ* clone() const override { return new TLBExceptionQ(*this); }

    QString message() const { return m_msg; }
    const char* what() const noexcept override {
        static thread_local QByteArray utf8;
        utf8 = m_msg.toUtf8();
        return utf8.constData();
    }

    excTLBException getType() const { return m_type; }
    quint64 getVirtualAddress() const { return m_virtualAddress; }
    quint64 getProgramCounter() const { return m_programCounter; }

    bool pushFrame(const ExceptionFrame& frame) {
        return m_excStack->pushFrame(frame) >= 0;
    }
    bool popFrame() {
        return m_excStack->popFrame();
    }
    int depth() const {
        return m_excStack->depth();
    }

private:
    void buildMessage()
    {
        m_msg = QString("TLB Exception: %1 at VA=0x%2 (PC=0x%3)")
            .arg(typeToString(m_type))
            .arg(QString::number(m_virtualAddress, 16))
            .arg(QString::number(m_programCounter, 16));
    }

    QString typeToString(excTLBException type) const
    {
        switch (type) {
        case excTLBException::INVALID_ENTRY:           return "Invalid Entry";
        case excTLBException::PROTECTION_FAULT:        return "Protection Fault";
        case excTLBException::ALIGNMENT_FAULT:         return "Alignment Fault";
        case excTLBException::PAGE_FAULT:              return "Page Fault";
        case excTLBException::ACCESS_VIOLATION:        return "Access Violation";
        case excTLBException::TRANSLATION_NOT_VALID:   return "Translation Not Valid";
        case excTLBException::PRIVILEGE_VIOLATION:      return "Privilege Violation";
        case excTLBException::PROTECTION_VIOLATION:     return "Protection Violation";
        case excTLBException::EXECUTE_PROTECTION_FAULT:return "Execute Protection Fault";
        case excTLBException::TLB_MISS:                return "TLB Miss";
        case excTLBException::INVALID_ADDRESS:         return "Invalid Address";
        case excTLBException::WRITE_PROTECTION_FAULT:  return "Write Protection Fault";
        case excTLBException::DOUBLE_FAULT:            return "Double Fault";
        case excTLBException::MACHINE_CHECK:           return "Machine Check";
        case excTLBException::NONE: default:           return "None";
        }
    }

private:
    excTLBException               m_type;
    QScopedPointer<StackManager>  m_excStack;       ///< dedicated stack for exception frames
    quint64                       m_virtualAddress;
    quint64                       m_programCounter;
    QString                       m_msg;
};
