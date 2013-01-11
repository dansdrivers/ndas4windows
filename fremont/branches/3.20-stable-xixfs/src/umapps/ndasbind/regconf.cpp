#include "stdafx.h"
#include <wincrypt.h>
#include "regconf.h"
#include <shlwapi.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")

CRegistryCfg::CRegistryCfg(
	LPCTSTR szConfRegKey, 
	IRegistryCfgDefaultValue* pDefaultValueProvider) :
	m_bEntropyProtected(FALSE),
	m_hCfgRootKey(HKEY_LOCAL_MACHINE),
	m_pDefaultValueProvider(pDefaultValueProvider)
{
	m_Entropy.cbData = 0;
	m_Entropy.pbData = NULL;

	HRESULT hr = ::StringCchPrintf(m_szCfgRegKey, _MAX_PATH + 1, TEXT("%s"), szConfRegKey);
	_ASSERT(SUCCEEDED(hr));
}

CRegistryCfg::CRegistryCfg(
	HKEY hRootKey, 
	LPCTSTR szConfRegKey,
	IRegistryCfgDefaultValue* pDefaultValueProvider) :
	m_bEntropyProtected(FALSE),
	m_hCfgRootKey(hRootKey),
	m_pDefaultValueProvider(pDefaultValueProvider)
{
	m_Entropy.cbData = 0;
	m_Entropy.pbData = NULL;

	HRESULT hr = ::StringCchPrintf(m_szCfgRegKey, _MAX_PATH + 1,TEXT("%s"), szConfRegKey);
	_ASSERT(SUCCEEDED(hr));
}

CRegistryCfg::~CRegistryCfg()
{
	if (m_Entropy.pbData) {
		::SecureZeroMemory(m_Entropy.pbData, m_Entropy.cbData);
		::LocalFree(m_Entropy.pbData);
	}
}

VOID
CRegistryCfg::SetCfgRegKey(LPCTSTR szCfgRegKey)
{
	HRESULT hr = ::StringCchPrintf(m_szCfgRegKey, _MAX_PATH + 1, TEXT("%s"), szCfgRegKey);
	_ASSERTE(SUCCEEDED(hr));
}

VOID
CRegistryCfg::SetDefaultValueProvider(IRegistryCfgDefaultValue *pProvider)
{
	m_pDefaultValueProvider = pProvider;
}

BOOL 
CRegistryCfg::DeleteContainer(LPCTSTR szContainer, BOOL bDeleteSubs)
{
	HKEY hKey = OpenRegKey(m_hCfgRootKey, szContainer, DELETE);
	if (INVALID_HANDLE_VALUE == hKey) {
		return FALSE;
	}
	if (bDeleteSubs) {
		DWORD dwError = SHDeleteKey(hKey, NULL);
		if (ERROR_SUCCESS != dwError) {
			::SetLastError(dwError);
			::RegCloseKey(hKey);
			return FALSE;
		}
	} else {
		LONG lResult = ::RegDeleteKey(hKey, NULL);
		if (ERROR_SUCCESS != lResult) {
			::SetLastError(lResult);
			::RegCloseKey(hKey);
			return FALSE;
		}
	}
	::RegCloseKey(hKey);
	return TRUE;
}

BOOL 
CRegistryCfg::DeleteValue(LPCTSTR szContainer, LPCTSTR szValueName)
{
	HKEY hKey = OpenRegKey(m_hCfgRootKey, szContainer, KEY_SET_VALUE);
	if (INVALID_HANDLE_VALUE == hKey) {
		return FALSE;
	}
	LONG lResult = ::RegDeleteValue(hKey, szValueName);
	if (ERROR_SUCCESS != lResult) {
		::SetLastError(lResult);
		::RegCloseKey(hKey);
		return FALSE;
	}
	::RegCloseKey(hKey);
	return TRUE;
}

HKEY
CRegistryCfg::OpenRegKey(
	HKEY hRootKey,
	LPCTSTR szContainer,
	REGSAM samDesired,
	LPBOOL lpbCreate)
{
	TCHAR szRegPath[_MAX_PATH + 1] = {0};
	TCHAR* pszSubKey = szRegPath;

	if (NULL != szContainer) {
		HRESULT hr = ::StringCchPrintf(pszSubKey, _MAX_PATH + 1, 
			TEXT("%s\\%s"), m_szCfgRegKey, szContainer);
		if (FAILED(hr)) {
			return NULL;
		}
	} else {
		pszSubKey = (TCHAR*) m_szCfgRegKey;
	}

	HKEY hKey;
	if (lpbCreate && *lpbCreate) {
		DWORD dwDisposition;
		LONG lResult = ::RegCreateKeyEx(
			hRootKey, 
			pszSubKey, 
			0, 
			NULL, 
			REG_OPTION_NON_VOLATILE,
			samDesired,
			NULL,
			&hKey,
			&dwDisposition);
		if (ERROR_SUCCESS != lResult) {
			::SetLastError(lResult);
			return NULL;
		}
		*lpbCreate = (dwDisposition == REG_CREATED_NEW_KEY) ? TRUE : FALSE;

	} else {
		LONG lResult = ::RegOpenKeyEx(hRootKey, pszSubKey, 0, samDesired, &hKey);
		if (ERROR_SUCCESS != lResult) {
			::SetLastError(lResult);
			return NULL;
		}

	}

	return hKey;
}

