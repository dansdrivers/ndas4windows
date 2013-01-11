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
#pragma warning(disable: 4995)
#include <set>
#pragma warning(default: 4995)
#include "task.h"
#include "observer.h"
#include "lpxcs.h"
#include "autores.h"

class CNdasDeviceHeartbeatListener;

typedef struct _NDAS_DEVICE_HEARTBEAT_DATA {
	LPX_ADDRESS localAddr;
	LPX_ADDRESS remoteAddr;
	UCHAR ucType;
	UCHAR ucVersion;
} NDAS_DEVICE_HEARTBEAT_DATA, *PNDAS_DEVICE_HEARTBEAT_DATA;

class CNdasDeviceHeartbeatListener : 
	public ximeta::CTask, 
	public ximeta::CSubject,
	public CLpxDatagramServer::IReceiveProcessor
{
public:

	typedef struct _HEARTBEAT_RAW_DATA {
		UCHAR ucType;
		UCHAR ucVersion;
	} HEARTBEAT_RAW_DATA, *PHEARTBEAT_RAW_DATA;
	
protected:

	const USHORT	m_usListenPort;

	// synchronization
	CRITICAL_SECTION m_crs;

	// ISubjectCtrlBroadcast data
	NDAS_DEVICE_HEARTBEAT_DATA m_lastHeartbeatData;

protected:

	// Implements CLpxDatagramServer::IReceiveProcessor
	VOID CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& cListener);

	// implementation of the CTask class
	virtual DWORD OnTaskStart();

	static BOOL spValidatePacketData(
		IN DWORD cbData,
		IN LPCVOID lpData);

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

	// implementation of ISubjectCtrlBroadcast interface
	virtual BOOL GetHeartbeatData(PNDAS_DEVICE_HEARTBEAT_DATA pData);
};

#endif // _NDHBRECV_H_
