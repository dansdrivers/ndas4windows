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

#include "lsbusctl.h"
#include "lfsfilterpublic.h"

#include <iphlpapi.h> // for GetAdapterInfo

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASIX
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
InfoXGetOSType(
		DWORD WinMajorVer,
		DWORD WinMinorVer) 
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
GetOSVersion(
	LPDWORD lpdwMajorVersion, 
	LPDWORD lpdwMinorVersion)
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
GetAdapterPrimaryIpAddress(
	IN const DWORD cbAdapterAddress,
	IN const BYTE* pAdapterAddress,
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
				DPErrorExWsa(_FT("WSAStringToAddress failed: "));
				break;
			}

			if (*pcbIpAddress < sizeof(sockAddress.sin_addr)) {
				*pcbIpAddress = sizeof(sockAddress.sin_addr);
				::SetLastError(ERROR_BUFFER_OVERFLOW);
				::HeapFree(::GetProcessHeap(), 0, pAdapterInfo);
				return FALSE;
			}

			*pcbIpAddress = sizeof(sockAddress.sin_addr);
			::CopyMemory(pIpAddress, &sockAddress.sin_addr, *pcbIpAddress);

			TCHAR wszIpAddress[17] = {0};
			::MultiByteToWideChar(CP_ACP, 0,
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

	::HeapFree(::GetProcessHeap(), 0, pAdapterInfo);
	return TRUE;

}

//////////////////////////////////////////////////////////////////////////

CNdasInfoExchangeServer::
CNdasInfoExchangeServer() :
	m_hLpxAddressListChangeEvent(NULL),
	m_usListenPort(NDASIX_LISTEN_PORT)
{
	::ZeroMemory(
		m_hDataEvents,
		sizeof(HANDLE) * MAX_SOCKETLPX_INTERFACE);
}

CNdasInfoExchangeServer::
~CNdasInfoExchangeServer()
{
	if (NULL != m_hLpxAddressListChangeEvent) {
		::CloseHandle(m_hLpxAddressListChangeEvent);
	}

	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL != m_hDataEvents[i]) {
			::CloseHandle(m_hDataEvents[i]);
		}
	}
}

BOOL
CNdasInfoExchangeServer::
Initialize()
{
	if (NULL == m_hLpxAddressListChangeEvent) {
		m_hLpxAddressListChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	if (NULL == m_hLpxAddressListChangeEvent) {
		DPErrorEx(_FT("Creating an reset bind event failed.\n"));
		return FALSE;
	}

	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL == m_hDataEvents[i]) {
			m_hDataEvents[i] = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		}

		if (NULL == m_hDataEvents) {
			DPErrorEx(_FT("Creating data event[%d] failed: "), i);
			return FALSE;
		}
	}

	DWORD fSuccess = CTask::Initialize();

	return fSuccess;
}

BOOL
CNdasInfoExchangeServer::
ResetLocalAddressList()
{
	DWORD cbBuffer(0);

	//
	// Get the buffer size
	//

	BOOL fSuccess = GetLocalLpxAddressList(
		0,
		NULL,
		&cbBuffer);

	if (!fSuccess && WSAEFAULT != ::WSAGetLastError()) {
		DPErrorExWsa(_FT("Getting local LPX address list size failed: "));
		return FALSE;
	}

	LPSOCKET_ADDRESS_LIST lpSockAddrList = (LPSOCKET_ADDRESS_LIST)
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbBuffer);

	if (NULL == lpSockAddrList) {
		DPErrorEx(_FT("Memory allocation failed: "));
		return FALSE;
	}

	//
	// Get the actual list
	//

	DWORD cbReturned;
	fSuccess = GetLocalLpxAddressList(
		cbBuffer, 
		lpSockAddrList, 
		&cbReturned);

	if (!fSuccess) {
		// TODO: Event Log Error Here
		DPError(_FT("GetLocalLpxAddressList failed!\n"));
		::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
		return FALSE;
	}

	//
	// Issue a warning if no interfaces are bound to LPX
	//

	if (0 == lpSockAddrList->iAddressCount) {
		// TODO: Event Log for WARNING for "No interfaces are bound to LPX"
		DPWarning(_FT("No interfaces are bound to LPX!\n"));
	}

	//
	// Now reset m_vLocalLpxAddress
	//

	m_vLocalLpxAddress.clear();
	for (DWORD i = 0; i < lpSockAddrList->iAddressCount; i++) {
		m_vLocalLpxAddress.push_back(
			((PSOCKADDR_LPX)lpSockAddrList->Address[i].lpSockaddr)->LpxAddress);
	}

	::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
	return TRUE;
}

