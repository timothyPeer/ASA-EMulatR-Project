#pragma once
#include <QObject>
#include "mmiohandler.h"

struct Region
{
    quint64 start;
    quint64 end;
    MmioHandler *handler;
};