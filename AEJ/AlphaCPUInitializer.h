// Alpha Memory System Integration Guide
// Complete setup and coordination between TLB, caches, and pipeline stages

#pragma once

#include "AlphaCPU.h"
#include "AlphaMemorySystem.h"
#include "UnifiedDataCache.h"
#include "AlphaMemorySystem_refactored.h"

/**
 * @brief Complete initialization sequence for Alpha CPU with all subsystems
 */
class AlphaCPUInitializer
{
  public:
    /**
     * @brief Initialize a complete Alpha CPU with all subsystems
     */
    static std::unique_ptr<AlphaCPU> createAlphaCPU(quint16 cpuId, AlphaSMPManager *smpManager)
    {
        auto cpu = std::make_unique<AlphaCPU>(cpuId);

        // Step 1: Initialize memory subsystems first
        initializeMemorySubsystems(cpu.get(), smpManager);

        // Step 2: Initialize caches
        initializeCaches(cpu.get());

        // Step 3: Initialize TLB system
        initializeTLBSystem(cpu.get());

        // Step 4: Initialize pipeline stages with all dependencies
        initializePipelineStages(cpu.get());

        // Step 5: Set up coordination and signals
        setupCoordination(cpu.get());

        return cpu;
    }

  private:
    static void initializeMemorySubsystems(AlphaCPU *cpu, AlphaSMPManager *smpManager)
    {
        // Get shared memory system from SMP manager
        AlphaMemorySystem *memSys = smpManager->getMemorySystem();
        cpu->attachMemorySystem(memSys);

        // Get shared safe memory and MMIO manager
        SafeMemory *safeMem = smpManager->getSafeMemory();
        MMIOManager *mmioMgr = smpManager->getMMIOManager();

        memSys->attachSafeMemory(safeMem);
        memSys->attachMMIOManager(mmioMgr);
        memSys->attachAlphaCPU(cpu);
    }

    static void initializeCaches(AlphaCPU *cpu)
    {
        // Create L1 data cache (private to this CPU)
        auto l1Cache = std::make_unique<UnifiedDataCache>(cpu);
        cpu->attachL1DataCache(l1Cache.release());

        // Get shared L2 cache from SMP manager
        UnifiedDataCache *l2Cache = cpu->getSMPManager()->getL2Cache();
        cpu->attachL2DataCache(l2Cache);

        // Create instruction cache
        auto iCache = std::make_unique<InstructionCache>(cpu);
        cpu->attachInstructionCache(iCache.release());
    }

    static void initializeTLBSystem(AlphaCPU *cpu)
    {
//         Get shared TLB system from SMP manager
//                 TLBSystem *tlbSys = cpu->getSMPManager()->getTLBSystem();
//                 cpu->attachTlbSystem(tlbSys);
//         
//                 // Initialize CPU-specific TLB entries
//                 tlbSys->initializeCPU(cpu->getCpuId());
    }

    static void initializePipelineStages(AlphaCPU *cpu)
    {
        // Create pipeline stages with dependency injection
        auto fetchUnit = std::make_unique<FetchUnit>(cpu);
        auto decodeStage = std::make_unique<DecodeStage>(cpu);
        auto executeStage = std::make_unique<ExecuteStage>(cpu);
        auto writebackStage = std::make_unique<WritebackStage>(cpu);

        // Inject all dependencies into each stage
        injectDependencies(fetchUnit.get(), cpu);
        injectDependencies(decodeStage.get(), cpu);
        injectDependencies(executeStage.get(), cpu);
        injectDependencies(writebackStage.get(), cpu);

        // Set up pipeline stage connections
        connectPipelineStages(fetchUnit.get(), decodeStage.get(), executeStage.get(), writebackStage.get());

        // Transfer ownership to CPU
        cpu->setPipelineStages(fetchUnit.release(), decodeStage.release(), executeStage.release(),
                               writebackStage.release());
    }