template <>
BOOL
CNdasInfoExchangeServer::
ProcessPacket<LSINFOX_PRIMARY_UPDATE>(
	CLpxUdpAsyncListener* pListener,
	const LSINFOX_PRIMARY_UPDATE* pData)
{
	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	
	CNdasLogicalDeviceManager* pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	CNdasDeviceRegistrar* pRegistrar = pInstMan->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	NDAS_UNITDEVICE_ID unitDeviceId = { { 
		pData->NetDiskNode[0], pData->NetDiskNode[1],
		pData->NetDiskNode[2], pData->NetDiskNode[3],
		pData->NetDiskNode[4], pData->NetDiskNode[5]
		}, pData->UnitDiskNo};

	CNdasDevice* pDevice = pRegistrar->Find(unitDeviceId.DeviceId);
	if (NULL == pDevice) {
		//
		// Discard non-registered device
		//
		return TRUE;
	}

	CNdasUnitDevice* pUnitDevice = pDevice->GetUnitDevice(unitDeviceId.UnitNo);
	if (NULL == pUnitDevice) {
		//
		// Discard non-discovered unit device
		//
		return TRUE;
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

	pUnitDevice->UpdatePrimaryHostInfo(&hostinfo);

	return TRUE;
}

template<>
BOOL
CNdasInfoExchangeServer::
ProcessPacket<LSINFOX_NDASDEV_USAGE_REQUEST>(
	CLpxUdpAsyncListener* pListener,
	const LSINFOX_NDASDEV_USAGE_REQUEST* pData)
{
	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	
	CNdasLogicalDeviceManager* pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	NDAS_UNITDEVICE_ID unitDeviceId = {
		{ 
			pData->NetDiskNode[0], pData->NetDiskNode[1],
			pData->NetDiskNode[2], pData->NetDiskNode[3],
			pData->NetDiskNode[4], pData->NetDiskNode[5]
		},
			pData->UnitDiskNo};

	CNdasLogicalDevice* pLogDevice = pLdm->Find(unitDeviceId);
	if (NULL == pLogDevice) {
		// Discard message
		return TRUE;
	}

	DPNoise(_FT("LSINFOX_PRIMARY_USAGE_MESSAGE: %02X:%02X:%02X:%02X:%02X:%02X@%d\n"),
		pData->NetDiskNode[0],
		pData->NetDiskNode[1],
		pData->NetDiskNode[2],
		pData->NetDiskNode[3],
		pData->NetDiskNode[4],
		pData->NetDiskNode[5],
		pData->UnitDiskNo);

	switch (pLogDevice->GetStatus()) {
	case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
	case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
		break;
	default:
		//
		// Otherwise, discard message
		//
		return TRUE;
	}


	ACCESS_MASK mountedAcces = pLogDevice->GetMountedAccess();
	
	const DWORD cbPacket = sizeof(LSINFOX_HEADER) + 
		sizeof(LSINFOX_NDASDEV_USAGE_REPLY);

	BYTE pbPacket[cbPacket] = {0};

	PLSINFOX_HEADER pHeader = 
		reinterpret_cast<PLSINFOX_HEADER>(pbPacket);

	PLSINFOX_NDASDEV_USAGE_REPLY pUsageReply = 
		reinterpret_cast<PLSINFOX_NDASDEV_USAGE_REPLY>(
		pbPacket + sizeof(LSINFOX_HEADER));

	//
	// Header
	//
	const BYTE NdasIxProtocolName[] = INFOX_DATAGRAM_PROTOCOL; 
	
	::CopyMemory(
		pHeader->Protocol, 
		NdasIxProtocolName, 
		sizeof(NdasIxProtocolName));

	pHeader->LSInfoXMajorVersion = INFOX_DATAGRAM_MAJVER;
	pHeader->LSInfoXMinorVersion = INFOX_DATAGRAM_MINVER;
	pHeader->OsMajorType = OSTYPE_WINDOWS;

	DWORD dwOSMajorVersion, dwOSMinorVersion;
	GetOSVersion(&dwOSMajorVersion, &dwOSMinorVersion);
	USHORT usLfsOsMinorType = 
		InfoXGetOSType(dwOSMajorVersion, dwOSMinorVersion);

	pHeader->OsMinorType = usLfsOsMinorType;
	pHeader->Type = LSINFOX_PRIMARY_UPDATE_MESSAGE | LSINFOX_TYPE_REPLY;
	pHeader->MessageSize = cbPacket;

	//
	// Body
	//

	LPX_ADDRESS localLpxAddress = pListener->GetLocalAddress();

	pUsageReply->HostLanAddr.AddressType = LSNODE_ADDRTYPE_ETHER;
	pUsageReply->HostLanAddr.AddressLen = LPXADDR_NODE_LENGTH;
	::CopyMemory(
		pUsageReply->HostLanAddr.Address,
		localLpxAddress.Node,
		LPXADDR_NODE_LENGTH);

	WCHAR wszHostName[MAX_HOSTNAME_LEN] = {0};
	USHORT hostNameType = LSNODENAME_DNSFULLYQ;
	DWORD cchHostName = MAX_HOSTNAME_LEN;
	
	BOOL fSuccess = ::GetComputerNameEx(
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
	::CopyMemory(pAdapterAddress, pListener->GetLocalAddress().Node, 6);

	DWORD cbIpAddress = 14; // TODO: why is this 14?
	BYTE pPrimaryIpAddress[14] = {0};

	fSuccess = GetAdapterPrimaryIpAddress(
		cbAdapterAddress, 
		pAdapterAddress,
		&cbIpAddress,
		pPrimaryIpAddress);

	if (!fSuccess) {

		DPWarningEx(_FT("Failed to get primary ip address of %s: "),
			CLpxAddress(pListener->GetLocalAddress()).ToString());

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

	DWORD cbSent(0);
	fSuccess = pListener->SendReply(cbPacket, pbPacket, &cbSent);

	if (!fSuccess) {
		DPErrorExWsa(_FT("Failed to send a reply (%d bytes): "), cbPacket);
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasInfoExchangeServer::
DispatchPacket(
	PCLpxUdpAsyncListener pListener, 
	DWORD cbPacket,
	const BYTE* pPacket)
{
	//
	// Sanity Check
	//
	const LSINFOX_HEADER* pHeader = 
		reinterpret_cast<const LSINFOX_HEADER*>(pPacket);
	
	UCHAR ProtocolName[4] = INFOX_DATAGRAM_PROTOCOL;

	if (pHeader->Protocol[0] != ProtocolName[0] ||
		pHeader->Protocol[1] != ProtocolName[1] ||
		pHeader->Protocol[2] != ProtocolName[2] ||
		pHeader->Protocol[3] != ProtocolName[3])
	{
		DPWarning(_FT("Invalid INFOX packet: protocol %c%c%c%c\n"),
			pHeader->Protocol[0],
			pHeader->Protocol[1],
			pHeader->Protocol[2],
			pHeader->Protocol[3]);
		
		return FALSE;
	}

	if (pHeader->MessageSize != cbPacket) {
		
		DPWarning(
			_FT("Invalid packet size: Received %d bytes, Claimed %d bytes\n"),
			cbPacket,
			pHeader->MessageSize);
		
		return FALSE;
	}

	const LSINFOX_DATA* pData = 
		reinterpret_cast<const LSINFOX_DATA*>(pPacket + sizeof(LSINFOX_HEADER));

	switch (LSINFOX_TYPE_MAJTYPE & pHeader->Type) {
	case LSINFOX_PRIMARY_UPDATE_MESSAGE:
		{
			const LSINFOX_PRIMARY_UPDATE* pPrimaryUpdateData = 
				reinterpret_cast<const LSINFOX_PRIMARY_UPDATE*>(pData);

			return ProcessPacket(pListener, pPrimaryUpdateData);
		}
	case LSINFOX_PRIMARY_USAGE_MESSAGE:
		{
			const LSINFOX_NDASDEV_USAGE_REQUEST* pUsageRequest = 
				reinterpret_cast<const LSINFOX_NDASDEV_USAGE_REQUEST*>(pData);
			return ProcessPacket(pListener, pUsageRequest);
		}

	default:
		
		return FALSE;
	}

}

DWORD
CNdasInfoExchangeServer::
OnTaskStart()
{
	BOOL fSuccess;

	HANDLE hEvents[2 + MAX_SOCKETLPX_INTERFACE];
	PCLpxUdpAsyncListener ppListener[MAX_SOCKETLPX_INTERFACE];
	BYTE pBuffer[MAX_SOCKETLPX_INTERFACE][INFOX_MAX_DATAGRAM_PKT_SIZE] = {0};

	hEvents[0] = m_hTaskTerminateEvent;
	hEvents[1] = m_hLpxAddressListChangeEvent;
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		hEvents[i + 2] = m_hDataEvents[i];
	}

	BOOL bTerminate(FALSE);

	while (!bTerminate) {

		CLpxAddressListChangeNotifier alcn(m_hLpxAddressListChangeEvent);

		BOOL fSuccess = alcn.Reset();
		if (!fSuccess) {
			DPErrorEx(_FT("Reset LpxAddressListChangeNotifier Event failed: "));
		}

		//
		// Reset m_vLocalLpxAddressList
		//
		ResetLocalAddressList();

		//
		// Now we got new local LPX address lists
		//
		std::vector<LPX_ADDRESS>::const_iterator itr = 
			m_vLocalLpxAddress.begin();

		DWORD dwBinded(0);

		for (DWORD i = 0; 
			i < MAX_SOCKETLPX_INTERFACE && itr != m_vLocalLpxAddress.end(); 
			++itr, ++i)
		{
			PCLpxUdpAsyncListener pListener = 
				new CLpxUdpAsyncListener(static_cast<LPX_ADDRESS>(*itr), m_usListenPort);
			_ASSERTE(NULL != pListener);

			BOOL fSuccess = pListener->Initialize();
			if (!fSuccess) {
				DPWarningExWsa(_FT("Listener initialization failed at %s: "),
					CLpxAddress(*itr).ToString());
				continue;
			}
			fSuccess = pListener->Bind();
			if (!fSuccess) {
				DPWarningExWsa(_FT("Listener bind failed on %s: "), 
					CLpxAddress(*itr).ToString());
				continue;
			}
			ppListener[dwBinded] = pListener;
			++dwBinded;

			DPInfo(_FT("LPX bound to %s\n"), 
				CLpxAddress(*itr).ToString());
		}

		for (DWORD i = 0; i < dwBinded; ++i) {
			DWORD cbReceived(0);
			BOOL fSuccess = ppListener[i]->StartReceive(
				m_hDataEvents[i], 
				INFOX_MAX_DATAGRAM_PKT_SIZE,
				pBuffer[i], 
				&cbReceived);
			if (!fSuccess) {
				DPWarningExWsa(_FT("Start receive failed: "));
			}
		}

		BOOL bResetBind(FALSE);
		while (!bResetBind && !bTerminate) {

			DWORD dwWaitResult = ::WSAWaitForMultipleEvents(
				2 + MAX_SOCKETLPX_INTERFACE,
				hEvents,
				FALSE,
				INFINITE,
				FALSE);

			if (WSA_WAIT_FAILED == dwWaitResult) {
				DPErrorEx(_FT("Wait failed: "));
			} else if (WSA_WAIT_TIMEOUT == dwWaitResult) {
				DPErrorEx(_FT("Wait timed out: "));
			} else if (WAIT_OBJECT_0 == dwWaitResult) {
				// Terminate Event
				bTerminate = TRUE;
				continue;
			} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {
				// Address List Change Event
				bResetBind = TRUE;
				DPInfo(_FT("LPX Address List Change Event Issued.\n"));
				continue;
			} else if (dwWaitResult >= WAIT_OBJECT_0 + 2 &&
				dwWaitResult < WAIT_OBJECT_0 + 2 + MAX_SOCKETLPX_INTERFACE)
			{
				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 2);

				// (VOID) ProcessPacket(ppListener[n], &pBuffer[n]);
				// Dispatch a packet!!!
				DWORD cbReceived(0);
				BOOL fSuccess = ppListener[n]->GetReceivedData(&cbReceived);
				if (!fSuccess) {
					DPWarningExWsa(_FT("Receiving data failed: "));
				} else {
					(VOID) DispatchPacket(ppListener[n], cbReceived, pBuffer[n]);
				}

				fSuccess = ppListener[n]->StartReceive(
					m_hDataEvents[n], 
					INFOX_MAX_DATAGRAM_PKT_SIZE, 
					pBuffer[n], 
					&cbReceived);

				if (!fSuccess) {
					DPWarningExWsa(_FT("Start receive failed: "));
				}

			} else {
				DPWarningEx(_FT("Invalid wait result: "));
			}
		} /* Reset Bind */

		for (DWORD i = 0; i < dwBinded; ++i) {
			_ASSERTE(!IsBadReadPtr(ppListener[i],sizeof(CLpxUdpAsyncListener)));
			ppListener[i]->Cleanup();
			delete ppListener[i];

			BOOL fSuccess = ::ResetEvent(m_hDataEvents[i]);
			_ASSERT(fSuccess);
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

CNdasInfoExchangeBroadcaster::
CNdasInfoExchangeBroadcaster() :
	m_hLpxAddressListChangeEvent(NULL)
{
	::ZeroMemory(&m_pLpxClient, sizeof(m_pLpxClient));
}

CNdasInfoExchangeBroadcaster::
~CNdasInfoExchangeBroadcaster()
{
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL != m_pLpxClient[i]) {
			delete m_pLpxClient[i];
		}
	}
}

BOOL
CNdasInfoExchangeBroadcaster::
BroadcastStatus()
{
	BUSENUM_QUERY_INFORMATION BusEnumQuery;
	BUSENUM_INFORMATION BusEnumInformation;

	DWORD dwSlotNo;

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasLogicalDeviceManager* pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	CNdasLogicalDeviceManager::ConstIterator itr = pLdm->begin();

	for (;itr != pLdm->end(); ++itr) {
		DWORD dwSlotNo = itr->first;

		CNdasLogicalDevice* pLogDevice = itr->second;
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != pLogDevice->GetStatus()) {
			continue;
		}

		BusEnumQuery.InfoClass = INFORMATION_PDO;
		BusEnumQuery.Size = sizeof(BUSENUM_QUERY_INFORMATION);
		BusEnumQuery.SlotNo = dwSlotNo;

		BOOL fSuccess = LsBusCtlQueryInformation(
			&BusEnumQuery,
			sizeof(BUSENUM_QUERY_INFORMATION),
			&BusEnumInformation,
			sizeof(BUSENUM_INFORMATION));

		if (!fSuccess) {
			DPErrorEx(_FT("LanscsiQueryInformation failed at slot %d: "), dwSlotNo);
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
			GetOSVersion(&dwOSMajorVersion, &dwOSMinorVersion);
			USHORT usLfsOsMinorType = 
				InfoXGetOSType(dwOSMajorVersion, dwOSMinorVersion);
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
				NDAS_UNITDEVICE_ID unitDeviceId = pLogDevice->GetUnitDeviceId(n);

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

					if (NULL != m_pLpxClient[i]) {
						
						//
						// Fill the Primary Node (LPX Address Node)
						//
						LPX_ADDRESS localLpxAddress = 
							m_pLpxClient[i]->GetLocalAddress();

						_ASSERTE(sizeof(pixData->Update.PrimaryNode) ==
							sizeof(localLpxAddress.Node));

						::CopyMemory(
							pixData->Update.PrimaryNode,
							localLpxAddress.Node,
							sizeof(pixData->Update.PrimaryNode));
						
						//
						// Send the data
						//
						DWORD cbSent(0);
						BOOL fSuccess = 
							m_pLpxClient[i]->Send(cbBuffer, lpbBuffer, &cbSent);
						if (!fSuccess) {
							DPWarningExWsa(_FT("Sending a packet failed: "));
						}

					} // end if
				} // for each local LPX address
			} // for each unit device
		} // if the logical device has a primary write access.
	} // for each logical device

	return TRUE;
}

DWORD
CNdasInfoExchangeBroadcaster::
OnTaskStart()
{
	HANDLE hEvents[2];
	hEvents[0] = m_hTaskTerminateEvent;
	hEvents[1] = m_hLpxAddressListChangeEvent;

	CLpxAddressListChangeNotifier alcn(m_hLpxAddressListChangeEvent);

	BOOL bTerminate(FALSE);
	do {

		alcn.Reset();
		BOOL fSuccess = ResetLocalAddressList();
		if (!fSuccess) {
			DPErrorExWsa(_FT("Resetting local LPX address list failed: "));
		}

		DWORD dwTimeout = 1000; // 1 sec

		BOOL bAlcReset(FALSE);

		do {

			DWORD dwWaitResult = ::WaitForMultipleObjects(
				2, hEvents, FALSE, dwTimeout);

			if (WAIT_OBJECT_0 == dwWaitResult) {
				bTerminate = TRUE;
			} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {
				bAlcReset = TRUE;
			} else if (WAIT_TIMEOUT == dwWaitResult) {
				BroadcastStatus();
			} else {
				DPErrorEx(_FT("Unexpected wait result %d: "), dwWaitResult);
			}

		} while (!bAlcReset && !bTerminate);

	} while (!bTerminate);

	return 0;
}

BOOL
CNdasInfoExchangeBroadcaster::
Initialize()
{
	if (NULL == m_hLpxAddressListChangeEvent) {
		m_hLpxAddressListChangeEvent = 
			::CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	if (NULL == m_hLpxAddressListChangeEvent) {
		DPErrorEx(_FT("Event creation failed: "));
		return FALSE;
	}

	BOOL fSuccess = ResetLocalAddressList();
	if (!fSuccess) {
		DPErrorEx(_FT("Reset Local Address List failed: "));
		return FALSE;
	}

	return CTask::Initialize();
}

BOOL
CNdasInfoExchangeBroadcaster::
ResetLocalAddressList()
{
	DWORD cbBuffer(0);

	//
	// Get the buffer size
	//
	AutoSocket sock = ::WSASocket(
		AF_LPX, 
		SOCK_STREAM, 
		IPPROTO_LPXTCP,
		NULL,
		0,
		0);

	if (INVALID_SOCKET == sock) {
		DPErrorExWsa(_FT("Socket creation failed: "));
		return FALSE;
	}

	BOOL fSuccess = GetLocalLpxAddressList(
		sock,
		0,
		NULL,
		&cbBuffer);

	if (!fSuccess && WSAEFAULT != ::WSAGetLastError()) {
		DPErrorExWsa(_FT("Getting local LPX address list size failed: "));
		return FALSE;
	}

	LPSOCKET_ADDRESS_LIST lpSockAddrList = (LPSOCKET_ADDRESS_LIST)
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbBuffer);

	if (NULL == lpSockAddrList) {
		DPErrorEx(_FT("Memory allocation failed: "));
		return FALSE;
	}

	//
	// Get the actual list
	//

	DWORD cbReturned;
	fSuccess = GetLocalLpxAddressList(
		sock,
		cbBuffer, 
		lpSockAddrList, 
		&cbReturned);

	if (!fSuccess) {
		// TODO: Event Log Error Here
		DPErrorEx(_FT("GetLocalLpxAddressList failed!\n"));
		::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
		return FALSE;
	}

	//
	// Issue a warning if no interfaces are bound to LPX
	//

	if (0 == lpSockAddrList->iAddressCount) {
		// TODO: Event Log for WARNING for "No interfaces are bound to LPX"
		DPWarning(_FT("No interfaces are bound to LPX!\n"));
	}

	//
	// Fulfill m_pLpxClient
	//

	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL != m_pLpxClient[i]) {
			delete m_pLpxClient[i];
			m_pLpxClient[i] = NULL;
		}
	}

	for (DWORD i = 0; i < lpSockAddrList->iAddressCount; ++i) {

		PSOCKADDR_LPX lpxSockAddr = 
			reinterpret_cast<SOCKADDR_LPX*>(lpSockAddrList->Address[i].lpSockaddr);

		LPX_ADDRESS lpxLocalAddress = lpxSockAddr->LpxAddress;

		m_pLpxClient[i] = new CLpxDatagramBroadcaster(
			lpxLocalAddress,
			NDASIX_BROADCAST_PORT);

		_ASSERTE(NULL != m_pLpxClient[i]);

		BOOL fSuccess = m_pLpxClient[i]->Initialize();

		if (!fSuccess) {
			DPErrorExWsa(_FT("Initializing LPX datagram sender failed on %s\n"),
				CLpxAddress(lpxLocalAddress).ToString());
			delete m_pLpxClient[i];
			m_pLpxClient[i] = NULL;
		}
		
	}

	::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
	return TRUE;
}


