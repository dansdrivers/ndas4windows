/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
// PnP Device Notification
#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>
#include <winioctl.h>
#include <lfsfiltctl.h>
#include <ndasbusctl.h>
#include <xtl/xtlautores.h>
#include <ndas/ndasportctl.h>
#include <ndas/ndasautoregscope.h>
#include <ndas/ndashix.h>

#include "ndasdevid.h"
#include "ndasobjs.h"
#include "ndassvc.h"
#include "ndaspnp.h"
#include "ndascmdserver.h"
#include "ndasdevhb.h"
#include "ndasdevreg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasix.h"
#include "ndashixsrv.h"
#include "ndasautoreg.h"

#include "ndaspnp.h"
#include "ndasdevhb.h"
#include "ndasix.h"
#include "ndashixsrv.h"
#include "ndascmdserver.h"
#include "ndasdevreg.h"
#include "ndasdevid.h"
#include "ndaslogdevman.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndaspowereventhandler.hpp"

#include "trace.h"
#ifdef RUN_WPP
#include "ndassvc.tmh"
#endif

BOOL IsNdasPortMode()
{
	return CNdasService::Instance()->NdasPortExists();
}

class CNdasServiceWindow : public CWindowImpl<CNdasServiceWindow>
{
	CNdasService& m_service;
	HANDLE m_destroyedEvent;

public:
	BEGIN_MSG_MAP(CNdasServiceWindow)
		MESSAGE_HANDLER(WM_DEVICECHANGE, OnDeviceChange)
	END_MSG_MAP()

	CNdasServiceWindow(CNdasService& service) : m_service(service) 
	{
		XTLVERIFY(m_destroyedEvent = CreateEvent(NULL, TRUE, TRUE, NULL));
	}

	~CNdasServiceWindow()
	{
		XTLVERIFY(CloseHandle(m_destroyedEvent));
	}

	LRESULT OnDeviceChange(UINT /*Msg*/, WPARAM wParam, LPARAM lParam, BOOL& Handled)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, "Type=%08X, Data=%p\n", wParam, lParam);

		PDEV_BROADCAST_HDR hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

		switch (wParam)
		{
		case DBT_DEVICEARRIVAL:
			if (hdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
			{
				PDEV_BROADCAST_VOLUME volumeInfo = reinterpret_cast<PDEV_BROADCAST_VOLUME>(lParam);
				m_service.GetDeviceEventHandler().OnVolumeArrival(volumeInfo);
			}
			break;
		case DBT_DEVICEREMOVECOMPLETE:
			if (hdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
			{
				PDEV_BROADCAST_VOLUME volumeInfo = reinterpret_cast<PDEV_BROADCAST_VOLUME>(lParam);
				m_service.GetDeviceEventHandler().OnVolumeRemoveComplete(volumeInfo);
			}
			break;
		}
		return TRUE;
	}
	static DWORD WINAPI ThreadStart(LPVOID Param)
	{
		return static_cast<CNdasServiceWindow*>(Param)->ThreadStart();
	}
	DWORD ThreadStart()
	{
		CCoInitialize coinit(COINIT_MULTITHREADED);

		CMessageLoop theLoop;
		_Module.AddMessageLoop(&theLoop);

		HWND parent = GetDesktopWindow();

		XTLVERIFY( ResetEvent(m_destroyedEvent) );

		if (this->Create(parent) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));

			XTLVERIFY( SetEvent(m_destroyedEvent) );
			return 1;
		}

		this->ShowWindow(SW_HIDE);

		int nRet = theLoop.Run();

		_Module.RemoveMessageLoop();

		return nRet;
	}
	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		XTLVERIFY( SetEvent(m_destroyedEvent) );
	}
	void WaitForDestroy()
	{
		WaitForSingleObject(m_destroyedEvent, INFINITE);
	}
};

