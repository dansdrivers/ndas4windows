#pragma once

class CNdasHIXServer :
	public CLpxDatagramServer::IReceiveProcessor
{
	GUID m_HostGuid;
	const BYTE* m_pbHostId;

public:

	CNdasHIXServer(LPCGUID lpHostGuid = NULL);
	~CNdasHIXServer();

	// Implements CLpxDatagramServer::IReceiveProcessor
	void CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& cListener);

	HRESULT Initialize();
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
