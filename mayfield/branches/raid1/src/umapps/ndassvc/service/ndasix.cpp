#include "stdafx.h"
#include "ndasix.h"

#include "lpxcomm.h"
#include "lsbusioctl.h"
#include "autores.h"

#include "ndasinstman.h"
#include "ndaslogdevman.h"
#include "ndaslogdev.h"
#include "ndasdev.h"
#include "ndasdevreg.h"
#include "ndasobjs.h"

#include "lsbusctl.h"
#include "lfsfilterpublic.h"

#include <iphlpapi.h> // for GetAdapterInfo

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASIX
#include "xdebug.h"

#define NDASIX_VERSION_MAJOR 3
#define NDASIX_VERSION_MINOR 20
#define NDASIX_VERSION_BUILD 2001

static inline bool IsEqualHostUsage(
	const NDAS_UNITDEVICE_HOST_USAGE& x, 
	const NDAS_UNITDEVICE_HOST_USAGE& y);

static inline bool IsDuplicateEntry(
	const std::vector<NDAS_UNITDEVICE_HOST_USAGE>& vHostUsages, 
	const NDAS_UNITDEVICE_HOST_USAGE& hostUsage);

static
USHORT
pInfoXGetOSType(
	DWORD WinMajorVer, 
	DWORD WinMinorVer);

static 
VOID
pGetOSVersion(
	LPDWORD lpdwMajorVersion, 
	LPDWORD lpdwMinorVersion);

static
BOOL
pGetAdapterPrimaryIpAddress(
	IN const DWORD cbAdapterAddress,
	IN const BYTE* pAdapterAddress,
	IN OUT LPDWORD pcbIpAddress,
	OUT LPBYTE pIpAddress);

//////////////////////////////////////////////////////////////////////////

CNdasIXServer::CNdasIXServer() :
	m_usListenPort(NDASIX_LISTEN_PORT),
	CTask(_T("NdasInfoExchangeServer Task"))
{
}

CNdasIXServer::~CNdasIXServer()
{
}

BOOL
CNdasIXServer::Initialize()
{
	BOOL fSuccess = m_dgs.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("CNdasIXServer init failed: "));
		return FALSE;
	}

	fSuccess = CTask::Initialize();

	return fSuccess;
}

VOID
CNdasIXServer::OnReceive(CLpxDatagramSocket& sock)
{
	SOCKADDR_LPX remoteAddr;
	DWORD cbReceived;
	BYTE* pPacket = NULL;
	DWORD dwRecvFlags;

	BOOL fSuccess = sock.GetRecvFromResult(
		&remoteAddr,
		&cbReceived,
		(BYTE**)&pPacket,
		&dwRecvFlags);

	//
	// Sanity Check
	//
	CONST LSINFOX_HEADER* pHeader = 
		reinterpret_cast<CONST LSINFOX_HEADER*>(pPacket);
	
	UCHAR ProtocolName[4] = INFOX_DATAGRAM_PROTOCOL;

	if (pHeader->Protocol[0] != ProtocolName[0] ||
		pHeader->Protocol[1] != ProtocolName[1] ||
		pHeader->Protocol[2] != ProtocolName[2] ||
		pHeader->Protocol[3] != ProtocolName[3])
	{
		DBGPRT_WARN(_FT("Invalid INFOX packet: protocol %c%c%c%c\n"),
			pHeader->Protocol[0],
			pHeader->Protocol[1],
			pHeader->Protocol[2],
			pHeader->Protocol[3]);
		return;
	}

	if (pHeader->MessageSize != cbReceived) {
		DBGPRT_WARN(
			_FT("Invalid packet size: Received %d bytes, Claimed %d bytes\n"),
			cbReceived,
			pHeader->MessageSize);
		return;
	}

	CONST LSINFOX_DATA* pData = 
		reinterpret_cast<CONST LSINFOX_DATA*>(pPacket + sizeof(LSINFOX_HEADER));

	switch (LSINFOX_TYPE_MAJTYPE & pHeader->Type) {
	case LSINFOX_PRIMARY_UPDATE_MESSAGE:
		{
			CONST LSINFOX_PRIMARY_UPDATE* pPrimaryUpdateData = 
				reinterpret_cast<const LSINFOX_PRIMARY_UPDATE*>(pData);

			OnIXPrimaryUpdate(sock,&remoteAddr,pPrimaryUpdateData);

			return;
		}
	case LSINFOX_PRIMARY_USAGE_MESSAGE:
		{
			CONST LSINFOX_NDASDEV_USAGE_REQUEST* pUsageRequest = 
				reinterpret_cast<const LSINFOX_NDASDEV_USAGE_REQUEST*>(pData);

			OnIXUsageRequest(sock,&remoteAddr,pUsageRequest);
			return;
		}

	default:
		
		return;
	}
}

VOID
CNdasIXServer::OnIXPrimaryUpdate(
	CLpxDatagramSocket& sock,
	CONST SOCKADDR_LPX* pRemoteAddr,
	CONST LSINFOX_PRIMARY_UPDATE* pData)
{
	NDAS_UNITDEVICE_ID unitDeviceId = { 
		{ 
			pData->NetDiskNode[0], pData->NetDiskNode[1],
			pData->NetDiskNode[2], pData->NetDiskNode[3],
			pData->NetDiskNode[4], pData->NetDiskNode[5]
		}, 
		pData->UnitDiskNo
	};

	CNdasUnitDevice* pUnitDevice = pGetNdasUnitDevice(unitDeviceId);
	if (NULL == pUnitDevice) {
		//
		// Discard non-discovered unit device
		//
		return;
	}

	NDAS_UNITDEVICE_PRIMARY_HOST_INFO hostinfo;

	::CopyMemory(
		hostinfo.Host.Node, 
		pData->PrimaryNode,
		sizeof(hostinfo.Host.Node));

	hostinfo.Host.Port = NTOHS(pData->PrimaryPort);
	hostinfo.SWMajorVersion = pData->SWMajorVersion;
	hostinfo.SWMinorVersion = pData->SWMinorVersion;
	hostinfo.SWBuildNumber = pData->SWBuildNumber;
	hostinfo.NDFSCompatVersion = pData->NDFSCompatVersion;
	hostinfo.NDFSVersion = pData->NDFSVersion;

	DPNoise(_FT("LSINFOX_PRIMATE_UPDATE_MESSAGE: %02X:%02X:%02X:%02X:%02X:%02X@%d\n"),
		pData->NetDiskNode[0],
		pData->NetDiskNode[1],
		pData->NetDiskNode[2],
		pData->NetDiskNode[3],
		pData->NetDiskNode[4],
		pData->NetDiskNode[5],
		pData->UnitDiskNo);

	pUnitDevice->UpdatePrimaryHostInfo(hostinfo);

	return;
}

