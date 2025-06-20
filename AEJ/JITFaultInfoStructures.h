#pragma once
#ifndef JITFaultInfoStructures_h__
#define JITFaultInfoStructures_h__
#include "../AEE/FPException.h"
#include "../AEJ/TLPHelpers.h"
#include "../AEJ/tlbSystem.h"
#include "../AEJ/helpers/helperSignExtend.h"
#include "../AEJ/enumerations/enumExceptionType.h"



/**
 * @brief Translate virtual address to physical
 * @param va Virtual address
 * @param asn Address space number
 * @param isKernelMode True if in kernel mode
 * @return Translation result
 */

// Combined fault type enumeration



struct ExecutionResult
{
    int instructionsExecuted = 0;
    quint64 finalPC = 0;
    QVector<quint64> registers;
    QVector<double> fpRegisters;
    int compiledBlocks = 0;
    int compiledTraces = 0;
    quint64 faultCode;
    quint64 status;
};



// /**
//  * @enum ProcessorMode
//  * @brief Alpha processor execution modes
//  */
// enum class ProcessorMode
// {
//     USER = 0,       // User mode (lowest privilege)
//     SUPERVISOR = 1, // Supervisor mode (medium privilege)
//     KERNEL = 2,     // Kernel mode (highest privilege)
//     PAL = 3,        // PAL mode (special execution mode)
//     MAX_MODES
// };






// enum class ExceptionType {
// 	ACCESS_CONTROL_VIOLATION,
//     ACCESS_VIOLATION,               // Protection violation
// 	ALIGNMENT_FAULT ,            // Misaligned access
// 	ARITHMETIC_TRAP ,
// 	BREAKPOINT,
// 	BUS_ERROR,
//     FAULT_ON_READ,                  // Error during read
//     FAULT_ON_WRITE,                 // Error during write
// 	FLOATING_POINT_DIVIDE_BY_ZERO,
// 	FLOATING_POINT_INVALID,
// 	FLOATING_POINT_OVERFLOW,
// 	FLOATING_POINT_UNDERFLOW,
//     GENERAL_PROTECTION_FAULT,       // General Protection Fault
// 	HALT,
// 	ILLEGAL_INSTRUCTION ,
// 	ILLEGAL_OPCODE,
//     INSTRUCTION_ACCESS_FAULT,       // Fault during instruction fetch
// 	INTEGER_DIVIDE_BY_ZERO,
// 	INTEGER_OVERFLOW,
// 	INTERRUPT,
// 	MACHINE_CHECK,
// 	MEMORY_ACCESS_VIOLATION,
// 	MEMORY_ALIGNMENT_FAULT,
// 	MEMORY_EXECUTE_FAULT,
// 	MEMORY_FAULT_ON_READ,
// 	MEMORY_FAULT_ON_WRITE,
//     NONE,                           // No fault
// 	OPCODE_RESERVED,
//     PAGE_FAULT,                     // Page Fault
// 	PRIVILEGED_INSTRUCTION ,
// 	PROTECTION_VIOLATION,
// 	RESERVED_OPERAND,
// 	SYSTEM_CALL,
// 	TRANSLATION_NOT_VALID,
//     TRANSLATION_NOT_VALID,          // TLB miss or invalid page
//     UNKNOWN_EXCEPTION
// };

inline size_t qHash(const ExceptionType &key, quint32 seed = 0) { return qHash(static_cast<int>(key), seed); }



// Structure for a single performance counter
struct PerfCounter
{
    quint64 value;             // Current counter value
    quint32 eventType;         // Type of event being counted
    quint32 control;           // Control bits (mode, etc.)
    quint32 overflowThreshold; // Value that triggers overflow action
    quint32 overflowAction;    // What to do on overflow
    quint32 qualifierRegister; // For EV5+ filtering
    quint32 counterMask;       // For EV6+ counter masking

    bool enabled;               // Is this counter enabled?
    bool countInKernelMode;     // Count in kernel mode?
    bool countInUserMode;       // Count in user mode?
    bool countInSupervisorMode; // Count in supervisor mode?
    bool countPalMode;          // Count in PAL mode? (EV6+)
    bool invertMode;            // Invert the mode check?
    bool interruptOnOverflow;   // Generate interrupt on overflow?

    QString description; // Human-readable description of what's being counted

    PerfCounter()
        : value(0), eventType(0), control(0), overflowAction(0),
          qualifierRegister(0), counterMask(0), enabled(false), countInKernelMode(false), countInUserMode(false),
          countInSupervisorMode(false), countPalMode(false), invertMode(false), interruptOnOverflow(false),
          description("Undefined")
    {
       // overflowThreshold = 0xFFFFFFFFFFFFFFFFULL;
    }
};

// Structure for a profiling sample entry
struct ProfileEntry
{
    quint64 pc; // Program counter value
    int count;  // Number of times this PC was sampled

    ProfileEntry() : pc(0), count(0) {}
};

// Structure for enhanced monitoring configuration (EV6+)
struct EnhancedMonitoring
{
    bool sampleAllProcesses;      // Sample all processes or just current?
    bool sampleUserMode;          // Sample in user mode?
    bool sampleKernelMode;        // Sample in kernel mode?
    bool sampleInstructionRetire; // Sample on instruction retire?
    bool sampleBranchEvents;      // Sample on branch events?
    bool sampleMemoryEvents;      // Sample on memory events?

    EnhancedMonitoring()
        : sampleAllProcesses(false), sampleUserMode(true), sampleKernelMode(false), sampleInstructionRetire(true),
          sampleBranchEvents(false), sampleMemoryEvents(false)
    {
    }
};

// Structure for monitoring filters
struct MonitoringFilters
{
    // Address range filter
    quint64 addrRangeStart;
    quint64 addrRangeEnd;
    bool addrRangeEnabled;

    // Process ID filter
    quint32 processId;
    bool processIdEnabled;

    // Instruction type filter
    quint32 instructionType;
    bool instructionTypeEnabled;

    MonitoringFilters()
        : addrRangeStart(0), addrRangeEnd(0), addrRangeEnabled(false), processId(0), processIdEnabled(false),
          instructionType(0), instructionTypeEnabled(false)
    {
    }
};

// Structure for module information (for symbol resolution)
struct ModuleInfo
{
    QString name;
    quint64 baseAddress;
    quint64 size;

    ModuleInfo() : baseAddress(0), size(0) {}
    ModuleInfo(const QString &n, quint64 addr, quint64 s) : name(n), baseAddress(addr), size(s) {}
};

#endif // JITFaultInfoStructures_h__