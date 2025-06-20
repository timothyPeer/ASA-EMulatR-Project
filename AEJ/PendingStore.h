#ifndef GlobalLockTracker_h__
#define GlobalLockTracker_h__

// PendingStore.H
// Represents a conditional store (STx_C) that only commits if the
// prior LDx_L reservation is still valid.
//
// Alpha AXP System Ref Man v6, §6.3 “Atomic Operations”:
//   – LDx_L sets a reservation flag + records the lockedPhysicalAddress.
//   – STx_C must fall through, operate-only in between, and
//     must be to the same 16-byte block. :contentReference[oaicite:2]{index=2}:contentReference[oaicite:3]{index=3}
/*
Because the STx_C commit already has a physical address (the reservation recorded lockedPhysicalAddress), we will bypass
VM translation (AlphaSystemMemory) path or risk trapping into page faults or MMIO. What is needed:

“Did my reservation still hold?”
“If so, poke the 64-bit value into DRAM and invalidate other reservations.”

Using with SafeMemory* takes care of clearing other reservations under the hood. 
Passing only SafeMemory* keeps the commit logic focused on physical memory semantics and atomic-operation bookkeeping.

*/

#pragma once
#include "PendingStore.H"
#include <QtGlobal>
#include "AlphaCPUState.h"
#include "SafeMemory.h"
#include "GlobalLockTracker.h"

class PendingStore
{
  public:
    //! Build a pending conditional store for CPU 'cpuId' at 'address'.
    //************************************
    // Method:    PendingStore
    // FullName:  PendingStore::PendingStore
    // Access:    public
    // Returns:
    // Qualifier: : m_cpuId(cpuId), m_address(address), m_value(value)
    // Parameter: AlphaCPUState cpuState -
    // Parameter: int cpuId
    // Parameter: quint64 address
    // Parameter: quint64 value
    //************************************
    PendingStore(AlphaCPUState cpuState, int cpuId, quint64 address, quint64 value)
        : m_cpuId(cpuId), m_address(address), m_value(value)
    {
    }

    void attachCPUState(AlphaCPUState *cpuState) { m_cpuState = cpuState; }
    void attachSafeMemory(SafeMemory *mem_) { m_safeMemory = mem_; }
    //! Returns true if the original LDx_L reservation still holds.
    bool checkLockValidity() const
    {
       
        if (!m_cpuState->lockFlag)
            return false;

        // must be in the same 16-byte block as the LDx_L
        quint64 base = m_cpuState->lockedPhysicalAddress & ~0xFULL;
        if ((m_address & ~0xFULL) != base)
            return false;

        // no other write has invalidated this block
        return !GlobalLockTracker::wasInvalidated(base);
    }

    /*!
     * \brief Attempt the conditional store.
     * \param pc    virtual PC of the STx_C instruction
     * \return true on success (store occurred), false on failure (ra ← 0)
     *
     * Clears the CPU’s reservation flag in all cases, per SRM §6.3.3.
     */
    bool commit( quint64 pc)
    {
       
        bool ok = checkLockValidity();
        m_cpuState->lockFlag = false; // reservation always cleared on STx_C

        if (ok)
        {
            // perform the actual store; this will also
            // invalidate other reservations via SafeMemory logic
            m_safeMemory->writeUInt64(m_address, m_value, pc);
        }
        // the caller (PalExecutor) must write ra ← ok?1:0
        return ok;
    }

  private:
    int m_cpuId;       ///< which CPU issued the STx_C
    AlphaCPUState* m_cpuState;  
    SafeMemory* m_safeMemory; 
    quint64 m_address; ///< physical address to store to
    quint64 m_value;   ///< value to store on success
};
#endif // GlobalLockTracker_h__
