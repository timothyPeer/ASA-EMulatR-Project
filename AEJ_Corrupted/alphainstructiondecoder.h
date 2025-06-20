#pragma once

// AlphaInstructionDecoder.h - Instruction decoder header
#ifndef ALPHAINSTRUCTIONDECODER_H
#define ALPHAINSTRUCTIONDECODER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>

/**
 * @brief AlphaInstruction - Represents a decoded Alpha instruction
 */
class AlphaInstruction {
public:
    enum InstructionFormat {
        OPERATE,
        BRANCH,
        MEMORY,
        SYSTEM,
        VECTOR,
        MEMORY_BARRIER
    };

    AlphaInstruction() : opcode(0), functionCode(0), format(OPERATE) {}
    
    quint32 opcode;             // Main opcode
    quint32 functionCode;       // Function code (for operate format)
    QString mnemonic;           // Instruction mnemonic
    InstructionFormat format;   // Instruction format
    QVector<QString> operands;  // Operand types
    QString description;        // Instruction description
    QMap<QString, quint32> decodedOperands; // Decoded operand values
    
    QString toString() const;
};

