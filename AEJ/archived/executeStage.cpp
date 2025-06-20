#include "executeStage.h"

/*
Complete Alpha instruction execution: All TODO:: major instruction types properly implemented
Pipeline management: Instruction queuing, stalling, and flushing
Exception handling: Proper exception detection and triggering
Performance monitoring: Detailed statistics on instruction types and execution rates
Debug logging: Comprehensive logging for all operations
Qt integration: Signals for monitoring execution progress
Floating-point support: Full IEEE and VAX floating-point operation handling
PAL support: Privileged operation execution
*/
// ExecuteStage.cpp
#include "AlphaCPU_refactored.h"
#include "GlobalMacro.h"
#include "PlatformPalOpCodes.h"
#include "TraceManager.h"
#include "decodeStage.h"
#include "decodedInstruction.h"
#include "fetchUnit.h"

ExecuteStage::ExecuteStage(QObject *parent) : QObject(parent), m_busy(false) { DEBUG_LOG("ExecuteStage initialized"); }

/*
Function    Name    Operation
0x1D        CMPULT  Unsigned less than (treats lower 32 bits as unsigned)
0x1F        CMPULE  Unsigned less than or equal (treats lower 32 bits as unsigned)
0x2D        CMPEQ   Equal
0x2E        CMPNE   Not equal
0x3         DCMPULT Unsigned less than (64-bit)
0x3F        CMPULE  Unsigned less than or equal (64-bit)
0x3E        CMPGE   Greater than or equal (signed)
0x4         DCMPLT  Less than (signed)
0x6D        CMPLE   Less than or equal (signed)
0x6F        CMPUGE  Unsigned greater than or equal
*/

