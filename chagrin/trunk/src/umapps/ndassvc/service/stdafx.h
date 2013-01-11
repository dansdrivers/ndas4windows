// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

//#define _WIN32_WINNT 0x0500 // Windows 2000 or later
#define STRICT
#include <winsock2.h>
#include <windows.h>
#include <basetsd.h>
#include <tchar.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <crtdbg.h>
#include <regstr.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <list>
#include <vector>
#include <map>
#include <queue>
#include <set>

// #include <winioctl.h>

//#include <xtl/xtlautores.h>
//#include <xtl/xtlthread.h>
//#include <xtl/xtlservice.h>
//#include <xtl/xtltrace.h>
//#include <xtl/xtllock.h>
#include <boost/shared_ptr.hpp>
#include <boost/mem_fn.hpp>
