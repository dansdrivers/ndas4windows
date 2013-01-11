#include "stdafx.h"
#include <cfgmgr32.h>
#include <setupapi.h>
#include <cfg.h>
#include <xtl/xtlautores.h>

#include "trace.h"
#ifdef RUN_WPP
#include "misc.tmh"
#endif

BOOL
pIsWindowsXPOrLater()
{
	// Initialize the OSVERSIONINFOEX structure.
	OSVERSIONINFOEX osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 5;
	osvi.dwMinorVersion = 1;
	osvi.wServicePackMajor = 0;

	// Initialize the condition mask.
	DWORDLONG dwlConditionMask = 0;
	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

	// Perform the test.
	return VerifyVersionInfo(&osvi, 
		VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
		dwlConditionMask);
}
