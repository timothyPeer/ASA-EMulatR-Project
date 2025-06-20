#pragma once
#include "../AESH/TraceManager.h"

#define TRACE_LOG(msg)     if (TraceManager::instance().isLevelEnabled(0)) TraceManager::instance().trace(msg)
#define DEBUG_LOG(msg)     if (TraceManager::instance().isLevelEnabled(1)) TraceManager::instance().debug(msg)
#define INFO_LOG(msg)      if (TraceManager::instance().isLevelEnabled(2)) TraceManager::instance().info(msg)
#define WARN_LOG(msg)	   if (TraceManager::instance().isLevelEnabled(3)) TraceManager::instance().warn(msg)
#define ERROR_LOG(msg)     if (TraceManager::instance().isLevelEnabled(4)) TraceManager::instance().error(msg)
#define CRITICAL_LOG(msg)  if (TraceManager::instance().isLevelEnabled(5)) TraceManager::instance().critical(msg)

#define MEM_BARRIER()   std::atomic_thread_fence(std::memory_order_seq_cst)
#define MEM_WMB()       std::atomic_thread_fence(std::memory_order_release)
#define MEM_RMB()       std::atomic_thread_fence(std::memory_order_acquire)
#define TRAP_BARRIER()  std::atomic_thread_fence(std::memory_order_seq_cst)




//Alpha the Global‑Pointer register (GP, used by PALcode WRKGP/RSKGP) 
static constexpr unsigned KERNEL_GP_INDEX = 27;
constexpr int kernelGpIndex = 29;  // R29 is typically used for GP
