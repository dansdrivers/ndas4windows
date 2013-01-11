#include <windows.h>
#include <tchar.h>
#include <msiquery.h>
#include <strsafe.h>
#include <crtdbg.h>
#include "autores.h"
#include "msisup.h"

#pragma comment(lib, "msi.lib")

#ifdef __cplusplus
#define MSICA_EXTERN extern "C"
#else
#define MSICA_EXTERN
#endif

#define MSICA_DECL MSICA_EXTERN /* __declspec(dllexport) */
#define MSICA_CALL __stdcall
#define MSICA_IMP MSICA_DECL UINT MSICA_CALL

LPSTR
pReadEulaFromFileA(
    MSIHANDLE hInstall);

LPSTR
pReadTextFromFileA(
    LPCTSTR szFileName);

MSICA_IMP
ReplaceEula(MSIHANDLE hInstall)
{
	// ::DebugBreak();

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
    
    //
    // EULA File Content is MBCS (RTF) not Unicode
    AutoProcessHeapPtr<LPSTR> lpEula = pReadEulaFromFileA(hInstall);
    if (NULL == (LPSTR) lpEula)
    {
        _RPT0(_CRT_ERROR, "pReadEulaFromFile failed\n");
        return ERROR_INSTALL_FAILURE;
    }
    
    _RPT0(_CRT_WARN, "ReplaceEula\n");
    
	PMSIHANDLE hDatabase = MsiGetActiveDatabase(hInstall);

	if (0 == hDatabase)
	{
        _RPT0(_CRT_ERROR, "MsiGetActiveDatabase failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	PMSIHANDLE hView;
	LPCTSTR query = _T("SELECT * FROM `Control`")
		_T(" WHERE `Dialog_` = 'LicenseAgreement' AND `Control` = 'Memo'");
	UINT msiret = MsiDatabaseOpenView(hDatabase, query, &hView);

	if (ERROR_SUCCESS != msiret)
	{
        _RPT0(_CRT_ERROR, "MsiDatabaseOpenView failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	msiret = MsiViewExecute(hView, 0);

	if (ERROR_SUCCESS != msiret)
	{
        _RPT0(_CRT_ERROR, "MsiViewExecute failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	PMSIHANDLE hRecord;
	msiret = MsiViewFetch(hView, &hRecord);

	if (ERROR_SUCCESS != msiret)
	{
        _RPT0(_CRT_ERROR, "MsiViewFetch failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	msiret = MsiViewModify(hView, MSIMODIFY_DELETE, hRecord);

	if (ERROR_SUCCESS != msiret)
	{
        _RPT0(_CRT_ERROR, "MsiViewModify failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	UINT nFields = MsiRecordGetFieldCount(hRecord);

	AutoProcessHeapPtr<LPTSTR> buffer;
	DWORD chBuffer = 0;
	msiret = MsiRecordGetString(hRecord, 10, _T(""), &chBuffer);

	if (ERROR_MORE_DATA == msiret)
	{
		++chBuffer;
		buffer = (LPTSTR) ::HeapAlloc(
            GetProcessHeap(), 
            HEAP_ZERO_MEMORY, 
            chBuffer * sizeof(TCHAR));
		msiret = MsiRecordGetString(hRecord, 1, buffer, &chBuffer);
	}
    
	msiret = MsiRecordSetStringA(hRecord, 10, lpEula);
    
	if (ERROR_SUCCESS != msiret)
	{
        _RPT0(_CRT_ERROR, "MsiRecordSetString failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	msiret = MsiViewModify(hView, MSIMODIFY_INSERT_TEMPORARY, hRecord);
    
	if (ERROR_SUCCESS != msiret)
	{
        _RPT0(_CRT_ERROR, "MsiViewModify failed\n");
		return ERROR_INSTALL_FAILURE;
	}

	//msiret = MsiViewExecute(hView, hRecord);
	//if (ERROR_SUCCESS != msiret)
	//{
	//	return ERROR_INSTALL_FAILURE;
	//}

	_RPT0(_CRT_WARN, "ReplaceEula completed successfully.\n");

	return ERROR_SUCCESS;
}

LPSTR
pReadEulaFromFileA(
    MSIHANDLE hInstall)
{
    DWORD cchEulaFileName;
    AutoProcessHeapPtr<LPTSTR> pszEulaFileName = 
        pMsiGetProperty(hInstall, _T("EulaFileName"), &cchEulaFileName);
    
    if (NULL == (LPTSTR) pszEulaFileName)
    {
        _RPT0(_CRT_ERROR, "MsiGetProperty(EulaFileName) failed\n");
        return NULL;
    }

    _RPT1(_CRT_WARN, "MsiGetProperty(EulaFileName=%ws)\n", pszEulaFileName);
    
    DWORD cchEulaSourceDir;
    AutoProcessHeapPtr<LPTSTR> pszEulaSourceDir = 
        pMsiGetProperty(hInstall, _T("EulaSourceDir"), &cchEulaSourceDir);
    
    if (NULL == (LPTSTR) cchEulaSourceDir)
    {
        _RPT0(_CRT_ERROR, "MsiGetProperty(EulaSourceDir) failed\n");
        return NULL;
    }

    _RPT1(_CRT_WARN, "MsiGetProperty(EulaSourceDir=%ws)\n", pszEulaSourceDir);

    DWORD cchEulaFilePath = cchEulaFileName + cchEulaSourceDir + 1;

    AutoProcessHeapPtr<LPTSTR> pszEulaFilePath = 
        (LPTSTR) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cchEulaFilePath * sizeof(TCHAR));
    
    if (NULL == (LPTSTR) pszEulaFilePath)
    {
        _RPT0(_CRT_ERROR, "MsiGetProperty(EulaSourceDir) failed\n");
        return NULL;
    }
    
    HRESULT hr = StringCchCopy(pszEulaFilePath, cchEulaFilePath, pszEulaSourceDir);

    if (FAILED(hr))
    {
        _RPT0(_CRT_ERROR, "StringCchCopy(pszEulaFilePath) failed\n");
        return NULL;
    }
    
    hr = StringCchCat(pszEulaFilePath, cchEulaFilePath, pszEulaFileName);
    
    if (FAILED(hr))
    {
        _RPT0(_CRT_ERROR, "StringCchCat(pszEulaFilePath) failed\n");
        return NULL;
    }

    _RPT1(_CRT_WARN, "MsiGetProperty(EulaFilePath=%ws)\n", pszEulaFilePath);

	AutoProcessHeapPtr<LPSTR> lpEula = pReadTextFromFileA(pszEulaFilePath);

    if (NULL == (LPSTR) lpEula)
    {
        _RPT0(_CRT_ERROR, "pReadTextFromFile(pszEulaFilePath) failed\n");
        return NULL;
    }
    
    return lpEula.Detach();
}

LPSTR
pReadTextFromFileA(
    LPCTSTR szFileName)
{
    AutoFileHandle hFile = CreateFile(
        szFileName, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    
    if (INVALID_HANDLE_VALUE == (HANDLE) hFile)
    {
        return NULL;
    }
    
    LARGE_INTEGER fileSize;
    BOOL fSuccess = GetFileSizeEx(hFile, &fileSize);

    if (!fSuccess)
    {
        return NULL;
    }
    
    DWORD cbToRead = fileSize.LowPart;

    AutoProcessHeapPtr<LPSTR> lpBuffer = (LPSTR) ::HeapAlloc(
        GetProcessHeap(), 
        HEAP_ZERO_MEMORY, 
        cbToRead);

    if (NULL == (LPSTR) lpBuffer)
    {
        return NULL;
    }

    DWORD cbRead;

    fSuccess = ReadFile(
        hFile,
        lpBuffer,
        cbToRead,
        &cbRead,
        NULL);

    if (!fSuccess)
    {
        return NULL;
    }        
    
    return lpBuffer.Detach();
}
