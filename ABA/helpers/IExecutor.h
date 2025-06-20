#pragma once
#include "../structs/operateInstruction.h"


class  IExecutor {
	virtual void execute(OperateInstruction& inst) = 0;

};