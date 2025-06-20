#pragma once

/*#include "pipeline_Instruction.h"*/
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QSharedPointer>
#include <QVector>
#include <QWaitCondition>
#include <atomic>
#include <QHash>
#include <QElapsedTimer>
#include <QObject>
#include <atomic>
#include <chrono>
#include <QMutex>
#include <QObject>
#include <QSemaphore>
#include <QThread>
#include <atomic>
#include "globalmacro.h"
#include "utilitySafeIncrement.h"
#include "pipeline_alphainstructions.h"

// Forward declarations
class alphaInstructionQueue;
class AlphaMemorySystem;
class AlphaPerformanceCounters;
class AlphaRegisterFile;

using InstrPtr = QSharedPointer<alphaInstructionBase>;

class basePipelineStage : public QObject
{
    Q_OBJECT

  public:
    explicit basePipelineStage(const QString &name, int maxInFlight = 1024, QObject *parent = nullptr);
    virtual ~basePipelineStage();

    void initialize();
    void initialize_SignalsAndSlots();

    void moveToWorkerThread();
    void shutdown();

    // Performance monitoring
    struct Stats
    {
        std::atomic<uint64_t> instructionsProcessed{0};
        std::atomic<uint64_t> totalCycles{0};
        std::atomic<uint64_t> stallCycles{0};
        std::atomic<uint64_t> queueDepth{0};
        std::atomic<uint64_t> backpressureEvents{0};
    };

    const Stats &getStats() const { return m_stats; }
    void resetStats();

    // Queue depth monitoring for dynamic tuning
    size_t currentQueueDepth() const;
    void adjustMaxInFlight(int newMax);

    // Stage identification
    const QString &getStageName() const { return m_name; }
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

  signals:
    void sigOutputReady(InstrPtr instr);
    void sigStageStalled(const QString &stageName);
    void sigBackpressureTriggered(const QString &stageName);
    void sigStageStarted(const QString &stageName);
    void sigStageStopped(const QString &stageName);

  public slots:
    bool submit(InstrPtr instr);

  protected:
    // Pure virtual - must be implemented by derived classes
    virtual void process(InstrPtr instr) = 0;

    // Optional overrides for stage lifecycle
    virtual void onStageStart() {}
    virtual void onStageShutdown() {}
    virtual void onStageInitialize() {}

    // Utility methods for derived classes
    void recordProcessingTime(uint64_t cycles);
    void incrementStallCounter();
    void incrementBackpressureCounter();

    // Access to stage infrastructure
    QString getStageName() { return m_name; }
    QElapsedTimer &getStageTimer() { return m_stageTimer; }

  private slots:
    void updateStats();
    void performanceMonitoring();

  private:
    // Core stage infrastructure
    QString m_name;
    QThread *m_thread{nullptr};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shutdownRequested{false};

    // Queue management
    QSemaphore m_gate;
    alphaInstructionQueue *m_inQueue;

    // Performance monitoring
    Stats m_stats;
    QElapsedTimer m_stageTimer;
    QTimer *m_statsTimer{nullptr};
    QTimer *m_performanceTimer{nullptr};

    // Dynamic tuning
    std::atomic<int> m_maxInFlight;
    std::atomic<uint64_t> m_lastStatsUpdate{0};

    // Thread safety
    mutable QMutex m_statsMutex;

    // Stage execution
    void execLoop();
    void initializeStageInfrastructure();
    void cleanupStageInfrastructure();

    // Performance tracking
    void updateQueueDepthStats();
    void checkForStalls();
    void emitPerformanceSignals();

    // Dynamic optimization
    void adaptQueueSize();
    void monitorBackpressure();
};

// Fetch Stage - reads instructions from memory
class alphaFetchStage : public basePipelineStage
{
    Q_OBJECT

  public:
    explicit alphaFetchStage(QObject *parent = nullptr);
    ~alphaFetchStage();

    void initialize() override;
    void initialize_SignalsAndSlots() override;

