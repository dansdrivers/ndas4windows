#include "stdafx.h"
#include "ndas/ndasautoregscope.h"
#include "ndas/ndascntenc.h"
#include "fstrbuf.h"
#include "misc.h"
#include "msilog.h"

//
// Set Encryption System Key
//
// Commit Custom Action
//
// Custom Action Data contains the path to the key file
// If the file exists, This function tries to import the key file.
//

UINT 
__stdcall
NDMsiSetECFlags(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	LPCTSTR pszFileName;
	CADParser parser;
	parser.AddToken<LPCTSTR>(pszFileName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Token paring failure"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("Importing SysKey file from: %s\n"), pszFileName));

	UINT uiRet = ::NdasEncImportSysKeyFromFile(pszFileName);

	if (ERROR_SUCCESS != uiRet) 
	{
		MSILOGERR(FSB256(_T("Importing SysKey from %s failed: RetCode %08X"), pszFileName, uiRet));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("SysKey imported from %s successfully\n"), pszFileName));

	return ERROR_SUCCESS;
}

//
// Set the auto registration flag
//
// Commit Custom Action
//
UINT 
__stdcall
NDMsiSetARFlags(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	const static NDAS_DEVICE_ID REDDOTNET_ALL_AR_BEGIN = { 0x00, 0x0B, 0xD0, 0x27, 0x60, 0x00 };
	const static NDAS_DEVICE_ID REDDOTNET_ALL_AR_END =	{ 0x00, 0x0B, 0xD0, 0x27, 0x9F, 0xFF };
	const static ACCESS_MASK REDDOTNET_ALL_AR_ACCESS = 	GENERIC_READ | GENERIC_WRITE;

	const static NDAS_DEVICE_ID REDDOTNET_A_AR_BEGIN =	{ 0x00, 0x0B, 0xD0, 0x27, 0x80, 0x00 };
	const static NDAS_DEVICE_ID REDDOTNET_A_AR_END =	{ 0x00, 0x0B, 0xD0, 0x27, 0x9F, 0xFF };
	const static ACCESS_MASK REDDOTNET_A_AR_ACCESS = 	GENERIC_READ | GENERIC_WRITE;

	const static NDAS_DEVICE_ID REDDOTNET_B_AR_BEGIN =	{ 0x00, 0x0B, 0xD0, 0x27, 0x60, 0x00 };
	const static NDAS_DEVICE_ID REDDOTNET_B_AR_END =	{ 0x00, 0x0B, 0xD0, 0x27, 0x7F, 0xFF };
	const static ACCESS_MASK REDDOTNET_B_AR_ACCESS =	GENERIC_READ | GENERIC_WRITE;

	CNdasAutoRegScopeData arsData;

	LPCTSTR pszScopeName;
	CADParser parser;
	parser.AddToken<LPCTSTR>(pszScopeName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Token paring failure"));
		return ERROR_INSTALL_FAILURE;
	}

	//
	// For setup products, Auto Registration Scopes are hard-coded
	// by the OEM partners to prevent tampering with these values
	//

	NDAS_DEVICE_ID scopeBegin, scopeEnd;
	ACCESS_MASK grantedAccess;

	if (0 == ::lstrcmpi(_T("REDDOTNET_A"), pszScopeName)) 
	{
		scopeBegin = REDDOTNET_A_AR_BEGIN;
		scopeEnd = REDDOTNET_A_AR_END;
		grantedAccess = REDDOTNET_A_AR_ACCESS;
	}
	else if (0 == ::lstrcmpi(_T("REDDOTNET_B"), pszScopeName)) 
	{
		scopeBegin = REDDOTNET_B_AR_BEGIN;
		scopeEnd = REDDOTNET_B_AR_END;
		grantedAccess = REDDOTNET_B_AR_ACCESS;
	}
	else 
	{
		// ignore invalid AR flags
		MSILOGERR(FSB256(_T("Invalid scope name (%s)"), pszScopeName));
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fSuccess = arsData.AddScope(scopeBegin, scopeEnd, grantedAccess);
	if (!fSuccess) 
	{
		MSILOGERR(FSB256(_T("AddScope(%s) failed"), pszScopeName));
		return ERROR_INSTALL_FAILURE;
	}

	fSuccess = arsData.SaveToSystem();
	if (!fSuccess) 
	{
		MSILOGERR(FSB256(_T("SaveToSystem(%s) failed"), pszScopeName));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("ARS Data is set to %s successfully\n"), pszScopeName));

	return ERROR_SUCCESS;
}
