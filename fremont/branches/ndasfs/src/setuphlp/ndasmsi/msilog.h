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

#define TFN _T(__FUNCTION__)

#define MSILOGINFO(msg) pMsiLogMessage(hInstall, LMM_INFO, TFN, msg);
#define MSILOGERR(msg) pMsiLogMessage(hInstall, LMM_ERROR, TFN, msg);
#define MSILOGERR_PROC(proc) pMsiLogError(hInstall, TFN, proc);
#define MSILOGERR_PROC2(proc, err) pMsiLogError(hInstall, TFN, proc, err);

#define RUN_MODE_TRACE(hInstall)  \
	do { \
	if (::MsiGetMode(hInstall,MSIRUNMODE_ROLLBACK)) { \
		MSILOGINFO(_T("Rollback Action Started")); \
	} else if (::MsiGetMode(hInstall,MSIRUNMODE_COMMIT)) { \
		MSILOGINFO(_T("Commit Action Started")); \
	} else if (::MsiGetMode(hInstall,MSIRUNMODE_SCHEDULED)) { \
		MSILOGINFO(_T("Scheduled Action Started")); \
	} else { \
		MSILOGINFO(_T("Action Started")); \
	} } while(0)


