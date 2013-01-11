#include "stdafx.h"
#include <objbase.h>
#include "xguid.h"
#include "ndasdevid.h"
#include "ndashix.h"
#include "ndashixcli.h"
#include "ndashixutil.h"
#include <algorithm>
#include "xdebug.h"

template <typename T>
struct MappedPointerDataDeleter {
	void operator()(const T& v) const 
	{ if (v.second) delete v.second; }
};


static
NDAS_UNITDEVICE_ID&
pBuildUnitDeviceIdFromEntry(
	NDAS_UNITDEVICE_ID& UnitDeviceId,
	const NDAS_HIX::UNITDEVICE_ENTRY_DATA& entry)
{
	_ASSERTE(sizeof(entry.DeviceId) == 
		sizeof(UnitDeviceId.DeviceId.Node));
	::CopyMemory(
		UnitDeviceId.DeviceId.Node, 
		entry.DeviceId,
		sizeof(UnitDeviceId.DeviceId.Node));
	UnitDeviceId.UnitNo = (DWORD) entry.UnitNo;
	return UnitDeviceId;
}


static 
LPCTSTR
pUDAString(NHIX_UDA uda)
{
	switch (uda) {
	case NHIX_UDA_READ_ACCESS:	return _T("RO");
	// case NHIX_UDA_WRITE_ONLY_ACCESS: return _T("WO");
	case NHIX_UDA_READ_WRITE_ACCESS: return _T("RW");
	case NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS: return _T("SHRW_PRIM");
	case NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS: return _T("SHRW_SEC");
	// case NHIX_UDA_ANY_ACCESS: return _T("ANY");
	case NHIX_UDA_NO_ACCESS: return _T("NONE");
	default: return _T("INVALID_UDA");
	}
}

