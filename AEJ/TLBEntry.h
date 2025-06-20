#pragma once

#include "../AEJ/constants/constAlphaMemorySystem.h"
#include <QtGlobal>

class TLBEntry
{
  public:
    TLBEntry()
        : m_virtualPage(0), m_physicalPage(0), m_asn(0), m_protection(0), m_bValid(false), m_dirty(false), m_referenced(false),
          m_granularity(0), m_slotIndex(0), m_lastUsed(0), m_bIsInstructionTLB(false)
    {
    }

    // Allow the compiler to generate default copy?ctor and copy?assignment:
    TLBEntry(const TLBEntry &) = default;
    TLBEntry(TLBEntry &&) noexcept = default;
    TLBEntry &operator=(const TLBEntry &) = default;
    TLBEntry &operator=(TLBEntry &&) noexcept = default;

    // Getters:
    inline bool isValid() const { return m_bValid && (m_protection & AlphaMemoryConstants::TLB_VALID); }
    inline bool isReadable(bool kernelMode = false) const
    {
        if (!isValid())
            return false;
        if ((m_protection & AlphaMemoryConstants::TLB_KERNEL) && !kernelMode)
            return false;
        return true;
    }
    inline bool isWritable() const { return isValid() && (m_protection & AlphaMemoryConstants::TLB_WRITE); }
    inline bool isDirty() const { return m_dirty; }
    inline bool isExecutable() const { return isValid() && (m_protection & AlphaMemoryConstants::TLB_EXEC); }
    inline bool isGlobal() const { return isValid() && (m_protection & AlphaMemoryConstants::TLB_GLOBAL); }
    inline bool isInstructionEntry() const
    {
        // Option 1: If you have separate I-TLB and D-TLB
        if (m_bIsInstructionTLB)
        {
            return true;
        }
        /*
        * Considerations: 
        *  1. VAX Memory Layout (32-bit segmented):
        *    VAX virtual memory is divided into four sections, each 1GB in size: P0 (user process space), P1 (process stack), S0
        *    (operating system), and S1 (reserved) WikipediaHpe. The layout is:

        *    P0 space (0x00000000 - 0x3FFFFFFF): Program region, starts at location 0 and expands toward increasing addresses HP
        *    OpenVMS Programming Concepts Manual P1 space (0x40000000 - 0x7FFFFFFF): Control region, starts at location 7FFFFFFF and
        *    expands toward decreasing addresses HP OpenVMS Programming Concepts Manual S0 space (0x80000000 - 0xBFFFFFFF): System
        *    space, begins at 80000000 and expands toward increasing addresses HP OpenVMS Programming Concepts Manual S1 space
        *    (0xC0000000 - 0xFFFFFFFF): Reserved

        *  2. Alpha and Tru64 Memory Layout (64-bit linear):
        *     Alpha has a 64-bit linear virtual address space with no memory segmentation
        *     No Segmentation: Alpha has no memory segmentation DEC Alpha - Uses canonical addressing where 32-bit addresses are
        *     sign-extended, making certain address patterns predictable Page Granularity: TLBEntry already handles different
        *     page sizes (8KB, 64KB, 4MB, 256MB) which is Alpha-specific.
        */
        // Option 2: VAX-specific memory region analysis
        if (isExecutable())
        {
            // Extract the top 2 bits to determine VAX memory space
            quint32 topBits = (m_virtualPage >> 30) & 0x3;

            switch (topBits)
            {
            case 0: // P0 space (0x00000000 - 0x3FFFFFFF)
                // P0 contains user program code - this is likely instruction space
                // Main executable typically loads in lower P0 addresses
                if (m_virtualPage < 0x10000000)
                {                // Lower 256MB of P0
                    return true; // Main program text segment
                }
                // Higher P0 addresses could be shared libraries or dynamic code
                return true; // Most executable content in P0 is instruction code

            case 1: // P1 space (0x40000000 - 0x7FFFFFFF)
                // P1 is control region (stack, etc.) - executable pages here are rare
                // but could be trampolines, signal handlers, or dynamic code
                return true; // If it's executable in P1, it's likely instructions

            case 2: // S0 space (0x80000000 - 0xBFFFFFFF)
                // S0 is system space - contains OS kernel code
                return true; // Executable S0 pages are definitely instructions

            case 3: // S1 space (0xC0000000 - 0xFFFFFFFF)
                // S1 is reserved on VAX - shouldn't normally have executable pages
                return false;

            default:
                break;
            }
        }

        // Option 3: Fall back to explicitly set value
        return m_isInstructionEntry;
    }
    inline bool isReferenced() const { return m_referenced; }
  
    inline quint64 getAsn() const { return m_asn; }

    inline quint64 getPageSize() const
    {
        switch (m_granularity)
        {
        case 1:
            return AlphaMemoryConstants::PAGE_SIZE_64KB;
        case 2:
            return AlphaMemoryConstants::PAGE_SIZE_4MB;
        case 3:
            return AlphaMemoryConstants::PAGE_SIZE_256MB;
        default:
            return AlphaMemoryConstants::PAGE_SIZE_8KB;
        }
    }
    inline quint64 getPhysicalAddress() const { return m_physicalPage; }
    inline quint64 getVirtualAddress() const { return m_virtualPage; }

    // Setters:
    inline void setVirtualPage(quint64 va) { m_virtualPage = va; }
    inline void setPhysicalPage(quint64 pa) { m_physicalPage = pa; }
    inline void setAsn(quint64 a) { m_asn = a; }
    inline void setProtection(quint32 p) { m_protection = p; }
    inline void setValid(bool v) { m_bValid = v; }
    inline void setReferenced(bool r) { m_referenced = r; }
    inline void setDirty(bool d) { m_dirty = d; }
    inline void setExecutable(bool e) { m_executable = e; }
    inline void setGranularity(quint8 g) { m_granularity = g; }

    // (Optional) slot?index & LRU timestamp if you need them:
    inline void setSlotIndex(quint64 idx) { m_slotIndex = idx; }
    inline quint64 getSlotIndex() const { return m_slotIndex; }

    inline void setLastUsed(quint64 ts) { m_lastUsed = ts; }
    inline quint64 getLastUsed() const { return m_lastUsed; }

    inline void setIsInstructionTLB(bool flag) { m_bIsInstructionTLB = flag; }
    inline bool getIsInstructionTLB() const { return m_bIsInstructionTLB; }
    inline void setIsInstructionEntry(bool flag) { m_isInstructionEntry = flag; }
    inline quint32 getProtection() { return m_protection; }
  private:
    quint64 m_virtualPage;
    quint64 m_physicalPage;
    quint64 m_asn;
    quint32 m_protection;

    bool m_bValid;
    bool m_dirty;
    bool m_referenced;
    bool m_executable;
    bool m_isInstructionEntry = false;
    quint8 m_granularity;

    quint64 m_slotIndex;
    quint64 m_lastUsed;
    bool m_bIsInstructionTLB;
};
