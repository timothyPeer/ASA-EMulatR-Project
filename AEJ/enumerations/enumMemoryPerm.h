#pragma once
#include <QtCore>

enum class MemoryPerm : quint8
{
    Read = 1 << 0,
    Write = 1 << 1,
    Execute = 1 << 2,
    ReadWrite = Read | Write,
    ReadExec = Read | Execute,
    RWExec = Read | Write | Execute
};
