/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "ndas/ndasmsg.h"
#include "ndas/ndascmd.h"
#include "ndas/ndascmddbg.h"
#include "ndas/ndashostinfo.h"
#include "ndas/ndassvcparam.h"

#include "ndasinstman.h"
#include "ndascmdproc.h"
#include "syncobj.h"
#include "ndasdevreg.h"
#include "ndashostinfocache.h"
#include "ndasobjs.h"
#include "ndashixcli.h"
#include "ndascfg.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_CMDPROC
#include "xdebug.h"

using ximeta::CAutoLock;

//////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////

CNdasCommandProcessor::CNdasCommandProcessor(ITransport* pTransport) :
	m_pTransport(pTransport),
	m_command(NDAS_CMD_TYPE_NONE),
	m_hEvent(NULL)
{
}

//////////////////////////////////////////////////////////////////////////
//
// Destructor
//
//////////////////////////////////////////////////////////////////////////

CNdasCommandProcessor::~CNdasCommandProcessor()
{
	if (m_hEvent) {
		::CloseHandle(m_hEvent);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// InitOverlapped
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::InitOverlapped()
{
	if (NULL == m_hEvent) {
		m_hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hEvent) return FALSE;
	} else {
		if (!::ResetEvent(m_hEvent)) return FALSE;
	}
	::ZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
	m_overlapped.hEvent = m_hEvent;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// ReadPacket
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::ReadPacket(
	LPVOID lpBuffer, 
	DWORD cbToRead, 
	LPDWORD lpcbRead)
{
	BOOL fSuccess(FALSE);

	_ASSERTE(lpcbRead != NULL);

	fSuccess = InitOverlapped();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = m_pTransport->Receive(lpBuffer, cbToRead, lpcbRead, &m_overlapped);
	return fSuccess;
}

//////////////////////////////////////////////////////////////////////////
//
// WritePacket
//
//////////////////////////////////////////////////////////////////////////

BOOL 
CNdasCommandProcessor::WritePacket(
	LPCVOID lpBuffer,
	DWORD cbToWrite,
	LPDWORD lpcbWritten)
{
	BOOL fSuccess(FALSE);

	_ASSERTE(NULL != lpcbWritten);

	fSuccess = InitOverlapped();
	if (!fSuccess) {
		return FALSE;
	}
	
	fSuccess = m_pTransport->Send(lpBuffer, cbToWrite, lpcbWritten, &m_overlapped);
	return fSuccess;
}

//////////////////////////////////////////////////////////////////////////
//
// ReadHeader
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::ReadHeader()
{
	DWORD cbRead;

	NDAS_CMD_HEADER* pRequestHeader = &m_requestHeader;	
	::ZeroMemory(pRequestHeader, sizeof(NDAS_CMD_HEADER));

	BOOL fSuccess = ReadPacket(
		pRequestHeader,
		sizeof(NDAS_CMD_HEADER),
		&cbRead);

	if (!fSuccess && ::GetLastError() != ERROR_MORE_DATA) {
		DBGPRT_ERR_EX(_FT("Read packet failed: "));
		return FALSE;
	}

	DBGPRT_INFO(_FT("RequestHeader: %s\n"), CStructFormat().CreateString(pRequestHeader));

	// header check

	if (0 != ::memcmp(
		pRequestHeader->Protocol, 
		NDAS_CMD_PROTOCOL, 
		sizeof(NDAS_CMD_PROTOCOL)))
	{
		DBGPRT_ERR(_FT("Unexpected Protocol %c%c%c%c\n"),
			pRequestHeader->Protocol[0], pRequestHeader->Protocol[1],
			pRequestHeader->Protocol[2], pRequestHeader->Protocol[3]);
		return FALSE;
	}

	// version mismatch is handled from Process()

	if (cbRead != sizeof(NDAS_CMD_HEADER)) {
		// header size mismatch
		DBGPRT_ERR(_FT("Header size mismatch, read %d bytes, expected value %d\n"),
			cbRead, sizeof(NDAS_CMD_HEADER));
		return FALSE;
	}

	m_command = static_cast<NDAS_CMD_TYPE>(pRequestHeader->Command);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// WriteHeader
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::WriteHeader(NDAS_CMD_STATUS opStatus, DWORD cbDataSize)
{
	NDAS_CMD_HEADER replyHeader;
	NDAS_CMD_HEADER* pReplyHeader = &replyHeader;
	DWORD cbReplyHeader = sizeof(NDAS_CMD_HEADER);
	::ZeroMemory(pReplyHeader, cbReplyHeader);
	::CopyMemory(pReplyHeader, NDAS_CMD_PROTOCOL, sizeof(NDAS_CMD_PROTOCOL));

	pReplyHeader->VersionMajor = NDAS_CMD_PROTOCOL_VERSION_MAJOR;
	pReplyHeader->VersionMinor = NDAS_CMD_PROTOCOL_VERSION_MINOR;
	pReplyHeader->Command = m_command;
	pReplyHeader->Status = opStatus;
	pReplyHeader->MessageSize = cbReplyHeader + cbDataSize;

	DBGPRT_INFO(_FT("ReplyHeader: %s\n"), CStructFormat().CreateString(pReplyHeader));

	DWORD cbWritten;
	BOOL fSuccess = WritePacket(
		pReplyHeader, 
		cbReplyHeader,
		&cbWritten);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Writing a header failed: "));
		return FALSE;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// WriteErrorReply
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::WriteErrorReply(
	NDAS_CMD_STATUS opStatus,
	DWORD dwErrorCode,
	DWORD cbDataLength,
	LPVOID lpData)
{
	BOOL fSuccess;
	NDAS_CMD_ERROR::REPLY errorReply;
	NDAS_CMD_ERROR::REPLY* pErrorReply = &errorReply;
	DWORD cbErrorReply = sizeof(NDAS_CMD_ERROR::REPLY);

	fSuccess = WriteHeader(
		opStatus,
		cbErrorReply + cbDataLength);

	if (!fSuccess) {
		return FALSE;
	}

	pErrorReply->dwErrorCode = dwErrorCode;
	pErrorReply->dwDataLength = cbDataLength;

	DWORD cbWritten;

	fSuccess = WritePacket(pErrorReply, cbErrorReply, &cbWritten);
	if (!fSuccess) {
		return FALSE;
	}

	if (NULL != lpData) {
		fSuccess = WritePacket(lpData, cbDataLength, &cbWritten);
		if (!fSuccess) {
			return FALSE;
		}
	}

	return TRUE;
}


#define NDAS_CMD_NO_EXTRA_DATA(cbExtraInData) \
	if (cbExtraInData > 0) { \
	return WriteErrorReply(NDAS_CMD_STATUS_INVALID_REQUEST); \
	}

#define NDAS_GET_REGISTRAR(pRegistrar) \
	CNdasInstanceManager* pInstanceManager = CNdasInstanceManager::Instance(); \
	_ASSERTE(NULL != pInstanceManager); \
	CNdasDeviceRegistrar* pRegistrar = pInstanceManager->GetRegistrar(); \
	_ASSERTE(NULL != pRegistrar); \
	CAutoLock autolock(pRegistrar);

#define NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan) \
	CNdasInstanceManager* pInstanceManager = CNdasInstanceManager::Instance(); \
	_ASSERTE(NULL != pInstanceManager); \
	CNdasLogicalDeviceManager* pLogDevMan = pInstanceManager->GetLogDevMan(); \
	_ASSERTE(NULL != pLogDevMan); \
	CAutoLock autolock(pLogDevMan);

#define NDAS_DECLARE_REPLY_DATA(_CMD_STRUCT_, pReply, cbReply, cbReplyExtra, cbReplyExtraValue) \
	_CMD_STRUCT_::REPLY buffer; \
	_CMD_STRUCT_::REPLY* pReply = &buffer; \
	DWORD cbReply = (sizeof(buffer) > 1) ? sizeof(buffer) : 0; \
	DWORD cbReplyExtra(cbReplyExtraValue);

#define NDAS_DECLARE_REPLY_DATA_NOEXTRA(_CMD_STRUCT_,pReply, cbReply) \
	NDAS_DECLARE_REPLY_DATA(_CMD_STRUCT_, pReply, cbReply, cbReplyExtra, 0)

#define NDAS_WRITE_HEADER_SUCCESS(cbReply) \
	{ \
		BOOL fSuccess = WriteHeader(NDAS_CMD_STATUS_SUCCESS, cbReply); \
		if (!fSuccess) { \
			return FALSE; \
		} \
	}
	
#define NDAS_WRITE_PACKET(pReply, cbReply) \
	{ \
		DWORD cbWritten; \
		BOOL fSuccess = WritePacket(pReply, cbReply, &cbWritten); \
		if (!fSuccess) { \
			return FALSE; \
		} \
	}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_REGISTER_DEVICE
//
//////////////////////////////////////////////////////////////////////////

static bool NcpIsBadRegisterFlags(DWORD dwRegFlags);

bool NcpIsBadRegisterFlags(DWORD dwRegFlags)
{
	static const DWORD LEGIMATE_REG_FLAGS = 
		NDAS_DEVICE_REG_FLAG_HIDDEN | 
		NDAS_DEVICE_REG_FLAG_VOLATILE |
		NDAS_DEVICE_REG_FLAG_USE_OEM_CODE;
	return 0 != (dwRegFlags & ~LEGIMATE_REG_FLAGS);
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_REGISTER_DEVICE::REQUEST>(
	NDAS_CMD_REGISTER_DEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	
	//
	// Preparing parameters
	//

	if (NcpIsBadRegisterFlags(pRequest->RegFlags))
	{
		// TODO: Create a new error entry
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ERROR_INVALID_PARAMETER);
	}

	//
	// Device Name
	//
	TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];

#ifdef UNICODE
	{
		// We should copy the string to prevent buffer overflow.
		HRESULT hr = ::StringCchCopy(
			szDeviceName, 
			MAX_NDAS_DEVICE_NAME_LEN + 1, 
			pRequest->wszDeviceName);

		// Ignore truncation
		if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) {
			return NDAS_CMD_STATUS_INVALID_REQUEST;
		}
	}
#else
	// guard against non-null terminated device name
	{
		DWORD cchLength;

		HRESULT hr = ::StringCchLengthW(
			pRequest->wszDeviceName,
			MAX_NDAS_DEVICE_NAME_LEN,
			&cchLength);

		_ASSERT(SUCCEEDED(hr));

		BOOL fSuccess = ::WideCharToMultiByte(
			CP_ACP,
			0,
			pRequest->wszDeviceName,
			cchLength,
			szDeviceName,
			MAX_NDAS_DEVICE_NAME_LEN,
			NULL,
			NULL);

		_ASSERT(fSuccess);
	}
#endif

	//
	// The actual operation should be recoverable
	//
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = 
		pRegistrar->Register(pRequest->DeviceId, pRequest->RegFlags);

	if (NULL == pDevice.p) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	pDevice->SetGrantedAccess(pRequest->GrantingAccess);
	pDevice->SetName(szDeviceName);

	// If USE_OEM_CODE is set, change the OEM Code
	if (NDAS_DEVICE_REG_FLAG_USE_OEM_CODE & pRequest->RegFlags)
	{
		pDevice->SetOemCode(&pRequest->OEMCode);
	}

	DWORD dwSlot = pDevice->GetSlotNo();

	_ASSERTE(0 != dwSlot);

	//
	// Write Response
	//

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_REGISTER_DEVICE, pReply, cbReply)

	pReply->SlotNo = dwSlot;

	if (!WriteHeader(NDAS_CMD_STATUS_SUCCESS, cbReply + cbReplyExtra)) { return FALSE; }

	DWORD cbWritten;
	if (!WritePacket(pReply, cbReply, &cbWritten) && cbReply != cbWritten) { return FALSE; }

	DumpPacket(pReply);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_DEVICES
//
//////////////////////////////////////////////////////////////////////////

struct NcpFilterHiddenDevice
{
	CNdasDeviceCollection& _coll;

	NcpFilterHiddenDevice(CNdasDeviceCollection& coll) : _coll(coll)
	{ }

	void operator ()(CNdasDevice* pDevice)
	{
		_ASSERTE(NULL != pDevice);
		if (NULL == pDevice) return;

		// Do not add hidden device
		if (pDevice->IsHidden()) return;

		// Add a non-hidden device to the collection 
		_coll.push_back(pDevice);
	}
};

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)

	CNdasDeviceRegistrar* pRegistrar = pGetNdasDeviceRegistrar();

	CNdasDeviceCollection coll;
	{
		CNdasDeviceCollection collAll;
		pRegistrar->Lock();
		pRegistrar->GetItems(collAll);
		pRegistrar->Unlock();
		// Filter out hidden devices
		std::for_each(
			collAll.begin(),
			collAll.end(),
			NcpFilterHiddenDevice(coll));
	}

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_DEVICES, pReply, cbReply, cbReplyExtra, 0);

	DWORD dwEntries = coll.size();

	pReply->nDeviceEntries = dwEntries;
	cbReplyExtra = (dwEntries * sizeof(NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY));

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pReply, sizeof(pReply))

	for (CNdasDeviceCollection::const_iterator itr = coll.begin();
		itr != coll.end();
		++itr)
	{
		NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY entry = {0};
		CNdasDevice* pDevice = *itr;

		pDevice->Lock();

		entry.DeviceId = pDevice->GetDeviceId();
		entry.SlotNo = pDevice->GetSlotNo();
		entry.GrantedAccess = pDevice->GetGrantedAccess();
		pDevice->GetName(
			MAX_NDAS_DEVICE_NAME_LEN+1,
			entry.wszDeviceName);
		
		pDevice->Unlock();

		DumpPacket(&entry);
		NDAS_WRITE_PACKET(&entry, sizeof(entry))
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_UNREGISTER_DEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_UNREGISTER_DEVICE::REQUEST>(
	NDAS_CMD_UNREGISTER_DEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_UNREGISTER_DEVICE, pReply, cbReply)
	NDAS_GET_REGISTRAR(pRegistrar)

	BOOL fSuccess(FALSE);
	if (pRequest->DeviceIdOrSlot.bUseSlotNo) {
		fSuccess = pRegistrar->Unregister(pRequest->DeviceIdOrSlot.SlotNo);
	} else {
		fSuccess = pRegistrar->Unregister(pRequest->DeviceIdOrSlot.DeviceId);
	}

	if (!fSuccess) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError()); 
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply, cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_DEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_DEVICE_INFORMATION::REQUEST>(
	NDAS_CMD_QUERY_DEVICE_INFORMATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_DEVICE_INFORMATION, pReply, cbReply)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	pDevice->Lock();

	const CNdasDevice::HARDWARE_INFO& hwInfo = pDevice->GetHardwareInfo();

	pReply->HardwareInfo.dwHwType =  hwInfo.ucType;
	pReply->HardwareInfo.dwHwVersion = hwInfo.ucVersion;
	pReply->HardwareInfo.nSlots = hwInfo.nSlots;
	pReply->HardwareInfo.nTargets = hwInfo.nTargets;
	pReply->HardwareInfo.nMaxTargets = hwInfo.nMaxTargets;
	pReply->HardwareInfo.nMaxLUs = hwInfo.nMaxLUs;
	pReply->HardwareInfo.nMaxRequestBlocks = hwInfo.nMaxRequestBlocks;

	pReply->DeviceId = pDevice->GetDeviceId();
	pReply->SlotNo = pDevice->GetSlotNo();
	pReply->GrantedAccess = pDevice->GetGrantedAccess();

	pReply->DeviceParams.RegFlags = NDAS_DEVICE_REG_FLAG_NONE;
	if (pDevice->IsHidden())
	{
		pReply->DeviceParams.RegFlags |=  NDAS_DEVICE_REG_FLAG_HIDDEN;
	}
	if (pDevice->IsAutoRegistered())
	{
		pReply->DeviceParams.RegFlags |=  NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED;
	}
	if (pDevice->IsVolatile()) 
	{
		pReply->DeviceParams.RegFlags |=  NDAS_DEVICE_REG_FLAG_VOLATILE;
	}

	BOOL fSuccess = pDevice->GetName(
		MAX_NDAS_DEVICE_NAME_LEN + 1,
		pReply->wszDeviceName);

	_ASSERTE(fSuccess);

	pDevice->Unlock();

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply, cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_DEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_DEVICE_STATUS::REQUEST>(
	NDAS_CMD_QUERY_DEVICE_STATUS::REQUEST* pRequest, DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_DEVICE_STATUS, pReply, cbReply)

	pReply->Status = pDevice->GetStatus();
	pReply->LastError = pDevice->GetLastDeviceError();

	BOOL fSuccess = WriteHeader(NDAS_CMD_STATUS_SUCCESS, cbReply);
	if (!fSuccess) { 
		return FALSE; 
	}
	
	DWORD cbWritten;
	fSuccess = WritePacket(pReply, cbReply, &cbWritten);
	if (!fSuccess) { 
		return FALSE; 
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_UNITDEVICES
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_UNITDEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_UNITDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_UNITDEVICES, pReply, cbReply, cbReplyExtra, 0)

	//
	// unit device numbers are not contiguous
	//
	ximeta::CAutoLock autoDeviceLock(pDevice.p);

	DWORD nUnitDevices = pDevice->GetUnitDeviceCount();

	cbReplyExtra = nUnitDevices * sizeof(NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY);
	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)

	pReply->nUnitDeviceEntries = nUnitDevices;
	NDAS_WRITE_PACKET(pReply, cbReply);

	for (DWORD i = 0; i < CNdasDevice::MAX_NDAS_UNITDEVICE_COUNT; ++i) {
		CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(i);
		if (NULL != pUnitDevice.p) {

			NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY entry;
			entry.UnitNo = i;
			entry.UnitDeviceType = pUnitDevice->GetType();

			DumpPacket(&entry);
			NDAS_WRITE_PACKET(&entry, sizeof(entry));
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_UNITDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_UNITDEVICE_INFORMATION::REQUEST>(
	NDAS_CMD_QUERY_UNITDEVICE_INFORMATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NDAS_UNITDEVICE_INFORMATION unitDevInfo = pUnitDevice->GetUnitDevInfo();

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_UNITDEVICE_INFORMATION, 
		pReply, cbReply, cbReplyExtra, 0)

	pReply->UnitDeviceType = pUnitDevice->GetType();
	pReply->UnitDeviceSubType = pUnitDevice->GetSubType();
	pReply->HardwareInfo.bLBA = unitDevInfo.bLBA;
	pReply->HardwareInfo.bLBA48 = unitDevInfo.bLBA48;
	pReply->HardwareInfo.nROHosts = unitDevInfo.dwROHosts;
	pReply->HardwareInfo.nRWHosts = unitDevInfo.dwRWHosts;
	pReply->HardwareInfo.MediaType = unitDevInfo.MediaType;
	pReply->HardwareInfo.bPIO = unitDevInfo.bPIO;
	pReply->HardwareInfo.bDMA = unitDevInfo.bDMA;
	pReply->HardwareInfo.bUDMA = unitDevInfo.bUDMA;

	HRESULT hr;
	
	hr = ::StringCchCopyN(
		pReply->HardwareInfo.szModel, 41, 
		unitDevInfo.szModel, 40);
	_ASSERT(SUCCEEDED(hr));
	
	hr = ::StringCchCopyN(
		pReply->HardwareInfo.szFwRev, 9, 
		unitDevInfo.szFwRev, 8);
	_ASSERT(SUCCEEDED(hr));
	
	hr = ::StringCchCopyN(
		pReply->HardwareInfo.szSerialNo, 41, 
		unitDevInfo.szSerialNo, 40);
	_ASSERT(SUCCEEDED(hr));

	pReply->HardwareInfo.SectorCountLowPart = 0xFFFFFFFF & unitDevInfo.SectorCount;
	pReply->HardwareInfo.SectorCountHighPart = unitDevInfo.SectorCount >> 32;

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_UNITDEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_UNITDEVICE_STATUS::REQUEST>(
	NDAS_CMD_QUERY_UNITDEVICE_STATUS::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_UNITDEVICE_STATUS, pReply, cbReply)

	pReply->Status = pUnitDevice->GetStatus();
	pReply->LastError = pUnitDevice->GetLastError();

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply, cbReply)

	return TRUE;

}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_SET_DEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_SET_DEVICE_PARAM::REQUEST>(
	NDAS_CMD_SET_DEVICE_PARAM::REQUEST* pRequest, DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_DEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

	switch (pParam->ParamType) {
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_ENABLE:
		fSuccess = pDevice->Enable(pParam->bEnable);
		break;
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME:
		pDevice->SetName(pParam->wszName);
		fSuccess = TRUE;
		break;
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS:
		pDevice->SetGrantedAccess(pParam->GrantingAccess);
		fSuccess = TRUE;
		break;
	}

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_SET_DEVICE_PARAM, pReply, cbReply)

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_SET_UNITDEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_SET_UNITDEVICE_PARAM::REQUEST>(
	NDAS_CMD_SET_UNITDEVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->dwUnitNo);
	if (NULL == pUnitDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_UNITDEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

	::SetLastError(NDASSVC_ERROR_NOT_IMPLEMENTED);

	if (!fSuccess) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			::GetLastError());
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_SET_UNITDEVICE_PARAM, pReply, cbReply)

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT::REQUEST>(
	NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	CRefObjPtr<CNdasDevice> pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice.p) { 
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT, pReply, cbReply)

	autolock.Release();

	BOOL fSuccess = pUnitDevice->GetActualHostUsageCount(
		&pReply->nROHosts,
		&pReply->nRWHosts,
		TRUE);

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			::GetLastError());
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_SET_LOGICALDEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_SET_LOGICALDEVICE_PARAM::REQUEST>(
	NDAS_CMD_SET_LOGICALDEVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

//	switch (pParam->ParamType) {
//		// add more parameter settings here
//	}
	::SetLastError(NDASSVC_ERROR_NOT_IMPLEMENTED);

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_SET_LOGICALDEVICE_PARAM, pReply, cbReply)

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE::REQUEST>(
	NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	NDAS_DEVICE_ID DeviceId;
	if (pRequest->DeviceIdOrSlot.bUseSlotNo) {
		NDAS_GET_REGISTRAR(pRegistrar)
		CRefObjPtr<CNdasDevice> pDevice = pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo);
		if (NULL == pDevice.p) {
			return WriteErrorReply(
				NDAS_CMD_STATUS_FAILED, 
				NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
		}
		DeviceId = pDevice->GetDeviceId();
	} else {
		DeviceId = pRequest->DeviceIdOrSlot.DeviceId;
	}

	NDAS_UNITDEVICE_ID unitDeviceId = {DeviceId, pRequest->UnitNo};
	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pGetNdasLogicalDevice(unitDeviceId);
	if (NULL == pLogDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE, pReply, cbReply)

	pReply->LogicalDeviceId = pLogDevice->GetLogicalDeviceId();

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_LOGICALDEVICES
//
//////////////////////////////////////////////////////////////////////////

//
// Filter out hidden logical devices
// A hidden logical device is a logical device, 
// of which the device of the primary unit device
// is hidden.
//
struct NcpFilterLogicalDevice
{
	CNdasLogicalDeviceCollection& _coll;

	NcpFilterLogicalDevice(CNdasLogicalDeviceCollection& coll) : _coll(coll)
	{ }

	void operator ()(CNdasLogicalDevice* pLogicalDevice)
	{
		_ASSERTE(NULL != pLogicalDevice);
		if (NULL == pLogicalDevice) return;

		//
		// Filter Logical Device filters out hidden devices
		// A logical device is hidden if the device of the first
		// unit device is set to hide.
		//
		CRefObjPtr<CNdasUnitDevice> pUnitDevice = pLogicalDevice->GetUnitDevice(0);

		//
		// If the first unit device of the logical device is not registered,
		// it is assumed that the device of the unit device is NOT hidden.
		// This happens when RAID members are partially registered,
		// and the first one is not registered.
		//
		if (NULL != pUnitDevice.p)
		{
			CRefObjPtr<CNdasDevice> pDevice = pUnitDevice->GetParentDevice();
			_ASSERTE(NULL != pDevice.p);

			//
			// Do not add hidden device
			//
			if (pDevice->IsHidden()) return;
		}

		//
		// Add a non-hidden device to the collection 
		//
		_coll.push_back(pLogicalDevice);
	}
};

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_LOGICALDEVICES, pReply, cbReply, cbReplyExtra, 0)

	typedef CNdasLogicalDeviceCollection Collection;

	Collection coll; // only non-hidden logical devices
	{
		//
		// Filter out hidden logical devices
		// A hidden logical device is a logical device, 
		// of which the device of the primary unit device
		// is hidden.
		//
		Collection collAll; // all logical devices (unfiltered)
		CNdasLogicalDeviceManager* pLogDevMan = pGetNdasLogicalDeviceManager();
		pLogDevMan->Lock();
		pLogDevMan->GetItems(collAll);
		pLogDevMan->Unlock();

		std::for_each(
			collAll.begin(), 
			collAll.end(), 
			NcpFilterLogicalDevice(coll));
	}


	DWORD nLogDevices =	coll.size();

	cbReplyExtra = nLogDevices * sizeof(NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY);
	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)

	pReply->nLogicalDeviceEntries = nLogDevices;
	NDAS_WRITE_PACKET(pReply, cbReply);

	for (Collection::const_iterator itr = coll.begin(); itr != coll.end(); ++itr) {

		CNdasLogicalDevice* pLogicalDevice = *itr;

		_ASSERTE(NULL != pLogicalDevice);
		NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY entry;
		entry.LogicalDeviceId = pLogicalDevice->GetLogicalDeviceId();
		entry.LogicalDeviceType = pLogicalDevice->GetType();

		DumpPacket(&entry);

		NDAS_WRITE_PACKET(&entry, sizeof(entry))
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_PLUGIN_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_PLUGIN_LOGICALDEVICE::REQUEST>(
	NDAS_CMD_PLUGIN_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_PLUGIN_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = 
		pLogDevMan->Find(pRequest->LogicalDeviceId);

	if (NULL == pLogDevice.p)
	{
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	ACCESS_MASK requestingAccess = pRequest->Access;
	ACCESS_MASK allowedAccess = pLogDevice->GetGrantedAccess();
	if ((requestingAccess & allowedAccess) != requestingAccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASUSER_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED); 
	}

	BOOL fSuccess = pLogDevice->PlugIn(requestingAccess);

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pReply, cbReply);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_EJECT_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_EJECT_LOGICALDEVICE::REQUEST>(
	NDAS_CMD_EJECT_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_EJECT_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	BOOL fSuccess = pLogDevice->Eject();
	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pReply, cbReply);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_UNPLUG_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_UNPLUG_LOGICALDEVICE::REQUEST>(
	NDAS_CMD_UNPLUG_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_UNPLUG_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	BOOL fSuccess = pLogDevice->Unplug();
	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pReply, cbReply);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REQUEST>(
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	DWORD nEntries = pLogDevice->GetUnitDeviceCount();

	pReply->LogicalDeviceType = pLogDevice->GetType();
	pReply->GrantedAccess = pLogDevice->GetGrantedAccess();
	pReply->MountedAccess = pLogDevice->GetMountedAccess();
	pReply->nUnitDeviceEntries = nEntries;

	// Logical Device Params
	pReply->LogicalDeviceParams.CurrentMaxRequestBlocks = 
		pLogDevice->GetCurrentMaxRequestBlocks();

	switch (pLogDevice->GetType()) {
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		{
			pReply->LogicalDiskInfo.Blocks = pLogDevice->GetUserBlockCount();

			break;
		}
	case NDAS_LOGICALDEVICE_TYPE_DVD:
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
	case NDAS_LOGICALDEVICE_TYPE_MO:
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
		break;
	default:
		{
			return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
				NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
			break;
		}
	}

	cbReplyExtra = 
		sizeof(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::UNITDEVICE_ENTRY) * nEntries;

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra);
	NDAS_WRITE_PACKET(pReply,cbReply);

	for (DWORD i = 0; i < nEntries; ++i) {
		NDAS_UNITDEVICE_ID entry = pLogDevice->GetUnitDeviceID(i);
		NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::UNITDEVICE_ENTRY replyEntry;
		replyEntry.DeviceId = entry.DeviceId;
		replyEntry.UnitNo = entry.UnitNo;
		NDAS_WRITE_PACKET(&replyEntry,sizeof(replyEntry));
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_LOGICALDEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_LOGICALDEVICE_STATUS::REQUEST>(
	NDAS_CMD_QUERY_LOGICALDEVICE_STATUS::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_LOGICALDEVICE_STATUS, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	pReply->Status = pLogDevice->GetStatus();
	pReply->LastError = pLogDevice->GetLastError();

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply, cbReply);

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_HOST_INFO::REQUEST>(
	NDAS_CMD_QUERY_HOST_INFO::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_HOST_INFO, pReply, cbReply, cbReplyExtra, 0)

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	const NDAS_HOST_INFO* pHostInfo = pHostInfoCache->GetHostInfo(&pRequest->HostGuid);

	if (NULL == pHostInfo) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			NDASSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pHostInfo, cbReply)

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REQUEST>(
	NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_HOST_INFO, pReply, cbReply, cbReplyExtra, 0)

	return WriteErrorReply(NDAS_CMD_STATUS_FAILED,NDASSVC_ERROR_NOT_IMPLEMENTED);

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_HOST_UNITDEVICE::REQUEST>(
	NDAS_CMD_QUERY_HOST_UNITDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_HOST_UNITDEVICE, pReply, cbReply, cbReplyExtra, 0)

	CRefObjPtr<CNdasDevice> pDevice = (pRequest->DeviceIdOrSlot.bUseSlotNo) ?
		pGetNdasDevice(pRequest->DeviceIdOrSlot.SlotNo) :
		pGetNdasDevice(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	BOOL fSuccess = hixdisc.Initialize();
	DWORD nROHosts, nRWHosts;
	fSuccess = pUnitDevice->GetHostUsageCount(&nROHosts,&nRWHosts,TRUE);
	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	DWORD nHosts = nROHosts + nRWHosts;

	DBGPRT_INFO(_FT("Expected: nHosts %d, nRoHost %d, nRwHost %d\n"), nHosts, nROHosts, nRWHosts);

	//
	// discover only for all hosts
	// NHIX_UDA_READ_ACCESS means to discover 
	// for hosts using READ_ACCESS, which includes RW hosts
	//

	NDAS_HIX_UDA uda = NHIX_UDA_READ_ACCESS; 
	NDAS_UNITDEVICE_ID unitDeviceId = pUnitDevice->GetUnitDeviceId();

	if (nHosts > 0) {

		fSuccess = hixdisc.Discover(unitDeviceId,uda,nHosts,1000);
		if (!fSuccess) {
			return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
		}

		nHosts = hixdisc.GetHostCount(unitDeviceId);
	}

	cbReplyExtra = nHosts * sizeof(NDAS_CMD_QUERY_HOST_ENTRY);
	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)

	pReply->EntryCount = nHosts;
	NDAS_WRITE_PACKET(pReply, cbReply)

	for (DWORD i = 0; i < nHosts; ++i) {

		NDAS_HIX_UDA hostUda = 0;
		NDAS_CMD_QUERY_HOST_ENTRY hostEntry = {0};

		fSuccess = hixdisc.GetHostData(
			unitDeviceId,
			i,
			&hostUda,
			&hostEntry.HostGuid,
			NULL,
			NULL);

		if (hostUda & NHIX_UDA_READ_ACCESS) {
			hostEntry.Access |= GENERIC_READ;
		}
		if (hostUda & NHIX_UDA_WRITE_ACCESS) {
			hostEntry.Access |= GENERIC_WRITE;
		}

		if (!fSuccess) {
			// warning!!!
		}

		NDAS_WRITE_PACKET(&hostEntry, sizeof(NDAS_CMD_QUERY_HOST_ENTRY));
	}

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_REQUEST_SURRENDER_ACCESS::REQUEST>(
	NDAS_CMD_REQUEST_SURRENDER_ACCESS::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_HOST_UNITDEVICE, pReply, cbReply, cbReplyExtra, 0)

	NHIX_UDA nhixUDA = 0;
	if (pRequest->Access & GENERIC_READ) nhixUDA |= NHIX_UDA_READ_ACCESS;
	if (pRequest->Access & GENERIC_WRITE) nhixUDA |= NHIX_UDA_WRITE_ACCESS;

	if (0 == nhixUDA) {
		return WriteErrorReply(NDAS_CMD_STATUS_INVALID_REQUEST);
	}

	CRefObjPtr<CNdasDevice> pDevice = 
		(pRequest->DeviceIdOrSlot.bUseSlotNo) ?
			pGetNdasDevice(pRequest->DeviceIdOrSlot.SlotNo) :
			pGetNdasDevice(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	SOCKADDR_LPX boundAddr, remoteAddr;
	BOOL fSuccess = pHostInfoCache->GetHostNetworkInfo(
		&pRequest->HostGuid,
		&boundAddr,
		&remoteAddr);

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			NDASSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
	}


	CNdasHIXSurrenderAccessRequest sar(pGetNdasHostGuid());
	fSuccess = sar.Initialize();
	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			::GetLastError());
	}

	NDAS_UNITDEVICE_ID unitDeviceId = pUnitDevice->GetUnitDeviceId();
	fSuccess = sar.Request(
		&boundAddr, 
		&remoteAddr, 
		unitDeviceId, 
		nhixUDA, 
		2000);

	if (!fSuccess) {
		// TODO: Last Error may not be correct.
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			::GetLastError());
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE::REQUEST>(
	NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE, pReply, cbReply, cbReplyExtra, 0)

	CRefObjPtr<CNdasDevice> pDevice = (pRequest->DeviceIdOrSlot.bUseSlotNo) ?
		pGetNdasDevice(pRequest->DeviceIdOrSlot.SlotNo) :
		pGetNdasDevice(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	if (NDAS_DEVICE_STATUS_DISABLED == pDevice->GetStatus()) {
		// do nothing for disabled device
	} else {
		pDevice->Lock();
		(VOID) pDevice->Enable(FALSE);
		(VOID) pDevice->Enable(TRUE);
		pDevice->Unlock();
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply);
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_GET_SERVICE_PARAM::REQUEST>(
	NDAS_CMD_GET_SERVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_GET_SERVICE_PARAM, pReply, cbReply, cbReplyExtra, 0)

	if (NDASSVC_PARAM_SUSPEND_PROCESS == pRequest->ParamCode) {

		DWORD dwValue = NdasServiceConfig::Get(nscSuspendOptions);

		pReply->Param.ParamCode = NDASSVC_PARAM_SUSPEND_PROCESS;
		pReply->Param.Value.DwordValue = dwValue;

		NDAS_WRITE_HEADER_SUCCESS(cbReply)
		NDAS_WRITE_PACKET(pReply,cbReply)

		return TRUE;

	} else {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_INVALID_SERVICE_PARAMETER);
	}

	return TRUE;
}