const SOCKADDR_LPX 
CNdasInfoExchangeUsage::
NDASIX_BCAST_ADDR = { 
	AF_LPX, 
	{HTONS(INFOEX_PORT), 
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}};

const BYTE 
CNdasInfoExchangeUsage::
NDASIX_PROTOCOL_NAME[4] = INFOX_DATAGRAM_PROTOCOL;

CNdasInfoExchangeUsage::
CNdasInfoExchangeUsage(
	const NDAS_UNITDEVICE_ID& unitDeviceId,
	DWORD dwExpectedReplies) :
	m_unitDeviceId(unitDeviceId),
	m_dwExpectedReplies(dwExpectedReplies),
	m_dwLocalAddresses(0),
	m_hTimer(NULL)
{
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		::ZeroMemory(&m_wsaBuffers[i], sizeof(WSABUF));
		::ZeroMemory(&m_overlapped[i], sizeof(OVERLAPPED));
		m_socks[i] = INVALID_SOCKET;
		::ZeroMemory(&m_saLocalAddresses[i], sizeof(SOCKADDR_LPX));
		m_hDataEvents[i] = NULL;
		::ZeroMemory(&m_ppbRecvBuf[i], sizeof(BYTE) * INFOX_MAX_DATAGRAM_PKT_SIZE);
	}

}

