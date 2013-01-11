#include "stdafx.h"
#include "ndashixsrv.h"
#include "ndashixutil.h"
#include "ndasobjs.h"
#include "ndaslogdev.h"
#include "ndaslogdevman.h"
#include "ndaseventpub.h"
#include "ndas/ndasevent.h" // for #define NDAS_SRF_READ, NDAS_SRF_WRITE

#include <objbase.h>
#include "autores.h"
#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_HIXSERVER
#include "xdebug.h"

typedef enum _UDA_RESPONSE {
	UDA_RESPONSE_REJECT = 0x01,
	UDA_RESPONSE_REMOUNT_RO,
	UDA_RESPONSE_UNMOUNT
} UDA_RESPONSE;

static BOOL pSendSurrenderReply(
	CLpxDatagramSocket& sock,
	PSOCKADDR_LPX pRemoteAddr,
	LPCGUID pHostGuid,
	UCHAR Status)
{
	const DWORD cbLength = sizeof(NDAS_HIX::SURRENDER_ACCESS::REPLY);
	BYTE pbBuffer[cbLength] = {0};

	NDAS_HIX::SURRENDER_ACCESS::PREPLY pReply = 
		reinterpret_cast<NDAS_HIX::SURRENDER_ACCESS::PREPLY>(pbBuffer);

	NDAS_HIX::PHEADER pHeader = &pReply->Header;

	pBuildNHIXReplyHeader(
		pHeader, pHostGuid, NHIX_TYPE_SURRENDER_ACCESS, cbLength);

	pNHIXHeaderToNetwork(pHeader);

	NDAS_HIX::SURRENDER_ACCESS::REPLY::PDATA pData = &pReply->Data;
	pData->Status = Status;


	BOOL fSuccess = sock.SendToSync(pRemoteAddr, cbLength, (LPBYTE)pReply);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Sending a REPLY_ERROR failed: "));
		return FALSE;
	}

	return TRUE;
}

CNdasHIXServer::CNdasHIXServer(LPCGUID lpHostGuid) :
	ximeta::CTask(_T("NHIXServer"))
{
	if (NULL != lpHostGuid) {
		m_HostGuid = *lpHostGuid;
	} else {
		HRESULT hr = ::CoCreateGuid(&m_HostGuid);
		_ASSERTE(SUCCEEDED(hr));
	}
	m_pbHostId = (CONST BYTE*) &m_HostGuid;
}

CNdasHIXServer::~CNdasHIXServer()
{
}

BOOL
CNdasHIXServer::Initialize()
{
	return CTask::Initialize();
}

DWORD
CNdasHIXServer::OnTaskStart()
{
	DBGPRT_INFO(_FT("Starting NDAS HIX Server.\n"));

	CLpxDatagramServer m_dgs;
	BOOL fSuccess = m_dgs.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LPX Datagram Server initialization failed: "));
		return FALSE;
	}

	fSuccess = m_dgs.Receive(
		this, 
		NDAS_HIX_LISTEN_PORT, 
		NHIX_MAX_MESSAGE_LEN,
		m_hTaskTerminateEvent);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Listening HIX at port %d failed: "), NDAS_HIX_LISTEN_PORT);
		return 255;
	}

	return 0;
}

