#include "ExecutePalInstruction.h"

#include "AlphaCPU.h"
#include "AlphaPlatformGuards.h"
#include "InstructionPipeLine.h"
#include "PlatformPalOpcodes.h"
#include <QDebug>

/**
 * @brief Handles platform-specific PAL instruction execution
 *
 * This function provides platform-specific PAL instruction handling for the
 * Alpha instruction pipeline. It is called from ExecuteStage::executePAL()
 * and implements conditional execution based on the target platform.
 *
 * @param instruction The decoded PAL instruction to execute
 * @param cpu Pointer to the AlphaCPU instance
 * @return true if the instruction was handled, false otherwise
 */
bool ExecutePalInstruction(quint32 instruction, AlphaCPU *cpu)
{
    // Extract PAL function code (bits 25:0)
    quint32 palFunction = instruction & 0x03FFFFFF;

    // Common PAL instructions for all platforms
    if (palFunction == PalOpcodes::Common::HALT)
    {
        DEBUG_LOG("ExecuteStage: PAL HALT");
        cpu->halt();
        return true;
    }

    if (palFunction == PalOpcodes::Common::CFLUSH)
    {
        DEBUG_LOG("ExecuteStage: PAL CFLUSH");
        cpu->flushCaches();
        return true;
    }

    if (palFunction == PalOpcodes::Common::DRAINA)
    {
        DEBUG_LOG("ExecuteStage: PAL DRAINA");
        cpu->drainAborts();
        return true;
    }

    if (palFunction == PalOpcodes::Common::REI)
    {
        DEBUG_LOG("ExecuteStage: PAL REI");
        // Return from exception/interrupt
        cpu->returnFromException();
        return true;
    }

    // Platform-specific PAL instructions
#ifdef ALPHA_PLATFORM_TRU64
    // Tru64/Digital UNIX PAL instructions
    switch (palFunction)
    {
    case PalOpcodes::Tru64::CSERVE:
        DEBUG_LOG("ExecuteStage: PAL CSERVE");
        cpu->executeConsoleService();
        return true;

    case PalOpcodes::Tru64::MTPR_IPIR:
        DEBUG_LOG("ExecuteStage: PAL WRIPIR");
        cpu->writeIPIR(cpu->getRegister(16)); // R16 contains processor mask
        return true;

    case PalOpcodes::Tru64::RDMCES:
        DEBUG_LOG("ExecuteStage: PAL RDMCES");
        cpu->setRegister(0, cpu->readMCES());
        return true;

    case PalOpcodes::Tru64::WRMCES:
        DEBUG_LOG("ExecuteStage: PAL WRMCES");
        cpu->writeMCES(cpu->getRegister(16)); // R16 contains value
        return true;

    case PalOpcodes::Tru64::WRFEN:
        DEBUG_LOG("ExecuteStage: PAL WRFEN");
        cpu->writeFEN(cpu->getRegister(16) & 1); // R16 contains enable bit
        return true;

    case PalOpcodes::Tru64::SWPIRQL:
        DEBUG_LOG("ExecuteStage: PAL SWPIRQL");
        {
            quint64 newLevel = cpu->getRegister(16) & 0x1F; // R16 contains new level
            quint64 oldLevel = cpu->swapIRQL(newLevel);
            cpu->setRegister(0, oldLevel); // Store old level in R0
        }
        return true;

    case PalOpcodes::Tru64::RDIRQL:
        DEBUG_LOG("ExecuteStage: PAL RDIRQL");
        cpu->setRegister(0, cpu->readIRQL()); // Store current IRQL in R0
        return true;

    case PalOpcodes::Tru64::DI:
        DEBUG_LOG("ExecuteStage: PAL DI");
        cpu->disableInterrupts();
        return true;

    case PalOpcodes::Tru64::EI:
        DEBUG_LOG("ExecuteStage: PAL EI");
        cpu->enableInterrupts();
        return true;

    case PalOpcodes::Tru64::SWPPAL:
        DEBUG_LOG("ExecuteStage: PAL SWPPAL");
        {
            quint64 newBase = cpu->getRegister(16); // R16 contains new PAL base
            quint64 oldBase = cpu->swapPALBase(newBase);
            cpu->setRegister(0, oldBase); // Store old base in R0
        }
        return true;

    case PalOpcodes::Tru64::WRVPTPTR:
        DEBUG_LOG("ExecuteStage: PAL WRVPTPTR");
        cpu->writeVPTPtr(cpu->getRegister(16)); // R16 contains pointer
        return true;

    case PalOpcodes::Tru64::WTKTRP:
        DEBUG_LOG("ExecuteStage: PAL WTKTRP");
        cpu->writeTLBTrap(cpu->getRegister(16)); // R16 contains trap address
        return true;

    case PalOpcodes::Tru64::WRENT:
        DEBUG_LOG("ExecuteStage: PAL WRENT");
        {
            quint64 address = cpu->getRegister(16); // R16 contains entry address
            quint64 type = cpu->getRegister(17);    // R17 contains entry type
            cpu->writeSystemEntry(address, type);
        }
        return true;

    case PalOpcodes::Tru64::RDPS:
        DEBUG_LOG("ExecuteStage: PAL RDPS");
        cpu->setRegister(0, cpu->readProcessorStatus()); // Store PS in R0
        return true;

    case PalOpcodes::Tru64::WRKGP:
        DEBUG_LOG("ExecuteStage: PAL WRKGP");
        cpu->writeKGP(cpu->getRegister(16)); // R16 contains KGP value
        return true;

    case PalOpcodes::Tru64::WRUSP:
        DEBUG_LOG("ExecuteStage: PAL WRUSP");
        cpu->writeUSP(cpu->getRegister(16)); // R16 contains USP value
        return true;

    case PalOpcodes::Tru64::WRPERFMON:
        DEBUG_LOG("ExecuteStage: PAL WRPERFMON");
        {
            quint64 function = cpu->getRegister(16); // R16 contains function
            quint64 value = cpu->getRegister(17);    // R17 contains value
            cpu->writePerfMon(function, value);
        }
        return true;

    case PalOpcodes::Tru64::RDUSP:
        DEBUG_LOG("ExecuteStage: PAL RDUSP");
        cpu->setRegister(0, cpu->readUSP()); // Store USP in R0
        return true;

    case PalOpcodes::Tru64::TBI:
        DEBUG_LOG("ExecuteStage: PAL TBI");
        {
            quint64 type = cpu->getRegister(16);    // R16 contains invalidation type
            quint64 address = cpu->getRegister(17); // R17 contains address
            cpu->invalidateTB(type, address);
        }
        return true;

    case PalOpcodes::Tru64::RDVAL:
        DEBUG_LOG("ExecuteStage: PAL RDVAL");
        cpu->setRegister(0, cpu->readVAL()); // Store VAL in R0
        return true;

    case PalOpcodes::Tru64::WRVAL:
        DEBUG_LOG("ExecuteStage: PAL WRVAL");
        cpu->writeVAL(cpu->getRegister(16)); // R16 contains value
        return true;

    case PalOpcodes::Tru64::SWPCTX:
        DEBUG_LOG("ExecuteStage: PAL SWPCTX");
        {
            quint64 newContext = cpu->getRegister(16); // R16 contains new PCB address
            quint64 oldContext = cpu->swapContext(newContext);
            cpu->setRegister(0, oldContext); // Store old context in R0
        }
        return true;

    case PalOpcodes::Tru64::CALLSYS:
        DEBUG_LOG("ExecuteStage: PAL CALLSYS (System Call)");
        // System call handling - pass to OS emulation layer
        cpu->handleSystemCall();
        return true;

    case PalOpcodes::Tru64::IMB:
        DEBUG_LOG("ExecuteStage: PAL IMB (Instruction Memory Barrier)");
        cpu->instructionMemoryBarrier();
        return true;

    default:
        // Unhandled Tru64 PAL instruction
        DEBUG_LOG(QString("ExecuteStage: Unknown Tru64 PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        return false;
    }
#endif // ALPHA_PLATFORM_TRU64

#ifdef ALPHA_PLATFORM_OPENVMS
    // OpenVMS PAL instructions
    switch (palFunction)
    {
    case PalOpcodes::OpenVMS::SWPCTX:
        DEBUG_LOG("ExecuteStage: PAL SWPCTX (OpenVMS)");
        {
            quint64 newContext = cpu->getRegister(16); // R16 contains new PCB address
            quint64 oldContext = cpu->swapContext(newContext);
            cpu->setRegister(0, oldContext); // Store old context in R0
        }
        return true;

    case PalOpcodes::OpenVMS::MFPR_ASN:
        DEBUG_LOG("ExecuteStage: PAL MFPR_ASN");
        cpu->setRegister(0, cpu->readASN());
        return true;

    case PalOpcodes::OpenVMS::MTPR_ASTEN:
        DEBUG_LOG("ExecuteStage: PAL MTPR_ASTEN");
        cpu->writeASTEN(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MTPR_ASTSR:
        DEBUG_LOG("ExecuteStage: PAL MTPR_ASTSR");
        cpu->writeASTSR(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MTPR_IPIR:
        DEBUG_LOG("ExecuteStage: PAL MTPR_IPIR");
        cpu->writeIPIR(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MFPR_IPL:
        DEBUG_LOG("ExecuteStage: PAL MFPR_IPL");
        cpu->setRegister(0, cpu->readIPL());
        return true;

    case PalOpcodes::OpenVMS::MTPR_IPL:
        DEBUG_LOG("ExecuteStage: PAL MTPR_IPL");
        cpu->writeIPL(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MFPR_MCES:
        DEBUG_LOG("ExecuteStage: PAL MFPR_MCES");
        cpu->setRegister(0, cpu->readMCES());
        return true;

    case PalOpcodes::OpenVMS::MTPR_MCES:
        DEBUG_LOG("ExecuteStage: PAL MTPR_MCES");
        cpu->writeMCES(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MFPR_PCBB:
        DEBUG_LOG("ExecuteStage: PAL MFPR_PCBB");
        cpu->setRegister(0, cpu->readPCBB());
        return true;

    case PalOpcodes::OpenVMS::MFPR_PTBR:
        DEBUG_LOG("ExecuteStage: PAL MFPR_PTBR");
        cpu->setRegister(0, cpu->readPTBR());
        return true;

    case PalOpcodes::OpenVMS::MFPR_SCBB:
        DEBUG_LOG("ExecuteStage: PAL MFPR_SCBB");
        cpu->setRegister(0, cpu->readSCBB());
        return true;

    case PalOpcodes::OpenVMS::MTPR_SCBB:
        DEBUG_LOG("ExecuteStage: PAL MTPR_SCBB");
        cpu->writeSCBB(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MTPR_SIRR:
        DEBUG_LOG("ExecuteStage: PAL MTPR_SIRR");
        cpu->writeSIRR(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::MFPR_SISR:
        DEBUG_LOG("ExecuteStage: PAL MFPR_SISR");
        cpu->setRegister(0, cpu->readSISR());
        return true;

    case PalOpcodes::OpenVMS::MTPR_TBIA:
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBIA");
        cpu->invalidateTlb();
        return true;

    case PalOpcodes::OpenVMS::MTPR_TBIAP:
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBIAP");
        cpu->invalidateTlbProcess();
        return true;

    case PalOpcodes::OpenVMS::MTPR_TBIS:
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBIS");
        cpu->invalidateTlbSingle(cpu->getRegister(16));
        return true;

    case PalOpcodes::OpenVMS::CHME:
        DEBUG_LOG("ExecuteStage: PAL CHME (Change Mode to Executive)");
        cpu->changeMode(ProcessorMode::EXECUTIVE);
        return true;

    case PalOpcodes::OpenVMS::CHMS:
        DEBUG_LOG("ExecuteStage: PAL CHMS (Change Mode to Supervisor)");
        cpu->changeMode(ProcessorMode::SUPERVISOR);
        return true;

    case PalOpcodes::OpenVMS::CHMU:
        DEBUG_LOG("ExecuteStage: PAL CHMU (Change Mode to User)");
        cpu->changeMode(ProcessorMode::USER);
        return true;

    default:
        // Unhandled OpenVMS PAL instruction
        DEBUG_LOG(QString("ExecuteStage: Unknown OpenVMS PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        return false;
    }
#endif // ALPHA_PLATFORM_OPENVMS

#ifdef ALPHA_PLATFORM_WINDOWS
    // Windows NT PAL instructions
    switch (palFunction)
    {
    case PalOpcodes::WindowsNT::SWPCTX:
        DEBUG_LOG("ExecuteStage: PAL SWPCTX (Windows NT)");
        {
            quint64 newContext = cpu->getRegister(16); // R16 contains new PCB address
            quint64 oldContext = cpu->swapContext(newContext);
            cpu->setRegister(0, oldContext); // Store old context in R0
        }
        return true;

    case PalOpcodes::WindowsNT::SWPPAL:
        DEBUG_LOG("ExecuteStage: PAL SWPPAL (Windows NT)");
        {
            quint64 newBase = cpu->getRegister(16); // R16 contains new PAL base
            quint64 oldBase = cpu->swapPALBase(newBase);
            cpu->setRegister(0, oldBase); // Store old base in R0
        }
        return true;

    case PalOpcodes::WindowsNT::IMB:
        DEBUG_LOG("ExecuteStage: PAL IMB (Windows NT)");
        cpu->instructionMemoryBarrier();
        return true;

    case PalOpcodes::WindowsNT::RDIRQL:
        DEBUG_LOG("ExecuteStage: PAL RDIRQL (Windows NT)");
        cpu->setRegister(0, cpu->readIRQL()); // Store current IRQL in R0
        return true;

    case PalOpcodes::WindowsNT::SWPIRQL:
        DEBUG_LOG("ExecuteStage: PAL SWPIRQL (Windows NT)");
        {
            quint64 newLevel = cpu->getRegister(16) & 0x1F; // R16 contains new level
            quint64 oldLevel = cpu->swapIRQL(newLevel);
            cpu->setRegister(0, oldLevel); // Store old level in R0
        }
        return true;

    case PalOpcodes::WindowsNT::WRFEN:
        DEBUG_LOG("ExecuteStage: PAL WRFEN (Windows NT)");
        cpu->writeFEN(cpu->getRegister(16) & 1); // R16 contains enable bit
        return true;

    case PalOpcodes::WindowsNT::TBIA:
        DEBUG_LOG("ExecuteStage: PAL TBIA (Windows NT)");
        cpu->invalidateTlb();
        return true;

    case PalOpcodes::WindowsNT::TBIS:
        DEBUG_LOG("ExecuteStage: PAL TBIS (Windows NT)");
        cpu->invalidateTlbSingle(cpu->getRegister(16));
        return true;

    case PalOpcodes::WindowsNT::GENTRAP:
        DEBUG_LOG("ExecuteStage: PAL GENTRAP (Windows NT)");
        cpu->generateTrap(cpu->getRegister(16));
        return true;

    case PalOpcodes::WindowsNT::RDMCES:
        DEBUG_LOG("ExecuteStage: PAL RDMCES (Windows NT)");
        cpu->setRegister(0, cpu->readMCES());
        return true;

    case PalOpcodes::WindowsNT::WRMCES:
        DEBUG_LOG("ExecuteStage: PAL WRMCES (Windows NT)");
        cpu->writeMCES(cpu->getRegister(16));
        return true;

    case PalOpcodes::WindowsNT::DBGSTOP:
        DEBUG_LOG("ExecuteStage: PAL DBGSTOP (Windows NT)");
        cpu->debugStop();
        return true;

    default:
        // Unhandled Windows NT PAL instruction
        DEBUG_LOG(QString("ExecuteStage: Unknown Windows NT PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        return false;
    }
#endif // ALPHA_PLATFORM_WINDOWS

#ifdef ALPHA_PLATFORM_SRM
    // SRM/Linux PAL instructions
    switch (palFunction)
    {
    case PalOpcodes::SRM::SWPCTX:
        DEBUG_LOG("ExecuteStage: PAL SWPCTX (SRM/Linux)");
        {
            quint64 newContext = cpu->getRegister(16); // R16 contains new PCB address
            quint64 oldContext = cpu->swapContext(newContext);
            cpu->setRegister(0, oldContext); // Store old context in R0
        }
        return true;

    case PalOpcodes::SRM::CSERVE:
        DEBUG_LOG("ExecuteStage: PAL CSERVE (SRM/Linux)");
        cpu->executeConsoleService();
        return true;

    case PalOpcodes::SRM::SWPPAL:
        DEBUG_LOG("ExecuteStage: PAL SWPPAL (SRM/Linux)");
        {
            quint64 newBase = cpu->getRegister(16); // R16 contains new PAL base
            quint64 oldBase = cpu->swapPALBase(newBase);
            cpu->setRegister(0, oldBase); // Store old base in R0
        }
        return true;

    case PalOpcodes::SRM::RDIRQL:
        DEBUG_LOG("ExecuteStage: PAL RDIRQL (SRM/Linux)");
        cpu->setRegister(0, cpu->readIRQL()); // Store current IRQL in R0
        return true;

    case PalOpcodes::SRM::SWPIRQL:
        DEBUG_LOG("ExecuteStage: PAL SWPIRQL (SRM/Linux)");
        {
            quint64 newLevel = cpu->getRegister(16) & 0x1F; // R16 contains new level
            quint64 oldLevel = cpu->swapIRQL(newLevel);
            cpu->setRegister(0, oldLevel); // Store old level in R0
        }
        return true;

    case PalOpcodes::SRM::DI:
        DEBUG_LOG("ExecuteStage: PAL DI (SRM/Linux)");
        cpu->disableInterrupts();
        return true;

    case PalOpcodes::SRM::EI:
        DEBUG_LOG("ExecuteStage: PAL EI (SRM/Linux)");
        cpu->enableInterrupts();
        return true;

    case PalOpcodes::SRM::WRKGP:
        DEBUG_LOG("ExecuteStage: PAL WRKGP (SRM/Linux)");
        cpu->writeKGP(cpu->getRegister(16)); // R16 contains KGP value
        return true;

    case PalOpcodes::SRM::WRUSP:
        DEBUG_LOG("ExecuteStage: PAL WRUSP (SRM/Linux)");
        cpu->writeUSP(cpu->getRegister(16)); // R16 contains USP value
        return true;

    case PalOpcodes::SRM::RDUSP:
        DEBUG_LOG("ExecuteStage: PAL RDUSP (SRM/Linux)");
        cpu->setRegister(0, cpu->readUSP()); // Store USP in R0
        return true;

    case PalOpcodes::SRM::TBI:
        DEBUG_LOG("ExecuteStage: PAL TBI (SRM/Linux)");
        {
            quint64 type = cpu->getRegister(16);    // R16 contains invalidation type
            quint64 address = cpu->getRegister(17); // R17 contains address
            cpu->invalidateTB(type, address);
        }
        return true;

    case PalOpcodes::SRM::RDMCES:
        DEBUG_LOG("ExecuteStage: PAL RDMCES (SRM/Linux)");
        cpu->setRegister(0, cpu->readMCES());
        return true;

    case PalOpcodes::SRM::WRMCES:
        DEBUG_LOG("ExecuteStage: PAL WRMCES (SRM/Linux)");
        cpu->writeMCES(cpu->getRegister(16));
        return true;

    case PalOpcodes::SRM::CALLSYS:
        DEBUG_LOG("ExecuteStage: PAL CALLSYS (SRM/Linux)");
        // Linux system call handling
        cpu->handleLinuxSystemCall();
        return true;

    case PalOpcodes::SRM::IMB:
        DEBUG_LOG("ExecuteStage: PAL IMB (SRM/Linux)");
        cpu->instructionMemoryBarrier();
        return true;

    case PalOpcodes::SRM::BPT:
        DEBUG_LOG("ExecuteStage: PAL BPT (SRM/Linux)");
        cpu->handleBreakpoint();
        return true;

    case PalOpcodes::SRM::BUGCHK:
        DEBUG_LOG("ExecuteStage: PAL BUGCHK (SRM/Linux)");
        cpu->handleBugCheck();
        return true;

    default:
        // Unhandled SRM/Linux PAL instruction
        DEBUG_LOG(QString("ExecuteStage: Unknown SRM/Linux PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        return false;
    }
#endif // ALPHA_PLATFORM_SRM

#ifdef ALPHA_PLATFORM_CUSTOM
    // Custom platform PAL instructions
    switch (palFunction)
    {
    case PalOpcodes::Custom::SWPCTX:
        DEBUG_LOG("ExecuteStage: PAL SWPCTX (Custom)");
        // Implement custom context swap
        return true;

    case PalOpcodes::Custom::CALLSYS:
        DEBUG_LOG("ExecuteStage: PAL CALLSYS (Custom)");
        // Implement custom system call
        return true;

    case PalOpcodes::Custom::BPT:
        DEBUG_LOG("ExecuteStage: PAL BPT (Custom)");
        // Implement custom breakpoint
        return true;

    default:
        // Unhandled custom PAL instruction
        DEBUG_LOG(QString("ExecuteStage: Unknown custom PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        return false;
    }
#endif // ALPHA_PLATFORM_CUSTOM

    // If no platform-specific handler executed, we've got an unrecognized PAL instruction
    DEBUG_LOG(QString("ExecuteStage: Unrecognized PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
    return false;
}
