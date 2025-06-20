#pragma once
// #include "JITFaultInfoStructures.h"
#include <QtAlgorithms> // for qCountTrailingZeroBits()
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

/**
 * @brief Enhanced cache line structure for high performance
 */
class alignas(64) CacheLine
{
  private:
    std::atomic<bool> m_valid{false};
    std::atomic<uint64_t> m_tag{0};
    std::atomic<uint64_t> m_address{0};
    std::vector<uint8_t> m_data;

    // Metadata
    std::atomic<bool> m_dirty{false};
    std::atomic<bool> m_prefetched{false};
    std::atomic<uint32_t> m_refCount{0};

  public:
    explicit CacheLine(size_t lineSize = 64) : m_data(lineSize, 0)
    {
        static_assert(sizeof(CacheLine) % 64 == 0, "CacheLine should be cache-line aligned");
    }

    // Atomic accessors for thread safety
    bool isValid() const { return m_valid.load(std::memory_order_acquire); }
    void setValid(bool valid) { m_valid.store(valid, std::memory_order_release); }

    uint64_t getTag() const { return m_tag.load(std::memory_order_acquire); }
    void setTag(uint64_t tag) { m_tag.store(tag, std::memory_order_release); }

    uint64_t getAddress() const { return m_address.load(std::memory_order_acquire); }
    void setAddress(uint64_t addr) { m_address.store(addr, std::memory_order_release); }

    bool isDirty() const { return m_dirty.load(std::memory_order_acquire); }
    void setDirty(bool dirty) { m_dirty.store(dirty, std::memory_order_release); }

    bool isPrefetched() const { return m_prefetched.load(std::memory_order_acquire); }
    void setPrefetched(bool prefetched) { m_prefetched.store(prefetched, std::memory_order_release); }

    /**
     * @brief Thread-safe data access
     * @param offset Offset within the cache line
     * @param size Number of bytes to read
     * @param buffer Output buffer
     * @return True if read successful
     */
    bool readData(size_t offset, size_t size, void *buffer) const
    {
        if (offset + size > m_data.size())
        {
            return false;
        }

        std::memcpy(buffer, m_data.data() + offset, size);
        return true;
    }

    /**
     * @brief Thread-safe data write
     * @param offset Offset within the cache line
     * @param size Number of bytes to write
     * @param buffer Input buffer
     * @return True if write successful
     */
    bool writeData(size_t offset, size_t size, const void *buffer)
    {
        if (offset + size > m_data.size())
        {
            return false;
        }

        std::memcpy(m_data.data() + offset, buffer, size);
        setDirty(true);
        return true;
    }

    /**
     * @brief Clear cache line data
     */
    void clear()
    {
        std::fill(m_data.begin(), m_data.end(), 0);
        setDirty(false);
        setPrefetched(false);
        m_refCount.store(0, std::memory_order_release);
    }

    /**
     * @brief Get cache line data pointer (use with caution)
     * @return Pointer to data array
     */
    const uint8_t *getData() const { return m_data.data(); }
    uint8_t *getMutableData() { return m_data.data(); }

    /**
     * @brief Get cache line size
     * @return Size in bytes
     */
    size_t getSize() const { return m_data.size(); }

    /**
     * @brief Reference counting for coherency protocols
     */
    void addRef() { m_refCount.fetch_add(1, std::memory_order_relaxed); }
    void removeRef() { m_refCount.fetch_sub(1, std::memory_order_relaxed); }
    uint32_t getRefCount() const { return m_refCount.load(std::memory_order_relaxed); }
};
