#pragma once
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QSharedPointer>
// =============================================================================
// ALPHA IPR BANK - COMPLETE INLINE IMPLEMENTATION
// =============================================================================
// File: Alpha/Include/AlphaIprBank.h

#ifndef ALPHA_IPR_BANK_H
#define ALPHA_IPR_BANK_H

#include "BaseIprBank.h"
#include "BaseProcessorStatus.h"
#include "GlobalMacro.h"
#include "ModularArchitectureSupport.h"
#include "StackFrame.h"
#include "StackManager.h"
#include "asa_util.h"
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>

class AlphaIprBank : public BaseIprBank
{
    Q_OBJECT

  protected:
    mutable QReadWriteLock m_lock;
    QVector<quint64> m_registers;
    QMap<quint16, IPRDescriptor> m_iprDescriptors;
    QSharedPointer<StackManager> m_stackManager;
    QSharedPointer<BaseProcessorStatus> m_processorStatus;

    // Mode-specific stack pointers
    quint64 m_userStackPointer;
    quint64 m_supervisorStackPointer;
    quint64 m_executiveStackPointer;
    quint64 m_kernelStackPointer;

    // Exception state
    bool m_inExceptionHandler;
    quint64 m_exceptionPC;
    quint64 m_exceptionPS;
    quint64 m_exceptionSum;
    quint64 m_exceptionAddr;

  public:
    /**
     * @brief Constructor - creates Alpha IPR bank
     * @param parent Qt parent object
     */
    explicit AlphaIprBank(QObject *parent = nullptr)
        : BaseIprBank(parent), m_registers(IPR_COUNT, 0), m_userStackPointer(0x10000000),
          m_supervisorStackPointer(0x20000000), m_executiveStackPointer(0x30000000), m_kernelStackPointer(0x40000000),
          m_inExceptionHandler(false), m_exceptionPC(0), m_exceptionPS(0), m_exceptionSum(0), m_exceptionAddr(0)
    {
        DEBUG_LOG("AlphaIprBank: Constructor called");
        initialize();
    }

    virtual ~AlphaIprBank() = default;

    /**
     * @brief Initialize Alpha IPR bank
     */
    void initialize()
    {
        DEBUG_LOG("AlphaIprBank: Initializing");
        initializeArchitectureSpecific();
        initialize_SignalsAndSlots();
        DEBUG_LOG("AlphaIprBank: Initialization complete");
    }

    // ==========================================================================
    // BASEIPRBANK INTERFACE IMPLEMENTATION
    // ==========================================================================

    /**
     * @brief Read IPR register with privilege checking
     * @param iprNumber IPR number to read
     * @param currentMode Current processor mode
     * @return IPR value or 0 on error
     */
    quint64 read(quint16 iprNumber, ProcessorMode currentMode) override
    {
        QReadLocker lock(&m_lock);

        if (!isValidIPR(iprNumber))
        {
            ERROR_LOG("AlphaIprBank: Invalid IPR number 0x%04X", iprNumber);
            return 0;
        }

        if (!canAccess(iprNumber, currentMode, false))
        {
            emit sigAccessViolation(iprNumber, currentMode, false);
            WARN_LOG("AlphaIprBank: Access violation reading IPR 0x%04X in mode %d", iprNumber,
                     static_cast<int>(currentMode));
            return 0;
        }

        quint64 value = 0;

        // Handle special read cases
        auto it = m_iprDescriptors.find(iprNumber);
        if (it != m_iprDescriptors.end() && it->preRead)
        {
            value = it->preRead();
        }
        else
        {
            value = readIPRDirect(iprNumber);
        }

        // Call post-read handler if exists
        if (it != m_iprDescriptors.end() && it->postRead)
        {
            it->postRead(value);
        }

        DEBUG_LOG("AlphaIprBank: Read IPR 0x%04X = 0x%016llX (mode %d)", iprNumber, value,
                  static_cast<int>(currentMode));

        return value;
    }