VOID
CNdasHIXServer::OnReceive(CLpxDatagramSocket& sock)
{
	SOCKADDR_LPX remoteAddr;
	DWORD cbReceived;

	NDAS_HIX::PHEADER pHeader = NULL;

	DWORD dwRecvFlags;
	BOOL fSuccess = sock.GetRecvFromResult(
		&remoteAddr, 
		&cbReceived, 
		(BYTE**)&pHeader, 
		&dwRecvFlags);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("HIX Packet Receive failed: "));
		return;
	}

	DBGPRT_INFO(_FT("Received HIX Packet (%d bytes) from %s at %s: ")
		_T("Receive Flag %08X\n"),
		cbReceived, 
		CSockLpxAddr(remoteAddr).ToString(),
		CSockLpxAddr(sock.GetBoundAddr()).ToString(),
		dwRecvFlags);

	if (cbReceived < sizeof(NDAS_HIX::HEADER)) {
		return;
	}

	pNHIXHeaderFromNetwork(pHeader);
	fSuccess = pIsValidNHIXRequestHeader(cbReceived, pHeader);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("HIX Message Header is invalid.\n"));
		return;
	}

	switch (pHeader->Type) {
	case NHIX_TYPE_DISCOVER:
		OnHixDiscover(
			sock, 
			&remoteAddr, 
			(CONST NDAS_HIX::DISCOVER::REQUEST*) pHeader);
		break;
	case NHIX_TYPE_QUERY_HOST_INFO:
		OnHixQueryHostInfo(
			sock, 
			&remoteAddr, 
			(CONST NDAS_HIX::QUERY_HOST_INFO::REQUEST*) pHeader);
		break;
	case NHIX_TYPE_SURRENDER_ACCESS:
		OnHixSurrenderAccess(
			sock,
			&remoteAddr, 
			(CONST NDAS_HIX::SURRENDER_ACCESS::REQUEST*) pHeader);
		break;
	case NHIX_TYPE_DEVICE_CHANGE:
		OnHixDeviceChangeNotify(
			sock,
			&remoteAddr,
			(CONST NDAS_HIX::DEVICE_CHANGE::NOTIFY*) pHeader);
		break;
	case NHIX_TYPE_UNITDEVICE_CHANGE:
		OnHixUnitDeviceChangeNotify(
			sock,
			&remoteAddr,
			(CONST NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY*) pHeader);
		break;
	default:
		DBGPRT_WARN(_FT("Unknown HIX Packet Type Received: %d\n"), pHeader->Type);
	}
}


