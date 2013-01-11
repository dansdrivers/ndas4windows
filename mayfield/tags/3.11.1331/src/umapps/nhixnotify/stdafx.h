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
#ifndef WINVER		
#define _WIN32_IE	0x0400
#endif 
#define _RICHEDIT_VER	0x0100

#include <windows.h>
#include <tchar.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <crtdbg.h>
#include <map>


