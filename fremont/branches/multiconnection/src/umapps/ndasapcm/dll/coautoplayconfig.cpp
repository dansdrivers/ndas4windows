#include "stdatl.hpp"
#include "resource.h"
#include "autoplayconfig.h"
#include "coautoplayconfig.hpp"

static LPCTSTR PolicyKeyPath = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer");
static LPCTSTR PolicyValueName = _T("NoDriveTypeAutoRun");
static const DWORD NoDriveTypeAutoRunDefault = 0x95;

STDMETHODIMP
CAutoPlayConfig::SetNoDriveTypeAutoRun(
	__in ULONG_PTR RootKey, 
	__in DWORD Mask, 
	__in DWORD Value)
{
	HKEY keyHandle = (HKEY) INVALID_HANDLE_VALUE;
	DWORD disp = 0;
	LONG result = RegCreateKeyEx(
		reinterpret_cast<HKEY>(RootKey),
		PolicyKeyPath,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE | KEY_SET_VALUE,
		NULL,
		&keyHandle,
		&disp);

	if (ERROR_SUCCESS != result)
	{
		return HRESULT_FROM_WIN32(result);
	}

	DWORD newValue = 0;
	DWORD newValueSize = sizeof(DWORD);
	result = RegQueryValueEx(
		keyHandle,
		PolicyValueName,
		0,
		NULL,
		reinterpret_cast<LPBYTE>(&newValue),
		&newValueSize);

	if (ERROR_SUCCESS != result)
	{
		newValue = NoDriveTypeAutoRunDefault;
	}

	newValue &= ~Mask;
	newValue |= (Value & Mask);

	result = RegSetValueEx(
		keyHandle,
		PolicyValueName,
		0,
		REG_DWORD,
		reinterpret_cast<CONST BYTE*>(&newValue),
		sizeof(DWORD));

	if (ERROR_SUCCESS != result)
	{
		ATLVERIFY(ERROR_SUCCESS == RegCloseKey(keyHandle));
		return HRESULT_FROM_WIN32(result);
	}

	ATLVERIFY(ERROR_SUCCESS == RegCloseKey(keyHandle));

	return S_OK;
}

STDMETHODIMP
CAutoPlayConfig::GetNoDriveTypeAutoRun(
	__in ULONG_PTR RootKey, 
	__out DWORD* Value)
{
	if (NULL == Value)
	{
		return E_POINTER;
	}

	*Value = 0;

	HKEY keyHandle = (HKEY) INVALID_HANDLE_VALUE;
	LONG result = RegOpenKeyEx(
		reinterpret_cast<HKEY>(RootKey),
		PolicyKeyPath,
		0,
		KEY_QUERY_VALUE,
		&keyHandle);

	if (ERROR_SUCCESS != result)
	{
		if (ERROR_FILE_NOT_FOUND == result)
		{
			//
			// If the registry key does not exist,
			// we returns the default value
			//
			*Value = NoDriveTypeAutoRunDefault;
			return S_OK;
		}
		return HRESULT_FROM_WIN32(result);
	}

	DWORD value = 0;
	DWORD valueSize = sizeof(DWORD);
	result = RegQueryValueEx(
		keyHandle,
		PolicyValueName,
		NULL,
		NULL,
		reinterpret_cast<LPBYTE>(&value),
		&valueSize);

	if (ERROR_SUCCESS != result)
	{
		ATLVERIFY(ERROR_SUCCESS == RegCloseKey(keyHandle));
		return HRESULT_FROM_WIN32(result);
	}

	*Value = value;

	ATLVERIFY(ERROR_SUCCESS == RegCloseKey(keyHandle));

	return S_OK;
}