    // Fetch control interface
    void setProgramCounter(uint64_t pc) { m_nextPC.store(pc, std::memory_order_release); }
    void setBranchTarget(uint64_t target) { m_branchTarget.store(target, std::memory_order_release); }
    void flushPipeline() { m_flushRequested.store(true, std::memory_order_release); }

    // Memory system integration
    void attachMemorySystem(AlphaMemorySystem *memSys) { m_memorySystem = memSys; }

    // Cache management
    void invalidateICache();
    void invalidateICacheLine(uint64_t pc);

    // Statistics
    struct FetchStats
    {
        std::atomic<uint64_t> instructionsFetched{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> cacheMisses{0};
        std::atomic<uint64_t> branchRedirects{0};
        std::atomic<uint64_t> pipelineFlushes{0};
    };

    const FetchStats &getFetchStats() const { return m_fetchStats; }

  signals:
    void sigICacheMiss(uint64_t pc);
    void sigBranchRedirect(uint64_t oldPC, uint64_t newPC);
    void sigPipelineFlush();

  protected:
    void process(InstrPtr instr) override;
    void onStageStart() override;
    void onStageInitialize() override;
    void onStageShutdown() override;

  private:
    // Fetch control
    std::atomic<uint64_t> m_nextPC{0};
    std::atomic<uint64_t> m_branchTarget{0};
    std::atomic<bool> m_flushRequested{false};
    std::atomic<bool> m_branchPending{false};

    // Memory system interface
    AlphaMemorySystem *m_memorySystem{nullptr};

    // Instruction cache simulation
    struct ICacheEntry
    {
        uint64_t tag;
        uint32_t data[16]; // 64-byte cache line (16 instructions)
        bool valid;
        uint64_t accessTime;
    };

    static constexpr size_t ICACHE_SIZE = 512;
    static constexpr size_t CACHE_LINE_SIZE = 64;       // bytes
    static constexpr size_t INSTRUCTIONS_PER_LINE = 16; // 4 bytes per instruction

    std::array<ICacheEntry, ICACHE_SIZE> m_icache;
    mutable QMutex m_cacheMutex; // Protect cache for statistics/management

    // Fetch statistics
    FetchStats m_fetchStats;

    // Prefetch and prediction
    std::atomic<uint64_t> m_lastFetchedPC{0};
    std::atomic<bool> m_sequentialFetch{true};

    // Helper methods
    uint32_t fetchInstruction(uint64_t pc);
    InstrPtr createInstructionObject(uint32_t rawBits, uint64_t pc);

    // Cache management
    uint64_t getCacheIndex(uint64_t pc) const;
    uint64_t getCacheTag(uint64_t pc) const;
    uint64_t getOffsetInLine(uint64_t pc) const;
    bool lookupICache(uint64_t pc, uint32_t &instruction);
    void updateICache(uint64_t pc, uint32_t instruction);
    void fillCacheLine(uint64_t pc);

    // Fetch control logic
    uint64_t getNextFetchPC();
    void handleBranchRedirect();
    void handlePipelineFlush();
    bool shouldFetchInstruction();

    // Performance optimization
    void performPrefetch(uint64_t pc);
    void updateFetchPrediction(uint64_t pc);
};

// Decode Stage - decodes instruction fields and determines type
class alphaDecodeStage : public basePipelineStage
{
    Q_OBJECT

  public:
    explicit alphaDecodeStage(QObject *parent = nullptr);
    ~alphaDecodeStage();

    void initialize() override;
    void initialize_SignalsAndSlots() override;

    // Decode statistics
    struct DecodeStats
    {
        std::atomic<uint64_t> instructionsDecoded{0};
        std::atomic<uint64_t> memoryInstructions{0};
        std::atomic<uint64_t> operateInstructions{0};
        std::atomic<uint64_t> branchInstructions{0};
        std::atomic<uint64_t> jumpInstructions{0};
        std::atomic<uint64_t> floatInstructions{0};
        std::atomic<uint64_t> miscInstructions{0};
        std::atomic<uint64_t> invalidInstructions{0};
    };

