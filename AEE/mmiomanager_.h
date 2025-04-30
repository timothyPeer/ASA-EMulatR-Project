// MMIOManager.h
// Qt‐based MMIOManager for memory‐mapped I/O dispatching
// Inline documentation with references to Alpha AXP System Reference Manual, v6, 1994, Part I Chapter 8 :contentReference[oaicite:0]{index=0}&#8203;:contentReference[oaicite:1]{index=1}

#ifndef MMIOMANAGER_H
#define MMIOMANAGER_H

#include <cstdint>
#include <QVector>
#include <QMutex>
#include <QMutexLocker>

/**
 * @brief Abstract interface for an MMIO device handler.
 *
 * Subclasses must implement methods to read/write 8/16/32/64-bit values
 * at a given offset within their mapped register space.
 */
class MmioHandler {
public:
    virtual ~MmioHandler() {}
    virtual uint8_t  mmioRead8(quint64 offset) = 0;
    virtual uint16_t mmioRead16(quint64 offset) = 0;
    virtual uint32_t mmioRead32(quint64 offset) = 0;
    virtual quint64  mmioRead64(quint64 offset) = 0;
    virtual void     mmioWrite8(quint64 offset, uint8_t  value) = 0;
    virtual void     mmioWrite16(quint64 offset, uint16_t value) = 0;
    virtual void     mmioWrite32(quint64 offset, uint32_t value) = 0;
    virtual void     mmioWrite64(quint64 offset, quint64  value) = 0;
};

/**
 * @brief Manager for multiple MMIO regions, dispatching accesses to handlers.
 *
 * Maintains a thread-safe list of address regions [start,end] each bound
 * to a MmioHandler. On read/write, finds matching region and forwards
 * the access. Regions may be queried and reset. Uses QMutex for SMP safety.
 *
 * Reference: Alpha AXP System Reference Manual, v6, 1994, Part I §8 Input/Output Overview :contentReference[oaicite:2]{index=2}&#8203;:contentReference[oaicite:3]{index=3}
 */
class MMIOManager {
public:
    MMIOManager() {}
    ~MMIOManager() {}

    /**
     * @brief Map a device into the MMIO space.
     * @param device Pointer to handler for this region.
     * @param base   Start physical address.
     * @param size   Size in bytes.
     */
    void mapDevice(MmioHandler* device, quint64 base, quint64 size) {
        QMutexLocker locker(&m_mutex);
        m_regions.append({ base, base + size - 1, device });
        return true;
    }

    /// Read an 8-bit value from MMIO or return 0xFF if unmapped.
    uint8_t  read8(quint64 addr) { return readGeneric<uint8_t >(addr, 0xFFu); }

    /// Read a 16-bit value from MMIO or return 0xFFFF if unmapped.
    uint16_t read16(quint64 addr) { return readGeneric<uint16_t>(addr, 0xFFFFu); }

    /// Read a 32-bit value from MMIO or return 0xFFFFFFFF if unmapped.
    uint32_t read32(quint64 addr) { return readGeneric<uint32_t>(addr, 0xFFFFFFFFu); }

    /// Read a 64-bit value from MMIO or return all-ones if unmapped.
    quint64  read64(quint64 addr) { return readGeneric<quint64 >(addr, 0xFFFFFFFFFFFFFFFFULL); }

    /// Write an 8-bit value to MMIO if mapped.
    void write8(quint64 addr, uint8_t  val) { writeGeneric(addr, val); }

    /// Write a 16-bit value to MMIO if mapped.
    void write16(quint64 addr, uint16_t val) { writeGeneric(addr, val); }

    /// Write a 32-bit value to MMIO if mapped.
    void write32(quint64 addr, uint32_t val) { writeGeneric(addr, val); }

    /// Write a 64-bit value to MMIO if mapped.
    void write64(quint64 addr, quint64  val) { writeGeneric(addr, val); }

    /**
     * @brief Check if an address lies within any mapped region.
     * @param addr Physical address to test.
     * @return true if addr is in [start,end] of any region.
     */
    bool isMMIOAddress(quint64 addr) const {
        QMutexLocker locker(&m_mutex);
        for (const auto& r : m_regions) {
            if (addr >= r.start && addr <= r.end)
                return true;
        }
        return false;
    }

    /// Unmap all devices and clear regions.
    void reset() {
        QMutexLocker locker(&m_mutex);
        m_regions.clear();
    }

private:
    struct Region {
        quint64       start;   ///< Base of region
        quint64       end;     ///< Last byte of region
        MmioHandler* handler; ///< Handler for accesses
    };

    QVector<Region> m_regions; ///< List of all mapped MMIO regions
    mutable QMutex  m_mutex;   ///< Protects m_regions for concurrent access

    // Generic reader: locks, finds region, invokes correct handler or returns default
    template<typename T>
    T readGeneric(quint64 addr, T defaultVal) const {
        QMutexLocker locker(&m_mutex);
        for (const auto& r : m_regions) {
            if (addr >= r.start && addr <= r.end) {
                auto off = addr - r.start;
                if constexpr (std::is_same_v<T, uint8_t >) return r.handler->mmioRead8(off);
                if constexpr (std::is_same_v<T, uint16_t>) return r.handler->mmioRead16(off);
                if constexpr (std::is_same_v<T, uint32_t>) return r.handler->mmioRead32(off);
                if constexpr (std::is_same_v<T, quint64>) return r.handler->mmioRead64(off);
            }
        }
        return defaultVal;
    }

    // Generic writer: locks, finds region, invokes correct handler
    template<typename V>
    void writeGeneric(quint64 addr, V val) {
        QMutexLocker locker(&m_mutex);
        for (const auto& r : m_regions) {
            if (addr >= r.start && addr <= r.end) {
                auto off = addr - r.start;
                if constexpr (std::is_same_v<V, uint8_t >) r.handler->mmioWrite8(off, val);
                if constexpr (std::is_same_v<V, uint16_t>) r.handler->mmioWrite16(off, val);
                if constexpr (std::is_same_v<V, uint32_t>) r.handler->mmioWrite32(off, val);
                if constexpr (std::is_same_v<V, quint64>) r.handler->mmioWrite64(off, val);
                return;
            }
        }
    }
};

#endif // MMIOMANAGER_H
