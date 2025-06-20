#pragma once
// PalInstruction_Alpha.h
// Header for decoding and executing Alpha AXP PALcode (CALL_PAL) instructions in JitLoadExecutor
// All functions are inline for maximum performance.
// Instruction format: 6-bit opcode in bits<31:26>, 26-bit function code in bits<25:0> (Appendix C.1, Table C-1) :contentReference[oaicite:0]{index=0}
// Required PALcode function codes (DRAINA, HALT, IMB, etc.) in Table C-15 :contentReference[oaicite:1]{index=1}



#include <cstdint>
#include <atomic>
#include <iostream>
#include "../ABA/structs/Instruction.h"
#include <stdexcept>

namespace Arch {

    // Representation of a 32-bit CALL_PAL instruction word
    struct PalInstruction_Alpha : public Arch::Instruction {
        uint32_t raw;    ///< raw instruction bits
        uint8_t  opcode; ///< bits <31:26>, should equal CALL_PAL (0x00)
        uint32_t fnc;    ///< 26-bit function code field bits <25:0>

        /// Decode raw instruction into opcode and function code
        inline void decode() {
            opcode = static_cast<uint8_t>((raw >> 26) & 0x3F);
            fnc = raw & 0x03FFFFFF;
        }

        FormatID format() const override { return FormatID::ALPHA_PAL; }
        uint16_t getCode() const override { return opcode; }