    const DecodeStats &getDecodeStats() const { return m_decodeStats; }

  signals:
    void sigInvalidInstruction(uint64_t pc, uint32_t rawBits);
    void sigComplexInstructionDecoded(uint64_t pc, const QString &instrType);

  protected:
    void process(InstrPtr instr) override;
    void onStageStart() override;
    void onStageInitialize() override;
    void onStageShutdown() override;

  private:
    enum class InstructionFormat
    {
        Memory,  // LDA, STQ, etc.
        Operate, // ADDQ, SUBQ, etc.
        Branch,  // BR, BEQ, etc.
        Jump,    // JMP, JSR, etc.
        FloatOp, // ADDT, MULT, etc.
        Misc,    // CALL_PAL, etc.
        Invalid  // Unrecognized format
    };

    DecodeStats m_decodeStats;

    // Decode methods
    InstructionFormat determineFormat(uint32_t opcode);
    void decodeMemoryFormat(InstrPtr instr);
    void decodeOperateFormat(InstrPtr instr);
    void decodeBranchFormat(InstrPtr instr);
    void decodeJumpFormat(InstrPtr instr);
    void decodeFloatFormat(InstrPtr instr);
    void decodeMiscFormat(InstrPtr instr);

    // Alpha instruction field extraction
    uint32_t extractOpcode(uint32_t rawBits) const;
    uint32_t extractRa(uint32_t rawBits) const;
    uint32_t extractRb(uint32_t rawBits) const;
    uint32_t extractRc(uint32_t rawBits) const;
    uint32_t extractFunction(uint32_t rawBits) const;
    uint32_t extractLiteral(uint32_t rawBits) const;
    int32_t extractDisplacement(uint32_t rawBits) const;
    int32_t extractBranchDisplacement(uint32_t rawBits) const;
    bool isLiteralMode(uint32_t rawBits) const;

    // Instruction validation
    bool validateInstruction(uint32_t rawBits, uint32_t opcode) const;
    bool isReservedOpcode(uint32_t opcode) const;
    bool isPrivilegedInstruction(uint32_t rawBits) const;

    // Helper methods
    void populateCommonFields(InstrPtr instr);
    void handleInvalidInstruction(InstrPtr instr);
    QString getInstructionMnemonic(uint32_t rawBits) const;
    void updateDecodeStatistics(InstructionFormat format);
};

// Execute Stage - performs the actual computation
class alphaExecuteStage : public basePipelineStage
{
    Q_OBJECT

  public:
    explicit alphaExecuteStage(QObject *parent = nullptr);
    ~alphaExecuteStage();

    void initialize() override;
    void initialize_SignalsAndSlots() override;

    // Resource attachment
    void attachRegisterFile(AlphaRegisterFile *regFile) { m_registerFile = regFile; }
    void attachMemorySystem(AlphaMemorySystem *memSys) { m_memorySystem = memSys; }

    // Execution statistics
    struct ExecuteStats
    {
        std::atomic<uint64_t> instructionsExecuted{0};
        std::atomic<uint64_t> trivialInstructions{0};
        std::atomic<uint64_t> moderateInstructions{0};
        std::atomic<uint64_t> heavyInstructions{0};
        std::atomic<uint64_t> inlineExecutions{0};
        std::atomic<uint64_t> asyncExecutions{0};
        std::atomic<uint64_t> executionExceptions{0};
        std::atomic<uint64_t> totalExecutionCycles{0};
    };

    const ExecuteStats &getExecuteStats() const { return m_executeStats; }
    int getPendingHeavyOps() const { return m_pendingHeavyOps.load(std::memory_order_acquire); }

  signals:
    void sigExecutionException(uint64_t pc, const QString &error);
    void sigHeavyOperationStarted(uint64_t pc, const QString &operation);
    void sigHeavyOperationCompleted(uint64_t pc, uint64_t cycles);

