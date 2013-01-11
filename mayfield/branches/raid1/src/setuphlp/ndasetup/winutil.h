#pragma once
#include "setupui.h"

BOOL IsAdmin();

DWORD 
GetFileVersionNumber(
	LPCTSTR szFilename, 
	DWORD * pdwMSVer, 
	DWORD * pdwLSVer);

BOOL 
IsMsiUpgradeNecessary(ULONG ulReqMsiMinVer);

UINT 
UpgradeMsi(
	ISetupUI* pSetupUI,
	LPCTSTR szBase, 
	LPCTSTR szUpdate, 
	ULONG ulMinVer);

UINT
CacheMsi(
	ISetupUI* pSetupUI, 
	LPCTSTR szMsiFile, 
	LPTSTR szCachedMsiFile, 
	DWORD_PTR cchMax);

DWORD 
WaitForHandle(HANDLE handle, DWORD dwTimeout = INFINITE);

BOOL
MinimumWindowsPlatform(
	DWORD dwMajorVersion, 
	DWORD dwMinorVersion, 
	WORD wServicePackMajor);

BOOL 
IsPlatform(DWORD dwPlatformId);

BOOL
IsMsiOSSupported(DWORD dwMSIVersion);

