#pragma once
// Protection flags for virtual memory
enum class ProtectionFlags
{
    Read = 0x1,
    Write = 0x2,
    Execute = 0x4
};
