#pragma once

// JitLDWU.h
#pragma once
#include "Assembler.h"
#include "OperateInstruction.h"
#include "IExecutionContext.h"
#include "AlphaMemorySystem.h"

struct JitLDWU : JitLoadExecutor {
	void emit(Assembler& as,
		IExecutionContext* ctx,
		AlphaMemorySystem* memSystem,
		const OperateInstruction& op) const override
	{
		// 1) Compute VA = R[ra] + R[rb]
		as.emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, op.ra);
		as.emitAddRegReg(HostReg::RAX, HostReg::GPR_BASE, op.rb);

		// 2) Call memSystem->readVirtualMemory(cpu, VA, &temp, 2)
		//    -> Returns in RDI (bool) with RAX holding temp if true
		//    Setup arguments (SysV ABI): 
		//    RDI = cpu*, RSI = VA, RDX = &temp, RCX = 2
		as.emitMovRegReg(HostReg::RDI, HostReg::GPR_BASE, /*cpu ptr slot*/  /* see your context layout */);
		as.emitMovRegReg(HostReg::RSI, HostReg::RAX, 0);    // RSI ? VA (already in RAX)
		// TODO: load &temp into RDX, load constant 2 into RCX...
		as.emitCall(reinterpret_cast<void(*)(void)>(
			&AlphaMemorySystem::readVirtualMemory),
			HostReg::RDI /*actually uses all args*/);

		// 3) Test the boolean return, trap on failure
		as.emitTestRegReg(HostReg::RAX, HostReg::RAX);
		as.emitJcc(Condition::EQ, "ldwu_trap");

		// 4) On success: ZEXT16(temp) ? RDX
		as.emitMovzxRegMem(HostReg::RDX, HostReg::RAX, /*disp=*/0, /*bits=*/16);

		// 5) Write back: R[rc] = RDX
		as.emitStoreRegMem(HostReg::RDX, HostReg::GPR_BASE, op.rc, /*bits=*/64);

		// 6) Notify memory?access hook
		as.emitCall(reinterpret_cast<void(*)(uint64_t, uint64_t, bool)>(
			&IExecutionContext::notifyMemoryAccessed),
			HostReg::RAX /*addr in RAX, value in RDX,...*/);

		// 7) Jump over trap handler
		as.emitJmp("ldwu_done");

		// Trap path:
		as.bindLabel("ldwu_trap");
		as.emitCall(reinterpret_cast<void(*)(helpers_JIT::TrapType)>(
			&IExecutionContext::notifyTrapRaised),
			/*TrapType::MMUAccessFault*/);

		// Continue:
		as.bindLabel("ldwu_done");
	}
};
