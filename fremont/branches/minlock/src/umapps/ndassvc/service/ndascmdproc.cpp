/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <ndas/ndasmsg.h>
#include <ndas/ndascmd.h>
#include <ndas/ndascmddbg.h>
#include <ndas/ndashostinfo.h>
#include <ndas/ndassvcparam.h>

#include "ndasdevid.h"
#include "ndascomobjectsimpl.hpp"

#include "ndasdevreg.h"
#include "ndashostinfocache.h"
#include "ndasobjs.h"
#include "ndashixcli.h"
#include "ndascfg.h"

#include "ndascmdproc.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndascmdproc.tmh"
#endif

template <typename T, typename ReplyT = T::REPLY>
struct NdasCmdReply : ReplyT
{
	NdasCmdReply()
	{
		::ZeroMemory(this, sizeof(ReplyT));
		C_ASSERT(sizeof(ReplyT) == sizeof(NdasCmdReply));
	}
	size_t Size() const
	{ 
		return sizeof(ReplyT) > 1 ? sizeof(ReplyT) : 0; 
	}
};

namespace 
{
	__forceinline bool FlagIsSet(DWORD Flags, DWORD TargetFlag)
	{
		return TargetFlag == (Flags & TargetFlag);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////

CNdasCommandProcessor::CNdasCommandProcessor(
	ITransport* pTransport) :
	m_pTransport(pTransport),
	m_command(NDAS_CMD_TYPE_NONE)
{
}

//////////////////////////////////////////////////////////////////////////
//
// Destructor
//
//////////////////////////////////////////////////////////////////////////

CNdasCommandProcessor::~CNdasCommandProcessor()
{
}

//////////////////////////////////////////////////////////////////////////
//
// InitOverlapped
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::InitOverlapped()
{
	if (m_hEvent.IsInvalid()) 
	{
		m_hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		XTLENSURE_RETURN_BOOL( !m_hEvent.IsInvalid() );
	}
	else 
	{
		XTLENSURE_RETURN_BOOL(::ResetEvent(m_hEvent));
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
	XTLASSERT(lpcbRead != NULL);
	XTLENSURE_RETURN_BOOL( InitOverlapped() );
	XTLENSURE_RETURN_BOOL( m_pTransport->Receive(lpBuffer, cbToRead, lpcbRead, &m_overlapped) );
	return TRUE;
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
	XTLENSURE_RETURN_BOOL( InitOverlapped() );

	// lpcbWritten is required for Send
	DWORD cbWritten;
	if (NULL == lpcbWritten)
	{
		lpcbWritten = &cbWritten;
	}

	XTLENSURE_RETURN_BOOL( m_pTransport->Send(lpBuffer, cbToWrite, lpcbWritten, &m_overlapped) );
	return TRUE;
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

	BOOL success = ReadPacket(
		pRequestHeader,
		sizeof(NDAS_CMD_HEADER),
		&cbRead);

	if (!success && ::GetLastError() != ERROR_MORE_DATA) 
	{
		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"Read packet failed, error=0x%X\n", GetLastError());

		return FALSE;
	}

	// header check

	if (0 != ::memcmp(
		pRequestHeader->Protocol, 
		NDAS_CMD_PROTOCOL, 
		sizeof(NDAS_CMD_PROTOCOL)))
	{
		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"Unexpected Protocol %c%c%c%c\n",
			pRequestHeader->Protocol[0], pRequestHeader->Protocol[1],
			pRequestHeader->Protocol[2], pRequestHeader->Protocol[3]);
		return FALSE;
	}

	// version mismatch is handled from Process()

	if (cbRead != sizeof(NDAS_CMD_HEADER)) 
	{
		// header size mismatch
		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"Header size mismatch, read %d bytes, expected value %d\n",
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
	NDAS_CMD_HEADER replyHeader = {0};
	NDAS_CMD_HEADER* pReplyHeader = &replyHeader;
	DWORD cbReplyHeader = sizeof(NDAS_CMD_HEADER);
	::CopyMemory(pReplyHeader, NDAS_CMD_PROTOCOL, sizeof(NDAS_CMD_PROTOCOL));

	pReplyHeader->VersionMajor = NDAS_CMD_PROTOCOL_VERSION_MAJOR;
	pReplyHeader->VersionMinor = NDAS_CMD_PROTOCOL_VERSION_MINOR;
	pReplyHeader->Command = m_command;
	pReplyHeader->Status = opStatus;
	pReplyHeader->MessageSize = cbReplyHeader + cbDataSize;

	DWORD cbWritten;
	BOOL success = WritePacket(
		pReplyHeader, 
		cbReplyHeader,
		&cbWritten);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"WritePacket(Header) failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasCommandProcessor::ReplySimple(
	NDAS_CMD_STATUS status,
	LPCVOID lpData,
	DWORD cbData,
	LPDWORD lpcbWritten /* = NULL */)
{
	BOOL success = WriteHeader(status, cbData);
	if (!success)
	{
		return FALSE;
	}
	return WritePacket(lpData, cbData, lpcbWritten);
}

//////////////////////////////////////////////////////////////////////////
//
// WriteErrorReply
//
//////////////////////////////////////////////////////////////////////////

BOOL
CNdasCommandProcessor::ReplyError(
	NDAS_CMD_STATUS opStatus,
	DWORD dwErrorCode,
	DWORD cbDataLength,
	LPVOID lpData)
{
	BOOL success;
	NDAS_CMD_ERROR::REPLY errorReply;
	NDAS_CMD_ERROR::REPLY* pErrorReply = &errorReply;
	DWORD cbErrorReply = sizeof(NDAS_CMD_ERROR::REPLY);

	success = WriteHeader(
		opStatus,
		cbErrorReply + cbDataLength);

	if (!success) 
	{
		return FALSE;
	}

	pErrorReply->dwErrorCode = dwErrorCode;
	pErrorReply->dwDataLength = cbDataLength;

	DWORD cbWritten;

	success = WritePacket(pErrorReply, cbErrorReply, &cbWritten);
	if (!success) 
	{
		return FALSE;
	}

	if (NULL != lpData) 
	{
		success = WritePacket(lpData, cbDataLength, &cbWritten);
		if (!success) 
		{
			return FALSE;
		}
	}

	return TRUE;
}

#define NDAS_CMD_NO_EXTRA_DATA(cbExtraInData) \
	if (cbExtraInData > 0) { \
		return ReplyError(NDAS_CMD_STATUS_INVALID_REQUEST); \
	} do {} while(0)

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_REGISTER_DEVICE
//
//////////////////////////////////////////////////////////////////////////

namespace
{

bool NcpIsBadRegisterFlags(DWORD dwRegFlags)
{
	static const DWORD LEGIMATE_REG_FLAGS = 
		NDAS_DEVICE_REG_FLAG_HIDDEN | 
		NDAS_DEVICE_REG_FLAG_VOLATILE |
		NDAS_DEVICE_REG_FLAG_USE_OEM_CODE;
	return 0 != (dwRegFlags & ~LEGIMATE_REG_FLAGS);
}

struct InvalidateEncryptedNdasUnit : std::unary_function<INdasUnit*, void>
{
	void operator()(INdasUnit* pNdasUnit) const
	{
		if (NULL == pNdasUnit)
		{
			return;
		}

		NDAS_UNITDEVICE_TYPE unitType;
		COMVERIFY(pNdasUnit->get_Type(&unitType));
		if (NDAS_UNITDEVICE_TYPE_DISK != unitType)
		{
			return;
		}

		CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pNdasUnit);
		ATLASSERT(pNdasDiskUnit.p);

		NDAS_CONTENT_ENCRYPT encryption;
		COMVERIFY(pNdasDiskUnit->get_ContentEncryption(&encryption));

		if (NDAS_CONTENT_ENCRYPT_METHOD_NONE == encryption.Method)
		{
			return;
		}

		CComPtr<INdasDevice> pNdasDevice;
		COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));

		pNdasDevice->InvalidateNdasUnit(pNdasUnit);
	}
};