CNdasInfoExchangeUsage::
~CNdasInfoExchangeUsage()
{
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL != m_hDataEvents) {
			BOOL fSuccess = ::WSACloseEvent(m_hDataEvents);
			if (!fSuccess) {
				DPWarningExWsa(_FT("Closing an event %d failed: "));
			}
		}
	}

	if (NULL != m_hTimer) {
		BOOL fSuccess = ::CloseHandle(m_hTimer);
		if (!fSuccess) {
			DPWarningEx(_FT("Closing a timer handle failed: "));
		}
	}

	CleanupSockets();
}

BOOL
CNdasInfoExchangeUsage::
Initialize()
{
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {

		if (NULL == m_hDataEvents[i]) {
			m_hDataEvents[i] = ::WSACreateEvent();
		}

		if (NULL == m_hDataEvents[i]) {
			DPErrorExWsa(_FT("Creating WSAEvent failed: "));
			return FALSE;
		}
	}

	if (NULL == m_hTimer) {
		m_hTimer = ::CreateWaitableTimer(NULL, TRUE, NULL);
	}

	if (NULL == m_hTimer) {
		return FALSE;
	}

	return TRUE;
}

VOID
CNdasInfoExchangeUsage::
CleanupSockets()
{
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (INVALID_SOCKET != m_socks[i]) {
			
			INT iResult = ::closesocket(m_socks[i]);
			if (0 != iResult) {
				DPWarningExWsa(_FT("Closing a socket %d failed: "));
			}

		}
	}
}

