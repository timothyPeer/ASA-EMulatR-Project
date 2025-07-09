#ifndef BR_BGT_INSTRUCTIONGRAIN_H
#define BR_BGT_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "grainDependencies.h"
#include "InstructionProfile.h"
#include <QString>
#include <QPair>

/** *********************************************************************************************
 * @brief br_BGT (Branch if Greater Than Zero)
 *
 * Architectural Opcode: 0x3F
 * Architectural Function Code: 0x00 (bits <5:0> of instruction word)
 *
 * Description:
 * Conditional branch instruction.
 * Causes a branch to the target address if the signed 64-bit value in integer register Ra
 * is greater than zero (i.e., Ra > 0).
 * The target address is calculated as:
 *   target_PC = current_PC + (signed 21-bit displacement << 2)
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5, Page C-10 and Section 4.8.1 (Page 4-28)
 ********************************************************************************************* */
class br_BGT_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the br_BGT instruction.
     *
     * @param cpu        AlphaCPU instance
     * @param context    AlphaProcessorContext (register and PC state)
     * @param rawInstr   Raw 64-bit instruction word
     */
    void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
    {
        quint64 pc = context->getPC();
        auto startCycles = cpu->readCpuCycleCounter();

        auto regsInt = context->registerBank();
        qint64 regValue = static_cast<qint64>(regsInt->readIntReg(m_ra));

        qint64 displacement = static_cast<qint64>(m_signedBranchDisplacement) << 2;
        quint64 targetAddress = pc + displacement;

        bool branchTaken = (regValue > 0);

        if (branchTaken) {
            context->setPC(targetAddress);
            cpu->log(QString("br_BGT taken: PC=0x%1 ? Target=0x%2 (R%3=%4 > 0)")
                .arg(pc, 0, 16).arg(targetAddress, 0, 16).arg(m_ra).arg(regValue), LogLevel::CONFIGURATION);
        }
        else {
            context->setPC(pc + 4);
            cpu->log(QString("br_BGT not taken: PC=0x%1 (R%2=%3 ? 0)")
                .arg(pc, 0, 16).arg(m_ra).arg(regValue), LogLevel::CONFIGURATION);
        }

        context->notifyInstructionExecuted(pc, rawInstr,
            branchTaken ? targetAddress : pc + 4);

        auto endCycles = cpu->readCpuCycleCounter();

        auto& profile = cpu->getInstructionProfile(grainType());
        profile.incrementExec();
        profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
        profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

        // Forcefully advance PC after execution as per convention
        context->setPC(context->getPC() + 4);
    }

    /**
     * @brief Decodes the br_BGT instruction.
     *
     * @param rawInstr   Raw 64-bit instruction word
     * @param cpu        AlphaCPU instance (for decode profiling)
     */
    void decode(quint64 rawInstr, AlphaCPU* cpu) override
    {
        auto startCycles = cpu->readCpuCycleCounter();

        m_opcode = static_cast<quint8>((rawInstr >> 26) & 0x3F);
        m_ra = static_cast<quint8>((rawInstr >> 21) & 0x1F);

        // Sign-extend the 21-bit displacement and validate the range
        m_signedBranchDisplacement =
            static_cast<qint32>((static_cast<int32_t>((rawInstr & 0x1FFFFF) << 11)) >> 11);

        Q_ASSERT_X(m_signedBranchDisplacement >= -1048576 && m_signedBranchDisplacement <= 1048575,
            "br_BGT decode",
            "Displacement out of valid 21-bit signed range");

        auto endCycles = cpu->readCpuCycleCounter();

        auto& profile = cpu->getInstructionProfile(grainType());
        profile.incrementDecode();
        profile.addDecodeTime(cpu->convertCyclesToNs(endCycles - startCycles));
    }

    QPair<quint8, quint16> opcodeAndFunction() const override {
        return qMakePair(opcode(), functionCode());
    }

    static constexpr quint8 opcode() { return 0x3F; }
    static constexpr quint16 functionCode() { return 0x00; }

    GrainType grainType() const override { return GrainType::BR_BGT; }
    QString mnemonic() const override { return "br_BGT"; }

private:
    quint8 m_opcode;                      ///< Decoded opcode field
    quint8 m_ra;                          ///< Source register
    qint32 m_signedBranchDisplacement;   ///< 21-bit sign-extended branch displacement

};

#endif // BR_BGT_INSTRUCTIONGRAIN_H
