#include "stdafx.h"
#include <xtl/xtlautores.h>
#include <xtl/xtllock.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasautoregscope.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndassvcparam.h>

#include "ndascomobjectsimpl.hpp"
#include "ndassvcworkitem.h"
#include "ndasdevhb.h"
#include "ndasobjs.h"
#include "ndascfg.h"
#include "eventlog.h"

#include "ndasdevid.h"
#include "ndaseventpub.h"

#include "ndasautoreg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasautoreg.tmh"
#endif


LONG DbgLevelSvcAutoReg = DBG_LEVEL_SVC_AUTO_REG;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelSvcAutoReg) {							\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

CNdasAutoRegister::CNdasAutoRegister()
{
}

void 
CNdasAutoRegister::FinalRelease()
{
}

HRESULT
CNdasAutoRegister::AutoRegisterInitialize()
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
	const NDAS_DEVICE_ID &deviceId = Data->NdasDeviceId;

	ACCESS_MASK			 autoRegAccess = FALSE;
	DWORD				 autoPnpValue;

#if 0
	autoRegAccess = m_data.GetAutoRegAccess(deviceId);
#endif

	if (autoRegAccess == FALSE) {

		autoPnpValue = NdasServiceConfig::Get(nscAutoPnp);

		if (autoPnpValue == NDASSVC_AUTO_PNP_DENY) {

			return;
		}
	}

	// If already registered, do nothing

	CComPtr<INdasDevice> pNdasDevice;
	HRESULT hr = pGetNdasDevice(deviceId, &pNdasDevice);

	if (SUCCEEDED(hr)) {

		if (deviceId.Node[5] == 0xE5) {

			NDAS_DEVICE_ID ndasDeviceId2;

			pNdasDevice->get_NdasDeviceId(&ndasDeviceId2);

			NdasUiDbgCall( 4, "deviceId.Vid = %d, ndasDeviceId2->Vid = %d\n", deviceId.Vid, ndasDeviceId2.Vid );
		}

		// NdasDevice is already registered.
		// Ignore the registration

		return;
	}

	::NdasLogEventInformation( EVT_NDASSVC_INFO_AUTOREG_NDAS_DEVICE_FOUND,
							   NULL, 
							   0, 
							   sizeof(deviceId), 
							   NULL, 
							   &deviceId );
	
	(VOID) AddToQueue( deviceId, autoRegAccess );
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

VOID CNdasAutoRegister::ProcessRegister (const NDAS_DEVICE_ID &DeviceId, ACCESS_MASK AutoRegAccess)
{
	HRESULT hr;
	DWORD	autoPnpValue;

	if (AutoRegAccess == FALSE) {

		autoPnpValue = NdasServiceConfig::Get(nscAutoPnp);

		if (autoPnpValue == NDASSVC_AUTO_PNP_DENY) {

			return;
		}
	}

	NdasUiDbgCall( 2, "%s\n", CNdasDeviceId(DeviceId).ToStringA() );

	CComPtr<INdasDeviceRegistrar> pRegistrar;

	COMVERIFY( hr = pGetNdasDeviceRegistrar(&pRegistrar) );
	
	if (FAILED(hr)) {

		return;
	}

	CComPtr<INdasDevice> pExistingDevice;

	hr = pRegistrar->get_NdasDevice( &const_cast<NDAS_DEVICE_ID&>(DeviceId), &pExistingDevice );
	
	if (SUCCEEDED(hr)) {

		return;
	}

	DWORD dwRegFlags;
	
	if (AutoRegAccess) {

		dwRegFlags = NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED | NDAS_DEVICE_REG_FLAG_VOLATILE;
	
	} else {

		dwRegFlags = NDAS_DEVICE_REG_FLAG_HIDDEN | NDAS_DEVICE_REG_FLAG_VOLATILE;
	}
	
	// Register function returns the locked pointer

	CComPtr<INdasDevice> pNdasDevice;
	
	hr = pRegistrar->DeviceRegister( 0, DeviceId, dwRegFlags, NULL, CComBSTR(L"NDAS Device"), AutoRegAccess, NULL, &pNdasDevice );

	if (FAILED(hr)) {

		ATLASSERT(FALSE);

		NdasUiDbgCall( 1, "Register failed, hr=0x%X\n", hr );
		return;
	}

	DWORD slotNo;

	COMVERIFY( pNdasDevice->get_SlotNo(&slotNo) );

	CComBSTR ndasDeviceName(MAX_NDAS_DEVICE_NAME_LEN + 1);

	COMVERIFY( StringCchPrintfW( ndasDeviceName, MAX_NDAS_DEVICE_NAME_LEN + 1, L"NDAS Device A%04d", slotNo) );
	COMVERIFY( pNdasDevice->put_Name(ndasDeviceName) );

	hr = pNdasDevice->put_Enabled( AutoRegAccess ? TRUE : FALSE );

	ATLASSERT( hr == S_FALSE );

	if (FAILED(hr)) {

		ATLASSERT(FALSE);

		NdasUiDbgCall( 1, "Enable failed, hr=0x%X\n", hr );

		return;
	}

	TCHAR ndasStringId[NDAS_DEVICE_STRING_ID_LEN+1];

	NdasIdDeviceToStringEx( &DeviceId, ndasStringId, NULL, NULL, NULL );

	NdasUiDbgCall( 6, _T("strDeviceId = %s\n"), ndasStringId );

	CNdasEventPublisher &epub = pGetNdasEventPublisher();

	ZeroMemory( &ndasStringId[5], sizeof(TCHAR)*(NDAS_DEVICE_STRING_ID_LEN+1-5) );

	epub.NdasAutoPnp(ndasStringId);

	return;
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
