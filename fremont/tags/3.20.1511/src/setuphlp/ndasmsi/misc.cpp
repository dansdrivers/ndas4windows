#include "stdafx.h"
#include <xtl/xtlautores.h>
#include "misc.h"
#include "msilog.h"
#include "fstrbuf.h"
#define TFN _T(__FUNCTION__)

//LPCTSTR 
//pGetNextToken(
//	LPCTSTR lpszStart, 
//	CONST TCHAR chToken)
//{
//	LPCTSTR pch = lpszStart;
//	while (chToken != *pch && _T('\0') != *pch) {
//		++pch;
//	}
//	return pch;
//}
//
//LPTSTR 
//pGetTokensV(
//	LPCTSTR szData, 
//	CONST TCHAR chToken,
//	va_list ap)
//{
//	size_t cch = 0;
//	HRESULT hr = ::StringCchLength(szData, STRSAFE_MAX_CCH, &cch);
//	if (FAILED(hr)) {
//		return NULL;
//	}
//
//	size_t cbOutput = (cch + 1) * sizeof(TCHAR);
//
//	LPTSTR lpOutput = (LPTSTR) ::LocalAlloc(
//		LPTR, cbOutput);
//
//	if (NULL == lpOutput) {
//		return NULL;
//	}
//
//	::CopyMemory(lpOutput, szData, cbOutput);
//
//	LPTSTR lpStart = lpOutput;
//	LPTSTR lpNext = lpStart;
//
//	while (TRUE) {
//
//		TCHAR** ppszToken = va_arg(ap, TCHAR**);
//
//		if (NULL == ppszToken) {
//			break;
//		}
//
//		if (_T('\0') != *lpStart) {
//
//			lpNext = (LPTSTR) pGetNextToken(lpStart, chToken);
//
//			if (_T('\0') == *lpNext) {
//				*ppszToken = lpStart;
//				lpStart = lpNext;
//			} else {
//				*lpNext = _T('\0');
//				*ppszToken = lpStart;
//				lpStart = lpNext + 1;
//			}
//
//		} else {
//			*ppszToken = lpStart; 
//		}
//
//	}
//
//	return lpOutput;
//}
//
//LPTSTR 
//pGetTokens(
//	LPCTSTR szData, 
//	CONST TCHAR chToken, ...)
//{
//	va_list ap;
//	va_start(ap, chToken);
//	LPTSTR lp = pGetTokensV(szData, chToken, ap);
//	va_end(ap);
//	return lp;
//}

NetClass 
pGetNetClass(LPCTSTR szNetClass)
{
	static const struct { LPCTSTR szClass; NetClass nc; } 
	nctable[] = {
		{_T("protocol"), NC_NetProtocol},
		{_T("adapter"), NC_NetAdapter},
		{_T("service"), NC_NetService},
		{_T("client"), NC_NetClient}
	};
	
	size_t nEntries = RTL_NUMBER_OF(nctable);

	for (size_t i = 0; i < nEntries; ++i) {
		if (0 == lstrcmpi(szNetClass, nctable[i].szClass)) {
			return nctable[i].nc;
		}
	}

	return NC_Unknown;
}


DWORD
CADParser::
Parse(
	MSIHANDLE hInstall, 
	LPCTSTR szPropName,
	TCHAR chDelimiter)
{
	LPTSTR lpProperty = pMsiGetProperty(hInstall, szPropName, NULL);
	if (NULL == lpProperty)
	{
		return 0;
	}
	// attaching to the auto resource
	XTL::AutoLocalHandle hLocal = (HLOCAL) lpProperty;
	return CTokenParser::Parse(lpProperty, chDelimiter);
}

CMsiProperty::
CMsiProperty() :
	m_lpBuffer(NULL)
{
}

CMsiProperty::
~CMsiProperty()
{
	FreeBuffer();
}

void
CMsiProperty::
FreeBuffer()
{
	if (m_lpBuffer)
	{
		HLOCAL hFreed = ::LocalFree((HLOCAL)m_lpBuffer);
		_ASSERTE(NULL == hFreed);
	}
}

LPCTSTR
CMsiProperty::
Get(
	MSIHANDLE hInstall, 
	LPCTSTR szPropName, 
	LPDWORD pcchData /* = NULL */)
{
	FreeBuffer();
	m_lpBuffer = pMsiGetProperty(hInstall, szPropName, pcchData);
	return m_lpBuffer;
}