VOID
CNdasIXServer::OnIXUsageRequest(
	CLpxDatagramSocket& sock,
	CONST SOCKADDR_LPX* pRemoteAddr,
	CONST LSINFOX_NDASDEV_USAGE_REQUEST* pData)
{
	NDAS_UNITDEVICE_ID unitDeviceId = {
		{ 
			pData->NetDiskNode[0], pData->NetDiskNode[1],
			pData->NetDiskNode[2], pData->NetDiskNode[3],
			pData->NetDiskNode[4], pData->NetDiskNode[5]
		},
		pData->UnitDiskNo
	};

	DPNoise(_FT("LSINFOX_PRIMARY_USAGE_MESSAGE: %02X:%02X:%02X:%02X:%02X:%02X@%d\n"),
		pData->NetDiskNode[0],
		pData->NetDiskNode[1],
		pData->NetDiskNode[2],
		pData->NetDiskNode[3],
		pData->NetDiskNode[4],
		pData->NetDiskNode[5],
		pData->UnitDiskNo);

	CNdasLogicalDevice* pLogDevice = pGetNdasLogicalDevice(unitDeviceId);
	if (NULL == pLogDevice) {
		// Discard message
		return;
	}

	switch (pLogDevice->GetStatus()) {
	case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
	case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
		break;
	default:
		//
		// Otherwise, discard message
		//
		return;
	}

	ACCESS_MASK mountedAcces = pLogDevice->GetMountedAccess();
	
	CONST DWORD cbPacket = 
		sizeof(LSINFOX_HEADER) + 
		sizeof(LSINFOX_NDASDEV_USAGE_REPLY) +
		MAX_HOSTNAME_LEN * sizeof(WCHAR);

	BYTE pbPacket[cbPacket] = {0};

	PLSINFOX_HEADER pHeader = 
		reinterpret_cast<PLSINFOX_HEADER>(pbPacket);

	PLSINFOX_NDASDEV_USAGE_REPLY pUsageReply = 
		reinterpret_cast<PLSINFOX_NDASDEV_USAGE_REPLY>(
		pbPacket + sizeof(LSINFOX_HEADER));

	//
	// Header
	//
	CONST BYTE NdasIxProtocolName[] = INFOX_DATAGRAM_PROTOCOL; 
	
	::CopyMemory(
		pHeader->Protocol, 
		NdasIxProtocolName, 
		sizeof(NdasIxProtocolName));

	pHeader->LSInfoXMajorVersion = INFOX_DATAGRAM_MAJVER;
	pHeader->LSInfoXMinorVersion = INFOX_DATAGRAM_MINVER;
	pHeader->OsMajorType = OSTYPE_WINDOWS;

	DWORD dwOSMajorVersion, dwOSMinorVersion;
	pGetOSVersion(&dwOSMajorVersion, &dwOSMinorVersion);
	USHORT usLfsOsMinorType = 
		pInfoXGetOSType(dwOSMajorVersion, dwOSMinorVersion);

	pHeader->OsMinorType = usLfsOsMinorType;
	pHeader->Type = LSINFOX_PRIMARY_UPDATE_MESSAGE | LSINFOX_TYPE_REPLY;
	pHeader->MessageSize = cbPacket;

	//
	// Body
	//

	LPX_ADDRESS localLpxAddress = sock.GetBoundAddr()->LpxAddress;

	pUsageReply->HostLanAddr.AddressType = LSNODE_ADDRTYPE_ETHER;
	pUsageReply->HostLanAddr.AddressLen = LPXADDR_NODE_LENGTH;
	::CopyMemory(
		pUsageReply->HostLanAddr.Address,
		localLpxAddress.Node,
		LPXADDR_NODE_LENGTH);

	WCHAR wszHostName[MAX_HOSTNAME_LEN] = {0};
	USHORT hostNameType = LSNODENAME_DNSFULLYQ;
	DWORD cchHostName = MAX_HOSTNAME_LEN;
	
	BOOL fSuccess = ::GetComputerNameExW(
		ComputerNameDnsFullyQualified,
		wszHostName,
		&cchHostName);

	if (!fSuccess) {
		hostNameType = LSNODENAME_NETBOIS;
		cchHostName = MAX_HOSTNAME_LEN;
		fSuccess = ::GetComputerNameExW(
			ComputerNameNetBIOS,
			wszHostName,
			&cchHostName);
	}

	if (!fSuccess) {
		hostNameType = LSNODENAME_UNKNOWN;
		cchHostName = 0;
	}

	pUsageReply->HostNameType = hostNameType;
	pUsageReply->HostNameLength = cchHostName;
	::CopyMemory(
		pUsageReply->HostName,
		wszHostName,
		cchHostName * sizeof(WCHAR));

	//
	// LPX Address.Node is an adapter address.
	//
	const DWORD cbAdapterAddress = 6;
	BYTE pAdapterAddress[cbAdapterAddress] = {0};
	::CopyMemory(pAdapterAddress, localLpxAddress.Node, 6);

	DWORD cbIpAddress = 14; // TODO: why is this 14?
	BYTE pPrimaryIpAddress[14] = {0};

	fSuccess = pGetAdapterPrimaryIpAddress(
		cbAdapterAddress, 
		pAdapterAddress,
		&cbIpAddress,
		pPrimaryIpAddress);

	fSuccess = FALSE;

	if (!fSuccess) {

		DBGPRT_WARN_EX(_FT("Failed to get primary ip address of %s: "),
			CSockLpxAddr(sock.GetBoundAddr()).ToString());

		pUsageReply->HostWanAddr.AddressLen = 0;
		pUsageReply->HostWanAddr.AddressType = LSNODE_ADDRTYPE_IP;
		::ZeroMemory(pUsageReply->HostWanAddr.Address, LSNODE_ADDR_LENGTH);

	} else {
		pUsageReply->HostWanAddr.AddressLen = (USHORT) cbIpAddress;
		pUsageReply->HostWanAddr.AddressType = LSNODE_ADDRTYPE_IP;
		_ASSERTE(cbIpAddress <= LSNODE_ADDR_LENGTH);
		::CopyMemory(
			pUsageReply->HostWanAddr.Address,
			pPrimaryIpAddress, 
			cbIpAddress);
	}

	//
	// Software Versions, status, etc
	//
	if (mountedAcces & GENERIC_READ) {
		pUsageReply->AccessRight |= LSSESSION_ACCESS_READ;
	}
	if (mountedAcces & GENERIC_WRITE) {
		pUsageReply->AccessRight |= LSSESSION_ACCESS_WRITE;
	}

	pUsageReply->NetDiskPort = NDAS_DEVICE_LPX_PORT;
	pUsageReply->UnitDiskNo = unitDeviceId.UnitNo;
	pUsageReply->UsageID = 0;
	pUsageReply->SWMajorVersion = NDASIX_VERSION_MAJOR;
	pUsageReply->SWMinorVersion = NDASIX_VERSION_MINOR;
	pUsageReply->SWBuildNumber = NDASIX_VERSION_BUILD;
	pUsageReply->NDFSCompatVersion = NDFS_COMPAT_VERSION;
	pUsageReply->NDFSVersion = NDFS_VERSION;

	DWORD cbSent = 0;
	fSuccess = sock.SendToSync(pRemoteAddr, cbPacket, pbPacket, 0, &cbSent);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Failed to send a reply (%d bytes): "), cbPacket);
		return;
	}

	return;
}