BOOL
CNdasInfoExchangeUsage::
InitSockets()
{
	//
	// Initialize sockets for sending data
	// 
	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {

		m_socks[i] = INVALID_SOCKET;

		SOCKET sock = ::WSASocket(
			AF_LPX, 
			SOCK_DGRAM, 
			IPPROTO_LPXUDP, 
			NULL, 
			0, 
			0);

		//
		// INVALID_SOCKET will be checked when sending actual data
		//
		if (INVALID_SOCKET == sock) {
			DPErrorExWsa(_FT("Creating a socket failed on interface %s: "),
				CLpxAddress(m_saLocalAddresses[i].LpxAddress).ToString());
			continue;
		}

		BOOL bBroadcast = TRUE;
		INT iResult = ::setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_BROADCAST, 
			(char*)&bBroadcast, 
			sizeof(bBroadcast));

		if (0 != iResult) {
			DPErrorExWsa(_FT("Setting socket options failed on interface %s: "),
				CLpxAddress(m_saLocalAddresses[i].LpxAddress).ToString());
			::closesocket(sock);
			continue;
		}

		iResult = ::bind(
			sock,
			(struct sockaddr*) &m_saLocalAddresses[i],
			sizeof(SOCKADDR_LPX));

		if (0 != iResult) {
			DPErrorExWsa(_FT("Binding a socket failed on interface %s: "),
				CLpxAddress(m_saLocalAddresses[i].LpxAddress).ToString());
			::closesocket(sock);
			continue;
		}

		m_socks[i] = sock;
	}

	return TRUE;
}

