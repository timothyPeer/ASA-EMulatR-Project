// JitCompiler.h
#pragma once

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QRunnable>
#include <QSharedPointer>
#include <QThreadPool>
#include <QVector>
#include <functional>

// Forward declarations
class AlphaRegisterFile;
class AlphaMemorySystem;


// Compiled block represents native code
// alphaCompiledBlock.h
#pragma once

#include "globalmacro.h"
#include "utilitySafeIncrement.h"
#include <atomic>
#include <cstring>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Forward declarations
class AlphaRegisterFile;
class AlphaMemorySystem;

class alphaCompiledBlock
{
  public:
    using HostFunction = std::function<void(AlphaRegisterFile &, AlphaMemorySystem &)>;

    explicit alphaCompiledBlock(HostFunction func, size_t instructionCount);
    ~alphaCompiledBlock();

    // Hot path execution
    void execute(AlphaRegisterFile &regs, AlphaMemorySystem &mem);

    // Getters - lock-free access
    size_t getInstructionCount() const { return m_instructionCount; }
    uint64_t getExecutionCount() const { return m_executionCount.load(std::memory_order_relaxed); }

    // Profiling - minimal overhead
    void recordExecution(uint64_t cycles);
    double getAverageExecutionTime() const;

    // Native code management
    bool allocateNativeCodeBuffer(size_t size);
    void freeNativeCodeBuffer();
    bool writeNativeCode(const void *code, size_t size);
    bool hasNativeCode() const { return m_nativeCode != nullptr; }

    // Performance queries
    bool isHot(uint64_t threshold) const;

  private:
    // Core execution data
    HostFunction m_hostFunction;
    size_t m_instructionCount;

    // Lock-free performance counters
    std::atomic<uint64_t> m_executionCount{0};
    std::atomic<uint64_t> m_totalCycles{0};

    // Native code buffer
    void *m_nativeCode{nullptr};
    size_t m_codeSize{0};

    // Hot path helpers - inlined
    inline void executeNativeCode(AlphaRegisterFile &regs, AlphaMemorySystem &mem);
    inline void executeInterpretedCode(AlphaRegisterFile &regs, AlphaMemorySystem &mem);
};

// Translation cache manages compiled blocks
class alphaTranslationCache : public QObject
{
    Q_OBJECT

  public:
    explicit alphaTranslationCache(size_t maxBlocks = 1024, QObject *parent = nullptr);
    ~alphaTranslationCache();

    void initialize();
    void initialize_SignalsAndSlots();

    // Cache operations
    QSharedPointer<alphaCompiledBlock> lookup(uint64_t pc);
    void insert(uint64_t pc, QSharedPointer<alphaCompiledBlock> block);
    void invalidate(uint64_t pc);
    void invalidateRange(uint64_t startPC, uint64_t endPC);
    void clear();

    // Cache statistics
    struct CacheStats
    {
        std::atomic<uint64_t> hits{0};
        std::atomic<uint64_t> misses{0};
        std::atomic<uint64_t> evictions{0};
        std::atomic<uint64_t> invalidations{0};
    };

    const CacheStats &getStats() const { return m_stats; }
    double getHitRate() const;

    // Cache management
    void setMaxBlocks(size_t maxBlocks);
    size_t getCurrentSize() const;

  signals:
    void sigBlockEvicted(uint64_t pc);
    void sigCacheInvalidated();

  private:
    struct CacheEntry
    {
        QSharedPointer<alphaCompiledBlock> block;
        uint64_t lastAccessTime;
        uint64_t accessCount;
    };

    QHash<uint64_t, CacheEntry> m_cache;
    mutable QReadWriteLock m_cacheLock;
    size_t m_maxBlocks;
    CacheStats m_stats;
    QElapsedTimer m_timer;

    void evictOldest();
    void evictLRU();
    uint64_t getCurrentTimestamp() const;
    void updateAccessTime(CacheEntry &entry);
};

// Basic block represents a sequence of instructions for compilation
class alphaBasicBlock
{
  public:
    explicit alphaBasicBlock(uint64_t startPC);

    void addInstruction(uint32_t rawBits, uint64_t pc);
    void setEndPC(uint64_t endPC) { m_endPC = endPC; }

