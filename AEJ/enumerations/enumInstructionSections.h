#pragma once
/**
 * Main opcode sections in Alpha architecture
 */
enum class InstructionSections
{
    SECTION_INTEGER = 0x10,
    SECTION_FLOATING_POINT = 0x11,
    SECTION_VECTOR = 0x12,
    SECTION_CONTROL = 0x1A,
    SECTION_PAL = 0x18
};