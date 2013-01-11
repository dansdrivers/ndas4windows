#pragma once
#include <windows.h>

// &GUID_DEVCLASS_SCSIADAPTER

BOOL
__stdcall
NdasDiInstallDeviceDriver(
	CONST GUID* classGUID, 
	LPCTSTR devInstID, 
	LPCTSTR szDriverPath);

typedef BOOL (CALLBACK* NDASDIENUMPROC)(LPCTSTR devInstID, LPVOID lpContext);

BOOL
__stdcall
NdasDiEnumUnconfigedDevices(
	LPCTSTR szFilter, 
	NDASDIENUMPROC enumProc, 
	LPVOID lpContext);