DWORD
CNdasIXServer::OnTaskStart()
{
	DBGPRT_INFO(_FT("Starting NdasIXServer.\n"));

	BOOL fSuccess = m_dgs.Receive(
		this,
		m_usListenPort,
		INFOX_MAX_DATAGRAM_PKT_SIZE,
		m_hTaskTerminateEvent);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Listening IXServer at port %d failed: "), m_usListenPort);
		return 255;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

CONST USHORT 
CNdasIXBcast::NDASIX_BROADCAST_PORT = LPXRP_LSHELPER_INFOEX;

CONST SOCKADDR_LPX 
CNdasIXBcast::NDASIX_BCAST_ADDR = { 
	AF_LPX, 
	{HTONS(CNdasIXBcast::NDASIX_BROADCAST_PORT), 
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}
};

CNdasIXBcast::CNdasIXBcast() :
	m_lpSocketAddressList(NULL),
	ximeta::CTask(_T("NdasIXBroadcaster Task"))
{
}

CNdasIXBcast::~CNdasIXBcast()
{
	DWORD err = ::GetLastError();
	if (NULL != m_lpSocketAddressList) {
		::LocalFree(m_lpSocketAddressList);
	}
	::SetLastError(err);
}

BOOL
CNdasIXBcast::Initialize()
{
	BOOL fSuccess = FALSE;
	for (DWORD i = 0; i < m_nSenders; ++i) {
		fSuccess = m_senders[i].Initialize();
		if (!fSuccess) {
			return FALSE;
		}
	}

	fSuccess = m_sockAddrChangeNotifier.Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = m_sockAddrChangeNotifier.Reset();
	if (!fSuccess) {
		return FALSE;
	}

	return CTask::Initialize();
}

BOOL
CNdasIXBcast::BroadcastStatus()
{
	BUSENUM_QUERY_INFORMATION BusEnumQuery;
	BUSENUM_INFORMATION BusEnumInformation;

	CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();

	ximeta::CAutoLock autolock(pLdm);
	CNdasLogicalDeviceManager::ConstIterator itr = pLdm->begin();

	for (;itr != pLdm->end(); ++itr) {

		CNdasLogicalDevice* pLogDevice = itr->second;
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != pLogDevice->GetStatus()) {
			continue;
		}

		CONST NDAS_SCSI_LOCATION& location = pLogDevice->GetNdasScsiLocation();

		BusEnumQuery.InfoClass = INFORMATION_PDO;
		BusEnumQuery.Size = sizeof(BUSENUM_QUERY_INFORMATION);
		BusEnumQuery.SlotNo = location.SlotNo;

		BOOL fSuccess = LsBusCtlQueryInformation(
			&BusEnumQuery,
			sizeof(BUSENUM_QUERY_INFORMATION),
			&BusEnumInformation,
			sizeof(BUSENUM_INFORMATION));

		if (!fSuccess) {
			DPErrorEx(_FT("LanscsiQueryInformation failed at slot %d: "), location.SlotNo);
			continue;
		}

		//
		// Broadcast a primary write access status
		//
		if (ND_ACCESS_ISRW(BusEnumInformation.PdoInfo.GrantedAccess)) {

			const DWORD cbBuffer = 
				sizeof(LSINFOX_HEADER) + sizeof(LSINFOX_PRIMARY_UPDATE);

			BYTE lpbBuffer[cbBuffer] = {0};

			PLSINFOX_HEADER pixHeader = 
				reinterpret_cast<PLSINFOX_HEADER>(lpbBuffer);

			PLSINFOX_DATA pixData = 
				reinterpret_cast<PLSINFOX_DATA>(lpbBuffer + sizeof(LSINFOX_HEADER));

			//
			// CAUTION: InfoExchange Protocol uses little-endian (Intel)
			//

			//
			// Header
			//
			UCHAR ixProtocolName[4] = INFOX_DATAGRAM_PROTOCOL;
			::CopyMemory(pixHeader->Protocol, ixProtocolName, 4);
			pixHeader->LSInfoXMajorVersion = INFOX_DATAGRAM_MAJVER;
			pixHeader->LSInfoXMinorVersion = INFOX_DATAGRAM_MINVER;
			pixHeader->OsMajorType = OSTYPE_WINDOWS;
			
			DWORD dwOSMajorVersion, dwOSMinorVersion;
			pGetOSVersion(&dwOSMajorVersion, &dwOSMinorVersion);
			USHORT usLfsOsMinorType = 
				pInfoXGetOSType(dwOSMajorVersion, dwOSMinorVersion);
			pixHeader->OsMinorType = usLfsOsMinorType;
			pixHeader->Type = LSINFOX_PRIMARY_UPDATE_MESSAGE;
			pixHeader->MessageSize = cbBuffer;

			//
			// Data
			//

			// primary node is dependent to each interface
			pixData->Update.PrimaryNode; 
			pixData->Update.PrimaryPort = LPXRP_LFS_PRIMARY;
			pixData->Update.SWMajorVersion = NDASIX_VERSION_MAJOR; // TODO: Change these values
			pixData->Update.SWMinorVersion = NDASIX_VERSION_MINOR;
			pixData->Update.SWBuildNumber = NDASIX_VERSION_BUILD;
			pixData->Update.NDFSCompatVersion = NDFS_COMPAT_VERSION;
			pixData->Update.NDFSVersion = NDFS_VERSION;

			//
			// NetDisk Node is a property of each unit device
			//
			pixData->Update.NetDiskNode;
			//
			// We have fixed the port 
			// (CNdasDevice does not store Port Number internally)
			// Do not try to retrieve from GetRemoteLpxAddress()
			//
			pixData->Update.NetDiskPort = NDAS_DEVICE_LPX_PORT;

			//
			// Unit Disk Number is a property of each unit device
			//
			pixData->Update.UnitDiskNo;

			//
			// pLogDevice->GetStatus()
			//
			for (DWORD n = 0; n < pLogDevice->GetUnitDeviceCount(); ++n) {
				//
				// Actually, we should traverse the real entry
				// from the device registrar.
				// However, here we do the shortcut for using NDAS device id
				// and fixed NetDiskPort, etc.
				//
				NDAS_UNITDEVICE_ID unitDeviceId = pLogDevice->GetUnitDeviceID(n);

				_ASSERTE(sizeof(pixData->Update.NetDiskNode) ==
					sizeof(unitDeviceId.DeviceId));

				::CopyMemory(
					pixData->Update.NetDiskNode,
					unitDeviceId.DeviceId.Node,
					sizeof(pixData->Update.NetDiskNode));

				pixData->Update.UnitDiskNo = unitDeviceId.UnitNo;

				//
				// Broadcast the data to every interface
				//
				for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {

					if (INVALID_SOCKET != (SOCKET) m_senders[i]) {
						
						//
						// Fill the Primary Node (LPX Address Node)
						//
						LPX_ADDRESS localLpxAddress = 
							m_senders[i].GetBoundAddr()->LpxAddress;

						_ASSERTE(sizeof(pixData->Update.PrimaryNode) ==
							sizeof(localLpxAddress.Node));

						::CopyMemory(
							pixData->Update.PrimaryNode,
							localLpxAddress.Node,
							sizeof(pixData->Update.PrimaryNode));
						
						//
						// Send the data
						//
						DWORD cbSent = 0;
						BOOL fSuccess =	m_senders[i].SendToSync(
								&NDASIX_BCAST_ADDR,
								cbBuffer,
								lpbBuffer,
								0,
								&cbSent);
						if (!fSuccess) {
							DPWarningEx(_FT("Sending a packet failed: "));
						}

					} // end if
				} // for each local LPX address
			} // for each unit device
		} // if the logical device has a primary write access.
	} // for each logical device

	return TRUE;
}

