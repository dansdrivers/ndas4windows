#include "stdafx.h"
#include <objbase.h>
#include <algorithm>
#include <ndas/ndashix.h>
#include "xguid.h"
#include "ndasdevid.h"
#include "ndashixcli.h"
#include "ndashixutil.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndashixcli.tmh"
#endif

LONG DbgLevelSvcHIxc = DBG_LEVEL_SVC_HIXC;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelSvcHIxc) {								\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

namespace
{
	template <typename T>
	struct MappedPointerDataDeleter {
		void operator()(const T& v) const {
			if (v.second) delete v.second; 
		}
	};

	NDAS_UNITDEVICE_ID&
	pBuildUnitDeviceIdFromEntry(
		NDAS_UNITDEVICE_ID& UnitDeviceId,
		const NDAS_HIX::UNITDEVICE_ENTRY_DATA& entry)
	{
		XTLC_ASSERT_EQUAL_SIZE( entry.Node, UnitDeviceId.DeviceId.Node );

		::CopyMemory( UnitDeviceId.DeviceId.Node, entry.Node, sizeof(UnitDeviceId.DeviceId.Node) );

		UnitDeviceId.UnitNo = (DWORD) entry.UnitNo;
		return UnitDeviceId;
	}

	LPCSTR
	pUnitDeviceAccessString(NHIX_UDA uda)
	{
		switch (uda) 
		{
		case NHIX_UDA_READ_ACCESS:	return "RO";
		// case NHIX_UDA_WRITE_ONLY_ACCESS: return "WO";
		case NHIX_UDA_READ_WRITE_ACCESS: return "RW";
		case NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS: return "SHRW_PRIM";
		case NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS: return "SHRW_SEC";
		// case NHIX_UDA_ANY_ACCESS: return "ANY";
		case NHIX_UDA_NO_ACCESS: return "NONE";
		default: return "INVALID_UDA";
		}
	}
}

CNdasHIXClient::CNdasHIXClient(LPCGUID lpHostGuid)
{
	if (NULL == lpHostGuid) 
	{
		HRESULT hr = ::CoCreateGuid(&m_hostGuid);
		XTLASSERT(SUCCEEDED(hr));
	} else 
	{
		m_hostGuid = *lpHostGuid;
	}
}

CNdasHIXDiscover::CNdasHIXDiscover(LPCGUID lpHostGuid) :
	CNdasHIXClient(lpHostGuid)
{
}

CNdasHIXDiscover::~CNdasHIXDiscover()
{
	// clear the reply host set
	ClearHostData();
}

BOOL
CNdasHIXDiscover::OnReceive(CLpxDatagramSocket& sock)
{
	CSockLpxAddr remoteAddr;

	NDAS_HIX::DISCOVER::PREPLY pReply = NULL;
	DWORD cbReceived = 0, dwRecvFlags = 0;

	SOCKADDR_LPX remoteLpxAddr;
	BOOL success = sock.GetRecvFromResult(
		&remoteAddr.m_sockLpxAddr,
		&cbReceived, 
		(BYTE**)&pReply, 
		&dwRecvFlags);

	CSockLpxAddr boundAddr = sock.GetBoundAddr();

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Failed to receive data at %s, error=0x%X", 
			boundAddr.ToStringA(), GetLastError());
		// continue receiving
		return TRUE;
	}

	XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_INFORMATION, "Data received in %d ms.\n", 
		::GetTickCount() - m_dwInitTickCount);

	if (cbReceived < sizeof(NDAS_HIX::HEADER)) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_WARNING,
			"Data received %d bytes from %s at %s "
			"is less than the header size. Ignored.\n",
			cbReceived,
			remoteAddr.ToStringA(),
			boundAddr.ToStringA());
		// continue receiving
		return TRUE;
	}

	NDAS_HIX::PHEADER pHeader = &pReply->Header;
	pNHIXHeaderFromNetwork(pHeader);
	success = pIsValidNHIXReplyHeader(cbReceived, pHeader);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_WARNING,
			"Data received %d bytes from %s at %s "
			"has an invalid header. Ignored.\n", 
			cbReceived,
			remoteAddr.ToStringA(),
			boundAddr.ToStringA());
		// still try again to recv another
		return TRUE;
	}

	// check for duplicate reply
	if (m_hostGuidSet.find(pHeader->HostGuid) != m_hostGuidSet.end()) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_INFORMATION, 
			"Duplicate data for the host guid %ls is ignored.\n",
			ximeta::CGuid(pHeader->HostGuid).ToString()); 
		// discard redundant packet!
		return TRUE;
	}

	++m_dwReplyCount;

	NDAS_HIX::DISCOVER::REPLY::PDATA pData = &pReply->Data;

	XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_INFORMATION, 
		"Received reply from %ws (%ws) at %ws, entry count: %d.\n",
		remoteAddr.ToString(),
		ximeta::CGuid(pHeader->HostGuid).ToString(),
		boundAddr.ToString(),
		pData->EntryCount);
	
	// Workaround: 
	// Macintosh may respond with zero entry.
	// As zero entry is invalid (and of no use), we can ignore it.
	if (0 == pData->EntryCount)
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_WARNING,
			"Zero entry count packet is ignored.\n");
		// continue receiving
		return TRUE;
	}

	for (DWORD i = 0; i < pData->EntryCount; ++i) 
	{

		PHOST_DATA pHostData = (PHOST_DATA) new HOST_DATA;
		if (NULL == pHostData) 
		{
			// MEM ALLOC FAILED!
			XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
				"Allocating Host Data failed, bytes=%d\n", 
				sizeof(HOST_DATA));
			// still try again to recv another
			continue;
		}

		NDAS_UNITDEVICE_ID unitDeviceId;
		pBuildUnitDeviceIdFromEntry(unitDeviceId, pData->Entry[i]);

		pHostData->HostGuid = pHeader->HostGuid;
		pHostData->AccessType = pData->Entry[i].AccessType;
		pHostData->BoundAddr = boundAddr;
		pHostData->RemoteAddr = remoteAddr;

		m_udHostDataMap.insert(UnitDev_HostData_Map::value_type(
			unitDeviceId, pHostData));

		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_INFORMATION,
			"Entry %d: %s %s(%02X)\n",
			i + 1,
			CNdasUnitDeviceId(unitDeviceId).ToStringA(),
			pUnitDeviceAccessString(pHostData->AccessType),
			pHostData->AccessType);
	}

	// add host guid and connection info to discard duplicates
	m_hostGuidSet.insert(pHeader->HostGuid);

	if (m_dwMaxReply > 0 && m_dwReplyCount < m_dwMaxReply) 
	{
		// continue receiving
		return TRUE;
	}
	else 
	{
		// stop receiving
		return FALSE;
	}
}

