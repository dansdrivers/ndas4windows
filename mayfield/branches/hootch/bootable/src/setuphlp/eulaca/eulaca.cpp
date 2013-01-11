/*++

  Copyright (C) 2005-2006 XIMETA, Inc. All rights reserved.

  Author: Chesong Lee <cslee@ximeta.com>

  --*/
#include <windows.h>
#include <tchar.h>
#include <msiquery.h>
#include <strsafe.h>
#include <crtdbg.h>
#include <xtl/xtlautores.h>
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
pReadEulaFromFile(
	MSIHANDLE hInstall);

MSICA_IMP
ReplaceEula(MSIHANDLE hInstall)
{
	// ::DebugBreak();

	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	
	//
	// EULA File Content is MBCS (RTF) not Unicode
	//
	XTL::AutoProcessHeapPtr<CHAR> lpEula = pReadEulaFromFile(hInstall);
	if (NULL == (LPSTR) lpEula)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: ReadEulaFromFile failed, error=0x%X"), 
			GetLastError());

		return ERROR_INSTALL_FAILURE;
	}

	//
	// Replacing EULA text control in the database
	//
	PMSIHANDLE hDatabase = MsiGetActiveDatabase(hInstall);

	if (0 == hDatabase)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiGetActiveDatabase failed, error=0x%X"), 
			GetLastError());

		return ERROR_INSTALL_FAILURE;
	}

	PMSIHANDLE hView;
	LPCTSTR query = _T("SELECT * FROM `Control` ")
		_T(" WHERE `Dialog_` = 'LicenseAgreement' AND `Control` = 'Memo' ");
	UINT ret = MsiDatabaseOpenView(hDatabase, query, &hView);

	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiDatabaseOpenView failed, error=0x%X"), 
			ret);

		return ERROR_INSTALL_FAILURE;
	}

	ret = MsiViewExecute(hView, 0);

	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiViewExecute failed, error=0x%X"), 
			ret);

		return ERROR_INSTALL_FAILURE;
	}

	PMSIHANDLE hRecord;
	ret = MsiViewFetch(hView, &hRecord);

	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiViewFetch failed, error=0x%X"), 
			ret);

		return ERROR_INSTALL_FAILURE;
	}

	ret = MsiViewModify(hView, MSIMODIFY_DELETE, hRecord);

	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiViewModify failed, error=0x%X"), 
			ret);

		return ERROR_INSTALL_FAILURE;
	}

	//
	// 10th field is the Text column
	//
	// Dialog_, Control, Type, X, Y, 
	// Width, Height, Attributes, Property, Text
	// Control_Next, Help
	//
	ret = MsiRecordSetStringA(hRecord, 10, lpEula);
	
	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiRecordSetString failed, error=0x%X"), 
			ret);

		return ERROR_INSTALL_FAILURE;
	}

	//
	// Commit the changes temporarily
	//
	ret = MsiViewModify(hView, MSIMODIFY_INSERT_TEMPORARY, hRecord);
	
	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiViewModify failed, error=0x%X"), 
			ret);

		return ERROR_INSTALL_FAILURE;
	}

	pMsiLogMessage(
		hInstall,
		_T("EULACA: EULA is replaced successfully."));

	return ERROR_SUCCESS;
}

LPSTR
pReadTextFromFile(
	LPCTSTR szFileName)
{
	XTL::AutoFileHandle hFile = CreateFile(
		szFileName, 
		GENERIC_READ, 
		FILE_SHARE_READ, 
		NULL, 
		OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);
	
	if (INVALID_HANDLE_VALUE == static_cast<HANDLE>(hFile))
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

	XTL::AutoProcessHeapPtr<CHAR> lpBuffer = (LPSTR) 
		::HeapAlloc(
			GetProcessHeap(), 
			HEAP_ZERO_MEMORY, 
			cbToRead);

	if (NULL == static_cast<LPSTR>(lpBuffer))
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

LPSTR
pReadEulaFromFileEx(
	MSIHANDLE hInstall,
	LPCTSTR SourceDir,
	LPCTSTR PropertyName)
{
	TCHAR fullPath[MAX_PATH];

	XTL::AutoProcessHeapPtr<TCHAR> eulaFileName;

	UINT ret = pMsiGetProperty(hInstall, PropertyName, &eulaFileName, NULL);

	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiGetProperty(%s) failed, error=0x%X"),
			PropertyName,
			ret);

		return NULL;
	}

	StringCchCopy(fullPath, MAX_PATH, SourceDir);
	StringCchCat(fullPath, MAX_PATH, eulaFileName);

	pMsiLogMessage(
		hInstall, 
		_T("EULACA: EulaFile=%s"),
		fullPath);

	//
	// RTF file is an ANSI text file not unicode.
	// RTF has the CodePage tag inside, so we don't have to 
	// worry about displaying non-English text.
	// 

	XTL::AutoProcessHeapPtr<CHAR> eulaText = pReadTextFromFile(fullPath);

	if (NULL == (LPSTR) eulaText)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: ReadTextFromFile(%s) failed, error=0x%X"),
			fullPath,
			GetLastError());

		return NULL;
	}

	pMsiLogMessage(
		hInstall, 
		_T("EULACA: Read EULA text from %s"),
		fullPath);

	return eulaText.Detach();
}

LPSTR
pReadEulaFromFile(
	MSIHANDLE hInstall)
{
	XTL::AutoProcessHeapPtr<TCHAR> sourceDir;

	UINT ret = pMsiGetProperty(hInstall, _T("SourceDir"), &sourceDir, NULL);
	
	if (ERROR_SUCCESS != ret)
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: MsiGetProperty(SourceDir) failed, error=0x%X"),
			ret);

		return NULL;
	}

	//
	// EULA from the property EulaFileName
	//

	XTL::AutoProcessHeapPtr<CHAR> eulaText = 
		pReadEulaFromFileEx(hInstall, sourceDir, _T("EulaFileName"));

	if (NULL == static_cast<LPSTR>(eulaText))
	{
		// 
		// try again with EulaFallbackFileName
		//
		eulaText = pReadEulaFromFileEx(
			hInstall, sourceDir, _T("EulaFallbackFileName"));

		if (NULL == static_cast<LPSTR>(eulaText))
		{
			//
			// Once more with EulaFallbackFileName2
			//
			eulaText = pReadEulaFromFileEx(
				hInstall, sourceDir, _T("EulaFallbackFileName2"));
		}
	}

	//
	// If eulaText is still NULL, every attempt has been failed.
	//
	if (NULL == static_cast<LPSTR>(eulaText))
	{
		pMsiLogMessage(
			hInstall, 
			_T("EULACA: EULA files are not available!"));

		return NULL;
	}

	//
	// Now we have the EULA Text!
	//
	return eulaText.Detach();
}

