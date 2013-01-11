#include "stdafx.h"
#include <iphlpapi.h> // for GetAdapterInfo
#include <ndasbusctl.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasportctl.h>
#include <lfsfiltctl.h>
#include <lfsfilterpublic.h>
#include <xtl/xtlautores.h>
#include <ntddscsi.h>
#include <ndas/ndasvolex.h>

#include "ndasdevid.h"
#include "ndasix.h"
#include "lpxcomm.h"
#include "ndasdevreg.h"
#include "ndasobjs.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasix.tmh"
#endif

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
	BOOL success = FALSE;

	success = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL,
		&m_NDFSVersion.wMajor,
		&m_NDFSVersion.wMinor);

	if (!success)
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

	BOOL success = sock.GetRecvFromResult(
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
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR, 
			"Invalid INFOX packet: protocol %c%c%c%c\n",
			pHeader->Protocol[0],
			pHeader->Protocol[1],
			pHeader->Protocol[2],
			pHeader->Protocol[3]);
		return;
	}

	if (pHeader->MessageSize != cbReceived) {
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR, 
			"Invalid packet size: Received %d bytes, Claimed %d bytes\n",
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

	CComPtr<INdasUnit> pNdasUnit;
	
	HRESULT hr = pGetNdasUnit(unitDeviceId, &pNdasUnit);

	if (FAILED(hr))
	{
		//
		// Discard non-discovered unit device
		//
		return;
	}

	NDAS_UNITDEVICE_PRIMARY_HOST_INFO hostinfo;

	PLPX_ADDRESS hostLpxAddress = (PLPX_ADDRESS) hostinfo.Host;

	::CopyMemory(
		hostLpxAddress->Node,
		pData->PrimaryNode,
		sizeof(hostLpxAddress->Node));

	hostLpxAddress->Port = ntohs(pData->PrimaryPort);

	hostinfo.SWMajorVersion = pData->SWMajorVersion;
	hostinfo.SWMinorVersion = pData->SWMinorVersion;
	hostinfo.SWBuildNumber = pData->SWBuildNumber;
	hostinfo.NDFSCompatVersion = pData->NDFSCompatVersion;
	hostinfo.NDFSVersion = pData->NDFSVersion;

	XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_RESERVED6,
		"LSINFOX_PRIMATE_UPDATE_MESSAGE: %02X:%02X:%02X:%02X:%02X:%02X@%d\n",
		pData->NetDiskNode[0],
		pData->NetDiskNode[1],
		pData->NetDiskNode[2],
		pData->NetDiskNode[3],
		pData->NetDiskNode[4],
		pData->NetDiskNode[5],
		pData->UnitDiskNo);

	// pNdasUnit->UpdatePrimaryHostInfo(hostinfo);

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

	XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_RESERVED6,
		"LSINFOX_PRIMARY_USAGE_MESSAGE: %02X:%02X:%02X:%02X:%02X:%02X@%d\n",
		pData->NetDiskNode[0],
		pData->NetDiskNode[1],
		pData->NetDiskNode[2],
		pData->NetDiskNode[3],
		pData->NetDiskNode[4],
		pData->NetDiskNode[5],
		pData->UnitDiskNo);
 
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(unitDeviceId, &pNdasLogicalUnit);
	if (FAILED(hr)) 
	{
		// Discard message
		return;
	}

	NDAS_LOGICALDEVICE_STATUS status;
	COMVERIFY(pNdasLogicalUnit->get_Status(&status));
	
	switch (status) 
	{
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

	ACCESS_MASK mountedAcces;
	COMVERIFY(pNdasLogicalUnit->get_MountedAccess(&mountedAcces));
	
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
	
	BOOL success = ::GetComputerNameExW(
		ComputerNameDnsFullyQualified,
		wszHostName,
		&cchHostName);

	if (!success) {
		hostNameType = LSNODENAME_NETBOIS;
		cchHostName = MAX_HOSTNAME_LEN;
		success = ::GetComputerNameExW(
			ComputerNameNetBIOS,
			wszHostName,
			&cchHostName);
	}

	if (!success) {
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

	success = pGetAdapterPrimaryIpAddress(
		cbAdapterAddress, 
		pAdapterAddress,
		&cbIpAddress,
		pPrimaryIpAddress);

	success = FALSE;

	if (!success) {

		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_WARNING,
			"Getting Primary IP Address failed, if=%s, error=0x%X\n",
			CSockLpxAddr(sock.GetBoundAddr()).ToStringA(),
			GetLastError());

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
	success = sock.SendToSync(pRemoteAddr, cbPacket, pbPacket, 0, &cbSent);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"SendToSync failed, bytes=%d, error=0x%X\n",
			cbPacket, GetLastError());
		return;
	}

	return;
}