BOOL 
CRegistryCfg::GetRegValueEx(
	HKEY hRootKey,
	LPCTSTR szContainer, 
	LPCTSTR szValueName, LPDWORD lpdwRegValueType,
	LPBYTE lpValue, DWORD cbBuffer, LPDWORD lpcbUsed)
{
	HKEY hKey = OpenRegKey(hRootKey, szContainer, KEY_READ);
	if (INVALID_HANDLE_VALUE == hKey) {
		return FALSE;
	}

	DWORD dwType;
	DWORD cbData = cbBuffer;
	LONG lResult = ::RegQueryValueEx(hKey, szValueName, NULL, &dwType, lpValue, &cbData);
	if (ERROR_SUCCESS != lResult) {
		::SetLastError(lResult);
		::RegCloseKey(hKey);
		return FALSE;
	}

	if (NULL != lpcbUsed) {
		*lpcbUsed = cbData;
	}
	
	if (NULL != lpdwRegValueType) {
		*lpdwRegValueType = dwType;
	}

	::RegCloseKey(hKey);
	return TRUE;
}

BOOL 
CRegistryCfg::SetRegValueEx(
	HKEY hRootKey,
	LPCTSTR szContainer, 
	LPCTSTR szValueName, DWORD dwRegValueType,
	const BYTE* lpValue, DWORD cbValue)
{
	BOOL bCreate(TRUE);
	HKEY hKey = OpenRegKey(hRootKey, szContainer, KEY_WRITE, &bCreate);
	if (INVALID_HANDLE_VALUE == hKey) {
		return FALSE;
	}

	LONG lResult = ::RegSetValueEx(
		hKey, 
		szValueName, 
		NULL, 
		dwRegValueType, 
		lpValue, 
		cbValue);

	if (ERROR_SUCCESS != lResult) {
		::RegCloseKey(hKey);
		return FALSE;
	}

	::RegCloseKey(hKey);
	return TRUE;
}

BOOL
CRegistryCfg::ProtectEntropy(BOOL bProtect)
{
#ifdef USE_CRYPT_PROTECT_MEMORY
	if (bProtect) {
		if (m_bEntropyProtected) {
			return TRUE;
		}
		if (m_Entropy.cbData > 0) {
			BOOL fSuccess = ::CryptProtectMemory(
				m_Entropy.pbData,
				m_Entropy.cbData,
				CRYPTPROTECTMEMORY_SAME_PROCESS);

			if (!fSuccess) {
				return FALSE;
			}
		}
		m_bEntropyProtected = TRUE;
	}
	else
	{
		if (!m_bEntropyProtected) {
			return TRUE;
		}
		if (m_Entropy.cbData > 0) {
			BOOL fSuccess = ::CryptUnprotectMemory(
				m_Entropy.pbData,
				m_Entropy.cbData,
				CRYPTPROTECTMEMORY_SAME_PROCESS);

			if (!fSuccess) {
				return FALSE;
			}
		}
		m_bEntropyProtected = FALSE;
	}
	return TRUE;
#else
	return TRUE;
#endif
}


