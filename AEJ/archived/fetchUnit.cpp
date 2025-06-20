#include "fetchUnit.h"
#include "GlobalMacro.h"
#include "InstructionPipeLine.h"
#include "AlphaCPU_refactored.h"


void FetchUnit::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);

    m_totalFetches = 0;
    m_cacheHits = 0;
    m_tlbMisses = 0;
    m_faultCount = 0;

    DEBUG_LOG("FetchUnit: Statistics cleared");
}
FetchUnit::FetchUnit(QObject *parent) : QObject(parent)
{
    DEBUG_LOG("FetchUnit initialized");
    emit fetchUnitStarted();
}
// quint32 FetchUnit::fetchInstruction(quint64 pc)
// {
//     m_totalFetches++;
//     bool cacheHit = false;
//     bool tlbMiss = false;
//     bool fault = false;
//     quint32 instruction = 0;
// 
//     // Align the PC to instruction boundary (4-byte alignment for Alpha)
//     quint64 alignedPC = pc & ~0x3ULL;
// 
//     // First, check the instruction cache
//     InstructionCache *iCache = m_cpu->getInstructionCache();
//     if (iCache && iCache->isHit(alignedPC))
//     {
//         // Cache hit - get instruction directly from cache
//         cacheHit = true;
//         instruction = iCache->read(alignedPC);
//         DEBUG_LOG(QString("FetchUnit: Cache hit for PC=0x%1").arg(alignedPC, 16, 16, QChar('0')));
//     }
//     else
//     {
//         // Cache miss - need to access memory
//         // First translate virtual address to physical address
//         quint64 physicalAddress = 0;
// 
//         // Define translation struct to track translation status
//         struct Translation
//         {
//             bool valid = false;
//             bool fault = false;
//             QString faultReason;
//             quint64 physicalAddress = 0;
//         } translation;
// 
//         // Try to translate address using TLB
//         if (m_cpu->translate(alignedPC, physicalAddress, 2))
//         { // '2' indicates instruction fetch
//             // Translation successful
//             translation.valid = true;
//             translation.physicalAddress = physicalAddress;
// 
//             // Read from physical memory
//             bool readSuccess = m_cpu->readMemory32(physicalAddress, instruction);
//             if (!readSuccess)
//             {
//                 // Memory read failed
//                 translation.fault = true;
//                 translation.faultReason = "Memory read failed";
//                 instruction = 0;
//                 fault = true;
//             }
// 
//             // Update instruction cache with new entry
//             if (iCache && instruction != 0)
//             {
//                 iCache->insert(alignedPC, instruction);
//             }
//         }
//         else
//         {
//             // Translation failed - TLB miss or access violation
//             translation.valid = false;
//             translation.fault = true;
//             translation.faultReason = "Address translation failed";
//             instruction = 0;
// 
//             // Check if this is a TLB miss
//             if (!m_cpu->handleTLBMiss(alignedPC, m_cpu->readASN(), true))
//             {
//                 tlbMiss = true;
//                 emit tlbMiss(alignedPC);
//                 DEBUG_LOG(QString("FetchUnit: TLB miss for PC=0x%1").arg(alignedPC, 16, 16, QChar('0')));
//             }
//             else
//             {
//                 // Not a TLB miss, but another type of fault (e.g., protection violation)
//                 fault = true;
//                 emit fetchError(alignedPC, translation.faultReason);
//                 DEBUG_LOG(QString("FetchUnit: Fetch fault for PC=0x%1: %2")
//                               .arg(alignedPC, 16, 16, QChar('0'))
//                               .arg(translation.faultReason));
//             }
//         }
//     }
// 
//     // Update statistics
//     updateStatistics(cacheHit, tlbMiss, fault);
// 
//     // Perform prefetch if enabled
//     if (m_prefetchEnabled && instruction != 0)
//     {
//         prefetchNextInstructions(alignedPC);
//     }
// 
//     // Emit signal on successful fetch
//     if (instruction != 0)
//     {
//         emit instructionFetched(alignedPC, instruction);
//     }
// 
//     return instruction;
// }

