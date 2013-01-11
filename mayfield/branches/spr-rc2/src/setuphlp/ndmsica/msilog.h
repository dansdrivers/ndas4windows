#pragma once

typedef enum _LOGMSIMESSAGE_TYPE {
	LMM_ERROR,
	LMM_INFO,
	LMM_WARNING
} LOGMSIMESSAGE_TYPE;

VOID 
pMsiLogMessage(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szMessage);

VOID 
pMsiLogError(
	MSIHANDLE hInstall, 
	LPCTSTR szSource, 
	LPCTSTR szCallee,
	DWORD dwError = GetLastError());

VOID 
pMsiLogMessageEx(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szFormat,
	...);

LPTSTR 
pGetSystemErrorText(DWORD dwError);

