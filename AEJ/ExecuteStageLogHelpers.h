#pragma once

/*==============================================================================
 *  ExecuteStageLogHelpers.H      ―  Alpha AXP integer-logical trace helpers
 *------------------------------------------------------------------------------
 *  Provides six small inline helpers that encapsulate the DEBUG_LOG() calls
 *  used by ExecuteStage’s integer-logical group.  They keep the main switch-
 *  table tidy and guarantee a consistent log format across all Boolean ops.
 *
 *  Architectural reference
 *    Alpha AXP System Reference Manual, v6 (1994)
 *      • Chapter 4, §4.5 “Boolean Instructions”, p.4-38  – AND / BIC / BIS
 *      • same page lists XOR, EQV, ORNOT mnemonics and semantics :contentReference[oaicite:0]{index=0}
 *------------------------------------------------------------------------------
 *  Usage example
 *      quint64 res = ra & rb;
 *      LOG_AND(ra, rb, res);      // emits: “ExecuteStage: AND 0x… & 0x… = 0x…”
 *------------------------------------------------------------------------------
 *  All code is header-only and requires:
 *      #include "DebugLog.h"      // supplies the DEBUG_LOG(QString) macro
 *      #include <QtCore/QString>  // QString, QChar
 *============================================================================*/
#pragma once
#include <QString>
#include "GlobalMacro.h"
#include <QString>

/* ───────────────────── generic little helper ───────────────────── */
static inline QString hex64(quint64 v, int w = 16) { return QStringLiteral("0x%1").arg(v, w, 16, QChar('0')); }

/* ───────────────────── Boolean-op trace macros ──────────────────── */
#define LOG_AND(a, b, r) DEBUG_LOG(QStringLiteral("AND    %1  &  %2  =  %3").arg(hex64(a)).arg(hex64(b)).arg(hex64(r)))
#define LOG_BIC(a, b, r) DEBUG_LOG(QStringLiteral("BIC    %1  & ~%2  =  %3").arg(hex64(a)).arg(hex64(b)).arg(hex64(r)))
#define LOG_BIS(a, b, r) DEBUG_LOG(QStringLiteral("BIS    %1  |  %2  =  %3").arg(hex64(a)).arg(hex64(b)).arg(hex64(r)))
#define LOG_XOR(a, b, r) DEBUG_LOG(QStringLiteral("XOR    %1  ^  %2  =  %3").arg(hex64(a)).arg(hex64(b)).arg(hex64(r)))
#define LOG_EQV(a, b, r) DEBUG_LOG(QStringLiteral("EQV   ~(%1 ^ %2) =  %3").arg(hex64(a)).arg(hex64(b)).arg(hex64(r)))
#define LOG_ORNOT(a, b, r) DEBUG_LOG(QStringLiteral("ORNOT  %1 | ~%2  =  %3").arg(hex64(a)).arg(hex64(b)).arg(hex64(r)))


#undef FMT16
