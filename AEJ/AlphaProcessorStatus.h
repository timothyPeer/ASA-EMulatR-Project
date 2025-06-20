#pragma once
#include <QtCore>
#include "enumerations/enumProcessorMode.h"
#include "constants/const_conditionCodes.h"
#include "helpers/calculateConditionCodes.h"
#include "enumerations/enumFlags.h"
#include "enumerations/enumProcessorStatus.h"


class AlphaProcessorStatus
{

	quint64 rawPS_;      // only bits 0–2 used for IPL
    ProcessorMode mode_; // stored separately
    bool palMode_;
    ProcessorStatusFlags flags_;
    quint64 statusWord_ = 0; ///< Internal representation of 64-bit PS register

    public:
    // ctor guarantees mask & default state
    AlphaProcessorStatus(quint8 initialIPL = 0)
    {
    }

    void attachAlphaProcessorStatus(AlphaProcessorStatus *procStatus) { m_status = procStatus; }
    /*   ProcessorStatusFlags calculateConditionCodes(qint64 result, qint64 op1, qint64 op2,*/

     /// Check if CPU may take interrupts (EI && not IPL masked)
    bool canTakeInterrupt(quint8 currentIPL) const
    {
        bool ei = getFlag(enumProcessorStatus::PS_FLAG_INT_ENABLE);
        quint8 ipl = static_cast<quint8>((statusWord_ & PS_FLAG_IPL_MASK) >> 1);
        return ei && (currentIPL <= ipl);
    }

    /// Enter PALmode (for trap handler)
    void enterPALMode() { setFlag(enumProcessorStatus::PS_FLAG_SUPERVISOR_MODE, true); }

    /// Return true if specified status flag is set
    bool getFlag(enumProcessorStatus flag) const { return (statusWord_ & static_cast<quint64>(flag)) != 0; }

    // IPL access
    inline quint8 getIPL() const { return rawPS_ & 0x7; }

    /// Query PAL mode (supervisor enabled)
    bool isPALModeActive() const { return getFlag(enumProcessorStatus::PS_FLAG_SUPERVISOR_MODE); }

    /// Check if processor status is consistent
    bool isValidState() const
    {
        // TODO

        // For now, assume statusWord_ is always valid
        // Future: verify against illegal combinations or reserved bits
        return true;
    }

    /// Return full status word (raw access)
    quint64 raw() const { return statusWord_; }

    /// Save status register on exception trap
    void saveForException(quint64 trapVector)
    {

        // TODO
        //  In a full PAL model, this would save PS to shadow registers or stack.
        //  For now, store to internal vector to signal exception source (debug only).
        Q_UNUSED(trapVector);
        // Extend with save-to-stack or interrupt frame if needed
    }

    quint64 saveForException() const
    {
        return statusWord_; // or a copy with trap bits cleared if needed
    }

    /// Replace full status word (raw access)
    void setRaw(quint64 value) { statusWord_ = value; }

     /// Set or clear a status flag
    void setFlag(enumProcessorStatus flag, bool enable)
    {
        if (enable)
            statusWord_ |= static_cast<quint64>(flag);
        else
            statusWord_ &= ~static_cast<quint64>(flag);
    }
    bool isFlagSet(quint64 bitMask, enumFlagDomain domain) const
    {
        switch (domain)
        {
        case enumFlagDomain::ProcessorStatus:
            if (m_status)
                return (m_status->raw() & bitMask) != 0;
            break;

        case enumFlagDomain::FloatingPointControl:
        {
            quint64 tmp = m_regBank->getFpBank()->fpcr().raw();
            return (tmp & bitMask) != 0;
        }
        }

        return false; // If status_ is null or domain is invalid
    }
    inline void setIPL(quint8 ipl) { rawPS_ = (rawPS_ & ~0x7) | (ipl & 0x7); }

    // Mode access
    inline ProcessorMode mode() const { return mode_; }
    inline void setMode(ProcessorMode m) { mode_ = m; }

    // PAL mode
    inline bool isPAL() const { return palMode_; }
    inline void enterPAL()
    {
        palMode_ = true;
        setIPL(7);
        setMode(ProcessorMode::MODE_KERNEL);
    }
    inline void exitPAL() { palMode_ = false; /* restore state via saved PS */ }

    // Condition codes
    inline void updateCC(qint64 result, qint64 op1, qint64 op2, bool sub = false)
    {
        flags_ = calculateConditionCodes(result, op1, op2, sub);
        // mirror into rawPS_ only if you want PS bits updated:
        rawPS_ = (rawPS_ & ~PS_ZNVC_MASK) | (flags_.zero << PS_Z_BIT) | (flags_.negative << PS_N_BIT) |
                 (flags_.overflow << PS_V_BIT) | (flags_.carry << PS_C_BIT);
    }
    inline ProcessorStatusFlags getFlags() const { return flags_; }
    inline ProcessorStatusFlags flags() const { return flags_; }

};


