#include "stdafx.h"
#include <ndas/ndasmsg.h>
#include "ndasautoreg.h"
#include "ndasobjs.h"
#include "ndasdev.h"
#include "ndasdevreg.h"
#include "ndasdevhb.h"
#include "ndascfg.h"
#include "eventlog.h"
#include "xdebug.h"

CNdasAutoRegister::
CNdasAutoRegister(CNdasService& service) :
	m_service(service)
{
}

CNdasAutoRegister::~CNdasAutoRegister()
{
}

bool
CNdasAutoRegister::Initialize()
{
	if (m_hSemQueue.IsInvalid())
	{
		m_hSemQueue = ::CreateSemaphore(NULL, 0, 255, NULL);
		if (m_hSemQueue.IsInvalid())
		{
			return false;
		}
	}
	(void) m_data.LoadFromSystem();
	return true;
}

DWORD 
CNdasAutoRegister::ThreadStart(HANDLE hStopEvent)
{
	XTLASSERT(!m_hSemQueue.IsInvalid() && "Don't forget to call initialize().");

	// Queue Semaphore, Terminating Thread, Pipe Instances(MAX...)
	HANDLE hWaitHandles[2] = { hStopEvent, m_hSemQueue };

	CNdasDeviceHeartbeatListener& listener = m_service.GetDeviceHeartbeatListener();

	listener.Attach(this);

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
				m_queueLock.Lock();
				if (m_queue.empty()) 
				{
					m_queueLock.Unlock();
					break;
				}
				QUEUE_ENTRY entry = m_queue.front();
				m_queue.pop();
				m_queueLock.Unlock();
				(VOID) ProcessRegister(entry.deviceID, entry.access);
			}
		} 
		else 
		{
			XTLASSERT(FALSE);
			// ERROR
		}

	} while (TRUE);

	listener.Detach(this);

	return 0;
}

VOID 
CNdasAutoRegister::Update(ximeta::CSubject* pChangedSubject)
{
	const CNdasDeviceHeartbeatListener& listener = m_service.GetDeviceHeartbeatListener();
	const CNdasDeviceHeartbeatListener* pListener = &listener;

	//
	// Ignore other than subscribed heartbeat listener
	//

	if (pListener == pChangedSubject) {

		NDAS_DEVICE_HEARTBEAT_DATA hbData;

		pListener->GetHeartbeatData(&hbData);

		NDAS_DEVICE_ID deviceId = {0};
		::CopyMemory(&deviceId, hbData.remoteAddr.Node, sizeof(deviceId));

		ACCESS_MASK autoRegAccess = m_data.GetAutoRegAccess(deviceId);
		if (!autoRegAccess) 
		{
			return;
		}

		//
		// If already registered, do nothing
		//
		CNdasDevicePtr pDevice = pGetNdasDevice(deviceId);
		if (0 != pDevice.get()) 
		{
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
}

BOOL
CNdasAutoRegister::AddToQueue(
	CONST NDAS_DEVICE_ID& deviceId,
	ACCESS_MASK autoRegAccess)
{
	QUEUE_ENTRY entry = { deviceId, autoRegAccess };

	m_queueLock.Lock();
	m_queue.push(entry);
	m_queueLock.Unlock();

	BOOL fSuccess = ::ReleaseSemaphore(m_hSemQueue, 1, NULL);
	if (!fSuccess) {
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
	CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

	CNdasDevicePtr pExistingDevice = registrar.Find(deviceId);
	if (CNdasDeviceNullPtr != pExistingDevice) 
	{
		return TRUE;
	}

	DWORD dwRegFlags = 
		NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED |
		NDAS_DEVICE_REG_FLAG_VOLATILE;

	CNdasDevicePtr pDevice = registrar.Register(0, deviceId, dwRegFlags, NULL);
	if (0 == pDevice.get()) {
		return FALSE;
	}

	pDevice->SetGrantedAccess(autoRegAccess);

	BOOL fSuccess = pDevice->Enable(TRUE);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("Enable failed: "));
	}

	TCHAR szName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	HRESULT hr = ::StringCchPrintf(szName, MAX_NDAS_DEVICE_NAME_LEN + 1,
		_T("NDAS Device A%04d"), pDevice->GetSlotNo());
	XTLASSERT(SUCCEEDED(hr));

	pDevice->SetName(szName);

	return TRUE;
}
