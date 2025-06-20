#pragma once
#include "structs/operateInstruction.h"
#include "structs/memoryInstruction.h"


class  IExecutor {

	virtual void execute(const OperateInstruction& inst) = 0;
	virtual void execute(const MemoryInstruction& inst) = 0;
	virtual void execute(const BranchInstruction& inst) = 0;

};