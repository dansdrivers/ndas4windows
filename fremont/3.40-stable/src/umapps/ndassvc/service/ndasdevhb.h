/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _NDHBRECV_H_
#define _NDHBRECV_H_
#pragma once
#include <windows.h>
#include <winsock2.h>
#include <socketlpx.h>
#include <xtl/xtllock.h>
#include <xtl/xtlthread.h>
#include "lpxcs.h"
#include "ndassvcworkitem.h"

class CNdasService;
class CNdasDeviceHeartbeatListener;

typedef struct _NDAS_DEVICE_HEARTBEAT_DATA {
	NDAS_DEVICE_ID NdasDeviceId;
	SOCKET_ADDRESS LocalAddress;
	SOCKET_ADDRESS RemoteAddress;
	UCHAR Type;
	UCHAR Version;
	USHORT Revision;
	DWORD Timestamp;
} NDAS_DEVICE_HEARTBEAT_DATA, *PNDAS_DEVICE_HEARTBEAT_DATA;

class CNdasDeviceHeartbeatListener : 
	public CLpxDatagramServer::IReceiveProcessor
{
protected:

	const USHORT m_ListenPort;

	mutable XTL::CReaderWriterLock m_datalock;

protected:

	// Implements CLpxDatagramServer::IReceiveProcessor
	void CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& cListener);

	std::vector<INdasHeartbeatSink*> m_sinks;

public:

	static const DWORD DEFAULT_BROADCASE_TIMEOUT = 6000;

	CNdasDeviceHeartbeatListener(
		USHORT ListenPort = 10002, 
		DWORD WaitTimeout = DEFAULT_BROADCASE_TIMEOUT);

	virtual ~CNdasDeviceHeartbeatListener();

	HRESULT Initialize();

	HRESULT Advise(INdasHeartbeatSink* pSink);
	HRESULT Unadvise(INdasHeartbeatSink* pSink);

	// XTL::CQueuedWorker
	DWORD ThreadStart(HANDLE hStopEvent);
};

#endif // _NDHBRECV_H_
