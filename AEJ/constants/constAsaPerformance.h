#pragma once
#include <QtCore>
constexpr quint32 MISC_INSTRUCTIONS = 0x20;             // Total miscellaneous instructions
constexpr quint32 BARRIER_INSTRUCTIONS = 0x21;          // Memory/exception barriers
constexpr quint32 CACHE_INSTRUCTIONS = 0x22;            // Cache management instructions
constexpr quint32 TIMING_INSTRUCTIONS = 0x23;           // Performance/timing instructions
constexpr quint32 LOCK_INSTRUCTIONS = 0x24;             // Lock flag instructions
constexpr quint32 HARDWARE_INSTRUCTIONS = 0x30;         // Total hardware instructions
constexpr quint32 IPR_INSTRUCTIONS = 0x31;              // IPR read/write instructions
constexpr quint32 HARDWARE_MEMORY_INSTRUCTIONS = 0x32;  // Hardware memory instructions
constexpr quint32 HARDWARE_CONTROL_INSTRUCTIONS = 0x33; // Hardware control instructions

// Constants for performance monitoring
static const int MAX_PERF_COUNTERS = 8;      // Maximum number of performance counters
static const int PROFILE_BUFFER_SIZE = 1024; // Size of profiling sample buffer

// Constants for system entry points
static const int MAX_SYSTEM_ENTRY_POINTS = 64; // Maximum number of system entries
static const int MAX_CUSTOM_ENTRIES = 32;      // Number of custom entry points



