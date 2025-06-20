#include "AlphaProcessContext.h"



 AlphaProcessorContext::AlphaProcessorContext(QSharedPointer<AlphaProcessorStatus> status, int maxStackDepth /*= 1024*/)
    : m_status(status), m_stackManager(std::make_unique<StackManager>(maxStackDepth)), m_programCounter(0),
      m_excbAddress(0), m_synchronousTrapsEnabled(false)
{
    // Initialize registers (R31 and F31 are hardwired to zero)
    memset(m_generalRegisters, 0, sizeof(m_generalRegisters));
    memset(m_floatingRegisters, 0, sizeof(m_floatingRegisters));
}

 quint64 AlphaProcessorContext::getProgramCounter() const override
{
    QReadLocker l(&m_lock);
    return m_pc.get();
}

bool AlphaProcessorContext::isFlagSet(quint64 bitMask, enumFlagDomain domain) const
{
    switch (domain)
    {
    case enumFlagDomain::ProcessorStatus:
        return (m_status && (m_status->raw() & bitMask)) != 0;

    case enumFlagDomain::FloatingPointControl:
        if (auto *fpBank = registerBank() ? registerBank()->getFpBank() : nullptr)
            return (fpBank->fpcr().raw() & bitMask) != 0;
        break;
    }
    return false;
}

void AlphaProcessorContext::setProgramCounter(quint64 v) override
{
    QWriteLocker l(&m_lock);
    m_pc.set(v);
}
quint64 AlphaProcessorContext::getNextInstructionPC() const override
{
    QReadLocker l(&m_lock);
    return m_pc.next();
}