BOOL
CNdasInfoExchangeUsage::
ResetLocalAddressList()
{
	//
	// Get the list of local LPX addresses
	//

	DWORD cbAddressList(0);
	BOOL fSuccess = GetLocalLpxAddressList(0, NULL, &cbAddressList);
	if (!fSuccess && WSAEFAULT != ::WSAGetLastError()) {
		DPErrorExWsa(_FT("Getting local address list size failed: "));
		return FALSE;
	}

	LPSOCKET_ADDRESS_LIST lpSockAddrList = (LPSOCKET_ADDRESS_LIST)
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbAddressList);

	if (NULL == lpSockAddrList) {
		DPErrorEx(_FT("Memory allocation failed: "));
		return FALSE;
	}

	m_dwLocalAddresses = lpSockAddrList->iAddressCount;
	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {
		m_saLocalAddresses[i] = * reinterpret_cast<SOCKADDR_LPX*>(
			lpSockAddrList->Address[i].lpSockaddr);
		m_saLocalAddresses[i].sin_family = AF_LPX;
		m_saLocalAddresses[i].LpxAddress.Port = 0;
	}

	(VOID) ::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);

	return TRUE;
}

BOOL
CNdasInfoExchangeUsage::
SendRequests()
{
	const DWORD cbPacket = sizeof(LSINFOX_HEADER) + 
		sizeof(LSINFOX_NDASDEV_USAGE_REQUEST);

	BYTE pPacket[cbPacket] = {0};

	PLSINFOX_HEADER pHeader = 
		reinterpret_cast<PLSINFOX_HEADER>(pPacket);

	PLSINFOX_NDASDEV_USAGE_REQUEST pRequest = 
		reinterpret_cast<PLSINFOX_NDASDEV_USAGE_REQUEST>(
		pPacket + sizeof(LSINFOX_HEADER));

	DWORD dwOsVerMajor, dwOsVerMinor;
	GetOSVersion(&dwOsVerMajor, &dwOsVerMinor);
	USHORT usOsMinorType = InfoXGetOSType(dwOsVerMajor, dwOsVerMinor);

	_ASSERTE(sizeof(pHeader->Protocol) == sizeof(NDASIX_PROTOCOL_NAME));
	::CopyMemory(
		pHeader->Protocol, 
		NDASIX_PROTOCOL_NAME, 
		sizeof(NDASIX_PROTOCOL_NAME));

	pHeader->LSInfoXMajorVersion = INFOX_DATAGRAM_MAJVER;
	pHeader->LSInfoXMinorVersion = INFOX_DATAGRAM_MINVER;
	pHeader->OsMajorType = OSTYPE_WINDOWS;
	pHeader->OsMinorType = usOsMinorType;
	pHeader->Type = LSINFOX_PRIMARY_USAGE_MESSAGE | LSINFOX_TYPE_REQUEST |
		LSINFOX_TYPE_BROADCAST | LSINFOX_TYPE_DATAGRAM;
	pHeader->MessageSize = cbPacket;

	_ASSERTE(sizeof(pRequest->NetDiskNode) == sizeof(m_unitDeviceId.DeviceId.Node));
	::CopyMemory(
		pRequest->NetDiskNode, 
		m_unitDeviceId.DeviceId.Node,
		sizeof(pRequest->NetDiskNode));

	//
	// Packet data is of little-endian.
	//
	pRequest->NetDiskPort = NDAS_DEVICE_LPX_PORT;
	pRequest->UnitDiskNo = m_unitDeviceId.UnitNo;

	//
	// Send the request
	//
	WSABUF wsaSendingBuffer = { cbPacket, (char*) pPacket };
	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {

		//
		// Synchronous send
		//
		DWORD cbSent(0);
		
		//
		// Ignore INVALID_SOCKETS
		//
		if (INVALID_SOCKET == m_socks[i]) {
			continue;
		}

		INT iResult = ::WSASendTo(
			m_socks[i],
			&wsaSendingBuffer,
			1,
			&cbSent,
			0,
			(struct sockaddr *)&NDASIX_BCAST_ADDR,
			sizeof(SOCKADDR_LPX),
			NULL,
			NULL);

		if (0 != iResult) {
			DPErrorExWsa(_FT("WSASendTo failed: "));
		}

	}

	return TRUE;
}