static void NcpInvalidateEncryptedUnitDevice(CNdasDevice* pDevice);

void 
NcpInvalidateEncryptedUnitDevice(CNdasDevice* pDevice)
{
	ximeta::CAutoLock autolock(pDevice);
	DWORD count = pDevice->GetUnitDeviceCount();

	// Invalid key will not create an instance of NDAS unit device
	// So we should do this for non-instantiated unit devices also
	// This is a workaround and should be changed!
	if (0 == count && NDAS_DEVICE_STATUS_CONNECTED == pDevice->GetStatus())
	{
		pDevice->Lock();
		pDevice->Enable(FALSE);
		pDevice->Enable(TRUE);
		pDevice->Unlock();
	}

	for (DWORD i = 0; i < count; ++i)
	{
		CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(i);
		if (NULL != pUnitDevice.p)
		{
			if (NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType())
			{
				CNdasUnitDiskDevice* pUnitDiskDevice =
					reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice.p);
				if (NDAS_CONTENT_ENCRYPT_METHOD_NONE != 
					pUnitDiskDevice->GetEncryption().Method)
				{
					(VOID) pDevice->InvalidateUnitDevice(i);
				}
			}
		}
	}
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_SET_SERVICE_PARAM::REQUEST>(
	NDAS_CMD_SET_SERVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_SET_SERVICE_PARAM, pReply, cbReply, cbReplyExtra, 0)

	if (NDASSVC_PARAM_SUSPEND_PROCESS == pRequest->Param.ParamCode) {

		if (NDASSVC_SUSPEND_DENY == pRequest->Param.Value.DwordValue ||
			NDASSVC_SUSPEND_ALLOW == pRequest->Param.Value.DwordValue ) 
// ||
//			NDASSVC_SUSPEND_EJECT_ALL == pRequest->Param.DwordValue ||
//			NDASSVC_SUSPEND_UNPLUG_ALL == pRequest->Param.DwordValue)
		{

			NdasServiceConfig::Set(nscSuspendOptions, pRequest->Param.Value.DwordValue);

			NDAS_WRITE_HEADER_SUCCESS(cbReply)
			NDAS_WRITE_PACKET(pReply,cbReply)

			return TRUE;

		} 
		else 
		{
			return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
				NDASSVC_ERROR_INVALID_SERVICE_PARAMETER);
		}

	}
	else if (NDASSVC_PARAM_RESET_SYSKEY == pRequest->Param.ParamCode) 
	{
		CNdasDeviceRegistrar* pRegistrar = pGetNdasDeviceRegistrar();
		CNdasDeviceCollection coll;
		pRegistrar->Lock();
		pRegistrar->GetItems(coll);
		pRegistrar->Unlock();
		std::for_each(
			coll.begin(), 
			coll.end(), 
			NcpInvalidateEncryptedUnitDevice);

		NDAS_WRITE_HEADER_SUCCESS(cbReply)
		NDAS_WRITE_PACKET(pReply,cbReply)

		return TRUE;
	} 
	else 
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASSVC_ERROR_INVALID_SERVICE_PARAMETER);
	}

}


