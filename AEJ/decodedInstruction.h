#pragma once
#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include "decodeOperate.h"
#include "AsaNamespaces.h"
#include "enumerations/enumInstructionFormat.h"



 /**
 * @brief Decode instruction
 * @param instruction Raw instruction word
 * @param pc Program counter
 * @return Decoded instruction
 */
class DecodedInstruction 
{
   

  public:

    quint32 raw;            // Raw 32-bit instruction
    quint32 rawInstruction; // Alias for compatibility
    quint64 pc;             // Program counter
/*    quint32 opcode = 0;*/
/*    quint32 function = 0;*/
/*    quint8 ra = 0;*/
/*    quint8 rb = 0;*/
/*    quint8 rc = 0;*/
/*    qint64 immediate = 0;*/

    // Instruction fields (Alpha format)
    quint8 opcode;             // Bits 31:26
    quint8 ra;                 // Bits 25:21 (source/dest register A)
    quint8 rb;                 // Bits 20:16 (source register B)
    quint8 rc;                 // Bits 4:0 (dest register C)
    quint8 function;           // Function field (varies by instruction type)
    quint16 literal;           // Literal value for immediate instructions
    qint16 displacement;       // Displacement for memory/branch instructions
    qint32 memoryDisplacement; // Memory displacement
    quint64 immediate;         // Full immediate value
    bool valid = false;
    
    // Decoded information
    bool isMemoryInstruction;
    bool isBranchInstruction;
    bool isFloatingPoint;
    bool isPALInstruction;
    bool isPrivileged;

    QString mnemonic;                 // Instruction mnemonic
    InstructionFormat instructionFormat; // Instruction instructionFormat
    QVector<QString> operands;        // Operand types
    QMap<QString, quint32> decodedOperands; // Decoded operand values

    DecodedInstruction();
    DecodedInstruction(quint32 instruction, quint64 programCounter = 0);
};

// /**
//  * Decoded instruction structure
//  */
// struct DecodedInstruction {
//     quint32 opcode;                   // Main opcode
//     quint32 function;                 // Function code
//     quint32 ra;                       // Ra register number
//     quint32 rb;                       // Rb register number
//     quint32 rc;                       // Rc register number
//     QString mnemonic;                 // Instruction mnemonic
//     InstructionFormat instructionFormat;         // Instruction instructionFormat
//     QVector<QString> operands;        // Operand types
//     QMap<QString, quint32> decodedOperands; // Decoded operand values
// };