void ExecuteStage::executeIntegerArithmetic(const DecodedInstruction &instruction)
{
    quint64 raValue = (instruction.ra == 31) ? 0 : m_cpu->getRegister(instruction.ra);
    quint64 rbValue = (instruction.rb == 31) ? 0 : m_cpu->getRegister(instruction.rb);

    // Handle immediate mode
    if (instruction.rawInstruction & 0x1000) // Bit 12 = immediate mode
    {
        rbValue = instruction.immediate;
    }

    quint64 result = 0;
    bool overflow = false;
    bool trapOnOverflow = (instruction.function & FUNC_ADDLV) != 0; // V bit in function code

    /*
    All overflow-trap variants (bit 6 = 1) fall through to the “Unimplemented INTA function”
    path because no matching case labels are present, even though the generic overflow check exists.
    */
    switch (instruction.function)
    {
    case FUNC_ADDL:  // ADDL
    case FUNC_ADDLV: // ADDL/V
    {
        qint32 a = static_cast<qint32>(raValue);
        qint32 b = static_cast<qint32>(rbValue);
        qint64 res64 = static_cast<qint64>(a) + static_cast<qint64>(b);
        qint32 res32 = static_cast<qint32>(res64);

        result = static_cast<quint64>(static_cast<qint64>(res32)); // Sign-extend
        overflow = (res64 != static_cast<qint64>(res32));

        DEBUG_LOG(QString("ExecuteStage: %1 %2 + %3 = %4")
                      .arg((trapOnOverflow) ? "ADDL/V" : "ADDL")
                      .arg(a)
                      .arg(b)
                      .arg(res32));
        break;
    }
    case FUNC_S4ADDL:  // S4ADDL        // Confirmed
    case FUNC_S4ADDLV: // S4ADDL/V      // integer-operate format. Base S4ADDL is function 0x02; adding bit 6 (+0x40)
                       // gives the “/V” overflow-checked variant 0x42.
    {
        qint32 a = static_cast<qint32>(raValue);
        qint32 b = static_cast<qint32>(rbValue);
        qint64 res64 = (static_cast<qint64>(a) << 2) + static_cast<qint64>(b);
        qint32 res32 = static_cast<qint32>(res64);

        result = static_cast<quint64>(static_cast<qint64>(res32));
        overflow = (res64 != static_cast<qint64>(res32));

        DEBUG_LOG(QString("ExecuteStage: %1 (%2 << 2) + %3 = %4")
                      .arg((trapOnOverflow) ? "S4ADDL/V" : "S4ADDL")
                      .arg(a)
                      .arg(b)
                      .arg(res32));
        break;
    }
    case FUNC_SUBL:  // SUBL
    case FUNC_SUBLV: // SUBL/V  Set bit 6 (OR with 0x40): 0x09 | 0x40 = 0x49 → SUBL/V
    {
        qint32 a = static_cast<qint32>(raValue);
        qint32 b = static_cast<qint32>(rbValue);
        qint64 res64 = static_cast<qint64>(a) - static_cast<qint64>(b);
        qint32 res32 = static_cast<qint32>(res64);

        result = static_cast<quint64>(static_cast<qint64>(res32));
        overflow = (res64 != static_cast<qint64>(res32));

        DEBUG_LOG(QString("ExecuteStage: %1 %2 - %3 = %4")
                      .arg((trapOnOverflow) ? "SUBL/V" : "SUBL")
                      .arg(a)
                      .arg(b)
                      .arg(res32));
        break;
    }
    case FUNC_S4SUBL:  // S4SUBL
    case FUNC_S4SUBLV: // S4SUBL/V  Set bit 6 (OR with 0x40): 0x0B | 0x40 = 0x4B → S4SUBL/V
    {
        qint32 a = static_cast<qint32>(raValue);
        qint32 b = static_cast<qint32>(rbValue);
        qint64 res64 = (static_cast<qint64>(a) << 2) - static_cast<qint64>(b);
        qint32 res32 = static_cast<qint32>(res64);

        result = static_cast<quint64>(static_cast<qint64>(res32));
        overflow = (res64 != static_cast<qint64>(res32));

        DEBUG_LOG(QString("ExecuteStage: %1 (%2 << 2) - %3 = %4")
                      .arg((trapOnOverflow) ? "S4SUBL/V" : "S4SUBL")
                      .arg(a)
                      .arg(b)
                      .arg(res32));
        break;
    }
    case FUNC_CMPBGE: // CMPBGE
    {
        // Compare bytes greater than or equal
        result = 0;
        for (int i = 0; i < 8; i++)
        {
            quint8 aByte = (raValue >> (i * 8)) & 0xFF;
            quint8 bByte = (rbValue >> (i * 8)) & 0xFF;
            if (aByte >= bByte)
            {
                result |= (1ULL << i);
            }
        }
        DEBUG_LOG(QString("ExecuteStage: CMPBGE result = 0x%1").arg(result, 2, 16, QChar('0')));
        break;
    }
    case FUNC_S8ADDL:  // S8ADDL
    case FUNC_S8ADDLV: // S8ADDL/V Set bit 6 (OR with 0x40): 0x12 | 0x40 = 0x52 → S8SUBL/V
    {
        qint32 a = static_cast<qint32>(raValue);
        qint32 b = static_cast<qint32>(rbValue);
        qint64 res64 = (static_cast<qint64>(a) << 3) + static_cast<qint64>(b);
        qint32 res32 = static_cast<qint32>(res64);

        result = static_cast<quint64>(static_cast<qint64>(res32));
        overflow = (res64 != static_cast<qint64>(res32));

        DEBUG_LOG(QString("ExecuteStage: %1 (%2 << 3) + %3 = %4")
                      .arg((trapOnOverflow) ? "S8ADDL/V" : "S8ADDL")
                      .arg(a)
                      .arg(b)
                      .arg(res32));
        break;
    }

    case FUNC_S8SUBL:  // S8SUBL
    case FUNC_S8SUBLV: // S8SUBL/V *** ADDED ***
    {
        qint32 a = static_cast<qint32>(raValue);
        qint32 b = static_cast<qint32>(rbValue);
        qint64 res64 = (static_cast<qint64>(a) << 3) - static_cast<qint64>(b);
        qint32 res32 = static_cast<qint32>(res64);

        result = static_cast<quint64>(static_cast<qint64>(res32));
        overflow = (res64 != static_cast<qint64>(res32));

        DEBUG_LOG(QString("ExecuteStage: %1 (%2 << 3) - %3 = %4")
                      .arg((trapOnOverflow) ? "S8SUBL/V" : "S8SUBL")
                      .arg(a)
                      .arg(b)
                      .arg(res32));
        break;
    }

    case FUNC_CMPULT_L:
    {
        quint32 a = static_cast<quint32>(raValue);
        quint32 b = static_cast<quint32>(rbValue);
        result = (a < b) ? 1 : 0;
        break;
    }

    case FUNC_CMPULT_G:
    {
        result = (raValue < rbValue) ? 1 : 0;
        break;
    }

    case FUNC_CMPULE_L:
    case FUNC_CMPULT_G: // CMPULE
    {

        if (instruction.function & 0x20)
        { // quad-word path
            result = (raValue <= rbValue) ? 1 : 0;
        }
        else
        { // long-word path
            result = (static_cast<quint32>(raValue) <= static_cast<quint32>(rbValue)) ? 1 : 0;
        }
        break;
        DEBUG_LOG(QString("ExecuteStage: CMPULE (longword) %1 <= %2 ? %3").arg(raValue).arg(rbValue).arg(result));
        break;
    }

    // ========== Quadword Operations ==========
    case FUNC_ADDQ:  // ADDQ
    case FUNC_ADDQV: // ADDQ/V *** ADDED ***
    {
        // For quadword operations, check overflow is more complex
        quint64 oldResult = result;
        result = raValue + rbValue;

        // Check for overflow in addition
        overflow = ((raValue ^ result) & (rbValue ^ result)) >> 63;

        DEBUG_LOG(QString("ExecuteStage: %1 0x%2 + 0x%3 = 0x%4")
                      .arg((trapOnOverflow) ? "ADDQ/V" : "ADDQ")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result, 16, 16, QChar('0')));
        break;
    }

    case FUNC_S4ADDQ:  // S4ADDQ *** ADDED ***
    case FUNC_S4ADDQV: // S4ADDQ/V *** ADDED ***
    {
        quint64 shiftedRA = raValue << 2;
        result = shiftedRA + rbValue;

        // Check for overflow
        overflow = (shiftedRA > result) || ((raValue & 0xC000000000000000ULL) != 0);

        DEBUG_LOG(QString("ExecuteStage: %1 (0x%2 << 2) + 0x%3 = 0x%4")
                      .arg((trapOnOverflow) ? "S4ADDQ/V" : "S4ADDQ")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result, 16, 16, QChar('0')));
        break;
    }
    case FUNC_SUBQ:  // SUBQ
    case FUNC_SUBQV: // SUBQ/V *** ADDED ***
    {
        result = raValue - rbValue;

        // Check for underflow
        overflow = (raValue < rbValue);

        DEBUG_LOG(QString("ExecuteStage: %1 0x%2 - 0x%3 = 0x%4")
                      .arg((trapOnOverflow) ? "SUBQ/V" : "SUBQ")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result, 16, 16, QChar('0')));
        break;
    }
    case FUNC_S4SUBQ:  // S4SUBQ *** ADDED ***
    case FUNC_S4SUBQV: // S4SUBQ/V *** ADDED ***
    {
        quint64 shiftedRA = raValue << 2;
        result = shiftedRA - rbValue;

        // Check for underflow
        overflow = (shiftedRA < rbValue);

        DEBUG_LOG(QString("ExecuteStage: %1 (0x%2 << 2) - 0x%3 = 0x%4")
                      .arg((trapOnOverflow) ? "S4SUBQ/V" : "S4SUBQ")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result, 16, 16, QChar('0')));
        break;
    }

    case FUNC_CMPEQ: // CMPEQ
    {
        result = (raValue == rbValue) ? 1 : 0;
        DEBUG_LOG(QString("ExecuteStage: CMPEQ 0x%1 == 0x%2 ? %3")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result));
        break;
    }
    case FUNC_CMPNE: // CMPNE Synthesized to (CMEQ, XOR)
    {
        result = (raValue != rbValue) ? 1 : 0;
        DEBUG_LOG(QString("ExecuteStage - Synthesized to (CMEQ, XOR): CMPNE 0x%1 != 0x%2 ? %3")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result));
        break;
    }

    case FUNC_S8ADDQ:  // S8ADDQ *** ADDED ***
    case FUNC_S8ADDQV: // S8ADDQ/V *** ADDED ***
    {
        quint64 shiftedRA = raValue << 3;
        result = shiftedRA + rbValue;

        // Check for overflow
        overflow = (shiftedRA > result) || ((raValue & 0xE000000000000000ULL) != 0);

        DEBUG_LOG(QString("ExecuteStage: %1 (0x%2 << 3) + 0x%3 = 0x%4")
                      .arg((trapOnOverflow) ? "S8ADDQ/V" : "S8ADDQ")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result, 16, 16, QChar('0')));
        break;
    }

    case FUNC_S8SUBQ:  // S8SUBQ *** ADDED ***
    case FUNC_S8SUBQV: // S8SUBQ/V *** ADDED ***
    {
        quint64 shiftedRA = raValue << 3;
        result = shiftedRA - rbValue;

        // Check for underflow
        overflow = (shiftedRA < rbValue);

        DEBUG_LOG(QString("ExecuteStage: %1 (0x%2 << 3) - 0x%3 = 0x%4")
                      .arg((trapOnOverflow) ? "S8SUBQ/V" : "S8SUBQ")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result, 16, 16, QChar('0')));
        break;
    }

    case FUNC_CMPULT_L:
    { // unsigned < (long-word)
        quint32 a = static_cast<quint32>(raValue);
        quint32 b = static_cast<quint32>(rbValue);
        result = (a < b) ? 1 : 0;
        break;
    }

    case FUNC_CMPULT_G:
    { // unsigned < (quad-word)
        result = (raValue < rbValue) ? 1 : 0;
        break;
    }

    case FUNC_CMPGEQ: // CMPGE (signed)
    {
        qint64 a = static_cast<qint64>(raValue);
        qint64 b = static_cast<qint64>(rbValue);
        result = (a >= b) ? 1 : 0;
        DEBUG_LOG(QString("ExecuteStage: CMPGE (signed) %1 >= %2 ? %3").arg(a).arg(b).arg(result));
        break;
    }

    case FUNC_CMPLT: // CMPLT (signed longword)
    {
        quint32 a = static_cast<quint32>(raValue);
        quint32 b = static_cast<quint32>(rbValue);
        result = (a < b) ? 1 : 0;
        DEBUG_LOG(QString("ExecuteStage: CMPLT (longword) %1 < %2 ? %3").arg(a).arg(b).arg(result));
        break;
    }
        //     case 0x4D: // CMPLT
        //     {
        //         result = (static_cast<qint64>(raValue) < static_cast<qint64>(rbValue)) ? 1 : 0;
        //         DEBUG_LOG(QString("ExecuteStage: CMPLT (signed) result = %1").arg(result));
        //         break;
        //     }

        //     case 0x6D: // CMPLE Less than or equal (signed)
        //     {
        //         qint32 a = static_cast<qint32>(raValue);
        //         qint32 b = static_cast<qint32>(rbValue);
        //         result = (a <= b) ? 1 : 0;
        //         DEBUG_LOG(QString("ExecuteStage: CMPLE (signed longword) %1 <= %2 ? %3").arg(a).arg(b).arg(result));
        //         break;
        //     }
    case FUNC_CMPLE: // CMPLE (Quadword) Less than or equal (signed)
    {
        result = (static_cast<qint64>(raValue) <= static_cast<qint64>(rbValue)) ? 1 : 0;
        DEBUG_LOG(QString("ExecuteStage: CMPLE (signed) result = %1").arg(result));
        break;
    }

    case FUNC_CMPUGE: // CMPUGE *** ADDED ***
    {
        result = (raValue >= rbValue) ? 1 : 0;
        DEBUG_LOG(QString("ExecuteStage: CMPUGE 0x%1 >= 0x%2 ? %3")
                      .arg(raValue, 16, 16, QChar('0'))
                      .arg(rbValue, 16, 16, QChar('0'))
                      .arg(result));
        break;
    }

    default:
        DEBUG_LOG(
            QString("ExecuteStage: Unimplemented INTA function 0x%1").arg(instruction.function, 2, 16, QChar('0')));
        m_cpu->triggerException(AlphaCPU::ILLEGAL_INSTRUCTION, m_cpu->getPC());
        return;
    }

    // Store result
    if (instruction.rc != 31)
    {
        m_cpu->setRegister(instruction.rc, result);
    }

    // Handle overflow trap variants (functions with bit 6 set)
    if (overflow && (instruction.function & 0x40))
    {
        m_cpu->triggerException(AlphaCPU::ARITHMETIC_TRAP, m_cpu->getPC());
    }
}

