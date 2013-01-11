/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "ndassvc.h"
#include "ndasinstman.h"

// PnP Device Notification
#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>
#include <winioctl.h>

#include <process.h>

#include "ndaspnp.h"
#include "ndascmdserver.h"
#include "ndasdevhb.h"
#include "ndasdevreg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasix.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_SERVICE
#include "xdebug.h"

//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK DebugWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static unsigned int _stdcall DebugWndThreadProc(void * pArg);

//////////////////////////////////////////////////////////////////////////

LPCTSTR CNdasService::SERVICE_NAME = NDAS_SERVICE_NAME;
LPCTSTR CNdasService::DISPLAY_NAME = NDAS_SERVICE_DISPLAY_NAME;

PCNdasService
CNdasService::
Instance()
{
	if (NULL != ximeta::CService::s_pServiceInstance)
		return reinterpret_cast<PCNdasService>(ximeta::CService::s_pServiceInstance);
	else
		return new CNdasService();
}

CNdasService::
CNdasService() : 
ximeta::CService(CNdasService::SERVICE_NAME, CNdasService::DISPLAY_NAME)
{
	ximeta::CService::s_pServiceInstance = this;
}

CNdasService::
~CNdasService()
{
}

DWORD
CNdasService::
OnTaskStart()
{
	//
	// Call Instance Manager to initialize the objects
	// functions Initialize() of objects are called automatically
	//

	BOOL fSuccess = CNdasInstanceManager::Initialize();
	
	if (!fSuccess) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return ::GetLastError();
	} 

	//
	// Get the initialized instance
	//

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();

	fSuccess = pInstMan->GetCommandServer()->Run();
	if (!fSuccess) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	} 

	fSuccess = pInstMan->GetHBListener()->Run();
	if (!fSuccess) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	} 

	fSuccess = pInstMan->GetEventMonitor()->Run();
	if (!fSuccess) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	}

	fSuccess = pInstMan->GetEventPublisher()->Run();
	if (!fSuccess) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	}

	//
	// Register Plug'n'Play Device Notification
	//

	CNdasServiceDeviceEventHandler* pDeviceEventHandler;
	if (m_bDebugMode) {

		pDeviceEventHandler = pInstMan->CreateDeviceEventHandler(
			m_hWndDebug, 
			DEVICE_NOTIFY_WINDOW_HANDLE);

	} else {

		pDeviceEventHandler = pInstMan->CreateDeviceEventHandler(
			m_sshStatusHandle, 
			DEVICE_NOTIFY_SERVICE_HANDLE);

	}

	if (NULL == pDeviceEventHandler) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	}

	fSuccess = pDeviceEventHandler->Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("PnpHandler Initialization failed: "));
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	}

	CNdasServicePowerEventHandler *pPowerEventHandler =
		pInstMan->CreatePowerEventHandler();

	CNdasInfoExchangeBroadcaster cixb;
	fSuccess = cixb.Initialize();
	_ASSERTE(fSuccess);
	fSuccess = cixb.Run();
	_ASSERTE(fSuccess);

	CNdasInfoExchangeServer cixserver;
	fSuccess = cixserver.Initialize();
	_ASSERTE(fSuccess);
	fSuccess = cixserver.Run();
	_ASSERTE(fSuccess);

	//
	// Now time to bootstrap registrar from the registry
	//

	fSuccess = pInstMan->GetRegistrar()->Bootstrap();
	_ASSERTE(fSuccess && "Bootstrap should not failed!");

	//
	// Termination Handle
	//

	BOOL bTerminate = FALSE;
	DWORD dwWin32ExitCode = 0;
	HANDLE hEvents[1];
	hEvents[0] = m_hTaskTerminateEvent;

	while (!bTerminate) {
		DWORD dwWaitResult = ::WaitForMultipleObjects(
			1,
			hEvents,
			FALSE,
			INFINITE);

		switch (dwWaitResult) {
		case WAIT_OBJECT_0:
			bTerminate = TRUE;
			break;
		default:
			;
		}
	}

	cixserver.Stop();
	cixb.Stop();
	pInstMan->GetCommandServer()->Stop();
	pInstMan->GetHBListener()->Stop();
	pInstMan->GetEventMonitor()->Stop();

	CNdasInstanceManager::Cleanup();

	ReportStatusToSCMgr(SERVICE_STOPPED, 0, dwWin32ExitCode);

	return dwWin32ExitCode;
}


