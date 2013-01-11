#include "stdafx.h"
#include "appconf.h"
#include "regconf.h"

static LPCTSTR NDAS_CFG_SUBKEY = TEXT("Software\\NDAS");
static LPCTSTR APP_CFG_SUBKEY = TEXT("Software\\NDAS\\ndasmgmt");

static CRegistryCfg* _pNdasCfg = NULL;
static CRegistryCfg* _pSystemCfg = NULL;
static CRegistryCfg* _pUserCfg = NULL;

void InitAppConfig()
{
	static CRegistryCfg ndasCfg(HKEY_LOCAL_MACHINE, NDAS_CFG_SUBKEY);
	_pNdasCfg = &ndasCfg;

	static CRegistryCfg sysCfg(HKEY_LOCAL_MACHINE, APP_CFG_SUBKEY);
	_pSystemCfg = &sysCfg;

	static CRegistryCfg userCfg(HKEY_CURRENT_USER, APP_CFG_SUBKEY);
	_pUserCfg = &userCfg;
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

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPDWORD lpdwValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	BOOL fSuccess = FALSE;

	CRegistryCfg* pCfg = _pUserCfg;
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

	CRegistryCfg* pCfg = _pUserCfg;
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

	CRegistryCfg* pCfg = _pUserCfg;
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
		_pSystemCfg = _pSystemCfg;
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
		_pSystemCfg = _pSystemCfg;
	}
	return pCfg->SetValue(szConfigName,dwValue);
}

BOOL pSetAppConfigValueBOOL(
	LPCTSTR szConfigName, BOOL fValue, DWORD dwFlags)
{
	_ASSERTE(NULL != _pUserCfg && NULL != _pSystemCfg);
	_ASSERTE(!NACFG_INVALID_FLAGS(dwFlags));

	CRegistryCfg* pCfg = _pUserCfg;
	if (dwFlags & NACFG_SYSTEM) {
		_pSystemCfg = _pSystemCfg;
	}
	return pCfg->SetValue(szConfigName,fValue);
}
