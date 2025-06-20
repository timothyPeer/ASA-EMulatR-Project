#pragma once

#include <QObject>
#include <QVector>
#include <array>
#include <QDebug>

class VectorRegisterBank : public QObject {
	Q_OBJECT

public:
	explicit VectorRegisterBank(QObject* parent = nullptr) : QObject(parent) {
		vectorRegisters.resize(32); // V0–V31
		for (auto& reg : vectorRegisters) {
			reg = { 0, 0 };
		}
	}

	void writeVec(quint8 regIndex, const std::array<quint64, 2>& value) {
		if (regIndex >= vectorRegisters.size()) {
			qWarning() << "[VectorRegisterBank] writeVec: Invalid index" << regIndex;
			return;
		}
		vectorRegisters[regIndex] = value;
	}

	std::array<quint64, 2> readVec(quint8 regIndex) const {
		if (regIndex >= vectorRegisters.size()) {
			qWarning() << "[VectorRegisterBank] readVec: Invalid index" << regIndex;
			return { 0, 0 };
		}
		return vectorRegisters[regIndex];
	}

	void writeVecLane(quint8 regIndex, int lane, quint64 value) {
		if (regIndex >= vectorRegisters.size() || lane < 0 || lane >= 2) {
			qWarning() << "[VectorRegisterBank] writeVecLane: Invalid access to V" << regIndex << " lane " << lane;
			return;
		}
		vectorRegisters[regIndex][lane] = value;
	}

	quint64 readVecLane(quint8 regIndex, int lane) const {
		if (regIndex >= vectorRegisters.size() || lane < 0 || lane >= 2) {
			qWarning() << "[VectorRegisterBank] readVecLane: Invalid access to V" << regIndex << " lane " << lane;
			return 0;
		}
		return vectorRegisters[regIndex][lane];
	}

private:
	QVector<std::array<quint64, 2>> vectorRegisters;
};

