#include "pipeline_stages.h"
#include "GlobalMacro.h"
#include <array>
#include <atomic>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QTextStream>
#include <QTimer>
#include <atomic>
#include "pipeline_alphainstructions.h"

// Implementation

basePipelineStage::basePipelineStage(const QString &name, int maxInFlight, QObject *parent)
    : QObject(parent), m_name(name), m_gate(maxInFlight), m_inQueue(nullptr), m_maxInFlight(maxInFlight),
      m_statsTimer(new QTimer(this)), m_performanceTimer(new QTimer(this))
{
    DEBUG_LOG("basePipelineStage '%s' created - maxInFlight: %d", qPrintable(m_name), maxInFlight);
}

basePipelineStage::~basePipelineStage()
{
    DEBUG_LOG("basePipelineStage '%s' destroyed - processed %llu instructions", qPrintable(m_name),
              m_stats.instructionsProcessed.load());
    shutdown();
}

void basePipelineStage::initialize()
{
    DEBUG_LOG("basePipelineStage '%s'::initialize()", qPrintable(m_name));

    initializeStageInfrastructure();

    // Reset all statistics
    resetStats();

    // Initialize timers
    m_stageTimer.start();

    // Configure performance monitoring
    m_statsTimer->setInterval(1000); // Update stats every second
    m_statsTimer->setSingleShot(false);

    m_performanceTimer->setInterval(5000); // Performance monitoring every 5 seconds
    m_performanceTimer->setSingleShot(false);

    // Call derived class initialization
    onStageInitialize();

    initialize_SignalsAndSlots();
}

void basePipelineStage::initialize_SignalsAndSlots()
{
    DEBUG_LOG("basePipelineStage '%s'::initialize_SignalsAndSlots()", qPrintable(m_name));

    // Connect internal timers
    connect(m_statsTimer, &QTimer::timeout, this, &basePipelineStage::updateStats);

    connect(m_performanceTimer, &QTimer::timeout, this, &basePipelineStage::performanceMonitoring);
}

void basePipelineStage::moveToWorkerThread()
{
    DEBUG_LOG("basePipelineStage '%s'::moveToWorkerThread()", qPrintable(m_name));

    if (m_thread)
    {
        DEBUG_LOG("WARNING: Stage '%s' already has a worker thread", qPrintable(m_name));
        return;
    }

    // Create worker thread
    m_thread = QThread::create([this] { execLoop(); });
    m_thread->setObjectName(QString("PipelineStage_%1").arg(m_name));

    // Move this object to the worker thread
    this->moveToThread(m_thread);

    // Start the thread
    m_thread->start();

    // Start monitoring timers
    m_statsTimer->start();
    m_performanceTimer->start();

    DEBUG_LOG("Worker thread started for stage '%s'", qPrintable(m_name));
}

void basePipelineStage::shutdown()
{
    DEBUG_LOG("basePipelineStage '%s'::shutdown()", qPrintable(m_name));

    if (!m_running.load())
    {
        return; // Already shutdown
    }

    // Signal shutdown
    m_shutdownRequested.store(true, std::memory_order_release);
    m_running.store(false, std::memory_order_release);

    // Stop timers
    if (m_statsTimer)
        m_statsTimer->stop();
    if (m_performanceTimer)
        m_performanceTimer->stop();

    // Call derived class shutdown
    onStageShutdown();

    // Wait for thread to finish
    if (m_thread)
    {
        if (!m_thread->wait(5000))
        { // Wait up to 5 seconds
            DEBUG_LOG("WARNING: Stage '%s' thread did not finish within timeout", qPrintable(m_name));
            m_thread->terminate();
            m_thread->wait(1000);
        }

        m_thread->deleteLater();
        m_thread = nullptr;
    }

    cleanupStageInfrastructure();
    emit sigStageStopped(m_name);
}

bool basePipelineStage::submit(InstrPtr instr)
{
    if (!instr)
    {
        DEBUG_LOG("WARNING: Stage '%s' received null instruction", qPrintable(m_name));
        return false;
    }

    if (!m_running.load(std::memory_order_acquire))
    {
        DEBUG_LOG("WARNING: Stage '%s' not running, dropping instruction", qPrintable(m_name));
        return false;
    }

    // Try to acquire gate (non-blocking for backpressure detection)
    if (!m_gate.tryAcquire())
    {
        // Backpressure detected
        incrementBackpressureCounter();
        emit sigBackpressureTriggered(m_name);
        return false;
    }

    // Submit to queue
    if (!m_inQueue->enqueue(instr))
    {
        // Queue full - release gate and signal backpressure
        m_gate.release();
        incrementBackpressureCounter();
        emit sigBackpressureTriggered(m_name);
        return false;
    }

    return true;
}

size_t basePipelineStage::currentQueueDepth() const
{
    if (m_inQueue)
    {
        return m_inQueue->size();
    }
    return 0;
}

void basePipelineStage::adjustMaxInFlight(int newMax)
{
    int oldMax = m_maxInFlight.exchange(newMax);

    // Adjust semaphore capacity
    int difference = newMax - oldMax;
    if (difference > 0)
    {
        m_gate.release(difference);
    }
    else if (difference < 0)
    {
        // Acquire the excess permits (may block temporarily)
        for (int i = 0; i < -difference; ++i)
        {
            m_gate.tryAcquire(); // Non-blocking to avoid deadlock
        }
    }

    DEBUG_LOG("Stage '%s' maxInFlight adjusted from %d to %d", qPrintable(m_name), oldMax, newMax);
}

void basePipelineStage::resetStats()
{
    QMutexLocker locker(&m_statsMutex);

    m_stats.instructionsProcessed.store(0);
    m_stats.totalCycles.store(0);
    m_stats.stallCycles.store(0);
    m_stats.queueDepth.store(0);
    m_stats.backpressureEvents.store(0);

    m_lastStatsUpdate.store(0);

    DEBUG_LOG("Stage '%s' statistics reset", qPrintable(m_name));
}

void basePipelineStage::recordProcessingTime(uint64_t cycles)
{
    m_stats.totalCycles.fetch_add(cycles, std::memory_order_relaxed);
}

void basePipelineStage::incrementStallCounter() { m_stats.stallCycles.fetch_add(1, std::memory_order_relaxed); }

void basePipelineStage::incrementBackpressureCounter()
{
    m_stats.backpressureEvents.fetch_add(1, std::memory_order_relaxed);
}

void basePipelineStage::execLoop()
{
    DEBUG_LOG("Stage '%s' execution loop started", qPrintable(m_name));

    m_running.store(true, std::memory_order_release);
    emit sigStageStarted(m_name);

    // Call derived class start hook
    onStageStart();

    while (m_running.load(std::memory_order_acquire))
    {
        InstrPtr instr;

        // Try to dequeue instruction with timeout
        if (m_inQueue->dequeue(instr, 100))
        { // 100ms timeout
            if (instr)
            {
                QElapsedTimer procTimer;
                procTimer.start();

                try
                {
                    // Process instruction in derived class
                    process(instr);

                    // Record processing time
                    uint64_t procTime = static_cast<uint64_t>(procTimer.nsecsElapsed());
                    recordProcessingTime(procTime);

                    // Update statistics
                    m_stats.instructionsProcessed.fetch_add(1, std::memory_order_relaxed);

                    // Emit output signal
                    emit sigOutputReady(instr);
                }
                catch (const std::exception &e)
                {
                    DEBUG_LOG("ERROR: Stage '%s' processing exception: %s", qPrintable(m_name), e.what());
                    incrementStallCounter();
                }
                catch (...)
                {
                    DEBUG_LOG("ERROR: Stage '%s' unknown processing exception", qPrintable(m_name));
                    incrementStallCounter();
                }

                // Release gate permit
                m_gate.release();
            }
        }
        else
        {
            // Timeout - check for stalls
            if (currentQueueDepth() == 0 && m_gate.available() < m_maxInFlight.load())
            {
                incrementStallCounter();
                emit sigStageStalled(m_name);
            }
        }

        // Check for shutdown request
        if (m_shutdownRequested.load(std::memory_order_acquire))
        {
            break;
        }
    }

    DEBUG_LOG("Stage '%s' execution loop finished", qPrintable(m_name));
}

void basePipelineStage::initializeStageInfrastructure()
{
    // Create instruction queue
    m_inQueue = new alphaInstructionQueue(m_maxInFlight.load());

    DEBUG_LOG("Stage '%s' infrastructure initialized", qPrintable(m_name));
}

void basePipelineStage::cleanupStageInfrastructure()
{
    if (m_inQueue)
    {
        delete m_inQueue;
        m_inQueue = nullptr;
    }

    DEBUG_LOG("Stage '%s' infrastructure cleaned up", qPrintable(m_name));
}

void basePipelineStage::updateStats()
{
    updateQueueDepthStats();
    checkForStalls();

    // Update timestamp
    m_lastStatsUpdate.store(static_cast<uint64_t>(m_stageTimer.elapsed()), std::memory_order_relaxed);
}

void basePipelineStage::performanceMonitoring()
{
    adaptQueueSize();
    monitorBackpressure();
    emitPerformanceSignals();
}

void basePipelineStage::updateQueueDepthStats()
{
    if (m_inQueue)
    {
        size_t currentDepth = m_inQueue->size();
        m_stats.queueDepth.store(currentDepth, std::memory_order_relaxed);
    }
}

void basePipelineStage::checkForStalls()
{
    // Check if stage appears to be stalled
    static uint64_t lastInstructionCount = 0;
    uint64_t currentCount = m_stats.instructionsProcessed.load(std::memory_order_relaxed);

    if (currentCount == lastInstructionCount && currentQueueDepth() > 0)
    {
        // No progress but queue has items - potential stall
        incrementStallCounter();
        emit sigStageStalled(m_name);
    }

    lastInstructionCount = currentCount;
}

void basePipelineStage::emitPerformanceSignals()
{
    // Could emit detailed performance signals here
    // For now, just log performance summary
    uint64_t processed = m_stats.instructionsProcessed.load();
    uint64_t stalls = m_stats.stallCycles.load();
    size_t queueDepth = currentQueueDepth();

    if (processed > 0)
    {
        DEBUG_LOG("Stage '%s' performance: processed=%llu, stalls=%llu, queue_depth=%zu", qPrintable(m_name), processed,
                  stalls, queueDepth);
    }
}

void basePipelineStage::adaptQueueSize()
{
    // Simple adaptive queue sizing based on backpressure
    uint64_t backpressureEvents = m_stats.backpressureEvents.load(std::memory_order_relaxed);
    static uint64_t lastBackpressureCount = 0;

    if (backpressureEvents > lastBackpressureCount + 10)
    {
        // Frequent backpressure - consider increasing queue size
        int currentMax = m_maxInFlight.load();
        int newMax = qMin(currentMax * 1.2, 2048.0); // Cap at 2048

        if (newMax > currentMax)
        {
            adjustMaxInFlight(newMax);
            DEBUG_LOG("Stage '%s' increased queue size to %d due to backpressure", qPrintable(m_name), newMax);
        }

        lastBackpressureCount = backpressureEvents;
    }
}

void basePipelineStage::monitorBackpressure()
{
    // Monitor and potentially signal upstream about backpressure
    if (currentQueueDepth() > (m_maxInFlight.load() * 0.8))
    {
        // Queue is 80% full - potential backpressure building
        emit sigBackpressureTriggered(m_name);
    }
}

//--

alphaFetchStage::alphaFetchStage(QObject *parent) : basePipelineStage("Fetch", 1024, parent), m_memorySystem(nullptr)
{
    DEBUG_LOG("alphaFetchStage created");

    // Initialize instruction cache
    for (auto &entry : m_icache)
    {
        entry.tag = 0;
        entry.valid = false;
        entry.accessTime = 0;
        std::fill(std::begin(entry.data), std::end(entry.data), 0);
    }
}

alphaFetchStage::~alphaFetchStage()
{
    DEBUG_LOG("alphaFetchStage destroyed - fetched %llu instructions, cache hit rate: %.2f%%",
              m_fetchStats.instructionsFetched.load(),
              m_fetchStats.cacheHits.load() * 100.0 / qMax(1ULL, m_fetchStats.instructionsFetched.load()));
}

void alphaFetchStage::initialize()
{
    DEBUG_LOG("alphaFetchStage::initialize()");

    // Call base class initialization
    basePipelineStage::initialize();

    // Reset fetch-specific state
    m_nextPC.store(0, std::memory_order_relaxed);
    m_branchTarget.store(0, std::memory_order_relaxed);
    m_flushRequested.store(false, std::memory_order_relaxed);
    m_branchPending.store(false, std::memory_order_relaxed);
    m_lastFetchedPC.store(0, std::memory_order_relaxed);
    m_sequentialFetch.store(true, std::memory_order_relaxed);

    // Reset statistics
    m_fetchStats.instructionsFetched.store(0);
    m_fetchStats.cacheHits.store(0);
    m_fetchStats.cacheMisses.store(0);
    m_fetchStats.branchRedirects.store(0);
    m_fetchStats.pipelineFlushes.store(0);

    invalidateICache();
}

void alphaFetchStage::initialize_SignalsAndSlots()
{
    DEBUG_LOG("alphaFetchStage::initialize_SignalsAndSlots()");

    // Call base class signal/slot initialization
    basePipelineStage::initialize_SignalsAndSlots();

    // No additional internal connections needed for fetch stage
}

void alphaFetchStage::process(InstrPtr instr)
{
    // Fetch stage doesn't process incoming instructions in the traditional sense
    // Instead, it generates new instructions to feed the pipeline

    if (!shouldFetchInstruction())
    {
        return;
    }

    // Handle pipeline control
    handlePipelineFlush();
    handleBranchRedirect();

    // Get next PC to fetch
    uint64_t fetchPC = getNextFetchPC();

    // Fetch instruction from memory/cache
    uint32_t rawBits = fetchInstruction(fetchPC);

    if (rawBits != 0)
    { // Valid instruction fetched
        // Create instruction object
        InstrPtr newInstr = createInstructionObject(rawBits, fetchPC);

        if (newInstr)
        {
            // Update statistics
            m_fetchStats.instructionsFetched.fetch_add(1, std::memory_order_relaxed);

            // Update fetch prediction
            updateFetchPrediction(fetchPC);

            // Advance PC for next fetch
            m_nextPC.store(fetchPC + 4, std::memory_order_relaxed);
            m_lastFetchedPC.store(fetchPC, std::memory_order_relaxed);

            // Perform prefetch for next instruction
            performPrefetch(fetchPC + 4);

            // Emit the fetched instruction
            emit sigOutputReady(newInstr);

            DEBUG_LOG("Fetched instruction 0x%08x at PC 0x%llx", rawBits, fetchPC);
        }
    }
    else
    {
        // Fetch failed - record stall
        incrementStallCounter();
        DEBUG_LOG("Fetch failed at PC 0x%llx", fetchPC);
    }
}

void alphaFetchStage::onStageStart()
{
    DEBUG_LOG("alphaFetchStage::onStageStart()");

    if (!m_memorySystem)
    {
        DEBUG_LOG("WARNING: alphaFetchStage started without memory system attached");
    }

    // Initialize fetch PC if not already set
    if (m_nextPC.load(std::memory_order_acquire) == 0)
    {
        m_nextPC.store(0x10000, std::memory_order_release); // Default start address
        DEBUG_LOG("Set default fetch PC to 0x10000");
    }
}

