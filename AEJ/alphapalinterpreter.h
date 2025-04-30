#pragma once

#include <QObject>
#include "..\AESH\Helpers.h"

class AlphaCPU;

class AlphaPALInterpreter : public QObject
{
    Q_OBJECT

public:
    explicit AlphaPALInterpreter(QObject* parent = nullptr);

    /**
     * Process a PAL instruction.
     * @param cpu - Pointer to the AlphaCPU instance
     * @param palFunction - PAL function code (26 bits)
     */
    void processPALInstruction(AlphaCPU* cpu, quint32 palFunction);

signals:
    void privilegedOperationFault(int cpuId, quint64 pc);

private:
    /**
     * Internal helper to dispatch known PAL functions.
     */
    void handleHalt(AlphaCPU* cpu);
    void handlePrivilegedContextSwitch(AlphaCPU* cpu);
    void handleSystemCall(AlphaCPU* cpu);
	void handleWriteKernelGP(AlphaCPU* cpu);
	void handleWriteUserSP(AlphaCPU* cpu);
	void handleReadUserSP(AlphaCPU* cpu);
	void handleMachineCheck(AlphaCPU* cpu);
	void handleBusError(AlphaCPU* cpu);
    /**
     * Handle unknown or unsupported PAL functions.
     */
    void handleUnknownPAL(AlphaCPU* cpu, quint32 palFunction);
};
