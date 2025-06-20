#pragma once
#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>

class AlphaCPU;
class AlphaMemorySystem;

class FetchUnit : public QObject
{
    Q_OBJECT

    AlphaCPU *m_cpu;
    AlphaMemorySystem *m_memorySystem;
  public:


    void attachAlphaCPU(AlphaCPU *cpu_) { m_cpu = cpu_; }
    void attachAlphaMemorySystem(AlphaMemorySystem *memSys) {m_memorySystem = memSys; }
    void clearStatistics();
    void enablePrefetch(bool enable) { m_prefetchEnabled = enable; }
    /**
     * @brief Constructor
     * @param cpu Reference to CPU
     */
    explicit FetchUnit(QObject *parent = nullptr);
   
	void flushInstructionCache();

	void invalidateCacheEntry(quint64 address);
    

    /**
     * @brief Fetch instruction at PC
     * @param pc Program counter
     * @return Fetched instruction or 0 if failed
     */
    quint32 fetchInstruction(quint64 pc);
    double getCacheHitRate() const;

    /**
     * @brief Check if unit is running
     */
    bool isRunning() { return m_running; }

    void pause();
    void prefetchNextInstructions(quint64 currentPC);
    // Statistics
    void printStatistics() const;
    void setPrefetchDepth(quint32 depth) { m_prefetchDepth = depth; }
    void reset();
    void resume();
    /**
     * @brief Start fetching instructions
     */
    void start();

    /**
     * @brief Stop fetching instructions
     */
    void stop();
    void updateStatistics(bool cacheHit, bool tlbMiss, bool fault);



  signals:
    void fetchUnitStopped();
    void fetchUnitStarted();
    void fetchUnitPaused();
    void fetchUnitResumed();
    void instructionFetched(quint64 pc, quint32 instruction);
    void fetchError(quint64 pc, QString reason);
    void tlbMiss(quint64 pc);

  private slots:
    void performPrefetch();

private:

    bool m_running = false;
    bool m_paused = false;
    // Prefetch support
    bool m_prefetchEnabled = true;
    quint32 m_prefetchDepth = 4;
    QQueue<quint64> m_prefetchQueue;

    // Statistics
    mutable QMutex m_statsMutex;
    quint64 m_totalFetches = 0;
    quint64 m_cacheHits = 0;
    quint64 m_tlbMisses = 0;
    quint64 m_faultCount = 0;
};
