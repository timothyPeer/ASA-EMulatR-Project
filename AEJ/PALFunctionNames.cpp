// PALFunctionNames.cpp - Implementation of PAL function name lookup

#include "PALFunctionConstants.h"
#include "../AEJ/constants/const_OpCode_0_PAL.h"
#include <QMap>
#include <QString>

// Static lookup table for PAL function names
static const QMap<quint32, const char *> palFunctionNames = {
    // Common PAL Functions
    {FUNC_Common_HALT, "HALT"},
    {FUNC_Common_CFLUSH, "CFLUSH"},
    {FUNC_Common_DRAINA, "DRAINA"},
    {FUNC_Common_CSERVE, "CSERVE"},
    {FUNC_Common_IMB, "IMB"},
    {FUNC_Common_SWPCTX, "SWPCTX"},
    {FUNC_Common_REI, "REI"},
    {FUNC_Common_TBI, "TBI"},
    {FUNC_Common_MTPR_TBIA, "MTPR_TBIA"},
    {FUNC_Common_MTPR_TBIS, "MTPR_TBIS"},
    {FUNC_Common_MTPR_TBISD, "MTPR_TBISD"},
    {FUNC_Common_MTPR_TBISI, "MTPR_TBISI"},
    {FUNC_Common_MTPR_VPTB, "MTPR_VPTB"},
    {FUNC_Common_MFPR_VPTB, "MFPR_VPTB"},
    {FUNC_Common_MFPR_ASTEN, "MFPR_ASTEN"},
    {FUNC_Common_MFPR_ASTSR, "MFPR_ASTSR"},
    {FUNC_Common_MFPR_FEN, "MFPR_FEN"},
    {FUNC_Common_WRVAL, "WRVAL"},
    {FUNC_Common_RDVAL, "RDVAL"},
    {FUNC_Common_WRENT, "WRENT"},
    {FUNC_Common_SWPIPL, "SWPIPL"},
    {FUNC_Common_RDPS, "RDPS"},
    {FUNC_Common_WRKGP, "WRKGP"},
    {FUNC_Common_WRUSP, "WRUSP"},
    {FUNC_Common_RDUSP, "RDUSP"},
    {FUNC_Common_WRPERFMON, "WRPERFMON"},
    {FUNC_Common_BPT, "BPT"},
    {FUNC_Common_BUGCHK, "BUGCHK"},
    {FUNC_Common_CHME, "CHME"},
    {FUNC_Common_CHMS, "CHMS"},
    {FUNC_Common_CHMU, "CHMU"},
    {FUNC_Common_GENTRAP, "GENTRAP"},
    {FUNC_Common_PROBEW, "PROBEW"},
    {FUNC_Common_PROBER, "PROBER"},
    {FUNC_Common_INSQHIL, "INSQHIL"},
    {FUNC_Common_INSQTIL, "INSQTIL"},
    {FUNC_Common_INSQHIQ, "INSQHIQ"},
    {FUNC_Common_REMQHIL, "REMQHIL"},
    {FUNC_Common_REMQTIL, "REMQTIL"},
    {FUNC_Common_REMQHIQ, "REMQHIQ"},
    {FUNC_Common_REMQTIQ, "REMQTIQ"},

    // Alpha-Specific PAL Functions
    {FUNC_Alpha_LDQP, "Alpha_LDQP"},
    {FUNC_Alpha_STQP, "Alpha_STQP"},
    {FUNC_Alpha_MFPR_ASN, "Alpha_MFPR_ASN"},
    {FUNC_Alpha_MTPR_ASTEN, "Alpha_MTPR_ASTEN"},
    {FUNC_Alpha_MTPR_ASTSR, "Alpha_MTPR_ASTSR"},
    {FUNC_Alpha_MFPR_MCES, "Alpha_MFPR_MCES"},
    {FUNC_Alpha_MTPR_MCES, "Alpha_MTPR_MCES"},
    {FUNC_Alpha_MFPR_PCBB, "Alpha_MFPR_PCBB"},
    {FUNC_Alpha_MFPR_PRBR, "Alpha_MFPR_PRBR"},
    {FUNC_Alpha_MTPR_PRBR, "Alpha_MTPR_PRBR"},
    {FUNC_Alpha_MFPR_PTBR, "Alpha_MFPR_PTBR"},
    {FUNC_Alpha_MTPR_SCBB, "Alpha_MTPR_SCBB"},
    {FUNC_Alpha_MTPR_SIRR, "Alpha_MTPR_SIRR"},
    {FUNC_Alpha_MFPR_SISR, "Alpha_MFPR_SISR"},
    {FUNC_Alpha_MFPR_SSP, "Alpha_MFPR_SSP"},
    {FUNC_Alpha_MTPR_SSP, "Alpha_MTPR_SSP"},
    {FUNC_Alpha_MFPR_USP, "Alpha_MFPR_USP"},
    {FUNC_Alpha_MTPR_USP, "Alpha_MTPR_USP"},
    {FUNC_Alpha_MTPR_FEN, "Alpha_MTPR_FEN"},
    {FUNC_Alpha_MTPR_IPIR, "Alpha_MTPR_IPIR"},
    {FUNC_Alpha_MFPR_IPL, "Alpha_MFPR_IPL"},
    {FUNC_Alpha_MTPR_IPL, "Alpha_MTPR_IPL"},
    {FUNC_Alpha_MFPR_TBCHK, "Alpha_MFPR_TBCHK"},
    {FUNC_Alpha_MTPR_TBIAP, "Alpha_MTPR_TBIAP"},
    {FUNC_Alpha_MFPR_ESP, "Alpha_MFPR_ESP"},
    {FUNC_Alpha_MTPR_ESP, "Alpha_MTPR_ESP"},
    {FUNC_Alpha_MTPR_PERFMON, "Alpha_MTPR_PERFMON"},
    {FUNC_Alpha_MFPR_WHAMI, "Alpha_MFPR_WHAMI"},
    {FUNC_Alpha_READ_UNQ, "Alpha_READ_UNQ"},
    {FUNC_Alpha_WRITE_UNQ, "Alpha_WRITE_UNQ"},
    {FUNC_Alpha_INITPAL, "Alpha_INITPAL"},
    {FUNC_Alpha_WRENTRY, "Alpha_WRENTRY"},
    {FUNC_Alpha_SWPIRQL, "Alpha_SWPIRQL"},
    {FUNC_Alpha_RDIRQL, "Alpha_RDIRQL"},
    {FUNC_Alpha_DI, "Alpha_DI"},
    {FUNC_Alpha_EI, "Alpha_EI"},
    {FUNC_Alpha_SWPPAL, "Alpha_SWPPAL"},
    {FUNC_Alpha_SSIR, "Alpha_SSIR"},
    {FUNC_Alpha_CSIR, "Alpha_CSIR"},
    {FUNC_Alpha_RFE, "Alpha_RFE"},
    {FUNC_Alpha_RETSYS, "Alpha_RETSYS"},
    {FUNC_Alpha_RESTART, "Alpha_RESTART"},
    {FUNC_Alpha_SWPPROCESS, "Alpha_SWPPROCESS"},
    {FUNC_Alpha_RDMCES, "Alpha_RDMCES"},
    {FUNC_Alpha_WRMCES, "Alpha_WRMCES"},
    {FUNC_Alpha_TBIA, "Alpha_TBIA"},
    {FUNC_Alpha_TBIS, "Alpha_TBIS"},
    {FUNC_Alpha_TBISASN, "Alpha_TBISASN"},
    {FUNC_Alpha_RDKSP, "Alpha_RDKSP"},
    {FUNC_Alpha_SWPKSP, "Alpha_SWPKSP"},
    {FUNC_Alpha_RDPSR, "Alpha_RDPSR"},
    {FUNC_Alpha_REBOOT, "Alpha_REBOOT"},
    {FUNC_Alpha_CHMK, "Alpha_CHMK"},
    {FUNC_Alpha_CALLKD, "Alpha_CALLKD"},
    {FUNC_Alpha_GENTRAP, "Alpha_GENTRAP"},
    {FUNC_Alpha_KBPT, "Alpha_KBPT"},
#if defined(TRU64_BUILD)
    // Tru64 UNIX PAL Functions
    {FUNC_Tru64_REBOOT, "Tru64_REBOOT"},
    {FUNC_Tru64_INITPAL, "Tru64_INITPAL"},
    {FUNC_Tru64_SWPIRQL, "Tru64_SWPIRQL"},
    {FUNC_Tru64_RDIRQL, "Tru64_RDIRQL"},
    {FUNC_Tru64_DI, "Tru64_DI"},
    {FUNC_Tru64_RDMCES, "Tru64_RDMCES"},
    {FUNC_Tru64_WRMCES, "Tru64_WRMCES"},
    {FUNC_Tru64_RDPCBB, "Tru64_RDPCBB"},
    {FUNC_Tru64_WRPRBR, "Tru64_WRPRBR"},
    {FUNC_Tru64_TBIA, "Tru64_TBIA"},
    {FUNC_Tru64_TBIS, "Tru64_TBIS"},
    {FUNC_Tru64_DTBIS, "Tru64_DTBIS"},
    {FUNC_Tru64_TBISASN, "Tru64_TBISASN"},
    {FUNC_Tru64_RDKSP, "Tru64_RDKSP"},
    {FUNC_Tru64_SWPKSP, "Tru64_SWPKSP"},
    {FUNC_Tru64_WRPERFMON, "Tru64_WRPERFMON"},
    {FUNC_Tru64_SWPIPL, "Tru64_SWPIPL"},
    {FUNC_Tru64_RDUSP, "Tru64_RDUSP"},
    {FUNC_Tru64_WRUSP, "Tru64_WRUSP"},
    {FUNC_Tru64_RDCOUNTERS, "Tru64_RDCOUNTERS"},
    {FUNC_Tru64_CALLSYS, "Tru64_CALLSYS"},
    {FUNC_Tru64_SSIR, "Tru64_SSIR"},
    {FUNC_Tru64_WRIPIR, "Tru64_WRIPIR"},
    {FUNC_Tru64_RFE, "Tru64_RFE"},
    {FUNC_Tru64_RETSYS, "Tru64_RETSYS"},
    {FUNC_Tru64_RDPSR, "Tru64_RDPSR"},
    {FUNC_Tru64_RDPER, "Tru64_RDPER"},
    {FUNC_Tru64_RDTHREAD, "Tru64_RDTHREAD"},
    {FUNC_Tru64_SWPCTX, "Tru64_SWPCTX"},
    {FUNC_Tru64_WRFEN, "Tru64_WRFEN"},
    {FUNC_Tru64_RTI, "Tru64_RTI"},
    {FUNC_Tru64_RDUNIQUE, "Tru64_RDUNIQUE"},
    {FUNC_Tru64_WRUNIQUE, "Tru64_WRUNIQUE"}
#endif
};

