#ifndef ALPHA_PROCESSOR_INTEGRATION_MANAGER_H
#define ALPHA_PROCESSOR_INTEGRATION_MANAGER_H

#include "AlphaInstructionCache.h"
#include "AlphaMemorySystem_refactored.h"
#include "AlphaTranslationCache.h"
#include "CacheLine.h"
#include "CacheSet.h"
#include "UnifiedDataCache.h"
#include "pipeline_alphainstructions.h"
#include "tlbSystem.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <QAtomicInt>
#include <QObject>

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(push, 8)
#endif

// Forward declarations
class alphaRegisterFile;
class alphaBranchPredictor;
class alphaPerformanceCounters;

/**
 * @brief Complete header-only Alpha Processor Integration Manager
 *
 * Integrates all processor components including:
 * - Cache hierarchy (I-cache, L1D, L2, L3)
 * - TLB system for virtual memory
 * - Memory system coordination
 * - Instruction execution pipeline
 * - Performance monitoring
 */
class alphaProcessorIntegrationManager : QObject
{
    Q_OBJECT

  public:
    using InstructionFactory = std::function<std::unique_ptr<alphaInstructionBase>(uint32_t, uint32_t)>;

    // Execution context for shared state
    struct ExecutionContext
    {
        std::array<uint64_t, 32> integerRegisters{}; // R0-R31
        std::array<double, 32> floatingRegisters{};  // F0-F31
        uint64_t programCounter{0};
        uint64_t stackPointer{0x7FFFFFFF0000ULL};
        uint64_t linkRegister{0};
        uint64_t processorStatus{0};
        uint64_t floatingPointControl{0};
        uint64_t memoryManagementUnit{0};
        uint64_t cycleCounter{0};
        bool interruptsEnabled{true};
        bool privilegedMode{false};
        uint32_t processorId{0};
    };

    // Instruction decode result
    struct DecodedInstruction
    {
        uint32_t opcode;
        uint8_t primaryOp;
        uint8_t function;
        uint8_t ra, rb, rc;
        int16_t displacement;
        uint32_t literal;
        uint32_t branchDisplacement;
        bool isLiteral;
        std::unique_ptr<alphaInstructionBase> instruction;
    };

    // Performance metrics
    struct PerformanceMetrics
    {
        // High-frequency counters - pure performance, no overhead
        std::atomic<uint64_t> totalInstructions{0}; // ~584 years at 1B inst/sec
        std::atomic<uint64_t> totalCycles{0};       // ~584 years at 1B cycles/sec
        std::atomic<uint64_t> cacheHits{0};         // ~584 years at 1B hits/sec
        std::atomic<uint64_t> cacheMisses{0};       // ~584 years at 1B misses/sec

        // Low-frequency counters - Qt integration for Cold Path
        QAtomicInt branchMispredictions{0};
        QAtomicInt memoryStalls{0};
        QAtomicInt floatingPointOps{0};
        QAtomicInt unalignedAccesses{0};
        QAtomicInt atomicOperations{0};
        QAtomicInt palCalls{0};
        double ipc{0.0};

         void resetAll()
        {
            totalInstructions.store(0, std::memory_order_relaxed);
            totalCycles.store(0, std::memory_order_relaxed);
            cacheHits.store(0, std::memory_order_relaxed);
            cacheMisses.store(0, std::memory_order_relaxed);

            branchMispredictions.storeRelease(0);
            memoryStalls.storeRelease(0);
            floatingPointOps.storeRelease(0);
            unalignedAccesses.storeRelease(0);
            atomicOperations.storeRelease(0);
            palCalls.storeRelease(0);

            ipc = 0.0;

            DEBUG_LOG("Performance counters reset");
        }

        // Optional: Get current values safely (for display only)
        uint64_t getInstructions() const { return totalInstructions.load(std::memory_order_acquire); }
        uint64_t getCycles() const { return totalCycles.load(std::memory_order_acquire); }
        uint64_t getCacheHits() const { return cacheHits.load(std::memory_order_acquire); }
        uint64_t getCacheMisses() const { return cacheMisses.load(std::memory_order_acquire); }

    };

   

   

    // Memory reservation for LL/SC operations
    struct MemoryReservation
    {
        uint64_t address;
        uint32_t size;
        uint64_t timestamp;
        bool valid;
    };


  signals:
    void sigPerformanceUpdate(const PerformanceMetrics &metrics);
    void sigExecutionStateChanged(bool halted);
    void sigCacheStatsChanged();
    void sigComponentAttached(const QString &componentName);
  private:
    // Execution state
    ExecutionContext m_context;
    PerformanceMetrics m_metrics;
    MemoryReservation m_memoryReservation;

    // System components - using existing cache classes
    std::unique_ptr<AlphaMemorySystem> m_memorySystem;
    TLBSystem *m_tlbSystem{nullptr}; // Owned by memory system
    std::unique_ptr<alphaRegisterFile> m_registerFile;
    std::unique_ptr<alphaBranchPredictor> m_branchPredictor;
    std::unique_ptr<alphaPerformanceCounters> m_perfCounters;

    // Cache hierarchy using existing classes
    std::unique_ptr<AlphaInstructionCache> m_instructionCache;
    std::unique_ptr<UnifiedDataCache> m_level1DataCache;
    std::unique_ptr<UnifiedDataCache> m_level2Cache;
    UnifiedDataCache *m_level3Cache{nullptr}; // Owned by memory system

    // Instruction factories
    std::unordered_map<uint8_t, InstructionFactory> m_instructionFactories;

    // Execution state
    bool m_initialized{false};
    bool m_halted{false};
    uint32_t m_currentInstruction{0};
    uint64_t m_executionStartTime{0};

