#include "stdafx.h"
#include "msilog.h"
#include "misc.h"

VOID 
pMsiLogMessage(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szMessage)
{
	RESERVE_LAST_ERROR();

	PMSIHANDLE hRecord = MsiCreateRecord(2);

	TCHAR *fmtString = NULL;
	switch (Type) {
	case LMM_INFO:
		fmtString = _T("NDMSICA Info: Source: [1] Message: [2]");
		break;
	case LMM_ERROR:
		fmtString = _T("NDMSICA Error: Source: [1] Message: [2]");
		break;
	case LMM_WARNING:
		fmtString = _T("NDMSICA Warning: Source: [1] Message: [2]");
		break;
	default:
		fmtString = _T("NDMSICA: Source: [1] Message: [2]");
	}

	MsiRecordSetString(hRecord, 0, fmtString);
	MsiRecordSetString(hRecord, 1, (szSource) ? szSource : _T("(none)"));
	MsiRecordSetString(hRecord, 2, (szMessage) ? szMessage : _T("(none"));

	MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);

}

VOID 
pMsiLogMessageEx(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szFormat,
	...)
{
	RESERVE_LAST_ERROR();

	TCHAR szMessage[512]; // maximum 512 chars
	va_list ap;
	va_start(ap,szFormat);

	HRESULT hr = StringCchVPrintf(szMessage, 512, szFormat, ap);
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	pMsiLogMessage(hInstall, Type, szSource, szMessage);

	va_end(ap);
}

VOID 
pMsiLogError(
	MSIHANDLE hInstall, 
	LPCTSTR szSource, 
	LPCTSTR szCallee,
	DWORD dwError)
{
	RESERVE_LAST_ERROR();

	PMSIHANDLE hRecord = MsiCreateRecord(4);
	TCHAR* fmtString = 
		_T("NDMSICA Error: Source: [1] Callee: [2] Error: [3] [4]");

	MsiRecordSetString(hRecord, 0, fmtString);
	MsiRecordSetString(hRecord, 1, (szSource) ? szSource : _T("(none)"));
	MsiRecordSetString(hRecord, 2, (szCallee) ? szCallee : _T("(none)"));
	MsiRecordSetInteger(hRecord, 3, dwError);

	LPTSTR lpMsgBuf = NULL;
	DWORD cchMsgBuf = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), // We can only read English!!!
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);

	if (cchMsgBuf > 0) 
	{
		MsiRecordSetString(hRecord, 4, lpMsgBuf);
	}

	MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);

	if (NULL != lpMsgBuf) 
	{
		LocalFree(lpMsgBuf);
	}
}

LPTSTR
pGetSystemErrorText(DWORD dwError)
{
	RESERVE_LAST_ERROR();

	LPTSTR lpMsgBuf = NULL;
	DWORD cchMsgBuf = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // We can only read English!!!
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);
	if (0 == cchMsgBuf)
	{
		if (lpMsgBuf) 
		{
			LocalFree((HLOCAL)lpMsgBuf);
		}
		return NULL;
	}
	return lpMsgBuf;
}
