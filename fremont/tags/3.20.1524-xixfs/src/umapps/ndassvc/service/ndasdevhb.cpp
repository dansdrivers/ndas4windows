/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <ndas/ndascomm.h>
#include "ndasdevhb.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasdevhb.tmh"
#endif

namespace
{

typedef struct _HEARTBEAT_RAW_DATA {
	UCHAR Type;
	UCHAR Version;
} HEARTBEAT_RAW_DATA, *PHEARTBEAT_RAW_DATA;

bool IsValidHeartbeatPacket(DWORD cbData, LPCVOID lpData);

} // local namespace

CNdasDeviceHeartbeatListener::
CNdasDeviceHeartbeatListener(
	const CNdasService& service,
	USHORT ListenPort, 
	DWORD dwWaitTimeout) :
	m_service(service),
	m_ListenPort(ListenPort)
{
	::ZeroMemory(&m_lastHeartbeatData, sizeof(NDAS_DEVICE_HEARTBEAT_DATA));
}

CNdasDeviceHeartbeatListener::
~CNdasDeviceHeartbeatListener()
{
}

void
CNdasDeviceHeartbeatListener::
GetHeartbeatData(PNDAS_DEVICE_HEARTBEAT_DATA pData) const
{
	XTL::CReaderLockHolder dataReaderHolder(m_datalock);
	*pData = m_lastHeartbeatData;
}

bool 
CNdasDeviceHeartbeatListener::
Initialize()
{
	return m_datalock.Initialize();
}

VOID
CNdasDeviceHeartbeatListener::OnReceive(CLpxDatagramSocket& sock)
{
	SOCKADDR_LPX remoteAddr;
	PHEARTBEAT_RAW_DATA pData = NULL;
	DWORD cbReceived;
	DWORD dwRecvFlags;
	BOOL fSuccess = sock.GetRecvFromResult(
		&remoteAddr, 
		&cbReceived, 
		(BYTE**)&pData,
		&dwRecvFlags);

	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_ERROR, 
			"Heartbeat Packet Receive failed, error=0x%X\n", GetLastError());
		return;
	}

	XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_VERBOSE,
		"Received Heartbeat Packet (%d bytes) from %s at %s, RecvFlags %08X\n",
		cbReceived, 
		CSockLpxAddr(remoteAddr).ToStringA(),
		CSockLpxAddr(sock.GetBoundAddr()).ToStringA(),
		dwRecvFlags);

	if (!IsValidHeartbeatPacket(cbReceived, (LPCVOID) pData))
	{
		return;
	}

	//
	// Notify to observers
	//

	//
	// Copy the data
	//
	NDAS_DEVICE_HEARTBEAT_DATA receivedData;
	receivedData.localAddr = sock.GetBoundAddr()->LpxAddress;
	receivedData.remoteAddr = remoteAddr.LpxAddress;
	receivedData.Type = pData->Type;
	receivedData.Version = pData->Version;
	receivedData.Timestamp = ::GetTickCount();

	{
		XTL::CWriterLockHolder dataWriteHolder(m_datalock);
		m_lastHeartbeatData = receivedData;
	}

	Notify();
}

DWORD
CNdasDeviceHeartbeatListener::ThreadStart(HANDLE hStopEvent)
{
	DWORD ret;

	XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_INFORMATION, 
		"Starting NDAS Heartbeat Listener.\n");

	CLpxDatagramServer dgramServer;

	BOOL fSuccess = dgramServer.Initialize();
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_ERROR, 
			"CLpxDatagramServer init failed, error=0x%X\n",
			GetLastError());
		return ret = 254;
	}

	fSuccess = dgramServer.Receive(
		this, 
		m_ListenPort, 
		sizeof(HEARTBEAT_RAW_DATA),
		hStopEvent);

	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_ERROR,
			"Listening Heartbeat at port %d failed, error=0x%X\n", 
			m_ListenPort, GetLastError());
		return ret = 255;
	}

	XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_INFORMATION, 
		"NDAS Heartbeat Stopped.\n");

	return ret = 0;
}

namespace
{

bool IsValidHeartbeatPacket(DWORD cbData, LPCVOID lpData)
{
	if (sizeof(HEARTBEAT_RAW_DATA) != cbData) 
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_WARNING,
			"Invalid heartbeat - size mismatch: Size %d, should be %d.\n",
			cbData, sizeof(HEARTBEAT_RAW_DATA));
		return FALSE;
	}

	const HEARTBEAT_RAW_DATA* lpmsg = static_cast<const HEARTBEAT_RAW_DATA*>(lpData);

	//
	// version check: ucType == 0 and ucVersion = {0,1}
	//
	if (!(lpmsg->Type == 0 && 
		(lpmsg->Version == 0 || lpmsg->Version == 1 || lpmsg->Version == 2)))
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_WARNING,
			"Invalid heartbeat - version or type mismatch: Type %d, Version %d.\n", 
			lpmsg->Type, lpmsg->Version);
		return false;
	}

	XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_VERBOSE,
		"Heartbeat received: Type %d, Version %d.\n",
		lpmsg->Type, lpmsg->Version);

	return true;
}

} // local namespace