    /**
     * @brief Write IPR register with privilege checking
     * @param iprNumber IPR number to write
     * @param value Value to write
     * @param currentMode Current processor mode
     */
    void write(quint16 iprNumber, quint64 value, ProcessorMode currentMode) override
    {
        if (!isValidIPR(iprNumber))
        {
            ERROR_LOG("AlphaIprBank: Invalid IPR number 0x%04X", iprNumber);
            return;
        }

        if (!canAccess(iprNumber, currentMode, true))
        {
            emit sigAccessViolation(iprNumber, currentMode, true);
            WARN_LOG("AlphaIprBank: Access violation writing IPR 0x%04X in mode %d", iprNumber,
                     static_cast<int>(currentMode));
            return;
        }

        quint64 oldValue = 0;
        quint64 maskedValue = value;

        {
            QWriteLocker lock(&m_lock);

            // Get current value for change detection
            oldValue = readIPRDirect(iprNumber);

            // Apply write mask if descriptor exists
            auto it = m_iprDescriptors.find(iprNumber);
            if (it != m_iprDescriptors.end())
            {
                maskedValue = (value & it->writeMask) | (oldValue & ~it->writeMask);

                // Call pre-write handler if exists
                if (it->preWrite && !it->preWrite(maskedValue))
                {
                    DEBUG_LOG("AlphaIprBank: Pre-write handler rejected write to IPR 0x%04X", iprNumber);
                    return;
                }
            }

            // Handle special write cases
            if (!writeIPRDirect(iprNumber, maskedValue))
            {
                ERROR_LOG("AlphaIprBank: Failed to write IPR 0x%04X", iprNumber);
                return;
            }
        }

        // Emit change signal if value actually changed
        if (maskedValue != oldValue)
        {
            emit sigIprChanged(iprNumber, oldValue, maskedValue);
        }

        // Handle post-write processing
        auto it = m_iprDescriptors.find(iprNumber);
        if (it != m_iprDescriptors.end() && it->postWrite)
        {
            it->postWrite(maskedValue);
        }

        handleAlphaSpecialWrites(iprNumber, maskedValue);

        DEBUG_LOG("AlphaIprBank: Write IPR 0x%04X = 0x%016llX (mode %d)", iprNumber, maskedValue,
                  static_cast<int>(currentMode));
    }

    /**
     * @brief Check if IPR number is valid
     * @param iprNumber IPR number to check
     * @return true if valid, false otherwise
     */
    bool isValidIPR(quint16 iprNumber) const override
    {
        QReadLocker lock(&m_lock);
        return m_iprDescriptors.contains(iprNumber);
    }

    /**
     * @brief Check if IPR can be accessed in current mode
     * @param iprNumber IPR number
     * @param mode Current processor mode
     * @param isWrite true for write access, false for read
     * @return true if access allowed, false otherwise
     */
    bool canAccess(quint16 iprNumber, ProcessorMode mode, bool isWrite) const override
    {
        QReadLocker lock(&m_lock);

        auto it = m_iprDescriptors.find(iprNumber);
        if (it == m_iprDescriptors.end())
        {
            return false;
        }

        const IPRDescriptor &desc = it.value();

        // Check if write to read-only register
        if (isWrite && desc.type == IPRType::IPR_READ_ONLY)
        {
            return false;
        }

        // Check privilege level
        switch (desc.privilege)
        {
        case IPRPrivilege::IPR_USER_ACCESS:
            return true;

        case IPRPrivilege::IPR_SUPERVISOR_ACCESS:
            return mode <= ProcessorMode::MODE_SUPERVISOR;

        case IPRPrivilege::IPR_EXECUTIVE_ACCESS:
            return mode <= ProcessorMode::MODE_EXECUTIVE;

        case IPRPrivilege::IPR_KERNEL_ACCESS:
            return mode == ProcessorMode::MODE_KERNEL;

        case IPRPrivilege::IPR_PAL_ACCESS:
            if (m_processorStatus)
            {
                return m_processorStatus->isPALModeActive();
            }
            return mode == ProcessorMode::MODE_KERNEL;

        default:
            return false;
        }
    }

    /**
     * @brief Get processor architecture
     * @return Alpha architecture enum
     */
    ProcessorArchitecture getArchitecture() const override { return ProcessorArchitecture::ARCHITECTURE_ALPHA; }

    QString getArchitectureName() const override { return "Alpha AXP"; }

    /**
     * @brief Set stack manager
     * @param stackMgr Shared pointer to stack manager
     */
    void attachStackManager(QSharedPointer<StackManager> stackMgr) override
    {
        QWriteLocker lock(&m_lock);
        m_stackManager = stackMgr;
        DEBUG_LOG("AlphaIprBank: Stack manager attached");
    }

    QSharedPointer<StackManager> getStackManager() const override
    {
        QReadLocker lock(&m_lock);
        return m_stackManager;
    }

    /**
     * @brief Handle exception entry
     * @param exceptionType Type of exception
     * @param parameter Exception parameter
     */
    void handleException(quint16 exceptionType, quint64 parameter) override
    {
        QWriteLocker lock(&m_lock);

        m_inExceptionHandler = true;
        m_exceptionSum = (static_cast<quint64>(exceptionType) << 32) | (parameter & 0xFFFFFFFF);

        if (m_processorStatus)
        {
            m_exceptionPS = m_processorStatus->saveForException();
        }

        DEBUG_LOG("AlphaIprBank: Exception 0x%04X handled, parameter 0x%016llX", exceptionType, parameter);
        emit sigExceptionStateChanged();
    }