//
// Host Usage Set Trait (less)
//
// for checking duplicate packets for the same NDAS host 
//

static inline bool IsEqualHostUsage(
	const NDAS_UNITDEVICE_HOST_USAGE& x, 
	const NDAS_UNITDEVICE_HOST_USAGE& y)
{
	if (x.HostLanAddr.AddressType == y.HostLanAddr.AddressType &&
		x.HostLanAddr.AddressLen == y.HostLanAddr.AddressLen &&
		::memcmp(x.HostLanAddr.Address, y.HostLanAddr.Address, 
		x.HostLanAddr.AddressLen) == 0&&
		x.UsageId == y.UsageId)
	{
		return true;
	}

	if (x.HostNameLength == y.HostNameLength &&
		::memcmp(x.HostName, y.HostName, x.HostNameLength) == 0 &&
		x.UsageId == y.UsageId)
	{
		return true;
	}

	return false;
}

static inline bool IsDuplicateEntry(
	const std::vector<NDAS_UNITDEVICE_HOST_USAGE>& vHostUsages, 
	const NDAS_UNITDEVICE_HOST_USAGE& hostUsage)
{
	std::vector<NDAS_UNITDEVICE_HOST_USAGE>::const_iterator itr =
		vHostUsages.begin();

	while (itr != vHostUsages.end()) {
		const NDAS_UNITDEVICE_HOST_USAGE& existingHostUsage = *itr;
		if (IsEqualHostUsage(existingHostUsage, hostUsage)) {
			return true;
		}
		++itr;
	}

	return false;
}

