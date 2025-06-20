#pragma once
#include <QString>
#include <QStringList>
struct InstructionDefinition
{
    QString mnemonic;
    int opcode = 0;
    int functionCode = -1;
    QString instructionClass;
    QStringList operands;
    QString description;
};
