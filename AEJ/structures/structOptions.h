#pragma once

// JIT optimization options
struct Options
{
    int optimizationLevel = 2;
    int traceCompilationThreshold = 50;
    int blockCompilationThreshold = 10;
    bool enableTraceCompilation = true;
};