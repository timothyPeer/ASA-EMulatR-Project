
#pragma once
#include <QObject>
#include <QString>
#include "GlobalMacro.h"
#include "decodeOperate.h"

class AlphaCPU;

class DecodeStage : public QObject
{
    Q_OBJECT

  public:
    explicit DecodeStage(AlphaCPU *cpu);
    void attachAlphaCPU(AlphaCPU *cpu_) { m_cpu = cpu_;  }
    void clearStatistics();

     
    double getMemoryInstructionRate() const;

     // Instruction analysis
    enum InstructionType
    {
        INVALID = 0,
        MEMORY,
        BRANCH,
        OPERATE,
        PAL,
        JUMP,
        UNKNOWN
    };

    InstructionType getInstructionType(quint32 opcode) const;
    QString getInstructionMnemonic(const DecodedInstruction &instruction) const;

    /**
     * @brief Flush decode stage
     */
    void flush();
    void printStatistics() const;
    void updateStatistics(InstructionType type);

  signals:
    void instructionDecoded(const DecodedInstruction &instruction);
    void decodeError(DecodedInstruction instruction, QString reason);

  private:
    AlphaCPU *m_cpu;
    DecodedInstruction m_currentInstruction;

    mutable QMutex m_statsMutex;
    quint64 m_totalInstructions = 0;
    quint64 m_memoryInstructions = 0;
    quint64 m_branchInstructions = 0;
    quint64 m_operateInstructions = 0;
    quint64 m_palInstructions = 0;
    quint64 m_jumpInstructions = 0;
    quint64 m_unknownInstructions = 0;
};