DWORD
CNdasIXServer::ThreadStart(HANDLE hStopEvent)
{
	XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_INFORMATION, 
		"Starting NdasIXServer.\n");

	CLpxDatagramServer m_dgs;
	BOOL success = m_dgs.Initialize();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"CLpxDatagramServer.Initialize failed, error=0x%X\n", 
			GetLastError());
		return 255;
	}

	success = m_dgs.Receive(
		this,
		m_usListenPort,
		INFOX_MAX_DATAGRAM_PKT_SIZE,
		hStopEvent);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"Listening IXServer at port %d failed, error=0x%X\n", 
			m_usListenPort, GetLastError());
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
	{htons(CNdasIXBcast::NDASIX_BROADCAST_PORT), 
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
	BOOL success = FALSE;

	success = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL,
		&m_NDFSVersion.wMajor,
		&m_NDFSVersion.wMinor);

	if (!success)
	{
		m_NDFSVersion.wMajor = 0;
		m_NDFSVersion.wMinor = 0;
	}

	for (DWORD i = 0; i < m_nSenders; ++i) 
	{
		success = m_senders[i].Initialize();
		if (!success) 
		{
			return false;
		}
	}

	success = m_sockAddrChangeNotifier.Initialize();
	if (!success) 
	{
		return false;
	}

	success = m_sockAddrChangeNotifier.Reset();
	if (!success) 
	{
		return false;
	}

	return true;
}

BOOL
CNdasIXBcast::BroadcastStatus()
{
	NDASBUS_QUERY_INFORMATION BusEnumQuery;
	NDASBUS_INFORMATION BusEnumInformation;

	HRESULT hr;

	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));

	CInterfaceArray<INdasLogicalUnit> ndasLogicalUnits;
	COMVERIFY(pManager->get_NdasLogicalUnits(NDAS_ENUM_DEFAULT, ndasLogicalUnits));

	size_t count = ndasLogicalUnits.GetCount();
	for (size_t index = 0; index < count; ++index)
	{
		INdasLogicalUnit* pNdasLogicalUnit = ndasLogicalUnits.GetAt(index);

		NDAS_LOGICALDEVICE_STATUS status;
		COMVERIFY(pNdasLogicalUnit->get_Status(&status));
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != status) 
		{
			continue;
		}

		NDAS_LOCATION location;
		COMVERIFY(pNdasLogicalUnit->get_NdasLocation(&location));

		BusEnumQuery.InfoClass = INFORMATION_PDO;
		BusEnumQuery.Size = sizeof(NDASBUS_QUERY_INFORMATION);
		BusEnumQuery.SlotNo = location;
		BusEnumQuery.Flags = 0;

		BOOL success = NdasBusCtlQueryInformation(
			&BusEnumQuery,
			sizeof(NDASBUS_QUERY_INFORMATION),
			&BusEnumInformation,
			sizeof(NDASBUS_INFORMATION));

		if (!success) {
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"LanscsiQueryInformation failed, slot=%d, error=0x%X\n",
				location, GetLastError());
			continue;
		}

		//
		// Broadcast a primary write access status
		//
		if (BusEnumInformation.PdoInfo.DeviceMode == DEVMODE_SHARED_READWRITE &&
			!(BusEnumInformation.PdoInfo.EnabledFeatures & NDASFEATURE_SECONDARY)) {

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
			// pNdasLogicalUnit->GetStatus()
			//
			DWORD ndasUnitCount;
			COMVERIFY(pNdasLogicalUnit->get_NdasUnitCount(&ndasUnitCount));
			for (DWORD n = 0; n < ndasUnitCount; ++n) {
				//
				// Actually, we should traverse the real entry
				// from the device registrar.
				// However, here we do the shortcut for using NDAS device id
				// and fixed NetDiskPort, etc.
				//
				NDAS_UNITDEVICE_ID unitDeviceId;
				COMVERIFY(pNdasLogicalUnit->get_NdasUnitId(n, &unitDeviceId));

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
						BOOL success =	m_senders[i].SendToSync(
								&NDASIX_BCAST_ADDR,
								cbBuffer,
								lpbBuffer,
								0,
								&cbSent);
						if (!success) {
							XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
								"SendToSync failed, bytes=%d, error=0x%X\n",
								cbBuffer, GetLastError());
						}

					} // end if
				} // for each local LPX address
			} // for each unit device
		} // if the logical device has a primary write access.
	} // for each logical device

	return TRUE;
}

