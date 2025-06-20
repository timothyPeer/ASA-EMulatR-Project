// JitLoadExecutors.h
#pragma once
#ifndef JIT_LOAD_EXECUTORS_H_
#define JIT_LOAD_EXECUTORS_H_

#include "Assembler.h"            // JIT code emission DSL
#include "../AEJ/IExecutionContext.h"
#include "../AEJ/AlphaMemorySystem_refactored.h"
#include "OperateInstruction.h"
#include <cstdint>

/**
 * @brief Base interface for JIT emission of load instructions.
 * @see Alpha AXP System Reference Manual, Version 6,
 *      Section 4.2.2 (LDL) :contentReference[oaicite:0]{index=0}
 *      Section 4.2.4 (LDL_L) :contentReference[oaicite:1]{index=1}
 */
struct JitLoadExecutor {
    virtual ~JitLoadExecutor() = default;

    /**
     * @brief Emit host machine code for a single Alpha load.
     * @param as        The assembler abstraction.
     * @param ctx       Execution context (for register bases).
     * @param memSystem Memory system (for fault handling, if needed).
     * @param op        Decoded Alpha OperateInstruction (ra, rb, rc, disp).
     */
    virtual void emit(Assembler& as,
        IExecutionContext* ctx,
        AlphaMemorySystem* memSystem,
        const OperateInstruction& op) const = 0;
};

/**
 * @brief JIT emitter for LDL (Load Sign-Extended Longword).
 *        Function code: 0x28 :contentReference[oaicite:2]{index=2}
 */
struct JitLDL : JitLoadExecutor {
    void emit(Assembler& as,
        IExecutionContext* ctx,
        AlphaMemorySystem* memSystem,
        const OperateInstruction& op) const override
    {
        // 1) Compute effective VA = GPR[ra] + GPR[rb]
        as.emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, op.ra);
        as.emitAddRegReg(HostReg::RAX, HostReg::GPR_BASE, op.rb);

        // 2) Load signed 32-bit from [RAX + disp]
        as.emitMovsxRegMem(HostReg::RDX, HostReg::RAX, op.disp, 32);

        // 3) Write back to GPR[rc], sign-extended to 64 bits
        as.emitMovRegReg(HostReg::GPR_BASE, op.rc, HostReg::RDX);
    }
};

/**
 * @brief JIT emitter for LDL_L (Load Sign-Extended Longword Locked).
 *        Function code: 0x2A :contentReference[oaicite:3]{index=3}
 * @note  Same as LDL, but sets the per-processor lock flag and records
 *        the locked physical address for a subsequent STx_C.
 */
struct JitLDL_L : JitLoadExecutor {
    void emit(Assembler& as,
        IExecutionContext* ctx,
        AlphaMemorySystem* memSystem,
        const OperateInstruction& op) const override
    {
        // Address computation + signed load, same as JitLDL
        as.emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, op.ra);
        as.emitAddRegReg(HostReg::RAX, HostReg::GPR_BASE, op.rb);
        as.emitMovsxRegMem(HostReg::RDX, HostReg::RAX, op.disp, 32);

        // Record the locked physical address in AlphaCPU::lock_state
        as.emitCall(&AlphaCPU::recordLoadLocked, HostReg::RAX);

        // Write result back to GPR[rc]
        as.emitMovRegReg(HostReg::GPR_BASE, op.rc, HostReg::RDX);
    }
};

/**
 * @brief Cacheable table of JIT load executors, indexed by 6-bit func code.
 *        func 0x28?LDL, 0x2A?LDL_L :contentReference[oaicite:4]{index=4}
 */
static const JitLoadExecutor* jitLoadHandlers[64] = {
    /* … other entries … */
    [0x28] = &JitLDL_Instance,
    [0x2A] = &JitLDL_L_Instance,
    /* … */
};

#endif // JIT_LOAD_EXECUTORS_H_
