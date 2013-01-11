#pragma once
#include <winsock2.h>
#include <ndas/ndastypeex.h>
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
	public CLpxDatagramServer::IReceiveProcessor
{
public:

	static const USHORT NDASIX_LISTEN_PORT = LPXRP_LSHELPER_INFOEX;

protected:

	const USHORT m_usListenPort;

	struct {
		WORD wMajor;
		WORD wMinor;
	} m_NDFSVersion;

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

public:

	CNdasIXServer();
	virtual ~CNdasIXServer();

	bool Initialize();
	DWORD ThreadStart(HANDLE hStopEvent);
};

class CNdasIXBcast
{
public:

	static const USHORT NDASIX_BROADCAST_PORT;
	static const SOCKADDR_LPX NDASIX_BCAST_ADDR;

protected:

	struct 
	{
		WORD wMajor;
		WORD wMinor;
	} m_NDFSVersion;

	const static DWORD m_nSenders = MAX_SOCKETLPX_INTERFACE ;
	LPSOCKET_ADDRESS_LIST m_lpSocketAddressList;
	CLpxDatagramSocket m_senders[m_nSenders];
	CLpxSockAddrListChangeNotifier m_sockAddrChangeNotifier;

public:

	CNdasIXBcast();
	~CNdasIXBcast();

	void ResetBind(HANDLE hStopEvent);
	BOOL BroadcastStatus();

	bool Initialize();

	DWORD ThreadStart(HANDLE hStopEvent);
};

#define NDASIX_MAX_HOSTNAME_LEN 256

typedef struct _NDAS_UNITDEVICE_HOST_USAGE {

	LSNODE_ADDRESS	HostLanAddr;
	LSNODE_ADDRESS	HostWanAddr;
	LONG			HostNameLength;
	USHORT			HostNameType;
	WCHAR			HostName[NDASIX_MAX_HOSTNAME_LEN];		// Unicode
	UCHAR			UsageId;
	UCHAR			AccessRight;

} NDAS_UNITDEVICE_HOST_USAGE, *PNDAS_UNITDEVICE_HOST_USAGE;
