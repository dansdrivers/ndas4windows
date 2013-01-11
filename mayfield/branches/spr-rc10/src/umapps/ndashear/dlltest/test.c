#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <ndas/ndashear.h>

VOID 
NdasHeartbeatCallback(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, 
	LPVOID lpContext)
{
	_tprintf(
		_T("1: %02X:%02X:%02X:%02X:%02X:%02X %02X:%02X:%02X:%02X:%02X:%02X %d.%d\n"),
		pHeartbeat->deviceAddress[0], pHeartbeat->deviceAddress[1],
		pHeartbeat->deviceAddress[2], pHeartbeat->deviceAddress[3],
		pHeartbeat->deviceAddress[4], pHeartbeat->deviceAddress[5],
		pHeartbeat->localAddress[0], pHeartbeat->localAddress[1],
		pHeartbeat->localAddress[2], pHeartbeat->localAddress[3],
		pHeartbeat->localAddress[4], pHeartbeat->localAddress[5],
		pHeartbeat->type, 
		pHeartbeat->version);
}

VOID
NdasHeartbeatCallback2(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, 
	LPVOID lpContext)
{
	_tprintf(
		_T("2: [%08X] %02X:%02X:%02X:%02X:%02X:%02X %02X:%02X:%02X:%02X:%02X:%02X, type:%d, ver:%d\n"),
		pHeartbeat->timestamp,
		pHeartbeat->deviceAddress[0], pHeartbeat->deviceAddress[1],
		pHeartbeat->deviceAddress[2], pHeartbeat->deviceAddress[3],
		pHeartbeat->deviceAddress[4], pHeartbeat->deviceAddress[5],
		pHeartbeat->localAddress[0], pHeartbeat->localAddress[1],
		pHeartbeat->localAddress[2], pHeartbeat->localAddress[3],
		pHeartbeat->localAddress[4], pHeartbeat->localAddress[5],
		pHeartbeat->type, 
		pHeartbeat->version);
}

int __cdecl wmain()
{
	HANDLE h, h2;
	BOOL fSuccess;

	if (!NdasHeartbeatInitialize())
	{
		_tprintf(_T("Init Error %d\n"), GetLastError());
	}

	_tprintf(_T("Registering...\n"));	

	if (NULL == (h = NdasHeartbeatRegister(NdasHeartbeatCallback, NULL)))
	{
		_tprintf(_T("Error %d\n"), GetLastError());
	}
	
	if (NULL == (h2 = NdasHeartbeatRegister(NdasHeartbeatCallback2, NULL)))
	{
		_tprintf(_T("Error h2 %d\n"), GetLastError());
	}

	_tprintf(_T("Waiting...\n"));

	Sleep(3000);

	if (h && !NdasHeartbeatUnregister(h))
	{
		_tprintf(_T("Unregister error %d\n"), GetLastError());
	}

	Sleep(3000);

	if (h2 && !NdasHeartbeatUnregister(h2))
	{
		_tprintf(_T("Unregister error h2 %d\n"), GetLastError());
	}

	_tprintf(_T("Uninit...\n"));

	if (!NdasHeartbeatUninitialize())
	{
		_tprintf(_T("Uninit error %d\n"), GetLastError());
	}
	
	return 0;
}
