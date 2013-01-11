#include "stdafx.h"
#include "appconf.h"
#include "regconf.h"

static LPCTSTR HOST_CFG_SUBKEY = TEXT("Software\\NDAS\\Host");
static LPCTSTR APP_CFG_SUBKEY = TEXT("Software\\NDAS\\ndasbind");

static CRegistryCfg* _pSystemCfg = NULL;
static CRegistryCfg* _pUserCfg = NULL;

BOOL InitSystemCfg()
{
	_pSystemCfg = new CRegistryCfg(HKEY_LOCAL_MACHINE,APP_CFG_SUBKEY);
	return (NULL != _pSystemCfg);
}

VOID CleanupSystemCfg()
{
	delete _pSystemCfg;
	_pSystemCfg = NULL;
}

BOOL InitUserCfg()
{
	_pUserCfg = new CRegistryCfg(HKEY_CURRENT_USER,APP_CFG_SUBKEY);
	return (NULL != _pUserCfg);
}

VOID CleanupUserCfg()
{
	delete _pUserCfg;
	_pUserCfg = NULL;
}

CRegistryCfg* pGetSystemCfg()
{
	return _pSystemCfg;
}

CRegistryCfg* pGetUserCfg()
{
	return _pUserCfg;
}

#define NACFG_INVALID_FLAGS(flags) (((~NACFG_ALL) & (flags)) > 0)

LPCGUID
pGetNdasHostGuid()
{
	static LPGUID pHostGuid = NULL;
	static GUID hostGuid;

	if (NULL != pHostGuid) {
		return pHostGuid;
	}

	CRegistryCfg *pNDASCfg = new CRegistryCfg(HKEY_LOCAL_MACHINE, HOST_CFG_SUBKEY);
	pNDASCfg->GetValue(_T("HostID"),  (LPTSTR)&hostGuid, sizeof(GUID));
	delete pNDASCfg;

	pHostGuid = &hostGuid;
	return pHostGuid;
}

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPDWORD lpdwValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	BOOL fSuccess = FALSE;

	if (dwFlags & NACFG_USER) {
		fSuccess = _pUserCfg->GetValue(szConfigName, lpdwValue);
		if (fSuccess) {
			return TRUE;
		}
	}

	if (dwFlags & NACFG_SYSTEM) {
		fSuccess = _pSystemCfg->GetValue(szConfigName, lpdwValue);
		if (fSuccess) {
			return TRUE;
		}
	}

	return FALSE;
}

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPBOOL lpbValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	BOOL fSuccess = FALSE;

	if (dwFlags & NACFG_USER) {
		fSuccess = _pUserCfg->GetValue(szConfigName, lpbValue);
		if (fSuccess) {
			return TRUE;
		}
	}

	if (dwFlags & NACFG_SYSTEM) {
		fSuccess = _pSystemCfg->GetValue(szConfigName, lpbValue);
		if (fSuccess) {
			return TRUE;
		}
	}

	return FALSE;
}

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPTSTR lpszValue, DWORD cchMaxValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	BOOL fSuccess = FALSE;

	if (dwFlags & NACFG_USER) {
		fSuccess = _pUserCfg->GetValue(szConfigName, lpszValue,	cchMaxValue*sizeof(WCHAR));
		if (fSuccess) {
			return TRUE;
		}
	}

	if (dwFlags & NACFG_SYSTEM) {
		fSuccess = _pSystemCfg->GetValue(szConfigName, lpszValue, cchMaxValue*sizeof(WCHAR));
		if (fSuccess) {
			return TRUE;
		}
	}

	return FALSE;
}

BOOL pSetAppConfigValue(
	LPCTSTR szConfigName, LPCTSTR szValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	CRegistryCfg* pCfg = _pUserCfg;
	if (dwFlags & NACFG_SYSTEM) {
		pCfg = _pSystemCfg;
	}
	return pCfg->SetValue(szConfigName,szValue);
}

BOOL pSetAppConfigValue(
	LPCTSTR szConfigName, DWORD dwValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	CRegistryCfg* pCfg = _pUserCfg;
	if (dwFlags & NACFG_SYSTEM) {
		pCfg = _pSystemCfg;
	}
	return pCfg->SetValue(szConfigName,dwValue);
}

BOOL pSetAppConfigValue(
	LPCTSTR szConfigName, BOOL fValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	CRegistryCfg* pCfg = _pUserCfg;
	if (dwFlags & NACFG_SYSTEM) {
		pCfg = _pSystemCfg;
	}
	return pCfg->SetValue(szConfigName,fValue);
}
