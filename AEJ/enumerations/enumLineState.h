#pragma once



    // Cache line states for SMP coherency
enum class LineState
{
    INVALID,  // Line is invalid
    VALID,    // Line contains valid data
    SHARED,   // Line is shared with other caches (SMP)
    EXCLUSIVE // Line is exclusive to this cache
};