  protected:
    void process(InstrPtr instr) override;
    void onStageStart() override;
    void onStageInitialize() override;
    void onStageShutdown() override;

  private:
    enum class ExecutionCost
    {
        Trivial = 1,  // Simple ALU ops
        Moderate = 5, // FP ops, shifts
        Heavy = 50    // Division, square root
    };

    // Resource interfaces
    AlphaRegisterFile *m_registerFile{nullptr};
    AlphaMemorySystem *m_memorySystem{nullptr};

    // Execution infrastructure
    QThreadPool *m_heavyOpPool{nullptr};
    std::atomic<int> m_pendingHeavyOps{0};
    QMutex m_executionMutex;

    // Execution statistics
    ExecuteStats m_executeStats;

    // Execution control
    ExecutionCost classifyInstruction(InstrPtr instr);
    bool executeInline(InstrPtr instr);
    void executeAsync(InstrPtr instr);

    // Execution units
    void executeInteger(QSharedPointer<alphaIntegerInstruction> instr);
    void executeFloatingPoint(QSharedPointer<alphaFloatingPointInstruction> instr);
    void executeMemory(QSharedPointer<alphaMemoryInstruction> instr);
    void executeBranch(QSharedPointer<alphaBranchInstruction> instr);

    // Integer ALU operations
    bool executeIntegerArithmetic(QSharedPointer<alphaIntegerInstruction> instr);
    bool executeIntegerLogical(QSharedPointer<alphaIntegerInstruction> instr);
    bool executeIntegerShift(QSharedPointer<alphaIntegerInstruction> instr);
    bool executeIntegerMultiply(QSharedPointer<alphaIntegerInstruction> instr);

    // Floating-point operations
    bool executeFloatArithmetic(QSharedPointer<alphaFloatingPointInstruction> instr);
    bool executeFloatComparison(QSharedPointer<alphaFloatingPointInstruction> instr);
    bool executeFloatConversion(QSharedPointer<alphaFloatingPointInstruction> instr);

    // Memory operations
    bool executeLoad(QSharedPointer<alphaMemoryInstruction> instr);
    bool executeStore(QSharedPointer<alphaMemoryInstruction> instr);
    uint64_t calculateEffectiveAddress(QSharedPointer<alphaMemoryInstruction> instr);

    // Branch operations
    bool evaluateBranchCondition(QSharedPointer<alphaBranchInstruction> instr);
    void updateBranchPrediction(QSharedPointer<alphaBranchInstruction> instr, bool taken);

    // Utility methods
    void handleExecutionException(InstrPtr instr, const QString &error);
    void updateExecutionStatistics(ExecutionCost cost, bool isAsync);
    int64_t readRegister(uint32_t reg);
    void writeRegister(uint32_t reg, int64_t value);
    double readFloatRegister(uint32_t reg);
    void writeFloatRegister(uint32_t reg, double value);

    // Heavy operation task wrapper
    class HeavyOperationTask;
};
// Writeback Stage - commits results and handles pipeline control
class alphaWritebackStage : public basePipelineStage
{
    Q_OBJECT

  public:
    explicit alphaWritebackStage(QObject *parent = nullptr);
    ~alphaWritebackStage();

    void initialize() override;
    void initialize_SignalsAndSlots() override;

    // Resource attachment
    void attachRegisterFile(AlphaRegisterFile *regFile) { m_registerFile = regFile; }
    void attachPerformanceCounters(AlphaPerformanceCounters *perfCounters) { m_performanceCounters = perfCounters; }

    // Writeback statistics
    struct WritebackStats
    {
        std::atomic<uint64_t> instructionsCommitted{0};
        std::atomic<uint64_t> branchesTaken{0};
        std::atomic<uint64_t> branchesNotTaken{0};
        std::atomic<uint64_t> exceptionsRaised{0};
        std::atomic<uint64_t> registerWrites{0};
        std::atomic<uint64_t> floatRegisterWrites{0};
        std::atomic<uint64_t> retiredInstructions{0};
        std::atomic<uint64_t> commitStalls{0};
    };

