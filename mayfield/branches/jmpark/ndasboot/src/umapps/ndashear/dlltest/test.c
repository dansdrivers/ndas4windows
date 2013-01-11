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
		pHeartbeat->DeviceAddress.Node[0],
		pHeartbeat->DeviceAddress.Node[1],
		pHeartbeat->DeviceAddress.Node[2], 
		pHeartbeat->DeviceAddress.Node[3],
		pHeartbeat->DeviceAddress.Node[4], 
		pHeartbeat->DeviceAddress.Node[5],
		pHeartbeat->LocalAddress.Node[0],
		pHeartbeat->LocalAddress.Node[1],
		pHeartbeat->LocalAddress.Node[2], 
		pHeartbeat->LocalAddress.Node[3],
		pHeartbeat->LocalAddress.Node[4], 
		pHeartbeat->LocalAddress.Node[5],
		pHeartbeat->Type, 
		pHeartbeat->Version);
}

VOID
NdasHeartbeatCallback2(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, 
	LPVOID lpContext)
{
	_tprintf(
		_T("2: [%08X] %02X:%02X:%02X:%02X:%02X:%02X %02X:%02X:%02X:%02X:%02X:%02X, type:%d, ver:%d\n"),
		pHeartbeat->Timestamp,
		pHeartbeat->DeviceAddress.Node[0], 
		pHeartbeat->DeviceAddress.Node[1],
		pHeartbeat->DeviceAddress.Node[2], 
		pHeartbeat->DeviceAddress.Node[3],
		pHeartbeat->DeviceAddress.Node[4], 
		pHeartbeat->DeviceAddress.Node[5],
		pHeartbeat->LocalAddress.Node[0], 
		pHeartbeat->LocalAddress.Node[1],
		pHeartbeat->LocalAddress.Node[2], 
		pHeartbeat->LocalAddress.Node[3],
		pHeartbeat->LocalAddress.Node[4],
		pHeartbeat->LocalAddress.Node[5],
		pHeartbeat->Type, 
		pHeartbeat->Version);
}

int __cdecl wmain()
{
	HANDLE h, h2;
	BOOL fSuccess;

	if (NdasHeartbeatInitialize())
	{

		_tprintf(_T("Registering h1...\n"));
		h = NdasHeartbeatRegisterNotification(NdasHeartbeatCallback, NULL);
		if (NULL == h)
		{
			_tprintf(_T("Error %d\n"), GetLastError());
		}

		_tprintf(_T("Registering h2...\n"));
		h2 = NdasHeartbeatRegisterNotification(NdasHeartbeatCallback2, NULL);
		if (NULL == h2)
		{
			_tprintf(_T("Error h2 %d\n"), GetLastError());
		}

		_tprintf(_T("Waiting...\n"));

		Sleep(3000);

		if (h)
		{
			_tprintf(_T("Deregistering h..\n"));
			if (!NdasHeartbeatUnregisterNotification(h))
			{
				_tprintf(_T("Deregister error %d\n"), GetLastError());
			}
		}

		Sleep(3000);

		if (h2)
		{
			_tprintf(_T("Deregistering h2...\n"));
			if (!NdasHeartbeatUnregisterNotification(h2))
			{
				_tprintf(_T("Deregister error h2 %d\n"), GetLastError());
			}
		}

		_tprintf(_T("Cleaning up...\n"));
		if (!NdasHeartbeatUninitialize())
		{
			_tprintf(_T("Uninit error %d\n"), GetLastError());
		}
	}
	else
	{
		_tprintf(_T("Init Error %d\n"), GetLastError());
	}
	
	return 0;
}