void alphaFetchStage::onStageInitialize()
{
    DEBUG_LOG("alphaFetchStage::onStageInitialize()");
    // Stage-specific initialization can go here
}

void alphaFetchStage::onStageShutdown()
{
    DEBUG_LOG("alphaFetchStage::onStageShutdown()");

    // Log final statistics
    DEBUG_LOG("Final fetch statistics:");
    DEBUG_LOG("  Instructions fetched: %llu", m_fetchStats.instructionsFetched.load());
    DEBUG_LOG("  Cache hits: %llu", m_fetchStats.cacheHits.load());
    DEBUG_LOG("  Cache misses: %llu", m_fetchStats.cacheMisses.load());
    DEBUG_LOG("  Branch redirects: %llu", m_fetchStats.branchRedirects.load());
    DEBUG_LOG("  Pipeline flushes: %llu", m_fetchStats.pipelineFlushes.load());
}

uint32_t alphaFetchStage::fetchInstruction(uint64_t pc)
{
    uint32_t instruction = 0;

    // Try cache first
    if (lookupICache(pc, instruction))
    {
        m_fetchStats.cacheHits.fetch_add(1, std::memory_order_relaxed);
        return instruction;
    }

    // Cache miss - fetch from memory
    m_fetchStats.cacheMisses.fetch_add(1, std::memory_order_relaxed);
    emit sigICacheMiss(pc);

    if (m_memorySystem)
    {
        // This would call your memory system
        // instruction = m_memorySystem->readLong(pc);

        // For now, return a NOP instruction as placeholder
        instruction = 0x47FF041F; // Alpha NOP

        // Update cache
        updateICache(pc, instruction);
    }
    else
    {
        DEBUG_LOG("ERROR: No memory system available for fetch at PC 0x%llx", pc);
        return 0;
    }

    return instruction;
}

InstrPtr alphaFetchStage::createInstructionObject(uint32_t rawBits, uint64_t pc)
{
    // Use instruction factory to create appropriate instruction type
    return alphaInstructionFactory::instance().createInstruction(rawBits, pc);
}

void alphaFetchStage::invalidateICache()
{
    QMutexLocker locker(&m_cacheMutex);

    for (auto &entry : m_icache)
    {
        entry.valid = false;
        entry.tag = 0;
        entry.accessTime = 0;
    }

    DEBUG_LOG("Instruction cache invalidated");
}

void alphaFetchStage::invalidateICacheLine(uint64_t pc)
{
    QMutexLocker locker(&m_cacheMutex);

    uint64_t index = getCacheIndex(pc);
    if (index < ICACHE_SIZE)
    {
        m_icache[index].valid = false;
        DEBUG_LOG("Invalidated I-cache line for PC 0x%llx", pc);
    }
}

uint64_t alphaFetchStage::getCacheIndex(uint64_t pc) const { return (pc / CACHE_LINE_SIZE) % ICACHE_SIZE; }

uint64_t alphaFetchStage::getCacheTag(uint64_t pc) const { return pc / (CACHE_LINE_SIZE * ICACHE_SIZE); }

uint64_t alphaFetchStage::getOffsetInLine(uint64_t pc) const
{
    return (pc % CACHE_LINE_SIZE) / 4; // 4 bytes per instruction
}

bool alphaFetchStage::lookupICache(uint64_t pc, uint32_t &instruction)
{
    uint64_t index = getCacheIndex(pc);
    uint64_t tag = getCacheTag(pc);
    uint64_t offset = getOffsetInLine(pc);

    if (index >= ICACHE_SIZE || offset >= INSTRUCTIONS_PER_LINE)
    {
        return false;
    }

    ICacheEntry &entry = m_icache[index];

    if (entry.valid && entry.tag == tag)
    {
        instruction = entry.data[offset];
        entry.accessTime = static_cast<uint64_t>(getStageTimer().elapsed());
        return true;
    }

    return false;
}

void alphaFetchStage::updateICache(uint64_t pc, uint32_t instruction)
{
    uint64_t index = getCacheIndex(pc);
    uint64_t tag = getCacheTag(pc);
    uint64_t offset = getOffsetInLine(pc);

    if (index >= ICACHE_SIZE || offset >= INSTRUCTIONS_PER_LINE)
    {
        return;
    }

    ICacheEntry &entry = m_icache[index];

    // If this is a new cache line, initialize it
    if (!entry.valid || entry.tag != tag)
    {
        entry.tag = tag;
        entry.valid = true;
        entry.accessTime = static_cast<uint64_t>(getStageTimer().elapsed());
        std::fill(std::begin(entry.data), std::end(entry.data), 0);
    }

    entry.data[offset] = instruction;
}

