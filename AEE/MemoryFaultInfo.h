#pragma once
#include <QtGlobal>
#include "../AEJ/helpers/helperSignExtend.h"
#include "../AEJ/enumerations/enumMemoryFaultType.h"


/* ── Fault descriptor ─────────────────────────────────────────────────── */
struct MemoryFaultInfo
{
	MemoryFaultType faultType = MemoryFaultType::NONE;
	quint64         faultAddress = 0;
	quint64         physicalAddress = 0;
	int             accessSize = 0;
	bool            isWrite = false;
	bool            isExecute = false;
	quint64         pc = 0;
	quint32         instruction = 0;
};

