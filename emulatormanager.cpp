// EmulatorManager.cpp
#include "EmulatorManager.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "Helpers.h"
#include "AEB/devicemanager.h"
#include "AEC/AlphaCoreContext.h"

EmulatorManager::EmulatorManager(QObject* parent)
	: QObject(parent), state(EmulationState::Uninitialized)
{
	qDebug() << "EmulatorManager: Created";
}

EmulatorManager::~EmulatorManager()
{
	// Ensure all resources are properly cleaned up
	cleanup();
}

bool EmulatorManager::initialize(quint64 memorySize, int cpuCount)
{
	if (state != EmulationState::Uninitialized) {
		qWarning() << "EmulatorManager: Already initialized";
		return false;
	}
	// AlphaSMPManager -
	m_smpManager.reset(new AlphaSMPManager(cpuCount));



// 	Create core components
// 	irqController.reset(new IRQController());
// 	irqController->initialize(cpuCount);
// 
// 	systemBus.reset(new SystemBus());
// 
// 	mmioManager.reset(new MMIOManager(irqController.data()));
// 
// 	memory.reset(new SafeMemory(mmioManager.data(), memorySize));
// 
// 	deviceManager.reset(new DeviceManager(mmioManager.data(), irqController.data()));
// 
	// Create CPUs and their threads
//	createCPUThreads(cpuCount);

// 	Initialize default devices
// 		deviceManager->initializeDefaultDevices();

	state = EmulationState::Initialized;
	emit statusChanged("System initialized");

	qDebug() << "EmulatorManager: Initialized with" << cpuCount << "CPUs and"
		<< memorySize << "bytes of memory";
	return true;
}

void EmulatorManager::initialize_signalsAndSlots() {


}

bool EmulatorManager::start()
{
	if (state != EmulationState::Initialized && state != EmulationState::Stopped) {
		qWarning() << "EmulatorManager: Cannot start from current state";
		return false;
	}

	// Start all CPU threads
	for (int i = 0; i < cpus.size(); ++i) {
		cpus[i]->run();
	}

	state = EmulationState::Running;
	emit emulationStarted();
	emit statusChanged("Emulation started");

	qDebug() << "EmulatorManager: Emulation started";
	return true;
}

void EmulatorManager::pause()
{
	if (state != EmulationState::Running) {
		qWarning() << "EmulatorManager: Not running, cannot pause";
		return;
	}

	// Pause all CPUs
	for (int i = 0; i < cpus.size(); ++i) {
		cpus[i]->requestStop();
	}

	// Wait for all CPU threads to stop
	for (int i = 0; i < cpuThreads.size(); ++i) {
		cpuThreads[i]->wait(1000);
	}

	state = EmulationState::Paused;
	emit emulationPaused();
	emit statusChanged("Emulation paused");

	qDebug() << "EmulatorManager: Emulation paused";
}

void EmulatorManager::resume()
{
	if (state != EmulationState::Paused) {
		qWarning() << "EmulatorManager: Not paused, cannot resume";
		return;
	}

	// Resume all CPUs
	for (int i = 0; i < cpus.size(); ++i) {
		cpus[i]->run();
	}

	state = EmulationState::Running;
	emit emulationResumed();
	emit statusChanged("Emulation resumed");

	qDebug() << "EmulatorManager: Emulation resumed";
}

void EmulatorManager::stop()
{
	if (state != EmulationState::Running && state != EmulationState::Paused) {
		qWarning() << "EmulatorManager: Not running or paused, cannot stop";
		return;
	}

	// Stop all CPUs
	for (int i = 0; i < cpus.size(); ++i) {
		cpus[i]->requestStop();
	}

	// Wait for all CPU threads to stop
	for (int i = 0; i < cpuThreads.size(); ++i) {
		cpuThreads[i]->wait(1000);
	}

	state = EmulationState::Stopped;
	emit emulationStopped();
	emit statusChanged("Emulation stopped");

	qDebug() << "EmulatorManager: Emulation stopped";
}