void alphaFetchStage::fillCacheLine(uint64_t pc)
{
    // This would fetch an entire cache line from memory
    // For now, just a placeholder
    uint64_t lineStart = (pc / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;

    for (int i = 0; i < INSTRUCTIONS_PER_LINE; ++i)
    {
        uint64_t fetchPC = lineStart + (i * 4);
        uint32_t instr = 0x47FF041F; // NOP placeholder
        updateICache(fetchPC, instr);
    }

    DEBUG_LOG("Filled cache line starting at 0x%llx", lineStart);
}

uint64_t alphaFetchStage::getNextFetchPC()
{
    // Check for branch target first
    if (m_branchPending.load(std::memory_order_acquire))
    {
        return m_branchTarget.load(std::memory_order_acquire);
    }

    return m_nextPC.load(std::memory_order_acquire);
}

void alphaFetchStage::handleBranchRedirect()
{
    if (m_branchPending.exchange(false, std::memory_order_acq_rel))
    {
        uint64_t oldPC = m_nextPC.load(std::memory_order_relaxed);
        uint64_t newPC = m_branchTarget.load(std::memory_order_acquire);

        m_nextPC.store(newPC, std::memory_order_release);
        m_fetchStats.branchRedirects.fetch_add(1, std::memory_order_relaxed);

        DEBUG_LOG("Branch redirect from 0x%llx to 0x%llx", oldPC, newPC);
        emit sigBranchRedirect(oldPC, newPC);

        // Update prediction state
        m_sequentialFetch.store(false, std::memory_order_relaxed);
    }
}

void alphaFetchStage::handlePipelineFlush()
{
    if (m_flushRequested.exchange(false, std::memory_order_acq_rel))
    {
        m_fetchStats.pipelineFlushes.fetch_add(1, std::memory_order_relaxed);

        DEBUG_LOG("Pipeline flush requested");
        emit sigPipelineFlush();

        // Reset prediction state
        m_sequentialFetch.store(true, std::memory_order_relaxed);
    }
}

bool alphaFetchStage::shouldFetchInstruction()
{
    // Don't fetch if stage is shutting down
    if (!isRunning())
    {
        return false;
    }

    // Don't fetch if no memory system available
    if (!m_memorySystem)
    {
        return false;
    }

    return true;
}

void alphaFetchStage::performPrefetch(uint64_t pc)
{
    // Simple prefetch - fetch next cache line if we're near the end of current line
    uint64_t offset = getOffsetInLine(pc);

    if (offset >= (INSTRUCTIONS_PER_LINE - 2))
    { // Near end of line
        uint64_t nextLinePc = ((pc / CACHE_LINE_SIZE) + 1) * CACHE_LINE_SIZE;

        uint32_t dummy;
        if (!lookupICache(nextLinePc, dummy))
        {
            // Prefetch next line
            fillCacheLine(nextLinePc);
            DEBUG_LOG("Prefetched cache line at 0x%llx", nextLinePc);
        }
    }
}

void alphaFetchStage::updateFetchPrediction(uint64_t pc)
{
    uint64_t lastPC = m_lastFetchedPC.load(std::memory_order_relaxed);

    if (lastPC != 0)
    {
        bool isSequential = (pc == lastPC + 4);
        m_sequentialFetch.store(isSequential, std::memory_order_relaxed);
    }
}

// --

alphaDecodeStage::alphaDecodeStage(QObject *parent) : basePipelineStage("Decode", 1024, parent)
{
    DEBUG_LOG("alphaDecodeStage created");
}

alphaDecodeStage::~alphaDecodeStage()
{
    DEBUG_LOG("alphaDecodeStage destroyed - decoded %llu instructions", m_decodeStats.instructionsDecoded.load());
}

void alphaDecodeStage::initialize()
{
    DEBUG_LOG("alphaDecodeStage::initialize()");

    // Call base class initialization
    basePipelineStage::initialize();

    // Reset decode statistics
    m_decodeStats.instructionsDecoded.store(0);
    m_decodeStats.memoryInstructions.store(0);
    m_decodeStats.operateInstructions.store(0);
    m_decodeStats.branchInstructions.store(0);
    m_decodeStats.jumpInstructions.store(0);
    m_decodeStats.floatInstructions.store(0);
    m_decodeStats.miscInstructions.store(0);
    m_decodeStats.invalidInstructions.store(0);
}

void alphaDecodeStage::initialize_SignalsAndSlots()
{
    DEBUG_LOG("alphaDecodeStage::initialize_SignalsAndSlots()");

    // Call base class signal/slot initialization
    basePipelineStage::initialize_SignalsAndSlots();

    // No additional internal connections needed for decode stage
}

void alphaDecodeStage::process(InstrPtr instr)
{
    if (!instr)
    {
        DEBUG_LOG("ERROR: alphaDecodeStage received null instruction");
        incrementStallCounter();
        return;
    }

    uint32_t rawBits = instr->getRawBits();
    uint64_t pc = instr->getPC();
    uint32_t opcode = extractOpcode(rawBits);

    DEBUG_LOG("Decoding instruction 0x%08x at PC 0x%llx", rawBits, pc);

    // Validate instruction first
    if (!validateInstruction(rawBits, opcode))
    {
        handleInvalidInstruction(instr);
        return;
    }

    // Populate common instruction fields
    populateCommonFields(instr);

    // Determine instruction format and decode accordingly
    InstructionFormat format = determineFormat(opcode);

    switch (format)
    {
    case InstructionFormat::Memory:
        decodeMemoryFormat(instr);
        break;

    case InstructionFormat::Operate:
        decodeOperateFormat(instr);
        break;

    case InstructionFormat::Branch:
        decodeBranchFormat(instr);
        break;

    case InstructionFormat::Jump:
        decodeJumpFormat(instr);
        break;

    case InstructionFormat::FloatOp:
        decodeFloatFormat(instr);
        break;

    case InstructionFormat::Misc:
        decodeMiscFormat(instr);
        break;

    case InstructionFormat::Invalid:
    default:
        handleInvalidInstruction(instr);
        return;
    }

    // Update statistics
    updateDecodeStatistics(format);
    m_decodeStats.instructionsDecoded.fetch_add(1, std::memory_order_relaxed);

    // Mark instruction as decoded
    instr->setDecoded(true);

    DEBUG_LOG("Successfully decoded %s instruction at PC 0x%llx", qPrintable(getInstructionMnemonic(rawBits)), pc);
}

void alphaDecodeStage::onStageStart() { DEBUG_LOG("alphaDecodeStage::onStageStart()"); }

void alphaDecodeStage::onStageInitialize() { DEBUG_LOG("alphaDecodeStage::onStageInitialize()"); }

void alphaDecodeStage::onStageShutdown()
{
    DEBUG_LOG("alphaDecodeStage::onStageShutdown()");

    // Log final decode statistics
    DEBUG_LOG("Final decode statistics:");
    DEBUG_LOG("  Total decoded: %llu", m_decodeStats.instructionsDecoded.load());
    DEBUG_LOG("  Memory: %llu", m_decodeStats.memoryInstructions.load());
    DEBUG_LOG("  Operate: %llu", m_decodeStats.operateInstructions.load());
    DEBUG_LOG("  Branch: %llu", m_decodeStats.branchInstructions.load());
    DEBUG_LOG("  Jump: %llu", m_decodeStats.jumpInstructions.load());
    DEBUG_LOG("  Float: %llu", m_decodeStats.floatInstructions.load());
    DEBUG_LOG("  Misc: %llu", m_decodeStats.miscInstructions.load());
    DEBUG_LOG("  Invalid: %llu", m_decodeStats.invalidInstructions.load());
}

alphaDecodeStage::InstructionFormat alphaDecodeStage::determineFormat(uint32_t opcode)
{
    switch (opcode)
    {
    // Memory format instructions
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B: // LDA, LDAH, LDBU, LDQ_U
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F: // LDWU, STW, STB, STQ_U
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23: // LDF, LDG, LDS, LDT
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27: // STF, STG, STS, STT
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B: // LDL, LDQ, LDL_L, LDQ_L
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F: // STL, STQ, STL_C, STQ_C
        return InstructionFormat::Memory;

    // Operate format instructions
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13: // Integer arithmetic/logical
        return InstructionFormat::Operate;

    // Branch format instructions
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33: // BR, FBEQ, FBLT, FBLE
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37: // BSR, FBNE, FBGE, FBGT
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B: // BLBC, BEQ, BLT, BLE
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F: // BLBS, BNE, BGE, BGT
        return InstructionFormat::Branch;

    // Jump format instructions
    case 0x1A: // JMP, JSR, RET, JSR_COROUTINE
        return InstructionFormat::Jump;

    // Floating-point operate instructions
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17: // ITFP, FLTV, FLTI, FLTL
        return InstructionFormat::FloatOp;

    // Miscellaneous instructions
    case 0x00:
    case 0x18:
    case 0x1C: // CALL_PAL, MISC, FPTI
        return InstructionFormat::Misc;

    default:
        return InstructionFormat::Invalid;
    }
}

void alphaDecodeStage::decodeMemoryFormat(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();

    instr->setRa(extractRa(rawBits));
    instr->setRb(extractRb(rawBits));
    instr->setDisplacement(extractDisplacement(rawBits));

    // Cast to memory instruction for specific fields
    auto memInstr = qSharedPointerDynamicCast<alphaMemoryInstruction>(instr);
    if (memInstr)
    {
        uint32_t opcode = extractOpcode(rawBits);

        // Determine if load or store
        memInstr->setIsLoad(opcode < 0x2C); // Stores start at 0x2C
        memInstr->setIsStore(!memInstr->getIsLoad());

        // Determine access size (in bytes) based on opcode
        switch (opcode)
        {
        case 0x0A:
        case 0x0E: // LDBU, STB
            memInstr->setAccessSize(1);
            break;
        case 0x0C:
        case 0x0D: // LDWU, STW
            memInstr->setAccessSize(2);
            break;
        case 0x28:
        case 0x2C: // LDL, STL
            memInstr->setAccessSize(4);
            break;
        case 0x29:
        case 0x2D: // LDQ, STQ
        default:
            memInstr->setAccessSize(8);
            break;
        }

        DEBUG_LOG("Memory instruction: %s, size=%d, disp=%d", memInstr->getIsLoad() ? "LOAD" : "STORE",
                  memInstr->getAccessSize(), memInstr->getDisplacement());
    }
}

void alphaDecodeStage::decodeOperateFormat(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();

    instr->setRa(extractRa(rawBits));
    instr->setRc(extractRc(rawBits));
    instr->setFunction(extractFunction(rawBits));

    if (isLiteralMode(rawBits))
    {
        instr->setLiteral(extractLiteral(rawBits));
        instr->setIsLiteral(true);
    }
    else
    {
        instr->setRb(extractRb(rawBits));
        instr->setIsLiteral(false);
    }

    // Cast to integer instruction for specific fields
    auto intInstr = qSharedPointerDynamicCast<alphaIntegerInstruction>(instr);
    if (intInstr)
    {
        // Will be populated during execution with actual operand values
        DEBUG_LOG("Integer instruction: func=0x%x, literal_mode=%s", instr->getFunction(),
                  instr->getIsLiteral() ? "true" : "false");
    }
}

void alphaDecodeStage::decodeBranchFormat(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();

    instr->setRa(extractRa(rawBits));
    instr->setDisplacement(extractBranchDisplacement(rawBits));

    // Cast to branch instruction for specific fields
    auto branchInstr = qSharedPointerDynamicCast<alphaBranchInstruction>(instr);
    if (branchInstr)
    {
        uint32_t opcode = extractOpcode(rawBits);

        // Determine if conditional or unconditional
        branchInstr->setIsConditional(opcode != 0x30 && opcode != 0x34); // BR and BSR are unconditional

        // Calculate target address
        uint64_t targetPC = instr->getPC() + 4 + (instr->getDisplacement() * 4);
        branchInstr->setTargetAddress(targetPC);

        DEBUG_LOG("Branch instruction: conditional=%s, target=0x%llx",
                  branchInstr->getIsConditional() ? "true" : "false", branchInstr->getTargetAddress());
    }
}

void alphaDecodeStage::decodeJumpFormat(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();

    instr->setRa(extractRa(rawBits));
    instr->setRb(extractRb(rawBits));
    instr->setFunction(extractFunction(rawBits) & 0x3); // Only lower 2 bits for jump function

    // Cast to branch instruction (jumps are a type of branch)
    auto branchInstr = qSharedPointerDynamicCast<alphaBranchInstruction>(instr);
    if (branchInstr)
    {
        uint32_t jumpFunc = instr->getFunction();

        // JMP functions: 0=JMP, 1=JSR, 2=RET, 3=JSR_COROUTINE
        branchInstr->setIsConditional(false); // Jumps are always unconditional

        DEBUG_LOG("Jump instruction: function=%d (%s)", jumpFunc,
                  jumpFunc == 0   ? "JMP"
                  : jumpFunc == 1 ? "JSR"
                  : jumpFunc == 2 ? "RET"
                                  : "JSR_COROUTINE");
    }
}

void alphaDecodeStage::decodeFloatFormat(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();

    instr->setRa(extractRa(rawBits));
    instr->setRb(extractRb(rawBits));
    instr->setRc(extractRc(rawBits));
    instr->setFunction(extractFunction(rawBits));

    // Cast to floating point instruction for specific fields
    auto fpInstr = qSharedPointerDynamicCast<alphaFloatingPointInstruction>(instr);
    if (fpInstr)
    {
        // FP-specific decoding can be added here
        DEBUG_LOG("Floating-point instruction: func=0x%x", instr->getFunction());

        emit sigComplexInstructionDecoded(instr->getPC(), "FloatingPoint");
    }
}

void alphaDecodeStage::decodeMiscFormat(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();
    uint32_t opcode = extractOpcode(rawBits);

    switch (opcode)
    {
    case 0x00:                                   // CALL_PAL
        instr->setFunction(rawBits & 0x3FFFFFF); // 26-bit PAL function code
        DEBUG_LOG("CALL_PAL instruction: func=0x%x", instr->getFunction());
        break;

    case 0x18: // MISC (Memory barrier, etc.)
        instr->setFunction(extractFunction(rawBits));
        DEBUG_LOG("MISC instruction: func=0x%x", instr->getFunction());
        break;

    default:
        DEBUG_LOG("Other misc instruction: opcode=0x%x", opcode);
        break;
    }

    emit sigComplexInstructionDecoded(instr->getPC(), "Miscellaneous");
}

// Field extraction methods
uint32_t alphaDecodeStage::extractOpcode(uint32_t rawBits) const { return (rawBits >> 26) & 0x3F; }

uint32_t alphaDecodeStage::extractRa(uint32_t rawBits) const { return (rawBits >> 21) & 0x1F; }

uint32_t alphaDecodeStage::extractRb(uint32_t rawBits) const { return (rawBits >> 16) & 0x1F; }

uint32_t alphaDecodeStage::extractRc(uint32_t rawBits) const { return rawBits & 0x1F; }

uint32_t alphaDecodeStage::extractFunction(uint32_t rawBits) const
{
    return rawBits & 0x7FF; // 11-bit function field
}

uint32_t alphaDecodeStage::extractLiteral(uint32_t rawBits) const
{
    return (rawBits >> 13) & 0xFF; // 8-bit literal
}

int32_t alphaDecodeStage::extractDisplacement(uint32_t rawBits) const
{
    int32_t disp = rawBits & 0xFFFF;
    // Sign extend 16-bit displacement
    if (disp & 0x8000)
    {
        disp |= 0xFFFF0000;
    }
    return disp;
}

int32_t alphaDecodeStage::extractBranchDisplacement(uint32_t rawBits) const
{
    int32_t disp = rawBits & 0x1FFFFF; // 21-bit displacement
    // Sign extend 21-bit displacement
    if (disp & 0x100000)
    {
        disp |= 0xFFE00000;
    }
    return disp;
}

bool alphaDecodeStage::isLiteralMode(uint32_t rawBits) const { return (rawBits >> 12) & 0x1; }

bool alphaDecodeStage::validateInstruction(uint32_t rawBits, uint32_t opcode) const
{
    // Check for reserved opcodes
    if (isReservedOpcode(opcode))
    {
        return false;
    }

    // Additional validation can be added here
    return true;
}

bool alphaDecodeStage::isReservedOpcode(uint32_t opcode) const
{
    // Alpha reserved opcodes
    switch (opcode)
    {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x19:
    case 0x1B:
    case 0x1D:
    case 0x1E:
    case 0x1F:
        return true;
    default:
        return false;
    }
}

bool alphaDecodeStage::isPrivilegedInstruction(uint32_t rawBits) const
{
    uint32_t opcode = extractOpcode(rawBits);

    // CALL_PAL instructions are privileged
    if (opcode == 0x00)
    {
        return true;
    }

    // Some MISC instructions are privileged
    if (opcode == 0x18)
    {
        uint32_t function = extractFunction(rawBits);
        // Memory barriers and some other functions
        return (function >= 0x4000);
    }

    return false;
}

void alphaDecodeStage::populateCommonFields(InstrPtr instr)
{
    uint32_t rawBits = instr->getRawBits();

    instr->setOpcode(extractOpcode(rawBits));
    // Other common fields are set by specific decode methods
}

void alphaDecodeStage::handleInvalidInstruction(InstrPtr instr)
{
    DEBUG_LOG("ERROR: Invalid instruction 0x%08x at PC 0x%llx", instr->getRawBits(), instr->getPC());

    m_decodeStats.invalidInstructions.fetch_add(1, std::memory_order_relaxed);
    emit sigInvalidInstruction(instr->getPC(), instr->getRawBits());

    // Mark instruction as invalid but continue processing
    instr->setValid(false);
}

QString alphaDecodeStage::getInstructionMnemonic(uint32_t rawBits) const
{
    uint32_t opcode = extractOpcode(rawBits);

    // Simplified mnemonic lookup
    switch (opcode)
    {
    case 0x08:
        return "LDA";
    case 0x09:
        return "LDAH";
    case 0x28:
        return "LDL";
    case 0x29:
        return "LDQ";
    case 0x2C:
        return "STL";
    case 0x2D:
        return "STQ";
    case 0x10:
        return "ARITH";
    case 0x11:
        return "LOGICAL";
    case 0x30:
        return "BR";
    case 0x34:
        return "BSR";
    case 0x39:
        return "BEQ";
    case 0x1A:
        return "JMP";
    case 0x16:
        return "FLOP";
    case 0x00:
        return "CALL_PAL";
    default:
        return QString("UNK_%1").arg(opcode, 2, 16, QChar('0'));
    }
}

void alphaDecodeStage::updateDecodeStatistics(InstructionFormat format)
{
    switch (format)
    {
    case InstructionFormat::Memory:
        m_decodeStats.memoryInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case InstructionFormat::Operate:
        m_decodeStats.operateInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case InstructionFormat::Branch:
        m_decodeStats.branchInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case InstructionFormat::Jump:
        m_decodeStats.jumpInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case InstructionFormat::FloatOp:
        m_decodeStats.floatInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case InstructionFormat::Misc:
        m_decodeStats.miscInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case InstructionFormat::Invalid:
        m_decodeStats.invalidInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    }
}

// Heavy operation task for async execution

alphaExecuteStage::alphaExecuteStage(QObject *parent)
    : basePipelineStage("Execute", 1024, parent), m_heavyOpPool(new QThreadPool(this))
{
    DEBUG_LOG("alphaExecuteStage created");

    // Configure heavy operation thread pool
    m_heavyOpPool->setMaxThreadCount(qMax(1, QThread::idealThreadCount() / 4));
    m_heavyOpPool->setExpiryTimeout(10000); // 10 seconds
}

alphaExecuteStage::~alphaExecuteStage()
{
    DEBUG_LOG("alphaExecuteStage destroyed - executed %llu instructions", m_executeStats.instructionsExecuted.load());
}

void alphaExecuteStage::initialize()
{
    DEBUG_LOG("alphaExecuteStage::initialize()");

    // Call base class initialization
    basePipelineStage::initialize();

    // Reset execution statistics
    m_executeStats.instructionsExecuted.store(0);
    m_executeStats.trivialInstructions.store(0);
    m_executeStats.moderateInstructions.store(0);
    m_executeStats.heavyInstructions.store(0);
    m_executeStats.inlineExecutions.store(0);
    m_executeStats.asyncExecutions.store(0);
    m_executeStats.executionExceptions.store(0);
    m_executeStats.totalExecutionCycles.store(0);

    m_pendingHeavyOps.store(0);

    // Clear any pending tasks
    if (m_heavyOpPool)
    {
        m_heavyOpPool->clear();
    }
}

void alphaExecuteStage::initialize_SignalsAndSlots()
{
    DEBUG_LOG("alphaExecuteStage::initialize_SignalsAndSlots()");

    // Call base class signal/slot initialization
    basePipelineStage::initialize_SignalsAndSlots();

    // No additional internal connections needed
}

void alphaExecuteStage::process(InstrPtr instr)
{
    if (!instr)
    {
        DEBUG_LOG("ERROR: alphaExecuteStage received null instruction");
        incrementStallCounter();
        return;
    }

    if (!instr->isDecoded() || !instr->isValid())
    {
        DEBUG_LOG("ERROR: Cannot execute undecoded or invalid instruction at PC 0x%llx", instr->getPC());
        handleExecutionException(instr, "Instruction not properly decoded");
        return;
    }

    if (!m_registerFile || !m_memorySystem)
    {
        DEBUG_LOG("ERROR: Execute stage missing required resources");
        handleExecutionException(instr, "Missing register file or memory system");
        return;
    }

    DEBUG_LOG("Executing instruction at PC 0x%llx, opcode=0x%x", instr->getPC(), instr->getOpcode());

    // Classify instruction cost
    ExecutionCost cost = classifyInstruction(instr);

    // Choose execution method based on cost
    bool success = false;
    if (cost == ExecutionCost::Heavy)
    {
        // Heavy operations go to thread pool
        executeAsync(instr);
        success = true; // Assume success for async operations
    }
    else
    {
        // Trivial and moderate operations execute inline
        success = executeInline(instr);
    }

    if (success)
    {
        // Update statistics
        updateExecutionStatistics(cost, cost == ExecutionCost::Heavy);
        m_executeStats.instructionsExecuted.fetch_add(1, std::memory_order_relaxed);

        // Mark instruction as executed
        instr->setExecuted(true);

        DEBUG_LOG("Successfully executed instruction at PC 0x%llx", instr->getPC());
    }
    else
    {
        handleExecutionException(instr, "Execution failed");
    }
}

void alphaExecuteStage::onStageStart()
{
    DEBUG_LOG("alphaExecuteStage::onStageStart()");

    if (!m_registerFile)
    {
        DEBUG_LOG("WARNING: alphaExecuteStage started without register file attached");
    }

    if (!m_memorySystem)
    {
        DEBUG_LOG("WARNING: alphaExecuteStage started without memory system attached");
    }
}

void alphaExecuteStage::onStageInitialize() { DEBUG_LOG("alphaExecuteStage::onStageInitialize()"); }

void alphaExecuteStage::onStageShutdown()
{
    DEBUG_LOG("alphaExecuteStage::onStageShutdown()");

    // Wait for pending heavy operations
    if (m_heavyOpPool)
    {
        m_heavyOpPool->waitForDone(5000); // Wait up to 5 seconds
    }

    // Log final statistics
    DEBUG_LOG("Final execution statistics:");
    DEBUG_LOG("  Total executed: %llu", m_executeStats.instructionsExecuted.load());
    DEBUG_LOG("  Trivial: %llu", m_executeStats.trivialInstructions.load());
    DEBUG_LOG("  Moderate: %llu", m_executeStats.moderateInstructions.load());
    DEBUG_LOG("  Heavy: %llu", m_executeStats.heavyInstructions.load());
    DEBUG_LOG("  Inline: %llu", m_executeStats.inlineExecutions.load());
    DEBUG_LOG("  Async: %llu", m_executeStats.asyncExecutions.load());
    DEBUG_LOG("  Exceptions: %llu", m_executeStats.executionExceptions.load());
}

alphaExecuteStage::ExecutionCost alphaExecuteStage::classifyInstruction(InstrPtr instr)
{
    uint32_t opcode = instr->getOpcode();

    switch (opcode)
    {
    // Trivial operations
    case 0x10: // Integer arithmetic - most are trivial
    case 0x11: // Integer logical
    {
        uint32_t function = instr->getFunction();
        if (function == 0x2C || function == 0x6C)
        { // MULQ variants
            return ExecutionCost::Moderate;
        }
        return ExecutionCost::Trivial;
    }

    case 0x08:
    case 0x09: // LDA, LDAH
    case 0x30:
    case 0x34: // BR, BSR
    case 0x39:
    case 0x3D:
    case 0x3E:
    case 0x3F: // BEQ, BNE, BGE, BGT
    case 0x1A: // JMP format
        return ExecutionCost::Trivial;

    // Moderate operations
    case 0x12:
    case 0x13: // Integer shifts and byte manipulation
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D: // Memory operations
    case 0x16: // Most floating-point operations
    {
        if (opcode == 0x16)
        {
            uint32_t function = instr->getFunction();
            if (function == 0x083 || function == 0x0A3)
            { // DIVS, DIVT
                return ExecutionCost::Heavy;
            }
            if (function == 0x08A || function == 0x0AA)
            { // SQRTS, SQRTT
                return ExecutionCost::Heavy;
            }
        }
        return ExecutionCost::Moderate;
    }

    // Heavy operations
    case 0x14:
    case 0x15:
    case 0x17: // Complex floating-point
        return ExecutionCost::Heavy;

    default:
        return ExecutionCost::Moderate;
    }
}

bool alphaExecuteStage::executeInline(InstrPtr instr)
{
    QElapsedTimer execTimer;
    execTimer.start();

    bool success = false;

    try
    {
        // Cast to specific instruction types and execute
        if (auto intInstr = qSharedPointerDynamicCast<alphaIntegerInstruction>(instr))
        {
            executeInteger(intInstr);
            success = true;
        }
        else if (auto fpInstr = qSharedPointerDynamicCast<alphaFloatingPointInstruction>(instr))
        {
            executeFloatingPoint(fpInstr);
            success = true;
        }
        else if (auto memInstr = qSharedPointerDynamicCast<alphaMemoryInstruction>(instr))
        {
            executeMemory(memInstr);
            success = true;
        }
        else if (auto branchInstr = qSharedPointerDynamicCast<alphaBranchInstruction>(instr))
        {
            executeBranch(branchInstr);
            success = true;
        }
        else
        {
            DEBUG_LOG("ERROR: Unknown instruction type at PC 0x%llx", instr->getPC());
            success = false;
        }
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG("ERROR: Exception during inline execution: %s", e.what());
        success = false;
    }

    // Record execution time
    uint64_t execTime = static_cast<uint64_t>(execTimer.nsecsElapsed());
    m_executeStats.totalExecutionCycles.fetch_add(execTime, std::memory_order_relaxed);

    return success;
}

void alphaExecuteStage::executeAsync(InstrPtr instr)
{
    DEBUG_LOG("Scheduling heavy operation for PC 0x%llx", instr->getPC());

    m_pendingHeavyOps.fetch_add(1, std::memory_order_acq_rel);

    QString operation = QString("Heavy_Op_0x%1").arg(instr->getOpcode(), 2, 16, QChar('0'));
    emit sigHeavyOperationStarted(instr->getPC(), operation);

    auto *task = new HeavyOperationTask(this, instr);
    m_heavyOpPool->start(task);
}

void alphaExecuteStage::executeInteger(QSharedPointer<alphaIntegerInstruction> instr)
{
    uint32_t opcode = instr->getOpcode();

    // Load operands
    int64_t operandA = readRegister(instr->getRa());
    int64_t operandB = instr->getIsLiteral() ? static_cast<int64_t>(instr->getLiteral()) : readRegister(instr->getRb());

    instr->setOperandA(operandA);
    instr->setOperandB(operandB);

    bool success = false;

    switch (opcode)
    {
    case 0x10: // Integer arithmetic
        success = executeIntegerArithmetic(instr);
        break;

    case 0x11: // Integer logical
        success = executeIntegerLogical(instr);
        break;

    case 0x12: // Integer shift
        success = executeIntegerShift(instr);
        break;

    case 0x13: // Integer multiply
        success = executeIntegerMultiply(instr);
        break;

    default:
        DEBUG_LOG("ERROR: Unhandled integer opcode 0x%x", opcode);
        success = false;
        break;
    }

    if (success && instr->getRc() != 31)
    { // R31 is always zero, don't write
        writeRegister(instr->getRc(), instr->getResult());
    }
}

void alphaExecuteStage::executeFloatingPoint(QSharedPointer<alphaFloatingPointInstruction> instr)
{
    uint32_t function = instr->getFunction();

    // Load FP operands
    double operandA = readFloatRegister(instr->getRa());
    double operandB = readFloatRegister(instr->getRb());

    instr->setFpOperandA(operandA);
    instr->setFpOperandB(operandB);

    bool success = false;

    if (function >= 0x080 && function <= 0x0BF)
    {
        success = executeFloatArithmetic(instr);
    }
    else if (function >= 0x0A0 && function <= 0x0AF)
    {
        success = executeFloatComparison(instr);
    }
    else
    {
        success = executeFloatConversion(instr);
    }

    if (success && instr->getRc() != 31)
    {
        writeFloatRegister(instr->getRc(), instr->getFpResult());
    }
}

void alphaExecuteStage::executeMemory(QSharedPointer<alphaMemoryInstruction> instr)
{
    // Calculate effective address
    uint64_t effectiveAddr = calculateEffectiveAddress(instr);
    instr->setEffectiveAddress(effectiveAddr);

    bool success = false;

    if (instr->getIsLoad())
    {
        success = executeLoad(instr);
    }
    else if (instr->getIsStore())
    {
        success = executeStore(instr);
    }
    else
    {
        // Address calculation only (LDA, LDAH)
        if (instr->getRa() != 31)
        {
            writeRegister(instr->getRa(), static_cast<int64_t>(effectiveAddr));
        }
        success = true;
    }

    if (!success)
    {
        DEBUG_LOG("ERROR: Memory operation failed at PC 0x%llx, addr=0x%llx", instr->getPC(), effectiveAddr);
    }
}

void alphaExecuteStage::executeBranch(QSharedPointer<alphaBranchInstruction> instr)
{
    bool conditionMet = evaluateBranchCondition(instr);
    instr->setConditionMet(conditionMet);

    if (conditionMet)
    {
        // Branch will be taken - this would typically signal the fetch stage
        DEBUG_LOG("Branch taken to 0x%llx", instr->getTargetAddress());
    }

    updateBranchPrediction(instr, conditionMet);
}

// Detailed implementation methods would continue...
// For brevity, showing key structure and pattern

bool alphaExecuteStage::executeIntegerArithmetic(QSharedPointer<alphaIntegerInstruction> instr)
{
    uint32_t function = instr->getFunction();
    int64_t result = 0;

    switch (function)
    {
    case 0x00: // ADDL
        result = static_cast<int32_t>(instr->getOperandA() + instr->getOperandB());
        break;
    case 0x20: // ADDQ
        result = instr->getOperandA() + instr->getOperandB();
        break;
    case 0x09: // SUBL
        result = static_cast<int32_t>(instr->getOperandA() - instr->getOperandB());
        break;
    case 0x29: // SUBQ
        result = instr->getOperandA() - instr->getOperandB();
        break;
    case 0x0C: // MULL
        result = static_cast<int32_t>(instr->getOperandA() * instr->getOperandB());
        break;
    case 0x2C: // MULQ
        result = instr->getOperandA() * instr->getOperandB();
        break;
    default:
        return false;
    }

    instr->setResult(result);
    return true;
}

// Additional helper methods implementation...

int64_t alphaExecuteStage::readRegister(uint32_t reg)
{
    if (reg == 31)
        return 0; // R31 is always zero

    if (m_registerFile)
    {
        // return m_registerFile->readInteger(reg);
        return 0; // Placeholder
    }
    return 0;
}

void alphaExecuteStage::writeRegister(uint32_t reg, int64_t value)
{
    if (reg == 31)
        return; // R31 is read-only zero

    if (m_registerFile)
    {
        // m_registerFile->writeInteger(reg, value);
        DEBUG_LOG("Write R%d = 0x%llx", reg, value);
    }
}

double alphaExecuteStage::readFloatRegister(uint32_t reg)
{
    if (reg == 31)
        return 0.0; // F31 is always zero

    if (m_registerFile)
    {
        // return m_registerFile->readFloat(reg);
        return 0.0; // Placeholder
    }
    return 0.0;
}

void alphaExecuteStage::writeFloatRegister(uint32_t reg, double value)
{
    if (reg == 31)
        return; // F31 is read-only zero

    if (m_registerFile)
    {
        // m_registerFile->writeFloat(reg, value);
        DEBUG_LOG("Write F%d = %f", reg, value);
    }
}

uint64_t alphaExecuteStage::calculateEffectiveAddress(QSharedPointer<alphaMemoryInstruction> instr)
{
    int64_t baseAddr = readRegister(instr->getRb());
    int64_t displacement = instr->getDisplacement();

    return static_cast<uint64_t>(baseAddr + displacement);
}

bool alphaExecuteStage::executeLoad(QSharedPointer<alphaMemoryInstruction> instr)
{
    // Placeholder - would call memory system
    // instr->setMemoryData(m_memorySystem->read(instr->getEffectiveAddress(), instr->getAccessSize()));

    DEBUG_LOG("Load from 0x%llx, size=%d", instr->getEffectiveAddress(), instr->getAccessSize());
    return true;
}

bool alphaExecuteStage::executeStore(QSharedPointer<alphaMemoryInstruction> instr)
{
    // Placeholder - would call memory system
    int64_t storeData = readRegister(instr->getRa());
    // m_memorySystem->write(instr->getEffectiveAddress(), storeData, instr->getAccessSize());

    DEBUG_LOG("Store to 0x%llx, data=0x%llx, size=%d", instr->getEffectiveAddress(), storeData, instr->getAccessSize());
    return true;
}

void alphaExecuteStage::handleExecutionException(InstrPtr instr, const QString &error)
{
    m_executeStats.executionExceptions.fetch_add(1, std::memory_order_relaxed);
    emit sigExecutionException(instr->getPC(), error);

    DEBUG_LOG("Execution exception at PC 0x%llx: %s", instr->getPC(), qPrintable(error));
}

void alphaExecuteStage::updateExecutionStatistics(ExecutionCost cost, bool isAsync)
{
    switch (cost)
    {
    case ExecutionCost::Trivial:
        m_executeStats.trivialInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case ExecutionCost::Moderate:
        m_executeStats.moderateInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    case ExecutionCost::Heavy:
        m_executeStats.heavyInstructions.fetch_add(1, std::memory_order_relaxed);
        break;
    }

    if (isAsync)
    {
        m_executeStats.asyncExecutions.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        m_executeStats.inlineExecutions.fetch_add(1, std::memory_order_relaxed);
    }
}

bool alphaExecuteStage::evaluateBranchCondition(QSharedPointer<alphaBranchInstruction> instr)
{
    uint32_t opcode = instr->getOpcode();
    int64_t regValue = readRegister(instr->getRa());

    switch (opcode)
    {
    case 0x39:
        return regValue == 0; // BEQ
    case 0x3D:
        return regValue != 0; // BNE
    case 0x3A:
        return regValue < 0; // BLT
    case 0x3E:
        return regValue >= 0; // BGE
    case 0x3B:
        return regValue <= 0; // BLE
    case 0x3F:
        return regValue > 0; // BGT
    case 0x38:
        return (regValue & 1) == 0; // BLBC
    case 0x3C:
        return (regValue & 1) != 0; // BLBS
    case 0x30:
    case 0x34:
        return true; // BR, BSR (unconditional)
    default:
        return false;
    }
}

void alphaExecuteStage::updateBranchPrediction(QSharedPointer<alphaBranchInstruction> instr, bool taken)
{
    // Placeholder for branch prediction update
    DEBUG_LOG("Branch prediction update: PC=0x%llx, taken=%s", instr->getPC(), taken ? "true" : "false");
}

// Placeholder implementations for other execute methods...
bool alphaExecuteStage::executeIntegerLogical(QSharedPointer<alphaIntegerInstruction> instr) { return true; }
bool alphaExecuteStage::executeIntegerShift(QSharedPointer<alphaIntegerInstruction> instr) { return true; }
bool alphaExecuteStage::executeIntegerMultiply(QSharedPointer<alphaIntegerInstruction> instr) { return true; }
bool alphaExecuteStage::executeFloatArithmetic(QSharedPointer<alphaFloatingPointInstruction> instr) { return true; }
bool alphaExecuteStage::executeFloatComparison(QSharedPointer<alphaFloatingPointInstruction> instr) { return true; }
bool alphaExecuteStage::executeFloatConversion(QSharedPointer<alphaFloatingPointInstruction> instr) { return true; }

// --

alphaWritebackStage::alphaWritebackStage(QObject *parent)
    : basePipelineStage("Writeback", 512, parent), m_registerFile(nullptr), m_performanceCounters(nullptr)
{
    DEBUG_LOG("alphaWritebackStage created");
}

alphaWritebackStage::~alphaWritebackStage()
{
    DEBUG_LOG("alphaWritebackStage destroyed - committed %llu instructions, %llu exceptions",
              m_writebackStats.instructionsCommitted.load(), m_writebackStats.exceptionsRaised.load());
}

void alphaWritebackStage::initialize()
{
    DEBUG_LOG("alphaWritebackStage::initialize()");

    // Call base class initialization
    basePipelineStage::initialize();

    // Reset writeback statistics
    m_writebackStats.instructionsCommitted.store(0);
    m_writebackStats.branchesTaken.store(0);
    m_writebackStats.branchesNotTaken.store(0);
    m_writebackStats.exceptionsRaised.store(0);
    m_writebackStats.registerWrites.store(0);
    m_writebackStats.floatRegisterWrites.store(0);
    m_writebackStats.retiredInstructions.store(0);
    m_writebackStats.commitStalls.store(0);

    m_totalCommitCycles.store(0);
    m_lastCommittedPC.store(0);

    // Clear pending exceptions
    QMutexLocker locker(&m_exceptionMutex);
    m_pendingExceptions.clear();
}

void alphaWritebackStage::initialize_SignalsAndSlots()
{
    DEBUG_LOG("alphaWritebackStage::initialize_SignalsAndSlots()");

    // Call base class signal/slot initialization
    basePipelineStage::initialize_SignalsAndSlots();

    // No additional internal connections needed
}

void alphaWritebackStage::process(InstrPtr instr)
{
    if (!instr)
    {
        DEBUG_LOG("ERROR: alphaWritebackStage received null instruction");
        incrementStallCounter();
        return;
    }

    if (!instr->isExecuted())
    {
        DEBUG_LOG("WARNING: Attempting to writeback unexecuted instruction at PC 0x%llx", instr->getPC());
        handleCommitFailure(instr, "Instruction not executed");
        return;
    }

    DEBUG_LOG("Writing back instruction at PC 0x%llx, opcode=0x%x", instr->getPC(), instr->getOpcode());

    QElapsedTimer commitTimer;
    commitTimer.start();

    try
    {
        // Validate instruction can be committed
        if (!validateCommit(instr))
        {
            handleCommitFailure(instr, "Commit validation failed");
            return;
        }

        // Check for exceptions first
        if (instr->hasException())
        {
            handleException(instr);
            return;
        }

        // Handle branch instructions
        if (auto branchInstr = qSharedPointerDynamicCast<alphaBranchInstruction>(instr))
        {
            handleBranch(instr);
        }

        // Commit instruction results
        commitInstruction(instr);

        // Update performance counters
        updatePerformanceCounters(instr);

        // Record retirement
        recordInstructionRetirement(instr);

        // Update statistics
        m_writebackStats.instructionsCommitted.fetch_add(1, std::memory_order_relaxed);
        m_lastCommittedPC.store(instr->getPC(), std::memory_order_relaxed);

        // Record commit timing
        uint64_t commitCycles = static_cast<uint64_t>(commitTimer.nsecsElapsed());
        m_totalCommitCycles.fetch_add(commitCycles, std::memory_order_relaxed);

        // Signal successful commit
        emit sigInstructionCommitted(instr->getPC());
        emit sigRetirementComplete(instr->getPC(), commitCycles);

        DEBUG_LOG("Successfully committed instruction at PC 0x%llx", instr->getPC());
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG("ERROR: Exception during writeback: %s", e.what());
        handleCommitFailure(instr, QString("Writeback exception: %1").arg(e.what()));
    }
}

void alphaWritebackStage::onStageStart()
{
    DEBUG_LOG("alphaWritebackStage::onStageStart()");

    if (!m_registerFile)
    {
        DEBUG_LOG("WARNING: alphaWritebackStage started without register file attached");
    }
}

void alphaWritebackStage::onStageInitialize() { DEBUG_LOG("alphaWritebackStage::onStageInitialize()"); }

void alphaWritebackStage::onStageShutdown()
{
    DEBUG_LOG("alphaWritebackStage::onStageShutdown()");

    // Log final statistics
    DEBUG_LOG("Final writeback statistics:");
    DEBUG_LOG("  Instructions committed: %llu", m_writebackStats.instructionsCommitted.load());
    DEBUG_LOG("  Branches taken: %llu", m_writebackStats.branchesTaken.load());
    DEBUG_LOG("  Branches not taken: %llu", m_writebackStats.branchesNotTaken.load());
    DEBUG_LOG("  Exceptions raised: %llu", m_writebackStats.exceptionsRaised.load());
    DEBUG_LOG("  Register writes: %llu", m_writebackStats.registerWrites.load());
    DEBUG_LOG("  Float register writes: %llu", m_writebackStats.floatRegisterWrites.load());
    DEBUG_LOG("  Retired instructions: %llu", m_writebackStats.retiredInstructions.load());
    DEBUG_LOG("  Commit stalls: %llu", m_writebackStats.commitStalls.load());

    // Log any pending exceptions
    QMutexLocker locker(&m_exceptionMutex);
    if (!m_pendingExceptions.isEmpty())
    {
        DEBUG_LOG("WARNING: %d pending exceptions at shutdown", m_pendingExceptions.size());
    }
}

void alphaWritebackStage::commitInstruction(InstrPtr instr)
{
    // Commit results based on instruction type
    if (auto intInstr = qSharedPointerDynamicCast<alphaIntegerInstruction>(instr))
    {
        if (intInstr->getRc() != 31 && intInstr->hasValidResult())
        {
            writeRegister(intInstr->getRc(), intInstr->getResult());
        }
    }
    else if (auto fpInstr = qSharedPointerDynamicCast<alphaFloatingPointInstruction>(instr))
    {
        if (fpInstr->getRc() != 31 && fpInstr->hasValidResult())
        {
            writeFloatRegister(fpInstr->getRc(), fpInstr->getFpResult());
        }
    }
    else if (auto memInstr = qSharedPointerDynamicCast<alphaMemoryInstruction>(instr))
    {
        if (memInstr->getIsLoad() && memInstr->getRa() != 31)
        {
            // Convert memory data to appropriate format based on access size
            int64_t loadData = static_cast<int64_t>(memInstr->getMemoryData());

            switch (memInstr->getAccessSize())
            {
            case 1: // Byte load - sign extend
                loadData = static_cast<int8_t>(loadData);
                break;
            case 2: // Word load - sign extend
                loadData = static_cast<int16_t>(loadData);
                break;
            case 4: // Longword load - sign extend
                loadData = static_cast<int32_t>(loadData);
                break;
            case 8: // Quadword load - no extension needed
                break;
            }

            writeRegister(memInstr->getRa(), loadData);
        }
        // Store instructions don't need writeback to registers
    }

    logCommitEvent(instr, "COMMIT");
}

void alphaWritebackStage::handleBranch(InstrPtr instr)
{
    auto branchInstr = qSharedPointerDynamicCast<alphaBranchInstruction>(instr);
    if (!branchInstr)
    {
        return;
    }

    bool taken = branchInstr->getConditionMet();
    uint64_t target = taken ? branchInstr->getTargetAddress() : (instr->getPC() + 4);

    // Update branch statistics
    updateBranchStatistics(taken);

    // Notify branch predictor
    notifyBranchPredictor(instr->getPC(), taken, target);

    // Signal branch resolution
    emit sigBranchResolved(instr->getPC(), taken, target);

    DEBUG_LOG("Branch at PC 0x%llx: %s, target=0x%llx", instr->getPC(), taken ? "TAKEN" : "NOT_TAKEN", target);

    logCommitEvent(instr, taken ? "BRANCH_TAKEN" : "BRANCH_NOT_TAKEN");
}

void alphaWritebackStage::handleException(InstrPtr instr)
{
    uint32_t vector = getExceptionVector(instr);
    QString description = instr->getExceptionDescription();

    // Process different exception types
    switch (vector)
    {
    case 0x01: // Arithmetic exception
        handleArithmeticException(instr);
        break;
    case 0x02: // Memory management exception
        handleMemoryException(instr);
        break;
    case 0x03: // Privilege violation
        handlePrivilegeException(instr);
        break;
    default:
        processException(instr);
        break;
    }

    raiseException(vector, instr->getPC(), description);
    logCommitEvent(instr, QString("EXCEPTION_%1").arg(vector));
}

void alphaWritebackStage::updatePerformanceCounters(InstrPtr instr)
{
    if (!m_performanceCounters)
    {
        return;
    }

    // Update instruction mix counters
    updateInstructionMix(instr);

    // Record execution metrics
    recordExecutionMetrics(instr);

    // Update cycle counters
    uint64_t executionCycles = instr->getExecutionCycles();
    if (executionCycles > 0)
    {
        // m_performanceCounters->addCycles(executionCycles);
        emit sigPerformanceEvent("EXECUTION_CYCLES", instr->getPC(), executionCycles);
    }

    // Record cache events
    if (instr->hasCacheMiss())
    {
        // m_performanceCounters->incrementCacheMisses();
        emit sigPerformanceEvent("CACHE_MISS", instr->getPC(), 1);
    }
}

void alphaWritebackStage::writeRegister(uint32_t reg, int64_t value)
{
    if (reg == 31)
    {
        return; // R31 is read-only zero
    }

    if (m_registerFile)
    {
        // m_registerFile->writeInteger(reg, value);
        m_writebackStats.registerWrites.fetch_add(1, std::memory_order_relaxed);

        DEBUG_LOG("Writeback: R%d = 0x%llx", reg, value);
    }
    else
    {
        DEBUG_LOG("ERROR: No register file attached for writeback");
    }
}

void alphaWritebackStage::writeFloatRegister(uint32_t reg, double value)
{
    if (reg == 31)
    {
        return; // F31 is read-only zero
    }

    if (m_registerFile)
    {
        // m_registerFile->writeFloat(reg, value);
        m_writebackStats.floatRegisterWrites.fetch_add(1, std::memory_order_relaxed);

        DEBUG_LOG("Writeback: F%d = %f", reg, value);
    }
    else
    {
        DEBUG_LOG("ERROR: No register file attached for float writeback");
    }
}

bool alphaWritebackStage::validateCommit(InstrPtr instr)
{
    // Check if instruction is in a valid state for commit
    if (!instr->isValid())
    {
        DEBUG_LOG("ERROR: Cannot commit invalid instruction at PC 0x%llx", instr->getPC());
        return false;
    }

    if (!instr->isDecoded())
    {
        DEBUG_LOG("ERROR: Cannot commit undecoded instruction at PC 0x%llx", instr->getPC());
        return false;
    }

    if (!instr->isExecuted())
    {
        DEBUG_LOG("ERROR: Cannot commit unexecuted instruction at PC 0x%llx", instr->getPC());
        return false;
    }

    return true;
}

bool alphaWritebackStage::canCommitInstruction(InstrPtr instr)
{
    // Check for resource availability and dependencies
    return validateCommit(instr);
}

void alphaWritebackStage::handleCommitFailure(InstrPtr instr, const QString &reason)
{
    DEBUG_LOG("ERROR: Commit failure at PC 0x%llx: %s", instr->getPC(), qPrintable(reason));

    m_writebackStats.commitStalls.fetch_add(1, std::memory_order_relaxed);
    incrementStallCounter();

    // Could potentially retry or raise an exception
    logCommitEvent(instr, QString("COMMIT_FAILURE_%1").arg(reason));
}

void alphaWritebackStage::updateBranchStatistics(bool taken)
{
    if (taken)
    {
        m_writebackStats.branchesTaken.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        m_writebackStats.branchesNotTaken.fetch_add(1, std::memory_order_relaxed);
    }
}

void alphaWritebackStage::notifyBranchPredictor(uint64_t pc, bool taken, uint64_t target)
{
    // This would notify the fetch stage's branch predictor
    // For now, just log the event
    DEBUG_LOG("Branch predictor update: PC=0x%llx, taken=%s, target=0x%llx", pc, taken ? "true" : "false", target);
}

void alphaWritebackStage::raiseException(uint32_t vector, uint64_t pc, const QString &description)
{
    QMutexLocker locker(&m_exceptionMutex);

    ExceptionInfo exception;
    exception.vector = vector;
    exception.pc = pc;
    exception.description = description;
    exception.handled = false;

    m_pendingExceptions.append(exception);

    m_writebackStats.exceptionsRaised.fetch_add(1, std::memory_order_relaxed);

    emit sigExceptionRaised(vector, pc);

    DEBUG_LOG("Exception raised: vector=0x%x, PC=0x%llx, desc='%s'", vector, pc, qPrintable(description));
}

void alphaWritebackStage::recordInstructionRetirement(InstrPtr instr)
{
    m_writebackStats.retiredInstructions.fetch_add(1, std::memory_order_relaxed);

    // Record retirement timing and other metrics
    uint64_t retirementCycle = static_cast<uint64_t>(getStageTimer().elapsed());

    // Could update retirement order buffer or other structures here
    DEBUG_LOG("Instruction retired: PC=0x%llx, cycle=%llu", instr->getPC(), retirementCycle);
}

void alphaWritebackStage::updateInstructionMix(InstrPtr instr)
{
    QString instrType = getInstructionTypeName(instr);
    emit sigPerformanceEvent("INSTRUCTION_MIX", instr->getPC(), static_cast<uint64_t>(instr->getOpcode()));
}

void alphaWritebackStage::recordExecutionMetrics(InstrPtr instr)
{
    // Record various execution metrics for performance analysis
    if (instr->getExecutionTime() > 0)
    {
        emit sigPerformanceEvent("EXECUTION_TIME", instr->getPC(), instr->getExecutionTime());
    }

    if (instr->getStallCycles() > 0)
    {
        emit sigPerformanceEvent("STALL_CYCLES", instr->getPC(), instr->getStallCycles());
    }
}

QString alphaWritebackStage::getInstructionTypeName(InstrPtr instr) const
{
    if (qSharedPointerDynamicCast<alphaIntegerInstruction>(instr))
    {
        return "INTEGER";
    }
    else if (qSharedPointerDynamicCast<alphaFloatingPointInstruction>(instr))
    {
        return "FLOATING_POINT";
    }
    else if (qSharedPointerDynamicCast<alphaMemoryInstruction>(instr))
    {
        return "MEMORY";
    }
    else if (qSharedPointerDynamicCast<alphaBranchInstruction>(instr))
    {
        return "BRANCH";
    }
    else
    {
        return "UNKNOWN";
    }
}

uint32_t alphaWritebackStage::getExceptionVector(InstrPtr instr) const
{
    // Extract exception vector from instruction or use default
    return instr->getExceptionVector();
}

bool alphaWritebackStage::isPrivilegedInstruction(InstrPtr instr) const
{
    // Check if instruction requires privilege
    return instr->getOpcode() == 0x00; // CALL_PAL
}

void alphaWritebackStage::logCommitEvent(InstrPtr instr, const QString &event) const
{
    DEBUG_LOG("COMMIT_EVENT: PC=0x%llx, event=%s, opcode=0x%x", instr->getPC(), qPrintable(event), instr->getOpcode());
}

// Exception handling implementations
void alphaWritebackStage::processException(InstrPtr instr)
{
    DEBUG_LOG("Processing generic exception for instruction at PC 0x%llx", instr->getPC());
}

void alphaWritebackStage::handleArithmeticException(InstrPtr instr)
{
    DEBUG_LOG("Handling arithmetic exception at PC 0x%llx", instr->getPC());
    // Handle overflow, underflow, division by zero, etc.
}

void alphaWritebackStage::handleMemoryException(InstrPtr instr)
{
    DEBUG_LOG("Handling memory exception at PC 0x%llx", instr->getPC());
    // Handle page faults, access violations, etc.
}

void alphaWritebackStage::handlePrivilegeException(InstrPtr instr)
{
    DEBUG_LOG("Handling privilege exception at PC 0x%llx", instr->getPC());
    // Handle privilege violations
}

//--

alphaHybridExecuteStage::alphaHybridExecuteStage(QObject *parent)
    : alphaExecuteStage(parent), m_jitCompiler(nullptr), m_jitEnabled(true)
{
    DEBUG_LOG("alphaHybridExecuteStage created");

    // Update stage name to reflect hybrid nature
    // Note: This would require a protected setter in base class
}

alphaHybridExecuteStage::~alphaHybridExecuteStage()
{
    DEBUG_LOG("alphaHybridExecuteStage destroyed - interpreted: %llu, compiled: %llu, JIT hit rate: %.2f%%",
              m_hybridStats.interpretedCount.load(), m_hybridStats.compiledCount.load(), getJitHitRate());
}

void alphaHybridExecuteStage::initialize()
{
    DEBUG_LOG("alphaHybridExecuteStage::initialize()");

    // Call base class initialization
    alphaExecuteStage::initialize();

    // Reset hybrid-specific statistics
    m_hybridStats.interpretedCount.store(0);
    m_hybridStats.compiledCount.store(0);
    m_hybridStats.profiledCount.store(0);
    m_hybridStats.jitHits.store(0);
    m_hybridStats.jitMisses.store(0);
    m_hybridStats.modeTransitions.store(0);
    m_hybridStats.compilationTriggers.store(0);

    // Clear tracking data
    QMutexLocker locker(&m_trackingMutex);
    m_executionCounts.clear();
    m_executionTimers.clear();
    m_currentModes.clear();
}

void alphaHybridExecuteStage::initialize_SignalsAndSlots()
{
    DEBUG_LOG("alphaHybridExecuteStage::initialize_SignalsAndSlots()");

    // Call base class signal/slot initialization
    alphaExecuteStage::initialize_SignalsAndSlots();

    // Connect to JIT compiler if available
    if (m_jitCompiler)
    {
        connect(this, &alphaHybridExecuteStage::sigJitCompilationTriggered, m_jitCompiler,
                &alphaJitCompiler::onHotBlockDetected, Qt::QueuedConnection);
    }
}

void alphaHybridExecuteStage::process(InstrPtr instr)
{
    if (!instr)
    {
        DEBUG_LOG("ERROR: alphaHybridExecuteStage received null instruction");
        incrementStallCounter();
        return;
    }

    uint64_t pc = instr->getPC();

    DEBUG_LOG("Hybrid execution for instruction at PC 0x%llx", pc);

    QElapsedTimer execTimer;
    execTimer.start();

    // Update execution count for this PC
    updateExecutionCount(pc);

    // Select execution mode based on profiling data
    ExecutionMode mode = selectExecutionMode(instr);
    ExecutionMode previousMode = getCurrentMode(pc);

    // Check for mode transition
    if (mode != previousMode)
    {
        transitionExecutionMode(instr, mode);
    }

    // Execute based on selected mode
    bool success = false;

    switch (mode)
    {
    case ExecutionMode::Interpret:
        executeInterpreted(instr);
        m_hybridStats.interpretedCount.fetch_add(1, std::memory_order_relaxed);
        success = true;
        break;

    case ExecutionMode::Profile:
        executeWithProfiling(instr);
        m_hybridStats.profiledCount.fetch_add(1, std::memory_order_relaxed);
        success = true;
        break;

    case ExecutionMode::Compiled:
        success = tryExecuteCompiled(instr);
        if (success)
        {
            m_hybridStats.compiledCount.fetch_add(1, std::memory_order_relaxed);
            recordJitHit(instr);
        }
        else
        {
            // Fallback to interpreted execution
            executeInterpreted(instr);
            m_hybridStats.interpretedCount.fetch_add(1, std::memory_order_relaxed);
            recordJitMiss(instr);
            success = true;
        }
        break;
    }

    // Record execution performance
    uint64_t executionTime = static_cast<uint64_t>(execTimer.nsecsElapsed());
    recordExecutionPerformance(instr, mode, executionTime);

    if (success)
    {
        // Mark instruction as executed
        instr->setExecuted(true);

        DEBUG_LOG("Hybrid execution completed for PC 0x%llx, mode=%s, time=%llu ns", pc,
                  qPrintable(executionModeToString(mode)), executionTime);
    }
    else
    {
        DEBUG_LOG("ERROR: Hybrid execution failed for PC 0x%llx", pc);
        // Call base class exception handling
        handleExecutionException(instr, "Hybrid execution failed");
    }
}

void alphaHybridExecuteStage::onStageStart()
{
    DEBUG_LOG("alphaHybridExecuteStage::onStageStart()");

    // Call base class start
    alphaExecuteStage::onStageStart();

    if (!m_jitCompiler && m_jitEnabled)
    {
        DEBUG_LOG("WARNING: JIT compilation enabled but no JIT compiler attached");
    }
}

void alphaHybridExecuteStage::onStageInitialize()
{
    DEBUG_LOG("alphaHybridExecuteStage::onStageInitialize()");

    // Call base class initialization
    alphaExecuteStage::onStageInitialize();
}

void alphaHybridExecuteStage::onStageShutdown()
{
    DEBUG_LOG("alphaHybridExecuteStage::onStageShutdown()");

    // Log hybrid execution statistics
    DEBUG_LOG("Hybrid execution final statistics:");
    DEBUG_LOG("  Interpreted executions: %llu", m_hybridStats.interpretedCount.load());
    DEBUG_LOG("  Compiled executions: %llu", m_hybridStats.compiledCount.load());
    DEBUG_LOG("  Profiled executions: %llu", m_hybridStats.profiledCount.load());
    DEBUG_LOG("  JIT hits: %llu", m_hybridStats.jitHits.load());
    DEBUG_LOG("  JIT misses: %llu", m_hybridStats.jitMisses.load());
    DEBUG_LOG("  JIT hit rate: %.2f%%", getJitHitRate());
    DEBUG_LOG("  Mode transitions: %llu", m_hybridStats.modeTransitions.load());
    DEBUG_LOG("  Compilation triggers: %llu", m_hybridStats.compilationTriggers.load());

    // Call base class shutdown
    alphaExecuteStage::onStageShutdown();
}

alphaHybridExecuteStage::ExecutionMode alphaHybridExecuteStage::selectExecutionMode(InstrPtr instr)
{
    uint64_t pc = instr->getPC();
    int execCount = getExecutionCount(pc);

    if (!m_jitEnabled)
    {
        return ExecutionMode::Interpret;
    }

    // Check if compiled version is available
    if (m_jitCompiler && m_jitCompiler->hasCompiledBlock(pc))
    {
        return ExecutionMode::Compiled;
    }

    // Determine mode based on execution count
    if (execCount >= m_compilationThreshold)
    {
        // Hot enough for compilation
        if (shouldCompileInstruction(instr))
        {
            triggerJitCompilation(instr);
        }
        return ExecutionMode::Profile; // Continue profiling while compilation happens
    }
    else if (execCount >= m_profilingThreshold)
    {
        // Warm enough for profiling
        return ExecutionMode::Profile;
    }
    else
    {
        // Cold - just interpret
        return ExecutionMode::Interpret;
    }
}

void alphaHybridExecuteStage::executeInterpreted(InstrPtr instr)
{
    DEBUG_LOG("Executing interpreted mode for PC 0x%llx", instr->getPC());

    // Call base class execution - this handles the full instruction execution
    alphaExecuteStage::process(instr);
}

void alphaHybridExecuteStage::executeWithProfiling(InstrPtr instr)
{
    DEBUG_LOG("Executing with profiling for PC 0x%llx", instr->getPC());

    // Record profiling data if JIT compiler is available
    if (m_jitCompiler)
    {
        m_jitCompiler->recordExecution(instr->getPC(), instr->getRawBits());
    }

    // Execute using base class implementation
    alphaExecuteStage::process(instr);
}

bool alphaHybridExecuteStage::tryExecuteCompiled(InstrPtr instr)
{
    if (!m_jitCompiler)
    {
        DEBUG_LOG("No JIT compiler available for compiled execution");
        return false;
    }

    DEBUG_LOG("Attempting compiled execution for PC 0x%llx", instr->getPC());

    // Try to execute compiled code
    bool success = m_jitCompiler->tryExecuteCompiled(instr->getPC(), *m_registerFile, *m_memorySystem);

    if (success)
    {
        DEBUG_LOG("Successfully executed compiled code for PC 0x%llx", instr->getPC());
    }
    else
    {
        DEBUG_LOG("Compiled execution failed for PC 0x%llx, falling back", instr->getPC());
    }

    return success;
}

void alphaHybridExecuteStage::transitionExecutionMode(InstrPtr instr, ExecutionMode newMode)
{
    uint64_t pc = instr->getPC();
    ExecutionMode oldMode = getCurrentMode(pc);

    setCurrentMode(pc, newMode);
    m_hybridStats.modeTransitions.fetch_add(1, std::memory_order_relaxed);

    emit sigExecutionModeChanged(pc, executionModeToString(newMode));

    DEBUG_LOG("Execution mode transition for PC 0x%llx: %s -> %s", pc, qPrintable(executionModeToString(oldMode)),
              qPrintable(executionModeToString(newMode)));
}

bool alphaHybridExecuteStage::shouldProfileInstruction(InstrPtr instr)
{
    int execCount = getExecutionCount(instr->getPC());
    return execCount >= m_profilingThreshold && execCount < m_compilationThreshold;
}

bool alphaHybridExecuteStage::shouldCompileInstruction(InstrPtr instr)
{
    int execCount = getExecutionCount(instr->getPC());
    return execCount >= m_compilationThreshold && m_jitEnabled && m_jitCompiler;
}

void alphaHybridExecuteStage::recordExecutionPerformance(InstrPtr instr, ExecutionMode mode, uint64_t executionTime)
{
    uint64_t pc = instr->getPC();

    // Store timing data for performance analysis
    QMutexLocker locker(&m_trackingMutex);

    // Update execution timer for this PC
    if (!m_executionTimers.contains(pc))
    {
        m_executionTimers[pc] = QElapsedTimer();
        m_executionTimers[pc].start();
    }

    // Record performance data based on mode
    static QHash<uint64_t, uint64_t> interpretedTimes;
    static QHash<uint64_t, uint64_t> compiledTimes;

    if (mode == ExecutionMode::Interpret || mode == ExecutionMode::Profile)
    {
        interpretedTimes[pc] = executionTime;
    }
    else if (mode == ExecutionMode::Compiled)
    {
        compiledTimes[pc] = executionTime;

        // Calculate performance improvement if we have interpreted baseline
        if (interpretedTimes.contains(pc))
        {
            measurePerformanceImprovement(instr, interpretedTimes[pc], executionTime);
        }
    }
}

void alphaHybridExecuteStage::updateExecutionCount(uint64_t pc)
{
    QMutexLocker locker(&m_trackingMutex);

    if (!m_executionCounts.contains(pc))
    {
        m_executionCounts[pc] = 0;
    }

    m_executionCounts[pc]++;
}

void alphaHybridExecuteStage::measurePerformanceImprovement(InstrPtr instr, uint64_t interpretedTime,
                                                            uint64_t compiledTime)
{
    if (interpretedTime > 0)
    {
        double speedup = static_cast<double>(interpretedTime) / compiledTime;

        if (speedup > 1.1)
        { // At least 10% improvement
            emit sigPerformanceImprovement(instr->getPC(), speedup);

            DEBUG_LOG("Performance improvement detected: PC=0x%llx, speedup=%.2fx", instr->getPC(), speedup);
        }
    }
}

void alphaHybridExecuteStage::triggerJitCompilation(InstrPtr instr)
{
    if (!m_jitCompiler)
    {
        return;
    }

    m_hybridStats.compilationTriggers.fetch_add(1, std::memory_order_relaxed);
    emit sigJitCompilationTriggered(instr->getPC());

    DEBUG_LOG("Triggering JIT compilation for hot block at PC 0x%llx", instr->getPC());
}

void alphaHybridExecuteStage::recordJitHit(InstrPtr instr)
{
    m_hybridStats.jitHits.fetch_add(1, std::memory_order_relaxed);
}

void alphaHybridExecuteStage::recordJitMiss(InstrPtr instr)
{
    m_hybridStats.jitMisses.fetch_add(1, std::memory_order_relaxed);
}

alphaHybridExecuteStage::ExecutionMode alphaHybridExecuteStage::getCurrentMode(uint64_t pc) const
{
    QMutexLocker locker(&m_trackingMutex);
    return m_currentModes.value(pc, ExecutionMode::Interpret);
}

void alphaHybridExecuteStage::setCurrentMode(uint64_t pc, ExecutionMode mode)
{
    QMutexLocker locker(&m_trackingMutex);
    m_currentModes[pc] = mode;
}

int alphaHybridExecuteStage::getExecutionCount(uint64_t pc) const
{
    QMutexLocker locker(&m_trackingMutex);
    return m_executionCounts.value(pc, 0);
}

QString alphaHybridExecuteStage::executionModeToString(ExecutionMode mode) const
{
    switch (mode)
    {
    case ExecutionMode::Interpret:
        return "INTERPRET";
    case ExecutionMode::Profile:
        return "PROFILE";
    case ExecutionMode::Compiled:
        return "COMPILED";
    default:
        return "UNKNOWN";
    }
}

double alphaHybridExecuteStage::getJitHitRate() const
{
    uint64_t hits = m_hybridStats.jitHits.load();
    uint64_t misses = m_hybridStats.jitMisses.load();
    uint64_t total = hits + misses;

    return (total > 0) ? (static_cast<double>(hits) / total * 100.0) : 0.0;
}

QString alphaHybridExecuteStage::generateHybridReport() const
{
    QString report;
    QTextStream stream(&report);

    uint64_t totalExecutions =
        m_hybridStats.interpretedCount.load() + m_hybridStats.compiledCount.load() + m_hybridStats.profiledCount.load();

    stream << "=== Alpha Hybrid Execute Stage Report ===" << Qt::endl;
    stream << QString("Total Executions: %1").arg(totalExecutions) << Qt::endl;
    stream << QString("Interpreted: %1 (%.1f%%)")
                  .arg(m_hybridStats.interpretedCount.load())
                  .arg(totalExecutions > 0 ? m_hybridStats.interpretedCount.load() * 100.0 / totalExecutions : 0.0)
           << Qt::endl;
    stream << QString("Profiled: %1 (%.1f%%)")
                  .arg(m_hybridStats.profiledCount.load())
                  .arg(totalExecutions > 0 ? m_hybridStats.profiledCount.load() * 100.0 / totalExecutions : 0.0)
           << Qt::endl;
    stream << QString("Compiled: %1 (%.1f%%)")
                  .arg(m_hybridStats.compiledCount.load())
                  .arg(totalExecutions > 0 ? m_hybridStats.compiledCount.load() * 100.0 / totalExecutions : 0.0)
           << Qt::endl;
    stream << Qt::endl;

    stream << QString("JIT Hit Rate: %.2f%%").arg(getJitHitRate()) << Qt::endl;
    stream << QString("Mode Transitions: %1").arg(m_hybridStats.modeTransitions.load()) << Qt::endl;
    stream << QString("Compilation Triggers: %1").arg(m_hybridStats.compilationTriggers.load()) << Qt::endl;
    stream << Qt::endl;

    stream << QString("Profiling Threshold: %1").arg(m_profilingThreshold) << Qt::endl;
    stream << QString("Compilation Threshold: %1").arg(m_compilationThreshold) << Qt::endl;
    stream << QString("JIT Enabled: %1").arg(m_jitEnabled ? "Yes" : "No") << Qt::endl;

    return report;
}

//--

alphaPipelineController::alphaPipelineController(QObject *parent)
    : QObject(parent), m_metricsTimer(new QTimer(this)), m_tuningTimer(new QTimer(this))
{
    DEBUG_LOG("alphaPipelineController created");

    // Initialize performance tracking
    m_performanceTimer.start();

    // Configure timers
    m_metricsTimer->setInterval(1000); // Update metrics every second
    m_metricsTimer->setSingleShot(false);

    m_tuningTimer->setInterval(5000); // Tune every 5 seconds
    m_tuningTimer->setSingleShot(false);
}

alphaPipelineController::~alphaPipelineController()
{
    DEBUG_LOG("alphaPipelineController destroyed - executed %llu instructions, efficiency: %.2f%%",
              m_instructionsExecuted.load(), calculatePipelineEfficiency() * 100.0);
    shutdown();
}

void alphaPipelineController::initialize()
{
    DEBUG_LOG("alphaPipelineController::initialize()");

    if (m_state.load() != PipelineState::Stopped)
    {
        DEBUG_LOG("WARNING: Cannot initialize pipeline: not in stopped state");
        return;
    }

    transitionToState(PipelineState::Starting);

    try
    {
        createPipelineStages();
        connectPipelineStages();
        initializeMonitoring();
        setupPerformanceTracking();

        // Validate configuration
        validatePipelineConfiguration();

        transitionToState(PipelineState::Stopped);
        DEBUG_LOG("Pipeline controller initialized successfully");
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG("ERROR: Failed to initialize pipeline: %s", e.what());
        transitionToState(PipelineState::Stopped);
        throw;
    }

    initialize_SignalsAndSlots();
}

void alphaPipelineController::initialize_SignalsAndSlots()
{
    DEBUG_LOG("alphaPipelineController::initialize_SignalsAndSlots()");

    // Connect timer signals
    connect(m_metricsTimer, &QTimer::timeout, this, &alphaPipelineController::updatePerformanceMetrics);

    connect(m_tuningTimer, &QTimer::timeout, this, &alphaPipelineController::performPeriodicTuning);

    // Connect stage signals (if stages exist)
    if (m_writebackStage)
    {
        connect(m_writebackStage, &alphaWritebackStage::sigInstructionCommitted, this,
                &alphaPipelineController::onInstructionCommitted);

        connect(m_writebackStage, &alphaWritebackStage::sigBranchResolved, this,
                &alphaPipelineController::onBranchResolved);

        connect(m_writebackStage, &alphaWritebackStage::sigExceptionRaised, this,
                &alphaPipelineController::onExceptionRaised);
    }

    // Connect all stage stall and backpressure signals
    QVector<basePipelineStage *> stages = {m_fetchStage, m_decodeStage, m_executeStage, m_writebackStage};

    for (auto *stage : stages)
    {
        if (stage)
        {
            connect(stage, &basePipelineStage::sigStageStalled, this, &alphaPipelineController::onStageStalled);

            connect(stage, &basePipelineStage::sigBackpressureTriggered, this,
                    &alphaPipelineController::onBackpressureTriggered);
        }
    }
}

void alphaPipelineController::start()
{
    DEBUG_LOG("alphaPipelineController::start()");

    if (m_state.load() != PipelineState::Stopped)
    {
        DEBUG_LOG("WARNING: Cannot start pipeline: not in stopped state");
        return;
    }

    transitionToState(PipelineState::Starting);

    try
    {
        // Initialize all stages
        if (m_fetchStage)
            m_fetchStage->initialize();
        if (m_decodeStage)
            m_decodeStage->initialize();
        if (m_executeStage)
            m_executeStage->initialize();
        if (m_writebackStage)
            m_writebackStage->initialize();

        // Initialize support components
        if (m_monitor)
            m_monitor->initialize();
        if (m_stats)
            m_stats->initialize();
        if (m_jitCompiler)
            m_jitCompiler->initialize();

        // Move stages to worker threads
        if (m_fetchStage)
            m_fetchStage->moveToWorkerThread();
        if (m_decodeStage)
            m_decodeStage->moveToWorkerThread();
        if (m_executeStage)
            m_executeStage->moveToWorkerThread();
        if (m_writebackStage)
            m_writebackStage->moveToWorkerThread();

        // Start performance monitoring
        m_metricsTimer->start();
        if (m_dynamicTuningEnabled.load())
        {
            m_tuningTimer->start();
        }

        // Reset performance counters
        m_instructionsExecuted.store(0);
        m_cyclesElapsed.store(0);
        m_lastInstructionCount.store(0);
        m_lastCycleCount.store(0);
        m_performanceTimer.restart();

        transitionToState(PipelineState::Running);
        emit sigPipelineStarted();

        DEBUG_LOG("Pipeline started successfully");
    }
    catch (const std::exception &e)
    {
        DEBUG_LOG("ERROR: Failed to start pipeline: %s", e.what());
        transitionToState(PipelineState::Stopped);
        throw;
    }
}

void alphaPipelineController::stop()
{
    DEBUG_LOG("alphaPipelineController::stop()");

    if (m_state.load() != PipelineState::Running)
    {
        DEBUG_LOG("WARNING: Cannot stop pipeline: not in running state");
        return;
    }

    transitionToState(PipelineState::Stopping);

    // Stop timers
    m_metricsTimer->stop();
    m_tuningTimer->stop();

    // Shutdown stages gracefully
    if (m_fetchStage)
        m_fetchStage->shutdown();
    if (m_decodeStage)
        m_decodeStage->shutdown();
    if (m_executeStage)
        m_executeStage->shutdown();
    if (m_writebackStage)
        m_writebackStage->shutdown();

    // Shutdown support components
    if (m_jitCompiler)
        m_jitCompiler->shutdown();

    transitionToState(PipelineState::Stopped);
    emit sigPipelineStopped();

    DEBUG_LOG("Pipeline stopped successfully");
}

void alphaPipelineController::shutdown()
{
    DEBUG_LOG("alphaPipelineController::shutdown()");

    if (m_state.load() == PipelineState::Running)
    {
        stop();
    }

    cleanupResources();
    DEBUG_LOG("Pipeline shutdown complete");
}

void alphaPipelineController::attachRegisterFile(AlphaRegisterFile *regFile)
{
    m_registerFile = regFile;

    // Attach to stages that need register file access
    if (m_executeStage)
        m_executeStage->attachRegisterFile(regFile);
    if (m_writebackStage)
        m_writebackStage->attachRegisterFile(regFile);

    DEBUG_LOG("Register file attached to pipeline");
}

void alphaPipelineController::attachMemorySystem(AlphaMemorySystem *memSys)
{
    m_memorySystem = memSys;

    // Attach to stages that need memory system access
    if (m_fetchStage)
        m_fetchStage->attachMemorySystem(memSys);
    if (m_executeStage)
        m_executeStage->attachMemorySystem(memSys);

    DEBUG_LOG("Memory system attached to pipeline");
}

void alphaPipelineController::setProgramCounter(uint64_t pc)
{
    m_currentPC.store(pc, std::memory_order_release);

    if (m_fetchStage)
    {
        m_fetchStage->setProgramCounter(pc);
    }

    DEBUG_LOG("Program counter set to 0x%llx", pc);
}

void alphaPipelineController::handleBranch(uint64_t pc, bool taken, uint64_t target)
{
    if (taken)
    {
        setProgramCounter(target);

        // May need to flush pipeline for mispredicted branches
        if (shouldFlushForBranch(pc, target))
        {
            flushPipeline();
        }
    }

    DEBUG_LOG("Branch handled: PC=0x%llx, taken=%s, target=0x%llx", pc, taken ? "true" : "false", target);
}

void alphaPipelineController::flushPipeline()
{
    DEBUG_LOG("Pipeline flush requested");

    m_flushRequested.store(true, std::memory_order_release);
    m_pendingFlushes.fetch_add(1, std::memory_order_acq_rel);

    initiatePipelineFlush();
}

void alphaPipelineController::handleException(uint32_t vector, uint64_t faultingPC)
{
    DEBUG_LOG("Exception occurred: vector=0x%x, PC=0x%llx", vector, faultingPC);

    PipelineState oldState = m_state.exchange(PipelineState::Exception);

    recordException(vector, faultingPC, "Pipeline exception");
    handlePipelineException(vector, faultingPC);

    emit sigExceptionOccurred(vector, faultingPC);
}

alphaPipelineController::PipelinePerformance alphaPipelineController::getCurrentPerformance() const
{
    PipelinePerformance perf;

    uint64_t totalInstructions = m_instructionsExecuted.load();
    uint64_t totalCycles = m_cyclesElapsed.load();
    qint64 elapsedMs = m_performanceTimer.elapsed();

    perf.totalInstructions = totalInstructions;
    perf.totalCycles = totalCycles;

    // Calculate instructions per second
    if (elapsedMs > 0)
    {
        perf.instructionsPerSecond = (totalInstructions * 1000.0) / elapsedMs;
    }
    else
    {
        perf.instructionsPerSecond = 0.0;
    }

    // Calculate average IPC
    if (totalCycles > 0)
    {
        perf.averageIPC = static_cast<double>(totalInstructions) / totalCycles;
    }
    else
    {
        perf.averageIPC = 0.0;
    }

    // Calculate pipeline efficiency
    perf.pipelineEfficiency = calculatePipelineEfficiency();

    // Get stall cycles from stages
    perf.stallCycles = 0;
    if (m_fetchStage)
        perf.stallCycles += m_fetchStage->getStats().stallCycles;
    if (m_decodeStage)
        perf.stallCycles += m_decodeStage->getStats().stallCycles;
    if (m_executeStage)
        perf.stallCycles += m_executeStage->getStats().stallCycles;
    if (m_writebackStage)
        perf.stallCycles += m_writebackStage->getStats().stallCycles;

    // Get bottleneck stage
    if (m_monitor)
    {
        perf.bottleneckStage = m_monitor->getBottleneckStage();
    }

    return perf;
}

QString alphaPipelineController::generatePerformanceReport() const
{
    auto perf = getCurrentPerformance();

    QString report;
    QTextStream stream(&report);

    stream << "=== Alpha Pipeline Controller Performance Report ===" << Qt::endl;
    stream << Qt::endl;
    stream << QString("Pipeline State: %1").arg(stateToString(m_state.load())) << Qt::endl;
    stream << QString("Current PC: 0x%1").arg(m_currentPC.load(), 0, 16) << Qt::endl;
    stream << Qt::endl;

    stream << QString("Total Instructions: %1").arg(perf.totalInstructions) << Qt::endl;
    stream << QString("Total Cycles: %1").arg(perf.totalCycles) << Qt::endl;
    stream << QString("Instructions/Second: %1").arg(perf.instructionsPerSecond, 0, 'f', 2) << Qt::endl;
    stream << QString("Average IPC: %1").arg(perf.averageIPC, 0, 'f', 3) << Qt::endl;
    stream << QString("Pipeline Efficiency: %1%").arg(perf.pipelineEfficiency * 100, 0, 'f', 1) << Qt::endl;
    stream << QString("Stall Cycles: %1").arg(perf.stallCycles) << Qt::endl;

    if (!perf.bottleneckStage.isEmpty())
    {
        stream << QString("Bottleneck Stage: %1").arg(perf.bottleneckStage) << Qt::endl;
    }

    stream << Qt::endl;

    // Add stage-specific statistics
    QVector<QPair<QString, basePipelineStage *>> stages = {{"Fetch", m_fetchStage},
                                                           {"Decode", m_decodeStage},
                                                           {"Execute", m_executeStage},
                                                           {"Writeback", m_writebackStage}};

    for (const auto &pair : stages)
    {
        if (pair.second)
        {
            auto stats = pair.second->getStats();
            stream << QString("%1 Stage - Processed: %2, Queue Depth: %3, Stalls: %4")
                          .arg(pair.first)
                          .arg(stats.instructionsProcessed.load())
                          .arg(stats.queueDepth.load())
                          .arg(stats.stallCycles.load())
                   << Qt::endl;
        }
    }

    // Add JIT compiler statistics if available
    if (m_jitCompiler)
    {
        stream << Qt::endl;
        stream << m_jitCompiler->generateReport();
    }

    // Add hybrid execution statistics if available
    if (m_executeStage)
    {
        stream << Qt::endl;
        stream << m_executeStage->generateHybridReport();
    }

    // Add exception information
    QMutexLocker locker(&m_exceptionMutex);
    if (!m_recentExceptions.isEmpty())
    {
        stream << Qt::endl;
        stream << QString("Recent Exceptions (%1):").arg(m_recentExceptions.size()) << Qt::endl;

        for (int i = qMax(0, m_recentExceptions.size() - 5); i < m_recentExceptions.size(); ++i)
        {
            const auto &ex = m_recentExceptions[i];
            stream << QString("  Vector 0x%1 at PC 0x%2: %3")
                          .arg(ex.vector, 0, 16)
                          .arg(ex.faultingPC, 0, 16)
                          .arg(ex.description)
                   << Qt::endl;
        }
    }

    return report;
}

void alphaPipelineController::enableDynamicTuning(bool enable)
{
    m_dynamicTuningEnabled.store(enable);

    if (enable && m_state.load() == PipelineState::Running)
    {
        m_tuningTimer->start();
    }
    else
    {
        m_tuningTimer->stop();
    }

    DEBUG_LOG("Dynamic tuning %s", enable ? "enabled" : "disabled");
}

void alphaPipelineController::applyTuningRecommendations()
{
    if (!m_monitor)
    {
        return;
    }

    auto recommendations = m_monitor->getTuningRecommendations();

    for (const auto &rec : recommendations)
    {
        DEBUG_LOG("Applying tuning recommendation for %s: queue size %d, reason: %s", qPrintable(rec.stageName),
                  rec.recommendedQueueSize, qPrintable(rec.reason));

        setMaxInFlight(rec.stageName, rec.recommendedQueueSize);
    }
}

void alphaPipelineController::setMaxInFlight(const QString &stageName, int maxInFlight)
{
    if (stageName.toLower() == "fetch" && m_fetchStage)
    {
        m_fetchStage->adjustMaxInFlight(maxInFlight);
    }
    else if (stageName.toLower() == "decode" && m_decodeStage)
    {
        m_decodeStage->adjustMaxInFlight(maxInFlight);
    }
    else if (stageName.toLower() == "execute" && m_executeStage)
    {
        m_executeStage->adjustMaxInFlight(maxInFlight);
    }
    else if (stageName.toLower() == "writeback" && m_writebackStage)
    {
        m_writebackStage->adjustMaxInFlight(maxInFlight);
    }
    else
    {
        DEBUG_LOG("WARNING: Unknown stage name for tuning: %s", qPrintable(stageName));
    }
}

void alphaPipelineController::setJitEnabled(bool enabled)
{
    if (m_jitCompiler && m_executeStage)
    {
        m_executeStage->enableJitCompilation(enabled);
        DEBUG_LOG("JIT compilation %s", enabled ? "enabled" : "disabled");
    }
}

void alphaPipelineController::setJitHotThreshold(int threshold)
{
    if (m_jitCompiler)
    {
        m_jitCompiler->setHotThreshold(threshold);
        DEBUG_LOG("JIT hot threshold set to %d", threshold);
    }
}

// Slot implementations
void alphaPipelineController::onInstructionCommitted(uint64_t pc)
{
    m_instructionsExecuted.fetch_add(1, std::memory_order_relaxed);
    m_currentPC.store(pc, std::memory_order_relaxed);
}

void alphaPipelineController::onBranchResolved(uint64_t pc, bool taken, uint64_t target)
{
    handleBranch(pc, taken, target);
}

void alphaPipelineController::onExceptionRaised(uint32_t vector, uint64_t pc) { handleException(vector, pc); }

void alphaPipelineController::onStageStalled(const QString &stageName)
{
    DEBUG_LOG("Stage stalled: %s", qPrintable(stageName));

    if (m_monitor)
    {
        m_monitor->recordStall(stageName, 1);
    }
}

void alphaPipelineController::onBackpressureTriggered(const QString &stageName)
{
    DEBUG_LOG("Backpressure triggered in stage: %s", qPrintable(stageName));

    if (m_monitor)
    {
        m_monitor->recordBackpressure(stageName);
    }
}

void alphaPipelineController::updatePerformanceMetrics()
{
    m_cyclesElapsed.fetch_add(1, std::memory_order_relaxed);

    recordPerformanceMetrics();

    auto perf = getCurrentPerformance();
    emit sigPerformanceUpdate(perf);

    // Check for bottlenecks
    analyzeBottlenecks();
}

void alphaPipelineController::logPerformanceStats()
{
    auto perf = getCurrentPerformance();

    DEBUG_LOG("=== Pipeline Performance ===");
    DEBUG_LOG("Instructions/sec: %.2f", perf.instructionsPerSecond);
    DEBUG_LOG("Average IPC: %.3f", perf.averageIPC);
    DEBUG_LOG("Efficiency: %.1f%%", perf.pipelineEfficiency * 100.0);
    DEBUG_LOG("Total instructions: %llu", perf.totalInstructions);
    DEBUG_LOG("Total cycles: %llu", perf.totalCycles);
    DEBUG_LOG("Stall cycles: %llu", perf.stallCycles);

    if (!perf.bottleneckStage.isEmpty())
    {
        DEBUG_LOG("Bottleneck: %s", qPrintable(perf.bottleneckStage));
    }
}

// Private implementation methods continue...
// For brevity, showing key methods and structure

void alphaPipelineController::createPipelineStages()
{
    DEBUG_LOG("Creating pipeline stages");

    m_fetchStage = new alphaFetchStage(this);
    m_decodeStage = new alphaDecodeStage(this);
    m_executeStage = new alphaHybridExecuteStage(this);
    m_writebackStage = new alphaWritebackStage(this);

    // Create support components
    m_monitor = new alphaPipelineMonitor(this);
    m_stats = new alphaPipelineStats(this);
    m_jitCompiler = new alphaJitCompiler(this);

    DEBUG_LOG("Pipeline stages created");
}

void alphaPipelineController::connectPipelineStages()
{
    DEBUG_LOG("Connecting pipeline stages");

    // Connect stage data flow
    connect(m_fetchStage, &alphaFetchStage::sigOutputReady, m_decodeStage, &alphaDecodeStage::submit,
            Qt::QueuedConnection);

    connect(m_decodeStage, &alphaDecodeStage::sigOutputReady, m_executeStage, &alphaHybridExecuteStage::submit,
            Qt::QueuedConnection);

    connect(m_executeStage, &alphaHybridExecuteStage::sigOutputReady, m_writebackStage, &alphaWritebackStage::submit,
            Qt::QueuedConnection);

    // Attach JIT compiler to execute stage
    m_executeStage->attachJitCompiler(m_jitCompiler);

    DEBUG_LOG("Pipeline stages connected");
}

void alphaPipelineController::transitionToState(PipelineState newState)
{
    PipelineState oldState = m_state.exchange(newState);

    if (oldState != newState)
    {
        handleStateTransition(oldState, newState);
        emit sigStateChanged(stateToString(newState));
    }
}

QString alphaPipelineController::stateToString(PipelineState state) const
{
    switch (state)
    {
    case PipelineState::Stopped:
        return "STOPPED";
    case PipelineState::Starting:
        return "STARTING";
    case PipelineState::Running:
        return "RUNNING";
    case PipelineState::Stopping:
        return "STOPPING";
    case PipelineState::Flushing:
        return "FLUSHING";
    case PipelineState::Exception:
        return "EXCEPTION";
    default:
        return "UNKNOWN";
    }
}

double alphaPipelineController::calculatePipelineEfficiency() const
{
    uint64_t totalInstructions = m_instructionsExecuted.load();
    uint64_t totalCycles = m_cyclesElapsed.load();

    if (totalCycles == 0)
        return 0.0;

    // Ideal pipeline would achieve close to 1 IPC after initial fill
    uint64_t idealCycles = totalInstructions + 3; // 4-stage pipeline startup

    return qMin(1.0, static_cast<double>(idealCycles) / totalCycles);
}

void alphaPipelineController::recordException(uint32_t vector, uint64_t pc, const QString &description)
{
    QMutexLocker locker(&m_exceptionMutex);

    ExceptionInfo exception;
    exception.vector = vector;
    exception.faultingPC = pc;
    exception.description = description;
    exception.timestamp = QDateTime::currentDateTime();

    m_recentExceptions.append(exception);

    // Keep only recent exceptions (last 100)
    while (m_recentExceptions.size() > 100)
    {
        m_recentExceptions.removeFirst();
    }

    DEBUG_LOG("Exception recorded: vector=0x%x, PC=0x%llx, desc=%s", vector, pc, qPrintable(description));
}

void alphaPipelineController::handleStateTransition(PipelineState oldState, PipelineState newState)
{
    DEBUG_LOG("Pipeline state transition: %s -> %s", qPrintable(stateToString(oldState)),
              qPrintable(stateToString(newState)));

    switch (newState)
    {
    case PipelineState::Starting:
        DEBUG_LOG("Pipeline starting...");
        break;

    case PipelineState::Running:
        DEBUG_LOG("Pipeline now running");
        break;

    case PipelineState::Stopping:
        DEBUG_LOG("Pipeline stopping...");
        break;

    case PipelineState::Stopped:
        DEBUG_LOG("Pipeline stopped");
        break;

    case PipelineState::Flushing:
        DEBUG_LOG("Pipeline flushing...");
        initiatePipelineFlush();
        break;

    case PipelineState::Exception:
        DEBUG_LOG("Pipeline in exception state");
        break;
    }
}

void alphaPipelineController::initializeMonitoring()
{
    DEBUG_LOG("Initializing pipeline monitoring");

    if (!m_monitor)
        return;

    // Register stages with monitor
    QVector<QPair<QString, basePipelineStage *>> stages = {{"fetch", m_fetchStage},
                                                           {"decode", m_decodeStage},
                                                           {"execute", m_executeStage},
                                                           {"writeback", m_writebackStage}};

    for (const auto &pair : stages)
    {
        if (pair.second)
        {
            // m_monitor->registerStage(pair.first, pair.second->getQueue());
            DEBUG_LOG("Registered stage '%s' with monitor", qPrintable(pair.first));
        }
    }

    // Connect monitoring signals
    connect(m_monitor, &alphaPipelineMonitor::sigBottleneckDetected, this,
            &alphaPipelineController::sigBottleneckDetected, Qt::QueuedConnection);
}

void alphaPipelineController::setupPerformanceTracking()
{
    DEBUG_LOG("Setting up performance tracking");

    // Initialize performance tracking
    m_performanceTimer.start();

    // Connect to stage statistics updates
    if (m_stats)
    {
        connect(m_stats, &alphaPipelineStats::sigStatsUpdated, this, &alphaPipelineController::updatePerformanceMetrics,
                Qt::QueuedConnection);
    }
}

void alphaPipelineController::analyzeBottlenecks()
{
    if (!m_monitor)
        return;

    QString bottleneck = m_monitor->getBottleneckStage();
    if (!bottleneck.isEmpty())
    {
        emit sigBottleneckDetected(bottleneck);

        DEBUG_LOG("Bottleneck detected in stage: %s", qPrintable(bottleneck));

        // Log queue depths for analysis
        QVector<QPair<QString, basePipelineStage *>> stages = {{"fetch", m_fetchStage},
                                                               {"decode", m_decodeStage},
                                                               {"execute", m_executeStage},
                                                               {"writeback", m_writebackStage}};

        for (const auto &pair : stages)
        {
            if (pair.second)
            {
                DEBUG_LOG("%s queue depth: %zu", qPrintable(pair.first), pair.second->currentQueueDepth());
            }
        }
    }
}

void alphaPipelineController::updateIPC()
{
    uint64_t currentInstructions = m_instructionsExecuted.load();
    uint64_t currentCycles = m_cyclesElapsed.load();

    uint64_t deltaInstructions = currentInstructions - m_lastInstructionCount.load();
    uint64_t deltaCycles = currentCycles - m_lastCycleCount.load();

    if (deltaCycles > 0)
    {
        double instantIPC = static_cast<double>(deltaInstructions) / deltaCycles;
        if (m_stats)
        {
            m_stats->setGauge("instant_ipc", static_cast<int64_t>(instantIPC * 1000));
        }
    }

    m_lastInstructionCount.store(currentInstructions);
    m_lastCycleCount.store(currentCycles);
}

void alphaPipelineController::recordPerformanceMetrics()
{
    updateIPC();

    if (m_stats)
    {
        m_stats->setGauge("total_instructions", static_cast<int64_t>(m_instructionsExecuted.load()));
        m_stats->setGauge("total_cycles", static_cast<int64_t>(m_cyclesElapsed.load()));
        m_stats->setGauge("current_pc", static_cast<int64_t>(m_currentPC.load()));
        m_stats->setGauge("pipeline_efficiency", static_cast<int64_t>(calculatePipelineEfficiency() * 1000));
    }
}

void alphaPipelineController::adjustStageParameters()
{
    auto perf = getCurrentPerformance();

    if (perf.averageIPC < 0.5 && !perf.bottleneckStage.isEmpty())
    {
        // Low IPC suggests bottleneck - increase queue size for bottleneck stage
        int newSize = 1536; // 50% increase from default 1024

        DEBUG_LOG("Low IPC detected (%.3f), increasing %s queue size to %d", perf.averageIPC,
                  qPrintable(perf.bottleneckStage), newSize);
        setMaxInFlight(perf.bottleneckStage, newSize);
    }
}

void alphaPipelineController::balancePipelineLoad()
{
    QVector<QPair<QString, size_t>> queueDepths;

    if (m_fetchStage)
        queueDepths.append({"fetch", m_fetchStage->currentQueueDepth()});
    if (m_decodeStage)
        queueDepths.append({"decode", m_decodeStage->currentQueueDepth()});
    if (m_executeStage)
        queueDepths.append({"execute", m_executeStage->currentQueueDepth()});
    if (m_writebackStage)
        queueDepths.append({"writeback", m_writebackStage->currentQueueDepth()});

    // Find stage with highest queue depth
    auto maxIt = std::max_element(queueDepths.begin(), queueDepths.end(),
                                  [](const auto &a, const auto &b) { return a.second < b.second; });

    if (maxIt != queueDepths.end() && maxIt->second > 512)
    {
        DEBUG_LOG("High queue depth detected in %s (%zu), attempting load balancing", qPrintable(maxIt->first),
                  maxIt->second);

        // Could implement load balancing strategies here
        adjustStageParameters();
    }
}

void alphaPipelineController::optimizeQueueSizes()
{
    static QHash<QString, QVector<double>> utilizationHistory;

    QStringList stageNames = {"fetch", "decode", "execute", "writeback"};
    QVector<basePipelineStage *> stages = {m_fetchStage, m_decodeStage, m_executeStage, m_writebackStage};

    for (int i = 0; i < stageNames.size() && i < stages.size(); ++i)
    {
        if (!stages[i])
            continue;

        QString stageName = stageNames[i];
        size_t currentDepth = stages[i]->currentQueueDepth();
        size_t maxCapacity = 1024; // Default capacity

        double utilization = static_cast<double>(currentDepth) / maxCapacity;
        utilizationHistory[stageName].append(utilization);

        // Keep only recent history (60 samples = 1 minute at 1Hz)
        if (utilizationHistory[stageName].size() > 60)
        {
            utilizationHistory[stageName].removeFirst();
        }

        // Calculate average utilization and adjust if needed
        if (utilizationHistory[stageName].size() >= 10)
        {
            double avgUtilization = 0.0;
            for (double util : utilizationHistory[stageName])
            {
                avgUtilization += util;
            }
            avgUtilization /= utilizationHistory[stageName].size();

            if (avgUtilization > 0.8)
            {
                // High utilization - increase queue size
                int newSize = static_cast<int>(maxCapacity * 1.2);
                setMaxInFlight(stageName, newSize);
                DEBUG_LOG("Increased %s queue size to %d (avg util: %.2f)", qPrintable(stageName), newSize,
                          avgUtilization);
            }
            else if (avgUtilization < 0.3)
            {
                // Low utilization - decrease queue size to save memory
                int newSize = qMax(256, static_cast<int>(maxCapacity * 0.8));
                setMaxInFlight(stageName, newSize);
                DEBUG_LOG("Decreased %s queue size to %d (avg util: %.2f)", qPrintable(stageName), newSize,
                          avgUtilization);
            }
        }
    }
}

void alphaPipelineController::performPeriodicTuning()
{
    if (!m_dynamicTuningEnabled.load())
    {
        return;
    }

    DEBUG_LOG("Performing periodic pipeline tuning");

    adjustStageParameters();
    balancePipelineLoad();
    optimizeQueueSizes();

    // Apply monitor recommendations
    applyTuningRecommendations();

    // Clean up old exceptions
    clearOldExceptions();
}

void alphaPipelineController::handleFlushCompletion()
{
    int pending = m_pendingFlushes.fetch_sub(1, std::memory_order_acq_rel);

    if (pending <= 1)
    {
        completePipelineFlush();
    }
}

void alphaPipelineController::initiatePipelineFlush()
{
    DEBUG_LOG("Initiating pipeline flush");

    // Signal all stages to flush
    if (m_fetchStage)
        m_fetchStage->flushPipeline();
    if (m_decodeStage)
    {
        // Decode stage would need flush implementation
    }
    if (m_executeStage)
    {
        // Execute stage would need flush implementation
    }
    if (m_writebackStage)
    {
        // Writeback stage would need flush implementation
    }

    emit sigPipelineFlushed();
}

void alphaPipelineController::completePipelineFlush()
{
    m_flushRequested.store(false, std::memory_order_release);

    // Return to previous state if we were flushing
    if (m_state.load() == PipelineState::Flushing)
    {
        transitionToState(PipelineState::Running);
    }

    DEBUG_LOG("Pipeline flush completed");
}

void alphaPipelineController::handlePipelineException(uint32_t vector, uint64_t pc)
{
    QString description;
    switch (vector)
    {
    case 0x01:
        description = "Arithmetic Exception";
        break;
    case 0x02:
        description = "Memory Management Fault";
        break;
    case 0x03:
        description = "Privilege Violation";
        break;
    case 0x04:
        description = "Illegal Instruction";
        break;
    case 0x05:
        description = "Interrupt";
        break;
    default:
        description = QString("Unknown Exception (0x%1)").arg(vector, 0, 16);
        break;
    }

    recordException(vector, pc, description);

    // Attempt recovery
    recoverFromException();
}

void alphaPipelineController::recoverFromException()
{
    DEBUG_LOG("Attempting pipeline recovery from exception");

    // Flush pipeline to clear any corrupted state
    flushPipeline();

    // Reset performance counters if exceptions are frequent
    QMutexLocker locker(&m_exceptionMutex);
    if (m_recentExceptions.size() > 10)
    {
        DEBUG_LOG("WARNING: Frequent exceptions detected, resetting performance counters");
        m_instructionsExecuted.store(0);
        m_cyclesElapsed.store(0);
        m_performanceTimer.restart();
    }

    // Transition back to running state
    if (m_state.load() == PipelineState::Exception)
    {
        transitionToState(PipelineState::Running);
    }
}

void alphaPipelineController::clearOldExceptions()
{
    QMutexLocker locker(&m_exceptionMutex);

    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-300); // 5 minutes ago

    auto it = std::remove_if(m_recentExceptions.begin(), m_recentExceptions.end(),
                             [cutoff](const ExceptionInfo &ex) { return ex.timestamp < cutoff; });

    if (it != m_recentExceptions.end())
    {
        int removed = m_recentExceptions.end() - it;
        m_recentExceptions.erase(it, m_recentExceptions.end());
        DEBUG_LOG("Cleared %d old exceptions", removed);
    }
}