    const WritebackStats &getWritebackStats() const { return m_writebackStats; }

  signals:
    void sigInstructionCommitted(uint64_t pc);
    void sigBranchResolved(uint64_t pc, bool taken, uint64_t target);
    void sigExceptionRaised(uint32_t vector, uint64_t pc);
    void sigPerformanceEvent(const QString &eventType, uint64_t pc, uint64_t value);
    void sigRetirementComplete(uint64_t pc, uint64_t cycles);

  protected:
    void process(InstrPtr instr) override;
    void onStageStart() override;
    void onStageInitialize() override;
    void onStageShutdown() override;

  private:
    // Resource interfaces
    AlphaRegisterFile *m_registerFile{nullptr};
    AlphaPerformanceCounters *m_performanceCounters{nullptr};

    // Writeback statistics
    WritebackStats m_writebackStats;

    // Exception handling
    struct ExceptionInfo
    {
        uint32_t vector;
        uint64_t pc;
        QString description;
        bool handled;
    };

    QVector<ExceptionInfo> m_pendingExceptions;
    QMutex m_exceptionMutex;

    // Performance tracking
    std::atomic<uint64_t> m_totalCommitCycles{0};
    std::atomic<uint64_t> m_lastCommittedPC{0};

    // Commit methods
    void commitInstruction(InstrPtr instr);
    void handleBranch(InstrPtr instr);
    void handleException(InstrPtr instr);
    void updatePerformanceCounters(InstrPtr instr);

    // Register file updates
    void writeRegister(uint32_t reg, int64_t value);
    void writeFloatRegister(uint32_t reg, double value);

    // Commit validation
    bool validateCommit(InstrPtr instr);
    bool canCommitInstruction(InstrPtr instr);
    void handleCommitFailure(InstrPtr instr, const QString &reason);

    // Branch handling
    void processBranchInstruction(QSharedPointer<alphaBranchInstruction> instr);
    void updateBranchStatistics(bool taken);
    void notifyBranchPredictor(uint64_t pc, bool taken, uint64_t target);

    // Exception processing
    void processException(InstrPtr instr);
    void raiseException(uint32_t vector, uint64_t pc, const QString &description);
    void handleArithmeticException(InstrPtr instr);
    void handleMemoryException(InstrPtr instr);
    void handlePrivilegeException(InstrPtr instr);

    // Performance monitoring
    void recordInstructionRetirement(InstrPtr instr);
    void updateInstructionMix(InstrPtr instr);
    void recordExecutionMetrics(InstrPtr instr);

    // Utility methods
    QString getInstructionTypeName(InstrPtr instr) const;
    uint32_t getExceptionVector(InstrPtr instr) const;
    bool isPrivilegedInstruction(InstrPtr instr) const;
    void logCommitEvent(InstrPtr instr, const QString &event) const;
};

class alphaHybridExecuteStage : public alphaExecuteStage
{
    Q_OBJECT

  public:
    explicit alphaHybridExecuteStage(QObject *parent = nullptr);
    ~alphaHybridExecuteStage();

    void initialize() override;
    void initialize_SignalsAndSlots() override;

    // JIT integration
    void attachJitCompiler(alphaJitCompiler *jitCompiler) { m_jitCompiler = jitCompiler; }
    void enableJitCompilation(bool enable) { m_jitEnabled = enable; }
    void setProfilingThreshold(int threshold) { m_profilingThreshold = threshold; }
    void setCompilationThreshold(int threshold) { m_compilationThreshold = threshold; }

    // Hybrid execution statistics
    struct HybridStats
    {
        std::atomic<uint64_t> interpretedCount{0};
        std::atomic<uint64_t> compiledCount{0};
        std::atomic<uint64_t> profiledCount{0};
        std::atomic<uint64_t> jitHits{0};
        std::atomic<uint64_t> jitMisses{0};
        std::atomic<uint64_t> modeTransitions{0};
        std::atomic<uint64_t> compilationTriggers{0};
    };

