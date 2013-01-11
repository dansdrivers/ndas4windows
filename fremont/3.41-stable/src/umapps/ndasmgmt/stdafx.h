// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change these values to use different versions

#ifdef _RICHEDIT_VER
#undef _RICHEDIT_VER
#endif
#define _RICHEDIT_VER	0x0100

// You need to declare this to use WTL style wizard
//
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS
// #define _WTL_USE_CSTRING
#define _WTL_NO_CSTRING

#include <atlbase.h>
#include <atlstr.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <shellapi.h> // required for atlctrlx.h

#define _WTL_TASKDIALOG

#include <atlframe.h>
#include <atlwin.h>
#include <atlctl.h>
#include <atlmisc.h>
#include <atlddx.h>
#include <atldlgs.h>
#include <atlddx.h>
#include <atlctrls.h>

#include <atlctrlx.h>
#include <atlctrlw.h>
#include <atlcoll.h>

#include <atlcrack.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <shlwapi.h>
#include <dde.h>

#include <xtaskdlg/xtaskdlg.h>
#include "atldlgs_ext.h"

using WTLEX::AtlTaskDialogEx;
using WTLEX::CTaskDialogEx;
using WTLEX::CTaskDialogExImpl;

#include <vector>
#include <algorithm>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <ndas/ndasuidebug.h>

// BALLOONTIP
// #if (_WIN32_WINNT >= 0x501)
#if (_WIN32_WINNT < 0x501)
#define ECM_FIRST               0x1500      // Edit control messages
#define BCM_FIRST               0x1600      // Button control messages
#define CBM_FIRST               0x1700      // Combobox control messages
#endif

// #if (_WIN32_WINNT >= 0x501)
#if (_WIN32_WINNT < 0x501)
#define	EM_SETCUEBANNER	    (ECM_FIRST + 1)		// Set the cue banner with the lParm = LPCWSTR
#define Edit_SetCueBannerText(hwnd, lpcwText) \
        (BOOL)SNDMSG((hwnd), EM_SETCUEBANNER, 0, (LPARAM)(lpcwText))
#define	EM_GETCUEBANNER	    (ECM_FIRST + 2)		// Set the cue banner with the lParm = LPCWSTR
#define Edit_GetCueBannerText(hwnd, lpwText, cchText) \
        (BOOL)SNDMSG((hwnd), EM_GETCUEBANNER, (WPARAM)(lpwText), (LPARAM)(cchText))

typedef struct _tagEDITBALLOONTIP
{
    DWORD   cbStruct;
    LPCWSTR pszTitle;
    LPCWSTR pszText;
    INT     ttiIcon; // From TTI_*
} EDITBALLOONTIP, *PEDITBALLOONTIP;
#define	EM_SHOWBALLOONTIP   (ECM_FIRST + 3)		// Show a balloon tip associated to the edit control
#define Edit_ShowBalloonTip(hwnd, peditballoontip) \
        (BOOL)SNDMSG((hwnd), EM_SHOWBALLOONTIP, 0, (LPARAM)(peditballoontip))
#define EM_HIDEBALLOONTIP   (ECM_FIRST + 4)     // Hide any balloon tip associated with the edit control
#define Edit_HideBalloonTip(hwnd) \
        (BOOL)SNDMSG((hwnd), EM_HIDEBALLOONTIP, 0, 0)
#endif

inline bool IsWindowsVistaOrLater()
{
	OSVERSIONINFO osvi = { sizeof(OSVERSIONINFO) };
	ATLVERIFY(GetVersionEx(&osvi));
	return (osvi.dwMajorVersion >= 6);
}

#ifndef COMVERIFY
#define COMVERIFY(x) ATLVERIFY(SUCCEEDED(x))
#endif
