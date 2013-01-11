#include "stdafx.h"
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <crtdbg.h>

namespace w32sys {

	UINT WINAPI GetShellIconSize(UINT uiDefault = ::GetSystemMetrics(SM_CXICON))
	{
		HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;

		LONG lResult = ::RegOpenKeyEx(
			HKEY_CURRENT_USER,
			_T("Control Panel\\Desktop\\WindowMetrics"),
			0,
			KEY_READ,
			&hKey);

		if (ERROR_SUCCESS != lResult) {
			return uiDefault;
		}

		TCHAR szData[30] = {0};
		DWORD cbData = sizeof(szData);
		DWORD regType;

		lResult = ::RegQueryValueEx(
			hKey, 
			_T("Shell Icon Size"), 
			NULL,
			&regType,
			(BYTE*) szData,
			&cbData);

		if (ERROR_SUCCESS != lResult) {
			::RegCloseKey(hKey);
			return uiDefault;
		}

		UINT uiValue = uiDefault;
		BOOL fSuccess = StrToIntEx(szData, STIF_DEFAULT, (int*)&uiValue);
		if (!fSuccess) {
			::RegCloseKey(hKey);
			return uiDefault;
		}

		::RegCloseKey(hKey);
		return uiValue;
	}

	BOOL WINAPI SetShellIconSize(UINT uiSize)
	{
		HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;

		LONG lResult = ::RegOpenKeyEx(
			HKEY_CURRENT_USER,
			_T("Control Panel\\Desktop\\WindowMetrics"),
			0,
			KEY_WRITE,
			&hKey);

		if (ERROR_SUCCESS != lResult) {
			return FALSE;
		}

		TCHAR szData[30] = {0};
		size_t cbData = sizeof(szData);
		size_t cbRemaining = cbData;

		HRESULT hr = ::StringCbPrintfEx(
			szData, 
			cbData, 
			NULL,
			&cbRemaining,
			STRSAFE_IGNORE_NULLS,
			_T("%d"), 
			uiSize);

		_ASSERTE(SUCCEEDED(hr));

		cbData -= cbRemaining - sizeof(TCHAR); // should include NULL

		lResult = ::RegSetValueEx(
			hKey, 
			_T("Shell Icon Size"), 
			0,
			REG_SZ,
			(CONST BYTE*)szData,
			cbData);

		if (ERROR_SUCCESS != lResult) {
			::RegCloseKey(hKey);
			return FALSE;
		}

		::RegCloseKey(hKey);

		::SendMessage(
			HWND_BROADCAST, 
			WM_SETTINGCHANGE, 
			SPI_SETNONCLIENTMETRICS, 
			0L);

		return TRUE;
	}

	VOID WINAPI RefreshShellIconCache()
	{
		UINT uiSize = GetShellIconSize();
		SetShellIconSize(uiSize - 1);
		SetShellIconSize(uiSize);

		// 
		// Buggy COMCTL32 requires additional process.
		// But we do not handle it right now!
		// 
	}
}

#ifdef _TEST_MAIN_

void __cdecl _tmain()
{
	w32sys::RefreshShellIconCache();
}

#endif
