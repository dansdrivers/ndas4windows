/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include "autores.h"
#include "ndasheartbeat.h"
#include "xdebug.h"

static BOOL 
IsValidHeartbeat(DWORD cbData, LPCVOID lpData);

BOOL 
IsValidHeartbeat(DWORD cbData, LPCVOID lpData)
{
	if (sizeof(NDAS_DEVICE_HEARTBEAT) != cbData) {
		return FALSE;
	}

	const NDAS_DEVICE_HEARTBEAT* pHeartbeat = 
		reinterpret_cast<const NDAS_DEVICE_HEARTBEAT*>(lpData);

	//
	// version check: ucType == 0 and ucVersion = {0,1,2}
	//
	if (!
		((pHeartbeat->type == 0) && 
		 (pHeartbeat->version == 0 || 
		  pHeartbeat->version == 1 ||
		  pHeartbeat->version == 2)))
	{
		return FALSE;
	}

	return TRUE;
}

CNdasDeviceHeartbeatListener::
CNdasDeviceHeartbeatListener(
	USHORT usListenPort, 
	DWORD dwWaitTimeout) :
	m_usListenPort(usListenPort),
	CTask(_T("NdasDeviceHeartbeatListener Task"))
{
}

CNdasDeviceHeartbeatListener::
~CNdasDeviceHeartbeatListener()
{
}

BOOL 
CNdasDeviceHeartbeatListener::
Initialize()
{
	BOOL fSuccess = m_dgramServer.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("CLpxDatagramServer init failed: "));
		return FALSE;
	}
	return CTask::Initialize();
}

VOID
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

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Heartbeat Packet Receive failed: "));
		return;
	}

	DBGPRT_NOISE(_FT("Received Heartbeat Packet (%d bytes) ")
		_T("from %s at %s: ")
		_T("Receive Flag %08X\n"),
		cbReceived, 
		CSockLpxAddr(remoteAddr).ToString(),
		CSockLpxAddr(sock.GetBoundAddr()).ToString(),
		dwRecvFlags);


	fSuccess = IsValidHeartbeat(cbReceived, (LPCVOID) pData);

	if (!fSuccess) {
		DBGPRT_WARN(_FT("Invalid packet received!\n"));
		return;
	}

	const NDAS_DEVICE_HEARTBEAT* pHeartbeat = 
		reinterpret_cast<const NDAS_DEVICE_HEARTBEAT*>(pData);

	NDAS_DEVICE_HEARTBEAT_INFO eventData = {0};
	
	::CopyMemory(
		eventData.deviceAddress,
		remoteAddr.LpxAddress.Node,
		sizeof(remoteAddr.LpxAddress.Node));
	C_ASSERT(sizeof(eventData.deviceAddress)==sizeof(remoteAddr.LpxAddress.Node));

	::CopyMemory(
		eventData.localAddress,
		sock.GetBoundAddr()->LpxAddress.Node,
		sizeof(remoteAddr.LpxAddress.Node));
	C_ASSERT(sizeof(eventData.localAddress)==sizeof(remoteAddr.LpxAddress.Node));

	eventData.timestamp = ::GetTickCount();
	eventData.type = pHeartbeat->type;
	eventData.version = pHeartbeat->version;
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
		DBGPRT_ERR_EX(_FT("Listening Heartbeat at port %d failed: "), m_usListenPort);
		return 255;
	}

	return 0;
}