    // Alpha opcode constants
    static constexpr uint8_t OP_PAL = 0x00;
    static constexpr uint8_t OP_LDA = 0x08;
    static constexpr uint8_t OP_LDAH = 0x09;
    static constexpr uint8_t OP_LDBU = 0x0A;
    static constexpr uint8_t OP_LDQ_U = 0x0B;
    static constexpr uint8_t OP_LDWU = 0x0C;
    static constexpr uint8_t OP_STW = 0x0D;
    static constexpr uint8_t OP_STB = 0x0E;
    static constexpr uint8_t OP_STQ_U = 0x0F;
    static constexpr uint8_t OP_INTA = 0x10;
    static constexpr uint8_t OP_INTL = 0x11;
    static constexpr uint8_t OP_INTS = 0x12;
    static constexpr uint8_t OP_INTM = 0x13;
    static constexpr uint8_t OP_ITFP = 0x14;
    static constexpr uint8_t OP_FLTV = 0x15;
    static constexpr uint8_t OP_FLTI = 0x16;
    static constexpr uint8_t OP_FLTL = 0x17;
    static constexpr uint8_t OP_MISC = 0x18;
    static constexpr uint8_t OP_HW_MFPR = 0x19;
    static constexpr uint8_t OP_JSR = 0x1A;
    static constexpr uint8_t OP_HW_LD = 0x1B;
    static constexpr uint8_t OP_HW_ST = 0x1F;
    static constexpr uint8_t OP_LDF = 0x20;
    static constexpr uint8_t OP_LDG = 0x21;
    static constexpr uint8_t OP_LDS = 0x22;
    static constexpr uint8_t OP_LDT = 0x23;
    static constexpr uint8_t OP_STF = 0x24;
    static constexpr uint8_t OP_STG = 0x25;
    static constexpr uint8_t OP_STS = 0x26;
    static constexpr uint8_t OP_STT = 0x27;
    static constexpr uint8_t OP_LDL = 0x28;
    static constexpr uint8_t OP_LDQ = 0x29;
    static constexpr uint8_t OP_LDL_L = 0x2A;
    static constexpr uint8_t OP_LDQ_L = 0x2B;
    static constexpr uint8_t OP_STL = 0x2C;
    static constexpr uint8_t OP_STQ = 0x2D;
    static constexpr uint8_t OP_STL_C = 0x2E;
    static constexpr uint8_t OP_STQ_C = 0x2F;
    static constexpr uint8_t OP_BR = 0x30;
    static constexpr uint8_t OP_FBEQ = 0x31;
    static constexpr uint8_t OP_FBLT = 0x32;
    static constexpr uint8_t OP_FBLE = 0x33;
    static constexpr uint8_t OP_BSR = 0x34;
    static constexpr uint8_t OP_FBNE = 0x35;
    static constexpr uint8_t OP_FBGE = 0x36;
    static constexpr uint8_t OP_FBGT = 0x37;
    static constexpr uint8_t OP_BLBC = 0x38;
    static constexpr uint8_t OP_BEQ = 0x39;
    static constexpr uint8_t OP_BLT = 0x3A;
    static constexpr uint8_t OP_BLE = 0x3B;
    static constexpr uint8_t OP_BLBS = 0x3C;
    static constexpr uint8_t OP_BNE = 0x3D;
    static constexpr uint8_t OP_BGE = 0x3E;
    static constexpr uint8_t OP_BGT = 0x3F;

  public:
    /**
     * @brief Constructor - initialize processor integration manager
     */
    explicit alphaProcessorIntegrationManager(QObject *parent) : QObject(parent)
    {
        // Initialize execution context
        std::fill(m_context.integerRegisters.begin(), m_context.integerRegisters.end(), 0);
        std::fill(m_context.floatingRegisters.begin(), m_context.floatingRegisters.end(), 0.0);

        // Initialize memory reservation
        m_memoryReservation.valid = false;
        m_memoryReservation.address = 0;
        m_memoryReservation.size = 0;
        m_memoryReservation.timestamp = 0;
    }

    static QString getOverflowDocumentation()
    {
        return QString("Performance Counter Overflow Timeline:\n"
                       "• At 1 billion operations/sec: 584 years to overflow\n"
                       "• At 10 billion operations/sec: 58 years to overflow\n"
                       "• At 100 billion operations/sec: 5.8 years to overflow\n"
                       "• Use resetPerformanceCounters() for multi-year simulations");
    }

     void initialize_SignalsAndSlots()
    {
        // Set up Qt signal/slot connections
        // Timer-based performance updates, etc.
    }
    /**
     * @brief Destructor
     */
    ~alphaProcessorIntegrationManager() { shutdown(); }

