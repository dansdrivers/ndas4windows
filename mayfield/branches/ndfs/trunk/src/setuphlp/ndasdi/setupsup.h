#pragma once

#ifndef SUOI_FORCEDELETE
#define SUOI_FORCEDELETE   0x00000001
#endif

BOOL WINAPI
Setupapi_SetupUninstallOEMInf(
	LPCTSTR szInfFileName,
	DWORD Flags,
	PVOID Reserved);


