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

#include "ndascmdproc.h"
#include "syncobj.h"
#include "ndasdevreg.h"
#include "ndashostinfocache.h"
#include "ndasobjs.h"
#include "ndashixcli.h"
#include "ndascfg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndascmdproc.tmh"
#endif

using ximeta::CAutoLock;

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

//////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////

CNdasCommandProcessor::CNdasCommandProcessor(
	CNdasService& service,
	ITransport* pTransport) :
	m_service(service),
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

	BOOL fSuccess = ReadPacket(
		pRequestHeader,
		sizeof(NDAS_CMD_HEADER),
		&cbRead);

	if (!fSuccess && ::GetLastError() != ERROR_MORE_DATA) 
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
	BOOL fSuccess = WritePacket(
		pReplyHeader, 
		cbReplyHeader,
		&cbWritten);

	if (!fSuccess) 
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
	BOOL fSuccess = WriteHeader(status, cbData);
	if (!fSuccess)
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
	BOOL fSuccess;
	NDAS_CMD_ERROR::REPLY errorReply;
	NDAS_CMD_ERROR::REPLY* pErrorReply = &errorReply;
	DWORD cbErrorReply = sizeof(NDAS_CMD_ERROR::REPLY);

	fSuccess = WriteHeader(
		opStatus,
		cbErrorReply + cbDataLength);

	if (!fSuccess) 
	{
		return FALSE;
	}

	pErrorReply->dwErrorCode = dwErrorCode;
	pErrorReply->dwDataLength = cbDataLength;

	DWORD cbWritten;

	fSuccess = WritePacket(pErrorReply, cbErrorReply, &cbWritten);
	if (!fSuccess) 
	{
		return FALSE;
	}

	if (NULL != lpData) 
	{
		fSuccess = WritePacket(lpData, cbDataLength, &cbWritten);
		if (!fSuccess) 
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

struct InvalidateEncryptedUnitDevice : std::unary_function<CNdasDevicePtr, void> {
	void operator()(const CNdasDevicePtr& pDevice) const {

		CNdasDevice::InstanceAutoLock autolock(pDevice);

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
			CNdasUnitDevicePtr pUnitDevice = pDevice->GetUnitDevice(i);
			if (CNdasUnitDeviceNullPtr != pUnitDevice)
			{
				if (NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType())
				{
					CNdasUnitDiskDevice* pUnitDiskDevice =
						reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice.get());
					if (NDAS_CONTENT_ENCRYPT_METHOD_NONE != 
						pUnitDiskDevice->GetEncryption().Method)
					{
						(VOID) pDevice->InvalidateUnitDevice(i);
					}
				}
			}
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
	CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

	CNdasDevicePtr pDevice = registrar.Register(
		0, pRequest->DeviceId, pRequest->RegFlags, NULL);

	if (0 == pDevice.get()) 
	{
		return ReplyCommandFailed(::GetLastError());
	}

	NdasCmdReply<NDAS_CMD_REGISTER_DEVICE> reply;

	{
		CNdasDevice::InstanceAutoLock autoDeviceLock(pDevice);
		pDevice->SetGrantedAccess(pRequest->GrantingAccess);
		pDevice->SetName(szDeviceName);
		// If USE_OEM_CODE is set, change the OEM Code
		if (NDAS_DEVICE_REG_FLAG_USE_OEM_CODE & pRequest->RegFlags)
		{
			pDevice->SetOemCode(pRequest->OEMCode);
		}
		reply.SlotNo = pDevice->GetSlotNo();
		XTLASSERT(0 != reply.SlotNo);
	}
	
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
	CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

	CNdasDevicePtr pDevice = registrar.Register(
		0, pRequest->DeviceId, pRequest->RegFlags, &pRequest->NdasIdExtension);

	if (0 == pDevice.get()) 
	{
		return ReplyCommandFailed(::GetLastError());
	}

	NdasCmdReply<NDAS_CMD_REGISTER_DEVICE_V2> reply;

	{
		CNdasDevice::InstanceAutoLock autoDeviceLock(pDevice);
		pDevice->SetGrantedAccess(pRequest->GrantingAccess);
		pDevice->SetName(szDeviceName);
		// If USE_OEM_CODE is set, change the OEM Code
		if (NDAS_DEVICE_REG_FLAG_USE_OEM_CODE & pRequest->RegFlags)
		{
			pDevice->SetOemCode(pRequest->OEMCode);
		}
		reply.SlotNo = pDevice->GetSlotNo();
		XTLASSERT(0 != reply.SlotNo);
	}
	
	return ReplySuccessSimple(&reply);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_ENUMERATE_DEVICES
//
//////////////////////////////////////////////////////////////////////////

//namespace
//{
//	struct CopyNonHiddenNdasDevice
//	{
//		CNdasDeviceCollection& coll_;
//
//		CopyNonHiddenNdasDevice(CNdasDeviceCollection& coll) : 
//			coll_(coll)
//		{
//		}
//
//		void operator ()(CNdasDevicePtr pDevice)
//		{
//			XTLASSERT(NULL != pDevice);
//			if (NULL == pDevice) return;
//
//			// Do not add hidden device
//			if (pDevice->IsHidden()) return;
//
//			// Add a non-hidden device to the collection 
//			coll_.push_back(pDevice);
//		}
//	};
//
//	struct HiddenNdasDevice : 
//		std::unary_function<CNdasDevicePtr, bool>
//	{
//		bool operator()(CNdasDevicePtr pDevice)
//		{
//			return pDevice->IsHidden() ? true : false;
//		}
//	};
//}

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

	CNdasDeviceVector devices;
	{
		registrar.Lock();
		registrar.GetItems(devices);
		registrar.Unlock();

		// Filter out hidden devices
		CNdasDeviceVector::iterator invalid_begin = 
			std::remove_if(
				devices.begin(), devices.end(),
				boost::mem_fn(&CNdasDevice::IsHidden));
		devices.erase(
			invalid_begin,
			devices.end());
	}

	NDAS_CMD_ENUMERATE_DEVICES::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_DEVICES::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD dwEntries = devices.size();

	pReply->nDeviceEntries = dwEntries;
	DWORD cbReplyExtra = (dwEntries * sizeof(NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY));

	DWORD cbData = cbReply + cbReplyExtra;

	BOOL fSuccess = WriteSuccessHeader(cbData);
	if (!fSuccess)
	{
		return FALSE;
	}

	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (CNdasDeviceVector::const_iterator itr = devices.begin();
		itr != devices.end();
		++itr)
	{
		NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY entry = {0};
		CNdasDevicePtr pDevice = *itr;

		pDevice->Lock();

		entry.DeviceId = pDevice->GetDeviceId();
		entry.SlotNo = pDevice->GetSlotNo();
		entry.GrantedAccess = pDevice->GetGrantedAccess();
		pDevice->GetName(
			MAX_NDAS_DEVICE_NAME_LEN+1,
			entry.wszDeviceName);
		
		pDevice->Unlock();

		fSuccess = WritePacket(&entry, sizeof(entry));
		if (!fSuccess)
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

	CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

	CNdasDeviceVector devices;
	{
		registrar.Lock();
		registrar.GetItems(devices);
		registrar.Unlock();

		// Filter out hidden devices
		CNdasDeviceVector::iterator invalid_begin = 
			std::remove_if(
				devices.begin(), devices.end(),
				boost::mem_fn(&CNdasDevice::IsHidden));
		devices.erase(
			invalid_begin,
			devices.end());
	}

	NDAS_CMD_ENUMERATE_DEVICES_V2::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_DEVICES_V2::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD dwEntries = devices.size();

	pReply->nDeviceEntries = dwEntries;
	DWORD cbReplyExtra = (dwEntries * sizeof(NDAS_CMD_ENUMERATE_DEVICES_V2::ENUM_ENTRY));

	DWORD cbData = cbReply + cbReplyExtra;

	BOOL fSuccess = WriteSuccessHeader(cbData);
	if (!fSuccess)
	{
		return FALSE;
	}

	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (CNdasDeviceVector::const_iterator itr = devices.begin();
		itr != devices.end();
		++itr)
	{
		NDAS_CMD_ENUMERATE_DEVICES_V2::ENUM_ENTRY entry = {0};
		CNdasDevicePtr pDevice = *itr;

		pDevice->Lock();

		entry.DeviceId = pDevice->GetDeviceId();
		entry.SlotNo = pDevice->GetSlotNo();
		entry.GrantedAccess = pDevice->GetGrantedAccess();
		entry.NdasIdExtension = pDevice->GetNdasIdExtension();
		pDevice->GetName(
			MAX_NDAS_DEVICE_NAME_LEN+1,
			entry.wszDeviceName);
		
		pDevice->Unlock();

		fSuccess = WritePacket(&entry, sizeof(entry));
		if (!fSuccess)
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

	CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

	BOOL fSuccess = FALSE;
	if (pRequest->DeviceIdOrSlot.UseSlotNo) 
	{
		fSuccess = registrar.Unregister(pRequest->DeviceIdOrSlot.SlotNo);
	}
	else 
	{
		fSuccess = registrar.Unregister(pRequest->DeviceIdOrSlot.DeviceId);
	}

	if (!fSuccess) 
	{ 
		return ReplyCommandFailed(::GetLastError()); 
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

	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);

	if (0 == pDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_INFORMATION> reply;
	NDAS_CMD_QUERY_DEVICE_INFORMATION::REPLY* pReply = &reply;

	{
		CNdasDevice::InstanceAutoLock autoDeviceLock(pDevice);

		pDevice->GetHardwareInfo(pReply->HardwareInfo);
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

		pDevice->GetName(
			MAX_NDAS_DEVICE_NAME_LEN + 1,
			pReply->wszDeviceName);

	}

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

	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);

	if (0 == pDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_INFORMATION_V2> reply;
	NDAS_CMD_QUERY_DEVICE_INFORMATION_V2::REPLY* pReply = &reply;

	{
		CNdasDevice::InstanceAutoLock autoDeviceLock(pDevice);

		pDevice->GetHardwareInfo(pReply->HardwareInfo);
		pReply->DeviceId = pDevice->GetDeviceId();
		pReply->SlotNo = pDevice->GetSlotNo();
		pReply->GrantedAccess = pDevice->GetGrantedAccess();
		pReply->NdasIdExtension = pDevice->GetNdasIdExtension();

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

		pDevice->GetName(
			MAX_NDAS_DEVICE_NAME_LEN + 1,
			pReply->wszDeviceName);

	}

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

	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);
	if (0 == pDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}
	
	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_STATUS> reply;

	{
		CNdasDevice::InstanceAutoLock autoDeviceLock(pDevice);
		reply.Status = pDevice->GetStatus();
		reply.LastError = pDevice->GetLastDeviceError();
	}

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

	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);

	if (0 == pDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	NDAS_CMD_ENUMERATE_UNITDEVICES::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_UNITDEVICES::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	//
	// unit device numbers are not contiguous (by definition only)
	//
	CNdasDevice::InstanceAutoLock autoDeviceLock(pDevice);

	DWORD nUnitDevices = pDevice->GetUnitDeviceCount();

	DWORD cbReplyExtra = nUnitDevices * sizeof(NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY);
	DWORD cbData = cbReply + cbReplyExtra;

	BOOL fSuccess = WriteSuccessHeader(cbData);
	if (!fSuccess)
	{
		return FALSE;
	}

	pReply->nUnitDeviceEntries = nUnitDevices;
	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < CNdasDevice::MAX_NDAS_UNITDEVICE_COUNT; ++i) 
	{
		CNdasUnitDevicePtr pUnitDevice = pDevice->GetUnitDevice(i);
		if (0 != pUnitDevice.get()) 
		{
			NDAS_CMD_ENUMERATE_UNITDEVICES::ENUM_ENTRY entry;
			entry.UnitNo = i;
			entry.UnitDeviceType = pUnitDevice->GetType();

			fSuccess = WritePacket(&entry, sizeof(entry));
			if (!fSuccess)
			{
				return FALSE;
			}
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
	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);

	if (0 == pUnitDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_INFORMATION> reply;

	{
		CNdasUnitDevice::InstanceAutoLock autoUnitDeviceLock(pUnitDevice);

		reply.UnitDeviceType = pUnitDevice->GetType();
		reply.UnitDeviceSubType = pUnitDevice->GetSubType();
		reply.HardwareInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
		pUnitDevice->GetHardwareInfo(&reply.HardwareInfo);
		pUnitDevice->GetStats(reply.Stats);
	}

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

	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);
	
	if (0 == pUnitDevice.get()) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_STATUS> reply;

	{
		CNdasUnitDevice::InstanceAutoLock autoUnitDeviceLock(pUnitDevice);
		reply.Status = pUnitDevice->GetStatus();
		reply.LastError = pUnitDevice->GetLastError();
	}

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

	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);

	if (0 == pDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND); 
	}

	PNDAS_CMD_SET_DEVICE_PARAM_DATA pParam = &pRequest->Param;
	
	BOOL fSuccess(FALSE);

	switch (pParam->ParamType) 
	{
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

	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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

	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->dwUnitNo);
	if (0 == pUnitDevice.get()) 
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

	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);

	if (0 == pUnitDevice.get()) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT> reply;

	BOOL fSuccess = pUnitDevice->GetActualHostUsageCount(
		&reply.nROHosts,
		&reply.nRWHosts,
		TRUE);

	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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

	CNdasLogicalDevicePtr pLogDevice = pGetNdasLogicalDevice(pRequest->LogicalDeviceId);
	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
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

	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);

	if (0 == pUnitDevice.get()) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pUnitDevice->GetUnitDeviceId());

	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE> reply;

	reply.LogicalDeviceId = pLogDevice->GetLogicalDeviceId();

	return ReplySuccessSimple(&reply);
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
//struct NcpFilterLogicalDevice
//{
//	CNdasLogicalDeviceCollection& _coll;
//
//	NcpFilterLogicalDevice(CNdasLogicalDeviceCollection& coll) : _coll(coll)
//	{ }
//
//	void operator ()(CNdasLogicalDevice* pLogicalDevice)
//	{
//		XTLASSERT(NULL != pLogicalDevice);
//		if (NULL == pLogicalDevice) return;
//
//		//
//		// Filter Logical Device filters out hidden devices
//		// A logical device is hidden if the device of the first
//		// unit device is set to hide.
//		//
//		CNdasUnitDevicePtr pUnitDevice = pLogicalDevice->GetUnitDevice(0);
//
//		//
//		// If the first unit device of the logical device is not registered,
//		// it is assumed that the device of the unit device is NOT hidden.
//		// This happens when RAID members are partially registered,
//		// and the first one is not registered.
//		//
//		if (0 != pUnitDevice.get())
//		{
//			CNdasDevicePtr pDevice = pUnitDevice->GetParentDevice();
//			XTLASSERT(0 != pDevice.get());
//
//			//
//			// Do not add hidden device
//			//
//			if (pDevice->IsHidden()) return;
//		}
//
//		//
//		// Add a non-hidden device to the collection 
//		//
//		_coll.push_back(pLogicalDevice);
//	}
//};

