#ifndef BR_FBGE_INSTRUCTIONGRAIN_H
#define BR_FBGE_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "InstructionProfile.h"
#include "grainDependencies.h"
#include <QString>
#include <QPair>

/** *********************************************************************************************
 * @brief br_FBGE (Floating-Point Branch if Greater Than or Equal to Zero)
 *
 * Architectural Opcode: 0x36
 * Architectural Function Code: 0x00 (bits <5:0> of instruction word)
 *
 * Description:
 * Floating-point conditional branch instruction.
 * Branches to the target address if the FPGE (Floating Branch Greater or Equal) condition code
 * in the Floating-Point Condition Register (FPCR) is set (FPCC_GE = 1).
 * The target address is calculated as:  
 * target_PC = current_PC + (signed 21-bit displacement << 2)
 *
 * Implementation Notes:
 * - Opcode: 0x36 (per ASA Table C-5)
 * - Function code: 0x00
 * - Displacement field: bits <20:0>, sign-extended, left-shifted by 2 for byte offset.
 * - Condition check: Test FPCR condition code bit for GE status.
 *
 * Dispatch Model Notes:
 * - Uses both opcode and functionCode for dispatch indexing.
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5, Page C-10 and Section 4.8.2 (Page 4-29)
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */

class br_FBGE_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the br_FBGE instruction.
     *
     * @param cpu        AlphaCPU instance (for PC update and FPCR access)
     * @param context     AlphaProcessorContext (register and FPCR state)
     * @param rawInstr   Raw 64-bit instruction word
     */

	void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
	{
		quint64 pc = context->getPC();
		auto startCycles = cpu->readCpuCycleCounter();

		// Evaluate FPCR FPCC_GE condition correctly, including edge cases (boundary & NaN)
		bool conditionMet = context->fpcr().getFPConditionGreaterEqual();

		// Sign-extend and shift 21-bit displacement
		qint64 displacement = static_cast<qint64>(m_signedBranchDisplacement) << 2;
		quint64 targetAddress = pc + displacement;

		if (conditionMet) {
			context->setPC(targetAddress);
			cpu->log(QString("br_FBGE taken: PC=0x%1 ? Target=0x%2 (FPCC_GE=1)")
				.arg(pc, 0, 16)
				.arg(targetAddress, 0, 16), LogLevel::CONFIGURATION);
		}
		else {
			context->setPC(pc + 4);
			cpu->log(QString("br_FBGE not taken: PC=0x%1 (FPCC_GE=0)")
				.arg(pc, 0, 16), LogLevel::CONFIGURATION);
		}

		context->notifyInstructionExecuted(pc, rawInstr,
			conditionMet ? targetAddress : pc + 4);

		auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementExec();
		profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
		profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

		// Final PC advance after execution to meet convention
		context->setPC(context->getPC() + 4);
	}


    /**
     * @brief Decodes the br_FBGE instruction.
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
     * @return QPair where .first = opcode (0x36), .second = function code (0x00)
     */
    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    /**
     * @brief Returns the opcode for br_FBGE.
     * @return Opcode (0x36)
     */
    static constexpr quint8 opcode() { return 0x36; }

    /**
     * @brief Returns the function code for br_FBGE.
     * @return Function code (0x00 from ASA Table C-5)
     */
    static constexpr quint16 functionCode() { return 0x00; }

    /**
     * @brief Returns the grain type for profiling and dispatch indexing.
     */
    GrainType grainType() const override { return GrainType::BR_FBGE; }

    /**
     * @brief Returns the mnemonic name for disassembly and debug output.
     */
    QString mnemonic() const override { return "br_FBGE"; }

private:
    quint8 m_opcode;               ///< Decoded opcode field
    qint32 m_signedBranchDisplacement; ///< Sign-extended 21-bit branch displacement (<<2 during execution)

    /**
     * TODO: Validate FPCC_GE evaluation with boundary and NaN cases.
     * - Add unit tests for all FP branch outcomes.
     */
};

#endif // BR_FBGE_INSTRUCTIONGRAIN_H
