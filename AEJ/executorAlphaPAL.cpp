#include "executorAlphaPAL.h"
//#include "AlphaBarrierExecutor.h"
#include "AlphaCPU_refactored.h"
#include "executorAlphaFloatingPoint.h"
#include "opcode11_executorAlphaIntegerLogical.h"
#include "AlphaTranslationCache.h"
#include "DecodedInstruction.h"
#include "UnifiedDataCache.h"
#include "constants/const_OpCode_0_PAL.h"
#include "enumerations/enumPALFunctionClass.h"
#include "structures/structPALInstruction.h"
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QtConcurrent>
#include "IllegalInstructionException.h"
#include "PALFunctionConstants.h"

// Individual PAL Function Implementations

bool executorAlphaPAL::executeMTPR_FEN(const PALInstruction &instr)
{
    qDebug() << "PAL MTPR_FEN executed at PC:" << Qt::hex << instr.pc;

    // Write floating-point enable register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 fenValue = 0;

    if (!readIntegerRegisterWithCache(ra, fenValue))
    {
        return false;
    }

    // Enable/disable floating-point based on value
    if (m_fpExecutor)
    {
        bool enableFP = (fenValue & 1) != 0;
        if (enableFP)
        {
            qDebug() << "Floating-point enabled";
        }
        else
        {
            qDebug() << "Floating-point disabled";
        }
    }

    return writeIPRWithCache("FEN", fenValue);
}

bool executorAlphaPAL::executeAlphaSpecific(const PALInstruction &instr)
{
#if defined(ALPHA_BUILD)
    qDebug() << "PAL Alpha-specific function executed at PC:" << Qt::hex << instr.pc;

    switch (instr.function)
    {
    case FUNC_Alpha_LDQP_:
        return executeAlpha_LDQP(instr);
    case FUNC_Alpha_STQP_:
        return executeAlpha_STQP(instr);
    case FUNC_Alpha_MFPR_ASN_:
        return executeAlpha_MFPR_ASN(instr);
    case FUNC_Alpha_MTPR_ASTEN_:
        return executeAlpha_MTPR_ASTEN(instr);
    case FUNC_Alpha_MTPR_ASTSR_:
        return executeAlpha_MTPR_ASTSR(instr);
    case FUNC_Alpha_MFPR_MCES_:
        return executeAlpha_MFPR_MCES(instr);
    case FUNC_Alpha_MTPR_MCES_:
        return executeAlpha_MTPR_MCES(instr);
    case FUNC_Alpha_MFPR_PCBB_:
        return executeAlpha_MFPR_PCBB(instr);
    case FUNC_Alpha_MFPR_PRBR_:
        return executeAlpha_MFPR_PRBR(instr);
    case FUNC_Alpha_MTPR_PRBR_:
        return executeAlpha_MTPR_PRBR(instr);
    case FUNC_Alpha_MFPR_PTBR_:
        return executeAlpha_MFPR_PTBR(instr);
    case FUNC_Alpha_MTPR_SCBB_:
        return executeAlpha_MTPR_SCBB(instr);
    case FUNC_Alpha_MTPR_SIRR_:
        return executeAlpha_MTPR_SIRR(instr);
    case FUNC_Alpha_MFPR_SISR_:
        return executeAlpha_MFPR_SISR(instr);
    case FUNC_Alpha_MFPR_SSP_:
        return executeAlpha_MFPR_SSP(instr);
    case FUNC_Alpha_MTPR_SSP_:
        return executeAlpha_MTPR_SSP(instr);
    case FUNC_Alpha_MFPR_USP_:
        return executeAlpha_MFPR_USP(instr);
    case FUNC_Alpha_MTPR_USP_:
        return executeAlpha_MTPR_USP(instr);
    case FUNC_Alpha_MTPR_FEN_:
        return executeMTPR_FEN(instr);
    case FUNC_Alpha_MTPR_IPIR_:
        return executeAlpha_MTPR_IPIR(instr);
    case FUNC_Alpha_MFPR_IPL_:
        return executeAlpha_MFPR_IPL(instr);
    case FUNC_Alpha_MTPR_IPL_:
        return executeAlpha_MTPR_IPL(instr);
    case FUNC_Alpha_MFPR_TBCHK_:
        return executeAlpha_MFPR_TBCHK(instr);
    case FUNC_Alpha_MTPR_TBIAP_:
        return executeAlpha_MTPR_TBIAP(instr);
    case FUNC_Alpha_MFPR_ESP_:
        return executeAlpha_MFPR_ESP(instr);
    case FUNC_Alpha_MTPR_ESP_:
        return executeAlpha_MTPR_ESP(instr);
    case FUNC_Alpha_MTPR_PERFMON_:
        return executeAlpha_MTPR_PERFMON(instr);
    case FUNC_Alpha_MFPR_WHAMI_:
        return executeAlpha_MFPR_WHAMI(instr);
    case FUNC_Alpha_READ_UNQ_:
        return executeAlpha_READ_UNQ(instr);
    case FUNC_Alpha_WRITE_UNQ_:
        return executeAlpha_WRITE_UNQ(instr);
    case FUNC_Alpha_INITPAL_:
        return executeAlpha_INITPAL(instr);
    case FUNC_Alpha_WRENTRY_:
        return executeAlpha_WRENTRY(instr);
    case FUNC_Alpha_SWPIRQL_:
        return executeAlpha_SWPIRQL(instr);
    case FUNC_Alpha_RDIRQL_:
        return executeAlpha_RDIRQL(instr);
    case FUNC_Alpha_DI_:
        return executeAlpha_DI(instr);
    case FUNC_Alpha_EI_:
        return executeAlpha_EI(instr);
    case FUNC_Alpha_SWPPAL_:
        return executeAlpha_SWPPAL(instr);
    case FUNC_Alpha_SSIR_:
        return executeAlpha_SSIR(instr);
    case FUNC_Alpha_CSIR_:
        return executeAlpha_CSIR(instr);
    case FUNC_Alpha_RFE_:
        return executeAlpha_RFE(instr);
    case FUNC_Alpha_RETSYS_:
        return executeAlpha_RETSYS(instr);
    case FUNC_Alpha_RESTART_:
        return executeAlpha_RESTART(instr);
    case FUNC_Alpha_SWPPROCESS_:
        return executeAlpha_SWPPROCESS(instr);
    case FUNC_Alpha_RDMCES_:
        return executeAlpha_RDMCES(instr);
    case FUNC_Alpha_WRMCES_:
        return executeAlpha_WRMCES(instr);
    case FUNC_Alpha_TBIA_:
        return executeAlpha_TBIA(instr);
    case FUNC_Alpha_TBIS_:
        return executeAlpha_TBIS(instr);
    case FUNC_Alpha_TBISASN_:
        return executeAlpha_TBISASN(instr);
    case FUNC_Alpha_RDKSP_:
        return executeAlpha_RDKSP(instr);
    case FUNC_Alpha_SWPKSP_:
        return executeAlpha_SWPKSP(instr);
    case FUNC_Alpha_RDPSR_:
        return executeAlpha_RDPSR(instr);
    case FUNC_Alpha_REBOOT_:
        return executeAlpha_REBOOT(instr);
    case FUNC_Alpha_CHMK_:
        return executeAlpha_CHMK(instr);
    case FUNC_Alpha_CALLKD_:
        return executeAlpha_CALLKD(instr);
    case FUNC_Alpha_GENTRAP_:
        return executeAlpha_GENTRAP(instr);
    case FUNC_Alpha_KBPT_:
        return executeAlpha_KBPT(instr);
    default:
        qWarning() << "Unknown Alpha-specific PAL function:" << Qt::hex << instr.function;
        return false;
    }
#else
    Q_UNUSED(instr);
    qWarning() << "Alpha-specific PAL functions not compiled in";
    return false;
#endif
}

bool executorAlphaPAL::executeTru64Specific(const PALInstruction &instr)
{
#if defined(TRU64_BUILD)
    qDebug() << "PAL Tru64-specific function executed at PC:" << Qt::hex << instr.pc;

    switch (instr.function)
    {
    case FUNC_Tru64_REBOOT:
        return executeTru64_REBOOT(instr);
    case FUNC_Tru64_INITPAL:
        return executeTru64_INITPAL(instr);
    case FUNC_Tru64_SWPIRQL:
        return executeTru64_SWPIRQL(instr);
    case FUNC_Tru64_RDIRQL:
        return executeTru64_RDIRQL(instr);
    case FUNC_Tru64_DI:
        return executeTru64_DI(instr);
    case FUNC_Tru64_RDMCES:
        return executeTru64_RDMCES(instr);
    case FUNC_Tru64_WRMCES:
        return executeTru64_WRMCES(instr);
    case FUNC_Tru64_RDPCBB:
        return executeTru64_RDPCBB(instr);
    case FUNC_Tru64_WRPRBR:
        return executeTru64_WRPRBR(instr);
    case FUNC_Tru64_TBIA:
        return executeTru64_TBIA(instr);
    case FUNC_Tru64_THIS:
        return executeTru64_THIS(instr);
    case FUNC_Tru64_DTBIS:
        return executeTru64_DTBIS(instr);
    case FUNC_Tru64_TBISASN:
        return executeTru64_TBISASN(instr);
    case FUNC_Tru64_RDKSP:
        return executeTru64_RDKSP(instr);
    case FUNC_Tru64_SWPKSP:
        return executeTru64_SWPKSP(instr);
    case FUNC_Tru64_WRPERFMON:
        return executeTru64_WRPERFMON(instr);
    case FUNC_Tru64_SWPIPL:
        return executeTru64_SWPIPL(instr);
    case FUNC_Tru64_RDUSP:
        return executeTru64_RDUSP(instr);
    case FUNC_Tru64_WRUSP:
        return executeTru64_WRUSP(instr);
    case FUNC_Tru64_RDCOUNTERS:
        return executeTru64_RDCOUNTERS(instr);
    case FUNC_Tru64_CALLSYS:
        return executeTru64_CALLSYS(instr);
    case FUNC_Tru64_SSIR:
        return executeTru64_SSIR(instr);
    case FUNC_Tru64_WRIPIR:
        return executeTru64_WRIPIR(instr);
    case FUNC_Tru64_RFE:
        return executeTru64_RFE(instr);
    case FUNC_Tru64_RETSYS:
        return executeTru64_RETSYS(instr);
    case FUNC_Tru64_RDPSR:
        return executeTru64_RDPSR(instr);
    case FUNC_Tru64_RDPER:
        return executeTru64_RDPER(instr);
    case FUNC_Tru64_RDTHREAD:
        return executeTru64_RDTHREAD(instr);
    case FUNC_Tru64_SWPCTX:
        return executeTru64_SWPCTX(instr);
    case FUNC_Tru64_WRFEN:
        return executeTru64_WRFEN(instr);
    case FUNC_Tru64_RTI:
        return executeTru64_RTI(instr);
    case FUNC_Tru64_RDUNIQUE:
        return executeTru64_RDUNIQUE(instr);
    case FUNC_Tru64_WRUNIQUE:
        return executeTru64_WRUNIQUE(instr);
    default:
        qWarning() << "Unknown Tru64-specific PAL function:" << Qt::hex << instr.function;
        return false;
    }
#else
    Q_UNUSED(instr);
    qWarning() << "Tru64-specific PAL functions not compiled in";
    return false;
#endif
}

bool executorAlphaPAL::writeMemoryWithFaultHandling(quint64 address, quint64 value, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    // Check if this access would cause a fault BEFORE attempting it
    bool wouldFault = checkMemoryAccessWouldFault(address, true); // true = write access

    if (wouldFault)
    {
        // We know this will fault, so handle it directly
        return handleMemoryFault(address, true, instr);
    }

    // Try to write memory
    if (m_cpu->writeVirtualMemory(address, value))
    {
        return true;
    }

    // Write failed even though we didn't expect it to - handle fault
    return handleMemoryFault(address, true, instr);
}

executorAlphaPAL::~executorAlphaPAL() { stopAsyncPipeline(); }

bool executorAlphaPAL::readMemoryWithFaultHandling(quint64 address, quint64 &value, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    // Check if this access would cause a fault BEFORE attempting it
    bool wouldFault = checkMemoryAccessWouldFault(address, false); // false = read access

    if (wouldFault)
    {
        // We know this will fault, so handle it directly
        return handleMemoryFault(address, false, instr);
    }

    // Try to read memory
    quint8 buffer[8];
    if (m_cpu->readMemory(address, buffer, 8))
    {
        memcpy(&value, buffer, 8);
        return true;
    }

    // Read failed even though we didn't expect it to - handle fault
    return handleMemoryFault(address, false, instr);
}


bool executorAlphaPAL::readMemoryWithoutFault(quint64 address, quint64 &value)
{
    // This is a non-faulting memory read for internal use
    // Implementation depends on your memory system design

    if (!m_cpu || !m_cpu->getMemorySystem())
    {
        return false;
    }

    // Try to read without triggering exceptions
    // You might have a special method for this in your memory system
    auto memSys = m_cpu->getMemorySystem();

    // Option 1: If you have a non-faulting read method
    if (memSys->hasNonFaultingRead())
    {
        return memSys->readWithoutFault(address, value, 8);
    }

    // Option 2: Use physical address if we can translate
    quint64 physicalAddr = 0;
    if (memSys->translateAddressNonFaulting(address, physicalAddr))
    {
        return memSys->readPhysicalMemory(physicalAddr, value, 8);
    }

    // Option 3: Try regular read but catch exceptions
    try
    {
        quint8 buffer[8];
        if (m_cpu->readMemory(address, buffer, 8))
        {
            memcpy(&value, buffer, 8);
            return true;
        }
    }
    catch (...)
    {
        // Caught exception, return false
        return false;
    }

    return false;
}


void executorAlphaPAL::startAsyncPipeline()
{
    if (m_pipelineActive.exchange(true))
    {
        return; // Already running
    }

    // Clear pipeline state
    {
        QMutexLocker locker(&m_pipelineMutex);
        m_fetchQueue.clear();
        m_decodeQueue.clear();
        m_executeQueue.clear();
        m_writebackQueue.clear();
        m_sequenceCounter.store(0);
    }

    // Start worker threads with PAL-specific priorities
    m_fetchWorker = QtConcurrent::run([this]() { fetchWorker(); });
    m_decodeWorker = QtConcurrent::run([this]() { decodeWorker(); });
    m_executeWorker = QtConcurrent::run([this]() { executeWorker(); });
    m_writebackWorker = QtConcurrent::run([this]() { writebackWorker(); });

    qDebug() << "Alpha PAL async pipeline started";
}

void executorAlphaPAL::stopAsyncPipeline()
{
    if (!m_pipelineActive.exchange(false))
    {
        return; // Already stopped
    }

    // Wake up all workers
    m_pipelineCondition.wakeAll();

    // Wait for workers to complete
    m_fetchWorker.waitForFinished();
    m_decodeWorker.waitForFinished();
    m_executeWorker.waitForFinished();
    m_writebackWorker.waitForFinished();

    qDebug() << "Alpha PAL async pipeline stopped";
}

executorAlphaPAL::executorAlphaPAL(AlphaCPU *cpu, QObject *parent)
    : QObject(parent), m_cpu(cpu), m_barrierExecutor(nullptr), m_fpExecutor(nullptr), m_intExecutor(nullptr)
{
    qDebug() << "executorAlphaPAL: Initialized for OpCode 0 PAL instructions";

     m_instructionCache = icache;

    // Initialize JIT optimization tracking
    m_functionExecutionCount.clear();
    m_frequentFunctions.clear();
    m_criticalFunctions.clear();

    // Pre-populate critical functions that are always optimized
    m_criticalFunctions.insert(FUNC_Common_HALT_);
    m_criticalFunctions.insert(FUNC_Common_SWPCTX_);
    m_criticalFunctions.insert(FUNC_Common_REI_);
    m_criticalFunctions.insert(FUNC_Common_SWPIPL_);

    if (icache)
    {
        connect(icache.get(), &AlphaInstructionCache::cacheHit, this, &executorAlphaPAL::onInstructionCacheHit);
        connect(icache.get(), &AlphaInstructionCache::cacheMiss, this, &executorAlphaPAL::onInstructionCacheMiss);
        connect(icache.get(), &AlphaInstructionCache::lineInvalidated, this, &executorAlphaPAL::onCacheLineInvalidated);
        connect(icache.get(), &AlphaInstructionCache::coherencyEventHandled, this,
                &executorAlphaPAL::onCacheCoherencyEvent);
    }

}

bool executorAlphaPAL::submitInstruction(const DecodedInstruction &instruction, quint64 pc)
{
    if (!m_pipelineActive.load())
    {
        return false;
    }

    QMutexLocker locker(&m_pipelineMutex);

    if (m_fetchQueue.size() >= MAX_PIPELINE_DEPTH)
    {
        return false; // Pipeline full
    }

    quint64 seqNum = m_sequenceCounter.fetch_add(1);
    PALInstruction palInstr(instruction, pc, seqNum);

    // Analyze the PAL instruction for optimization
    analyzePALInstruction(palInstr);

    m_fetchQueue.enqueue(palInstr);
    m_pipelineCondition.wakeOne();

    return true;
}

