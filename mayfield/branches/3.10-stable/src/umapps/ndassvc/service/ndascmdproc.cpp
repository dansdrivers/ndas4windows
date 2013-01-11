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

	if (pRequestHeader->Protocol[0] != NDAS_CMD_PROTOCOL[0] ||
		pRequestHeader->Protocol[1] != NDAS_CMD_PROTOCOL[1] ||
		pRequestHeader->Protocol[2] != NDAS_CMD_PROTOCOL[2] ||
		pRequestHeader->Protocol[3] != NDAS_CMD_PROTOCOL[3])
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
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_RECOVER_LOGICALDEVICE,NDAS_CMD_RECOVER_LOGICALDEVICE)

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

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_REGISTER_DEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	
	//
	// Preparing parameters
	//

	TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];

	NDAS_GET_REGISTRAR(pRegistrar)

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

	CRefObjPtr<CNdasDevice> pDevice = pRegistrar->Register(pRequest->DeviceId);

	if (NULL == pDevice.p) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	pDevice->SetGrantedAccess(pRequest->GrantingAccess);
	(VOID) pDevice->SetName(szDeviceName);
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

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)

	CNdasDeviceRegistrar* pRegistrar = pGetNdasDeviceRegistrar();
	pRegistrar->Lock();
	CNdasDeviceCollection coll;
	pRegistrar->GetItems(coll);
	pRegistrar->Unlock();

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_DEVICES, pReply, cbReply, cbReplyExtra, 0)

	cbReplyExtra = (coll.size() * sizeof(NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY));

	pReply->nDeviceEntries = coll.size();

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pReply, sizeof(pReply))

	for (CNdasDeviceCollection::const_iterator itr = coll.begin();
		itr != coll.end();
		++itr)
	{
		NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY entry = {0};
		CNdasDevice* pDevice = *itr;

		entry.DeviceId = pDevice->GetDeviceId();
		entry.SlotNo = pDevice->GetSlotNo();
		entry.GrantedAccess = pDevice->GetGrantedAccess();

		pDevice->Lock();
		HRESULT hr = ::StringCchCopy(
			entry.wszDeviceName, 
			MAX_NDAS_DEVICE_NAME_LEN+1, 
			pDevice->GetName());
		_ASSERTE(SUCCEEDED(hr));
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
CNdasCommandProcessor::ProcessCommandRequest(
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
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

	/* 3.10 only supports NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED flags */
	pReply->DeviceParams.RegFlags = 
		pDevice->IsAutoRegistered() ? NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED : 0;

	DWORD devicePropFlags = 0x00000000;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(_T("ndassvc"), _T("DevicePropFlags"), &devicePropFlags);
	if (fSuccess && (devicePropFlags & 0x00080000))
	{
		pReply->HardwareInfo.MACAddress.IsSet = TRUE;

		C_ASSERT(
			sizeof(pReply->HardwareInfo.MACAddress.Node) ==
			RTL_FIELD_SIZE(NDAS_DEVICE_ID,Node));

		CopyMemory(
			pReply->HardwareInfo.MACAddress.Node,
			&pDevice->GetDeviceId(),
			RTL_FIELD_SIZE(NDAS_DEVICE_ID,Node));
	}
	else
	{
		pReply->HardwareInfo.MACAddress.IsSet = FALSE;
	}

	HRESULT hr = ::StringCchCopy(
		pReply->wszDeviceName,
		MAX_NDAS_DEVICE_NAME_LEN + 1,
		pDevice->GetName());

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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->dwUnitNo);
	if (NULL == pUnitDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_UNITDEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

	::SetLastError(NDASHLPSVC_ERROR_NOT_IMPLEMENTED);

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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_SET_LOGICALDEVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p) {
		return WriteErrorReply(
			NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

//	switch (pParam->ParamType) {
//		// add more parameter settings here
//	}
	::SetLastError(NDASHLPSVC_ERROR_NOT_IMPLEMENTED);

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
CNdasCommandProcessor::ProcessCommandRequest(
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
				NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_LOGICALDEVICES, pReply, cbReply, cbReplyExtra, 0)

	typedef CNdasLogicalDeviceCollection Collection;

	Collection coll;
	{
		CNdasLogicalDeviceManager* pLogDevMan = pGetNdasLogicalDeviceManager();
		pLogDevMan->Lock();
		pLogDevMan->GetItems(coll);
		pLogDevMan->Unlock();
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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
// NDAS_CMD_RECOVER_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_RECOVER_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_RECOVER_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId);
	if (NULL == pLogDevice.p)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	BOOL fSuccess = pLogDevice->Recover();
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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
			const NDAS_CONTENT_ENCRYPT* pNCE = pLogDevice->GetContentEncrypt();
			if (NULL == pNCE)
			{
				break;
			}
			//
			// For now, we cannot retrieve Revision information from NDAS_CONTENT_ENCRYPT.
			// However, there is only one NDAS_CONTENT_ENCRYPT_REVISION if instantiated.
			//
			pReply->LogicalDiskInfo.ContentEncrypt.Revision = NDAS_CONTENT_ENCRYPT_REVISION;
			pReply->LogicalDiskInfo.ContentEncrypt.Type = pNCE->Method;
			pReply->LogicalDiskInfo.ContentEncrypt.KeyLength = pNCE->KeyLength;
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
				NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	pReply->Status = pLogDevice->GetStatus();
	pReply->LastError = pLogDevice->GetLastError();

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply, cbReply);

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_QUERY_HOST_INFO::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_HOST_INFO, pReply, cbReply, cbReplyExtra, 0)

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	const NDAS_HOST_INFO* pHostInfo = pHostInfoCache->GetHostInfo(&pRequest->HostGuid);

	if (NULL == pHostInfo) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			NDASHLPSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pHostInfo, cbReply)

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_HOST_INFO, pReply, cbReply, cbReplyExtra, 0)

	return WriteErrorReply(NDAS_CMD_STATUS_FAILED,NDASHLPSVC_ERROR_NOT_IMPLEMENTED);

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	BOOL fSuccess = hixdisc.Initialize();
	DWORD nROHosts, nRWHosts;
	fSuccess = pUnitDevice->GetHostUsageCount(&nROHosts,&nRWHosts,TRUE);
	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	// 0xFFFFFFFF is deemed infinity -> infinity + 1 = infinity!
	DWORD nHosts = (((DWORD)(-1) == nRWHosts) || ((DWORD)(-1) == nROHosts)) ?
		(DWORD)(-1) : nROHosts + nRWHosts;

	DBGPRT_INFO(_FT("Expected: nHosts %d, nRoHost %d, nRwHost %d\n"), nHosts, nROHosts, nRWHosts);

	//
	// discover only for all hosts
	// NHIX_UDA_READ_ACCESS means to discover 
	// for hosts using READ_ACCESS, which includes RW hosts
	//

	NDAS_HIX_UDA uda = NHIX_UDA_READ_ACCESS; 
	NDAS_UNITDEVICE_ID unitDeviceId = pUnitDevice->GetUnitDeviceId();

	if (nHosts > 0) {

		fSuccess = hixdisc.Discover(unitDeviceId,uda,nHosts,500);
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	SOCKADDR_LPX boundAddr, remoteAddr;
	BOOL fSuccess = pHostInfoCache->GetHostNetworkInfo(
		&pRequest->HostGuid,
		&boundAddr,
		&remoteAddr);

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED,
			NDASHLPSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
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
CNdasCommandProcessor::ProcessCommandRequest(
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
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice.p) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	if (NDAS_DEVICE_STATUS_DISABLED == pDevice->GetStatus()) {
		// do nothing for disabled device
	} else {
		(VOID) pDevice->Enable(FALSE);
		(VOID) pDevice->Enable(TRUE);
	}

	NDAS_WRITE_HEADER_SUCCESS(cbReply);
	NDAS_WRITE_PACKET(pReply,cbReply)

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_GET_SERVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_GET_SERVICE_PARAM, pReply, cbReply, cbReplyExtra, 0)

	if (NDASSVC_PARAM_SUSPEND_PROCESS == pRequest->ParamCode) {

		DWORD dwValue = 0;
		BOOL fSuccess = _NdasSystemCfg.GetValueEx(
			_T("ndassvc"),
			_T("Suspend"), 
			&dwValue);

		if (!fSuccess &&
			NDASSVC_SUSPEND_DENY != dwValue &&
			NDASSVC_SUSPEND_ALLOW != dwValue ) // &&
//			NDASSVC_SUSPEND_EJECT_ALL != dwValue &&
//			NDASSVC_SUSPEND_UNPLUG_ALL != dwValue)
		{
			dwValue = NDASSVC_SUSPEND_DENY;
		}

		pReply->Param.ParamCode = NDASSVC_PARAM_SUSPEND_PROCESS;
		pReply->Param.DwordValue = dwValue;

		NDAS_WRITE_HEADER_SUCCESS(cbReply)
		NDAS_WRITE_PACKET(pReply,cbReply)

		return TRUE;

	} else {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_INVALID_SERVICE_PARAMETER);
	}

	return TRUE;
}

