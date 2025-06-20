#include "alphacoreadapter.h"

AlphaCoreAdapter::AlphaCoreAdapter(QObject *parent)
	: QObject(parent)
{}

AlphaCoreAdapter::~AlphaCoreAdapter()
{}

void AlphaCoreAdapter::connectSignals() {
	if (!cpu) return;

	// Signal: Instruction executed — log or forward to UI
	connect(cpu, &AlphaCoreContext::instructionExecuted,
		this, [this](quint64 pc, quint32 instruction) {
			qDebug() << "[Adapter] Instruction executed:"
				<< QString("PC=0x%1").arg(pc, 8, 16)
				<< QString("INST=0x%1").arg(instruction, 8, 16);
			emit instructionExecuted(pc, instruction);
		});

	// Signal: Trap raised — notify debugger or halt system
	connect(cpu, &AlphaCoreContext::trapRaised,
		this, [this](AlphaCoreContext::TrapType type) {
			qDebug() << "[Adapter] Trap raised:" << cpu->trapTypeToString(type);
			emit trapRaised(static_cast<int>(type), cpu->getPC());
		});

	// Signal: CPU halted
	connect(cpu, &AlphaCoreContext::halted,
		this, [this]() {
			qDebug() << "[Adapter] CPU halted.";
			emit stateChanged(); // optionally trigger UI refresh
		});
}