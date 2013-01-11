#include "stdafx.h"
#include <iphlpapi.h> // for GetAdapterInfo
#include <ndasbusctl.h>
#include <lfsfiltctl.h>
#include <lfsfilterpublic.h>
#include <xtl/xtlautores.h>

#include "ndasix.h"
#include "lpxcomm.h"
#include "ndaslogdevman.h"
#include "ndaslogdev.h"
#include "ndasdev.h"
#include "ndasdevreg.h"
#include "ndasobjs.h"

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
	m_usListenPort(NDASIX_LISTEN_PORT)
{
}

CNdasIXServer::~CNdasIXServer()
{
}

bool
CNdasIXServer::Initialize()
{
	BOOL fSuccess = FALSE;

	fSuccess = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL,
		&m_NDFSVersion.wMajor,
		&m_NDFSVersion.wMinor);

	if (!fSuccess)
	{
		m_NDFSVersion.wMajor = 0;
		m_NDFSVersion.wMinor = 0;
	}

	return true;
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

	CNdasUnitDevicePtr pUnitDevice = pGetNdasUnitDevice(unitDeviceId);
	if (0 == pUnitDevice.get()) {
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

	CNdasLogicalDevicePtr pLogDevice = pGetNdasLogicalDevice(unitDeviceId);
	if (CNdasLogicalDeviceNullPtr == pLogDevice) {
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
		NDASIX_MAX_HOSTNAME_LEN * sizeof(WCHAR);

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
	pUsageReply->HostNameLength = static_cast<USHORT>(cchHostName);
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
		XTLASSERT(cbIpAddress <= LSNODE_ADDR_LENGTH);
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
	pUsageReply->UnitDiskNo = static_cast<USHORT>(unitDeviceId.UnitNo);
	pUsageReply->UsageID = 0;
	pUsageReply->SWMajorVersion = NDASIX_VERSION_MAJOR;
	pUsageReply->SWMinorVersion = NDASIX_VERSION_MINOR;
	pUsageReply->SWBuildNumber = NDASIX_VERSION_BUILD;
	pUsageReply->NDFSCompatVersion = m_NDFSVersion.wMajor;
	pUsageReply->NDFSVersion = m_NDFSVersion.wMinor;

	DWORD cbSent = 0;
	fSuccess = sock.SendToSync(pRemoteAddr, cbPacket, pbPacket, 0, &cbSent);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Failed to send a reply (%d bytes): "), cbPacket);
		return;
	}

	return;
}

DWORD
CNdasIXServer::ThreadStart(HANDLE hStopEvent)
{
	XTLTRACE("Starting NdasIXServer.\n");

	CLpxDatagramServer m_dgs;
	BOOL fSuccess = m_dgs.Initialize();
	if (!fSuccess) 
	{
		XTLTRACE_ERR("CNdasIXServer init failed.\n");
		return 255;
	}

	fSuccess = m_dgs.Receive(
		this,
		m_usListenPort,
		INFOX_MAX_DATAGRAM_PKT_SIZE,
		hStopEvent);

	if (!fSuccess) 
	{
		XTLTRACE_ERR("Listening IXServer at port %d failed: ", m_usListenPort);
		return 255;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

const USHORT 
CNdasIXBcast::NDASIX_BROADCAST_PORT = LPXRP_LSHELPER_INFOEX;

const SOCKADDR_LPX 
CNdasIXBcast::NDASIX_BCAST_ADDR = { 
	AF_LPX, 
	{HTONS(CNdasIXBcast::NDASIX_BROADCAST_PORT), 
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}
};

CNdasIXBcast::CNdasIXBcast() :
	m_lpSocketAddressList(NULL)
{
	::ZeroMemory(
		&m_NDFSVersion, 
		sizeof(m_NDFSVersion));
}

CNdasIXBcast::~CNdasIXBcast()
{
	DWORD err = ::GetLastError();
	if (NULL != m_lpSocketAddressList) 
	{
		::LocalFree(m_lpSocketAddressList);
	}
	::SetLastError(err);
}

bool
CNdasIXBcast::Initialize()
{
	BOOL fSuccess = FALSE;

	fSuccess = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL,
		&m_NDFSVersion.wMajor,
		&m_NDFSVersion.wMinor);

	if (!fSuccess)
	{
		m_NDFSVersion.wMajor = 0;
		m_NDFSVersion.wMinor = 0;
	}

	for (DWORD i = 0; i < m_nSenders; ++i) 
	{
		fSuccess = m_senders[i].Initialize();
		if (!fSuccess) 
		{
			return false;
		}
	}

	fSuccess = m_sockAddrChangeNotifier.Initialize();
	if (!fSuccess) 
	{
		return false;
	}

	fSuccess = m_sockAddrChangeNotifier.Reset();
	if (!fSuccess) 
	{
		return false;
	}

	return true;
}

BOOL
CNdasIXBcast::BroadcastStatus()
{
	BUSENUM_QUERY_INFORMATION BusEnumQuery;
	BUSENUM_INFORMATION BusEnumInformation;

	CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
	CNdasLogicalDeviceVector logDevices;
	manager.Lock();
	manager.GetItems(logDevices);
	manager.Unlock();

	for (CNdasLogicalDeviceVector::const_iterator itr = logDevices.begin();
		itr != logDevices.end();
		++itr)
	{
		CNdasLogicalDevicePtr pLogDevice = *itr;

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
			DBGPRT_ERR_EX(_FT("LanscsiQueryInformation failed at slot %d: "), location.SlotNo);
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
			pixData->Update.NDFSCompatVersion = m_NDFSVersion.wMajor;
			pixData->Update.NDFSVersion = m_NDFSVersion.wMinor;

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

				C_ASSERT(sizeof(pixData->Update.NetDiskNode) ==
					sizeof(unitDeviceId.DeviceId.Node));

				::CopyMemory(
					pixData->Update.NetDiskNode,
					unitDeviceId.DeviceId.Node,
					sizeof(pixData->Update.NetDiskNode));

				pixData->Update.UnitDiskNo = static_cast<USHORT>(unitDeviceId.UnitNo);

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

						C_ASSERT(sizeof(pixData->Update.PrimaryNode) ==
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
							DBGPRT_WARN_EX(_FT("Sending a packet failed: "));
						}

					} // end if
				} // for each local LPX address
			} // for each unit device
		} // if the logical device has a primary write access.
	} // for each logical device

	return TRUE;
}

