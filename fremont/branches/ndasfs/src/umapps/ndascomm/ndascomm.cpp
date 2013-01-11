#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <stdlib.h>
#include <winsock2.h>
#include <lpxwsock.h>
#include <strsafe.h>
#include <ndas/ndasid.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndascomm.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>
#include "ndascomm_type_internal.h"
#include "ndascommlsp.h"
#include "ndascommp.h"
#include "ndascommtransport.h"

#define ASSERT_PARAM_ERROR NDASCOMM_ERROR_INVALID_PARAMETER

#define APITRACE_SE(_err_, _stmt_) \
	do { \
		if (!(_stmt_)) { \
			XTLTRACE1(TRACE_LEVEL_ERROR, #_stmt_ ", error=0x%X\n", _err_); \
		} \
	} while(0)

#define APITRACE_SE_RET(_err_,_stmt_) \
	do { \
		BOOL success__; \
		APITRACE_SE((_err_), success__ = (_stmt_)); \
		if (!success__) return FALSE; \
	} while(0)

#define ASSERT_PARAM(pred) APITRACE_SE_RET(ASSERT_PARAM_ERROR, pred)

#ifdef RUN_WPP
#include "ndascomm.tmh"
#endif

// remove this line
#define NDASCOMM_ERROR_NOT_INITIALIZED	0xFFFFFFFF

static const NDAS_OEM_CODE
NDAS_PRIVILEGED_OEM_CODE_DEFAULT = {
	0x1E, 0x13, 0x50, 0x47, 0x1A, 0x32, 0x2B, 0x3E };

const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE  = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT = { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };
const NDAS_OEM_CODE NDAS_OEM_CODE_RUTTER  = NDAS_OEM_CODE_DEFAULT;
const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE = { 0x52, 0x41, 0x27, 0x46, 0xBC, 0x6E, 0xA2, 0x99 };

const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT = { 0xCD, NDAS_VID_DEFAULT, 0xFF, 0xFF };
const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE = { 0xCD, NDAS_VID_SEAGATE, 0xFF, 0xFF };

enum {
	NDASCOMM_BLOCK_LENGTH = 512
};


// anonymous namespace for local functions
namespace 
{

// Assertion Helper Functions
FORCEINLINE BOOL IsValidWritePtr(LPVOID lp, UINT_PTR ucb)
{ 
	return !IsBadWritePtr(lp, ucb); 
}

FORCEINLINE BOOL IsValidReadPtr(CONST VOID* lp, UINT_PTR ucb) 
{
	return !IsBadReadPtr(lp, ucb); 
}

template <typename T>
T* ByteOffset(T* Pointer, ULONG_PTR Offset)
{
	return reinterpret_cast<T*>(
		reinterpret_cast<PBYTE>(Pointer) + Offset);
}

template <typename T>
const T* ByteOffset(const T* Pointer, ULONG_PTR Offset)
{
	return reinterpret_cast<const T*>(
		reinterpret_cast<const BYTE*>(Pointer) + Offset);
}

} // end of anonymous namespace

FORCEINLINE
BOOL 
NdasCommpIsValidNdasHandleForRW(
	HNDAS NdasHandle);

FORCEINLINE
BOOL 
NdasCommpIsValidNdasHandle(
	HNDAS NdasHandle);

FORCEINLINE
BOOL
NdasCommpIsValidSocketAddressList(
	const SOCKET_ADDRESS_LIST* SocketAddressList);

FORCEINLINE
BOOL
NdasCommpIsValidNdasConnectionInfo(
	CONST NDASCOMM_CONNECTION_INFO* pci);

BOOL
NdasCommpConnectionInfoToDeviceID(
	__in CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	__out PNDAS_UNITDEVICE_ID pUnitDeviceID,
	__out_opt NDASID_EXT_DATA* NdasIdExtentionData);

FORCEINLINE
UINT64 
NdasCommpGetPassword(
	__in CONST BYTE* pAddress,
	__in CONST NDASID_EXT_DATA* NdasIdExtensionData);

FORCEINLINE
VOID
NdasCommpCleanupInitialLocks(HNDAS NdasHandle);

VOID 
NdasCommpDisconnect(
	PNDASCOMM_CONTEXT context,
	DWORD DisconnectFlags);

NDASCOMM_API
DWORD
NDASAPICALL
NdasCommGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDASCOMM_API_VERSION_MAJOR, 
		NDASCOMM_API_VERSION_MINOR);
}

