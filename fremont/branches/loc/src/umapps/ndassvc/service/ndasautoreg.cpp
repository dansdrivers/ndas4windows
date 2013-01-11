#include "stdafx.h"
#include <xtl/xtlautores.h>
#include <xtl/xtllock.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasautoregscope.h>
#include <ndas/ndasmsg.h>

#include "ndascomobjectsimpl.hpp"
#include "ndassvcworkitem.h"
#include "ndasdevhb.h"
#include "ndasobjs.h"
#include "ndascfg.h"
#include "eventlog.h"

#include "ndasautoreg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasautoreg.tmh"
#endif

CNdasAutoRegister::CNdasAutoRegister()
{
}

void 
CNdasAutoRegister::FinalRelease()
{
}

HRESULT
CNdasAutoRegister::Initialize()
{
	if (m_hSemQueue.IsInvalid())
	{
		m_hSemQueue = ::CreateSemaphore(NULL, 0, 255, NULL);
		if (m_hSemQueue.IsInvalid())
		{
			HRESULT hr = AtlHresultFromLastError();
			return hr;
		}
	}
	(void) m_data.LoadFromSystem();
	return S_OK;
}

DWORD 
CNdasAutoRegister::ThreadStart(HANDLE hStopEvent)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

	XTLASSERT(!m_hSemQueue.IsInvalid() && "Don't forget to call initialize().");

	// Queue Semaphore, Terminating Thread, Pipe Instances(MAX...)
	HANDLE hWaitHandles[2] = { hStopEvent, m_hSemQueue };

	CNdasDeviceHeartbeatListener& listener = pGetNdasDeviceHeartbeatListener();

	COMVERIFY(listener.Advise(this));

	do 
	{
		DWORD waitResult = ::WaitForMultipleObjects(
			RTL_NUMBER_OF(hWaitHandles), 
			hWaitHandles, 
			FALSE, INFINITE);

		if (WAIT_OBJECT_0 == waitResult) 
		{
			break;
		} 
		else if (WAIT_OBJECT_0 + 1 == waitResult) 
		{
			while (TRUE) 
			{
				m_queueLock.LockInstance();
				if (m_queue.empty()) 
				{
					m_queueLock.UnlockInstance();
					break;
				}
				QUEUE_ENTRY entry = m_queue.front();
				m_queue.pop();
				m_queueLock.UnlockInstance();
				(VOID) ProcessRegister(entry.deviceID, entry.access);
			}
		} 
		else 
		{
			XTLASSERT(FALSE);
			// ERROR
		}

	} while (TRUE);

	COMVERIFY(listener.Unadvise(this));

	return 0;
}

void 
CNdasAutoRegister::NdasHeartbeatReceived(const NDAS_DEVICE_HEARTBEAT_DATA* Data)
{
	const NDAS_DEVICE_ID& deviceId = Data->NdasDeviceId;
	
	ACCESS_MASK autoRegAccess = m_data.GetAutoRegAccess(deviceId);
	if (!autoRegAccess) 
	{
		return;
	}

	//
	// If already registered, do nothing
	//
	CComPtr<INdasDevice> pNdasDevice;
	HRESULT hr = pGetNdasDevice(deviceId, &pNdasDevice);
	if (SUCCEEDED(hr))
	{
		//
		// NdasDevice is already registered.
		// Ignore the registration
		//
		return;
	}

	::NdasLogEventInformation(
		EVT_NDASSVC_INFO_AUTOREG_NDAS_DEVICE_FOUND,
		NULL, 
		0, 
		sizeof(deviceId), 
		NULL, 
		&deviceId);
	
	(VOID) AddToQueue(deviceId, autoRegAccess);
}

BOOL
CNdasAutoRegister::AddToQueue(
	CONST NDAS_DEVICE_ID& deviceId,
	ACCESS_MASK autoRegAccess)
{
	QUEUE_ENTRY entry = { deviceId, autoRegAccess };

	m_queueLock.LockInstance();
	m_queue.push(entry);
	m_queueLock.UnlockInstance();

	BOOL success = ::ReleaseSemaphore(m_hSemQueue, 1, NULL);
	if (!success) {
		// Queue Full
		return FALSE;
	}
	return TRUE;
}

BOOL
CNdasAutoRegister::ProcessRegister(
	const NDAS_DEVICE_ID& deviceId, 
	ACCESS_MASK autoRegAccess)
{
	HRESULT hr;

	CComPtr<INdasDeviceRegistrar> pRegistrar;
	COMVERIFY(hr = pGetNdasDeviceRegistrar(&pRegistrar));
	if (FAILED(hr))
	{
		return FALSE;
	}

	CComPtr<INdasDevice> pExistingDevice;
	hr = pRegistrar->get_NdasDevice(&const_cast<NDAS_DEVICE_ID&>(deviceId), &pExistingDevice);
	if (SUCCEEDED(hr)) 
	{
		return TRUE;
	}

	DWORD dwRegFlags = 
		NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED |
		NDAS_DEVICE_REG_FLAG_VOLATILE;
	
	//
	// Register function returns the locked pointer
	//
	CComPtr<INdasDevice> pNdasDevice;
	hr = pRegistrar->Register(
		0, 
		deviceId, 
		dwRegFlags, 
		NULL, 
		CComBSTR(L"NDAS Device"),
		autoRegAccess,
		NULL,
		&pNdasDevice);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_AUTOREG, TRACE_LEVEL_ERROR,
			"Register failed, hr=0x%X\n", hr);
		return FALSE;
	}

	DWORD slotNo;
	COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));

	CComBSTR ndasDeviceName(MAX_NDAS_DEVICE_NAME_LEN + 1);
	COMVERIFY(StringCchPrintfW(
		ndasDeviceName, MAX_NDAS_DEVICE_NAME_LEN + 1,
		L"NDAS Device A%04d", slotNo));
	COMVERIFY(pNdasDevice->put_Name(ndasDeviceName));

	hr = pNdasDevice->put_Enabled(TRUE);
	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_AUTOREG, TRACE_LEVEL_ERROR,
			"Enable failed, hr=0x%X\n", hr);
	}

	return TRUE;
}

//HRESULT 
//CNdasAutoRegister::Execute(DWORD_PTR dwParam, HANDLE hObject)
//{
//	CWorkerThread workerThread;
//	workerThread.Initialize();
//	workerThread.AddHandle(m_hSemQueue, this, 0);
//	workerThread.Shutdown();
//}
//
//HRESULT 
//CNdasAutoRegister::CloseHandle(HANDLE hHandle)
//{
//
//}