void
CNdasHIXDiscover::ClearHostData()
{
	std::for_each(
		m_udHostDataMap.begin(), m_udHostDataMap.end(), 
		MappedPointerDataDeleter<UnitDev_HostData_Map::value_type>());

	m_hostGuidSet.clear();
	m_udHostDataMap.clear();
}

BOOL
CNdasHIXDiscover::Initialize()
{
	return m_bcaster.Initialize();
}

BOOL
CNdasHIXDiscover::HixDiscover (
	const NDAS_UNITDEVICE_ID	&unitDeviceId, 
	NHIX_UDA					uda,
	DWORD						dwMaxReply,
	DWORD						dwTimeout
	)
{
	return HixDiscover( 1, &unitDeviceId, &uda, dwMaxReply, dwTimeout );
}

BOOL
CNdasHIXDiscover::HixDiscover (
	DWORD						nCount, 
	const NDAS_UNITDEVICE_ID	*pUnitDeviceId, 
	const NHIX_UDA				*pUda,
	DWORD						dwMaxReply,
	DWORD						dwTimeout
	)
{
	XTLASSERT(!::IsBadReadPtr(pUnitDeviceId, sizeof(NDAS_UNITDEVICE_ID) * nCount));
	XTLASSERT(!::IsBadReadPtr(pUda, sizeof(NHIX_UDA) * nCount));

	DWORD cbData = sizeof(NDAS_HIX::DISCOVER::REQUEST) +
		nCount * sizeof(NDAS_HIX::UNITDEVICE_ENTRY_DATA);

	PBYTE pbData = new BYTE[cbData];

	if (NULL == pbData) {

		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Memory allocation failed, bytes=%d.\n", cbData);
		return FALSE;
	}

	NDAS_HIX::DISCOVER::PREQUEST pRequest = 
		reinterpret_cast<NDAS_HIX::DISCOVER::PREQUEST>(pbData);

	NDAS_HIX::PHEADER pHeader = &pRequest->Header;
	NDAS_HIX::DISCOVER::REQUEST::PDATA pData = &pRequest->Data;

	XTLASSERT(nCount <= 0xFF);
	pData->EntryCount = static_cast<UCHAR>(nCount);

	for (DWORD i = 0; i < nCount; ++i) {

		XTLC_ASSERT_EQUAL_SIZE( pData->Entry[i].Node, pUnitDeviceId[i].DeviceId.Node );

		::CopyMemory( pData->Entry[i].Node, pUnitDeviceId[i].DeviceId.Node, sizeof(pData->Entry[i].Node) );

		pData->Entry[i].UnitNo = static_cast<UCHAR>(pUnitDeviceId[i].UnitNo);
		pData->Entry[i].AccessType = pUda[i];
	}
	
	pBuildNHIXRequestHeader( pHeader, &m_hostGuid, NHIX_TYPE_DISCOVER, static_cast<USHORT>(cbData) );
	pNHIXHeaderToNetwork(pHeader);

	// clear the reply host set

	ClearHostData();

	// payload does not have byte-order dependent data.

	m_dwInitTickCount = ::GetTickCount();
	m_dwMaxReply = dwMaxReply;
	m_dwReplyCount = 0;
	
	BOOL success = m_bcaster.BroadcastReceive( this, 
											   NDAS_HIX_LISTEN_PORT,
											   cbData,
											   pbData,
											   NHIX_MAX_MESSAGE_LEN,
											   dwTimeout );

	delete [] pbData;

	if (!success) {

		NdasUiDbgCall( 1, "Broadcast and gather failed, error=0x%X\n", GetLastError() );
		return FALSE;
	}

	// we got reply host set

	return TRUE;
}