VOID
CNdasHIXServer::OnHixDiscover(
	CLpxDatagramSocket& sock, 
	SOCKADDR_LPX* pRemoteAddr, 
	const NDAS_HIX::DISCOVER::REQUEST* pRequest)
{
	//
	// Header is NTOH'ed, Data is this function's resposibility
	//

	BYTE pbBuffer[NHIX_MAX_MESSAGE_LEN] = {0};

	NDAS_HIX::DISCOVER::PREPLY pReply = 
		reinterpret_cast<NDAS_HIX::DISCOVER::PREPLY>(pbBuffer);

	NDAS_HIX::DISCOVER::REPLY::PDATA pReplyData = &pReply->Data;
	NDAS_HIX::PUNITDEVICE_ENTRY_DATA pCurReplyEntryData = &pReply->Data.Entry[0];

	pReply->Data.EntryCount = 0;

	UCHAR ucPrevAccessType = NHIX_UDA_NONE;

	DBGPRT_INFO(_FT("NHIX Discover Request for %d unit devices.\n"), 
		pRequest->Data.EntryCount);

	for (DWORD i = 0; i < pRequest->Data.EntryCount; ++i) {

		CNdasDeviceId did(pRequest->Data.Entry[i].DeviceId);
		CNdasUnitDeviceId udid(did, pRequest->Data.Entry[i].UnitNo);

		DBGPRT_INFO(_FT("NHIX Discover Request for [%d/%d]: %s\n"), 
			(i+1), pRequest->Data.EntryCount,
			udid.ToString());

		CRefObjPtr<CNdasLogicalDevice> pLogDevice = pGetNdasLogicalDevice(udid);

		if (NULL == pLogDevice.p) {
			DBGPRT_INFO(_FT("Not found\n"));
			continue;
		}

//		if (pLogDevice != pPrevLogDevice) {
			NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
			if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != status &&
				NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING != status &&
				NDAS_LOGICALDEVICE_STATUS_MOUNTED != status)
			{
				DBGPRT_INFO(_FT("No access\n"));
				continue;
			}
//		}

		ACCESS_MASK mountedAccess = pLogDevice->GetMountedAccess();

		NHIX_UDA uda = NHIX_UDA_NONE;

		if (GENERIC_WRITE & mountedAccess) {

			BOOL fSharedWrite = FALSE, fPrimary = FALSE;

			BOOL fSuccess = pLogDevice->GetSharedWriteInfo(
				&fSharedWrite, &fPrimary);

			_ASSERTE(fSuccess);

			if (fSharedWrite) {
				if (fPrimary) {
					uda = NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS;
				} else {
					uda = NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS;
				}
			} else {
				uda = NHIX_UDA_READ_WRITE_ACCESS;
			}
		} else if (GENERIC_READ & mountedAccess) {
			uda = NHIX_UDA_READ_ACCESS;
		} else {
			// any other case?
			_ASSERTE(FALSE);
		}

		//
		// Reply for requested UDA only
		//
		// criteria: Response if (reqUDA & uda) == reqUDA
		//
		NHIX_UDA reqUDA = pRequest->Data.Entry[i].AccessType;

		if ((reqUDA & uda) != reqUDA) {
			// ignore
			continue;
		}

		::CopyMemory(
			pCurReplyEntryData,
			&pRequest->Data.Entry[i], 
			sizeof(NDAS_HIX::UNITDEVICE_ENTRY_DATA));

		pCurReplyEntryData->AccessType = uda;

		++(pReply->Data.EntryCount);
		++pCurReplyEntryData;
	}

	// don't send any replies when no unit devices are related
	// unless pRequest->Data.EntryCount = 0
	if (0 == pReply->Data.EntryCount && 0 != pRequest->Data.EntryCount) {
		return;
	}

	USHORT cbLength = sizeof(NDAS_HIX::DISCOVER::REPLY) +
		 sizeof(NDAS_HIX::UNITDEVICE_ENTRY_DATA) * (pReply->Data.EntryCount - 1);

	pBuildNHIXReplyHeader(&pReply->Header, &m_HostGuid, pRequest->Header.Type, cbLength);
	pNHIXHeaderToNetwork(&pReply->Header);

	BOOL fSuccess = sock.SendToSync(pRemoteAddr, cbLength, (CONST BYTE*)pReply);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Sending a DISCOVER reply to %s at %s failed: "), 
			CSockLpxAddr(pRemoteAddr).ToString(),
			CSockLpxAddr(sock.GetBoundAddr()).ToString());
		return;
	}

	DBGPRT_INFO(_FT("Sending a DISCOVER reply to %s at %s.\n"),
			CSockLpxAddr(pRemoteAddr).ToString(),
			CSockLpxAddr(sock.GetBoundAddr()).ToString());

}

VOID
CNdasHIXServer::OnHixQueryHostInfo(
	CLpxDatagramSocket& sock, 
	SOCKADDR_LPX* pRemoteAddr, 
	const NDAS_HIX::QUERY_HOST_INFO::REQUEST* pRequest)
{
	//
	// Header is NTOH'ed, Data is this function's resposibility
	//

	BYTE pbBuffer[NHIX_MAX_MESSAGE_LEN] = {0};

	NDAS_HIX::QUERY_HOST_INFO::PREPLY pReply = 
		reinterpret_cast<NDAS_HIX::QUERY_HOST_INFO::PREPLY>(pbBuffer);

	NDAS_HIX::QUERY_HOST_INFO::REPLY::PDATA pReplyData = &pReply->Data;

	DWORD cbUsed = pGetHostInfo(NHIX_MAX_MESSAGE_LEN, &pReply->Data.HostInfoData);
	USHORT cbLength = sizeof(NDAS_HIX::QUERY_HOST_INFO::REPLY) + cbUsed;

	pBuildNHIXReplyHeader(
		&pReply->Header, 
		&m_HostGuid, 
		NHIX_TYPE_QUERY_HOST_INFO, 
		cbLength);

	pNHIXHeaderToNetwork(&pReply->Header);

	BOOL fSuccess = sock.SendToSync(
		pRemoteAddr, 
		cbLength, 
		pbBuffer);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Sending a QUERY_HOST_INFO reply to %s at %s failed: "), 
			CSockLpxAddr(sock.GetBoundAddr()).ToString(),
			CSockLpxAddr(pRemoteAddr).ToString());
		return;
	}

	DBGPRT_INFO(_FT("Sending a QUERY_HOST_INFO reply to %s at %s.\n"),
			CSockLpxAddr(sock.GetBoundAddr()).ToString(),
			CSockLpxAddr(pRemoteAddr).ToString());
}

