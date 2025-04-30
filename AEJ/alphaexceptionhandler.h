#pragma once

// AlphaExceptionHandler.h - Exception handling header
#ifndef ALPHAEXCEPTIONHANDLER_H
#define ALPHAEXCEPTIONHANDLER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include <QMutex>
#include "..\AESH\Helpers.h"

class AlphaCPU;
class AlphaSMPManager;

/**
 * @brief AlphaExceptionHandler - Handles exceptions and traps for Alpha CPUs
 *
 * This class provides centralized exception and trap handling for all CPUs
 * in the system, dispatching to appropriate handlers.
 */
class AlphaExceptionHandler : public QObject
{
    Q_OBJECT

public:
    explicit AlphaExceptionHandler(AlphaSMPManager* smpManager, QObject* parent = nullptr);
    ~AlphaExceptionHandler();

    void initialize();

    // Handler registration
	void registerTrapHandler(helpers_JIT::ExceptionType trapType, QObject* receiver, const char* method);
	void unregisterTrapHandler(helpers_JIT::ExceptionType trapType, QObject * receiver, const char* method);

public slots:
    // Exception handling
    void handleException(int cpuId, helpers_JIT::ExceptionType, quint64 pc, quint64 faultAddr);
    void handleTrap(int cpuId, helpers_JIT::ExceptionType trapType, quint64 pc);
    void handleInterrupt(int cpuId, int interruptVector);

    // Special handling
    void handleSystemCall(int cpuId, int callNumber, const QVector<quint64>& params);
    void handlePALCall(int cpuId, int palFunction);

signals:
    // Handling notifications
    void exceptionHandled(int cpuId, helpers_JIT::ExceptionType);
    void trapHandled(int cpuId, helpers_JIT::ExceptionType trapType);
    void interruptHandled(int cpuId, int interruptVector);

    // State changes
    void kernelModeSwitched(int cpuId, bool kernelMode);
    void contextSwitched(int cpuId, int oldProcessId, int newProcessId);

    // Special notifications
    void systemCallHandled(int cpuId, int callNumber, quint64 result);
    void palCallHandled(int cpuId, int palFunction);

private:
    // SMP reference
    AlphaSMPManager* m_smpManager;

    // Handler registry
    QHash<helpers_JIT::ExceptionType, QList<QPair<QObject*, const char*>>> m_trapHandlers;
    QMutex m_handlerLock;

    // Process context management
    struct ProcessContext {
        int processId;
        QVector<quint64> registers;
        quint64 pc;
        QVector<double> fpRegisters;
        // Additional context...
    };
    QHash<int, ProcessContext> m_processContexts; // Maps CPU ID to current context

    // Helper methods
    void dispatchToKernel(int cpuId, helpers_JIT::ExceptionType, quint64 pc, quint64 faultAddr);
    void dumpException(int cpuId, helpers_JIT::ExceptionType exceptionType);
    void saveProcessContext(int cpuId);
    void restoreProcessContext(int cpuId, int processId);
    bool callRegisteredHandlers(helpers_JIT::ExceptionType trapType, int cpuId, quint64 pc);
};

#endif // ALPHAEXCEPTIONHANDLER_H