void alphaPipelineController::validatePipelineConfiguration() const
{
    if (!m_fetchStage)
    {
        throw std::runtime_error("Fetch stage not created");
    }
    if (!m_decodeStage)
    {
        throw std::runtime_error("Decode stage not created");
    }
    if (!m_executeStage)
    {
        throw std::runtime_error("Execute stage not created");
    }
    if (!m_writebackStage)
    {
        throw std::runtime_error("Writeback stage not created");
    }
    if (!m_monitor)
    {
        throw std::runtime_error("Pipeline monitor not created");
    }
    if (!m_stats)
    {
        throw std::runtime_error("Pipeline stats not created");
    }
    if (!m_jitCompiler)
    {
        throw std::runtime_error("JIT compiler not created");
    }

    DEBUG_LOG("Pipeline configuration validated");
}

void alphaPipelineController::cleanupResources()
{
    DEBUG_LOG("Cleaning up pipeline resources");

    // Stop timers
    if (m_metricsTimer)
    {
        m_metricsTimer->stop();
    }
    if (m_tuningTimer)
    {
        m_tuningTimer->stop();
    }

    // Clear tracking data
    m_instructionsExecuted.store(0);
    m_cyclesElapsed.store(0);
    m_currentPC.store(0);

    // Clear exception history
    QMutexLocker locker(&m_exceptionMutex);
    m_recentExceptions.clear();
}

