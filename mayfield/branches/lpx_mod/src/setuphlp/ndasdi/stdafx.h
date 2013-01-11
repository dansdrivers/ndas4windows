// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

//#define _WIN32_WINNT 0x0500 // Windows 2000 or later
#define STRICT
#include <windows.h>
#include <basetsd.h>
#include <tchar.h>
#include <strsafe.h>
#include <crtdbg.h>
//    NO_SHLWAPI_STRFCNS    String functions
//    NO_SHLWAPI_PATH       Path functions
//    NO_SHLWAPI_REG        Registry functions
//    NO_SHLWAPI_STREAM     Stream functions
//    NO_SHLWAPI_GDI        GDI helper functions
#define NO_SHLWAPI_STRFCNS
#define NO_SHLWAPI_REG
#define NO_SHLWAPI_STREAM
#define NO_SHLWAPI_GDI
#include <shlwapi.h>
#include <setupapi.h> 

// for netcomp.cpp
#include <netcfgx.h>
#include <netcfgn.h>
// #include <setupapi.h>

#include <devguid.h>