VOID
CNdasService::
ServiceDebug(DWORD dwArgc, LPTSTR *lpArgs)
{
	m_bDebugMode = TRUE;

	BOOL fSuccess = this->ximeta::CTask::Initialize();
	if (!fSuccess) {
		// TODO: Return appropriate error code for stopped
		// TODO: Event Log
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return;
	}

	//
	// Debug Window
	//

	DWORD dwDebugWndThreadId;
	HANDLE hDebugWndThread = (HANDLE) _beginthreadex( 
		NULL, 
		0, 
		&DebugWndThreadProc, 
		&m_hWndDebug, 
		0, 
		(PUINT) &dwDebugWndThreadId);

	while (!m_hWndDebug) {
		::Sleep(500); // wait for the window created.
	}

	fSuccess = this->Run();

	if (!fSuccess) {
		// TODO: Return appropriate error code for stopped
		// TODO: Event Log
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return;
	}

	HANDLE hServiceThread = this->GetTaskHandle();
	
	_tprintf(_T("Service Debug Started...\n"));
	while (TRUE) {
		_tprintf(_T("Press q and <Enter> to stop...\n"));
		TCHAR c = _gettc(stdin);
		if (c == _T('q') || c == _T('Q')) {
			break;
		}
	}
	

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPub = pInstMan->GetEventPublisher();
	if (NULL != pEventPub) {
		pEventPub->ServiceTerminating();
	}

	_tprintf(_T("Please wait while stopping...\n"));

	HANDLE hTask = GetTaskHandle();
	this->Stop(FALSE);
	
	DWORD dwWaitTimeout = 5000L; // 30 sec
	DWORD dwWaitResult = ::WaitForSingleObject(hTask, dwWaitTimeout);
	
	if (dwWaitResult == WAIT_TIMEOUT) {
		_tprintf(_T("Freezed, forcibly terminating...\n"));
	}

	return;
}

VOID
CNdasService::
ServiceMain(DWORD dwArgc, LPTSTR* lpArgs)
{
	m_bDebugMode = FALSE;

	BOOL fSuccess = this->ximeta::CTask::Initialize();
	if (!fSuccess) {
		// TODO: Return appropriate error code for stopped
		// TODO: Event Log
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return;
	}

	fSuccess = this->Run();
	
	if (!fSuccess) {
		// TODO: Return appropriate error code for stopped
		// TODO: Event Log
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return;
	}

	HANDLE hServiceThread = this->GetTaskHandle();
	HANDLE hNewWaitObject;
	
	OutputDebugString(_T("Registering Wait For Single Object"));

	fSuccess = RegisterWaitForSingleObject(
		&hNewWaitObject,
		hServiceThread,
		NULL,
		NULL,
		INFINITE,
		WT_EXECUTEONLYONCE);

	if (!fSuccess) {
		this->Stop();
		// TODO: Return appropriate error code for stopped
		// TODO: Event Log
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return;
	}

	OutputDebugString(_T("Service Running"));

	ReportStatusToSCMgr(SERVICE_RUNNING);

	return;
}

DWORD
CNdasService::
OnServiceStop()
{
	ReportStatusToSCMgr(SERVICE_STOP_PENDING);

	this->Stop();

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPub = pInstMan->GetEventPublisher();
	if (NULL != pEventPub) {
		pEventPub->ServiceTerminating();
	}

	HANDLE hTask = GetTaskHandle();
	this->Stop(FALSE);
	
	DWORD dwWaitTimeout = 5000L; // 30 sec
	DWORD dwWaitResult = ::WaitForSingleObject(hTask, dwWaitTimeout);
	
	if (dwWaitResult == WAIT_TIMEOUT) {
		_tprintf(_T("Freezed, forcibly terminating...\n"));
	}

	return NO_ERROR;
}

