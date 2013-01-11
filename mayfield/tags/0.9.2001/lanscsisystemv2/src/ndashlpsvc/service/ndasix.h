#pragma once
#include <winsock2.h>
#include <vector>
#include <set>

#include "task.h"
#include "ndasctype.h"
#include "socketlpx.h"

#include "lshelper.h"
#include "infoxchanger.h"

//
// Forward declarations for data types in the header
//
class CLpxDatagramSender;
class CLpxDatagramBroadcaster;

class CLpxUdpAsyncListener;

class CNdasInfoExchangeServer :
	public ximeta::CTask
{
public:
	static const USHORT NDASIX_LISTEN_PORT = LPXRP_LSHELPER_INFOEX;

protected:

	const USHORT m_usListenPort;

	std::vector<LPX_ADDRESS> m_vLocalLpxAddress;
	HANDLE m_hDataEvents[MAX_SOCKETLPX_INTERFACE];
	HANDLE m_hLpxAddressListChangeEvent;
	DWORD m_dwTimeout;

	BOOL ResetLocalAddressList();
	
	template<typename T>
	BOOL ProcessPacket(
		CLpxUdpAsyncListener* pListener,
		const T* pPacket);

	BOOL DispatchPacket(
		CLpxUdpAsyncListener* pListener,
		DWORD cbPacket,
		const BYTE* pPacket);

public:
	CNdasInfoExchangeServer();
	virtual ~CNdasInfoExchangeServer();

	virtual BOOL Initialize();
	virtual DWORD OnTaskStart();
};

class CNdasInfoExchangeClient
{
public:
	CNdasInfoExchangeClient();
	virtual ~CNdasInfoExchangeClient();
	virtual BOOL Initialize();
};

class CNdasInfoExchangeBroadcaster :
	public ximeta::CTask
{
public:
	static const USHORT NDASIX_BROADCAST_PORT = LPXRP_LSHELPER_INFOEX;

protected:

	CLpxDatagramBroadcaster* m_pLpxClient[MAX_SOCKETLPX_INTERFACE];
	HANDLE m_hLpxAddressListChangeEvent;

	BOOL ResetLocalAddressList();

public:
	CNdasInfoExchangeBroadcaster();
	virtual ~CNdasInfoExchangeBroadcaster();

	virtual BOOL BroadcastStatus();
	virtual BOOL Initialize();
	virtual DWORD OnTaskStart();
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