VOID
CNdasIXBcast::ResetBind()
{
	BOOL fSuccess = FALSE;

	if (NULL != m_lpSocketAddressList) {
		::LocalFree(m_lpSocketAddressList);
		m_lpSocketAddressList = NULL;
	}

	while (NULL == m_lpSocketAddressList) {
		m_lpSocketAddressList = pCreateLocalLpxAddressList();
		if (NULL == m_lpSocketAddressList) {
			DBGPRT_WARN_EX(_FT("Getting local lpx address list failed. Retry in 5 sec: "));
			// try to get address list again in 5 sec
			// we should terminate this routine at a task terminate event
			DWORD dwWaitResult = ::WaitForSingleObject(m_hTaskTerminateEvent, 5000);
			if (WAIT_OBJECT_0 == dwWaitResult) {
				return;
			}
		}
	}
	
	fSuccess = m_sockAddrChangeNotifier.Reset();
	// _ASSERTE(fSuccess);
	if (!fSuccess) {
		DBGPRT_WARN(_FT("Resetting sockAddrChangeNotifier failed: "));
	}

	DWORD nLocalAddrs =
		min((DWORD)m_lpSocketAddressList->iAddressCount, m_nSenders);

	for (DWORD i = 0; i < m_nSenders; ++i) {
		if (INVALID_SOCKET != (SOCKET)m_senders[i]) {
			m_senders[i].Close();
		}
	}

	for (DWORD i = 0; i < nLocalAddrs && i < m_nSenders; ++i) {

		PSOCKADDR_LPX pSockAddr = (PSOCKADDR_LPX)
			m_lpSocketAddressList->Address[i].lpSockaddr;
		pSockAddr->LpxAddress.Port = 0;

		fSuccess = m_senders[i].Create();
		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Creating a socket failed: "));
			continue;
		}

		//
		// This is a broadcast socket
		//
		BOOL bBroadcast = TRUE;
		fSuccess = m_senders[i].SetSockOpt(
			SO_BROADCAST, 
			(CONST BYTE*)&bBroadcast, 
			sizeof(BOOL));

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Setting a sock option to broadcast failed: "));
			(VOID) m_senders[i].Close();
			continue;
		}

		fSuccess = m_senders[i].Bind(pSockAddr);
		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Binding a sock %d to %s failed: "),
				i, CSockLpxAddr(pSockAddr).ToString());
			(VOID) m_senders[i].Close();
			continue;
		}
	}

}