    /**
     * @brief Save exception state from frame
     * @param frame Exception frame to save
     */
    void saveExceptionState(const ExceptionFrame &frame) override
    {
        QWriteLocker lock(&m_lock);

        m_exceptionPC = frame.pc;
        m_exceptionPS = frame.ps;
        m_exceptionSum = frame.excSum;
        m_exceptionAddr = 0; // Would be set based on exception type
        m_inExceptionHandler = true;

        DEBUG_LOG("AlphaIprBank: Exception state saved from frame");
        emit sigExceptionStateChanged();
    }

    /**
     * @brief Restore from exception
     */
    void restoreExceptionState() override
    {
        QWriteLocker lock(&m_lock);

        m_inExceptionHandler = false;
        m_exceptionPC = 0;
        m_exceptionPS = 0;
        m_exceptionSum = 0;
        m_exceptionAddr = 0;

        DEBUG_LOG("AlphaIprBank: Exception state restored");
        emit sigExceptionStateChanged();
    }

    /**
     * @brief Set processor status
     * @param status Shared pointer to processor status
     */
    void attachProcessorStatus(QSharedPointer<BaseProcessorStatus> status) override
    {
        QWriteLocker lock(&m_lock);
        m_processorStatus = status;
        DEBUG_LOG("AlphaIprBank: Processor status attached");
    }

    QSharedPointer<BaseProcessorStatus> getProcessorStatus() const override
    {
        QReadLocker lock(&m_lock);
        return m_processorStatus;
    }

    /**
     * @brief Get list of IPR names
     * @return QStringList of IPR names
     */
    QStringList getIPRNames() const override
    {
        QReadLocker lock(&m_lock);
        QStringList names;

        for (auto it = m_iprDescriptors.constBegin(); it != m_iprDescriptors.constEnd(); ++it)
        {
            names << it.value().name;
        }

        names.sort();
        return names;
    }

    /**
     * @brief Get IPR description
     * @param iprNumber IPR number
     * @return Description string
     */
    QString getIPRDescription(quint16 iprNumber) const override
    {
        QReadLocker lock(&m_lock);

        auto it = m_iprDescriptors.find(iprNumber);
        if (it != m_iprDescriptors.end())
        {
            return it.value().description;
        }

        return QString("Unknown IPR 0x%1").arg(iprNumber, 4, 16, QChar('0'));
    }

    /**
     * @brief Get all IPR values accessible in current mode
     * @param mode Current processor mode
     * @return Map of IPR name to value
     */
    QMap<QString, quint64> getAllIPRValues(ProcessorMode mode) const override
    {
        QReadLocker lock(&m_lock);
        QMap<QString, quint64> values;

        for (auto it = m_iprDescriptors.constBegin(); it != m_iprDescriptors.constEnd(); ++it)
        {
            quint16 iprNum = it.key();
            const IPRDescriptor &desc = it.value();

            if (canAccess(iprNum, mode, false))
            {
                lock.unlock();
                quint64 value = const_cast<AlphaIprBank *>(this)->read(iprNum, mode);
                lock.relock();
                values[desc.name] = value;
            }
        }

        return values;
    }

    // ==========================================================================
    // ALPHA-SPECIFIC METHODS
    // ==========================================================================

    /**
     * @brief Read generic IPR (PAL interface)
     * @param n IPR number (0-127)
     * @param mode Processor mode (default user)
     * @return IPR value
     */
    Q_INVOKABLE quint64 readIpr(quint8 n, int mode = 3)
    {
        if (n > 127)
        {
            ERROR_LOG("AlphaIprBank: Invalid generic IPR number %d", n);
            return 0;
        }

        ProcessorMode procMode = static_cast<ProcessorMode>(mode);
        return read(IPR_IPR0 + n, procMode);
    }

    /**
     * @brief Write generic IPR (PAL interface)
     * @param n IPR number (0-127)
     * @param v Value to write
     * @param mode Processor mode (default user)
     */
    Q_INVOKABLE void writeIpr(quint8 n, quint64 v, int mode = 3)
    {
        if (n > 127)
        {
            ERROR_LOG("AlphaIprBank: Invalid generic IPR number %d", n);
            return;
        }

        ProcessorMode procMode = static_cast<ProcessorMode>(mode);
        write(IPR_IPR0 + n, v, procMode);
    }

    /**
     * @brief Clear all IPRs to default values
     */
    Q_INVOKABLE void clear()
    {
        QWriteLocker lock(&m_lock);

        DEBUG_LOG("AlphaIprBank: Clearing all IPRs");

        m_registers.fill(0);

        // Reset stack pointers to defaults
        m_userStackPointer = 0x10000000;
        m_supervisorStackPointer = 0x20000000;
        m_executiveStackPointer = 0x30000000;
        m_kernelStackPointer = 0x40000000;

        // Reset exception state
        m_inExceptionHandler = false;
        m_exceptionPC = 0;
        m_exceptionPS = 0;
        m_exceptionSum = 0;
        m_exceptionAddr = 0;

        // Reinitialize defaults
        initializeAlphaIPRs();

        DEBUG_LOG("AlphaIprBank: Clear completed");
    }

