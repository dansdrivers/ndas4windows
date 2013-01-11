 /*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"


#include <socketlpx.h>

#include "ndasdevid.h"
#include "ndasdev.h"
#include "ndasdevreg.h"
#include "ndaslogdev.h"
#include "ndaslogdevman.h"
#include "ndassvc.h"
#include "ndasinstman.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include "task.h"
#include "svchelp.h"
#include "observer.h"
#include "ndascfg.h"

#include <regstr.h>

// Control GUID for XDebugEventTrace
// {a4f3cd16-e134-4d13-bafe-3e22abd4fcd8}
static const GUID XDebugEventTraceControlGuid =
{ 0xa4f3cd16, 0xe134, 0x4d13, { 0xba, 0xfe, 0x3e, 0x22, 0xab, 0xd4, 0xfc, 0xd8 } };

// {2560cabf-47ec-4c43-a021-0f2b927025e6}
static const GUID XDebugEventTraceTransactionGuid =
{ 0x2560cabf, 0x47ec, 0x4c43, { 0xa0, 0x21, 0xf, 0x2b, 0x92, 0x70, 0x25, 0xe6 } };

#define XDBG_MAIN_MODULE
#include "xdbgflags.h"
#define XDEBUG_APPNAME TEXT("ndassvc")
#define XDBG_MODULE_FLAG XDF_MAIN
#include "xdebug.h"

static VOID XDbgShowEnabledFlags();
static DWORD WINAPI XDbgCfgMonProc(LPVOID lParam);

int __cdecl wmain(DWORD argc, LPWSTR *argv)
{
	BOOL fSuccess(FALSE);
	WSADATA wsaData;
	INT iResult = WSAStartup(MAKEWORD(2,2), &wsaData);

	//
	// Service Debugging support
	//
	BOOL bDebugBreak = 0;
	fSuccess = _NdasServiceCfg.GetValue(_T("InitialBreak"), &bDebugBreak);
	if (fSuccess && bDebugBreak) {
		DebugBreak();
	}

	static const DWORD MAX_STARTUP_DELAY = 30 * 1000;
	DWORD dwStartupDelay = 0;
	fSuccess = _NdasServiceCfg.GetValue(_T("StartupDelay"), &dwStartupDelay);
	if (fSuccess) {
		::Sleep((dwStartupDelay < MAX_STARTUP_DELAY) ? 
			dwStartupDelay : MAX_STARTUP_DELAY);
	}

	DBGPRT_INFO(_FT("WSAStartup(2,2) returns %d.\n"), iResult);
	if (0 != iResult) {
		DBGPRT_ERR_EX(_FT("WSAStartup failed : "));
		return -1;
	}

	if (!XDbgInit(_T("ndassvc"))) {
		OutputDebugString(_T("XDBGInit failed.\n"));
	}

	if (!XDbgLoadSettingsFromRegistry(_T("Software\\NDAS\\NDASSVC"))) {
		OutputDebugString(_T("XDBGLSFR failed.\n"));
	}

	ximeta::CService* pNdasService;
	pNdasService = CNdasService::Instance();

	if (NULL == pNdasService) {
		DBGPRT_ERR(_FT("Out of memory\n"));
		// out of memory
		return -1;
	}

	if ( (argc > 1) &&
		((*argv[1] == '-') || (*argv[1] == '/')) )
	{
		if ( lstrcmpi( _T("install"), argv[1]+1 ) == 0 )
		{
			TCHAR szBinaryPath[_MAX_PATH];

			// Retrieve the current process image path
			GetModuleFileName(NULL, szBinaryPath, _MAX_PATH);

			// Install the service with current image path
			fSuccess = CNdasServiceInstaller::Install(szBinaryPath);

			if (!fSuccess) {
				_tprintf(
					_T("Service install failure (%s (%s)) - Error %d\n"), 
					CNdasServiceInstaller::SERVICE_NAME,
					CNdasServiceInstaller::DISPLAY_NAME,
					::GetLastError());
			} else {
				_tprintf(
					_T("%s (%s) service installed.\n"),
					CNdasServiceInstaller::DISPLAY_NAME,
					CNdasServiceInstaller::SERVICE_NAME);
			}
		}
		else if ( lstrcmpi(_T("remove"), argv[1]+1 ) == 0 ||
			lstrcmpi(_T("uninstall"), argv[1]+1) == 0)
		{
			fSuccess = CNdasServiceInstaller::Remove();

			if (!fSuccess) {
				_tprintf(
					_T("Service removal failure (%s (%s)) - Error %d\n"),
					CNdasServiceInstaller::SERVICE_NAME,
					CNdasServiceInstaller::DISPLAY_NAME,
					::GetLastError());
			} else {
				_tprintf(
					_T("%s (%s) service removed successfully.\n"),
					CNdasServiceInstaller::DISPLAY_NAME,
					CNdasServiceInstaller::SERVICE_NAME);
			}

		}
		else if ( lstrcmpi( _T("debug"), argv[1]+1 ) == 0 )
		{
			--argc;
			++argv;

			HANDLE hStopEvent = NULL;
			HANDLE hThread = NULL;

			if (_PXDebug != NULL) {

				hStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

				if (NULL != hStopEvent) {
					hThread = ::CreateThread(
						NULL,
						0,
						XDbgCfgMonProc,
						hStopEvent,
						0,
						NULL);
				}
			}
				
			pNdasService->ServiceDebug(argc, argv);

			if (NULL != hStopEvent) {
				::SetEvent(hStopEvent);
				if (NULL != hThread) {
					::WaitForSingleObject(hThread, 100000);
				}
				::CloseHandle(hStopEvent);
			}

		}
		else
		{
			goto dispatch;
		}
		delete pNdasService;
//		fSuccess = xdEventTracer.Unregister();

		XDbgCleanup();
		return 0;

	}

	// if it doesn't match any of the above parameters
	// the service control manager may be starting the service
	// so we must call StartServiceCtrlDispatcher
dispatch:
	// this is just to be friendly
	_tprintf( _T("%s -install          to install the service\n"), argv[0]);
	_tprintf( _T("%s -remove           to remove the service\n"), argv[0]);
	_tprintf( _T("%s -debug <params>   to run as a console app for debugging\n"), argv[0]);
	_tprintf( _T("\nStartServiceCtrlDispatcher being called.\n"));
	_tprintf( _T("This may take several seconds.  Please wait.\n"));

	SERVICE_TABLE_ENTRY dispatchTable[2];
	dispatchTable[0] = *(pNdasService->GetDispatchTableEntry());
	dispatchTable[1].lpServiceName = NULL;
	dispatchTable[1].lpServiceProc = NULL;

	fSuccess = StartServiceCtrlDispatcher(dispatchTable);

	if (!fSuccess) {
		_tprintf(
			_T("StartServiceCtrlDispatcher failed - Error %d\n"), 
			::GetLastError());
	}

	delete pNdasService;

//	fSuccess = xdEventTracer.Unregister();
//	_ASSERT(fSuccess);

	WSACleanup();
	XDbgCleanup();

	return 0;
}


static DWORD MonitorSubKeyCreation(
	HKEY hRootKey,
	LPCTSTR szRegKeyPath,
	LPCTSTR szRegSubKeyName,
	HANDLE hStopEvent)
{
	HKEY hKey;

	LONG lResult = ::RegOpenKeyEx(
		hRootKey,
		szRegKeyPath,
		0,
		KEY_NOTIFY,
		&hKey);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	HANDLE hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == hEvent) {
		::RegCloseKey(hKey);
		return ::GetLastError();
	}

	DWORD dwRet = ERROR_SUCCESS;

	while (TRUE) {

		lResult = ::RegNotifyChangeKeyValue(
			hKey,
			TRUE,
			REG_NOTIFY_CHANGE_NAME,
			hEvent,
			TRUE);

		if (ERROR_SUCCESS != lResult) {
			dwRet = lResult;
			break;
		}

		HANDLE hWaitEvents[2] = {hStopEvent, hEvent};

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			2, 
			hWaitEvents, 
			FALSE, 
			INFINITE);

		if (WAIT_OBJECT_0 == dwWaitResult) {
		
			dwRet = 0xFFFFFF;
			break;

		} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {

			HKEY hSubKey;
			lResult = ::RegOpenKeyEx(
				hKey,
				szRegSubKeyName,
				0,
				KEY_NOTIFY,
				&hSubKey);

			if (ERROR_SUCCESS == lResult) {
				dwRet = ERROR_SUCCESS;
				::RegCloseKey(hSubKey);
				break;
			}

			BOOL fSuccess = ::ResetEvent(hEvent);
			_ASSERTE(fSuccess);

		} else {

			dwRet = ::GetLastError();
			break;
		}
	}

	::RegCloseKey(hKey);
	::CloseHandle(hEvent);

	return dwRet;
}

static DWORD WINAPI XDbgCfgMonProc(LPVOID lParam)
{
	HANDLE hStopEvent = reinterpret_cast<HANDLE>(lParam);
	HKEY hKey;

	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		REGSTR_PATH_SERVICES _T("\\") NDAS_SERVICE_NAME _T("\\Parameters"),
		0,
		KEY_NOTIFY,
		&hKey);

	if (ERROR_SUCCESS != lResult) {

		DWORD dwResult = MonitorSubKeyCreation(
			HKEY_LOCAL_MACHINE,
			REGSTR_PATH_SERVICES _T("\\") NDAS_SERVICE_NAME,
			_T("Parameters"),
			hStopEvent);
		if (dwResult != 0) {
			return dwResult;
		}

		lResult = ::RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			REGSTR_PATH_SERVICES _T("\\") NDAS_SERVICE_NAME _T("\\Parameters"),
			0,
			KEY_NOTIFY,
			&hKey);

		if (ERROR_SUCCESS != lResult) {
			return lResult;
		}
	}

	HANDLE hChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == hChangeEvent) {
		::RegCloseKey(hKey);
		return ::GetLastError();
	}

	DWORD dwRet = 0xFFFFFFFF;

	while (TRUE) {

		lResult = ::RegNotifyChangeKeyValue(
			hKey,
			TRUE,
			REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
			hChangeEvent,
			TRUE);

		if (ERROR_SUCCESS != lResult) {
			break;
		}

		HANDLE hWaitEvents[2] = { hStopEvent, hChangeEvent };

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			2,
			hWaitEvents,
			FALSE,
			INFINITE);

		if (WAIT_OBJECT_0 == dwWaitResult) {
			dwRet = 0xFFFFFFFF;
			break;
		} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {

			//
			// Check value changes
			//
			BOOL fSuccess(FALSE);

			DWORD dwValue(0);
			fSuccess = _NdasServiceCfg.GetValue(_T("DebugLevel"), &dwValue);
			if (fSuccess) {
				if (_PXDebug->dwOutputLevel != dwValue) {
					_PXDebug->dwOutputLevel = dwValue;
					DPAlways(_T("Debug Level Changed to %d\n"), dwValue);
				}
			}

			fSuccess = _NdasServiceCfg.GetValue(_T("DebugFlags"), &dwValue);
			if (fSuccess) {
				if (_PXDebug->dwEnabledModules != dwValue) {
					_PXDebug->dwEnabledModules = dwValue;
					DPAlways(_T("Debug Flags Changed to 0x%08X\n"), dwValue);
					DPAlways(_T("Debug Flags: "));
					XDbgShowEnabledFlags();
					DPAlways(_T("\n"));
				}
			}

			fSuccess = ::ResetEvent(hChangeEvent);
			_ASSERTE(fSuccess);

		}

	}

	::CloseHandle(hChangeEvent);
	::RegCloseKey(hKey);

	return dwRet;
}

static VOID XDbgShowEnabledFlags()
{
	if (NULL == _PXDebug) {
		return;
	}
	DWORD dwFlags = _PXDebug->dwEnabledModules;
#define XDBG_FLAG_PRINT(_f_) \
	do { \
	if (_f_ & dwFlags) \
		_PXDebug->VRawPrintf(XDebug::OL_ALWAYS, _T(" ") _T(#_f_), NULL); \
	} while(0)

	XDBG_FLAG_PRINT(XDF_LPXCOMM);
	XDBG_FLAG_PRINT(XDF_INSTMAN);
	XDBG_FLAG_PRINT(XDF_EVENTPUB);
	XDBG_FLAG_PRINT(XDF_EVENTMON);
	XDBG_FLAG_PRINT(XDF_NDASIX);
	XDBG_FLAG_PRINT(XDF_NDASLOGDEV);
	XDBG_FLAG_PRINT(XDF_NDASLOGDEVMAN);
	XDBG_FLAG_PRINT(XDF_NDASPNP);
	XDBG_FLAG_PRINT(XDF_SERVICE);
	XDBG_FLAG_PRINT(XDF_DRVMATCH);
	XDBG_FLAG_PRINT(XDF_CMDPROC);
	XDBG_FLAG_PRINT(XDF_CMDSERVER);
	XDBG_FLAG_PRINT(XDF_NDASDEV);
	XDBG_FLAG_PRINT(XDF_NDASDEVHB);
	XDBG_FLAG_PRINT(XDF_NDASDEVREG);
	XDBG_FLAG_PRINT(XDF_MAIN);
	_PXDebug->VRawPrintf(XDebug::OL_ALWAYS, _T("\n"), NULL);

#undef XDBG_FLAG_PRINT

}