BOOL
CNdasInfoExchangeUsage::
CollectReplies()
{
	m_dwROHosts = 0;
	m_dwRWHosts = 0;
	m_vHostUsages.clear();

	for (DWORD i = 0; i < m_dwLocalAddresses; ++i) {

		DWORD cbReceived(0);
		DWORD dwFlags = MSG_PARTIAL;

		m_overlapped[i].hEvent = m_hDataEvents[i];
		m_wsaBuffers[i].buf = (char*)m_ppbRecvBuf[i];
		m_wsaBuffers[i].len = INFOX_MAX_DATAGRAM_PKT_SIZE;

		INT iFromLen(0);
		INT iResult = ::WSARecvFrom(
			m_socks[i],
			&m_wsaBuffers[i],
			1,
			&cbReceived,
			&dwFlags,
			(struct sockaddr*)&m_saLocalAddresses[i],
			&iFromLen,
			&m_overlapped[i],
			NULL);

		if (0 == iResult) {
			//
			// Data is available (this case should not occur??)
			//
			BOOL fSuccess = ::WSASetEvent(m_hDataEvents[i]);
			_ASSERTE(fSuccess);
		} else if (0 != iResult && WSA_IO_PENDING != ::WSAGetLastError()) {
			DPErrorExWsa(_FT("Receiving from %d-th socket failed: "));
		}

	}

	DWORD dwTimeoutCount(0), dwLoopCount(0);

	LARGE_INTEGER liTimeout;
	//
	// 100 nanosec scale
	// Negative value indicates relative time
	//
	liTimeout.QuadPart = - LONGLONG(NDASIX_COLLECT_TIMEOUT) * 10L * 1000L; 

	BOOL fSuccess = ::SetWaitableTimer(m_hTimer, &liTimeout, 0, NULL, NULL, FALSE);
	_ASSERTE(fSuccess);

	HANDLE hWaitHandles[MAX_SOCKETLPX_INTERFACE + 1];
	hWaitHandles[0] = m_hTimer;
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		hWaitHandles[i+1] = (HANDLE)m_hDataEvents[i];
	}

	while (dwLoopCount < m_dwExpectedReplies + 1 &&
		dwTimeoutCount < USAGE_TIMEOUT_LOOP)
	{
		DWORD dwWaitResult = ::WaitForMultipleObjects(
			MAX_SOCKETLPX_INTERFACE + 1,
			hWaitHandles,
			FALSE,
			USAGE_TIMEOUT);

		if (WAIT_TIMEOUT == dwWaitResult) {

			++dwTimeoutCount;
			DPInfo(_FT("Wait timed out (%d).\n"), dwTimeoutCount);

		} else if (WAIT_OBJECT_0 == dwWaitResult) {

			DPWarning(_FT("Reply data collection timed out.\n"));

		} else if (WAIT_OBJECT_0 >= dwWaitResult + 1&& 
			dwWaitResult <= WAIT_OBJECT_0 + m_dwLocalAddresses + 1)
		{
			DWORD n = dwWaitResult - WAIT_OBJECT_0 - 1;
			DWORD cbReceived(0), dwFlags(0);

			PBYTE pReceivedData = m_ppbRecvBuf[i];

			BOOL fSuccess = ::WSAGetOverlappedResult(
				m_socks[n],
				&m_overlapped[n],
				&cbReceived,
				TRUE,
				&dwFlags);

			if (!fSuccess) {
				DPErrorExWsa(_FT("Getting overlapped result failed: "));
				continue;
			}

			//
			// Check the received packet
			//
			if (cbReceived < sizeof(LSINFOX_HEADER)) {
				DPWarning(_FT("Invalid packet received: size too small, ")
					_T("Expected at least %d, Received %d\n"),
					sizeof(LSINFOX_HEADER), cbReceived);
				continue;
			}

			PLSINFOX_HEADER pHeader = 
				reinterpret_cast<PLSINFOX_HEADER>(pReceivedData);

			//
			// Sanity check
			//

			//
			// Protocol Header
			//
			int iResult = ::memcmp(
				pHeader->Protocol, 
				NDASIX_PROTOCOL_NAME, 
				sizeof(NDASIX_PROTOCOL_NAME));

			if (0 != iResult) {
				DPWarning(_FT("Invalid packet received: invalid protocol, ")
					_T("%c%c%c%c \n"), 
					pHeader->Protocol[0], pHeader->Protocol[1],
					pHeader->Protocol[2], pHeader->Protocol[3]);
				continue;
			}

			//
			// Packet size
			//
			if (cbReceived != pHeader->MessageSize) {
				DPWarning(_FT("Invalid packet received: invalid packet size, ")
					_T("Received %d, Claimed %d\n"), 
					cbReceived, pHeader->MessageSize);
				continue;
			}

			PLSINFOX_NDASDEV_USAGE_REPLY pReply =
				reinterpret_cast<PLSINFOX_NDASDEV_USAGE_REPLY>(
				pReceivedData + sizeof(LSINFOX_HEADER));


			NDAS_UNITDEVICE_HOST_USAGE hostUsage;

			hostUsage.HostLanAddr.AddressType = pReply->HostLanAddr.AddressType;
			hostUsage.HostLanAddr.AddressLen = pReply->HostLanAddr.AddressLen;
			::CopyMemory(
				hostUsage.HostLanAddr.Address,
				pReply->HostLanAddr.Address,
				pReply->HostLanAddr.AddressLen);

			hostUsage.HostWanAddr.AddressType = pReply->HostWanAddr.AddressType;
			hostUsage.HostWanAddr.AddressLen = pReply->HostWanAddr.AddressLen;
			::CopyMemory(
				hostUsage.HostWanAddr.Address,
				pReply->HostWanAddr.Address,
				pReply->HostWanAddr.AddressLen);

			hostUsage.HostNameLength = pReply->HostNameLength;
			::CopyMemory(
				hostUsage.HostName,
				pReply->HostName,
				pReply->HostNameLength);

			hostUsage.UsageId = pReply->UsageID;
			hostUsage.AccessRight = pReply->AccessRight;

			//
			// Check duplicate entry
			//
			if (IsDuplicateEntry(m_vHostUsages, hostUsage)) {
				continue;
			}

			if (hostUsage.AccessRight & LSSESSION_ACCESS_WRITE) {
				++m_dwRWHosts;
			} else {
				++m_dwROHosts;
			}
			
		} else {
			DPErrorExWsa(_FT("Wait failed: "));
		}
	}
	
	return TRUE;
}

BOOL
CNdasInfoExchangeUsage::
Query()
{
	CleanupSockets();

	BOOL fSuccess = ResetLocalAddressList();
	if (!fSuccess) {
		DPErrorExWsa(_FT("Resetting local address list failed: "));
	}

	fSuccess = InitSockets();
	if (!fSuccess) {
		DPErrorExWsa(_FT("Socket initialization failed: "));
		return FALSE;
	}

	fSuccess = SendRequests();
	if (!fSuccess) {
		DPErrorExWsa(_FT("Sending requests failed: "));
		return FALSE;
	}

	fSuccess = CollectReplies();
	if (!fSuccess) {
		DPErrorExWsa(_FT("Collecting replies failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasInfoExchangeUsage::
GetHostUsage(
	DWORD dwIndex, 
	PNDAS_UNITDEVICE_HOST_USAGE pHostUsage)
{
	if (dwIndex >= m_vHostUsages.size()) {

		DPError(_FT("Invalid index %d, limited to [0..%d).\n"), 
			dwIndex, m_vHostUsages.size());

		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (IsBadWritePtr(pHostUsage, sizeof(NDAS_UNITDEVICE_HOST_USAGE))) {

		DPError(_FT("Pointer to the output parameter is incorrect:")
			_T("pHostUsage=%p.\n"), pHostUsage);

		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	::CopyMemory(
		pHostUsage, 
		&m_vHostUsages[dwIndex],
		sizeof(NDAS_UNITDEVICE_HOST_USAGE));

	return TRUE;
}
