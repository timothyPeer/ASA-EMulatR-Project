#include "alphajitsystem.h"
#include "Helpers.h"
#include <QString>
#include "alphatrace.h"
#include "AlphaBasicBlock.h"
#include <QVariantList>
#include <QVariantMap>
#include <QList>


AlphaJITSystem::AlphaJITSystem(const QVariantMap& customOptions /*= QVariantMap()*/) : QObject(nullptr)
{
	// Initialize options with defaults, then override with custom options
	if (customOptions.contains("optimizationLevel"))
		options.optimizationLevel = customOptions.value("optimizationLevel").toInt();

	if (customOptions.contains("traceCompilationThreshold"))
		options.traceCompilationThreshold = customOptions.value("traceCompilationThreshold").toInt();

	if (customOptions.contains("blockCompilationThreshold"))
		options.blockCompilationThreshold = customOptions.value("blockCompilationThreshold").toInt();

	if (customOptions.contains("enableTraceCompilation"))
		options.enableTraceCompilation = customOptions.value("enableTraceCompilation").toBool();

	// Initialize the execution engine
	engine = new AlphaJITExecutionEngine();
	// Assume options.optimizationLevel is of type AlphaJITCompiler::OptimizationLevel
	engine->getAlphaJITCompiler()->setOptimizationLevel(static_cast<int>(options.optimizationLevel));
	engine->getAlphaJITCompiler()->setOptimizationLevel(options.optimizationLevel);
	engine->getAlphaJITProfiler()->setHotThreshold(options.blockCompilationThreshold);
	engine->setTraceThreshold(options.traceCompilationThreshold);
}

QVector<InstructionDefinition> AlphaJITSystem::parseInstructionDefinitions(const QString& csv)
{
	QVector<InstructionDefinition> instructionDefs;
	QStringList lines = csv.trimmed().split('\n');
	QStringList header = lines[0].split(',');

	// Find the indices of the columns we're interested in
	int mnemonicIdx = header.indexOf("Mnemonic");
	int opcodeIdx = header.indexOf("Opcode (hex)");
	int functionIdx = header.indexOf("Function (hex)");
	int classIdx = header.indexOf("Class");
	int operandsIdx = header.indexOf("Operands");
	int descriptionIdx = header.indexOf("Description");

	if (mnemonicIdx == -1 || opcodeIdx == -1) {
		qCritical() << "CSV is missing required columns";
		throw std::runtime_error("CSV is missing required columns");
	}

	for (int i = 1; i < lines.size(); i++) {
		QString line = lines[i].trimmed();
		if (line.isEmpty()) continue;

		QStringList columns = line.split(',');

		QString mnemonic = (mnemonicIdx < columns.size()) ? columns[mnemonicIdx].trimmed() : QString();
		QString opcodeHex = (opcodeIdx < columns.size()) ? columns[opcodeIdx].trimmed() : QString();
		QString functionHex = (functionIdx < columns.size() && functionIdx >= 0) ? columns[functionIdx].trimmed() : QString();
		QString className = (classIdx < columns.size() && classIdx >= 0) ? columns[classIdx].trimmed() : QString();
		QString operandsStr = (operandsIdx < columns.size() && operandsIdx >= 0) ? columns[operandsIdx].trimmed() : QString();
		QString description = (descriptionIdx < columns.size() && descriptionIdx >= 0) ? columns[descriptionIdx].trimmed() : QString();

		if (!mnemonic.isEmpty() && !opcodeHex.isEmpty()) {
			InstructionDefinition def;
			def.mnemonic = mnemonic;
			def.opcode = opcodeHex.toInt(nullptr, 16);

			if (!functionHex.isEmpty()) {
				def.functionCode = functionHex.toInt(nullptr, 16);
			}
			else {
				def.functionCode = -1; // Null represented as -1
			}

			def.instructionClass = className;
			def.operands = operandsStr.split(' ');
			def.description = description;

			instructionDefs.append(def);

			// Also add to the map for quick lookup by mnemonic
			instructionMap[mnemonic] = def;
		}
	}

	return instructionDefs;
}

int AlphaJITSystem::loadInstructionDefinitions(const QVector<InstructionDefinition>& instructionDefs)
{
	// Clear existing instruction map
	instructionMap.clear();

	// Populate the instruction map
	for (const auto& def : instructionDefs) {
		instructionMap[def.mnemonic] = def;
	}

	qDebug() << "Loaded" << instructionDefs.size() << "instruction definitions";

	// Group instructions by section for summary
	QMap<QString, int> sectionCounts;

	for (const auto& def : instructionDefs) {
		QString section = def.instructionClass;
		if (def.description.contains('(')) {
			QRegularExpression rx("\\(([^)]+)\\)", QRegularExpression::CaseInsensitiveOption);
			QRegularExpressionMatch match = rx.match(def.description);

			if (match.hasMatch()) {

				QString section = match.captured(1);
				// Use 'section' as needed
				sectionCounts[section] = sectionCounts.value(section, 0) + 1;
			}

		}
	}

	qDebug() << "\nInstruction Set Summary:";
	for (auto it = sectionCounts.constBegin(); it != sectionCounts.constEnd(); ++it) {
		qDebug() << "-" << it.key() << ":" << it.value() << "instructions";
	}

	return instructionDefs.size();
}