VOID
CNdasIXBcast::ResetBind(HANDLE hStopEvent)
{
	BOOL fSuccess = FALSE;

	if (NULL != m_lpSocketAddressList) 
	{
		::LocalFree(m_lpSocketAddressList);
		m_lpSocketAddressList = NULL;
	}

	while (NULL == m_lpSocketAddressList) 
	{
		m_lpSocketAddressList = pCreateLocalLpxAddressList();
		if (NULL == m_lpSocketAddressList) 
		{
			DBGPRT_WARN_EX(_FT("Getting local lpx address list failed. Retry in 5 sec: "));
			// try to get address list again in 5 sec
			// we should terminate this routine at a task terminate event
			DWORD dwWaitResult = ::WaitForSingleObject(hStopEvent, 5000);
			if (WAIT_OBJECT_0 == dwWaitResult) 
			{
				return;
			}
		}
	}
	
	fSuccess = m_sockAddrChangeNotifier.Reset();
	// XTLASSERT(fSuccess);
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
CNdasIXBcast::ThreadStart(HANDLE hStopEvent)
{
	if (0 == m_NDFSVersion.wMajor && 0 == m_NDFSVersion.wMinor)
	{
		return 1;
	}

	HANDLE waitHandles[] = { 
		hStopEvent, 
		m_sockAddrChangeNotifier.GetChangeEvent()};
	const DWORD nWaitHandles = RTL_NUMBER_OF(waitHandles);

	// CTask::Initialized called?
	XTLASSERT(NULL != waitHandles[0]);
	// m_sockAddrChangeNotifier is initialized?
	XTLASSERT(NULL != waitHandles[1]);

	//
	// Initial bind
	//
	ResetBind(hStopEvent);

	//
	// initial LPX socket address list is attained
	//
	DWORD dwTimeout = 1000; // broadcast interval
	while (TRUE) 
	{
		DWORD waitResult = ::WaitForMultipleObjects(
			nWaitHandles, waitHandles, FALSE, dwTimeout);

		if (WAIT_OBJECT_0 == waitResult) 
		{
			return 0;
		}
		else if (WAIT_OBJECT_0 + 1 == waitResult) 
		{
			// reset bind
			ResetBind(hStopEvent);
		}
		else if (WAIT_TIMEOUT == waitResult) 
		{
			BroadcastStatus();
		}
		else 
		{
			XTLTRACE("Unexpected wait result %d.\n", waitResult);
			XTLASSERT(FALSE);
		}
	}
}

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
	// _ASSERT(WinMajorVer == 5) ;
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
	XTLASSERT(!IsBadWritePtr(lpdwMajorVersion, sizeof(DWORD)));
	XTLASSERT(!IsBadWritePtr(lpdwMinorVersion, sizeof(DWORD)));

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
	XTLASSERT(!IsBadReadPtr(pAdapterAddress, cbAdapterAddress));
	XTLASSERT(!IsBadWritePtr(pcbIpAddress, sizeof(DWORD)));
	XTLASSERT(!IsBadWritePtr(pIpAddress, *pcbIpAddress));

	ULONG ulOutBufLen(0);

	DWORD dwResult = ::GetAdaptersInfo(NULL, &ulOutBufLen);
	if (dwResult != ERROR_BUFFER_OVERFLOW) {
		DBGPRT_ERR_EX(_FT("Getting adapter info size failed: "));
		return FALSE;
	}

	PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		ulOutBufLen);

	if (NULL == pAdapterInfo) {
		DBGPRT_ERR_EX(_FT("Out of memory: "));
		return FALSE;
	}

	XTL::AutoProcessHeap autoAdapterInfo = pAdapterInfo;

	dwResult = ::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if (dwResult != ERROR_SUCCESS) 
	{
		DBGPRT_ERR_EX(_FT("Getting adapter info failed: "));
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
				DBGPRT_ERR_EX(_FT("WSAStringToAddress failed: "));
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
			DBGPRT_INFO(_FT("IP Address: %s\n"), wszIpAddress);

			bFound = TRUE;
			break;
		}

		pAdapter = pAdapter->Next;
	}

	if (!bFound) 
	{
		DBGPRT_WARN(_FT("No IP addresses are associated with the adapter.\n"));
	}

	return TRUE;

}

