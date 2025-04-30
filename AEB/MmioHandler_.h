#pragma once
#ifndef MmioHandler_h__
#define MmioHandler_h__

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QDebug>
#include <QtEndian>
#include <QByteArray>
#include <QBitArray>
#include "BaseDevice.h"
#include "IRQController.h"

/*
Interface Role : MMIOHandler

Interface				Description
MMIOHandler				Defines mmioRead(uint64_t) and mmioWrite(uint64_t, uint32_t) for devices mapped into MMIOManager
Implemented by			TulipNIC, UartDevice, VirtualScsiController, etc.
Used by					MMIOManager::mapDevice(MMIOHandler*, base, size)
*/

class MmioHandler {
public:
	virtual uint32_t mmioRead(uint64_t addr) = 0;
	virtual void mmioWrite(uint64_t addr, uint32_t value) = 0;
	virtual ~MmioHandler() = default;
};
#endif // MmioHandler_h__



