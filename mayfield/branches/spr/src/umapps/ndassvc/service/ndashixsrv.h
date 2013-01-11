#pragma once
#include <set>
#include "ndas/ndastypeex.h"
#include "ndas/ndashix.h"
#include "lpxtrans.h"
#include "lpxcs.h"
#include "task.h"


class CNdasHIXServer :
	public ximeta::CTask,
	public CLpxDatagramServer::IReceiveProcessor
{

	GUID m_HostGuid;
	CONST BYTE* m_pbHostId;

//	HANDLE m_hNotifyThread;

//	static DWORD WINAPI NotifyThreadProc(LPVOID lpParam);

public:

	CNdasHIXServer(LPCGUID lpHostGuid = NULL);
	virtual ~CNdasHIXServer();

	// Implements CLpxDatagramServer::IReceiveProcessor
	VOID CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& cListener);

	// Implements CTask
	virtual BOOL Initialize();
	virtual DWORD OnTaskStart();

	VOID OnHixDiscover(
		CLpxDatagramSocket& sock,
		SOCKADDR_LPX* pRemoteAddr,
		CONST NDAS_HIX::DISCOVER::REQUEST* pRequest);

	VOID OnHixQueryHostInfo(
		CLpxDatagramSocket& sock, 
		SOCKADDR_LPX* pRemoteAddr, 
		CONST NDAS_HIX::QUERY_HOST_INFO::REQUEST* pRequest);

	VOID OnHixSurrenderAccess(
		CLpxDatagramSocket& sock,
		SOCKADDR_LPX* pRemoteAddr,
		CONST NDAS_HIX::SURRENDER_ACCESS::REQUEST* pRequest);

	VOID OnHixDeviceChangeNotify(
		CLpxDatagramSocket& sock,
		SOCKADDR_LPX* pRemoteAddr,
		CONST NDAS_HIX::DEVICE_CHANGE::NOTIFY* pNotify);

	VOID OnHixUnitDeviceChangeNotify(
		CLpxDatagramSocket& sock, 
		SOCKADDR_LPX* pRemoteAddr, 
		CONST NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY* pNotify);

};
