/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include "ndasheartbeat.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasheartbeat.tmh"
#endif

//
// Local functions
//
namespace
{

BOOL 
IsValidHeartbeat(DWORD cbData, LPCVOID lpData)
{
	if (sizeof(NDAS_DEVICE_HEARTBEAT) != cbData) 
	{
		return FALSE;
	}

	//
	// We will not filter the type and the version of the NDAS device.
	//
	
	//
	//const NDAS_DEVICE_HEARTBEAT* pHeartbeat = 
	//	reinterpret_cast<const NDAS_DEVICE_HEARTBEAT*>(lpData);

	////
	//// version check: ucType == 0 and ucVersion = {0,1,2}
	////
	//if (!
	//	((pHeartbeat->Type == 0) && 
	//	 (pHeartbeat->Version == 0 || 
	//	  pHeartbeat->Version == 1 ||
	//	  pHeartbeat->Version == 2)))
	//{
	//	return FALSE;
	//}

	return TRUE;
}

} // namespace


CNdasDeviceHeartbeatListener::CNdasDeviceHeartbeatListener(
	USHORT usListenPort, 
	DWORD dwWaitTimeout) :
	m_usListenPort(usListenPort),
	CTask(_T("NdasDeviceHeartbeatListener Task"))
{
}

CNdasDeviceHeartbeatListener::~CNdasDeviceHeartbeatListener()
{
}

BOOL 
CNdasDeviceHeartbeatListener::Initialize()
{
	BOOL fSuccess = m_dgramServer.Initialize();
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"CLpxDatagramServer init failed, error=0x%X\n", GetLastError());
		return FALSE;
	}
	return CTask::Initialize();
}

void
CNdasDeviceHeartbeatListener::OnReceive(CLpxDatagramSocket& sock)
{
	SOCKADDR_LPX remoteAddr;
	DWORD cbReceived;

	PNDAS_DEVICE_HEARTBEAT pData = NULL;

	DWORD dwRecvFlags;
	BOOL fSuccess = sock.GetRecvFromResult(
		&remoteAddr, 
		&cbReceived, 
		(BYTE**)&pData,
		&dwRecvFlags);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"Heartbeat Packet Receive failed, error=0x%X\n", GetLastError());
		return;
	}

	XTLTRACE1(TRACE_LEVEL_VERBOSE,
		"Received Heartbeat Packet (%d bytes) "
		"from %s at %s: "
		"Receive Flag %08X\n",
		cbReceived, 
		CSockLpxAddr(remoteAddr).ToStringA(),
		CSockLpxAddr(sock.GetBoundAddr()).ToStringA(),
		dwRecvFlags);


	fSuccess = IsValidHeartbeat(cbReceived, (LPCVOID) pData);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_WARNING, 
			"Invalid packet received!\n");
		return;
	}

	const NDAS_DEVICE_HEARTBEAT* pHeartbeat = 
		reinterpret_cast<const NDAS_DEVICE_HEARTBEAT*>(pData);

	NDAS_DEVICE_HEARTBEAT_INFO eventData = {0};
	
	::CopyMemory(
		eventData.DeviceAddress.Node,
		remoteAddr.LpxAddress.Node,
		sizeof(remoteAddr.LpxAddress.Node));

	C_ASSERT(
		sizeof(eventData.DeviceAddress.Node) ==
		sizeof(remoteAddr.LpxAddress.Node));

	::CopyMemory(
		eventData.LocalAddress.Node,
		sock.GetBoundAddr()->LpxAddress.Node,
		sizeof(remoteAddr.LpxAddress.Node));

	C_ASSERT(
		sizeof(eventData.LocalAddress.Node) ==
		sizeof(remoteAddr.LpxAddress.Node));

	eventData.Timestamp = ::GetTickCount();
	eventData.Type = pHeartbeat->Type;
	eventData.Version = pHeartbeat->Version;

	__raise this->OnHeartbeat(eventData);
}

DWORD
CNdasDeviceHeartbeatListener::OnTaskStart()
{
	BOOL fSuccess = m_dgramServer.Receive(
		this, 
		m_usListenPort, 
		sizeof(NDAS_DEVICE_HEARTBEAT),
		m_hTaskTerminateEvent);
	
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Listening Heartbeat at port %d failed, error=0x%X\n",
			m_usListenPort, GetLastError());
		return 255;
	}

	return 0;
}


