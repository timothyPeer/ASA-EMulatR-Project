#ifndef BR_BLE_INSTRUCTIONGRAIN_H
#define BR_BLE_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "InstructionProfile.h"
#include "grainDependencies.h"
#include <QString>
#include <QPair>

/** *********************************************************************************************
 * @brief br_BLE (Branch if Less Than or Equal to Zero)
 *
 * Architectural Opcode: 0x3B
 * Architectural Function Code: 0x00 (bits <5:0> of instruction word)
 *
 * Description:
 * Conditional branch instruction.
 * Branches to the target address if the signed 64-bit value in integer register Ra
 * is less than or equal to zero (Ra ? 0).
 * The target address is calculated as:
 * target_PC = current_PC + (signed 21-bit displacement << 2)
 *
 * Implementation Notes:
 * - Opcode: 0x3B (per ASA Table C-5)
 * - Function code: 0x00
 * - Displacement field: bits <20:0>, sign-extended, left-shifted by 2 for byte offset.
 *
 * Dispatch Model Notes:
 * - Uses both opcode and functionCode for dispatch indexing.
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5, Page C-10 and Section 4.8.1 (Page 4-28)
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */
class br_BLE_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the br_BLE instruction.
     *
     * @param cpu        AlphaCPU instance (for PC update and logging)
     * @param context    AlphaProcessorContext" (register and PC state)
     * @param rawInstr   Raw 64-bit instruction word
     */
 
	void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
	{
		quint64 pc = context->getPC();
		auto startCycles = cpu->readCpuCycleCounter();

		auto regsInt = context->registerBank();
		qint64 regValue = static_cast<qint64>(regsInt->readIntReg(m_ra));

		// Ensure displacement is properly sign-extended and <<2 for byte offset
		qint64 displacement = static_cast<qint64>(m_signedBranchDisplacement) << 2;
		quint64 targetAddress = pc + displacement;

		bool branchTaken = (regValue <= 0);

		if (branchTaken) {
			context->setPC(targetAddress);
			cpu->log(QString("br_BLE taken: PC=0x%1 ? Target=0x%2 (R%3=%4 ? 0)")
				.arg(pc, 0, 16).arg(targetAddress, 0, 16).arg(m_ra).arg(regValue), LogLevel::CONFIGURATION);
		}
		else {
			context->setPC(pc + 4);
			cpu->log(QString("br_BLE not taken: PC=0x%1 (R%2=%3 > 0)")
				.arg(pc, 0, 16).arg(m_ra).arg(regValue, 0, 16), LogLevel::CONFIGURATION);
		}

		context->notifyInstructionExecuted(pc, rawInstr,
			branchTaken ? targetAddress : pc + 4);

		auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementExec();
		profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
		profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

		// Advance PC after execution (even if already set) to ensure consistency
		context->setPC(context->getPC() + 4);
	}


    /**
     * @brief Decodes the br_BLE instruction.
     *
     * @param rawInstr   Raw 64-bit instruction word
     * @param cpu        AlphaCPU instance (for decode profiling)
     */
    void decode(quint64 rawInstr, AlphaCPU* cpu) override
    {
        auto startCycles = cpu->readCpuCycleCounter();

        m_opcode = static_cast<quint8>((rawInstr >> 26) & 0x3F);
        m_ra = static_cast<quint8>((rawInstr >> 21) & 0x1F);
        m_signedBranchDisplacement = static_cast<qint32>(static_cast<quint32>((rawInstr & 0x001FFFFF) << 11) >> 11);

        auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementDecode();
		profile.addDecodeTime(cpu->convertCyclesToNs(endCycles - startCycles));
    }

    /**
     * @brief Returns the opcode and function code for dispatch mapping.
     *
     * @return QPair where .first = opcode (0x3B), .second = function code (0x00)
     */
    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    /**
     * @brief Returns the opcode for br_BLE.
     * @return Opcode (0x3B)
     */
    static constexpr quint8 opcode() { return 0x3B; }

    /**
     * @brief Returns the function code for br_BLE.
     * @return Function code (0x00 from ASA Table C-5)
     */
    static constexpr quint16 functionCode() { return 0x00; }

    /**
     * @brief Returns the grain type for profiling and dispatch indexing.
     */
    GrainType grainType() const override { return GrainType::BR_BLE; }

    /**
     * @brief Returns the mnemonic name for disassembly and debug output.
     */
    QString mnemonic() const override { return "br_BLE"; }

private:
    quint8 m_opcode;               ///< Decoded opcode field
    quint8 m_ra;                   ///< Source register for condition check
    qint32 m_signedBranchDisplacement; ///< Sign-extended 21-bit branch displacement (<<2 during execution)

    /**
     * TODO:
     * - Verify sign-extension and branch offset calculation for 21-bit displacement.
     * - Confirm PC-relative branching handles cross-page branches correctly.
     */
};

#endif // BR_BLE_INSTRUCTIONGRAIN_H
