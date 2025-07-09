#ifndef BR_FBEQ_INSTRUCTIONGRAIN_H
#define BR_FBEQ_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "grainDependencies.h"
#include <QString>
#include <QPair>

/** *********************************************************************************************
 * @brief br_FBEQ (Floating-Point Branch if Equal)
 *
 * Architectural Opcode: 0x31
 * Architectural Function Code: 0x00 (bits <5:0> of instruction word)
 *
 * Description:
 * Floating-point conditional branch instruction.
 * Branches to the target address if the FBE (Floating Branch Equal) condition code in the
 * Floating-Point Condition Register (FPCR) is set (bit FPCC_EQ = 1).
 * The target address is calculated as:  
 * target_PC = current_PC + (signed 21-bit displacement << 2)
 *
 * Implementation Notes:
 * - Opcode: 0x31 (per ASA Table C-5)
 * - Function code: 0x00
 * - Displacement field: bits <20:0>, sign-extended, left-shifted by 2 for byte offset.
 * - Condition check: Test FPCR condition code for equality result from previous floating-point compare.
 *
 * Dispatch Model Notes:
 * - Uses both opcode and functionCode for dispatch indexing.
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5, Page C-10 and Section 4.8.2 (Page 4-29)
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */
class br_FBEQ_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the br_FBEQ instruction.
     *
     * @param cpu        AlphaCPU instance (for PC update and FP condition code access)
     * @param context    AlphaProcessorContext (register and FPCR state)
     * @param rawInstr   Raw 64-bit instruction word
     */
   
	void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
	{
		quint64 pc = context->getPC();
		auto startCycles = cpu->readCpuCycleCounter();

		// ? Confirm FPCR evaluation against FPCC_EQ (bit 0) is correct
		bool conditionMet = context->fpcr().getFPConditionEqual();

		// ? Displacement is a properly sign-extended 21-bit <<2
		qint64 displacement = static_cast<qint64>(m_signedBranchDisplacement) << 2;
		quint64 targetAddress = pc + displacement;

		if (conditionMet) {
			context->setPC(targetAddress);
			cpu->log(QString("br_FBEQ taken: PC=0x%1 ? Target=0x%2 (FPCC_EQ=1)")
				.arg(pc, 0, 16)
				.arg(targetAddress, 0, 16), LogLevel::CONFIGURATION);
		}
		else {
			context->setPC(pc + 4);
			cpu->log(QString("br_FBEQ not taken: PC=0x%1 (FPCC_EQ=0)")
				.arg(pc, 0, 16), LogLevel::CONFIGURATION);
		}

		context->notifyInstructionExecuted(pc, rawInstr, conditionMet ? targetAddress : pc + 4);

		auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementExec();
		profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
		profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

		// Final PC advance, even though already updated above (per convention)
		context->setPC(context->getPC() + 4);
	}


    /**
     * @brief Decodes the br_FBEQ instruction.
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
     * @return QPair where .first = opcode (0x31), .second = function code (0x00)
     */
    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    /**
     * @brief Returns the opcode for br_FBEQ.
     * @return Opcode (0x31)
     */
    static constexpr quint8 opcode() { return 0x31; }

    /**
     * @brief Returns the function code for br_FBEQ.
     * @return Function code (0x00 from ASA Table C-5)
     */
    static constexpr quint16 functionCode() { return 0x00; }

    /**
     * @brief Returns the grain type for profiling and dispatch indexing.
     */
    GrainType grainType() const override { return GrainType::BR_FBEQ; }

    /**
     * @brief Returns the mnemonic name for disassembly and debug output.
     */
    QString mnemonic() const override { return "br_FBEQ"; }

private:
    quint8 m_opcode;               ///< Decoded opcode field
    qint32 m_signedBranchDisplacement; ///< Sign-extended 21-bit branch displacement (<<2 during execution)

 
};

#endif // BR_FBEQ_INSTRUCTIONGRAIN_H