    uint64_t getStartPC() const { return m_startPC; }
    uint64_t getEndPC() const { return m_endPC; }
    const QVector<uint32_t> &getInstructions() const { return m_instructions; }
    const QVector<uint64_t> &getInstructionPCs() const { return m_instructionPCs; }

    size_t getInstructionCount() const { return static_cast<size_t>(m_instructions.size()); }
    bool isEmpty() const { return m_instructions.isEmpty(); }

    // Block analysis
    bool hasBranches() const;
    bool hasMemoryAccesses() const;
    bool hasFloatingPoint() const;
    int getComplexityScore() const;

  private:
    uint64_t m_startPC;
    uint64_t m_endPC;
    QVector<uint32_t> m_instructions;
    QVector<uint64_t> m_instructionPCs;

    // Alpha instruction analysis helpers
    bool isBranchInstruction(uint32_t rawBits) const;
    bool isMemoryInstruction(uint32_t rawBits) const;
    bool isFloatingPointInstruction(uint32_t rawBits) const;
    uint32_t extractOpcode(uint32_t rawBits) const;
    uint32_t extractFunction(uint32_t rawBits) const;
};

// JIT compilation task
class alphaJitCompileTask : public QRunnable
{
  public:
    explicit alphaJitCompileTask(const alphaBasicBlock &block, alphaTranslationCache *cache, QObject *parent = nullptr);

    void run() override;

  private:
    alphaBasicBlock m_block;
    alphaTranslationCache *m_cache;

    // Compilation methods
    alphaCompiledBlock::HostFunction compileToHost(const alphaBasicBlock &block);
    alphaCompiledBlock::HostFunction compileInterpreted(const alphaBasicBlock &block);
    alphaCompiledBlock::HostFunction compileOptimized(const alphaBasicBlock &block);

    // Instruction compilation helpers
    void compileIntegerOp(uint32_t rawBits, QVector<QString> &code);
    void compileMemoryOp(uint32_t rawBits, QVector<QString> &code);
    void compileFloatOp(uint32_t rawBits, QVector<QString> &code);
    void compileBranchOp(uint32_t rawBits, QVector<QString> &code);

    // Code generation
    QString generateCppCode(const alphaBasicBlock &block);
    alphaCompiledBlock::HostFunction compileCppToFunction(const QString &code);

    // Alpha instruction decoding helpers
    uint32_t extractOpcode(uint32_t rawBits) const;
    uint32_t extractRa(uint32_t rawBits) const;
    uint32_t extractRb(uint32_t rawBits) const;
    uint32_t extractRc(uint32_t rawBits) const;
    uint32_t extractFunction(uint32_t rawBits) const;
    uint32_t extractLiteral(uint32_t rawBits) const;
    int32_t extractDisplacement(uint32_t rawBits) const;

    // Code generation utilities
    QString formatRegisterAccess(uint32_t reg, bool isFloat = false) const;
    QString formatMemoryAccess(const QString &address, int size) const;
    QString formatConditionCheck(uint32_t opcode, const QString &regValue) const;
};

// Profile-guided optimization
class alphaBlockProfiler : public QObject
{
    Q_OBJECT

  public:
    explicit alphaBlockProfiler(QObject *parent = nullptr);
    ~alphaBlockProfiler();

    void initialize();
    void initialize_SignalsAndSlots();

    // Profile recording - hot path operations
    void recordExecution(uint64_t pc);
    void recordBranch(uint64_t pc, bool taken, uint64_t target);
    void recordMemoryAccess(uint64_t pc, uint64_t address, bool isLoad);

    // Hot block detection
    bool isHotBlock(uint64_t pc, int threshold = 1000) const;
    QVector<uint64_t> getHotBlocks(int threshold = 1000) const;

    // Block boundary detection
    alphaBasicBlock identifyBasicBlock(uint64_t startPC) const;
    QVector<alphaBasicBlock> identifyHotBlocks() const;

    // Profiling data access
    int getExecutionCount(uint64_t pc) const;
    double getBranchProbability(uint64_t pc) const;

    void reset();

    // Statistics and reporting
    QString generateReport() const;
    void setMemorySystem(AlphaMemorySystem *memSys) { m_memorySystem = memSys; }

  signals:
    void sigHotBlockDetected(uint64_t pc, int executionCount);

