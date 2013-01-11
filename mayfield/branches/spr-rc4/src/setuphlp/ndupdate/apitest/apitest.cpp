#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <crtdbg.h>
//
// static link requires to declare XDBG_MAIN_MODULE
// and it is controlled by this module
//
// (dynamic link (DLL) does not requires XDEBUG)
// DLL debug configuration is set from Registry
//

#include <wininet.h>
#include <urlmon.h>

#include "ndupdate.h"
#include "updateui.h"

#define XDBG_MAIN_MODULE
#include "xdebug.h"

int __cdecl _tmain(int, TCHAR**)
{
#if 0
	NDUPDATE_SYSTEM_INFO sysInfo = {0};
	NDUPDATE_UPDATE_INFO updateInfo = {0};

	CDownloadUI downloadUI;
	HMODULE hUpdater = ::LoadLibrary(_T("ndupdate.dll"));
	if (NULL == hUpdater) {
		_tprintf(_T("Unable to load ndupdate.dll\n"));
		return -1;
	}

	downloadUI.Initialize(hUpdater, NULL, _T("NDAS Software Updater"));

	irmProgress status = downloadUI.SetBannerText(_T("NDAS Software Updater"));

	status = downloadUI.SetActionText(_T("Checking Updater Version"));
	if (irmCancel == status) {
		_tprintf(_T("Cancelled by user.\n"));
		return 0;
	}

	DWORD dwAPIVersion = NdasUpdateGetAPIVersion();
	_tprintf(_T("UpdateAPI: %d.%d\n"), 
		HIWORD(dwAPIVersion), 
		LOWORD(dwAPIVersion));

	status = downloadUI.SetActionText(_T("Checking for an update from the server"));
	if (irmCancel == status) {
		_tprintf(_T("Cancelled by user.\n"));
		return 0;
	}

	TCHAR szBaseURL[] = _T("http://updates.ximeta.com/update/");

	sysInfo.dwPlatform = 1;
	sysInfo.ProductVersion.wMajor = 2;
	sysInfo.ProductVersion.wMinor = 4;
	sysInfo.ProductVersion.wBuild = 1000;
	sysInfo.ProductVersion.wPrivate = 0;
	sysInfo.dwVendor = 0;
	sysInfo.dwLanguageSet = 5;

	BOOL fSuccess = ::NdasUpdateGetUpdateInfo(
		&CDownloadBindStatusCallback(&downloadUI),
		szBaseURL, 
		&sysInfo,
		&updateInfo);

	if (!fSuccess) {
		_tprintf(_T("NdasUpdateGetUpdateInfo: Error %u (%08X)\n"), 
			GetLastError(),
			GetLastError());
		return 255;
	}

	if (updateInfo.fNeedUpdate) {
		_tprintf(_T("Update is required.\n"));
		_tprintf(_T("New Version: %d.%d.%d.%d\n"),
			updateInfo.ProductVersion.wMajor,
			updateInfo.ProductVersion.wMinor,
			updateInfo.ProductVersion.wBuild,
			updateInfo.ProductVersion.wPrivate);
		_tprintf(_T("Update URL: %s\n"), updateInfo.szUpdateFileURL);
		_tprintf(_T("Update Size: %d\n"), updateInfo.dwUpdateFileSize);
		_tprintf(_T("Reference URL: %s\n"), updateInfo.szRefURL);
		//fSuccess = NdasUpdateGetUpdateFile(
		//	L"update.exe", 
		//	updateInfo.szUpdateFileURL, 
		//	DownloadCallBack, 
		//	&updateInfo);

		//if (!fSuccess) {
		//	_tprintf(_T("NdasUpdateGetUpdateFile: Error %d\n"), GetLastError());
		//}
	} else {
		_tprintf(_T("Update not required\n"));
		return 0;
	}

	//TCHAR szMyDocuments[MAX_PATH];
	//HRESULT hr = ::SHGetFolderPath(
	//	NULL, 
	//	CSIDL_PERSONAL,
	//	NULL,
	//	SHGFP_TYPE_CURRENT,
	//	szMyDocuments);


	TCHAR szDownloadedFile[MAX_PATH] = {0};

	status = downloadUI.SetActionText(_T("Downloading an update"));
	if (irmCancel == status) {
		_tprintf(_T("Cancelled by user\n"));
		return 0;
	}

	HRESULT hr = ::URLDownloadToCacheFile(
		NULL,
		updateInfo.szUpdateFileURL,
		szDownloadedFile,
		sizeof(szDownloadedFile),
		0,
		&CDownloadBindStatusCallback(&downloadUI));

	
	_tprintf(_T("Updater downloaded to %s\n"), szDownloadedFile);

	status = downloadUI.SetActionText(_T("Download completed."));
	downloadUI.Terminate();


	STARTUPINFO si = {0};
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi = {0};

	::CreateProcess(
		szDownloadedFile,
		NULL,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&si,
		&pi);

	fSuccess = DeleteUrlCacheEntry(updateInfo.szUpdateFileURL);
	if (!fSuccess) {
		_tprintf(_T("Failed to delete cache entry: %s with error %d"),
			updateInfo.szUpdateFileURL,
			::GetLastError());
	}
#endif
	return 0;
}
