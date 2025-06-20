#pragma once
#include <QtGlobal>

// ═══════════════════════════════════════════════════════════════════════════
// ALPHA MEMORY SYSTEM CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════

namespace AlphaMemoryConstants
{

// Page size constants (Alpha uses 8KB pages)
constexpr quint64 PAGE_SIZE = 8192;
constexpr quint64 PAGE_MASK = ~(PAGE_SIZE - 1);
constexpr quint64 PAGE_OFFSET_MASK = (PAGE_SIZE - 1);

// TLB constants
constexpr int ITLB_SIZE = 48; // Instruction TLB entries
constexpr int DTLB_SIZE = 64; // Data TLB entries

// Protection and access bits
// TLB protection bits (per ASA Vol II-A section 3.4.3, Table 3-3):
static constexpr quint32 TLB_VALID = 1u << 0;  // PTE<V>
static constexpr quint32 TLB_WRITE = 1u << 1;  // inverse of Fault-On-Write
static constexpr quint32 TLB_EXEC = 1u << 2;   // execute enable
static constexpr quint32 TLB_KERNEL = 1u << 3; // kernel-mode only
static constexpr quint32 TLB_USER = 1u << 4;   // user-mode allowed
static constexpr quint32 TLB_GLOBAL = 1u << 5; // global across ASNs
//                       TLB_READ is enabled by default

// And then separately:
static constexpr quint32 TLB_ACCESSED = 1u << 6; // software side: page has been read
static constexpr quint32 TLB_DIRTY = 1u << 7;    // software side: page has been written



 static constexpr quint64 PAGE_SIZE_8KB = 8ULL * 1024;            ///< 8 KB page size
static constexpr quint64 PAGE_SIZE_64KB = 64ULL * 1024;          ///< 64 KB page size
static constexpr quint64 PAGE_SIZE_4MB = 4ULL * 1024 * 1024;     ///< 4 MB page size
static constexpr quint64 PAGE_SIZE_256MB = 256ULL * 1024 * 1024; ///< 256 MB page size

static constexpr quint64 PAGE_MASK_8KB = PAGE_SIZE_8KB - 1;
static constexpr quint64 PAGE_MASK_64KB = PAGE_SIZE_64KB - 1;
static constexpr quint64 PAGE_MASK_4MB = PAGE_SIZE_4MB - 1;
static constexpr quint64 PAGE_MASK_256MB = PAGE_SIZE_256MB - 1;



// Memory access types
constexpr int ACCESS_READ = 0;
constexpr int ACCESS_WRITE = 1;
constexpr int ACCESS_EXEC = 2;
} // namespace AlphaMemoryConstants

// ═══════════════════════════════════════════════════════════════════════════
// TLB STATISTICS STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

