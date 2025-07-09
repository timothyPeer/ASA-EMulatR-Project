#ifndef BR_FBLT_INSTRUCTIONGRAIN_H
#define BR_FBLT_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "grainDependencies.h"
#include <QString>
#include <QPair>

/** *********************************************************************************************
 * @brief br_FBLT (Floating-Point Branch if Less Than Zero)
 *
 * Architectural Opcode: 0x32
 * Architectural Function Code: 0x00 (bits <5:0> of instruction word)
 *
 * Description:
 * Floating-point conditional branch instruction.
 * Branches to the target address if the FPCC_LT (Floating-Point Condition Code - Less Than Zero)
 * bit in the Floating-Point Condition Register (FPCR) is set (FPCC_LT = 1).
 * The target address is calculated as:  
 * target_PC = current_PC + (signed 21-bit displacement << 2)
 *
 * Implementation Notes:
 * - Opcode: 0x32 (per ASA Table C-5)
 * - Function code: 0x00
 * - Displacement field: bits <20:0>, sign-extended, left-shifted by 2 for byte offset.
 * - Condition check: Test FPCR LT condition flag.
 *
 * Dispatch Model Notes:
 * - Uses both opcode and functionCode for dispatch indexing.
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5, Page C-10 and Section 4.8.2 (Page 4-29)
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */

class br_FBLT_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the br_FBLT instruction.
     *
     * @param cpu        AlphaCPU instance (for PC update and FPCR access)
     * @param context    #include "AlphaProcessorContext.h" (register and FPCR state)
     * @param rawInstr   Raw 64-bit instruction word
     */

	void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
	{
		quint64 pc = context->getPC();
		auto startCycles = cpu->readCpuCycleCounter();

		// Confirm FPCR FPCC_LT condition evaluation is correct per Alpha FP semantics
		bool conditionMet = context->fpcr().getFPConditionLessThan();

		// Properly sign-extend and shift the 21-bit displacement
		qint64 displacement = static_cast<qint64>(m_signedBranchDisplacement) << 2;
		quint64 targetAddress = pc + displacement;

		if (conditionMet) {
			context->setPC(targetAddress);
			cpu->log(QString("br_FBLT taken: PC=0x%1 → Target=0x%2 (FPCC_LT=1)")
				.arg(pc, 0, 16)
				.arg(targetAddress, 0, 16), LogLevel::CONFIGURATION);
		}
		else {
			context->setPC(pc + 4);
			cpu->log(QString("br_FBLT not taken: PC=0x%1 (FPCC_LT=0)")
				.arg(pc, 0, 16), LogLevel::CONFIGURATION);
		}

		context->notifyInstructionExecuted(pc, rawInstr, conditionMet ? targetAddress : pc + 4);

		auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementExec();
		profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
		profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

		// Final PC advance to maintain convention even if branch occurred
		context->setPC(context->getPC() + 4);
	}


    /**
     * @brief Decodes the br_FBLT instruction.
     *
     * @param rawInstr   Raw 64-bit instruction word
     * @param cpu        AlphaCPU instance (for decode profiling)
     */
    void decode(quint64 rawInstr, AlphaCPU* cpu) override
    {
        auto startCycles = cpu->readCpuCycleCounter();

        m_opcode = static_cast<quint8>((rawInstr >> 26) & 0x3F);
        m_signedBranchDisplacement = static_cast<qint32>(static_cast<quint32>((rawInstr & 0x001FFFFF) << 11) >> 11);

        auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementDecode();
		profile.addDecodeTime(cpu->convertCyclesToNs(endCycles - startCycles));
    }

    /**
     * @brief Returns the opcode and function code for dispatch mapping.
     *
     * @return QPair where .first = opcode (0x32), .second = function code (0x00)
     */
    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    /**
     * @brief Returns the opcode for br_FBLT.
     * @return Opcode (0x32)
     */
    static constexpr quint8 opcode() { return 0x32; }

    /**
     * @brief Returns the function code for br_FBLT.
     * @return Function code (0x00 from ASA Table C-5)
     */
    static constexpr quint16 functionCode() { return 0x00; }

    /**
     * @brief Returns the grain type for profiling and dispatch indexing.
     */
    GrainType grainType() const override { return GrainType::BR_FBLT; }

    /**
     * @brief Returns the mnemonic name for disassembly and debug output.
     */
    QString mnemonic() const override { return "br_FBLT"; }

private:
    quint8 m_opcode;               ///< Decoded opcode field
    qint32 m_signedBranchDisplacement; ///< Sign-extended 21-bit branch displacement (<<2 during execution)


};

#endif // BR_FBLT_INSTRUCTIONGRAIN_H
