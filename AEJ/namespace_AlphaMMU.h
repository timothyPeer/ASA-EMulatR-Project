#pragma once
#include "GlobalMacro.h"
#include "namespace_SystemRegisters.h"
#include "TLBEntry.h"
#include <QAtomicInteger>
#include <QObject>
#include <memory>

namespace AlphaMMU
{

/**
 * @brief Alpha AXP Page Table Walker - Hardware-accurate implementation
 *
 * Implements Alpha's 3-level page table structure with proper privilege
 * checking and granularity hint support. Integrates with register system
 * for PTBR/ASN/mode changes.
 */
class AlphaPageTableWalker : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Page table entry structure matching Alpha hardware
     */
    struct PageTableEntry
    {
        union
        {
            quint64 raw;
            struct
            {
                quint64 valid : 1;            ///< Valid bit
                quint64 fault_on_read : 1;    ///< Fault on read
                quint64 fault_on_write : 1;   ///< Fault on write
                quint64 fault_on_execute : 1; ///< Fault on execute
                quint64 asm_bit : 1;          ///< ASM bit (for software use)
                quint64 granularity : 2;      ///< Page granularity hint (0=8KB, 1=64KB, 2=4MB, 3=256MB)
                quint64 reserved : 1;         ///< Reserved bit
                quint64 pfn : 32;             ///< Page frame number (bits 8-39)
                quint64 software : 16;        ///< Software-defined bits (40-55)
                quint64 reserved2 : 8;        ///< Reserved (bits 56-63)
            } fields;
        };

        PageTableEntry() : raw(0) {}
        explicit PageTableEntry(quint64 value) : raw(value) {}

        inline bool isValid() const { return fields.valid; }
        inline bool canRead() const { return !fields.fault_on_read; }
        inline bool canWrite() const { return !fields.fault_on_write; }
        inline bool canExecute() const { return !fields.fault_on_execute; }
        inline quint32 getPageFrameNumber() const { return static_cast<quint32>(fields.pfn); }
        inline quint8 getGranularity() const { return static_cast<quint8>(fields.granularity); }

        inline quint64 getPhysicalAddress() const
        {
            return static_cast<quint64>(fields.pfn) << 13; // PFN is in 8KB units
        }
    };

    /**
     * @brief Translation result with detailed fault information
     */
    struct TranslationResult
    {
        bool success;
        quint64 physicalAddress;
        quint8 granularity;
        bool readable;
        bool writable;
        bool executable;
        QString faultReason;

        TranslationResult()
            : success(false), physicalAddress(0), granularity(0), readable(false), writable(false), executable(false)
        {
        }
    };

    explicit AlphaPageTableWalker(QObject *parent = nullptr);
    ~AlphaPageTableWalker() = default;

    // Integration with register system
    bool initialize();
    void initialize_SignalsAndSlots();

    // Register system integration
    template <typename RegisterCollection> void attachRegisterCollection(RegisterCollection *regCollection)
    {
        m_registerCollection = regCollection;

        // Get register handles for fast access
        m_ptbrHandle = regCollection->template getRegister<SystemRegisters::PtbrRegister>();
        m_psHandle = regCollection->template getRegister<SystemRegisters::ProcessorStatusRegister>();

        DEBUG_LOG("PageTableWalker: Attached to register collection");
    }

    /**
     * @brief High-performance page table walk
     * @param virtualAddress Virtual address to translate
     * @param asn Address Space Number
     * @param accessType Access type flags (read/write/execute)
     * @param cpuMode Current CPU privilege mode
     * @return Translation result with physical address or fault information
     */
    TranslationResult translateAddress(quint64 virtualAddress, quint64 asn, quint32 accessType, quint32 cpuMode);

    /**
     * @brief Fast path for instruction fetches (most common case)
     * @param virtualAddress Virtual address to translate
     * @param asn Address Space Number
     * @return Physical address on success, 0 on fault
     */
    inline quint64 fastInstructionTranslate(quint64 virtualAddress, quint64 asn)
    {
        // Fast path with minimal checks for instruction fetch
        quint32 mode = getCurrentMode();
        auto result = translateAddress(virtualAddress, asn, ACCESS_EXECUTE, mode);
        return result.success ? result.physicalAddress : 0;
    }

    // Memory interface for page table reads
    void setMemoryInterface(std::function<bool(quint64, quint64 &)> readFunc) { m_memoryRead = readFunc; }

    // Access type constants
    static constexpr quint32 ACCESS_READ = 0x1;
    static constexpr quint32 ACCESS_WRITE = 0x2;
    static constexpr quint32 ACCESS_EXECUTE = 0x4;

