#pragma once
/**
 * Main entry point for the Alpha JIT System
 */

#include <QObject>
#include <QVector>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QList>
#include <QVariantList>
#include <QRegularExpression>
#include "alphabasicblock.h"
#include "alphatrace.h"
#include "alphajitexecutionengine.h"
#include "..\AESH\Helpers.h"
#include "instructiondefinition.h"

class AlphaJITSystem : public QObject
{
    Q_OBJECT

   

private:
    helpers_JIT::Options options;
    AlphaJITExecutionEngine* engine;

	
public:
    /**
     * Constructor with optional configuration options
     * @param customOptions - Optional configuration parameters
     */
    explicit AlphaJITSystem(const QVariantMap& customOptions = QVariantMap()) : QObject(nullptr)
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
        engine->getAlphaJITProfiler()->setHotThreshold(options.blockCompilationThreshold);
        engine->setTraceThreshold(options.traceCompilationThreshold);
    }

    /**
     * Destructor
     */
    ~AlphaJITSystem()
    {
        delete engine;
    }

    /**
     * Load Alpha assembly code
     * @param assembly - The Alpha assembly code
     * @param baseAddress - The base address to load at
     * @return The number of instructions loaded
     */
    int loadAssembly(const QString& assembly, quint64 baseAddress = 0)
    {
        QVector<quint32> code = assembleAlphaCode(assembly);
        engine->loadCode(code, baseAddress);
        return code.size();
    }

    /**
     * Load Alpha binary code
     * @param code - The Alpha binary code
     * @param baseAddress - The base address to load at
     * @return The number of instructions loaded
     */
    int loadBinary(const QVector<quint32>& code, quint64 baseAddress = 0)
    {
        engine->loadCode(code, baseAddress);
        return code.size();
    }

    /**
     * Parse a CSV containing Alpha instruction definitions
     * @param csv - The CSV data
     * @return Vector of instruction definitions
     */
    QVector<InstructionDefinition> parseInstructionDefinitions(const QString& csv)
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
            }
        }

        return instructionDefs;
    }

    /**
     * Load instruction definitions into the decoder
     * @param instructionDefs - The instruction definitions
     * @return The number of definitions loaded
     */
    int loadInstructionDefinitions(const QVector<InstructionDefinition>& instructionDefs)
    {
        // In a real implementation, this would update the decoder's instruction map
        qDebug() << "Loaded" << instructionDefs.size() << "instruction definitions";
        return instructionDefs.size();
    }

    /**
     * Run the JIT system from a start address
     * @param startAddress - The address to start execution at
     * @param maxInstructions - Maximum number of instructions to execute
     * @return Execution results
     */
    QVariantMap run(quint64 startAddress = 0, int maxInstructions = 1000000)
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

    /**
     * Dump the state of the JIT system for debugging
     * @return System state information
     */
    QVariantMap dumpState();

private:
   

    

    /**
     * Helper function to convert Alpha assembly to machine code
     * @param assembly - The Alpha assembly code
     * @return Vector of 32-bit instructions
     */
    QVector<quint32> assembleAlphaCode(const QString& assembly)
    {
        QStringList lines = assembly.trimmed().split('\n');
        QVector<quint32> code;

        // Define instruction map
        QMap<QString, QPair<quint32, quint32>> instructionMap;

        // Integer operations
        instructionMap["ADDL"] = qMakePair(0x10u, 0x00u);
        instructionMap["ADDQ"] = qMakePair(0x10u, 0x20u);
        instructionMap["SUBL"] = qMakePair(0x10u, 0x09u);
        instructionMap["SUBQ"] = qMakePair(0x10u, 0x29u);
        instructionMap["MULL"] = qMakePair(0x10u, 0x0Cu);

        // Branch operations
        instructionMap["BR"] = qMakePair(0x30u, 0u);
        instructionMap["BEQ"] = qMakePair(0x39u, 0u);
        instructionMap["BNE"] = qMakePair(0x3Du, 0u);

        // Logical operations
        instructionMap["AND"] = qMakePair(0x11u, 0x00u);
        instructionMap["BIS"] = qMakePair(0x11u, 0x14u);

        // Memory operations (simplified)
        instructionMap["LDL"] = qMakePair(0x28u, 0u);
        instructionMap["STL"] = qMakePair(0x2Cu, 0u);

        for (const QString& line : lines) {
            QString trimmedLine = line.trimmed();
            if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
                continue;
            }

            // Parse the instruction
			QRegularExpression rx("^([A-Z0-9]+)\\s+([^,]+)(?:,\\s*([^,]+))?(?:,\\s*(.+))?$");
			QRegularExpressionMatch match = rx.match(trimmedLine);

			if (match.hasMatch()) {
				QString mnemonic = match.captured(1).toUpper();
				QString op1 = match.captured(2).trimmed();
				QString op2 = match.captured(3).trimmed();  // might be empty
				QString op3 = match.captured(4).trimmed();  // might be empty

				// Now you can safely use mnemonic, op1, op2, op3
				if (!instructionMap.contains(mnemonic)) {
					qCritical() << "Unknown instruction:" << mnemonic;
					throw std::runtime_error("Unknown instruction: " + mnemonic.toStdString());
				}
				quint32 opcode = instructionMap[mnemonic].first;
				quint32 functionCode = instructionMap[mnemonic].second;

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
};