    template <typename StageType> static void injectDependencies(StageType *stage, AlphaCPU *cpu)
    {
        // Inject CPU context
        stage->attachAlphaCPU(cpu);

        // Inject memory system
        stage->attachMemorySystem(cpu->getMemorySystem());

        // Inject appropriate caches
        if constexpr (std::is_same_v<StageType, FetchUnit>)
        {
            stage->attachInstructionCache(cpu->getInstructionCache());
        }

        if constexpr (std::is_same_v<StageType, ExecuteStage>)
        {
            stage->attachDataCache(cpu->getL1DataCache());
            stage->attachL2Cache(cpu->getL2DataCache());
        }

        // Inject TLB system
        stage->attachTLBSystem(cpu->getTLBSystem());

        // Inject register file
        stage->attachRegisterBank(cpu->getRegisterBank());
    }

    static void setupCoordination(AlphaCPU *cpu)
    {
        // Set up TLB coordination
        setupTLBCoordination(cpu);

        // Set up cache coordination
        setupCacheCoordination(cpu);

        // Set up pipeline coordination
        setupPipelineCoordination(cpu);

        // Set up SMP coordination
        setupSMPCoordination(cpu);
    }
};

/**
 * @brief Enhanced AlphaCPU with proper TLB and cache coordination
 */
class AlphaCPU : public QObject, public IExecutionContext
{
    Q_OBJECT

  private:
    // Pipeline stages
    QScopedPointer<FetchUnit> m_fetchUnit;
    QScopedPointer<DecodeStage> m_decodeStage;
    QScopedPointer<ExecuteStage> m_executeStage;
    QScopedPointer<WritebackStage> m_writebackStage;

    // Memory subsystems
    AlphaMemorySystem *m_memorySystem;
    UnifiedDataCache *m_l1DataCache;
    UnifiedDataCache *m_l2DataCache;
    InstructionCache *m_instructionCache;
    TLBSystem *m_tlbSystem;

  public:
    /**
     * @brief Execute instruction with full TLB and cache coordination
     */
    void executeInstruction()
    {
        try
        {
            // 1. Fetch instruction (with ITLB and I-cache)
            quint32 instruction;
            if (!fetchInstructionWithTLB(m_pc, instruction))
            {
                return; // TLB miss or cache miss handled
            }

            // 2. Decode instruction
            DecodedInstruction decoded;
            if (!decodeInstructionSafely(instruction, decoded))
            {
                handleIllegalInstruction(instruction);
                return;
            }

            // 3. Execute with memory operations (DTLB and D-cache)
            if (!executeInstructionWithMemory(decoded))
            {
                return; // Memory fault handled
            }

            // 4. Writeback results
            writebackResults(decoded);

            // 5. Update PC and performance counters
            updateProgramCounter(decoded);
            updatePerformanceCounters();
        }
        catch (const MemoryException &e)
        {
            handleMemoryException(e);
        }
        catch (const excTLBException &e)
        {
            handleTLBException(e);
        }
    }

  private:
    /**
     * @brief Fetch instruction with TLB translation and cache lookup
     */
    bool fetchInstructionWithTLB(quint64 pc, quint32 &instruction)
    {
        // 1. Check for alignment
        if (pc & 0x3)
        {
            triggerException(ExceptionType::ALIGNMENT_FAULT, pc);
            return false;
        }

        // 2. Translate virtual to physical address via ITLB
        quint64 physicalAddr;
        if (!m_tlbSystem->translateInstruction(pc, physicalAddr, getCurrentASN()))
        {
            // ITLB miss - try to fill from page tables
            if (handleInstructionTLBMiss(pc))
            {
                // Retry translation after TLB fill
                if (!m_tlbSystem->translateInstruction(pc, physicalAddr, getCurrentASN()))
                {
                    triggerException(ExceptionType::PAGE_FAULT, pc);
                    return false;
                }
            }
            else
            {
                return false; // Exception already triggered
            }
        }

        // 3. Try instruction cache first
        if (m_instructionCache && m_instructionCache->read(physicalAddr, instruction))
        {
            // I-cache hit
            updatePerformanceCounter(PERF_ICACHE_HIT);
            return true;
        }

        // 4. I-cache miss - fetch from memory
        updatePerformanceCounter(PERF_ICACHE_MISS);
        if (!m_memorySystem->readPhysicalMemory(physicalAddr, instruction, 4))
        {
            triggerException(ExceptionType::MACHINE_CHECK, pc);
            return false;
        }

        // 5. Fill instruction cache
        if (m_instructionCache)
        {
            m_instructionCache->fill(physicalAddr, instruction);
        }

        return true;
    }

