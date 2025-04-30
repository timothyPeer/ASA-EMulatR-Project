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

/**
 * @brief AlphaInstructionDecoder - Decodes Alpha instructions
 *
 * This class provides decoding functionality for Alpha instructions,
 * converting machine code to a structured representation.
 */
class AlphaInstructionDecoder : public QObject
{
    Q_OBJECT

public:
    explicit AlphaInstructionDecoder(QObject *parent = nullptr);
    ~AlphaInstructionDecoder();
    
    // Decode a single instruction
    AlphaInstruction decode(quint32 instructionWord);
    
    // Load instruction definitions from external sources
    bool loadInstructionDefinitions(const QString &definitionFile);
    void addCustomInstruction(quint32 opcode, quint32 functionCode, 
                             const QString &mnemonic, AlphaInstruction::InstructionFormat format,
                             const QVector<QString> &operands, const QString &description);

signals:
    void decodingError(quint32 instructionWord, const QString &errorMessage);

private:
    // Instruction definitions
    QMap<QString, AlphaInstruction> m_instructionMap; // Maps opcode+function to instruction template
    
    // Helper methods
    void initializeInstructionMap();
    QString getInstructionKey(quint32 opcode, quint32 functionCode);
    void decodeOperands(AlphaInstruction &instruction, quint32 instructionWord);
    
    // Format-specific decoders
    void decodeOperateOperands(AlphaInstruction &instruction, quint32 instructionWord);
    void decodeBranchOperands(AlphaInstruction &instruction, quint32 instructionWord);
    void decodeMemoryOperands(AlphaInstruction &instruction, quint32 instructionWord);
    void decodePALOperands(AlphaInstruction &instruction, quint32 instructionWord);
};

#endif // ALPHAINSTRUCTIONDECODER_H