class CNdasService::Impl :
	public XTL::CThreadedWorker<CNdasService::Impl>
{
protected:
	static const DWORD MAX_WORK_ITEMS = 10;

	CNdasService& m_service;

	XTL::AutoObjectHandle m_hStartServiceEvent;
	XTL::AutoObjectHandle m_hStopServiceEvent;
	XTL::AutoObjectHandle m_hWorkItemSemaphore;
	DWORD m_nWorkItems;

public:

	Impl(CNdasService& service);

	DWORD ThreadStart(LPVOID);
	HRESULT InitializeInstances();
	bool StartWorkItems(DWORD& nStarted);

	DWORD OnServiceStop();
	DWORD OnServiceShutdown();
	BOOL ServiceStart(DWORD dwArgc, LPTSTR* lpArgs);

	template <typename T>
	BOOL QueueUserWorkItem(
		CNdasServiceWorkItem<T,HANDLE>& workItem,
		T& Instance, DWORD Flags = WT_EXECUTELONGFUNCTION)
	{
		return workItem.QueueUserWorkItemParam(
			&Instance,
			m_hStartServiceEvent,
			m_hStopServiceEvent,
			m_hWorkItemSemaphore,
			m_hStopServiceEvent,
			Flags);
	}

	BOOL ReportServicePaused() {
		return m_service.ReportServicePaused();
	}
	BOOL ReportServiceRunning() {
		return m_service.ReportServiceRunning();
	}
	BOOL ReportServiceStopped(DWORD Win32ExitCode = NO_ERROR, DWORD ServiceExitCode = NO_ERROR)	{
		return m_service.ReportServiceStopped(Win32ExitCode, ServiceExitCode);
	}
	BOOL ReportServiceContinuePending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1) {
		return m_service.ReportServiceContinuePending(WaitHint, CheckPointIncrement);
	}
	BOOL ReportServicePausePending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1) {
		return m_service.ReportServicePausePending(WaitHint, CheckPointIncrement);
	}
	BOOL ReportServiceStartPending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1) {
		return m_service.ReportServiceStartPending(WaitHint, CheckPointIncrement);
	}
	BOOL ReportServiceStopPending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1)	{
		return m_service.ReportServiceStopPending(WaitHint, CheckPointIncrement);
	}

	BOOL NdasPortExists() {
		return m_NdasPort;
	}
	DWORD GetNdasPortNumber() {
		return m_NdasPortNumber;
	}

public:

	//
	// Classes associated with its own thread or work item.
	//
	CNdasDeviceHeartbeatListener m_cHeartbeatListener;
	CNdasServiceWorkItem<CNdasDeviceHeartbeatListener,HANDLE> m_wiHeartbeatListener;

	CNdasAutoRegister m_cAutoRegProcessor;
	CNdasServiceWorkItem<CNdasAutoRegister,HANDLE> m_wiAutoRegProcess;

#ifdef __NDASSVC_ENABLE_IX__
	CNdasIXBcast m_cIXBCaster;
	CNdasServiceWorkItem<CNdasIXBcast,HANDLE> m_wiIXBCaster;

	CNdasIXServer m_cIXServer;
	CNdasServiceWorkItem<CNdasIXServer,HANDLE> m_wiIXServer;
#endif
	CNdasHIXServer m_cHIXServer;
	CNdasServiceWorkItem<CNdasHIXServer,HANDLE> m_wiHIXServer;

	CNdasEventMonitor m_cEventMonitor;
	CNdasServiceWorkItem<CNdasEventMonitor,HANDLE> m_wiEventMonitor;

	CNdasEventPublisher m_cEventPublisher;
	CNdasServiceWorkItem<CNdasEventPublisher,HANDLE> m_wiEventPublisher;

	CNdasCommandServer m_cCmdServer;
	XTL::CThread<CNdasCommandServer> m_wiCmdServer;

	//
	// Simple classes
	//
	CNdasServiceDeviceEventHandler m_cDeviceEventHandler;
	CNdasServicePowerEventHandler m_cPowerEventHandler;

	CComPtr<INdasDeviceRegistrarInternal> m_pNdasDeviceRegistrar;
	CComPtr<INdasLogicalUnitManagerInternal> m_pNdasLogicalUnitManager;

	//
	// Volume Device Event Handler
	//
	CNdasServiceWindow m_serviceWindow;

	//
	// NDAS port information
	//
	BOOL	m_NdasPort;
	UCHAR	m_NdasPortNumber;
};

