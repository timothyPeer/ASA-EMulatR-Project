#pragma once

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QRegularExpression>
#include "..\AESH\Helpers.h"
#include "decodeOperate.h"

/**
 * InstructionDefinition - Represents a single Alpha instruction definition
 */
class InstructionDefinition
{
public:
   

    // Constructors
    InstructionDefinition() = default;

    InstructionDefinition(const QString& mnemonic, quint32 opcode, qint32 functionCode = -1,
        Format format = Format::FORMAT_OPERATE, Section section = Section::SECTION_OTHER,
        const QString& description = QString())
        : mnemonic(mnemonic),
        opcode(opcode),
        functionCode(functionCode),
        format(format),
        section(section),
        description(description)
    {
    }

    // Create from CSV fields
    static InstructionDefinition fromCSV(const QStringList& fields, const QStringList& header) {
        InstructionDefinition def;

        // Find the indices of the columns we're interested in
        int mnemonicIdx = header.indexOf("Mnemonic");
        int opcodeIdx = header.indexOf("Opcode (hex)");
        int functionIdx = header.indexOf("Function (hex)");
        int classIdx = header.indexOf("Class");
        int operandsIdx = header.indexOf("Operands");
        int descriptionIdx = header.indexOf("Description");

        if (mnemonicIdx >= 0 && mnemonicIdx < fields.size())
            def.mnemonic = fields[mnemonicIdx].trimmed();

        if (opcodeIdx >= 0 && opcodeIdx < fields.size())
            def.opcode = fields[opcodeIdx].trimmed().toUInt(nullptr, 16);

        if (functionIdx >= 0 && functionIdx < fields.size() && !fields[functionIdx].trimmed().isEmpty())
            def.functionCode = fields[functionIdx].trimmed().toInt(nullptr, 16);

        if (operandsIdx >= 0 && operandsIdx < fields.size())
            def.parseOperands(fields[operandsIdx].trimmed());

        if (descriptionIdx >= 0 && descriptionIdx < fields.size())
            def.description = fields[descriptionIdx].trimmed();

        if (classIdx >= 0 && classIdx < fields.size()) {
            QString className = fields[classIdx].trimmed();
            def.setFormatAndSectionFromClass(className);
        }
        else {
            // Guess format and section from mnemonic if class not provided
            def.guessFormatAndSection();
        }

        return def;
    }

    // Get machine code encoding for this instruction (placeholder)
    quint32 encode(const QList<quint32>& operandValues) const {
    quint32 encoding = opcode << 26;  // Opcode in bits 31-26

        switch (format) {
        case Format::FORMAT_BRANCH:
            if (operandValues.size() >= 2) {
                quint32 ra = operandValues[0];
                quint32 disp = operandValues[1];
                encoding |= (ra & 0x1F) << 21;       // Ra in bits 25-21
                encoding |= (disp & 0x1FFFFF);       // Displacement in bits 20-0
            }
            break;

        case Format::FORMAT_OPERATE:
            if (operandValues.size() >= 3) {
                quint32 ra = operandValues[0];
                quint32 rb = operandValues[1];
                quint32 rc = operandValues[2];
                encoding |= (ra & 0x1F) << 21;       // Ra in bits 25-21
                encoding |= (rb & 0x1F) << 16;       // Rb in bits 20-16
                encoding |= (functionCode & 0x7F) << 5;  // Function in bits 11-5
                encoding |= (rc & 0x1F);             // Rc in bits 4-0
            }
            break;

        case Format::FORMAT_MEMORY:
            if (operandValues.size() >= 3) {
                quint32 ra = operandValues[0];
                quint32 disp = operandValues[1];
                quint32 rb = operandValues[2];
                encoding |= (ra & 0x1F) << 21;       // Ra in bits 25-21
                encoding |= (rb & 0x1F) << 16;       // Rb in bits 20-16
                encoding |= (disp & 0xFFFF);         // Displacement in bits 15-0
            }
            break;

            // Other formats would be implemented here
        default:
            break;
        }

        return encoding;
    }

    // Serialization
    QVariantMap toVariantMap() const {
        QVariantMap map;
        map["mnemonic"] = mnemonic;
        map["opcode"] = QString("0x%1").arg(opcode, 2, 16, QChar('0'));

        if (functionCode >= 0)
            map["functionCode"] = QString("0x%1").arg(functionCode, 2, 16, QChar('0'));

        map["format"] = formatToString(format);
        map["section"] = sectionToString(section);
        map["description"] = description;

        QVariantList operandsList;
        for (const QString& op : operands) {
            operandsList.append(op);
        }
        map["operands"] = operandsList;

        return map;
    }

    // Format to string
    QString formatToString(Format fmt) const {
        switch (fmt) {
        case Format::FORMAT_OPERATE: return "Operate";
        case Format::FORMAT_BRANCH: return "Branch";
        case Format::FORMAT_MEMORY: return "Memory";
        case Format::FORMAT_SYSTEM: return "System";
        case Format::FORMAT_VECTOR: return "Vector";
        case Format::FORMAT_MEMORY_BARRIER: return "MemoryBarrier";
        default: return "Unknown";
        }
    }

