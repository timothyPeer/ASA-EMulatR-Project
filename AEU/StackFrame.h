#ifndef StackFrame_h__
#define StackFrame_h__

#pragma once
/**
 * @file StackFrameManager.h
 * @brief  Revised StackFrame and StackManager for Alpha AXP emulator.
 *
 * Implements two Plain Old Data (POD) structures for fast, predictable
 * exception?frame handling and a lightweight manager that is SMP safe
 * and never exposes dangling references after releasing its lock.
 *
 * Register Storage: 
 * - Optimizes for space by storing only the essential registers (R16-R21, R26, R27, R30) 
 *   that are needed for exception handling
 * - The full register state is saved in a separate SavedContext structure only when 
 *   needed for context switching
 * - Aligns with the Alpha architecture's actual PALcode behavior
 * 
 * 
 * (c) 2025  Timothy Peer & contributors MIT License
 */

#include <QtGlobal>
#include <QReadWriteLock>
#include <QVector>
#include <cstring>      // for std::memcpy
#include <optional>

 /* =============================================================
  * 1.  ExceptionFrame  what PAL actually pushes on a trap.
  *     Fixed size, cache  Friendly 128 bytes no constructors.
  * ============================================================= */
struct ExceptionFrame final
{
    quint64 pc;                 //!< Faulting PC (hardware saved)
    quint64 ps;                 //!< Processor Status (hardware saved)
    quint64 excSum;             //!< Exception Summary (PAL saved)

    // Argument registers R16?R21 (most OSes need them immediately)
    quint64 r16_21[6];          //!< 6 × 8 B = 48 byte block

    quint64 ra;                 //!< R26 (Return Address)
    quint64 pv;                 //!< R27 (Procedure Value / Global Pointer)
    quint64 sp;                 //!< R30 (Stack Pointer at fault time)

    quint64 fpcr;               //!< Floating point control (valid only if PS.FEN)
};

/* =============================================================
 * 2.  SavedContext  –  full register spill used by scheduler.
 * ============================================================= */
struct SavedContext final
{
    quint64 intRegs[32];        //!< Full integer register file
    quint64 fpRegs[32];         //!< Full floating point register file
    quint64 fpcr;               //!< FP control register
    quint64 asn;                //!< Address Space Number (optional for context)
    quint64 ptbr;               //!< Page Table Base Register
};


/* =============================================================
 * 3.  StackFrame  –  wraps the mandatory ExceptionFrame plus an
 *     optional full SavedContext if the kernel performs a context
 *     switch at this depth.  POD: no vtable, no Qt QObject.
 * ============================================================= */
struct StackFrame final
{
    ExceptionFrame hwFrame;                     //!< always present
    std::optional<SavedContext> savedCtx;       //!< present only after switch
};
#endif // StackFrame_h__



