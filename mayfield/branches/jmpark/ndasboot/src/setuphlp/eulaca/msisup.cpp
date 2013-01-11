#include <windows.h>
#include <tchar.h>
#include <msiquery.h>
#include <strsafe.h>
#include <crtdbg.h>
#include <xtl/xtlautores.h>
#include "msisup.h"

UINT
pMsiLogMessage(
	MSIHANDLE hInstall,
	LPCTSTR Format,
	...)
{
	TCHAR buffer[512];
	PMSIHANDLE hRecord = MsiCreateRecord(2);
												
	if (NULL == hRecord)
	{
		_RPT1(_CRT_ERROR, "MsiCreateRecord(2) failed, error=0x%X\n", GetLastError());
		return ERROR_OUTOFMEMORY;
	}

	UINT ret = MsiRecordSetString(hRecord, 0, _T("[1]"));
	_ASSERT(ERROR_SUCCESS == ret);
	if (ret != ERROR_SUCCESS)
	{
		_RPT1(_CRT_ERROR, "MsiRecordSetString(0) failed, error=0x%X\n", ret);
		return ret;
	}

	va_list ap;
	va_start(ap, Format);

	HRESULT hr = StringCchVPrintf(
		buffer,
		RTL_NUMBER_OF(buffer),
		Format,
		ap);

	_ASSERT(SUCCEEDED(hr)); hr;

	va_end(ap);

	ret = MsiRecordSetString(hRecord, 1, buffer);
	if (ret != ERROR_SUCCESS)
	{
		_RPT1(_CRT_ERROR, "MsiRecordSetString(1) failed, error=0x%X\n", ret);
		return ret;
	}

	ret = MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);
	if (ret != ERROR_SUCCESS)
	{
		_RPT1(_CRT_ERROR, "MsiProcessMessage failed, error=0x%X\n", ret);
		return ret;
	}

	return ERROR_SUCCESS;
}

UINT
pMsiGetProperty(
    MSIHANDLE hInstall, 
    LPCTSTR PropertyName,
	LPTSTR* Value,
    LPDWORD ValueLength)
{
	XTL::AutoProcessHeapPtr<TCHAR> buffer;

	*Value = NULL;
    if (NULL != ValueLength) *ValueLength = 0;

    DWORD length = 0;
    UINT ret = MsiGetProperty(hInstall, PropertyName, _T(""), &length);
    
    if (ERROR_SUCCESS != ret && ERROR_MORE_DATA != ret)
    {
		return ret;
    }
    
    if (ERROR_MORE_DATA == ret)
    {
		++length; // plus NULL

		buffer = static_cast<LPTSTR>(
			HeapAlloc(GetProcessHeap(), 0, length * sizeof(TCHAR)));

        if (NULL == static_cast<LPTSTR>(buffer))
        {
			return ERROR_OUTOFMEMORY;
        }

        ret = MsiGetProperty(hInstall, PropertyName, buffer, &length);
        if (ERROR_SUCCESS != ret)
        {
			return ret;
        }
    }

    if (NULL != ValueLength) *ValueLength = length;
	*Value = buffer.Detach();

    return ERROR_SUCCESS;
}

LPTSTR
pMsiGetSourcePath(
    MSIHANDLE hInstall, 
    LPCTSTR szFolder,
    LPDWORD pcch)
{
	XTL::AutoProcessHeapPtr<TCHAR> pszValue;
    DWORD cchValue = 0;

    if (NULL != pcch) *pcch = 0;

    UINT msiret = MsiGetSourcePath(hInstall, szFolder, _T(""), &cchValue);

    if (ERROR_MORE_DATA == msiret)
    {
        ++cchValue;
        pszValue = (LPTSTR) HeapAlloc(
            GetProcessHeap(), 
            0, 
            cchValue * sizeof(TCHAR));

        if (NULL == (LPTSTR) pszValue)
        {
            return NULL;
        }

        msiret = MsiGetSourcePath(hInstall, szFolder, pszValue, &cchValue);
    }

    if (ERROR_SUCCESS != msiret)
    {
        return NULL;
    }

    if (NULL != pcch) *pcch = cchValue;

    return pszValue.Detach();
}
