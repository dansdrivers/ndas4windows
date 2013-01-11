// ndasuser.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "ndas/ndasmsg.h"

#include "ndas/ndascmd.h"
#include "ndas/ndastypeex.h"

#include "ndas/ndasid.h"
#include "ndas/ndasuser.h"

#include "ndasscc.h"
#include "autores.h"

#define XDBG_FILENAME "ndasuser.cpp"
#include "xdebug.h"

NDASUSER_LINKAGE
DWORD
NDASUSERAPI
NdasGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDASUSER_API_VERSION_MAJOR, 
		NDASUSER_API_VERSION_MINOR);
}

inline 
void 
SetErrorStatus(NDAS_CMD_STATUS status, const NDAS_CMD_ERROR::REPLY* lpErrorReply)
{
	switch (status) {
	case NDAS_CMD_STATUS_FAILED:
		::SetLastError(lpErrorReply->dwErrorCode);
		break;
	case NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED:
		::SetLastError(NDASUSER_ERROR_SERVICE_RETURNED_NOT_IMPLEMENTED);
		break;
	case NDAS_CMD_STATUS_INVALID_REQUEST:
		::SetLastError(NDASUSER_ERROR_SERVICE_RETURNED_INVALID_REQUEST);
		break;
	case NDAS_CMD_STATUS_TERMINATION:
		::SetLastError(NDASUSER_ERROR_SERVICE_RETURNED_TERMINATION);
		break;
	case NDAS_CMD_STATUS_UNSUPPORTED_VERSION:
		::SetLastError(NDASUSER_ERROR_SERVICE_RETURNED_UNSUPPORTED_VERSION);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// MACROS for Connections
//
// Note:
//
// 
//////////////////////////////////////////////////////////////////////////

#define NDAS_DEFINE_TYPES(x,lpRequest,lpReply,lpErrorReply,scc) \
	typedef x COMMAND; \
	COMMAND::REQUEST buffer; \
	COMMAND::REQUEST* lpRequest = &buffer; \
	COMMAND::REPLY* lpReply; \
	NDAS_CMD_ERROR::REPLY* lpErrorReply; \
	CNdasServiceCommandClient<COMMAND> scc; /* scc is a managed resource */ \
	do { ; } while(0)

#define NDAS_CONNECT(scc) \
	do { \
	BOOL fSuccess = scc.Connect(); \
	if (!fSuccess) \
	{ \
		::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE); \
		return FALSE; \
	} \
	} while(0)

#define NDAS_TRANSACT(cbRequestExtraValue,cbReplyExtra,dwCmdStatus) \
	DWORD cbRequestExtra(cbRequestExtraValue); \
	DWORD cbReplyExtra, cbErrorExtra; \
	DWORD dwCmdStatus; \
	{ \
	BOOL fSuccess = scc.Transact( \
		lpRequest, cbRequestExtra, \
		&lpReply, &cbReplyExtra, \
		&lpErrorReply, &cbErrorExtra, \
		&dwCmdStatus); \
	if (!fSuccess) { \
		DBGPRT_ERR_EX(_FT("Transact failed: ")); \
		return FALSE; \
	} \
	DBGPRT_INFO(_FT("Transact completed successfully.\n")); \
	NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); \
	if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) { \
		DBGPRT_ERR(_FT("Command returned failure!\n")); \
		SetErrorStatus(cmdStatus, lpErrorReply); \
		return FALSE; \
	} \
	} \
	do {;} while(0)

