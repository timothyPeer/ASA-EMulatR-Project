#pragma once

/*──────────────────────────────────────────────────────────────────────────────
 *  pci_window.h                    -- Alpha AXP PCI/IO window abstraction
 *
 *  Covers the three canonical window types used by every DEC/Compaq Alpha
 *  workstation and server:
 *
 *      ▸  _Dense_   : 1-to-1, byte-addressable (PA<5:0> → AD<5:0>)            – SRM v6 §7.3.1
 *      ▸  _Sparse_  : each 8-/16-/32-bit datum lives in a 32-byte quadword;   – SRM v6 fig. 7-2
 *                     byte-lane is encoded in AD<4:3> (read) and AD<4:2> (wr)
 *      ▸  _CSR_     : chip-set control/status space (P-chip, Pyxis, Marvel). – 21272 DS §3.2
 *
 *  The class does **no** MMU or privilege checks – it assumes the caller has
 *  already produced a physical address.  Use AlphaSystemManager to choose the
 *  right concrete window for each CPU model (EV4/5 vs EV6/7, etc.).
 *
 *  References
 *      • Alpha Architecture Handbook v6, ch. 7 “I/O Addressing”, pp. 7-8 → 7-11
 *      • 21272 (“Tsunami”) Core-Logic Data-Sheet, rev. 1.2, table 3-1
 *      • 21364 (“Marvel”) System Programmer’s Manual, ch. 4
 *────────────────────────────────────────────────────────────────────────────*/
#pragma once
#include <QtGlobal>
#include <QVector>
#include <QReadWriteLock>
#include <functional>

class PciWindow
{
public:
    enum class Kind { Dense, Sparse, CSR };

    struct Mapping
    {
        quint64     start;      //!< offset inside the window
        quint64     length;     //!< length in bytes
        std::function<quint64(quint64 /*busAddr*/,
            quint64 /*data*/,
            int     /*size*/,
            bool    /*write*/)> io;
    };

protected:
    const quint64 m_base;   //!< physical base address
    const quint64 m_size;   //!< total window size
    const int     m_hose;   //!< PCI hose / bus number
    const Kind    m_kind;

    QVector<Mapping> m_map;         //!< installed devices
    mutable QReadWriteLock m_lock;

public:
    PciWindow(int hose, quint64 base, quint64 size, Kind kind)
        : m_base(base), m_size(size), m_hose(hose), m_kind(kind) {
    }

    virtual ~PciWindow() = default;

    bool contains(quint64 pa) const
    {
        return pa >= m_base && pa < m_base + m_size;
    }

    Kind kind()   const { return m_kind; }
    int  hose()   const { return m_hose; }
    quint64 base() const { return m_base; }
    quint64 size() const { return m_size; }

    /** Translate physical address → **bus** address as it appears on AD<31:0>.
     *  (Sparse-window encoding differs from dense.)
     */
    virtual quint64 toBusAddr(quint64 pa) const = 0;

    /** Install a device sub-range.  Caller supplies a lambda that performs the
     *  actual read/write; MMIOManager will forward to the correct Mapping.     */
//     void mapDevice(quint64 offset, quint64 length, Mapping::io&& cb)
//     {
//         QWriteLocker L(&m_lock);
//         m_map.push_back({ offset, length, std::move(cb) });
//     }

    /** Entry point used by MMIOManager */
    quint64 access(quint64 pa, quint64 data, int size, bool write) const
    {
        const quint64 bus = toBusAddr(pa);

        QReadLocker L(&m_lock);
        for (const auto& m : m_map)
            if (bus >= m.start && bus < m.start + m.length)
                return m.io(bus, data, size, write);

        /* No device decoded the address – return ~0 on read, ignore write    */
        return write ? 0 : ~0ULL;
    }
};





