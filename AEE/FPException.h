#pragma once
#include <QException>
#include <QString>
#include <QByteArray>
#include <QtGlobal>
#include <QScopedPointer>
#include "..\AEJ\ASAnameSpaces.h"
#include "..\AEJ\traps\trapFpType.h"
#include "..\AEU\StackManager.h"
#include "..\AEJ\constants\constExceptionConstants.h"
#include "..\AEU\StackFrame.h"



/**
 * @brief Exception class for floating-point exceptions, Qt-compliant.
 */
class FPException : public QException
{
public:
	
    // Constructor
    FPException(FPTrapType type, quint64 pc)
        : m_type(type), m_pc(pc)
    {
        m_excStack.reset(new StackManager(EXC_STACK_SIZE));
        buildMessage();
    }
    
	FPException(const FPException& other)
		: QException(other)
		, m_type(other.m_type)
		, m_pc(other.m_pc)
	{
		// Allocate a fresh StackManager of the same size...
		m_excStack.reset(new StackManager(EXC_STACK_SIZE));
		// ...and copy every frame from the other exception’s stack
		auto frames = other.m_excStack->snapshot();
		for (const auto& frame : frames) {
			m_excStack->pushFrame(frame.hwFrame);
		}
		// Preserve the original message
		m_msg = other.m_msg;
	}
    FPException* clone() const override { return new FPException(*this); }
    /**
     * @brief Push a new exception frame onto our internal StackManager.
     *
     * Internally calls StackManager::push(), which appends a copy of the
     * ExceptionFrame (hardware trap frame) under lock and returns –1 on overflow.
     * @param frame  The ExceptionFrame to push (PC, PS, GPR, FPCR, etc.)
     * @return       true if the frame was pushed successfully; false on overflow.
     * @see StackManager::push(const ExceptionFrame&) :contentReference[oaicite:0]{index=0}
     */
    bool pushFrame(const ExceptionFrame& frame) {
        return m_excStack->pushFrame(frame) >= 0;
    }

    /**
     * @brief Pop the most-recent exception frame.
     *
     * Internally calls StackManager::pop(), which removes the last frame
     * under write-lock and returns false if the stack was already empty.
     * @return  true if a frame was popped; false on underflow.
     * @see StackManager::pop() :contentReference[oaicite:1]{index=1}
     */
    bool popFrame() {
        return m_excStack->popFrame();
    }

    /**
     * @brief How many exception frames are currently on the stack?
     *
     * Internally calls StackManager::depth(), which returns the current
     * QVector size under read-lock.
     * @return  Number of frames in the stack.
     * @see StackManager::depth() :contentReference[oaicite:2]{index=2}
     */
    int depth() const {
        return m_excStack->depth();
    }

    // Qt-style clone method
    void raise() const override { throw* this; }
 

    // Qt-style message accessor
    QString message() const { return m_msg; }

    // C-string message for std::exception compatibility
    const char* what() const noexcept override {
        static thread_local QByteArray utf8Data;
        utf8Data = m_msg.toUtf8();
        return utf8Data.constData();
    }


	
    // Getter for trap type
    FPTrapType getTrapType() const { return m_type; }

    // Getter for program counter
    quint64 getPC() const { return m_pc; }

private:
    // Helper function to build error message
    void buildMessage() {
        m_msg = "Floating-point exception: ";

        switch (m_type) {
        case FPTrapType::FP_DIVISION_BY_ZERO:
            m_msg += "division by zero";
            break;
        case FPTrapType::FP_OVERFLOW:
            m_msg += "overflow";
            break;
        case FPTrapType::FP_UNDERFLOW:
            m_msg += "underflow";
            break;
        case FPTrapType::FP_INEXACT:
            m_msg += "inexact result";
            break;
        case FPTrapType::FP_INVALID_OPERATION:
            m_msg += "invalid operation";
            break;
        default:
            m_msg += "unknown";
            break;
        }

        m_msg += QString(" at PC=0x%1").arg(m_pc, 0, 16);
    }

private:
    FPTrapType m_type;
    QScopedPointer<StackManager> m_excStack;  ///< dedicated stack for exception frames
    quint64 m_pc;
    QString m_msg;
};
