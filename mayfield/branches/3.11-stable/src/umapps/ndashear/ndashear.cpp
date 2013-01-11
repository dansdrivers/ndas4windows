#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <xdebug.h>
#include <winsock2.h>
#include <ndas/ndashear.h>
#include "ndasheartbeat.h"
#include <new>

[event_receiver(native)]
struct INdasDeviceHeartbeatHandler
{
	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData) = 0;
};

class CNdasDeviceHeartbeatHandler : 
	public INdasDeviceHeartbeatHandler
{
	INdasDeviceHeartbeat* m_pHeartbeater;
public:
	CNdasDeviceHeartbeatHandler(INdasDeviceHeartbeat* pHeartbeater) :
		m_pHeartbeater(pHeartbeater)
	{
		__hook(
			&INdasDeviceHeartbeat::OnHeartbeat, m_pHeartbeater,
			&INdasDeviceHeartbeatHandler::OnHeartbeat, this);
	}
	virtual ~CNdasDeviceHeartbeatHandler()
	{
		__unhook(
			m_pHeartbeater);
	}
	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData) = 0;
};


class CNdasDeviceHeartbeatProcHandler : 
	public CNdasDeviceHeartbeatHandler
{
	NDASHEARPROC m_proc;
	LPVOID m_procContext;
public:
	CNdasDeviceHeartbeatProcHandler(
		INdasDeviceHeartbeat* pHeartbeater,
		NDASHEARPROC proc, 
		LPVOID lpContext) :
		CNdasDeviceHeartbeatHandler(pHeartbeater),
		m_proc(proc),
		m_procContext(lpContext)
	{
	}
	virtual ~CNdasDeviceHeartbeatProcHandler()
	{
	}
	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData)
	{
		m_proc(&eventData, m_procContext);
	}
};

namespace
{

typedef struct _SHARED_DATA {
	LONG RefCount;
	CNdasDeviceHeartbeatListener* listener;
} SHARED_DATA, *PSHARED_DATA;

SHARED_DATA SharedData = {0};
PSHARED_DATA pSharedData = &SharedData;

typedef struct _NDASHEAR_HANDLE
{
	DWORD Signature; // 0xAB3FEF12
	CNdasDeviceHeartbeatProcHandler* pHandler;
} NDASHEAR_HANDLE, *PNDASHEAR_HANDLE;

const DWORD NDASHEAR_HANDLE_SIGNATURE = 0xAB3FEF12;

CNdasDeviceHeartbeatListener*
GetListenerInstance()
{
	CNdasDeviceHeartbeatListener* pListener = pSharedData->listener;
	if (NULL != pListener)
	{
		::InterlockedIncrement(&pSharedData->RefCount);
		return pListener;
	}

	::InterlockedIncrement(&pSharedData->RefCount);

	pListener = new CNdasDeviceHeartbeatListener();
	if (NULL == pListener)
	{
		::SetLastError(ERROR_OUTOFMEMORY);
		::InterlockedDecrement(&pSharedData->RefCount);
		return NULL;
	}

	BOOL fSuccess = pListener->Initialize();
	if (!fSuccess) 
	{
		DBGPRT_ERR(_T("Failed to init listener: "));
		::InterlockedDecrement(&pSharedData->RefCount);
		return NULL;
	}

	if (!pListener->Run()) 
	{
		DBGPRT_ERR(_T("Failed to run listener: "));
		::InterlockedDecrement(&pSharedData->RefCount);
		return NULL;
	}

	// okay, we've got pListener running
	pSharedData->listener = pListener;
	return pListener;
}

void
ReleaseListener()
{
	::InterlockedDecrement(&pSharedData->RefCount);
	if (0 == pSharedData->RefCount)
	{
		pSharedData->listener->Stop(TRUE);
		delete pSharedData->listener;
	}
}

HANDLE 
CreateNdasHearHandle(CNdasDeviceHeartbeatProcHandler* p)
{
	NDASHEAR_HANDLE* pHandle = new NDASHEAR_HANDLE;
	if (NULL == pHandle)
	{
		::SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}
	pHandle->Signature = NDASHEAR_HANDLE_SIGNATURE;
	pHandle->pHandler = p;
	return (HANDLE) pHandle;
}

void
DestroyNdasHearHandle(HANDLE h)
{
	PNDASHEAR_HANDLE pHandle = reinterpret_cast<PNDASHEAR_HANDLE>(h);
	pHandle->Signature = 0; // prevent destroying more than once
	delete pHandle->pHandler;
	delete pHandle;
}

BOOL
IsBadNdasHearHandle(HANDLE h)
{
	PNDASHEAR_HANDLE pHandle = reinterpret_cast<PNDASHEAR_HANDLE>(h);
	return 
		::IsBadWritePtr(pHandle, sizeof(NDASHEAR_HANDLE)) ||
		(NDASHEAR_HANDLE_SIGNATURE != pHandle->Signature) ||
		::IsBadWritePtr(pHandle->pHandler, sizeof(CNdasDeviceHeartbeatProcHandler));
}

} // namespace


BOOL 
APIENTRY 
DllMain(
	HANDLE hModule, 
	DWORD  dwReason, 
	LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatInitialize()
{
	WSADATA wsaData;
	int ret = ::WSAStartup(MAKEWORD(2,2), &wsaData);
	if (0 != ret)
	{
		_ASSERTE(WSAEFAULT != ret);
 		::SetLastError(ret);
		return FALSE;
	}
	return TRUE;
}

NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatUninitialize()
{
	int ret = ::WSACleanup();
	if (0 != ret)
	{
		return FALSE;
	}
	return TRUE;
}

NDASHEAR_LINKAGE
HANDLE
NDASHEARAPI
NdasHeartbeatRegisterNotification(NDASHEARPROC proc, LPVOID lpContext)
{
	if (::IsBadCodePtr((FARPROC)proc))
	{
		::SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	CNdasDeviceHeartbeatListener* pListener = GetListenerInstance();
	if (NULL == pListener)
	{
		return NULL;
	}

	CNdasDeviceHeartbeatProcHandler* pHandler = 
		new CNdasDeviceHeartbeatProcHandler(pListener, proc, lpContext);
	if (NULL == pHandler)
	{
		ReleaseListener();
		::SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	return CreateNdasHearHandle(pHandler);
}

NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatUnregisterNotification(HANDLE h)
{
	if (IsBadNdasHearHandle(h))
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	DestroyNdasHearHandle(h);
	ReleaseListener();

	return TRUE;
}
