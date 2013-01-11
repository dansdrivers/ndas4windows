#pragma once
#include <set>
#include <vector>
#include <map>
#include "ndastypeex.h"
#include "lpxcs.h"
#include "ndashix.h"
#include "ws2tcpip.h"
#include "stdutils.h"

//
// Base class for HIX clients
//
class CNdasHIXClient
{
protected:
	GUID m_hostGuid;
	CNdasHIXClient(LPCGUID lpHostGuid = NULL);
};

/*
typedef struct lpx_addr {
union {
u_char Byte[6];
u_short Word[3];
} u;
} lpx_addr;

typedef lpx_addr LPX_ADDR;
class CNdasHIXHostInfo
{
	LPCTSTR m_pszHostName;
	LPCTSTR m_pszNetBIOSName;
	LPCTSTR m_pszFQDN;

	NHIX_HIC_OS_FAMILY_TYPE m_osfamily;
	NHIX_HIC_HOST_FEATURE_TYPE m_hostFeature;
	NHIX_HIC_TRANSPORT_TYPE m_transport; 

	LPX_ADDR* m_pLpxAddr; 
	DWORD m_cLpxAddr;
	IN_ADDR* m_pInAddr; 
	DWORD m_cInAddr;
	IN6_ADDR* m_pIn6Addr; 
	DWORD m_cIn6Addr;

public:
	CNdasHIXHostInfo();
	~CNdasHIXHostInfo();

	LPCTSTR GetHostName();
	LPCTSTR GetNetBIOSName();
	LPCTSTR GetFQDN();

	NHIX_HIC_OS_FAMILY_TYPE GetOSFamily();
	NHIX_HIC_HOST_FEATURE_TYPE GetFeatures();
	NHIX_HIC_TRANSPORT_TYPE GetTransportTypes();

	const LPX_ADDR* GetLPXAddressList(LPDWORD lpdwCount);
	const IN_ADDR* GetIPv4AddressList(LPDWORD lpdwCount);
	const IN6_ADDR* GetIPv6AddressList(LPDWORD lpdwCount);

	BOOL GetOSVersion(LPWORD dwMajor, LPWORD dwMinor, LPWORD dwBuild, LPWORD dwPrivate);
	BOOL GetNDFSVersion(LPWORD dwMajor, LPWORD dwMinor);
	BOOL GetNDASSWVersion(LPWORD dwMajor, LPWORD dwMinor, LPWORD dwBuild, LPWORD dwPrivate);
	DWORD GetFields();

	BOOL Parse(DWORD cbData, const BYTE* pbData);
};
*/

class CNdasHIXSurrenderAccessRequest :
	public CNdasHIXClient
{
	CLpxDatagramSocket m_dgSock;

public:

	CNdasHIXSurrenderAccessRequest(LPCGUID lpHostGuid = NULL) :
	  CNdasHIXClient(lpHostGuid) {}

	BOOL Initialize();
	BOOL Request(
		CONST SOCKADDR_LPX *pLocalAddr,
		CONST SOCKADDR_LPX *pRemoteAddr,
		const NDAS_UNITDEVICE_ID& unitDeviceId, 
		const NHIX_UDA uda,
		DWORD dwTimeout = 1000);

};

class CNdasHIXQueryHostInfo : 
	public CLpxDatagramMultiClient::IReceiveProcessor,
	public CNdasHIXClient
{
public:

	typedef struct _HOST_ENTRY {
		GUID HostGuid;
		DWORD cbData;
		SOCKADDR_LPX boundAddr;
		SOCKADDR_LPX remoteAddr;
		PNDAS_HIX_HOST_INFO_DATA pData;
	} HOST_ENTRY, *PHOST_ENTRY;

protected:

	CLpxDatagramMultiClient m_dgclient;

	DWORD m_dwInitTickCount;
	DWORD m_dwReplyCount;
	DWORD m_dwMaxReply;

	typedef std::vector<HOST_ENTRY> HostInfoDataVector;
	typedef std::set<GUID,less_GUID> HostGuidSet;

	HostGuidSet m_hostGuidSet;
	HostInfoDataVector m_hostInfoDataSet;

	VOID ClearHostData();
	// implements CLpxDatagramBroadcastClient::IReceiveProcessor
	BOOL OnReceive(CLpxDatagramSocket& cListener);	

public:

	CNdasHIXQueryHostInfo(LPCGUID lpHostGuid = NULL);
	virtual ~CNdasHIXQueryHostInfo();

	BOOL Initialize();

	DWORD GetHostInfoCount();
	CONST HOST_ENTRY* GetHostInfo(DWORD dwIndex);

	BOOL BroadcastQuery(
		DWORD dwTimeout,
		USHORT usRemotePort = NDAS_HIX_LISTEN_PORT,
		DWORD dwMaxRecvHint = 0);

	BOOL Query(
		DWORD dwTimeout,
		const SOCKADDR_LPX* pRemoteAddr,
		DWORD dwMaxRecvHint = 1);
};

// UDA is defined in NDASHIX.H
typedef UCHAR NDAS_HIX_UDA;

class CNdasHIXDiscover : 
	public CLpxDatagramMultiClient::IReceiveProcessor,
	public CNdasHIXClient
{
public:

	typedef struct _HOST_DATA {
		GUID HostGuid;
		SOCKADDR_LPX BoundAddr;
		SOCKADDR_LPX RemoteAddr;
		NDAS_HIX_UDA AccessType;
	} HOST_DATA, *PHOST_DATA;

protected:

	typedef std::set<GUID,less_GUID> HostGuidSet;
	typedef std::multimap<NDAS_UNITDEVICE_ID,PHOST_DATA> UnitDev_HostData_Map;

	HostGuidSet m_hostGuidSet;
	UnitDev_HostData_Map m_udHostDataMap;

	DWORD m_dwInitTickCount;
	DWORD m_dwReplyCount;
	DWORD m_dwMaxReply;

	VOID ClearHostData();

	CLpxDatagramMultiClient m_bcaster;

	BOOL OnReceive(CLpxDatagramSocket& cListener);	

public:

	CNdasHIXDiscover(LPCGUID lpHostGuid = NULL);
	~CNdasHIXDiscover();

	BOOL Initialize();
	BOOL Discover(
		const NDAS_UNITDEVICE_ID& unitDeviceId, 
		NHIX_UDA uda,
		DWORD dwMaxReply,
		DWORD dwTimeout = 1000);

	BOOL Discover(
		DWORD nCount, 
		const NDAS_UNITDEVICE_ID* pUnitDeviceId, 
		const NHIX_UDA* pUda,
		DWORD dwMaxReply,
		DWORD dwTimeout = 1000);

	DWORD GetHostCount(const NDAS_UNITDEVICE_ID& unitDeviceId);

	BOOL GetHostData(
		const NDAS_UNITDEVICE_ID& unitDeviceId,
		DWORD index,
		NHIX_UDA* pUDA = NULL,
		LPGUID pHostGuid = NULL,
		PSOCKADDR_LPX pRemoteAddr = NULL, 
		PSOCKADDR_LPX pLocalAddr = NULL);

};

class CNdasHIXChangeNotify :
	public CNdasHIXClient
{
	CLpxDatagramMultiClient m_bcaster;

public:
	CNdasHIXChangeNotify(LPCGUID lpHostGuid = NULL) :
	  CNdasHIXClient(lpHostGuid) 
	{}

	BOOL Initialize();
	//
	// Notification of Unit Device Change
	//
	BOOL Notify(CONST NDAS_UNITDEVICE_ID& unitDeviceId);
	BOOL Notify(CONST NDAS_DEVICE_ID& deviceId);
};