struct InvalidateEncryptedNdasUnits : std::unary_function<INdasDevice*, void> 
{
	void operator()(INdasDevice* pNdasDevice) const 
	{
		CInterfaceArray<INdasUnit> ndasUnits;
		pNdasDevice->get_NdasUnits(ndasUnits);

		//
		// Invalid key will not create an instance of NDAS unit device
		// So we should do this for non-instantiated unit devices also
		// This is a workaround and should be changed!
		//
		if (ndasUnits.IsEmpty())
		{
			COMVERIFY(pNdasDevice->put_Enabled(FALSE));
			COMVERIFY(pNdasDevice->put_Enabled(TRUE));
		}
		else
		{
			AtlForEach(ndasUnits, InvalidateEncryptedNdasUnit());
		}
	}
};

} // namespace

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_REGISTER_DEVICE::REQUEST>(
	NDAS_CMD_REGISTER_DEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);
	
	HRESULT hr;

	//
	// Preparing parameters
	//

	if (NcpIsBadRegisterFlags(pRequest->RegFlags))
	{
		// TODO: Create a new error entry
		return ReplyCommandFailed(ERROR_INVALID_PARAMETER);
	}

	//
	// Device Name
	//
	TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];

#ifdef UNICODE
	{
		// We should copy the string to prevent buffer overflow.
		hr = ::StringCchCopy(
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

		BOOL success = ::WideCharToMultiByte(
			CP_ACP,
			0,
			pRequest->wszDeviceName,
			cchLength,
			szDeviceName,
			MAX_NDAS_DEVICE_NAME_LEN,
			NULL,
			NULL);

		_ASSERT(success);
	}
#endif

	//
	// The actual operation should be recoverable
	//
	CComPtr<INdasDeviceRegistrar> pRegistrar;
	hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(hr);
	}

	const NDAS_OEM_CODE* ndasOemCode = NULL;
	if (NDAS_DEVICE_REG_FLAG_USE_OEM_CODE & pRequest->RegFlags)
	{
		ndasOemCode = &pRequest->OEMCode;
	}

	CComPtr<INdasDevice> pNdasDevice;

	hr = pRegistrar->Register(
		0, 
		pRequest->DeviceId, 
		pRequest->RegFlags, 
		NULL, 
		CComBSTR(szDeviceName),
		pRequest->GrantingAccess,
		ndasOemCode,
		&pNdasDevice);

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	NdasCmdReply<NDAS_CMD_REGISTER_DEVICE> reply;

	COMVERIFY(pNdasDevice->get_SlotNo(&reply.SlotNo));
	XTLASSERT(0 != reply.SlotNo);

	return ReplySuccessSimple(&reply);
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_REGISTER_DEVICE_V2::REQUEST>(
	NDAS_CMD_REGISTER_DEVICE_V2::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	HRESULT hr;

	//
	// Preparing parameters
	//

	if (NcpIsBadRegisterFlags(pRequest->RegFlags))
	{
		// TODO: Create a new error entry
		return ReplyCommandFailed(ERROR_INVALID_PARAMETER);
	}

	//
	// Device Name
	//
	TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];