CNdasHIXClient::CNdasHIXClient(LPCGUID lpHostGuid)
{
	if (NULL == lpHostGuid) {
		HRESULT hr = ::CoCreateGuid(&m_hostGuid);
		_ASSERTE(SUCCEEDED(hr));
	} else {
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
	BOOL fSuccess = sock.GetRecvFromResult(
		&remoteAddr.m_sockLpxAddr,
		&cbReceived, 
		(BYTE**)&pReply, 
		&dwRecvFlags);

	CSockLpxAddr boundAddr = sock.GetBoundAddr();

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Failed to receive data at %s: "), 
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	DBGPRT_INFO(_FT("Data received in %d ms.\n"), 
		::GetTickCount() - m_dwInitTickCount);

	_tprintf(_T("Data received in %d ms.\n"),
		::GetTickCount() - m_dwInitTickCount);

	if (cbReceived < sizeof(NDAS_HIX::HEADER)) {
		DBGPRT_ERR(_FT("Data received %d bytes from %s at %s ")
			_T("is less than the header size. Ignored.\n"), 
			cbReceived,
			remoteAddr.ToString(),
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	NDAS_HIX::PHEADER pHeader = &pReply->Header;
	pNHIXHeaderFromNetwork(pHeader);
	fSuccess = pIsValidNHIXReplyHeader(cbReceived, pHeader);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("Data received %d bytes from %s at %s ")
			_T("has an invalid header. Ignored.\n"), 
			cbReceived,
			remoteAddr.ToString(),
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	// check for duplicate reply
	if (m_hostGuidSet.find(pHeader->HostGuid) != m_hostGuidSet.end()) {
		DBGPRT_INFO(_FT("Duplicate data for the host guid %s is ignored.\n"),
			ximeta::CGuid(pHeader->HostGuid).ToString()); 
		// discard redundant packet!
		return TRUE;
	}

	++m_dwReplyCount;

	NDAS_HIX::DISCOVER::REPLY::PDATA pData = &pReply->Data;

	DBGPRT_INFO(_FT("Received reply from %s (%s) at %s, entry count: %d.\n"),
		remoteAddr.ToString(),
		ximeta::CGuid(pHeader->HostGuid).ToString(),
		boundAddr.ToString(),
		pData->EntryCount);
	
	for (DWORD i = 0; i < pData->EntryCount; ++i) {

		PHOST_DATA pHostData = (PHOST_DATA) new HOST_DATA;
		if (NULL == pHostData) {
			// MEM ALLOC FAILED!
			DBGPRT_ERR_EX(_FT("Allocating Host Data for %d bytes failed: "), 
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

		DBGPRT_INFO(_FT("Entry %d: %s %s\n"),
			i + 1,
			CNdasUnitDeviceId(unitDeviceId).ToString(),
			pUDAString(pHostData->AccessType));
	}

	// add host guid and connection info to discard duplicates
	m_hostGuidSet.insert(pHeader->HostGuid);

	if (m_dwMaxReply > 0 && m_dwReplyCount < m_dwMaxReply) {
		return TRUE;
	} else {
		// stop receiving
		return FALSE;
	}
}

VOID
CNdasHIXDiscover::ClearHostData()
{
	std::for_each(m_udHostDataMap.begin(), m_udHostDataMap.end(), 
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
CNdasHIXDiscover::Discover(
	const NDAS_UNITDEVICE_ID& unitDeviceId, 
	NHIX_UDA uda,
	DWORD dwMaxReply,
	DWORD dwTimeout)
{
	return Discover(1, &unitDeviceId, &uda, dwMaxReply, dwTimeout);
}

BOOL
CNdasHIXDiscover::Discover(
	DWORD nCount, 
	const NDAS_UNITDEVICE_ID* pUnitDeviceId, 
	const NHIX_UDA* pUda,
	DWORD dwMaxReply,
	DWORD dwTimeout)
{
	_ASSERTE(!::IsBadReadPtr(pUnitDeviceId, sizeof(NDAS_UNITDEVICE_ID) * nCount));
	_ASSERTE(!::IsBadReadPtr(pUda, sizeof(NHIX_UDA) * nCount));

	DWORD cbData = sizeof(NDAS_HIX::DISCOVER::REQUEST) +
		nCount * sizeof(NDAS_HIX::UNITDEVICE_ENTRY_DATA);

	PBYTE pbData = new BYTE[cbData];

	if (NULL == pbData) {
		DBGPRT_ERR_EX(_FT("Memory alloc for %d bytes failed: "), cbData);
		return FALSE;
	}

	NDAS_HIX::DISCOVER::PREQUEST pRequest = 
		reinterpret_cast<NDAS_HIX::DISCOVER::PREQUEST>(pbData);

	NDAS_HIX::PHEADER pHeader = &pRequest->Header;
	NDAS_HIX::DISCOVER::REQUEST::PDATA pData = &pRequest->Data;

	pData->EntryCount = nCount;
	for (DWORD i = 0; i < nCount; ++i) {
		_ASSERTE(
			sizeof(pData->Entry[i].DeviceId) == 
			sizeof(pUnitDeviceId[i].DeviceId.Node));
		::CopyMemory(
			pData->Entry[i].DeviceId,
			pUnitDeviceId[i].DeviceId.Node,
			sizeof(pData->Entry[i].DeviceId));
		pData->Entry[i].UnitNo = (UCHAR) pUnitDeviceId[i].UnitNo;
		pData->Entry[i].AccessType = pUda[i];
	}
	
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_DISCOVER, cbData);
	pNHIXHeaderToNetwork(pHeader);

	//
	// clear the reply host set
	//
	ClearHostData();

	//
	// payload does not have byte-order dependent data.
	//

	m_dwInitTickCount = ::GetTickCount();
	m_dwMaxReply = dwMaxReply;
	m_dwReplyCount = 0;
	
	BOOL fSuccess = m_bcaster.BroadcastReceive(
		this, 
		NDAS_HIX_LISTEN_PORT,
		cbData,
		pbData,
		NHIX_MAX_MESSAGE_LEN,
		dwTimeout);

	delete [] pbData;

	if (!fSuccess) {
		DBGPRT_ERR(_FT("Broadcast and gather failed: "));
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
	for (; itr != m_udHostDataMap.end(); ++itr) {
		CONST NDAS_UNITDEVICE_ID& unitDeviceId = itr->first;
		CONST PHOST_DATA& pHostData = itr->second;
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
	if (index < m_udHostDataMap.count(unitDeviceId)) {
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
	BOOL fSuccess = m_dgclient.Initialize();
	if (!fSuccess) {
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
	BOOL fSuccess = dgSock.GetRecvFromResult(
		&remoteAddr.m_sockLpxAddr,
		&cbReceived, 
		(BYTE**)&pReply, 
		&dwRecvFlags);

	CSockLpxAddr boundAddr = dgSock.GetBoundAddr();

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Failed to receive data at %s: "), 
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	DBGPRT_INFO(_FT("Data received in %d ms.\n"), 
		::GetTickCount() - m_dwInitTickCount);

	if (cbReceived < sizeof(NDAS_HIX::HEADER)) {
		DBGPRT_ERR(_FT("Data received %d bytes from %s at %s ")
			_T("is less than the header size. Ignored.\n"), 
			cbReceived,
			remoteAddr.ToString(),
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	NDAS_HIX::PHEADER pHeader = &pReply->Header;
	pNHIXHeaderFromNetwork(pHeader);
	fSuccess = pIsValidNHIXReplyHeader(cbReceived, pHeader);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("Data received %d bytes from %s at %s ")
			_T("has an invalid header. Ignored.\n"), 
			cbReceived,
			remoteAddr.ToString(),
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	DWORD cbDataPart = cbReceived - sizeof(NDAS_HIX::HEADER);
	NDAS_HIX::QUERY_HOST_INFO::REPLY::PDATA pData = &pReply->Data;

	if (cbDataPart < sizeof(NDAS_HIX_HOST_INFO_DATA) ||
		cbDataPart < pData->HostInfoData.Length) 
	{
		DBGPRT_ERR(_FT("Data Part received %d bytes from %s at %s ")
			_T("has an invalid data. Ignored.\n"), 
			cbDataPart,
			remoteAddr.ToString(),
			boundAddr.ToString());
		// still try again to recv another
		return TRUE;
	}

	// check for duplicate reply
	if (m_hostGuidSet.find(pHeader->HostGuid) != m_hostGuidSet.end()) {
		DBGPRT_INFO(_FT("Duplicate data for the host guid %s is ignored.\n"),
			ximeta::CGuid(pHeader->HostGuid).ToString()); 
		// discard redundant packet!
		return TRUE;
	}

	++m_dwReplyCount;

	DBGPRT_INFO(_FT("Received reply from %s (%s) at %s, len: %d.\n"),
		remoteAddr.ToString(),
		ximeta::CGuid(pHeader->HostGuid).ToString(),
		boundAddr.ToString(),
		pHeader->Length);

	PNDAS_HIX_HOST_INFO_DATA pStoringData = 
		reinterpret_cast<PNDAS_HIX_HOST_INFO_DATA>(
		::LocalAlloc(LPTR, pData->HostInfoData.Length));

	if (NULL == pStoringData) {
		DBGPRT_ERR_EX(_FT("memory allocation %d bytes failed: "), 
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

	if (m_dwMaxReply == 0 || m_dwReplyCount < m_dwMaxReply) {
		return TRUE;
	} else {
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

CONST CNdasHIXQueryHostInfo::HOST_ENTRY*
CNdasHIXQueryHostInfo::GetHostInfo(DWORD dwIndex)
{
	if (dwIndex < m_hostInfoDataSet.size()) {
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

	DWORD cbToSend = sizeof(request);;
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

	BOOL fSuccess = m_dgclient.SendReceive(
		this,
		pRemoteAddr,
		cbToSend,
		pDataToSend,
		NHIX_MAX_MESSAGE_LEN,
		dwTimeout,
		dwMaxRecvHint);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LPX Dgram Multiclient SendReceive failed: "));
		return FALSE;
	}

	return TRUE;
}

VOID 
CNdasHIXQueryHostInfo::ClearHostData()
{
	HostInfoDataVector::const_iterator itr = m_hostInfoDataSet.begin();
	for (; itr != m_hostInfoDataSet.end(); ++itr) {
		CONST HOST_ENTRY* pEntry = &*itr;
		if (NULL != pEntry->pData) {
			HLOCAL hLocal = ::LocalFree(reinterpret_cast<HLOCAL>(pEntry->pData));
			_ASSERTE(NULL == hLocal);
		}
	}

	m_hostInfoDataSet.clear();
	m_hostGuidSet.clear();
}

BOOL
CNdasHIXSurrenderAccessRequest::Initialize()
{
	BOOL fSuccess = m_dgSock.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LpxDatagramSocket init failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasHIXSurrenderAccessRequest::Request(
	CONST SOCKADDR_LPX *pLocalAddr, 
	CONST SOCKADDR_LPX *pRemoteAddr, 
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

	BOOL fSuccess = m_dgSock.Create();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = m_dgSock.Bind(&localAddr);
	if (!fSuccess) {
		return FALSE;
	}

	NDAS_HIX::SURRENDER_ACCESS::REQUEST request;
	NDAS_HIX::PHEADER pHeader = &request.Header;
	NDAS_HIX::SURRENDER_ACCESS::REQUEST::PDATA pData = &request.Data;

	pData->EntryCount = 1;
	pData->Entry[0].AccessType = uda;
	pBuildHIXUnitDeviceEntryFromUnitDeviceId(
		UnitDeviceId, &pData->Entry[0]);

	DWORD cbLength = sizeof(NDAS_HIX::SURRENDER_ACCESS::REQUEST);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_SURRENDER_ACCESS, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	fSuccess = m_dgSock.SendToSync(&remoteAddr, cbLength, (LPBYTE) pHeader);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("SendToSync failed: "));
		return FALSE;
	}

	fSuccess = m_dgSock.RecvFrom(NHIX_MAX_MESSAGE_LEN,0);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("RecvFrom failed: "));
		return FALSE;
	}

	HANDLE hRecvEvent = m_dgSock.GetReceivedEvent();
	DWORD dwWaitResult = ::WaitForSingleObject(hRecvEvent, dwTimeout);
	if (WAIT_OBJECT_0 != dwWaitResult) {
		DBGPRT_ERR_EX(_FT("Host not replied in %d ms: "), dwTimeout);
		return FALSE;
	}

	LPBYTE pbReply;
	DWORD cbReceived, dwRecvFlags;
	fSuccess = m_dgSock.GetRecvFromResult(
		&remoteAddr,
		&cbReceived,
		&pbReply,
		&dwRecvFlags);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("GetRecvFromResult failed: "));
		return FALSE;
	}

	if (cbReceived < sizeof(NDAS_HIX::SURRENDER_ACCESS::REPLY)) {
		DBGPRT_ERR(_FT("Reply data (%d) is too small: should be %d bytes.\n"),
			cbReceived, sizeof(NDAS_HIX::SURRENDER_ACCESS::REPLY));
		return FALSE;
	}

	NDAS_HIX::SURRENDER_ACCESS::PREPLY pReply = 
		reinterpret_cast<NDAS_HIX::SURRENDER_ACCESS::PREPLY>(pbReply);

	NDAS_HIX::PHEADER pReplyHeader = &pReply->Header;

	pNHIXHeaderFromNetwork(pReplyHeader);
	if (! pIsValidNHIXReplyHeader(cbReceived, pReplyHeader)) {
		DBGPRT_ERR(_FT("Invalid reply header\n"));
		return FALSE;
	}

	NDAS_HIX::SURRENDER_ACCESS::REPLY::PDATA pReplyData = &pReply->Data;

	if (NHIX_SURRENDER_REPLY_STATUS_QUEUED != pReplyData->Status) {
		DBGPRT_ERR(_FT("Surrender Request not queued: %d.\n"),
			pReplyData->Status);
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasHIXChangeNotify::Initialize()
{
	BOOL fSuccess = m_bcaster.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("CLpxDatagramMultiClient init failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasHIXChangeNotify::Notify(
	CONST NDAS_UNITDEVICE_ID& unitDeviceId)
{
	NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY notify = {0};
	NDAS_HIX::UNITDEVICE_CHANGE::PNOTIFY pNotify = &notify;
	NDAS_HIX::PHEADER pHeader = &pNotify->Header;

	_ASSERTE(sizeof(pNotify->DeviceId == unitDeviceId.DeviceId.Node));
	::CopyMemory(
		pNotify->DeviceId,
		unitDeviceId.DeviceId.Node,
		sizeof(pNotify->DeviceId));
	pNotify->UnitNo = (UCHAR) unitDeviceId.UnitNo;

	DWORD cbLength = sizeof(NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_UNITDEVICE_CHANGE, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	BOOL fSuccess = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(CONST BYTE*) pNotify);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Broadcast failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasHIXChangeNotify::Notify(
	CONST NDAS_DEVICE_ID& deviceId)
{
	NDAS_HIX::DEVICE_CHANGE::NOTIFY notify = {0};
	NDAS_HIX::DEVICE_CHANGE::PNOTIFY pNotify = &notify;
	NDAS_HIX::PHEADER pHeader = &pNotify->Header;

	_ASSERTE(sizeof(pNotify->DeviceId) == sizeof(deviceId.Node));
	::CopyMemory(
		pNotify->DeviceId,
		deviceId.Node,
		sizeof(pNotify->DeviceId));

	DWORD cbLength = sizeof(NDAS_HIX::DEVICE_CHANGE::NOTIFY);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_DEVICE_CHANGE, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	BOOL fSuccess = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(CONST BYTE*) pNotify);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Broadcast failed: "));
		return FALSE;
	}

	return TRUE;
}

