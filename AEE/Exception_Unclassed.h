#pragma once

// Exception.h
#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QException>
#include <QString>
#include "../AEU/StackManager.h"
#include "../AEU/StackFrame.h"
#include "trapTrapType.h"

/// @brief Represents an in-emulator exception/interrupt context
/*  Per the Alpha Architecture Reference Manual, sections 
*   '6.7.3 Stack Alignment' and '6.7.2 Stack Residency,' all 
*   frames must be 64-byte aligned and properly set up for CALL_PAL/REI Alpha_AXP_System_Reference. 
*   The StackManager should ensure that alignment internally (e.g. rounding SP to 64-byte boundary on each push).
*/
class Exception : public QException {

public:
	explicit Exception(TrapType type, quint64 pc)
		: m_type(type), m_pc(pc)
	{
		constexpr int EXC_STACK_SIZE = 4 * 1024;  // small stack for exception frames
		m_excStack.reset(new StackManager(EXC_STACK_SIZE));
	}

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

private:
	QScopedPointer<StackManager> m_excStack;  ///< dedicated stack for exception frames
	TrapType m_type;
	quint64 m_pc;
	QString m_msg;
};
