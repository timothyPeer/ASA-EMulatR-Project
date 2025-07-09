#ifndef OPR_ADDG_INSTRUCTIONGRAIN_H
#define OPR_ADDG_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "grainDependencies.h"
#include <QString>
#include <QPair>

/** *********************************************************************************************
 * @brief opr_ADDG (Add Floating-point - IEEE G_floating format)
 *
 * Architectural Opcode: 0x15 (Operate Format)
 * Architectural Function Code: 0x0A0
 *
 * Description:
 * Performs floating-point addition on two G_floating (double-precision IEEE) source registers.
 * Stores the IEEE G_floating result in the destination register.
 *
 * Assembly Mnemonic: ADDG
 *
 * Implementation Notes:
 * - Opcode: 0x15 (per ASA Table C-5, Operate format)
 * - Function code: 0x0A0 (per Section 4.10.2, Floating-Point Operate Instructions)
 * - IEEE-compliant double-precision floating-point addition.
 *
 * Dispatch Model Notes:
 * - Uses both opcode and functionCode for dispatch indexing.
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5 (Page C-10), Section 4.10.2 (Page 4-33)
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */

class fp_ADDG_InstructionGrain : public AlphaInstructionGrain
{
public:
    /**
     * @brief Executes the opr_ADDG instruction.
     *
     * @param cpu        AlphaCPU instance (for FP unit and logging)
     * @param context    #include "AlphaProcessorContext.h" (register state and FPCR)
     * @param rawInstr   Raw 64-bit instruction word
     */
   
	void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
	{
		quint64 pc = context->getPC();
		auto startCycles = cpu->readCpuCycleCounter();

		auto fpRegs = context->registerBank();
		auto fpcr = context->fpcr();

		// Read operands as IEEE G_floating (double)
		double srcA = fpRegs->readFpReg(m_ra);
		double srcB = fpRegs->readFpReg(m_rb);

		// Raw addition result
		double rawResult = srcA + srcB;

		// Apply FPCR-based rounding
		double result = context->roundFloat(rawResult); // ok to overload this for double

		// IEEE exception flags
		bool invalid = std::isnan(srcA) || std::isnan(srcB) || std::isnan(result);
		bool overflow = std::isinf(result) && !std::isinf(srcA) && !std::isinf(srcB);
		bool underflow = (result != 0.0 && std::fpclassify(result) == FP_SUBNORMAL);
		bool inexact = (result != rawResult);

		if (invalid)   fpcr.setFlag(STATUS_INVALID, true);
		if (overflow)  fpcr.setFlag(STATUS_OVERFLOW, true);
		if (underflow) fpcr.setFlag(STATUS_UNDERFLOW, true);
		if (inexact)   fpcr.setFlag(FLAG_INEXACT, true);

		// Optional: log FPCR rounding mode if tracing is enabled
		if (cpu->traceFP()) {
			QString mode;
			auto modeBits = fpcr.raw() & ROUNDING_CONTROL_MASK;
			switch (modeBits) {
			case ROUND_TO_NEAREST:   mode = "Round to Nearest"; break;
			case ROUND_TO_MINUS_INF: mode = "Round to -Inf"; break;
			case ROUND_TO_PLUS_INF:  mode = "Round to +Inf"; break;
			case ROUND_TO_ZERO:      mode = "Round to Zero"; break;
			default:                 mode = "Unknown"; break;
			}
			cpu->log(QString("FPCR Rounding Mode at PC=0x%1: %2")
				.arg(pc, 0, 16).arg(mode), LogLevel::DEBUG);
		}

		// Write result
		fpRegs->writeFpReg(m_rc, result);

		cpu->log(QString("opr_ADDG executed at PC=0x%1 : R%2(%.16e) + R%3(%.16e) = %.16e ? R%4")
			.arg(pc, 0, 16)
			.arg(m_ra).arg(srcA)
			.arg(m_rb).arg(srcB)
			.arg(result).arg(m_rc), LogLevel::DEBUG);

		context->notifyInstructionExecuted(pc, rawInstr, *reinterpret_cast<const quint64*>(&result));

		auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementExec();
		profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
		profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

		// Advance PC after execution
		context->setPC(pc + 4);
	}


    /**
     * @brief Decodes the opr_ADDG instruction.
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
        m_function = static_cast<quint16>(rawInstr & 0x7FF);  // Lower 11 bits for function code

        auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementDecode();
		profile.addDecodeTime(cpu->convertCyclesToNs(endCycles - startCycles));
    }

    /**
     * @brief Returns the opcode and function code for dispatch mapping.
     *
     * @return QPair where .first = opcode (0x15), .second = function code (0x0A0)
     */
    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    /**
     * @brief Returns the opcode for opr_ADDG.
     * @return Opcode (0x15)
     */
    static constexpr quint8 opcode() { return 0x15; }

    /**
     * @brief Returns the function code for opr_ADDG.
     * @return Function code (0x0A0)
     */
    static constexpr quint16 functionCode() { return 0x0A0; }

    /**
     * @brief Returns the grain type for profiling and dispatch indexing.
     */
    GrainType grainType() const override { return GrainType::OPR_ADDG; }

    /**
     * @brief Returns the mnemonic name for disassembly and debug output.
     */
    QString mnemonic() const override { return "opr_ADDG"; }

private:
    quint8 m_opcode;       ///< Decoded opcode field
    quint8 m_ra;           ///< First source register
    quint8 m_rb;           ///< Second source register
    quint8 m_rc;           ///< Destination register
    quint16 m_function;    ///< Decoded 11-bit function code

};

#endif // OPR_ADDG_INSTRUCTIONGRAIN_H