DWORD
CNdasService::
OnServiceShutdown()
{
	ReportStatusToSCMgr(SERVICE_STOP_PENDING);

	// TODO: Do I have to stop here?
	this->Stop();

	return NO_ERROR;
}

DWORD
CNdasService::
OnServicePause()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD
CNdasService::
OnServiceResume()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD
CNdasService::
OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// Ignore if pDeviceEventHandler is not available yet.
	//

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasServiceDeviceEventHandler *pDeviceEventHandler = 
		pInstMan->GetDeviceEventHandler();

	if (NULL == pDeviceEventHandler) {
		DPWarning(_FT("NdasServicePnpHandler not yet available.\n"));
		return TRUE;
	}
	return pDeviceEventHandler->OnDeviceEvent(dwEventType, (LPARAM)lpEventData);
}

DWORD
CNdasService::
OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// Ignore if pDeviceEventHandler is not available yet.
	//

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasServicePowerEventHandler *pPowerEventHandler = 
		pInstMan->GetPowerEventHandler();

	if (NULL == pPowerEventHandler) {
		DPWarning(_FT("NdasServicePnpHandler not yet available.\n"));
		return TRUE;
	}
	return pPowerEventHandler->OnPowerEvent(dwEventType, (LPARAM)lpEventData);
}


//////////////////////////////////////////////////////////////////////////

LPCTSTR 
CNdasServiceInstaller::
SERVICE_NAME = NDAS_SERVICE_NAME;

LPCTSTR 
CNdasServiceInstaller::
DISPLAY_NAME = NDAS_SERVICE_DISPLAY_NAME;

BOOL 
CNdasServiceInstaller::
Install(LPCTSTR lpBinaryPathName)
{
	return ximeta::CServiceInstaller::InstallService(
		NULL,
		CNdasServiceInstaller::SERVICE_NAME,
		CNdasServiceInstaller::DISPLAY_NAME,
		SERVICE_ALL_ACCESS,
		CNdasServiceInstaller::SERVICE_TYPE,
		CNdasServiceInstaller::START_TYPE,
		CNdasServiceInstaller::ERROR_CONTROL,
		lpBinaryPathName);
}

BOOL 
CNdasServiceInstaller::
Remove()
{
	return ximeta::CServiceInstaller::RemoveService(
		NULL,
		CNdasServiceInstaller::SERVICE_NAME);
}


//////////////////////////////////////////////////////////////////////////

//
// debugging mode windows thread proc
//

unsigned int _stdcall DebugWndThreadProc(void * pArg)
{
	//
	// A hidden window for device notification
	// (Message-only windows cannot receive WM_BROADCAST)
	//

	WNDCLASSEX wcex;

	::ZeroMemory(&wcex, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= 0;
	wcex.lpfnWndProc	= (WNDPROC)DebugWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= NULL;
	wcex.hIcon			= NULL;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= TEXT("NDASHelperServiceWnd");
	wcex.hIconSm		= NULL;

	::RegisterClassEx(&wcex);

	HWND* phWnd = (HWND*) pArg;

	*phWnd = ::CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,
		TEXT("NDASHelperServiceWnd"), 
		TEXT("NDAS Helper Service"), 
		WS_EX_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		NULL,
		NULL,
		NULL);

	if (*phWnd == NULL) {
		DPErrorEx(TEXT("Creating Debug Window failed.\n"));
		_ASSERT(FALSE && "Creating Debug Window failed.\n");
	};

	// ::ShowWindow(*phWnd, SW_SHOW);

	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	_endthreadex(0);

	return 0;
}

//
// debugging mode PNP Message Recipient
//

LRESULT CALLBACK DebugWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
	case WM_DEVICECHANGE:
		{
			//
			// Ignore if pDeviceEventHandler is not available yet.
			//

			CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
			_ASSERTE(NULL != pInstMan);

			CNdasServiceDeviceEventHandler *pDeviceEventHandler = pInstMan->GetDeviceEventHandler();
			if (NULL == pDeviceEventHandler) {
				DPWarning(_FT("NdasServicePnpHandler not yet available.\n"));
				return TRUE;
			}
			return pDeviceEventHandler->OnDeviceEvent(wParam, lParam);
		}

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

