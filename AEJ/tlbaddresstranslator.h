#pragma once

#include "GlobalMacro.h"
#include "utilitySafeIncrement.h"
#include <QtCore/QAtomicInteger>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QtAlgorithms>

/*
tlbAddressTranslator provides the foundational address translation mechanics that the higher-level TLB system will use
to coordinate with the collision detector and state manager.
*/
class tlbAddressTranslator : public QObject
{
    Q_OBJECT

  public:
    enum TranslationResult
    {
        TRANSLATION_HIT = 0,
        TRANSLATION_MISS,
        TRANSLATION_FAULT,
        TRANSLATION_PROTECTION_VIOLATION,
        TRANSLATION_INVALID_ADDRESS
    };

    enum AddressingMode
    {
        DIRECT_MAPPED = 0,
        SET_ASSOCIATIVE,
        FULLY_ASSOCIATIVE
    };

    struct TranslationRequest
    {
        quint64 virtualAddress;
        quint32 processId;
        bool isLoad;
        bool isStore;
        bool isExecute;
        quint64 requestTimestamp;

        TranslationRequest()
            : virtualAddress(0), processId(0), isLoad(false), isStore(false), isExecute(false), requestTimestamp(0)
        {
        }
    };

    struct TranslationResponse
    {
        TranslationResult result;
        quint64 physicalAddress;
        quint32 tbIndex;
        quint64 virtualTag;
        bool hitInTlb;
        quint64 translationTime;

        TranslationResponse()
            : result(TRANSLATION_FAULT), physicalAddress(0), tbIndex(0), virtualTag(0), hitInTlb(false),
              translationTime(0)
        {
        }
    };

  private:
    static const int TLB_SIZE = 64;
    static const int PAGE_SIZE = 4096;                  // 4KB pages
    static const int PAGE_OFFSET_BITS = 12;             // log2(4096)
    static const quint64 PAGE_OFFSET_MASK = 0xFFF;     // Lower 12 bits
    static const quint64 VPN_MASK = ~PAGE_OFFSET_MASK;  // Upper bits for VPN

    AddressingMode m_addressingMode;
    quint32 m_tlbIndexMask;
    quint32 m_tlbIndexShift;
    QMutex m_translationMutex;
    QAtomicInteger<quint64> m_translationHits;
    QAtomicInteger<quint64> m_translationMisses;
    QAtomicInteger<quint64> m_translationFaults;
    QAtomicInteger<quint64> m_protectionViolations;
    QAtomicInteger<quint64> m_totalTranslations;
    bool m_initialized;

  public:
    explicit tlbAddressTranslator(QObject *parent = nullptr)
        : QObject(parent), m_addressingMode(DIRECT_MAPPED), m_tlbIndexMask(TLB_SIZE - 1), // 63 for 64-entry TLB
          m_tlbIndexShift(PAGE_OFFSET_BITS), m_translationHits(0), m_translationMisses(0), m_translationFaults(0),
          m_protectionViolations(0), m_totalTranslations(0), m_initialized(false)
    {
        initialize();
    }

    ~tlbAddressTranslator()
    {
        quint64 totalOps = m_totalTranslations.loadAcquire();
        quint64 hitRate = totalOps > 0 ? (m_translationHits.loadAcquire() * 100) / totalOps : 0;
        DEBUG_LOG("tlbAddressTranslator destroyed - Total: %llu, Hit Rate: %llu%%", totalOps, hitRate);
    }

    void initialize()
    {
        if (m_initialized)
            return;

        // Calculate index bits based on TLB size
        quint32 indexBits = 0;
        quint32 tempSize = TLB_SIZE;
        while (tempSize > 1)
        {
            tempSize >>= 1;
            indexBits++;
        }

        m_tlbIndexMask = (1 << indexBits) - 1;
        m_tlbIndexShift = PAGE_OFFSET_BITS;

        m_initialized = true;
        DEBUG_LOG("tlbAddressTranslator initialized - TLB Size: %d, Index Mask: 0x%x", TLB_SIZE, m_tlbIndexMask);
    }

    void initialize_SignalsAndSlots()
    {
        // Connect translation result signals for monitoring
    }

    quint32 calculateTlbIndex(quint64 virtualAddress) const
    {
        // Extract VPN (Virtual Page Number) and map to TLB index
        quint64 vpn = (virtualAddress & VPN_MASK) >> PAGE_OFFSET_BITS;
        return static_cast<quint32>(vpn & m_tlbIndexMask);
    }

    quint64 extractVirtualTag(quint64 virtualAddress) const
    {
        // Virtual tag is the VPN portion that doesn't fit in the index
        quint64 vpn = (virtualAddress & VPN_MASK) >> PAGE_OFFSET_BITS;
        return vpn >> qCountTrailingZeroBits(static_cast<quint32>(TLB_SIZE)); // Remove index bits from VPN
    }

    quint64 extractPageOffset(quint64 virtualAddress) const { return virtualAddress & PAGE_OFFSET_MASK; }

    bool isValidVirtualAddress(quint64 virtualAddress, quint32 processId) const
    {
        // Basic address validation
#if defined(_WIN64) || defined(_WIN32)
        // Windows x64 canonical addressing (48-bit)
        quint64 signExtendedBits = (virtualAddress >> 47) & 0x1FFFF;
        if (signExtendedBits != 0 && signExtendedBits != 0x1FFFF)
        {
            return false;
        }
#elif defined(__x86_64__)
        // Linux x64 canonical addressing
        quint64 signExtendedBits = (virtualAddress >> 47) & 0x1FFFF;
        if (signExtendedBits != 0 && signExtendedBits != 0x1FFFF)
        {
            return false;
        }
#endif

        // Additional process-specific validation could go here
        return true;
    }

