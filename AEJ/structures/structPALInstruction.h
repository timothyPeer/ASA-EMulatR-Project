#pragma once
#pragma once
#include "..\AEJ\constants\const_OpCode_0_PAL.h"
#include <QFuture>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QVector>
#include <QWaitCondition>
#include <QtConcurrent>
#include <atomic>
#include "../decodedInstruction.h"

// Forward declarations
class AlphaCPU;
class UnifiedDataCache;
class AlphaInstructionCache;
class AlphaTranslationCache;
class AlphaBarrierExecutor;
class executorAlphaFloatingPoint;
class opcode11_executorAlphaIntegerLogical;


// PAL Instruction structure for pipeline processing
struct PALInstruction
{
    DecodedInstruction instruction;
    quint64 pc;
    quint64 sequenceNumber;
    quint32 function; // PAL function code

    // Execution state
    bool isReady = false;
    bool isCompleted = false;
    bool hasException = false;
    quint64 exceptionType = 0;

    // Result handling
    bool writeResult = false;
    quint8 targetRegister = 31;
    quint64 result = 0;

    // Instruction properties (set during analysis)
    bool requiresKernelMode = true;
    bool modifiesSystemState = false;
    bool invalidatesTLB = false;
    bool flushesCache = false;
    bool requiresBarrier = false;
    bool touchesIPR = false;

    // Dependency tracking
    QSet<quint8> srcRegisters;
    QSet<quint8> dstRegisters;

    // Constructor
    PALInstruction() = default;

    PALInstruction(const DecodedInstruction &instr, quint64 programCounter, quint64 seqNum)
        : instruction(instr), pc(programCounter), sequenceNumber(seqNum)
    {
        // Extract PAL function from instruction
        function = (instr.raw >> 5) & 0x3FFFFFF; // Bits 30:5 contain function
    }
};


