#pragma once
#include <stdatomic.h>
#include <QtCore>
#include "../AEJ/GlobalMacro.h"
#include <QAtomicInt>
#include <QAtomicInteger>

namespace asa_utils
{

class OverflowSafeCounter
{
  private:
    std::atomic<uint64_t> m_value{0};
    std::atomic<bool> m_overflowDetected{false};
    static constexpr uint64_t OVERFLOW_THRESHOLD = UINT64_MAX - 1000000; // Safety margin

  public:
    // Safe increment with overflow detection
    uint64_t safeIncrement(uint64_t increment = 1) noexcept
    {
        uint64_t currentValue = m_value.load(std::memory_order_relaxed);

        // Check if increment would cause overflow
        if (currentValue > OVERFLOW_THRESHOLD)
        {
            m_overflowDetected.store(true, std::memory_order_release);
            DEBUG_LOG(QString("Counter overflow detected at value: %1").arg( currentValue));
            return currentValue; // Saturate at current value
        }

        return m_value.fetch_add(increment, std::memory_order_relaxed);
    }

    // Get current value
    uint64_t load() const noexcept { return m_value.load(std::memory_order_acquire); }

    // Check overflow status
    bool hasOverflowed() const noexcept { return m_overflowDetected.load(std::memory_order_acquire); }

    // Reset counter (for management operations)
    void reset() noexcept
    {
        m_value.store(0, std::memory_order_release);
        m_overflowDetected.store(false, std::memory_order_release);
    }

