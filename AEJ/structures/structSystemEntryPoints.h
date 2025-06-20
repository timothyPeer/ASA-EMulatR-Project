#pragma once
#include <QtCore>


// Structure to hold system entry points
struct SystemEntryPoints
{
    // Standard entry points - used by most PALcodes
    quint64 reset;
    quint64 machineCheck;
    quint64 kernelStackNotValid;
    quint64 powerFail;
    quint64 memoryFault;
    quint64 arithmeticTrap;
    quint64 interrupt;
    quint64 astEntry;
    quint64 alignmentFault;
    quint64 translationInvalid;
    quint64 accessViolation;
    quint64 opcodeInvalid;
    quint64 floatingPointException;
    quint64 deviceInterrupt;
    quint64 systemCall;

    // OpenVMS-specific entry points
    quint64 changeModeKernel;
    quint64 changeModeExec;
    quint64 changeModeSuper;
    quint64 changeModeUser;

    // Tru64/Digital UNIX specific entry points
    quint64 unixSystemCall;
    quint64 unixUserSignal;

    // Windows NT specific entry points
    quint64 windowsSystemService;
    quint64 windowsDispatchException;

    // Custom entry points
    quint64 customEntries[MAX_CUSTOM_ENTRIES];

    // Constructor to initialize everything to zero
    SystemEntryPoints()
    {
        reset = 0;
        machineCheck = 0;
        kernelStackNotValid = 0;
        powerFail = 0;
        memoryFault = 0;
        arithmeticTrap = 0;
        interrupt = 0;
        astEntry = 0;
        alignmentFault = 0;
        translationInvalid = 0;
        accessViolation = 0;
        opcodeInvalid = 0;
        floatingPointException = 0;
        deviceInterrupt = 0;
        systemCall = 0;

        changeModeKernel = 0;
        changeModeExec = 0;
        changeModeSuper = 0;
        changeModeUser = 0;

        unixSystemCall = 0;
        unixUserSignal = 0;

        windowsSystemService = 0;
        windowsDispatchException = 0;

        for (int i = 0; i < MAX_CUSTOM_ENTRIES; i++)
        {
            customEntries[i] = 0;
        }
    }
};