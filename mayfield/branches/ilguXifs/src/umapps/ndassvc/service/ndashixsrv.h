#pragma once
#include <set>
#include "ndas/ndastypeex.h"
#include "ndas/ndashix.h"
#include "lpxtrans.h"
#include "lpxcs.h"

class CNdasService;

class CNdasHIXServer :
	public CLpxDatagramServer::IReceiveProcessor
{
	CNdasService& m_service;
	GUID m_HostGuid;
	const BYTE* m_pbHostId;

public:

	CNdasHIXServer(CNdasService& service, LPCGUID lpHostGuid = NULL);
	virtual ~CNdasHIXServer();

	// Implements CLpxDatagramServer::IReceiveProcessor
	void CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& cListener);

	bool Initialize();
	DWORD ThreadStart(HANDLE hStopEvent);

	void OnHixDiscover(
		CLpxDatagramSocket& sock,
		SOCKADDR_LPX* pRemoteAddr,
		const NDAS_HIX::DISCOVER::REQUEST* pRequest);

	void OnHixQueryHostInfo(
		CLpxDatagramSocket& sock, 
		SOCKADDR_LPX* pRemoteAddr, 
		const NDAS_HIX::QUERY_HOST_INFO::REQUEST* pRequest);

	void OnHixSurrenderAccess(
		CLpxDatagramSocket& sock,
		SOCKADDR_LPX* pRemoteAddr,
		const NDAS_HIX::SURRENDER_ACCESS::REQUEST* pRequest);

	void OnHixDeviceChangeNotify(
		CLpxDatagramSocket& sock,
		SOCKADDR_LPX* pRemoteAddr,
		const NDAS_HIX::DEVICE_CHANGE::NOTIFY* pNotify);

	void OnHixUnitDeviceChangeNotify(
		CLpxDatagramSocket& sock, 
		SOCKADDR_LPX* pRemoteAddr, 
		const NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY* pNotify);

};
