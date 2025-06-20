#pragma once

#include <QObject>
#include <QVariantMap>
#include <QIODevice>
#include <QVector>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include "alphajitexecutionengine.h"
#include "..\AESH\Helpers.h"
#include "instructiondefinition.h"
#include "..\AEC\fpRegisterBankCls.h"


/**
 * @brief AlphaJITCompiler - Just-In-Time compiler for Alpha instructions
 *
 * This class translates Alpha instructions to native code for efficient execution.
 * It runs in a separate thread to avoid blocking CPU execution.
 *
 * Usage examples:
 *
 * // Basic usage (compatible with simple version)
 * compiler.setOptimizationLevel(2);
 * int level = compiler.getOptimizationLevel();
 *
 * // Enum-based usage (preferred method)
 * compiler.setOptimizationLevel(AlphaJITCompiler::ADVANCED);
 * AlphaJITCompiler::OptimizationLevel level = compiler.optimizationLevel();
 */

/**
 * Main entry point for the Alpha JIT System
 */
class AlphaJITSystem : public QObject
{
    Q_OBJECT

private:

   
	helpers_JIT::Options options;
	AlphaJITExecutionEngine* engine;

    QMap<QString, InstructionDefinition> instructionMap; // Store parsed CSV data as a map
public:
    /**
     * Constructor with optional configuration options
     * @param customOptions - Optional configuration parameters
     */
    explicit AlphaJITSystem(const QVariantMap& customOptions = QVariantMap());

    void InitializeSystem(RegisterBank* registerBank_, FpRegisterBankcls* fpRegisterBankCls_, SafeMemory* safeMemory_);
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
     * Load instruction definitions from a CSV file
     * @param filePath - Path to the CSV file
     * @return Number of instructions loaded
     */
    int loadInstructionDefinitionsFromFile(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "Failed to open instruction definitions file:" << filePath;
            return 0;
        }

        QString csvData = QString::fromUtf8(file.readAll());
        file.close();

        auto instructionDefs = parseInstructionDefinitions(csvData);
        return loadInstructionDefinitions(instructionDefs);
    }


	/// Return the integer-register snapshot
	QVector<quint64> getRegisters() const { return engine->getRegisters(); }

	/// Return the floating-point register snapshot
	QVector<double> getFpRegisters() const { return engine->getFpRegisters(); }


    /**
     * Parse a CSV containing Alpha instruction definitions
     * @param csv - The CSV data
     * @return Vector of instruction definitions
     */
    QVector<InstructionDefinition> parseInstructionDefinitions(const QString& csv);

    /**
     * Load instruction definitions into the decoder
     * @param instructionDefs - The instruction definitions
     * @return The number of definitions loaded
     */
    int loadInstructionDefinitions(const QVector<InstructionDefinition>& instructionDefs);

    /**
     * Get an instruction definition by mnemonic
     * @param mnemonic - The instruction mnemonic
     * @return The instruction definition or null if not found
     */
    InstructionDefinition getInstructionDefinition(const QString& mnemonic) const;

    /**
     * Run the JIT system from a start address
     * @param startAddress - The address to start execution at
     * @param maxInstructions - Maximum number of instructions to execute
     * @return Execution results
     */
    QVariantMap run(quint64 startAddress = 0, int maxInstructions = 1000000);

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
    QVector<quint32> assembleAlphaCode(const QString& assembly);
    // required for AlphaJITExecutionEngine
    RegisterBank* registerBank; 
    FpRegisterBankcls* fpRegisterBankcls;
    SafeMemory* safeMemory;
};