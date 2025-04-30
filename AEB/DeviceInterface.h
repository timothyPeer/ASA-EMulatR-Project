#pragma once
#include <QString>

#pragma once

class DeviceInterface {
public:
	virtual ~DeviceInterface() = default;

	virtual QString identifier() const = 0;
	virtual QString description() const = 0;
	virtual void reset() = 0;
};