BOOL 
CRegistryCfg::SetEntropy(LPBYTE lpbEntropy, DWORD cbEntropy)
{
	if(IsBadReadPtr(lpbEntropy, cbEntropy)) {
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	// memory to encrypt must be a multiple of CRYPTPROTECTMEMORY_BLOCK_SIZE.
	DWORD dwMod = cbEntropy % CRYPTPROTECTMEMORY_BLOCK_SIZE;
	DWORD cbData(0);
	if (0 == dwMod) {
		cbData = cbEntropy + (CRYPTPROTECTMEMORY_BLOCK_SIZE - dwMod);
	} else {
		cbData = cbEntropy;
	}

	LPBYTE lpb = (LPBYTE) ::LocalAlloc(LPTR, cbData);
	if (NULL == lpb) {
		return FALSE;
	}

	::CopyMemory(lpb, lpbEntropy, cbEntropy);

#ifdef USE_CRYPT_PROTECT_MEMORY
	BOOL fSuccess = ::CryptProtectMemory(
		lpb, 
		cbData, 
		CRYPTPROTECTMEMORY_SAME_PROCESS);
	if (!fSuccess) {
		::SecureZeroMemory(lpb, cbData);
		::LocalFree(lpb);
		return FALSE;
	}
#endif

	if (m_Entropy.pbData) {
		::SecureZeroMemory(m_Entropy.pbData, m_Entropy.cbData);
		::LocalFree(m_Entropy.pbData);
	}

	m_bEntropyProtected = TRUE;
	m_Entropy.pbData = lpb;
	m_Entropy.cbData = cbData;

	return TRUE;
}

BOOL 
CRegistryCfg::GetSecureValueEx(
	LPCTSTR szContainer, LPCTSTR szValueName, 
	LPVOID lpValue, DWORD cbBuffer, LPDWORD lpcbUsed)
{
	BOOL fSuccess(FALSE);
	DWORD dwRegValueType;
	DWORD cbSecuredData;

	fSuccess = GetRegValueEx(
		m_hCfgRootKey, 
		szContainer, 
		szValueName, 
		&dwRegValueType,
		NULL, 
		0, 
		&cbSecuredData);

	if (!fSuccess) {
		return FALSE;
	}

	LPBYTE lpbSecuredData = (LPBYTE) ::LocalAlloc(LPTR, cbSecuredData);

	if (NULL == lpbSecuredData) {
		return FALSE;
	}

	// FROM HERE: call LocalFree(lpbSecuredData) when returning

	fSuccess = GetRegValueEx(
		HKEY_LOCAL_MACHINE,
		szContainer,
		szValueName,
		&dwRegValueType,
		lpbSecuredData,
		cbSecuredData,
		&cbSecuredData);

	DATA_BLOB DataSecured;
	DATA_BLOB DataUnsecured;
	DataSecured.cbData = cbSecuredData;
	DataSecured.pbData = lpbSecuredData;

	fSuccess = ProtectEntropy(FALSE);
	if (!fSuccess) {
		::LocalFree(lpbSecuredData);
		return FALSE;
	}
	// FROM HERE: call ProtectEntropy(TRUE) when returning

	fSuccess = ::CryptUnprotectData(
		&DataSecured,
		NULL,
		(m_Entropy.cbData > 0) ? &m_Entropy : NULL,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&DataUnsecured);

	// FROM HERE: call LocalFree(DataUnsecured.pbData) when returning
	
	if (!fSuccess) {
		(VOID) ProtectEntropy(TRUE);
		::LocalFree(lpbSecuredData);
		return FALSE;
	}

	if (NULL != lpcbUsed) {
		*lpcbUsed = DataUnsecured.cbData;
	}

	if (cbBuffer < DataUnsecured.cbData) {
		::SetLastError(ERROR_MORE_DATA);
		::LocalFree(DataUnsecured.pbData);
		(VOID) ProtectEntropy(TRUE);
		::LocalFree(lpbSecuredData);
		return FALSE;
	}

	if (NULL != lpValue) {
		::CopyMemory(lpValue, DataUnsecured.pbData, DataUnsecured.cbData);
	}

	::LocalFree(DataUnsecured.pbData);
	(VOID) ProtectEntropy(TRUE);
	::LocalFree(lpbSecuredData);

	return TRUE;
}

BOOL 
CRegistryCfg::SetSecureValueEx(
	LPCTSTR szContainer, LPCTSTR szValueName, 
	LPCVOID lpValue, DWORD cbValue)
{
	DATA_BLOB DataIn;
	DataIn.cbData = cbValue;
	DataIn.pbData = (LPBYTE) lpValue;
	DATA_BLOB DataOut; // to call LocalFree to free pbData after use

	BOOL fSuccess(FALSE);

	static const WCHAR DESCRIPTION[] = L"";
	// for Windows 2000, DESCRIPTION field cannot null
	// so we are using zero character here.

	fSuccess = ::CryptProtectData(
		&DataIn,
		DESCRIPTION,
		(m_Entropy.cbData > 0) ? &m_Entropy : NULL,
		NULL,
		NULL,
		CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN,
		&DataOut);

	if (!fSuccess) {
		return FALSE;
	}

	// FROM HERE: call LocalFree(DataOut.pbData) when returning

	fSuccess = SetRegValueEx(
		m_hCfgRootKey, 
		szContainer, 
		szValueName, 
		REG_BINARY,
		DataOut.pbData,
		DataOut.cbData);

	if (!fSuccess) {
		::LocalFree(DataOut.pbData);
		return FALSE;
	}

	::LocalFree(DataOut.pbData);
	return TRUE;
}