bool executorAlphaPAL::executePALInstruction(const DecodedInstruction &instruction)
{
    PALInstruction instr(instruction, 0, 0);
    analyzePALInstruction(instr);

    // Check privilege level immediately for synchronous execution
    if (!checkPrivilegeLevel(instr))
    {
        m_privilegeViolations++;
        emit privilegeViolation(instr.function, instr.pc);
        return false;
    }

    return executeSystemCall(instr) || executeMemoryManagement(instr) || executePrivilegeOperation(instr) ||
           executePerformanceCounter(instr) || executeContextSwitch(instr);
}

// Pipeline Workers

void executorAlphaPAL::fetchWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_fetchQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 100);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_fetchQueue.isEmpty() && m_decodeQueue.size() < MAX_PIPELINE_DEPTH)
        {
            PALInstruction instr = m_fetchQueue.dequeue();

            // For frequent/critical functions, use optimized fetch
            if (isFrequentFunction(instr.function) || isCriticalFunction(instr.function))
            {
                // JIT-optimized path: instruction likely in L1 cache
                quint32 instruction;
                if (fetchInstructionWithCache(instr.pc, instruction))
                {
                    instr.isReady = true;
                    updateCacheStatistics("L1I", true);
                }
                else
                {
                    // Prefetch next likely instructions for critical functions
                    if (isCriticalFunction(instr.function))
                    {
                        preloadCriticalInstructions();
                    }
                    instr.isReady = false;
                }
            }
            else
            {
                // Standard fetch path
                quint32 instruction;
                instr.isReady = fetchInstructionWithCache(instr.pc, instruction);
            }

            if (instr.isReady)
            {
                m_decodeQueue.enqueue(instr);
                m_pipelineCondition.wakeOne();
            }
            else
            {
                // Cache miss - requeue
                m_fetchQueue.enqueue(instr);
            }
        }
    }
}

void executorAlphaPAL::decodeWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_decodeQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 50);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_decodeQueue.isEmpty() && m_executeQueue.size() < MAX_PIPELINE_DEPTH)
        {
            PALInstruction instr = m_decodeQueue.dequeue();

            // PAL decode is fast - mainly privilege checking and dependency analysis
            analyzeDependencies(instr);
            instr.isReady = checkPrivilegeLevel(instr);

            if (!instr.isReady)
            {
                instr.hasException = true;
                instr.exceptionType = 0x0080; // Privilege violation
                m_privilegeViolations++;
            }

            m_executeQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

void executorAlphaPAL::executeWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_executeQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 50);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_executeQueue.isEmpty())
        {
            PALInstruction instr = m_executeQueue.dequeue();

            // Check dependencies before execution
            if (!checkDependencies(instr))
            {
                m_executeQueue.enqueue(instr); // Requeue
                continue;
            }

            locker.unlock(); // Release lock during execution

            // Update JIT statistics
            updateJITStats(instr.function);

            // Execute based on function classification
            bool success = false;
            if (!instr.hasException)
            {
                PALFunctionClass classification = classifyPALFunction(instr.function);

                switch (classification)
                {
                case PALFunctionClass::SYSTEM_CALL:
                    success = executeSystemCall(instr);
                    break;
                case PALFunctionClass::MEMORY_MANAGEMENT:
                    success = executeMemoryManagement(instr);
                    break;
                case PALFunctionClass::PRIVILEGE_OPERATION:
                    success = executePrivilegeOperation(instr);
                    break;
                case PALFunctionClass::PERFORMANCE_COUNTER:
                    success = executePerformanceCounter(instr);
                    break;
                case PALFunctionClass::CONTEXT_SWITCH:
                    success = executeContextSwitch(instr);
                    break;
                default:
                    success = executePrivilegeOperation(instr); // Default fallback
                    break;
                }
            }

            locker.relock();
            instr.isCompleted = success;

            m_writebackQueue.enqueue(instr);
            m_pipelineCondition.wakeOne();
        }
    }
}

void executorAlphaPAL::writebackWorker()
{
    while (m_pipelineActive.load())
    {
        QMutexLocker locker(&m_pipelineMutex);

        while (m_writebackQueue.isEmpty() && m_pipelineActive.load())
        {
            m_pipelineCondition.wait(&m_pipelineMutex, 30);
        }

        if (!m_pipelineActive.load())
            break;

        if (!m_writebackQueue.isEmpty())
        {
            PALInstruction instr = m_writebackQueue.dequeue();

            // Writeback results and update system state
            if (instr.isCompleted && instr.writeResult && instr.targetRegister != 31)
            {
                writeIntegerRegisterWithCache(instr.targetRegister, instr.result);
            }

            // Update system state if needed
            if (instr.modifiesSystemState)
            {
                updateSystemState(instr);
            }

            // Coordinate with other execution units
            if (instr.requiresBarrier)
            {
                coordinateWithOtherExecutors(instr);
            }

            // Update dependency tracking
            updateDependencies(instr);

            // Emit completion signal
            int cycles = measureExecutionCycles(instr);
            emit palInstructionExecuted(instr.function, instr.isCompleted, cycles);

            // Handle exceptions
            if (instr.hasException)
            {
                emit privilegeViolation(instr.function, instr.pc);
            }
        }
    }
}

void executorAlphaPAL::analyzePALInstruction(PALInstruction &instr)
{
    // Extract and classify the PAL function
    PALFunctionClass classification = classifyPALFunction(instr.function);

    // Set instruction attributes based on function
    switch (classification)
    {
    case PALFunctionClass::SYSTEM_CALL:
        instr.requiresKernelMode = false; // System calls transition to kernel
        instr.modifiesSystemState = true;
        instr.requiresBarrier = true;
        break;

    case PALFunctionClass::MEMORY_MANAGEMENT:
        instr.requiresKernelMode = true;
        instr.modifiesSystemState = true;
        instr.invalidatesTLB = true;
        instr.requiresBarrier = true;
        break;

    case PALFunctionClass::CACHE_CONTROL:
        instr.requiresKernelMode = true;
        instr.flushesCache = true;
        instr.requiresBarrier = true;
        break;

    case PALFunctionClass::CONTEXT_SWITCH:
        instr.requiresKernelMode = true;
        instr.modifiesSystemState = true;
        instr.flushesCache = true;
        instr.invalidatesTLB = true;
        instr.requiresBarrier = true;
        break;

    default:
        instr.requiresKernelMode = true;
        instr.modifiesSystemState = false;
        instr.requiresBarrier = false;
        break;
    }

    // Determine result register for functions that return values
    switch (instr.function)
    {
    case FUNC_Common_MFPR_ASTEN:
    case FUNC_Common_MFPR_ASTSR:
    case FUNC_Common_MFPR_VPTB:
    case FUNC_Common_RDVAL:
    case FUNC_Common_RDPS:
    case FUNC_Common_RDUSP:
        instr.writeResult = true;
        instr.targetRegister = instr.instruction.raw & 0x1F; // Ra field
        break;
    default:
        instr.writeResult = false;
        instr.targetRegister = 31;
        break;
    }
}

bool executorAlphaPAL::executeSystemCall(const PALInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    m_systemCalls++;

    switch (instr.function)
    {
    case FUNC_Common_CSERVE:
        return executeCSERVE(instr);
    case FUNC_Common_BPT:
        return executeBPT(instr);
    case FUNC_Common_BUGCHK:
        return executeBUGCHK(instr);
    case FUNC_Common_CHME:
        return executeCHME(instr);
    case FUNC_Common_CHMS:
        return executeCHMS(instr);
    case FUNC_Common_CHMU:
        return executeCHMU(instr);
    default:
        return false;
    }
}

bool executorAlphaPAL::executeMemoryManagement(const PALInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    m_tlbOperations++;

    switch (instr.function)
    {
    case FUNC_Common_MTPR_TBISD:
        return executeMTPR_TBISD(instr);
    case FUNC_Common_MTPR_TBISI:
        return executeMTPR_TBISI(instr);
    case FUNC_Common_MTPR_TBIA:
        return executeMTPR_TBIA(instr);
    case FUNC_Common_MTPR_TBIS:
        return executeMTPR_TBIS(instr);
    case FUNC_Common_TBI:
        return executeTBI(instr);
    case FUNC_Common_MTPR_VPTB:
        return executeMTPR_VPTB(instr);
    case FUNC_Common_MFPR_VPTB:
        return executeMFPR_VPTB(instr);
    case FUNC_Common_PROBEW:
        return executePROBEW(instr);
    case FUNC_Common_PROBER:
        return executePROBER(instr);
    default:
        return false;
    }
}

bool executorAlphaPAL::executePrivilegeOperation(const PALInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    m_iprOperations++;

    switch (instr.function)
    {
    case FUNC_Common_HALT:
        return executeHALT(instr);
    case FUNC_Common_MFPR_ASTEN:
        return executeMFPR_ASTEN(instr);
    case FUNC_Common_MFPR_ASTSR:
        return executeMFPR_ASTSR(instr);
    case FUNC_Common_WRVAL:
        return executeWRVAL(instr);
    case FUNC_Common_RDVAL:
        return executeRDVAL(instr);
    case FUNC_Common_WRENT:
        return executeWRENT(instr);
    case FUNC_Common_SWPIPL:
        return executeSWPIPL(instr);
    case FUNC_Common_RDPS:
        return executeRDPS(instr);
    case FUNC_Common_WRKGP:
        return executeWRKGP(instr);
    case FUNC_Common_WRUSP:
        return executeWRUSP(instr);
    case FUNC_Common_RDUSP:
        return executeRDUSP(instr);
    case FUNC_Common_MFPR_FEN:
        return executeMFPR_FEN(instr);
    case FUNC_Common_WRPERFMON:
        return executeWRPERFMON(instr);
    case FUNC_Common_REI:
        return executeREI(instr);
    case FUNC_Common_IMB:
        return executeIMB(instr);
    default:
        return false;
    }
}

bool executorAlphaPAL::executePerformanceCounter(const PALInstruction &instr)
{
    switch (instr.function)
    {
    case FUNC_Common_WRPERFMON:
        return executeWRPERFMON(instr);
    default:
        return false;
    }
}

bool executorAlphaPAL::executeContextSwitch(const PALInstruction &instr)
{
    QMutexLocker locker(&m_statsMutex);
    m_contextSwitches++;

    switch (instr.function)
    {
    case FUNC_Common_SWPCTX:
        return executeSWPCTX(instr);
    default:
        return false;
    }
}

// Individual PAL Function Implementations

bool executorAlphaPAL::executeHALT(const PALInstruction &instr)
{
    qDebug() << "PAL HALT executed at PC:" << Qt::hex << instr.pc;

    // Coordinate with barrier executor to ensure all operations complete
    if (m_barrierExecutor)
    {
        // Wait for all pending operations to complete
        int timeout = 5000; // 5 second timeout
        while (timeout-- > 0 &&
               (m_barrierExecutor->isMemoryBarrierPending() || m_barrierExecutor->isWriteBarrierPending() ||
                m_barrierExecutor->isTrapBarrierPending()))
        {
            QThread::msleep(1);
        }
    }

    // Flush all caches
    flushL1Cache(true, true);
    flushL2Cache();
    flushL3Cache();

    // Stop the CPU
    if (m_cpu)
    {
        m_cpu->halt();
    }

    emit palInstructionExecuted(FUNC_Common_HALT, true, 100);
    return true;
}

bool executorAlphaPAL::executeCFLUSH(const PALInstruction &instr)
{
    qDebug() << "PAL CFLUSH executed at PC:" << Qt::hex << instr.pc;

    QMutexLocker locker(&m_statsMutex);
    m_cacheFlushes++;

    // Flush cache hierarchy
    flushL1Cache(true, true); // Both I and D cache
    flushL2Cache();
    flushL3Cache();

    emit cacheFlushRequested("ALL");
    return true;
}
bool executorAlphaPAL::executeDRAINA(const PALInstruction &instr)
{
    qDebug() << "PAL DRAINA executed at PC:" << Qt::hex << instr.pc;

    // Drain all pending memory operations
    if (m_barrierExecutor)
    {
        // Issue memory barrier to drain all operations
        DecodedInstruction barrierInstr;
        barrierInstr.raw = 0x18000000 | (FUNC_MB << 5);
        m_barrierExecutor->executeBarrier(barrierInstr);
    }

    // Wait for write buffers to drain
    QThread::msleep(1); // Simulate drain time

    return true;
}

bool executorAlphaPAL::executeSWPCTX(const PALInstruction &instr)
{
    qDebug() << "PAL SWPCTX executed at PC:" << Qt::hex << instr.pc;

    if (!m_cpu)
        return false;

    // Get old and new context from register Ra
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newContext = 0;

    if (!readIntegerRegisterWithCache(ra, newContext))
    {
        return false;
    }

    quint64 oldContext = m_cpu->getCurrentContext();

    // Perform context switch
    // 1. Save current context
    m_cpu->saveContext(oldContext);

    // 2. Flush TLB for old ASN
    if (m_dTLB)
    {
        m_dTLB->invalidateASN(oldContext & 0xFF);
    }
    if (m_iTLB)
    {
        m_iTLB->invalidateASN(oldContext & 0xFF);
    }

    // 3. Load new context
    m_cpu->loadContext(newContext);

    // 4. Return old context in Ra
    const_cast<PALInstruction &>(instr).result = oldContext;
    const_cast<PALInstruction &>(instr).writeResult = true;
    const_cast<PALInstruction &>(instr).targetRegister = ra;

    emit contextSwitchRequested(oldContext, newContext);
    return true;
}

bool executorAlphaPAL::executeCSERVE(const PALInstruction &instr)
{
    qDebug() << "PAL CSERVE executed at PC:" << Qt::hex << instr.pc;

    // Console service function - implementation depends on platform
    // For now, just acknowledge the call
    return true;
}

bool executorAlphaPAL::executeMTPR_TBISD(const PALInstruction &instr)
{
    qDebug() << "PAL MTPR_TBISD executed at PC:" << Qt::hex << instr.pc;

    // Invalidate single data TLB entry
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 virtualAddress = 0;

    if (!readIntegerRegisterWithCache(ra, virtualAddress))
    {
        return false;
    }

    if (m_dTLB)
    {
        m_dTLB->invalidateAddress(virtualAddress);
        emit tlbOperationCompleted("TBISD", 1);
    }

    return true;
}

bool executorAlphaPAL::executeMTPR_TBISI(const PALInstruction &instr)
{
    qDebug() << "PAL MTPR_TBISI executed at PC:" << Qt::hex << instr.pc;

    // Invalidate single instruction TLB entry
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 virtualAddress = 0;

    if (!readIntegerRegisterWithCache(ra, virtualAddress))
    {
        return false;
    }

    if (m_iTLB)
    {
        m_iTLB->invalidateAddress(virtualAddress);
        emit tlbOperationCompleted("TBISI", 1);
    }

    return true;
}

bool executorAlphaPAL::executeMTPR_TBIA(const PALInstruction &instr)
{
    qDebug() << "PAL MTPR_TBIA executed at PC:" << Qt::hex << instr.pc;

    // Invalidate all TLB entries
    int entriesInvalidated = 0;

    if (m_dTLB)
    {
        auto stats = m_dTLB->getStatistics();
        entriesInvalidated += stats.insertions - stats.evictions;
        m_dTLB->invalidateAll();
    }

    if (m_iTLB)
    {
        auto stats = m_iTLB->getStatistics();
        entriesInvalidated += stats.insertions - stats.evictions;
        m_iTLB->invalidateAll();
    }

    emit tlbOperationCompleted("TBIA", entriesInvalidated);
    return true;
}

bool executorAlphaPAL::executeMTPR_TBIS(const PALInstruction &instr)
{
    qDebug() << "PAL MTPR_TBIS executed at PC:" << Qt::hex << instr.pc;

    // Invalidate single TLB entry (both I and D)
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 virtualAddress = 0;

    if (!readIntegerRegisterWithCache(ra, virtualAddress))
    {
        return false;
    }

    int entriesInvalidated = 0;
    if (m_dTLB)
    {
        m_dTLB->invalidateAddress(virtualAddress);
        entriesInvalidated++;
    }
    if (m_iTLB)
    {
        m_iTLB->invalidateAddress(virtualAddress);
        entriesInvalidated++;
    }

    emit tlbOperationCompleted("TBIS", entriesInvalidated);
    return true;
}

bool executorAlphaPAL::executeTBI(const PALInstruction &instr)
{
    qDebug() << "PAL TBI executed at PC:" << Qt::hex << instr.pc;

    // TBI with different modes based on Ra value
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 mode = 0;

    if (!readIntegerRegisterWithCache(ra, mode))
    {
        return false;
    }

    int entriesInvalidated = 0;
    switch (mode & 0x3)
    {
    case 0: // Invalidate all
        if (m_dTLB)
        {
            m_dTLB->invalidateAll();
            entriesInvalidated += m_dTLB->getTotalEntries();
        }
        if (m_iTLB)
        {
            m_iTLB->invalidateAll();
            entriesInvalidated += m_iTLB->getTotalEntries();
        }
        break;
    case 1: // Invalidate by ASN
    {
        quint64 asn = (mode >> 8) & 0xFF;
        if (m_dTLB)
        {
            m_dTLB->invalidateASN(asn);
            entriesInvalidated += 10; // Estimate
        }
        if (m_iTLB)
        {
            m_iTLB->invalidateASN(asn);
            entriesInvalidated += 10; // Estimate
        }
    }
    break;
    default:
        return false;
    }

    emit tlbOperationCompleted("TBI", entriesInvalidated);
    return true;
}