CNdasService::Impl::Impl(CNdasService& service) :
	m_service(service),
	m_nWorkItems(0),
	m_cHIXServer(pGetNdasHostGuid()),
	m_cPowerEventHandler(service),
	m_serviceWindow(service),
	m_NdasPort(FALSE),
	m_NdasPortNumber(-1)
{
}

HRESULT
CNdasService::Impl::InitializeInstances()
{
	HRESULT hr;

	CComObject<CNdasDeviceRegistrar>* pNdasDeviceRegistrarInstance;
	hr = CComObject<CNdasDeviceRegistrar>::CreateInstance(&pNdasDeviceRegistrarInstance);
	if (FAILED(hr))
	{
		return hr;
	}

	m_pNdasDeviceRegistrar = pNdasDeviceRegistrarInstance;

	CComObject<CNdasLogicalUnitManager>* pNdasLogicalUnitManagerInstance;
	hr = CComObject<CNdasLogicalUnitManager>::CreateInstance(&pNdasLogicalUnitManagerInstance);
	if (FAILED(hr))
	{
		return hr;
	}
	
	m_pNdasLogicalUnitManager = pNdasLogicalUnitManagerInstance;

	hr = m_cHeartbeatListener.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_cAutoRegProcessor.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}
#ifdef __NDASSVC_ENABLE_IX__
	if (!m_cIXBCaster.Initialize())
	{
		return E_FAIL;
	}
	if (!m_cIXServer.Initialize())
	{
		return E_FAIL;
	}
#endif

	hr = m_cHIXServer.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_cEventMonitor.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_cEventPublisher.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_cCmdServer.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}

	HANDLE hDevNotifyRecipient = m_service.m_bDebugMode ? 
		(HANDLE)m_service.m_hDebugWnd : 
		(HANDLE)m_service.m_hService;

	DWORD dwDevNotifyFlags = m_service.m_bDebugMode ? 
		DEVICE_NOTIFY_WINDOW_HANDLE : 
		DEVICE_NOTIFY_SERVICE_HANDLE;

	hr = m_cDeviceEventHandler.Initialize(
		hDevNotifyRecipient, 
		dwDevNotifyFlags);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_cPowerEventHandler.Initialize();
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}

bool
CNdasService::Impl::StartWorkItems(DWORD& nStarted)
{
	nStarted = 0;
	if (!QueueUserWorkItem(m_wiHeartbeatListener, m_cHeartbeatListener)) 
	{
		return false;
	}
	++nStarted;
	if (!QueueUserWorkItem(m_wiAutoRegProcess, m_cAutoRegProcessor)) 
	{
		return false;
	}
#ifdef __NDASSVC_ENABLE_IX__
	++nStarted;
	if (!QueueUserWorkItem(m_wiIXBCaster, m_cIXBCaster)) 
	{
		return false;
	} 
	++nStarted;
	if (!QueueUserWorkItem(m_wiIXServer, m_cIXServer)) 
	{
		return false;
	}
#endif
	++nStarted;
	if (!QueueUserWorkItem(m_wiHIXServer, m_cHIXServer)) 
	{
		return false;
	} 
	++nStarted;
	if (!QueueUserWorkItem(m_wiEventMonitor, m_cEventMonitor)) 
	{
		return false;
	}
	++nStarted;
	if (!QueueUserWorkItem(m_wiEventPublisher, m_cEventPublisher)) 
	{
		return false;
	}
	++nStarted;
	return true;
}

