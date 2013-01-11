#include "stdafx.h"
#include "ndas/ndassyscfg.h"
#include "xs/xregcfg.h"

// {5D303CCE-0F53-4351-A881-D1CC828F47AE}
static CONST GUID NDASSYS_GUID = 
{ 0x5d303cce, 0xf53, 0x4351, { 0xa8, 0x81, 0xd1, 0xcc, 0x82, 0x8f, 0x47, 0xae } };

static LPCTSTR NDASSYS_REGKEY = _T("Software\\NDAS");

// exported values
LPCTSTR NDASSYS_KEYS_REGKEY = _T("Keys");
LPCTSTR NDASSYS_SERVICE_REGKEY	= _T("ndassvc");
LPCTSTR NDASSYS_SERVICE_ARFLAGS_REGVAL	= _T("ARFlags");

static xs::CXSRegistryCfg* 
pGetNdasSysCfg()
{
	static xs::CXSRegistryCfg cfg(HKEY_LOCAL_MACHINE, NDASSYS_REGKEY);
	static xs::CXSRegistryCfg* pCfg = NULL;
	if (NULL == pCfg) {
		BOOL fSuccess = cfg.SetEntropy(
			(LPBYTE)&NDASSYS_GUID, 
			sizeof(NDASSYS_GUID));
		if (!fSuccess) {
			return NULL;
		}
		pCfg = &cfg;
	}
	return pCfg;
}

BOOL 
WINAPI
NdasSysDeleteConfigValue(
	LPCTSTR szContainer,
	LPCTSTR szValueName)
{
	xs::CXSRegistryCfg* pCfg = pGetNdasSysCfg();
	return pCfg->DeleteValue(szContainer, szValueName);
}

BOOL 
WINAPI
NdasSysGetConfigValue(
	LPCTSTR szContainer,
	LPCTSTR szValueName,
	LPVOID lpValue,
	DWORD cbValue,
	LPDWORD pcbUsed)
{
	xs::CXSRegistryCfg* pCfg = pGetNdasSysCfg();
	if (NULL == pCfg) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Internal error %08X at SE01\n"), dwStatus);
		return FALSE;
	}

	BOOL fSuccess = pCfg->GetSecureValueEx(
		szContainer,
		szValueName,
		lpValue,
		cbValue,
		pcbUsed);

	if (!fSuccess) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Internal error %08X at SE02\n"), dwStatus);
		return FALSE;
	}

	return TRUE;
}

BOOL 
WINAPI
NdasSysSetConfigValue(
	LPCTSTR szContainer,
	LPCTSTR szValueName,
	CONST VOID* lpValue,
	DWORD cbValue)
{
	xs::CXSRegistryCfg* pCfg = pGetNdasSysCfg();
	if (NULL == pCfg) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Internal error %08X at SE01\n"), dwStatus);
		return FALSE;
	}

	BOOL fSuccess = pCfg->SetSecureValueEx(
		szContainer, 
		szValueName, 
		lpValue, 
		cbValue);

	if (!fSuccess) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Internal error %08X at SE02\n"), dwStatus);
		return FALSE;
	}

	return TRUE;
}

