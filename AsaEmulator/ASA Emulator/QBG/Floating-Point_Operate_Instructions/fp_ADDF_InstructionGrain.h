#ifndef OPR_ADDF_INSTRUCTIONGRAIN_H
#define OPR_ADDF_INSTRUCTIONGRAIN_H

#include "alphaInstructionGrain.h"
#include "AlphaCPU_refactored.h"
#include "AlphaProcessorContext.h"
#include "grainDependencies.h"
#include "enumFlags.h"
#include <QString>
#include <QPair>
#include <cmath>
#include <limits>
#include "../../QEC/FpcrRegister.h"

/** *********************************************************************************************
 * @brief opr_ADDF (Add Floating-point - IEEE S_floating format)
 *
 * Architectural Opcode: 0x15 (Operate Format)
 * Architectural Function Code: 0x080
 *
 * Description:
 * Performs floating-point addition on two S_floating (single-precision IEEE) source registers.
 * Applies FPCR-based rounding. Sets exception flags for inexact, underflow, overflow, etc.
 *
 * Reference:
 * Alpha AXP System Reference Manual (ASA), Table C-5 (Page C-10),
 * Section 4.10.2 (Page 4-33), FPCR behavior Section 4.9.
 * Source: Alpha_AXP_System_Reference_Manual_Version_6_1994.pdf
 ********************************************************************************************* */

class fp_ADDF_InstructionGrain : public AlphaInstructionGrain
{
public:

	void execute(AlphaCPU* cpu, AlphaProcessorContext* context, quint64 rawInstr) const override
	{
		quint64 pc = context->getPC();
		auto startCycles = cpu->readCpuCycleCounter();

		auto fpRegs = context->registerBank();
		auto& fpcr = context->fpcr();

		float srcA = fpRegs->readFpRegSingle(m_ra);
		float srcB = fpRegs->readFpRegSingle(m_rb);
		float rawResult = srcA + srcB;

		// Round result per FPCR mode
		float result = context->roundFloat(rawResult);

		// Exception flags
		bool invalid = std::isnan(srcA) || std::isnan(srcB);
		bool overflow = std::isinf(result) && !std::isinf(srcA) && !std::isinf(srcB);
		bool underflow = (result != 0.0f && std::fpclassify(result) == FP_SUBNORMAL);
		bool inexact = (result != rawResult);

		if (invalid)   fpcr.setFlag(STATUS_INVALID, true);
		if (overflow)  fpcr.setFlag(STATUS_OVERFLOW, true);
		if (underflow) fpcr.setFlag(STATUS_UNDERFLOW, true);
		if (inexact)   fpcr.setFlag(FLAG_INEXACT, true);

		// Diagnostic logging of FPCR rounding mode if traceFP is enabled

		if (cpu->traceFP()) {
			QString mode;
			auto fpcrVal = fpcr.getRaw() & ROUNDING_CONTROL_MASK;
			switch (fpcrVal) {
			case ROUND_TO_NEAREST:   mode = "Round to Nearest"; break;
			case ROUND_TO_MINUS_INF: mode = "Round to -Inf";    break;
			case ROUND_TO_PLUS_INF:  mode = "Round to +Inf";    break;
			case ROUND_TO_ZERO:      mode = "Round to Zero";    break;
			default:                 mode = "Unknown";          break;
			}
			cpu->log(QString("FPCR Rounding Mode at PC=0x%1: %2")
				.arg(pc, 0, 16).arg(mode), LogLevel::DEBUG);
		}

		fpRegs->writeFpRegSingle(m_rc, result);

		cpu->log(QString("opr_ADDF executed at PC=0x%1 : R%2(%.8e) + R%3(%.8e) = %.8e ? R%4")
			.arg(pc, 0, 16)
			.arg(m_ra).arg(srcA)
			.arg(m_rb).arg(srcB)
			.arg(result).arg(m_rc), LogLevel::DEBUG);

		context->notifyInstructionExecuted(pc, rawInstr, *reinterpret_cast<const quint32*>(&result));

		auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementExec();
		profile.addExecuteTime(cpu->convertCyclesToNs(endCycles - startCycles));
		profile.addEstimatedAlphaCycles(estimateAlphaCycles(endCycles - startCycles));

		// Advance PC after execution
		context->setPC(pc + 4);
	}


    void decode(quint64 rawInstr, AlphaCPU* cpu) override
    {
        auto startCycles = cpu->readCpuCycleCounter();

        m_opcode = static_cast<quint8>((rawInstr >> 26) & 0x3F);
        m_ra = static_cast<quint8>((rawInstr >> 21) & 0x1F);
        m_rb = static_cast<quint8>((rawInstr >> 16) & 0x1F);
        m_rc = static_cast<quint8>((rawInstr >> 11) & 0x1F);
        m_function = static_cast<quint16>(rawInstr & 0x7FF);

        auto endCycles = cpu->readCpuCycleCounter();

		auto& profile = cpu->getInstructionProfile(grainType());
		profile.incrementDecode();
		profile.addDecodeTime(cpu->convertCyclesToNs(endCycles - startCycles));
    }

    QPair<quint8, quint16> opcodeAndFunction() const override
    {
        return qMakePair(opcode(), functionCode());
    }

    static constexpr quint8 opcode() { return 0x15; }
    static constexpr quint16 functionCode() { return 0x080; }
    GrainType grainType() const override { return GrainType::OPR_ADDF; }
    QString mnemonic() const override { return "opr_ADDF"; }

private:
    quint8 m_opcode;
    quint8 m_ra;
    quint8 m_rb;
    quint8 m_rc;
    quint16 m_function;

};

#endif // OPR_ADDF_INSTRUCTIONGRAIN_H