DWORD
CNdasService::Impl::ThreadStart(LPVOID)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

	BOOL success;

	//
	// Detect NDAS port
	//

	XTL::AutoFileHandle handle;

	HRESULT hr = NdasPortCtlCreateControlDevice(
		GENERIC_READ | GENERIC_WRITE, &handle);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_ERROR,
			"NdasPortCtlCreateControlDevice failed, hr=0x%X\n", hr);
	} 
	else 
	{

		hr = NdasPortCtlGetPortNumber(
			handle, 
			&m_NdasPortNumber);
		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_ERROR,
				"NdasPortCtlGetPortNumber failed, hr=0x%X\n", hr);
		} 
		else 
		{
			// Indicate NDAS port exists.
			m_NdasPort = TRUE;		
		}
	}

	//
	//	Stop NdasBus auto-plugin feature to take over plugin facility.
	//
	if (!m_NdasPort)
	{
		success = NdasBusCtlStartStopRegistrarEnum(FALSE, NULL);
		XTLASSERT(success);
	}
	else
	{
		hr = NdasPortCtlStartStopRegistrarEnum(FALSE, NULL);
		XTLASSERT(SUCCEEDED(hr));
	}

	//////////////////////////////////////////////////////////////////////////
	// Get the initialized instance
	//////////////////////////////////////////////////////////////////////////

	hr = InitializeInstances();
	if (FAILED(hr))
	{
		OnServiceStop();
		return 1;
	}

	//////////////////////////////////////////////////////////////////////////
	// Bootstrap registrar from the registry
	//////////////////////////////////////////////////////////////////////////

	COMVERIFY(m_pNdasDeviceRegistrar->Bootstrap());
	// XTLVERIFY( m_cDeviceRegistrar.Bootstrap() );

	//////////////////////////////////////////////////////////////////////////
	// Start Queuing Work Items
	//////////////////////////////////////////////////////////////////////////

	m_nWorkItems = 0;
	if (!StartWorkItems(m_nWorkItems))
	{
		OnServiceStop();
		return 1;
	}

	// Release semaphore to start workers (semaphore increment)
	LONG prev;
	XTLVERIFY( ::ReleaseSemaphore(m_hWorkItemSemaphore, m_nWorkItems, &prev) );
	XTLASSERT( 0 == prev );

	//////////////////////////////////////////////////////////////////////////
	// Start Command Processor Thread
	//////////////////////////////////////////////////////////////////////////

	if (!m_wiCmdServer.CreateThreadEx(
			&m_cCmdServer, 
			&CNdasCommandServer::ThreadStart,
			LPVOID(m_hStopServiceEvent)))
	{
		OnServiceStop();
		return 1;
	}

	//////////////////////////////////////////////////////////////////////////
	// Initialization thread is done
	//////////////////////////////////////////////////////////////////////////

	m_serviceWindow.ThreadStart();

	return 0;
}

BOOL
CNdasService::Impl::ServiceStart(DWORD dwArgc, LPTSTR* lpArgs)
{
	XTLVERIFY(ReportServiceStartPending(1000));
	m_hStopServiceEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_hStopServiceEvent.IsInvalid())
	{
		return FALSE;
	}

	XTLVERIFY(ReportServiceStartPending(1000));
	m_hWorkItemSemaphore = ::CreateSemaphore(NULL, 0, MAX_WORK_ITEMS, NULL);
	if (m_hWorkItemSemaphore.IsInvalid())
	{
		return FALSE;
	}

	XTLVERIFY(ReportServiceStartPending(1000));
	m_hStartServiceEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_hStartServiceEvent.IsInvalid())
	{
		return FALSE;
	}

	XTLVERIFY(ReportServiceStartPending(1000));
	// Create an initialization thread
	if (!CreateThread())
	{
		return FALSE;
	}


	XTLVERIFY(ReportServiceRunning());

	return TRUE;
}

