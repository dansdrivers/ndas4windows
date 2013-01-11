/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <ndas/ndascomm.h>
#include "ndasdevhb.h"
#include "traceflags.h"

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
		XTLTRACE2(nstf::tHeartbeatListener, nstf::tError, 
			"Heartbeat Packet Receive failed\n");
		return;
	}

	XTLTRACE2(nstf::tHeartbeatListener, nstf::tNoise,
		"Received Heartbeat Packet (%d bytes) from %ws at %ws, RecvFlags %08X\n",
		cbReceived, 
		CSockLpxAddr(remoteAddr).ToString(),
		CSockLpxAddr(sock.GetBoundAddr()).ToString(),
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
	XTLCALLTRACE_RET(DWORD, ret);

	XTLTRACE2(nstf::tHeartbeatListener, nstf::tInfo, "Starting NDAS Heartbeat Listener.\n");

	CLpxDatagramServer dgramServer;

	BOOL fSuccess = dgramServer.Initialize();
	if (!fSuccess) 
	{
		XTLTRACE2_ERR(nstf::tHeartbeatListener, nstf::tError, 
			"CLpxDatagramServer init failed.\n");
		return ret = 254;
	}

	fSuccess = dgramServer.Receive(
		this, 
		m_ListenPort, 
		sizeof(HEARTBEAT_RAW_DATA),
		hStopEvent);

	if (!fSuccess) 
	{
		XTLTRACE2_ERR(nstf::tHeartbeatListener, nstf::tError,
			"Listening Heartbeat at port %d failed\n", m_ListenPort);
		return ret = 255;
	}

	XTLTRACE2(nstf::tHeartbeatListener, nstf::tInfo, "NDAS Heartbeat Stopped.\n");

	return ret = 0;
}

namespace
{

bool IsValidHeartbeatPacket(DWORD cbData, LPCVOID lpData)
{
	if (sizeof(HEARTBEAT_RAW_DATA) != cbData) 
	{
		XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
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
		XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
			"Invalid heartbeat - version or type mismatch: Type %d, Version %d.\n", 
			lpmsg->Type, lpmsg->Version);
		return false;
	}

	XTLTRACE2(nstf::tHeartbeatListener, nstf::tNoise,
		"Heartbeat received: Type %d, Version %d.\n",
		lpmsg->Type, lpmsg->Version);

	return true;
}

} // local namespace