    /**
     * @brief Execute instruction with memory operations using DTLB and D-cache
     */
    bool executeInstructionWithMemory(const DecodedInstruction &decoded)
    {
        // Handle different instruction types
        switch (decoded.type)
        {
        case InstructionType::MEMORY_LOAD:
            return executeMemoryLoad(decoded);

        case InstructionType::MEMORY_STORE:
            return executeMemoryStore(decoded);

        case InstructionType::MEMORY_LOAD_LOCKED:
            return executeLoadLocked(decoded);

        case InstructionType::MEMORY_STORE_CONDITIONAL:
            return executeStoreConditional(decoded);

        case InstructionType::ARITHMETIC:
        case InstructionType::LOGICAL:
        case InstructionType::BRANCH:
            return executeNonMemoryInstruction(decoded);

        default:
            return executeNonMemoryInstruction(decoded);
        }
    }

    /**
     * @brief Execute memory load with DTLB and cache coordination
     */
    bool executeMemoryLoad(const DecodedInstruction &decoded)
    {
        quint64 virtualAddr = calculateEffectiveAddress(decoded);

        // 1. Check alignment
        if (!isProperlyAligned(virtualAddr, decoded.memorySize))
        {
            triggerException(ExceptionType::ALIGNMENT_FAULT, virtualAddr);
            return false;
        }

        // 2. Translate via DTLB
        quint64 physicalAddr;
        if (!m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), false))
        {
            // DTLB miss - try to fill from page tables
            if (handleDataTLBMiss(virtualAddr, false))
            {
                // Retry translation after TLB fill
                if (!m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), false))
                {
                    triggerException(ExceptionType::PAGE_FAULT, virtualAddr);
                    return false;
                }
            }
            else
            {
                return false; // Exception already triggered
            }
        }

        // 3. Try L1 data cache first
        quint64 data;
        if (m_l1DataCache && m_l1DataCache->contains(physicalAddr))
        {
            // L1 hit
            bool success = m_l1DataCache->read(physicalAddr, &data, decoded.memorySize,
                                               [this](quint64 addr, void *buf, size_t size)
                                               { return m_memorySystem->readBlock(addr, buf, size); });

            if (success)
            {
                updatePerformanceCounter(PERF_L1_DCACHE_HIT);
                writeRegister(decoded.destReg, data);
                return true;
            }
        }

        // 4. L1 miss - try L2 cache
        updatePerformanceCounter(PERF_L1_DCACHE_MISS);
        if (m_l2DataCache && m_l2DataCache->contains(physicalAddr))
        {
            // L2 hit - fill L1 and complete load
            bool success = m_l2DataCache->read(physicalAddr, &data, decoded.memorySize,
                                               [this](quint64 addr, void *buf, size_t size)
                                               { return m_memorySystem->readBlock(addr, buf, size); });

            if (success)
            {
                updatePerformanceCounter(PERF_L2_CACHE_HIT);

                // Fill L1 cache
                if (m_l1DataCache)
                {
                    m_l1DataCache->write(physicalAddr, &data, decoded.memorySize,
                                         [this](quint64 addr, const void *buf, size_t size)
                                         { return m_memorySystem->writeBlock(addr, buf, size); });
                }

                writeRegister(decoded.destReg, data);
                return true;
            }
        }

        // 5. L2 miss - access main memory
        updatePerformanceCounter(PERF_L2_CACHE_MISS);
        if (!m_memorySystem->readBlock(physicalAddr, &data, decoded.memorySize))
        {
            triggerException(ExceptionType::MACHINE_CHECK, virtualAddr);
            return false;
        }

        // 6. Fill cache hierarchy
        fillCacheHierarchy(physicalAddr, &data, decoded.memorySize, false);

        writeRegister(decoded.destReg, data);
        return true;
    }

    /**
     * @brief Execute memory store with cache coherency
     */
    bool executeMemoryStore(const DecodedInstruction &decoded)
    {
        quint64 virtualAddr = calculateEffectiveAddress(decoded);
        quint64 data = readRegister(decoded.srcReg);

        // 1. Check alignment
        if (!isProperlyAligned(virtualAddr, decoded.memorySize))
        {
            triggerException(ExceptionType::ALIGNMENT_FAULT, virtualAddr);
            return false;
        }

        // 2. Translate via DTLB
        quint64 physicalAddr;
        if (!m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), true))
        {
            if (handleDataTLBMiss(virtualAddr, true))
            {
                if (!m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), true))
                {
                    triggerException(ExceptionType::PAGE_FAULT, virtualAddr);
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        // 3. Handle cache coherency for stores
        handleStoreCoherency(physicalAddr, decoded.memorySize);

        // 4. Write to L1 cache (write-through or write-back)
        if (m_l1DataCache)
        {
            bool success = m_l1DataCache->write(physicalAddr, &data, decoded.memorySize,
                                                [this](quint64 addr, const void *buf, size_t size)
                                                { return m_memorySystem->writeBlock(addr, buf, size); });

            if (success)
            {
                updatePerformanceCounter(PERF_L1_DCACHE_HIT);
                return true;
            }
        }

        // 5. Direct memory write if no cache
        if (!m_memorySystem->writeBlock(physicalAddr, &data, decoded.memorySize))
        {
            triggerException(ExceptionType::MACHINE_CHECK, virtualAddr);
            return false;
        }

        return true;
    }

    /**
     * @brief Execute load-locked instruction (LDL_L/LDQ_L)
     */
    bool executeLoadLocked(const DecodedInstruction &decoded)
    {
        quint64 virtualAddr = calculateEffectiveAddress(decoded);

        // 1. Perform normal load operation
        if (!executeMemoryLoad(decoded))
        {
            return false;
        }

        // 2. Set reservation for this CPU
        quint64 physicalAddr;
        if (m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), false))
        {
            setLoadReservation(physicalAddr, decoded.memorySize);
        }

        return true;
    }

    /**
     * @brief Execute store-conditional instruction (STL_C/STQ_C)
     */
    bool executeStoreConditional(const DecodedInstruction &decoded)
    {
        quint64 virtualAddr = calculateEffectiveAddress(decoded);

        // 1. Translate address
        quint64 physicalAddr;
        if (!m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), true))
        {
            if (handleDataTLBMiss(virtualAddr, true))
            {
                if (!m_tlbSystem->translateData(virtualAddr, physicalAddr, getCurrentASN(), true))
                {
                    triggerException(ExceptionType::PAGE_FAULT, virtualAddr);
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        // 2. Check reservation
        if (!checkLoadReservation(physicalAddr, decoded.memorySize))
        {
            // Reservation failed - write 0 to destination register
            writeRegister(decoded.destReg, 0);
            return true;
        }

        // 3. Perform store operation
        quint64 data = readRegister(decoded.srcReg);

        // Handle cache coherency
        handleStoreCoherency(physicalAddr, decoded.memorySize);

        // Write to memory
        bool success = false;
        if (m_l1DataCache)
        {
            success = m_l1DataCache->write(physicalAddr, &data, decoded.memorySize,
                                           [this](quint64 addr, const void *buf, size_t size)
                                           { return m_memorySystem->writeBlock(addr, buf, size); });
        }
        else
        {
            success = m_memorySystem->writeBlock(physicalAddr, &data, decoded.memorySize);
        }

        if (success)
        {
            // Clear all reservations for this cache line
            m_memorySystem->clearReservations(physicalAddr & ~63, 64);
            writeRegister(decoded.destReg, 1); // Success
        }
        else
        {
            writeRegister(decoded.destReg, 0); // Failure
        }

        return success;
    }

    /**
     * @brief Handle store coherency across the cache hierarchy
     */
    void handleStoreCoherency(quint64 physicalAddr, int size)
    {
        // 1. Invalidate other CPUs' cache lines
        if (m_AlphaSMPManager)
        {
            m_AlphaSMPManager->invalidateOtherCaches(m_cpuId, physicalAddr);
        }

        // 2. Clear any load reservations on this cache line
        m_memorySystem->clearReservations(physicalAddr & ~63, 64);

        // 3. Emit coherency event
        emit cacheCoherencyEvent(physicalAddr, COHERENCY_INVALIDATE);
    }

    /**
     * @brief Fill cache hierarchy after memory access
     */
    void fillCacheHierarchy(quint64 physicalAddr, void *data, int size, bool isWrite)
    {
        // Fill L2 cache first
        if (m_l2DataCache && !m_l2DataCache->contains(physicalAddr))
        {
            m_l2DataCache->write(physicalAddr, data, size, [this](quint64 addr, const void *buf, size_t size)
                                 { return m_memorySystem->writeBlock(addr, buf, size); });
        }

        // Fill L1 cache
        if (m_l1DataCache && !m_l1DataCache->contains(physicalAddr))
        {
            if (isWrite)
            {
                m_l1DataCache->write(physicalAddr, data, size, [this](quint64 addr, const void *buf, size_t size)
                                     { return m_memorySystem->writeBlock(addr, buf, size); });
            }
            else
            {
                m_l1DataCache->read(physicalAddr, data, size, [this](quint64 addr, void *buf, size_t size)
                                    { return m_memorySystem->readBlock(addr, buf, size); });
            }
        }
    }

  public:
    /**
     * @brief Handle TLB invalidation from PAL instructions
     */
    void handlePALTLBInvalidate(quint64 type, quint64 address = 0)
    {
        DEBUG_LOG(QString("PAL TLB invalidate: type=%1, addr=0x%2").arg(type).arg(address, 16, 16, QChar('0')));

        switch (type)
        {
        case 0: // TBIA - invalidate all TLB entries
            m_tlbSystem->invalidateAll();
            flushAllCaches();
            notifyTLBInvalidation(type, 0);
            break;

        case 1: // TBIAP - invalidate all for current process
            m_tlbSystem->invalidateByASN(getCurrentASN());
            flushInstructionCache();
            notifyTLBInvalidation(type, getCurrentASN());
            break;

        case 2: // TBIS - invalidate single entry
            m_tlbSystem->invalidateEntry(address, getCurrentASN());
            if (isInstructionPage(address))
            {
                invalidateInstructionCache(address);
            }
            notifyTLBInvalidation(type, address);
            break;

        case 3: // TBISI - invalidate single instruction entry
            m_tlbSystem->invalidateInstructionEntry(address, getCurrentASN());
            invalidateInstructionCache(address);
            notifyTLBInvalidation(type, address);
            break;

        case 4: // TBISD - invalidate single data entry
            m_tlbSystem->invalidateDataEntry(address, getCurrentASN());
            invalidateDataCache(address);
            notifyTLBInvalidation(type, address);
            break;
        }

        // Update performance counters
        updateTLBInvalidateCounters(type);
    }

    /**
     * @brief Handle cache management PAL instructions
     */
    void handlePALCacheOperation(quint64 operation, quint64 address)
    {
        switch (operation)
        {
        case 0: // Cache flush
            flushAllCaches();
            break;

        case 1: // Instruction cache flush
            flushInstructionCache();
            break;

        case 2: // Data cache flush
            flushDataCache();
            break;

        case 3: // Cache line invalidate
            invalidateCacheLine(address);
            break;

        case 4: // Cache line flush
            flushCacheLine(address);
            break;
        }
    }

  private:
    /**
     * @brief Flush all caches (instruction and data)
     */
    void flushAllCaches()
    {
        flushInstructionCache();
        flushDataCache();
    }

    /**
     * @brief Flush instruction cache
     */
    void flushInstructionCache()
    {
        if (m_instructionCache)
        {
            m_instructionCache->flush();
        }

        // Clear fetch unit buffers
        if (m_fetchUnit)
        {
            m_fetchUnit->flushBuffers();
        }
    }

    /**
     * @brief Flush data cache hierarchy
     */
    void flushDataCache()
    {
        // Flush L1 data cache with writeback
        if (m_l1DataCache)
        {
            m_l1DataCache->writeBackAllDirty([this](quint64 addr, const void *data, size_t size)
                                             { return m_memorySystem->writeBlock(addr, data, size); });
            m_l1DataCache->flush();
        }

        // Flush L2 cache with writeback
        if (m_l2DataCache)
        {
            m_l2DataCache->writeBackAllDirty([this](quint64 addr, const void *data, size_t size)
                                             { return m_memorySystem->writeBlock(addr, data, size); });
        }
    }

    /**
     * @brief Notify other CPUs of TLB invalidation for SMP coherency
     */
    void notifyTLBInvalidation(quint64 type, quint64 address)
    {
        if (m_AlphaSMPManager)
        {
            m_AlphaSMPManager->broadcastTLBInvalidation(m_cpuId, type, address);
        }

        emit tlbInvalidated(type, address);
    }

    /**
     * @brief Handle TLB invalidation from other CPUs
     */
    void handleRemoteTLBInvalidation(quint64 type, quint64 address, quint16 sourceCpuId)
    {
        if (sourceCpuId == m_cpuId)
        {
            return; // Ignore our own invalidations
        }

        DEBUG_LOG(QString("Remote TLB invalidate from CPU%1: type=%2, addr=0x%3")
                      .arg(sourceCpuId)
                      .arg(type)
                      .arg(address, 16, 16, QChar('0')));

        // Apply the same invalidation locally
        switch (type)
        {
        case 0: // Global invalidate
            m_tlbSystem->invalidateAll();
            flushAllCaches();
            break;

        case 1: // Process invalidate
            m_tlbSystem->invalidateByASN(address);
            flushInstructionCache();
            break;

        case 2: // Single entry invalidate
            m_tlbSystem->invalidateEntry(address, getCurrentASN());
            if (isInstructionPage(address))
            {
                invalidateInstructionCache(address);
            }
            break;
        }
    }

  public slots:
    /**
     * @brief Handle cache coherency events from other CPUs
     */
    void onCacheCoherencyEvent(quint64 physicalAddr, int eventType, quint16 sourceCpuId)
    {
        if (sourceCpuId == m_cpuId)
        {
            return; // Ignore our own events
        }

        switch (eventType)
        {
        case COHERENCY_INVALIDATE:
            // Invalidate our cache lines
            if (m_l1DataCache)
            {
                m_l1DataCache->invalidate(physicalAddr);
            }
            if (m_l2DataCache)
            {
                m_l2DataCache->invalidate(physicalAddr);
            }
            break;

        case COHERENCY_SHARED:
            // Mark cache line as shared if we have it
            markCacheLineShared(physicalAddr);
            break;

        case COHERENCY_EXCLUSIVE:
            // Another CPU has exclusive access
            if (m_l1DataCache)
            {
                m_l1DataCache->invalidate(physicalAddr);
            }
            break;
        }
    }

    /**
     * @brief Handle memory barrier instructions
     */
    void executeMemoryBarrier(quint64 barrierType)
    {
        switch (barrierType)
        {
        case 0: // MB - Memory barrier
            // Ensure all memory operations complete
            flushWriteBuffers();
            break;

        case 1: // WMB - Write memory barrier
            // Ensure all writes complete
            flushWriteBuffers();
            break;

        case 2: // RMB - Read memory barrier
            // Ensure all reads complete
            flushReadBuffers();
            break;
        }

        // Coordinate with memory system
        m_memorySystem->executeMemoryBarrier(barrierType);
    }

  signals:
    // TLB coordination
    void tlbInvalidated(quint64 type, quint64 address);
    void tlbMiss(quint64 virtualAddr, bool isInstruction);

    // Cache coherency
    void cacheCoherencyEvent(quint64 physicalAddr, int eventType);
    void cacheLineFlushed(quint64 physicalAddr);

    // Performance monitoring
    void performanceEvent(int eventType, quint64 data);
};

/**
 * @brief Performance counter definitions for TLB and cache monitoring
 */
enum PerformanceCounters
{
    PERF_CYCLES = 0,
    PERF_INSTRUCTIONS = 1,
    PERF_ICACHE_HIT = 2,
    PERF_ICACHE_MISS = 3,
    PERF_L1_DCACHE_HIT = 4,
    PERF_L1_DCACHE_MISS = 5,
    PERF_L2_CACHE_HIT = 6,
    PERF_L2_CACHE_MISS = 7,
    PERF_ITLB_HIT = 8,
    PERF_ITLB_MISS = 9,
    PERF_DTLB_HIT = 10,
    PERF_DTLB_MISS = 11,
    PERF_TLB_INVALIDATE = 12,
    PERF_MEMORY_BARRIER = 13
};

/**
 * @brief Cache coherency protocol states
 */
enum CoherencyEvents
{
    COHERENCY_INVALIDATE = 0,
    COHERENCY_SHARED = 1,
    COHERENCY_EXCLUSIVE = 2,
    COHERENCY_MODIFIED = 3
};