    TranslationResponse translateAddress(const TranslationRequest &request)
    {
        QMutexLocker locker(&m_translationMutex);

        TranslationResponse response;
        response.translationTime = QDateTime::currentMSecsSinceEpoch();

        m_totalTranslations.fetchAndAddAcquire(1);

        // Validate virtual address
        if (!isValidVirtualAddress(request.virtualAddress, request.processId))
        {
            response.result = TRANSLATION_INVALID_ADDRESS;
            DEBUG_LOG("Invalid virtual address: 0x%llx for PID %u", request.virtualAddress, request.processId);
            emit sigTranslationFailed(request.virtualAddress, request.processId, response.result);
            return response;
        }

        // Calculate TLB mapping
        response.tbIndex = calculateTlbIndex(request.virtualAddress);
        response.virtualTag = extractVirtualTag(request.virtualAddress);

        DEBUG_LOG("Translation request: VA=0x%llx, PID=%u, TLB Index=%u, Tag=0x%llx", request.virtualAddress,
                  request.processId, response.tbIndex, response.virtualTag);

        emit sigTranslationRequested(request.virtualAddress, request.processId, response.tbIndex, response.virtualTag);

        // This would typically interface with the TLB entry state manager
        // For now, we return the mapping information for the higher-level TLB system
        response.result = TRANSLATION_HIT; // Placeholder - actual hit/miss determined by caller

        return response;
    }

    bool validateTagMatch(quint64 storedTag, quint64 requestTag) const { return storedTag == requestTag; }

    quint64 constructPhysicalAddress(quint64 physicalPageAddress, quint64 virtualAddress) const
    {
        quint64 pageOffset = extractPageOffset(virtualAddress);
        return (physicalPageAddress & VPN_MASK) | pageOffset;
    }

    void recordTranslationHit(quint32 tbIndex, quint64 virtualAddress)
    {
        m_translationHits.fetchAndAddAcquire(1);
        DEBUG_LOG("Translation HIT: TLB Index=%u, VA=0x%llx", tbIndex, virtualAddress);
        emit sigTranslationHit(tbIndex, virtualAddress);
    }

    void recordTranslationMiss(quint32 tbIndex, quint64 virtualAddress)
    {
        m_translationMisses.fetchAndAddAcquire(1);
        DEBUG_LOG("Translation MISS: TLB Index=%u, VA=0x%llx", tbIndex, virtualAddress);
        emit sigTranslationMiss(tbIndex, virtualAddress);
    }

    void recordTranslationFault(quint64 virtualAddress, quint32 processId)
    {
        m_translationFaults.fetchAndAddAcquire(1);
        DEBUG_LOG("Translation FAULT: VA=0x%llx, PID=%u", virtualAddress, processId);
        emit sigTranslationFault(virtualAddress, processId);
    }

    void recordProtectionViolation(quint64 virtualAddress, quint32 processId)
    {
        m_protectionViolations.fetchAndAddAcquire(1);
        DEBUG_LOG("Protection VIOLATION: VA=0x%llx, PID=%u", virtualAddress, processId);
        emit sigProtectionViolation(virtualAddress, processId);
    }

    void setAddressingMode(AddressingMode mode)
    {
        QMutexLocker locker(&m_translationMutex);
        m_addressingMode = mode;
        DEBUG_LOG("Addressing mode changed to: %d", mode);
    }

    AddressingMode getAddressingMode() const { return m_addressingMode; }
    quint32 getTlbSize() const { return TLB_SIZE; }
    quint32 getPageSize() const { return PAGE_SIZE; }
    quint32 getTlbIndexMask() const { return m_tlbIndexMask; }

    // Performance calculation methods
    double getHitRatio() const
    {
        quint64 total = m_totalTranslations.loadAcquire();
        if (total == 0)
            return 0.0;
        return static_cast<double>(m_translationHits.loadAcquire()) / static_cast<double>(total);
    }

    double getMissRatio() const
    {
        quint64 total = m_totalTranslations.loadAcquire();
        if (total == 0)
            return 0.0;
        return static_cast<double>(m_translationMisses.loadAcquire()) / static_cast<double>(total);
    }

    // Statistics accessors
    quint64 getTranslationHits() const { return m_translationHits.loadAcquire(); }
    quint64 getTranslationMisses() const { return m_translationMisses.loadAcquire(); }
    quint64 getTranslationFaults() const { return m_translationFaults.loadAcquire(); }
    quint64 getProtectionViolations() const { return m_protectionViolations.loadAcquire(); }
    quint64 getTotalTranslations() const { return m_totalTranslations.loadAcquire(); }

    void resetStatistics()
    {
        m_translationHits.storeRelease(0);
        m_translationMisses.storeRelease(0);
        m_translationFaults.storeRelease(0);
        m_protectionViolations.storeRelease(0);
        m_totalTranslations.storeRelease(0);
        DEBUG_LOG("Translation statistics reset");
    }

  signals:
    void sigTranslationRequested(quint64 virtualAddress, quint32 processId, quint32 tbIndex, quint64 virtualTag);
    void sigTranslationHit(quint32 tbIndex, quint64 virtualAddress);
    void sigTranslationMiss(quint32 tbIndex, quint64 virtualAddress);
    void sigTranslationFault(quint64 virtualAddress, quint32 processId);
    void sigProtectionViolation(quint64 virtualAddress, quint32 processId);
    void sigTranslationFailed(quint64 virtualAddress, quint32 processId, TranslationResult reason);
};

