#pragma once

#ifndef StackManager_h__
#define StackManager_h__

/**
 * @file StackFrameManager.h
 * @brief  Revised StackFrame and StackManager for Alpha AXP emulator.
 *
 * Implements two Plain Old Data (POD) structures for fast, predictable
 * exception frame handling and a lightweight manager that is SMP safe
 * and never exposes dangling references after releasing its lock.
 *
 * (c) 2025  Timothy Peer & contributors - MIT License
 */

#include <QtGlobal>
#include <QReadWriteLock>
#include <QVector>
#include <cstring>      // for std::memcpy
#include <optional>
#include "StackFrame.h"

/* =============================================================
 * 4.  StackManager - container of StackFrame objects per mode.
 *     Thread safe via QReadWriteLock; never exposes references
 *     that could dangle once the lock is released.
 * ============================================================= */
class StackManager
{
public:
    explicit StackManager(int maxDepth = kDefaultMaxDepth) : maxDepth_(maxDepth) {}

/**
     * @brief Push a hardware trap frame (as PAL pushed it) onto the stack.
     * @details  Wraps the raw ExceptionFrame into our StackFrame POD,
     *           leaving savedCtx empty.
     * @return   Zero-based index of the new frame, or -1 on overflow.
     * @sa Alpha AXP Architecture Reference Manual, Version 6 (1994),
     *     Section 2.4.3 "Trap Frame Format" (p. 2-14)
     */
    int pushFrame(const ExceptionFrame &frame)
    {
        QWriteLocker w(&lock_);
        if (frames_.size() >= maxDepth_)
            return -1;
        // Wrap the hardware frame into our StackFrame struct
        StackFrame sf;
        sf.hwFrame = frame;
        frames_.append(sf); // QVector<StackFrame>::append(StackFrame)
        return frames_.size() - 1;
    }

    /**
     * @brief Push a full StackFrame, including any SavedContext.
     * @details  Use this overload when you already have a constructed
     *           StackFrame (for example, replaying an exception stack).
     * @return   Zero-based index of the new frame, or -1 on overflow.
     * @sa Alpha AXP Architecture Reference Manual, Version 6 (1994),
     *     Section 3.2.1 "Context Switch and SavedContext" (p. 3-8)
     */
    int pushFrame(const StackFrame &frame)
    {
        QWriteLocker w(&lock_);
        if (frames_.size() >= maxDepth_)
            return -1;
        frames_.append(frame);
        return frames_.size() - 1;
    }

    /* Pop top frame. Returns true on success. */
    bool popFrame()
    {
        QWriteLocker w(&lock_);
        if (frames_.isEmpty()) return false;
        frames_.removeLast();
        return true;
    }

    /* Return a copy of the top frame (caller owns the copy). */
    std::optional<StackFrame> top() const
    {
        QReadLocker r(&lock_);
        if (frames_.isEmpty()) return std::nullopt;
        return frames_.last();   // QVector::last() returns *reference*, copy elided via NRVO
    }

    /* Retrieve immutable copy of the whole stack (debugging/UI). */
    QVector<StackFrame> snapshot() const
    {
        QReadLocker r(&lock_);
        return frames_;          // deep copy under lock, then unlock
    }

    /* Provide direct write access to SavedContext when scheduler runs. */
    SavedContext* allocateSavedContextForTop()
    {
        QWriteLocker w(&lock_);
        if (frames_.isEmpty()) return nullptr;
        StackFrame& f = frames_.last();
        if (!f.savedCtx) f.savedCtx.emplace();
        return &(*f.savedCtx);   // pointer valid until next reallocation
    }

    int depth() const { QReadLocker r(&lock_); return frames_.size(); }

private:
    static constexpr int kDefaultMaxDepth = 1024;

    mutable QReadWriteLock lock_;
    QVector<StackFrame>    frames_;
    const int              maxDepth_;
};

/* =============================================================
 * 5.  Construction helpers - inline utility functions you can
 *     call from PAL stubs or your CPU core to build frames
 *     quickly without manual field by field assignments.
 * ============================================================= */
namespace FrameHelpers {

    /// Build an ExceptionFrame from raw CPU state.
    /// @param gpr  Pointer to 32 element array of integer registers R0...R31.
    /// @note       Copies only the architecturally required subset  R16-R21, R26,
    ///             R27, R30).  The caller must supply FPCR and ExcSum.
    inline ExceptionFrame makeExceptionFrame(quint64 pc, quint64 ps,
        quint64 excSum,
        const quint64* gpr,
        quint64 fpcr)
    {
        ExceptionFrame f{};                        // value initialises all fields to 0
        f.pc = pc;
        f.ps = ps;
        f.excSum = excSum;

        std::memcpy(f.r16_21, gpr + 16, 6 * sizeof(quint64));
        f.ra = gpr[26];
        f.pv = gpr[27];
        f.sp = gpr[30];
        f.fpcr = fpcr;
        return f;
    }

    /// Convenience wrapper: pushes a freshly built frame onto a StackManager.
    inline bool pushTrapFrame(StackManager& mgr, quint64 pc, quint64 ps,
        quint64 excSum, const quint64* gpr, quint64 fpcr)
    {
        ExceptionFrame f = makeExceptionFrame(pc, ps, excSum, gpr, fpcr);
        return mgr.pushFrame(f) >= 0;
    }

} // namespace FrameHelpers

/* =============================================================
 * 6.  Integration notes
 * -------------------------------------------------------------
 *  - From your CPU execute loop, call FrameHelpers::pushTrapFrame() right
 *    after detecting a fault/interrupt and before switching to PAL mode.
 *
 *  - When the scheduler decides to context switch, obtain a pointer to the
 *    SavedContext via 'allocateSavedContextForTop()' and spill the *entire*
 *    register set there.  The pointer remains valid until the next pushFrame()
 *    that grows the underlying QVector.
 *
 *  - To inspect the current call stack in a debugger or GUI panel, call
 *    StackManager::snapshot() – it returns a deep copy so the UI thread can
 *    traverse it without touching the emulator's locks.
 *
 *  -  StackManager is per CPU.  Keep one instance inside each AlphaCPU
 *    object.  For SMP flushing (e.g., on INIT), simply clear() each manager
 *    under the global emulator pause.
 * ============================================================= */

#endif // StackManager_h__
