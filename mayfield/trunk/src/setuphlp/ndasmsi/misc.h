#pragma once
#include "tokenparser.hpp"

//
// MSI CustomActionData Parser
//
static const LPCTSTR CAD_DEFAULT_PROPERTY_NAME = _T("CustomActionData");
static const TCHAR CAD_DEFAULT_DELIMITER = _T('|');

class CADParser : public CTokenParser
{

public:

	DWORD 
	Parse(
		MSIHANDLE hInstall, 
		LPCTSTR szPropName = CAD_DEFAULT_PROPERTY_NAME,
		TCHAR chDelimiter = CAD_DEFAULT_DELIMITER);
};

// Wrapper class for MSI Property
class CMsiProperty
{
	LPTSTR m_lpBuffer;
	void FreeBuffer();
public:
	CMsiProperty();
	~CMsiProperty();

	LPCTSTR 
	Get(
		MSIHANDLE hInstall,
		LPCTSTR szPropName = CAD_DEFAULT_PROPERTY_NAME, 
		LPDWORD pcchData = NULL);
};

LPTSTR
pMsiGetProperty(
	MSIHANDLE hInstall,
	LPCTSTR szPropName,
	LPDWORD pcchData);

//LPCTSTR 
//pGetNextToken(
//	LPCTSTR lpszStart, 
//	CONST TCHAR chToken);
//
//LPTSTR 
//pGetTokensV(
//	LPCTSTR szData, 
//	CONST TCHAR chToken, 
//	va_list ap);
//
//LPTSTR 
//pGetTokens(
//	LPCTSTR szData,
//	CONST TCHAR chToken, 
//	...);

UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError,
	LPCTSTR szErrorText);

UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError = GetLastError());

UINT 
pMsiMessageBox(
	MSIHANDLE,
	INT iErrorDialog,
	INSTALLMESSAGE Flags,
	LPCTSTR szText);

#include "netcomp.h"

NetClass 
pGetNetClass(LPCTSTR szNetClass);


static LPCTSTR FORCE_REBOOT_REG_KEY = _T("Software\\NDASSetup\\PrebootScheduled");

BOOL
pMsiSchedulePreboot(MSIHANDLE hInstall);

UINT
pMsiScheduleRebootForDeferredCA(MSIHANDLE hInstall);

UINT
pMsiScheduleReboot(MSIHANDLE hInstall);

BOOL 
pIsPrebootScheduled(MSIHANDLE hInstall);

BOOL
pIsDeferredRebootScheduled(MSIHANDLE hInstall);

BOOL 
pSetInstalledPath(
	MSIHANDLE hInstall,
	HKEY hKeyRoot, 
	LPCTSTR szRegPath, 
	LPCTSTR szValue, 
	LPCTSTR szData);

UINT
pMsiResetProgressBar(
	MSIHANDLE hInstall,
	INT totalTicks, 
	INT direction, 
	INT execType);

UINT
pMsiIncrementProgressBar(
	MSIHANDLE hInstall,
	INT ticks);

//
// Last Error Saver
//

class CReserveLastError
{
	DWORD m_err;
public:
	CReserveLastError() : m_err(::GetLastError()) {}
	~CReserveLastError() { ::SetLastError(m_err); }
};

#define RESERVE_LAST_ERROR() CReserveLastError _last_error_holder
