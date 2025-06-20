#pragma once


/*=============================================================================
 *  PCIConfigWindow.H
 *  ---------------------------------------------------------------------------
 *  Window-class helper for a GUI front-end that exposes Alpha-PCI
 *  configuration-space registers to the user.
 *
 *  ─  All structures/constants follow the little-endian byte-addressing
 *     model defined for Alpha AXP systems (see §2.1 “Addressing”,
 *     Alpha AXP System Reference Manual p. 2-1). :contentReference[oaicite:0]{index=0}
 *  ─  The class is intended for configuration utilities that visualise /
 *     edit the 256-byte PCI header mapped by console firmware (see Fig. 8-1,
 *     “Alpha AXP System Overview”, p. 8-1). :contentReference[oaicite:1]{index=1}
 *
 *  Each function is documented with a reference to the relevant Win32 API
 *  entry (MSDN RegisterClassEx, 2022-11-08) for portability reasons.
 *===========================================================================*/

#ifndef  PCICONFIGWINDOW_H
#define  PCICONFIGWINDOW_H

#include <windows.h>

 /* -------------------------------------------------------------------------
  *  Compile-time constant that identifies the registered class name.
  * ---------------------------------------------------------------------- */
#define PCI_CFG_WND_CLASS_NAME  TEXT("PCIConfigWindow")

#ifdef __cplusplus
extern "C" {
#endif

    /* -------------------------------------------------------------------------
     *  BOOL  RegisterPCIConfigWindowClass( HINSTANCE hInstance )
     *  ----------------------------------------------------------------------
     *  Registers the PCIConfigWindow class with the system.  Must be invoked
     *  once—typically from WinMain—before any window of this class is created.
     *
     *  Returns: TRUE on success, FALSE on failure (inspect GetLastError()).
     *
     *  Reference:  “RegisterClassEx function”, Win32 API, MSDN Library,
     *              2022-11-08. (Used here to populate a WNDCLASSEX structure
     *              with CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS style flags.)
     * ---------------------------------------------------------------------- */
    BOOL RegisterPCIConfigWindowClass(HINSTANCE hInstance);

    /* -------------------------------------------------------------------------
     *  LRESULT  CALLBACK  PCIConfigWindowProc( … )
     *  ----------------------------------------------------------------------
     *  Standard window procedure.  In addition to generic WM_* messages it
     *  recognises:
     *
     *      WM_USER + 0x100 … WM_USER + 0x1FF
     *          • Reads from / writes to the PCI configuration d-word whose
     *            offset equals (uMsg & 0xFF) * 4.
     *          • Access rules mirror Alpha console PAL-code behaviour.
     *
     *  The handler performs no UI drawing unless a WM_PAINT is received.
     * ---------------------------------------------------------------------- */
    LRESULT CALLBACK PCIConfigWindowProc(HWND   hwnd,
        UINT   uMsg,
        WPARAM wParam,
        LPARAM lParam);

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* PCICONFIGWINDOW_H */