void EmulatorManager::reset()
{
	// First stop emulation if it's running
	if (state == EmulationState::Running || state == EmulationState::Paused) {
		stop();
	}

// 	Reset all CPUs
// 		for (int i = 0; i < cpus.size(); ++i) {
// 			cpus[i]->reset();
// 		}

	this->m_smpManager.data()->reset();
	// Reset devices
	// Typically, we would loop through devices and call a reset method

	state = EmulationState::Initialized;
	emit statusChanged("System reset");

	qDebug() << "EmulatorManager: System reset";
}

// AlphaCoreContext* EmulatorManager::getCPU(int index) const
// {
// 	if (index < 0 || index >= cpus.size()) {
// 		return nullptr;
// 	}
// 	return cpus[index];
// }

// SafeMemory* EmulatorManager::getMemory() const
// {
// 	return memory.data();
// }
// 
// DeviceManager* EmulatorManager::getDeviceManager() const
// {
// 	return deviceManager.data();
// }
// 
// MMIOManager* EmulatorManager::getMMIOManager() const
// {
// 	return mmioManager.data();
// }
// 
// IRQController* EmulatorManager::getIRQController() const
// {
// 	return irqController.data();
// }

bool EmulatorManager::loadProgram(const QString& filename, quint64 loadAddress, bool setCPUPC)
{
	if (!memory) {
		qWarning() << "EmulatorManager: No memory system available";
		return false;
	}

	if (!memory->loadBinary(filename, loadAddress)) {
		qWarning() << "EmulatorManager: Failed to load program" << filename;
		return false;
	}

	if (setCPUPC && !cpus.isEmpty()) {
		// Set PC of first CPU to the load address
		cpus[0]->setPC(loadAddress);
		qDebug() << "EmulatorManager: Set CPU 0 PC to" << QString("0x%1").arg(loadAddress, 0, 16);
	}

	emit statusChanged(QString("Program loaded: %1").arg(filename));
	return true;
}

bool EmulatorManager::saveState(const QString& filename)
{
	QJsonObject state;
	QJsonArray cpuStates;

	// Save all CPU states
	for (int i = 0; i < cpus.size(); ++i) {
		AlphaCPUState cpuState = cpus[i]->captureState();
		// Convert CPU state to JSON
		QJsonObject cpuJson = cpuState.toJson();
		cpuStates.append(cpuJson);
	}

	state["cpuStates"] = cpuStates;

	// Save memory state (this would be large, so in practice might use
	// a binary format or compression)

	// Write to file
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "EmulatorManager: Failed to open save state file" << filename;
		return false;
	}

	QJsonDocument doc(state);
	file.write(doc.toJson());
	file.close();

	emit statusChanged(QString("State saved to %1").arg(filename));
	qDebug() << "EmulatorManager: State saved to" << filename;
	return true;
}

bool EmulatorManager::loadState(const QString& filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning() << "EmulatorManager: Failed to open save state file" << filename;
		return false;
	}

	QByteArray data = file.readAll();
	file.close();

	QJsonDocument doc = QJsonDocument::fromJson(data);
	if (doc.isNull() || !doc.isObject()) {
		qWarning() << "EmulatorManager: Invalid save state format";
		return false;
	}

	QJsonObject state = doc.object();

	// Load CPU states
	QJsonArray cpuStates = state["cpuStates"].toArray();
	if (cpuStates.size() != cpus.size()) {
		qWarning() << "EmulatorManager: CPU count mismatch in save state";
		return false;
	}

	for (int i = 0; i < cpus.size(); ++i) {
		AlphaCPUState cpuState = AlphaCPUState::fromJson(cpuStates[i].toObject());
		cpus[i]->applyState(cpuState);
	}

	// Load memory state (would need to handle binary data)

	emit statusChanged(QString("State loaded from %1").arg(filename));
	qDebug() << "EmulatorManager: State loaded from" << filename;
	return true;
}

void EmulatorManager::setCPUSpeed(int mips)
{
	//TODO setCPUSpeed(int mips)
}