  private:
    struct ProfileData
    {
        std::atomic<int> executionCount{0};
        std::atomic<int> branchCount{0};
        std::atomic<int> branchTaken{0};
        std::atomic<int> memoryAccesses{0};
        uint64_t lastSeen{0};
    };

    QHash<uint64_t, ProfileData> m_profiles;
    mutable QReadWriteLock m_profileLock;
    QElapsedTimer m_timer;
    AlphaMemorySystem *m_memorySystem;

    // Hot block detection tracking
    QVector<uint64_t> m_lastHotBlocks;
    int m_defaultThreshold{1000};

    // Block boundary analysis
    bool isBranchInstruction(uint32_t rawBits) const;
    bool isJumpInstruction(uint32_t rawBits) const;
    bool isReturnInstruction(uint32_t rawBits) const;
    bool isCallInstruction(uint32_t rawBits) const;

    // Helper methods
    uint32_t fetchInstruction(uint64_t pc) const;
    uint64_t getCurrentTimestamp() const;
    void checkForNewHotBlocks();
    alphaBasicBlock traceBasicBlock(uint64_t startPC) const;

    // Instruction analysis
    uint32_t extractOpcode(uint32_t rawBits) const;
    uint32_t extractFunction(uint32_t rawBits) const;
    bool isBlockTerminator(uint32_t rawBits) const;
};

// JIT compiler coordinator
class alphaJitCompiler : public QObject
{
    Q_OBJECT

  public:
    explicit alphaJitCompiler(QObject *parent = nullptr);
    ~alphaJitCompiler();

    void initialize();
    void initialize_SignalsAndSlots();
    void shutdown();

    // Execution interface - hot path
    bool tryExecuteCompiled(uint64_t pc, AlphaRegisterFile &regs, AlphaMemorySystem &mem);
    void recordExecution(uint64_t pc, uint32_t rawBits);

    // Configuration
    void setHotThreshold(int threshold) { m_hotThreshold = threshold; }
    void setMaxCompiledBlocks(size_t max);
    void setOptimizationLevel(int level) { m_optimizationLevel = level; }

    // Statistics and monitoring
    struct JitStats
    {
        std::atomic<uint64_t> interpretedInstructions{0};
        std::atomic<uint64_t> compiledInstructions{0};
        std::atomic<uint64_t> compilationTime{0};
        std::atomic<uint64_t> compiledBlocks{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> cacheMisses{0};
    };

    const JitStats &getStats() const { return m_stats; }
    QString generateReport() const;

    // Dynamic optimization
    void enableAdaptiveOptimization(bool enable) { m_adaptiveOptimization = enable; }
    void tuneThresholds();

  public slots:
    void onHotBlockDetected(uint64_t pc, int executionCount);

  signals:
    void sigCompilationStarted(uint64_t pc);
    void sigCompilationCompleted(uint64_t pc, bool success);
    void sigOptimizationApplied(uint64_t pc, const QString &optimization);

  private:
    alphaTranslationCache *m_translationCache;
    alphaBlockProfiler *m_profiler;
    QThreadPool *m_compilerPool;

    JitStats m_stats;
    int m_hotThreshold{1000};
    int m_optimizationLevel{2};
    bool m_adaptiveOptimization{true};

    // Adaptive thresholds
    std::atomic<int> m_dynamicHotThreshold{1000};
    std::atomic<double> m_compilationSuccessRate{1.0};

    // Performance monitoring
    QTimer *m_tuningTimer;
    QElapsedTimer m_compilationTimer;

    // Compilation tracking
    QHash<uint64_t, QElapsedTimer> m_activeCompilations;
    QMutex m_compilationMutex;

    // Statistics tracking
    std::atomic<uint64_t> m_totalCompilationAttempts{0};
    std::atomic<uint64_t> m_successfulCompilations{0};

    void adjustThresholds();
    void scheduleCompilation(const alphaBasicBlock &block);
    void updateCompilationStats(uint64_t pc, bool success, qint64 compilationTimeMs);
    bool shouldCompileBlock(uint64_t pc, int executionCount) const;
    void optimizeExistingBlocks();

  private slots:
    void performPeriodicTuning();
    void onCompilationTaskFinished();
};