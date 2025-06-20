#include "alphainstructiondecoder.h"
#include "decodeOperate.h"







bool InstructionFormatDecoder::loadInstructionDefinitions(const QString& definitionFile)


void InstructionFormatDecoder::addCustomInstruction(quint32 opcode, quint32 functionCode,
	const QString& mnemonic, InstructionFormat format,


void InstructionFormatDecoder::initializeInstructionMap()


QString InstructionFormatDecoder::getInstructionKey(quint32 opcode, quint32 functionCode)


void InstructionFormatDecoder::decodeOperands(InstructionFormat& instruction, quint32 instructionWord)

void InstructionFormatDecoder::decodeOperateOperands(InstructionFormat& instruction, quint32 instructionWord)


void AlphaInstructionDecoder::addCustomInstruction(const QString& mnemonic, quint32 opcode,
                                                   int functionCode, AlphaInstruction::InstructionFormat format,
                                                   const QStringList& operands, const QString& description)
{
    AlphaInstruction instr;
    instr.mnemonic = mnemonic;
    instr.opcode = opcode;
    instr.functionCode = functionCode;
    instr.format = format;
    instr.operands = operands;
    instr.description = description;

    if (format == AlphaInstruction::OPERATE && functionCode >= 0) {
        operateTable[qMakePair(opcode, functionCode)] = instr;
    } else {
        instructionTable[opcode] = instr;
    }
}