#ifdef UNICODE
	{
		// We should copy the string to prevent buffer overflow.
		hr = ::StringCchCopy(
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

		BOOL success = ::WideCharToMultiByte(
			CP_ACP,
			0,
			pRequest->wszDeviceName,
			cchLength,
			szDeviceName,
			MAX_NDAS_DEVICE_NAME_LEN,
			NULL,
			NULL);

		_ASSERT(success);
	}
#endif

	//
	// The actual operation should be recoverable
	//
	CComPtr<INdasDeviceRegistrar> pRegistrar;
	hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(hr);
	}

	const NDAS_OEM_CODE* ndasOemCode = NULL;
	if (NDAS_DEVICE_REG_FLAG_USE_OEM_CODE & pRequest->RegFlags)
	{
		ndasOemCode = &pRequest->OEMCode;
	}

	CComPtr<INdasDevice> pNdasDevice;
	hr = pRegistrar->Register(
		0, 
		pRequest->DeviceId, 
		pRequest->RegFlags, 
		&pRequest->NdasIdExtension,
		CComBSTR(szDeviceName),
		pRequest->GrantingAccess,
		ndasOemCode,
		&pNdasDevice);

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	NdasCmdReply<NDAS_CMD_REGISTER_DEVICE_V2> reply;

	// If USE_OEM_CODE is set, change the OEM Code
	if (NDAS_DEVICE_REG_FLAG_USE_OEM_CODE & pRequest->RegFlags)
	{
		COMVERIFY(pNdasDevice->put_OemCode(&pRequest->OEMCode));
	}
	COMVERIFY(pNdasDevice->get_SlotNo(&reply.SlotNo));
	XTLASSERT(0 != reply.SlotNo);

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_DEVICES
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasDeviceRegistrar> pRegistrar;
	HRESULT hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(hr);
	}

	CInterfaceArray<INdasDevice> ndasDevices;
	COMVERIFY(pRegistrar->get_NdasDevices(NDAS_ENUM_EXCLUDE_HIDDEN, ndasDevices));

	NDAS_CMD_ENUMERATE_DEVICES::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_DEVICES::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD dwEntries = static_cast<DWORD>(ndasDevices.GetCount());

	pReply->nDeviceEntries = dwEntries;
	DWORD cbReplyExtra = (dwEntries * sizeof(NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY));

	DWORD cbData = cbReply + cbReplyExtra;

	BOOL success = WriteSuccessHeader(cbData);
	if (!success)
	{
		return FALSE;
	}

	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	size_t count = ndasDevices.GetCount();
	for (size_t index = 0; index < count; ++index)
	{
		NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY entry = {0};
		INdasDevice* pNdasDevice = ndasDevices.GetAt(index);

		COMVERIFY(pNdasDevice->get_NdasDeviceId(&entry.DeviceId));
		COMVERIFY(pNdasDevice->get_SlotNo(&entry.SlotNo));
		COMVERIFY(pNdasDevice->get_GrantedAccess(&entry.GrantedAccess));
		
		CComBSTR ndasDeviceName;
		COMVERIFY(pNdasDevice->get_Name(&ndasDeviceName));
		COMVERIFY(StringCchCopy(
			entry.wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN+1,
			ndasDeviceName));
		
		success = WritePacket(&entry, sizeof(entry));
		if (!success)
		{
			return FALSE;
		}
	}

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_DEVICES_V2::REQUEST>(
	NDAS_CMD_ENUMERATE_DEVICES_V2::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasDeviceRegistrar> pRegistrar;
	HRESULT hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(hr);
	}

	CInterfaceArray<INdasDevice> ndasDevices;
	COMVERIFY(pRegistrar->get_NdasDevices(NDAS_ENUM_EXCLUDE_HIDDEN, ndasDevices));

	NDAS_CMD_ENUMERATE_DEVICES_V2::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_DEVICES_V2::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD dwEntries = static_cast<DWORD>(ndasDevices.GetCount());

	pReply->nDeviceEntries = dwEntries;
	DWORD cbReplyExtra = (dwEntries * sizeof(NDAS_CMD_ENUMERATE_DEVICES_V2::ENUM_ENTRY));

	DWORD cbData = cbReply + cbReplyExtra;

	BOOL success = WriteSuccessHeader(cbData);
	if (!success)
	{
		return FALSE;
	}

	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	size_t count = ndasDevices.GetCount();
	for (size_t index = 0; index < count; ++index)
	{
		NDAS_CMD_ENUMERATE_DEVICES_V2::ENUM_ENTRY entry = {0};
		INdasDevice* pNdasDevice = ndasDevices.GetAt(index);

		COMVERIFY(pNdasDevice->get_NdasDeviceId(&entry.DeviceId));
		COMVERIFY(pNdasDevice->get_SlotNo(&entry.SlotNo));
		COMVERIFY(pNdasDevice->get_GrantedAccess(&entry.GrantedAccess));
		COMVERIFY(pNdasDevice->get_NdasIdExtension(&entry.NdasIdExtension));

		CComBSTR ndasDeviceName;
		COMVERIFY(pNdasDevice->get_Name(&ndasDeviceName));
		COMVERIFY(StringCchCopy(
			entry.wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN+1,
			ndasDeviceName));
		
		success = WritePacket(&entry, sizeof(entry));
		if (!success)
		{
			return FALSE;
		}
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	HRESULT hr;

	CComPtr<INdasDeviceRegistrar> pRegistrar;
	COMVERIFY(hr = pGetNdasDeviceRegistrar(&pRegistrar));
	if (FAILED(hr))
	{
		return ReplyCommandFailed(hr);
	}

	if (pRequest->DeviceIdOrSlot.UseSlotNo) 
	{
		CComPtr<INdasDevice> pNdasDevice;
		hr = pRegistrar->get_NdasDevice(pRequest->DeviceIdOrSlot.SlotNo, &pNdasDevice);
		if (SUCCEEDED(hr))
		{
			hr = pRegistrar->Deregister(pNdasDevice);
		}
	}
	else 
	{
		CComPtr<INdasDevice> pNdasDevice;
		hr = pRegistrar->get_NdasDevice(&pRequest->DeviceIdOrSlot.DeviceId, &pNdasDevice);
		if (SUCCEEDED(hr))
		{
			hr = pRegistrar->Deregister(pNdasDevice);
		}
	}

	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(hr); 
	}

	return ReplySuccessDummy();
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);
	
	CComPtr<INdasDevice> pNdasDevice;	
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_INFORMATION> reply;
	NDAS_CMD_QUERY_DEVICE_INFORMATION::REPLY* pReply = &reply;

	COMVERIFY(pNdasDevice->get_HardwareInfo(&pReply->HardwareInfo));
	COMVERIFY(pNdasDevice->get_NdasDeviceId(&pReply->DeviceId));
	COMVERIFY(pNdasDevice->get_SlotNo(&pReply->SlotNo));
	COMVERIFY(pNdasDevice->get_GrantedAccess(&pReply->GrantedAccess));

	COMVERIFY(pNdasDevice->get_RegisterFlags(&pReply->DeviceParams.RegFlags));

	CComBSTR ndasDeviceName;
	COMVERIFY(pNdasDevice->get_Name(&ndasDeviceName));
	COMVERIFY(StringCchCopyW(
		pReply->wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN+1,
		ndasDeviceName));

	return ReplySuccessSimple(&reply);
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_DEVICE_INFORMATION_V2::REQUEST>(
	NDAS_CMD_QUERY_DEVICE_INFORMATION_V2::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasDevice> pNdasDevice;	
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_INFORMATION_V2> reply;
	NDAS_CMD_QUERY_DEVICE_INFORMATION_V2::REPLY* pReply = &reply;

	COMVERIFY(pNdasDevice->get_HardwareInfo(&pReply->HardwareInfo));
	COMVERIFY(pNdasDevice->get_NdasDeviceId(&pReply->DeviceId));
	COMVERIFY(pNdasDevice->get_SlotNo(&pReply->SlotNo));
	COMVERIFY(pNdasDevice->get_GrantedAccess(&pReply->GrantedAccess));
	COMVERIFY(pNdasDevice->get_NdasIdExtension(&pReply->NdasIdExtension));

	COMVERIFY(pNdasDevice->get_RegisterFlags(&pReply->DeviceParams.RegFlags));

	CComBSTR ndasDeviceName;
	COMVERIFY(pNdasDevice->get_Name(&ndasDeviceName));
	COMVERIFY(StringCchCopyW(
		pReply->wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN+1,
		ndasDeviceName));

	return ReplySuccessSimple(&reply);
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasDevice> pNdasDevice;	
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_STATUS> reply;

	COMVERIFY(pNdasDevice->get_Status(&reply.Status));
	COMVERIFY(pNdasDevice->get_DeviceError(&reply.LastError));

	return ReplySuccessSimple(&reply);
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasDevice> pNdasDevice;	
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_CMD_ENUMERATE_UNITDEVICES::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_UNITDEVICES::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	//
	// unit device numbers are not contiguous (by definition only)
	//

	CInterfaceArray<INdasUnit> pNdasUnits;
	pNdasDevice->get_NdasUnits(pNdasUnits);

	size_t count = pNdasUnits.GetCount();

	DWORD cbReplyExtra = count * sizeof(NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY);
	DWORD cbData = cbReply + cbReplyExtra;

	BOOL success = WriteSuccessHeader(cbData);
	if (!success)
	{
		return FALSE;
	}

	pReply->nUnitDeviceEntries = count;
	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	for (size_t index = 0; index < count; ++index)
	{
		CComPtr<INdasUnit> pNdasUnit = pNdasUnits.GetAt(index);

		NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY entry;
		COMVERIFY(pNdasUnit->get_UnitNo(&entry.UnitNo));
		COMVERIFY(pNdasUnit->get_Type(&entry.UnitDeviceType));

		success = WritePacket(&entry, sizeof(entry));
		if (!success)
		{
			return FALSE;
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
	CComPtr<INdasUnit> pNdasUnit;

	HRESULT hr = pGetNdasUnit(
		pRequest->DeviceIdOrSlot, 
		pRequest->UnitNo, 
		&pNdasUnit);

	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_INFORMATION> reply;

	pNdasUnit->get_Type(&reply.UnitDeviceType);
	pNdasUnit->get_SubType(&reply.UnitDeviceSubType);
	reply.HardwareInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
	pNdasUnit->get_HardwareInfo(&reply.HardwareInfo);
	pNdasUnit->get_UnitStat(&reply.Stats);

	return ReplySuccessSimple(&reply);
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasUnit> pNdasUnit;

	HRESULT hr = pGetNdasUnit(
		pRequest->DeviceIdOrSlot, 
		pRequest->UnitNo, 
		&pNdasUnit);

	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_STATUS> reply;

	COMVERIFY(pNdasUnit->get_Status(&reply.Status));
	COMVERIFY(pNdasUnit->get_Error(&reply.LastError));

	return ReplySuccessSimple(&reply);

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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasDevice> pNdasDevice;	
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_DEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	hr = S_OK;

	switch (pParam->ParamType) 
	{
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_ENABLE:
		hr = pNdasDevice->put_Enabled(pParam->bEnable);
		break;
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME:
		hr = pNdasDevice->put_Name(CComBSTR(pParam->wszName));
		break;
	case NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS:
		hr = pNdasDevice->put_GrantedAccess(pParam->GrantingAccess);
		break;
	}

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	return ReplySuccessDummy();
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasUnit> pNdasUnit;

	HRESULT hr = pGetNdasUnit(
		pRequest->DeviceIdOrSlot, 
		pRequest->dwUnitNo, 
		&pNdasUnit);

	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	PNDAS_CMD_SET_UNITDEVICE_PARAM_DATA pParam = &pRequest->Param;

	return ReplyCommandFailed(NDASSVC_ERROR_NOT_IMPLEMENTED);
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasUnit> pNdasUnit;

	HRESULT hr = pGetNdasUnit(
		pRequest->DeviceIdOrSlot, 
		pRequest->UnitNo, 
		&pNdasUnit);

	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT> reply;

	hr = pNdasUnit->GetActualHostUsageCount(
		&reply.nROHosts,
		&reply.nRWHosts,
		TRUE);

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	return ReplySuccessSimple(&reply);
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA pParam = &pRequest->Param;

	return ReplyCommandFailed(NDASSVC_ERROR_NOT_IMPLEMENTED);
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasUnit> pNdasUnit;

	HRESULT hr = pGetNdasUnit(
		pRequest->DeviceIdOrSlot, 
		pRequest->UnitNo, 
		&pNdasUnit);

	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NDAS_UNITDEVICE_ID ndasUnitId;
	pNdasUnit->get_NdasUnitId(&ndasUnitId);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	hr = pGetNdasLogicalUnit(ndasUnitId, &pNdasLogicalUnit);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}
	ATLASSERT(pNdasLogicalUnit.p);

	NdasCmdReply<NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE> reply;

	COMVERIFY(pNdasLogicalUnit->get_Id(&reply.LogicalDeviceId));

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_LOGICALDEVICES
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	HRESULT hr;

	// only non-hidden logical devices
	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));

	CInterfaceArray<INdasLogicalUnit> ndasLogicalUnits;
	COMVERIFY(pManager->get_NdasLogicalUnits(NDAS_ENUM_EXCLUDE_HIDDEN, ndasLogicalUnits));

	size_t ndasLogicalUnitCount = ndasLogicalUnits.GetCount();

	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD entryCount = static_cast<DWORD>(ndasLogicalUnitCount);

	DWORD cbReplyExtra = entryCount * sizeof(NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY);
	DWORD cbData = cbReply + cbReplyExtra;

	BOOL success = WriteSuccessHeader(cbData);
	if (!success)
	{
		return FALSE;
	}

	pReply->nLogicalDeviceEntries = entryCount;
	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	for (size_t index = 0; index < ndasLogicalUnitCount; ++index)
	{
		INdasLogicalUnit* pNdasLogicalUnit = ndasLogicalUnits.GetAt(index);
		XTLASSERT(pNdasLogicalUnit);

		NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY entry;
		COMVERIFY(pNdasLogicalUnit->get_Id(&entry.LogicalDeviceId));
		COMVERIFY(pNdasLogicalUnit->get_Type(&entry.LogicalDeviceType));

		success = WritePacket(&entry, sizeof(entry));
		if (!success)
		{
			return FALSE;
		}
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
	if (sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM) != pRequest->LpiParam.Size)
	{
		return ReplyCommandFailed(NDASUSER_ERROR_INVALID_PARAMETER);
	}

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LpiParam.LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	ACCESS_MASK requestingAccess = pRequest->LpiParam.Access;
	ACCESS_MASK allowedAccess;
	COMVERIFY(pNdasLogicalUnit->get_GrantedAccess(&allowedAccess));
	if ((requestingAccess & allowedAccess) != requestingAccess) 
	{
		return ReplyCommandFailed(NDASUSER_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED); 
	}

	hr = pNdasLogicalUnit->PlugIn(
		requestingAccess,
		pRequest->LpiParam.LdpfFlags,
		pRequest->LpiParam.LdpfValues);

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	return ReplySuccessDummy();
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	hr = pNdasLogicalUnit->Eject();

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	return ReplySuccessDummy();
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_EJECT_LOGICALDEVICE_2::REQUEST>(
	NDAS_CMD_EJECT_LOGICALDEVICE_2::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_EJECT_LOGICALDEVICE_2> reply;

	CComBSTR vetoName;
	
	hr = pNdasLogicalUnit->EjectEx(
		&reply.ConfigRet, 
		&reply.VetoType, 
		&vetoName);

	COMVERIFY(StringCchCopyW(reply.VetoName, MAX_PATH, vetoName));

	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	return ReplySuccessSimple(&reply);
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
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	hr = pNdasLogicalUnit->Unplug();
	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	return ReplySuccessDummy();
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REPLY reply = {0};
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD nEntries;
	COMVERIFY(pNdasLogicalUnit->get_NdasUnitCount(&nEntries));

	COMVERIFY(pNdasLogicalUnit->get_Type(&pReply->LogicalDeviceType));
	COMVERIFY(pNdasLogicalUnit->get_GrantedAccess(&pReply->GrantedAccess));
	COMVERIFY(pNdasLogicalUnit->get_MountedAccess(&pReply->MountedAccess));
	pReply->nUnitDeviceEntries = nEntries;

	// Logical Device Params

	// deprecated: CurrentMaxRequestBlocks
	pReply->LogicalDeviceParams.CurrentMaxRequestBlocks = 0; 

	NDAS_LOGICALDEVICE_TYPE logicalUnitType;
	COMVERIFY(pNdasLogicalUnit->get_Type(&logicalUnitType));

	switch (logicalUnitType) 
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:		
		{
			UINT64 blocks;
			COMVERIFY(pNdasLogicalUnit->get_UserBlocks(&blocks));

			pReply->LogicalDiskInfo.Blocks = blocks;
			
			NDAS_CONTENT_ENCRYPT encryption;
			COMVERIFY(pNdasLogicalUnit->get_ContentEncryption(&encryption));

			if (NDAS_CONTENT_ENCRYPT_METHOD_NONE == encryption.Method)
			{
				break;
			}

			//
			// For now, we cannot retrieve Revision information from NDAS_CONTENT_ENCRYPT.
			// However, there is only one NDAS_CONTENT_ENCRYPT_REVISION if instantiated.
			//
			pReply->LogicalDiskInfo.ContentEncrypt.Revision = NDAS_CONTENT_ENCRYPT_REVISION;
			pReply->LogicalDiskInfo.ContentEncrypt.Type = encryption.Method;
			pReply->LogicalDiskInfo.ContentEncrypt.KeyLength = encryption.KeyLength;
			break;
		}
	case NDAS_LOGICALDEVICE_TYPE_DVD:
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
	case NDAS_LOGICALDEVICE_TYPE_MO:
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
		break;
	default:
		{
			return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
			break;
		}
	}

	DWORD cbReplyExtra = 
		sizeof(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::UNITDEVICE_ENTRY) * nEntries;
	DWORD cbData = cbReply + cbReplyExtra;

	BOOL success = WriteSuccessHeader(cbData);
	if (!success)
	{
		return FALSE;
	}

	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < nEntries; ++i) 
	{
		NDAS_UNITDEVICE_ID entry;
		COMVERIFY(pNdasLogicalUnit->get_NdasUnitId(i, &entry));
		NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::UNITDEVICE_ENTRY replyEntry;
		replyEntry.DeviceId = entry.DeviceId;
		replyEntry.UnitNo = entry.UnitNo;
		success = WritePacket(&replyEntry,sizeof(replyEntry));
		if (!success)
		{
			return FALSE;
		}
	}

	return TRUE;
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REQUEST>(
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REPLY reply = {0};
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD nEntries;
	COMVERIFY(pNdasLogicalUnit->get_NdasUnitCount(&nEntries));

	COMVERIFY(pNdasLogicalUnit->get_Type(&pReply->LogicalDeviceType));
	COMVERIFY(pNdasLogicalUnit->get_GrantedAccess(&pReply->GrantedAccess));
	COMVERIFY(pNdasLogicalUnit->get_MountedAccess(&pReply->MountedAccess));
	pReply->nUnitDeviceEntries = nEntries;

	// Logical Device Params
	// deprecated: CurrentMaxRequestBlocks
	pReply->LogicalDeviceParams.CurrentMaxRequestBlocks = 0; 

	COMVERIFY(pNdasLogicalUnit->get_MountedDriveSet(&pReply->LogicalDeviceParams.MountedLogicalDrives));

	NDAS_LOGICALDEVICE_TYPE logicalUnitType;
	COMVERIFY(pNdasLogicalUnit->get_Type(&logicalUnitType));

	switch (logicalUnitType) 
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:		
		{
			UINT64 blocks = 0;
			COMVERIFY(pNdasLogicalUnit->get_UserBlocks(&blocks));
//			XTLASSERT(blocks <= (UINT64)((DWORD)(-1)));
//			pReply->LogicalDiskInfo.Blocks = static_cast<DWORD>(blocks);
			pReply->LogicalDiskInfo.Blocks = blocks;

			NDAS_CONTENT_ENCRYPT encryption;
			COMVERIFY(pNdasLogicalUnit->get_ContentEncryption(&encryption));

			if (NDAS_CONTENT_ENCRYPT_METHOD_NONE == encryption.Method)
			{
				break;
			}

			//
			// For now, we cannot retrieve Revision information from NDAS_CONTENT_ENCRYPT.
			// However, there is only one NDAS_CONTENT_ENCRYPT_REVISION if instantiated.
			//
			pReply->LogicalDiskInfo.ContentEncrypt.Revision = NDAS_CONTENT_ENCRYPT_REVISION;
			pReply->LogicalDiskInfo.ContentEncrypt.Type = encryption.Method;
			pReply->LogicalDiskInfo.ContentEncrypt.KeyLength = encryption.KeyLength;
			break;
		}
	case NDAS_LOGICALDEVICE_TYPE_DVD:
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
	case NDAS_LOGICALDEVICE_TYPE_MO:
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
		break;
	default:
		{
			return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
			break;
		}
	}

	DWORD cbReplyExtra = 
		sizeof(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::UNITDEVICE_ENTRY) * nEntries;
	DWORD cbData = cbReply + cbReplyExtra;

	BOOL success = WriteSuccessHeader(cbData);
	if (!success)
	{
		return FALSE;
	}

	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < nEntries; ++i) 
	{
		NDAS_UNITDEVICE_ID entry;
		COMVERIFY(pNdasLogicalUnit->get_NdasUnitId(i, &entry));
		NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::UNITDEVICE_ENTRY replyEntry;
		replyEntry.DeviceId = entry.DeviceId;
		replyEntry.UnitNo = entry.UnitNo;

		{
			CComPtr<INdasDevice> pRegisteredDevice;
			hr = pGetNdasDevice(entry.DeviceId, &pRegisteredDevice);
			if (FAILED(hr))
			{
				// If not registered, we don't know extension. Just give default extension.
				replyEntry.NdasIdExtension = NDAS_ID_EXTENSION_DEFAULT;
			}
			else
			{
				pRegisteredDevice->get_NdasIdExtension(&replyEntry.NdasIdExtension);
			}
		}

		success = WritePacket(&replyEntry,sizeof(replyEntry));
		if (!success)
		{
			return FALSE;
		}
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
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_LOGICALDEVICE_STATUS> reply;

	COMVERIFY(pNdasLogicalUnit->get_Status(&reply.Status));
	COMVERIFY(pNdasLogicalUnit->get_Error(&reply.LastError));

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_NDAS_LOCATION
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_NDAS_LOCATION::REQUEST>(
	NDAS_CMD_QUERY_NDAS_LOCATION::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnit(pRequest->LogicalDeviceId, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_NDAS_LOCATION> reply;

	COMVERIFY(pNdasLogicalUnit->get_NdasLocation(&reply.NdasLocation));

	return ReplySuccessSimple(&reply);
}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_NDAS_LOGICALDEVICE_ID::REQUEST>(
		NDAS_CMD_QUERY_NDAS_LOGICALDEVICE_ID::REQUEST* pRequest, 
		DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	HRESULT hr = pGetNdasLogicalUnitByNdasLocation(pRequest->NdasLocation, &pNdasLogicalUnit);

	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_NDAS_LOGICALDEVICE_ID> reply;

	COMVERIFY(pNdasLogicalUnit->get_Id(&reply.LogicalDeviceId));

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_HOST_INFO
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_HOST_INFO::REQUEST>(
	NDAS_CMD_QUERY_HOST_INFO::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	const NDAS_HOST_INFO* pHostInfo = pHostInfoCache->GetHostInfo(&pRequest->HostGuid);

	if (NULL == pHostInfo) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_HOST_INFO> reply;
	reply.HostInfo = *pHostInfo;

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_HOST_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////


template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REQUEST>(
	NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);
	return ReplyCommandFailed(NDASSVC_ERROR_NOT_IMPLEMENTED);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_HOST_UNITDEVICE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_HOST_UNITDEVICE::REQUEST>(
	NDAS_CMD_QUERY_HOST_UNITDEVICE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pGetNdasUnit(pRequest->DeviceIdOrSlot, pRequest->UnitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	BOOL success = hixdisc.Initialize();
	DWORD nROHosts, nRWHosts;

	hr = pNdasUnit->GetHostUsageCount(&nROHosts,&nRWHosts,TRUE);
	if (FAILED(hr)) 
	{
		return ReplyCommandFailed(hr);
	}

	DWORD nHosts = (NDAS_HOST_COUNT_UNKNOWN == nROHosts) ? 
		NDAS_MAX_CONNECTION_V11 : nROHosts + nRWHosts;

	XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_INFORMATION,
		"Expected: nHosts %d, nRoHost %d, nRwHost %d\n",
		nHosts, nROHosts, nRWHosts);

	//
	// discover only for all hosts
	// NHIX_UDA_READ_ACCESS means to discover 
	// for hosts using READ_ACCESS, which includes RW hosts
	//

	NDAS_HIX_UDA uda = NHIX_UDA_READ_ACCESS; 
	NDAS_UNITDEVICE_ID unitDeviceId;
	pNdasUnit->get_NdasUnitId(&unitDeviceId);

	if (nHosts > 0) 
	{
		success = hixdisc.Discover(unitDeviceId,uda,nHosts,1000);
		if (!success) 
		{
			return ReplyCommandFailed(::GetLastError());
		}

		nHosts = hixdisc.GetHostCount(unitDeviceId);
	}

	NDAS_CMD_QUERY_HOST_UNITDEVICE::REPLY reply = {0};
	NDAS_CMD_QUERY_HOST_UNITDEVICE::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;
	DWORD cbReplyExtra = nHosts * sizeof(NDAS_CMD_QUERY_HOST_ENTRY);

	success = WriteHeader(NDAS_CMD_STATUS_SUCCESS, cbReply + cbReplyExtra);
	if (!success)
	{
		return FALSE;
	}

	pReply->EntryCount = nHosts;

	success = WritePacket(pReply, cbReply);
	if (!success)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < nHosts; ++i) 
	{
		NDAS_HIX_UDA hostUda = 0;
		NDAS_CMD_QUERY_HOST_ENTRY hostEntry = {0};

		success = hixdisc.GetHostData(
			unitDeviceId,
			i,
			&hostUda,
			&hostEntry.HostGuid,
			NULL,
			NULL);

		if (!success) 
		{
			// warning!!!
		}

		if (FlagIsSet(hostUda, NHIX_UDA_BIT_READ)) 
		{
			hostEntry.Access |= GENERIC_READ;
		}
		if (FlagIsSet(hostUda, NHIX_UDA_BIT_WRITE)) 
		{
			hostEntry.Access |= GENERIC_WRITE;
		}
		if (FlagIsSet(hostUda, NHIX_UDA_BIT_SHARED_RW))
		{
			hostEntry.Access |= NDAS_ACCESS_BIT_EXTENDED;
			hostEntry.Access |= NDAS_ACCESS_BIT_SHARED_WRITE;
			if (FlagIsSet(hostUda, NHIX_UDA_BIT_SHARED_RW_PRIMARY))
			{
				hostEntry.Access |= NDAS_ACCESS_BIT_SHARED_WRITE_PRIMARY;
			}
		}

		success = WritePacket(&hostEntry, sizeof(NDAS_CMD_QUERY_HOST_ENTRY));
		if (!success)
		{
			return FALSE;
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_REQUEST_SURRENDER_ACCESS
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_REQUEST_SURRENDER_ACCESS::REQUEST>(
	NDAS_CMD_REQUEST_SURRENDER_ACCESS::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NHIX_UDA nhixUDA = 0;
	if (pRequest->Access & GENERIC_READ) nhixUDA |= NHIX_UDA_READ_ACCESS;
	if (pRequest->Access & GENERIC_WRITE) nhixUDA |= NHIX_UDA_WRITE_ACCESS;

	if (0 == nhixUDA) 
	{
		return ReplyError(NDAS_CMD_STATUS_INVALID_REQUEST);
	}

	NDAS_UNITDEVICE_ID unitDeviceId;

	{
		CComPtr<INdasUnit> pNdasUnit;

		HRESULT hr = pGetNdasUnit(
			pRequest->DeviceIdOrSlot, 
			pRequest->UnitNo, 
			&pNdasUnit);

		if (FAILED(hr)) 
		{ 
			return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
		}

		pNdasUnit->get_NdasUnitId(&unitDeviceId);
	}

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	SOCKADDR_LPX boundAddr, remoteAddr;
	BOOL success = pHostInfoCache->GetHostNetworkInfo(
		&pRequest->HostGuid,
		&boundAddr,
		&remoteAddr);

	if (!success) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
	}

	CNdasHIXSurrenderAccessRequest sar(pGetNdasHostGuid());
	success = sar.Initialize();
	if (!success) 
	{
		return ReplyCommandFailed(::GetLastError());
	}

	success = sar.Request(
		&boundAddr, 
		&remoteAddr, 
		unitDeviceId, 
		nhixUDA, 
		2000);

	if (!success) 
	{
		// TODO: Last Error may not be correct.
		return ReplyCommandFailed(::GetLastError());
	}

	return ReplySuccessDummy();
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE::REQUEST>(
	NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	CComPtr<INdasDevice> pNdasDevice;
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr)) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
	}

	CComPtr<INdasUnit> pNdasUnit;
	hr = pNdasDevice->get_NdasUnit(pRequest->UnitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NDAS_DEVICE_STATUS status;
	COMVERIFY(pNdasDevice->get_Status(&status));

	if (NDAS_DEVICE_STATUS_DISABLED == status) 
	{
		// do nothing for disabled device
	}
	else 
	{
		COMVERIFY(pNdasDevice->put_Enabled(FALSE));
		COMVERIFY(pNdasDevice->put_Enabled(TRUE));
	}

	return ReplySuccessDummy();
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_GET_SERVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_GET_SERVICE_PARAM::REQUEST>(
	NDAS_CMD_GET_SERVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	if (NDASSVC_PARAM_SUSPEND_PROCESS == pRequest->ParamCode) 
	{
		NdasCmdReply<NDAS_CMD_GET_SERVICE_PARAM> reply;

		DWORD dwValue = NdasServiceConfig::Get(nscSuspendOptions);

		reply.Param.ParamCode = NDASSVC_PARAM_SUSPEND_PROCESS;
		reply.Param.Value.DwordValue = dwValue;

		return ReplySuccessSimple(&reply);
	}

	// all others are not supported
	return ReplyCommandFailed(NDASSVC_ERROR_INVALID_SERVICE_PARAMETER);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_SET_SERVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_SET_SERVICE_PARAM::REQUEST>(
	NDAS_CMD_SET_SERVICE_PARAM::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	if (NDASSVC_PARAM_SUSPEND_PROCESS == pRequest->Param.ParamCode) 
	{
		if (NDASSVC_SUSPEND_DENY == pRequest->Param.Value.DwordValue ||
			NDASSVC_SUSPEND_ALLOW == pRequest->Param.Value.DwordValue ) 
		{
			NdasServiceConfig::Set(nscSuspendOptions, pRequest->Param.Value.DwordValue);
			return ReplySuccessDummy();
		} 
	}
	else if (NDASSVC_PARAM_RESET_SYSKEY == pRequest->Param.ParamCode) 
	{
		CComPtr<INdasDeviceRegistrar> pRegistrar;
		HRESULT hr = pGetNdasDeviceRegistrar(&pRegistrar);

		CInterfaceArray<INdasDevice> ndasDevices;
		COMVERIFY(pRegistrar->get_NdasDevices(NDAS_ENUM_DEFAULT, ndasDevices));

		size_t count = ndasDevices.GetCount();
		for (size_t index = 0; index < count; ++index)
		{
			INdasDevice* pNdasDevice = ndasDevices.GetAt(index);
			InvalidateEncryptedNdasUnits()(pNdasDevice);
		}

		return ReplySuccessDummy();
	} 

	// all others are not supported
	return ReplyCommandFailed(NDASSVC_ERROR_INVALID_SERVICE_PARAMETER);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_DEVICE_STAT
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_DEVICE_STAT::REQUEST>(
	NDAS_CMD_QUERY_DEVICE_STAT::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	CComPtr<INdasDevice> pNdasDevice;	
	HRESULT hr = pGetNdasDevice(pRequest->DeviceIdOrSlot, &pNdasDevice);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_STAT> reply;

	pNdasDevice->get_DeviceStat(&reply.DeviceStat);

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_UNITDEVICE_STAT
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_QUERY_UNITDEVICE_STAT::REQUEST>(
	NDAS_CMD_QUERY_UNITDEVICE_STAT::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pGetNdasUnit(pRequest->DeviceIdOrSlot, pRequest->UnitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_STAT> reply;

	pNdasUnit->get_UnitStat(&reply.UnitDeviceStat);

	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_NOTIFY_DEVICE_CHANGE
//
//////////////////////////////////////////////////////////////////////////

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_NOTIFY_DEVICE_CHANGE::REQUEST>(
	NDAS_CMD_NOTIFY_DEVICE_CHANGE::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	// nothing now
	return ReplySuccessDummy();
}

//////////////////////////////////////////////////////////////////////////
//
// Process
//
//////////////////////////////////////////////////////////////////////////

BOOL 
CNdasCommandProcessor::Process()
{
	BOOL success(FALSE);
	
	success = ReadHeader();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"ReadHeader failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	// handling version mismatch
	if (m_requestHeader.VersionMajor != NDAS_CMD_PROTOCOL_VERSION_MAJOR ||
		m_requestHeader.VersionMinor != NDAS_CMD_PROTOCOL_VERSION_MINOR)
	{
		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"HPVM:Handling Protocol Version Mismatch!\n");

		// we should read the remaining packet!
		DWORD cbToRead = m_requestHeader.MessageSize - sizeof(NDAS_CMD_HEADER);
		DWORD cbRead(0);
		LPBYTE lpBuffer = new BYTE[cbToRead];
		success = ReadPacket(lpBuffer, cbToRead, &cbRead);
		delete [] lpBuffer; // discard the buffer!
		if (!success) 
		{
			XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
				"HPVM:Reading remaining packet failed, error=0x%X\n", GetLastError());
			XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
				"HPVM:Fin\n");
			return FALSE;
		}

		success = WriteHeader(NDAS_CMD_STATUS_UNSUPPORTED_VERSION, 0);
		if (!success) 
		{
			XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
				"HPVM:Writing Reply Header failed, error=0x%X\n", GetLastError());
			XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
				"HPVM:Fin\n");
			return FALSE;
		}

		XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
			"HPVM:Fin\n");

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
			BOOL success = ReadPacket(pRequest, cbRequest, &cbRead); \
			if (!success || cbRead != cbRequest) { \
				return FALSE; \
			} \
			return ProcessCommandRequest(pRequest, cbData); \
		}

	switch (m_command) {
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_REGISTER_DEVICE,NDAS_CMD_REGISTER_DEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_REGISTER_DEVICE_V2,NDAS_CMD_REGISTER_DEVICE_V2)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_UNREGISTER_DEVICE,NDAS_CMD_UNREGISTER_DEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_DEVICES,NDAS_CMD_ENUMERATE_DEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_DEVICES_V2,NDAS_CMD_ENUMERATE_DEVICES_V2)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_DEVICE_PARAM,NDAS_CMD_SET_DEVICE_PARAM)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_DEVICE_STATUS,NDAS_CMD_QUERY_DEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION,NDAS_CMD_QUERY_DEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION_V2,NDAS_CMD_QUERY_DEVICE_INFORMATION_V2)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_DEVICE_STAT,NDAS_CMD_QUERY_DEVICE_STAT)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES,NDAS_CMD_ENUMERATE_UNITDEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM,NDAS_CMD_SET_UNITDEVICE_PARAM)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_STATUS,NDAS_CMD_QUERY_UNITDEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_INFORMATION,NDAS_CMD_QUERY_UNITDEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT,NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_UNITDEVICE_STAT,NDAS_CMD_QUERY_UNITDEVICE_STAT)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE,NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES,NDAS_CMD_ENUMERATE_LOGICALDEVICES)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM,NDAS_CMD_SET_LOGICALDEVICE_PARAM)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS,NDAS_CMD_QUERY_LOGICALDEVICE_STATUS)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION,NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION_V2,NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE,NDAS_CMD_PLUGIN_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_EJECT_LOGICALDEVICE,NDAS_CMD_EJECT_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE,NDAS_CMD_UNPLUG_LOGICALDEVICE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE,NDAS_CMD_QUERY_HOST_UNITDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_HOST_LOGICALDEVICE,NDAS_CMD_QUERY_HOST_LOGICALDEVICE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_HOST_INFO,NDAS_CMD_QUERY_HOST_INFO)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS,NDAS_CMD_REQUEST_SURRENDER_ACCESS)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_NOTIFY_DEVICE_CHANGE,NDAS_CMD_NOTIFY_DEVICE_CHANGE)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE,NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_SET_SERVICE_PARAM,NDAS_CMD_SET_SERVICE_PARAM)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_GET_SERVICE_PARAM,NDAS_CMD_GET_SERVICE_PARAM)

		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_EJECT_LOGICALDEVICE_2,NDAS_CMD_EJECT_LOGICALDEVICE_2)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_NDAS_LOCATION,NDAS_CMD_QUERY_NDAS_LOCATION)
		CASE_PROCESS_COMMAND(NDAS_CMD_TYPE_QUERY_NDAS_LOGICALDEVICE_ID,NDAS_CMD_QUERY_NDAS_LOGICALDEVICE_ID)
	}

#undef CASE_PROCESS_COMMAND

	return FALSE;
}
