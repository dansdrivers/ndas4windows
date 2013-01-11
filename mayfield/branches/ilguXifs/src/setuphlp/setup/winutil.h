#pragma once

BOOL 
IsAdmin();

BOOL
MinimumWindowsPlatform(
	DWORD dwMajorVersion, 
	DWORD dwMinorVersion, 
	WORD wServicePackMajor);

BOOL 
IsPlatform(DWORD dwPlatformId);

BOOL
IsMsiOSSupported(DWORD dwMSIVersion);

BOOL 
IsMsiUpgradeNecessary(
	ULONG ulReqMsiMinVer);

HANDLE
RunMsiInstaller(
	LPCTSTR InstallCommand);

LPTSTR
GetErrorMessage(
	DWORD ErrorCode = ::GetLastError(),
	DWORD LanguageId = 0);