DWORD 
CNdasHIXDiscover::GetHostCount(
	const NDAS_UNITDEVICE_ID& unitDeviceId)
{
#ifdef DBG
	UnitDev_HostData_Map::const_iterator itr = m_udHostDataMap.begin();
	for (; itr != m_udHostDataMap.end(); ++itr) 
	{
		const NDAS_UNITDEVICE_ID& unitDeviceId = itr->first;
		const PHOST_DATA& pHostData = itr->second;
		pHostData->HostGuid;
	}
#endif
	return (DWORD) m_udHostDataMap.count(unitDeviceId);
}

//
// We need another external iterator for traversing
// host data for each unit device.
// When we just use this interface, it takes O(HostCount^2).
// If we make another iterator, it will be O(HostCount)
//

BOOL 
CNdasHIXDiscover::GetHostData(
	const NDAS_UNITDEVICE_ID& unitDeviceId,
	DWORD index, 
	NHIX_UDA* pUDA,
	LPGUID pHostGuid,
	PSOCKADDR_LPX pRemoteAddr, 
	PSOCKADDR_LPX pBoundAddr)
{
	typedef UnitDev_HostData_Map::const_iterator citr_t;
	if (index < m_udHostDataMap.count(unitDeviceId)) 
	{
		citr_t citr = m_udHostDataMap.lower_bound(unitDeviceId);
		for (DWORD i = 0; i < index; ++i) ++citr;
		PHOST_DATA pHostData = citr->second;
		if (pHostGuid) *pHostGuid = pHostData->HostGuid;
		if (pUDA) *pUDA = pHostData->AccessType;
		if (pRemoteAddr) *pRemoteAddr = pHostData->RemoteAddr;
		if (pBoundAddr) *pBoundAddr = pHostData->BoundAddr;
		return TRUE;
	}
	return FALSE;
}

CNdasHIXQueryHostInfo::CNdasHIXQueryHostInfo(
	LPCGUID lpHostGuid /* = NULL */) :
	CNdasHIXClient(lpHostGuid),
	m_dwMaxReply(0),
	m_dwReplyCount(0),
	m_dwInitTickCount(0)
{
}

CNdasHIXQueryHostInfo::~CNdasHIXQueryHostInfo()
{
	ClearHostData();
}

BOOL
CNdasHIXQueryHostInfo::Initialize()
{
	BOOL success = m_dgclient.Initialize();
	if (!success) 
	{
		return FALSE;
	}
	return TRUE;
}