static void 
InvalidateEncryptedUnitDevice(CNdasDevice* pDevice)
{
	ximeta::CAutoLock autolock(pDevice);
	DWORD count = pDevice->GetUnitDeviceCount();

	// Invalid key will not create an instance of NDAS unit device
	// So we should do this for non-instantiated unit devices also
	// This is a workaround and should be changed!
	if (0 == count && NDAS_DEVICE_STATUS_CONNECTED == pDevice->GetStatus())
	{
		pDevice->Enable(FALSE);
		pDevice->Enable(TRUE);
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
CNdasCommandProcessor::ProcessCommandRequest(
	NDAS_CMD_SET_SERVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_SET_SERVICE_PARAM, pReply, cbReply, cbReplyExtra, 0)

	if (NDASSVC_PARAM_SUSPEND_PROCESS == pRequest->Param.ParamCode) {

		if (NDASSVC_SUSPEND_DENY == pRequest->Param.DwordValue ||
			NDASSVC_SUSPEND_ALLOW == pRequest->Param.DwordValue ) 
// ||
//			NDASSVC_SUSPEND_EJECT_ALL == pRequest->Param.DwordValue ||
//			NDASSVC_SUSPEND_UNPLUG_ALL == pRequest->Param.DwordValue)
		{

			BOOL fSuccess = _NdasSystemCfg.SetValueEx(
				_T("ndassvc"),
				_T("Suspend"), 
				pRequest->Param.DwordValue);

			if (!fSuccess) 
			{
				return WriteErrorReply(
					NDAS_CMD_STATUS_FAILED, 
					::GetLastError());
			}

			NDAS_WRITE_HEADER_SUCCESS(cbReply)
			NDAS_WRITE_PACKET(pReply,cbReply)

			return TRUE;

		} 
		else 
		{
			return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
				NDASHLPSVC_ERROR_INVALID_SERVICE_PARAMETER);
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
			InvalidateEncryptedUnitDevice);

		NDAS_WRITE_HEADER_SUCCESS(cbReply)
		NDAS_WRITE_PACKET(pReply,cbReply)

		return TRUE;
	} 
	else 
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_INVALID_SERVICE_PARAMETER);
	}

}