quint32 FetchUnit::fetchInstruction(quint64 pc) {

    quint32 instruction;

    // Simple, clean interface - all complexity hidden in AlphaMemorySystem
    if (m_memorySystem->readVirtualMemory(m_cpu, pc, instruction, 4, pc))
    {
        emit instructionFetched(pc, instruction);
        return true;
    }
    else
    {
        emit fetchError(pc, "Translation or protection fault");
        return false;
    }

}
void FetchUnit::flushInstructionCache()
{
    InstructionCache *iCache = m_cpu->getInstructionCache();
    if (iCache)
    {
        iCache->invalidateAll();
        DEBUG_LOG("FetchUnit: Instruction cache flushed");
    }
}
double FetchUnit::getCacheHitRate() const
{
    QMutexLocker locker(&m_statsMutex);

    if (m_totalFetches == 0)
    {
        return 0.0;
    }

    return (double)m_cacheHits / m_totalFetches * 100.0;
}
void FetchUnit::invalidateCacheEntry(quint64 address)
{
    InstructionCache *iCache = m_cpu->getInstructionCache();
    if (iCache)
    {
        iCache->invalidate(address);
        DEBUG_LOG(QString("FetchUnit: Cache entry invalidated for address=0x%1").arg(address, 16, 16, QChar('0')));
    }
}
void FetchUnit::prefetchNextInstructions(quint64 currentPC)
{
    // Prefetch next few instructions sequentially
    for (quint32 i = 1; i <= m_prefetchDepth; i++)
    {
        quint64 nextPC = currentPC + (i * 4); // 4 bytes per instruction

        // Check if already in cache
        InstructionCache *iCache = m_cpu->getInstructionCache();
        if (iCache && iCache->isHit(nextPC))
            continue;

        // Add to prefetch queue (to be processed later)
        if (m_prefetchQueue.size() < m_prefetchDepth)
        {
            m_prefetchQueue.enqueue(nextPC);
        }
    }

    // Process one prefetch per fetch to avoid blocking
    performPrefetch();
}
void FetchUnit::updateStatistics(bool cacheHit, bool tlbMiss, bool fault)
{
    QMutexLocker locker(&m_statsMutex);

    if (cacheHit)
        m_cacheHits++;
    if (tlbMiss)
        m_tlbMisses++;
    if (fault)
        m_faultCount++;
}
void FetchUnit::pause()
{
    if (m_running)
    {
        m_paused = true;
        DEBUG_LOG("FetchUnit: Paused");
    }
}


void FetchUnit::printStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    double hitRate = m_totalFetches > 0 ? (double)m_cacheHits / m_totalFetches * 100.0 : 0.0;
    double missRate = m_totalFetches > 0 ? (double)m_tlbMisses / m_totalFetches * 100.0 : 0.0;
    double faultRate = m_totalFetches > 0 ? (double)m_faultCount / m_totalFetches * 100.0 : 0.0;

    DEBUG_LOG("FetchUnit Statistics:");
    DEBUG_LOG(QString("  Total Fetches: %1").arg(m_totalFetches));
    DEBUG_LOG(QString("  Cache Hit Rate: %1%").arg(hitRate, 0, 'f', 2));
    DEBUG_LOG(QString("  TLB Miss Rate: %1%").arg(missRate, 0, 'f', 2));
    DEBUG_LOG(QString("  Fault Rate: %1%").arg(faultRate, 0, 'f', 2));
}
void FetchUnit::performPrefetch()
{
    if (m_prefetchQueue.isEmpty())
        return;

    quint64 prefetchPC = m_prefetchQueue.dequeue();

    // Perform background fetch (without blocking)
    // This would typically be done asynchronously
    quint32 prefetchedInstruction = fetchInstruction(prefetchPC);

    if (prefetchedInstruction != 0)
    {
        DEBUG_LOG(QString("FetchUnit: Prefetched instruction at PC=0x%1").arg(prefetchPC, 16, 16, QChar('0')));
    }
}
void FetchUnit::reset()
{
    m_running = false;
    m_paused = false;
    m_prefetchQueue.clear();
    clearStatistics();
    DEBUG_LOG("FetchUnit: Reset");
}
void FetchUnit::resume()
{
    if (m_running && m_paused)
    {
        m_paused = false;
        DEBUG_LOG("FetchUnit: Resumed");
    }
}
void FetchUnit::start()
{
    if (!m_running)
    {
        m_running = true;
        // Could emit a signal or notify other components
        DEBUG_LOG("FetchUnit started");
    }
}
void FetchUnit::stop()
{
    if (m_running)
    {
        m_running = false;
        DEBUG_LOG("FetchUnit stopped");

        // Could emit a signal here if FetchUnit inherits from QObject
        emit fetchUnitStopped();
    }
}