// Function to get PAL function name by code
const char *getPALFunctionName(quint32 function)
{
    auto it = palFunctionNames.find(function);
    if (it != palFunctionNames.end())
    {
        return it.value();
    }

    // Return a generic name if not found
    static QString genericName;
    genericName = QString("PAL_0x%1").arg(function, 4, 16, QChar('0'));
    return genericName.toLatin1().constData();
}

// Function to get PAL function code by name (reverse lookup)
quint32 getPALFunctionCode(const QString &name)
{
    auto it = palFunctionNames.begin();
    while (it != palFunctionNames.end())
    {
        if (QString(it.value()) == name)
        {
            return it.key();
        }
        ++it;
    }

    // Try parsing as hex if direct lookup fails
    bool ok;
    if (name.startsWith("PAL_0x") || name.startsWith("0x"))
    {
        QString hexPart = name.startsWith("PAL_0x") ? name.mid(6) : name.mid(2);
        quint32 code = hexPart.toUInt(&ok, 16);
        if (ok)
        {
            return code;
        }
    }

    return 0xFFFFFFFF; // Invalid function code
}

// Function to get all PAL function names for a specific type
QStringList getPALFunctionNames(const QString &type)
{
    QStringList result;

    for (auto it = palFunctionNames.begin(); it != palFunctionNames.end(); ++it)
    {
        QString name = it.value();
        if (type == "Common" && !name.contains('_'))
        {
            result << name;
        }
        else if (type == "Alpha" && name.startsWith("Alpha_"))
        {
            result << name;
        }
        else if (type == "Tru64" && name.startsWith("Tru64_"))
        {
            result << name;
        }
        else if (type == "All")
        {
            result << name;
        }
    }

    result.sort();
    return result;
}

