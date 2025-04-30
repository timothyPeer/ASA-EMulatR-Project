#include "AlphaInstruction.h"




// AlphaInstructionDecoder.cpp - Complete implementation
#include "AlphaInstructionDecoder.h"
#include <QFile>
#include <QDebug>

QString AlphaInstruction::toString() const
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

AlphaInstructionDecoder::AlphaInstructionDecoder(QObject* parent)
	: QObject(parent)
{
	// Initialize with the standard Alpha instruction set
	initializeInstructionMap();
}

AlphaInstructionDecoder::~AlphaInstructionDecoder()
{
	// Clean up
}

AlphaInstruction AlphaInstructionDecoder::decode(quint32 instructionWord)
{
	// Extract the opcode (bits 31-26)
	quint32 opcode = (instructionWord >> 26) & 0x3F;

	// For PAL calls, the function code is in bits 25-0
	if (opcode == 0x00) {
		quint32 palFunction = instructionWord & 0x3FFFFFF;

		// Look up the PAL instruction
		QString key = getInstructionKey(opcode, 0);

		if (!m_instructionMap.contains(key)) {
			emit decodingError(instructionWord, QString("Unknown PAL function: 0x%1").arg(palFunction, 0, 16));
			throw std::runtime_error("Unknown PAL function");
		}

		// Create a copy of the template
		AlphaInstruction instruction = m_instructionMap[key];

		// Set the function code
		instruction.decodedOperands["palcode_entry"] = palFunction;

		return instruction;
	}

	// For operate format instructions, the function code is in bits 11-5
	quint32 functionCode = (instructionWord >> 5) & 0x7F;

	// Look up the instruction
	QString key = getInstructionKey(opcode, functionCode);

	if (!m_instructionMap.contains(key)) {
		emit decodingError(instructionWord, QString("Unknown instruction: opcode=0x%1, function=0x%2")
			.arg(opcode, 0, 16).arg(functionCode, 0, 16));
		throw std::runtime_error("Unknown instruction");
	}

	// Create a copy of the template
	AlphaInstruction instruction = m_instructionMap[key];

	// Decode the operands
	decodeOperands(instruction, instructionWord);

	return instruction;
}

bool AlphaInstructionDecoder::loadInstructionDefinitions(const QString& definitionFile)
{
	QFile file(definitionFile);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << "Failed to open instruction definition file:" << definitionFile;
		return false;
	}

	// Clear existing definitions
	m_instructionMap.clear();

	// Read the definitions
	QTextStream in(&file);
	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();

		// Skip empty lines and comments
		if (line.isEmpty() || line.startsWith('#')) {
			continue;
		}

		// Parse the definition
		QStringList parts = line.split(',');
		if (parts.size() < 4) {
			qDebug() << "Invalid instruction definition:" << line;
			continue;
		}

		// Extract fields
		QString section = parts[0].trimmed();
		QString mnemonic = parts[1].trimmed();
		QString opcodeHex = parts[2].trimmed();
		QString functionHex = parts[3].trimmed();
		QString formatName = parts[4].trimmed();

		// Parse opcode and function
		bool opcodeOk, functionOk;
		quint32 opcode = opcodeHex.toUInt(&opcodeOk, 16);
		quint32 functionCode = functionHex.toUInt(&functionOk, 16);

		if (!opcodeOk) {
			qDebug() << "Invalid opcode:" << opcodeHex;
			continue;
		}

		// Parse format
		AlphaInstruction::InstructionFormat format;
		if (formatName == "Operate") {
			format = AlphaInstruction::OPERATE;
		}
		else if (formatName == "Branch") {
			format = AlphaInstruction::BRANCH;
		}
		else if (formatName == "Memory") {
			format = AlphaInstruction::MEMORY;
		}
		else if (formatName == "System") {
			format = AlphaInstruction::SYSTEM;
		}
		else if (formatName == "Vector") {
			format = AlphaInstruction::VECTOR;
		}
		else if (formatName == "MemoryBarrier") {
			format = AlphaInstruction::MEMORY_BARRIER;
		}
		else {
			qDebug() << "Invalid instruction format:" << formatName;
			continue;
		}

		// Parse operands
		QVector<QString> operands;
		if (parts.size() > 5) {
			QStringList opList = parts[5].split(' ');
			for (const QString& op : opList) {
				operands.append(op.trimmed());
			}
		}

		// Parse description
		QString description;
		if (parts.size() > 6) {
			description = parts[6].trimmed();
		}

		// Add the instruction
		addCustomInstruction(opcode, functionCode, mnemonic, format, operands, description);
	}

	file.close();

	qDebug() << "Loaded" << m_instructionMap.size() << "instructions from" << definitionFile;

	return true;
}