#if 0

BOOL
CNdasIXBcast::BroadcastStatus()
{
	NDASSCSI_QUERY_INFO_DATA NdasScsiQuery;
	NDSCIOCTL_PRIMUNITDISKINFO LurInformation;

	CNdasLogicalDeviceManager& manager = pGetNdasLogicalUnitManager();
	CNdasLogicalDeviceVector logDevices;
	manager.Lock();
	manager.GetItems(logDevices);
	manager.Unlock();

	for (CNdasLogicalDeviceVector::const_iterator itr = logDevices.begin();
		itr != logDevices.end();
		++itr)
	{
		CNdasLogicalUnitPtr pNdasLogicalUnit = *itr;

		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != pNdasLogicalUnit->GetStatus()) {
			continue;
		}

		CONST NDAS_LOCATION& location = pNdasLogicalUnit->GetNdasLocation();

		NdasScsiQuery.InfoClass = NdscPrimaryUnitDiskInformation;
		NdasScsiQuery.Length = sizeof(NDASSCSI_QUERY_INFO_DATA);
		NdasScsiQuery.NdasScsiAddress.SlotNo = location;
		NdasScsiQuery.QueryDataLength = 0;

		XTL::AutoFileHandle	handle = pOpenStorageDeviceByNumber(
			pNdasLogicalUnit->GetDeviceNumberHint(),
			GENERIC_WRITE|GENERIC_READ);

		HRESULT hr = NdasPortCtlQueryInformation(
			handle,
			&NdasScsiQuery,
			sizeof(NDASSCSI_QUERY_INFO_DATA),
			&LurInformation,
			sizeof(NDSCIOCTL_PRIMUNITDISKINFO));

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"LanscsiQueryInformation failed, slot=%d, hr=0x%X\n",
				location, hr);
			continue;
		}

		//
		// Broadcast a primary write access status
		//
		if (LurInformation.Lur.DeviceMode == DEVMODE_SHARED_READWRITE &&
			!(LurInformation.Lur.EnabledFeatures & NDASFEATURE_SECONDARY)) {

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
			// pNdasLogicalUnit->GetStatus()
			//
			for (DWORD n = 0; n < pNdasLogicalUnit->GetUnitDeviceCount(); ++n) {
				//
				// Actually, we should traverse the real entry
				// from the device registrar.
				// However, here we do the shortcut for using NDAS device id
				// and fixed NetDiskPort, etc.
				//
				NDAS_UNITDEVICE_ID unitDeviceId = pNdasLogicalUnit->GetUnitDeviceID(n);

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
						BOOL success =	m_senders[i].SendToSync(
								&NDASIX_BCAST_ADDR,
								cbBuffer,
								lpbBuffer,
								0,
								&cbSent);
						if (!success) {
							XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
								"SendToSync failed, bytes=%d, error=0x%X\n",
								cbBuffer, GetLastError());
						}

					} // end if
				} // for each local LPX address
			} // for each unit device
		} // if the logical device has a primary write access.
	} // for each logical device

	return TRUE;
}
#endif

