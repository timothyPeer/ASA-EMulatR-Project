#pragma once
#include "enumerations/enumPALFunctionClass.h"
#include <QtCore>

// Global helper functions for PAL classification
PALFunctionClass classifyPALFunction(quint32 function);
int estimatePALCycles(quint32 function, PALFunctionClass classification);
bool requiresSystemBarrier(quint32 function, PALFunctionClass classification);