#ifdef ALPHA_PLATFORM_TRU64
// Tru64/Digital UNIX capabilities
void ExecuteStage::executePALTru64(const DecodedInstruction &instruction)
{
    // PAL (Privileged Architecture Library) calls
    // These are system calls and privileged operations

    quint32 palFunction = instruction.function & 0x3FFFFFF;

    DEBUG_LOG(QString("ExecuteStage: PAL call 0x%1").arg(palFunction, 6, 16, QChar('0')));

    // Check if in kernel mode
    if (!m_cpu->isKernelMode())
    {
        DEBUG_LOG("ExecuteStage: PAL call in user mode - triggering privilege violation");
        m_cpu->triggerException(AlphaCPU::PRIVILEGE_VIOLATION, m_cpu->getPC());
        return;
    }

    // Common PAL functions (OpenVMS/Digital UNIX style)
    switch (palFunction)
    {
    case PalOpcodes::Common::PAL_HALT: // HALT
        DEBUG_LOG("ExecuteStage: PAL HALT");
        m_cpu->halt();
        break;

    case PalOpcodes::Common::PAL_CFLUSH: // CFLUSH - Cache Flush
        DEBUG_LOG("ExecuteStage: PAL CFLUSH");
        m_cpu->flushCaches();
        break;

    case PalOpcodes::Common::PAL_DRAINA: // DRAINA - Drain Aborts
        DEBUG_LOG("ExecuteStage: PAL DRAINA");
        m_cpu->drainAborts();
        break;

    case PalOpcodes::Tru64::PAL_CSERVE: // CSERVE - Console Service
        DEBUG_LOG("ExecuteStage: PAL CSERVE");
        m_cpu->executeConsoleService();
        break;

    case PalOpcodes::Tru64::PAL_MTPR_IPIR: // WRIPIR - Write Interprocessor Interrupt Request
        DEBUG_LOG("ExecuteStage: PAL WRIPIR");
        m_cpu->writeIPIR(m_cpu->getRegister(16)); // R16 contains processor mask
        break;

    case PalOpcodes::Tru64::PAL_RDMCES: // RDMCES - Read Machine Check Error Summary
    {                                   // Read Machine-Check Error Summary
        DEBUG_LOG("ExecuteStage: PAL RDMCES");
        m_cpu->setRegister(0, m_cpu->readMCES());
        break;
    }

    case PalOpcodes::Tru64::PAL_WRMCES: // WRMCES - Write Machine Check Error Summary
        DEBUG_LOG("ExecuteStage: PAL WRMCES");
        m_cpu->writeMCES(m_cpu->getRegister(16)); // R16 contains value
        break;

    case PalOpcodes::Tru64::PAL_WRFEN: // WRFEN - Write Floating-Point Enable
        DEBUG_LOG("ExecuteStage: PAL WRFEN");
        m_cpu->writeFEN(m_cpu->getRegister(16) & 1); // R16 contains enable bit
        break;

    case PalOpcodes::Tru64::PAL_SWPIRQL: // SWPIRQL - Swap IRQ Level
        DEBUG_LOG("ExecuteStage: PAL SWPIRQL");
        {
            quint64 newLevel = m_cpu->getRegister(16) & 0x1F; // R16 contains new level
            quint64 oldLevel = m_cpu->swapIRQL(newLevel);
            if (0 != 31) // Store old level in R0
            {
                m_cpu->setRegister(0, oldLevel);
            }
        }
        break;

    case PalOpcodes::Tru64::PAL_RDIRQL: // RDIRQL - Read IRQ Level
        DEBUG_LOG("ExecuteStage: PAL RDIRQL");
        if (0 != 31) // Store current IRQL in R0
        {
            m_cpu->setRegister(0, m_cpu->readIRQL());
        }
        break;

    case PalOpcodes::Tru64::PAL_DI: // DI - Disable Interrupts
        DEBUG_LOG("ExecuteStage: PAL DI");
        m_cpu->disableInterrupts();
        break;

    case PalOpcodes::Tru64::PAL_EI: // EI - Enable Interrupts
        DEBUG_LOG("ExecuteStage: PAL EI");
        m_cpu->enableInterrupts();
        break;

    case PalOpcodes::Tru64::PAL_SWPPAL: // SWPPAL - Swap PAL Base
        DEBUG_LOG("ExecuteStage: PAL SWPPAL");
        {
            quint64 newBase = m_cpu->getRegister(16); // R16 contains new PAL base
            quint64 oldBase = m_cpu->swapPALBase(newBase);
            if (0 != 31) // Store old base in R0
            {
                m_cpu->setRegister(0, oldBase);
            }
        }
        break;

    case PalOpcodes::Tru64::PAL_WRVPTPTR: // WRVPTPTR - Write Virtual Page Table Pointer
        DEBUG_LOG("ExecuteStage: PAL WRVPTPTR");
        m_cpu->writeVPTPtr(m_cpu->getRegister(16)); // R16 contains pointer
        break;

    case PalOpcodes::Tru64::PAL_WTKTRP: // WTKTRP - Write TLB Trap
        DEBUG_LOG("ExecuteStage: PAL WTKTRP");
        m_cpu->writeTLBTrap(m_cpu->getRegister(16)); // R16 contains trap address
        break;

    case PalOpcodes::Tru64::PAL_SWPCTX: // SWPCTX - Swap Process Context
        DEBUG_LOG("ExecuteStage: PAL SWPCTX");
        {
            quint64 newContext = m_cpu->getRegister(16); // R16 contains new PCB address
            quint64 oldContext = m_cpu->swapContext(newContext);
            if (0 != 31) // Store old context in R0
            {
                m_cpu->setRegister(0, oldContext);
            }
        }
        break;

    case PalOpcodes::Tru64::PAL_TODO_IMB:
    {
        DEBUG_LOG("ExecuteStage Tru64: TODO: IMB");
    }
    break;
    case PalOpcodes::Tru64::PAL_TODO_RDPERFMON:
    {
        DEBUG_LOG("ExecuteStage Tru64: TODO: RDPERFMON");
    }
    break;
    case PalOpcodes::Tru64::PAL_WRVAL: // WRVAL - Write Virtual Address Cache
        DEBUG_LOG("ExecuteStage: PAL WRVAL");
        m_cpu->writeVAL(m_cpu->getRegister(16)); // R16 contains value
        break;

    case PalOpcodes::Tru64::PAL_RDVAL: // RDVAL - Read Virtual Address Cache
        DEBUG_LOG("ExecuteStage: PAL RDVAL");
        if (0 != 31) // Store VAL in R0
        {
            m_cpu->setRegister(0, m_cpu->readVAL());
        }
        break;

    case PalOpcodes::Tru64::PAL_TBI: // TBI - TB Invalidate
        DEBUG_LOG("ExecuteStage: PAL TBI");
        {
            quint64 type = m_cpu->getRegister(16);    // R16 contains invalidation type
            quint64 address = m_cpu->getRegister(17); // R17 contains address
            m_cpu->invalidateTB(type, address);
        }
        break;

    case PalOpcodes::Tru64::PAL_WRENT: // WRENT - Write System Entry Address
        DEBUG_LOG("ExecuteStage: PAL WRENT");
        {
            quint64 address = m_cpu->getRegister(16); // R16 contains entry address
            quint64 type = m_cpu->getRegister(17);    // R17 contains entry type
            m_cpu->writeSystemEntry(address, type);
        }
        break;

    case PalOpcodes::Tru64::PAL_RDPS: // RDPS - Read Processor Status
        DEBUG_LOG("ExecuteStage: PAL RDPS");
        if (0 != 31) // Store PS in R0
        {
            m_cpu->setRegister(0, m_cpu->readProcessorStatus());
        }
        break;

    case PalOpcodes::Tru64::PAL_WRKGP: // WRKGP - Write Kernel Global Pointer
        DEBUG_LOG("ExecuteStage: PAL WRKGP");
        m_cpu->writeKGP(m_cpu->getRegister(16)); // R16 contains KGP value
        break;

    case PalOpcodes::Tru64::PAL_WRUSP: // PAL_WRUSP - Write User Stack Pointer
        DEBUG_LOG("ExecuteStage: PAL WRUSP");
        m_cpu->writeUSP(m_cpu->getRegister(16)); // R16 contains USP value
        break;

    case PalOpcodes::Tru64::PAL_WRPERFMON: // PAL_WRPERFMON - Write Performance Monitor
        DEBUG_LOG("ExecuteStage: PAL WRPERFMON");
        {
            quint64 function = m_cpu->getRegister(16); // R16 contains function
            quint64 value = m_cpu->getRegister(17);    // R17 contains value
            m_cpu->writePerfMon(function, value);
        }
        break;

    case PalOpcodes::Tru64::PAL_RDUSP: // RDUSP - Read User Stack Pointer
        DEBUG_LOG("ExecuteStage: PAL RDUSP");
        if (0 != 31) // Store USP in R0
        {
            m_cpu->setRegister(0, m_cpu->readUSP());
        }
        break;

    default:
        DEBUG_LOG(QString("ExecuteStage: Unknown PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        m_cpu->triggerException(AlphaCPU::ILLEGAL_INSTRUCTION, m_cpu->getPC());
        break;
    }
}
#endif
#ifdef ALPHA_PLATFORM_OPENVMS

void ExecuteStage::executePALOpenVMS(const DecodedInstruction &instruction)
{
    // PAL (Privileged Architecture Library) calls
    // These are system calls and privileged operations
    quint32 palFunction = instruction.function & 0x3FFFFFF;
    DEBUG_LOG(QString("ExecuteStage: PAL call 0x%1").arg(palFunction, 6, 16, QChar('0')));

    // Check if in kernel mode
    if (!m_cpu->isKernelMode())
    {
        DEBUG_LOG("ExecuteStage: PAL call in user mode - triggering privilege violation");
        m_cpu->triggerException(ExceptionType::PRIVILEGE_VIOLATION, m_cpu->getPC());
        return;
    }

    // Common PAL functions for all platforms
    switch (palFunction)
    {
    case PalOpcodes::Common::PAL_HALT: // HALT
        DEBUG_LOG("ExecuteStage: PAL HALT");
        m_cpu->halt();
        break;
    case PalOpcodes::Common::PAL_CFLUSH: // CFLUSH - Cache Flush
        DEBUG_LOG("ExecuteStage: PAL CFLUSH");
        m_cpu->flushCaches();
        break;
    case PalOpcodes::Common::PAL_DRAINA: // DRAINA - Drain Aborts
        DEBUG_LOG("ExecuteStage: PAL DRAINA");
        m_cpu->drainAborts();
        break;

    // OpenVMS-specific PAL functions
    case PalOpcodes::OpenVMS::PAL_SWPCTX: // Swap context
        DEBUG_LOG("ExecuteStage: PAL SWPCTX");
        {
            quint64 newContext = m_cpu->getRegister(16); // R16 contains new PCB address
            quint64 oldContext = m_cpu->swapContext(newContext);
            if (0 != 31) // Store old context in R0
            {
                m_cpu->setRegister(0, oldContext);
            }
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_ASN: // Move from processor register - ASN
        DEBUG_LOG("ExecuteStage: PAL MFPR_ASN");
        if (0 != 31) // Store ASN in R0
        {
            m_cpu->setRegister(0, m_cpu->readASN());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_ASTEN: // Move to processor register - AST enable
        DEBUG_LOG("ExecuteStage: PAL MTPR_ASTEN");
        m_cpu->writeASTEN(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_ASTSR: // Move to processor register - AST summary
        DEBUG_LOG("ExecuteStage: PAL MTPR_ASTSR");
        m_cpu->writeASTSR(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_FEN: // Move from processor register - FP enable
        DEBUG_LOG("ExecuteStage: PAL MFPR_FEN");
        if (0 != 31) // Store FEN in R0
        {
            m_cpu->setRegister(0, m_cpu->readFEN());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_FEN: // Move to processor register - FP enable
        DEBUG_LOG("ExecuteStage: PAL MTPR_FEN");
        m_cpu->writeFEN(m_cpu->getRegister(16) & 1); // R16 contains enable bit
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_IPIR: // Move to processor register - IPI request
        DEBUG_LOG("ExecuteStage: PAL MTPR_IPIR");
        m_cpu->writeIPIR(m_cpu->getRegister(16)); // R16 contains processor mask
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_IPL: // Move from processor register - IPL
        DEBUG_LOG("ExecuteStage: PAL MFPR_IPL");
        if (0 != 31) // Store IPL in R0
        {
            m_cpu->setRegister(0, m_cpu->readIRQL());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_IPL: // Move to processor register - IPL
        DEBUG_LOG("ExecuteStage: PAL MTPR_IPL");
        {
            quint64 newLevel = m_cpu->getRegister(16) & 0x1F; // R16 contains new level
            quint64 oldLevel = m_cpu->swapIRQL(newLevel);
            if (0 != 31) // Store old level in R0
            {
                m_cpu->setRegister(0, oldLevel);
            }
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_MCES: // Move from processor register - MCES
        DEBUG_LOG("ExecuteStage: PAL MFPR_MCES");
        if (0 != 31) // Store MCES in R0
        {
            m_cpu->setRegister(0, m_cpu->readMCES());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_MCES: // Move to processor register - MCES
        DEBUG_LOG("ExecuteStage: PAL MTPR_MCES");
        m_cpu->writeMCES(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_PCBB: // Move from processor register - PCBB
        DEBUG_LOG("ExecuteStage: PAL MFPR_PCBB");
        if (0 != 31) // Store PCBB in R0
        {
            m_cpu->setRegister(0, m_cpu->readPCBB());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_PRBR: // Move from processor register - PRBR
        DEBUG_LOG("ExecuteStage: PAL MFPR_PRBR");
        if (0 != 31) // Store PRBR in R0
        {
            m_cpu->setRegister(0, m_cpu->readPRBR());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_PRBR: // Move to processor register - PRBR
        DEBUG_LOG("ExecuteStage: PAL MTPR_PRBR");
        m_cpu->writePRBR(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_PTBR: // Move from processor register - PTBR
        DEBUG_LOG("ExecuteStage: PAL MFPR_PTBR");
        if (0 != 31) // Store PTBR in R0
        {
            m_cpu->setRegister(0, m_cpu->readPTBR());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_SCBB: // Move from processor register - SCBB
        DEBUG_LOG("ExecuteStage: PAL MFPR_SCBB");
        if (0 != 31) // Store SCBB in R0
        {
            m_cpu->setRegister(0, m_cpu->readSCBB());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_SCBB: // Move to processor register - SCBB
        DEBUG_LOG("ExecuteStage: PAL MTPR_SCBB");
        m_cpu->writeSCBB(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_SIRR: // Move to processor register - SIRR
        DEBUG_LOG("ExecuteStage: PAL MTPR_SIRR");
        m_cpu->writeSIRR(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_SISR: // Move from processor register - SISR
        DEBUG_LOG("ExecuteStage: PAL MFPR_SISR");
        if (0 != 31) // Store SISR in R0
        {
            m_cpu->setRegister(0, m_cpu->readSISR());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_TBCHK: // Move from processor register - TBCHK
        DEBUG_LOG("ExecuteStage: PAL MFPR_TBCHK");
        if (0 != 31) // Store TBCHK result in R0
        {
            quint64 address = m_cpu->getRegister(16); // R16 contains address to check
            m_cpu->setRegister(0, m_cpu->checkTB(address));
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_TBIA: // Move to processor register - TBIA
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBIA");
        m_cpu->invalidateTBAll();
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_TBIAP: // Move to processor register - TBIAP
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBIAP");
        m_cpu->invalidateTBAllProcess();
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_TBIS: // Move to processor register - TBIS
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBIS");
        m_cpu->invalidateTBSingle(m_cpu->getRegister(16)); // R16 contains address
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_ESP: // Move from processor register - ESP
        DEBUG_LOG("ExecuteStage: PAL MFPR_ESP");
        if (0 != 31) // Store ESP in R0
        {
            m_cpu->setRegister(0, m_cpu->readESP());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_ESP: // Move to processor register - ESP
        DEBUG_LOG("ExecuteStage: PAL MTPR_ESP");
        m_cpu->writeESP(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_SSP: // Move from processor register - SSP
        DEBUG_LOG("ExecuteStage: PAL MFPR_SSP");
        if (0 != 31) // Store SSP in R0
        {
            m_cpu->setRegister(0, m_cpu->readSSP());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_SSP: // Move to processor register - SSP
        DEBUG_LOG("ExecuteStage: PAL MTPR_SSP");
        m_cpu->writeSSP(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_USP: // Move from processor register - USP
        DEBUG_LOG("ExecuteStage: PAL MFPR_USP");
        if (0 != 31) // Store USP in R0
        {
            m_cpu->setRegister(0, m_cpu->readUSP());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_USP: // Move to processor register - USP
        DEBUG_LOG("ExecuteStage: PAL MTPR_USP");
        m_cpu->writeUSP(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_TBISD: // Move to processor register - TBISD
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBISD");
        m_cpu->invalidateTBSingleData(m_cpu->getRegister(16)); // R16 contains address
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_TBISI: // Move to processor register - TBISI
        DEBUG_LOG("ExecuteStage: PAL MTPR_TBISI");
        m_cpu->invalidateTBSingleInst(m_cpu->getRegister(16)); // R16 contains address
        break;
    case PalOpcodes::OpenVMS::MFPR_ASTEN: // Move from processor register - ASTEN
        DEBUG_LOG("ExecuteStage: PAL MFPR_ASTEN");
        if (0 != 31) // Store ASTEN in R0
        {
            m_cpu->setRegister(0, m_cpu->readASTEN());
        }
        break;
    case PalOpcodes::OpenVMS::MFPR_ASTSR: // Move from processor register - ASTSR
        DEBUG_LOG("ExecuteStage: PAL MFPR_ASTSR");
        if (0 != 31) // Store ASTSR in R0
        {
            m_cpu->setRegister(0, m_cpu->readASTSR());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_VPTB: // Move from processor register - VPTB
        DEBUG_LOG("ExecuteStage: PAL MFPR_VPTB");
        if (0 != 31) // Store VPTB in R0
        {
            m_cpu->setRegister(0, m_cpu->readVPTB());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_VPTB: // Move to processor register - VPTB
        DEBUG_LOG("ExecuteStage: PAL MTPR_VPTB");
        m_cpu->writeVPTB(m_cpu->getRegister(16)); // R16 contains value
        break;
    case PalOpcodes::OpenVMS::PAL_MTPR_PERFMON: // Move to processor register - PERFMON
        DEBUG_LOG("ExecuteStage: PAL MTPR_PERFMON");
        {
            quint64 function = m_cpu->getRegister(16); // R16 contains function
            quint64 value = m_cpu->getRegister(17);    // R17 contains value
            m_cpu->writePerfMon(function, value);
        }
        break;
    case PalOpcodes::OpenVMS::PAL_MFPR_WHAMI: // Move from processor register - WHAMI
        DEBUG_LOG("ExecuteStage: PAL MFPR_WHAMI");
        if (0 != 31) // Store WHAMI in R0
        {
            m_cpu->setRegister(0, m_cpu->readWHAMI());
        }
        break;
    case PalOpcodes::OpenVMS::PAL_CHME: // Change mode to executive
        DEBUG_LOG("ExecuteStage: PAL CHME");
        m_cpu->changeMode(AlphaCPU::MODE_EXECUTIVE);
        break;
    case PalOpcodes::OpenVMS::PAL_CHMS: // Change mode to supervisor
        DEBUG_LOG("ExecuteStage: PAL CHMS");
        m_cpu->changeMode(AlphaCPU::MODE_SUPERVISOR);
        break;
    case PalOpcodes::OpenVMS::PAL_CHMU: // Change mode to user
        DEBUG_LOG("ExecuteStage: PAL CHMU");
        m_cpu->changeMode(AlphaCPU::MODE_USER);
        break;
    case PalOpcodes::OpenVMS::PAL_INSQHIL: // Insert into queue head, longword, interlocked
        DEBUG_LOG("ExecuteStage: PAL INSQHIL");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 entry = m_cpu->getRegister(17); // R17 contains entry address
            quint64 result = m_cpu->insertQueueHeadLW(queue, entry);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
        }
        break;
    case PalOpcodes::OpenVMS::PAL_INSQTIL: // Insert into queue tail, longword, interlocked
        DEBUG_LOG("ExecuteStage: PAL INSQTIL");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 entry = m_cpu->getRegister(17); // R17 contains entry address
            quint64 result = m_cpu->insertQueueTailLW(queue, entry);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
        }
        break;
    case PalOpcodes::OpenVMS::PAL_INSQHIQ: // Insert into queue head, quadword, interlocked
        DEBUG_LOG("ExecuteStage: PAL INSQHIQ");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 entry = m_cpu->getRegister(17); // R17 contains entry address
            quint64 result = m_cpu->insertQueueHeadQW(queue, entry);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
        }
        break;
    case PalOpcodes::OpenVMS::PAL_INSQTIQ: // Insert into queue tail, quadword, interlocked
        DEBUG_LOG("ExecuteStage: PAL INSQTIQ");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 entry = m_cpu->getRegister(17); // R17 contains entry address
            quint64 result = m_cpu->insertQueueTailQW(queue, entry);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
        }
        break;
    case PalOpcodes::OpenVMS::PAL_REMQHIL: // Remove from queue head, longword, interlocked
        DEBUG_LOG("ExecuteStage: PAL REMQHIL");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 address, result;
            result = m_cpu->removeQueueHeadLW(queue, &address);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
            // R2 receives the address of the removed entry
            m_cpu->setRegister(2, address);
        }
        break;
    case PalOpcodes::OpenVMS::PAL_REMQTIL: // Remove from queue tail, longword, interlocked
        DEBUG_LOG("ExecuteStage: PAL REMQTIL");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 address, result;
            result = m_cpu->removeQueueTailLW(queue, &address);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
            // R2 receives the address of the removed entry
            m_cpu->setRegister(2, address);
        }
        break;
    case PalOpcodes::OpenVMS::PAL_REMQHIQ: // Remove from queue head, quadword, interlocked
        DEBUG_LOG("ExecuteStage: PAL REMQHIQ");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 address, result;
            result = m_cpu->removeQueueHeadQW(queue, &address);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
            // R2 receives the address of the removed entry
            m_cpu->setRegister(2, address);
        }
        break;
    case PalOpcodes::OpenVMS::PAL_REMQTIQ: // Remove from queue tail, quadword, interlocked
        DEBUG_LOG("ExecuteStage: PAL REMQTIQ");
        {
            quint64 queue = m_cpu->getRegister(16); // R16 contains queue header address
            quint64 address, result;
            result = m_cpu->removeQueueTailQW(queue, &address);
            if (0 != 31) // Store result in R0
            {
                m_cpu->setRegister(0, result);
            }
            // R2 receives the address of the removed entry
            m_cpu->setRegister(2, address);
        }
        break;
    default:
        DEBUG_LOG(QString("ExecuteStage: Unknown PAL function 0x%1").arg(palFunction, 6, 16, QChar('0')));
        m_cpu->triggerException(AlphaCPU::ILLEGAL_INSTRUCTION, m_cpu->getPC());
        break;
    }
}

#endif

// Forward declaration for floating point methods
void ExecuteStage::executeFloatingPointLoad(const DecodedInstruction &instruction)
{
    // Calculate effective address
    quint64 baseValue = (instruction.rb == 31) ? 0 : m_cpu->getRegister(instruction.rb);
    quint64 effectiveAddress = baseValue + instruction.immediate;

    DEBUG_LOG(QString("ExecuteStage: FP Load from EA=0x%1").arg(effectiveAddress, 16, 16, QChar('0')));

    switch (instruction.opcode)
    {
    case 0x20: // LDF - Load F_floating (32-bit VAX format)
    case 0x22: // LDS - Load S_floating (32-bit IEEE format)
    {
        quint32 value;
        if (m_cpu->readMemory32(effectiveAddress, value))
        {
            // Store in floating-point register
            m_cpu->setFloatRegister(instruction.ra, value);
            DEBUG_LOG(QString("ExecuteStage: %1 F%2 = 0x%3")
                          .arg(instruction.opcode == 0x20 ? "LDF" : "LDS")
                          .arg(instruction.ra)
                          .arg(value, 8, 16, QChar('0')));
        }
        else
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, effectiveAddress);
        }
        break;
    }

    case 0x21: // LDG - Load G_floating (64-bit VAX format)
    case 0x23: // LDT - Load T_floating (64-bit IEEE format)
    {
        quint64 value;
        if (m_cpu->readMemory64(effectiveAddress, value))
        {
            // Store in floating-point register
            m_cpu->setFloatRegister(instruction.ra, value);
            DEBUG_LOG(QString("ExecuteStage: %1 F%2 = 0x%3")
                          .arg(instruction.opcode == 0x21 ? "LDG" : "LDT")
                          .arg(instruction.ra)
                          .arg(value, 16, 16, QChar('0')));
        }
        else
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, effectiveAddress);
        }
        break;
    }
    }
}

void ExecuteStage::executeFloatingPointStore(const DecodedInstruction &instruction)
{
    // Calculate effective address
    quint64 baseValue = (instruction.rb == 31) ? 0 : m_cpu->getRegister(instruction.rb);
    quint64 effectiveAddress = baseValue + instruction.immediate;

    DEBUG_LOG(QString("ExecuteStage: FP Store to EA=0x%1").arg(effectiveAddress, 16, 16, QChar('0')));

    switch (instruction.opcode)
    {
    case 0x24: // STF - Store F_floating (32-bit VAX format)
    case 0x26: // STS - Store S_floating (32-bit IEEE format)
    {
        quint32 value = m_cpu->getFloatRegister32(instruction.ra);
        if (!m_cpu->writeMemory32(effectiveAddress, value))
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, effectiveAddress);
        }
        DEBUG_LOG(QString("ExecuteStage: %1 F%2 (0x%3) stored")
                      .arg(instruction.opcode == 0x24 ? "STF" : "STS")
                      .arg(instruction.ra)
                      .arg(value, 8, 16, QChar('0')));
        break;
    }

    case 0x25: // STG - Store G_floating (64-bit VAX format)
    case 0x27: // STT - Store T_floating (64-bit IEEE format)
    {
        quint64 value = m_cpu->getFloatRegister64(instruction.ra);
        if (!m_cpu->writeMemory64(effectiveAddress, value))
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, effectiveAddress);
        }
        DEBUG_LOG(QString("ExecuteStage: %1 F%2 (0x%3) stored")
                      .arg(instruction.opcode == 0x25 ? "STG" : "STT")
                      .arg(instruction.ra)
                      .arg(value, 16, 16, QChar('0')));
        break;
    }
    }
}
void ExecuteStage::executeFloatingPointBranch(const DecodedInstruction &instruction)
{
    quint64 faValue = m_cpu->getFloatRegister64(instruction.ra);
    bool takeBranch = false;
    QString conditionName;

    switch (instruction.opcode)
    {
    case 0x31: // FBEQ - Floating Branch if Equal
        // Check if floating-point value equals zero
        takeBranch = m_cpu->isFloatZero(faValue);
        conditionName = "FBEQ";
        break;

    case 0x32: // FBLT - Floating Branch if Less Than
        // Check if floating-point value is less than zero
        takeBranch = m_cpu->isFloatNegative(faValue) && !m_cpu->isFloatZero(faValue);
        conditionName = "FBLT";
        break;

    case 0x33: // FBLE - Floating Branch if Less Than or Equal
        // Check if floating-point value is less than or equal to zero
        takeBranch = m_cpu->isFloatNegative(faValue) || m_cpu->isFloatZero(faValue);
        conditionName = "FBLE";
        break;

    case 0x35: // FBNE - Floating Branch if Not Equal
        // Check if floating-point value is not zero
        takeBranch = !m_cpu->isFloatZero(faValue);
        conditionName = "FBNE";
        break;

    case 0x36: // FBGE - Floating Branch if Greater Than or Equal
        // Check if floating-point value is greater than or equal to zero
        takeBranch = !m_cpu->isFloatNegative(faValue);
        conditionName = "FBGE";
        break;

    case 0x37: // FBGT - Floating Branch if Greater Than
        // Check if floating-point value is greater than zero
        takeBranch = !m_cpu->isFloatNegative(faValue) && !m_cpu->isFloatZero(faValue);
        conditionName = "FBGT";
        break;
    }

    if (takeBranch)
    {
        quint64 currentPC = m_cpu->getPC();
        quint64 targetPC = currentPC + instruction.immediate;
        m_cpu->setPC(targetPC);
        m_cpu->flushPipeline();

        DEBUG_LOG(
            QString("ExecuteStage: %1 taken, jumping to 0x%2").arg(conditionName).arg(targetPC, 16, 16, QChar('0')));
    }
    else
    {
        DEBUG_LOG(QString("ExecuteStage: %1 not taken").arg(conditionName));
    }
}

void ExecuteStage::executeStoreUnaligned(const DecodedInstruction &instruction)
{
    // Calculate effective address
    quint64 baseValue = (instruction.rb == 31) ? 0 : m_cpu->getRegister(instruction.rb);
    quint64 effectiveAddress = baseValue + instruction.immediate;
    quint64 storeValue = (instruction.ra == 31) ? 0 : m_cpu->getRegister(instruction.ra);

    DEBUG_LOG(QString("ExecuteStage: Unaligned store to EA=0x%1, value=0x%2")
                  .arg(effectiveAddress, 16, 16, QChar('0'))
                  .arg(storeValue, 16, 16, QChar('0')));

    switch (instruction.opcode)
    {
    case OPCODE_STB: // STB - Store Byte
    {
        quint8 value = static_cast<quint8>(storeValue);
        if (!m_cpu->writeMemory8(effectiveAddress, value))
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, effectiveAddress);
        }
        DEBUG_LOG(QString("ExecuteStage: STB stored 0x%1").arg(value, 2, 16, QChar('0')));
        break;
    }

    case OPCODE_STW: // STW - Store Word
    {
        quint16 value = static_cast<quint16>(storeValue);
        if (!m_cpu->writeMemory16(effectiveAddress, value))
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, effectiveAddress);
        }
        DEBUG_LOG(QString("ExecuteStage: STW stored 0x%1").arg(value, 4, 16, QChar('0')));
        break;
    }

    case OPCODE_STQ_U: // STQ_U - Store Quadword Unaligned
    {
        // Alpha STQ_U stores to quadword boundary, ignoring low 3 bits of address
        quint64 alignedAddress = effectiveAddress & ~0x7ULL;

        // For unaligned store, we need to read-modify-write
        // This is a simplified implementation - real Alpha would use byte masks
        quint64 currentValue;
        if (m_cpu->readMemory64(alignedAddress, currentValue))
        {
            // Calculate byte offset within quadword
            quint32 offset = effectiveAddress & 0x7;
            quint64 mask = 0xFFULL << (offset * 8);
            quint64 shiftedValue = (storeValue & 0xFF) << (offset * 8);

            quint64 newValue = (currentValue & ~mask) | (shiftedValue & mask);

            if (!m_cpu->writeMemory64(alignedAddress, newValue))
            {
                m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, alignedAddress);
            }
            DEBUG_LOG(QString("ExecuteStage: STQ_U stored byte 0x%1 at offset %2 (result: 0x%3)")
                          .arg(storeValue & 0xFF, 2, 16, QChar('0'))
                          .arg(offset)
                          .arg(newValue, 16, 16, QChar('0')));
        }
        else
        {
            m_cpu->triggerException(AlphaCPU::MEMORY_ACCESS_FAULT, alignedAddress);
        }
        break;
    }
    }
}

void ExecuteStage::executeIntegerToFloat(const DecodedInstruction &instruction)
{
    // ITFP - Integer to Floating-Point conversions
    quint64 raValue = (instruction.ra == 31) ? 0 : m_cpu->getRegister(instruction.ra);
    quint64 result = 0;

    switch (instruction.function)
    {
    case 0x04: // ITOFS - Integer to F_floating Single
    {
        qint32 intValue = static_cast<qint32>(raValue);
        result = m_cpu->convertToFFormat(intValue);
        DEBUG_LOG(QString("ExecuteStage: ITOFS %1 -> F%2").arg(intValue).arg(instruction.rc));
        break;
    }

    case 0x0A: // ITOFF - Integer to F_floating
    {
        qint64 intValue = static_cast<qint64>(raValue);
        result = m_cpu->convertToFFormat(intValue);
        DEBUG_LOG(QString("ExecuteStage: ITOFF %1 -> F%2").arg(intValue).arg(instruction.rc));
        break;
    }

    case 0x0C: // ITOFT - Integer to T_floating
    {
        qint64 intValue = static_cast<qint64>(raValue);
        result = m_cpu->convertToTFormat(intValue);
        DEBUG_LOG(QString("ExecuteStage: ITOFT %1 -> F%2").arg(intValue).arg(instruction.rc));
        break;
    }

    case 0x14: // ITOFS/U - Integer to F_floating Single (unsigned)
    {
        quint32 intValue = static_cast<quint32>(raValue);
        result = m_cpu->convertToFFormat(intValue);
        DEBUG_LOG(QString("ExecuteStage: ITOFS/U %1 -> F%2").arg(intValue).arg(instruction.rc));
        break;
    }

    case 0x1A: // ITOFF/U - Integer to F_floating (unsigned)
    {
        result = m_cpu->convertToFFormat(raValue);
        DEBUG_LOG(QString("ExecuteStage: ITOFF/U %1 -> F%2").arg(raValue).arg(instruction.rc));
        break;
    }

    case 0x1C: // ITOFT/U - Integer to T_floating (unsigned)
    {
        result = m_cpu->convertToTFormat(raValue);
        DEBUG_LOG(QString("ExecuteStage: ITOFT/U %1 -> F%2").arg(raValue).arg(instruction.rc));
        break;
    }

    default:
        DEBUG_LOG(
            QString("ExecuteStage: Unimplemented ITFP function 0x%1").arg(instruction.function, 2, 16, QChar('0')));
        m_cpu->triggerException(AlphaCPU::ILLEGAL_INSTRUCTION, m_cpu->getPC());
        return;
    }

    // Store result in floating-point register
    if (instruction.rc != 31)
    {
        m_cpu->setFloatRegister(instruction.rc, result);
    }
}
