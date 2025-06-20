#pragma once
#include <QtCore>
// Add this to your AlphaJITCompiler class
struct UnalignedAccessContext
{
    quint8 baseReg;  // Base address register
    quint8 valueReg; // Value register (for stores) or destination (for loads)
    quint64 offset;  // Offset from base
    int size;        // Access size in bytes (2, 4, or 8)
    bool isStore;    // True if this is a store operation
};