bool executorAlphaPAL::executeMemoryOperation(quint64 address, bool isWrite, const PALInstruction &instr)
{
    try
    {
        // Try the memory operation
        if (isWrite)
        {
            return performMemoryWrite(address, instr);
        }
        else
        {
            return performMemoryRead(address, instr);
        }
    }
    catch (const TLBExceptionQ &tlbEx)
    {
        // Handle TLB exception using your exception class
        return handleTLBException(tlbEx, instr);
    }
    catch (const MemoryAccessException &memEx)
    {
        // Handle memory access exception using your exception class
        return handleMemoryAccessException(memEx, instr);
    }
    catch (const FPException &fpEx)
    {
        // Handle floating-point exception
        return handleFloatingPointException(fpEx, instr);
    }
    catch (const IllegalInstructionException &illEx)
    {
        // Handle illegal instruction exception
        return handleIllegalInstructionException(illEx, instr);
    }
    catch (...)
    {
        // Handle any other exceptions
        qWarning() << "Unknown exception in PAL executor";
        return false;
    }
}


bool executorAlphaPAL::executeMFPR_ASTEN(const PALInstruction &instr)
{
    qDebug() << "PAL MFPR_ASTEN executed at PC:" << Qt::hex << instr.pc;

    // Read AST enable register
    quint64 astenValue = 0;
    if (!readIPRWithCache("ASTEN", astenValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = astenValue;
    return true;
}

bool executorAlphaPAL::executeMFPR_ASTSR(const PALInstruction &instr)
{
    qDebug() << "PAL MFPR_ASTSR executed at PC:" << Qt::hex << instr.pc;

    // Read AST summary register
    quint64 astsrValue = 0;
    if (!readIPRWithCache("ASTSR", astsrValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = astsrValue;
    return true;
}

bool executorAlphaPAL::executeMFPR_VPTB(const PALInstruction &instr)
{
    qDebug() << "PAL MFPR_VPTB executed at PC:" << Qt::hex << instr.pc;

    // Read virtual page table base register
    quint64 vptbValue = 0;
    if (!readIPRWithCache("VPTB", vptbValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = vptbValue;
    return true;
}

bool executorAlphaPAL::executeMTPR_VPTB(const PALInstruction &instr)
{
    qDebug() << "PAL MTPR_VPTB executed at PC:" << Qt::hex << instr.pc;

    // Write virtual page table base register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 vptbValue = 0;

    if (!readIntegerRegisterWithCache(ra, vptbValue))
    {
        return false;
    }

    return writeIPRWithCache("VPTB", vptbValue);
}

bool executorAlphaPAL::executeWRVAL(const PALInstruction &instr)
{
    qDebug() << "PAL WRVAL executed at PC:" << Qt::hex << instr.pc;

    // Write value to PAL temporary register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 value = 0;

    if (!readIntegerRegisterWithCache(ra, value))
    {
        return false;
    }

    return writeIPRWithCache("PAL_TEMP", value);
}

bool executorAlphaPAL::executeRDVAL(const PALInstruction &instr)
{
    qDebug() << "PAL RDVAL executed at PC:" << Qt::hex << instr.pc;

    // Read value from PAL temporary register
    quint64 value = 0;
    if (!readIPRWithCache("PAL_TEMP", value))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = value;
    return true;
}

bool executorAlphaPAL::executeWRENT(const PALInstruction &instr)
{
    qDebug() << "PAL WRENT executed at PC:" << Qt::hex << instr.pc;

    // Write entry point address
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 entryPoint = 0;
    quint64 entryType = 0;

    if (!readIntegerRegisterWithCache(ra, entryPoint) || !readIntegerRegisterWithCache(rb, entryType))
    {
        return false;
    }

    // Store entry point based on type
    QString iprName = QString("ENTRY_%1").arg(entryType);
    return writeIPRWithCache(iprName, entryPoint);
}

bool executorAlphaPAL::executeSWPIPL(const PALInstruction &instr)
{
    qDebug() << "PAL SWPIPL executed at PC:" << Qt::hex << instr.pc;

    // Swap interrupt priority level
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newIPL = 0;

    if (!readIntegerRegisterWithCache(ra, newIPL))
    {
        return false;
    }

    quint64 oldIPL = 0;
    if (!readIPRWithCache("IPL", oldIPL))
    {
        return false;
    }

    // Write new IPL
    if (!writeIPRWithCache("IPL", newIPL))
    {
        return false;
    }

    // Return old IPL
    const_cast<PALInstruction &>(instr).result = oldIPL;
    return true;
}

bool executorAlphaPAL::executeRDPS(const PALInstruction &instr)
{
    qDebug() << "PAL RDPS executed at PC:" << Qt::hex << instr.pc;

    // Read processor status
    quint64 psValue = 0;
    if (!readIPRWithCache("PS", psValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = psValue;
    return true;
}

bool executorAlphaPAL::executeWRKGP(const PALInstruction &instr)
{
    qDebug() << "PAL WRKGP executed at PC:" << Qt::hex << instr.pc;

    // Write kernel global pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 kgpValue = 0;

    if (!readIntegerRegisterWithCache(ra, kgpValue))
    {
        return false;
    }

    return writeIPRWithCache("KGP", kgpValue);
}

bool executorAlphaPAL::executeWRUSP(const PALInstruction &instr)
{
    qDebug() << "PAL WRUSP executed at PC:" << Qt::hex << instr.pc;

    // Write user stack pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 uspValue = 0;

    if (!readIntegerRegisterWithCache(ra, uspValue))
    {
        return false;
    }

    return writeIPRWithCache("USP", uspValue);
}

bool executorAlphaPAL::executeRDUSP(const PALInstruction &instr)
{
    qDebug() << "PAL RDUSP executed at PC:" << Qt::hex << instr.pc;

    // Read user stack pointer
    quint64 uspValue = 0;
    if (!readIPRWithCache("USP", uspValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = uspValue;
    return true;
}

bool executorAlphaPAL::executeMFPR_FEN(const PALInstruction &instr)
{
    qDebug() << "PAL MFPR_FEN executed at PC:" << Qt::hex << instr.pc;

    // Read floating-point enable register
    quint64 fenValue = 0;
    if (!readIPRWithCache("FEN", fenValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = fenValue;
    return true;
}

bool executorAlphaPAL::executeWRPERFMON(const PALInstruction &instr)
{
    qDebug() << "PAL WRPERFMON executed at PC:" << Qt::hex << instr.pc;

    // Write performance monitor register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 pmValue = 0;
    quint64 pmSelect = 0;

    if (!readIntegerRegisterWithCache(ra, pmValue) || !readIntegerRegisterWithCache(rb, pmSelect))
    {
        return false;
    }

    QString iprName = QString("PERFMON_%1").arg(pmSelect);
    return writeIPRWithCache(iprName, pmValue);
}

bool executorAlphaPAL::executeREI(const PALInstruction &instr)
{
    qDebug() << "PAL REI executed at PC:" << Qt::hex << instr.pc;

    // Return from exception/interrupt
    if (!m_cpu)
        return false;

    // Restore processor state from exception stack
    quint64 newPC = 0;
    quint64 newPS = 0;

    if (!readIPRWithCache("EXC_PC", newPC) || !readIPRWithCache("EXC_PS", newPS))
    {
        return false;
    }

    // Restore state
    m_cpu->setPC(newPC);
    writeIPRWithCache("PS", newPS);

    return true;
}

bool executorAlphaPAL::executeIMB(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL IMB executed at PC: %1")
        .arg( QString::number(instr.pc,16)));

    // Instruction memory barrier - flush instruction cache and pipeline
    if (m_instructionCache)
    {
        // Invalidate all instruction cache lines
        m_instructionCache->invalidateAll();

        // Disable auto-prefetch temporarily to avoid fetching stale instructions
        m_instructionCache->enableAutoPrefetch(false);

        // Re-enable after a short delay (could be done with a timer)
        QTimer::singleShot(10,
                           [this]()
                           {
                               if (m_instructionCache)
                               {
                                   m_instructionCache->enableAutoPrefetch(true);
                               }
                           });
    }

    // Coordinate with other execution units to flush pipelines
    if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive())
    {
        // Signal pipeline flush needed
        DEBUG_LOG("PAL IMB: Signaling FP pipeline flush");
    }

    if (m_intExecutor && m_intExecutor->isAsyncPipelineActive())
    {
        // Signal pipeline flush needed
        DEBUG_LOG("PAL IMB: Signaling Integer pipeline flush");
    }

    return true;
}

bool executorAlphaPAL::executeBPT(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL BPT executed at PC: %1")
        .arg(QString::number(instr.pc, 16)));

    // Breakpoint trap
    if (m_cpu)
    {
        m_cpu->raiseException(0x0080, instr.pc); // Breakpoint exception
    }

    return true;
}

bool executorAlphaPAL::executeBUGCHK(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL BUGCHK executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Bug check - system error
    if (m_cpu)
    {
        m_cpu->raiseException(0x0200, instr.pc); // System error
    }

    return true;
}

bool executorAlphaPAL::executeCHME(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL CHME executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Change mode to executive
    if (m_cpu)
    {
        m_cpu->setPrivilegeMode(1); // Executive mode
    }

    emit systemCallInvoked(FUNC_Common_CHME_, instr.pc);
    return true;
}

bool executorAlphaPAL::executeCHMS(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL CHMS executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Change mode to supervisor
    if (m_cpu)
    {
        m_cpu->setPrivilegeMode(2); // Supervisor mode
    }

    emit systemCallInvoked(FUNC_Common_CHMS_, instr.pc);
    return true;
}

bool executorAlphaPAL::executeCHMU(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL CHMU executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Change mode to user
    if (m_cpu)
    {
        m_cpu->setPrivilegeMode(3); // User mode
    }

    emit systemCallInvoked(FUNC_Common_CHMU_, instr.pc);
    return true;
}

bool executorAlphaPAL::executePROBEW(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL PROBEW executed at PC: %1").arg(QString::number(instr.pc, 16)));


    // Probe for write access
    quint8 ra = (instr.instruction.rawInstruction >> 21) & 0x1F;
    quint64 virtualAddress = 0;

    if (!readIntegerRegisterWithCache(ra, virtualAddress))
    {
        return false;
    }

    // Check write permissions via TLB
    quint64 physicalAddress = 0;
    bool accessible = false;

    if (m_dTLB)
    {
        accessible = m_dTLB->lookup(virtualAddress, m_cpu->getCurrentASN(), false, false, physicalAddress);
        // Additional permission check would go here
    }

    const_cast<PALInstruction &>(instr).result = accessible ? 1 : 0;
    return true;
}

bool executorAlphaPAL::executePROBER(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL PROBER executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Probe for read access
    quint8 ra = (instr.instruction.rawInstruction >> 21) & 0x1F;
    quint64 virtualAddress = 0;

    if (!readIntegerRegisterWithCache(ra, virtualAddress))
    {
        return false;
    }

    // Check read permissions via TLB
    quint64 physicalAddress = 0;
    bool accessible = false;

    if (m_dTLB)
    {
        accessible = m_dTLB->lookup(virtualAddress, m_cpu->getCurrentASN(), false, false, physicalAddress);
    }

    const_cast<PALInstruction &>(instr).result = accessible ? 1 : 0;
    return true;
}

// Queue Operations (Alpha-specific interlocked operations)

bool executorAlphaPAL::executeINSQHIL(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL INSQHIL executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Insert entry at head of longword queue, interlocked
    quint8 ra = (instr.instruction.rawInstruction >> 21) & 0x1F;
    quint8 rb = (instr.instruction.rawInstruction >> 16) & 0x1F;

    quint64 entryAddr = 0;
    quint64 headerAddr = 0;

    if (!readIntegerRegisterWithCache(ra, entryAddr) || !readIntegerRegisterWithCache(rb, headerAddr))
    {
        return false;
    }

    // This would implement the interlocked queue insertion
    // For now, just return success
    const_cast<PALInstruction &>(instr).result = 1; // Success
    return true;
}

bool executorAlphaPAL::executeINSQTIL(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL INSQTIL executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Insert entry at tail of longword queue, interlocked
    // Similar to INSQHIL but at tail
    const_cast<PALInstruction &>(instr).result = 1; // Success
    return true;
}

bool executorAlphaPAL::executeINSQHIQ(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL INSQHIQ executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Insert entry at head of quadword queue, interlocked
    const_cast<PALInstruction &>(instr).result = 1; // Success
    return true;
}

bool executorAlphaPAL::executeREMQHIL(const PALInstruction &instr)
{
   DEBUG_LOG(QString("PAL REMQHIL executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Remove entry from head of longword queue, interlocked
    const_cast<PALInstruction &>(instr).result = 0x12345678; // Mock entry
    return true;
}

bool executorAlphaPAL::executeREMQTIL(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL REMQHIL executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Remove entry from tail of longword queue, interlocked
    const_cast<PALInstruction &>(instr).result = 0x87654321; // Mock entry
    return true;
}

bool executorAlphaPAL::executeREMQHIQ(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL REMQHIL executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Remove entry from head of quadword queue, interlocked
    const_cast<PALInstruction &>(instr).result = 0x123456789ABCDEF0ULL; // Mock entry
    return true;
}

bool executorAlphaPAL::executeREMQTIQ(const PALInstruction &instr)
{
    DEBUG_LOG(QString("PAL REMQHIL executed at PC: %1").arg(QString::number(instr.pc, 16)));

    // Remove entry from tail of quadword queue, interlocked
    const_cast<PALInstruction &>(instr).result = 0xFEDCBA9876543210ULL; // Mock entry
    return true;
}

// Helper Methods Implementation

void executorAlphaPAL::analyzeDependencies(PALInstruction &instr)
{
    quint32 raw = instr.instruction.rawInstruction;
    quint8 ra = (raw >> 21) & 0x1F;
    quint8 rb = (raw >> 16) & 0x1F;

    // Clear existing dependencies
    instr.srcRegisters.clear();
    instr.dstRegisters.clear();

    // Most PAL instructions that use Ra as source
    switch (instr.function)
    {
    case FUNC_Common_MTPR_TBISD:
    case FUNC_Common_MTPR_TBISI:
    case FUNC_Common_MTPR_TBIS:
    case FUNC_Common_TBI:
    case FUNC_Common_WRVAL:
    case FUNC_Common_SWPIPL:
    case FUNC_Common_WRKGP:
    case FUNC_Common_WRUSP:
    case FUNC_Common_MTPR_VPTB:
    case FUNC_Common_WRPERFMON:
        if (ra != 31)
        {
            instr.srcRegisters.insert(ra);
        }
        break;
    }

    // Instructions that use Rb as source
    switch (instr.function)
    {
    case FUNC_Common_WRENT:
    case FUNC_Common_WRPERFMON:
        if (rb != 31)
        {
            instr.srcRegisters.insert(rb);
        }
        break;
    }

    // Instructions that write to Ra
    switch (instr.function)
    {
    case FUNC_Common_MFPR_ASTEN:
    case FUNC_Common_MFPR_ASTSR:
    case FUNC_Common_MFPR_VPTB:
    case FUNC_Common_RDVAL:
    case FUNC_Common_RDPS:
    case FUNC_Common_RDUSP:
    case FUNC_Common_MFPR_FEN:
    case FUNC_Common_SWPIPL: // Returns old IPL
    case FUNC_Common_SWPCTX: // Returns old context
    case FUNC_Common_PROBEW:
    case FUNC_Common_PROBER:
        if (ra != 31)
        {
            instr.dstRegisters.insert(ra);
        }
        break;
    }

    // Mark IPR access
    switch (instr.function)
    {
    case FUNC_Common_MFPR_ASTEN:
    case FUNC_Common_MFPR_ASTSR:
    case FUNC_Common_MFPR_VPTB:
    case FUNC_Common_MFPR_FEN:
    case FUNC_Common_MTPR_VPTB:
    case FUNC_Common_SWPIPL:
    case FUNC_Common_RDPS:
    case FUNC_Common_WRKGP:
    case FUNC_Common_WRUSP:
    case FUNC_Common_RDUSP:
    case FUNC_Common_WRPERFMON:
        instr.touchesIPR = true;
        break;
    }
}

bool executorAlphaPAL::checkDependencies(const PALInstruction &instr) const
{
    // For PAL instructions, most dependencies are resolved by privilege checking
    // and system state coordination rather than register dependencies

    // Check if system is in correct state for execution
    if (instr.requiresBarrier && m_barrierExecutor)
    {
        if (m_barrierExecutor->isMemoryBarrierPending() || m_barrierExecutor->isWriteBarrierPending() ||
            m_barrierExecutor->isTrapBarrierPending())
        {
            return false; // Wait for barriers to complete
        }
    }

    // Check for IPR write ordering
    if (instr.touchesIPR && m_pendingIPRWrites.load() > 0)
    {
        return false; // Serialize IPR operations
    }

    return true;
}

void executorAlphaPAL::updateDependencies(const PALInstruction &instr)
{
    // Update IPR write tracking
    if (instr.touchesIPR)
    {
        if (instr.isCompleted)
        {
            m_pendingIPRWrites.fetch_sub(1);
        }
        else
        {
            m_pendingIPRWrites.fetch_add(1);
        }
    }
}


bool executorAlphaPAL::checkForTLBMiss(quint64 virtualAddress, bool isWrite)
{
    // Method 1: Check via TLB lookup if we have TLB access
    if (m_dTLB && m_iTLB)
    {
        quint64 physicalAddr = 0;
        quint64 currentASN = m_cpu ? m_cpu->getCurrentASN() : 0;

        // Choose appropriate TLB based on access type
        AlphaTranslationCache *tlb = isWrite ? m_dTLB.get() : m_iTLB.get();

        // Try to lookup the address in TLB
        bool tlbHit = tlb->lookup(virtualAddress, currentASN, isWrite, false, physicalAddr);

        return !tlbHit; // TLB miss if lookup failed
    }

    // Method 2: Check via memory system if no direct TLB access
    if (m_cpu && m_cpu->getMemorySystem())
    {
        return m_cpu->getMemorySystem()->wouldCauseTLBMiss(virtualAddress, currentASN, isWrite);
    }

    // Method 3: Try actual memory access and catch the fault
    return checkMemoryAccessWouldFault(virtualAddress, isWrite);
}

bool executorAlphaPAL::checkMemoryAccessWouldFault(quint64 virtualAddress, bool isWrite)
{
    if (!m_cpu)
        return true; // Assume fault if no CPU

    // Try to translate the address through the MMU
    // This is a non-faulting probe of the memory system

    quint64 physicalAddr = 0;

    // Method A: Use existing memory system translation check
    if (m_cpu->getMemorySystem())
    {
        auto memSys = m_cpu->getMemorySystem();

        // Check if this would cause a translation fault
        // You might have a method like translateAddress that doesn't fault
        if (memSys->hasTranslateAddressMethod())
        {
            return !memSys->translateAddress(virtualAddress, physicalAddr, isWrite, false); // false = don't fault
        }
    }

    // Method B: Check page table directly if accessible
    return checkPageTableEntry(virtualAddress, isWrite);
}

bool executorAlphaPAL::checkPageTableEntry(quint64 virtualAddress, bool isWrite)
{
    if (!m_cpu)
        return true; // Assume fault

    // Get page table base from VPTB register
    quint64 vptb = 0;
    if (!readIPRWithCache("VPTB", vptb))
    {
        return true; // Can't read VPTB, assume fault
    }

    // Calculate page table entry address
    // Alpha uses 8KB pages, so we need bits 63:13 of virtual address
    quint64 vpn = virtualAddress >> 13; // Virtual page number
    quint64 pteAddr = vptb + (vpn * 8); // Each PTE is 8 bytes

    // Try to read the page table entry
    quint64 pte = 0;
    if (!readMemoryWithoutFault(pteAddr, pte))
    {
        return true; // Can't read PTE, assume fault
    }

    // Check if PTE is valid
    bool pteValid = (pte & 0x1) != 0; // Valid bit is bit 0
    if (!pteValid)
    {
        return true; // Invalid PTE = translation fault
    }

    // Check permissions if write access
    if (isWrite)
    {
        bool writable = (pte & 0x2) != 0; // Write bit is bit 1 (example)
        if (!writable)
        {
            return true; // Write to read-only page = fault
        }
    }

    return false; // No fault expected
}


bool executorAlphaPAL::checkPrivilegeLevel(const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    // System calls from user mode are allowed
    PALFunctionClass classification = classifyPALFunction(instr.function);
    if (classification == PALFunctionClass::SYSTEM_CALL)
    {
        return true;
    }

    // Other PAL operations require kernel mode
    if (instr.requiresKernelMode)
    {
        return m_cpu->isKernelMode();
    }

    return true;
}

void executorAlphaPAL::updateSystemState(const PALInstruction &instr)
{
    // Update system state based on completed instruction
    if (instr.flushesCache)
    {
        emit cacheFlushRequested("SYSTEM_STATE_CHANGE");
    }

    if (instr.invalidatesTLB)
    {
        // TLB invalidation already handled in specific functions
    }

    if (instr.modifiesSystemState && m_cpu)
    {
        m_cpu->notifySystemStateChange();
    }
}

void executorAlphaPAL::coordinateWithOtherExecutors(const PALInstruction &instr)
{
    // Coordinate with other execution units for barrier operations
    if (instr.requiresBarrier && m_barrierExecutor)
    {
        // Issue appropriate barrier
        DecodedInstruction barrierInstr;
        barrierInstr.rawInstruction = 0x18000000; // Base barrier instruction

        if (instr.flushesCache)
        {
            barrierInstr.rawInstruction |= (FUNC_MB << 5); // Memory barrier
        }
        else
        {
            barrierInstr.rawInstruction |= (FUNC_TRAPB << 5); // Trap barrier
        }

        m_barrierExecutor->submitBarrier(barrierInstr, instr.pc);
    }
}

// Cache Operations

bool executorAlphaPAL::fetchInstructionWithCache(quint64 pc, quint32 &instruction)
{
    QMutexLocker locker(&m_statsMutex);

    // TLB translation for instruction fetch
    quint64 physicalPC;
    if (m_iTLB && !m_iTLB->lookup(pc, m_cpu->getCurrentASN(), false, true, physicalPC))
    {
        m_l1ICacheMisses++;
        return false; // TLB miss
    }
    else if (!m_iTLB)
    {
        physicalPC = pc; // Direct mapping if no TLB
    }

    // Use instruction cache
    if (m_instructionCache)
    {
        InstructionWord instrWord;
        if (m_instructionCache->fetch(physicalPC, instrWord))
        {
            instruction = instrWord.getRawInstruction();
            m_l1ICacheHits++;
            updateCacheStatistics("L1I", true);
            return true;
        }
        else
        {
            m_l1ICacheMisses++;
            updateCacheStatistics("L1I", false);
        }
    }

    // Fallback to direct memory access
    return m_cpu ? m_cpu->readMemory(physicalPC, reinterpret_cast<quint8 *>(&instruction), 4) : false;
}

bool executorAlphaPAL::readIntegerRegisterWithCache(quint8 reg, quint64 &value)
{
    if (!m_cpu)
        return false;

    value = m_cpu->getIntegerRegister(reg);

    QMutexLocker locker(&m_statsMutex);
    m_l1DCacheHits++; // Register access is always a cache hit
    updateCacheStatistics("L1D", true);

    return true;
}

bool executorAlphaPAL::writeIntegerRegisterWithCache(quint8 reg, quint64 value)
{
    if (!m_cpu)
        return false;

    m_cpu->setIntegerRegister(reg, value);

    QMutexLocker locker(&m_statsMutex);
    m_l1DCacheHits++; // Register access is always a cache hit
    updateCacheStatistics("L1D", true);

    return true;
}

bool executorAlphaPAL::readIPRWithCache(const QString &iprName, quint64 &value)
{
    if (!m_cpu)
        return false;

    // Simulate IPR read with cache optimization for frequent registers
    static QMap<QString, quint64> iprCache;

    if (iprCache.contains(iprName))
    {
        value = iprCache[iprName];
        updateCacheStatistics("IPR", true);
    }
    else
    {
        // Read from CPU IPR bank
        value = m_cpu->readIPR(iprName);
        iprCache[iprName] = value;
        updateCacheStatistics("IPR", false);
    }

    return true;
}

bool executorAlphaPAL::writeIPRWithCache(const QString &iprName, quint64 value)
{
    if (!m_cpu)
        return false;

    // Write to CPU IPR bank
    bool success = m_cpu->writeIPR(iprName, value);

    // Invalidate cache entry
    static QMap<QString, quint64> iprCache;
    iprCache.remove(iprName);

    updateCacheStatistics("IPR", true);
    return success;
}

// JIT Optimization Methods

void executorAlphaPAL::updateJITStats(quint32 function)
{
    m_functionExecutionCount[function]++;

    // Mark as frequent if executed > 1000 times
    if (m_functionExecutionCount[function] > 1000)
    {
        m_frequentFunctions.insert(function);
    }

    // Track critical path for system functions
    trackCriticalPath(function);
}

bool executorAlphaPAL::isFrequentFunction(quint32 function) const { return m_frequentFunctions.contains(function); }

bool executorAlphaPAL::isCriticalFunction(quint32 function) const { return m_criticalFunctions.contains(function); }

void executorAlphaPAL::trackCriticalPath(quint32 function)
{
    // Functions that are on critical performance paths
    switch (function)
    {
    case FUNC_Common_SWPCTX:
    case FUNC_Common_REI:
    case FUNC_Common_SWPIPL:
    case FUNC_Common_MTPR_TBIA:
    case FUNC_Common_MTPR_TBIS:
        m_criticalFunctions.insert(function);
        break;
    }
}

void executorAlphaPAL::optimizeFrequentFunction(quint32 function)
{
    // JIT optimization for frequently executed functions
    qDebug() << "Optimizing frequent PAL function:" << Qt::hex << function;

    // Pre-load related cache lines
    preloadCriticalInstructions();

    // Prefetch system data that function typically accesses
    prefetchSystemData();
}

void executorAlphaPAL::preloadCriticalInstructions()
{
    if (!m_instructionCache)
        return;

    // Preload known critical PAL instruction sequences
    const QVector<quint64> criticalAddresses = {
        0x8000, // Typical PAL entry point
        0x8100, // Exception handlers
        0x8200, // Interrupt handlers
        0x8300, // System call handlers
    };

    for (quint64 addr : criticalAddresses)
    {
        // Warm the cache for these critical regions
        m_instructionCache->warmCache(addr, 256); // 256 bytes = 4 cache lines
    }

    // Add current hot spots
    for (quint32 func : m_frequentFunctions)
    {
        if (m_functionExecutionCount[func] > 1000)
        {
            // This is a frequently executed PAL function
            // Add its likely address range as a hot spot
            quint64 estimatedAddr = 0x8000 + (func * 64); // Estimate based on function
            m_instructionCache->addHotSpot(estimatedAddr, 128);
        }
    }
}


void executorAlphaPAL::prefetchSystemData()
{
    // Prefetch system data structures commonly accessed by PAL
    if (m_level1DataCache)
    {
        // Prefetch IPR data, page tables, etc.
        // Implementation depends on system layout
    }
}

// Cache Management

void executorAlphaPAL::flushL1Cache(bool instructionCache, bool dataCache)
{
    if (instructionCache && m_instructionCache)
    {
        m_instructionCache->flush();
         DEBUG_LOG( "PAL: Flushed instruction cache");
    }

    if (dataCache && m_level1DataCache)
    {
        m_level1DataCache->flush();
        DEBUG_LOG("PAL: Flushed data cache");
    }

    QMutexLocker locker(&m_statsMutex);
    m_cacheFlushes++;
}

// L2 Cache Flush Method Implementation for executorAlphaPAL

void executorAlphaPAL::flushL2Cache()
{
     DEBUG_LOG( "Flushing L2 Cache");

    bool cacheWasFlushed = false;

    if (m_level2Cache)
    {
        // Perform L2 cache flush operation
        m_level2Cache->flush();
        cacheWasFlushed = true;
         DEBUG_LOG( "L2 cache flush completed");
    }
    else
    {
        WARN_LOG( "L2 cache not available for flush operation");
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_cacheFlushes++;
    }

    // Emit cache flush signal
    emit cacheFlushRequested("L2");

    // Coordinate with barrier executor for memory ordering
    if (m_barrierExecutor && cacheWasFlushed)
    {
        m_barrierExecutor->notifyMemoryOperation(true); // L2 flush affects write ordering
        m_barrierExecutor->notifyMemoryOperationComplete(true);
    }

    // For SMP systems, coordinate L2 cache flush with other CPUs
    // L2 cache may be shared or private depending on Alpha implementation
    if (cacheWasFlushed)
    {
        broadcastCacheFlush("L2");
    }

    // Coordinate with other execution units
    if (cacheWasFlushed)
    {
        // Notify FP executor of L2 cache flush
        if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive())
        {
            DEBUG_LOG( "Notifying FP executor of L2 cache flush");
        }

        // Notify integer executor of L2 cache flush
        if (m_intExecutor && m_intExecutor->isAsyncPipelineActive())
        {
            DEBUG_LOG( "Notifying Integer executor of L2 cache flush");
        }
    }

    DEBUG_LOG(  "L2 cache flush operation complete");
}

// Helper method for broadcasting cache flush operations in SMP systems
void executorAlphaPAL::broadcastCacheFlush(const QString &cacheLevel)
{
    DEBUG_LOG(QString("Broadcasting %1 cache flush to SMP system").arg(cacheLevel));

    // In a real Alpha SMP system, this would:
    // 1. Send cache flush commands to other CPUs
    // 2. Coordinate shared cache invalidations
    // 3. Ensure cache coherency protocol compliance
    // 4. Wait for flush completion acknowledgments

    if (m_cpu)
    {
        DEBUG_LOG( QString("Broadcasting %1 flush from CPU %2").arg(cacheLevel).arg(m_cpu->getCpuId()));
    }

    // Emit signal for SMP manager coordination
    emit cacheFlushRequested(QString("SMP_%1_Flush").arg(cacheLevel));

    // Simulate inter-processor cache flush coordination delay
    QThread::msleep(2); // L2 flush takes longer than L1

    DEBUG_LOG( QString("SMP %1 cache flush broadcast complete").arg(cacheLevel));
}

void executorAlphaPAL::flushL3Cache()
{
    if (m_level3Cache)
    {
        m_level3Cache->flush();
    }
}

void executorAlphaPAL::updateCacheStatistics(const QString &level, bool hit)
{
    // Statistics already updated in caller
    // This could emit signals or update other tracking
}

int executorAlphaPAL::measureExecutionCycles(const PALInstruction &instr)
{
    PALFunctionClass classification = classifyPALFunction(instr.function);
    return estimatePALCycles(instr.function, classification);
}

// Statistics and Monitoring

// Helper method to identify critical PAL addresses:
bool executorAlphaPAL::isCriticalPALAddress(quint64 address) const
{
    // Define ranges of critical PAL code
    const QVector<QPair<quint64, quint64>> criticalRanges = {
        {0x8000, 0x80FF}, // Exception handlers
        {0x8100, 0x81FF}, // Interrupt handlers
        {0x8200, 0x82FF}, // System call handlers
        {0x8300, 0x83FF}, // Memory management handlers
    };

    for (const auto &range : criticalRanges)
    {
        if (address >= range.first && address <= range.second)
        {
            return true;
        }
    }

    return false;
}

// Enhanced statistics method to include instruction cache details:
void executorAlphaPAL::printStatistics() const
{
    QMutexLocker locker(&m_statsMutex);

    qDebug() << "=== Alpha PAL Executor Statistics ===";
    qDebug() << "Total PAL Instructions:" << m_palInstructions;
    qDebug() << "System Calls:" << m_systemCalls;
    qDebug() << "Privilege Violations:" << m_privilegeViolations;
    qDebug() << "IPR Operations:" << m_iprOperations;
    qDebug() << "TLB Operations:" << m_tlbOperations;
    qDebug() << "Cache Flushes:" << m_cacheFlushes;
    qDebug() << "Context Switches:" << m_contextSwitches;

    qDebug() << "\n=== Cache Performance ===";
    qDebug() << "L1 I-Cache: Hits=" << m_l1ICacheHits << ", Misses=" << m_l1ICacheMisses;
    qDebug() << "L1 D-Cache: Hits=" << m_l1DCacheHits << ", Misses=" << m_l1DCacheMisses;
    qDebug() << "L2 Cache: Hits=" << m_l2CacheHits << ", Misses=" << m_l2CacheMisses;
    qDebug() << "L3 Cache: Hits=" << m_l3CacheHits << ", Misses=" << m_l3CacheMisses;

    // Enhanced instruction cache statistics
    if (m_instructionCache)
    {
        auto icacheStats = m_instructionCache->getStatistics();
        qDebug() << "\n=== Instruction Cache Details ===";
        qDebug() << "Size:" << (m_instructionCache->getCacheSize() / 1024) << "KB";
        qDebug() << "Hit Rate:" << QString::number(icacheStats.getHitRate(), 'f', 2) << "%";
        qDebug() << "Invalidations:" << icacheStats.invalidations;
        qDebug() << "Prefetches:" << icacheStats.prefetches;
        qDebug() << "Coherency Events:" << icacheStats.coherencyEvents;
        qDebug() << "Used Lines:" << m_instructionCache->getUsedLines() << "/" << m_instructionCache->getTotalLines();
    }

    // Calculate overall hit rates
    quint64 totalL1IAccess = m_l1ICacheHits + m_l1ICacheMisses;
    if (totalL1IAccess > 0)
    {
        double hitRate = (static_cast<double>(m_l1ICacheHits) / totalL1IAccess) * 100.0;
        qDebug() << "Overall L1 I-Cache Hit Rate:" << QString::number(hitRate, 'f', 2) << "%";
    }
}

void executorAlphaPAL::clearStatistics()
{
    QMutexLocker locker(&m_statsMutex);

    m_palInstructions = 0;
    m_systemCalls = 0;
    m_privilegeViolations = 0;
    m_iprOperations = 0;
    m_tlbOperations = 0;
    m_cacheFlushes = 0;
    m_contextSwitches = 0;

    m_l1ICacheHits = 0;
    m_l1ICacheMisses = 0;
    m_l1DCacheHits = 0;
    m_l1DCacheMisses = 0;
    m_l2CacheHits = 0;
    m_l2CacheMisses = 0;
    m_l3CacheHits = 0;
    m_l3CacheMisses = 0;
}

void executorAlphaPAL::printJITOptimizationStats() const
{
    DEBUG_LOG("\n=== PAL JIT Optimization Statistics ===");
    DEBUG_LOG(QString("Frequent Functions: %1")
        .arg( m_frequentFunctions.size()));
    DEBUG_LOG(QString("Critical Functions: %1")
        .arg(m_criticalFunctions.size()));

    DEBUG_LOG( "\nFunction Execution Counts:");
    auto it = m_functionExecutionCount.constBegin();
    while (it != m_functionExecutionCount.constEnd())
    {
        if (it.value() > 100)
        { // Only show frequently executed functions
            DEBUG_LOG( QString("  0x%1: %2 times").arg(it.key(), 0, 16).arg(it.value()));
        }
        ++it;
    }

    DEBUG_LOG("\nCritical Functions:");
    for (quint32 func : m_criticalFunctions)
    {
        quint64 count = m_functionExecutionCount.value(func, 0);
        DEBUG_LOG(QString("  0x%1: %2 times").arg(func, 0, 16).arg(count));
    }
}

// Global helper functions implementation

PALFunctionClass classifyPALFunction(quint32 function)
{
    switch (function)
    {
    case FUNC_Common_BPT:
    case FUNC_Common_BUGCHK:
    case FUNC_Common_CHME:
    case FUNC_Common_CHMS:
    case FUNC_Common_CHMU:
    case FUNC_Common_CSERVE:
        return PALFunctionClass::SYSTEM_CALL;

    case FUNC_Common_MTPR_TBISD:
    case FUNC_Common_MTPR_TBISI:
    case FUNC_Common_MTPR_TBIA:
    case FUNC_Common_MTPR_TBIS:
    case FUNC_Common_TBI:
    case FUNC_Common_MTPR_VPTB:
    case FUNC_Common_MFPR_VPTB:
    case FUNC_Common_PROBEW:
    case FUNC_Common_PROBER:
        return PALFunctionClass::MEMORY_MANAGEMENT;

    case FUNC_Common_CFLUSH:
    case FUNC_Common_DRAINA:
    case FUNC_Common_IMB:
        return PALFunctionClass::CACHE_CONTROL;

    case FUNC_Common_SWPCTX:
        return PALFunctionClass::CONTEXT_SWITCH;

    case FUNC_Common_REI:
        return PALFunctionClass::INTERRUPT_HANDLING;

    case FUNC_Common_WRPERFMON:
        return PALFunctionClass::PERFORMANCE_COUNTER;

    case FUNC_Common_INSQHIL:
    case FUNC_Common_INSQTIL:
    case FUNC_Common_INSQHIQ:
    case FUNC_Common_REMQHIL:
    case FUNC_Common_REMQTIL:
    case FUNC_Common_REMQHIQ:
    case FUNC_Common_REMQTIQ:
        return PALFunctionClass::QUEUE_OPERATION;

    default:
        return PALFunctionClass::PRIVILEGE_OPERATION;
    }
}

int estimatePALCycles(quint32 function, PALFunctionClass classification)
{
    switch (classification)
    {
    case PALFunctionClass::SYSTEM_CALL:
        return 50; // System calls are expensive
    case PALFunctionClass::MEMORY_MANAGEMENT:
        return 25; // TLB operations
    case PALFunctionClass::CACHE_CONTROL:
        return 100; // Cache operations are very expensive
    case PALFunctionClass::CONTEXT_SWITCH:
        return 200; // Context switches are most expensive
    case PALFunctionClass::INTERRUPT_HANDLING:
        return 30; // Interrupt handling
    case PALFunctionClass::PERFORMANCE_COUNTER:
        return 5; // Performance counters are fast
    case PALFunctionClass::QUEUE_OPERATION:
        return 15; // Queue operations
    case PALFunctionClass::PRIVILEGE_OPERATION:
        return 10; // Basic privilege operations
    default:
        return 10;
    }
}

bool requiresSystemBarrier(quint32 function, PALFunctionClass classification)
{
    switch (classification)
    {
    case PALFunctionClass::MEMORY_MANAGEMENT:
    case PALFunctionClass::CACHE_CONTROL:
    case PALFunctionClass::CONTEXT_SWITCH:
        return true;
    case PALFunctionClass::SYSTEM_CALL:
        // Some system calls require barriers
        switch (function)
        {
        case FUNC_Common_CHME:
        case FUNC_Common_CHMS:
        case FUNC_Common_CHMU:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

// TLB

// TLB Coordination Methods Implementation for executorAlphaPAL

void executorAlphaPAL::invalidateTLBEntry(quint64 virtualAddress, quint64 asn)
{
    qDebug() << "Invalidating TLB entry for VA:" << Qt::hex << virtualAddress << "ASN:" << asn;

    int entriesInvalidated = 0;

    if (m_dTLB)
    {
        if (asn == 0)
        {
            // Invalidate for all ASNs
            m_dTLB->invalidateAddress(virtualAddress);
        }
        else
        {
            // Check if entry matches ASN before invalidating
            if (m_dTLB->contains(virtualAddress, asn))
            {
                m_dTLB->invalidateAddress(virtualAddress, asn);
                entriesInvalidated++;
            }
        }
    }

    if (m_iTLB)
    {
        if (asn == 0)
        {
            // Invalidate for all ASNs
            m_iTLB->invalidateAddress(virtualAddress);
        }
        else
        {
            // Check if entry matches ASN before invalidating
            if (m_iTLB->contains(virtualAddress, asn))
            {
                m_iTLB->invalidateAddress(virtualAddress, asn);
                entriesInvalidated++;
            }
        }
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_tlbOperations++;
    }

    emit tlbOperationCompleted("InvalidateEntry", entriesInvalidated);

    // Coordinate with barrier executor for memory ordering
    if (m_barrierExecutor && entriesInvalidated > 0)
    {
        m_barrierExecutor->notifyMemoryOperation(false); // TLB invalidation is a read-like operation
        m_barrierExecutor->notifyMemoryOperationComplete(false);
    }
}

void executorAlphaPAL::invalidateTLBByASN(quint64 asn)
{
    qDebug() << "Invalidating TLB entries for ASN:" << asn;

    int entriesInvalidated = 0;

    if (m_dTLB)
    {
        auto statsBefore = m_dTLB->getStatistics();
        m_dTLB->invalidateASN(asn);
        auto statsAfter = m_dTLB->getStatistics();
        entriesInvalidated += (statsAfter.invalidations - statsBefore.invalidations);
    }

    if (m_iTLB)
    {
        auto statsBefore = m_iTLB->getStatistics();
        m_iTLB->invalidateASN(asn);
        auto statsAfter = m_iTLB->getStatistics();
        entriesInvalidated += (statsAfter.invalidations - statsBefore.invalidations);
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_tlbOperations++;
    }

    emit tlbOperationCompleted("InvalidateASN", entriesInvalidated);

    // Coordinate with barrier executor
    if (m_barrierExecutor && entriesInvalidated > 0)
    {
        m_barrierExecutor->notifyMemoryOperation(false);
        m_barrierExecutor->notifyMemoryOperationComplete(false);
    }

    // For SMP systems, broadcast TLB invalidation to other CPUs
    broadcastTLBInvalidation("ASN", asn, 0);
}

bool executorAlphaPAL::handleFloatingPointException(const FPException &fpEx, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    qDebug() << "PAL Executor: Handling FP Exception -" << fpEx.message();

    // Handle floating-point exceptions
    // You might want to set specific FP exception flags in FPCR or other registers

    // Convert FP exception to appropriate system exception
    quint64 excSum = 0;

    switch (fpEx.getTrapType())
    {
    case FPTrapType::FP_DIVISION_BY_ZERO:
        // Could map to arithmetic exception
        excSum = 0x0100; // Arithmetic exception code
        break;
    case FPTrapType::FP_OVERFLOW:
    case FPTrapType::FP_UNDERFLOW:
    case FPTrapType::FP_INEXACT:
    case FPTrapType::FP_INVALID_OPERATION:
        excSum = 0x0100; // Arithmetic exception code
        break;
    case FPTrapType::FP_DISABLED:
        excSum = 0x0040; // FP disabled exception
        break;
    default:
        excSum = 0x0100; // Default to arithmetic
        break;
    }

    // Set exception information
    if (m_cpu->m_iprs)
    {
        m_cpu->m_iprs->write(Ipr::EXC_SUM, excSum);
        m_cpu->m_iprs->write(Ipr::EXC_PC, fpEx.getPC());
    }

    // Trigger FP exception handling
    if (m_cpu)
    {
        m_cpu->raiseException(excSum, fpEx.getPC());
    }

    return false;
}


bool executorAlphaPAL::handleIllegalInstructionException(const IllegalInstructionException &illEx,
                                                         const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    qDebug() << "PAL Executor: Handling Illegal Instruction Exception at PC:" << Qt::hex << illEx.getProgramCounter();

    // Set illegal instruction exception
    quint64 excSum = 0x0020; // Illegal instruction exception code

    if (m_cpu->m_iprs)
    {
        m_cpu->m_iprs->write(Ipr::EXC_SUM, excSum);
        m_cpu->m_iprs->write(Ipr::EXC_PC, illEx.getProgramCounter());
    }

    if (m_cpu)
    {
        m_cpu->raiseException(excSum, illEx.getProgramCounter());
    }

    return false;
}

bool executorAlphaPAL::handleMemoryAccessException(const MemoryAccessException &memEx, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    qDebug() << "PAL Executor: Handling Memory Access Exception -" << memEx.message();

    // Convert memory exception to your EXC_SUM flags
    quint64 excSum = EXC_SUM_ACCESS_VIOLATION;

    // Add read/write flag
    if (!memEx.isWrite())
    {
        excSum |= EXC_SUM_FAULT_ON_READ;
    }

    // Check for alignment based on exception type
    switch (memEx.getType())
    {
    case MemoryFaultType::ALIGNMENT_FAULT:
        excSum |= EXC_SUM_ALIGNMENT_FAULT;
        break;
    case MemoryFaultType::PROTECTION_VIOLATION:
        // Already have ACCESS_VIOLATION
        break;
    case MemoryFaultType::PRIVILEGE_VIOLATION:
        // Could add a privilege violation flag if you have one
        break;
    default:
        break;
    }

    // Set exception information
    if (m_cpu->m_iprs)
    {
        m_cpu->m_iprs->write(Ipr::EXC_SUM, excSum);
        m_cpu->m_iprs->write(Ipr::EXC_ADDR, memEx.getAddress());
        m_cpu->m_iprs->write(Ipr::EXC_PC, memEx.getPC());
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_privilegeViolations++;
    }

    emit privilegeViolation(instr.function, instr.pc);
    return false;
}

bool executorAlphaPAL::handleMemoryFault(quint64 faultingAddress, bool isWrite, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    // Determine fault type by checking various conditions
    bool isTranslationFault = false;
    bool isAlignmentFault = false;

    // Check for alignment fault (address not properly aligned)
    if ((faultingAddress & 0x7) != 0)
    { // Check 8-byte alignment for quadword access
        isAlignmentFault = true;
    }

    // Check for translation fault via TLB lookup
    isTranslationFault = checkForTLBMiss(faultingAddress, isWrite);

    // Raise memory exception with your EXC_SUM constants
    m_cpu->raiseMemoryException(faultingAddress, isWrite, isTranslationFault, isAlignmentFault);

    // Update PAL executor statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_privilegeViolations++;
    }

    // Emit fault signal
    emit privilegeViolation(instr.function, instr.pc);

    return false; // Fault occurred
}

bool executorAlphaPAL::handleMemoryFaultSimple(quint64 faultingAddress, bool isWrite, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    // Simple fault categorization based on address and context
    bool isTranslationFault = true; // Default assumption
    bool isAlignmentFault = false;

    // Check for alignment fault
    if ((faultingAddress & 0x7) != 0)
    {
        isAlignmentFault = true;
        // If misaligned, it's primarily an alignment fault, not translation
        isTranslationFault = false;
    }

    // Additional heuristics based on address range
    if (faultingAddress < 0x1000)
    {
        // Very low addresses are usually null pointer dereferences
        isTranslationFault = true;
        isAlignmentFault = false;
    }
    else if (faultingAddress >= 0xFFFFFFFF80000000ULL)
    {
        // Kernel space - might be translation or permission issue
        isTranslationFault = true;
    }

    // Raise memory exception with your EXC_SUM constants
    m_cpu->raiseMemoryException(faultingAddress, isWrite, isTranslationFault, isAlignmentFault);

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_privilegeViolations++;
    }

    emit privilegeViolation(instr.function, instr.pc);
    return false;
}
void executorAlphaPAL::invalidateAllTLB()
{
    qDebug() << "Invalidating all TLB entries";

    int entriesInvalidated = 0;

    if (m_dTLB)
    {
        entriesInvalidated += m_dTLB->getTotalEntries();
        m_dTLB->invalidateAll();
    }

    if (m_iTLB)
    {
        entriesInvalidated += m_iTLB->getTotalEntries();
        m_iTLB->invalidateAll();
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_tlbOperations++;
    }

    emit tlbOperationCompleted("InvalidateAll", entriesInvalidated);

    // Coordinate with barrier executor - this is a major operation
    if (m_barrierExecutor)
    {
        m_barrierExecutor->notifyMemoryOperation(false);
        m_barrierExecutor->notifyMemoryOperationComplete(false);
    }

    // For SMP systems, broadcast to all CPUs
    broadcastTLBInvalidation("ALL", 0, 0);

    // Flush any cached translations in the CPU
    if (m_cpu)
    {
        m_cpu->flushTLBCache();
    }
}

void executorAlphaPAL::flushInstructionTLB()
{
    qDebug() << "Flushing Instruction TLB";

    int entriesInvalidated = 0;

    if (m_iTLB)
    {
        entriesInvalidated = m_iTLB->getTotalEntries();
        m_iTLB->invalidateAll();
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_tlbOperations++;
    }

    emit tlbOperationCompleted("FlushITLB", entriesInvalidated);

    // Coordinate with barrier executor
    if (m_barrierExecutor && entriesInvalidated > 0)
    {
        m_barrierExecutor->notifyMemoryOperation(false);
        m_barrierExecutor->notifyMemoryOperationComplete(false);
    }

    // Flush instruction cache as well since TLB and cache are related
    flushL1Cache(true, false); // Instruction cache only

    // For SMP systems, broadcast instruction TLB flush
    broadcastTLBInvalidation("ITLB", 0, 0);

    // Coordinate with other execution units that might have cached instruction translations
    if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive())
    {
        // Signal that instruction addresses may have changed
        qDebug() << "Notifying FP executor of ITLB flush";
    }

    if (m_intExecutor && m_intExecutor->isAsyncPipelineActive())
    {
        // Signal that instruction addresses may have changed
        qDebug() << "Notifying Integer executor of ITLB flush";
    }
}

void executorAlphaPAL::flushDataTLB()
{
    qDebug() << "Flushing Data TLB";

    int entriesInvalidated = 0;

    if (m_dTLB)
    {
        entriesInvalidated = m_dTLB->getTotalEntries();
        m_dTLB->invalidateAll();
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_tlbOperations++;
    }

    emit tlbOperationCompleted("FlushDTLB", entriesInvalidated);

    // Coordinate with barrier executor
    if (m_barrierExecutor && entriesInvalidated > 0)
    {
        m_barrierExecutor->notifyMemoryOperation(false);
        m_barrierExecutor->notifyMemoryOperationComplete(false);
    }

    // Flush data cache as well since TLB and cache are related
    flushL1Cache(false, true); // Data cache only

    // For SMP systems, broadcast data TLB flush
    broadcastTLBInvalidation("DTLB", 0, 0);

    // Coordinate with other execution units that access memory
    if (m_fpExecutor && m_fpExecutor->isAsyncPipelineActive())
    {
        // FP operations that access memory need to know about TLB changes
        qDebug() << "Notifying FP executor of DTLB flush";
    }

    if (m_intExecutor && m_intExecutor->isAsyncPipelineActive())
    {
        // Integer operations that access memory need to know
        qDebug() << "Notifying Integer executor of DTLB flush";
    }
}

// Helper method for SMP TLB coordination
// Enhanced invalidation with performance optimization
void executorAlphaPAL::broadcastTLBInvalidation(const QString &type, quint64 asn, quint64 virtualAddress)
{
    // This would interface with the SMP manager to coordinate TLB invalidations
    // across multiple CPUs in an SMP system

    qDebug() << QString("Broadcasting TLB invalidation: Type=%1, ASN=%2, VA=0x%3")
                    .arg(type)
                    .arg(asn)
                    .arg(virtualAddress, 0, 16);

    // In a real SMP system, this would:
    // 1. Send inter-processor interrupts to other CPUs
    // 2. Wait for acknowledgments
    // 3. Ensure cache coherency protocols are followed

    // For now, just emit a signal that the SMP manager can listen to
    emit tlbOperationCompleted(QString("Broadcast_%1").arg(type), 1);
}

bool executorAlphaPAL::handleTLBException(const TLBExceptionQ &tlbEx, const PALInstruction &instr)
{
    if (!m_cpu)
        return false;

    qDebug() << "PAL Executor: Handling TLB Exception -" << tlbEx.message();

    // Convert TLB exception to your EXC_SUM flags
    quint64 excSum = EXC_SUM_ACCESS_VIOLATION;

    switch (tlbEx.getType())
    {
    case excTLBException::TRANSLATION_NOT_VALID:
        excSum |= EXC_SUM_TRANS_NOT_VALID;
        break;
    case excTLBException::ALIGNMENT_FAULT:
        excSum |= EXC_SUM_ALIGNMENT_FAULT;
        break;
    case excTLBException::TLB_MISS:
        excSum |= EXC_SUM_TRANS_NOT_VALID;
        break;
    case excTLBException::ACCESS_VIOLATION:
        // Already set ACCESS_VIOLATION above
        break;
    default:
        // Other TLB exceptions
        break;
    }

    // Set exception information in your IPR system
    if (m_cpu->m_iprs)
    {
        m_cpu->m_iprs->write(Ipr::EXC_SUM, excSum);
        m_cpu->m_iprs->write(Ipr::EXC_ADDR, tlbEx.getVirtualAddress());
        m_cpu->m_iprs->write(Ipr::EXC_PC, tlbEx.getProgramCounter());
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_privilegeViolations++;
    }

    // Emit signals
    emit privilegeViolation(instr.function, instr.pc);

    return false;
}


void executorAlphaPAL::invalidateTLBOptimized(quint64 virtualAddress, quint64 asn, bool isInstruction)
{
    qDebug() << QString("Optimized TLB invalidation: VA=0x%1, ASN=%2, Instruction=%3")
                    .arg(virtualAddress, 0, 16)
                    .arg(asn)
                    .arg(isInstruction);

    int entriesInvalidated = 0;

    if (isInstruction && m_iTLB)
    {
        if (m_iTLB->contains(virtualAddress, asn))
        {
            m_iTLB->invalidateAddress(virtualAddress, asn);
            entriesInvalidated++;
        }
    }
    else if (!isInstruction && m_dTLB)
    {
        if (m_dTLB->contains(virtualAddress, asn))
        {
            m_dTLB->invalidateAddress(virtualAddress, asn);
            entriesInvalidated++;
        }
    }

    // Only perform expensive operations if we actually invalidated something
    if (entriesInvalidated > 0)
    {
        // Update statistics
        {
            QMutexLocker locker(&m_statsMutex);
            m_tlbOperations++;
        }

        emit tlbOperationCompleted("OptimizedInvalidate", entriesInvalidated);

        // Selective cache flushing based on what was invalidated
        if (isInstruction)
        {
            // Only flush instruction cache for instruction TLB invalidations
            flushL1Cache(true, false);
        }
        else
        {
            // Only flush data cache for data TLB invalidations
            flushL1Cache(false, true);
        }

        // Coordinate with barrier executor
        if (m_barrierExecutor)
        {
            m_barrierExecutor->notifyMemoryOperation(false);
            m_barrierExecutor->notifyMemoryOperationComplete(false);
        }

        // Broadcast only if necessary
        broadcastTLBInvalidation(isInstruction ? "ITLB_Entry" : "DTLB_Entry", asn, virtualAddress);
    }
}

// Batch TLB invalidation for efficiency
void executorAlphaPAL::invalidateTLBBatch(const QVector<quint64> &virtualAddresses, quint64 asn)
{
    qDebug() << QString("Batch TLB invalidation: %1 addresses, ASN=%2").arg(virtualAddresses.size()).arg(asn);

    int totalInvalidated = 0;

    // Process in batches to minimize lock contention
    const int batchSize = 16;
    for (int i = 0; i < virtualAddresses.size(); i += batchSize)
    {
        int endIdx = qMin(i + batchSize, virtualAddresses.size());

        // Invalidate batch in data TLB
        if (m_dTLB)
        {
            for (int j = i; j < endIdx; j++)
            {
                if (m_dTLB->contains(virtualAddresses[j], asn))
                {
                    m_dTLB->invalidateAddress(virtualAddresses[j], asn);
                    totalInvalidated++;
                }
            }
        }

        // Invalidate batch in instruction TLB
        if (m_iTLB)
        {
            for (int j = i; j < endIdx; j++)
            {
                if (m_iTLB->contains(virtualAddresses[j], asn))
                {
                    m_iTLB->invalidateAddress(virtualAddresses[j], asn);
                    totalInvalidated++;
                }
            }
        }
    }

    if (totalInvalidated > 0)
    {
        // Update statistics
        {
            QMutexLocker locker(&m_statsMutex);
            m_tlbOperations++;
        }

        emit tlbOperationCompleted("BatchInvalidate", totalInvalidated);

        // Coordinate with barrier executor
        if (m_barrierExecutor)
        {
            m_barrierExecutor->notifyMemoryOperation(false);
            m_barrierExecutor->notifyMemoryOperationComplete(false);
        }

        // Broadcast batch invalidation
        broadcastTLBInvalidation("BATCH", asn, virtualAddresses.size());
    }
}


// Cache Invalidation Methods Implementation for executorAlphaPAL

void executorAlphaPAL::invalidateCacheLine(quint64 address)
{
    qDebug() << "Invalidating cache line for address:" << Qt::hex << address;

    const quint64 cacheLineSize = 64;
    quint64 cacheLineAddr = address & ~(cacheLineSize - 1);

    int linesInvalidated = 0;

    // Enhanced instruction cache invalidation
    if (m_instructionCache)
    {
        if (m_instructionCache->invalidateLine(cacheLineAddr, true))
        {
            linesInvalidated++;
            DEBUG_LOG(QString("Invalidated I-cache line at:%1")
                .arg(QString::number(cacheLineAddr,16)));
        }
    }

    // Invalidate in data caches
    if (m_level1DataCache)
    {
        if (m_level1DataCache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
    }

    if (m_level2Cache)
    {
        if (m_level2Cache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
    }

    if (m_level3Cache)
    {
        if (m_level3Cache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
    }

    // Update statistics
    {
        QMutexLocker locker(&m_statsMutex);
        m_cacheFlushes++;
    }

    emit cacheFlushRequested(QString("InvalidateLine_0x%1").arg(cacheLineAddr, 0, 16));

    // Coordinate with barrier executor
    if (m_barrierExecutor && linesInvalidated > 0)
    {
        m_barrierExecutor->notifyMemoryOperation(true);
        m_barrierExecutor->notifyMemoryOperationComplete(true);
    }

    // For SMP systems, broadcast cache line invalidation
    broadcastCacheInvalidation(cacheLineAddr);

    qDebug() << QString("Cache line invalidation complete: %1 lines invalidated").arg(linesInvalidated);
}

void executorAlphaPAL::broadcastCacheInvalidation(quint64 address)
{
    qDebug() << "Broadcasting cache invalidation for address:" << Qt::hex << address;

    // Calculate cache line address for consistency
    const quint64 cacheLineSize = 64;
    quint64 cacheLineAddr = address & ~(cacheLineSize - 1);

    // In a real SMP Alpha system, this would implement the MESI (Modified, Exclusive, Shared, Invalid)
    // cache coherency protocol or similar coherency mechanism

    // Steps for SMP cache coherency:
    // 1. Send inter-processor interrupt to all other CPUs
    // 2. Each CPU invalidates the cache line if present
    // 3. Wait for acknowledgments from all CPUs
    // 4. Ensure memory ordering is maintained

    DEBUG_LOG( QString("SMP Cache Invalidation Broadcast: Line=0x%1").arg(cacheLineAddr, 0, 16));

    // Simulate MESI protocol states
    QString coherencyAction = determineCacheCoherencyAction(cacheLineAddr);
    DEBUG_LOG( QString("Cache coherency action: %1").arg(coherencyAction));

    // For SMP coordination, this would interface with the AlphaSMPManager
    // to send cache invalidation messages to other CPUs
    if (m_cpu)
    {
        // Get SMP manager reference and broadcast
        // smpManager->broadcastCacheInvalidation(cacheLineAddr, m_cpu->getCpuId());
        DEBUG_LOG( QString("Broadcasting cache invalidation from CPU %1").arg(m_cpu->getCpuId()));
    }

    // Emit signal for SMP manager to handle
    emit cacheFlushRequested(QString("SMP_Invalidate_0x%1").arg(cacheLineAddr, 0, 16));

    // Simulate inter-processor communication delay
    // In real hardware, this would be the time for:
    // - IPI transmission
    // - Remote cache lookup
    // - Invalidation acknowledgment
    // - Memory barrier completion
    QThread::msleep(1); // Simulate minimal IPI latency

    // Update statistics for SMP operations
    {
        QMutexLocker locker(&m_statsMutex);
        // Could add SMP-specific counters here
        // m_smpCacheInvalidations++;
    }

    DEBUG_LOG( "SMP cache invalidation broadcast complete");
}

// Helper method to determine cache coherency protocol action
QString executorAlphaPAL::determineCacheCoherencyAction(quint64 cacheLineAddr)
{
    // Simulate MESI protocol decision making
    // In a real implementation, this would check:
    // - Current cache line state (Modified, Exclusive, Shared, Invalid)
    // - Ownership information
    // - Pending transactions

    // For demonstration, simulate different coherency actions
    quint64 lineState = cacheLineAddr & 0x3; // Use low bits to simulate state

    switch (lineState)
    {
    case 0:                                // Modified
        return "Writeback_and_Invalidate"; // Line is dirty, must write back
    case 1:                                // Exclusive
        return "Invalidate";               // Line is clean but exclusive, just invalidate
    case 2:                                // Shared
        return "Shared_Invalidate";        // Broadcast invalidation to all sharers
    case 3:                                // Invalid
    default:
        return "No_Action"; // Line already invalid
    }
}


// Advanced cache invalidation with range support
void executorAlphaPAL::invalidateCacheRange(quint64 startAddress, quint64 endAddress)
{
    qDebug() << QString("Invalidating cache range: 0x%1 - 0x%2").arg(startAddress, 0, 16).arg(endAddress, 0, 16);

    const quint64 cacheLineSize = 64;
    quint64 startLine = startAddress & ~(cacheLineSize - 1);
    quint64 endLine = (endAddress + cacheLineSize - 1) & ~(cacheLineSize - 1);

    int totalLinesInvalidated = 0;

    // Invalidate all cache lines in the range
    for (quint64 addr = startLine; addr < endLine; addr += cacheLineSize)
    {
        invalidateCacheLine(addr);
        totalLinesInvalidated++;

        // Prevent excessive IPI traffic by batching
        if (totalLinesInvalidated % 16 == 0)
        {
            QThread::msleep(1); // Small delay to prevent IPI flooding
        }
    }

    qDebug() << QString("Cache range invalidation complete: %1 lines").arg(totalLinesInvalidated);
}

// Selective cache invalidation based on cache level
void executorAlphaPAL::invalidateCacheLineSelective(quint64 address, bool l1Only, bool l2Only, bool l3Only)
{
    qDebug() << QString("Selective cache invalidation for address: 0x%1 (L1=%2, L2=%3, L3=%4)")
                    .arg(address, 0, 16)
                    .arg(l1Only)
                    .arg(l2Only)
                    .arg(l3Only);

    const quint64 cacheLineSize = 64;
    quint64 cacheLineAddr = address & ~(cacheLineSize - 1);

    int linesInvalidated = 0;

    // L1 caches
    if (l1Only || (!l2Only && !l3Only))
    {
        if (m_instructionCache && m_instructionCache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
        if (m_level1DataCache && m_level1DataCache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
    }

    // L2 cache
    if (l2Only || (!l1Only && !l3Only))
    {
        if (m_level2Cache && m_level2Cache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
    }

    // L3 cache
    if (l3Only || (!l1Only && !l2Only))
    {
        if (m_level3Cache && m_level3Cache->invalidateLine(cacheLineAddr))
        {
            linesInvalidated++;
        }
    }

    if (linesInvalidated > 0)
    {
        // Update statistics
        {
            QMutexLocker locker(&m_statsMutex);
            m_cacheFlushes++;
        }

        // Broadcast if necessary
        broadcastCacheInvalidation(cacheLineAddr);
    }

    qDebug() << QString("Selective invalidation complete: %1 lines").arg(linesInvalidated);
}
// Add these missing implementations to executorAlphaPAL.cpp

#if defined(ALPHA_BUILD)

bool executorAlphaPAL::executeAlpha_SSIR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SSIR executed at PC:" << Qt::hex << instr.pc;

    // Set Software Interrupt Request (same as Tru64)
    return executeTru64_SSIR(instr);
}

bool executorAlphaPAL::executeAlpha_CSIR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_CSIR executed at PC:" << Qt::hex << instr.pc;

    // Clear Software Interrupt Request
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 sirMask = 0;

    if (!readIntegerRegisterWithCache(ra, sirMask))
    {
        return false;
    }

    // Clear specified software interrupt bits
    quint64 currentSIRR = 0;
    if (readIPRWithCache("SIRR", currentSIRR))
    {
        currentSIRR &= ~sirMask;
        writeIPRWithCache("SIRR", currentSIRR);
    }

    return true;
}

bool executorAlphaPAL::executeAlpha_RFE(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RFE executed at PC:" << Qt::hex << instr.pc;

    // Return From Exception (same as REI)
    return executeREI(instr);
}

bool executorAlphaPAL::executeAlpha_RETSYS(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RETSYS executed at PC:" << Qt::hex << instr.pc;

    // Return from system call (same as Tru64)
    return executeTru64_RETSYS(instr);
}

bool executorAlphaPAL::executeAlpha_RESTART(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RESTART executed at PC:" << Qt::hex << instr.pc;

    // Restart processor
    if (m_cpu)
    {
        // Reset processor state
        m_cpu->initializeSMP();

        // Jump to restart vector
        quint64 restartVector = 0;
        if (readIPRWithCache("RESTART_VECTOR", restartVector))
        {
            m_cpu->setPC(restartVector);
        }
    }

    return true;
}

bool executorAlphaPAL::executeAlpha_SWPPROCESS(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SWPPROCESS executed at PC:" << Qt::hex << instr.pc;

    // Swap process context
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newProcess = 0;

    if (!readIntegerRegisterWithCache(ra, newProcess))
    {
        return false;
    }

    quint64 oldProcess = 0;
    if (readIPRWithCache("PROCESS", oldProcess))
    {
        writeIPRWithCache("PROCESS", newProcess);

        // Invalidate TLB for process switch
        if (m_cpu)
        {
            m_cpu->invalidateTBAllProcess();
        }

        const_cast<PALInstruction &>(instr).result = oldProcess;
        return true;
    }

    return false;
}

bool executorAlphaPAL::executeAlpha_RDMCES(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDMCES executed at PC:" << Qt::hex << instr.pc;

    // Same as MFPR_MCES
    return executeAlpha_MFPR_MCES(instr);
}

bool executorAlphaPAL::executeAlpha_WRMCES(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_WRMCES executed at PC:" << Qt::hex << instr.pc;

    // Same as MTPR_MCES
    return executeAlpha_MTPR_MCES(instr);
}

bool executorAlphaPAL::executeAlpha_TBIA(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_TBIA executed at PC:" << Qt::hex << instr.pc;

    // Same as common TBIA
    return executeMTPR_TBIA(instr);
}

bool executorAlphaPAL::executeAlpha_TBIS(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_TBIS executed at PC:" << Qt::hex << instr.pc;

    // Same as common TBIS
    return executeMTPR_TBIS(instr);
}

bool executorAlphaPAL::executeAlpha_TBISASN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_TBISASN executed at PC:" << Qt::hex << instr.pc;

    // Same as Tru64 TBISASN
    return executeTru64_TBISASN(instr);
}

bool executorAlphaPAL::executeAlpha_RDKSP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDKSP executed at PC:" << Qt::hex << instr.pc;

    // Same as Tru64 RDKSP
    return executeTru64_RDKSP(instr);
}

bool executorAlphaPAL::executeAlpha_SWPKSP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SWPKSP executed at PC:" << Qt::hex << instr.pc;

    // Same as Tru64 SWPKSP
    return executeTru64_SWPKSP(instr);
}

bool executorAlphaPAL::executeAlpha_RDPSR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDPSR executed at PC:" << Qt::hex << instr.pc;

    // Same as common RDPS
    return executeRDPS(instr);
}

bool executorAlphaPAL::executeAlpha_REBOOT(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_REBOOT executed at PC:" << Qt::hex << instr.pc;

    // Same as Tru64 REBOOT
    return executeTru64_REBOOT(instr);
}

bool executorAlphaPAL::executeAlpha_CHMK(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_CHMK executed at PC:" << Qt::hex << instr.pc;

    // Change mode to kernel
    if (m_cpu)
    {
        m_cpu->setPrivilegeMode(0); // Kernel mode
    }

    emit systemCallInvoked(FUNC_Alpha_CHMK_, instr.pc);
    return true;
}

bool executorAlphaPAL::executeAlpha_CALLKD(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_CALLKD executed at PC:" << Qt::hex << instr.pc;

    // Call kernel debugger
    if (m_cpu)
    {
        // Save current state
        quint64 currentPC = m_cpu->getPC();
        quint64 currentPS = 0;
        readIPRWithCache("PS", currentPS);

        writeIPRWithCache("EXC_PC", currentPC);
        writeIPRWithCache("EXC_PS", currentPS);

        // Vector to debugger
        quint64 debuggerVector = 0;
        if (readIPRWithCache("DEBUGGER_VECTOR", debuggerVector))
        {
            m_cpu->setPC(debuggerVector);
        }
    }

    return true;
}

bool executorAlphaPAL::executeAlpha_GENTRAP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_GENTRAP executed at PC:" << Qt::hex << instr.pc;

    // Generate trap
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 trapCode = 0;

    if (!readIntegerRegisterWithCache(ra, trapCode))
    {
        return false;
    }

    if (m_cpu)
    {
        m_cpu->raiseException(0x0100 | (trapCode & 0xFF), instr.pc);
    }

    return true;
}

bool executorAlphaPAL::executeAlpha_KBPT(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_KBPT executed at PC:" << Qt::hex << instr.pc;

    // Kernel breakpoint (same as BPT but in kernel mode)
    if (m_cpu)
    {
        m_cpu->raiseException(0x0080, instr.pc); // Breakpoint exception
    }

    return true;
}

// Add these missing implementations to executorAlphaPAL.cpp

// Missing Alpha-specific PAL function implementations


bool executorAlphaPAL::executeAlpha_MFPR_ASN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_ASN executed at PC:" << Qt::hex << instr.pc;

    // Read Address Space Number register
    quint64 asnValue = 0;
    if (!readIPRWithCache("ASN", asnValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = asnValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_ASTEN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ASTEN executed at PC:" << Qt::hex << instr.pc;

    // Write AST Enable register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 astenValue = 0;

    if (!readIntegerRegisterWithCache(ra, astenValue))
    {
        return false;
    }

    return writeIPRWithCache("ASTEN", astenValue);
}

bool executorAlphaPAL::executeAlpha_MTPR_ASTSR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ASTSR executed at PC:" << Qt::hex << instr.pc;

    // Write AST Summary register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 astsrValue = 0;

    if (!readIntegerRegisterWithCache(ra, astsrValue))
    {
        return false;
    }

    return writeIPRWithCache("ASTSR", astsrValue);
}

// Add these missing implementations to executorAlphaPAL.cpp

// Missing Alpha-specific PAL function implementations

bool executorAlphaPAL::executeAlpha_LDQP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_LDQP executed at PC:" << Qt::hex << instr.pc;

    try
    {
        quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
        quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

        quint64 physicalAddr = 0;
        if (!readIntegerRegisterWithCache(rb, physicalAddr))
        {
            return false;
        }

        // This might throw TLBExceptionQ or MemoryAccessException
        quint64 value = 0;
        if (m_cpu && m_cpu->readPhysicalMemory(physicalAddr, value))
        {
            const_cast<PALInstruction &>(instr).result = value;
            const_cast<PALInstruction &>(instr).writeResult = true;
            const_cast<PALInstruction &>(instr).targetRegister = ra;
            return true;
        }

        return false;
    }
    catch (const TLBExceptionQ &tlbEx)
    {
        return handleTLBException(tlbEx, instr);
    }
    catch (const MemoryAccessException &memEx)
    {
        return handleMemoryAccessException(memEx, instr);
    }
}



bool executorAlphaPAL::executeAlpha_MFPR_ASN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_ASN executed at PC:" << Qt::hex << instr.pc;

    // Read Address Space Number register
    quint64 asnValue = 0;
    if (!readIPRWithCache("ASN", asnValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = asnValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_ASTEN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ASTEN executed at PC:" << Qt::hex << instr.pc;

    // Write AST Enable register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 astenValue = 0;

    if (!readIntegerRegisterWithCache(ra, astenValue))
    {
        return false;
    }

    return writeIPRWithCache("ASTEN", astenValue);
}

bool executorAlphaPAL::executeAlpha_MTPR_ASTSR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ASTSR executed at PC:" << Qt::hex << instr.pc;

    // Write AST Summary register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 astsrValue = 0;

    if (!readIntegerRegisterWithCache(ra, astsrValue))
    {
        return false;
    }

    return writeIPRWithCache("ASTSR", astsrValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_MCES(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_MCES executed at PC:" << Qt::hex << instr.pc;

    // Read Machine Check Error Summary register
    quint64 mcesValue = 0;
    if (!readIPRWithCache("MCES", mcesValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = mcesValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_MCES(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_MCES executed at PC:" << Qt::hex << instr.pc;

    // Write Machine Check Error Summary register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 mcesValue = 0;

    if (!readIntegerRegisterWithCache(ra, mcesValue))
    {
        return false;
    }

    return writeIPRWithCache("MCES", mcesValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_PCBB(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_PCBB executed at PC:" << Qt::hex << instr.pc;

    // Read Process Control Block Base register
    quint64 pcbbValue = 0;
    if (!readIPRWithCache("PCBB", pcbbValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = pcbbValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MFPR_PRBR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_PRBR executed at PC:" << Qt::hex << instr.pc;

    // Read Processor Base Register
    quint64 prbrValue = 0;
    if (!readIPRWithCache("PRBR", prbrValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = prbrValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_PRBR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_PRBR executed at PC:" << Qt::hex << instr.pc;

    // Write Processor Base Register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 prbrValue = 0;

    if (!readIntegerRegisterWithCache(ra, prbrValue))
    {
        return false;
    }

    return writeIPRWithCache("PRBR", prbrValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_PTBR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_PTBR executed at PC:" << Qt::hex << instr.pc;

    // Read Page Table Base Register
    quint64 ptbrValue = 0;
    if (!readIPRWithCache("PTBR", ptbrValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = ptbrValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_SCBB(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_SCBB executed at PC:" << Qt::hex << instr.pc;

    // Write System Control Block Base register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 scbbValue = 0;

    if (!readIntegerRegisterWithCache(ra, scbbValue))
    {
        return false;
    }

    return writeIPRWithCache("SCBB", scbbValue);
}

bool executorAlphaPAL::executeAlpha_MTPR_SIRR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_SIRR executed at PC:" << Qt::hex << instr.pc;

    // Write Software Interrupt Request Register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 sirrValue = 0;

    if (!readIntegerRegisterWithCache(ra, sirrValue))
    {
        return false;
    }

    // Trigger software interrupt based on value
    if (m_cpu && sirrValue != 0)
    {
        m_cpu->triggerSoftwareInterrupt(sirrValue);
    }

    return writeIPRWithCache("SIRR", sirrValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_SISR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_SISR executed at PC:" << Qt::hex << instr.pc;

    // Read Software Interrupt Summary Register
    quint64 sisrValue = 0;
    if (!readIPRWithCache("SISR", sisrValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = sisrValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MFPR_SSP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_SSP executed at PC:" << Qt::hex << instr.pc;

    // Read System Stack Pointer
    quint64 sspValue = 0;
    if (!readIPRWithCache("SSP", sspValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = sspValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_SSP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_SSP executed at PC:" << Qt::hex << instr.pc;

    // Write System Stack Pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 sspValue = 0;

    if (!readIntegerRegisterWithCache(ra, sspValue))
    {
        return false;
    }

    return writeIPRWithCache("SSP", sspValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_USP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_USP executed at PC:" << Qt::hex << instr.pc;

    // Read User Stack Pointer
    quint64 uspValue = 0;
    if (!readIPRWithCache("USP", uspValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = uspValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_USP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_USP executed at PC:" << Qt::hex << instr.pc;

    // Write User Stack Pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 uspValue = 0;

    if (!readIntegerRegisterWithCache(ra, uspValue))
    {
        return false;
    }

    return writeIPRWithCache("USP", uspValue);
}

bool executorAlphaPAL::executeAlpha_MTPR_IPIR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_IPIR executed at PC:" << Qt::hex << instr.pc;

    // Write Inter-Processor Interrupt Request register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 ipirValue = 0;

    if (!readIntegerRegisterWithCache(ra, ipirValue))
    {
        return false;
    }

    // Send IPI to specified CPU
    if (m_cpu && ipirValue != 0)
    {
        quint16 targetCpu = (ipirValue >> 8) & 0xFF;
        int vector = ipirValue & 0xFF;
        m_cpu->sendIPI(targetCpu, vector);
    }

    return writeIPRWithCache("IPIR", ipirValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_IPL(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_IPL executed at PC:" << Qt::hex << instr.pc;

    // Read Interrupt Priority Level
    quint64 iplValue = 0;
    if (!readIPRWithCache("IPL", iplValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = iplValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_IPL(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_IPL executed at PC:" << Qt::hex << instr.pc;

    // Write Interrupt Priority Level
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 iplValue = 0;

    if (!readIntegerRegisterWithCache(ra, iplValue))
    {
        return false;
    }

    return writeIPRWithCache("IPL", iplValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_TBCHK(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_TBCHK executed at PC:" << Qt::hex << instr.pc;

    // Read Translation Buffer Check register
    quint64 tbchkValue = 0;
    if (!readIPRWithCache("TBCHK", tbchkValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = tbchkValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_TBIAP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_TBIAP executed at PC:" << Qt::hex << instr.pc;

    // Translation Buffer Invalidate All Process
    if (m_cpu)
    {
        m_cpu->invalidateTBAllProcess();
    }

    // Also invalidate local TLBs
    if (m_dTLB && m_iTLB)
    {
        quint64 currentASN = m_cpu ? m_cpu->getCurrentASN() : 0;
        m_dTLB->invalidateASN(currentASN);
        m_iTLB->invalidateASN(currentASN);
    }

    emit tlbOperationCompleted("TBIAP", 1);
    return true;
}

bool executorAlphaPAL::executeAlpha_MFPR_ESP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_ESP executed at PC:" << Qt::hex << instr.pc;

    // Read Executive Stack Pointer
    quint64 espValue = 0;
    if (!readIPRWithCache("ESP", espValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = espValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_ESP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ESP executed at PC:" << Qt::hex << instr.pc;

    // Write Executive Stack Pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 espValue = 0;

    if (!readIntegerRegisterWithCache(ra, espValue))
    {
        return false;
    }

    return writeIPRWithCache("ESP", espValue);
}

bool executorAlphaPAL::executeAlpha_MTPR_PERFMON(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_PERFMON executed at PC:" << Qt::hex << instr.pc;

    // Write Performance Monitor register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 pmValue = 0;
    quint64 pmSelect = 0;

    if (!readIntegerRegisterWithCache(ra, pmValue) || !readIntegerRegisterWithCache(rb, pmSelect))
    {
        return false;
    }

    // Enable/configure performance monitoring
    if (m_cpu && m_cpu->hasPerformanceCounters())
    {
        m_cpu->setPerformanceCounter(pmSelect, pmValue);
    }

    QString iprName = QString("PERFMON_%1").arg(pmSelect);
    return writeIPRWithCache(iprName, pmValue);
}

bool executorAlphaPAL::executeAlpha_MFPR_WHAMI(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_WHAMI executed at PC:" << Qt::hex << instr.pc;

    // Read "Who Am I" register - returns CPU identification
    quint64 whamiValue = m_cpu ? m_cpu->readWHAMI() : 0;

    const_cast<PALInstruction &>(instr).result = whamiValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_READ_UNQ(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_READ_UNQ executed at PC:" << Qt::hex << instr.pc;

    // Read Unique register
    quint64 unqValue = 0;
    if (!readIPRWithCache("UNQ", unqValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = unqValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_WRITE_UNQ(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_WRITE_UNQ executed at PC:" << Qt::hex << instr.pc;

    // Write Unique register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 unqValue = 0;

    if (!readIntegerRegisterWithCache(ra, unqValue))
    {
        return false;
    }

    return writeIPRWithCache("UNQ", unqValue);
}

bool executorAlphaPAL::executeAlpha_INITPAL(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_INITPAL executed at PC:" << Qt::hex << instr.pc;

    // Initialize PAL code - system initialization
    if (m_cpu)
    {
        // Initialize CPU state for PAL operation
        m_cpu->initializeSMP();

        // Set up basic PAL environment
        writeIPRWithCache("PAL_BASE", m_cpu->getPALBase());
        writeIPRWithCache("PAL_MODE", 1); // Enable PAL mode
    }

    emit systemCallInvoked(FUNC_Alpha_INITPAL_, instr.pc);
    return true;
}

bool executorAlphaPAL::executeAlpha_WRENTRY(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_WRENTRY executed at PC:" << Qt::hex << instr.pc;

    // Write exception entry point
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 entryPoint = 0;
    quint64 entryType = 0;

    if (!readIntegerRegisterWithCache(ra, entryPoint) || !readIntegerRegisterWithCache(rb, entryType))
    {
        return false;
    }

    QString iprName = QString("ENTRY_%1").arg(entryType);
    return writeIPRWithCache(iprName, entryPoint);
}

bool executorAlphaPAL::executeAlpha_SWPIRQL(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SWPIRQL executed at PC:" << Qt::hex << instr.pc;

    // Swap Interrupt Request Level
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newIRQL = 0;

    if (!readIntegerRegisterWithCache(ra, newIRQL))
    {
        return false;
    }

    quint64 oldIRQL = 0;
    if (!readIPRWithCache("IRQL", oldIRQL))
    {
        return false;
    }

    writeIPRWithCache("IRQL", newIRQL);

    const_cast<PALInstruction &>(instr).result = oldIRQL;
    return true;
}

bool executorAlphaPAL::executeAlpha_RDIRQL(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDIRQL executed at PC:" << Qt::hex << instr.pc;

    // Read Interrupt Request Level
    quint64 irqlValue = 0;
    if (!readIPRWithCache("IRQL", irqlValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = irqlValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_DI(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_DI executed at PC:" << Qt::hex << instr.pc;

    // Disable Interrupts
    quint64 oldPS = 0;
    if (!readIPRWithCache("PS", oldPS))
    {
        return false;
    }

    // Clear interrupt enable bit
    quint64 newPS = oldPS & ~0x1; // Clear bit 0 (IE)
    writeIPRWithCache("PS", newPS);

    // Disable interrupts on CPU
    if (m_cpu)
    {
        m_cpu->disableInterrupts();
    }

    const_cast<PALInstruction &>(instr).result = oldPS;
    return true;
}

bool executorAlphaPAL::executeAlpha_EI(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_EI executed at PC:" << Qt::hex << instr.pc;

    // Enable Interrupts
    quint64 oldPS = 0;
    if (!readIPRWithCache("PS", oldPS))
    {
        return false;
    }

    // Set interrupt enable bit
    quint64 newPS = oldPS | 0x1; // Set bit 0 (IE)
    writeIPRWithCache("PS", newPS);

    // Enable interrupts on CPU
    if (m_cpu)
    {
        m_cpu->enableInterrupts();
    }

    const_cast<PALInstruction &>(instr).result = oldPS;
    return true;
}

bool executorAlphaPAL::executeAlpha_SWPPAL(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SWPPAL executed at PC:" << Qt::hex << instr.pc;

    // Switch PAL code base
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newPalBase = 0;

    if (!readIntegerRegisterWithCache(ra, newPalBase))
    {
        return false;
    }

    quint64 oldPalBase = 0;
    if (m_cpu)
    {
        oldPalBase = m_cpu->swppalSMP(newPalBase, true);
    }

    const_cast<PALInstruction &>(instr).result = oldPalBase;
    return true;
}

// Additional missing PAL function implementations

bool executorAlphaPAL::executeAlpha_SSIR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SSIR executed at PC:" << Qt::hex << instr.pc;

    // Set Software Interrupt Request (same as Tru64)
    return executeTru64_SSIR(instr);
}

bool executorAlphaPAL::executeAlpha_CSIR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_CSIR executed at PC:" << Qt::hex << instr.pc;

    // Clear Software Interrupt Request
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 sirMask = 0;

    if (!readIntegerRegisterWithCache(ra, sirMask))
    {
        return false;
    }

    // Clear specified software interrupt bits
    quint64 currentSIRR = 0;
    if (readIPRWithCache("SIRR", currentSIRR))
    {
        currentSIRR &= ~sirMask;
        writeIPRWithCache("SIRR", currentSIRR);
    }

    return true;
}

bool executorAlphaPAL::executeAlpha_RFE(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RFE executed at PC:" << Qt::hex << instr.pc;

    // Return From Exception (same as REI)
    return executeREI(instr);
}

bool executorAlphaPAL::executeAlpha_RETSYS(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RETSYS executed at PC:" << Qt::hex << instr.pc;

    // Return from system call (same as Tru64)
    return executeTru64_RETSYS(instr);
}

bool executorAlphaPAL::executeAlpha_RESTART(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RESTART executed at PC:" << Qt::hex << instr.pc;

    // Restart processor
    if (m_cpu)
    {
        // Reset processor state
        m_cpu->initializeSMP();

        // Jump to restart vector
        quint64 restartVector = 0;
        if (readIPRWithCache("RESTART_VECTOR", restartVector))
        {
            m_cpu->setPC(restartVector);
        }
    }

    return true;
}

bool executorAlphaPAL::executeAlpha_SWPPROCESS(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_SWPPROCESS executed at PC:" << Qt::hex << instr.pc;

    // Swap process context
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newProcess = 0;

    if (!readIntegerRegisterWithCache(ra, newProcess))
    {
        return false;
    }

    quint64 oldProcess = 0;
    if (readIPRWithCache("PROCESS", oldProcess))
    {
        writeIPRWithCache("PROCESS", newProcess);

        // Invalidate TLB for process switch
        if (m_cpu)
        {
            m_cpu->invalidateTBAllProcess();
        }

        const_cast<PALInstruction &>(instr).result = oldProcess;
        return true;
    }

    return false;
}

bool executorAlphaPAL::executeAlpha_RDMCES(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDMCES executed at PC:" << Qt::hex << instr.pc;

    // Same as MFPR_MCES
    return executeAlpha_MFPR_MCES(instr);
}









bool executorAlphaPAL::executeAlpha_RDKSP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDKSP executed at PC:" << Qt::hex << instr.pc;

    // Same as Tru64 RDKSP
    return executeTru64_RDKSP(instr);
}

bool executorAlphaPAL::executeAlpha_RDPSR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_RDPSR executed at PC:" << Qt::hex << instr.pc;

    // Same as common RDPS
    return executeRDPS(instr);
}

bool executorAlphaPAL::executeAlpha_REBOOT(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_REBOOT executed at PC:" << Qt::hex << instr.pc;

    // Same as Tru64 REBOOT
    return executeTru64_REBOOT(instr);
}


bool executorAlphaPAL::executeAlpha_STQP(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_STQP executed at PC:" << Qt::hex << instr.pc;

    // Store quadword physical - direct physical memory access
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;

    quint64 value = 0;
    quint64 physicalAddr = 0;

    if (!readIntegerRegisterWithCache(ra, value) || !readIntegerRegisterWithCache(rb, physicalAddr))
    {
        return false;
    }

    // Write to physical memory (bypass virtual translation)
    return m_cpu ? m_cpu->writePhysicalMemory(physicalAddr, value) : false;
}

bool executorAlphaPAL::executeAlpha_MFPR_ASN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MFPR_ASN executed at PC:" << Qt::hex << instr.pc;

    // Read Address Space Number register
    quint64 asnValue = 0;
    if (!readIPRWithCache("ASN", asnValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = asnValue;
    return true;
}

bool executorAlphaPAL::executeAlpha_MTPR_ASTEN(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ASTEN executed at PC:" << Qt::hex << instr.pc;

    // Write AST Enable register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 astenValue = 0;

    if (!readIntegerRegisterWithCache(ra, astenValue))
    {
        return false;
    }

    return writeIPRWithCache("ASTEN", astenValue);
}

bool executorAlphaPAL::executeAlpha_MTPR_ASTSR(const PALInstruction &instr)
{
    qDebug() << "PAL Alpha_MTPR_ASTSR executed at PC:" << Qt::hex << instr.pc;

    // Write AST Summary register
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 astsrValue = 0;

    if (!readIntegerRegisterWithCache(ra, astsrValue))
    {
        return false;
    }

    return writeIPRWithCache("ASTSR", astsrValue);
}

// Add these Tru64-specific PAL function implementations to executorAlphaPAL.cpp


#endif

#if defined(TRU64_BUILD)

bool executorAlphaPAL::executeTru64_REBOOT(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_REBOOT executed at PC:" << Qt::hex << instr.pc;

    // System reboot - halt all CPUs and reset
    if (m_cpu)
    {
        m_cpu->sendIPIBroadcast(0xFF); // Broadcast halt to all CPUs
        m_cpu->halt();
    }

    emit systemCallInvoked(FUNC_Tru64_REBOOT, instr.pc);
    return true;
}

bool executorAlphaPAL::executeTru64_INITPAL(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_INITPAL executed at PC:" << Qt::hex << instr.pc;

    // Initialize PAL for Tru64 UNIX
    if (m_cpu)
    {
        m_cpu->initializeSMP();
    }

    // Set Tru64-specific PAL mode
    writeIPRWithCache("PAL_MODE", 2); // Tru64 mode

    emit systemCallInvoked(FUNC_Tru64_INITPAL, instr.pc);
    return true;
}

bool executorAlphaPAL::executeTru64_SWPIRQL(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SWPIRQL executed at PC:" << Qt::hex << instr.pc;

    // Same as Alpha SWPIRQL but with Tru64 semantics
    return executeAlpha_SWPIRQL(instr);
}

bool executorAlphaPAL::executeTru64_RDIRQL(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDIRQL executed at PC:" << Qt::hex << instr.pc;

    // Same as Alpha RDIRQL
    return executeAlpha_RDIRQL(instr);
}

bool executorAlphaPAL::executeTru64_DI(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_DI executed at PC:" << Qt::hex << instr.pc;

    // Same as Alpha DI
    return executeAlpha_DI(instr);
}

bool executorAlphaPAL::executeTru64_RDMCES(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDMCES executed at PC:" << Qt::hex << instr.pc;

    // Read Machine Check Error Summary
    return executeAlpha_MFPR_MCES(instr);
}

bool executorAlphaPAL::executeTru64_WRMCES(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRMCES executed at PC:" << Qt::hex << instr.pc;

    // Write Machine Check Error Summary
    return executeAlpha_MTPR_MCES(instr);
}

bool executorAlphaPAL::executeTru64_RDPCBB(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDPCBB executed at PC:" << Qt::hex << instr.pc;

    // Read Process Control Block Base
    return executeAlpha_MFPR_PCBB(instr);
}

bool executorAlphaPAL::executeTru64_WRPRBR(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRPRBR executed at PC:" << Qt::hex << instr.pc;

    // Write Processor Base Register
    return executeAlpha_MTPR_PRBR(instr);
}

bool executorAlphaPAL::executeTru64_TBIA(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_TBIA executed at PC:" << Qt::hex << instr.pc;

    // Translation Buffer Invalidate All
    return executeMTPR_TBIA(instr);
}

bool executorAlphaPAL::executeTru64_THIS(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_THIS executed at PC:" << Qt::hex << instr.pc;

    // Translation Buffer Invalidate Single (Instruction)
    return executeMTPR_TBISI(instr);
}

bool executorAlphaPAL::executeTru64_DTBIS(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_DTBIS executed at PC:" << Qt::hex << instr.pc;

    // Data Translation Buffer Invalidate Single
    return executeMTPR_TBISD(instr);
}

bool executorAlphaPAL::executeTru64_TBISASN(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_TBISASN executed at PC:" << Qt::hex << instr.pc;

    // Translation Buffer Invalidate by ASN
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 asn = 0;

    if (!readIntegerRegisterWithCache(ra, asn))
    {
        return false;
    }

    if (m_dTLB)
    {
        m_dTLB->invalidateASN(asn);
    }
    if (m_iTLB)
    {
        m_iTLB->invalidateASN(asn);
    }

    emit tlbOperationCompleted("TBISASN", 1);
    return true;
}

bool executorAlphaPAL::executeTru64_RDKSP(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDKSP executed at PC:" << Qt::hex << instr.pc;

    // Read Kernel Stack Pointer
    quint64 kspValue = 0;
    if (!readIPRWithCache("KSP", kspValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = kspValue;
    return true;
}

bool executorAlphaPAL::executeTru64_SWPKSP(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SWPKSP executed at PC:" << Qt::hex << instr.pc;

    // Swap Kernel Stack Pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newKSP = 0;

    if (!readIntegerRegisterWithCache(ra, newKSP))
    {
        return false;
    }

    quint64 oldKSP = 0;
    if (!readIPRWithCache("KSP", oldKSP))
    {
        return false;
    }

    writeIPRWithCache("KSP", newKSP);

    const_cast<PALInstruction &>(instr).result = oldKSP;
    return true;
}

bool executorAlphaPAL::executeTru64_WRPERFMON(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRPERFMON executed at PC:" << Qt::hex << instr.pc;

    // Write Performance Monitor (same as Alpha)
    return executeAlpha_MTPR_PERFMON(instr);
}

bool executorAlphaPAL::executeTru64_SWPIPL(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SWPIPL executed at PC:" << Qt::hex << instr.pc;

    // Swap Interrupt Priority Level (same as common SWPIPL)
    return executeSWPIPL(instr);
}

bool executorAlphaPAL::executeTru64_RDUSP(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDUSP executed at PC:" << Qt::hex << instr.pc;

    // Read User Stack Pointer (same as Alpha)
    return executeAlpha_MFPR_USP(instr);
}

bool executorAlphaPAL::executeTru64_WRUSP(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRUSP executed at PC:" << Qt::hex << instr.pc;

    // Write User Stack Pointer (same as Alpha)
    return executeAlpha_MTPR_USP(instr);
}

bool executorAlphaPAL::executeTru64_RDCOUNTERS(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDCOUNTERS executed at PC:" << Qt::hex << instr.pc;

    // Read Performance Counters
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    quint64 counterSelect = 0;

    if (!readIntegerRegisterWithCache(rb, counterSelect))
    {
        return false;
    }

    quint64 counterValue = 0;
    if (m_cpu && m_cpu->hasPerformanceCounters())
    {
        counterValue = m_cpu->getPerformanceCounter(counterSelect);
    }

    const_cast<PALInstruction &>(instr).result = counterValue;
    const_cast<PALInstruction &>(instr).writeResult = true;
    const_cast<PALInstruction &>(instr).targetRegister = ra;
    return true;
}


bool executorAlphaPAL::executeTru64_CALLSYS(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_CALLSYS executed at PC:" << Qt::hex << instr.pc;

    // System call entry point for Tru64
    if (m_cpu)
    {
        // Switch to kernel mode
        m_cpu->setPrivilegeMode(0);

        // Save user context
        quint64 userPC = m_cpu->getPC();
        quint64 userPS = 0;
        readIPRWithCache("PS", userPS);

        writeIPRWithCache("EXC_PC", userPC);
        writeIPRWithCache("EXC_PS", userPS);
    }

    emit systemCallInvoked(FUNC_Tru64_CALLSYS_, instr.pc);
    return true;
}

bool executorAlphaPAL::executeTru64_SSIR(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SSIR executed at PC:" << Qt::hex << instr.pc;

    // Set Software Interrupt Request
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 sirValue = 0;

    if (!readIntegerRegisterWithCache(ra, sirValue))
    {
        return false;
    }

    // Set software interrupt bit
    if (m_cpu && sirValue != 0)
    {
        m_cpu->triggerSoftwareInterrupt(sirValue);
    }

    return writeIPRWithCache("SIRR", sirValue);
}

bool executorAlphaPAL::executeTru64_WRIPIR(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRIPIR executed at PC:" << Qt::hex << instr.pc;

    // Write Inter-Processor Interrupt Request (same as Alpha)
    return executeAlpha_MTPR_IPIR(instr);
}

bool executorAlphaPAL::executeTru64_RFE(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RFE executed at PC:" << Qt::hex << instr.pc;

    // Return From Exception (same as common REI)
    return executeREI(instr);
}

bool executorAlphaPAL::executeTru64_RETSYS(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RETSYS executed at PC:" << Qt::hex << instr.pc;

    // Return from system call
    if (m_cpu)
    {
        // Restore user context
        quint64 userPC = 0;
        quint64 userPS = 0;

        if (readIPRWithCache("EXC_PC", userPC) && readIPRWithCache("EXC_PS", userPS))
        {
            m_cpu->setPC(userPC);
            writeIPRWithCache("PS", userPS);
        }

        // Return to user mode
        m_cpu->setPrivilegeMode(3);
    }

    return true;
}

bool executorAlphaPAL::executeTru64_RDPER(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDPER executed at PC:" << Qt::hex << instr.pc;

    // Read Performance Counter
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint8 rb = (instr.instruction.raw >> 16) & 0x1F;
    quint64 counterNum = 0;

    if (!readIntegerRegisterWithCache(rb, counterNum))
    {
        return false;
    }

    quint64 counterValue = 0;
    if (m_cpu)
    {
        counterValue = m_cpu->readDetailedPerformanceCounter(counterNum);
    }

    const_cast<PALInstruction &>(instr).result = counterValue;
    const_cast<PALInstruction &>(instr).writeResult = true;
    const_cast<PALInstruction &>(instr).targetRegister = ra;
    return true;
}

bool executorAlphaPAL::executeTru64_RDTHREAD(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDTHREAD executed at PC:" << Qt::hex << instr.pc;

    // Read Thread ID register
    quint64 threadValue = 0;
    if (!readIPRWithCache("THREAD", threadValue))
    {
        return false;
    }

    const_cast<PALInstruction &>(instr).result = threadValue;
    return true;
}

bool executorAlphaPAL::executeTru64_SWPCTX(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SWPCTX executed at PC:" << Qt::hex << instr.pc;

    // Context switch (same as common SWPCTX)
    return executeSWPCTX(instr);
}

bool executorAlphaPAL::executeTru64_WRFEN(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRFEN executed at PC:" << Qt::hex << instr.pc;

    // Write Floating-Point Enable (same as Alpha MTPR_FEN)
    return executeMTPR_FEN(instr);
}

bool executorAlphaPAL::executeTru64_RTI(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RTI executed at PC:" << Qt::hex << instr.pc;

    // Return from Interrupt (same as REI)
    return executeREI(instr);
}

bool executorAlphaPAL::executeTru64_RDUNIQUE(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDUNIQUE executed at PC:" << Qt::hex << instr.pc;

    // Read Unique register (same as Alpha READ_UNQ)
    return executeAlpha_READ_UNQ(instr);
}

bool executorAlphaPAL::executeTru64_WRUNIQUE(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_WRUNIQUE executed at PC:" << Qt::hex << instr.pc;

    // Write Unique register (same as Alpha WRITE_UNQ)
    return executeAlpha_WRITE_UNQ(instr);
}

bool executorAlphaPAL::executeTru64_SWPIRQL(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SWPIRQL executed at PC:" << Qt::hex << instr.pc;

    // Same as Alpha SWPIRQL but with Tru64 semantics
    return executeAlpha_SWPIRQL(instr);
}
bool executorAlphaPAL::executeTru64_DI(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_DI executed at PC:" << Qt::hex << instr.pc;

    // Same as Alpha DI
    return executeAlpha_DI(instr);
}

bool executorAlphaPAL::executeTru64_TBIA(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_TBIA executed at PC:" << Qt::hex << instr.pc;

    // Translation Buffer Invalidate All
    return executeMTPR_TBIA(instr);
}
bool executorAlphaPAL::executeTru64_SWPKSP(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_SWPKSP executed at PC:" << Qt::hex << instr.pc;

    // Swap Kernel Stack Pointer
    quint8 ra = (instr.instruction.raw >> 21) & 0x1F;
    quint64 newKSP = 0;

    if (!readIntegerRegisterWithCache(ra, newKSP))
    {
        return false;
    }

    quint64 oldKSP = 0;
    if (!readIPRWithCache("KSP", oldKSP))
    {
        return false;
    }

    writeIPRWithCache("KSP", newKSP);

    const_cast<PALInstruction &>(instr).result = oldKSP;
    return true;
}

bool executorAlphaPAL::executeTru64_RDPSR(const PALInstruction &instr)
{
    qDebug() << "PAL Tru64_RDPSR executed at PC:" << Qt::hex << instr.pc;

    // Read Processor Status Register (same as common RDPS)
    return executeRDPS(instr);
}

#endif

// Additional missing PAL function implementations


void executorAlphaPAL::onInstructionCacheHit(quint64 address)
{
    // Update PAL-specific cache statistics
    QMutexLocker locker(&m_statsMutex);
    m_l1ICacheHits++;

    // Track if this was a PAL instruction access
    if (address >= 0x8000 && address < 0x10000)
    {
        // Likely PAL code region
        updateJITStats(address); // Could enhance JIT optimization
    }
}

void executorAlphaPAL::onInstructionCacheMiss(quint64 address)
{
    QMutexLocker locker(&m_statsMutex);
    m_l1ICacheMisses++;

    // If this is a frequently accessed PAL function, consider it for optimization
    if (address >= 0x8000 && address < 0x10000)
    {
        // Add to potential optimization candidates
        m_instructionCache->addHotSpot(address, 64);
    }
}

void executorAlphaPAL::onCacheLineInvalidated(quint64 address)
{
   DEBUG_LOG( QString("PAL: I-cache line invalidated at 0x%1").arg(address, 0, 16));

    // If this was a critical PAL function, we may need to reload it
    if (isCriticalPALAddress(address))
    {
        // Consider immediate reload for critical functions
        QTimer::singleShot(1,
                           [this, address]()
                           {
                               if (m_instructionCache)
                               {
                                   m_instructionCache->prefetch(address);
                               }
                           });
    }
}

void executorAlphaPAL::onCacheCoherencyEvent(quint64 address, const QString &eventType)
{
    qDebug() << QString("PAL: Cache coherency event %1 at 0x%2").arg(eventType).arg(address, 0, 16);

    // Update coherency statistics
    QMutexLocker locker(&m_statsMutex);
    // Could add m_coherencyEventsHandled++;

    // Coordinate with other execution units if needed
    if (eventType == "INVALIDATE" && isCriticalPALAddress(address))
    {
        // Critical PAL code was invalidated - may affect other units
        coordinateWithOtherExecutors(PALInstruction()); // Dummy instruction for coordination
    }
}