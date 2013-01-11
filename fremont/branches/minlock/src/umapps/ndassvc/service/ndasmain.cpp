 /*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <regstr.h>
#include <socketlpx.h>
#include <ndas/ndascomm.h>
#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>

#include "ndassvc.h"
#include "ndascfg.h"

// Control GUID for XDebugEventTrace
// {a4f3cd16-e134-4d13-bafe-3e22abd4fcd8}
static const GUID XDebugEventTraceControlGuid =
{ 0xa4f3cd16, 0xe134, 0x4d13, { 0xba, 0xfe, 0x3e, 0x22, 0xab, 0xd4, 0xfc, 0xd8 } };

// {2560cabf-47ec-4c43-a021-0f2b927025e6}
static const GUID XDebugEventTraceTransactionGuid =
{ 0x2560cabf, 0x47ec, 0x4c43, { 0xa0, 0x21, 0xf, 0x2b, 0x92, 0x70, 0x25, 0xe6 } };

#include "trace.h"
#ifdef RUN_WPP
#include "ndasmain.tmh"
#endif

// Auto Cleanup Routines

struct AutoNdasCommConfig {
	static BOOL GetInvalidValue() { return FALSE; }
	static void Release(BOOL v)
	{
		XTL_SAVE_LAST_ERROR();
		if (v) XTLVERIFY( ::NdasCommUninitialize() );
	}
};

typedef XTL::AutoCleanupT<BOOL,AutoNdasCommConfig> AutoNdasComm;
using XTL::AutoWSA;

CAppModule _Module;

int __cdecl wmain(DWORD argc, LPWSTR *argv)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

	//
	// Service Debugging support
	//
	DWORD dwStartupDelay = NdasServiceConfig::Get(nscDebugStartupDelay);
	if (dwStartupDelay > 0)
	{
		::Sleep(dwStartupDelay);
	}

	BOOL fDebugBreak = NdasServiceConfig::Get(nscDebugInitialBreak);
	if (fDebugBreak) 
	{
		::DebugBreak();
	}

#ifdef RUN_WPP
	WPP_INIT_TRACING(L"NDASSVC");
#endif

	XTL::XtlTraceSetTraceLevel(TRACE_LEVEL_INFORMATION);

	//
	// WSA Startup
	//
	WSADATA wsaData = {0};
	AutoWSA autoWSA = ::WSAStartup(MAKEWORD(2,2), &wsaData);
	if (autoWSA.IsInvalid())
	{
		_tprintf(_T("WSAStartup failed with error %d\n"), ::GetLastError());
		return -1;
	}

	//
	// NdasComm initialization
	//
	AutoNdasComm autoNdasComm = ::NdasCommInitialize();
	if (autoNdasComm.IsInvalid())
	{
		_tprintf(_T("Initialization of ndascomm failed with error %d\n"), ::GetLastError());
		return -1;
	}

	if ( (argc > 1) &&
		((*argv[1] == '-') || (*argv[1] == '/')) )
	{
		if ( lstrcmpi( _T("install"), argv[1]+1 ) == 0 )
		{
			// Install the service with current image path
			BOOL success = CNdasService::InstallService();
			if (!success) 
			{
				_tprintf(
					_T("%s (%s) service installed.\n"),
					CNdasService::GetServiceName(),
					CNdasService::GetServiceDisplayName());
			}
			else 
			{
				_tprintf(
					_T("Service install failure (%s (%s)) - Error %d\n"), 
					CNdasService::GetServiceName(),
					CNdasService::GetServiceDisplayName(),
					::GetLastError());
			}
		}
		else if ( lstrcmpi(_T("remove"), argv[1]+1 ) == 0 ||
			lstrcmpi(_T("uninstall"), argv[1]+1) == 0)
		{
			if (CNdasService::RemoveService()) 
			{
				_tprintf(
					_T("%s (%s) service removed successfully.\n"),
					CNdasService::GetServiceName(),
					CNdasService::GetServiceDisplayName());
			}
			else 
			{
				_tprintf(
					_T("Service removal failure (%s (%s)) - Error %d\n"),
					CNdasService::GetServiceName(),
					CNdasService::GetServiceDisplayName(),
					::GetLastError());
			}

		}
		else if ( lstrcmpi( _T("debug"), argv[1]+1 ) == 0 )
		{
			_Module.Init(NULL, NULL);
			int ret = CNdasService::ServiceDebugMain(argc, argv);
			_Module.Term();
			return ret;
		}
	}
	else
	{
		// if it doesn't match any of the above parameters
		// the service control manager may be starting the service
		// so we must call StartServiceCtrlDispatcher
				
		// this is just to be friendly
		_tprintf( _T("%s -install          to install the service\n"), argv[0]);
		_tprintf( _T("%s -remove           to remove the service\n"), argv[0]);
		_tprintf( _T("%s -debug <params>   to run as a console app for debugging\n"), argv[0]);
		_tprintf( _T("\nStartServiceCtrlDispatcher being called.\n"));
		_tprintf( _T("This may take several seconds.  Please wait.\n"));
	
		_Module.Init(NULL, NULL);

		SERVICE_TABLE_ENTRY entry[2] = {0};
		CNdasService::GetServiceTableEntry(&entry[0]);
		if (!::StartServiceCtrlDispatcher(entry))
		{
			_tprintf(
				_T("StartServiceCtrlDispatcher failed - Error %d\n"), 
				::GetLastError());
		}

		_Module.Term();

	}

#ifdef RUN_WPP
	WPP_CLEANUP();
#endif

	return 0;
}
