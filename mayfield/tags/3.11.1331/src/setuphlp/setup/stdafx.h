// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change these values to use different versions
#ifndef WINVER
#define WINVER		0x0400
#endif
//#define _WIN32_WINNT	0x0400
#ifndef _WIN32_IE
#define _WIN32_IE	0x0400
#endif
#define _RICHEDIT_VER	0x0100

#include <atlbase.h>
#include <atlapp.h>

class CMyAppModule : public CAppModule
{
public:
	CMyAppModule() : m_wResLangId(0) {}
	LANGID m_wResLangId;
};


extern CMyAppModule _Module;

#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#define WTL_USE_CSTRING
#include <atlmisc.h>
#include <atlcrack.h>

#include <strsafe.h>
// #include <shlwapi.h>

#define SHLWAPI
#define WINBASE
#define WINNLS
