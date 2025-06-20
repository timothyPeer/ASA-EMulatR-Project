#include "systembus.h"



// void SystemBus::mapDevice(BusInterface* device, quint64 base, quint64 size)
// {
// 	Q_ASSERT(device);
// 
// 	DeviceMapping m;
// 	m.startAddr = base;
// 	m.endAddr = base + size - 1;
// 	m.device = device;
// 	mappings.append(m);
// 
// 	qDebug() << "[SystemBus] Mapped device" << device->identifier()
// 		<< "to" << QString("0x%1 - 0x%2").arg(base, 0, 16).arg(base + size - 1, 0, 16);
// }
