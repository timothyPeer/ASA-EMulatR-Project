#pragma once
#pragma once
// PalInstruction_Tru64.h
// Header for decoding and executing Alpha AXP PALcode (CALL_PAL) instructions in JitLoadExecutor
// All functions are inline for maximum performance.
// Instruction format: 6-bit opcode in bits<31:26>, 26-bit function code in bits<25:0> (Appendix C.1, Table C-1) :contentReference[oaicite:0]{index=0}
// Required PALcode function codes (DRAINA, HALT, IMB, etc.) in Table C-15 :contentReference[oaicite:1]{index=1}



#include <cstdint>
#include <atomic>
#include <iostream>
#include "../ABA/structs/Instruction.h"

namespace Arch {

    // Representation of a 32-bit CALL_PAL instruction word
    struct PalInstruction_Tru64_Tru64 : public Arch::Instruction {
        uint32_t raw;    ///< raw instruction bits
        uint8_t  opcode; ///< bits <31:26>, should equal CALL_PAL (0x00)
        uint32_t fnc;    ///< 26-bit function code field bits <25:0>

        /// Decode raw instruction into opcode and function code
        inline void decode() {
            opcode = static_cast<uint8_t>((raw >> 26) & 0x3F);
            fnc = raw & 0x03FFFFFF;
        }

        FormatID format() const override { return FormatID::TRU64_PAL; }
        uint16_t getCode() const override { return opcode; }


		/** [0.0000] Description*/
		static void emitTru64_HALT(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0001] Stop CPU execution and enter PAL mode  */
		static void emitTru64_CFLUSH(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0002] Branch if FP ? (Alpha)*/
		static void emitTru64_DRAINA(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0004] Initialize the PAL environment  */
		static void emitTru64_INITPAL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0006] Swap interrupt priority level (IPL)  */
		static void emitTru64_SWPIRQL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0007] Read interrupt priority level (IPL)  */
		static void emitTru64_RDIRQL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0008] Disable interrupts  */
		static void emitTru64_DI(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0009] Clear service-request history  */
		static void emitTru64_CSERVE(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0010] Read MCES register  */
		static void emitTru64_RDMCES(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0010] Disable interrupts (Alpha)*/
		static void emitTru64_SWPCTX(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0011] Set software interrupt request (Alpha)*/
		static void emitTru64_WRMCES(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.12] Read PCBB register  */
		static void emitTru64_RDPCBB(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0013] */
		static void emitTru64_WRVIRBBND(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0013] 0*/
		static void emitTru64_wrsysptb(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.15] Read “this” PAL return address register  */
		static void emitTru64_THIS(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0016] Invalidate a single TLB entry  */
		static void emitTru64_DTBIS(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0018] Read Kernel Stack Pointer  */
		static void emitTru64_RDKSP(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0019] Swap Kernel Stack Pointer  */
		static void emitTru64_SWPKSP(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0030] Read Performance Counters  */
		static void emitTru64_RDCOUNTERS(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0031] Write syscall return-value register  */
		static void emitTru64_WRVAL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0032] Write syscall return-value register  */
		static void emitTru64_RDVAL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0033] Read syscall return-value register  */
		static void emitTru64_TBI(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0034] Write PAL entry-point register  */
		static void emitTru64_WRENT(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0035] Swap IPL and current state  */
		static void emitTru64_SWPIPL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0036] Swap IPL and current state (alias)  */
		static void emitTru64_RDPS(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0037] Write Kernel Global Pointer register  */
		static void emitTru64_WRKGP(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0038] Write USP register  */
		static void emitTru64_WRUSP(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0039] Write Performance Monitor registers  */
		static void emitTru64_WRPERFMON(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0080] Breakpoint trap  */
		static void emitTru64_BPT(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0081] Bug-check/fatal-error trap  */
		static void emitTru64_BUGCHK(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0083] Call to system service routine  */
		static void emitTru64_CALLSYS(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0086] Instruction Memory Barrier  */
		static void emitTru64_IMB(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.0092] */
		static void emitTru64_URTI(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.??13] Write PRBR register  */
		static void emitTru64_WRPRBR_TOCHECK(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.??14] Invalidate entire TLB (global) TODO -- Requires OpCode Validation */
		static void emitTru64_TBIA_to_verify(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.??17] Invalidate TLB entries by ASN  TODO -- Requires OpCode Validation*/
		static void emitTru64_TBIMSASN(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000A] Remove queue entry (deferred)  */
		static void emitTru64_SWPPAL(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000C] Set Software Interrupt register  */
		static void emitTru64_SSIR(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000D] Reserved PAL opcode (Pal)*/
		static void emitTru64_WRIPIR(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.000E] Return from Exception  */
		static void emitTru64_RFE(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001A] Read Processor Status register (alias)  */
		static void emitTru64_RDPSR(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001C] Read Performance Error register  */
		static void emitTru64_RDPER(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.001E] Read Current Thread ID register  */
		static void emitTru64_RDTHREAD(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002B] Write Performance Monitor control register*/
		static void emitTru64_WRFEN(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002D] Write Floating-Point Exception Enable register*/
		static void emitTru64_WRVPTPTR(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.002E] Write Virtual Page Table Pointer register*/
		static void emitTru64_WRASN(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003A] Read USP register  */
		static void emitTru64_RDUSP(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003C] Write Data Function Extension register*/
		static void emitTru64_WHAMI(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003D] Return from System CALL  */
		static void emitTru64_RETSYS(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003E] 0*/
		static void emitTru64_WTINT(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.003F] Return from interrupt*/
		static void emitTru64_RTI(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009E] Read system cycle counter*/
		static void emitTru64_RDUNIQUE(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.009F] Write unique register*/
		static void emitTru64_WRUNIQUE(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AA] Remove queue tail (interrupt-high) with release*/
		static void emitTru64_GENTRAP(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AB] Generate a trap to PAL*/
		static void emitTru64_RDTEB(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AC] Read Thread Environment Block register*/
		static void emitTru64_KBPT(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [0.00AD] Kernel breakpoint trap*/
		static void emitTru64_CALLKD(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}


		/** [] 0*/
		static void emitTru64_CLRFEN(PalInstruction_Tru64 inst)
		{
			throw std::logic_error("The method or operation is not implemented.");
		}



    };


}