    /**
     * @brief Get stack pointer for mode
     * @param mode Processor mode
     * @return Stack pointer value
     */
    quint64 getStackPointer(ProcessorMode mode) const
    {
        QReadLocker lock(&m_lock);

        switch (mode)
        {
        case ProcessorMode::MODE_USER:
            return m_userStackPointer;
        case ProcessorMode::MODE_SUPERVISOR:
            return m_supervisorStackPointer;
        case ProcessorMode::MODE_EXECUTIVE:
            return m_executiveStackPointer;
        case ProcessorMode::MODE_KERNEL:
            return m_kernelStackPointer;
        default:
            return 0;
        }
    }

    /**
     * @brief Set stack pointer for mode
     * @param mode Processor mode
     * @param value Stack pointer value
     */
    void setStackPointer(ProcessorMode mode, quint64 value)
    {
        QWriteLocker lock(&m_lock);

        quint64 oldValue = getStackPointer(mode);

        switch (mode)
        {
        case ProcessorMode::MODE_USER:
            m_userStackPointer = value;
            break;
        case ProcessorMode::MODE_SUPERVISOR:
            m_supervisorStackPointer = value;
            break;
        case ProcessorMode::MODE_EXECUTIVE:
            m_executiveStackPointer = value;
            break;
        case ProcessorMode::MODE_KERNEL:
            m_kernelStackPointer = value;
            break;
        default:
            return;
        }

        if (value != oldValue)
        {
            quint16 iprNumber = getStackPointerIPR(mode);
            lock.unlock();
            emit sigIprChanged(iprNumber, oldValue, value);
        }

        DEBUG_LOG("AlphaIprBank: Stack pointer for mode %d set to 0x%016llX", static_cast<int>(mode), value);
    }

    /**
     * @brief Switch stack pointers during mode transition
     * @param fromMode Source mode
     * @param toMode Target mode
     */
    void switchStackPointers(ProcessorMode fromMode, ProcessorMode toMode)
    {
        if (m_stackManager)
        {
            DEBUG_LOG("AlphaIprBank: Switching stack pointers from mode %d to mode %d", static_cast<int>(fromMode),
                      static_cast<int>(toMode));
            // Integration with StackManager would happen here
        }
    }

    /**
     * @brief Read PAL register
     * @param palRegNum PAL register number
     * @return PAL register value
     */
    quint64 readPALRegister(quint16 palRegNum)
    {
        ProcessorMode currentMode = ProcessorMode::MODE_KERNEL;

        if (m_processorStatus && !m_processorStatus->isPALModeActive())
        {
            WARN_LOG("AlphaIprBank: PAL register access requires PAL mode");
            return 0;
        }

        return read(IPR_PAL_BASE + palRegNum, currentMode);
    }

    /**
     * @brief Write PAL register
     * @param palRegNum PAL register number
     * @param value Value to write
     */
    void writePALRegister(quint16 palRegNum, quint64 value)
    {
        ProcessorMode currentMode = ProcessorMode::MODE_KERNEL;

        if (m_processorStatus && !m_processorStatus->isPALModeActive())
        {
            WARN_LOG("AlphaIprBank: PAL register access requires PAL mode");
            return;
        }

        write(IPR_PAL_BASE + palRegNum, value, currentMode);
    }

  signals:
    void sigIprChanged(quint16 iprNumber, quint64 oldValue, quint64 newValue);
    void sigIprWritten(quint16 iprNumber, quint64 value);
    void sigAccessViolation(quint16 iprNumber, ProcessorMode mode, bool isWrite);
    void sigExceptionStateChanged();
    void sigExceptionOccurred(quint16 exceptionType);
 

  protected:
    /**
     * @brief Initialize architecture-specific features
     */
    void initializeArchitectureSpecific() override
    {
        DEBUG_LOG("AlphaIprBank: Initializing Alpha-specific features");
        setupAlphaIPRDescriptors();
        initializeAlphaIPRs();
    }

  private:
    /**
     * @brief Initialize signal and slot connections
     */
    void initialize_SignalsAndSlots()
    {
        // Connect internal signals if needed
        DEBUG_LOG("AlphaIprBank: Signal/slot connections initialized");
    }

    /**
     * @brief Initialize Alpha IPR default values
     */
    void initializeAlphaIPRs()
    {
        // Set default values for Alpha IPRs
        m_registers[IPR_SCBB] = 0x10000000;                // System Control Block Base
        m_registers[IPR_WHAMI] = 0;                        // CPU ID
        m_registers[IPR_IPLR] = 0;                         // Interrupt Priority Level
        m_registers[IPR_PS] = 0x8;                         // Processor Status (user mode)
        m_registers[IPR_PAL_BASE] = 0xFFFFFFFF80000000ULL; // PAL base
        m_registers[IPR_FEN] = 1;                          // Floating point enable
        m_registers[IPR_ASN] = 0;                          // Address space number

        // Initialize performance counters
        for (int i = 0; i < 8; ++i)
        {
            m_registers[IPR_PERFMON_0 + i] = 0;
        }

        // Initialize exception entry points
        for (int i = 0; i < 8; ++i)
        {
            m_registers[IPR_ENTRY_0 + i] = 0x8000 + (i * 0x100);
        }

        DEBUG_LOG("AlphaIprBank: Alpha IPR defaults initialized");
    }

