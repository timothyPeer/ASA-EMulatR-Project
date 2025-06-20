#include <QString>
#include "IprBank.h"
#include "AlphaCPU_refactored.h"
#include "enumerations/enumIPRNumbers.h"

void IprBank::write(IPRNumbers id, quint64 value)
{
    {
        QWriteLocker l(&lock_);
        const int idx = static_cast<int>(id);
        if (regs_[idx] == value)
            return; // no change ? no signal
        regs_[idx] = value;
    }
    emit sigRegisterChanged(id, value);
    // Post-write notifications for specific registers
    handlePostWrite(id, value);
}

Q_INVOKABLE quint64 IprBank::readIpr(quint8 n) const { 
        
 using underlying = std::underlying_type_t<IPRNumbers>;
    if (n > 127)
    {
        return 0;
    }
    return read(static_cast<IPRNumbers>(static_cast<underlying>(IPRNumbers::IPR_IPR0) + n));
    //return read(static_cast<IPRNumbers>(IPRNumbers::IPR_IPR0 + n)); 
 }

Q_INVOKABLE void IprBank::writeIpr(quint8 n, quint64 v)
 {
    using underlying = std::underlying_type_t<IPRNumbers>;
    if (n > 127)
    {
        return ;
    }
    write(static_cast<IPRNumbers>(static_cast<underlying>(IPRNumbers::IPR_IPR0) + n),v);
    // write(static_cast<IPRNumbers>(IPRNumbers::IPR_IPR0 + n), v);
 }

Q_INVOKABLE void IprBank::clear()
 {
     QWriteLocker l(&lock_);
     regs_.fill(0);
     initializeDefaults();
 }

 void IprBank::setCpu(AlphaCPU *cpu_) { m_cpu = cpu_; }