DWORD
CNdasService::Impl::OnServiceStop()
{
	//////////////////////////////////////////////////////////////////////////
	// We should report the SCM that the service is stopping
	// Otherwise, the service will terminate the thread.
	// And we'll get ACCESS VIOLATION ERROR!
	//////////////////////////////////////////////////////////////////////////

	XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION,
		"Service is stopping...\n");

	ReportServiceStopPending(1000);

	m_serviceWindow.DestroyWindow();
	m_serviceWindow.Detach();
	// m_serviceWindow.WaitForDestroy();

	XTLVERIFY( ::SetEvent(m_hStopServiceEvent) );

	// Yield to other threads to finish themselves.
	::Sleep(0);

	//////////////////////////////////////////////////////////////////////////
	// Wait for the command processor thread to stop
	//////////////////////////////////////////////////////////////////////////

	XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION,
		"Waiting for worker threads to stop....\n");

	DWORD waitResult = WAIT_TIMEOUT;
	while (WAIT_OBJECT_0 != waitResult)
	{
		ReportServiceStopPending(3000);
		// yield to work threads for them to handle their terminations
		::Sleep(0);
		waitResult = ::WaitForSingleObject(m_wiCmdServer.GetThreadHandle(), 3000);
		XTLASSERT(WAIT_OBJECT_0 == waitResult || WAIT_TIMEOUT == waitResult);
		if (WAIT_OBJECT_0 != waitResult && WAIT_TIMEOUT != waitResult)
		{
			XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_ERROR,
				"Abnormal wait for command server occurred while stopping ndassvc....\n");
			break;
		}
		XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION,
			"Waiting for command processors to stop in 3 seconds....\n");
	}
	
	XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION,
		"Command processors stopped....\n");

	//////////////////////////////////////////////////////////////////////////
	// Wait for the work items to stop
	//////////////////////////////////////////////////////////////////////////

	DWORD finished = 0;
	while (finished < m_nWorkItems)
	{
		ReportServiceStopPending(1500);
		// yield to work threads for them to handle their terminations
		::Sleep(0);
		DWORD waitResult = ::WaitForSingleObject(m_hWorkItemSemaphore, 1000);
		if (waitResult == WAIT_OBJECT_0)
		{
			++finished;
			XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION,
				"(%d/%d) WorkItems stopped...\n", finished, m_nWorkItems);
		}
		XTLASSERT(WAIT_OBJECT_0 == waitResult || WAIT_TIMEOUT == waitResult);
		if (WAIT_OBJECT_0 != waitResult && WAIT_TIMEOUT != waitResult)
		{
			XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_ERROR,
				"Abnormal wait occurred for work items while stopping ndassvc....\n");
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// All threads and work items are done
	//////////////////////////////////////////////////////////////////////////

	XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION, 
		"All work items stopped....\n");

	XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION, 
		"Reporting to the SCM that the service is stopped....\n");

	m_cDeviceEventHandler.Uninitialize();

	COMVERIFY(m_pNdasDeviceRegistrar->Shutdown());

	COMVERIFY(m_pNdasLogicalUnitManager->Cleanup());

	ReportServiceStopped();

	return NO_ERROR;
}

