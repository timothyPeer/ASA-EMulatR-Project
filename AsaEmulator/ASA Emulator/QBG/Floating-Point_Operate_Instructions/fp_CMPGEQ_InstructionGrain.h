#ifndef FP_CMPGEQ_INSTRUCTIONGRAIN_H
#define FP_CMPGEQ_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "InstructionProfile.h"
#include "grainDependencies.h"
#include <QString>
#include <QPair>
#include <cmath>
#include <limits>

/** *********************************************************************************************
 * @brief fp_CMPGEQ (Compare Greater Than or Equal - G_Floating Format)
 *
 * Architectural Opcode: 0x15 (Floating-Point Operate Format)
 * Architectural Function Code: 0x0A5
 *
 * Description:
 * Compares two IEEE G_floating (double-precision) operands from Ra and Rb.
 * If srcA >= srcB (according to IEEE floating-point rules), writes 1 to Rc.
 * If srcA < srcB, writes 0 to Rc.
 * Traps are raised for Invalid Operation (e.g., NaN operands), but only if
 * the corresponding FPCR INVALID_ENABLE bit is set.
 *
 * Assembly Mnemonic: CMPGEQ
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-7 (Page C-16), Section 4.7.7 (Floating-Point Exceptions)
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */

class fp_CMPGEQ_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the fp_CMPGEQ instruction with IEEE NaN checks and FPCR filtering.
     *
     * @param cpu        AlphaCPU instance (for exception signaling and logging)
     * @param context    #include "AlphaProcessorContext.h" (register and FP state)
     * @param rawInstr   Raw 64-bit instruction word
     */
    void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
    {
        quint64 pc = context->getPC();
        auto startCycles = cpu->readCpuCycleCounter();

        auto regs = context->registerBank();
        double srcA = regs->readFpReg(m_ra);
        double srcB = regs->readFpReg(m_rb);

        // Invalid Operation Check: NaN operand triggers invalid exception
        if (isInvalidFpOperandDouble(srcA) || isInvalidFpOperandDouble(srcB))
        {
            if (checkAndRaiseFpExceptionIfEnabled(cpu, context, pc, FpcrRegister::FpcrBit::INVALID_ENABLE, ExceptionType::FP_EXCEPTION))
                return;
        }

        quint64 result = (srcA >= srcB) ? 1 : 0;
        regs->writeIntReg(m_rc, result);

        cpu->log(QString("fp_CMPGEQ executed at PC=0x%1 : %2 >= %3 -> %4 (written to R%5)")
                 .arg(pc, 0, 16)
                 .arg(srcA)
                 .arg(srcB)
                 .arg(result)
                 .arg(m_rc));

        context->notifyInstructionExecuted(pc, rawInstr, result);

        auto endCycles = cpu->readCpuCycleCounter();
        auto& profile = cpu->instructionProfiles[static_cast<int>(grainType())];
        profile.executeCount++;
        profile.totalExecuteTimeNs += cpu->convertCyclesToNs(endCycles - startCycles);
        profile.totalEstimatedAlphaCycles += estimateAlphaCycles(endCycles - startCycles);
        // Advance PC after execution
        context->setPC(pc + 4);
    }

    /**
     * @brief Decodes the fp_CMPGEQ instruction.
     *
     * @param rawInstr   Raw 64-bit instruction word
     * @param cpu        AlphaCPU instance (for decode profiling)
     */
    void decode(quint64 rawInstr, AlphaCPU* cpu) override
    {
        auto startCycles = cpu->readCpuCycleCounter();

        m_opcode = static_cast<quint8>((rawInstr >> 26) & 0x3F);
        m_ra = static_cast<quint8>((rawInstr >> 21) & 0x1F);
        m_rb = static_cast<quint8>((rawInstr >> 16) & 0x1F);
        m_rc = static_cast<quint8>((rawInstr >> 11) & 0x1F);
        m_function = static_cast<quint16>(rawInstr & 0x7FF);

        auto endCycles = cpu->readCpuCycleCounter();
        auto& profile = cpu->instructionProfiles[static_cast<int>(grainType())];
        profile.decodeCount++;
        profile.totalDecodeTimeNs += cpu->convertCyclesToNs(endCycles - startCycles);
    }

    /**
     * @brief Returns the opcode and function code for dispatch mapping.
     *
     * @return QPair where .first = opcode (0x15), .second = function code (0x0A5)
     */
    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    /**
     * @brief Returns the opcode for fp_CMPGEQ.
     * @return Opcode (0x15)
     */
    static constexpr quint8 opcode() { return 0x15; }

    /**
     * @brief Returns the function code for fp_CMPGEQ.
     * @return Function code (0x0A5)
     */
    static constexpr quint16 functionCode() { return 0x0A5; }

    /**
     * @brief Returns the grain type for profiling and dispatch indexing.
     */
    GrainType grainType() const override { return GrainType::FP_CMPGEQ; }

    /**
     * @brief Returns the mnemonic name for disassembly and debug output.
     */
    QString mnemonic() const override { return "fp_CMPGEQ"; }

private:
    quint8 m_opcode;       ///< Decoded opcode field
    quint8 m_ra;           ///< First source register (floating-point)
    quint8 m_rb;           ///< Second source register (floating-point)
    quint8 m_rc;           ///< Destination integer register
    quint16 m_function;    ///< Decoded 11-bit function code

    /** TODO: None */
};

#endif // FP_CMPGEQ_INSTRUCTIONGRAIN_H