  signals:
    void sigTranslationFault(quint64 virtualAddress, quint64 asn, const QString &reason);
    void sigPageTableAccess(quint64 physicalAddress, int level);

  private:
    // Register system integration
    void *m_registerCollection{nullptr};
    void *m_ptbrHandle{nullptr};
    void *m_psHandle{nullptr};

    // Memory interface
    std::function<bool(quint64, quint64 &)> m_memoryRead;

    // Performance counters
    QAtomicInteger<quint64> m_translationCount{0};
    QAtomicInteger<quint64> m_faultCount{0};
    QAtomicInteger<quint64> m_level1Hits{0};
    QAtomicInteger<quint64> m_level2Hits{0};
    QAtomicInteger<quint64> m_level3Hits{0};

    // Helper methods
    inline quint32 getCurrentMode() const
    {
        // Fast register access through handle
        if (m_psHandle)
        {
            auto *psReg =
                static_cast<SystemRegisters::RegisterHandle<SystemRegisters::ProcessorStatusRegister> *>(m_psHandle);
            return static_cast<quint32>(**psReg & SystemRegisters::ProcessorStatusRegister::PS_MODE_MASK);
        }
        return 0; // Default to kernel mode
    }

    inline quint64 getPageTableBase() const
    {
        if (m_ptbrHandle)
        {
            auto *ptbrReg = static_cast<SystemRegisters::RegisterHandle<SystemRegisters::PtbrRegister> *>(m_ptbrHandle);
            return (**ptbrReg) & SystemRegisters::PtbrRegister::PTBR_BASE_MASK;
        }
        return 0;
    }

    // Alpha page table structure helpers
    inline quint64 getLevel1Index(quint64 va) const { return (va >> 33) & 0x3FF; }
    inline quint64 getLevel2Index(quint64 va) const { return (va >> 23) & 0x3FF; }
    inline quint64 getLevel3Index(quint64 va) const { return (va >> 13) & 0x3FF; }
    inline quint64 getPageOffset(quint64 va, quint8 granularity) const
    {
        static const quint64 offsetMasks[] = {
            0x1FFF,   // 8KB pages
            0xFFFF,   // 64KB pages
            0x3FFFFF, // 4MB pages
            0xFFFFFFF // 256MB pages
        };
        return va & offsetMasks[granularity & 3];
    }

    // Page table walking implementation
    bool readPageTableEntry(quint64 address, PageTableEntry &entry);
    bool checkPrivileges(const PageTableEntry &entry, quint32 accessType, quint32 cpuMode);
    TranslationResult walkPageTable(quint64 virtualAddress, quint64 asn, quint32 accessType, quint32 cpuMode);
};

/**
 * @brief Optimized TLB integration with page table walker
 */
class TLBPageTableIntegration : public QObject
{
    Q_OBJECT

  public:
    explicit TLBPageTableIntegration(class TLBSystem *tlbSystem, AlphaPageTableWalker *pageWalker,
                                     QObject *parent = nullptr);

    /**
     * @brief High-performance address translation with TLB caching
     * @param cpuId CPU making the request
     * @param virtualAddress Virtual address to translate
     * @param asn Address Space Number
     * @param accessType Access type (read/write/execute)
     * @return Physical address on success, 0 on fault
     */
    quint64 translateWithTLB(quint16 cpuId, quint64 virtualAddress, quint64 asn, quint32 accessType);

    /**
     * @brief Handle TLB miss with page table walk and insertion
     */
    bool handleTLBMiss(quint16 cpuId, quint64 virtualAddress, quint64 asn, quint32 accessType);

    // Performance optimization for instruction fetch hot path
    quint64 fastInstructionTranslateWithTLB(quint16 cpuId, quint64 virtualAddress, quint64 asn)
    {
        // Try TLB first
        quint64 physAddr = m_tlbSystem->checkTB(cpuId, virtualAddress, asn, false);
        if (physAddr)
        {
            return physAddr;
        }

        // TLB miss - do page table walk
        return m_pageWalker->fastInstructionTranslate(virtualAddress, asn);
    }

  private slots:
    void onTranslationFault(quint64 virtualAddress, quint64 asn, const QString &reason);

  private:
    TLBSystem *m_tlbSystem;
    AlphaPageTableWalker *m_pageWalker;

    // Statistics
    QAtomicInteger<quint64> m_tlbHits{0};
    QAtomicInteger<quint64> m_tlbMisses{0};
    QAtomicInteger<quint64> m_pageTableWalks{0};
};

} // namespace AlphaMMU