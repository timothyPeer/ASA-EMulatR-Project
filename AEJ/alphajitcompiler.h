// AlphaJITCompiler.h
#pragma once

#ifndef ALPHA_JIT_COMPILER_H
#define ALPHA_JIT_COMPILER_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <functional>
#include "RegisterBank.h"
#include "FpRegisterBankcls.h"
#include "helpers.h"
#include "AlphaJITProfiler.h"
#include "safememory.h"

/**
 * @brief Just-In-Time (JIT) compiler for Alpha instruction blocks with profiling.
 *
 * Integrates runtime profiling (hit counters) to auto-trigger compilation of hot blocks,
 * plus a simple branch predictor for future execution hints.
 */
class AlphaJITCompiler : public QObject {
    Q_OBJECT

public:
    using BlockFunc = std::function<quint64()>;

	explicit AlphaJITCompiler(RegisterBank* intRegs,
		FpRegisterBankcls* fpRegs,
		SafeMemory* memory,
		QObject* parent = nullptr);

    /**
     * @brief Get or compile a block at `pc` based on hit counters.
     */
    BlockFunc compileOrGetBlock(quint64 pc);
    // compile the instruction to a block (Legacy Support)  
    inline BlockFunc compileBlock(quint64 pc) { return compileOrGetBlock(pc); }

	// In AlphaJITCompiler.h, public:
	/// Returns true if we have a compiled lambda cached
	bool hasBlock(quint64 pc) const {
		QMutexLocker l(&cacheMutex);
		return cache.contains(pc);
	}

	/// Run the compiled block (must exist) and return next PC
	quint64 runBlock(quint64 pc) {
		return compileBlock(pc)();    // compileBlock is our alias for compileOrGetBlock
	}


    /**
     * @brief Execute a block (interpreted or JIT-compiled).
     */
    quint64 executeBlock(quint64 pc);

private:
    RegisterBank* integerRegs;                  ///< Integer registers & memory accessor
    FpRegisterBankcls* floatingRegs;               ///< Floating-point registers
    SafeMemory* memory;

    AlphaJITProfiler profiler;                  ///< Profiler (hot threshold)
    QHash<quint64, quint64> hitCounters;        ///< Execution count per PC

    QHash<quint64, BlockFunc> cache;            ///< Cached JIT-compiled blocks
    QMutex cacheMutex;                          ///< Protects cache

    QHash<quint64, quint64> branchPredictor;    ///< Last observed branch target

    /**
     * @brief Create a JIT-compiled block lambda for `pc`.
     */
    BlockFunc createJITBlock(quint64 pc);

    /**
     * @brief Create an interpreter-only block lambda for `pc`.
     */
    BlockFunc createInterpreterBlock(quint64 pc);

    /**
     * @brief Fallback interpreter for a block starting at `pc`.
     */
    quint64 interpretBlock(quint64 pc);

    /**
     * @brief Update branch predictor with actual target for block at `pc`.
     */
    void updateBranchPredictor(quint64 pc, quint64 actualTarget);
};
#endif // ALPHA_JIT_COMPILER_H