    /**
     * @brief Setup Alpha IPR descriptors
     */
    void setupAlphaIPRDescriptors()
    {
        // ASN - Address Space Number
        m_iprDescriptors[IPR_ASN] = {IPR_ASN,
                                     "ASN",
                                     IPRType::IPR_READ_WRITE,
                                     IPRPrivilege::IPR_KERNEL_ACCESS,
                                     0,
                                     0xFF,
                                     false,
                                     "Address Space Number for TLB management"};

        // Stack Pointers
        m_iprDescriptors[IPR_USP] = {IPR_USP,
                                     "USP",
                                     IPRType::IPR_STACK_POINTER,
                                     IPRPrivilege::IPR_SUPERVISOR_ACCESS,
                                     0x10000000,
                                     0xFFFFFFFFFFFFFFFFULL,
                                     true,
                                     "User Stack Pointer"};

        m_iprDescriptors[IPR_SSP] = {IPR_SSP,
                                     "SSP",
                                     IPRType::IPR_STACK_POINTER,
                                     IPRPrivilege::IPR_SUPERVISOR_ACCESS,
                                     0x20000000,
                                     0xFFFFFFFFFFFFFFFFULL,
                                     true,
                                     "Supervisor Stack Pointer"};

        m_iprDescriptors[IPR_ESP] = {IPR_ESP,
                                     "ESP",
                                     IPRType::IPR_STACK_POINTER,
                                     IPRPrivilege::IPR_EXECUTIVE_ACCESS,
                                     0x30000000,
                                     0xFFFFFFFFFFFFFFFFULL,
                                     true,
                                     "Executive Stack Pointer"};

        m_iprDescriptors[IPR_KSP] = {IPR_KSP,
                                     "KSP",
                                     IPRType::IPR_STACK_POINTER,
                                     IPRPrivilege::IPR_KERNEL_ACCESS,
                                     0x40000000,
                                     0xFFFFFFFFFFFFFFFFULL,
                                     true,
                                     "Kernel Stack Pointer"};

        // Exception Registers
        m_iprDescriptors[IPR_EXC_PC] = {IPR_EXC_PC,
                                        "EXC_PC",
                                        IPRType::IPR_EXCEPTION_STATE,
                                        IPRPrivilege::IPR_PAL_ACCESS,
                                        0,
                                        0xFFFFFFFFFFFFFFFFULL,
                                        true,
                                        "Exception Program Counter"};

        m_iprDescriptors[IPR_EXC_PS] = {IPR_EXC_PS,
                                        "EXC_PS",
                                        IPRType::IPR_EXCEPTION_STATE,
                                        IPRPrivilege::IPR_PAL_ACCESS,
                                        0,
                                        0xFFFFFFFFFFFFFFFFULL,
                                        true,
                                        "Exception Processor Status"};

        m_iprDescriptors[IPR_EXC_SUM] = {
            IPR_EXC_SUM,           "EXC_SUM", IPRType::IPR_EXCEPTION_STATE, IPRPrivilege::IPR_PAL_ACCESS, 0,
            0xFFFFFFFFFFFFFFFFULL, true,      "Exception Summary Register"};

        // Add more IPR descriptors...
        addStandardAlphaIPRs();
        addPerformanceCounterIPRs();
        addGenericIPRWindow();

        DEBUG_LOG("AlphaIprBank: %d IPR descriptors setup", m_iprDescriptors.size());
    }

    /**
     * @brief Add standard Alpha IPRs
     */
    void addStandardAlphaIPRs()
    {
        // Processor Status
        m_iprDescriptors[IPR_PS] = {IPR_PS, "PS",  IPRType::IPR_READ_WRITE,    IPRPrivilege::IPR_KERNEL_ACCESS, 0x8,
                                    0x7,    false, "Processor Status Register"};

        // PAL Base
        m_iprDescriptors[IPR_PAL_BASE] = {IPR_PAL_BASE,
                                          "PAL_BASE",
                                          IPRType::IPR_PAL_REGISTER,
                                          IPRPrivilege::IPR_PAL_ACCESS,
                                          0xFFFFFFFF80000000ULL,
                                          0xFFFFFFFFFFFFFFFFULL,
                                          false,
                                          "PAL Code Base Address"};

        // TLB Invalidation (write-only function registers)
        m_iprDescriptors[IPR_TBIA] = {IPR_TBIA, "TBIA", IPRType::IPR_WRITE_FUNCTION, IPRPrivilege::IPR_KERNEL_ACCESS, 0,
                                      0,        false,  "TLB Invalidate All"};
    }

