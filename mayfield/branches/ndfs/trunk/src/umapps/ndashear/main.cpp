#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <crtdbg.h>
#include "ndasheartbeat.h"
#include <xdebug.h>

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

	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData)
	{
		printf("%02X:%02X:%02X:%02X:%02X:%02X : %d %d\n", 
			eventData.DeviceAddress.Node[0],
			eventData.DeviceAddress.Node[1],
			eventData.DeviceAddress.Node[2],
			eventData.DeviceAddress.Node[3],
			eventData.DeviceAddress.Node[4],
			eventData.DeviceAddress.Node[5],
			eventData.Type, 
			eventData.Version);
	}
};

class CMyClass : public CNdasDeviceHeartbeatHandler
{
public:
	CMyClass(INdasDeviceHeartbeat* pHeartbeater) :
		CNdasDeviceHeartbeatHandler(pHeartbeater)
	{
	}

	virtual void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData)
	{
		printf("MY: %02X:%02X:%02X:%02X:%02X:%02X : %d %d\n", 
			eventData.DeviceAddress.Node[0],
			eventData.DeviceAddress.Node[1],
			eventData.DeviceAddress.Node[2],
			eventData.DeviceAddress.Node[3],
			eventData.DeviceAddress.Node[4],
			eventData.DeviceAddress.Node[5],
			eventData.Type, 
			eventData.Version);
	}
};

int __cdecl wmain()
{
	WSADATA wsaData;
	if (0 != ::WSAStartup(MAKEWORD(2,2), &wsaData))
	{
		_tprintf(_T("Failed to initialize socket: %d\n"), ::GetLastError());
	}

	CNdasDeviceHeartbeatListener listener;
	BOOL fSuccess = listener.Initialize();
	if (!fSuccess) 
	{
		_tprintf(_T("Failed to init listener : %d\n"), ::GetLastError());
		return 1;
	}

	if (!listener.Run()) 
	{
		_tprintf(_T("Failed to run listener: %d\n"), ::GetLastError());
		return 1;
	}

	CNdasDeviceHeartbeatHandler* subscriber = new CMyClass(&listener);

	::Sleep(60000);

	listener.Stop(TRUE);

	delete subscriber;

	return 0;
}