void alphaPipelineController::validatePipelineState() const
{
    PipelineState currentState = m_state.load();

    if (currentState == PipelineState::Running)
    {
        if (!m_fetchStage || !m_decodeStage || !m_executeStage || !m_writebackStage)
        {
            DEBUG_LOG("ERROR: Pipeline running but stages not properly initialized");
        }
        if (!m_registerFile)
        {
            DEBUG_LOG("WARNING: Pipeline running without register file attached");
        }
        if (!m_memorySystem)
        {
            DEBUG_LOG("WARNING: Pipeline running without memory system attached");
        }
    }
}

bool alphaPipelineController::canTransitionTo(PipelineState newState) const
{
    PipelineState currentState = m_state.load();

    // Define valid state transitions
    switch (currentState)
    {
    case PipelineState::Stopped:
        return newState == PipelineState::Starting;

    case PipelineState::Starting:
        return newState == PipelineState::Running || newState == PipelineState::Stopped;

    case PipelineState::Running:
        return newState == PipelineState::Stopping || newState == PipelineState::Flushing ||
               newState == PipelineState::Exception;

    case PipelineState::Stopping:
        return newState == PipelineState::Stopped;

    case PipelineState::Flushing:
        return newState == PipelineState::Running || newState == PipelineState::Exception;

    case PipelineState::Exception:
        return newState == PipelineState::Stopped || newState == PipelineState::Flushing ||
               newState == PipelineState::Running;
    }

    return false;
}

bool alphaPipelineController::shouldFlushForBranch(uint64_t pc, uint64_t target) const
{
    // Simple heuristic: flush if branch target is not sequential
    return (target != pc + 4);
}