// Function to categorize PAL function by code
QString getPALFunctionCategory(quint32 function)
{
    // Check if it's a known function first
    if (palFunctionNames.contains(function))
    {
        QString name = palFunctionNames[function];
        if (name.startsWith("Alpha_"))
        {
            return "Alpha";
        }
        else if (name.startsWith("Tru64_"))
        {
            return "Tru64";
        }
        else
        {
            return "Common";
        }
    }

    // Categorize by function code ranges if unknown
    if (function <= 0x003F)
    {
        return "System";
    }
    else if (function >= 0x0080 && function <= 0x00BF)
    {
        return "SystemCall";
    }
    else if (function >= 0x0040 && function <= 0x007F)
    {
        return "Reserved";
    }
    else if (function >= 0x00C0 && function <= 0x00FF)
    {
        return "Implementation";
    }
    else
    {
        return "Unknown";
    }
}

// Function to check if PAL function requires kernel mode
bool isPALFunctionPrivileged(quint32 function)
{
    // Most PAL functions require kernel mode except system calls
    QString category = getPALFunctionCategory(function);
    return (category != "SystemCall");
}

// Function to get estimated execution cycles for PAL function
int getPALFunctionCycles(quint32 function)
{
    // Estimate based on function type and complexity
    QString name = getPALFunctionName(function);

    // System control functions (expensive)
    if (name.contains("HALT") || name.contains("REBOOT") || name.contains("SWPCTX"))
    {
        return 200;
    }

    // TLB operations (moderately expensive)
    if (name.contains("TBI") || name.contains("TLB"))
    {
        return 50;
    }

    // Cache operations (expensive)
    if (name.contains("CFLUSH") || name.contains("IMB"))
    {
        return 100;
    }

    // IPR operations (moderate)
    if (name.contains("MFPR") || name.contains("MTPR"))
    {
        return 10;
    }

    // System calls (moderate to expensive)
    if (name.contains("CHM") || name.contains("CALLSYS"))
    {
        return 30;
    }

    // Simple operations
    if (name.contains("RD") || name.contains("WR"))
    {
        return 5;
    }

    // Default estimate
    return 15;
}