BOOL
CNdasHIXQueryHostInfo::OnReceive(CLpxDatagramSocket& dgSock)
{
	CSockLpxAddr remoteAddr;

	NDAS_HIX::QUERY_HOST_INFO::PREPLY pReply = NULL;
	DWORD cbReceived = 0, dwRecvFlags = 0;

	SOCKADDR_LPX remoteLpxAddr;
	BOOL success = dgSock.GetRecvFromResult(
		&remoteAddr.m_sockLpxAddr,
		&cbReceived, 
		(BYTE**)&pReply, 
		&dwRecvFlags);

	CSockLpxAddr boundAddr = dgSock.GetBoundAddr();

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Failed to receive data at %s, error=0x%X\n", 
			boundAddr.ToStringA(), GetLastError());
		// still try again to recv another
		return TRUE;
	}

	XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_INFORMATION, "Data received in %d ms.\n", 
		::GetTickCount() - m_dwInitTickCount);

	if (cbReceived < sizeof(NDAS_HIX::HEADER))
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Data received %d bytes from %s at %s "
			"is less than the header size. Ignored.\n", 
			cbReceived,
			remoteAddr.ToStringA(),
			boundAddr.ToStringA());
		// still try again to recv another
		return TRUE;
	}

	NDAS_HIX::PHEADER pHeader = &pReply->Header;
	pNHIXHeaderFromNetwork(pHeader);
	success = pIsValidNHIXReplyHeader(cbReceived, pHeader);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Data received %d bytes from %s at %s "
			"has an invalid header. Ignored.\n", 
			cbReceived,
			remoteAddr.ToStringA(),
			boundAddr.ToStringA());
		// still try again to recv another
		return TRUE;
	}

	DWORD cbDataPart = cbReceived - sizeof(NDAS_HIX::HEADER);
	NDAS_HIX::QUERY_HOST_INFO::REPLY::PDATA pData = &pReply->Data;

	if (cbDataPart < sizeof(NDAS_HIX_HOST_INFO_DATA) ||
		cbDataPart < pData->HostInfoData.Length) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Data Part received %d bytes from %s at %s "
			"has an invalid data. Ignored.\n", 
			cbDataPart,
			remoteAddr.ToStringA(),
			boundAddr.ToStringA());
		// still try again to recv another
		return TRUE;
	}

	// check for duplicate reply
	if (m_hostGuidSet.find(pHeader->HostGuid) != m_hostGuidSet.end())
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"Duplicate data for the host guid %ws is ignored.\n",
			ximeta::CGuid(pHeader->HostGuid).ToString()); 
		// discard redundant packet!
		return TRUE;
	}

	++m_dwReplyCount;

	XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_INFORMATION,
		"Received reply from %ws (%ws) at %ws, len: %d.\n",
		remoteAddr.ToString(),
		ximeta::CGuid(pHeader->HostGuid).ToString(),
		boundAddr.ToString(),
		pHeader->Length);

	PNDAS_HIX_HOST_INFO_DATA pStoringData = 
		reinterpret_cast<PNDAS_HIX_HOST_INFO_DATA>(
		::LocalAlloc(LPTR, pData->HostInfoData.Length));

	if (NULL == pStoringData) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Memory allocation failed, bytes=%d\n", 
			pData->HostInfoData.Length);
		return TRUE;
	}

	::CopyMemory(
		pStoringData, 
		&pData->HostInfoData, 
		pData->HostInfoData.Length);

	HOST_ENTRY hostDataEntry;
	hostDataEntry.HostGuid = pHeader->HostGuid;
	hostDataEntry.boundAddr = boundAddr;
	hostDataEntry.remoteAddr = remoteAddr;
	hostDataEntry.cbData = pData->HostInfoData.Length;
	hostDataEntry.pData = pStoringData;

	m_hostInfoDataSet.push_back(hostDataEntry);
	// add host guid and connection info to discard duplicates
	m_hostGuidSet.insert(pHeader->HostGuid);

	if (m_dwMaxReply == 0 || m_dwReplyCount < m_dwMaxReply) 
	{
		return TRUE;
	}
	else 
	{
		// stop receiving
		return FALSE;
	}

	return TRUE;
}

