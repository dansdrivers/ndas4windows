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
#include "ndasobjs.h"
#include "ndashixsrv.h"
#include "ndasautoreg.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_SERVICE
#include "xdebug.h"

#include "lfsfiltctl.h"
#include "lsbusctl.h"

//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK DebugWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static unsigned int _stdcall DebugWndThreadProc(void * pArg);

//////////////////////////////////////////////////////////////////////////

LPCTSTR CNdasService::SERVICE_NAME = NDAS_SERVICE_NAME;
LPCTSTR CNdasService::DISPLAY_NAME = NDAS_SERVICE_DISPLAY_NAME;

CNdasService*
CNdasService::Instance()
{
	if (NULL != ximeta::CService::s_pServiceInstance)
		return reinterpret_cast<CNdasService*>(ximeta::CService::s_pServiceInstance);
	else
		return new CNdasService();
}

CNdasService::CNdasService() : 
	ximeta::CService(CNdasService::SERVICE_NAME, CNdasService::DISPLAY_NAME),
	ximeta::CTask(_T("NdasService Task"))
{
	ximeta::CService::s_pServiceInstance = this;
}

CNdasService::~CNdasService()
{
}

DWORD
CNdasService::OnTaskStart()
{
	//
	//	Stop NdasBus auto-plugin feature to take over plugin facility.
	//

	BOOL fSuccess = LsBusCtlStartStopRegistrarEnum(FALSE, NULL);
	_ASSERTE(fSuccess);

	//
	// Call Instance Manager to initialize the objects
	// functions Initialize() of objects are called automatically
	//

	fSuccess = CNdasInstanceManager::Initialize();
	
	if (!fSuccess) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		return ::GetLastError();
	} 

	//
	// Get the initialized instance
	//

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();

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
		DBGPRT_ERR_EX(_FT("PnpHandler Initialization failed: "));
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, ::GetLastError());
		CNdasInstanceManager::Cleanup();
		return ::GetLastError();
	}

	CNdasServicePowerEventHandler *pPowerEventHandler =
		pInstMan->CreatePowerEventHandler();

	CNdasIXBcast cixb;
	fSuccess = cixb.Initialize();
	_ASSERTE(fSuccess);
	fSuccess = cixb.Run();
	_ASSERTE(fSuccess);

	CNdasIXServer cixserver;
	fSuccess = cixserver.Initialize();
	_ASSERTE(fSuccess);
	fSuccess = cixserver.Run();
	_ASSERTE(fSuccess);

	LPCGUID lpHostGuid = pGetNdasHostGuid();

	CNdasHIXServer hixsrv(lpHostGuid);
	fSuccess = hixsrv.Initialize();
	_ASSERTE(fSuccess);
	fSuccess = hixsrv.Run();
	_ASSERTE(fSuccess);

	CNdasAutoRegister autoRegister;
	fSuccess = autoRegister.Initialize();
	_ASSERTE(fSuccess);
	fSuccess = autoRegister.Run();
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

	pInstMan->GetEventPublisher()->ServiceTerminating();

	autoRegister.Stop();
	hixsrv.Stop();
	cixserver.Stop();
	cixb.Stop();
	pInstMan->GetCommandServer()->Stop();
	pInstMan->GetHBListener()->Stop();
	pInstMan->GetEventMonitor()->Stop();
	pInstMan->GetEventPublisher()->Stop();
	
	CNdasInstanceManager::Cleanup();

	return dwWin32ExitCode;
}


VOID
CNdasService::ServiceDebug(DWORD dwArgc, LPTSTR *lpArgs)
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
CNdasService::ServiceMain(DWORD dwArgc, LPTSTR* lpArgs)
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

	DBGPRT_INFO(_FT("NDAS Service is running...\n"));

	ReportStatusToSCMgr(SERVICE_RUNNING);

	return;
}

DWORD
CNdasService::OnServiceStop()
{
	//
	// We should report the SCM that the service is stopping
	// Otherwise, the service will terminate the thread.
	// And we'll get ACCESS VIOLATION ERROR!
	//

	DBGPRT_INFO(_FT("Service is stopping...\n"));

#if 0
	//
	// Eject all logical device instances
	//
	if (NULL != GetTaskHandle()) {
		CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
		if (NULL != pInstMan) {
			CNdasLogicalDeviceManager* pLdm = pInstMan->GetLogDevMan();
			if (NULL != pLdm) {
				pLdm->Lock();
				CNdasLogicalDeviceManager::ConstIterator itr =
					pLdm->begin();
				for (; itr != pLdm->end(); ++itr) {
					CNdasLogicalDevice* pLogDevice = itr->second;
					if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus()) {
						//
						// If we should call pLogDevice->Eject here,
						// we must wait here until eject is really complete.
						//
						BOOL fSuccess = pLogDevice->Eject();
						ReportStatusToSCMgr(SERVICE_STOP_PENDING, 3000);
						::Sleep(2000);
					}
				}
				pLdm->Unlock();
			}
		}
	}