LONG NdasCommInitCount = 0;

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommInitialize()
{
	WSADATA wsaData;

	int ret = WSAStartup(MAKEWORD(2,0), &wsaData);

	if (0 != ret)
	{
		SetLastError(ret);
		return FALSE;
	}

	LONG init = InterlockedIncrement(&NdasCommInitCount);
	_ASSERT(init > 0);

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUninitialize()
{
	int ret = WSACleanup();

	if (0 != ret)
	{
		return FALSE;
	}

	LONG init = InterlockedDecrement(&NdasCommInitCount);
	_ASSERT(init >= 0);

	return TRUE;
}

PNDASCOMM_CONTEXT
NdasCommpCreateContext()
{
	const DWORD lspSessionBufferSize = lsp_get_session_buffer_size();
	const DWORD contextBufferSize = 
		FIELD_OFFSET(NDASCOMM_CONTEXT, LspSessionBuffer) + 
		lspSessionBufferSize;

	LPVOID contextBuffer = calloc(contextBufferSize, 1);

	if (NULL == contextBuffer)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"malloc failed, bytes=%d\n", contextBufferSize);
		SetLastError(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	PNDASCOMM_CONTEXT context = 
		reinterpret_cast<PNDASCOMM_CONTEXT>(contextBuffer);

	context->s = INVALID_SOCKET;
	context->TransferEvent = WSACreateEvent();
	context->ReceiveTimeout = NDASCOMM_RECEIVE_TIMEOUT_DEFAULT;
	context->SendTimeout = NDASCOMM_SEND_TIMEOUT_DEFAULT;

	if (NULL == context->TransferEvent)
	{
		XTL_SAVE_LAST_ERROR();
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CreateEvent failed, error=0x%X\n", GetLastError());		
		free(contextBuffer);
		return NULL;
	}

	context->State = NDASCOMM_STATE_CREATED;
	context->LspSessionBufferSize = lspSessionBufferSize;
	context->LspHandle = lsp_initialize_session(
		context->LspSessionBuffer, context->LspSessionBufferSize);

	_ASSERT(NULL != context->LspHandle);

	__try
	{
		InitializeCriticalSection(&context->ContextLock);
	}
	__except (1)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Context lock init failed, error=0x%X\n", GetLastError());
		free(contextBuffer);
		SetLastError(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	return context;
}

VOID
NdasCommpDeleteContext(
	__in PNDASCOMM_CONTEXT NdasCommContext)
{
	_ASSERT(NDASCOMM_STATE_CREATED == NdasCommContext->State);
	if (NULL != NdasCommContext->LspWriteBuffer)
	{
		free(NdasCommContext->LspWriteBuffer);
	}
	DeleteCriticalSection(&NdasCommContext->ContextLock);
	free(NdasCommContext);
}

NDASCOMM_API
HNDAS
NdasCommConnectEx(
	__in CONST NDASCOMM_CONNECTION_INFO* ConnectionInfo,
	__in LPOVERLAPPED Overlapped)
{
	SetLastError(NDASCOMM_ERROR_NOT_IMPLEMENTED);
	return NULL;
}

NDASCOMM_API
HNDAS
NDASAPICALL
NdasCommConnect(
	CONST NDASCOMM_CONNECTION_INFO* NdasConnInfo)
{
	int result;

	if (InterlockedCompareExchange(&NdasCommInitCount, 0, 0) < 1) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NDASCOMM_ERROR_NOT_INITIALIZED!\n");
		SetLastError(NDASCOMM_ERROR_NOT_INITIALIZED);
		return NULL;
	}

	if (!NdasCommpIsValidNdasConnectionInfo(NdasConnInfo))
	{
		// Last Error is set by IsValidNdasConnectionInfo
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Invalid Binding Socket Address List, error=0x%X\n",
			GetLastError());
		return NULL;
	}

	NDASID_EXT_DATA ndasIdExtension;
	NDAS_UNITDEVICE_ID unitDeviceID;
	
	BOOL success = NdasCommpConnectionInfoToDeviceID(
		NdasConnInfo, 
		&unitDeviceID,
		&ndasIdExtension);

	if (!success)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"DeviceID is invalid, error=0x%X\n",
			GetLastError());
		return NULL;
	}

	unitDeviceID.UnitNo = NdasConnInfo->UnitNo;

	PNDASCOMM_CONTEXT context = NdasCommpCreateContext();
	if (NULL == context)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Handle.CreateContext failed, error=0x%X\n",
			GetLastError());
		return NULL;
	}

	context->State = NDASCOMM_STATE_CREATED;

	// store connection flags
	context->Flags = NdasConnInfo->Flags;

	const SOCKET_ADDRESS_LIST* localSocketAddressList = 
		reinterpret_cast<const SOCKET_ADDRESS_LIST*>(
			NdasConnInfo->BindingSocketAddressList);

	switch (NdasConnInfo->AddressType)
	{
	case NDASCOMM_CIT_NDAS_IDW:
	case NDASCOMM_CIT_NDAS_IDA:
	case NDASCOMM_CIT_DEVICE_ID:
	case NDASCOMM_CIT_SA_LPX:
	case NDASCOMM_CIT_SA_IN:
		;
	}

	SOCKADDR_LPX sockAddrLpx;
	sockAddrLpx.slpx_family = AF_LPX;
	sockAddrLpx.slpx_port = htons(LPXPORT_NDAS_TARGET);
	sockAddrLpx.slpx_addr.node[0] = unitDeviceID.DeviceId.Node[0];
	sockAddrLpx.slpx_addr.node[1] = unitDeviceID.DeviceId.Node[1];
	sockAddrLpx.slpx_addr.node[2] = unitDeviceID.DeviceId.Node[2];
	sockAddrLpx.slpx_addr.node[3] = unitDeviceID.DeviceId.Node[3];
	sockAddrLpx.slpx_addr.node[4] = unitDeviceID.DeviceId.Node[4];
	sockAddrLpx.slpx_addr.node[5] = unitDeviceID.DeviceId.Node[5];

	SOCKET_ADDRESS deviceSocketAddress;
	deviceSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	deviceSocketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&sockAddrLpx);

	HRESULT hr = NdasCommTransportConnect(
		&context->s,
		LPXPROTO_STREAM,
		&deviceSocketAddress,
		localSocketAddressList);
	
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NdasCommpConnect failed, hr=0x%X\n", hr);
		return NULL;
	}
	
	context->State = NDASCOMM_STATE_CONNECTED;

	context->LspLoginInfo.login_type = 
		(NDASCOMM_LOGIN_TYPE_NORMAL == NdasConnInfo->LoginType ) ?
		LSP_LOGIN_TYPE_NORMAL : LSP_LOGIN_TYPE_DISCOVER;

	if (NdasConnInfo->OEMCode.UI64Value)
	{
		CopyMemory(
			context->LspLoginInfo.password,
			NdasConnInfo->OEMCode.Bytes,
			sizeof(context->LspLoginInfo.password));
	}
	else
	{
		UINT64 pwd = NdasCommpGetPassword(
			unitDeviceID.DeviceId.Node, 
			&ndasIdExtension);
		CopyMemory(
			context->LspLoginInfo.password,
			&pwd,
			sizeof(context->LspLoginInfo.password));
	}

	CopyMemory(
		context->LspLoginInfo.password,
		LSP_LOGIN_PASSWORD_ANY,
		sizeof(LSP_LOGIN_PASSWORD_ANY));

	context->LspLoginInfo.unit_no = (lsp_uint8_t)unitDeviceID.UnitNo;
	context->LspLoginInfo.write_access = (lsp_uint8_t)(NdasConnInfo->WriteAccess) ? 1 : 0;

	if (NdasConnInfo->Flags & NDASCOMM_CNF_ENABLE_DEFAULT_PRIVILEGED_MODE)
	{
		CopyMemory(
			context->LspLoginInfo.supervisor_password,
			NDAS_PRIVILEGED_OEM_CODE_DEFAULT.Bytes,
			sizeof(context->LspLoginInfo.supervisor_password));
	}
	else
	{
		if (NdasConnInfo->PrivilegedOEMCode.UI64Value)
		{
			CopyMemory(
				context->LspLoginInfo.supervisor_password,
				NdasConnInfo->PrivilegedOEMCode.Bytes,
				sizeof(context->LspLoginInfo.supervisor_password));
		}
	}

	hr = NdasCommiLogin(context, &context->LspLoginInfo);

	if (FAILED(hr))
	{
		NdasCommpDisconnect(context, NDASCOMM_DF_NONE);
		SetLastError(hr);
		return NULL;
	}

	context->LspHardwareData = lsp_get_hardware_data(context->LspHandle);

	if (context->LspHardwareData->data_encryption_algorithm)
	{
		size_t buflen = context->LspHardwareData->
			maximum_transfer_blocks * NDASCOMM_BLOCK_LENGTH;

		if (context->LspWriteBufferSize < buflen)
		{
			PVOID p = realloc(context->LspWriteBuffer, buflen);
			if (NULL == p)
			{
				NdasCommpDisconnect(context, NDASCOMM_DF_NONE);
				SetLastError(ERROR_OUTOFMEMORY);
				return NULL;
			}
			context->LspWriteBuffer = p;
			context->LspWriteBufferSize = buflen;
		}
	}

	context->VendorId = ndasIdExtension.VID;
	context->TransportProtocol = NdasConnInfo->Protocol;
	context->AddressType = NdasConnInfo->AddressType;
	CopyMemory(context->NdasDeviceId.Node, unitDeviceID.DeviceId.Node, sizeof(context->NdasDeviceId.Node));
	context->NdasDeviceId.VID = ndasIdExtension.VID;

	UINT32 protocolVersion;

	// Lock cleanup routine
	if (context->Flags & NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT)
	{
		// disabled by the flag
	}
	else
	{
		// use lock cleanup routine

		// only 2.0R0 requires this
		if (LSP_LOGIN_TYPE_NORMAL == context->LspLoginInfo.login_type &&
			((2 == context->LspHardwareData->hardware_version && 
			0 == context->LspHardwareData->hardware_revision)))
		{
			NdasCommpCleanupInitialLocks(context);
		}
	}


	if (LSP_LOGIN_TYPE_NORMAL == context->LspLoginInfo.login_type && 
		! *(UINT64*)&context->LspLoginInfo.supervisor_password)
	{
		hr = NdasCommiHandshake(context);

		if (FAILED(hr))
		{
			NdasCommpDisconnect(context, NDASCOMM_DF_NONE);
			SetLastError(hr);
			return NULL;
		}
	}

	context->LspAtaData = lsp_get_ata_handshake_data(context->LspHandle);

	return context;
}