    const HybridStats &getHybridStats() const { return m_hybridStats; }
    double getJitHitRate() const;
    QString generateHybridReport() const;

  signals:
    void sigExecutionModeChanged(uint64_t pc, const QString &mode);
    void sigJitCompilationTriggered(uint64_t pc);
    void sigPerformanceImprovement(uint64_t pc, double speedup);

  protected:
    void process(InstrPtr instr) override;
    void onStageStart() override;
    void onStageInitialize() override;
    void onStageShutdown() override;

  private:
    // JIT integration
    alphaJitCompiler *m_jitCompiler{nullptr};
    bool m_jitEnabled{true};

    // Execution mode selection
    enum class ExecutionMode
    {
        Interpret, // Cold path - normal interpretation
        Profile,   // Warm path - interpretation with profiling
        Compiled   // Hot path - execute compiled code
    };

    // Hybrid execution statistics
    HybridStats m_hybridStats;

    // Execution thresholds
    int m_profilingThreshold{10};    // Start profiling after N executions
    int m_compilationThreshold{100}; // Trigger compilation after N executions

    // Performance tracking
    QHash<uint64_t, int> m_executionCounts;           // PC -> execution count
    QHash<uint64_t, QElapsedTimer> m_executionTimers; // PC -> timing data
    QHash<uint64_t, ExecutionMode> m_currentModes;    // PC -> current execution mode
    QMutex m_trackingMutex;

    // Execution mode methods
    ExecutionMode selectExecutionMode(InstrPtr instr);
    void executeInterpreted(InstrPtr instr);
    void executeWithProfiling(InstrPtr instr);
    bool tryExecuteCompiled(InstrPtr instr);

    // Mode transition management
    void transitionExecutionMode(InstrPtr instr, ExecutionMode newMode);
    bool shouldProfileInstruction(InstrPtr instr);
    bool shouldCompileInstruction(InstrPtr instr);

    // Performance measurement
    void recordExecutionPerformance(InstrPtr instr, ExecutionMode mode, uint64_t executionTime);
    void updateExecutionCount(uint64_t pc);
    void measurePerformanceImprovement(InstrPtr instr, uint64_t interpretedTime, uint64_t compiledTime);

    // JIT integration helpers
    void triggerJitCompilation(InstrPtr instr);
    void recordJitHit(InstrPtr instr);
    void recordJitMiss(InstrPtr instr);

    // Utility methods
    ExecutionMode getCurrentMode(uint64_t pc) const;
    void setCurrentMode(uint64_t pc, ExecutionMode mode);
    int getExecutionCount(uint64_t pc) const;
    QString executionModeToString(ExecutionMode mode) const;
};

class alphaPipelineController : public QObject
{
    Q_OBJECT

  public:
    explicit alphaPipelineController(QObject *parent = nullptr);
    ~alphaPipelineController();

    void initialize();
    void initialize_SignalsAndSlots();

    // Lifecycle management
    void start();
    void stop();
    void shutdown();

    // Resource attachment
    void attachRegisterFile(AlphaRegisterFile *regFile);
    void attachMemorySystem(AlphaMemorySystem *memSys);

    // Execution control
    void setProgramCounter(uint64_t pc);
    void handleBranch(uint64_t pc, bool taken, uint64_t target);
    void flushPipeline();
    void handleException(uint32_t vector, uint64_t faultingPC);

    // Performance monitoring
    struct PipelinePerformance
    {
        double instructionsPerSecond;
        double averageIPC;
        double pipelineEfficiency;
        uint64_t totalInstructions;
        uint64_t totalCycles;
        uint64_t stallCycles;
        QString bottleneckStage;
    };

    PipelinePerformance getCurrentPerformance() const;
    QString generatePerformanceReport() const;

    // Dynamic tuning
    void enableDynamicTuning(bool enable);
    void applyTuningRecommendations();

    // Configuration
    void setMaxInFlight(const QString &stageName, int maxInFlight);
    void setJitEnabled(bool enabled);
    void setJitHotThreshold(int threshold);