#endif

	ReportStatusToSCMgr(SERVICE_STOP_PENDING, 3000);

	HANDLE hTask = GetTaskHandle();
	this->Stop(FALSE);
	
	DWORD dwWaitTimeout = 3000L; // 3 sec
	ReportStatusToSCMgr(SERVICE_STOP_PENDING, 3000);
	DWORD dwWaitResult = ::WaitForSingleObject(hTask, dwWaitTimeout);

	if (WAIT_OBJECT_0 == dwWaitResult) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 1000, NO_ERROR);
		return NO_ERROR;
	}

	ReportStatusToSCMgr(SERVICE_STOP_PENDING);
	dwWaitResult = ::WaitForSingleObject(hTask, dwWaitTimeout);

	if (WAIT_OBJECT_0 == dwWaitResult) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 1000, NO_ERROR);
		return NO_ERROR;
	}

	ReportStatusToSCMgr(SERVICE_STOP_PENDING);
	dwWaitResult = ::WaitForSingleObject(hTask, dwWaitTimeout);

	if (WAIT_OBJECT_0 == dwWaitResult) {
		ReportStatusToSCMgr(SERVICE_STOPPED, 1000, NO_ERROR);
		return NO_ERROR;
	}

	ReportStatusToSCMgr(SERVICE_STOP_PENDING);
	dwWaitResult = ::WaitForSingleObject(hTask, dwWaitTimeout);

	if (dwWaitResult == WAIT_TIMEOUT) {
	}

	ReportStatusToSCMgr(SERVICE_STOPPED, 100, 1);
	return 1;
}

DWORD
CNdasService::OnServiceShutdown()
{
	DBGPRT_INFO(_FT("System is shutting down...\n"));

	ReportStatusToSCMgr(SERVICE_STOP_PENDING, 1000);

	CNdasLogicalDeviceManager* pLogDevMan = pGetNdasLogicalDeviceManager();
	pLogDevMan->OnShutdown();

	ReportStatusToSCMgr(SERVICE_STOP_PENDING, 1000);

	(VOID) ::LfsFiltCtlShutdown();

	// TODO: Do I have to stop here?
	this->Stop();

	return NO_ERROR;
}

DWORD
CNdasService::OnServicePause()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD
CNdasService::OnServiceResume()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD
CNdasService::OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// Ignore if pDeviceEventHandler is not available yet.
	//

	CNdasServiceDeviceEventHandler 
		*pDeviceEventHandler = pGetNdasDeviceEventHandler();

	if (NULL == pDeviceEventHandler) {
		DBGPRT_WARN(_FT("NdasServicePnpHandler not yet available.\n"));
		return NO_ERROR;
	}

	//
	// Return code for services and Windows applications
	// for handling Device Events are different
	// Device Event Handler is based on Windows application,
	// which will return TRUE or BROADCAST_QUERY_DENY
	//
	// For services:
	//
	// If your service handles SERVICE_CONTROL_DEVICEEVENT, 
	// return NO_ERROR to grant the request 
	// and an error code to deny the request.
	//

	LRESULT lResult = pDeviceEventHandler->OnDeviceEvent(
		dwEventType, (LPARAM)lpEventData);

	if (BROADCAST_QUERY_DENY == lResult) {
		return 1;
	} else {
		return NO_ERROR;
	}

}

DWORD
CNdasService::OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// Ignore if pDeviceEventHandler is not available yet.
	//

	//
	// Return code for services and Windows applications
	// for handling Device Events are different
	// Device Event Handler is based on Windows application,
	// which will return TRUE or BROADCAST_QUERY_DENY
	//
	// For services:
	//
	// If your service handles HARDWAREPROFILECHANGE, 
	// return NO_ERROR to grant the request 
	// and an error code to deny the request.
	//

	CNdasServicePowerEventHandler*
		pPowerEventHandler = pGetNdasPowerEventHandler();

	if (NULL == pPowerEventHandler) {
		DBGPRT_WARN(_FT("NdasServicePnpHandler not yet available.\n"));
		return NO_ERROR;
	}
	
	LRESULT lResult = pPowerEventHandler->OnPowerEvent(dwEventType, (LPARAM)lpEventData);
	if (BROADCAST_QUERY_DENY == lResult) {
		return 1;
	} else {
		return NO_ERROR;
	}

}


//////////////////////////////////////////////////////////////////////////

LPCTSTR 
CNdasServiceInstaller::SERVICE_NAME = NDAS_SERVICE_NAME;

LPCTSTR 
CNdasServiceInstaller::DISPLAY_NAME = NDAS_SERVICE_DISPLAY_NAME;

BOOL 
CNdasServiceInstaller::Install(LPCTSTR lpBinaryPathName)
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
CNdasServiceInstaller::Remove()
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
		DBGPRT_ERR_EX(TEXT("Creating Debug Window failed.\n"));
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
				DBGPRT_WARN(_FT("NdasServicePnpHandler not yet available.\n"));
				return TRUE;
			}
			return pDeviceEventHandler->OnDeviceEvent(wParam, lParam);
		}
	case WM_ENDSESSION:
		{
			//
			// ENDSESSION_LOGOFF does not effect to the service
			//
			// If lParam is zero, the system is shutting down.
			if (0 == lParam) {
				CNdasService* pService = CNdasService::Instance();
				if (NULL != pService) {
					pService->OnServiceShutdown();
				}
			}
		}

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

