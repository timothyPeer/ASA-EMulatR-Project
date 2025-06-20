#pragma once
#include <QObject>

 /**
 * @brief Detailed result structure for TLB checks
 */
 struct TLBCheckResult
{
    bool isValid;           // Entry exists and is valid
    bool isReadable;        // Has read permission
    bool isWritable;        // Has write permission
    bool isExecutable;      // Has execute permission
    bool isGlobal;          // Global translation (ASN-independent)
    bool isDirty;           // Page has been written to
    bool isReferenced;      // Page has been accessed
    quint64 physicalAddr;   // Physical address (if valid)
    quint64 pageSize;       // Page size for this entry
    quint64 protectionBits; // Raw protection bits

    TLBCheckResult()
        : isValid(false), isReadable(false), isWritable(false), isExecutable(false), isGlobal(false), isDirty(false),
          isReferenced(false), physicalAddr(0), pageSize(0), protectionBits(0)
    {
    }
};