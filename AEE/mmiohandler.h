#pragma once
#ifndef mmiohandler_h__
#define mmiohandler_h__

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
    virtual uint8_t  mmioReadIUnt8(quint64 offset) = 0;
    virtual uint16_t mmioReadIUnt16(quint64 offset) = 0;
    virtual uint32_t mmioReadIUnt32(quint64 offset) = 0;
    virtual quint64  mmioReadIUnt64(quint64 offset) = 0;
    virtual void     mmioWriteIUnt8(quint64 offset, uint8_t  value) = 0;
    virtual void     mmioWriteIUnt16(quint64 offset, uint16_t value) = 0;
    virtual void     mmioWriteIUnt32(quint64 offset, uint32_t value) = 0;
    virtual void     mmioWriteIUnt64(quint64 offset, quint64  value) = 0;
};
#endif // mmiohandler_h__