LPTSTR 
pMsiGetProperty(
	MSIHANDLE hInstall,
	LPCTSTR szPropName,
	LPDWORD pcchData)
{
	UINT uiReturn = 0;
	LPTSTR lpszProperty = NULL;
	DWORD cchProperty = 0;

	uiReturn = ::MsiGetProperty(
		hInstall, 
		szPropName,
		_T(""),
		&cchProperty);

	switch (uiReturn) 
	{
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_PARAMETER:
		pMsiLogError(hInstall, TFN, _T("MsiGetProperty"), uiReturn);
		return NULL;
	}

	//
	// Add 1 as output does not include terminating null
	//
	++cchProperty;

	lpszProperty = (LPTSTR) ::LocalAlloc(
		LPTR, 
		(cchProperty) * sizeof(TCHAR));

	uiReturn = ::MsiGetProperty(
		hInstall, 
		szPropName,
		lpszProperty, 
		&cchProperty);

	switch (uiReturn) 
	{
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_PARAMETER:
	case ERROR_MORE_DATA:
		pMsiLogError(hInstall, TFN, _T("MsiGetProperty"), uiReturn);
		return NULL;
	}

	MSILOGINFO(FSB1024(_T("Prop %s: %s"), szPropName, lpszProperty));

	if (pcchData)
	{
		*pcchData = cchProperty;
	}

	return lpszProperty;
}

UINT 
pMsiMessageBox(
	MSIHANDLE hInstall,
	INT iErrorDialog,
	INSTALLMESSAGE Flags,
	LPCTSTR szText)
{
	PMSIHANDLE hActionRec = MsiCreateRecord(4);
	MsiRecordSetInteger(hActionRec, 1, iErrorDialog);

	//
	// (Workaround)
	// Send [4] as "\n" for more pretty text format.
	// Error Dialog Text property cannot contain "\n".
	// 

	MsiRecordSetString(hActionRec, 4, _T("\n"));
	MsiRecordSetString(hActionRec, 2, szText);

	UINT uiResponse = MsiProcessMessage(
		hInstall, 
		Flags,
		hActionRec);

	return uiResponse;
}

UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError,
	LPCTSTR szErrorText)
{
	TCHAR szErrorCodeText[255];

	HRESULT hr = StringCchPrintf(szErrorCodeText, 255, _T("0x%08X"), dwError);

	PMSIHANDLE hActionRec = MsiCreateRecord(4);
	MsiRecordSetInteger(hActionRec, 1, iErrorDialog);

	//
	// (Workaround)
	// Send [4] as "\n" for more pretty text format.
	// Error Dialog Text property cannot contain "\n".
	// 

	MsiRecordSetString(hActionRec, 4, _T("\n"));
	MsiRecordSetString(hActionRec, 2, szErrorCodeText);

	if (NULL == szErrorText) {
		MsiRecordSetString(hActionRec, 3, _T("(Description not available)"));
	} else {
		MsiRecordSetString(hActionRec, 3, szErrorText);
	}

	UINT uiResponse = MsiProcessMessage(
		hInstall, 
		INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_ABORTRETRYIGNORE|MB_ICONWARNING),
		hActionRec);

	return uiResponse;

}

UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError)
{
	LPTSTR lpszErrorText = pGetSystemErrorText(dwError);

	//
	// truncate \r or \n at the end of szErrorText
	//
	INT len = lstrlen(lpszErrorText);
	if (len > 0)
	{
		LPTSTR lpTrailing = &lpszErrorText[len-1];
		if (_T('\n') == *lpTrailing || _T('\r') == *lpTrailing)
		{
			*lpTrailing = _T('\0'); 
		}
		if (len > 1 && 
			(_T('\n') == *(lpTrailing-1) || _T('\r') == *(lpTrailing-1)))
		{
			*(lpTrailing-1) = _T('\0'); 
		}
	}

	UINT uiResponse = pMsiErrorMessageBox(
		hInstall, 
		iErrorDialog, 
		dwError, 
		lpszErrorText);

	if (lpszErrorText) 
	{
		LocalFree((HLOCAL)lpszErrorText);
	}

	return uiResponse;
}



UINT
pMsiScheduleRebootForDeferredCA(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;

	// Create a key for scheduled reboot
	LONG lResult = ::RegCreateKeyEx(
		HKEY_LOCAL_MACHINE, 
		_T("Software\\NDAS\\DeferredInstallReboot"),
		0,
		NULL,
		REG_OPTION_VOLATILE, // volatile key will be deleted after reboot
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);

	(VOID) ::RegCloseKey(hKey);

	MSILOGINFO(_T("ScheduleReboot For Deferred CA completed successfully."));

	return (UINT) lResult;
}

UINT
pMsiScheduleReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	UINT uiReturn = MsiDoAction(hInstall, _T("ScheduleReboot"));
	if (ERROR_SUCCESS != uiReturn) 
	{
		MSILOGERR_PROC2(_T("MsiDoAction(ScheduleReboot) failed"), uiReturn);
	}
	else
	{
		MSILOGINFO(_T("ScheduleReboot completed successfully."));
	}

	return uiReturn;
}