		/** [0.0000] Stop CPU execution and enter PAL mode   */
		static void emitAlpha_HALT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0001] Flush processor caches   */
		static void emitAlpha_CFLUSH(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0002] Drain write buffers and memory queues   */
		static void emitAlpha_DRAINA(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0003] Load quadword from PAL page   */
		static void emitAlpha_LDQP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0004] Store quadword to PAL page   */
		static void emitAlpha_STQP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0006] Read Address Space Number register   */
		static void emitAlpha_MFPR_ASN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0007] Write AST enable register   */
		static void emitAlpha_MTPR_ASTEN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0008] Write AST status register   */
		static void emitAlpha_MTPR_ASTSR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0009] 0 */
		static void emitAlpha_CSERVE(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0010] Read machine-check error summary (MCES) register   */
		static void emitAlpha_MFPR_MCES(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0011] Write MCES register   */
		static void emitAlpha_MTPR_MCES(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0012] Read Process Control Block Base register   */
		static void emitAlpha_MFPR_PCBB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0013] Read Processor Restart Block register   */
		static void emitAlpha_MFPR_PRBR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0014] Write Processor Restart Block register   */
		static void emitAlpha_MTPR_PRBR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0015] Read Page Table Base register   */
		static void emitAlpha_MFPR_PTBR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0016] Reserved opcode (Pal)  */
		static void emitAlpha_MFPR_SCBB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0017] Write Shadow Context Base register   */
		static void emitAlpha_MTPR_SCBB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0018] Write Software Interrupt Request register   */
		static void emitAlpha_MTPR_SIRR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0019] Read Software Interrupt Status register   */
		static void emitAlpha_MFPR_SISR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0020] Read Supervisor Stack Pointer   */
		static void emitAlpha_MFPR_SSP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0021] Write Supervisor Stack Pointer   */
		static void emitAlpha_MTPR_SSP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0022] Read User Stack Pointer   */
		static void emitAlpha_MFPR_USP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0023] Write User Stack Pointer   */
		static void emitAlpha_MTPR_USP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0024] Write TLB Invalidate Directive register   */
		static void emitAlpha_MTPR_TBISD(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0025] Write TLB Invalidate Instruction register   */
		static void emitAlpha_MTPR_TBISI(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0026] Read AST Enable register   */
		static void emitAlpha_MFPR_ASTEN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0027] Read AST Status register   */
		static void emitAlpha_MFPR_ASTSR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0029] Read Virtual Page Table Base register   */
		static void emitAlpha_MFPR_VPTB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0030] Read interrupt request level (Alpha)  */
		static void emitAlpha_SWPCTX(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0030] 0 */
		static void emitAlpha_WRVAL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0032] Read syscall return-value register   */
		static void emitAlpha_RDVAL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0033] Trap on immediate condition   */
		static void emitAlpha_TBI(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0034] 0 */
		static void emitAlpha_WRENT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0035] 0 */
		static void emitAlpha_SWPIPL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0036] Read Processor Status register   */
		static void emitAlpha_RDPS(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0037] 0 */
		static void emitAlpha_WRKGP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0038] Write User Stack Pointer (alias)   */
		static void emitAlpha_WRUSP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0039] Write machine check summary (Alpha)  */
		static void emitAlpha_WRPERFMON(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0080] Branch if FP less?than (Alpha)  */
		static void emitAlpha_BPT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0081] Reserved PAL opcode (Pal)  */
		static void emitAlpha_BUGCHK(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0082] Change to Machine Exception state   */
		static void emitAlpha_CHME(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0083] Change to Master Kernel mode   */
		static void emitAlpha_CHMK(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0084] Change to Master Supervisor mode   */
		static void emitAlpha_CHMS(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0085] Change to Master User mode   */
		static void emitAlpha_CHMU(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0086] Reserved PAL opcode (Pal)  */
		static void emitAlpha_IMB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0087] Insert queue head (interrupt-low)   */
		static void emitAlpha_INSQHIL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0088] Insert queue tail (interrupt-low)   */
		static void emitAlpha_INSQTIL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0089] Insert queue head (interrupt-high)   */
		static void emitAlpha_INSQHIQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0090] Probe memory write (fault detection)   */
		static void emitAlpha_PROBEW(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0091] Read Processor Status   */
		static void emitAlpha_RD_PS(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0092] Return from Exception/Interrupt   */
		static void emitAlpha_REI(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0093] Remove queue head (interrupt-low)   */
		static void emitAlpha_REMQHIL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0094] Remove queue tail (interrupt-low)   */
		static void emitAlpha_REMQTIL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0095] Remove queue head (interrupt-high)   */
		static void emitAlpha_REMQHIQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0096] Remove queue tail (interrupt-high)   */
		static void emitAlpha_REMQTIQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0097] Remove queue entry (low priority)   */
		static void emitAlpha_REMQUEL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0098] Remove queue entry (quiet)   */
		static void emitAlpha_REMQUEQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0099] Remove queue entry (quiet)   */
		static void emitAlpha_REMQUEL / D(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.???3] Reboot the system via PAL   */
		static void emitAlpha_REBOOT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000A] Switch to PAL mode   */
		static void emitAlpha_SWPPAL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000B] Read Floating-Point Enable register   */
		static void emitAlpha_MFPR_FEN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000C] Reserved PAL opcode (Pal)  */
		static void emitAlpha_MTPR_FEN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000D] Write In-Progress Interrupt register   */
		static void emitAlpha_MTPR_IPIR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000E] Read Interrupt Priority Level register   */
		static void emitAlpha_MFPR_IPL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000F] Write Interrupt Priority Level register   */
		static void emitAlpha_MTPR_IPL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001A] Read TLB Cache Hit register   */
		static void emitAlpha_MFPR_TBCHK(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001B] Reserved opcode (Pal)  */
		static void emitAlpha_MTPR_TBIA(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001C] Write TLB Invalidate All Processor register   */
		static void emitAlpha_MTPR_TBIAP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001D] Write TLB Invalidate Selective register   */
		static void emitAlpha_MTPR_TBIS(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001E] Read Event-Service Pointer register   */
		static void emitAlpha_MFPR_ESP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001F] Write Event-Service Pointer register   */
		static void emitAlpha_MTPR_ESP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002A] Swap user/kernel context registers (alias)  */
		static void emitAlpha_MTPR_VPTB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002B] Write Virtual Page Table Base register */
		static void emitAlpha_MTPR_PERFMON(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002D] 0 */
		static void emitAlpha_WRVPTPTR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002E] Write Floating-Point Exception Enable register */
		static void emitAlpha_MTPR_DATFX(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003A] Write Data Function Extension register */
		static void emitAlpha_RDUSP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003C] Read User Stack Pointer register */
		static void emitAlpha_WHAMI(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003D] Read hardware processor identifier (“Where Am I”)  */
		static void emitAlpha_RETSYS(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003E]  */
		static void emitAlpha_WTINT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003F] Return from system call to PAL */
		static void emitAlpha_MFPR_WHAMI(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.008A] Return from interrupt (alias)  */
		static void emitAlpha_INSQTIQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.008B] Insert queue tail (interrupt-quiet)  */
		static void emitAlpha_INSQUEL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.008C] Insert queue element (low priority)  */
		static void emitAlpha_INSQUEQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.008D] Insert queue element (quiet)  */
		static void emitAlpha_INSQUEUL / D(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.008E] Insert queue element (deferred)  */
		static void emitAlpha_INSQUEQ / D(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.008F] Insert queue element (quiet/deferred)  */
		static void emitAlpha_PROBER(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009A]  */
		static void emitAlpha_REMQUEQ / D(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009B] Remove queue entry (quiet/deferred)  */
		static void emitAlpha_SWASTEN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009C] Set software AST enable bit */
		static void emitAlpha_WR_PS_SW(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009D] Write Processor Status to shadow register */
		static void emitAlpha_RSCC(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009E] Read unique value register */
		static void emitAlpha_READ_UNQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009F] Read unique register (alias)  */
		static void emitAlpha_WRITE_UNQ(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A0] Write unique value register (alias)  */
		static void emitAlpha_AMOVRR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A1] Atomic Move Register-to-Register */
		static void emitAlpha_AMOVRM(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A2] Atomic Move Register-to-Memory */
		static void emitAlpha_INSQHILR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A3] Insert queue head (interrupt-low) with release */
		static void emitAlpha_INSQTILR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A4] Insert queue tail (interrupt-low) with release */
		static void emitAlpha_INSQHIQR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A5] Insert queue head (interrupt-high) with release */
		static void emitAlpha_INSQTIQR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A6] Insert queue tail (interrupt-high) with release */
		static void emitAlpha_REMQHILR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A7] Remove queue head (interrupt-low) with release */
		static void emitAlpha_REMQTILR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A8]  */
		static void emitAlpha_REMQHIQR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00A9] Remove queue tail (interrupt-low) with release */
		static void emitAlpha_REMQTIQR(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AA] Invalidate translation buffer by ASN (Alpha)  */
		static void emitAlpha_GENTRAP(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AB] Remove queue tail (interrupt-high) with release */
		static void emitAlpha_RDTEB(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AC] Generate a trap to PAL */
		static void emitAlpha_KBPT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AD] Read Thread Environment Block register */
		static void emitAlpha_CALLKD(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** []  */
		static void emitAlpha_CLRFEN(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0E] Write entrypoint (Alpha)  */
		static void emitAlpha_RFE(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.AB] Read kernel stack pointer (Alpha)  */
		static void emitAlpha_KBPT(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.] Call debugger via PAL */
		static void emitAlpha_CALL_PAL(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.] Swap to kernel stack pointer (Alpha)  */
		static void emitAlpha_SSW(PalInstruction_Alpha inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}










    };


}


