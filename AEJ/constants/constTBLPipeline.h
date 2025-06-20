#pragma once
#include <QtCore>


// For high-performance async pipeline
static constexpr int MAX_SETS = 512;   // 512 sets
static constexpr int MAX_WAYS = 8;     // 8-way associative
static constexpr int PAGE_SIZE = 8192; // 8KB pages (Alpha standard)

/*
// For compute-intensive workloads (scientific, HPC)
static constexpr int MAX_SETS = 1024;   // Larger working set
static constexpr int MAX_WAYS = 4;      // Lower associativity, faster lookup

// For instruction-heavy workloads (compilers, interpreters)
static constexpr int MAX_SETS = 256;    // Smaller, faster
static constexpr int MAX_WAYS = 16;     // Higher associativity for code locality

// For memory-intensive workloads (databases, analytics)
static constexpr int MAX_SETS = 2048;   // Maximum coverage
static constexpr int MAX_WAYS = 4;      // Balance speed vs. capacity
*/

// Partition TLB to reduce contention 
static constexpr int ASYNC_PARTITIONS = 8;
static constexpr int SETS_PER_PARTITION = MAX_SETS / ASYNC_PARTITIONS; // 64 sets each

static constexpr int constTuningOperationThrottle = 10000;

// Profile with different configurations
struct TLBConfig
{
    quint64 sets;
    quint64 ways;
    double hitRate;
    quint64 avgLatency;
    quint64 contention;
};

TLBConfig configs[] = {
    {256, 4, 0.0, 0, 0},  // Fast & small
    {512, 8, 0.0, 0, 0},  // Balanced (recommended)
    {1024, 4, 0.0, 0, 0}, // Large & fast
    {512, 16, 0.0, 0, 0}  // High associativity
};

