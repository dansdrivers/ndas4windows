// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change these values to use different versions

//#ifdef WINVER
//#undef WINVER
//#endif
//#ifdef _WIN32_WINNT
//#undef _WIN32_WINNT
//#endif
//#ifdef _WIN32_IE
//#undef _WIN32_IE
//#endif
#ifdef _RICHEDIT_VER
#undef _RICHEDIT_VER
#endif

//#define WINVER		0x0500
//#define _WIN32_WINNT	0x0501
//#define _WIN32_IE		0x0501
#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#define _RICHEDIT_VER	0x0100

// You need to declare this to use WTL style wizard
//
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS
#define _WTL_USE_CSTRING

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <shellapi.h> // required for atlctrlx.h
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

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
