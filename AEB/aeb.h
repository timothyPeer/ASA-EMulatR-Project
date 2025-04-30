#pragma once

#include "aeb_global.h"

/*
								+-------------------+
								|   EmulatorManager |
								+-------------------+
										  |
				  +-----------------------+-------------------------+
				  |                                                 |
		   +--------------+                              +-----------------+
		   | SystemConfig |                              | SystemBus       |
		   +--------------+                              +-----------------+
				  |                                                 |
				  |                        maps devices via         |
				  |<--------------------------------------------+   |
				  |                                             |   |
				  v                                             v   v
	   +------------------+                            +---------------------+
	   |  DeviceManager   |                            |     MMIOManager     |
	   +------------------+                            +---------------------+
				  |                                             |
				  |    .addDevice(), .getDevice()               |   maps MMIO regions
				  |                                             |<------------------------------+
				  v                                             |                               |
	   +---------------------+                                  |                               |
	   |   DeviceInterface   |<----------------------------+     |                               |
	   +---------------------+                             |     |                               |
				  ^                                        |     v                               v
	   +----------+-----------+                            |  +-----------------+    +----------------+
	   |                      |                            |  |   IRQController |    |  BusInterface  |
	   |                      |                            |  +-----------------+    +----------------+
+----------------+    +----------------+                   |          ^                       ^
|    TulipNIC    |    |   UartDevice   |<------+           |    connectsIRQ()           attachesToBus()
+----------------+    +----------------+       |           |                               |
	   |     ^             |           ^       |           |                               |
	   |     |             |           |       |           |                               |
	   |     |    inherits from        |       |           |                               |
	   |     +-------------------------+       +-----------+-------------------------------+
	   |                |                          inherits from
	   |         +---------------+
	   |         |  MMIOHandler  |<-----------------------------------+
	   |         +---------------+                                    |
	   |                      ^                                      |
	   |                      |   .mmioRead(), .mmioWrite()          |
	   +-------------------------------------------------------------+
							   inherits from

Also inherits:
+----------------+
|  BaseDevice    |
+----------------+

Legend:
-------
 ->  : method call or control path
 <-  : association / ownership
 <>  : inheritance



			   Interface	Purpose
DeviceInterface	Abstract base for all emulated devices (read/write/desc)
BaseDevice	Optional common behavior holder (reset/init/common I/O)
MMIOManager	Registers devices by memory region
IRQController	Routes and signals interrupt lines
BusInterface	Represents attachment of device to a physical/logical bus
DeviceManager	Holds and initializes DeviceInterface* devices
SystemBus	Maps device address ranges to MMIOManager
UartDevice	Emulated serial port, implements DeviceInterface
TulipNIC	Emulated DEC 21140 Ethernet, implements MmioHandler, DeviceInterface




Summary of Relationships

Class	Inherits From	Interfaces With
TulipNIC	DeviceInterface, MMIOHandler	MMIOManager, IRQController
UartDevice	DeviceInterface, BaseDevice	IRQController, BusInterface (likely)
DeviceManager	--	Manages all DeviceInterface instances
MMIOManager	--	Maps only MMIOHandler devices
IRQController	--	Handles device interrupt signaling
*/

/*

Clean separation of responsibilities across:

SystemBus (for mapping)
MMIOManager (for routing/dispatch)
IRQController (for signaling)
Inherits from QObject, enabling Qt signal/slot usage.

Internally registers all devices with:

MMIOManager for memory-mapped I/O
SystemBus for structural management
IRQController if canInterrupt() is true




*/

/*
* Json Snippet
{
  "id": "PKA0",
  "type": "SCSI",
  "base": "0x20000000",
  "size": 4096,
  "irq": 50
}
*/