#pragma once

#include <QObject>
#include <Qlist>
#include <QMap>
#include <QFile>
#include <QIODevice>
#include <instructiondefinition.h>

// Collection of instruction definitions
class InstructionSet : public QObject
{
	Q_OBJECT

public:
	explicit InstructionSet(QObject* parent = nullptr) : QObject(parent) {}

	// Load from CSV string
	int loadFromCSV(const QString& csvData) {
		QStringList lines = csvData.trimmed().split('\n');
		if (lines.isEmpty())
			return 0;

		// Parse header
		QStringList header = lines[0].split(',');

		// Parse each line
		for (int i = 1; i < lines.size(); i++) {
			QString line = lines[i].trimmed();
			if (line.isEmpty())
				continue;

			QStringList fields = line.split(',');

			InstructionDefinition def = InstructionDefinition::fromCSV(fields, header);
			if (!def.mnemonic.isEmpty()) {
				instructionMap[def.mnemonic] = def;
				definitions.append(def);
			}
		}

		return definitions.size();
	}

	// Load from CSV file
	int loadFromFile(const QString& filePath) {
		QFile file(filePath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			qCritical() << "Failed to open instruction definitions file:" << filePath;
			return 0;
		}

		QString csvData = QString::fromUtf8(file.readAll());
		file.close();

		return loadFromCSV(csvData);
	}

	// Get definition by mnemonic
	InstructionDefinition getDefinition(const QString& mnemonic) const {
		return instructionMap.value(mnemonic);
	}

	// Check if mnemonic exists
	bool hasDefinition(const QString& mnemonic) const {
		return instructionMap.contains(mnemonic);
	}

	// Get all definitions by section
	QList<InstructionDefinition> getDefinitionsBySection(helpers_JIT::Section section) const {
		QList<InstructionDefinition> result;

		for (const InstructionDefinition& def : definitions) {
			if (def.section == section)
				result.append(def);
		}

		return result;
	}

	// Get all definitions by format
	QList<InstructionDefinition> getDefinitionsByFormat(helpers_JIT::Format format) const {
		QList<InstructionDefinition> result;

		for (const InstructionDefinition& def : definitions) {
			if (def.format == format)
				result.append(def);
		}

		return result;
	}

	// Get all definitions
	const QList<InstructionDefinition>& getAllDefinitions() const {
		return definitions;
	}

	// Get summary of instruction set
	QMap<QString, int> getSectionSummary() const {
		QMap<QString, int> summary;

		for (const InstructionDefinition& def : definitions) {
			QString sectionName = def.sectionToString(def.section);
			summary[sectionName] = summary.value(sectionName, 0) + 1;
		}

		return summary;
	}

	// Clear all definitions
	void clear() {
		instructionMap.clear();
		definitions.clear();
	}

	// Size of instruction set
	int size() const {
		return definitions.size();
	}

private:
	QMap<QString, InstructionDefinition> instructionMap;
	QList<InstructionDefinition> definitions;
};