//////////////////////////////////////////////////////////////////////////
//
// Process
//
//////////////////////////////////////////////////////////////////////////

BOOL 
CNdasCommandProcessor::Process()
{
	BOOL fSuccess(FALSE);
	
	fSuccess = ReadHeader();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Reading Header failed: "));
		return FALSE;
	}

	// handling version mismatch
	if (m_requestHeader.VersionMajor != NDAS_CMD_PROTOCOL_VERSION_MAJOR ||
		m_requestHeader.VersionMinor != NDAS_CMD_PROTOCOL_VERSION_MINOR)
	{
		DBGPRT_ERR(_FT("HPVM:Handling Protocol Version Mismatch!\n"));

		// we should read the remaining packet!
		DWORD cbToRead = m_requestHeader.MessageSize - sizeof(NDAS_CMD_HEADER);
		DWORD cbRead(0);
		LPBYTE lpBuffer = new BYTE[cbToRead];
		fSuccess = ReadPacket(lpBuffer, cbToRead, &cbRead);
		delete [] lpBuffer; // discard the buffer!
		if (!fSuccess) {
			DBGPRT_ERR(_FT("HPVM:Reading remaining packet failed!\n"));
			DBGPRT_ERR(_FT("HPVM:Fin\n"));
			return FALSE;
		}

		fSuccess = WriteHeader(NDAS_CMD_STATUS_UNSUPPORTED_VERSION, 0);
		if (!fSuccess) {
			DBGPRT_ERR(_FT("HPVM:Writing Reply Header failed!\n"));
			DBGPRT_ERR(_FT("HPVM:Fin\n"));
			return FALSE;
		}

		DBGPRT_ERR(_FT("HPVM:Fin\n"));
		return TRUE;
	}

