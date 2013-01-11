#include "stdafx.h"
#include "ndas/ndasmsg.h"
#include "ndasautoreg.h"
#include "ndasobjs.h"
#include "ndasdev.h"
#include "ndasdevreg.h"
#include "ndasdevhb.h"
#include "ndascfg.h"
#include "eventlog.h"
#include "xdebug.h"

CNdasAutoRegister::CNdasAutoRegister() :
	m_hSemQueue(NULL),
	ximeta::CTask(_T("NdasAutoReg"))
{
}

CNdasAutoRegister::~CNdasAutoRegister()
{
	if (NULL != m_hSemQueue) {
		::CloseHandle(m_hSemQueue);
	}
}

BOOL
CNdasAutoRegister::Initialize()
{

	if (NULL == m_hSemQueue) {
		m_hSemQueue = ::CreateSemaphore(
			NULL, 
			0, 
			255, 
			NULL);

		if (NULL == m_hSemQueue) {
			return FALSE;
		}
	}

	(VOID) m_data.LoadFromSystem();

	return ximeta::CTask::Initialize();
}

DWORD 
CNdasAutoRegister::OnTaskStart()
{
	_ASSERTE(NULL != m_hSemQueue && "Don't forget to call initialize().");

	// Queue Semaphore, Terminating Thread, Pipe Instances(MAX...)
	HANDLE hWaitHandles[2];
	hWaitHandles[0] = m_hTaskTerminateEvent;
	hWaitHandles[1] = m_hSemQueue;

	CNdasDeviceHeartbeatListener* pListener = 
		pGetNdasDeviceHeartbeatListner();

	pListener->Attach(this);

	do {

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			2, hWaitHandles, 
			FALSE, INFINITE);

		if (WAIT_OBJECT_0 == dwWaitResult) {

			break;

		} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {

			while (TRUE) {
				m_queueLock.Lock();
				if (m_queue.empty()) {
					m_queueLock.Unlock();
					break;
				}
				QUEUE_ENTRY entry = m_queue.front();
				m_queue.pop();
				m_queueLock.Unlock();
				(VOID) ProcessRegister(entry.deviceID, entry.access);
			}


		} else {

			_ASSERTE(FALSE);
			// ERROR
		}

	} while (TRUE);

	pListener->Detach(this);

	return 0;
}

VOID 
CNdasAutoRegister::Update(ximeta::CSubject* pChangedSubject)
{

	CNdasDeviceHeartbeatListener* pListener = 
		pGetNdasDeviceHeartbeatListner();

	//
	// Ignore other than subscribed heartbeat listener
	//

	if (pListener == pChangedSubject) {

		NDAS_DEVICE_HEARTBEAT_DATA hbData;

		pListener->GetHeartbeatData(&hbData);

		NDAS_DEVICE_ID deviceId = {0};
		::CopyMemory(&deviceId, hbData.remoteAddr.Node, sizeof(deviceId));

		ACCESS_MASK autoRegAccess = m_data.GetAutoRegAccess(deviceId);
		if (!autoRegAccess) {
			return;
		}

		//
		// If already registered, do nothing
		//
		CNdasDevice* pDevice = pGetNdasDevice(deviceId);
		if (NULL != pDevice) {
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
	CONST NDAS_DEVICE_ID& deviceId, 
	ACCESS_MASK autoRegAccess)
{
	CNdasDeviceRegistrar* pRegistrar = pGetNdasDeviceRegistrar();
	CNdasDevice* pDevice = pRegistrar->Find(deviceId);
	if (NULL != pDevice) {
		return TRUE;
	}

	pDevice = pRegistrar->Register(deviceId);
	if (NULL == pDevice) {
		return FALSE;
	}

	BOOL fSuccess = pDevice->SetAutoRegistered(TRUE, autoRegAccess);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("SetAutoRegistered to %d failed: "), autoRegAccess);
	}

	fSuccess = pDevice->Enable(TRUE);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("Enable failed: "));
	}

	TCHAR szName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	HRESULT hr = ::StringCchPrintf(szName, MAX_NDAS_DEVICE_NAME_LEN + 1,
		_T("NDAS Device A%04d"), pDevice->GetSlotNo());
	_ASSERTE(SUCCEEDED(hr));
	fSuccess = pDevice->SetName(szName);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("Set Name failed: "));
	}

	return TRUE;
}