DWORD
CNdasService::Impl::OnServiceShutdown()
{
	XTLTRACE2(NDASSVC_INIT, TRACE_LEVEL_INFORMATION, 
		"System is shutting down...\n");


	COMVERIFY(m_pNdasLogicalUnitManager->OnSystemShutdown());

	m_cDeviceEventHandler.OnShutdown();

	XTLVERIFY( ::LfsFiltCtlShutdown() );
	
	return NO_ERROR;
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasService
//
//////////////////////////////////////////////////////////////////////////

CNdasService* CNdasService::instance = NULL;

CNdasService*
CNdasService::Instance()
{
	XTLASSERT(CNdasService::instance);
	return CNdasService::instance;
}

CNdasService* 
CNdasService::CreateInstance()
{
	WSADATA wsaData;
	XTLVERIFY(SOCKET_ERROR != WSAStartup(MAKEWORD(2,2),&wsaData) );
	XTLASSERT(NULL == CNdasService::instance);
	CNdasService::instance = new CNdasService();
	return CNdasService::instance;
}

void 
CNdasService::DestroyInstance(CNdasService* pService)
{
	delete pService;
	CNdasService::instance = NULL;
	XTLVERIFY(SOCKET_ERROR != WSACleanup());
}

#pragma warning(disable: 4355)
CNdasService::CNdasService() : 
	m_pImpl(new CNdasService::Impl(*this))
{
}
#pragma warning(default: 4355)

CNdasService::~CNdasService()
{
}

BOOL
CNdasService::ServiceStart(DWORD dwArgc, LPTSTR* lpArgs)
{
	return m_pImpl->ServiceStart(dwArgc, lpArgs);
}

DWORD 
CNdasService::OnServiceStop()
{
	return m_pImpl->OnServiceStop();
}

DWORD 
CNdasService::OnServiceShutdown()
{
	return m_pImpl->OnServiceShutdown();
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

	LRESULT ret = m_pImpl->m_cDeviceEventHandler.DeviceEventProc(dwEventType, (LPARAM)lpEventData);
	return (BROADCAST_QUERY_DENY == ret) ? BROADCAST_QUERY_DENY : NO_ERROR;
}

DWORD 
CNdasService::OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData)
{
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

	LRESULT ret = m_pImpl->m_cPowerEventHandler.PowerEventProc(dwEventType, (LPARAM) lpEventData);
	return (BROADCAST_QUERY_DENY == ret) ? BROADCAST_QUERY_DENY : NO_ERROR;
}


//
// Service to ServiceImpl Forwarders
//
CNdasDeviceHeartbeatListener& CNdasService::GetDeviceHeartbeatListener() 
{
	return m_pImpl->m_cHeartbeatListener;
}
CNdasAutoRegister& CNdasService::GetAutoRegisterHandler() 
{
	return m_pImpl->m_cAutoRegProcessor;
}

HRESULT CNdasService::GetDeviceRegistrar(__deref_out INdasDeviceRegistrar** ppRegistrar) 
{
	CComPtr<INdasDeviceRegistrar> p = m_pImpl->m_pNdasDeviceRegistrar;
	*ppRegistrar = p.Detach();
	return S_OK;
}

HRESULT CNdasService::GetLogicalUnitManager(__deref_out INdasLogicalUnitManager** ppManager) 
{
	CComPtr<INdasLogicalUnitManager> p = m_pImpl->m_pNdasLogicalUnitManager;
	*ppManager = p.Detach();
	return S_OK;
}

CNdasEventMonitor& CNdasService::GetEventMonitor() 
{
	return m_pImpl->m_cEventMonitor;
}
CNdasEventPublisher& CNdasService::GetEventPublisher() 
{
	return m_pImpl->m_cEventPublisher;
}
CNdasServiceDeviceEventHandler& CNdasService::GetDeviceEventHandler() 
{
	return m_pImpl->m_cDeviceEventHandler;
}

const CNdasDeviceHeartbeatListener& CNdasService::GetDeviceHeartbeatListener() const 
{
	return m_pImpl->m_cHeartbeatListener;
}
const CNdasAutoRegister& CNdasService::GetAutoRegisterHandler() const 
{
	return m_pImpl->m_cAutoRegProcessor;
}
const CNdasEventMonitor& CNdasService::GetEventMonitor() const 
{
	return m_pImpl->m_cEventMonitor;
}
const CNdasEventPublisher& CNdasService::GetEventPublisher() const
{
	return m_pImpl->m_cEventPublisher;
}
const CNdasServiceDeviceEventHandler& CNdasService::GetDeviceEventHandler() const 
{
	return m_pImpl->m_cDeviceEventHandler;
}
const BOOL CNdasService::NdasPortExists() const 
{
	return m_pImpl->m_NdasPort;
}
const DWORD CNdasService::GetNdasPortNumber() const 
{
	return m_pImpl->m_NdasPortNumber;
}
