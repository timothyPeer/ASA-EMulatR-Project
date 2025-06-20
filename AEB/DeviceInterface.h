#pragma once
#include <QString>
#include "BusInterface.h"
#include "IRQController.h"

class DeviceInterface : public BusInterface {
public:
	virtual ~DeviceInterface() {}

	virtual QString deviceName() const = 0;

	virtual void setIRQLine(IRQController* controller, int irqLine) {
		irqCtrl = controller;
		irqNumber = irqLine;
	}

protected:
	IRQController* irqCtrl = nullptr;
	int irqNumber = -1;
};

