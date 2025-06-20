// AlphaCPUFactory.h
#pragma once
#include "AlphaCPU.h"

/**
 * @brief Factory method to create and initialize an AlphaCPU instance.
 *
 * @param id      Logical CPU ID
 * @param memory  Shared pointer to SafeMemory
 * @param bus     Pointer to SystemBus
 * @param irq     Pointer to IRQController
 * @param parent  Optional QObject parent
 * @return        Fully constructed AlphaCPU pointer
 */
inline AlphaCPU* createCore(qint32 id, SafeMemory* memory, SystemBus* bus, IRQController* irq, QObject* parent = nullptr)
{
    AlphaCPU* cpu = new AlphaCPU(id, memory, bus, irq, parent);

    // Additional post-construction configuration if needed
    // For example: enable tracing, attach CLI monitor, set breakpoints
    qDebug() << "[Factory] Created AlphaCPU core ID:" << id;

    return cpu;
}

/* 
Usage Example:

SafeMemory* memory = new SafeMemory(...);
SystemBus* bus = new SystemBus();
IRQController* irq = new IRQController();

AlphaCPU* cpu0 = createCore(0, memory, bus, irq);
AlphaCPU* cpu1 = createCore(1, memory, bus, irq);

// Register with EmulatorManager or CPU pool
emulator->addCPU(cpu0);

*/