    /**
     * @brief Add performance counter IPRs
     */
    void addPerformanceCounterIPRs()
    {
        for (int i = 0; i < 8; ++i)
        {
            quint16 iprNum = IPR_PERFMON_0 + i;
            m_iprDescriptors[iprNum] = {iprNum,
                                        QString("PERFMON_%1").arg(i),
                                        IPRType::IPR_READ_WRITE,
                                        IPRPrivilege::IPR_KERNEL_ACCESS,
                                        0,
                                        0xFFFFFFFFFFFFFFFFULL,
                                        false,
                                        QString("Performance Counter %1").arg(i)};
        }
    }

    /**
     * @brief Add generic IPR window
     */
    void addGenericIPRWindow()
    {
        for (int i = 0; i < 128; ++i)
        {
            quint16 iprNum = IPR_IPR0 + i;
            m_iprDescriptors[iprNum] = {iprNum,
                                        QString("IPR%1").arg(i),
                                        IPRType::IPR_READ_WRITE,
                                        IPRPrivilege::IPR_PAL_ACCESS,
                                        0,
                                        0xFFFFFFFFFFFFFFFFULL,
                                        false,
                                        QString("Generic IPR %1").arg(i)};
        }
    }

    /**
     * @brief Handle Alpha-specific special writes
     * @param iprNumber IPR number
     * @param value Written value
     */
    void handleAlphaSpecialWrites(quint16 iprNumber, quint64 value)
    {
        switch (iprNumber)
        {
        case IPR_TBIA:
            DEBUG_LOG("AlphaIprBank: TLB Invalidate All");
            break;

        case IPR_SIRR:
            DEBUG_LOG("AlphaIprBank: Software Interrupt Request: 0x%016llX", value);
            break;

        case IPR_IPLR:
            if (m_processorStatus)
            {
                InterruptPriorityLevel ipl = static_cast<InterruptPriorityLevel>(value & 0x7);
                m_processorStatus->setCurrentIPL(ipl);
            }
            break;

        case IPR_ASN:
            DEBUG_LOG("AlphaIprBank: ASN changed to %llu", value);
            break;

        default:
            break;
        }
    }

    /**
     * @brief Update exception registers
     */
    void updateExceptionRegisters() { emit sigExceptionStateChanged(); }

    /**
     * @brief Validate privilege access
     * @param iprNumber IPR number
     * @param mode Current mode
     * @param isWrite Write access flag
     */
    /**
     * @brief Validate privilege access
     * @param iprNumber IPR number
     * @param mode Current mode
     * @param isWrite Write access flag
     */
    void validatePrivilegeAccess(quint16 iprNumber, ProcessorMode mode, bool isWrite)
    {
        if (!canAccess(iprNumber, mode, isWrite))
        {
            QString operation = isWrite ? "write to" : "read from";
            WARN_LOG("AlphaIprBank: Privilege violation - %s IPR 0x%04X in mode %d", qPrintable(operation), iprNumber,
                     static_cast<int>(mode));
            emit sigAccessViolation(iprNumber, mode, isWrite);
        }
    }

    /**
     * @brief Direct IPR read without privilege checking
     * @param iprNumber IPR number
     * @return IPR value
     */
    quint64 readIPRDirect(quint16 iprNumber)
    {
        switch (iprNumber)
        {
        case IPR_USP:
            return m_userStackPointer;
        case IPR_SSP:
            return m_supervisorStackPointer;
        case IPR_ESP:
            return m_executiveStackPointer;
        case IPR_KSP:
            return m_kernelStackPointer;
        case IPR_EXC_PC:
            return m_exceptionPC;
        case IPR_EXC_PS:
            return m_exceptionPS;
        case IPR_EXC_SUM:
            return m_exceptionSum;
        case IPR_EXC_ADDR:
            return m_exceptionAddr;
        default:
            if (iprNumber < m_registers.size())
            {
                return m_registers[iprNumber];
            }
            return 0;
        }
    }