VOID 
NdasCommpDisconnect(
	__in PNDASCOMM_CONTEXT context,
	__in DWORD DisconnectFlags)
{
	if (NDASCOMM_STATE_LOGGED_IN == context->State)
	{
		if (!(DisconnectFlags & NDASCOMM_DF_DONT_LOGOUT))
		{
			HRESULT hr = NdasCommiLogout(context);
			if (FAILED(hr))
			{
				XTLTRACE1(TRACE_LEVEL_WARNING, "NdasCommiLogout failed, hr=0x%X\n", hr);
			}
		}
		context->State = NDASCOMM_STATE_CONNECTED;
	}

	if (NDASCOMM_STATE_CONNECTED == context->State)
	{
		closesocket(context->s);
		context->s = INVALID_SOCKET;
		context->State = NDASCOMM_STATE_CREATED;
	}

	_ASSERT(NDASCOMM_STATE_CREATED);

	NdasCommpDeleteContext(context);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnect(
	HNDAS NdasHandle)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );

	NdasCommpDisconnect(NdasHandle, NDASCOMM_DF_NONE);

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnectEx(
	__in HNDAS NdasHandle, 
	__in DWORD DisconnectFlags)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );

	NdasCommpDisconnect(NdasHandle, DisconnectFlags);

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceRead(
	__in HNDAS NdasHandle,
	__in INT64 LogicalBlockAddress,
	__in UINT64 TransferBlocks,
	__out PVOID	Buffer)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( IsValidReadPtr(Buffer, (UINT32)TransferBlocks * NDASCOMM_BLOCK_LENGTH));

	NdasHandle->LspAtaData->lba_capacity;

	if (TransferBlocks > 0xFFFF)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	lsp_large_integer_t nextlba;
	lsp_uint16_t rblocks = static_cast<lsp_uint16_t>(TransferBlocks);

	nextlba.quad = LogicalBlockAddress;
	if (nextlba.quad < 0)
	{
		nextlba.quad += NdasHandle->LspAtaData->lba_capacity.quad;
	}

	lsp_large_integer_t fin_lba;
	fin_lba.quad = nextlba.quad + rblocks - 1;

	if (fin_lba.quad >= NdasHandle->LspAtaData->lba_capacity.quad)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	PVOID nextBuffer = Buffer;
	HRESULT hr = S_OK;

	while (rblocks > 0)
	{
		lsp_uint16_t blocks = min(
			rblocks, 
			NdasHandle->LspHardwareData->maximum_transfer_blocks);

		hr = NdasCommiAtaRead(
			NdasHandle,
			&nextlba,
			blocks,
			nextBuffer,
			blocks * NDASCOMM_BLOCK_LENGTH);

		if (FAILED(hr))
		{
			break;
		}

		rblocks -= blocks; 
		nextlba.quad += blocks;
		nextBuffer = ByteOffset(nextBuffer, blocks * NDASCOMM_BLOCK_LENGTH);
	}

	if (FAILED(hr))
	{
		SetLastError(hr);
		return FALSE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceLockedWrite(
	__in HNDAS NdasHandle,
	__in INT64 LogicalBlockAddress,
	__in UINT64	TransferBlocks,
	__in CONST VOID* Data)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandleForRW(NdasHandle) );

	BOOL success = NdasCommLockDevice(NdasHandle, 0, NULL);
	if (!success)
	{
		return FALSE;
	}

	__try
	{
		success = NdasCommBlockDeviceWrite(
			NdasHandle, 
			LogicalBlockAddress, 
			TransferBlocks, 
			Data);
	}
	__finally
	{
		DWORD lastError = GetLastError();
		{
			(VOID) NdasCommUnlockDevice(NdasHandle, 0, NULL);
		}
		SetLastError(lastError);
	}

	return success;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceWrite(
	__in HNDAS NdasHandle,
	__in INT64	LogicalBlockAddress,
	__in UINT64	TransferBlocks,
	__in CONST VOID* Buffer)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandleForRW(NdasHandle) );
	ASSERT_PARAM( IsValidReadPtr(Buffer, (UINT32)TransferBlocks * NDASCOMM_BLOCK_LENGTH));

	NdasHandle->LspAtaData->lba_capacity;

	if (TransferBlocks > 0xFFFF)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	lsp_large_integer_t nextlba;
	lsp_uint16_t rblocks = static_cast<lsp_uint16_t>(TransferBlocks);

	nextlba.quad = LogicalBlockAddress;
	if (nextlba.quad < 0)
	{
		nextlba.quad += NdasHandle->LspAtaData->lba_capacity.quad;
	}

	lsp_large_integer_t fin_lba;
	fin_lba.quad = nextlba.quad + rblocks - 1;

	if (fin_lba.quad >= NdasHandle->LspAtaData->lba_capacity.quad)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	CONST VOID* nextBuffer = Buffer;

	HRESULT hr = S_OK;

	while (rblocks > 0)
	{
		LPVOID nextWriteBuffer;

		lsp_uint16_t blocks = min(
			rblocks, 
			NdasHandle->LspHardwareData->maximum_transfer_blocks);

		if (NdasHandle->LspHardwareData->data_encryption_algorithm)
		{
			nextWriteBuffer = NdasHandle->LspWriteBuffer;
			CopyMemory(
				nextWriteBuffer, 
				nextBuffer, 
				blocks * NDASCOMM_BLOCK_LENGTH);
		}
		else
		{
			nextWriteBuffer = const_cast<LPVOID>(nextBuffer);
		}

		hr = NdasCommiAtaWrite(
			NdasHandle,
			&nextlba,
			blocks,
			nextWriteBuffer,
			blocks * NDASCOMM_BLOCK_LENGTH);

		if (FAILED(hr))
		{
			break;
		}

		rblocks -= blocks; 
		nextlba.quad += blocks;
		nextBuffer = ByteOffset(nextBuffer, blocks * NDASCOMM_BLOCK_LENGTH);
	}

	if (FAILED(hr))
	{
		SetLastError(hr);
		return FALSE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceVerify(
	IN HNDAS NdasHandle,
	IN INT64 LogicalBlockAddress,
	IN UINT64 TransferBlocks)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );

	if (TransferBlocks > 0xFFFF)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	lsp_large_integer_t nextlba;
	lsp_uint16_t rblocks = static_cast<lsp_uint16_t>(TransferBlocks);

	nextlba.quad = LogicalBlockAddress;
	if (nextlba.quad < 0)
	{
		nextlba.quad += NdasHandle->LspAtaData->lba_capacity.quad;
	}

	lsp_large_integer_t fin_lba;
	fin_lba.quad = nextlba.quad + rblocks - 1;

	if (fin_lba.quad >= NdasHandle->LspAtaData->lba_capacity.quad)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	HRESULT hr = S_OK;

	while (rblocks > 0)
	{
		lsp_uint16_t blocks = min(
			rblocks, 
			NdasHandle->LspHardwareData->maximum_transfer_blocks);

		hr = NdasCommiAtaVerify(
			NdasHandle,
			&nextlba,
			blocks,
			NULL,
			0);

		if (FAILED(hr))
		{
			break;
		}

		rblocks -= blocks; 
		nextlba.quad += blocks;
	}

	if (FAILED(hr))
	{
		SetLastError(hr);
		return FALSE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetFeatures(
	IN HNDAS NdasHandle,
	IN BYTE feature,
	IN BYTE param0,
	IN BYTE param1,
	IN BYTE param2,
	IN BYTE param3
	)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	
	lsp_status_t lspStatus = lsp_ide_setfeatures(
		NdasHandle->LspHandle,
		feature,
		param0,
		param1,
		param2,
		param3);

	HRESULT hr = NdasCommTransportLspRequest(NdasHandle, &lspStatus, NULL);

	if (FAILED(hr))
	{
		SetLastError(hr);
		return FALSE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommVendorCommand(
	IN HNDAS NdasHandle,
	IN NDASCOMM_VCMD_COMMAND vop_code,
	IN OUT PNDASCOMM_VCMD_PARAM param,
	IN OUT PBYTE pWriteData,
	IN UINT32 uiWriteDataLen,
	IN OUT PBYTE pReadData,
	IN UINT32 uiReadDataLen)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	// Write is IN and Read is OUT!. However, both are mutable
	ASSERT_PARAM( IsValidWritePtr(pWriteData, uiWriteDataLen) );
	ASSERT_PARAM( IsValidWritePtr(pReadData, uiReadDataLen) );

	UINT64 param_8; // 8 bytes if HwVersion <= 2
	lsp_uint8_t param_length = sizeof(param_8);

	lsp_io_data_buffer_t data_buf;

	data_buf.send_buffer = (lsp_uint8_t *)pWriteData;
	data_buf.send_size = (lsp_uint32_t)uiWriteDataLen;
	data_buf.recv_buffer = (lsp_uint8_t *)pReadData;
	data_buf.recv_size = (lsp_uint32_t)uiReadDataLen;

	//
	// Version check
	//

	if (NdasHandle->LspHardwareData->hardware_version < LSP_HARDWARE_VERSION_1_0)
	{
		switch (vop_code)
		{
		case ndascomm_vcmd_set_ret_time:
		case ndascomm_vcmd_set_max_conn_time:
		case ndascomm_vcmd_set_supervisor_pw:
		case ndascomm_vcmd_set_user_pw:
		case ndascomm_vcmd_reset:
			SetLastError(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED);
			return FALSE;
		}
	}

	if (NdasHandle->LspHardwareData->hardware_version < LSP_HARDWARE_VERSION_1_1)
	{
		switch (vop_code)
		{
		case ndascomm_vcmd_set_enc_opt:
		case ndascomm_vcmd_set_standby_timer:
		case ndascomm_vcmd_set_sema:
		case ndascomm_vcmd_free_sema:
		case ndascomm_vcmd_get_sema:
		case ndascomm_vcmd_get_owner_sema:
		case ndascomm_vcmd_set_dynamic_ret_time:
		case ndascomm_vcmd_get_dynamic_ret_time:
		case ndascomm_vcmd_get_ret_time:
		case ndascomm_vcmd_get_max_conn_time:
		case ndascomm_vcmd_get_standby_timer:
			SetLastError(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED);
			return FALSE;
		}
	}

	if (NdasHandle->LspHardwareData->hardware_version < LSP_HARDWARE_VERSION_2_0)
	{
		switch (vop_code)
		{
		case ndascomm_vcmd_set_delay:
		case ndascomm_vcmd_get_delay:
		case ndascomm_vcmd_set_lpx_address:
			SetLastError(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED);
			return FALSE;
		}
	}

#if 0
	if (NdasHandle->LspHardwareData->hardware_version < LSP_HARDWARE_VERSION_3_0)
	{
		switch (vop_code)
		{
		case ndascomm_vcmd_set_d_enc_opt:
		case ndascomm_vcmd_get_d_enc_opt:
			SetLastError(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED);
			return FALSE;
		}
	}
#endif

	//
	// Obsolete
	//
	if (TRUE)
	{
		switch (vop_code)
		{
		case ndascomm_vcmd_set_dynamic_max_conn_time :
			SetLastError(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED);
		}
	}

	param_8 = 0;

	switch(vop_code)
	{
	case ndascomm_vcmd_set_ret_time:

		if (param->SET_RET_TIME.RetTime == 0)
		{
			SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		
		param_8 = (UINT64)param->SET_RET_TIME.RetTime -1;
		break;
	case ndascomm_vcmd_set_max_conn_time:

		if (param->SET_RET_TIME.RetTime == 0)
		{
			SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
			return FALSE;
		}

		if (NdasHandle->LspHardwareData->hardware_version >= LSP_HARDWARE_VERSION_2_0)
		{
			param_8 = (UINT64)(param->SET_RET_TIME.RetTime -1);
		}
		else
		{
			param_8 = (UINT64)((param->SET_RET_TIME.RetTime -1) * 1000 * 1000); // micro second
		}

		break;
	case ndascomm_vcmd_set_supervisor_pw:
		CopyMemory(&param_8, param->SET_SUPERVISOR_PW.SupervisorPassword, sizeof(param_8));
		break;
	case ndascomm_vcmd_set_user_pw:
		CopyMemory(&param_8, param->SET_USER_PW.UserPassword, sizeof(param_8));
		break;
	case ndascomm_vcmd_set_enc_opt:
		param_8 |= (param->SET_ENC_OPT.EncryptHeader) ? 0x02 : 0x00;
		param_8 |= (param->SET_ENC_OPT.EncryptData) ? 0x01 : 0x00;
		break;
	case ndascomm_vcmd_set_standby_timer:
		param_8 |= (param->SET_STANDBY_TIMER.EnableTimer) ? 0x80000000 : 0x00000000;
		param_8 |= param->SET_STANDBY_TIMER.TimeValue & 0x7FFFFFFF;
		break;
	case ndascomm_vcmd_reset:
		break;
	case ndascomm_vcmd_set_sema:
		param_8 |= (UINT64)param->SET_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_free_sema:
		param_8 |= (UINT64)param->FREE_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_get_sema:
		param_8 |= (UINT64)param->GET_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_get_owner_sema:
		param_8 |= (UINT64)param->GET_OWNER_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_set_delay:

		if (param->SET_DELAY.Delay < 8)
		{
			SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		param_8 = (param->SET_DELAY.Delay)/ 8 -1;
		break;
	case ndascomm_vcmd_get_delay:
		break;
	case ndascomm_vcmd_get_dynamic_ret_time:
		break;
	case ndascomm_vcmd_get_ret_time:
		break;
	case ndascomm_vcmd_get_max_conn_time:
		break;
	case ndascomm_vcmd_get_standby_timer:
		break;
	case ndascomm_vcmd_set_lpx_address:
		param_8 = 
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[0] << 40) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[1] << 32) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[2] << 24) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[3] << 16) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[4] << 8) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[5] << 0);
		break;
	case ndascomm_vcmd_set_dynamic_max_conn_time:
	case ndascomm_vcmd_set_d_enc_opt:
	case ndascomm_vcmd_get_d_enc_opt:
	default:
		SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	HRESULT hr = NdasCommiVendorCommand(
		NdasHandle,
		NDASCOMM_VENDOR_ID,
		NDASCOMM_OP_VERSION,
		vop_code,
		(lsp_uint8_t *)&param_8,
		param_length,
		&data_buf);

	if (FAILED(hr))
	{
		SetLastError(hr);
		return FALSE;
	}

	switch(vop_code)
	{
	case ndascomm_vcmd_set_ret_time:
		break;
	case ndascomm_vcmd_set_max_conn_time:
		break;
	case ndascomm_vcmd_set_supervisor_pw:
		break;
	case ndascomm_vcmd_set_user_pw:
		break;
	case ndascomm_vcmd_set_enc_opt:
		break;
	case ndascomm_vcmd_set_standby_timer:
		break;
	case ndascomm_vcmd_reset:
		break;
	case ndascomm_vcmd_set_sema:
		param->SET_SEMA.SemaCounter = (UINT32)param_8;
		break;
	case ndascomm_vcmd_free_sema:
		param->FREE_SEMA.SemaCounter = (UINT32)param_8;
		break;
	case ndascomm_vcmd_get_sema:
		param->GET_SEMA.SemaCounter = (UINT32)param_8;
		break;
	case ndascomm_vcmd_get_owner_sema:
		CopyMemory(
			param->GET_OWNER_SEMA.AddressLPX, 
			&param_8, 
			sizeof(param->GET_OWNER_SEMA.AddressLPX));
		break;
	case ndascomm_vcmd_set_delay:
		break;
	case ndascomm_vcmd_get_delay:
		param->GET_DELAY.TimeValue = (UINT32)((param_8 +1)* 8);
		break;
	case ndascomm_vcmd_get_dynamic_ret_time:
		param->GET_DYNAMIC_RET_TIME.RetTime = (UINT32)(param_8 +1);
		break;
	case ndascomm_vcmd_get_ret_time:
		param->GET_RET_TIME.RetTime = (UINT32)param_8 ;
		break;
	case ndascomm_vcmd_get_max_conn_time:
		if (NdasHandle->LspHardwareData->hardware_version >= LSP_HARDWARE_VERSION_2_0)
		{
			param->GET_MAX_CONN_TIME.MaxConnTime = (UINT32)param_8 +1;
		}
		else
		{
			param->GET_MAX_CONN_TIME.MaxConnTime = ((UINT32)param_8 / 1000 / 1000) +1;
		}
		break;
	case ndascomm_vcmd_get_standby_timer:
		param->GET_STANDBY_TIMER.EnableTimer = (param_8 & 0x80000000) ? TRUE : FALSE;
		param->GET_STANDBY_TIMER.TimeValue = (UINT32)param_8 & 0x7FFFFFFF;
		break;
	case ndascomm_vcmd_set_lpx_address:
		break;
	case ndascomm_vcmd_set_dynamic_max_conn_time:
	case ndascomm_vcmd_set_d_enc_opt:
	case ndascomm_vcmd_get_d_enc_opt:
	default:
		SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return TRUE;
}