    // Section to string
    QString sectionToString(Section sec) const {
        switch (sec) {
        case Section::SECTION_INTEGER: return "Integer";
        case Section::SECTION_FLOATING_POINT: return "FloatingPoint";
        case Section::SECTION_CONTROL: return "Control";
        case Section::SECTION_PAL: return "PAL";
        case Section::SECTION_VECTOR: return "Vector";
        case Section::SECTION_MEMORY: return "Memory";
        case Section::SECTION_OTHER: return "Other";
        default: return "Unknown";
        }
    }

    // String representation
    QString toString() const {
        QString str = QString("%1 (0x%2").arg(mnemonic).arg(opcode, 2, 16, QChar('0'));

        if (functionCode >= 0)
            str += QString(", 0x%1").arg(functionCode, 2, 16, QChar('0'));

        str += ") " + formatToString(format) + " " + sectionToString(section);

        if (!operands.isEmpty())
            str += " Operands: " + operands.join(", ");

        if (!description.isEmpty())
            str += " - " + description;

        return str;
    }

    // Data members
    QString mnemonic;
    quint32 opcode = 0;
    qint32 functionCode = -1;  // -1 means no function code (null)
	Format format = Format::FORMAT_OPERATE;
	Section section = Section::SECTION_OTHER;
    QStringList operands;
    QString description;
    QString instructionClass;

private:
    // Parse operands string
    void parseOperands(const QString& operandsStr) {
        operands = operandsStr.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    }

    // Set format and section based on class name
    void setFormatAndSectionFromClass(const QString& className) {
        // Set section based on class name
        if (className.contains("Integer", Qt::CaseInsensitive))
            section = Section::SECTION_INTEGER;
        else if (className.contains("Float", Qt::CaseInsensitive))
            section = Section::SECTION_FLOATING_POINT;
        else if (className.contains("Control", Qt::CaseInsensitive) ||
            className.contains("Branch", Qt::CaseInsensitive))
            section = Section::SECTION_CONTROL;
        else if (className.contains("PAL", Qt::CaseInsensitive))
            section = Section::SECTION_PAL;
        else if (className.contains("Vector", Qt::CaseInsensitive))
            section = Section::SECTION_VECTOR;
        else if (className.contains("Memory", Qt::CaseInsensitive))
            section = Section::SECTION_MEMORY;

        // Set format based on operands and class name
        if (className.contains("Branch", Qt::CaseInsensitive) ||
            mnemonic.startsWith("B") || mnemonic.startsWith("J"))
            format = Format::FORMAT_BRANCH;
        else if (className.contains("Memory", Qt::CaseInsensitive) ||
            mnemonic.startsWith("LD") || mnemonic.startsWith("ST"))
            format = Format::FORMAT_MEMORY;
        else if (className.contains("System", Qt::CaseInsensitive) ||
            mnemonic.startsWith("CALL_PAL"))
            format = Format::FORMAT_SYSTEM;
        else if (className.contains("Vector", Qt::CaseInsensitive) ||
            mnemonic.startsWith("V"))
            format = Format::FORMAT_VECTOR;
        else if (mnemonic.contains("MB"))
            format = Format::FORMAT_MEMORY_BARRIER;
    }

    // Guess format and section from mnemonic
    void guessFormatAndSection() {
        // Try to guess the format
        if (mnemonic.startsWith("B") && !mnemonic.startsWith("BI")) {
            format = Format::FORMAT_BRANCH;
            section = Section::SECTION_CONTROL;
        }
        else if (mnemonic.startsWith("J") || mnemonic == "RET") {
            format = Format::FORMAT_BRANCH;
            section = Section::SECTION_CONTROL;
        }
        else if (mnemonic.startsWith("LD") || mnemonic.startsWith("ST")) {
            format = Format::FORMAT_MEMORY;
            section = Section::SECTION_MEMORY;
        }
        else if (mnemonic.contains("MB")) {
            format = Format::FORMAT_MEMORY_BARRIER;
            section = Section::SECTION_MEMORY;
        }
        else if (mnemonic.startsWith("CALL_PAL") || mnemonic.startsWith("PAL")) {
            format = Format::FORMAT_SYSTEM;
            section = Section::SECTION_PAL;
        }
        else if (mnemonic.startsWith("V")) {
            format = Format::FORMAT_VECTOR;
            section = Section::SECTION_VECTOR;
        }
        else if (mnemonic.contains("F") || mnemonic.contains("D") ||
            mnemonic.contains("G") || mnemonic.contains("T")) {
            format = Format::FORMAT_OPERATE;
            section = Section::SECTION_FLOATING_POINT;
        }
        else {
            // Default to operate format and integer section for most instructions
            format = Format::FORMAT_OPERATE;
            section = Section::SECTION_INTEGER;
        }
    }
};

