#pragma once

#include <QString>
#include <QVector>
#include <QMap>

class DecodedInstruction;

/**
 * Decode an Alpha instruction
 * @param instruction The raw 32-bit instruction
 * @return Decoded instruction information
 */
void decodeOperate(quint32 instruction, DecodedInstruction &result);