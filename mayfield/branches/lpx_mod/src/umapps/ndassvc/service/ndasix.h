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

	// implements CTask
	DWORD OnTaskStart();

public:

	CNdasIXServer();
	virtual ~CNdasIXServer();

	virtual BOOL Initialize();
};

class CNdasIXBcast :
	public ximeta::CTask
{
public:

	static CONST USHORT NDASIX_BROADCAST_PORT;
	static CONST SOCKADDR_LPX NDASIX_BCAST_ADDR;

protected:

	struct {
		WORD wMajor;
		WORD wMinor;
	} m_NDFSVersion;

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

typedef struct _NDAS_UNITDEVICE_HOST_USAGE {

	LSNODE_ADDRESS	HostLanAddr;
	LSNODE_ADDRESS	HostWanAddr;
	LONG			HostNameLength;
	USHORT			HostNameType;
	WCHAR			HostName[MAX_HOSTNAME_LEN];		// Unicode
	UCHAR			UsageId;
	UCHAR			AccessRight;

} NDAS_UNITDEVICE_HOST_USAGE, *PNDAS_UNITDEVICE_HOST_USAGE;
