#pragma once
#include <QtCore>
#include <QString>
#include <QStringList>


/**
 * @brief PAL instruction execution result with detailed information
 */
struct PALExecutionResult
{
    bool success = false;
    int cycles = 0;
    quint64 result = 0;
    QString errorMessage;
    bool systemStateChanged = false;
    bool requiresFlush = false;
    QStringList sideEffects;

    PALExecutionResult() = default;
    PALExecutionResult(bool ok, int cycleCount = 1) : success(ok), cycles(cycleCount) {}
};