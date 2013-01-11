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
#include "lpxcomm.h"
#include "autores.h"

class CNdasDeviceHeartbeatListener;
typedef CNdasDeviceHeartbeatListener *PCNdasDeviceHeartbeatListener;

struct NDAS_DEVICE_HEARTBEAT_DATA {
	LPX_ADDRESS localAddr;
	LPX_ADDRESS remoteAddr;
	UCHAR ucType;
	UCHAR ucVersion;
};

typedef NDAS_DEVICE_HEARTBEAT_DATA* PNDAS_DEVICE_HEARTBEAT_DATA;

class CNdasDeviceHeartbeatListener : 
	public ximeta::CTask, 
	public ximeta::CSubject
{
public:

	struct PAYLOAD {
		UCHAR ucType;
		UCHAR ucVersion;
	};
	
	typedef PAYLOAD* PPAYLOAD;

protected:

	const USHORT	m_usListenPort;

	std::vector<LPX_ADDRESS> m_vLocalLpxAddress;

	HANDLE	m_hDataEvents[MAX_SOCKETLPX_INTERFACE];
	HANDLE	m_hResetBindEvent;
	DWORD	m_dwTimeout;

	// synchronization
	CRITICAL_SECTION m_crs;

	// ISubjectCtrlBroadcast data
	NDAS_DEVICE_HEARTBEAT_DATA m_lastHeartbeatData;

protected:

	static BOOL ValidatePacketData(
		IN DWORD cbData,
		IN LPCVOID lpData);

	BOOL ResetLocalAddressList();
	BOOL ProcessPacket(
		PCLpxUdpAsyncListener pListener, 
		PPAYLOAD pPayload);

	virtual DWORD OnTaskStart();

public:

#define SEC 1000
#define DEFAULT_BROADCASE_TIMEOUT (6 * SEC)

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