// Debugging helper to print all PAL functions
void printAllPALFunctions()
{
    qDebug() << "=== PAL Function Directory ===";

    QStringList categories = {"Common", "Alpha", "Tru64"};

    for (const QString &category : categories)
    {
        qDebug() << QString("\n%1 PAL Functions:").arg(category);
        QStringList functions = getPALFunctionNames(category);

        for (const QString &funcName : functions)
        {
            quint32 code = getPALFunctionCode(funcName);
            if (code != 0xFFFFFFFF)
            {
                int cycles = getPALFunctionCycles(code);
                bool privileged = isPALFunctionPrivileged(code);
                qDebug() << QString("  0x%1: %2 [%3 cycles, %4]")
                                .arg(code, 4, 16, QChar('0'))
                                .arg(funcName)
                                .arg(cycles)
                                .arg(privileged ? "privileged" : "user");
            }
        }
    }
}

// Function to validate PAL function code
bool isValidPALFunction(quint32 function)
{
    // Check if it's in our known functions
    if (palFunctionNames.contains(function))
    {
        return true;
    }

    // Check if it's in valid ranges
    return (function <= 0x3FFFFFF); // 26-bit function code in CALL_PAL
}

// Function to format PAL instruction for disassembly
QString formatPALInstruction(quint32 instruction)
{
    if (!IS_CALL_PAL(instruction))
    {
        return "NOT_CALL_PAL";
    }

    quint32 function = EXTRACT_PAL_FUNCTION(instruction);
    const char *name = getPALFunctionName(function);

    return QString("CALL_PAL %1 ; 0x%2").arg(name).arg(function, 4, 16, QChar('0'));
}