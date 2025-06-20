#pragma once
#include <QtCore>

// MMIOAddress Boundaries for Alpha Processor Variants EV5-EV7
// Legacy I/O space (mapped through chipset)

// EV56 used similar addressing to EV5 but with enhanced chipsets
static constexpr quint64 EV56_MMIO_BASE = 0x8000000000ULL; // 512GB
static constexpr quint64 EV56_MMIO_END = 0x20000000000ULL; // 2TB

// Pyxis chipset additions
static constexpr quint64 PYXIS_PCI_IO_BASE = 0x8580000000ULL;
static constexpr quint64 PYXIS_PCI_MEM_BASE = 0x8000000000ULL;

// EV6 systems had expanded MMIO space
static constexpr quint64 EV6_MMIO_BASE = 0x10000000000ULL; // 1TB
static constexpr quint64 EV6_MMIO_END = 0x20000000000ULL;  // 2TB

// Tsunami chipset
static constexpr quint64 TSUNAMI_PCI_IO_BASE = 0x101FC000000ULL;
static constexpr quint64 TSUNAMI_PCI_MEM_BASE = 0x10000000000ULL;
static constexpr quint64 TSUNAMI_CSR_BASE = 0x10100000000ULL; // Chipset registers

// Similar to EV6 but with some extensions
static constexpr quint64 EV67_MMIO_BASE = 0x10000000000ULL; // 1TB
static constexpr quint64 EV67_MMIO_END = 0x40000000000ULL;  // 4TB (extended)

// Marvel chipset (for high-end EV67/EV68 systems)
static constexpr quint64 MARVEL_PCI_IO_BASE = 0x801FC000000ULL;
static constexpr quint64 MARVEL_PCI_MEM_BASE = 0x80000000000ULL;
static constexpr quint64 MARVEL_CSR_BASE = 0x801FE000000ULL;

// EV7 systems with Marvel chipset
static constexpr quint64 EV7_MMIO_BASE = 0x80000000000ULL; // 8TB
static constexpr quint64 EV7_MMIO_END = 0x100000000000ULL; // 16TB

// EV7 specific regions
static constexpr quint64 EV7_PCI_IO_BASE = 0x801FC000000ULL;
static constexpr quint64 EV7_PCI_MEM_BASE = 0x80000000000ULL;
static constexpr quint64 EV7_CSR_BASE = 0x801FE000000ULL;

