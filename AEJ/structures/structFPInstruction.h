#pragma once
#include <QtCore>
#include <QSet>
#include "decodedInstruction.h"

struct FPInstruction
{
    DecodedInstruction instruction;
    quint64 pc;
    quint64 sequenceNumber;
    bool isReady = false;
    bool isCompleted = false;
    bool hasException = false;
    quint32 exceptionType = 0;

    // Dependency tracking
    QSet<quint8> srcRegisters;
    QSet<quint8> dstRegisters;
    bool touchesFPCR = false;

    FPInstruction() = default;
    FPInstruction(const DecodedInstruction &instr, quint64 programCounter, quint64 seqNum)
        : instruction(instr), pc(programCounter), sequenceNumber(seqNum)
    {
    }
};