#pragma once
#include <QtCore>
#include <QVector>
#include <QString>
#include <QMap>
#include "StackFrame.h"
#include "enumerations/enumExceptionType.h"
#include "StackFrame.h"
#include "AlphaProcessorStatus.h"
#include "AlphaProcessContext.h"

enum class ProcessorArchitecture
{
ARCHITECTURE_ALPHA,
ARCHITECTURE_TRU64,
ARCHITECTURE_VAX
};

enum class AlphaInterruptType {


};
enum class STATUS_BREAKPOINT
{
    bpt, // USER_BREAKPOINT,
    kbpt, //KERNEL_BREAKPOINT
    callkd // call kernel debugger
};

enum class VAXInterruptType
{

};
enum class VAXTrapType
{

};

class baseProcessorContext {

	AlphaProcessorStatus m_status;
	quint64 m_programCounter; 
	quint64 m_excbAddress;
        bool m_synchronousTrapsEnabled = true;

	public: 

		virtual bool areSynchronousTrapsEnabled() const = 0;
        virtual void deliverSynchronousTrap(AlphaTrapType type) = 0; 
		virtual void enableSynchronousTraps(bool enable) = 0;
		virtual void executeExceptionContinuation() = 0; 
		virtual ProcessorArchitecture getArchitecture() const = 0;
        virtual QString getArchitectureName() const = 0;
        virtual quint64 getFloatingRegister(int regNum) const = 0;
        virtual QString getContextString() const = 0;
        virtual QMap<QString, quint64> getContextValues() const = 0;
		virtual std::optional<StackFrame> getCurrentFrame() const = 0;
        virtual AlphaProcessorStatus *getProcessorStatus()  = 0; 
		virtual quint64 getExceptionContinuationAddress() const = 0;
        virtual quint64 getGeneralRegister(int regNum) const = 0;
        virtual quint32 getInstructionSize(quint64 pc) const = 0;
        virtual quint64 getNextInstructionPC() const =0;
        virtual quint64 getStackPointer() const = 0;
		virtual quint64 getProgramCounter() const  = 0;
        virtual QVector<StackFrame> getStackSnapshot() const = 0;
        
		virtual void handleAlignmentFault(quint64 faultingAddress) = 0;
		virtual void handleException(ExceptionType type, quint64 parameter = 0) = 0;
        virtual void handleInterrupt(AlphaInterruptType type, quint8 level) = 0;
        virtual void handleInterrupt(VAXInterruptType type, quint8 level) = 0;

        virtual void handleMachineCheck(quint64 errorInfo) = 0;
        virtual void handleTrap(AlphaTrapType type, quint64 faultingPC) = 0;
        virtual void handleTrap(VAXTrapType type, quint64 faultingPC) = 0;

		virtual bool hasExceptionContinuation() const = 0;
		
		virtual bool isAligned(quint64 address, quint32 alignment) const = 0;
        virtual bool isInstructionAligned(quint64 pc) const = 0;

        virtual bool isValidContext() const = 0;
		virtual bool isValidStackAddress(quint64 address) const = 0;
		virtual bool isValidPC(quint64 pc) const = 0;
        virtual bool popExceptionFrame() = 0;
		virtual bool pushExceptionFrame(ExceptionType type, quint64 parameter = 0) = 0;

		virtual bool restoreFullContext() = 0;
		virtual bool saveFullContext() = 0;
        virtual void setExceptionContinuationAddress(quint64 address) = 0;
        virtual void setFloatingRegister(int regNum, quint64 value) = 0;
        virtual void setGeneralRegister(int regNum, quint64 value) = 0;
		virtual void setProgramCounter(quint64 pc)  = 0;
        virtual void setStackPointer(quint64 sp) = 0;
        virtual bool switchContext(AlphaProcessorContext *newContext) = 0;


};