BOOL
NdasCommpAtaExtCommand(UCHAR CommandReg)
{
	switch (CommandReg)
	{
	case /* DM EXT */ LSP_IDE_CMD_READ_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_READ_STREAM_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_DMA_FUA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_STREAM_DMA_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_READ_DMA_QUEUED_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_WRITE_DMA_QUEUED_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_WRITE_DMA_QUEUED_FUA_EXT:

	case /* ND EXT */ LSP_IDE_CMD_FLUSH_CACHE_EXT:
	case /* ND EXT */ LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT:
	case /* ND EXT */ LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT:
	case /* ND EXT */ LSP_IDE_CMD_SET_MAX_ADDRESS_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_LOG_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_MULTIPLE_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_MULTIPLE_FUA_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_SECTORS_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_STREAM_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_LOG_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_MULTIPLE_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_SECTORS_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_STREAM_EXT:

		return TRUE;

	default:

		return FALSE;
	}
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommIdeCommand(
   __in HNDAS NdasHandle,
   __inout PNDASCOMM_IDE_REGISTER pIdeRegister,
   __inout_bcount(uiWriteDataLen) PBYTE pWriteData,
   __in UINT32 uiWriteDataLen,
   __out_bcount(uiReadDataLen) PBYTE pReadData,
   __in UINT32 uiReadDataLen)
{
	ASSERT_PARAM( IsValidWritePtr(pIdeRegister, sizeof(NDASCOMM_IDE_REGISTER)) );
	//
	// Explicit enforcement of READ/WRITE access
	//
	switch (pIdeRegister->command.command)
	{
	case LSP_IDE_CMD_NOP:

	case LSP_IDE_CMD_READ_BUFFER:

	case LSP_IDE_CMD_READ_SECTORS:
	case LSP_IDE_CMD_READ_SECTORS_EXT:
	case LSP_IDE_CMD_READ_MULTIPLE:
	case LSP_IDE_CMD_READ_MULTIPLE_EXT:
	case LSP_IDE_CMD_READ_DMA:
	case LSP_IDE_CMD_READ_DMA_EXT:
	case LSP_IDE_CMD_READ_DMA_QUEUED:
	case LSP_IDE_CMD_READ_DMA_QUEUED_EXT:

	case LSP_IDE_CMD_READ_VERIFY_SECTORS:
	case LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT:

	case LSP_IDE_CMD_IDENTIFY_DEVICE:
	case LSP_IDE_CMD_IDENTIFY_PACKET_DEVICE:

	case LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS:
	case LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT:

		// Only these commands are available for explicit IdeCommands
		ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
		break;
	default:
		// Otherwise, IdeCommand requires RW access
		ASSERT_PARAM( NdasCommpIsValidNdasHandleForRW(NdasHandle) );
		break;
	}

	lsp_ide_register_param_t idereg;
	lsp_io_data_buffer_t data_buf;

	idereg.device.device = pIdeRegister->device.device;
	idereg.command.command = pIdeRegister->command.command;

	if (NdasCommpAtaExtCommand(idereg.command.command))
	{
		idereg.reg.basic_48.reg_prev[0] = pIdeRegister->reg.basic_48.reg_prev[0];
		idereg.reg.basic_48.reg_prev[1] = pIdeRegister->reg.basic_48.reg_prev[1];
		idereg.reg.basic_48.reg_prev[2] = pIdeRegister->reg.basic_48.reg_prev[2];
		idereg.reg.basic_48.reg_prev[3] = pIdeRegister->reg.basic_48.reg_prev[3];
		idereg.reg.basic_48.reg_prev[4] = pIdeRegister->reg.basic_48.reg_prev[4];
		idereg.reg.basic_48.reg_cur[0] = pIdeRegister->reg.basic_48.reg_cur[0];
		idereg.reg.basic_48.reg_cur[1] = pIdeRegister->reg.basic_48.reg_cur[1];
		idereg.reg.basic_48.reg_cur[2] = pIdeRegister->reg.basic_48.reg_cur[2];
		idereg.reg.basic_48.reg_cur[3] = pIdeRegister->reg.basic_48.reg_cur[3];
		idereg.reg.basic_48.reg_cur[4] = pIdeRegister->reg.basic_48.reg_cur[4];
	}
	else
	{
		idereg.reg.basic.reg[0] = pIdeRegister->reg.basic.reg[0];
		idereg.reg.basic.reg[1] = pIdeRegister->reg.basic.reg[1];
		idereg.reg.basic.reg[2] = pIdeRegister->reg.basic.reg[2];
		idereg.reg.basic.reg[3] = pIdeRegister->reg.basic.reg[3];
		idereg.reg.basic.reg[4] = pIdeRegister->reg.basic.reg[4];
	}

	data_buf.send_buffer = (lsp_uint8_t *)pWriteData;
	data_buf.send_size = (lsp_uint32_t)uiWriteDataLen;
	data_buf.recv_buffer = (lsp_uint8_t *)pReadData;
	data_buf.recv_size = (lsp_uint32_t)uiReadDataLen;

	HRESULT hr = NdasCommiAtaCommand(
		NdasHandle,
		&idereg,
		&data_buf,
		NULL);

	if (FAILED(hr))
	{
		SetLastError(hr);
		return NULL;
	}

	pIdeRegister->device.device = idereg.device.device;
	pIdeRegister->command.command = idereg.command.command;

	pIdeRegister->reg.basic_48.reg_prev[0] = idereg.reg.basic_48.reg_prev[0];
	pIdeRegister->reg.basic_48.reg_prev[1] = idereg.reg.basic_48.reg_prev[1];
	pIdeRegister->reg.basic_48.reg_prev[2] = idereg.reg.basic_48.reg_prev[2];
	pIdeRegister->reg.basic_48.reg_prev[3] = idereg.reg.basic_48.reg_prev[3];
	pIdeRegister->reg.basic_48.reg_prev[4] = idereg.reg.basic_48.reg_prev[4];
	pIdeRegister->reg.basic_48.reg_cur[0] = idereg.reg.basic_48.reg_cur[0];
	pIdeRegister->reg.basic_48.reg_cur[1] = idereg.reg.basic_48.reg_cur[1];
	pIdeRegister->reg.basic_48.reg_cur[2] = idereg.reg.basic_48.reg_cur[2];
	pIdeRegister->reg.basic_48.reg_cur[3] = idereg.reg.basic_48.reg_cur[3];
	pIdeRegister->reg.basic_48.reg_cur[4] = idereg.reg.basic_48.reg_cur[4];

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceHardwareInfo(
	IN HNDAS NdasHandle,
	IN OUT PNDAS_DEVICE_HARDWARE_INFO HardwareInfo)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( IsValidWritePtr(HardwareInfo, sizeof(NDAS_DEVICE_HARDWARE_INFO)) );
	ASSERT_PARAM( HardwareInfo->Size == sizeof(NDAS_DEVICE_HARDWARE_INFO) );

	const lsp_hardware_data_t* hwdat = 
		lsp_get_hardware_data(NdasHandle->LspHandle);

	HardwareInfo->HardwareType = hwdat->hardware_type;
	HardwareInfo->HardwareVersion = hwdat->hardware_version;
	HardwareInfo->HardwareRevision = hwdat->hardware_revision;
	HardwareInfo->NumberOfCommandProcessingSlots = hwdat->number_of_slots;
	HardwareInfo->MaximumNumberOfTargets = hwdat->maximum_target_id;
	HardwareInfo->MaximumNumberOfLUs = hwdat->maximum_lun;
	HardwareInfo->MaximumTransferBlocks = hwdat->maximum_transfer_blocks;
	HardwareInfo->ProtocolType = hwdat->protocol_type;
	HardwareInfo->ProtocolVersion = hwdat->protocol_version;
	HardwareInfo->HeaderEncryptionMode = hwdat->header_encryption_algorithm;
	HardwareInfo->HeaderDigestMode = hwdat->header_digest_algorithm;
	HardwareInfo->DataEncryptionMode = hwdat->data_encryption_algorithm;
	HardwareInfo->DataDigestMode = hwdat->data_digest_algorithm;

	HardwareInfo->NdasDeviceId = NdasHandle->NdasDeviceId;

	return TRUE;
}

template <typename CharT, typename StructT>
BOOL
NdasCommGetUnitDeviceInfoT(HNDAS NdasHandle, StructT* pUnitInfo)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( IsValidWritePtr(pUnitInfo, sizeof(StructT)) );
	ASSERT_PARAM( pUnitInfo->Size == sizeof(StructT) );

	const lsp_ide_identify_device_data_t* ident = 
		lsp_get_ide_identify_device_data(NdasHandle->LspHandle);

	pUnitInfo->LBA = NdasHandle->LspAtaData->lba;
	pUnitInfo->LBA48 = NdasHandle->LspAtaData->lba48;

	pUnitInfo->PIO = 1; // PIO mode is always supported

	if (LSP_IDE_TRANSFER_MODE_MULTIWORD_DMA == NdasHandle->LspAtaData->active.dma_mode ||
		LSP_IDE_TRANSFER_MODE_SINGLEWORD_DMA == NdasHandle->LspAtaData->active.dma_mode)
	{
		pUnitInfo->DMA = 1;
	}
	if (LSP_IDE_TRANSFER_MODE_ULTRA_DMA == NdasHandle->LspAtaData->active.dma_mode)
	{
		pUnitInfo->UDMA = 1;
	}

	pUnitInfo->SectorCount.HighPart = NdasHandle->LspAtaData->lba_capacity.u.high;
	pUnitInfo->SectorCount.LowPart = NdasHandle->LspAtaData->lba_capacity.u.low;

	if (NdasHandle->LspAtaData->device_type)
	{
		const lsp_ide_identify_packet_device_data_t* pident = 
			reinterpret_cast<const lsp_ide_identify_packet_device_data_t*>(ident);

		switch(pident->general_configuration.command_packet_set)
		{
		case 0x00: // Direct-access device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_DIRECT_ACCESS_DEVICE;
			break;
		case 0x01: // Sequential-access device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_SEQUENTIAL_ACCESS_DEVICE;
			break;
		case 0x02: // Printer device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_PRINTER_DEVICE;
			break;
		case 0x03: // Processor device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_PROCESSOR_DEVICE;
			break;
		case 0x04: // Write-once device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_WRITE_ONCE_DEVICE;
			break;
		case 0x05: // CD-ROM device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_CDROM_DEVICE;
			break;
		case 0x06: // Scanner device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_SCANNER_DEVICE;
			break;
		case 0x07: // Optical memory device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE;
			break;
		case 0x08: // Medium changer device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_MEDIUM_CHANGER_DEVICE;
			break;
		case 0x09: // Communications device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_COMMUNICATIONS_DEVICE;
			break;
		case 0x0C: // Array controller device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_ARRAY_CONTROLLER_DEVICE;
			break;
		case 0x0D: // Enclosure services device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_ENCLOSURE_SERVICES_DEVICE;
			break;
		case 0x0E: // Reduced block command devices
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_REDUCED_BLOCK_COMMAND_DEVICE;
			break;
		case 0x0F: // Optical card reader/writer device
			pUnitInfo->MediaType = NDAS_UNIT_ATAPI_OPTICAL_CARD_READER_WRITER_DEVICE;
			break;
		case 0x1F: // Unknown or no device type
		case 0x0B: // Reserved for ACS IT8 (Graphic arts pre-press devices)
		default:
			pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_UNKNOWN_DEVICE;
			break;
		}
	}
	else
	{
		pUnitInfo->MediaType = NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE;
	}

	CHAR buffer[40]; // Maximum of Model, FirmwareRevision and Serial Number

	// Model
	for (int i = 0; i < sizeof(ident->model_number) / sizeof(WORD); ++i)
	{
		((WORD *)buffer)[i] = ntohs(((WORD *)ident->model_number)[i]);
	}
	
	ZeroMemory(pUnitInfo->Model, sizeof(pUnitInfo->Model));
	for (int i = 0; i < sizeof(ident->model_number); ++i)
	{
		pUnitInfo->Model[i] = static_cast<CharT>(buffer[i]);
	}

	// FirmwareRevision
	for (int i = 0; i < sizeof(ident->firmware_revision) / sizeof(WORD); ++i)
	{
		((WORD *)buffer)[i] = ntohs(((WORD *)ident->firmware_revision)[i]);
	}

	ZeroMemory(pUnitInfo->FirmwareRevision, sizeof(pUnitInfo->FirmwareRevision));
	for (int i = 0; i < sizeof(ident->firmware_revision); ++i)
	{
		pUnitInfo->FirmwareRevision[i] = static_cast<CharT>(buffer[i]);
	}

	// Serial Number
	for (int i = 0; i < sizeof(ident->serial_number) / sizeof(WORD); ++i)
	{
		((WORD *)buffer)[i] = ntohs(((WORD *)ident->serial_number)[i]);
	}

	ZeroMemory(pUnitInfo->SerialNumber, sizeof(pUnitInfo->SerialNumber));
	for (int i = 0; i < sizeof(ident->serial_number); ++i)
	{
		pUnitInfo->SerialNumber[i] = static_cast<CharT>(buffer[i]);
	}

	return TRUE;
};

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceHardwareInfoW(
	IN HNDAS NdasHandle,
	IN OUT PNDAS_UNITDEVICE_HARDWARE_INFOW pUnitInfo)
{
	return NdasCommGetUnitDeviceInfoT<WCHAR,NDAS_UNITDEVICE_HARDWARE_INFOW>(NdasHandle, pUnitInfo);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceHardwareInfoA(
	IN HNDAS NdasHandle,
	IN OUT PNDAS_UNITDEVICE_HARDWARE_INFOA pUnitInfo)
{
	return NdasCommGetUnitDeviceInfoT<CHAR,NDAS_UNITDEVICE_HARDWARE_INFOA>(NdasHandle, pUnitInfo);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceStat(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDAS_DEVICE_STAT pDeviceStat)
{
	ASSERT_PARAM( IsValidReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM(sizeof(NDASCOMM_CONNECTION_INFO) == pConnectionInfo->Size);

	ASSERT_PARAM( IsValidWritePtr(pDeviceStat, sizeof(NDAS_DEVICE_STAT)) );
	ASSERT_PARAM(sizeof(NDAS_DEVICE_STAT) == pDeviceStat->Size);

	ASSERT_PARAM( NDASCOMM_LOGIN_TYPE_DISCOVER == pConnectionInfo->LoginType);

	HNDAS NdasHandle = NdasCommConnect(pConnectionInfo);

	if (NULL == NdasHandle)
	{
		return FALSE;
	}

	lsp_uint16_t data_reply;

	NDASCOMM_BIN_PARAM_TARGET_LIST target_list = {0};
	target_list.ParamType = NDASCOMM_BINPARM_TYPE_TARGET_LIST;

	HRESULT hr = NdasCommiTextCommand(
		NdasHandle, 
		NDASCOMM_TEXT_TYPE_BINARY,
		NDASCOMM_TEXT_VERSION,
		(lsp_uint8_t *)&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST,
		(lsp_uint8_t *)&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY);

	if (FAILED(hr))
	{
		NdasCommDisconnect(NdasHandle);
		SetLastError(hr);
		return FALSE;
	}

	pDeviceStat->NumberOfUnitDevices = target_list.NRTarget;

	BYTE hardwareVersion = NdasHandle->LspHardwareData->hardware_version;
	UINT16 hardwareRevision = NdasHandle->LspHardwareData->hardware_revision;

	for (int i = 0; i < NDAS_MAX_UNITDEVICES; ++i)
	{
		PNDAS_UNITDEVICE_STAT pUnitDeviceStat = &pDeviceStat->UnitDevices[i];
		pUnitDeviceStat->Size = sizeof(NDAS_UNITDEVICE_STAT);
		pUnitDeviceStat->IsPresent = (i < target_list.NRTarget) ? TRUE : FALSE;

		pUnitDeviceStat->RWHostCount = 
			target_list.PerTarget[i].NRRWHost;

		// chip bug : Read only host count is invalid at V 2.0 rev.0
		pUnitDeviceStat->ROHostCount = 
			(i < target_list.NRTarget) ? 
				(LSP_HARDWARE_VERSION_2_0 == hardwareVersion && 
				LSP_HARDWARE_V20_REV_0 == hardwareRevision) ? 
					NDAS_HOST_COUNT_UNKNOWN : 
					target_list.PerTarget[i].NRROHost :
				0;

		CopyMemory(
			pUnitDeviceStat->TargetData, 
			&target_list.PerTarget[i].TargetData0,
			sizeof(UINT32));

		CopyMemory(
			&pUnitDeviceStat->TargetData[0] + sizeof(UINT32),
			&target_list.PerTarget[i].TargetData1,
			sizeof(UINT32));
	}

	(VOID) NdasCommDisconnect(NdasHandle);
	
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceStat(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDAS_UNITDEVICE_STAT pUnitDeviceStat)
{
	ASSERT_PARAM( IsValidReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM(sizeof(NDASCOMM_CONNECTION_INFO) == pConnectionInfo->Size);

	ASSERT_PARAM( IsValidWritePtr(pUnitDeviceStat, sizeof(NDAS_UNITDEVICE_STAT)) );
	ASSERT_PARAM(sizeof(NDAS_UNITDEVICE_STAT) == pUnitDeviceStat->Size);

	ASSERT_PARAM( NDASCOMM_LOGIN_TYPE_DISCOVER == pConnectionInfo->LoginType);

	HNDAS NdasHandle = NdasCommConnect(pConnectionInfo);

	if (NULL == NdasHandle)
	{
		return FALSE;
	}

	lsp_uint16_t data_reply;

	NDASCOMM_BIN_PARAM_TARGET_LIST target_list = {0};
	target_list.ParamType = NDASCOMM_BINPARM_TYPE_TARGET_LIST;

	HRESULT hr = NdasCommiTextCommand(
		NdasHandle,
		NDASCOMM_TEXT_TYPE_BINARY,
		NDASCOMM_TEXT_VERSION,
		&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST,
		&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY);

	if (FAILED(hr))
	{
		NdasCommDisconnect(NdasHandle);
		SetLastError(hr);
		return FALSE;
	}

	// pUnitDeviceStat->NumberOfTargets = target_list.NRTarget;
	pUnitDeviceStat->IsPresent = 
		(NdasHandle->LspLoginInfo.unit_no < target_list.NRTarget) ? TRUE : FALSE;
	pUnitDeviceStat->RWHostCount = 
		target_list.PerTarget[NdasHandle->LspLoginInfo.unit_no].NRRWHost;

	BYTE hardwareVersion = NdasHandle->LspHardwareData->hardware_version;
	UINT16 hardwareRevision = NdasHandle->LspHardwareData->hardware_revision;

	if (LSP_HARDWARE_VERSION_2_0 == hardwareVersion && 
		LSP_HARDWARE_V20_REV_0 == hardwareRevision)
	{
		// chip bug : Read only host count is invalid at V 2.0 original.
		pUnitDeviceStat->ROHostCount = NDAS_HOST_COUNT_UNKNOWN;
	}
	else
	{
		pUnitDeviceStat->ROHostCount = 
			target_list.PerTarget[NdasHandle->LspLoginInfo.unit_no].NRROHost;
	}

	CopyMemory(
		pUnitDeviceStat->TargetData, 
		&target_list.PerTarget[NdasHandle->LspLoginInfo.unit_no].TargetData0,
		sizeof(UINT32));

	CopyMemory(
		&pUnitDeviceStat->TargetData[0] + sizeof(UINT32),
		&target_list.PerTarget[NdasHandle->LspLoginInfo.unit_no].TargetData1,
		sizeof(UINT32));

	(VOID) NdasCommDisconnect(NdasHandle);
	
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetReceiveTimeout(
	IN HNDAS NdasHandle, 
	IN DWORD Timeout)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	NdasHandle->ReceiveTimeout = Timeout;
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetReceiveTimeout(
	IN HNDAS NdasHandle, 
	OUT LPDWORD Timeout)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( IsValidWritePtr(Timeout, sizeof(DWORD)) );
	*Timeout = NdasHandle->ReceiveTimeout;
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetSendTimeout(
	IN HNDAS NdasHandle,
	IN DWORD Timeout)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	NdasHandle->SendTimeout = Timeout;
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetSendTimeout(
	IN HNDAS NdasHandle, 
	OUT LPDWORD Timeout)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( IsValidWritePtr(Timeout, sizeof(DWORD)) );
	*Timeout = NdasHandle->SendTimeout;
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetHostAddress(
	IN HNDAS NdasHandle,
	OUT PBYTE Buffer,
	IN OUT LPDWORD BufferLength)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( IsValidWritePtr(BufferLength, sizeof(DWORD)) );

	if (*BufferLength >= (DWORD)NdasHandle->HostSockAddress->iSockaddrLength)
	{
		CopyMemory(
			Buffer,
			NdasHandle->HostSockAddress->lpSockaddr,
			NdasHandle->HostSockAddress->iSockaddrLength);
	}

	*BufferLength = NdasHandle->HostSockAddress->iSockaddrLength;

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceID(
	__in_opt HNDAS NdasHandle,
	__in_opt CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	__out PBYTE pDeviceId,
	__out LPDWORD pUnitNo,
	__out LPBYTE VID)
{
	ASSERT_PARAM( (NdasHandle && NdasCommpIsValidNdasHandle(NdasHandle)) || (pConnectionInfo && NdasCommpIsValidNdasConnectionInfo(pConnectionInfo)));
	ASSERT_PARAM( !(NULL != NdasHandle && NULL != pConnectionInfo) );
	ASSERT_PARAM( IsValidWritePtr(pDeviceId, RTL_FIELD_SIZE(NDAS_DEVICE_ID, Node)) );
	ASSERT_PARAM( IsValidWritePtr(pUnitNo, sizeof(DWORD)) );
	ASSERT_PARAM( NULL == VID || IsValidWritePtr(VID, sizeof(BYTE)) );

	if (NdasHandle)
	{
		CopyMemory(
			pDeviceId, 
			NdasHandle->NdasDeviceId.Node, 
			sizeof(NdasHandle->NdasDeviceId.Node));

		*pUnitNo = (DWORD) NdasHandle->LspLoginInfo.unit_no;

		if (VID) 
		{
			*VID = NdasHandle->NdasDeviceId.VID;
		}
	}
	else
	{
		NDAS_UNITDEVICE_ID unitDeviceID;
		NDASID_EXT_DATA ndasIdExtension;

		BOOL fSuccess = NdasCommpConnectionInfoToDeviceID(
			pConnectionInfo, 
			&unitDeviceID,
			&ndasIdExtension);

		CopyMemory(
			pDeviceId, 
			unitDeviceID.DeviceId.Node,
			sizeof(unitDeviceID.DeviceId.Node));

		*pUnitNo = unitDeviceID.UnitNo;
		if (VID) *VID = ndasIdExtension.VID;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Functions, not for exporting
//
////////////////////////////////////////////////////////////////////////////////////////////////

FORCEINLINE
UINT64 
NdasCommpGetPassword(
	__in CONST BYTE* pAddress, 
	__in CONST NDASID_EXT_DATA* NdasIdExtension)
{
	_ASSERTE(IsValidReadPtr(pAddress, 6));

	if (NdasIdExtension && NDAS_VID_SEAGATE == NdasIdExtension->VID)
	{
		return NDAS_OEM_CODE_SEAGATE.UI64Value;
	}

	// password
	// if it's sample's address, use its password
	if (pAddress[0] == 0x00 &&
		pAddress[1] == 0xf0 &&
		pAddress[2] == 0x0f)
	{
		return NDAS_OEM_CODE_SAMPLE.UI64Value;
	}
#ifdef OEM_RUTTER
	else if (pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0 &&
		pAddress[3] & 0xFE == 0x20)
	{
		return NDAS_OEM_CODE_RUTTER.UI64Value;
	}
#endif // OEM_RUTTER
	else if (pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0)
	{
		return NDAS_OEM_CODE_DEFAULT.UI64Value;
	}
	else
	{
		//	default to XIMETA
		return NDAS_OEM_CODE_DEFAULT.UI64Value;
	}
}

FORCEINLINE
VOID
NdasCommpCleanupInitialLocks(HNDAS NdasHandle)
{
	for (DWORD i = 0; i < NDAS_DEVICE_LOCK_COUNT; ++i)
	{
		(VOID) NdasCommUnlockDevice(NdasHandle, i, NULL);
	}
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommLockDevice(
	IN HNDAS NdasHandle, 
	IN DWORD Index,
	OUT OPTIONAL LPDWORD LockCount)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( Index < 4 );
	ASSERT_PARAM( NULL == LockCount || IsValidWritePtr(LockCount, sizeof(DWORD)) );

	++(NdasHandle->LockCount[Index]);

	NDASCOMM_VCMD_PARAM param = {0};
	param.SET_SEMA.Index = static_cast<UINT8>(Index);
	
	BOOL fSuccess = NdasCommVendorCommand(
		NdasHandle, 
		ndascomm_vcmd_set_sema,
		&param,
		NULL, 0,
		NULL, 0);

	if (NULL != LockCount)
	{
		*LockCount = static_cast<DWORD>(param.SET_SEMA.SemaCounter);
	}

	return fSuccess;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUnlockDevice(
	IN HNDAS NdasHandle, 
	IN DWORD Index,
	OUT OPTIONAL LPDWORD LockCount)
{
	ASSERT_PARAM( NdasCommpIsValidNdasHandle(NdasHandle) );
	ASSERT_PARAM( Index < 4 );
	ASSERT_PARAM( NULL == LockCount || IsValidWritePtr(LockCount, sizeof(DWORD)) );

	if (0 == NdasHandle->LockCount[Index])
	{
		SetLastError(NDASCOMM_ERROR_INVALID_OPERATION);
		return FALSE;
	}

	--(NdasHandle->LockCount[Index]);

	if (FALSE && NdasHandle->LockCount[Index] > 0)
	{
		if (NULL != LockCount)
		{
			NDASCOMM_VCMD_PARAM param = {0};
			param.GET_SEMA.Index = static_cast<UINT8>(Index);
			BOOL fSuccess = NdasCommVendorCommand(
				NdasHandle, 
				ndascomm_vcmd_get_sema, 
				&param,
				NULL, 0,
				NULL, 0);
			if (fSuccess)
			{
				*LockCount = static_cast<DWORD>(param.GET_SEMA.SemaCounter);
			}
			else
			{
				*LockCount = 0;
			}
		}
		return TRUE;
	}
	else
	{
		NDASCOMM_VCMD_PARAM param = {0};
		param.FREE_SEMA.Index = static_cast<UINT8>(Index);

		BOOL fSuccess = NdasCommVendorCommand(
			NdasHandle, 
			ndascomm_vcmd_free_sema,
			&param,
			NULL, 0,
			NULL, 0);

		if (NULL != LockCount)
		{
			*LockCount = static_cast<DWORD>(param.FREE_SEMA.SemaCounter);
		}

		return fSuccess;
	}
}

#define NDAS_MAX_CONNECTION_V11 64

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommCleanupDeviceLocks(
	IN CONST NDASCOMM_CONNECTION_INFO* ConnInfo)
{
	HNDAS NdasHandles[NDAS_MAX_CONNECTION_V11];

	NDASCOMM_CONNECTION_INFO ci;
	CopyMemory(&ci, ConnInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ci.WriteAccess = FALSE;
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	// at least first connection should succeed
	NdasHandles[0] = NdasCommConnect(&ci);
	if (NULL == NdasHandles[0])
	{
		return FALSE;
	}

	// subsequent connection may fail
	for (DWORD i = 1; i < NDAS_MAX_CONNECTION_V11; ++i)
	{
		NdasHandles[i] = NdasCommConnect(&ci);
	}

	for (DWORD i = 0; i < NDAS_MAX_CONNECTION_V11; ++i)
	{
		// only connected handles will be attempted
		if (NULL != NdasHandles[i])
		{
			NdasCommpCleanupInitialLocks(NdasHandles[i]);
			XTLVERIFY( NdasCommDisconnect(NdasHandles[i]) );
		}
	}

	return TRUE;
}

FORCEINLINE
BOOL
NdasCommpIsValidNdasHandle(
	HNDAS NdasHandle)
{
	if (IsBadWritePtr(NdasHandle, sizeof(NDASCOMM_CONTEXT)))
	{
		return FALSE;
	}
	if (!NdasHandle->LspHandle)
	{
		return FALSE;
	}
	return TRUE;
}

FORCEINLINE
BOOL
NdasCommpIsValidNdasHandleForRW(
	HNDAS NdasHandle)
{
	if (!NdasCommpIsValidNdasHandle(NdasHandle))
	{
		return FALSE;
	}

	if (!NdasHandle->LspLoginInfo.write_access)
	{
		return FALSE;
	}

	return TRUE;
}

FORCEINLINE
BOOL
NdasCommpIsValidNdasConnectionInfo(
	CONST NDASCOMM_CONNECTION_INFO* pci)
{
	// process parameter
	ASSERT_PARAM( IsValidReadPtr(pci, sizeof(NDASCOMM_CONNECTION_INFO)));
	ASSERT_PARAM( sizeof(NDASCOMM_CONNECTION_INFO) == pci->Size );
	ASSERT_PARAM(
		NDASCOMM_CIT_NDAS_IDA == pci->AddressType ||		
		NDASCOMM_CIT_NDAS_IDW == pci->AddressType ||		
		NDASCOMM_CIT_DEVICE_ID == pci->AddressType ||
		NDASCOMM_CIT_SA_LPX == pci->AddressType ||
		NDASCOMM_CIT_SA_IN == pci->AddressType
		);

	ASSERT_PARAM(
		NDASCOMM_LOGIN_TYPE_NORMAL == pci->LoginType ||
		NDASCOMM_LOGIN_TYPE_DISCOVER == pci->LoginType);

	ASSERT_PARAM(0 == pci->UnitNo || 1 == pci->UnitNo);

	// Only LPX is available at this time
	ASSERT_PARAM(NDASCOMM_TRANSPORT_LPX == pci->Protocol);

	// A pointer to a socket address list is very prone to corrupting the memory
	const SOCKET_ADDRESS_LIST* sockAddrList = 
		reinterpret_cast<const SOCKET_ADDRESS_LIST*>(pci->BindingSocketAddressList);
	if (NULL != sockAddrList && !NdasCommpIsValidSocketAddressList(sockAddrList))
	{
		// Last Error is set by IsValidSocketAddressList
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Invalid Binding Socket Address List, error=0x%X\n",
			GetLastError());
		return FALSE;
	}
	return TRUE;
}

FORCEINLINE
BOOL
NdasCommpIsValidSocketAddressList(
	const SOCKET_ADDRESS_LIST* SocketAddressList)
{
	ASSERT_PARAM( IsValidReadPtr(SocketAddressList, sizeof(SOCKET_ADDRESS_LIST)) );
	ASSERT_PARAM( SocketAddressList->iAddressCount > 0 );
	ASSERT_PARAM( IsValidReadPtr(SocketAddressList, 
		sizeof(SOCKET_ADDRESS_LIST) + 
		sizeof(SOCKET_ADDRESS) * (SocketAddressList->iAddressCount - 1)) );

	for (int i = 0; i < SocketAddressList->iAddressCount; ++i)
	{
		ASSERT_PARAM( IsValidReadPtr(
			SocketAddressList->Address[i].lpSockaddr,
			SocketAddressList->Address[i].iSockaddrLength) );
	}
	return TRUE;
}

BOOL
NdasCommpConnectionInfoToDeviceID(
	__in CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	__out PNDAS_UNITDEVICE_ID pUnitDeviceID,
	__out_opt NDASID_EXT_DATA* NdasIdExtentionData)
{
	BOOL success;

	ASSERT_PARAM( IsValidReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM( sizeof(NDASCOMM_CONNECTION_INFO) == pConnectionInfo->Size );
	ASSERT_PARAM( 
		NDASCOMM_CIT_SA_LPX == pConnectionInfo->AddressType ||
		NDASCOMM_CIT_SA_IN == pConnectionInfo->AddressType  ||
		NDASCOMM_CIT_NDAS_IDA == pConnectionInfo->AddressType  ||
		NDASCOMM_CIT_NDAS_IDW == pConnectionInfo->AddressType  ||
		NDASCOMM_CIT_DEVICE_ID == pConnectionInfo->AddressType);
	ASSERT_PARAM(0 == pConnectionInfo->UnitNo || 1 == pConnectionInfo->UnitNo);
	ASSERT_PARAM(NDASCOMM_TRANSPORT_LPX == pConnectionInfo->Protocol);
	ASSERT_PARAM( IsValidWritePtr(pUnitDeviceID, sizeof(NDAS_UNITDEVICE_ID)) );

	switch(pConnectionInfo->AddressType)
	{
	case NDASCOMM_CIT_DEVICE_ID:
		{
			C_ASSERT(sizeof(pUnitDeviceID->DeviceId.Node) ==
				sizeof(pConnectionInfo->Address.DeviceId.Node));

			CopyMemory(
				pUnitDeviceID->DeviceId.Node, 
				pConnectionInfo->Address.DeviceId.Node, 
				sizeof(pUnitDeviceID->DeviceId.Node));
			NdasIdExtentionData->VID = pConnectionInfo->Address.DeviceId.VID;
		}
		break;
	case NDASCOMM_CIT_NDAS_IDW:
		{
			WCHAR wszID[20 +1] = {0};
			WCHAR wszKey[5 +1] = {0};

			CopyMemory(wszID, pConnectionInfo->Address.NdasIdW.Id, sizeof(WCHAR) * 20);
			CopyMemory(wszKey, pConnectionInfo->Address.NdasIdW.Key, sizeof(WCHAR) * 5);

			if (pConnectionInfo->WriteAccess)
			{
				// ***** is a magic write key
				if (0 == lstrcmpW(wszKey, L"*****"))
				{
					if (!NdasIdValidateW(wszID, NULL))
					{
						SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
						return FALSE;
					}
				}
				else
				{
					if (!NdasIdValidateW(wszID, wszKey))
					{
						SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
						return FALSE;
					}
				}
			}
			else
			{
				if (!NdasIdValidateW(wszID, NULL))
				{
					SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
					return FALSE;
				}
			}

			success = NdasIdStringToDeviceExW(
				wszID, 
				&pUnitDeviceID->DeviceId, 
				NULL, 
				NdasIdExtentionData);

			if (!success)
			{
				SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
				return FALSE;
			}
		}
		break;
	case NDASCOMM_CIT_NDAS_IDA:
		{
			CHAR szID[20 +1] = {0};
			CHAR szKey[5 +1] = {0};

			CopyMemory(szID, pConnectionInfo->Address.NdasIdA.Id, 20);
			CopyMemory(szKey, pConnectionInfo->Address.NdasIdA.Key, 5);

			if (pConnectionInfo->WriteAccess)
			{
				// Workaround:
				// There is no way to obtain the write key from the 
				// NDAS Service, neither there is a public API to generate
				// a write key. To assist the RW connection only
				// with the NDAS ID, we would use a magic write key
				// which can be a write key for all connections,
				// only for using NdasComm API.
				// "*****" will be a magic write key for the time being.
				// This behavior should be changed in the future releases.
				//
				if (0 == lstrcmpA(szKey, "*****"))
				{
					success = NdasIdValidateA(szID, NULL);
					if (!success)
					{
						SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
						return FALSE;
					}
				}
				else
				{
					success = NdasIdValidateA(szID, szKey);
					if (!success)
					{
						SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
						return FALSE;
					}
				}
			}
			else
			{
				success = NdasIdValidateA(szID, NULL);
				if (!success)
				{
					SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
					return FALSE;
				}
			}

			success = NdasIdStringToDeviceExA(
				szID, 
				&pUnitDeviceID->DeviceId, 
				NULL, 
				NdasIdExtentionData);

			if (!success)
			{
				SetLastError(NDASCOMM_ERROR_INVALID_PARAMETER);
				return FALSE;
			}
		}
		break;
	default:
		ASSERT_PARAM(FALSE);
		break;
	}

	pUnitDeviceID->UnitNo = pConnectionInfo->UnitNo;

	return TRUE;
}