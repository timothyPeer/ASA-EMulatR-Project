// IntelHexLoader.h
#pragma once

#include <QString>
#include <QByteArray>

class SafeMemory;



class IntelHexLoader {
public:
	/// Load an Intel-HEX file and write it into `memory` at `loadBase`.
	/// Returns true on success, false on parse-error or I/O error.
	static bool loadHexFile(const QString& filePath,
		SafeMemory* memory,
		quint64       loadBase = 0);

private:
	/// Parse one line of Intel-HEX into its components.
	static bool parseLine(const QString& line,
		quint8& byteCount,
		quint16& address,
		quint8& recordType,
		QByteArray& data);
};


/*
* 
* ach file pair corresponds to a different module:

hpmrom.hex / hpmfsrom.hex – Host Processor Module firmware
pbmrom.hex / pbmfsrom.hex – PCI-Bus Module firmware
psmrom.hex / psmfsrom.hex – Personal Station Module firmware
scmrom.hex / scmfsrom.hex – System Module firmware
wf_xsrom.hex – Wildfire or XStation firmware

// choose one of your .hex variants:
QString hexPath = ":/firmware/hpmfsrom.hex";  // or filesystem path
quint64 loadBase = 0xC0000000ULL;            // SRM area
if (!IntelHexLoader::loadHexFile(hexPath, safeMemory, loadBase)) {
	qFatal("Failed to load PALcode HEX");
}

// then reset the CPU into the firmware at its known entry point:
cpu->setPC(loadBase + 0x80);  // typical SRM reset vector

*/