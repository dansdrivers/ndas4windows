/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <ndas/ndascomm.h>
#include "ndascomobjectsimpl.hpp"

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

struct InvokeSink : public std::unary_function<INdasHeartbeatSink*, void>
{
	const NDAS_DEVICE_HEARTBEAT_DATA& data;
	InvokeSink(const NDAS_DEVICE_HEARTBEAT_DATA& data) : data(data) {}
	result_type operator()(const argument_type& sink) const
	{
		sink->NdasHeartbeatReceived(&data);
	}
};

void 
SockAddrLpxToNdasDeviceId(
	__in const SOCKADDR_LPX& SockAddrLpx,
	__out NDAS_DEVICE_ID& NdasDeviceId)
{
	ZeroMemory(
		&NdasDeviceId, 
		sizeof(NDAS_DEVICE_ID));
	CopyMemory(
		NdasDeviceId.Node, 
		SockAddrLpx.LpxAddress.Node, 
		sizeof(NdasDeviceId.Node));
}

} // local namespace

CNdasDeviceHeartbeatListener::CNdasDeviceHeartbeatListener(
	USHORT ListenPort, 
	DWORD dwWaitTimeout) :
	m_ListenPort(ListenPort)
{
}

CNdasDeviceHeartbeatListener::~CNdasDeviceHeartbeatListener()
{
}

HRESULT
CNdasDeviceHeartbeatListener::Initialize()
{
	if (!m_datalock.Initialize())
	{
		return AtlHresultFromLastError();
	}
	return S_OK;
}

VOID
CNdasDeviceHeartbeatListener::OnReceive(CLpxDatagramSocket& sock)
{
	SOCKADDR_LPX remoteAddr;
	PHEARTBEAT_RAW_DATA pData = NULL;
	DWORD receivedBytes;
	DWORD receiveFlags;

	BOOL success = sock.GetRecvFromResult(
		&remoteAddr, 
		&receivedBytes, 
		(BYTE**)&pData,
		&receiveFlags);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_ERROR, 
			"Heartbeat Packet Receive failed, error=0x%X\n", GetLastError());
		return;
	}

	XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_VERBOSE,
		"Received Heartbeat Packet (%d bytes) from %s at %s, RecvFlags %08X\n",
		receivedBytes, 
		CSockLpxAddr(remoteAddr).ToStringA(),
		CSockLpxAddr(sock.GetBoundAddr()).ToStringA(),
		receiveFlags);

	if (!IsValidHeartbeatPacket(receivedBytes, (LPCVOID) pData))
	{
		return;
	}

	//
	// Copy the data
	//
	SOCKADDR_LPX localLpxAddress, remoteLpxAddress;

	remoteLpxAddress = remoteAddr;
	remoteLpxAddress.LpxAddress.Port = 0;

	localLpxAddress = *sock.GetBoundAddr();
	localLpxAddress.LpxAddress.Port = 0;

	NDAS_DEVICE_HEARTBEAT_DATA receivedData = {0};

	SockAddrLpxToNdasDeviceId(
		remoteLpxAddress, 
		receivedData.NdasDeviceId);

	receivedData.LocalAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	receivedData.LocalAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&localLpxAddress);

	receivedData.RemoteAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	receivedData.RemoteAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&remoteLpxAddress);

	receivedData.Type = pData->Type;
	receivedData.Version = pData->Version;
	receivedData.Revision = 0;
	receivedData.Timestamp = ::GetTickCount();

	m_datalock.LockReader();
	std::for_each(
		m_sinks.begin(), 
		m_sinks.end(), 
		InvokeSink(receivedData));
	m_datalock.UnlockReader();
}

DWORD
CNdasDeviceHeartbeatListener::ThreadStart(HANDLE hStopEvent)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

	DWORD ret;

	XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_INFORMATION, 
		"Starting NDAS Heartbeat Listener.\n");

	CLpxDatagramServer dgramServer;

	BOOL success = dgramServer.Initialize();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HEARTBEAT, TRACE_LEVEL_ERROR, 
			"CLpxDatagramServer init failed, error=0x%X\n",
			GetLastError());
		return ret = 254;
	}

	success = dgramServer.Receive(
		this, 
		m_ListenPort, 
		sizeof(HEARTBEAT_RAW_DATA),
		hStopEvent);

	if (!success) 
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

HRESULT CNdasDeviceHeartbeatListener::Advise(INdasHeartbeatSink* pSink)
{
	HRESULT hr = S_OK;
	m_datalock.LockWriter();
	try
	{
		m_sinks.push_back(pSink);
	}
	catch (...)
	{
		hr = E_OUTOFMEMORY;
	}
	m_datalock.UnlockWriter();
	return hr;
}

HRESULT CNdasDeviceHeartbeatListener::Unadvise(INdasHeartbeatSink* pSink)
{
	HRESULT hr = S_OK;
	m_datalock.LockWriter();
	std::vector<INdasHeartbeatSink*>::iterator itr = 
		std::remove(m_sinks.begin(), m_sinks.end(), pSink);
	if (m_sinks.end() == itr)
	{
		hr = E_INVALIDARG;
	}
	else
	{
		m_sinks.erase(itr);
	}
	m_datalock.UnlockWriter();
	return hr;
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
