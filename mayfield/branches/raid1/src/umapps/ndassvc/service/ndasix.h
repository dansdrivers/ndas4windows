#pragma once
#include <winsock2.h>
#include <vector>
#include <set>

#include "task.h"
#include "ndasctype.h"
#include "socketlpx.h"
#include "lpxcs.h"
#include "lshelper.h"
#include "infoxchanger.h"

//
// Forward declarations for data types in the header
//
//class CLpxDatagramSender;
//class CLpxDatagramBroadcaster;
//class CLpxUdpAsyncListener;

class CNdasIXServer :
	public ximeta::CTask,
	public CLpxDatagramServer::IReceiveProcessor
{
public:

	static const USHORT NDASIX_LISTEN_PORT = LPXRP_LSHELPER_INFOEX;

protected:

	CLpxDatagramServer m_dgs;

	const USHORT m_usListenPort;

	DWORD m_dwTimeout;

	// implements IReceiveProcessor
	VOID CLpxDatagramServer::IReceiveProcessor::
		OnReceive(CLpxDatagramSocket& sock);

	// IX Primary Update
	VOID OnIXPrimaryUpdate(
		CLpxDatagramSocket& sock,
		CONST SOCKADDR_LPX* pRemoteAddr,
		CONST LSINFOX_PRIMARY_UPDATE* pData);

	// IX Usage Request
	VOID OnIXUsageRequest(
		CLpxDatagramSocket& sock,
		CONST SOCKADDR_LPX* pRemoteAddr,
		CONST LSINFOX_NDASDEV_USAGE_REQUEST* pData);

	// implements CTask
	DWORD OnTaskStart();

public:

	CNdasIXServer();
	virtual ~CNdasIXServer();

	virtual BOOL Initialize();
};

/*
class CNdasInfoExchangeClient
{
public:
	CNdasInfoExchangeClient();
	virtual ~CNdasInfoExchangeClient();
	virtual BOOL Initialize();
};
*/

class CNdasIXBcast :
	public ximeta::CTask
{
public:

	static CONST USHORT NDASIX_BROADCAST_PORT;
	static CONST SOCKADDR_LPX NDASIX_BCAST_ADDR;

protected:

	const static DWORD m_nSenders = MAX_SOCKETLPX_INTERFACE ;
	LPSOCKET_ADDRESS_LIST m_lpSocketAddressList;
	CLpxDatagramSocket m_senders[m_nSenders];
	CLpxSockAddrListChangeNotifier m_sockAddrChangeNotifier;

public:

	CNdasIXBcast();
	virtual ~CNdasIXBcast();

	virtual BOOL BroadcastStatus();
	virtual BOOL Initialize();
	virtual DWORD OnTaskStart();
	VOID ResetBind();
};

static const DWORD MAX_HOSTNAME_LEN = 256;

struct NDAS_UNITDEVICE_HOST_USAGE {

	LSNODE_ADDRESS	HostLanAddr;
	LSNODE_ADDRESS	HostWanAddr;
	LONG			HostNameLength;
	USHORT			HostNameType;
	WCHAR			HostName[MAX_HOSTNAME_LEN];		// Unicode
	UCHAR			UsageId;
	UCHAR			AccessRight;

};

typedef NDAS_UNITDEVICE_HOST_USAGE* PNDAS_UNITDEVICE_HOST_USAGE;
/*
class CNdasInfoExchangeUsage
{
protected:

	static const SOCKADDR_LPX NDASIX_BCAST_ADDR;
	static const BYTE NDASIX_PROTOCOL_NAME[4];
	static const DWORD NDASIX_COLLECT_TIMEOUT = 5000; // 0.5 seconds

	const NDAS_UNITDEVICE_ID m_unitDeviceId;

	HANDLE m_hTimer;
	DWORD m_dwExpectedReplies;
	DWORD m_dwLocalAddresses;
	BYTE m_ppbRecvBuf[MAX_SOCKETLPX_INTERFACE][INFOX_MAX_DATAGRAM_PKT_SIZE];
	WSABUF m_wsaBuffers[MAX_SOCKETLPX_INTERFACE];
	OVERLAPPED m_overlapped[MAX_SOCKETLPX_INTERFACE];
	SOCKET m_socks[MAX_SOCKETLPX_INTERFACE];
	SOCKADDR_LPX m_saLocalAddresses[MAX_SOCKETLPX_INTERFACE];
	WSAEVENT m_hDataEvents[MAX_SOCKETLPX_INTERFACE];

	INT m_iFromLen[MAX_SOCKETLPX_INTERFACE];
	DWORD m_dwRecvFlags[MAX_SOCKETLPX_INTERFACE];

	//
	// Statistics
	//
	std::vector<NDAS_UNITDEVICE_HOST_USAGE> m_vHostUsages;
	DWORD m_dwROHosts;
	DWORD m_dwRWHosts;

	BOOL SendRequests();
	BOOL CollectReplies();
	BOOL ResetLocalAddressList();

	BOOL InitSockets();
	VOID CleanupSockets();


public:

	CNdasInfoExchangeUsage(
		const NDAS_UNITDEVICE_ID& unitDeviceId,
		DWORD dwEstimatedReplies);

	virtual ~CNdasInfoExchangeUsage();

	BOOL Query();
	
	DWORD GetROHostCount() { return m_dwROHosts; }
	DWORD GetRWHostCount() { return m_dwRWHosts; }
	DWORD GetHostCount() { return m_vHostUsages.size(); }
	
	BOOL GetHostUsage(DWORD dwIndex, PNDAS_UNITDEVICE_HOST_USAGE pHostUsage);

	BOOL Initialize();
};

*/
