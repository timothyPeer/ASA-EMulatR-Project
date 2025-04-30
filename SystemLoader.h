#pragma once
#ifndef SYSTEMLOADER_H
#define SYSTEMLOADER_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QString>
#include <QList>
#include <QDebug>
#include "alphasmpmanager.h"

class AlphaSMPManager; // Forward declaration

/**
 * @brief SystemLoader loads emulator configuration from JSON file.
 * Supports: System RAM, CPUs, ROMs, Serial Lines, NICs, SCSI Controllers.
 */
class SystemLoader : public QObject
{
    Q_OBJECT

public:
    explicit SystemLoader(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    bool loadConfiguration(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "[SystemLoader] Failed to open configuration file:" << filePath;
            return false;
        }

        QByteArray data = file.readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "[SystemLoader] JSON parse error:" << parseError.errorString();
            return false;
        }

        if (!doc.isObject()) {
            qWarning() << "[SystemLoader] Root configuration is not a JSON object.";
            return false;
        }

        rootConfig = doc.object();

        // Load basic system settings
        QJsonObject system = rootConfig.value("System").toObject();
        QJsonObject ram = system.value("RAM").toObject();
        ramSizeMB = ram.value("size").toInt(512); // Default 512MB

        QJsonObject cpu = system.value("CPU").toObject();
        cpuCount = cpu.value("Processor-Count").toInt(1);
        coherencyCacheKB = cpu.value("Coherency-Cache").toInt(2048);
        jitEnabled = cpu.value("JIT").toBool(true);
        jitThreshold = cpu.value("JIT-Threshold").toInt(50);
        jitOptimizationLevel = cpu.value("JIT-Opt-Level").toInt(2);
        QJsonObject session = system.value("Session-Log").toObject();
        sessionLogFileName = session.value("fName").toString();
        sessionLogMethod = session.value("Method").toString();
        hardwareModel = session.value("hw-Model").toString();
        hardwareSerialNumber = session.value("hw-Serial-Number").toString();

        // Load ROM settings
        QJsonObject rom = rootConfig.value("ROM").toObject();
        romFileName = rom.value("fName").toString();
        srmRomFileName = rom.value("fName-SRM").toString();
        nvramFileName = rom.value("Cmos-NVRam-FileName").toString();

        // Load Interfaces (Serial-Lines)
        QJsonObject interfaces = rootConfig.value("Interfaces").toObject();
        QJsonArray serialArray = interfaces.value("Serial-Lines").toArray();
        for (const auto& entry : serialArray) {
            serialLines.append(entry.toObject());
        }

        // Load IO Manager
        QJsonObject ioManager = rootConfig.value("IO-Manager").toObject();
        ioThreadCount = ioManager.value("ThreadCnt").toInt(1);

        // Load Network interfaces (DE500, DE602)
        QJsonObject network = ioManager.value("Network").toObject();
        QJsonArray de500 = network.value("DE500").toArray();
        for (const auto& entry : de500) {
            networkInterfaces.append(entry.toObject());
        }
        QJsonArray de602 = network.value("DE602").toArray();
        for (const auto& entry : de602) {
            networkInterfaces.append(entry.toObject());
        }

        // Load Storage-Controllers
        QJsonObject storageControllersObj = ioManager.value("Storage-Controllers").toObject();
        QJsonObject kzPba = storageControllersObj.value("KZPBA").toObject();
        QJsonArray deviceArray = kzPba.value("Devices").toArray();
        for (const auto& device : deviceArray) {
            storageControllers.append(device.toObject());
        }

       

        return true;
    }

    void applyConfiguration(AlphaSMPManager* smpManager);

    quint8 getTraceLevel() { return traceLevel; }
private:
    QJsonObject rootConfig;

    // Basic system parameters
    quint64 ramSizeMB = 0;
    int cpuCount = 1;
    int coherencyCacheKB = 0;

    int traceLevel = 0; // Default = 0 = no trace

    QString sessionLogFileName;
    QString sessionLogMethod;
    QString hardwareModel;
    QString hardwareSerialNumber;
    bool jitEnabled;
    int jitThreshold;
    
    /*
	Optimization Level	Code Generation Features
		0	No JIT, interpreter only
		1	Basic block compilation(no inlining, limited scheduling)
		2	Peephole optimizations, register allocation, constant folding
		3 + Loop unrolling, function inlining, LICM, vectorization, etc.
    */
    int jitOptimizationLevel=2;                         // Optimization Level

    QString romFileName;
    QString srmRomFileName;
    QString nvramFileName;

    int ioThreadCount = 1;

    QList<QJsonObject> serialLines;
    QList<QJsonObject> networkInterfaces;
    QList<QJsonObject> storageControllers;
};

#endif // SYSTEMLOADER_H