    // Core lifecycle management
    bool initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        try
        {
            // Initialize system components
            initializeSystemComponents();

            // Set up cache hierarchy
            setupCacheHierarchy();

            // Initialize instruction factories
            initializeInstructionFactories();

            // Reset performance metrics
            resetPerformanceCounters();

            m_initialized = true;
            m_executionStartTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();

            DEBUG_LOG("Alpha processor integration manager initialized successfully");
            return true;
        }
        catch (const std::exception &e)
        {
            DEBUG_LOG("Failed to initialize processor: %s", e.what());
            return false;
        }
    }

    void reset()
    {
        DEBUG_LOG("Resetting Alpha processor integration manager");

        m_halted = true;

        // Reset execution context
        std::fill(m_context.integerRegisters.begin(), m_context.integerRegisters.end(), 0);
        std::fill(m_context.floatingRegisters.begin(), m_context.floatingRegisters.end(), 0.0);

        m_context.programCounter = 0;
        m_context.stackPointer = 0x7FFFFFFF0000ULL;
        m_context.linkRegister = 0;
        m_context.processorStatus = 0;
        m_context.floatingPointControl = 0;
        m_context.memoryManagementUnit = 0;
        m_context.cycleCounter = 0;
        m_context.interruptsEnabled = true;
        m_context.privilegedMode = false;
        m_context.processorId = 0;

        // Reset performance metrics
        resetPerformanceCounters();

        m_currentInstruction = 0;

        // Reset memory reservation
        m_memoryReservation.valid = false;
        m_memoryReservation.address = 0;
        m_memoryReservation.size = 0;
        m_memoryReservation.timestamp = 0;

        // Reset cache hierarchy
        if (m_instructionCache)
        {
            m_instructionCache->clear();
        }
        if (m_level1DataCache)
        {
            m_level1DataCache->flush();
            m_level1DataCache->clearStatistics();
        }
        if (m_level2Cache)
        {
            m_level2Cache->flush();
            m_level2Cache->clearStatistics();
        }
        if (m_level3Cache)
        {
            m_level3Cache->flush();
            m_level3Cache->clearStatistics();
        }

        // Reset TLB system
        if (m_memorySystem)
        {
            m_memorySystem->invalidateAllTLB(0);
        }

        m_executionStartTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        m_halted = false;

        DEBUG_LOG("Alpha processor reset completed");
    }

    void shutdown()
    {
        DEBUG_LOG("Shutting down Alpha processor integration manager");

        m_halted = true;
        m_initialized = false;

        // Dump final cache statistics
        dumpCacheStats();

        // Clean up cache hierarchy
        m_instructionCache.reset();
        m_level1DataCache.reset();
        m_level2Cache.reset();
        m_level3Cache = nullptr; // Owned by memory system

        // Clean up other components
        m_memorySystem.reset();
        m_tlbSystem = nullptr; // Owned by memory system
        m_registerFile.reset();
        m_branchPredictor.reset();
        m_perfCounters.reset();

        // Clear instruction factories
        m_instructionFactories.clear();

        // Final performance metrics dump
        DEBUG_LOG("Final performance metrics:");
        DEBUG_LOG("  Total instructions: %llu", m_metrics.totalInstructions);
        DEBUG_LOG("  Total cycles: %llu", m_metrics.totalCycles);
        DEBUG_LOG("  IPC: %.3f", m_metrics.ipc);
        DEBUG_LOG("  Cache hit rate: %.3f%%",
                  m_metrics.cacheHits + m_metrics.cacheMisses > 0
                      ? (100.0 * m_metrics.cacheHits) / (m_metrics.cacheHits + m_metrics.cacheMisses)
                      : 0.0);

        DEBUG_LOG("Alpha processor shutdown completed");
    }

    // Dynamic instruction registration
    bool registerInstructionType(uint8_t opcode, InstructionFactory factory)
    {
        if (!factory)
        {
            DEBUG_LOG("Attempted to register null instruction factory for opcode 0x%02X", opcode);
            return false;
        }

        auto existing = m_instructionFactories.find(opcode);
        if (existing != m_instructionFactories.end())
        {
            DEBUG_LOG("Warning: Overriding existing instruction factory for opcode 0x%02X", opcode);
        }

        m_instructionFactories[opcode] = factory;
        DEBUG_LOG("Registered instruction factory for opcode 0x%02X", opcode);
        return true;
    }

    // Core execution interface
    bool executeInstruction(uint32_t rawInstruction)
    {
        if (!m_initialized || m_halted)
        {
            return false;
        }

        m_currentInstruction = rawInstruction;

        auto decoded = decodeInstruction(rawInstruction);
        if (!decoded.instruction)
        {
            DEBUG_LOG("Failed to decode instruction: 0x%08X", rawInstruction);
            return false;
        }

        bool success = false;
        try
        {
            decoded.instruction->decode();
            success = decoded.instruction->execute();

            if (success)
            {
                decoded.instruction->writeback();
            }

            asa_utils::safeIncrement(m_metrics.totalInstructions);

            m_metrics.totalCycles += decoded.instruction->getCycleLatency();
            updatePerformanceMetrics();

            if (success && !isBranchInstruction(decoded.primaryOp))
            {
                incrementPC();
            }
        }
        catch (const std::exception &e)
        {
            DEBUG_LOG("Exception during instruction execution: %s", e.what());
            success = false;
        }

        return success;
    }

    bool executeCycle()
    {
        if (!m_initialized || m_halted)
        {
            return false;
        }

        updateCycleCounter();

        if (checkPendingInterrupts())
        {
            return true;
        }

        uint32_t instruction;
        if (!loadMemory(m_context.programCounter, reinterpret_cast<uint64_t *>(&instruction), 4))
        {
            DEBUG_LOG("Failed to fetch instruction at PC: 0x%016llX", m_context.programCounter);
            return false;
        }

        return executeInstruction(instruction);
    }

    // Register file access
    uint64_t getRegister(uint8_t reg) const { return (reg < 32) ? m_context.integerRegisters[reg] : 0; }

    void setRegister(uint8_t reg, uint64_t value)
    {
        if (reg < 31)
        { // R31 is hardwired to zero
            m_context.integerRegisters[reg] = value;
        }
    }

    double getFloatingRegister(uint8_t reg) const { return (reg < 32) ? m_context.floatingRegisters[reg] : 0.0; }

    void setFloatingRegister(uint8_t reg, double value)
    {
        if (reg < 32)
        {
            m_context.floatingRegisters[reg] = value;
        }
    }

    // Memory operations
    bool loadMemory(uint64_t address, uint64_t *value, uint32_t size = 8)
    {
        if (!validateMemoryAddress(address))
        {
            handleMemoryException(address, 0x1);
            return false;
        }

        // Try L1 data cache first
        if (m_level1DataCache)
        {
            if (m_level1DataCache->read(address, value, size))
            {
                m_metrics.cacheHits++;
                return true;
            }
        }

        m_metrics.cacheMisses++;

        // Go to memory system
        if (m_memorySystem)
        {
            return m_memorySystem->readPhysicalMemory(address, *value, size);
        }

        return false;
    }

    bool storeMemory(uint64_t address, uint64_t value, uint32_t size = 8)
    {
        if (!validateMemoryAddress(address))
        {
            handleMemoryException(address, 0x2);
            return false;
        }

        // Check memory reservations for LL/SC
        if (m_memoryReservation.valid)
        {
            uint64_t reservedStart = m_memoryReservation.address;
            uint64_t reservedEnd = reservedStart + m_memoryReservation.size;
            if (address >= reservedStart && address < reservedEnd)
            {
                m_memoryReservation.valid = false;
            }
        }

        // Write to L1 data cache
        if (m_level1DataCache)
        {
            bool success = m_level1DataCache->write(address, &value, size);

            if (success)
            {
                // Invalidate instruction cache line for self-modifying code
                if (m_instructionCache)
                {
                    m_instructionCache->invalidateLine(address);
                }
                return true;
            }
        }

        // Fallback to memory system
        if (m_memorySystem)
        {
            return m_memorySystem->writePhysicalMemory(address, value, size);
        }

        return false;
    }

    // Floating-point memory operations
    bool loadFloatingMemory(uint64_t address, double *value)
    {
        if (!value)
        {
            DEBUG_LOG("Null value pointer in loadFloatingMemory");
            return false;
        }

        if (!validateMemoryAddress(address))
        {
            handleMemoryException(address, 0x1);
            return false;
        }

        if (address & 0x7)
        {
            m_metrics.unalignedAccesses++;
            DEBUG_LOG("Unaligned floating point load at address 0x%016llX", address);
        }

        uint64_t rawValue;
        if (loadMemory(address, &rawValue, sizeof(double)))
        {
            *value = *reinterpret_cast<double *>(&rawValue);
            m_metrics.floatingPointOps++;
            return true;
        }

        return false;
    }

    bool storeFloatingMemory(uint64_t address, double value)
    {
        if (!validateMemoryAddress(address))
        {
            handleMemoryException(address, 0x2);
            return false;
        }

        if (address & 0x7)
        {
            m_metrics.unalignedAccesses++;
            DEBUG_LOG("Unaligned floating point store at address 0x%016llX", address);
        }

        uint64_t rawValue = *reinterpret_cast<const uint64_t *>(&value);

        if (storeMemory(address, rawValue, sizeof(double)))
        {
            m_metrics.floatingPointOps++;
            return true;
        }

        return false;
    }

    // Cache management
    void flushCache()
    {
        DEBUG_LOG("Flushing all cache levels");

        if (m_instructionCache)
        {
            m_instructionCache->flush();
        }
        if (m_level1DataCache)
        {
            m_level1DataCache->flush();
        }
        if (m_level2Cache)
        {
            m_level2Cache->flush();
        }
        if (m_level3Cache)
        {
            m_level3Cache->flush();
        }

        m_metrics.memoryStalls += 100;
        DEBUG_LOG("Cache flush completed");
    }

    void invalidateCache(uint64_t address)
    {
        DEBUG_LOG("Invalidating cache line for address 0x%016llX", address);

        if (m_instructionCache)
        {
            m_instructionCache->invalidateLine(address);
        }
        if (m_level1DataCache)
        {
            m_level1DataCache->invalidateLine(address);
        }
        if (m_level2Cache)
        {
            m_level2Cache->invalidateLine(address);
        }
        if (m_level3Cache)
        {
            m_level3Cache->invalidateLine(address);
        }

        m_metrics.memoryStalls += 10;
    }

    // Memory barriers
    void memoryBarrier()
    {
        DEBUG_LOG("Executing memory barrier");

        if (m_level1DataCache)
        {
            m_level1DataCache->writeBackAllDirty({});
        }
        if (m_level2Cache)
        {
            m_level2Cache->writeBackAllDirty({});
        }
        if (m_level3Cache)
        {
            m_level3Cache->writeBackAllDirty({});
        }

        if (m_memorySystem)
        {
            m_memorySystem->executeMemoryBarrier(memoryBarrierEmulationModeType::FULL_BARRIER, 0);
        }

        m_metrics.memoryStalls += 20;
        DEBUG_LOG("Memory barrier completed");
    }

    void instructionMemoryBarrier()
    {
        DEBUG_LOG("Executing instruction memory barrier");

        if (m_instructionCache)
        {
            m_instructionCache->flush();
        }

        if (m_memorySystem)
        {
            m_memorySystem->invalidateTlbSingleInstruction(0, 0);
        }

        m_metrics.memoryStalls += 50;
        DEBUG_LOG("Instruction memory barrier completed");
    }

    // Interrupt handling
    void handleInterrupt(uint32_t interruptVector)
    {
        DEBUG_LOG("Handling interrupt vector 0x%08X at PC 0x%016llX", interruptVector, m_context.programCounter);

        if (!m_context.interruptsEnabled)
        {
            DEBUG_LOG("Interrupt ignored - interrupts disabled");
            return;
        }

        uint64_t savedPS = m_context.processorStatus;
        m_context.interruptsEnabled = false;
        bool wasPrivileged = m_context.privilegedMode;
        m_context.privilegedMode = true;

        setRegister(26, m_context.programCounter);

        uint64_t handlerAddress = 0x8000 + (interruptVector * 0x10);
        m_context.programCounter = handlerAddress;
        m_context.processorStatus = (savedPS & 0xFFFFFFFFFFFFFFF0ULL) | (interruptVector & 0xF);

        DEBUG_LOG("Jumped to interrupt handler at 0x%016llX", handlerAddress);
    }

    bool checkPendingInterrupts()
    {
        static uint64_t lastTimerCheck = 0;
        if (m_context.cycleCounter - lastTimerCheck > 10000)
        {
            lastTimerCheck = m_context.cycleCounter;

            if (m_context.interruptsEnabled && (m_context.cycleCounter % 100000) == 0)
            {
                DEBUG_LOG("Timer interrupt pending at cycle %llu", m_context.cycleCounter);
                handleInterrupt(0x1);
                return true;
            }
        }

        return false;
    }

    // Context management
    const ExecutionContext &getContext() const { return m_context; }

    void saveContext(ExecutionContext &savedContext) const
    {
        DEBUG_LOG("Saving processor context");
        savedContext = m_context;
        DEBUG_LOG("Context saved - PC: 0x%016llX, PS: 0x%016llX", savedContext.programCounter,
                  savedContext.processorStatus);
    }

    void restoreContext(const ExecutionContext &savedContext)
    {
        DEBUG_LOG("Restoring processor context");
        DEBUG_LOG("Restoring - PC: 0x%016llX, PS: 0x%016llX", savedContext.programCounter,
                  savedContext.processorStatus);

        m_context = savedContext;
        m_memoryReservation.valid = false;
        m_metrics.memoryStalls += 50;

        DEBUG_LOG("Context restored successfully");
    }

    // Program counter management
    uint64_t getProgramCounter() const { return m_context.programCounter; }

    void setProgramCounter(uint64_t pc) { m_context.programCounter = pc; }

    void incrementPC() { m_context.programCounter += 4; }

    // Branch prediction
    bool predictBranch(uint64_t pc, uint32_t instruction)
    {
        if (m_branchPredictor)
        {
            return m_branchPredictor->predict(pc, instruction);
        }
        return false;
    }

    void updateBranchPrediction(uint64_t pc, bool taken, uint64_t target)
    {
        if (m_branchPredictor)
        {
            m_branchPredictor->update(pc, taken, target);
            if (!m_branchPredictor->wasCorrect())
            {
                m_metrics.branchMispredictions++;
            }
        }
    }

    // Exception handling
    void raiseException(uint32_t exceptionCode)
    {
        DEBUG_LOG("Exception raised: 0x%08X at PC: 0x%016llX", exceptionCode, m_context.programCounter);
        m_context.programCounter = 0x8000;
    }

    // PAL interface
    bool executePALCall(uint32_t palFunction, uint64_t argument)
    {
        m_metrics.palCalls++;

        auto palInstruction = std::make_unique<alphaCallPALInstruction>(
            0x00000000 | palFunction, static_cast<alphaCallPALInstruction::PALFunction>(palFunction));

        palInstruction->setArgumentValue(argument);
        return palInstruction->execute();
    }

    // Performance monitoring
    const PerformanceMetrics &getPerformanceMetrics() const { return m_metrics; }

    void resetPerformanceCounters()
    {
        m_metrics.resetAll();
        emit sigPerformanceUpdate(m_metrics);
    }

    void updatePerformanceMetrics()
    {
        // High-frequency: Direct atomic increment - fastest possible
        m_metrics.totalInstructions.fetch_add(1, std::memory_order_relaxed);
        m_metrics.totalCycles.fetch_add(1, std::memory_order_relaxed);

        // Low-frequency: Use asa_utils for Qt integration
        asa_utils::safeIncrement(m_metrics.branchMispredictions);

        // Calculate IPC (done periodically, not every instruction)
        static uint64_t lastIpcUpdate = 0;
        uint64_t currentCycles = m_metrics.totalCycles.load(std::memory_order_relaxed);

        if (currentCycles - lastIpcUpdate > 10000)
        { // Update IPC every 10K cycles
            uint64_t instructions = m_metrics.totalInstructions.load(std::memory_order_relaxed);
            if (currentCycles > 0)
            {
                m_metrics.ipc = static_cast<double>(instructions) / currentCycles;
            }
            lastIpcUpdate = currentCycles;
            emit sigPerformanceUpdate(m_metrics);
        }
    }

    // Debug interface
    void dumpRegisters() const
    {
        DEBUG_LOG("=== Alpha Register Dump ===");
        for (int i = 0; i < 32; i++)
        {
            DEBUG_LOG("R%02d: 0x%016llX  F%02d: %f", i, m_context.integerRegisters[i], i,
                      m_context.floatingRegisters[i]);
        }
        DEBUG_LOG("PC:  0x%016llX", m_context.programCounter);
        DEBUG_LOG("PS:  0x%016llX", m_context.processorStatus);
    }

    void dumpMemoryRegion(uint64_t start, uint64_t length) const
    {
        DEBUG_LOG("=== Memory Dump: 0x%016llX - 0x%016llX ===", start, start + length - 1);

        if (!validateMemoryAddress(start) || !validateMemoryAddress(start + length - 1))
        {
            DEBUG_LOG("Invalid memory address range for dump");
            return;
        }

        const uint32_t bytesPerLine = 16;
        const uint32_t maxLines = 64;
        uint32_t lineCount = 0;

        for (uint64_t addr = start; addr < start + length && lineCount < maxLines; addr += bytesPerLine)
        {
            uint64_t remainingBytes = std::min(static_cast<uint64_t>(bytesPerLine), start + length - addr);

            char line[256];
            int pos = snprintf(line, sizeof(line), "0x%016llX: ", addr);

            for (uint64_t i = 0; i < remainingBytes; i++)
            {
                uint64_t value;
                if (m_memorySystem && m_memorySystem->readPhysicalMemory(addr + i, value, 1))
                {
                    pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", static_cast<uint8_t>(value));
                }
                else
                {
                    pos += snprintf(line + pos, sizeof(line) - pos, "?? ");
                }
            }

            for (uint64_t i = remainingBytes; i < bytesPerLine; i++)
            {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            }

            pos += snprintf(line + pos, sizeof(line) - pos, " |");
            for (uint64_t i = 0; i < remainingBytes; i++)
            {
                uint64_t value;
                if (m_memorySystem && m_memorySystem->readPhysicalMemory(addr + i, value, 1))
                {
                    uint8_t byte = static_cast<uint8_t>(value);
                    char c = (byte >= 32 && byte <= 126) ? byte : '.';
                    pos += snprintf(line + pos, sizeof(line) - pos, "%c", c);
                }
                else
                {
                    pos += snprintf(line + pos, sizeof(line) - pos, "?");
                }
            }
            snprintf(line + pos, sizeof(line) - pos, "|");

            DEBUG_LOG("%s", line);
            lineCount++;
        }

        if (lineCount >= maxLines && start + length > start + (maxLines * bytesPerLine))
        {
            DEBUG_LOG("... (output truncated, %llu more bytes)", start + length - (start + maxLines * bytesPerLine));
        }

        DEBUG_LOG("=== End Memory Dump ===");
    }

    std::string getInstructionDisassembly(uint32_t instruction) const
    {
        auto decoded = const_cast<alphaProcessorIntegrationManager *>(this)->decodeInstruction(instruction);

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "0x%08X: op=0x%02X ra=%d rb=%d rc=%d disp=%d", instruction, decoded.primaryOp,
                 decoded.ra, decoded.rb, decoded.rc, decoded.displacement);

        return std::string(buffer);
    }

    // Accessor methods for cache components
    AlphaInstructionCache *getInstructionCache() const { return m_instructionCache.get(); }
    UnifiedDataCache *getL1DataCache() const { return m_level1DataCache.get(); }
    UnifiedDataCache *getL2Cache() const { return m_level2Cache.get(); }
    UnifiedDataCache *getL3Cache() const { return m_level3Cache; }
    TLBSystem *getTLBSystem() const { return m_tlbSystem; }
    AlphaMemorySystem *getMemorySystem() const { return m_memorySystem.get(); }

    // Cache statistics
    void dumpCacheStats() const
    {
        DEBUG_LOG("=== Cache Statistics ===");

        if (m_instructionCache)
        {
            auto stats = m_instructionCache->getStatistics();
            DEBUG_LOG("I-Cache: Hits=%llu, Misses=%llu, Hit Rate=%.2f%%", stats.hits, stats.misses, stats.getHitRate());
        }

        if (m_level1DataCache)
        {
            auto stats = m_level1DataCache->getStatistics();
            DEBUG_LOG("L1D: Hits=%llu, Misses=%llu, Hit Rate=%.2f%%", stats.hits, stats.misses, stats.getHitRate());
        }

        if (m_level2Cache)
        {
            auto stats = m_level2Cache->getStatistics();
            DEBUG_LOG("L2: Hits=%llu, Misses=%llu, Hit Rate=%.2f%%", stats.hits, stats.misses, stats.getHitRate());
        }

        if (m_level3Cache)
        {
            auto stats = m_level3Cache->getStatistics();
            DEBUG_LOG("L3: Hits=%llu, Misses=%llu, Hit Rate=%.2f%%", stats.hits, stats.misses, stats.getHitRate());
        }
    }

  private:
    // Initialize system components
    void initializeSystemComponents()
    {
        // Initialize memory system first - it creates the internal TLB system
        m_memorySystem = std::make_unique<AlphaMemorySystem>();

        // Get TLB system from memory system
        m_tlbSystem = m_memorySystem->getTlbSystem();

        // Initialize register file
        m_registerFile = std::make_unique<alphaRegisterFile>();

        // Initialize branch predictor
        m_branchPredictor = std::make_unique<alphaBranchPredictor>();

        // Initialize performance counters
        m_perfCounters = std::make_unique<alphaPerformanceCounters>();

        DEBUG_LOG("Alpha system components initialized");
    }

    // Set up cache hierarchy
    void setupCacheHierarchy()
    {
        // Create L3 shared cache using memory system
        UnifiedDataCache::Config l3Config;
        l3Config.numSets = 1024;
        l3Config.associativity = 16;
        l3Config.lineSize = 64;
        l3Config.totalSize = 1024 * 16 * 64; // 1MB L3
        l3Config.enableCoherency = true;
        l3Config.coherencyProtocol = "MESI";

        m_level3Cache = m_memorySystem->createL3Cache(l3Config);

        // Create L2 unified cache
        UnifiedDataCache::Config l2Config;
        l2Config.numSets = 512;
        l2Config.associativity = 8;
        l2Config.lineSize = 64;
        l2Config.totalSize = 512 * 8 * 64; // 256KB L2
        l2Config.enableCoherency = true;

        m_level2Cache = std::make_unique<UnifiedDataCache>(l2Config);
        m_level2Cache->setNextLevel(m_level3Cache);

        // Create L1 data cache
        UnifiedDataCache::Config l1dConfig;
        l1dConfig.numSets = 256;
        l1dConfig.associativity = 4;
        l1dConfig.lineSize = 64;
        l1dConfig.totalSize = 256 * 4 * 64; // 64KB L1D
        l1dConfig.enableCoherency = true;

        m_level1DataCache = std::make_unique<UnifiedDataCache>(l1dConfig);
        m_level1DataCache->setNextLevel(m_level2Cache.get());

        // Create instruction cache
        CacheConfig iCacheConfig;
        iCacheConfig.cacheSize = 65536;
        iCacheConfig.lineSize = 64;
        iCacheConfig.associativity = 4;
        iCacheConfig.autoPrefetchEnabled = true;

        m_instructionCache = std::make_unique<AlphaInstructionCache>(nullptr, m_memorySystem.get(), iCacheConfig, 0);

        // Connect instruction cache to L2
        if (m_instructionCache->getUnifiedCache())
        {
            m_instructionCache->getUnifiedCache()->setNextLevel(m_level2Cache.get());
        }

        // Set up TLB integration
        if (m_tlbSystem)
        {
            m_level1DataCache->setTLBSystem(m_tlbSystem, 0);
            m_level2Cache->setTLBSystem(m_tlbSystem, 0);
            if (m_instructionCache->getUnifiedCache())
            {
                m_instructionCache->getUnifiedCache()->setTLBSystem(m_tlbSystem, 0);
            }
        }

        DEBUG_LOG("Cache hierarchy established");
    }

    // Initialize instruction factories
    void initializeInstructionFactories()
    {
        // Integer arithmetic instructions
        m_instructionFactories[OP_INTA] = [this](uint32_t opcode, uint32_t instruction)
        { return createIntegerInstruction(opcode, instruction); };

        // Memory instructions
        m_instructionFactories[OP_LDA] = [this](uint32_t opcode, uint32_t instruction)
        { return createMemoryInstruction(opcode, instruction); };
        m_instructionFactories[OP_LDAH] = [this](uint32_t opcode, uint32_t instruction)
        { return createMemoryInstruction(opcode, instruction); };
        m_instructionFactories[OP_LDL] = [this](uint32_t opcode, uint32_t instruction)
        { return createMemoryInstruction(opcode, instruction); };
        m_instructionFactories[OP_LDQ] = [this](uint32_t opcode, uint32_t instruction)
        { return createMemoryInstruction(opcode, instruction); };
        m_instructionFactories[OP_STL] = [this](uint32_t opcode, uint32_t instruction)
        { return createMemoryInstruction(opcode, instruction); };
        m_instructionFactories[OP_STQ] = [this](uint32_t opcode, uint32_t instruction)
        { return createMemoryInstruction(opcode, instruction); };

        // Load-locked/Store-conditional
        m_instructionFactories[OP_LDL_L] = [this](uint32_t opcode, uint32_t instruction)
        {
            auto llsc = std::make_unique<alphaLoadStoreConditionalInstruction>();
            llsc->setupMemoryAccess(calculateEffectiveAddress(instruction), 4,
                                    alphaLoadStoreConditionalInstruction::LSCOperation::LDL_L);
            return std::move(llsc);
        };
        m_instructionFactories[OP_LDQ_L] = [this](uint32_t opcode, uint32_t instruction)
        {
            auto llsc = std::make_unique<alphaLoadStoreConditionalInstruction>();
            llsc->setupMemoryAccess(calculateEffectiveAddress(instruction), 8,
                                    alphaLoadStoreConditionalInstruction::LSCOperation::LDQ_L);
            return std::move(llsc);
        };

        // Branch instructions
        m_instructionFactories[OP_BR] = [this](uint32_t opcode, uint32_t instruction)
        { return createBranchInstruction(opcode, instruction); };
        m_instructionFactories[OP_BSR] = [this](uint32_t opcode, uint32_t instruction)
        { return createBranchInstruction(opcode, instruction); };
        m_instructionFactories[OP_BEQ] = [this](uint32_t opcode, uint32_t instruction)
        { return createBranchInstruction(opcode, instruction); };
        m_instructionFactories[OP_BNE] = [this](uint32_t opcode, uint32_t instruction)
        { return createBranchInstruction(opcode, instruction); };

        // PAL instructions
        m_instructionFactories[OP_PAL] = [this](uint32_t opcode, uint32_t instruction)
        { return createPALInstruction(opcode, instruction); };
    }

    // Instruction creation helpers
    std::unique_ptr<alphaInstructionBase> createIntegerInstruction(uint32_t opcode, uint32_t instruction)
    {
        uint8_t ra = (instruction >> 21) & 0x1F;
        uint8_t rb = (instruction >> 16) & 0x1F;
        uint8_t rc = instruction & 0x1F;
        uint8_t function = (instruction >> 5) & 0x7F;
        bool isLiteral = (instruction >> 12) & 0x1;
        uint8_t literal = (instruction >> 13) & 0xFF;

        alphaIntegerInstruction::IntegerOpType opType;
        switch (function)
        {
        case 0x00:
            opType = alphaIntegerInstruction::IntegerOpType::ADD;
            break;
        case 0x09:
            opType = alphaIntegerInstruction::IntegerOpType::SUB;
            break;
        case 0x10:
            opType = alphaIntegerInstruction::IntegerOpType::MUL;
            break;
        case 0x1E:
            opType = alphaIntegerInstruction::IntegerOpType::DIV;
            break;
        default:
            opType = alphaIntegerInstruction::IntegerOpType::ADD;
            break;
        }

        if (isLiteral)
        {
            return std::make_unique<alphaIntegerInstruction>(opcode, opType, rc, ra, static_cast<int16_t>(literal));
        }
        else
        {
            return std::make_unique<alphaIntegerInstruction>(opcode, opType, rc, ra, rb);
        }
    }

    std::unique_ptr<alphaInstructionBase> createMemoryInstruction(uint32_t opcode, uint32_t instruction)
    {
        uint8_t ra = (instruction >> 21) & 0x1F;
        uint8_t rb = (instruction >> 16) & 0x1F;
        int16_t displacement = static_cast<int16_t>(instruction & 0xFFFF);

        alphaMemoryInstruction::MemoryOpType opType;
        switch (opcode)
        {
        case OP_LDA:
            opType = alphaMemoryInstruction::MemoryOpType::LDA;
            break;
        case OP_LDAH:
            opType = alphaMemoryInstruction::MemoryOpType::LDAH;
            break;
        case OP_LDL:
            opType = alphaMemoryInstruction::MemoryOpType::LDL;
            break;
        case OP_LDQ:
            opType = alphaMemoryInstruction::MemoryOpType::LDQ;
            break;
        case OP_STL:
            opType = alphaMemoryInstruction::MemoryOpType::STL;
            break;
        case OP_STQ:
            opType = alphaMemoryInstruction::MemoryOpType::STQ;
            break;
        default:
            opType = alphaMemoryInstruction::MemoryOpType::LDQ;
            break;
        }

        return std::make_unique<alphaMemoryInstruction>(opcode, opType, ra, rb, displacement);
    }

    std::unique_ptr<alphaInstructionBase> createBranchInstruction(uint32_t opcode, uint32_t instruction)
    {
        uint8_t ra = (instruction >> 21) & 0x1F;
        int32_t displacement = static_cast<int32_t>((instruction & 0x1FFFFF) << 11) >> 9;

        alphaBranchInstruction::BranchOpType opType;
        switch (opcode)
        {
        case OP_BR:
            opType = alphaBranchInstruction::BranchOpType::BR;
            break;
        case OP_BSR:
            opType = alphaBranchInstruction::BranchOpType::BSR;
            break;
        case OP_BEQ:
            opType = alphaBranchInstruction::BranchOpType::BEQ;
            break;
        case OP_BNE:
            opType = alphaBranchInstruction::BranchOpType::BNE;
            break;
        default:
            opType = alphaBranchInstruction::BranchOpType::BR;
            break;
        }

        return std::make_unique<alphaBranchInstruction>(opcode, opType, ra, displacement);
    }

    std::unique_ptr<alphaInstructionBase> createPALInstruction(uint32_t opcode, uint32_t instruction)
    {
        uint32_t palFunction = instruction & 0x3FFFFFF;
        return std::make_unique<alphaCallPALInstruction>(
            opcode, static_cast<alphaCallPALInstruction::PALFunction>(palFunction));
    }

    // Instruction decoding
    DecodedInstruction decodeInstruction(uint32_t rawInstruction)
    {
        DecodedInstruction decoded;
        decoded.opcode = rawInstruction;

        decoded.primaryOp = (rawInstruction >> 26) & 0x3F;
        decoded.ra = (rawInstruction >> 21) & 0x1F;
        decoded.rb = (rawInstruction >> 16) & 0x1F;
        decoded.rc = (rawInstruction >> 0) & 0x1F;
        decoded.function = (rawInstruction >> 5) & 0x7F;
        decoded.displacement = static_cast<int16_t>(rawInstruction & 0xFFFF);
        decoded.literal = (rawInstruction >> 13) & 0xFF;
        decoded.isLiteral = (rawInstruction >> 12) & 0x1;
        decoded.branchDisplacement = static_cast<int32_t>((rawInstruction & 0x1FFFFF) << 11) >> 9;

        auto factory = m_instructionFactories.find(decoded.primaryOp);
        if (factory != m_instructionFactories.end())
        {
            decoded.instruction = factory->second(decoded.primaryOp, rawInstruction);
        }
        else
        {
            DEBUG_LOG("Unknown instruction opcode: 0x%02X", decoded.primaryOp);
            decoded.instruction = nullptr;
        }

        return decoded;
    }
    void attachAlphaInstructionCache(std::unique_ptr<AlphaInstructionCache> ins_cache)
    {
        m_instructionCache = std::move(ins_cache);
        DEBUG_LOG("External instruction cache attached");
    }

    void attachL1DataCache(std::unique_ptr<UnifiedDataCache> l1cache)
    {
        m_level1DataCache = std::move(l1cache);
        DEBUG_LOG("External L1 data cache attached");
    }

    void attachL2Cache(std::unique_ptr<UnifiedDataCache> l2cache)
    {
        m_level2Cache = std::move(l2cache);
        DEBUG_LOG("External L2 cache attached");
    }

    void attachL3Cache(UnifiedDataCache *l3cache)
    {
        m_level3Cache = l3cache; // L3 is owned by memory system, so raw pointer is correct
        DEBUG_LOG("External L3 cache attached");
    }

    void attachAlphaMemorySystem(std::unique_ptr<AlphaMemorySystem> memSys)
    {
        m_memorySystem = std::move(memSys);
        // Update TLB system reference when memory system changes
        m_tlbSystem = m_memorySystem ? m_memorySystem->getTlbSystem() : nullptr;
        DEBUG_LOG("External memory system attached");
    }

    void attachTLBSystem(TLBSystem *tlbSys)
    {
        m_tlbSystem = tlbSys; // TLB system is owned by memory system, so raw pointer is correct
        DEBUG_LOG("External TLB system attached");
    }

    // Alternative attach methods for raw pointers (when ownership is managed externally)
    void attachAlphaInstructionCachePtr(AlphaInstructionCache *ins_cache)
    {
        m_instructionCache.reset(ins_cache);
        DEBUG_LOG("External instruction cache pointer attached");
    }

    void attachL1DataCachePtr(UnifiedDataCache *l1cache)
    {
        m_level1DataCache.reset(l1cache);
        DEBUG_LOG("External L1 data cache pointer attached");
    }

    void attachL2CachePtr(UnifiedDataCache *l2cache)
    {
        m_level2Cache.reset(l2cache);
        DEBUG_LOG("External L2 cache pointer attached");
    }

    void attachAlphaMemorySystemPtr(AlphaMemorySystem *memSys)
    {
        m_memorySystem.reset(memSys);
        // Update TLB system reference when memory system changes
        m_tlbSystem = m_memorySystem ? m_memorySystem->getTlbSystem() : nullptr;
        DEBUG_LOG("External memory system pointer attached");
    }

    // Component detachment methods
    void detachInstructionCache()
    {
        m_instructionCache.reset();
        DEBUG_LOG("Instruction cache detached");
    }

    void detachL1DataCache()
    {
        m_level1DataCache.reset();
        DEBUG_LOG("L1 data cache detached");
    }

    void detachL2Cache()
    {
        m_level2Cache.reset();
        DEBUG_LOG("L2 cache detached");
    }

    void detachL3Cache()
    {
        m_level3Cache = nullptr;
        DEBUG_LOG("L3 cache detached");
    }

    void detachMemorySystem()
    {
        m_memorySystem.reset();
        m_tlbSystem = nullptr; // Clear TLB reference as well
        DEBUG_LOG("Memory system detached");
    }

    void detachTLBSystem()
    {
        m_tlbSystem = nullptr;
        DEBUG_LOG("TLB system detached");
    }

    // Component validation after attachment
    bool validateAttachedComponents() const
    {
        bool valid = true;

        if (!m_memorySystem)
        {
            DEBUG_LOG("Warning: No memory system attached");
            valid = false;
        }

        if (!m_instructionCache)
        {
            DEBUG_LOG("Warning: No instruction cache attached");
            valid = false;
        }

        if (!m_level1DataCache)
        {
            DEBUG_LOG("Warning: No L1 data cache attached");
            valid = false;
        }

        if (!m_level2Cache)
        {
            DEBUG_LOG("Warning: No L2 cache attached");
            valid = false;
        }

        // L3 cache and TLB system are optional in some configurations
        if (!m_level3Cache)
        {
            DEBUG_LOG("Info: No L3 cache attached (optional)");
        }

        if (!m_tlbSystem)
        {
            DEBUG_LOG("Info: No TLB system attached (optional)");
        }

        return valid;
    }

    // Rebuild cache hierarchy after component attachment
    bool rebuildCacheHierarchy()
    {
        DEBUG_LOG("Rebuilding cache hierarchy with attached components");

        // Set up cache chain: L1D -> L2 -> L3
        if (m_level1DataCache && m_level2Cache)
        {
            m_level1DataCache->setNextLevel(m_level2Cache.get());
            DEBUG_LOG("L1D -> L2 connection established");
        }

        if (m_level2Cache && m_level3Cache)
        {
            m_level2Cache->setNextLevel(m_level3Cache);
            DEBUG_LOG("L2 -> L3 connection established");
        }

        // Connect instruction cache to L2
        if (m_instructionCache && m_level2Cache)
        {
            if (m_instructionCache->getUnifiedCache())
            {
                m_instructionCache->getUnifiedCache()->setNextLevel(m_level2Cache.get());
                DEBUG_LOG("I-Cache -> L2 connection established");
            }
        }

        // Set up TLB integration for all caches
        if (m_tlbSystem)
        {
            if (m_level1DataCache)
            {
                m_level1DataCache->setTLBSystem(m_tlbSystem, 0);
                DEBUG_LOG("L1D TLB integration enabled");
            }

            if (m_level2Cache)
            {
                m_level2Cache->setTLBSystem(m_tlbSystem, 0);
                DEBUG_LOG("L2 TLB integration enabled");
            }

            if (m_instructionCache && m_instructionCache->getUnifiedCache())
            {
                m_instructionCache->getUnifiedCache()->setTLBSystem(m_tlbSystem, 0);
                DEBUG_LOG("I-Cache TLB integration enabled");
            }
        }

        return validateAttachedComponents();
    }
    // Helper methods
    uint64_t calculateEffectiveAddress(uint32_t instruction) const
    {
        uint8_t ra = (instruction >> 21) & 0x1F;
        int16_t displacement = static_cast<int16_t>(instruction & 0xFFFF);

        uint64_t baseAddress = (ra == 31) ? 0 : getRegister(ra);
        return baseAddress + static_cast<int64_t>(displacement);
    }

    bool validateMemoryAddress(uint64_t address) const { return (address < 0x8000000000000000ULL); }

    void handleMemoryException(uint64_t address, uint32_t exceptionType)
    {
        DEBUG_LOG("Memory exception: type=0x%X, address=0x%016llX", exceptionType, address);
    }

    bool isBranchInstruction(uint8_t opcode) const
    {
        return (opcode >= OP_BR && opcode <= OP_BGT) || (opcode == OP_JSR);
    }

    void updateCycleCounter() { m_context.cycleCounter++; }
};

#if defined(_WIN32) && defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif // ALPHA_PROCESSOR_INTEGRATION_MANAGER_H