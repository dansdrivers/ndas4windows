#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include "xdebug.h"
#include <winsock2.h>
#include <ndas/ndashear.h>
#include "ndasheartbeat.h"

struct {
	ULONG ref;
	CRITICAL_SECTION cs;
	CNdasDeviceHeartbeatListener* listener;
} SharedData;

[event_receiver(native)]
struct INdasDeviceHeartbeatHandler
{
	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData) = 0;
};

class CNdasDeviceHeartbeatHandler : public INdasDeviceHeartbeatHandler
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
		__unhook(m_pHeartbeater);
	}

	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData) = 0;
};


class CNdasDeviceHeartbeatProcHandler : public CNdasDeviceHeartbeatHandler
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

class CAutoLock
{
	LPCRITICAL_SECTION m_pcs;
public:
	explicit CAutoLock(LPCRITICAL_SECTION pcs) : m_pcs(pcs)
	{
		::EnterCriticalSection(m_pcs);
	}
	~CAutoLock()
	{
		::LeaveCriticalSection(m_pcs);
	}
};

VOID 
InitSharedData()
{
	::ZeroMemory(&SharedData, sizeof(SharedData));
	::InitializeCriticalSection(&SharedData.cs);
}

VOID 
CleanupSharedData()
{
	::DeleteCriticalSection(&SharedData.cs);
}

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
		InitSharedData();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		CleanupSharedData();
		break;
	}
	return TRUE;
}

typedef struct _NDASHEAR_HANDLE
{
	DWORD Signature; // 0xAB3FEF12
	CNdasDeviceHeartbeatProcHandler* pHandler;
} NDASHEAR_HANDLE, *PNDASHEAR_HANDLE;

const DWORD NDASHEAR_HANDLE_SIGNATURE = 0xAB3FEF12;


static
CNdasDeviceHeartbeatListener*
GetListenerInstance();

static
VOID
ReleaseListener();

static
HANDLE 
CreateNdasHearHandle(CNdasDeviceHeartbeatProcHandler* p);

static
VOID
DestroyNdasHearHandle(HANDLE h);

static
BOOL
IsBadNdasHearHandle(HANDLE h);


NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatInitialize()
{
	WSADATA wsaData;
	int ret = ::WSAStartup(MAKEWORD(2,2), &wsaData);
	if (ERROR_SUCCESS != ret)
	{
		_ASSERTE(WSAEFAULT != ret);
		::SetLastError(ret);
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

	CAutoLock autolock(&SharedData.cs);

	CNdasDeviceHeartbeatListener* pListener = 
		::GetListenerInstance();

	CNdasDeviceHeartbeatProcHandler* pHandler = 
		new CNdasDeviceHeartbeatProcHandler(pListener, proc, lpContext);

	if (NULL == pHandler)
	{
		::ReleaseListener();
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
	if (::IsBadNdasHearHandle(h))
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CAutoLock autolock(&SharedData.cs);
	::DestroyNdasHearHandle(h);
	::ReleaseListener();

	return TRUE;
}

NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatUninitialize()
{
	int ret = ::WSACleanup();
	if (ERROR_SUCCESS != ret)
	{
		return FALSE;
	}
	return TRUE;
}

CNdasDeviceHeartbeatListener*
GetListenerInstance()
{
	CNdasDeviceHeartbeatListener* pListener = SharedData.listener;
	if (NULL != pListener)
	{
		::InterlockedIncrement((LPLONG)&SharedData.ref);
		return pListener;
	}

	pListener = new CNdasDeviceHeartbeatListener();
	if (NULL == pListener)
	{
		::SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	BOOL fSuccess = pListener->Initialize();
	if (!fSuccess) {
		DBGPRT_ERR(_T("Failed to init listener: "));
		return NULL;
	}

	if (!pListener->Run()) {
		DBGPRT_ERR(_T("Failed to run listener: "));
		return NULL;
	}

	// okay, we've got pListener running
	SharedData.listener = pListener;
	::InterlockedIncrement((LPLONG)&SharedData.ref);
	return pListener;
}

VOID
ReleaseListener()
{
	::InterlockedDecrement((LPLONG)&SharedData.ref);
	if (0 == SharedData.ref)
	{
		SharedData.listener->Stop(TRUE);
		delete SharedData.listener;
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

VOID
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

