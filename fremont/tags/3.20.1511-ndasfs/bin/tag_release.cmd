#include <windows.h>
#include <tchar.h>
#include <msiquery.h>
#include <strsafe.h>
#include <crtdbg.h>
#include "autores.h"
#include "msisup.h"

LPTSTR
pMsiGetProperty(
    MSIHANDLE hInstall, 
    LPCTSTR szPropertyName,
    LPDWORD pcch)
{
	HANDLE hHeap = ::GetProcessHeap();
	AutoProcessHeapPtr<LPTSTR> pszValue;
    DWORD cchValue = 0;
    
    UINT msiret = MsiGetProperty(hInstall, szPropertyName, _T(""), &cchValue);
    
    if (ERROR_SUCCESS != msiret && ERROR_MORE_DATA != msiret)
    {
        return NULL;
    }
    
    if (ERROR_MORE_DATA == msiret)
    {
        ++cchValue;
        pszValue = (LPTSTR) HeapAlloc(GetProcessHeap(), 0, cchValue * sizeof(TCHAR));
        if (NULL == (LPTSTR) pszValue)
        {
            return NULL;
        }
        msiret = MsiGetProperty(hInstall, szPropertyName, pszValue, &cchValue);
        if (ERROR_SUCCESS != msiret)
        {
            return NULL;
        }
    }

    if (NULL != pcch)
    {
        *pcch = cchValue;
    }
    
    return pszValue.Detach();
}

LPTSTR
pMsiGetSourcePath(
    MSIHANDLE hInstall, 
    LPCTSTR szFolder,
    LPDWORD pcch)
{
    AutoProcessHeapPtr<LPTSTR> pszValue;
    DWORD cchValue = 0;

    