    /**
     * @brief Direct IPR write without privilege checking
     * @param iprNumber IPR number
     * @param value Value to write
     * @return true on success, false on failure
     */
    bool writeIPRDirect(quint16 iprNumber, quint64 value)
    {
        // Handle special function writes first
        auto it = m_iprDescriptors.find(iprNumber);
        if (it != m_iprDescriptors.end() && it->type == IPRType::IPR_WRITE_FUNCTION)
        {
            // Function registers don't store values
            return true;
        }

        switch (iprNumber)
        {
        case IPR_USP:
            m_userStackPointer = value;
            return true;
        case IPR_SSP:
            m_supervisorStackPointer = value;
            return true;
        case IPR_ESP:
            m_executiveStackPointer = value;
            return true;
        case IPR_KSP:
            m_kernelStackPointer = value;
            return true;
        case IPR_EXC_PC:
            m_exceptionPC = value;
            return true;
        case IPR_EXC_PS:
            m_exceptionPS = value;
            return true;
        case IPR_EXC_SUM:
            m_exceptionSum = value;
            return true;
        case IPR_EXC_ADDR:
            m_exceptionAddr = value;
            return true;
        default:
            if (iprNumber < m_registers.size())
            {
                m_registers[iprNumber] = value;
                return true;
            }
            return false;
        }
    }

    /**
     * @brief Get IPR number for stack pointer mode
     * @param mode Processor mode
     * @return IPR number for stack pointer
     */
    quint16 getStackPointerIPR(ProcessorMode mode) const
    {
        switch (mode)
        {
        case ProcessorMode::MODE_USER:
            return IPR_USP;
        case ProcessorMode::MODE_SUPERVISOR:
            return IPR_SSP;
        case ProcessorMode::MODE_EXECUTIVE:
            return IPR_ESP;
        case ProcessorMode::MODE_KERNEL:
            return IPR_KSP;
        default:
            return IPR_USP;
        }
    }

    /**
     * @brief Get operation counter reference
     * @return Reference to operation counter
     */
    static quint64 &getOperationCounter()
    {
        static quint64 s_operationCounter = 0;
        return s_operationCounter;
    }

    /**
     * @brief Increment operation counter safely
     */
    void incrementOperationCounter() { asa_util::safeIncrement(getOperationCounter()); }

    // ==========================================================================
    // ALPHA IPR CONSTANTS
    // ==========================================================================

    static constexpr quint16 IPR_ASN = 0x00;     // Address Space Number
    static constexpr quint16 IPR_ASTEN = 0x01;   // AST Enable
    static constexpr quint16 IPR_ASTSR = 0x02;   // AST Summary
    static constexpr quint16 IPR_ESP = 0x03;     // Executive Stack Pointer
    static constexpr quint16 IPR_FEN = 0x04;     // Floating Enable
    static constexpr quint16 IPR_IPLR = 0x05;    // Interrupt Priority Level
    static constexpr quint16 IPR_KSP = 0x06;     // Kernel Stack Pointer
    static constexpr quint16 IPR_MCES = 0x07;    // Machine Check Error Summary
    static constexpr quint16 IPR_PCBB = 0x08;    // Process Control Block Base
    static constexpr quint16 IPR_PRBR = 0x09;    // Processor Base Register
    static constexpr quint16 IPR_PTBR = 0x0A;    // Page Table Base Register
    static constexpr quint16 IPR_SCBB = 0x0B;    // System Control Block Base
    static constexpr quint16 IPR_SIRR = 0x0C;    // Software Interrupt Request
    static constexpr quint16 IPR_SISR = 0x0D;    // Software Interrupt Summary
    static constexpr quint16 IPR_SSP = 0x0E;     // Supervisor Stack Pointer
    static constexpr quint16 IPR_SYSPTBR = 0x0F; // System Page Table Base
    static constexpr quint16 IPR_TBCHK = 0x10;   // TB Check
    static constexpr quint16 IPR_TBIA = 0x11;    // TB Invalidate All
    static constexpr quint16 IPR_TBIAP = 0x12;   // TB Invalidate All Process
    static constexpr quint16 IPR_TBIS = 0x13;    // TB Invalidate Single
    static constexpr quint16 IPR_TBISD = 0x14;   // TB Invalidate Single Data
    static constexpr quint16 IPR_TBISI = 0x15;   // TB Invalidate Single Instruction
    static constexpr quint16 IPR_USP = 0x16;     // User Stack Pointer
    static constexpr quint16 IPR_VPTB = 0x17;    // Virtual Page Table Base
    static constexpr quint16 IPR_WHAMI = 0x18;   // Who Am I
    static constexpr quint16 IPR_PS = 0x19;      // Processor Status

    // Exception registers
    static constexpr quint16 IPR_EXC_PC = 0x1A;   // Exception PC
    static constexpr quint16 IPR_EXC_PS = 0x1B;   // Exception PS
    static constexpr quint16 IPR_EXC_SUM = 0x1C;  // Exception Summary
    static constexpr quint16 IPR_EXC_ADDR = 0x1D; // Exception Address
    static constexpr quint16 IPR_EXC_MASK = 0x1E; // Exception Mask

    // PAL registers
    static constexpr quint16 IPR_PAL_BASE = 0x20; // PAL Base Address
    static constexpr quint16 IPR_PAL_TEMP = 0x21; // PAL Temporary
    static constexpr quint16 IPR_IRQL = 0x22;     // Interrupt Request Level
    static constexpr quint16 IPR_UNQ = 0x23;      // Unique
    static constexpr quint16 IPR_THREAD = 0x24;   // Thread Pointer
    static constexpr quint16 IPR_PAL_MODE = 0x25; // PAL Mode