InstructionDefinition AlphaJITSystem::getInstructionDefinition(const QString& mnemonic) const
{
	return instructionMap.value(mnemonic);
}

QVariantMap AlphaJITSystem::run(quint64 startAddress /*= 0*/, int maxInstructions /*= 1000000*/)
{
	qDebug() << "Starting execution at address 0x" + QString::number(startAddress, 16);
	qDebug() << "Optimization level:" << options.optimizationLevel;

	QElapsedTimer timer;
	timer.start();

	helpers_JIT::ExecutionResult result = engine->execute(startAddress, maxInstructions);

	qint64 executionTime = timer.elapsed();
	double instructionsPerMs = result.instructionsExecuted / static_cast<double>(executionTime);

	qDebug() << "\nExecution completed in" << executionTime << "ms";
	qDebug() << "Instructions executed:" << result.instructionsExecuted;
	qDebug() << "Performance:" << QString::number(instructionsPerMs, 'f', 2) << "instructions/ms";
	qDebug() << "Compiled blocks:" << result.compiledBlocks;

	if (options.enableTraceCompilation) {
		qDebug() << "Compiled traces:" << result.compiledTraces;
	}

	QVariantMap resultMap;
	resultMap["instructionsExecuted"] = result.instructionsExecuted;
	resultMap["finalPC"] = result.finalPC;
	resultMap["compiledBlocks"] = result.compiledBlocks;
	resultMap["compiledTraces"] = result.compiledTraces;
	resultMap["executionTime"] = executionTime;
	resultMap["instructionsPerMs"] = instructionsPerMs;

	// Convert registers to QVariantList
	QVariantList registersList;
	for (int reg : result.registers) {
		registersList.append(reg);
	}
	resultMap["registers"] = registersList;

	// Convert floating point registers to QVariantList
	QVariantList fpRegistersList;
	for (double reg : result.fpRegisters) {
		fpRegistersList.append(reg);
	}
	resultMap["fpRegisters"] = fpRegistersList;

	return resultMap;
}

QVariantMap AlphaJITSystem::dumpState()
{
	QVariantMap state;

	// Get basic blocks and traces
	QList<AlphaBasicBlock*> basicBlocks;
	QList<AlphaTrace*> traces;

	for (const auto& blockPair : engine->getBasicBlocks()) {
		basicBlocks.append(blockPair);
	}

	for (const auto& tracePair : engine->getTraces()) {
		traces.append(tracePair);
	}

	// Convert registers to QVariantList
	QVariantList registersList;
	for (int reg : engine->getRegisters()) {
		registersList.append(reg);
	}
	state["registers"] = registersList;

	// Convert floating point registers to QVariantList
	QVariantList fpRegistersList;
	for (double reg : engine->getFpRegisters()) {
		fpRegistersList.append(reg);
	}
	state["fpRegisters"] = fpRegistersList;

	state["pc"] = engine->getPC();

	// Convert basic blocks to QVariantList
	QVariantList blocksList;
	for (AlphaBasicBlock* block : basicBlocks) {
		QVariantMap blockMap;
		blockMap["startAddress"] = block->getStartAddress();
		blockMap["endAddress"] = block->getEndAddress();
		blockMap["instructionCount"] = block->length();
		blockMap["executionCount"] = block->getExecutionCount();
		blockMap["isCompiled"] = block->isCompiled;

		QVariantList successors;
		for (AlphaBasicBlock* nextBlock : block->getNextBlocks()) {
			successors.append(nextBlock->getStartAddress());
		}
		blockMap["successors"] = successors;

		QVariantList predecessors;
		for (AlphaBasicBlock* prevBlock : block->getPrevBlocks()) {
			predecessors.append(prevBlock->getStartAddress());
		}
		blockMap["predecessors"] = predecessors;

		blocksList.append(blockMap);
	}
	state["basicBlocks"] = blocksList;

	// Convert traces to QVariantList
	QVariantList tracesList;
	for (AlphaTrace* trace : engine->getTraces()) {
		QVariantMap traceMap;
		traceMap["startAddress"] = trace->getStartAddress();
		traceMap["endAddress"] = trace->getEndAddress();
		traceMap["blockCount"] = trace->getBlocks().size();
		traceMap["executionCount"] = trace->getExecutionCount();
		traceMap["isCompiled"] = trace->getIsCompiled();

		tracesList.append(traceMap);
	}
	state["traces"] = tracesList;

	return state;
}

