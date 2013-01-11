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
#include <set>
#include <xtl/xtllock.h>
#include <xtl/xtlthread.h>
#include "observer.h"
#include "lpxcs.h"
#include "ndasdevid.h"
#include "ndassvcworkitem.h"

class CNdasService;
class CNdasDeviceHeartbeatListener;

typedef struct _NDAS_DEVICE_HEARTBEAT_DATA {
	LPX_ADDRESS localAddr;
	LPX_ADDRESS remoteAddr;
	UCHAR Type;
	UCHAR Version;
	DWORD Timestamp;
} NDAS_DEVICE_HEARTBEAT_DATA, *PNDAS_DEVICE_HEARTBEAT_DATA;

//// std::less specialization for NDAS_DEVICE_HEARTBEAT_DATA
//template<>
//struct std::less<NDAS_DEVICE_HEARTBEAT_DATA> : 
//	public std::binary_function<NDAS_DEVICE_HEARTBEAT_DATA, NDAS_DEVICE_HEARTBEAT_DATA, bool>
//{	// functor for operator<
//	bool operator()(const NDAS_DEVICE_HEARTBEAT_DATA& _Left, const NDAS_DEVICE_HEARTBEAT_DATA& _Right) const
//	{	// apply operator< to operands
//		CNdasDeviceId lhs(_Left.remoteAddr), rhs(_Right.remoteAddr);
//		return std::less<NDAS_DEVICE_ID>()(lhs,rhs);
//	}
//};

//typedef std::set<NDAS_DEVICE_HEARTBEAT_DATA> NdasDeviceHeartbeatDataSet;
//typedef std::vector<NDAS_DEVICE_ID> NdasDeviceIdVector;

class CNdasDeviceHeartbeatListener : 
	public ximeta::CSubject,
	public CLpxDatagramServer::IReceiveProcessor
{
protected:

	const CNdasService& m_service;
	const USHORT m_ListenPort;

	// ISubjectCtrlBroadcast data
	NDAS_DEVICE_HEARTBEAT_DATA m_lastHeartbeatData;
	mutable XTL::CReaderWriterLock m_datalock;

protected:

	// Implements CLpxDatagramServer::IReceiveProcessor
	void CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& cListener);

public:

	static const DWORD DEFAULT_BROADCASE_TIMEOUT = 6000;

	CNdasDeviceHeartbeatListener(
		const CNdasService& service,
		USHORT ListenPort = 10002, 
		DWORD dwWaitTimeout = DEFAULT_BROADCASE_TIMEOUT);

	virtual ~CNdasDeviceHeartbeatListener();

	bool Initialize();

	// implementation of ISubjectCtrlBroadcast interface
	virtual void GetHeartbeatData(PNDAS_DEVICE_HEARTBEAT_DATA pData) const;

	// XTL::CQueuedWorker
	DWORD ThreadStart(HANDLE hStopEvent);
};

#endif // _NDHBRECV_H_
