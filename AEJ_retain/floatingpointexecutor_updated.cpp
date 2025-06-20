#include "floatingpointexecutor.h"


void FloatingPointExecutor::setFPCR(FpcrRegister fpcr)
{
    this->fpcr = fpcr;
}

void FloatingPointExecutor::enableFloatingPoint(bool enabled)
{
    this->floatingPointEnabled = enabled;
}

void FloatingPointExecutor::resetState()
{
    // Reset internal state if necessary
    this->floatingPointEnabled = false;
    this->fpcr = FpcrRegister();
}
