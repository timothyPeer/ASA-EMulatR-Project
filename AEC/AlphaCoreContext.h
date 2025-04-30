#pragma once
#include "FpRegisterBankcls.h"
#include "RegisterBank.h"

#ifndef AlphaCoreContext_h__
#define AlphaCoreContext_h__


// AlphaCoreContext.h (or inside AlphaCPU.h if local to the class)

struct AlphaCoreContext {
	RegisterBank intRegs;
	FpRegisterBankcls fpRegs;
	quint64 pc = 0;
	quint64 fpcr = 0;
};

#endif // AlphaCoreContext_h__