QString IprBank::getRegisterName(IPRNumbers id) 
{
    switch (id)
    {
    // Your existing cases
    case IPRNumbers::IPR_ASN:
        return "ASN";
    case IPRNumbers::IPR_ASTEN:
        return "ASTEN";
    case IPRNumbers::IPR_ESP:
        return "ESP";
    case IPRNumbers::IPR_FEN:
        return "FEN";
    case IPRNumbers::IPR_PTBR:
        return "PTBR";
    case IPRNumbers::IPR_PCBB:
        return "PCBB";
    case IPRNumbers::IPR_PRBR:
        return "PRBR";
    case IPRNumbers::IPR_VPTB:
        return "VPTB";
    case IPRNumbers::IPR_ASTSR:
        return "ASTSR";
    case IPRNumbers::IPR_SIRR:
        return "SIRR";
    case IPRNumbers::IPR_SISR:
        return "SISR";
    case IPRNumbers::IPR_PS:
        return "PS";
    case IPRNumbers::IPR_MCES:
        return "MCES";
    case IPRNumbers::IPR_SCBB:
        return "SCBB";
    case IPRNumbers::IPR_WHAMI:
        return "WHAMI";
    // ... your other existing cases ...

    // ADD THESE NEW CASES FOR EXCEPTION REGISTERS:
    case IPRNumbers::IPR_EXC_PC:
        return "EXC_PC";
    case IPRNumbers::IPR_EXC_PS:
        return "EXC_PS";
    case IPRNumbers::IPR_EXC_SUM:
        return "EXC_SUM";
    case IPRNumbers::IPR_EXC_ADDR:
        return "EXC_ADDR";
    case IPRNumbers::IPR_EXC_MASK:
        return "EXC_MASK";

    // ADD THESE NEW CASES FOR PAL REGISTERS:
    case IPRNumbers::IPR_PAL_TEMP:
        return "PAL_TEMP";
    case IPRNumbers::IPR_IRQL:
        return "IRQL";
    case IPRNumbers::IPR_UNQ:
        return "UNQ";
    case IPRNumbers::IPR_THREAD:
        return "THREAD";
    case IPRNumbers::IPR_PAL_MODE:
        return "PAL_MODE";
    case IPRNumbers::IPR_PAL_BASE:
        return "PAL_BASE";
    case IPRNumbers::IPR_PROCESS:
        return "PROCESS";
    case IPRNumbers::IPR_RESTART_VECTOR:
        return "RESTART_VECTOR";
    case IPRNumbers::IPR_DEBUGGER_VECTOR:
        return "DEBUGGER_VECTOR";

    // ADD THESE FOR PERFORMANCE COUNTERS:
    case IPRNumbers::IPR_PERFMON_0:
        return "PERFMON_0";
    case IPRNumbers::IPR_PERFMON_1:
        return "PERFMON_1";
    case IPRNumbers::IPR_PERFMON_2:
        return "PERFMON_2";
    case IPRNumbers::IPR_PERFMON_3:
        return "PERFMON_3";
    case IPRNumbers::IPR_PERFMON_4:
        return "PERFMON_4";
    case IPRNumbers::IPR_PERFMON_5:
        return "PERFMON_5";
    case IPRNumbers::IPR_PERFMON_6:
        return "PERFMON_6";
    case IPRNumbers::IPR_PERFMON_7:
        return "PERFMON_7";

    // ADD THESE FOR EXCEPTION ENTRY POINTS:
    case IPRNumbers::IPR_ENTRY_0:
        return "ENTRY_0";
    case IPRNumbers::IPR_ENTRY_1:
        return "ENTRY_1";
    case IPRNumbers::IPR_ENTRY_2:
        return "ENTRY_2";
    case IPRNumbers::IPR_ENTRY_3:
        return "ENTRY_3";
    case IPRNumbers::IPR_ENTRY_4:
        return "ENTRY_4";
    case IPRNumbers::IPR_ENTRY_5:
        return "ENTRY_5";
    case IPRNumbers::IPR_ENTRY_6:
        return "ENTRY_6";
    case IPRNumbers::IPR_ENTRY_7:
        return "ENTRY_7";

    // ADD THESE FOR TLB CONTROL:
    case IPRNumbers::IPR_TBIA:
        return "TBIA";
    case IPRNumbers::IPR_TBIAP:
        return "TBIAP";
    case IPRNumbers::IPR_TBIS:
        return "TBIS";
    case IPRNumbers::IPR_TBISD:
        return "TBISD";
    case IPRNumbers::IPR_TBISI:
        return "TBISI";
    case IPRNumbers::IPR_TBCHK:
        return "TBCHK";

    // Your existing default case
    default:
        if (id >= IPRNumbers::IPR_IPR0 && id <= IPRNumbers::IPR_IPR127)
        {
            return QString("IPR%1").arg(static_cast<int>(id) - static_cast<int>(IPRNumbers::IPR_IPR0));
        }
        return QString("UNKNOWN_IPR_%1").arg(static_cast<int>(id));
    }
}

void IprBank::initializeDefaults()
{
    // Stack pointers - reasonable defaults
    regs_[static_cast<int>(IPRNumbers::IPR_USP)] = 0x10000000; // User stack
    regs_[static_cast<int>(IPRNumbers::IPR_ESP)] = 0x20000000; // Executive stack
    regs_[static_cast<int>(IPRNumbers::IPR_SSP)] = 0x30000000; // Supervisor stack
    regs_[static_cast<int>(IPRNumbers::IPR_KSP)] = 0x40000000; // Kernel stack

    // System Control Block Base
    regs_[static_cast<int>(IPRNumbers::IPR_SCBB)] = 0x10000000;

    // Who Am I - will be set by CPU
    regs_[static_cast<int>(IPRNumbers::IPR_WHAMI)] = 0;

    // Interrupt Priority Level
    regs_[static_cast<int>(IPRNumbers::IPR_IPLR)] = 0;

    // Process Status - default to user mode
    regs_[static_cast<int>(IPRNumbers::IPR_PS)] = 0x8;

    // PAL Base - typical Alpha PAL location
    regs_[static_cast<int>(IPRNumbers::IPR_PAL_BASE)] = 0xFFFFFFFF80000000ULL;

    // Exception registers start at 0
    regs_[static_cast<int>(IPRNumbers::IPR_EXC_PC)] = 0;
    regs_[static_cast<int>(IPRNumbers::IPR_EXC_PS)] = 0;
    regs_[static_cast<int>(IPRNumbers::IPR_EXC_SUM)] = 0;
    regs_[static_cast<int>(IPRNumbers::IPR_EXC_ADDR)] = 0;
    regs_[static_cast<int>(IPRNumbers::IPR_EXC_MASK)] = 0;

    // Initialize performance counters
    for (int i = 0; i < 8; ++i)
    {
        regs_[static_cast<int>(IPRNumbers::IPR_PERFMON_0) + i] = 0;
    }

    // Initialize exception entry points to default vectors
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_0)] = 0x8000; // System exception
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_1)] = 0x8100; // Arithmetic exception
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_2)] = 0x8200; // Interrupt exception
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_3)] = 0x8300; // Memory management exception
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_4)] = 0x8400; // Reserved
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_5)] = 0x8500; // Reserved
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_6)] = 0x8600; // Reserved
    regs_[static_cast<int>(IPRNumbers::IPR_ENTRY_7)] = 0x8700; // Reserved
}



