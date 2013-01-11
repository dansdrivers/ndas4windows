#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <strsafe.h>
#include <crtdbg.h>

namespace xs {

struct IXSRegistryCfgDefaultValue
{
	virtual BOOL GetDefaultValue(
		LPCTSTR szContainer,
		LPCTSTR szValueName,
		LPVOID lpValue, 
		DWORD cbBuffer, 
		LPDWORD lpcbUsed) = 0;
};

class CXSRegistryCfg
{
	static const MAX_REGKEY_LENGTH = 260 + 1;
	HKEY m_hCfgRootKey;
	TCHAR m_szCfgRegKey[MAX_REGKEY_LENGTH];

	DATA_BLOB m_Entropy;
	BOOL m_bEntropyProtected;

	IXSRegistryCfgDefaultValue* m_pDefaultValueProvider;

protected:

	BOOL ProtectEntropy(BOOL bProtect = TRUE);

	HKEY OpenRegKey(
		HKEY hRootKey,
		LPCTSTR szContainer,
		REGSAM samDesired,
		LPBOOL lpbCreate = NULL);

	BOOL GetRegValueEx(
		HKEY hRootKey,
		LPCTSTR szContainer, 
		LPCTSTR szValueName, LPDWORD lpdwRegValueType,
		LPBYTE lpValue, DWORD cbBuffer, LPDWORD lpcbUsed);

	BOOL SetRegValueEx(
		HKEY hRootKey,
		LPCTSTR szContainer, 
		LPCTSTR szValueName, DWORD dwRegValueType,
		const BYTE* lpValue, DWORD cbValue);

public:

	CXSRegistryCfg(
		LPCTSTR szConfRegKey, 
		IXSRegistryCfgDefaultValue* pDefaultValueProvider = NULL);

	CXSRegistryCfg(
		HKEY hRootKey, 
		LPCTSTR szConfRegKey, 
		IXSRegistryCfgDefaultValue* pDefaultValueProvider = NULL);

	~CXSRegistryCfg();

	VOID SetDefaultValueProvider(IXSRegistryCfgDefaultValue* pProvider);
	VOID SetCfgRegKey(LPCTSTR szCfgRegKey);

	BOOL DeleteContainer(LPCTSTR szContainer, BOOL bDeleteSubs = FALSE);
	BOOL DeleteValue(LPCTSTR szContainerName, LPCTSTR szValueName);

	HKEY OpenRootKey(REGSAM samDesired);

	BOOL GetValue(
		LPCTSTR szValueName,
		LPDWORD lpdwValue)
	{ 
		return GetValueEx(NULL, szValueName, lpdwValue, sizeof(DWORD), NULL); 
	}

	BOOL GetValue(
		LPCTSTR szValueName,
		LPBOOL lpbValue)
	{
		return GetValueEx(NULL, szValueName, lpbValue, sizeof(BOOL), NULL); 
	}

	BOOL GetValue(
		LPCTSTR szValueName, 
		LPTSTR lpValue, DWORD cbBuffer, LPDWORD lpcbUsed = NULL)
	{
		return GetValueEx(NULL, szValueName, lpValue, cbBuffer, lpcbUsed);
	}

	BOOL GetValueEx(
		LPCTSTR szContainerName,
		LPCTSTR szValueName,
		LPDWORD lpdwValue)
	{
		return GetValueEx(szContainerName, szValueName, lpdwValue, sizeof(DWORD), NULL);
	}

	BOOL GetValueEx(
		LPCTSTR szContainerName,
		LPCTSTR szValueName,
		LPBOOL lpbValue)
	{
		return GetValueEx(szContainerName, szValueName, lpbValue, sizeof(DWORD), NULL);
	}

	//
	// actual GetValueEx
	//
	BOOL GetValueEx(
		LPCTSTR szContainer, LPCTSTR szValueName, 
		LPVOID lpValue, DWORD cbBuffer, LPDWORD lpcbUsed = NULL)
	{
		DWORD dwRegValueType;
		BOOL fSuccess = GetRegValueEx(
			m_hCfgRootKey, 
			szContainer, 
			szValueName, 
			&dwRegValueType,
			(LPBYTE) lpValue, 
			cbBuffer, 
			lpcbUsed);
		if (!fSuccess) {
			if (m_pDefaultValueProvider) {
				return m_pDefaultValueProvider->GetDefaultValue(
					szContainer, 
					szValueName, 
					lpValue, 
					cbBuffer, 
					lpcbUsed);
			}
		}
		return fSuccess;
	}

	BOOL SetValue(
		LPCTSTR szValueName,
		DWORD dwValue)
	{
		return SetValueEx(NULL, szValueName, REG_DWORD, &dwValue, sizeof(DWORD));
	}

	BOOL SetValue(
		LPCTSTR szValueName,
		BOOL bValue)
	{
		return SetValueEx(NULL, szValueName, REG_DWORD, &bValue, sizeof(BOOL));
	}

	BOOL SetValue(
		LPCTSTR szValueName, 
		LPCTSTR szValue)
	{
		return SetValueEx(NULL, szValueName, szValue);
	}

	BOOL SetValueEx(
		LPCTSTR szContainer,
		LPCTSTR szValueName,
		BOOL bValue)
	{
		return SetValueEx(szContainer, szValueName, REG_DWORD, &bValue, sizeof(BOOL));
	}

	BOOL SetValueEx(
		LPCTSTR szContainer,
		LPCTSTR szValueName,
		DWORD dwValue)
	{
		return SetValueEx(szContainer, szValueName, REG_DWORD, &dwValue, sizeof(DWORD));
	}

	BOOL SetValueEx(
		LPCTSTR szContainer,
		LPCTSTR szValueName,
		LPCTSTR szValue)
	{
		size_t cch;
		HRESULT hr = ::StringCchLength(szValue, STRSAFE_MAX_CCH, &cch);
		_ASSERT(SUCCEEDED(hr));
		return SetValueEx(szContainer, szValueName, REG_SZ, szValue , cch * sizeof(TCHAR));
	}

	//
	// actual SetValueEx
	//
	BOOL SetValueEx(
		LPCTSTR szContainer, 
		LPCTSTR szValueName, 
		DWORD dwRegValueType,
		LPCVOID lpValue, 
		DWORD cbValue)
	{
		return SetRegValueEx(
			m_hCfgRootKey, 
			szContainer, 
			szValueName, 
			dwRegValueType, 
			(const BYTE*) lpValue, 
			cbValue);
	}

	BOOL GetSecureValueEx(
		LPCTSTR szContainer, 
		LPCTSTR szValueName, 
		LPVOID lpValue, DWORD cbBuffer, LPDWORD lpcbUsed = NULL);

	BOOL SetSecureValueEx(
		LPCTSTR szContainer, 
		LPCTSTR szValueName, 
		LPCVOID lpValue, DWORD cbValue);

	BOOL SetEntropy(LPBYTE lpbEntropy, DWORD cbEntropy);
};

}