    // Performance counters
    static constexpr quint16 IPR_PERFMON_0 = 0x30; // Performance Monitor 0
    static constexpr quint16 IPR_PERFMON_1 = 0x31; // Performance Monitor 1
    static constexpr quint16 IPR_PERFMON_2 = 0x32; // Performance Monitor 2
    static constexpr quint16 IPR_PERFMON_3 = 0x33; // Performance Monitor 3
    static constexpr quint16 IPR_PERFMON_4 = 0x34; // Performance Monitor 4
    static constexpr quint16 IPR_PERFMON_5 = 0x35; // Performance Monitor 5
    static constexpr quint16 IPR_PERFMON_6 = 0x36; // Performance Monitor 6
    static constexpr quint16 IPR_PERFMON_7 = 0x37; // Performance Monitor 7

    // Exception entry points
    static constexpr quint16 IPR_ENTRY_0 = 0x40; // Entry Point 0
    static constexpr quint16 IPR_ENTRY_1 = 0x41; // Entry Point 1
    static constexpr quint16 IPR_ENTRY_2 = 0x42; // Entry Point 2
    static constexpr quint16 IPR_ENTRY_3 = 0x43; // Entry Point 3
    static constexpr quint16 IPR_ENTRY_4 = 0x44; // Entry Point 4
    static constexpr quint16 IPR_ENTRY_5 = 0x45; // Entry Point 5
    static constexpr quint16 IPR_ENTRY_6 = 0x46; // Entry Point 6
    static constexpr quint16 IPR_ENTRY_7 = 0x47; // Entry Point 7

    // Generic IPR window (PAL accessible)
    static constexpr quint16 IPR_IPR0 = 0x80;   // Generic IPR 0
    static constexpr quint16 IPR_IPR127 = 0xFF; // Generic IPR 127
    static constexpr quint16 IPR_COUNT = 0x100; // Total IPR count

  public:
    /**
     * @brief Get operation statistics
     * @return Number of IPR operations performed
     */
    static quint64 getOperationCount() { return getOperationCounter(); }

    /**
     * @brief Reset operation statistics
     */
    static void resetOperationCount()
    {
        getOperationCounter() = 0;
        DEBUG_LOG("AlphaIprBank: Operation counter reset");
    }

    /**
     * @brief Check if IPR bank is in valid state
     * @return true if valid, false otherwise
     */
    bool isValidState() const
    {
        QReadLocker lock(&m_lock);

        // Check basic state validity
        if (m_registers.size() != IPR_COUNT)
        {
            ERROR_LOG("AlphaIprBank: Invalid register array size");
            return false;
        }

        // Check stack pointer validity
        if (m_userStackPointer == 0 || m_kernelStackPointer == 0)
        {
            WARN_LOG("AlphaIprBank: Invalid stack pointer configuration");
            return false;
        }

        // Check descriptor consistency
        if (m_iprDescriptors.isEmpty())
        {
            ERROR_LOG("AlphaIprBank: No IPR descriptors loaded");
            return false;
        }

        return true;
    }

    /**
     * @brief Get comprehensive status information
     * @return Status string with key information
     */
    QString getStatusInfo() const
    {
        QReadLocker lock(&m_lock);

        return QString("AlphaIprBank: %1 IPRs, Stacks(U:0x%2 S:0x%3 E:0x%4 K:0x%5), Exception:%6, Ops:%7")
            .arg(m_iprDescriptors.size())
            .arg(m_userStackPointer, 8, 16, QChar('0'))
            .arg(m_supervisorStackPointer, 8, 16, QChar('0'))
            .arg(m_executiveStackPointer, 8, 16, QChar('0'))
            .arg(m_kernelStackPointer, 8, 16, QChar('0'))
            .arg(m_inExceptionHandler ? "Active" : "Inactive")
            .arg(getOperationCount());
    }

    /**
     * @brief Export IPR configuration
     * @return Map of all IPR configurations
     */
    QMap<quint16, IPRDescriptor> exportConfiguration() const
    {
        QReadLocker lock(&m_lock);
        return m_iprDescriptors;
    }

    /**
     * @brief Import IPR configuration
     * @param config Configuration to import
     * @return true on success, false on failure
     */
    bool importConfiguration(const QMap<quint16, IPRDescriptor> &config)
    {
        if (config.isEmpty())
        {
            ERROR_LOG("AlphaIprBank: Cannot import empty configuration");
            return false;
        }

        QWriteLocker lock(&m_lock);
        m_iprDescriptors = config;

        DEBUG_LOG("AlphaIprBank: Imported %d IPR configurations", config.size());
        return true;
    }
};

#endif // ALPHA_IPR_BANK_H
