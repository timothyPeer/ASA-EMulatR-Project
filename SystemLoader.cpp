#include "SystemLoader.h"
#include "alphasmpmanager.h"


void SystemLoader::applyConfiguration(AlphaSMPManager* smpManager)
{
	if (!smpManager) {
		qWarning() << "[SystemLoader] SMP Manager pointer is null!";
		return;
	}

	// Step 1: Configure CPU and RAM
	smpManager->configureSystem(cpuCount, ramSizeMB, 0x21000000); // StartPC = 0x21000000 (PAL)

	// Step 2: Configure IO Thread Count
	smpManager->setIoThreadCount(ioThreadCount);

	// Step 3: Setup Session Logging (optional future)
	smpManager->setSessionLog(sessionLogFileName, sessionLogMethod);

	// Step 4: Set Hardware Model and Serial
	smpManager->setHardwareInfo(hardwareModel, hardwareSerialNumber);

	// Step 5: Load ROM and SRM images
	smpManager->setRomFile(romFileName);
	smpManager->setSrmFile(srmRomFileName);
	smpManager->setNvramFile(nvramFileName);

	// Step 6: Setup Serial Interfaces
	for (const auto& serialEntry : serialLines) {
		QString name = serialEntry.value("Name").toString();
		QString iface = serialEntry.value("iface").toString();
		QJsonObject netCfg = serialEntry.value("net-cfg").toObject();

		if (!netCfg.isEmpty()) {
			QString port = netCfg.value("Port").toString();
			QString application = netCfg.value("application").toString();
			smpManager->addSerialInterface(name, iface, port, application);
		}
		else {
			smpManager->addSerialInterface(name, iface);
		}
	}

	// Step 7: Setup Network Interfaces
	for (const auto& nicEntry : networkInterfaces) {
		QString name = nicEntry.value("name").toString();
		QString iface = nicEntry.value("iface").toString();
		smpManager->addNetworkInterface(name, iface);
	}

	// Step 8: Setup Storage Controllers (KZPBA / SCSI)
	for (const auto& storageEntry : storageControllers) {
		QString ctrlName = storageEntry.value("name").toString();
		int scsiId = storageEntry.value("scsi-id").toInt();

		QJsonArray units = storageEntry.value("units").toArray();
		QList<QPair<int, QString>> unitMappings;
		for (const auto& unitObj : units) {
			QJsonObject unit = unitObj.toObject();
			int unitId = unit.value("unit-id").toInt();
			QString location = unit.value("unit-location").toString();
			unitMappings.append(qMakePair(unitId, location));
		}

		smpManager->addScsiController(ctrlName, scsiId, unitMappings);
	}

	smpManager->setTraceLevel(traceLevel);
	qInfo() << "[SystemLoader] Configuration applied successfully.";
}