bool IprBank::handleSpecialWrite(IPRNumbers id, quint64 value)
{
    switch (id)
    {
    case IPRNumbers::IPR_TBIA:
        // TLB Invalidate All - doesn't store value
        if (m_cpu)
            m_cpu->invalidateTBAll();
        return true;

    case IPRNumbers::IPR_TBIAP:
        // TLB Invalidate All Process
        if (m_cpu)
            m_cpu->invalidateTBAllProcess();
        return true;

    case IPRNumbers::IPR_TBIS:
        // TLB Invalidate Single
        if (m_cpu)
            m_cpu->invalidateTBSingle(value);
        return true;

    case IPRNumbers::IPR_TBISD:
        // TLB Invalidate Single Data
        if (m_cpu)
            m_cpu->invalidateTBSingle(value);
        return true;

    case IPRNumbers::IPR_TBISI:
        // TLB Invalidate Single Instruction
        if (m_cpu)
            m_cpu->getMemorySystem()->invalidate(value);
        return true;

    default:
        return false; // Normal write behavior
    }
}

void IprBank::handlePostWrite(IPRNumbers id, quint64 value)
{
    switch (id)
    {
    // Your existing cases
    case IPRNumbers::IPR_SIRR:
        if (m_cpu)
            m_cpu->checkSoftwareInterrupts();
        break;
    case IPRNumbers::IPR_IPLR: // Note: you had IPLR, mapping to IPL
        if (m_cpu)
            m_cpu->updateInterruptPriority(value & 0x1F);
        break;
    case IPRNumbers::IPR_PS:
        if (m_cpu)
            m_cpu->updateProcessorStatus(value);
        break;

    // ADD THESE NEW POST-WRITE CASES:
    case IPRNumbers::IPR_EXC_PC:
    case IPRNumbers::IPR_EXC_PS:
    case IPRNumbers::IPR_EXC_ADDR:
        // Exception registers changed - update exception state
        if (m_cpu)
        {
            m_cpu->updateExceptionState();
        }
        break;

    case IPRNumbers::IPR_ASN:
        // Address space changed - invalidate TLB entries for old ASN
        if (m_cpu)
        {
            m_cpu->handleASNChange(value);
        }
        break;

    case IPRNumbers::IPR_VPTB:
        // Virtual page table base changed
        if (m_cpu)
        {
            m_cpu->handlePageTableBaseChange(value);
        }
        break;

    case IPRNumbers::IPR_FEN:
        // Floating point enable changed
        if (m_cpu)
        {
            m_cpu->handleFloatingPointEnableChange(value & 1);
        }
        break;

    case IPRNumbers::IPR_PAL_BASE:
        // PAL base address changed
        if (m_cpu)
        {
            m_cpu->handlePALBaseChange(value);
        }
        break;

    // Performance counter writes
    case IPRNumbers::IPR_PERFMON_0:
    case IPRNumbers::IPR_PERFMON_1:
    case IPRNumbers::IPR_PERFMON_2:
    case IPRNumbers::IPR_PERFMON_3:
    case IPRNumbers::IPR_PERFMON_4:
    case IPRNumbers::IPR_PERFMON_5:
    case IPRNumbers::IPR_PERFMON_6:
    case IPRNumbers::IPR_PERFMON_7:
        if (m_cpu)
        {
            int counterNum = static_cast<int>(id) - static_cast<int>(IPRNumbers::IPR_PERFMON_0);
            m_cpu->setPerformanceCounter(counterNum, value);
        }
        break;

    default:
        break;
    }
}
