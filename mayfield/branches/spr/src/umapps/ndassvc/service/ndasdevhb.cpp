/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "autores.h"
#include "ndasdevhb.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASDEVHB
#include "xdebug.h"

CNdasDeviceHeartbeatListener::
CNdasDeviceHeartbeatListener(
	USHORT usListenPort, 
	DWORD dwWaitTimeout) :
	m_usListenPort(usListenPort),
	ximeta::CTask(_T("NdasDeviceHeartbeatListener Task"))
{
	::InitializeCriticalSection(&m_crs);
	::ZeroMemory(
		&m_lastHeartbeatData,
		sizeof(NDAS_DEVICE_HEARTBEAT_DATA));
}

CNdasDeviceHeartbeatListener::
~CNdasDeviceHeartbeatListener()
{
	::DeleteCriticalSection(&m_crs);
}

BOOL
CNdasDeviceHeartbeatListener::
GetHeartbeatData(PNDAS_DEVICE_HEARTBEAT_DATA pData)
{
	::EnterCriticalSection(&m_crs);
	::CopyMemory(
		pData, 
		&m_lastHeartbeatData, 
		sizeof(NDAS_DEVICE_HEARTBEAT_DATA));
	::LeaveCriticalSection(&m_crs);

	return TRUE;
}

BOOL 
CNdasDeviceHeartbeatListener::
Initialize()
{
	return CTask::Initialize();
}

BOOL 
CNdasDeviceHeartbeatListener::
spValidatePacketData(
	DWORD cbData,
	LPCVOID lpData)
{
	if (sizeof(HEARTBEAT_RAW_DATA) != cbData) {
		DBGPRT_WARN(
			_FT("Invalid packet data - size mismatch: ")
			_T("Size %d, should be %d.\n"), 
			cbData, sizeof(HEARTBEAT_RAW_DATA));
		return FALSE;
	}

	PHEARTBEAT_RAW_DATA lpmsg = (PHEARTBEAT_RAW_DATA) lpData;

	//
	// version check: ucType == 0 and ucVersion = {0,1}
	//
	if (!(lpmsg->ucType == 0 && 
		(lpmsg->ucVersion == 0 || lpmsg->ucVersion == 1 || lpmsg->ucVersion == 2)))
	{
		DBGPRT_WARN(
			_FT("Invalid packet data - version or type mismatch:")
			_T("Type %d, Version %d.\n"), 
			lpmsg->ucType, lpmsg->ucVersion);
		return FALSE;
	}

	DBGPRT_NOISE(_FT("Valid packet data received: ")
		_T("Type %d, Version %d\n"), 
		lpmsg->ucType, lpmsg->ucVersion);

	return TRUE;
}

VOID
CNdasDeviceHeartbeatListener::OnReceive(CLpxDatagramSocket& sock)
{
	SOCKADDR_LPX remoteAddr;
	DWORD cbReceived;

	PHEARTBEAT_RAW_DATA pData = NULL;

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

	fSuccess = spValidatePacketData(cbReceived, (LPCVOID) pData);

	if (!fSuccess) {
		DBGPRT_WARN(_FT("Invalid packet received!\n"));
		return;
	}

	//
	// Notify to observers
	//

	// subject data update
	// TODO: Synchronization?
	m_lastHeartbeatData.localAddr = sock.GetBoundAddr()->LpxAddress;
	m_lastHeartbeatData.remoteAddr = remoteAddr.LpxAddress;
	m_lastHeartbeatData.ucType = pData->ucType;
	m_lastHeartbeatData.ucVersion = pData->ucVersion;

	Notify();

}

DWORD
CNdasDeviceHeartbeatListener::OnTaskStart()
{
	DBGPRT_INFO(_FT("Starting NDAS HIX Server.\n"));

	CLpxDatagramServer dgramServer;

	BOOL fSuccess = dgramServer.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("CLpxDatagramServer init failed: "));
		return 254;
	}

	fSuccess = dgramServer.Receive(
		this, 
		m_usListenPort, 
		sizeof(HEARTBEAT_RAW_DATA),
		m_hTaskTerminateEvent);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Listening Heartbeat at port %d failed: "), m_usListenPort);
		return 255;
	}

	return 0;
}

