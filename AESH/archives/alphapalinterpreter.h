#pragma once

#include <QObject>
#include "..\AESH\Helpers.h"
#include "IExecutionContext.h"
#include "../AESH/GlobalMacro.h"
#include "tlbSystem.h"

class AlphaCPU;




class AlphaPALInterpreter : public QObject
{
    Q_OBJECT

public:
    explicit AlphaPALInterpreter(IExecutionContext* context, QObject *parent = nullptr);

    void attachAlphaCPU(AlphaCPU* cpu_) { m_cpu = cpu_; }
    void attachTLBSystem(TLBSystem* tlb_) { m_tlbSystem = tlb_; }
    /**
     * Process a PAL instruction.
     * @param cpu - Pointer to the AlphaCPU instance
     * @param palFunction - PAL function code (26 bits)
     */
    void processPALInstruction(quint64 palFunction);
    void raiseException(ExceptionType, quint64 pc_);
signals:
    void privilegedOperationFault(int cpuId, quint64 pc);

private:
    /**
     * Internal helper to dispatch known PAL functions.
     */
    void handleHalt();
    void handlePrivilegedContextSwitch();
    void handleSystemCall();
	void handleWriteKernelGP();
	void handleWriteUserSP();
	void handleReadUserSP();
	void handleMachineCheck();
	void handleBusError();
	void handleWRKGP(quint64 value)
	{
		ctx->writeIntReg(KERNEL_GP_INDEX, value);
		ctx->notifyRegisterUpdated(false, KERNEL_GP_INDEX, value);
	}

    /**
     * Handle unknown or unsupported PAL functions.
     */
    void handleUnknownPAL(quint32 palFunction);
    IExecutionContext* ctx;
    AlphaCPU* m_cpu;
    TLBSystem* m_tlbSystem;

};