#define CHECK_STR_PTRW(ptr,len) \
	do { if (::IsBadStringPtrW(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_STR_PTRA(ptr,len) \
	do { if (::IsBadStringPtrA(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_STR_PTRW_NULLABLE(ptr,len) \
	do { if (ptr != NULL && ::IsBadStringPtrW(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_STR_PTRA_NULLABLE(ptr,len) \
	do { if (NULL != ptr && ::IsBadStringPtrA(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_READ_PTR(ptr,len) \
	do { if (::IsBadReadPtr(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_READ_PTR_NULLABLE(ptr, len) \
	do { if (NULL != ptr && ::IsBadReadPtr(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_WRITE_PTR(ptr,len) \
	do { if (::IsBadWritePtr(ptr, len)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define CHECK_PROC_PTR(proc) \
	do { if (::IsBadCodePtr(reinterpret_cast<FARPROC>(proc))) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	} \
	} while(0)

#define NDAS_VALIDATE_DEVICE_STRING_ID(lpszDeviceStringId) \
	do { \
	BOOL fSuccess = NdasIdValidateW(lpszDeviceStringId, NULL); \
	if (!fSuccess) { \
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID); \
		return FALSE; \
	} \
	} while(0)

#define NDAS_VALIDATE_DEVICE_STRING_IDA(lpszDeviceStringId) \
	do { \
	BOOL fSuccess = NdasIdValidateA(lpszDeviceStringId, NULL); \
	if (!fSuccess) { \
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID); \
		return FALSE; \
	} \
	} while(0)

#define NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is) \
	NDAS_DEVICE_ID_OR_SLOT is; \
	is.bUseSlotNo = TRUE; \
	is.SlotNo = dwSlotNo; \
	do { ; } while(0)

#define NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is) \
	CHECK_STR_PTRW(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN); \
	NDAS_VALIDATE_DEVICE_STRING_ID(lpszDeviceStringId); \
	NDAS_DEVICE_ID_OR_SLOT is; \
	is.bUseSlotNo = FALSE; \
	BOOL fSuccess = NdasIdStringToDeviceW(lpszDeviceStringId,&is.DeviceId); \
	_ASSERT(fSuccess); /*  should not fail as we already did Validation */ \
	do { ; } while(0)

#define NDAS_FORWARD_DEVICE_STRING_IDA(lpszDeviceStringId, is) \
	CHECK_STR_PTRA(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN); \
	NDAS_VALIDATE_DEVICE_STRING_IDA(lpszDeviceStringId); \
	NDAS_DEVICE_ID_OR_SLOT is; \
	is.bUseSlotNo = FALSE; \
	BOOL fSuccess = NdasIdStringToDeviceA(lpszDeviceStringId,&is.DeviceId); \
	_ASSERT(fSuccess); /* should not fail as we already did Validation */ \
	do { ; } while(0)

//////////////////////////////////////////////////////////////////////////
//
// Validate String Id and Key
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasValidateStringIdKeyW(
	LPCWSTR lpszDeviceStringId, 
	LPCWSTR lpszDeviceStringKey)
{
	//
	// pointer validation check - ALWAYS DO THIS FOR API CALLS
	// lpszDeviceStringKey can be NULL
	//

	CHECK_STR_PTRW(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	CHECK_STR_PTRW_NULLABLE(lpszDeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);

	BOOL bWritable;
	BOOL fSuccess = NdasIdValidateW(lpszDeviceStringId, lpszDeviceStringKey);

	if (!fSuccess) {
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID_OR_KEY);
		return FALSE;
	}

	return TRUE;

}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasValidateStringIdKeyA(
	LPCSTR lpszDeviceStringId, 
	LPCSTR lpszDeviceStringKey)
{
	//
	// pointer validation check - ALWAYS DO THIS FOR API CALLS
	// lpszDeviceStringKey can be NULL
	//

	CHECK_STR_PTRA(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	CHECK_STR_PTRA_NULLABLE(lpszDeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);

	BOOL bWritable;
	BOOL fSuccess = NdasIdValidateA(lpszDeviceStringId, lpszDeviceStringKey);

	if (!fSuccess) {
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID_OR_KEY);
		return FALSE;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Register Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE 
DWORD
NDASUSERAPI
NdasRegisterDeviceW(
	LPCWSTR lpszDeviceStringId, 
	LPCWSTR lpszDeviceStringKey,
	LPCWSTR lpszDeviceName,
	DWORD dwFlags)
{
	// ptr sanity check
	CHECK_STR_PTRW(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	CHECK_STR_PTRW_NULLABLE(lpszDeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);
	CHECK_STR_PTRW(lpszDeviceName, -1);

	NDAS_CMD_REGISTER_DEVICE::REQUEST buffer; 
	NDAS_CMD_REGISTER_DEVICE::REQUEST* lpRequest = &buffer; 
	NDAS_CMD_REGISTER_DEVICE::REPLY* lpReply; 
	NDAS_CMD_ERROR::REPLY* lpErrorReply; 

	// check ndas id
	BOOL fSuccess = NdasIdValidateW(lpszDeviceStringId, lpszDeviceStringKey);

	if (!fSuccess) {
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
		return 0;
	}
	// Prepare the parameters here
	lpRequest->GrantingAccess = 
		(lpszDeviceStringKey) ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;

	lpRequest->RegFlags = dwFlags;

	fSuccess = NdasIdStringToDeviceW(
		lpszDeviceStringId, 
		&lpRequest->DeviceId);

	_ASSERT(fSuccess); // must not fail as we already did Validation

	HRESULT hr = ::StringCchCopyW(
		lpRequest->wszDeviceName,
		MAX_NDAS_DEVICE_NAME_LEN + 1,
		lpszDeviceName);

	if (!(SUCCEEDED(hr) || (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER == hr)))
	{
		return 0;
	}

	// ready to connect
	CNdasServiceCommandClient<NDAS_CMD_REGISTER_DEVICE> scc;

	BOOL fConnected = scc.Connect();
	if (!fConnected) {
		XTRACE(TEXT("Service Connection Failure with error %d\n"), ::GetLastError());
		::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE); 
		return 0; 
	} 

	DWORD cbRequestExtra(0); 
	DWORD cbReplyExtra, cbErrorExtra; 
	DWORD dwCmdStatus; 
	{ 
		BOOL fSuccess = scc.Transact(
			lpRequest, 
			cbRequestExtra, 
			&lpReply, 
			&cbReplyExtra, 
			&lpErrorReply, 
			&cbErrorExtra, 
			&dwCmdStatus); 

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Transact failed: "));
			return FALSE; 
		}

		DBGPRT_INFO(_FT("Transact completed successfully.\n"));

		NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); 
		if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) { 
			DBGPRT_ERR(_FT("Command failed!\n")); 
			SetErrorStatus(cmdStatus, lpErrorReply); 
			return 0; 
		} 
	}

	// Process the result here
	return lpReply->SlotNo;
}

//////////////////////////////////////////////////////////////////////////
//
// Get Registration Data
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetRegistrationData(
	 IN	DWORD dwSlotNo,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// Set Registration Data
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationData(
	 IN	DWORD dwSlotNo,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 IN LPVOID lpBuffer,
	 IN DWORD cbBuffer)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 OUT LPCVOID lpBuffer,
	 IN DWORD cbBuffer)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// Unregister Device
//
//////////////////////////////////////////////////////////////////////////

BOOL
NdasUnregisterDeviceImpl(NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_UNREGISTER_DEVICE,lpRequest,lpReply,lpErrorReply,scc);
	NDAS_CONNECT(scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;

	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process the result here
	// None

	return TRUE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasUnregisterDevice(DWORD dwSlotNo)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasUnregisterDeviceImpl(is);
}

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasUnregisterDeviceByIdW(LPCWSTR lpszDeviceStringId)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasUnregisterDeviceImpl(is);
}

//////////////////////////////////////////////////////////////////////////
//
// Enumerate Devices
//
//////////////////////////////////////////////////////////////////////////

//
// lpFnumFunc:
//	[in] Pointer to an application-defined EnumNdasDeviceProc callback function
// lpContext:
//  [in] Application-defined value to be passed to the callback function
//

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasEnumDevicesW(NDASDEVICEENUMPROCW lpEnumFunc, LPVOID lpContext)
{
	CHECK_PROC_PTR(lpEnumFunc);

	NDAS_CMD_ENUMERATE_DEVICES::REQUEST buffer; 
	NDAS_CMD_ENUMERATE_DEVICES::REQUEST* lpRequest = &buffer; 
	NDAS_CMD_ENUMERATE_DEVICES::REPLY* lpReply; 
	NDAS_CMD_ERROR::REPLY* lpErrorReply; 

	CNdasServiceCommandClient<NDAS_CMD_ENUMERATE_DEVICES> scc;
	BOOL fConnected = scc.Connect();
	if (!fConnected) {
		DBGPRT_ERR_EX(_FT("Service Connection failed: "));
		::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE); 
		return FALSE; 
	} 
	
	DWORD cbRequestExtra(0); 
	DWORD cbReplyExtra, cbErrorExtra; 
	DWORD dwCmdStatus; 
	
	BOOL fSuccess = scc.Transact(lpRequest, cbRequestExtra, &lpReply, &cbReplyExtra, &lpErrorReply, &cbErrorExtra, &dwCmdStatus); 
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Transact failed: ")); 
		return FALSE; 
	}

	NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); 
	if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) { 
		DBGPRT_ERR(_FT("Command failed!\n")); 
		SetErrorStatus(cmdStatus, lpErrorReply); 
		return FALSE; 
	} 

	// Process the result here

	BOOL bCont(TRUE);
	if (lpEnumFunc) {
		for (DWORD i = 0; i < lpReply->nDeviceEntries && bCont; ++i) {
			NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY* pDeviceEntry = &lpReply->DeviceEntry[i];
			NDASUSER_DEVICE_ENUM_ENTRYW userEntry;
			userEntry.GrantedAccess = pDeviceEntry->GrantedAccess;
			userEntry.SlotNo = pDeviceEntry->SlotNo;
			HRESULT hr = ::StringCchCopyNW(
				userEntry.szDeviceName, MAX_NDAS_DEVICE_NAME_LEN + 1,
				pDeviceEntry->wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN);
			_ASSERT(SUCCEEDED(hr));
			BOOL fSuccess = NdasIdDeviceToStringW(
				&pDeviceEntry->DeviceId, userEntry.szDeviceStringId, NULL);
			_ASSERT(fSuccess);
			bCont = lpEnumFunc(&userEntry, lpContext);
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Enable Device
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasEnableDeviceImpl(NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, BOOL bEnable)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_SET_DEVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);
	NDAS_CONNECT(scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	lpRequest->Param.ParamType = NDAS_CMD_SET_DEVICE_PARAM_TYPE_ENABLE;
	lpRequest->Param.bEnable = bEnable;

	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnableDevice(DWORD dwSlotNo, BOOL bEnable)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasEnableDeviceImpl(is, bEnable);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnableDeviceByIdW(LPCWSTR lpszDeviceStringId, BOOL bEnable)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasEnableDeviceImpl(is, bEnable);
}

//////////////////////////////////////////////////////////////////////////
//
// Set Device Name
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasSetDeviceNameImplW(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, 
	LPCWSTR lpszDeviceName)
{
	CHECK_STR_PTRW(lpszDeviceName, -1);
	NDAS_DEFINE_TYPES(NDAS_CMD_SET_DEVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	lpRequest->Param.ParamType = NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME;
	HRESULT hr = ::StringCchCopyNW(
		lpRequest->Param.wszName, MAX_NDAS_DEVICE_NAME_LEN + 1, 
		lpszDeviceName, MAX_NDAS_DEVICE_NAME_LEN);
	_ASSERT(SUCCEEDED(hr));

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}


NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameW(
	DWORD dwSlotNo, 
	LPCWSTR lpszDeviceName)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasSetDeviceNameImplW(is, lpszDeviceName);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameByIdW(
	LPCWSTR lpszDeviceStringId, 
	LPCWSTR lpszDeviceName)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasSetDeviceNameImplW(is, lpszDeviceName);
}

//////////////////////////////////////////////////////////////////////////
//
// Set Device Access
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceAccessByIdW(
	LPCWSTR lpszDeviceStringId, 
	BOOL bWriteAccess, 
	LPCWSTR lpszDeviceStringKey)
{
	CHECK_STR_PTRW(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	CHECK_STR_PTRW_NULLABLE(lpszDeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);
	
	NDAS_DEFINE_TYPES(NDAS_CMD_SET_DEVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);

	if (bWriteAccess) {
		// check ndas id
		BOOL fSuccess = NdasIdValidateW(lpszDeviceStringId, lpszDeviceStringKey);
		if (!fSuccess) {
			::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
			return FALSE;
		}
	} else {
		NDAS_VALIDATE_DEVICE_STRING_ID(lpszDeviceStringId);
	}

	NDAS_CONNECT(scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot.bUseSlotNo = FALSE;
	BOOL fSuccess = NdasIdStringToDeviceW(
		lpszDeviceStringId,
		&lpRequest->DeviceIdOrSlot.DeviceId);
	_ASSERT(fSuccess); // should not fail as we already did Validation

	lpRequest->Param.ParamType = NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS;
	lpRequest->Param.GrantingAccess = bWriteAccess ? 
		(GENERIC_READ | GENERIC_WRITE) : (GENERIC_READ);

	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Query Device Status
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasQueryDeviceStatusImpl(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, 
	NDAS_DEVICE_STATUS *pStatus,
	NDAS_DEVICE_ERROR *pLastError)
{
	CHECK_WRITE_PTR(pStatus, sizeof(NDAS_DEVICE_STATUS));
	CHECK_WRITE_PTR(pLastError, sizeof(NDAS_DEVICE_ERROR));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_DEVICE_STATUS,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here
	*pStatus = lpReply->Status;
	*pLastError = lpReply->LastError;

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceStatus(
	DWORD dwSlotNo, 
	NDAS_DEVICE_STATUS *pStatus,
	NDAS_DEVICE_ERROR* pLastError)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo, is);
	return NdasQueryDeviceStatusImpl(is, pStatus, pLastError);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceStatusByIdW(
	LPCWSTR lpszDeviceStringId, 
	NDAS_DEVICE_STATUS* pStatus,
	NDAS_DEVICE_ERROR* pLastError)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasQueryDeviceStatusImpl(is, pStatus, pLastError);
}

//////////////////////////////////////////////////////////////////////////
//
// Query Device Information
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasQueryDeviceInformationImplW(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, NDASUSER_DEVICE_INFORMATIONW* pDevInfo)
{
	CHECK_WRITE_PTR(pDevInfo, sizeof(NDASUSER_DEVICE_INFORMATIONW));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_DEVICE_INFORMATION,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	pDevInfo->GrantedAccess = lpReply->GrantedAccess;
	HRESULT hr = ::StringCchCopy(
		pDevInfo->szDeviceName, 
		MAX_NDAS_DEVICE_NAME_LEN + 1, 
		lpReply->wszDeviceName);
	_ASSERT(SUCCEEDED(hr));

	pDevInfo->SlotNo = lpReply->SlotNo;
	BOOL fSuccess = NdasIdDeviceToStringW(
		&lpReply->DeviceId, pDevInfo->szDeviceId, NULL);
	_ASSERT(fSuccess);

	::CopyMemory(
		&pDevInfo->HardwareInfo, 
		&lpReply->HardwareInfo, 
		sizeof(NDAS_DEVICE_HW_INFORMATION));

	::CopyMemory(
		&pDevInfo->DeviceParams,
		&lpReply->DeviceParams,
		sizeof(NDAS_DEVICE_PARAMS));

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceInformationW(
	DWORD dwSlotNo, NDASUSER_DEVICE_INFORMATIONW* pDevInfo)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasQueryDeviceInformationImplW(is, pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceInformationByIdW(
	LPCWSTR lpszDeviceStringId, NDASUSER_DEVICE_INFORMATIONW* pDevInfo)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasQueryDeviceInformationImplW(is, pDevInfo);
}

//////////////////////////////////////////////////////////////////////////
//
// Enumerate Unit Device
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasEnumUnitDevicesImplW(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, 
	NDASUNITDEVICEENUMPROC lpEnumProc, LPVOID lpContext)
{
	CHECK_PROC_PTR(lpEnumProc);
	NDAS_DEFINE_TYPES(NDAS_CMD_ENUMERATE_UNITDEVICES,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process the result here
	BOOL bCont(TRUE);
	if (lpEnumProc) {
		for (DWORD i = 0; i < lpReply->nUnitDeviceEntries && bCont; ++i) {
			NDASUSER_UNITDEVICE_ENUM_ENTRY userEntry;
			userEntry.UnitNo = lpReply->UnitDeviceEntries[i].UnitNo;
			userEntry.UnitDeviceType = lpReply->UnitDeviceEntries[i].UnitDeviceType;
			bCont = lpEnumProc(&userEntry, lpContext);
		}
	}

	return TRUE;
}


NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnumUnitDevices(
	DWORD dwSlotNo, 
	NDASUNITDEVICEENUMPROC lpEnumProc, LPVOID lpContext)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasEnumUnitDevicesImplW(is, lpEnumProc, lpContext);
}


NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnumUnitDevicesByIdW(
	LPCWSTR lpszDeviceStringId, 
	NDASUNITDEVICEENUMPROC lpEnumProc, LPVOID lpContext)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasEnumUnitDevicesImplW(is, lpEnumProc, lpContext);
}


//////////////////////////////////////////////////////////////////////////
//
// Query Unit Device Status
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasQueryUnitDeviceStatusImpl(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError)
{
	CHECK_WRITE_PTR(pStatus, sizeof(NDAS_UNITDEVICE_STATUS));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_UNITDEVICE_STATUS,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	lpRequest->UnitNo = dwUnitNo;
	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here
	*pStatus = lpReply->Status;
	*pLastError = lpReply->LastError;

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceStatus(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_STATUS* pLastError)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasQueryUnitDeviceStatusImpl(is, dwUnitNo, pStatus, pLastError);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceStatusByIdW(
	LPCWSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasQueryUnitDeviceStatusImpl(is, dwUnitNo, pStatus, pLastError);
}

//////////////////////////////////////////////////////////////////////////
//
// Query Unit Device Information
//
//////////////////////////////////////////////////////////////////////////

template <typename TW, typename TA>
BOOL
WideToMbcs(TA* pa, const TW* pw);

template <>
BOOL
WideToMbcs<
NDAS_UNITDEVICE_HW_INFORMATIONW, 
NDAS_UNITDEVICE_HW_INFORMATIONA >(
	NDAS_UNITDEVICE_HW_INFORMATIONA* pDataA,
	const NDAS_UNITDEVICE_HW_INFORMATIONW* pDataW)
{
	pDataA->bLBA = pDataW->bLBA;
	pDataA->bLBA48 = pDataW->bLBA48;
	pDataA->nROHosts = pDataW->nROHosts;
	pDataA->nRWHosts = pDataW->nRWHosts;

	pDataA->bPIO = pDataW->bPIO;
	pDataA->bDMA = pDataW->bDMA;
	pDataA->bUDMA = pDataW->bUDMA;
	pDataA->MediaType = pDataW->MediaType;

	int iConverted;

	iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, 
		pDataW->szModel, 41, 
		pDataA->szModel, 41, NULL, NULL);
	_ASSERTE(iConverted == 41);

	iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, 
		pDataW->szFwRev, 9, 
		pDataA->szFwRev, 9, NULL, NULL);
	_ASSERTE(iConverted == 9);

	iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, 
		pDataW->szSerialNo, 41, 
		pDataA->szSerialNo, 41, NULL, NULL);
	_ASSERTE(iConverted == 41);

	pDataA->SectorCountLowPart = pDataW->SectorCountLowPart;
	pDataA->SectorCountHighPart = pDataW->SectorCountHighPart;

	return TRUE;
}

BOOL 
NdasQueryUnitDeviceInformationImplW(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo)
{
	CHECK_WRITE_PTR(pDevInfo, sizeof(NDASUSER_UNITDEVICE_INFORMATIONW));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_UNITDEVICE_INFORMATION,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	lpRequest->UnitNo = dwUnitNo;
	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here
	pDevInfo->UnitDeviceType = lpReply->UnitDeviceType;
	pDevInfo->UnitDeviceSubType = lpReply->UnitDeviceSubType;

	::CopyMemory(
		&pDevInfo->HardwareInfo, &lpReply->HardwareInfo,
		sizeof(NDAS_UNITDEVICE_HW_INFORMATIONW));

	::CopyMemory(
		&pDevInfo->UnitDeviceParams, &lpReply->UnitDeviceParams,
		sizeof(NDAS_UNITDEVICE_PARAMS));

	return TRUE;
}

BOOL 
NdasQueryUnitDeviceInformationImplA(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo)
{
	NDASUSER_UNITDEVICE_INFORMATIONW DevInfoW = {0};
	BOOL fSuccess = NdasQueryUnitDeviceInformationImplW(DeviceIdOrSlot, dwUnitNo, &DevInfoW);
	if (!fSuccess) {
		return FALSE;
	}

	// Convert NDASUSER_UNITDEVICE_INFORMATIONW to
	// NDASUSER_UNITDEVICE_INFORMATIONA
	pDevInfo->UnitDeviceType = DevInfoW.UnitDeviceType;
	pDevInfo->UnitDeviceSubType = DevInfoW.UnitDeviceSubType;

	WideToMbcs(&pDevInfo->HardwareInfo, &DevInfoW.HardwareInfo);

	::CopyMemory(
		&pDevInfo->UnitDeviceParams, 
		&DevInfoW.UnitDeviceParams,
		sizeof(NDAS_UNITDEVICE_PARAMS));

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationW(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasQueryUnitDeviceInformationImplW(is, dwUnitNo, pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationA(
	DWORD dwSlotNo, DWORD dwUnitNo,
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasQueryUnitDeviceInformationImplA(is, dwUnitNo, pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasQueryUnitDeviceInformationImplW(is, dwUnitNo, pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo)
{
	NDAS_FORWARD_DEVICE_STRING_IDA(lpszDeviceStringId, is);
	return NdasQueryUnitDeviceInformationImplA(is, dwUnitNo, pDevInfo);
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceHostStats(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	LPDWORD lpnROHosts, LPDWORD lpnRWHosts)
{
	CHECK_WRITE_PTR(lpnROHosts, sizeof(DWORD));
	CHECK_WRITE_PTR(lpnRWHosts, sizeof(DWORD));

	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot.bUseSlotNo = TRUE;
	lpRequest->DeviceIdOrSlot.SlotNo = dwSlotNo;
	lpRequest->UnitNo = dwUnitNo;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here

	*lpnROHosts = lpReply->nROHosts;
	*lpnRWHosts = lpReply->nRWHosts;

	return TRUE;

}

//////////////////////////////////////////////////////////////////////////
//
// Find Logical Device of Unit Device
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasFindLogicalDeviceOfUnitDeviceImpl(
	NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	CHECK_WRITE_PTR(pLogicalDeviceId, sizeof(NDAS_LOGICALDEVICE_ID));

	NDAS_DEFINE_TYPES(NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	lpRequest->UnitNo = dwUnitNo;
	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here
	*pLogicalDeviceId = lpReply->LogicalDeviceId;

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDevice(
	DWORD dwSlotNo, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasFindLogicalDeviceOfUnitDeviceImpl(
		is, 
		dwUnitNo, 
		pLogicalDeviceId);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is);
	return NdasFindLogicalDeviceOfUnitDeviceImpl(
		is, 
		dwUnitNo, 
		pLogicalDeviceId);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	NDAS_FORWARD_DEVICE_STRING_IDA(lpszDeviceStringId, is);
	return NdasFindLogicalDeviceOfUnitDeviceImpl(
		is, 
		dwUnitNo, 
		pLogicalDeviceId);
}

//////////////////////////////////////////////////////////////////////////
//
// Plug In Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasPlugInLogicalDevice(
	BOOL bWritable, 
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_PLUGIN_LOGICALDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->LogicalDeviceId = logicalDeviceId;
	lpRequest->Access = bWritable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process the result here
	// None.

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Eject Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEjectLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_EJECT_LOGICALDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);
	
	// Process the result here
	// None.

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Unplug Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasUnplugLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_UNPLUG_LOGICALDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);
	// Process the result here
	// None.

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Recover Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasRecoverLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_RECOVER_LOGICALDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process the result here
	// None.

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Enumerate Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasEnumLogicalDevices(
	NDASLOGICALDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext)
{
	CHECK_PROC_PTR(lpEnumProc);

	NDAS_DEFINE_TYPES(NDAS_CMD_ENUMERATE_LOGICALDEVICES,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	// None.

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process the result here
	BOOL bCont(TRUE);
	if (lpEnumProc) {
		for (DWORD i = 0; i < lpReply->nLogicalDeviceEntries && bCont; ++i) {

			NDASUSER_LOGICALDEVICE_ENUM_ENTRY userEntry;
			NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY* 
				pCmdEntry = &lpReply->LogicalDeviceEntries[i];

			userEntry.LogicalDeviceId = pCmdEntry->LogicalDeviceId;
			userEntry.Type = pCmdEntry->LogicalDeviceType;

			bCont = lpEnumProc(&userEntry, lpContext);
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Query Logical Device Status
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryLogicalDeviceStatus(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	NDAS_LOGICALDEVICE_STATUS* pStatus,
	NDAS_LOGICALDEVICE_ERROR* pLastError)
{
	CHECK_WRITE_PTR(pStatus, sizeof(NDAS_LOGICALDEVICE_STATUS));
	CHECK_WRITE_PTR(pLastError, sizeof(NDAS_LOGICALDEVICE_ERROR));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_LOGICALDEVICE_STATUS,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here
	*pStatus = lpReply->Status;
	*pLastError = lpReply->LastError;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Query Logical Device Information
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryLogicalDeviceInformationW(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	PNDASUSER_LOGICALDEVICE_INFORMATIONW pLogDevInfo)
{
	CHECK_WRITE_PTR(pLogDevInfo, sizeof(NDASUSER_LOGICALDEVICE_INFORMATIONW));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// reply
//	lpReply->LogicalDeviceType;
//	lpReply->GrantedAccess;
//	lpReply->MountedAccess;
//	lpReply->nUnitDeviceEntries;
//	lpReply->LogicalDiskInfo.Blocks;
//	lpReply->LogicalDiskInfo.LogicalDiskType;

	pLogDevInfo->LogicalDeviceType = lpReply->LogicalDeviceType;
	pLogDevInfo->GrantedAccess = lpReply->GrantedAccess;
	pLogDevInfo->MountedAccess = lpReply->MountedAccess;

	if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(pLogDevInfo->LogicalDeviceType)) {
		pLogDevInfo->LogicalDiskInformation.Blocks = 
			lpReply->LogicalDiskInfo.Blocks;
	} else if (IS_NDAS_LOGICALDEVICE_TYPE_DVD_GROUP(pLogDevInfo->LogicalDeviceType)) {
	}

	pLogDevInfo->nUnitDeviceEntries = lpReply->nUnitDeviceEntries;

	if (pLogDevInfo->nUnitDeviceEntries > 0) {

		pLogDevInfo->FirstUnitDevice.Index = 0;
		pLogDevInfo->FirstUnitDevice.UnitNo = 
			lpReply->UnitDeviceEntries[0].UnitNo;

		BOOL fSuccess = NdasIdDeviceToStringW(
			&lpReply->UnitDeviceEntries[0].DeviceId, 
			pLogDevInfo->FirstUnitDevice.szDeviceStringId,
			NULL);

		_ASSERT(fSuccess);
	}

	::CopyMemory(
		&pLogDevInfo->LogicalDeviceParams,
		&lpReply->LogicalDeviceParams,
		sizeof(NDAS_LOGICALDEVICE_PARAMS));

	return TRUE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEnumLogicalDeviceMembersW(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	NDASLOGICALDEVICEMEMBERENUMPROCW lpEnumProc,
	LPVOID lpContext)
{
	CHECK_PROC_PTR(lpEnumProc);
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	for (DWORD i = 0; i < lpReply->nUnitDeviceEntries; ++i) {

		NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW userEntry = {0};

		userEntry.Index = i;
		
		BOOL fSuccess = NdasIdDeviceToStringW(
			&lpReply->UnitDeviceEntries[i].DeviceId, 
			userEntry.szDeviceStringId,
			NULL);

		_ASSERT(fSuccess);

		userEntry.UnitNo = lpReply->UnitDeviceEntries[i].UnitNo;

		BOOL bCont = lpEnumProc(&userEntry, lpContext);
		if (!bCont) {
			break;
		}

	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Query Hosts for Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext)
{
	CHECK_PROC_PTR(lpEnumProc);
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_HOST_LOGICALDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->LogicalDeviceId = logicalDeviceId;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Query Hosts for Unit Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDevice(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext)
{
	CHECK_PROC_PTR(lpEnumProc);
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_HOST_UNITDEVICE,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot.bUseSlotNo = TRUE;
	lpRequest->DeviceIdOrSlot.SlotNo = dwSlotNo;
	lpRequest->UnitNo = dwUnitNo;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	for (DWORD i = 0; i < lpReply->EntryCount; ++i) {

		PNDAS_CMD_QUERY_HOST_ENTRY pEntry = &lpReply->Entry[i];

		BOOL bCont = lpEnumProc(
			&pEntry->HostGuid,
			pEntry->Access,
			lpContext);

		if (!bCont) {
			break;
		}

	}

	return TRUE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoW(
	IN LPCGUID lpHostGuid, 
	IN OUT NDAS_HOST_INFOW* pHostInfo)
{
	CHECK_READ_PTR(lpHostGuid, sizeof(GUID));
	CHECK_WRITE_PTR(pHostInfo,sizeof(NDAS_HOST_INFO));;
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_HOST_INFO,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	// None.

	lpRequest->HostGuid = *lpHostGuid;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	::CopyMemory(
		pHostInfo,
		&lpReply->HostInfo,
		sizeof(NDAS_HOST_INFOW));

	return TRUE;

}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoA(
	IN LPCGUID lpHostGuid, 
	IN OUT NDAS_HOST_INFOA* pHostInfo)
{
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasRequestSurrenderAccess(
	IN LPCGUID lpHostGuid,
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access)
{
	CHECK_READ_PTR(lpHostGuid, sizeof(GUID));
	NDAS_DEFINE_TYPES(NDAS_CMD_REQUEST_SURRENDER_ACCESS,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	// None.

	lpRequest->HostGuid = *lpHostGuid;
	lpRequest->DeviceIdOrSlot.bUseSlotNo = TRUE;
	lpRequest->DeviceIdOrSlot.SlotNo = dwSlotNo;
	lpRequest->UnitNo = dwUnitNo;
	lpRequest->Access = access;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyUnitDeviceChange(
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo)
{
	NDAS_DEFINE_TYPES(NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	// None.

	lpRequest->DeviceIdOrSlot.bUseSlotNo = TRUE;
	lpRequest->DeviceIdOrSlot.SlotNo = dwSlotNo;
	lpRequest->UnitNo = dwUnitNo;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetServiceParam(CONST NDAS_SERVICE_PARAM* pParam)
{
	CHECK_READ_PTR(pParam, sizeof(NDAS_SERVICE_PARAM));
	NDAS_DEFINE_TYPES(NDAS_CMD_SET_SERVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->Param = *pParam;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetServiceParam(DWORD ParamCode, NDAS_SERVICE_PARAM* pParam)
{
	CHECK_WRITE_PTR(pParam, sizeof(NDAS_SERVICE_PARAM));
	NDAS_DEFINE_TYPES(NDAS_CMD_GET_SERVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->ParamCode = ParamCode;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	*pParam = lpReply->Param;

	return TRUE;
}
