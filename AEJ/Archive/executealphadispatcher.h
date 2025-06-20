#pragma once
#include <QObject>
#include <QSharedPointer>
#include "opcode18_executorAlphaMemoryBarrier.h"
#include "opcode14_executorAlphaSQRT.h"

class executorAlphaDispatcher : public QObject
{
    // Route decoded instructions to appropriate executors
    bool dispatchInstruction(const DecodedInstruction &instruction, quint64 pc)
    {
        quint32 opcode = (instruction.raw >> 26) & 0x3F;

        switch (opcode)
        {
        case 0x01:
            return m_opcode01Executor->submitInstruction(instruction, pc);
        case 0x11:
            return m_11_IntegerLogicalExecutor->submitInstruction(instruction pc);
        case 0x12:
            return m_12_AlphaIntegerLogical->submitInstruction(instruction pc);
        case 0x13:
            return m_integerLogicalExecutor->submitInstruction(instruction, pc);
        case 0x14:
            return m_executorAlphaSQRT->submitInstruction(instruction, pc);
        
            // ... other opcodes
        }
    }

  private:
    QSharedPointer<opcode11_executorAlphaIntegerLogical> m_11_IntegerLogicalExecutor;
    QSharedPointer<opcode12_executorAlphaIntegerLogical> m_12_AlphaIntegerLogical;
    QSharedPointer<opcode01_executorAlpha> m_opcode01Executor;
    QSharedPointer<opcode18_executorAlphaMemoryBarrier m_memoryBarrier;
    QSharedPointer<opcode14_executorAlphaSQRT> m_executorAlphaSQRT;
    // ... other executors
};