DWORD
CNdasHIXQueryHostInfo::GetHostInfoCount()
{
	return (DWORD) m_hostInfoDataSet.size();
}

const CNdasHIXQueryHostInfo::HOST_ENTRY*
CNdasHIXQueryHostInfo::GetHostInfo(DWORD dwIndex)
{
	if (dwIndex < m_hostInfoDataSet.size()) 
	{
		return &m_hostInfoDataSet.at(dwIndex);
	}
	return NULL;
}

BOOL 
CNdasHIXQueryHostInfo::BroadcastQuery(
	DWORD dwTimeout,
	USHORT usRemotePort /* = NDAS_HIX_LISTEN_PORT */,
	DWORD dwMaxRecvHint /* = 0 */)
{
	SOCKADDR_LPX bcastAddr = pCreateLpxBroadcastAddress(usRemotePort);
	return Query(dwTimeout, &bcastAddr,dwMaxRecvHint);
}

BOOL
CNdasHIXQueryHostInfo::Query(
	DWORD dwTimeout,
	const SOCKADDR_LPX* pRemoteAddr,
	DWORD dwMaxRecvHint /* = 1 */)
{
	NDAS_HIX::QUERY_HOST_INFO::REQUEST request;
	NDAS_HIX::PHEADER pHeader = &request.Header;
	NDAS_HIX::QUERY_HOST_INFO::REQUEST::PDATA pData = &request.Data;

	USHORT cbToSend = sizeof(request);;
	BYTE* pDataToSend = (BYTE*)&request;
	::ZeroMemory(pDataToSend, cbToSend);

	pBuildNHIXRequestHeader(
		pHeader,
		&m_hostGuid,
		NHIX_TYPE_QUERY_HOST_INFO,
		cbToSend);

	pNHIXHeaderToNetwork(pHeader);

	//
	// clear the reply host set
	//
	ClearHostData();

	//
	// payload does not have byte-order dependent data.
	//

	m_dwInitTickCount = ::GetTickCount();
	m_dwMaxReply = dwMaxRecvHint;
	m_dwReplyCount = 0;

	BOOL success = m_dgclient.SendReceive(
		this,
		pRemoteAddr,
		cbToSend,
		pDataToSend,
		NHIX_MAX_MESSAGE_LEN,
		dwTimeout,
		dwMaxRecvHint);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"LPX Dgram Multiclient SendReceive failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	return TRUE;
}

void 
CNdasHIXQueryHostInfo::ClearHostData()
{
	HostInfoDataVector::const_iterator itr = m_hostInfoDataSet.begin();
	for (; itr != m_hostInfoDataSet.end(); ++itr) 
	{
		const HOST_ENTRY* pEntry = &*itr;
		if (NULL != pEntry->pData) 
		{
			HLOCAL hLocal = ::LocalFree(reinterpret_cast<HLOCAL>(pEntry->pData));
			XTLASSERT(NULL == hLocal);
		}
	}

	m_hostInfoDataSet.clear();
	m_hostGuidSet.clear();
}