  public slots:
    void onInstructionCommitted(uint64_t pc);
    void onBranchResolved(uint64_t pc, bool taken, uint64_t target);
    void onExceptionRaised(uint32_t vector, uint64_t pc);
    void onStageStalled(const QString &stageName);
    void onBackpressureTriggered(const QString &stageName);

    // Performance monitoring slots
    void updatePerformanceMetrics();
    void logPerformanceStats();

  signals:
    void sigPipelineStarted();
    void sigPipelineStopped();
    void sigPerformanceUpdate(const PipelinePerformance &perf);
    void sigBottleneckDetected(const QString &stageName);
    void sigExceptionOccurred(uint32_t vector, uint64_t pc);
    void sigStateChanged(const QString &newState);
    void sigPipelineFlushed();

  private:
    // Pipeline stages
    alphaFetchStage *m_fetchStage{nullptr};
    alphaDecodeStage *m_decodeStage{nullptr};
    alphaHybridExecuteStage *m_executeStage{nullptr};
    alphaWritebackStage *m_writebackStage{nullptr};

    // Support components
    alphaPipelineMonitor *m_monitor{nullptr};
    alphaPipelineStats *m_stats{nullptr};
    alphaJitCompiler *m_jitCompiler{nullptr};

    // Resource interfaces
    AlphaRegisterFile *m_registerFile{nullptr};
    AlphaMemorySystem *m_memorySystem{nullptr};

    // State management
    enum class PipelineState
    {
        Stopped,
        Starting,
        Running,
        Stopping,
        Flushing,
        Exception
    };

    std::atomic<PipelineState> m_state{PipelineState::Stopped};
    std::atomic<uint64_t> m_currentPC{0};
    std::atomic<bool> m_dynamicTuningEnabled{true};

    // Performance tracking
    QElapsedTimer m_performanceTimer;
    QTimer *m_metricsTimer{nullptr};
    QTimer *m_tuningTimer{nullptr};
    std::atomic<uint64_t> m_instructionsExecuted{0};
    std::atomic<uint64_t> m_cyclesElapsed{0};
    std::atomic<uint64_t> m_lastInstructionCount{0};
    std::atomic<uint64_t> m_lastCycleCount{0};

    // Exception handling
    struct ExceptionInfo
    {
        uint32_t vector;
        uint64_t faultingPC;
        QString description;
        QDateTime timestamp;
    };

    QVector<ExceptionInfo> m_recentExceptions;
    mutable QMutex m_exceptionMutex;

    // Pipeline control
    std::atomic<bool> m_flushRequested{false};
    std::atomic<int> m_pendingFlushes{0};

    // Initialization helpers
    void createPipelineStages();
    void connectPipelineStages();
    void initializeMonitoring();
    void setupPerformanceTracking();

    // State management helpers
    void transitionToState(PipelineState newState);
    bool canTransitionTo(PipelineState newState) const;
    bool shouldFlushForBranch(uint64_t pc, uint64_t target) const;
    void handleStateTransition(PipelineState oldState, PipelineState newState);
    QString stateToString(PipelineState state) const;

    // Performance analysis
    void analyzeBottlenecks();
    void updateIPC();
    double calculatePipelineEfficiency() const;
    void recordPerformanceMetrics();

    // Dynamic tuning implementation
    void adjustStageParameters();
    void balancePipelineLoad();
    void optimizeQueueSizes();
    void performPeriodicTuning();

    // Exception handling helpers
    void recordException(uint32_t vector, uint64_t pc, const QString &description);
    void handlePipelineException(uint32_t vector, uint64_t pc);
    void recoverFromException();
    void clearOldExceptions();

    // Pipeline control helpers
    void initiatePipelineFlush();
    void completePipelineFlush();
    void validatePipelineConfiguration() const;

    // Resource management
    void cleanupResources();
    void validatePipelineState() const;

  private slots:
    void performPeriodicTuning();
    void handleFlushCompletion();
};