void AlphaInstructionDecoder::addCustomInstruction(quint32 opcode, quint32 functionCode,
	const QString& mnemonic, AlphaInstruction::InstructionFormat format,
	const QVector<QString>& operands, const QString& description)
{
	// Create a new instruction
	AlphaInstruction instruction;
	instruction.opcode = opcode;
	instruction.functionCode = functionCode;
	instruction.mnemonic = mnemonic;
	instruction.format = format;
	instruction.operands = operands;
	instruction.description = description;

	// Add to the map
	QString key = getInstructionKey(opcode, functionCode);
	m_instructionMap[key] = instruction;

	qDebug() << "Added custom instruction:" << mnemonic << "opcode=" << Qt::hex << opcode
		<< "function=" << Qt::hex << functionCode;
}

void AlphaInstructionDecoder::initializeInstructionMap()
{
	// Integer arithmetic operations
	addCustomInstruction(0x10, 0x00, "ADDL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Integer Add (longword)");
	addCustomInstruction(0x10, 0x20, "ADDQ", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Integer Add (quadword)");
	addCustomInstruction(0x10, 0x09, "SUBL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Integer Subtract (longword)");
	addCustomInstruction(0x10, 0x29, "SUBQ", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Integer Subtract (quadword)");

	// Multiply and divide
	addCustomInstruction(0x10, 0x0C, "MULL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Integer Multiply (longword)");
	addCustomInstruction(0x10, 0x2C, "MULQ", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Integer Multiply (quadword)");
	addCustomInstruction(0x10, 0x30, "UMULH", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Unsigned Multiply High (quadword)");
	addCustomInstruction(0x10, 0x1D, "DIVL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Divide (longword)");
	addCustomInstruction(0x10, 0x3D, "DIVQ", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Divide (quadword)");

	// Compare operations
	addCustomInstruction(0x10, 0x2D, "CMPEQ", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Compare Equal");
	addCustomInstruction(0x10, 0x01, "CMPULT", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Compare Unsigned Less Than");
	addCustomInstruction(0x10, 0x11, "CMPULE", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Compare Unsigned Less Equal");
	addCustomInstruction(0x10, 0x02, "CMPLT", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Compare Signed Less Than");
	addCustomInstruction(0x10, 0x12, "CMPLE", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Compare Signed Less Equal");

	// Logical operations
	addCustomInstruction(0x11, 0x00, "AND", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Logical AND");
	addCustomInstruction(0x11, 0x08, "BIC", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Bit Clear");
	addCustomInstruction(0x11, 0x14, "BIS", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Bit Set");
	addCustomInstruction(0x11, 0x1C, "ORNOT", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "OR NOT");
	addCustomInstruction(0x11, 0x20, "XOR", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Logical Exclusive OR");
	addCustomInstruction(0x11, 0x28, "EQV", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Logical Equivalence");

	// Shift operations
	addCustomInstruction(0x12, 0x39, "SLL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Shift Left Logical");
	addCustomInstruction(0x12, 0x34, "SRL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Shift Right Logical");
	addCustomInstruction(0x12, 0x3C, "SRA", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Shift Right Arithmetic");

	// Byte manipulation
	addCustomInstruction(0x12, 0x30, "ZAP", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Zero Byte Mask");
	addCustomInstruction(0x12, 0x31, "ZAPNOT", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Zero Byte Not Mask");
	addCustomInstruction(0x12, 0x02, "MSKBL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Mask Byte Low");
	addCustomInstruction(0x12, 0x06, "EXTBL", AlphaInstruction::OPERATE,
		{ "ra", "rb", "rc" }, "Extract Byte Low");

	// Branch operations
	addCustomInstruction(0x30, 0, "BR", AlphaInstruction::BRANCH,
		{ "ra", "disp" }, "Branch Relative");
	addCustomInstruction(0x34, 0, "BSR", AlphaInstruction::BRANCH,
		{ "ra", "disp" }, "Branch to Subroutine");
	addCustomInstruction(0x38, 0, "BLBC", AlphaInstruction::BRANCH,
		{ "ra", "disp" }, "Branch Low Bit Clear");
	addCustomInstruction(0x3C, 0, "BLBS", AlphaInstruction::BRANCH,
		{ "ra", "disp" }, "Branch Low Bit Set");
	addCustomInstruction(0x39, 0, "BEQ", AlphaInstruction::BRANCH,
		{ "ra", "disp" }, "Branch if Equal");
	addCustomInstruction(0x3D, 0, "BNE", AlphaInstruction::BRANCH,
		{ "ra", "disp" }, "Branch if Not Equal");

	// Jump operations
	addCustomInstruction(0x1A, 0, "JMP", AlphaInstruction::BRANCH,
		{ "ra", "rb" }, "Jump Indirect");
	addCustomInstruction(0x1B, 0, "JSR", AlphaInstruction::BRANCH,
		{ "ra", "rb" }, "Jump to Subroutine Indirect");
	addCustomInstruction(0x1C, 0, "RET", AlphaInstruction::BRANCH,
		{ "ra", "rb" }, "Return from Subroutine");

	// Memory operations
	addCustomInstruction(0x28, 0, "LDL", AlphaInstruction::MEMORY,
		{ "ra", "disp", "rb" }, "Load Longword");
	addCustomInstruction(0x29, 0, "LDQ", AlphaInstruction::MEMORY,
		{ "ra", "disp", "rb" }, "Load Quadword");
	addCustomInstruction(0x2A, 0, "LDL_L", AlphaInstruction::MEMORY,
		{ "ra", "disp", "rb" }, "Load Longword Locked");
	addCustomInstruction(0x2B, 0, "LDQ_L", AlphaInstruction::MEMORY,
		{ "ra", "disp", "rb" }, "Load Quadword Locked");
	addCustomInstruction(0x2C, 0, "STL", AlphaInstruction::MEMORY,
		{ "ra", "disp", "rb" }, "Store Longword");
	addCustomInstruction(0x2D, 0, "STQ", AlphaInstruction::MEMORY,
		{ "ra", "disp", "rb" }, "Store Quadword");

	// Floating point operations
	addCustomInstruction(0x16, 0x00, "ADDF", AlphaInstruction::OPERATE,
		{ "fa", "fb", "fc" }, "Floating Add S (single)");
	addCustomInstruction(0x16, 0x01, "ADDD", AlphaInstruction::OPERATE,
		{ "fa", "fb", "fc" }, "Floating Add D (double)");
	addCustomInstruction(0x16, 0x20, "SUBF", AlphaInstruction::OPERATE,
		{ "fa", "fb", "fc" }, "Floating Subtract S");
	addCustomInstruction(0x16, 0x21, "SUBD", AlphaInstruction::OPERATE,
		{ "fa", "fb", "fc" }, "Floating Subtract D");

	// PAL calls
	addCustomInstruction(0x00, 0, "CALL_PAL", AlphaInstruction::SYSTEM,
		{ "palcode_entry" }, "Call PAL Routine");

	// Memory barriers
	addCustomInstruction(0x18, 0, "MB", AlphaInstruction::MEMORY_BARRIER,
		{ "none" }, "Memory Barrier");
	addCustomInstruction(0x19, 0, "WMB", AlphaInstruction::MEMORY_BARRIER,
		{ "none" }, "Write Memory Barrier");
}

QString AlphaInstructionDecoder::getInstructionKey(quint32 opcode, quint32 functionCode)
{
	return QString("%1-%2").arg(opcode).arg(functionCode);
}

void AlphaInstructionDecoder::decodeOperands(AlphaInstruction& instruction, quint32 instructionWord)
{
	switch (instruction.format) {
	case AlphaInstruction::OPERATE:
		decodeOperateOperands(instruction, instructionWord);
		break;

	case AlphaInstruction::BRANCH:
		decodeBranchOperands(instruction, instructionWord);
		break;

	case AlphaInstruction::MEMORY:
		decodeMemoryOperands(instruction, instructionWord);
		break;

	case AlphaInstruction::SYSTEM:
		decodePALOperands(instruction, instructionWord);
		break;

	case AlphaInstruction::MEMORY_BARRIER:
		// Memory barriers have no operands to decode
		break;

	case AlphaInstruction::VECTOR:
		// Vector operations would be decoded similarly to OPERATE format
		// but with vector registers instead
		break;

	default:
		qDebug() << "Unknown instruction format:" << instruction.format;
		break;
	}
}

void AlphaInstructionDecoder::decodeOperateOperands(AlphaInstruction& instruction, quint32 instructionWord)
{
	// Extract register numbers
	quint32 ra = (instructionWord >> 21) & 0x1F;
	quint32 rb = (instructionWord >> 16) & 0x1F;
	quint32 rc = instructionWord & 0x1F;

	// Check for literal mode (bit 12)
	bool literalMode = (instructionWord >> 12) & 0x1;
	quint32 literalValue = 0;

	if (literalMode) {
		// Extract 8-bit literal from bits 13-20
		literalValue = (instructionWord >> 13) & 0xFF;

		// Handle sign extension for some instructions
		if (instruction.mnemonic.contains("S")) {
			// Sign extension for S-suffix instructions
			if (literalValue & 0x80) {
				literalValue |= 0xFFFFFF00; // Extend sign bit
			}
		}
	}

	// Check if this is a floating point instruction
	bool isFloatingPoint = instruction.mnemonic.startsWith("F") ||
		instruction.mnemonic.startsWith("CPY") ||
		instruction.mnemonic.contains("D") ||
		instruction.mnemonic.contains("G") ||
		instruction.mnemonic.contains("T");

	// For floating-point instructions, registers are F0-F31, otherwise R0-R31
	if (isFloatingPoint) {
		instruction.decodedOperands["fa"] = ra;
		instruction.decodedOperands["fb"] = rb;
		instruction.decodedOperands["fc"] = rc;
	}
	else {
		instruction.decodedOperands["ra"] = ra;

		// For literal mode, store the literal value instead of rb
		if (literalMode) {
			instruction.decodedOperands["lit"] = literalValue;
		}
		else {
			instruction.decodedOperands["rb"] = rb;
		}

		instruction.decodedOperands["rc"] = rc;
	}
}

void AlphaInstructionDecoder::decodeBranchOperands(AlphaInstruction& instruction, quint32 instructionWord)
{
	// Extract fields
	quint32 ra = (instructionWord >> 21) & 0x1F;

	// For jump instructions (JMP, JSR, RET), extract rb
	if (instruction.mnemonic == "JMP" || instruction.mnemonic == "JSR" || instruction.mnemonic == "RET") {
		quint32 rb = (instructionWord >> 16) & 0x1F;
		instruction.decodedOperands["ra"] = ra;
		instruction.decodedOperands["rb"] = rb;
	}
	else {
		// For branch instructions, extract displacement
		// Displacement is a 21-bit signed value in bits 20-0
		qint32 displacement = instructionWord & 0x1FFFFF;

		// Sign-extend the displacement if the high bit is set
		if (displacement & 0x100000) {
			displacement |= ~0x1FFFFF;
		}

		// Store the operands
		instruction.decodedOperands["ra"] = ra;
		instruction.decodedOperands["disp"] = displacement;
	}
}

void AlphaInstructionDecoder::decodeMemoryOperands(AlphaInstruction& instruction, quint32 instructionWord)
{
	// Extract fields
	quint32 ra = (instructionWord >> 21) & 0x1F;
	quint32 rb = (instructionWord >> 16) & 0x1F;
	qint16 displacement = instructionWord & 0xFFFF; // 16-bit signed displacement

	// Store the decoded operands
	instruction.decodedOperands["ra"] = ra;
	instruction.decodedOperands["rb"] = rb;
	instruction.decodedOperands["disp"] = displacement;
}

void AlphaInstructionDecoder::decodePALOperands(AlphaInstruction& instruction, quint32 instructionWord)
{
	// For PAL calls, the function code is in bits 25-0
	quint32 palFunction = instructionWord & 0x3FFFFFF;

	// Store the decoded operand
	instruction.decodedOperands["palcode_entry"] = palFunction;
}

QVariantMap AlphaJITSystem::dumpState()
{
	QVariantMap state;

	// Get basic blocks and traces
	QList<AlphaBasicBlock*> basicBlocks;
	QList<AlphaTrace*> traces;

	for (AlphaBasicBlock* block : engine->getBasicBlocks()) {
		basicBlocks.append(block);
	}
	for (AlphaTrace* trace : engine->getTraces()) {
		traces.append(trace);
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
	for (AlphaTrace* trace : traces) {
		QVariantMap traceMap;
		traceMap["startAddress"] = trace->getStartAddress();
		traceMap["endAddress"] = trace->getEndAddress();
		traceMap["blockCount"] = trace->getBlocks().size();
		traceMap["executionCount"] = trace->getExecutionCount();
		traceMap["isCompiled"] = trace->isCompiled;

		tracesList.append(traceMap);
	}
	state["traces"] = tracesList;

	return state;
}