// QString EmulatorManager::getStatus() const
// {
// 	QString statusText;
// 
// 	switch (state) {
// 	case EmulationState::Uninitialized:
// 		statusText = "Not initialized";
// 		break;
// 	case EmulationState::Initialized:
// 		statusText = "Initialized, ready to run";
// 		break;
// 	case EmulationState::Running:
// 		statusText = "Running";
// 		break;
// 	case EmulationState::Paused:
// 		statusText = "Paused";
// 		break;
// 	case EmulationState::Stopped:
// 		statusText = "Stopped";
// 		break;
// 	default:
// 		statusText = "Unknown state";
// 		break;
// 	}
// 
// 	if (!cpus.isEmpty()) {
// 		// Include first CPU state in status
// 		AlphaCoreContext* cpu = cpus[0];
// 		statusText += QString(" | CPU0: PC=0x%1, Mode=%2")
// 			.arg(cpu->getPC(), 0, 16)
// 			.arg(static_cast<int>(cpu->currentMode()));
// 	}
// 
// 	return statusText;
// }

// AlphaCoreContext* EmulatorManager::createCPU(int cpuId)
// {
// 	AlphaCoreContext* cpu = new AlphaCoreContext(cpuId, memory.data(), systemBus.data(), irqController.data());
// 
// 	// Create and attach executors
// 	createExecutors(cpuId);
// 
// 	// Connect signals
// 	connect(cpu, &AlphaCoreContext::halted, this, [this, cpuId]() {
// 		qDebug() << "EmulatorManager: CPU" << cpuId << "halted";
// 		emit statusChanged(QString("CPU %1 halted").arg(cpuId));
// 		});
// 
// 	connect(cpu, &AlphaCoreContext::trapRaised, this, [this, cpuId](AlphaCoreContext::TrapType type) {
// 		qDebug() << "EmulatorManager: CPU" << cpuId << "raised trap:" << cpu->trapTypeToString(type);
// 		emit statusChanged(QString("CPU %1 trap: %2").arg(cpuId).arg(cpu->trapTypeToString(type)));
// 		});
// 
// 	cpus.append(cpu);
// 	return cpu;
// }

void EmulatorManager::createExecutors(int cpuId)
{
	// Create executors for the CPU
	RegisterBank* regBank = cpus[cpuId]->getIntegerBank();

	IntegerExecutor* intExec = new IntegerExecutor(cpus[cpuId], memory.data(), regBank);
	FloatingPointExecutor* fpExec = new FloatingPointExecutor(cpus[cpuId], memory.data(), regBank);
	VectorExecutor* vecExec = new VectorExecutor(cpus[cpuId], memory.data(), regBank);
	ControlExecutor* ctrlExec = new ControlExecutor(cpus[cpuId], memory.data(), regBank);

	// Store executors for cleanup
	intExecutors.append(intExec);
	fpExecutors.append(fpExec);
	vecExecutors.append(vecExec);
	ctrlExecutors.append(ctrlExec);

	// Attach executors to CPU
	cpus[cpuId]->attachExecutors(intExec, fpExec, vecExec, ctrlExec);
}

void EmulatorManager::createCPUThreads(int count)
{
	for (int i = 0; i < count; ++i) {
		// Create CPU
		AlphaCoreContext* cpu = createCPU(i);

		// Create thread for CPU
		QThread* thread = new QThread();
		cpu->moveToThread(thread);

		// Connect thread start/stop signals
		connect(thread, &QThread::started, cpu, &AlphaCoreContext::run);
		connect(cpu, &AlphaCoreContext::halted, thread, &QThread::quit);

		// Store thread
		cpuThreads.append(thread);
	}
}

void EmulatorManager::cleanup()
{
	// Stop emulation first
	if (state == EmulationState::Running || state == EmulationState::Paused) {
		stop();
	}

	// Delete CPU threads
	for (QThread* thread : cpuThreads) {
		if (thread->isRunning()) {
			thread->quit();
			thread->wait(1000);
		}
		delete thread;
	}
	cpuThreads.clear();

	// Delete CPUs
	qDeleteAll(cpus);
	cpus.clear();

	// Delete executors
	qDeleteAll(intExecutors);
	intExecutors.clear();

	qDeleteAll(fpExecutors);
	fpExecutors.clear();

	qDeleteAll(vecExecutors);
	vecExecutors.clear();

	qDeleteAll(ctrlExecutors);
	ctrlExecutors.clear();

	// QScopedPointer will handle the rest of the components

	state = EmulationState::Uninitialized;
}
