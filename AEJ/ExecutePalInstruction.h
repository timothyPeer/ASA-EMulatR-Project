#pragma once
#pragma once

#include "AlphaPlatformGuards.h"
#include <QtGlobal>

/**
 * @brief Executes a platform-specific PAL instruction
 *
 * This function is used by the instruction pipeline to handle PAL instructions
 * based on the currently selected Alpha platform (Tru64, OpenVMS, Windows NT, SRM/Linux).
 *
 * @param instruction The PAL instruction to execute
 * @param cpu Pointer to the AlphaCPU instance
 * @return true if the instruction was handled, false otherwise
 */
bool ExecutePalInstruction(quint32 instruction, class AlphaCPU *cpu);