#define CASE_PROCESS_COMMAND(cmdType, _CMD_STRUCT_) \
	case cmdType: \
		{ \
			_CMD_STRUCT_::REQUEST buffer; \
			_CMD_STRUCT_::REQUEST* pRequest = &buffer; \
			DWORD cbRequest = (sizeof(_CMD_STRUCT_::REQUEST) > 1) ? sizeof(_CMD_STRUCT_::REQUEST) : 0; \
			DWORD cbData = m_requestHeader.MessageSize - sizeof(NDAS_CMD_HEADER) - cbRequest; \
			DWORD cbRead(0); \
			BOOL fSuccess = ReadPacket(pRequest, cbRequest, &cbRead); \
			if (!fSuccess || cbRead != cbRequest) { \
				return FALSE; \
			} \
			return ProcessCommandRequest(pRequest, cbData); \
		}

	switch (m_command) {
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_REGISTER_DEVICE,NDAS_CMD_REGISTER_DEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_UNREGISTER_DEVICE,NDAS_CMD_UNREGISTER_DEVICE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_DEVICES,NDAS_CMD_ENUMERATE_DEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_DEVICE_STATUS,NDAS_CMD_QUERY_DEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION,NDAS_CMD_QUERY_DEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_DEVICE_PARAM,NDAS_CMD_SET_DEVICE_PARAM)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES,NDAS_CMD_ENUMERATE_UNITDEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_STATUS,NDAS_CMD_QUERY_UNITDEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_INFORMATION,NDAS_CMD_QUERY_UNITDEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM,NDAS_CMD_SET_UNITDEVICE_PARAM)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT,NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE,NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES,NDAS_CMD_ENUMERATE_LOGICALDEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION,NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS,NDAS_CMD_QUERY_LOGICALDEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM,NDAS_CMD_SET_LOGICALDEVICE_PARAM)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE,NDAS_CMD_PLUGIN_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_EJECT_LOGICALDEVICE,NDAS_CMD_EJECT_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE,NDAS_CMD_UNPLUG_LOGICALDEVICE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_HOST_LOGICALDEVICE,NDAS_CMD_QUERY_HOST_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE,NDAS_CMD_QUERY_HOST_UNITDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_HOST_INFO,NDAS_CMD_QUERY_HOST_INFO)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS,NDAS_CMD_REQUEST_SURRENDER_ACCESS)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE,NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_SERVICE_PARAM,NDAS_CMD_SET_SERVICE_PARAM)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_GET_SERVICE_PARAM,NDAS_CMD_GET_SERVICE_PARAM)

	}

#undef CASE_PROCESS_COMMAND

	return FALSE;
}