BOOL 
pMsiSchedulePreboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	HKEY hKey;

	//
	// This key is a volatile key, which will be delete on reboot
	//
	LONG lResult = ::RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,
		FORCE_REBOOT_REG_KEY,
		0,
		NULL,
		REG_OPTION_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);

	if (ERROR_SUCCESS != lResult) 
	{
		MSILOGERR_PROC2(_T("Scheduling Preboot failed: "), lResult);
		::SetLastError(lResult);
		return FALSE;
	}

	MSILOGINFO(_T("Preboot is now scheduled"));

	::RegCloseKey(hKey);
	return TRUE;
}

BOOL 
pIsPrebootScheduled(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	HKEY hKey;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		FORCE_REBOOT_REG_KEY,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lResult) 
	{
		MSILOGINFO(_T("Preboot is not previously scheduled"));
		return FALSE;
	}

	::RegCloseKey(hKey);

	MSILOGINFO(_T("Preboot is previously scheduled."));
	return TRUE;
}

//
// Deferred Custom Action to start a service if there is no deferred reboot
//

BOOL
pIsDeferredRebootScheduled(MSIHANDLE hInstall)
{
	HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, 
		_T("Software\\NDAS\\DeferredInstallReboot"),
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lResult)
	{
		// Not Scheduled!
		MSILOGINFO(_T("No deferred reboot."));
		return FALSE;
	}

	MSILOGINFO(_T("Deferred reboot is scheduled."));
	return TRUE;
}

BOOL 
pSetInstalledPath(
	MSIHANDLE hInstall,
	HKEY hKeyRoot, 
	LPCTSTR szRegPath, 
	LPCTSTR szValue, 
	LPCTSTR szData)
{
	HKEY hKey = NULL;
	DWORD dwDisp;
	LONG lResult = RegCreateKeyEx(
		hKeyRoot, 
		szRegPath, 
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		&dwDisp);

	//
	// Favor autoresource
	//

	XTL::AutoKeyHandle hKeyManaged = hKey;

	if (ERROR_SUCCESS != lResult) 
	{
		MSILOGERR_PROC2(FSB256(_T("Creating a reg key %s failed: "), szRegPath), lResult);
		return FALSE;
	}

	lResult = RegSetValueEx(
		hKey, 
		szValue, 
		0, 
		REG_SZ, 
		(const BYTE*) szData, 
		(lstrlen(szData) + 1) * sizeof(TCHAR));

	if (ERROR_SUCCESS != lResult) {
		MSILOGERR_PROC2(FSB256(_T("Setting a value %s:%s to %s failed"), szRegPath, szValue, szData), lResult);
		return FALSE;
	}

	MSILOGINFO(FSB256(_T("RegValueSet %s:%s to %s,\n"), szRegPath, szValue, szData));

	return TRUE;
}


UINT
pMsiResetProgressBar(
	MSIHANDLE hInstall,
	INT totalTicks, 
	INT direction, 
	INT execType)
{
	PMSIHANDLE hRecord = ::MsiCreateRecord(4);

	if (!hRecord)
	{
		return ERROR_INSTALL_FAILURE;
	}

	UINT msiRet = ::MsiRecordSetInteger(hRecord, 1, 0);
	if (ERROR_SUCCESS != msiRet)
	{
		return ERROR_INSTALL_FAILURE;
	}

	msiRet = ::MsiRecordSetInteger(hRecord, 2, totalTicks);
	if (ERROR_SUCCESS != msiRet)
	{
		return ERROR_INSTALL_FAILURE;
	}

	msiRet = ::MsiRecordSetInteger(hRecord, 3, direction);
	if (ERROR_SUCCESS != msiRet)
	{
		return ERROR_INSTALL_FAILURE;
	}

	msiRet = ::MsiRecordSetInteger(hRecord, 4, execType);
	if (ERROR_SUCCESS != msiRet)
	{
		return ERROR_INSTALL_FAILURE;
	}

	::MsiProcessMessage(hInstall, INSTALLMESSAGE_PROGRESS, hRecord);

	return ERROR_SUCCESS;
}

UINT
pMsiIncrementProgressBar(
	MSIHANDLE hInstall,
	INT ticks)
{
	PMSIHANDLE hRecord = ::MsiCreateRecord(4);

	if (!hRecord)
	{
		return ERROR_INSTALL_FAILURE;
	}

	UINT msiRet = ::MsiRecordSetInteger(hRecord, 1, 2);
	if (ERROR_SUCCESS != msiRet)
	{
		return ERROR_INSTALL_FAILURE;
	}

	msiRet = ::MsiRecordSetInteger(hRecord, 2, ticks);
	if (ERROR_SUCCESS != msiRet)
	{
		return ERROR_INSTALL_FAILURE;
	}

	::MsiProcessMessage(hInstall, INSTALLMESSAGE_PROGRESS, hRecord);

	return ERROR_SUCCESS;
}