VOID
CNdasHIXServer::OnHixSurrenderAccess(
	CLpxDatagramSocket& sock, 
	SOCKADDR_LPX* pRemoteAddr, 
	const NDAS_HIX::SURRENDER_ACCESS::REQUEST* pRequest)
{
	//
	// Header is NTOH'ed, Data is this function's responsibility
	//

	const NDAS_HIX::SURRENDER_ACCESS::REQUEST::DATA* pData = &pRequest->Data;

	//
	// Request Message Size Check
	//
	DWORD cbBasicData = sizeof(NDAS_HIX::SURRENDER_ACCESS::REQUEST);
	if (pRequest->Header.Length < cbBasicData) {

		BOOL fSuccess = pSendSurrenderReply(
			sock,
			pRemoteAddr,
			&m_HostGuid,
			NHIX_SURRENDER_REPLY_STATUS_INVALID_REQUEST);

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Error sending NHIX_SURRENDER_REPLY_ERROR_INVALID_REQUEST: "));
		}

		return;
	}

	//
	// Current implementation allows only a request
	// for a single NDAS unit device 
	//
#ifndef NHIX_SURRENDER_MULTIPLE_IMPL

	//
	// Request for multiple unit devices will return
	// INVALID_REQUEST
	//

	BOOL fSuccess = FALSE;

	if (1 != pData->EntryCount) {

		fSuccess = pSendSurrenderReply(
			sock,
			pRemoteAddr,
			&m_HostGuid,
			NHIX_SURRENDER_REPLY_STATUS_INVALID_REQUEST);

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Error sending NHIX_SURRENDER_REPLY_ERROR_INVALID_REQUEST: "));
		}

		return;
	}

	const NDAS_HIX::UNITDEVICE_ENTRY_DATA* pEntry = &pData->Entry[0];

	NDAS_UNITDEVICE_ID unitDeviceId;
	pBuildUnitDeviceIdFromHIXUnitDeviceEntry(pEntry, unitDeviceId);
	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pGetNdasLogicalDevice(unitDeviceId);
	if (NULL == pLogDevice.p) {
		
		fSuccess = pSendSurrenderReply(
			sock,
			pRemoteAddr,
			&m_HostGuid,
			NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS);

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Error sending NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS: "));
		}

		return;

	}

	if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != pLogDevice->GetStatus() &&
		NDAS_LOGICALDEVICE_STATUS_MOUNTED != pLogDevice->GetStatus() &&
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING != pLogDevice->GetStatus())
	{
		fSuccess = pSendSurrenderReply(
			sock,
			pRemoteAddr,
			&m_HostGuid,
			NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS);

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Error sending NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS: "));
		}

		return;
	}

	CRefObjPtr<CNdasDevice> pDevice = pGetNdasDevice(unitDeviceId.DeviceId);
	if (NULL == pDevice.p) {
		fSuccess = pSendSurrenderReply(
			sock,
			pRemoteAddr,
			&m_HostGuid,
			NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS);

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Error sending NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS: "));
		}

		return;
	}

	DWORD dwSlotNo = pDevice->GetSlotNo();
	DWORD dwUnitNo = unitDeviceId.UnitNo;

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();

	NHIX_UDA uda = pRequest->Data.Entry[0].AccessType;
	DWORD dwRequestFlags = 0;

	if (uda & NHIX_UDA_READ_ACCESS) dwRequestFlags |= NDAS_SRF_READ;
	if (uda & NHIX_UDA_WRITE_ACCESS) dwRequestFlags |= NDAS_SRF_WRITE;

	fSuccess = pEventPublisher->SurrenderRequest(
		dwSlotNo,
		dwUnitNo,
		&pRequest->Header.HostGuid,
		dwRequestFlags);

	fSuccess = pSendSurrenderReply(
		sock,
		pRemoteAddr,
		&m_HostGuid,
		NHIX_SURRENDER_REPLY_STATUS_QUEUED);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Error sending NHIX_SURRENDER_REPLY_STATUS_QUEUED: "));
	}

	return;

