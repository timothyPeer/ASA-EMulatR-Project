#pragma once
/**
 * Main entry point for the Alpha JIT System
 */

#include <QObject>
#include <QVector>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include "decodeOperate.h"

 /**
  * @brief AlphaInstruction - Represents a decoded Alpha instruction
  */

class AlphaInstruction {
public:

	AlphaInstruction() : opcode(0), functionCode(0), format(InstructionFormat::FORMAT_OPERATE) {}

	quint32 opcode;             // Main opcode
	quint32 functionCode;       // Function code (for operate format)
	QString mnemonic;           // Instruction mnemonic
	InstructionFormat format;   // Instruction format
	QVector<QString> operands;  // Operand types
	QString description;        // Instruction description
	QMap<QString, quint32> decodedOperands; // Decoded operand values

	QString toString() const
	{
		QString str = mnemonic;

		// Add operands
		QStringList opValues;
		for (const auto& op : operands) {
			if (decodedOperands.contains(op)) {
				opValues.append(QString("%1=%2").arg(op).arg(decodedOperands[op]));
			}
			else {
				opValues.append(op);
			}
		}

		if (!opValues.isEmpty()) {
			str += " " + opValues.join(", ");
		}

		return str;
	}
};