    // Get percentage to overflow (for monitoring)
    double getOverflowPercentage() const noexcept
    {
        uint64_t current = load();
        return (static_cast<double>(current) / UINT64_MAX) * 100.0;
    }
};


/************************************************************************/
/*@brief Safe increment for std::atomic<quint64> with overflow protection */
/************************************************************************/

inline void safeIncrement(std::atomic<quint64> &counter, quint64 increment = 1)
{
    quint64 oldValue = counter.fetch_add(increment, std::memory_order_relaxed);
    if (oldValue > (UINT64_MAX - 1000000))
    {
        WARN_LOG("std::atomic<quint64> overflow detected, resetting");
        counter.store(0, std::memory_order_relaxed);
    }
}

/**
@brief Safe increment for std::atomic<int> with overflow protection
*/
inline void safeIncrement(std::atomic<int> &counter, int increment = 1)
{
    int oldValue = counter.fetch_add(increment, std::memory_order_relaxed);
    if (increment > 0 && oldValue > (INT_MAX - 1000000))
    {
        WARN_LOG("std::atomic<int> overflow detected, resetting");
        counter.store(0, std::memory_order_relaxed);
    }
    else if (increment < 0 && oldValue < (INT_MIN + 1000000))
    {
        WARN_LOG("std::atomic<int> underflow detected, resetting");
        counter.store(0, std::memory_order_relaxed);
    }
}


/**
 * @brief Safe increment for QAtomicInt with overflow protection (relaxed memory order)
 *
 * This function increments the given QAtomicInt by the specified amount.
 * If the old value exceeds INT_MAX – 1,000,000 (to avoid overflow when adding up to 1,000,000),
 * the counter is reset to 0. Otherwise, it simply performs the increment.
 *
 * Example usage:
 *     QAtomicInt m_totalSqrtCycles = 0;
 *     safeIncrement(m_totalSqrtCycles);       // increments by 1
 *     safeIncrement(m_totalSqrtCycles, 100);  // increments by 100
 *
 * @param counter    The QAtomicInt to increment.
 * @param increment  The amount to add (defaults to 1).
 */
inline void safeIncrement(QAtomicInt &counter, int increment = 1)
{
    // Threshold chosen so that oldValue + increment cannot overflow an int.
    static constexpr int OVERFLOW_THRESHOLD = INT_MAX - 1'000'000;
    static constexpr int UNDERFLOW_THRESHOLD = INT_MIN + 1'000'000;

    // Perform the relaxed fetch-and-add
    int oldValue = counter.fetchAndAddRelaxed(increment);

    // If increment is positive and oldValue is already near INT_MAX, reset to zero
    if (increment > 0 && oldValue > OVERFLOW_THRESHOLD)
    {
        WARN_LOG("QAtomicInt overflow detected, resetting");
        counter.storeRelaxed(0);
    }
    // If increment is negative and oldValue is already near INT_MIN, reset to zero
    else if (increment < 0 && oldValue < UNDERFLOW_THRESHOLD)
    {
        WARN_LOG("QAtomicInt underflow detected, resetting");
        counter.storeRelaxed(0);
    }
}

/**
@brief Safe increment for std::atomic<quint32> with overflow protection
*/
inline void safeIncrement(std::atomic<quint32> &counter, quint32 increment = 1)
{
    quint32 oldValue = counter.fetch_add(increment, std::memory_order_relaxed);
    if (oldValue > (UINT32_MAX - 1000000))
    {
        WARN_LOG("std::atomic<quint32> overflow detected, resetting");
        counter.store(0, std::memory_order_relaxed);
    }
}

inline void safeIncrementAcquire(QAtomicInt &counter, int increment = 1)
{
    int oldValue = counter.fetchAndAddAcquire(increment);
    if (increment > 0 && oldValue > (INT_MAX - 1000000))
    {
        WARN_LOG("QAtomicInt overflow detected (acquire), resetting");
        counter.storeRelease(0);
    }
    else if (increment < 0 && oldValue < (INT_MIN + 1000000))
    {
        WARN_LOG("QAtomicInt underflow detected (acquire), resetting");
        counter.storeRelease(0);
    }
}

int SafeIncrement(QAtomicInteger<quint64> &counter, int increment)
{
    static constexpr int OVERFLOW_THRESHOLD = INT_MAX - 1000000;
    static constexpr int UNDERFLOW_THRESHOLD = INT_MIN + 1000000;

    int oldValue = counter.fetchAndAddRelaxed(increment);

    if (increment > 0 && oldValue > OVERFLOW_THRESHOLD)
    {
        WARN_LOG("QAtomicInteger<int> overflow detected, resetting");
        counter.storeRelaxed(0);
        return 0;
    }

    if (increment < 0 && oldValue < UNDERFLOW_THRESHOLD)
    {
        WARN_LOG("QAtomicInteger<int> underflow detected, resetting");
        counter.storeRelaxed(0);
        return 0;
    }

    return oldValue;
}


// int SafeIncrement(QAtomicInteger<int> &counter, int increment)
// {
//     static constexpr int OVERFLOW_THRESHOLD = INT_MAX - 1000000;
//     static constexpr int UNDERFLOW_THRESHOLD = INT_MIN + 1000000;
// 
//     int oldValue = counter.fetchAndAddRelaxed(increment);
// 
//     if (increment > 0 && oldValue > OVERFLOW_THRESHOLD)
//     {
//         counter.storeRelaxed(0);
//         return 0;
//     }
// 
//     if (increment < 0 && oldValue < UNDERFLOW_THRESHOLD)
//     {
//         counter.storeRelaxed(0);
//         return 0;
//     }
// 
//     return oldValue;
// }
quint64 SafeIncrement(QAtomicInteger<quint64> &counter, quint64 increment = 1)
{
    static constexpr quint64 OVERFLOW_THRESHOLD = UINT64_MAX - 1000000;

    quint64 oldValue = counter.fetchAndAddRelaxed(increment);

    if (oldValue > OVERFLOW_THRESHOLD)
    {
        counter.storeRelaxed(0);
        return 0;
    }

    return oldValue;
}

inline void safeIncrementAcquire(std::atomic<quint64> &counter, quint64 increment = 1)
{
    quint64 oldValue = counter.fetch_add(increment, std::memory_order_acquire);
    if (oldValue > (UINT64_MAX - 1000000))
    {
        WARN_LOG("std::atomic<quint64> overflow detected (acquire), resetting");
        counter.store(0, std::memory_order_release);
    }
}

inline bool isNearOverflow(const QAtomicInt &counter)
{
    int value = counter.loadRelaxed();
    return value > (INT_MAX - 1000000) || value < (INT_MIN + 1000000);
}

/**

@brief Check if QAtomicInteger<quint64> is near overflow
*/
inline bool isNearOverflow(const QAtomicInteger<quint64> &counter)
{
    quint64 value = counter.loadRelaxed();
    return value > (UINT64_MAX - 1000000);
}

/**

@brief Check if std::atomic<quint64> is near overflow
*/
inline bool isNearOverflow(const std::atomic<quint64> &counter)
{
    quint64 value = counter.load(std::memory_order_relaxed);
    return value > (UINT64_MAX - 1000000);
}

/**

@brief Check if std::atomic<int> is near overflow
*/
inline bool isNearOverflow(const std::atomic<int> &counter)
{
    int value = counter.load(std::memory_order_relaxed);
    return value > (INT_MAX - 1000000) || value < (INT_MIN + 1000000);
}

inline int safeIncrement(QAtomicInteger<quint64> &counter, int increment=1)
{
    static constexpr int OVERFLOW_THRESHOLD = INT_MAX - 1000000;
    static constexpr int UNDERFLOW_THRESHOLD = INT_MIN + 1000000;

    int oldValue = counter.fetchAndAddRelaxed(increment);

    if (increment > 0 && oldValue > OVERFLOW_THRESHOLD)
    {
        WARN_LOG("QAtomicInteger<int> overflow detected, resetting");
        counter.storeRelaxed(0);
        return 0;
    }

    if (increment < 0 && oldValue < UNDERFLOW_THRESHOLD)
    {
        WARN_LOG("QAtomicInteger<int> underflow detected, resetting");
        counter.storeRelaxed(0);
        return 0;
    }

    return oldValue;
}

}