BOOL
CNdasHIXSurrenderAccessRequest::Initialize()
{
	BOOL success = m_dgSock.Initialize();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"LpxDatagramSocket init failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasHIXSurrenderAccessRequest::Request(
	const SOCKADDR_LPX *pLocalAddr, 
	const SOCKADDR_LPX *pRemoteAddr, 
	const NDAS_UNITDEVICE_ID& UnitDeviceId, 
	const NHIX_UDA uda, 
	DWORD dwTimeout /* = 1000 */)
{
	SOCKADDR_LPX localAddr = *pLocalAddr;
	SOCKADDR_LPX remoteAddr = *pRemoteAddr;

	localAddr.sin_family = AF_LPX;
	localAddr.LpxAddress.Port = 0;

	remoteAddr.sin_family = AF_LPX;
	remoteAddr.LpxAddress.Port = htons(NDAS_HIX_LISTEN_PORT);

	BOOL success = m_dgSock.Create();
	if (!success) 
	{
		return FALSE;
	}

	success = m_dgSock.Bind(&localAddr);
	if (!success) 
	{
		return FALSE;
	}

	NDAS_HIX::SURRENDER_ACCESS::REQUEST request;
	NDAS_HIX::PHEADER pHeader = &request.Header;
	NDAS_HIX::SURRENDER_ACCESS::REQUEST::PDATA pData = &request.Data;

	pData->EntryCount = 1;
	pData->Entry[0].AccessType = uda;
	pBuildHIXUnitDeviceEntryFromUnitDeviceId(
		UnitDeviceId, &pData->Entry[0]);

	USHORT cbLength = sizeof(NDAS_HIX::SURRENDER_ACCESS::REQUEST);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_SURRENDER_ACCESS, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	success = m_dgSock.SendToSync(&remoteAddr, cbLength, (LPBYTE) pHeader);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"SendToSync failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	success = m_dgSock.RecvFrom(NHIX_MAX_MESSAGE_LEN,0);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"RecvFrom failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	HANDLE hRecvEvent = m_dgSock.GetReceivedEvent();
	DWORD dwWaitResult = ::WaitForSingleObject(hRecvEvent, dwTimeout);
	if (WAIT_OBJECT_0 != dwWaitResult) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_WARNING, 
			"Host not replied in %d ms.\n", dwTimeout);
		return FALSE;
	}

	LPBYTE pbReply;
	DWORD cbReceived, dwRecvFlags;
	success = m_dgSock.GetRecvFromResult(
		&remoteAddr,
		&cbReceived,
		&pbReply,
		&dwRecvFlags);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"GetRecvFromResult failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	if (cbReceived < sizeof(NDAS_HIX::SURRENDER_ACCESS::REPLY)) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Reply data (%d) is too small: should be %d bytes.\n",
			cbReceived, sizeof(NDAS_HIX::SURRENDER_ACCESS::REPLY));
		return FALSE;
	}

	NDAS_HIX::SURRENDER_ACCESS::PREPLY pReply = 
		reinterpret_cast<NDAS_HIX::SURRENDER_ACCESS::PREPLY>(pbReply);

	NDAS_HIX::PHEADER pReplyHeader = &pReply->Header;

	pNHIXHeaderFromNetwork(pReplyHeader);
	if (! pIsValidNHIXReplyHeader(cbReceived, pReplyHeader)) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Invalid reply header.\n");
		return FALSE;
	}

	NDAS_HIX::SURRENDER_ACCESS::REPLY::PDATA pReplyData = &pReply->Data;

	if (NHIX_SURRENDER_REPLY_STATUS_QUEUED != pReplyData->Status) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR,
			"Surrender Request not queued: %d.\n", pReplyData->Status);
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasHIXChangeNotify::Initialize()
{
	BOOL success = m_bcaster.Initialize();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"CLpxDatagramMultiClient init failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasHIXChangeNotify::Notify(
	const NDAS_UNITDEVICE_ID& unitDeviceId)
{
	NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY notify = {0};
	NDAS_HIX::UNITDEVICE_CHANGE::PNOTIFY pNotify = &notify;
	NDAS_HIX::PHEADER pHeader = &pNotify->Header;

	XTLC_ASSERT_EQUAL_SIZE(pNotify->DeviceId, unitDeviceId.DeviceId.Node);

	::CopyMemory(
		pNotify->DeviceId,
		unitDeviceId.DeviceId.Node,
		sizeof(pNotify->DeviceId));
	pNotify->UnitNo = (UCHAR) unitDeviceId.UnitNo;

	USHORT cbLength = sizeof(NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_UNITDEVICE_CHANGE, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	BOOL success = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(const BYTE*) pNotify);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"Broadcast failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasHIXChangeNotify::Notify(
	const NDAS_DEVICE_ID& deviceId)
{
	NDAS_HIX::DEVICE_CHANGE::NOTIFY notify = {0};
	NDAS_HIX::DEVICE_CHANGE::PNOTIFY pNotify = &notify;
	NDAS_HIX::PHEADER pHeader = &pNotify->Header;

	XTLC_ASSERT_EQUAL_SIZE(pNotify->DeviceId,deviceId.Node);

	::CopyMemory(
		pNotify->DeviceId,
		deviceId.Node,
		sizeof(pNotify->DeviceId));

	USHORT cbLength = sizeof(NDAS_HIX::DEVICE_CHANGE::NOTIFY);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_DEVICE_CHANGE, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	BOOL success = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(const BYTE*) pNotify);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_HIXCLIENT, TRACE_LEVEL_ERROR, 
			"Broadcast failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