template<>
BOOL
CNdasCommandProcessor::ProcessCommandRequest<
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST>(
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REQUEST* pRequest, 
	DWORD cbExtraInData)
{
	NDAS_CMD_NO_EXTRA_DATA(cbExtraInData);

	// only non-hidden logical devices
	CNdasLogicalDeviceVector logDevices;
	{
		CNdasLogicalDeviceManager& manager = m_service.GetLogicalDeviceManager();
		manager.Lock();
		manager.GetItems(logDevices);
		manager.Unlock();

		//
		// Filter out hidden logical devices
		// A hidden logical device is a logical device, 
		// of which the device of the primary unit device
		// is hidden.
		//
		CNdasLogicalDeviceVector::iterator erase_start = 
			std::remove_if(
				logDevices.begin(), logDevices.end(),
				boost::mem_fn(&CNdasLogicalDevice::IsHidden));
		logDevices.erase(
			erase_start, logDevices.end());
	}

	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REPLY reply = {0};
	NDAS_CMD_ENUMERATE_LOGICALDEVICES::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD nLogDevices =	logDevices.size();

	DWORD cbReplyExtra = nLogDevices * sizeof(NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY);
	DWORD cbData = cbReply + cbReplyExtra;

	BOOL fSuccess = WriteSuccessHeader(cbData);
	if (!fSuccess)
	{
		return FALSE;
	}

	pReply->nLogicalDeviceEntries = nLogDevices;
	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (CNdasLogicalDeviceVector::const_iterator itr = logDevices.begin(); 
		logDevices.end() != itr; 
		++itr) 
	{
		CNdasLogicalDevicePtr pLogicalDevice = *itr;
		XTLASSERT(CNdasLogicalDeviceNullPtr != pLogicalDevice);

		NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY entry;
		entry.LogicalDeviceId = pLogicalDevice->GetLogicalDeviceId();
		entry.LogicalDeviceType = pLogicalDevice->GetType();

		fSuccess = WritePacket(&entry, sizeof(entry));
		if (!fSuccess)
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LpiParam.LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	ACCESS_MASK requestingAccess = pRequest->LpiParam.Access;
	ACCESS_MASK allowedAccess = pLogDevice->GetGrantedAccess();
	if ((requestingAccess & allowedAccess) != requestingAccess) 
	{
		return ReplyCommandFailed(NDASUSER_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED); 
	}

	BOOL fSuccess = pLogDevice->PlugIn(
		requestingAccess,
		pRequest->LpiParam.LdpfFlags,
		pRequest->LpiParam.LdpfValues);

	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	BOOL fSuccess = pLogDevice->Eject();

	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_EJECT_LOGICALDEVICE_2> reply;

	BOOL fSuccess = pLogDevice->EjectEx(
		&reply.ConfigRet, 
		&reply.VetoType, 
		reply.VetoName, 
		MAX_PATH);

	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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
	CNdasLogicalDevicePtr pLogDevice =
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	BOOL fSuccess = pLogDevice->Unplug();
	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	CNdasLogicalDevice::InstanceAutoLock autolock(pLogDevice);

	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REPLY reply = {0};
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD nEntries = pLogDevice->GetUnitDeviceCount();

	pReply->LogicalDeviceType = pLogDevice->GetType();
	pReply->GrantedAccess = pLogDevice->GetGrantedAccess();
	pReply->MountedAccess = pLogDevice->GetMountedAccess();
	pReply->nUnitDeviceEntries = nEntries;

	// Logical Device Params
	pReply->LogicalDeviceParams.CurrentMaxRequestBlocks = 
		pLogDevice->GetCurrentMaxRequestBlocks();

	switch (pLogDevice->GetType()) 
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
		{
			UINT64 blocks = pLogDevice->GetUserBlockCount();
			pReply->LogicalDiskInfo.Blocks = blocks;
			const NDAS_CONTENT_ENCRYPT* pce = pLogDevice->GetContentEncrypt();
			if (NULL == pce)
			{
				break;
			}
			//
			// For now, we cannot retrieve Revision information from NDAS_CONTENT_ENCRYPT.
			// However, there is only one NDAS_CONTENT_ENCRYPT_REVISION if instantiated.
			//
			pReply->LogicalDiskInfo.ContentEncrypt.Revision = NDAS_CONTENT_ENCRYPT_REVISION;
			pReply->LogicalDiskInfo.ContentEncrypt.Type = pce->Method;
			pReply->LogicalDiskInfo.ContentEncrypt.KeyLength = pce->KeyLength;
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

	BOOL fSuccess = WriteSuccessHeader(cbData);
	if (!fSuccess)
	{
		return FALSE;
	}

	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < nEntries; ++i) 
	{
		NDAS_UNITDEVICE_ID entry = pLogDevice->GetUnitDeviceID(i);
		NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION::UNITDEVICE_ENTRY replyEntry;
		replyEntry.DeviceId = entry.DeviceId;
		replyEntry.UnitNo = entry.UnitNo;
		fSuccess = WritePacket(&replyEntry,sizeof(replyEntry));
		if (!fSuccess)
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	CNdasLogicalDevice::InstanceAutoLock autolock(pLogDevice);

	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REPLY reply = {0};
	NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;

	DWORD nEntries = pLogDevice->GetUnitDeviceCount();

	pReply->LogicalDeviceType = pLogDevice->GetType();
	pReply->GrantedAccess = pLogDevice->GetGrantedAccess();
	pReply->MountedAccess = pLogDevice->GetMountedAccess();
	pReply->nUnitDeviceEntries = nEntries;

	// Logical Device Params
	pReply->LogicalDeviceParams.CurrentMaxRequestBlocks = 
		pLogDevice->GetCurrentMaxRequestBlocks();

	switch (pLogDevice->GetType()) 
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
		{
			UINT64 blocks = pLogDevice->GetUserBlockCount();
//			XTLASSERT(blocks <= (UINT64)((DWORD)(-1)));
//			pReply->LogicalDiskInfo.Blocks = static_cast<DWORD>(blocks);
			pReply->LogicalDiskInfo.Blocks = blocks;
			const NDAS_CONTENT_ENCRYPT* pce = pLogDevice->GetContentEncrypt();
			if (NULL == pce)
			{
				break;
			}
			//
			// For now, we cannot retrieve Revision information from NDAS_CONTENT_ENCRYPT.
			// However, there is only one NDAS_CONTENT_ENCRYPT_REVISION if instantiated.
			//
			pReply->LogicalDiskInfo.ContentEncrypt.Revision = NDAS_CONTENT_ENCRYPT_REVISION;
			pReply->LogicalDiskInfo.ContentEncrypt.Type = pce->Method;
			pReply->LogicalDiskInfo.ContentEncrypt.KeyLength = pce->KeyLength;
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

	BOOL fSuccess = WriteSuccessHeader(cbData);
	if (!fSuccess)
	{
		return FALSE;
	}

	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < nEntries; ++i) 
	{
		NDAS_UNITDEVICE_ID entry = pLogDevice->GetUnitDeviceID(i);
		NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::UNITDEVICE_ENTRY replyEntry;
		replyEntry.DeviceId = entry.DeviceId;
		replyEntry.UnitNo = entry.UnitNo;

		{
			CNdasDevicePtr pRegisteredDevice = pGetNdasDevice(entry.DeviceId);
			if (pRegisteredDevice) {
				pRegisteredDevice->Lock();
				replyEntry.NdasIdExtension = pRegisteredDevice->GetNdasIdExtension();
				pRegisteredDevice->Unlock();
			}
			else
			{
				// If not registered, we don't know extension. Just give default extention.
				replyEntry.NdasIdExtension = NDAS_ID_EXTENSION_DEFAULT;
			}
		}

		fSuccess = WritePacket(&replyEntry,sizeof(replyEntry));
		if (!fSuccess)
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_LOGICALDEVICE_STATUS> reply;

	reply.Status = pLogDevice->GetStatus();
	reply.LastError = pLogDevice->GetLastError();

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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDevice(pRequest->LogicalDeviceId);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_NDAS_LOCATION> reply;

	reply.NdasLocation = pLogDevice->GetNdasLocation();

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

	CNdasLogicalDeviceManager& logDeviceManager = pGetNdasLogicalDeviceManager();

	CNdasLogicalDevicePtr pLogDevice = 
		logDeviceManager.FindByNdasLocation(pRequest->NdasLocation);

	if (CNdasLogicalDeviceNullPtr == pLogDevice)
	{
		return ReplyCommandFailed(NDASSVC_ERROR_LOGICALDEVICE_ENTRY_NOT_FOUND); 
	}

	NdasCmdReply<NDAS_CMD_QUERY_NDAS_LOGICALDEVICE_ID> reply;

	reply.LogicalDeviceId = pLogDevice->GetLogicalDeviceId();

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

	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);
	if (0 == pUnitDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	BOOL fSuccess = hixdisc.Initialize();
	DWORD nROHosts, nRWHosts;

	fSuccess = pUnitDevice->GetHostUsageCount(&nROHosts,&nRWHosts,TRUE);
	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
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
	NDAS_UNITDEVICE_ID unitDeviceId = pUnitDevice->GetUnitDeviceId();

	if (nHosts > 0) 
	{
		fSuccess = hixdisc.Discover(unitDeviceId,uda,nHosts,1000);
		if (!fSuccess) 
		{
			return ReplyCommandFailed(::GetLastError());
		}

		nHosts = hixdisc.GetHostCount(unitDeviceId);
	}

	NDAS_CMD_QUERY_HOST_UNITDEVICE::REPLY reply = {0};
	NDAS_CMD_QUERY_HOST_UNITDEVICE::REPLY* pReply = &reply;
	DWORD cbReply = sizeof(reply) > 1 ? sizeof(reply) : 0;
	DWORD cbReplyExtra = nHosts * sizeof(NDAS_CMD_QUERY_HOST_ENTRY);

	fSuccess = WriteHeader(NDAS_CMD_STATUS_SUCCESS, cbReply + cbReplyExtra);
	if (!fSuccess)
	{
		return FALSE;
	}

	pReply->EntryCount = nHosts;

	fSuccess = WritePacket(pReply, cbReply);
	if (!fSuccess)
	{
		return FALSE;
	}

	for (DWORD i = 0; i < nHosts; ++i) 
	{
		NDAS_HIX_UDA hostUda = 0;
		NDAS_CMD_QUERY_HOST_ENTRY hostEntry = {0};

		fSuccess = hixdisc.GetHostData(
			unitDeviceId,
			i,
			&hostUda,
			&hostEntry.HostGuid,
			NULL,
			NULL);

		if (hostUda & NHIX_UDA_READ_ACCESS) 
		{
			hostEntry.Access |= GENERIC_READ;
		}
		if (hostUda & NHIX_UDA_WRITE_ACCESS) 
		{
			hostEntry.Access |= GENERIC_WRITE;
		}

		if (!fSuccess) 
		{
			// warning!!!
		}

		fSuccess = WritePacket(&hostEntry, sizeof(NDAS_CMD_QUERY_HOST_ENTRY));
		if (!fSuccess)
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
		CNdasUnitDevicePtr pUnitDevice = 
			pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);
		if (0 == pUnitDevice.get()) 
		{ 
			return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
		}
		pUnitDevice->GetUnitDeviceId(&unitDeviceId);
	}

	CNdasHostInfoCache* pHostInfoCache = pGetNdasHostInfoCache();
	SOCKADDR_LPX boundAddr, remoteAddr;
	BOOL fSuccess = pHostInfoCache->GetHostNetworkInfo(
		&pRequest->HostGuid,
		&boundAddr,
		&remoteAddr);

	if (!fSuccess) 
	{
		return ReplyCommandFailed(NDASSVC_ERROR_NDAS_HOST_INFO_NOT_FOUND);
	}

	CNdasHIXSurrenderAccessRequest sar(pGetNdasHostGuid());
	fSuccess = sar.Initialize();
	if (!fSuccess) 
	{
		return ReplyCommandFailed(::GetLastError());
	}

	fSuccess = sar.Request(
		&boundAddr, 
		&remoteAddr, 
		unitDeviceId, 
		nhixUDA, 
		2000);

	if (!fSuccess) 
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
	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);
	if (0 == pDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
	}

	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);

	if (0 == pUnitDevice.get()) 
	{ 
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	pDevice->Lock();
	if (NDAS_DEVICE_STATUS_DISABLED == pDevice->GetStatus()) 
	{
		// do nothing for disabled device
	}
	else 
	{
		(void) pDevice->Enable(FALSE);
		(void) pDevice->Enable(TRUE);
	}
	pDevice->Unlock();

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
		CNdasDeviceRegistrar& registrar = m_service.GetDeviceRegistrar();

		CNdasDeviceVector devices;

		registrar.Lock();
		registrar.GetItems(devices);
		registrar.Unlock();

		std::for_each(
			devices.begin(), devices.end(),
			InvalidateEncryptedUnitDevice());

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
	CNdasDevicePtr pDevice = pGetNdasDevice(pRequest->DeviceIdOrSlot);
	if (0 == pDevice.get())
	{
		return ReplyCommandFailed(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_DEVICE_STAT> reply;

	pDevice->GetStats(reply.DeviceStat);

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
	CNdasUnitDevicePtr pUnitDevice = 
		pGetNdasUnitDevice(pRequest->DeviceIdOrSlot, pRequest->UnitNo);
	if (0 == pUnitDevice.get())
	{
		return ReplyCommandFailed(NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND);
	}

	NdasCmdReply<NDAS_CMD_QUERY_UNITDEVICE_STAT> reply;

	pUnitDevice->GetStats(reply.UnitDeviceStat);

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
	BOOL fSuccess(FALSE);
	
	fSuccess = ReadHeader();
	if (!fSuccess) 
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
		fSuccess = ReadPacket(lpBuffer, cbToRead, &cbRead);
		delete [] lpBuffer; // discard the buffer!
		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
				"HPVM:Reading remaining packet failed, error=0x%X\n", GetLastError());
			XTLTRACE2(NDASSVC_CMDPROCESSOR, TRACE_LEVEL_ERROR,
				"HPVM:Fin\n");
			return FALSE;
		}

		fSuccess = WriteHeader(NDAS_CMD_STATUS_UNSUPPORTED_VERSION, 0);
		if (!fSuccess) 
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
			BOOL fSuccess = ReadPacket(pRequest, cbRequest, &cbRead); \
			if (!fSuccess || cbRead != cbRequest) { \
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
