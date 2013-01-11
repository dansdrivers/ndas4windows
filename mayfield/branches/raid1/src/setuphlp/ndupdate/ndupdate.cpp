// ndaspatch.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <wininet.h>  // DeleteUrlCacheEntry, InternetCanonicalizeUrl
#include <urlmon.h> // URLDownloadToCacheFile
#include "ndasmsg.h"
#include "ndupdate.h"
#include "resource.h"

#include "autores.h"
#define XDBG_FILENAME "ndupdate.cpp"
#include "xdebug.h"

static LPCTSTR UPDATE_CLASS_STRING = _T("NDAS");
static LPCTSTR UPDATER_SECTION = _T("UPDATER");
static LPCTSTR UPDATE_SECTION = _T("UPDATE");

////////////////////////////////////////////////////////////////////////////////////////////////
//
// NDAS Patch functions
//
////////////////////////////////////////////////////////////////////////////////////////////////

#define NDAS_UPDATE_REV 1

NDUPDATE_API
DWORD
NdasUpdateGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDUPDATE_API_VERSION_MAJOR, 
		NDUPDATE_API_VERSION_MINOR);
}

NDUPDATE_API
BOOL
WINAPI
NdasUpdateGetUpdateInfo(
	IN IBindStatusCallback* pBSC,
	IN LPCTSTR szBaseURL,
	IN PNDUPDATE_SYSTEM_INFO pSystemInformation,
	OUT PNDUPDATE_UPDATE_INFO pUpdateInformation)
{
	BOOL  fSuccess = FALSE;
	HRESULT hr = E_FAIL;

	DBGPRT_INFO(_FT("BaseURL: %s\n"), szBaseURL);

	TCHAR szUpdateURL[NDUPDATE_MAX_URL];

	if (IsBadReadPtr(pSystemInformation, sizeof(NDUPDATE_SYSTEM_INFO)) ||
		IsBadWritePtr(pUpdateInformation, sizeof(NDUPDATE_UPDATE_INFO)))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	// create URL
	hr = StringCchPrintf(
		szUpdateURL,
		NDUPDATE_MAX_URL,
		_T("%s?REV=%d&PLATFORM=%d&MAJOR=%d&MINOR=%d&BUILD=%d&PRIV=%d&VENDOR=%d&LANGUAGESET=%d&CT=%08X"),
		szBaseURL,
		NDAS_UPDATE_REV,
		pSystemInformation->dwPlatform,
		pSystemInformation->ProductVersion.wMajor,
		pSystemInformation->ProductVersion.wMinor,
		pSystemInformation->ProductVersion.wBuild,
		pSystemInformation->ProductVersion.wPrivate,
		pSystemInformation->dwVendor,
		pSystemInformation->dwLanguageSet,
		GetTickCount());

	if (FAILED(hr)) {
		DBGPRT_ERR_EX(_FT("Making URL failed, too long?, hr=%08X: "), hr);
		return FALSE;
	}

	DBGPRT_INFO(_FT("UpdateURL: %s\n"), szUpdateURL);

	TCHAR szDownloadedFileName[MAX_PATH] = {0};
	hr = ::URLDownloadToCacheFile(
		NULL,
		szUpdateURL,
		szDownloadedFileName,
		sizeof(szDownloadedFileName),
		0,
		pBSC);

	if (FAILED(hr)) {
		DBGPRT_ERR_EX(_FT("Downloading a file failed.\n"));
		::SetLastError(hr);
		return FALSE;
	}

	DBGPRT_INFO(_FT("Downloaded to %s.\n"), szDownloadedFileName);

	TCHAR szINIFileName[MAX_PATH];
	TCHAR szINIString[NDUPDATE_STRING_SIZE] = {0};  
	UINT uiINIValue;

	GetPrivateProfileString(
		UPDATER_SECTION, 
		_T("CLASS"), 
		_T(""), 
		szINIString, 
		NDUPDATE_STRING_SIZE, 
		szDownloadedFileName);

	DBGPRT_INFO(_T("UPDATER CLASS: %s\n"), szINIString);

	if (0 != lstrcmpi(szINIString, UPDATE_CLASS_STRING)) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	uiINIValue = ::GetPrivateProfileInt(
		UPDATER_SECTION, 
		_T("REVISION"), 
		0, 
		szDownloadedFileName);

	DBGPRT_INFO(_T("UPDATER REVISION: %d\n"), uiINIValue);

	if (NDAS_UPDATE_REV != uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_UPDATE_VERSION_FROM_SERVER);
		return FALSE;
	}

	// read patch information
	uiINIValue = ::GetPrivateProfileInt(
		UPDATE_SECTION, 
		_T("NEED_UPDATE"), 
		0xFFFFFFFF,
		szDownloadedFileName);

	DBGPRT_INFO(_T("UPDATE NEED_UPDATE: %d\n"), uiINIValue);

	if (0xFFFFFFFF == uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	//
	// we should return TRUE even if update is not required!
	//
	if (0 == uiINIValue) {
		pUpdateInformation->fNeedUpdate = FALSE;
		return TRUE;
	}

	//
	// Update!
	//

	pUpdateInformation->fNeedUpdate = TRUE;

	uiINIValue = GetPrivateProfileInt(
		UPDATE_SECTION, 
		_T("PRODUCT_VERSION_MAJOR"),
		0xFFFFFFFF, 
		szDownloadedFileName);

	if (0xFFFFFFFF == uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	pUpdateInformation->ProductVersion.wMajor = (WORD) uiINIValue;

	uiINIValue = GetPrivateProfileInt(
		UPDATE_SECTION, 
		_T("PRODUCT_VERSION_MINOR"), 
		0xFFFFFFFF, 
		szDownloadedFileName);

	if (0xFFFFFFFF == uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	pUpdateInformation->ProductVersion.wMinor = (WORD) uiINIValue;

	uiINIValue = GetPrivateProfileInt(
		UPDATE_SECTION, 
		_T("PRODUCT_VERSION_BUILD"), 
		0xFFFFFFFF, 
		szDownloadedFileName);

	if (0xFFFFFFFF == uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	pUpdateInformation->ProductVersion.wBuild = (WORD) uiINIValue;

	uiINIValue = GetPrivateProfileInt(
		UPDATE_SECTION, 
		_T("PRODUCT_VERSION_PRIVATE"), 
		0xFFFFFFFF, 
		szDownloadedFileName);

	if (0xFFFFFFFF == uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	pUpdateInformation->ProductVersion.wPrivate = (WORD) uiINIValue;

	uiINIValue = GetPrivateProfileInt(
		UPDATE_SECTION, 
		_T("FILESIZE"), 
		0xFFFFFFFF, 
		szDownloadedFileName);

	if (0xFFFFFFFF == uiINIValue) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	pUpdateInformation->dwUpdateFileSize = (DWORD) uiINIValue;

	//
	// Update File URL
	//
	pUpdateInformation->szUpdateFileURL[0] = _T('\0');

	(VOID) GetPrivateProfileString(
		UPDATE_SECTION, 
		_T("FILE_URL"), 
		_T(""), 
		pUpdateInformation->szUpdateFileURL, 
		NDUPDATE_MAX_URL, 
		szDownloadedFileName);

	if (_T('\0') == pUpdateInformation->szUpdateFileURL[0]) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	(VOID) GetPrivateProfileString(
		UPDATE_SECTION,
		_T("FILENAME"),
		_T(""),
		pUpdateInformation->szUpdateFileName,
		MAX_PATH,
		szDownloadedFileName);

	if (_T('\0') == pUpdateInformation->szUpdateFileName[0]) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	//
	// Update Reference File URL
	//
	pUpdateInformation->szRefURL[0] = _T('\0');

	GetPrivateProfileString(
		UPDATE_SECTION, 
		_T("REF_URL"), 
		_T(""), 
		pUpdateInformation->szRefURL, 
		NDUPDATE_MAX_URL, 
		szDownloadedFileName);

	if (_T('\0') == pUpdateInformation->szRefURL[0]) {
		SetLastError(NDUPDATE_ERROR_INVALID_SERVER_RESPONSE);
		return FALSE;
	}

	// Failure will retain the downloaded file to check error!
	// Ignore error on delete

	(VOID) DeleteUrlCacheEntry(szDownloadedFileName);

	return TRUE;
}