DWORD
CNdasIXBcast::OnTaskStart()
{
	HANDLE hEvents[2];
	hEvents[0] = m_hTaskTerminateEvent;
	hEvents[1] = m_sockAddrChangeNotifier.GetChangeEvent();

	// CTask::Initialized called?
	_ASSERTE(NULL != hEvents[0]);
	// m_sockAddrChangeNotifier is initialized?
	_ASSERTE(NULL != hEvents[1]);

	//
	// Initial bind
	//
	ResetBind();

	//
	// initial LPX socket address list is attained
	//
	DWORD dwTimeout = 1000; // broadcast interval
	while (1) {

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			2, hEvents, FALSE, dwTimeout);

		if (WAIT_OBJECT_0 == dwWaitResult) {
			return 0;
		} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {
			// reset bind
			ResetBind();
		} else if (WAIT_TIMEOUT == dwWaitResult) {
			BroadcastStatus();
		} else {
			DPErrorEx(_FT("Unexpected wait result %d: "), dwWaitResult);
			_ASSERTE(FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

//const SOCKADDR_LPX 
//CNdasInfoExchangeUsage::NDASIX_BCAST_ADDR = { 
//	AF_LPX, 
//	{HTONS(INFOEX_PORT), 
//	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}};
//
//const BYTE 
//CNdasInfoExchangeUsage::NDASIX_PROTOCOL_NAME[4] = INFOX_DATAGRAM_PROTOCOL;
//
//CNdasInfoExchangeUsage::CNdasInfoExchangeUsage(
//	const NDAS_UNITDEVICE_ID& unitDeviceId,
//	DWORD dwExpectedReplies) :
//	m_unitDeviceId(unitDeviceId),
//	m_dwExpectedReplies(dwExpectedReplies),
//	m_dwLocalAddresses(0),
//	m_hTimer(NULL)
//{
//	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
//		::ZeroMemory(&m_wsaBuffers[i], sizeof(WSABUF));
//		::ZeroMemory(&m_overlapped[i], sizeof(OVERLAPPED));
//		m_socks[i] = INVALID_SOCKET;
//		::ZeroMemory(&m_saLocalAddresses[i], sizeof(SOCKADDR_LPX));
//		m_hDataEvents[i] = NULL;
//		::ZeroMemory(&m_ppbRecvBuf[i], sizeof(BYTE) * INFOX_MAX_DATAGRAM_PKT_SIZE);
//	}
//
//}
//
//CNdasInfoExchangeUsage::~CNdasInfoExchangeUsage()
//{
//	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
//		if (NULL != m_hDataEvents) {
//			BOOL fSuccess = ::WSACloseEvent(m_hDataEvents);
//			if (!fSuccess) {
//				DPWarningEx(_FT("Closing an event %d failed: "));
//			}
//		}
//	}
//
//	if (NULL != m_hTimer) {
//		BOOL fSuccess = ::CloseHandle(m_hTimer);
//		if (!fSuccess) {
//			DPWarningEx(_FT("Closing a timer handle failed: "));
//		}
//	}
//
//	CleanupSockets();
//}
//
//BOOL
//CNdasInfoExchangeUsage::Initialize()
//{
//	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
//
//		if (NULL == m_hDataEvents[i]) {
//			m_hDataEvents[i] = ::WSACreateEvent();
//		}
//
//		if (NULL == m_hDataEvents[i]) {
//			DPErrorEx(_FT("Creating WSAEvent failed: "));
//			return FALSE;
//		}
//	}
//
//	if (NULL == m_hTimer) {
//		m_hTimer = ::CreateWaitableTimer(NULL, TRUE, NULL);
//	}
//
//	if (NULL == m_hTimer) {
//		return FALSE;
//	}
//
//	return TRUE;
//}
//
//VOID
//CNdasInfoExchangeUsage::CleanupSockets()
//{
//	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
//		if (INVALID_SOCKET != m_socks[i]) {
//			
//			INT iResult = ::closesocket(m_socks[i]);
//			if (0 != iResult) {
//				DPWarningEx(_FT("Closing a socket %d failed: "));
//			}
//
//		}
//	}
//}
//
//BOOL
//CNdasInfoExchangeUsage::InitSockets()
//{
//	//
//	// Initialize sockets for sending data
//	// 
//	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {
//
//		m_socks[i] = INVALID_SOCKET;
//
//		SOCKET sock = ::WSASocket(
//			AF_LPX, 
//			SOCK_DGRAM, 
//			IPPROTO_LPXUDP, 
//			NULL, 
//			0, 
//			0);
//
//		//
//		// INVALID_SOCKET will be checked when sending actual data
//		//
//		if (INVALID_SOCKET == sock) {
//			DPErrorEx(_FT("Creating a socket failed on interface %s: "),
//				CLpxAddress(m_saLocalAddresses[i].LpxAddress).ToString());
//			continue;
//		}
//
//		BOOL bBroadcast = TRUE;
//		INT iResult = ::setsockopt(
//			sock, 
//			SOL_SOCKET, 
//			SO_BROADCAST, 
//			(char*)&bBroadcast, 
//			sizeof(bBroadcast));
//
//		if (0 != iResult) {
//			DPErrorEx(_FT("Setting socket options failed on interface %s: "),
//				CLpxAddress(m_saLocalAddresses[i].LpxAddress).ToString());
//			::closesocket(sock);
//			continue;
//		}
//
//		iResult = ::bind(
//			sock,
//			(struct sockaddr*) &m_saLocalAddresses[i],
//			sizeof(SOCKADDR_LPX));
//
//		if (0 != iResult) {
//			DPErrorEx(_FT("Binding a socket failed on interface %s: "),
//				CLpxAddress(m_saLocalAddresses[i].LpxAddress).ToString());
//			::closesocket(sock);
//			continue;
//		}
//
//		m_socks[i] = sock;
//	}
//
//	return TRUE;
//}
//
//BOOL
//CNdasInfoExchangeUsage::ResetLocalAddressList()
//{
//	//
//	// Get the list of local LPX addresses
//	//
//
//	DWORD cbAddressList(0);
//	BOOL fSuccess = GetLocalLpxAddressList(0, NULL, &cbAddressList);
//	if (!fSuccess && WSAEFAULT != ::WSAGetLastError()) {
//		DPErrorEx(_FT("Getting local address list size failed: "));
//		return FALSE;
//	}
//
//	LPSOCKET_ADDRESS_LIST lpSockAddrList = (LPSOCKET_ADDRESS_LIST)
//		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbAddressList);
//
//	if (NULL == lpSockAddrList) {
//		DPErrorEx(_FT("Memory allocation failed: "));
//		return FALSE;
//	}
//
//	m_dwLocalAddresses = lpSockAddrList->iAddressCount;
//	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {
//		m_saLocalAddresses[i] = * reinterpret_cast<SOCKADDR_LPX*>(
//			lpSockAddrList->Address[i].lpSockaddr);
//		m_saLocalAddresses[i].sin_family = AF_LPX;
//		m_saLocalAddresses[i].LpxAddress.Port = 0;
//	}
//
//	(VOID) ::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
//
//	return TRUE;
//}
//
//BOOL
//CNdasInfoExchangeUsage::SendRequests()
//{
//	const DWORD cbPacket = sizeof(LSINFOX_HEADER) + 
//		sizeof(LSINFOX_NDASDEV_USAGE_REQUEST);
//
//	BYTE pPacket[cbPacket] = {0};
//
//	PLSINFOX_HEADER pHeader = 
//		reinterpret_cast<PLSINFOX_HEADER>(pPacket);
//
//	PLSINFOX_NDASDEV_USAGE_REQUEST pRequest = 
//		reinterpret_cast<PLSINFOX_NDASDEV_USAGE_REQUEST>(
//		pPacket + sizeof(LSINFOX_HEADER));
//
//	DWORD dwOsVerMajor, dwOsVerMinor;
//	pGetOSVersion(&dwOsVerMajor, &dwOsVerMinor);
//	USHORT usOsMinorType = pInfoXGetOSType(dwOsVerMajor, dwOsVerMinor);
//
//	_ASSERTE(sizeof(pHeader->Protocol) == sizeof(NDASIX_PROTOCOL_NAME));
//	::CopyMemory(
//		pHeader->Protocol, 
//		NDASIX_PROTOCOL_NAME, 
//		sizeof(NDASIX_PROTOCOL_NAME));
//
//	pHeader->LSInfoXMajorVersion = INFOX_DATAGRAM_MAJVER;
//	pHeader->LSInfoXMinorVersion = INFOX_DATAGRAM_MINVER;
//	pHeader->OsMajorType = OSTYPE_WINDOWS;
//	pHeader->OsMinorType = usOsMinorType;
//	pHeader->Type = LSINFOX_PRIMARY_USAGE_MESSAGE | LSINFOX_TYPE_REQUEST |
//		LSINFOX_TYPE_BROADCAST | LSINFOX_TYPE_DATAGRAM;
//	pHeader->MessageSize = cbPacket;
//
//	_ASSERTE(sizeof(pRequest->NetDiskNode) == sizeof(m_unitDeviceId.DeviceId.Node));
//	::CopyMemory(
//		pRequest->NetDiskNode, 
//		m_unitDeviceId.DeviceId.Node,
//		sizeof(pRequest->NetDiskNode));
//
//	//
//	// Packet data is of little-endian.
//	//
//	pRequest->NetDiskPort = NDAS_DEVICE_LPX_PORT;
//	pRequest->UnitDiskNo = m_unitDeviceId.UnitNo;
//
//	//
//	// Send the request
//	//
//	WSABUF wsaSendingBuffer = { cbPacket, (char*) pPacket };
//	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {
//
//		//
//		// Synchronous send
//		//
//		DWORD cbSent(0);
//		
//		//
//		// Ignore INVALID_SOCKETS
//		//
//		if (INVALID_SOCKET == m_socks[i]) {
//			continue;
//		}
//
//		INT iResult = ::WSASendTo(
//			m_socks[i],
//			&wsaSendingBuffer,
//			1,
//			&cbSent,
//			0,
//			(struct sockaddr *)&NDASIX_BCAST_ADDR,
//			sizeof(SOCKADDR_LPX),
//			NULL,
//			NULL);
//
//		if (0 != iResult) {
//			DPErrorEx(_FT("WSASendTo failed: "));
//		}
//
//	}
//
//	return TRUE;
//}

//
// Host Usage Set Trait (less)
//
// for checking duplicate packets for the same NDAS host 
//

//static inline bool IsEqualHostUsage(
//	const NDAS_UNITDEVICE_HOST_USAGE& x, 
//	const NDAS_UNITDEVICE_HOST_USAGE& y)
//{
//	if (x.HostLanAddr.AddressType == y.HostLanAddr.AddressType &&
//		x.HostLanAddr.AddressLen == y.HostLanAddr.AddressLen &&
//		::memcmp(x.HostLanAddr.Address, y.HostLanAddr.Address, 
//		x.HostLanAddr.AddressLen) == 0&&
//		x.UsageId == y.UsageId)
//	{
//		return true;
//	}
//
//	if (x.HostNameLength == y.HostNameLength &&
//		::memcmp(x.HostName, y.HostName, x.HostNameLength) == 0 &&
//		x.UsageId == y.UsageId)
//	{
//		return true;
//	}
//
//	return false;
//}
//
//static inline bool IsDuplicateEntry(
//	const std::vector<NDAS_UNITDEVICE_HOST_USAGE>& vHostUsages, 
//	const NDAS_UNITDEVICE_HOST_USAGE& hostUsage)
//{
//	std::vector<NDAS_UNITDEVICE_HOST_USAGE>::const_iterator itr =
//		vHostUsages.begin();
//
//	while (itr != vHostUsages.end()) {
//		const NDAS_UNITDEVICE_HOST_USAGE& existingHostUsage = *itr;
//		if (IsEqualHostUsage(existingHostUsage, hostUsage)) {
//			return true;
//		}
//		++itr;
//	}
//
//	return false;
//}
//
//BOOL
//CNdasInfoExchangeUsage::CollectReplies()
//{
//	m_dwROHosts = 0;
//	m_dwRWHosts = 0;
//	m_vHostUsages.clear();
//
//	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {
//
//		DWORD cbReceived(0);
//
//		m_overlapped[i].hEvent = m_hDataEvents[i];
//		m_wsaBuffers[i].buf = (char*)m_ppbRecvBuf[i];
//		m_wsaBuffers[i].len = INFOX_MAX_DATAGRAM_PKT_SIZE;
//
//		//
//		// lpFlags, lpFrom, lpFromLen and lpOverlapped
//		// MUST be available until the completion of the overlapped operation.
//		// 
//
//		m_iFromLen[i] = sizeof(SOCKADDR_LPX);
//		m_dwRecvFlags[i] = MSG_PARTIAL;
//
//		INT iResult = ::WSARecvFrom(
//			m_socks[i],
//			&m_wsaBuffers[i],
//			1,
//			&cbReceived,
//			&m_dwRecvFlags[i],
//			(struct sockaddr*)&m_saLocalAddresses[i],
//			&m_iFromLen[i],
//			&m_overlapped[i],
//			NULL);
//
//		if (0 == iResult) {
//			//
//			// Data is available (this case should not occur??)
//			//
//			BOOL fSuccess = ::WSASetEvent(m_hDataEvents[i]);
//			_ASSERTE(fSuccess);
//		} else if (0 != iResult && WSA_IO_PENDING != ::WSAGetLastError()) {
//			DPErrorEx(_FT("Receiving from %d-th socket failed: "));
//		}
//
//	}
//
//	DWORD dwTimeoutCount(0), dwLoopCount(0);
//
//	LARGE_INTEGER liTimeout;
//	//
//	// 100 nanosec scale
//	// Negative value indicates relative time
//	//
//	liTimeout.QuadPart = - LONGLONG(NDASIX_COLLECT_TIMEOUT) * 10L * 1000L; 
//
//	BOOL fSuccess = ::SetWaitableTimer(m_hTimer, &liTimeout, 0, NULL, NULL, FALSE);
//	_ASSERTE(fSuccess);
//
//	HANDLE hWaitHandles[MAX_SOCKETLPX_INTERFACE + 1];
//	hWaitHandles[0] = m_hTimer;
//	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
//		hWaitHandles[i+1] = (HANDLE)m_hDataEvents[i];
//	}
//
//	while (dwLoopCount < m_dwExpectedReplies + 1 &&
//		dwTimeoutCount < USAGE_TIMEOUT_LOOP)
//	{
//		DWORD dwWaitResult = ::WaitForMultipleObjects(
//			MAX_SOCKETLPX_INTERFACE + 1,
//			hWaitHandles,
//			FALSE,
//			USAGE_TIMEOUT);
//
//		if (WAIT_TIMEOUT == dwWaitResult) {
//
//			++dwTimeoutCount;
//			DPInfo(_FT("Wait timed out (%d).\n"), dwTimeoutCount);
//
//		} else if (WAIT_OBJECT_0 == dwWaitResult) {
//
//			DPWarning(_FT("Reply data collection timed out.\n"));
//
//		} else if (WAIT_OBJECT_0 >= dwWaitResult + 1&& 
//			dwWaitResult <= WAIT_OBJECT_0 + m_dwLocalAddresses + 1)
//		{
//			DWORD n = dwWaitResult - WAIT_OBJECT_0 - 1;
//			DWORD cbReceived(0), dwFlags(0);
//
//			PBYTE pReceivedData = m_ppbRecvBuf[i];
//
//			BOOL fSuccess = ::WSAGetOverlappedResult(
//				m_socks[n],
//				&m_overlapped[n],
//				&cbReceived,
//				TRUE,
//				&dwFlags);
//
//			if (!fSuccess) {
//				DPErrorEx(_FT("Getting overlapped result failed: "));
//				continue;
//			}
//
//			//
//			// Check the received packet
//			//
//			if (cbReceived < sizeof(LSINFOX_HEADER)) {
//				DPWarning(_FT("Invalid packet received: size too small, ")
//					_T("Expected at least %d, Received %d\n"),
//					sizeof(LSINFOX_HEADER), cbReceived);
//				continue;
//			}
//
//			PLSINFOX_HEADER pHeader = 
//				reinterpret_cast<PLSINFOX_HEADER>(pReceivedData);
//
//			//
//			// Sanity check
//			//
//
//			//
//			// Protocol Header
//			//
//			int iResult = ::memcmp(
//				pHeader->Protocol, 
//				NDASIX_PROTOCOL_NAME, 
//				sizeof(NDASIX_PROTOCOL_NAME));
//
//			if (0 != iResult) {
//				DPWarning(_FT("Invalid packet received: invalid protocol, ")
//					_T("%c%c%c%c \n"), 
//					pHeader->Protocol[0], pHeader->Protocol[1],
//					pHeader->Protocol[2], pHeader->Protocol[3]);
//				continue;
//			}
//
//			//
//			// Packet size
//			//
//			if (cbReceived != pHeader->MessageSize) {
//				DPWarning(_FT("Invalid packet received: invalid packet size, ")
//					_T("Received %d, Claimed %d\n"), 
//					cbReceived, pHeader->MessageSize);
//				continue;
//			}
//
//			PLSINFOX_NDASDEV_USAGE_REPLY pReply =
//				reinterpret_cast<PLSINFOX_NDASDEV_USAGE_REPLY>(
//				pReceivedData + sizeof(LSINFOX_HEADER));
//
//
//			NDAS_UNITDEVICE_HOST_USAGE hostUsage;
//
//			hostUsage.HostLanAddr.AddressType = pReply->HostLanAddr.AddressType;
//			hostUsage.HostLanAddr.AddressLen = pReply->HostLanAddr.AddressLen;
//			::CopyMemory(
//				hostUsage.HostLanAddr.Address,
//				pReply->HostLanAddr.Address,
//				pReply->HostLanAddr.AddressLen);
//
//			hostUsage.HostWanAddr.AddressType = pReply->HostWanAddr.AddressType;
//			hostUsage.HostWanAddr.AddressLen = pReply->HostWanAddr.AddressLen;
//			::CopyMemory(
//				hostUsage.HostWanAddr.Address,
//				pReply->HostWanAddr.Address,
//				pReply->HostWanAddr.AddressLen);
//
//			hostUsage.HostNameLength = pReply->HostNameLength;
//			::CopyMemory(
//				hostUsage.HostName,
//				pReply->HostName,
//				pReply->HostNameLength);
//
//			hostUsage.UsageId = pReply->UsageID;
//			hostUsage.AccessRight = pReply->AccessRight;
//
//			//
//			// Check duplicate entry
//			//
//			if (IsDuplicateEntry(m_vHostUsages, hostUsage)) {
//				continue;
//			}
//
//			if (hostUsage.AccessRight & LSSESSION_ACCESS_WRITE) {
//				++m_dwRWHosts;
//			} else {
//				++m_dwROHosts;
//			}
//			
//		} else {
//			DPErrorEx(_FT("Wait failed: "));
//		}
//	}
//	
//	return TRUE;
//}
//
//BOOL
//CNdasInfoExchangeUsage::Query()
//{
//	CleanupSockets();
//
//	BOOL fSuccess = ResetLocalAddressList();
//	if (!fSuccess) {
//		DPErrorEx(_FT("Resetting local address list failed: "));
//	}
//
//	fSuccess = InitSockets();
//	if (!fSuccess) {
//		DPErrorEx(_FT("Socket initialization failed: "));
//		return FALSE;
//	}
//
//	fSuccess = SendRequests();
//	if (!fSuccess) {
//		DPErrorEx(_FT("Sending requests failed: "));
//		return FALSE;
//	}
//
//	fSuccess = CollectReplies();
//	if (!fSuccess) {
//		DPErrorEx(_FT("Collecting replies failed: "));
//		return FALSE;
//	}
//
//	return TRUE;
//}
//
//BOOL 
//CNdasInfoExchangeUsage::GetHostUsage(
//	DWORD dwIndex, 
//	PNDAS_UNITDEVICE_HOST_USAGE pHostUsage)
//{
//	if (dwIndex >= m_vHostUsages.size()) {
//
//		DPError(_FT("Invalid index %d, limited to [0..%d).\n"), 
//			dwIndex, m_vHostUsages.size());
//
//		::SetLastError(ERROR_INVALID_PARAMETER);
//		return FALSE;
//	}
//
//	if (IsBadWritePtr(pHostUsage, sizeof(NDAS_UNITDEVICE_HOST_USAGE))) {
//
//		DPError(_FT("Pointer to the output parameter is incorrect:")
//			_T("pHostUsage=%p.\n"), pHostUsage);
//
//		::SetLastError(ERROR_INVALID_PARAMETER);
//		return FALSE;
//	}
//
//	::CopyMemory(
//		pHostUsage, 
//		&m_vHostUsages[dwIndex],
//		sizeof(NDAS_UNITDEVICE_HOST_USAGE));
//
//	return TRUE;
//}

//////////////////////////////////////////////////////////////////////////

static
USHORT
pInfoXGetOSType(DWORD WinMajorVer, DWORD WinMinorVer) 
{
	USHORT	InfoXOsMinorType;

	UNREFERENCED_PARAMETER(WinMajorVer);
	//
	//	determine OS minor type
	//
	_ASSERT(WinMajorVer == 5) ;
	if(WinMinorVer == 0) {
		InfoXOsMinorType = OSTYPE_WIN2K ;
	} else if(WinMinorVer == 1) {
		InfoXOsMinorType = OSTYPE_WINXP ;
	} else if(WinMinorVer == 2) {
		InfoXOsMinorType = OSTYPE_WIN2003SERV ;
	} else {
		InfoXOsMinorType = OSTYPE_UNKNOWN ;
	}

	return InfoXOsMinorType;
}

static 
VOID
pGetOSVersion(LPDWORD lpdwMajorVersion, LPDWORD lpdwMinorVersion)
{
	_ASSERTE(!IsBadWritePtr(lpdwMajorVersion, sizeof(DWORD)));
	_ASSERTE(!IsBadWritePtr(lpdwMinorVersion, sizeof(DWORD)));

	OSVERSIONINFOEX osvi;
	::ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	BOOL fSuccess = ::GetVersionEx((OSVERSIONINFO*) &osvi);
	_ASSERT(fSuccess);
	*lpdwMajorVersion = osvi.dwMajorVersion;
	*lpdwMinorVersion = osvi.dwMinorVersion;
}

static
BOOL
pGetAdapterPrimaryIpAddress(
	IN CONST DWORD cbAdapterAddress,
	IN CONST BYTE* pAdapterAddress,
	IN OUT LPDWORD pcbIpAddress,
	OUT LPBYTE pIpAddress)
{
	_ASSERTE(!IsBadReadPtr(pAdapterAddress, cbAdapterAddress));
	_ASSERTE(!IsBadWritePtr(pcbIpAddress, sizeof(DWORD)));
	_ASSERTE(!IsBadWritePtr(pIpAddress, *pcbIpAddress));

	ULONG ulOutBufLen(0);

	DWORD dwResult = ::GetAdaptersInfo(NULL, &ulOutBufLen);
	if (dwResult != ERROR_BUFFER_OVERFLOW) {
		DPErrorEx(_FT("Getting adapter info size failed: "));
		return FALSE;
	}

	PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		ulOutBufLen);

	if (NULL == pAdapterInfo) {
		DPErrorEx(_FT("Out of memory: "));
		return FALSE;
	}

	AutoProcessHeap autoAdapterInfo = pAdapterInfo;

	dwResult = ::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if (dwResult != ERROR_SUCCESS) {
		DPErrorEx(_FT("Getting adapter info failed: "));
		return FALSE;
	}

	PIP_ADAPTER_INFO pAdapter = pAdapterInfo;

	BOOL bFound(FALSE);

	while (NULL != pAdapter) {

		if (cbAdapterAddress == pAdapter->AddressLength &&
			::memcmp(pAdapterAddress, pAdapter->Address, cbAdapterAddress) == 0)
		{
			SOCKADDR_IN sockAddress;
			INT iSockAddressLength = sizeof(SOCKADDR);
			
			INT iResult = ::WSAStringToAddressA(
				pAdapter->IpAddressList.IpAddress.String,
				AF_INET,
				NULL,
				(PSOCKADDR)&sockAddress,
				&iSockAddressLength);

			if (0 != iResult) {
				DPErrorEx(_FT("WSAStringToAddress failed: "));
				break;
			}

			if (*pcbIpAddress < sizeof(sockAddress.sin_addr)) {
				*pcbIpAddress = sizeof(sockAddress.sin_addr);
				::SetLastError(ERROR_BUFFER_OVERFLOW);
				return FALSE;
			}

			*pcbIpAddress = sizeof(sockAddress.sin_addr);
			::CopyMemory(pIpAddress, &sockAddress.sin_addr, *pcbIpAddress);

			TCHAR wszIpAddress[17] = {0};
			::MultiByteToWideChar(
				CP_ACP, 
				0,
				pAdapter->IpAddressList.IpAddress.String,
				16,
				wszIpAddress,
				17);
			DPInfo(_FT("IP Address: %s\n"), wszIpAddress);

			bFound = TRUE;
			break;
		}

		pAdapter = pAdapter->Next;
	}

	if (!bFound) {
		DPWarning(_FT("No IP addresses are associated with the adapter.\n"));
	}

	return TRUE;

}

