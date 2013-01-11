/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _NDAS_DEVICE_HEARTHEAT_H_
#define _NDAS_DEVICE_HEARTHEAT_H_
#pragma once
#include <windows.h>
#include <winsock2.h>
#include <socketlpx.h>
#include "task.h"
#include "lpxcs.h"
#include <autores.h>
#include <ndas/ndashear.h>

class CNdasDeviceHeartbeatListener;

#include <pshpack4.h>

typedef struct _NDAS_DEVICE_HEARTBEAT {
	UCHAR Type;
	UCHAR Version;
} NDAS_DEVICE_HEARTBEAT, *PNDAS_DEVICE_HEARTBEAT;

#include <poppack.h>

[event_source(native)]
struct INdasDeviceHeartbeat
{
	__event void OnHeartbeat(const NDAS_DEVICE_HEARTBEAT_INFO& eventData);
};

class CNdasDeviceHeartbeatListener : 
	public INdasDeviceHeartbeat,
	public CTask, 
	public CLpxDatagramServer::IReceiveProcessor
{
public:
	
protected:

	const USHORT	m_usListenPort;
	CLpxDatagramServer m_dgramServer;

protected:

	// Implements CLpxDatagramServer::IReceiveProcessor
	void OnReceive(CLpxDatagramSocket& cListener);

	// implementation of the CTask class
	virtual DWORD OnTaskStart();

public:

	static const DWORD DEFAULT_BROADCASE_TIMEOUT = 6000;

	CNdasDeviceHeartbeatListener(
		USHORT usListenPort = 10002, 
		DWORD dwWaitTimeout = DEFAULT_BROADCASE_TIMEOUT);
	virtual ~CNdasDeviceHeartbeatListener();

	//
	// implementation of the CTask class
	//
	virtual BOOL Initialize();
};

#endif // _NDHBRECV_H_