QVector<quint32> AlphaJITSystem::assembleAlphaCode(const QString& assembly)
{
	QStringList lines = assembly.trimmed().split('\n');
	QVector<quint32> code;

	// Use instruction map if available, otherwise fall back to defaults
	bool useInstructionMap = !instructionMap.isEmpty();

	// Default instruction map as fallback
	QMap<QString, QPair<quint32, quint32>> defaultInstructionMap;
	if (!useInstructionMap) {
		// Integer operations
		defaultInstructionMap["ADDL"] = qMakePair(0x10u, 0x00u);
		defaultInstructionMap["ADDQ"] = qMakePair(0x10u, 0x20u);
		defaultInstructionMap["SUBL"] = qMakePair(0x10u, 0x09u);
		defaultInstructionMap["SUBQ"] = qMakePair(0x10u, 0x29u);
		defaultInstructionMap["MULL"] = qMakePair(0x10u, 0x0Cu);

		// Branch operations
		defaultInstructionMap["BR"] = qMakePair(0x30u, 0u);
		defaultInstructionMap["BEQ"] = qMakePair(0x39u, 0u);
		defaultInstructionMap["BNE"] = qMakePair(0x3Du, 0u);

		// Logical operations
		defaultInstructionMap["AND"] = qMakePair(0x11u, 0x00u);
		defaultInstructionMap["BIS"] = qMakePair(0x11u, 0x14u);

		// Memory operations (simplified)
		defaultInstructionMap["LDL"] = qMakePair(0x28u, 0u);
		defaultInstructionMap["STL"] = qMakePair(0x2Cu, 0u);
	}

	for (const QString& line : lines) {
		QString trimmedLine = line.trimmed();
		if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
			continue;
		}

		// Parse the instruction
// Use QRegularExpression correctly
		QRegularExpression rx("^([A-Z0-9]+)\\s+([^,]+)(?:,\\s*([^,]+))?(?:,\\s*(.+))?$");

		// Match the trimmed line
		QRegularExpressionMatch match = rx.match(trimmedLine);

		// Check if a match was found
		if (match.hasMatch()) {
			QString mnemonic = match.captured(1).toUpper();
			QString op1 = match.captured(2).trimmed();
			QString op2 = match.captured(3).trimmed();
			QString op3 = match.captured(4).trimmed();

			// Example: print them
			qDebug() << "Mnemonic:" << mnemonic << "Op1:" << op1 << "Op2:" << op2 << "Op3:" << op3;
			quint32 opcode, functionCode = 0;

			if (useInstructionMap) {
				// Use loaded instruction definitions
				if (!instructionMap.contains(mnemonic)) {
					qCritical() << "Unknown instruction:" << mnemonic;
					throw std::runtime_error("Unknown instruction: " + mnemonic.toStdString());
				}

				opcode = instructionMap[mnemonic].opcode;
				if (instructionMap[mnemonic].functionCode >= 0) {
					functionCode = instructionMap[mnemonic].functionCode;
				}
			}
			else {
				// Use default instruction map
				if (!defaultInstructionMap.contains(mnemonic)) {
					qCritical() << "Unknown instruction:" << mnemonic;
					throw std::runtime_error("Unknown instruction: " + mnemonic.toStdString());
				}

				opcode = defaultInstructionMap[mnemonic].first;
				functionCode = defaultInstructionMap[mnemonic].second;
			}

			// Simplified encoding for demonstration
			quint32 encoded = opcode << 26;  // Opcode in bits 31-26

			if (mnemonic.startsWith("B") && !mnemonic.startsWith("BI")) {
				// Branch format (Ra, displacement)
				quint32 ra = op1.mid(1).toUInt();  // Extract register number
				quint32 disp = op2.toInt();

				encoded |= (ra & 0x1F) << 21;  // Ra in bits 25-21
				encoded |= (disp & 0x1FFFFF);  // Displacement in bits 20-0
			}
			else {
				// Operate format (Ra, Rb, Rc)
				quint32 ra = op1.mid(1).toUInt();  // Extract register number
				quint32 rb = op2.mid(1).toUInt();
				quint32 rc = op3.mid(1).toUInt();

				encoded |= (ra & 0x1F) << 21;  // Ra in bits 25-21
				encoded |= (rb & 0x1F) << 16;  // Rb in bits 20-16
				encoded |= (functionCode & 0x7F) << 5;  // Function in bits 11-5
				encoded |= (rc & 0x1F);  // Rc in bits 4-0
			}
			code.append(encoded);
		}
	}

	return code;
}