VOID
CNdasIXBcast::ResetBind(HANDLE hStopEvent)
{
	BOOL success = FALSE;

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
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"Getting local lpx address list failed. Retry in 5 sec, error=0x%X\n",
				GetLastError());
			// try to get address list again in 5 sec
			// we should terminate this routine at a task terminate event
			DWORD dwWaitResult = ::WaitForSingleObject(hStopEvent, 5000);
			if (WAIT_OBJECT_0 == dwWaitResult) 
			{
				return;
			}
		}
	}
	
	success = m_sockAddrChangeNotifier.Reset();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"Resetting sockAddrChangeNotifier failed, error=0x%X\n",
			GetLastError());
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

		success = m_senders[i].Create();
		if (!success) {
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"Creating a socket failed, index=%d, error=0x%X\n",
				i, GetLastError());
			continue;
		}

		//
		// This is a broadcast socket
		//
		BOOL bBroadcast = TRUE;
		success = m_senders[i].SetSockOpt(
			SO_BROADCAST, 
			(CONST BYTE*)&bBroadcast, 
			sizeof(BOOL));

		if (!success) 
		{
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"Setting a sock option to broadcast failed, index=%d, error=0x%X\n",
				i, GetLastError());
			(VOID) m_senders[i].Close();
			continue;
		}

		success = m_senders[i].Bind(pSockAddr);
		if (!success) 
		{
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"Binding failed, index=%d, address=%s, error=0x%X\n",
				i, CSockLpxAddr(pSockAddr).ToStringA(), GetLastError());
			(VOID) m_senders[i].Close();
			continue;
		}
	}

}

DWORD
CNdasIXBcast::ThreadStart(HANDLE hStopEvent)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

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
			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
				"WaitForMultipleObject failed, waitResult=%d.\n", waitResult);
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
	if (WinMinorVer == 0) {
		InfoXOsMinorType = OSTYPE_WIN2K ;
	} else if (WinMinorVer == 1) {
		InfoXOsMinorType = OSTYPE_WINXP ;
	} else if (WinMinorVer == 2) {
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
	BOOL success = ::GetVersionEx((OSVERSIONINFO*) &osvi);
	_ASSERT(success);
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
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"Getting adapter info size failed, result=0x%X, error=0x%X.\n", 
			dwResult, GetLastError());
		return FALSE;
	}

	PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		ulOutBufLen);

	if (NULL == pAdapterInfo) {
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"HeapAlloc failed, bytes=%d\n",
			ulOutBufLen);
		return FALSE;
	}

	XTL::AutoProcessHeap autoAdapterInfo = pAdapterInfo;

	dwResult = ::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if (dwResult != ERROR_SUCCESS) 
	{
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
			"Getting adapter info failed, result=0x%X, error=0x%X.\n", 
			dwResult, GetLastError());
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
				XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_ERROR,
					"WSAStringToAddress failed, error=0x%X.\n", 
					GetLastError());
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

			XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_INFORMATION,
				"IP address=%ls\n", wszIpAddress);

			bFound = TRUE;
			break;
		}

		pAdapter = pAdapter->Next;
	}

	if (!bFound) 
	{
		XTLTRACE2(NDASSVC_IX, TRACE_LEVEL_WARNING,
			"No IP addresses are associated with the adapter\n");
	}

	return TRUE;

}