#else

	// logical device set contains the logical device pointers
	// and minimum access of UDA 
	//
	// NHIX_UDA_READ_WRITE_ACCESS supersedes NHIX_UDA_WRITE_ACCESS
	//
	typedef std::map<CNdasLogicalDevice*,NHIX_UDA> LogDevSet;
	typedef std::pair<LogDevSet::iterator,bool> LogDevSetInsertResult;
	LogDevSet logDevSet;

	// Find logical device sets for the request
	for (DWORD i = 0; i < pData->EntryCount; ++i) {

		const NDAS_HIX::UNITDEVICE_ENTRY_DATA* pEntry = &pData->Entry[i];
		NHIX_UDA uda = pEntry->AccessType;
		if (NHIX_UDA_WRITE_ACCESS != uda ||
			NHIX_UDA_READ_WRITE_ACCESS != uda)
		{
			// invalid request!
			continue;
		}

		NDAS_UNITDEVICE_ID unitDeviceId;
		pBuildUnitDeviceIdFromHIXUnitDeviceEntry(unitDeviceId, pEntry);
		CNdasLogicalDevice* pLogDevice = pGetNdasLogicalDevice(unitDeviceId);
		if (NULL == pLogDevice) continue;
		else pLogDevice->AddRef();

		LogDevSetInsertResult pr = logDevSet.insert(LogDevSet::value_type(
			pLogDevice, uda));
		if (!pr.second) {
			LogDevSet::iterator itr = pr.first;
			NHIX_UDA existingUDA = itr->second;
			// NHIX_UDA_READ_WRITE_ACCESS supersedes NHIX_UDA_WRITE_ACCESS
			if (NHIX_UDA_WRITE_ACCESS == itr->second &&
				NHIX_UDA_READ_WRITE_ACCESS == uda)
			{
				itr->second = NHIX_UDA_READ_WRITE_ACCESS;
			}
		}
	}

	for (LogDevSet::const_iterator itr = logDevSet.begin();
		itr != logDevSet.end();
		++itr)
	{
		CNdasLogicalDevice* pLogDevice = itr->first;
		pLogDevice->Eject();
		pLogDevice->Release();
	}
#endif

}

VOID
CNdasHIXServer::OnHixDeviceChangeNotify(
	CLpxDatagramSocket& sock, 
	SOCKADDR_LPX* pRemoteAddr, 
	const NDAS_HIX::DEVICE_CHANGE::NOTIFY* pNotify)
{
	pNotify->DeviceId;
}

VOID
CNdasHIXServer::OnHixUnitDeviceChangeNotify(
	CLpxDatagramSocket& sock, 
	SOCKADDR_LPX* pRemoteAddr, 
	CONST NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY* pNotify)
{	
	NDAS_DEVICE_ID deviceId;
	DWORD dwUnitNo = (DWORD)pNotify->UnitNo;

	_ASSERTE(sizeof(deviceId.Node) == sizeof(pNotify->DeviceId));
	::CopyMemory(deviceId.Node, pNotify->DeviceId, sizeof(deviceId.Node));

	DBGPRT_INFO(_FT("HIX UnitDeviceChangeNotify: %s\n"), 
		CNdasUnitDeviceId(deviceId, dwUnitNo).ToString());

	CRefObjPtr<CNdasDevice> pDevice = pGetNdasDevice(deviceId);
	if (NULL == pDevice.p) {
		DBGPRT_INFO(_FT("Non-existing unit device, ignored.\n"));
		return;
	}

	pDevice->Lock();
	pDevice->InvalidateUnitDevice(dwUnitNo);
	pDevice->Unlock();
}

