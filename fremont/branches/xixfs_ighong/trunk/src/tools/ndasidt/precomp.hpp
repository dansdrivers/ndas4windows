// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change these values to use different versions

//#define WINVER		0x0500
//#define _WIN32_WINNT	0x0501
//#define _WIN32_IE		0x0501

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <shlwapi.h>
#include <atlwin.h>

#include <atlctl.h>
#include <atlmisc.h>
#include <atlctrls.h>
#include <atlcrack.h>

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

