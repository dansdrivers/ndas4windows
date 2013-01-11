/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "ndaserror.h"

#include "ndascmd.h"
#include "ndasinstman.h"
#include "ndascmdproc.h"
#include "ndascmddbg.h"
#include "syncobj.h"
#include "ndasdevreg.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_CMDPROC
#include "xdebug.h"

using ximeta::CAutoLock;

//////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////

CNdasCommandProcessor::
CNdasCommandProcessor(ITransport* pTransport) :
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

CNdasCommandProcessor::
~CNdasCommandProcessor()
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
CNdasCommandProcessor::
InitOverlapped()
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
CNdasCommandProcessor::
ReadPacket(
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
CNdasCommandProcessor::
WritePacket(
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
CNdasCommandProcessor::
ReadHeader()
{
	DWORD cbRead;

	NDAS_CMD_HEADER* pRequestHeader = &m_requestHeader;	
	::ZeroMemory(pRequestHeader, sizeof(NDAS_CMD_HEADER));

	BOOL fSuccess = ReadPacket(
		pRequestHeader,
		sizeof(NDAS_CMD_HEADER),
		&cbRead);

	if (!fSuccess && ::GetLastError() != ERROR_MORE_DATA) {
		DPErrorEx(_FT("Read packet failed: "));
		return FALSE;
	}

	DPInfo(_FT("RequestHeader: %s\n"), CStructFormat().CreateString(pRequestHeader));

	// header check

	if (pRequestHeader->Protocol[0] != NDAS_CMD_PROTOCOL[0] ||
		pRequestHeader->Protocol[1] != NDAS_CMD_PROTOCOL[1] ||
		pRequestHeader->Protocol[2] != NDAS_CMD_PROTOCOL[2] ||
		pRequestHeader->Protocol[3] != NDAS_CMD_PROTOCOL[3])
	{
		DPError(_FT("Unexpected Protocol %c%c%c%c\n"),
			pRequestHeader->Protocol[0], pRequestHeader->Protocol[1],
			pRequestHeader->Protocol[2], pRequestHeader->Protocol[3]);
		return FALSE;
	}

	// version mismatch is handled from Process()

	if (cbRead != sizeof(NDAS_CMD_HEADER)) {
		// header size mismatch
		DPError(_FT("Header size mismatch, read %d bytes, expected value %d\n"),
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
CNdasCommandProcessor::
WriteHeader(NDAS_CMD_STATUS opStatus, DWORD cbDataSize)
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

	DPInfo(_FT("ReplyHeader: %s\n"), CStructFormat().CreateString(pReplyHeader));

	DWORD cbWritten;
	BOOL fSuccess = WritePacket(
		pReplyHeader, 
		cbReplyHeader,
		&cbWritten);

	if (!fSuccess) {
		DPErrorEx(_FT("Writing a header failed: "));
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
CNdasCommandProcessor::
WriteErrorReply(
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
		DWORD cbWritten;
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
CNdasCommandProcessor::
Process()
{
	BOOL fSuccess(FALSE);
	
	fSuccess = ReadHeader();
	if (!fSuccess) {
		DPErrorEx(_FT("Reading Header failed: "));
		return FALSE;
	}

	// handling version mismatch
	if (m_requestHeader.VersionMajor != NDAS_CMD_PROTOCOL_VERSION_MAJOR ||
		m_requestHeader.VersionMinor != NDAS_CMD_PROTOCOL_VERSION_MINOR)
	{
		DPError(_FT("HPVM:Handling Protocol Version Mismatch!\n"));

		// we should read the remaining packet!
		DWORD cbToRead = m_requestHeader.MessageSize - sizeof(NDAS_CMD_HEADER);
		DWORD cbRead(0);
		LPBYTE lpBuffer = new BYTE[cbToRead];
		fSuccess = ReadPacket(lpBuffer, cbToRead, &cbRead);
		delete [] lpBuffer; // discard the buffer!
		if (!fSuccess) {
			DPError(_FT("HPVM:Reading remaining packet failed!\n"));
			DPError(_FT("HPVM:Fin\n"));
			return FALSE;
		}

		fSuccess = WriteHeader(NDAS_CMD_STATUS_UNSUPPORTED_VERSION, 0);
		if (!fSuccess) {
			DPError(_FT("HPVM:Writing Reply Header failed!\n"));
			DPError(_FT("HPVM:Fin\n"));
			return FALSE;
		}

		DPError(_FT("HPVM:Fin\n"));
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
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE,NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES,NDAS_CMD_ENUMERATE_LOGICALDEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION,NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS,NDAS_CMD_QUERY_LOGICALDEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM,NDAS_CMD_SET_LOGICALDEVICE_PARAM)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE,NDAS_CMD_PLUGIN_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_EJECT_LOGICALDEVICE,NDAS_CMD_EJECT_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE,NDAS_CMD_UNPLUG_LOGICALDEVICE)
	}

#undef CASE_PROCESS_COMMAND

	return FALSE;
}


#define NDAS_CMD_NO_EXTRA_DATA(cbExtraInData) \
	if (cbExtraInData > 0) { \
	return WriteErrorReply(NDAS_CMD_STATUS_INVALID_REQUEST); \
	}

#define NDAS_GET_REGISTRAR(pRegistrar) \
	PCNdasInstanceManager pInstanceManager = CNdasInstanceManager::Instance(); \
	_ASSERTE(NULL != pInstanceManager); \
	PCNdasDeviceRegistrar pRegistrar = pInstanceManager->GetRegistrar(); \
	_ASSERTE(NULL != pRegistrar); \
	CAutoLock autolock(pRegistrar);

#define NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan) \
	PCNdasInstanceManager pInstanceManager = CNdasInstanceManager::Instance(); \
	_ASSERTE(NULL != pInstanceManager); \
	PCNdasLogicalDeviceManager pLogDevMan = pInstanceManager->GetLogDevMan(); \
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
CNdasCommandProcessor::
ProcessCommandRequest(
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

	PCNdasDevice pDevice = pRegistrar->Register(pRequest->DeviceId);

	if (NULL == pDevice) {
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_DEVICES, pReply, cbReply, cbReplyExtra, 0)

	cbReplyExtra = (pRegistrar->Size() * sizeof(NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY));

	pReply->nDeviceEntries = pRegistrar->Size();

	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)
	NDAS_WRITE_PACKET(pReply, sizeof(pReply))

	CNdasDeviceRegistrar::ConstIterator itr = pRegistrar->begin();
	for (; itr != pRegistrar->end(); ++itr) {

		NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY entry;

		DWORD dwSlot = itr->first;
		PCNdasDevice pDevice = itr->second;

		entry.DeviceId = pDevice->GetDeviceId();
		entry.SlotNo = pDevice->GetSlotNo();
		entry.GrantedAccess = pDevice->GetGrantedAccess();
		pDevice->GetName(MAX_NDAS_DEVICE_NAME_LEN + 1, entry.wszDeviceName);

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
CNdasCommandProcessor::
ProcessCommandRequest(
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_QUERY_DEVICE_INFORMATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_DEVICE_INFORMATION, pReply, cbReply)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice) { return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError()); }

	CNdasDevice::HARDWARE_INFO hwInfo;
	pDevice->GetHWInfo(&hwInfo);

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
	pDevice->GetName(MAX_NDAS_DEVICE_NAME_LEN + 1, pReply->wszDeviceName);

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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_QUERY_DEVICE_STATUS::REQUEST* pRequest, DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice) { return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError()); }

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_DEVICE_STATUS, pReply, cbReply)

	pReply->Status = pDevice->GetStatus();
	pReply->LastError = pDevice->GetLastError();

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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_ENUMERATE_UNITDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError()); 
	}

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_UNITDEVICES, pReply, cbReply, cbReplyExtra, 0)

	//
	// unit device numbers are not contiguous
	//
	ximeta::CAutoLock autoDeviceLock(pDevice);

	DWORD nUnitDevices = pDevice->GetUnitDeviceCount();

	cbReplyExtra = nUnitDevices * sizeof(NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY);
	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)

	pReply->nUnitDeviceEntries = nUnitDevices;
	NDAS_WRITE_PACKET(pReply, cbReply);

	for (DWORD i = 0; i < CNdasDevice::MAX_NDAS_UNITDEVICE_COUNT; ++i) {
		PCNdasUnitDevice pUnitDevice = pDevice->GetUnitDevice(i);
		if (NULL != pUnitDevice) {

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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_QUERY_UNITDEVICE_INFORMATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);

	if (NULL == pDevice) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasUnitDevice pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NDAS_UNITDEVICE_INFORMATION unitDevInfo;
	pUnitDevice->GetUnitDevInfo(&unitDevInfo);

	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_QUERY_UNITDEVICE_INFORMATION, 
		pReply, cbReply, cbReplyExtra, 0)

	pReply->UnitDeviceType = pUnitDevice->GetType();
	pReply->UnitDeviceSubType = pUnitDevice->GetSubType();
	pReply->HardwareInfo.bLBA = unitDevInfo.bLBA;
	pReply->HardwareInfo.bLBA48 = unitDevInfo.bLBA48;
	pReply->HardwareInfo.nROHosts = unitDevInfo.dwROHosts;
	pReply->HardwareInfo.nRWHosts = unitDevInfo.dwRWHosts;

	DWORD nROHostsStats = 0, nRWHostStats = 0;
	BOOL fSuccess = pUnitDevice->GetHostUsageCount(
		&nROHostsStats, 
		&nRWHostStats, 
		TRUE);

	if (!fSuccess) {
		DPErrorEx(_FT("Getting Host Usage Count failed: "));
	}

	pReply->UsageStats.nROHosts = nROHostsStats;
	pReply->UsageStats.nRWHosts = nRWHostStats;

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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_QUERY_UNITDEVICE_STATUS::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasUnitDevice pUnitDevice = pDevice->GetUnitDevice(pRequest->UnitNo);
	if (NULL == pUnitDevice) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_SET_DEVICE_PARAM::REQUEST* pRequest, DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_DEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

	switch (pParam->ParamType) {
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_ENABLE:
		fSuccess = pDevice->Enable(pParam->bEnable);
		break;
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME:
		fSuccess = pDevice->SetName(pParam->wszName);
		break;
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS:
		fSuccess = pDevice->SetGrantedAccess(pParam->GrantingAccess);
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_SET_UNITDEVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_REGISTRAR(pRegistrar)

	PCNdasDevice pDevice = pRequest->DeviceIdOrSlot.bUseSlotNo ?
		pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo) :
		pRegistrar->Find(pRequest->DeviceIdOrSlot.DeviceId);
	if (NULL == pDevice) { 
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasUnitDevice pUnitDevice = pDevice->GetUnitDevice(pRequest->dwUnitNo);
	if (NULL == pUnitDevice) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_UNITDEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

//	switch (pParam->ParamType) {
//		// add more parameter settings here
//	}
	::SetLastError(NDASHLPSVC_ERROR_NOT_IMPLEMENTED);

	if (!fSuccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, ::GetLastError());
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_SET_UNITDEVICE_PARAM, pReply, cbReply)

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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_SET_LOGICALDEVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	// 
	// For LID (SlotNo, TargetId, LUN),
	// TargetId, and LUN is always 0 at this time
	// 
	if (pRequest->TargetId != 0 || pRequest->LUN != 0) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(pRequest->SlotNo);
	if (NULL == pLogDevice) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	NDAS_DEVICE_ID DeviceId;
	if (pRequest->DeviceIdOrSlot.bUseSlotNo) {
		NDAS_GET_REGISTRAR(pRegistrar)
		PCNdasDevice pDevice = pRegistrar->Find(pRequest->DeviceIdOrSlot.SlotNo);
		if (NULL == pDevice) {
			return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
				NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
		}
		DeviceId = pDevice->GetDeviceId();
	} else {
		DeviceId = pRequest->DeviceIdOrSlot.DeviceId;
	}

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(DeviceId, pRequest->UnitNo);
	if (NULL == pLogDevice) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE, pReply, cbReply)

	pReply->SlotNo = pLogDevice->GetSlot();
	pReply->TargetId = 0; // always 0
	pReply->LUN = 0; // always 0

	NDAS_WRITE_HEADER_SUCCESS(cbReply)
	NDAS_WRITE_PACKET(pReply,cbReply)

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_LOGICALDEVICES
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA(NDAS_CMD_ENUMERATE_LOGICALDEVICES, pReply, cbReply, cbReplyExtra, 0)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	DWORD nLogDevices =	pLogDevMan->Size();

	cbReplyExtra = nLogDevices * sizeof(NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY);
	NDAS_WRITE_HEADER_SUCCESS(cbReply + cbReplyExtra)

	pReply->nLogicalDeviceEntries = nLogDevices;
	NDAS_WRITE_PACKET(pReply, cbReply);

	CNdasLogicalDeviceManager::ConstIterator itr = pLogDevMan->begin();
	for (; itr != pLogDevMan->end(); ++itr) {

		PCNdasLogicalDevice pLogDev = itr->second;

		NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY entry;

		entry.LogicalDeviceId.SlotNo = pLogDev->GetSlot();
		entry.LogicalDeviceId.TargetId = 0;
		entry.LogicalDeviceId.LUN = 0;
		entry.LogicalDeviceType = pLogDev->GetType();

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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_PLUGIN_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_PLUGIN_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	if (pRequest->LogicalDeviceId.TargetId != 0 ||
		pRequest->LogicalDeviceId.LUN != 0)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasLogicalDevice pLogDevice = 
		pLogDevMan->Find(pRequest->LogicalDeviceId.SlotNo);

	if (NULL == pLogDevice)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	BOOL fSuccess(FALSE);

	ACCESS_MASK requestingAccess = pRequest->Access;
	ACCESS_MASK allowedAccess = pLogDevice->GetGrantedAccess();
	if ((requestingAccess & allowedAccess) != requestingAccess) {
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASUSER_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED); 
	}

	fSuccess = pLogDevice->PlugIn(requestingAccess);
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_EJECT_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_EJECT_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	if (pRequest->LogicalDeviceId.TargetId != 0 ||
		pRequest->LogicalDeviceId.LUN != 0)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId.SlotNo);
	if (NULL == pLogDevice)
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_UNPLUG_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_UNPLUG_LOGICALDEVICE, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	if (pRequest->LogicalDeviceId.TargetId != 0 ||
		pRequest->LogicalDeviceId.LUN != 0)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId.SlotNo);
	if (NULL == pLogDevice)
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
// NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	if (pRequest->LogicalDeviceId.TargetId != 0 ||
		pRequest->LogicalDeviceId.LUN != 0)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId.SlotNo);
	if (NULL == pLogDevice)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	DWORD nEntries = pLogDevice->GetUnitDeviceCount();

	pReply->LogicalDeviceType = pLogDevice->GetType();
	pReply->GrantedAccess = pLogDevice->GetGrantedAccess();
	pReply->MountedAccess = pLogDevice->GetMountedAccess();
	pReply->nUnitDeviceEntries = nEntries;

	switch (pLogDevice->GetType()) {
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID_1:
		{
			PCNdasLogicalDisk pLogDisk = 
				reinterpret_cast<PCNdasLogicalDisk>(pLogDevice);

			// pReply->LogicalDiskInfo.Blocks = pLogDisk->GetUserBlocks();
			pReply->LogicalDiskInfo.Blocks = pLogDisk->GetUserBlockCount();

			break;
		}
	case NDAS_LOGICALDEVICE_TYPE_DVD:
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
	case NDAS_LOGICALDEVICE_TYPE_MO:
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
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
		NDAS_UNITDEVICE_ID entry = pLogDevice->GetUnitDeviceId(i);
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
CNdasCommandProcessor::
ProcessCommandRequest(
	NDAS_CMD_QUERY_LOGICALDEVICE_STATUS::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData)
	NDAS_DECLARE_REPLY_DATA_NOEXTRA(NDAS_CMD_QUERY_LOGICALDEVICE_STATUS, pReply, cbReply)

	NDAS_GET_LOGICAL_DEVICE_MANAGER(pLogDevMan)

	if (pRequest->LogicalDeviceId.TargetId != 0 ||
		pRequest->LogicalDeviceId.LUN != 0)
	{
		return WriteErrorReply(NDAS_CMD_STATUS_FAILED, 
			NDASHLPSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(pRequest->LogicalDeviceId.SlotNo);
	if (NULL == pLogDevice)
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

