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

#include <pshpack1.h>

// 
// Used by NDAS 1.0, 1.1, 2.0
//
typedef struct _HEARTBEAT_RAW_DATA_V1 {
	UCHAR Type;
	UCHAR Version;
} HEARTBEAT_RAW_DATA_V1, *PHEARTBEAT_RAW_DATA_V1;

//
// Used by NDAS 2.5
//
// To do: separate Dgram header and Pnp message
#define NDAS_DGRAM_SIGNATURE 0xffd6

typedef struct _HEARTBEAT_RAW_DATA_V2 {
	struct {
		UINT16 Signature; // 0xffd6. Big endian.
		UINT16 Reserved1;
		UCHAR MsgVer; // 0x01
		UCHAR Reserved2;
		UCHAR Flags;
		UCHAR Reserved3;
		UCHAR OpCode;
		UCHAR Reserved4;
		UINT16 Length;	// Size Including this 
		UINT32 Reserved5;
	} DgramHeader;
	struct {
		UCHAR DeviceType; // 0 for ASIC
		UCHAR Version; // HW version. 3 for NDAS 2.5
		UINT16 Revision;	// HW Revision
		UINT16 StreamListenPort; // For HW always 0x2710. For Emulator, emulator defined value.
		UINT16 Reserved;
	} PnpMessage;
} HEARTBEAT_RAW_DATA_V2, *PHEARTBEAT_RAW_DATA_V2;

typedef union HEARTBEAT_RAW_DATA{
	HEARTBEAT_RAW_DATA_V1 v1;
	HEARTBEAT_RAW_DATA_V2 v2;
} HEARTBEAT_RAW_DATA, *PHEARTBEAT_RAW_DATA;

#include <poppack.h>

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
	if (pData->v2.DgramHeader.Signature == ntohs(NDAS_DGRAM_SIGNATURE)) {
		receivedData.Type = pData->v2.PnpMessage.DeviceType;
		receivedData.Version = pData->v2.PnpMessage.Version;		
		receivedData.Revision = pData->v2.PnpMessage.Revision;
	} else {
		receivedData.Type = pData->v1.Type;
		receivedData.Version = pData->v1.Version;
		receivedData.Revision = 0;
	}
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
	// Check minimum size
	if (cbData < sizeof(HEARTBEAT_RAW_DATA_V1)) 
	{
		XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
			"Invalid heartbeat - too small size %d\n",
			cbData);
		return FALSE;	
	}
	const HEARTBEAT_RAW_DATA_V1* lpmsg_v1 = static_cast<const HEARTBEAT_RAW_DATA_V1*>(lpData);
	const HEARTBEAT_RAW_DATA_V2* lpmsg_v2 = static_cast<const HEARTBEAT_RAW_DATA_V2*>(lpData);

	if (lpmsg_v2->DgramHeader.Signature == htons(NDAS_DGRAM_SIGNATURE)) {
		// Same or above version 2.5 PNP message

		// Check signature
		if (sizeof(HEARTBEAT_RAW_DATA_V2) != cbData ) 
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Invalid heartbeat - size mismatch: Size %d, should be %d.\n",
				cbData, sizeof(HEARTBEAT_RAW_DATA_V2));
			return FALSE;
		}
		
		// Check MSG version
		if (lpmsg_v2->DgramHeader.MsgVer != 0x01) 
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Invalid heartbeat - unknown message version %d.\n",
				lpmsg_v2->DgramHeader.MsgVer);
			return FALSE;
		}

		// Handle PNP broadcast only
		if (lpmsg_v2->DgramHeader.OpCode != 0x81) 
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Ignoring non PNP broadcast packet: Opcode= %d.\n",
				lpmsg_v2->DgramHeader.OpCode);
			return FALSE;
		}

		if (lpmsg_v2->DgramHeader.Length != ntohs(24)) 
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Invalid heartbeat - length mismatch: Size %d, should be %d.\n",
				cbData, sizeof(HEARTBEAT_RAW_DATA_V2));
		}
	
		//
		// version check: ucType == 0 and ucVersion = {3}
		//
		if (!(lpmsg_v2->PnpMessage.DeviceType == 0 && lpmsg_v2->PnpMessage.Version == 3))
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Invalid heartbeat - version or type mismatch: Type %d, Version %d.\n", 
				lpmsg_v2->PnpMessage.DeviceType, lpmsg_v2->PnpMessage.Version);
			return false;
		}

		// Ignore strema port other than 10000 right now
		if (lpmsg_v2->PnpMessage.StreamListenPort != htons(10000)) 
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Port other than 10000 is not supported yet\n", 
				lpmsg_v2->PnpMessage.StreamListenPort);
			return false;
		}

		XTLTRACE2(nstf::tHeartbeatListener, nstf::tNoise,
			"Heartbeat received: Type %d, Version %d.\n",
			lpmsg_v2->PnpMessage.DeviceType, lpmsg_v2->PnpMessage.Version);		
		return TRUE;
	} else {
		// Assume ~2.0 PNP message
		if (sizeof(HEARTBEAT_RAW_DATA_V1) != cbData ) 
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Invalid heartbeat - size mismatch: Size %d, should be %d.\n",
				cbData, sizeof(HEARTBEAT_RAW_DATA_V1));
			return FALSE;
		}

		//
		// version check: ucType == 0 and ucVersion = {0,1}
		//
		if (!(lpmsg_v1->Type == 0 && 
			(lpmsg_v1->Version == 0 || lpmsg_v1->Version == 1 || lpmsg_v1->Version == 2)))
		{
			XTLTRACE2(nstf::tHeartbeatListener, nstf::tWarning,
				"Invalid heartbeat - version or type mismatch: Type %d, Version %d.\n", 
				lpmsg_v1->Type, lpmsg_v1->Version);
			return false;
		}

		XTLTRACE2(nstf::tHeartbeatListener, nstf::tNoise,
			"Heartbeat received: Type %d, Version %d.\n",
			lpmsg_v1->Type, lpmsg_v1->Version);		
		return TRUE;
	}
} // local namespace

} // namespace
