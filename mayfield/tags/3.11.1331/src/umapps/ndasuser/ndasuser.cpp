// ndasuser.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include <ndas/ndasmsg.h>
#include <ndas/ndascmd.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasid.h>
#include <ndas/ndasuser.h>
#include <xtl/xtltrace.h>

#pragma warning(disable: 4127)

#include "ndasscc.h"
#include "misc.h"

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
SetErrorStatus(
	NDAS_CMD_STATUS status, 
	const NDAS_CMD_ERROR::REPLY* lpErrorReply)
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
		break;
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
		XTLTRACE_ERR("Transact failed.\n"); \
		return FALSE; \
	} \
	NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); \
	if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) { \
		XTLTRACE_ERR("Command returned failure!\n"); \
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

#define CHECK_WRITE_PTR_NULLABLE(ptr,len) \
	do { if (NULL != ptr && ::IsBadWritePtr(ptr, len)) \
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

__forceinline BOOL 
IsValidNdasId(LPCWSTR lpszNdasId, LPCWSTR lpszWriteKey = NULL)
{
	if (!NdasIdValidateW(lpszNdasId, lpszWriteKey))
	{
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
		return FALSE;
	}
	return TRUE;
}

__forceinline BOOL 
IsValidNdasId(LPCSTR lpszNdasId, LPCSTR lpszWriteKey = NULL)
{
	if (!NdasIdValidateA(lpszNdasId, lpszWriteKey))
	{
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
		return FALSE;
	}
	return TRUE;
}

#define NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is) \
	NDAS_DEVICE_ID_EX is; \
	is.UseSlotNo = TRUE; \
	is.SlotNo = dwSlotNo; \
	do { ; } while(0)

#define NDAS_FORWARD_DEVICE_STRING_IDW(lpszDeviceStringId, is) \
	CHECK_STR_PTRW(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN); \
	NDAS_VALIDATE_DEVICE_STRING_ID(lpszDeviceStringId); \
	NDAS_DEVICE_ID_EX is; \
	is.UseSlotNo = FALSE; \
	XTLVERIFY(NdasIdStringToDeviceW(lpszDeviceStringId,&is.DeviceId)); \
	/*  should not fail as we already did Validation */ \
	do { ; } while(0)

#define NDAS_FORWARD_DEVICE_STRING_IDA(lpszDeviceStringId, is) \
	CHECK_STR_PTRA(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN); \
	NDAS_VALIDATE_DEVICE_STRING_IDA(lpszDeviceStringId); \
	NDAS_DEVICE_ID_EX is; \
	is.UseSlotNo = FALSE; \
	XTLVERIFY(NdasIdStringToDeviceA(lpszDeviceStringId,&is.DeviceId)); \
	/* should not fail as we already did Validation */ \
	do { ; } while(0)

//
// PASSERT is a macro to check the validity of the pre-condition and
// when it fails returns FALSE with the last error set NDASUSER_ERROR_INVALID_PARAMETER
// 
#define PASSERT(pred) \
	if (!(pred)) \
	{ \
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER); \
		return FALSE; \
	}

#define PASSERT_READ_PTR(ptr, len)             PASSERT( ! ::IsBadReadPtr(ptr,len) )
#define PASSERT_WRITE_PTR(ptr, len)            PASSERT( ! ::IsBadWritePtr(ptr,len) )
#define PASSERT_STR_PTRW(ptr, MaxCCH)          PASSERT( ! ::IsBadStringPtrW(ptr, MaxCCH) )
#define PASSERT_STR_PTRA(ptr, MaxCCH)          PASSERT( ! ::IsBadStringPtrA(ptr, MaxCCH) )
#define PASSERT_READ_PTR_NULLABLE(ptr, len)    PASSERT(NULL == ptr || ! ::IsBadReadPtr(ptr,len) )
#define PASSERT_WRITE_PTR_NULLABLE(ptr, len)   PASSERT(NULL == ptr || ! ::IsBadWritePtr(ptr,len) )
#define PASSERT_STR_PTRW_NULLABLE(ptr, MaxCCH) PASSERT(NULL == ptr || ! ::IsBadStringPtrW(ptr, MaxCCH) )
#define PASSERT_STR_PTRA_NULLABLE(ptr, MaxCCH) PASSERT(NULL == ptr || ! ::IsBadStringPtrA(ptr, MaxCCH) )

//////////////////////////////////////////////////////////////////////////
//
// NdasCmd Base Definitions
//
//////////////////////////////////////////////////////////////////////////

template <typename T, typename RequestT = T::REQUEST, typename ReplyT = T::REPLY>
struct NdasCmdBase
{
	RequestT* m_pRequest;
	ReplyT* m_pReply;
	NDAS_CMD_ERROR::REPLY* m_pErrorReply;
	DWORD m_cbRequestExtra;
	DWORD m_cbReplyExtra;
	DWORD m_cbErrorExtra;
	/* scc is a managed resource */
	CNdasServiceCommandClient<T> m_scc;

	NdasCmdBase(RequestT* pRequest, DWORD cbRequestExtra) :
		m_pRequest(pRequest), 
		m_pReply(0), 
		m_pErrorReply(0),
		m_cbRequestExtra(cbRequestExtra), 
		m_cbReplyExtra(0), 
		m_cbErrorExtra(0)
	{}

	BOOL Process()
	{
		BOOL fSuccess;

		fSuccess = m_scc.Connect();
		if (!fSuccess)
		{
			::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE);
			return FALSE;
		}

		DWORD dwCmdStatus;

		fSuccess = m_scc.Transact(
			m_pRequest, m_cbRequestExtra,
			&m_pReply, &m_cbReplyExtra,
			&m_pErrorReply, &m_cbErrorExtra,
			&dwCmdStatus);
		if (!fSuccess) 
		{
			XTLTRACE_ERR("Transact failed.\n");
			return FALSE;
		}

		// XTLTRACE("Transact completed successfully.\n");

		NDAS_CMD_STATUS cmdStatus = static_cast<NDAS_CMD_STATUS>(dwCmdStatus);
		if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) 
		{
			SetErrorStatus(cmdStatus, m_pErrorReply);
			XTLTRACE_ERR("Command returned failure.\n");
			return FALSE;
		} 

		// XTLTRACE("Command completed successfully.\n");

		return OnSuccess();
	}

	virtual void OnFailure() { return; }
	virtual BOOL OnSuccess() { return TRUE; }
private:
	NdasCmdBase(const NdasCmdBase&);
	NdasCmdBase(const T&);
	const NdasCmdBase& operator=(const NdasCmdBase&);
	const T& operator=(const T&);
};

//////////////////////////////////////////////////////////////////////////
//
// CNdasCmd Definitions (Simple)
//
//////////////////////////////////////////////////////////////////////////

template <
	typename T, 
	typename RequestT = T::REQUEST, 
	typename ReplyT = T::REPLY>
struct NdasCmd : 
	NdasCmdBase<T, RequestT, ReplyT>
{
	RequestT m_requestBuffer;
	NdasCmd() :
		NdasCmdBase<T, RequestT, ReplyT>(&m_requestBuffer, 0)
	{}
private:
	NdasCmd(const NdasCmd&);
	NdasCmd(const T&);
	const NdasCmd& operator=(const NdasCmd&);
	const T& operator=(const T&);
};

class CNdasDeviceIdEx : 
	public NDAS_DEVICE_ID_EX
{
public:
	CNdasDeviceIdEx(DWORD dwSlotNo)
	{
		UseSlotNo = TRUE;
		SlotNo = dwSlotNo;
	}
	CNdasDeviceIdEx(const NDAS_DEVICE_ID& deviceId)
	{
		UseSlotNo = FALSE;
		DeviceId = deviceId;
	}
	CNdasDeviceIdEx(LPCWSTR lpszNdasId)
	{
		UseSlotNo = FALSE;
		if (!::NdasIdStringToDeviceW(lpszNdasId, &DeviceId))
		{
			::ZeroMemory(&DeviceId, sizeof(DeviceId));
		}
	}
	CNdasDeviceIdEx(LPCSTR lpszNdasId)
	{
		UseSlotNo = FALSE;
		if (!::NdasIdStringToDeviceA(lpszNdasId, &DeviceId))
		{
			::ZeroMemory(&DeviceId, sizeof(DeviceId));
		}
	}
	BOOL IsBadDeviceId()
	{
		if (UseSlotNo)
		{
			return 0 != SlotNo;
		}
		else
		{
			const NDAS_DEVICE_ID NullDeviceId = {0};
			return 0 != ::memcmp(
				NullDeviceId.Node, 
				DeviceId.Node, 
				sizeof(DeviceId.Node));
		}
	}
};

class CNdasUnitDeviceIdEx : 
	public NDAS_UNITDEVICE_ID_EX
{
public:
	CNdasUnitDeviceIdEx(DWORD slotNo, DWORD unitNo)
	{
		DeviceIdEx = CNdasDeviceIdEx(slotNo);
		UnitNo = unitNo;
	}
	CNdasUnitDeviceIdEx(LPCWSTR lpszNdasId, DWORD unitNo)
	{
		DeviceIdEx = CNdasDeviceIdEx(lpszNdasId);
		UnitNo = unitNo;
	}
	CNdasUnitDeviceIdEx(LPCSTR lpszNdasId, DWORD unitNo)
	{
		DeviceIdEx = CNdasDeviceIdEx(lpszNdasId);
		UnitNo = unitNo;
	}
};


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
	CHECK_STR_PTRW(lpszDeviceName, UINT_PTR(-1));

	// USE_OEM_CODE flag is not allowed in this function
	// NdasRegisterDeviceEx instead.
	PASSERT(!(dwFlags & NDAS_DEVICE_REG_FLAG_USE_OEM_CODE));

	NDAS_CMD_REGISTER_DEVICE_V2::REQUEST buffer; 
	NDAS_CMD_REGISTER_DEVICE_V2::REQUEST* lpRequest = &buffer; 
	NDAS_CMD_REGISTER_DEVICE_V2::REPLY* lpReply; 
	NDAS_CMD_ERROR::REPLY* lpErrorReply; 

	// check ndas id
	BOOL fSuccess = NdasIdValidateW(lpszDeviceStringId, lpszDeviceStringKey);

	if (!fSuccess) 
	{
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
		return 0;
	}
	// Prepare the parameters here
	lpRequest->GrantingAccess = 
		(lpszDeviceStringKey) ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;

	lpRequest->RegFlags = dwFlags;

	XTLVERIFY(::NdasIdStringToDeviceExW(
		lpszDeviceStringId, 
		&lpRequest->DeviceId,
		NULL,
		&lpRequest->NdasIdExtension));
	// must not fail as we already did Validation

	HRESULT hr = ::StringCchCopyW(
		lpRequest->wszDeviceName,
		MAX_NDAS_DEVICE_NAME_LEN + 1,
		lpszDeviceName);

	if (!(SUCCEEDED(hr) || (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER == hr)))
	{
		return 0;
	}

	// ready to connect
	CNdasServiceCommandClient<NDAS_CMD_REGISTER_DEVICE_V2> scc;

	BOOL fConnected = scc.Connect();
	if (!fConnected) 
	{
		XTLTRACE_ERR("Service Connection failed.\n");
		::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE); 
		return 0; 
	} 

	DWORD cbRequestExtra(0); 
	DWORD cbReplyExtra, cbErrorExtra; 
	DWORD dwCmdStatus; 
	{ 
		fSuccess = scc.Transact(
			lpRequest, 
			cbRequestExtra, 
			&lpReply, 
			&cbReplyExtra, 
			&lpErrorReply, 
			&cbErrorExtra, 
			&dwCmdStatus); 

		if (!fSuccess) 
		{
			XTLTRACE_ERR("Transact failed.\n");
			return FALSE; 
		}

		XTLTRACE("Transact completed successfully.\n");

		NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); 
		if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) 
		{ 
			XTLTRACE_ERR("Command failed!\n"); 
			SetErrorStatus(cmdStatus, lpErrorReply); 
			return 0; 
		} 
	}

	// Process the result here
	return lpReply->SlotNo;
}

NDASUSER_LINKAGE
DWORD
NdasRegisterDeviceExW(
	IN CONST NDAS_DEVICE_REGISTRATIONW* Registration)
{
	//
	// Parameter validation
	//
	PASSERT_READ_PTR(Registration, sizeof(NDAS_DEVICE_REGISTRATIONW));
	PASSERT(Registration->Size == sizeof(NDAS_DEVICE_REGISTRATIONW));
	PASSERT_STR_PTRW(Registration->DeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	PASSERT_STR_PTRW_NULLABLE(Registration->DeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);
	PASSERT_STR_PTRW(Registration->DeviceName, UINT_PTR(-1));

	NDAS_CMD_REGISTER_DEVICE_V2::REQUEST buffer; 
	NDAS_CMD_REGISTER_DEVICE_V2::REQUEST* pRequest = &buffer; 
	NDAS_CMD_REGISTER_DEVICE_V2::REPLY* pReply; 
	NDAS_CMD_ERROR::REPLY* pErrorReply; 

	//
	// Validate NDAS Device String ID
	//
	BOOL fSuccess = ::NdasIdValidateW(
		Registration->DeviceStringId, 
		Registration->DeviceStringKey);

	if (!fSuccess) 
	{
		::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
		return 0;
	}

	//
	// Prepare the parameters here
	//

	// Should not fail as we already did Validation
	XTLVERIFY(::NdasIdStringToDeviceExW(
		Registration->DeviceStringId, 
		&pRequest->DeviceId,
		NULL,
		&pRequest->NdasIdExtension));

	pRequest->RegFlags = Registration->RegFlags;
	pRequest->GrantingAccess = (Registration->DeviceStringKey) ? 
		(GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
	pRequest->OEMCode = Registration->OEMCode;

	//
	// Copy Device Name
	//
	HRESULT hr = ::StringCbCopyW(
		pRequest->wszDeviceName,
		sizeof(pRequest->wszDeviceName),
		Registration->DeviceName);

	// Allow truncation
	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr)
	{
		return 0;
	}

	//
	// ready to connect
	//
	CNdasServiceCommandClient<NDAS_CMD_REGISTER_DEVICE_V2> scc;

	BOOL fConnected = scc.Connect();
	if (!fConnected) 
	{
		XTLTRACE_ERR("Service Connection failed.\n");
		::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE); 
		return 0; 
	} 

	{ 
		DWORD cbRequestExtra = 0; 
		DWORD cbReplyExtra;
		DWORD cbErrorExtra; 
		DWORD dwCmdStatus; 
		fSuccess = scc.Transact(
			pRequest, 
			cbRequestExtra, 
			&pReply, 
			&cbReplyExtra, 
			&pErrorReply, 
			&cbErrorExtra, 
			&dwCmdStatus); 

		if (!fSuccess) 
		{
			XTLTRACE_ERR("Transact failed.\n");
			return 0;
		}

		XTLTRACE("Transact completed successfully.\n");

		NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); 
		if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) 
		{ 
			XTLTRACE("Command failed with error code: %08X\n", cmdStatus); 
			SetErrorStatus(cmdStatus, pErrorReply); 
			return 0; 
		} 
	}

	//
	// Process the result here
	//
	XTLTRACE("Registered at slot %d.\n", pReply->SlotNo);

	return pReply->SlotNo;
}

NDASUSER_LINKAGE
DWORD
NdasRegisterDeviceExA(
	IN CONST NDAS_DEVICE_REGISTRATIONA* Registration)
{
	PASSERT_READ_PTR(Registration, sizeof(NDAS_DEVICE_REGISTRATIONA));
	PASSERT(Registration->Size == sizeof(NDAS_DEVICE_REGISTRATIONA));
	PASSERT_STR_PTRA(Registration->DeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	PASSERT_STR_PTRA_NULLABLE(Registration->DeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);
	PASSERT_STR_PTRA(Registration->DeviceName, UINT_PTR(-1));

	WCHAR wszStringId[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	WCHAR wszStringKey[NDAS_DEVICE_STRING_KEY_LEN + 1] = {0};
	WCHAR wszName[MAX_NDAS_DEVICE_NAME_LEN + 1] = {0};

	int nConverted = ::MultiByteToWideChar(
		CP_ACP, 0, 
		Registration->DeviceStringId, -1, 
		wszStringId, RTL_NUMBER_OF(wszStringId));
	_ASSERTE(nConverted > 0);

	if (Registration->DeviceStringKey)
	{
		nConverted = ::MultiByteToWideChar(
			CP_ACP, 0, 
			Registration->DeviceStringKey, -1, 
			wszStringKey, RTL_NUMBER_OF(wszStringKey));
		_ASSERTE(nConverted > 0);
	}

	nConverted = ::MultiByteToWideChar(
		CP_ACP, 0, 
		Registration->DeviceName, -1, 
		wszName, RTL_NUMBER_OF(wszName));
	_ASSERTE(nConverted > 0);

	NDAS_DEVICE_REGISTRATIONW RegW = {0};
	RegW.Size = sizeof(NDAS_DEVICE_REGISTRATIONW);
	RegW.RegFlags = RegW.RegFlags;
	RegW.DeviceStringId = wszStringId;
	RegW.DeviceStringKey = (Registration->DeviceStringKey) ? wszStringKey : NULL;
	RegW.DeviceName = wszName;
	RegW.OEMCode = Registration->OEMCode;

	return NdasRegisterDeviceExW(&RegW);
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
	UNREFERENCED_PARAMETER(dwSlotNo);
	UNREFERENCED_PARAMETER(lpBuffer);
	UNREFERENCED_PARAMETER(cbBuffer);
	UNREFERENCED_PARAMETER(cbBufferUsed);
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
	UNREFERENCED_PARAMETER(lpszDeviceStringId);
	UNREFERENCED_PARAMETER(lpBuffer);
	UNREFERENCED_PARAMETER(cbBuffer);
	UNREFERENCED_PARAMETER(cbBufferUsed);
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
	UNREFERENCED_PARAMETER(lpszDeviceStringId);
	UNREFERENCED_PARAMETER(lpBuffer);
	UNREFERENCED_PARAMETER(cbBuffer);
	UNREFERENCED_PARAMETER(cbBufferUsed);
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
	UNREFERENCED_PARAMETER(dwSlotNo);
	UNREFERENCED_PARAMETER(lpBuffer);
	UNREFERENCED_PARAMETER(cbBuffer);
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer)
{
	UNREFERENCED_PARAMETER(lpszDeviceStringId);
	UNREFERENCED_PARAMETER(lpBuffer);
	UNREFERENCED_PARAMETER(cbBuffer);
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer)
{
	UNREFERENCED_PARAMETER(lpszDeviceStringId);
	UNREFERENCED_PARAMETER(lpBuffer);
	UNREFERENCED_PARAMETER(cbBuffer);
	::SetLastError(NDASUSER_ERROR_NOT_IMPLEMENTED);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// Unregister Device
//
//////////////////////////////////////////////////////////////////////////

BOOL
NdasUnregisterDeviceImpl(const NDAS_DEVICE_ID_EX& DeviceIdOrSlot)
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

	NDAS_CMD_ENUMERATE_DEVICES_V2::REQUEST buffer; 
	NDAS_CMD_ENUMERATE_DEVICES_V2::REQUEST* lpRequest = &buffer; 
	NDAS_CMD_ENUMERATE_DEVICES_V2::REPLY* lpReply; 
	NDAS_CMD_ERROR::REPLY* lpErrorReply; 

	CNdasServiceCommandClient<NDAS_CMD_ENUMERATE_DEVICES_V2> scc;
	BOOL fConnected = scc.Connect();
	if (!fConnected) {
		XTLTRACE_ERR("Service Connection failed.\n");
		::SetLastError(NDASUSER_ERROR_SERVICE_CONNECTION_FAILURE); 
		return FALSE; 
	} 
	
	DWORD cbRequestExtra(0); 
	DWORD cbReplyExtra, cbErrorExtra; 
	DWORD dwCmdStatus; 
	
	BOOL fSuccess = scc.Transact(lpRequest, cbRequestExtra, &lpReply, &cbReplyExtra, &lpErrorReply, &cbErrorExtra, &dwCmdStatus); 
	if (!fSuccess) {
		XTLTRACE_ERR("Transact failed.\n"); 
		return FALSE; 
	}

	NDAS_CMD_STATUS cmdStatus = (NDAS_CMD_STATUS)(dwCmdStatus); 
	if (NDAS_CMD_STATUS_SUCCESS != cmdStatus) { 
		XTLTRACE_ERR("Command failed!\n"); 
		SetErrorStatus(cmdStatus, lpErrorReply); 
		return FALSE; 
	} 

	// Process the result here

	BOOL bCont(TRUE);
	if (lpEnumFunc) 
	{
		for (DWORD i = 0; i < lpReply->nDeviceEntries && bCont; ++i) 
		{
			NDAS_CMD_ENUMERATE_DEVICES_V2::ENUM_ENTRY* pDeviceEntry = &lpReply->DeviceEntry[i];
			NDASUSER_DEVICE_ENUM_ENTRYW userEntry;
			userEntry.GrantedAccess = pDeviceEntry->GrantedAccess;
			userEntry.SlotNo = pDeviceEntry->SlotNo;
			HRESULT hr = ::StringCchCopyNW(
				userEntry.szDeviceName, MAX_NDAS_DEVICE_NAME_LEN + 1,
				pDeviceEntry->wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN);
			_ASSERT(SUCCEEDED(hr)); hr;
			XTLVERIFY(
			NdasIdDeviceToStringExW(
				&pDeviceEntry->DeviceId, 
				userEntry.szDeviceStringId, 
				NULL,
				NULL,
				&pDeviceEntry->NdasIdExtension));
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
NdasEnableDeviceImpl(const NDAS_DEVICE_ID_EX& DeviceIdOrSlot, BOOL bEnable)
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
	const NDAS_DEVICE_ID_EX& DeviceIdOrSlot, 
	LPCWSTR lpszDeviceName)
{
	CHECK_STR_PTRW(lpszDeviceName, UINT_PTR(-1));
	NDAS_DEFINE_TYPES(NDAS_CMD_SET_DEVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	lpRequest->Param.ParamType = NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME;
	XTLVERIFY(SUCCEEDED(
		::StringCchCopyNW(
			lpRequest->Param.wszName, MAX_NDAS_DEVICE_NAME_LEN + 1, 
			lpszDeviceName, MAX_NDAS_DEVICE_NAME_LEN)));

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

BOOL 
NdasSetDeviceNameImplA(
	const NDAS_DEVICE_ID_EX& DeviceIdOrSlot, 
	LPCSTR lpszDeviceName)
{
	WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1] = {0};
	INT iConverted = ::MultiByteToWideChar(
		CP_THREAD_ACP, 0, 
		lpszDeviceName, MAX_NDAS_DEVICE_NAME_LEN, 
		wszDeviceName, MAX_NDAS_DEVICE_NAME_LEN + 1);
	if (0 == iConverted)
	{
		return FALSE;
	}
	return NdasSetDeviceNameImplW(DeviceIdOrSlot,wszDeviceName);
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
NdasSetDeviceNameA(
	IN DWORD dwSlotNo, 
	IN LPCSTR lpszDeviceName)
{
	NDAS_FORWARD_DEVICE_SLOT_NO(dwSlotNo,is);
	return NdasSetDeviceNameImplA(is,lpszDeviceName);
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

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameByIdA(
	LPCSTR lpszDeviceStringId, 
	LPCSTR lpszDeviceName)
{
	NDAS_FORWARD_DEVICE_STRING_IDA(lpszDeviceStringId, is);
	return NdasSetDeviceNameImplA(is, lpszDeviceName);
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
	lpRequest->DeviceIdOrSlot.UseSlotNo = FALSE;
	XTLVERIFY(NdasIdStringToDeviceW(
		lpszDeviceStringId,
		&lpRequest->DeviceIdOrSlot.DeviceId));
	// should not fail as we already did Validation

	lpRequest->Param.ParamType = NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS;
	lpRequest->Param.GrantingAccess = bWriteAccess ? 
		(GENERIC_READ | GENERIC_WRITE) : (GENERIC_READ);

	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceAccessByIdA(
	LPCSTR lpszDeviceStringId, 
	BOOL bWriteAccess, 
	LPCSTR lpszDeviceStringKey)
{
	CHECK_STR_PTRA(lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN);
	CHECK_STR_PTRA_NULLABLE(lpszDeviceStringKey, NDAS_DEVICE_STRING_KEY_LEN);

	NDAS_DEFINE_TYPES(NDAS_CMD_SET_DEVICE_PARAM,lpRequest,lpReply,lpErrorReply,scc);

	if (bWriteAccess) {
		// check ndas id
		BOOL fSuccess = NdasIdValidateA(lpszDeviceStringId, lpszDeviceStringKey);
		if (!fSuccess) {
			::SetLastError(NDASUSER_ERROR_INVALID_DEVICE_STRING_ID);
			return FALSE;
		}
	} else {
		NDAS_VALIDATE_DEVICE_STRING_IDA(lpszDeviceStringId);
	}

	NDAS_CONNECT(scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot.UseSlotNo = FALSE;
	XTLVERIFY(NdasIdStringToDeviceA(
		lpszDeviceStringId,
		&lpRequest->DeviceIdOrSlot.DeviceId));
	// should not fail as we already did Validation

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
	const NDAS_DEVICE_ID_EX& DeviceIdOrSlot, 
	NDAS_DEVICE_STATUS *pStatus,
	NDAS_DEVICE_ERROR *pLastError)
{
	CHECK_WRITE_PTR_NULLABLE(pStatus, sizeof(NDAS_DEVICE_STATUS));
	CHECK_WRITE_PTR_NULLABLE(pLastError, sizeof(NDAS_DEVICE_ERROR));

	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_DEVICE_STATUS,lpRequest,lpReply,lpErrorReply,scc);

	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;
	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	// Process result here
	if (pStatus) *pStatus = lpReply->Status;
	if (pLastError) *pLastError = lpReply->LastError;

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

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceStatusByIdA(
	LPCSTR lpszDeviceStringId, 
	NDAS_DEVICE_STATUS* pStatus,
	NDAS_DEVICE_ERROR* pLastError)
{
	NDAS_FORWARD_DEVICE_STRING_IDA(lpszDeviceStringId, is);
	return NdasQueryDeviceStatusImpl(is, pStatus, pLastError);
}

//////////////////////////////////////////////////////////////////////////
//
// Query Device Information
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasQueryDeviceInformationImplW(
	const NDAS_DEVICE_ID_EX& DeviceIdOrSlot, 
	NDASUSER_DEVICE_INFORMATIONW* pDevInfo)
{
	CHECK_WRITE_PTR(pDevInfo, sizeof(NDASUSER_DEVICE_INFORMATIONW));
	NDAS_DEFINE_TYPES(NDAS_CMD_QUERY_DEVICE_INFORMATION_V2,lpRequest,lpReply,lpErrorReply,scc);

	// Prepare the parameters here
	lpRequest->DeviceIdOrSlot = DeviceIdOrSlot;

	NDAS_CONNECT(scc);
	NDAS_TRANSACT(0,cbReplyExtra,dwCmdStatus);

	pDevInfo->GrantedAccess = lpReply->GrantedAccess;
	XTLVERIFY(SUCCEEDED(
		::StringCchCopy(
			pDevInfo->szDeviceName, 
			MAX_NDAS_DEVICE_NAME_LEN + 1, 
			lpReply->wszDeviceName)));

	pDevInfo->SlotNo = lpReply->SlotNo;

	//
	// If the device registration flag is set hidden,
	// Device ID string will be filled with FFFFF-FFFFF-FFFFF-FFFFF
	//
	if (NDAS_DEVICE_REG_FLAG_HIDDEN & lpReply->DeviceParams.RegFlags)
	{
		::FillMemory(
			pDevInfo->szDeviceId, 
			sizeof(pDevInfo->szDeviceId), 
			'F');
	}
	else
	{
		XTLVERIFY(NdasIdDeviceToStringExW(
			&lpReply->DeviceId, 
			pDevInfo->szDeviceId, 
			NULL, 
			NULL, 
			&lpReply->NdasIdExtension));
	}
	
	::CopyMemory(
		&pDevInfo->HardwareInfo, 
		&lpReply->HardwareInfo, 
		sizeof(NDAS_DEVICE_HARDWARE_INFO));

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
	const NDAS_DEVICE_ID_EX& DeviceIdOrSlot, 
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
	const NDAS_UNITDEVICE_ID_EX& UnitDeviceId, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError)
{
	PASSERT_WRITE_PTR_NULLABLE(pStatus, sizeof(NDAS_UNITDEVICE_STATUS));
	PASSERT_WRITE_PTR_NULLABLE(pLastError, sizeof(NDAS_UNITDEVICE_ERROR));
	struct Command : NdasCmd<NDAS_CMD_QUERY_UNITDEVICE_STATUS>
	{
		NDAS_UNITDEVICE_STATUS* pStatus;
		NDAS_UNITDEVICE_ERROR* pLastError;
		Command(
			const NDAS_UNITDEVICE_ID_EX& UnitDeviceId,
			NDAS_UNITDEVICE_STATUS* pStatus,
			NDAS_UNITDEVICE_ERROR* pLastError) :
			pStatus(pStatus),
			pLastError(pLastError)
		{
			NDAS_CMD_QUERY_UNITDEVICE_STATUS::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = UnitDeviceId.DeviceIdEx;
			pRequest->UnitNo = UnitDeviceId.UnitNo;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_UNITDEVICE_STATUS::REPLY* pReply = m_pReply;
			if (pStatus) *pStatus = pReply->Status;
			if (pLastError) *pLastError = pReply->LastError;
			return TRUE;
		}
	};
	return Command(UnitDeviceId, pStatus, pLastError).Process();
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
	return NdasQueryUnitDeviceStatusImpl(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo),
		pStatus, 
		pLastError);
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
	if (!IsValidNdasId(lpszDeviceStringId)) return FALSE;

	return NdasQueryUnitDeviceStatusImpl(
		CNdasUnitDeviceIdEx(lpszDeviceStringId, dwUnitNo),
		pStatus, 
		pLastError);
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
NDAS_UNITDEVICE_HARDWARE_INFOW, 
NDAS_UNITDEVICE_HARDWARE_INFOA>(
	NDAS_UNITDEVICE_HARDWARE_INFOA* pDataA,
	const NDAS_UNITDEVICE_HARDWARE_INFOW* pDataW)
{
	pDataA->LBA = pDataW->LBA;
	pDataA->LBA48 = pDataW->LBA48;
	pDataA->PIO = pDataW->PIO;
	pDataA->DMA = pDataW->DMA;
	pDataA->UDMA = pDataW->UDMA;
	pDataA->MediaType = pDataW->MediaType;

	int iConverted;

	iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, 
		pDataW->Model, RTL_NUMBER_OF(pDataW->Model), 
		pDataA->Model, RTL_NUMBER_OF(pDataA->Model), 
		NULL, NULL);
	_ASSERTE(iConverted > 0);

	iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, 
		pDataW->FirmwareRevision, RTL_NUMBER_OF(pDataW->FirmwareRevision), 
		pDataA->FirmwareRevision, RTL_NUMBER_OF(pDataA->FirmwareRevision), 
		NULL, NULL);
	_ASSERTE(iConverted > 0);

	iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, 
		pDataW->SerialNumber, RTL_NUMBER_OF(pDataW->SerialNumber), 
		pDataA->SerialNumber, RTL_NUMBER_OF(pDataA->SerialNumber), NULL, NULL);
	_ASSERTE(iConverted > 0);

	pDataA->SectorCount = pDataW->SectorCount;

	return TRUE;
}

BOOL 
NdasQueryUnitDeviceInformationImplW(
	const NDAS_UNITDEVICE_ID_EX& unitDeviceId,
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo)
{
	CHECK_WRITE_PTR(pDevInfo, sizeof(NDASUSER_UNITDEVICE_INFORMATIONW));
	struct Command : NdasCmd<NDAS_CMD_QUERY_UNITDEVICE_INFORMATION>
	{
		PNDASUSER_UNITDEVICE_INFORMATIONW pudevinfo;
		Command(
			const NDAS_UNITDEVICE_ID_EX& unitDeviceId,
			PNDASUSER_UNITDEVICE_INFORMATIONW pudevinfo)
			: pudevinfo(pudevinfo)
		{
			NDAS_CMD_QUERY_UNITDEVICE_INFORMATION::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = unitDeviceId.DeviceIdEx;
			pRequest->UnitNo = unitDeviceId.UnitNo;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_UNITDEVICE_INFORMATION::REPLY* pReply = m_pReply;
			pudevinfo->UnitDeviceType = pReply->UnitDeviceType;
			pudevinfo->UnitDeviceSubType = pReply->UnitDeviceSubType;
			pudevinfo->HardwareInfo = pReply->HardwareInfo;
			pudevinfo->UnitDeviceParams = pReply->UnitDeviceParams;
			return TRUE;
		}
	};
	return Command(unitDeviceId, pDevInfo).Process();
}

BOOL 
NdasQueryUnitDeviceInformationImplA(
	const NDAS_UNITDEVICE_ID_EX& unitDeviceId,
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo)
{
	NDASUSER_UNITDEVICE_INFORMATIONW DevInfoW = {0};
	BOOL fSuccess = NdasQueryUnitDeviceInformationImplW(unitDeviceId, &DevInfoW);
	if (!fSuccess) 
	{
		return FALSE;
	}

	// Convert NDASUSER_UNITDEVICE_INFORMATIONW to
	// NDASUSER_UNITDEVICE_INFORMATIONA
	pDevInfo->UnitDeviceType = DevInfoW.UnitDeviceType;
	pDevInfo->UnitDeviceSubType = DevInfoW.UnitDeviceSubType;
	WideToMbcs(&pDevInfo->HardwareInfo, &DevInfoW.HardwareInfo);
	pDevInfo->UnitDeviceParams = DevInfoW.UnitDeviceParams;

	return TRUE;
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationW(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo)
{
	return NdasQueryUnitDeviceInformationImplW(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo), 
		pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationA(
	DWORD dwSlotNo, DWORD dwUnitNo,
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo)
{
	return NdasQueryUnitDeviceInformationImplA(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo), 
		pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo)
{
	return NdasQueryUnitDeviceInformationImplW(
		CNdasUnitDeviceIdEx(lpszDeviceStringId, dwUnitNo), 
		pDevInfo);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo)
{
	return NdasQueryUnitDeviceInformationImplA(
		CNdasUnitDeviceIdEx(lpszDeviceStringId, dwUnitNo), 
		pDevInfo);
}

//////////////////////////////////////////////////////////////////////////
//
// NdasQueryUnitDeviceHostStats
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceHostStats(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	LPDWORD lpnROHosts, LPDWORD lpnRWHosts)
{
	CHECK_WRITE_PTR_NULLABLE(lpnROHosts, sizeof(DWORD));
	CHECK_WRITE_PTR_NULLABLE(lpnRWHosts, sizeof(DWORD));

	struct Command : NdasCmd<NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT>
	{
		LPDWORD lpnROHosts;
		LPDWORD lpnRWHosts;
		Command(
			const NDAS_UNITDEVICE_ID_EX& UnitDeviceId, 
			LPDWORD lpnROHosts,
			LPDWORD lpnRWHosts) :
			lpnROHosts(lpnROHosts),
			lpnRWHosts(lpnRWHosts)
		{
			NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = UnitDeviceId.DeviceIdEx;
			pRequest->UnitNo = UnitDeviceId.UnitNo;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT::REPLY* pReply = m_pReply;
			if (lpnROHosts) *lpnROHosts = pReply->nROHosts;
			if (lpnRWHosts) *lpnRWHosts = pReply->nRWHosts;
			return TRUE;
		}

	};
	return Command(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo), 
		lpnROHosts, 
		lpnRWHosts).Process();
}

//////////////////////////////////////////////////////////////////////////
//
// Find Logical Device of Unit Device
//
//////////////////////////////////////////////////////////////////////////

BOOL 
NdasFindLogicalDeviceOfUnitDeviceImpl(
	const NDAS_UNITDEVICE_ID_EX& unitDeviceId,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	CHECK_WRITE_PTR(pLogicalDeviceId, sizeof(NDAS_LOGICALDEVICE_ID));
	struct Command : NdasCmd<NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE>
	{
		NDAS_LOGICALDEVICE_ID* pLogicalDeviceId;
		Command(
			const NDAS_UNITDEVICE_ID_EX& unitDeviceId,
			NDAS_LOGICALDEVICE_ID* pLogicalDeviceId) :
			pLogicalDeviceId(pLogicalDeviceId)
		{
			NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = unitDeviceId.DeviceIdEx;
			pRequest->UnitNo = unitDeviceId.UnitNo;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE::REPLY* pReply = m_pReply;
			*pLogicalDeviceId = pReply->LogicalDeviceId;
			return TRUE;
		}
	};
	return Command(unitDeviceId, pLogicalDeviceId).Process();
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDevice(
	DWORD dwSlotNo, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	return NdasFindLogicalDeviceOfUnitDeviceImpl(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo),
		pLogicalDeviceId);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	return NdasFindLogicalDeviceOfUnitDeviceImpl(
		CNdasUnitDeviceIdEx(lpszDeviceStringId, dwUnitNo),
		pLogicalDeviceId);
}

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	return NdasFindLogicalDeviceOfUnitDeviceImpl(
		CNdasUnitDeviceIdEx(lpszDeviceStringId, dwUnitNo),
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
NdasPlugInLogicalDeviceEx(
	IN CONST NDAS_LOGICALDEVICE_PLUGIN_PARAM* Param)
{
	PASSERT_READ_PTR(Param, sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM));
	PASSERT(Param->Size == sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM));
	struct Command : NdasCmd<NDAS_CMD_PLUGIN_LOGICALDEVICE>	
	{
		Command(const NDAS_LOGICALDEVICE_PLUGIN_PARAM* PlugInParam)
		{
			NDAS_CMD_PLUGIN_LOGICALDEVICE::REQUEST* pRequest = m_pRequest;
			pRequest->LpiParam = *PlugInParam;
		}
		// Nothing to process on success
	};
	return Command(Param).Process();
}

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasPlugInLogicalDevice(
	BOOL bWritable, 
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_LOGICALDEVICE_PLUGIN_PARAM param = {0};
	param.Size = sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM);
	param.LogicalDeviceId = logicalDeviceId;
	param.Access = bWritable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
	return NdasPlugInLogicalDeviceEx(&param);
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
	struct Command : NdasCmd<NDAS_CMD_EJECT_LOGICALDEVICE>	
	{
		Command(NDAS_LOGICALDEVICE_ID logicalDeviceId) 
		{
			NDAS_CMD_EJECT_LOGICALDEVICE::REQUEST* pRequest = m_pRequest;
			pRequest->LogicalDeviceId = logicalDeviceId; 
		} 
	};
	return Command(logicalDeviceId).Process();
}

//////////////////////////////////////////////////////////////////////////
//
// NdasEjectLogicalDeviceEx
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEjectLogicalDeviceExW(
	PNDAS_LOGICALDEVICE_EJECT_PARAMW EjectParam)
{
	PASSERT_READ_PTR(EjectParam, sizeof(NDAS_LOGICALDEVICE_EJECT_PARAMW));
	PASSERT(EjectParam->Size == sizeof(NDAS_LOGICALDEVICE_EJECT_PARAMW));

	if (pIsWindowsXPOrLater())
	{
		struct Command : NdasCmd<NDAS_CMD_EJECT_LOGICALDEVICE_2>	
		{
			PNDAS_LOGICALDEVICE_EJECT_PARAMW EjectParam;
			Command(PNDAS_LOGICALDEVICE_EJECT_PARAMW EjectParam) :
				EjectParam(EjectParam)
			{
				NDAS_CMD_EJECT_LOGICALDEVICE_2::REQUEST* pRequest = m_pRequest;
				pRequest->LogicalDeviceId = EjectParam->LogicalDeviceId;
			}
			BOOL OnSuccess()
			{
				NDAS_CMD_EJECT_LOGICALDEVICE_2::REPLY* pReply = m_pReply;
				EjectParam->ConfigRet = pReply->ConfigRet;
				EjectParam->VetoType = pReply->VetoType;
				XTLVERIFY(SUCCEEDED(
					::StringCchCopy(
						EjectParam->VetoName,
						RTL_NUMBER_OF(EjectParam->VetoName),
						pReply->VetoName)));
				return TRUE;
			}
		};
		return Command(EjectParam).Process();
	}
	else
	{
		NDAS_SCSI_LOCATION NdasScsiLocation = {0};

		BOOL fSuccess = NdasQueryNdasScsiLocation(
			EjectParam->LogicalDeviceId, 
			&NdasScsiLocation);

		if (!fSuccess)
		{
			return FALSE;
		}

		return pRequestEject(
			NdasScsiLocation.SlotNo,
			&EjectParam->ConfigRet, 
			&EjectParam->VetoType,
			EjectParam->VetoName,
			MAX_PATH);
	}
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryNdasScsiLocation(
	NDAS_LOGICALDEVICE_ID LogicalDeviceId,
	PNDAS_SCSI_LOCATION pNdasScsiLocation)
{
	PASSERT_WRITE_PTR(pNdasScsiLocation, sizeof(NDAS_SCSI_LOCATION));
	struct Command : NdasCmd<NDAS_CMD_QUERY_NDAS_SCSI_LOCATION> {
		NDAS_SCSI_LOCATION& NdasScsiLocation;
		Command(NDAS_LOGICALDEVICE_ID logDeviceId, NDAS_SCSI_LOCATION& NdasScsiLocation) : 
			NdasScsiLocation(NdasScsiLocation) {
			NDAS_CMD_QUERY_NDAS_SCSI_LOCATION::REQUEST* pRequest = m_pRequest;
			pRequest->LogicalDeviceId = logDeviceId;
		}
		BOOL OnSuccess() {
			NDAS_CMD_QUERY_NDAS_SCSI_LOCATION::REPLY* pReply = m_pReply;
			NdasScsiLocation = pReply->NdasScsiLocation;
			return TRUE;
		}
	};
	return Command(LogicalDeviceId, *pNdasScsiLocation).Process();
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEjectLogicalDeviceExA(
	PNDAS_LOGICALDEVICE_EJECT_PARAMA EjectParam)
{
	PASSERT_READ_PTR(EjectParam, sizeof(NDAS_LOGICALDEVICE_EJECT_PARAMA));
	PASSERT(EjectParam->Size == sizeof(NDAS_LOGICALDEVICE_EJECT_PARAMA));

	NDAS_LOGICALDEVICE_EJECT_PARAMW EjectParamW = {0};
	EjectParamW.Size = sizeof(NDAS_LOGICALDEVICE_EJECT_PARAMW);
	EjectParamW.LogicalDeviceId = EjectParam->LogicalDeviceId;
	BOOL fSuccess = NdasEjectLogicalDeviceExW(&EjectParamW);
	EjectParam->ConfigRet = EjectParamW.ConfigRet;
	EjectParam->VetoType = EjectParamW.VetoType;

	C_ASSERT(RTL_NUMBER_OF(EjectParamW.VetoName) == RTL_NUMBER_OF(EjectParam->VetoName));

	for (DWORD i = 0; i < RTL_NUMBER_OF(EjectParamW.VetoName); ++i)
	{
		EjectParam->VetoName[i] = static_cast<CHAR>(EjectParamW.VetoName[i]);
	}

	return fSuccess;
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
	struct Command : NdasCmd<NDAS_CMD_UNPLUG_LOGICALDEVICE>	
	{
		Command(NDAS_LOGICALDEVICE_ID logicalDeviceId) 
		{
			m_pRequest->LogicalDeviceId = logicalDeviceId; 
		} 
	};
	return Command(logicalDeviceId).Process();
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
	struct Command : NdasCmd<NDAS_CMD_ENUMERATE_LOGICALDEVICES>
	{
		NDASLOGICALDEVICEENUMPROC m_lpEnumProc;
		LPVOID m_lpContext;

		Command(
			NDASLOGICALDEVICEENUMPROC lpEnumProc, 
			LPVOID lpContext) : 
			m_lpEnumProc(lpEnumProc), 
			m_lpContext(lpContext) 
		{
		}

		BOOL OnSuccess()
		{
			BOOL bContinue = TRUE;
			if (m_lpEnumProc) 
			{
				for (DWORD i = 0; i < m_pReply->nLogicalDeviceEntries && bContinue; ++i) 
				{
					NDASUSER_LOGICALDEVICE_ENUM_ENTRY userEntry;
					NDAS_CMD_ENUMERATE_LOGICALDEVICES::ENUM_ENTRY* 
						pCmdEntry = &m_pReply->LogicalDeviceEntries[i];

					userEntry.LogicalDeviceId = pCmdEntry->LogicalDeviceId;
					userEntry.Type = pCmdEntry->LogicalDeviceType;

					bContinue = m_lpEnumProc(&userEntry, m_lpContext);
				}
			}
			return TRUE;
		}
	};
	return Command(lpEnumProc, lpContext).Process();
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
	PASSERT_WRITE_PTR(pStatus, sizeof(NDAS_LOGICALDEVICE_STATUS));
	PASSERT_WRITE_PTR(pLastError, sizeof(NDAS_LOGICALDEVICE_ERROR));
	struct Command : NdasCmd<NDAS_CMD_QUERY_LOGICALDEVICE_STATUS> 
	{
		//
		// output parameter
		//
		NDAS_LOGICALDEVICE_STATUS* pStatus;
		NDAS_LOGICALDEVICE_ERROR* pLastError;
		//
		// constructor
		//
		Command(NDAS_LOGICALDEVICE_ID logicalDeviceId,
			NDAS_LOGICALDEVICE_STATUS* pStatus,
			NDAS_LOGICALDEVICE_ERROR* pLastError) :
			pStatus(pStatus),
			pLastError(pLastError)
		{
			m_pRequest->LogicalDeviceId = logicalDeviceId;
		}
		//
		// output handler
		//
		BOOL OnSuccess()
		{
			*pStatus = m_pReply->Status;
			*pLastError = m_pReply->LastError;
			return TRUE;
		}
	};
	return Command(logicalDeviceId, pStatus, pLastError).Process();
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
	PASSERT_WRITE_PTR(pLogDevInfo, sizeof(NDASUSER_LOGICALDEVICE_INFORMATIONW));
	struct Command : NdasCmd<NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2>
	{
		//
		// output parameter
		//
		PNDASUSER_LOGICALDEVICE_INFORMATIONW pLogDevInfo;
		Command(
			NDAS_LOGICALDEVICE_ID logicalDeviceId,
			PNDASUSER_LOGICALDEVICE_INFORMATIONW pLogDevInfo) :
			pLogDevInfo(pLogDevInfo)
		{
			NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REQUEST* pRequest = m_pRequest;
			pRequest->LogicalDeviceId = logicalDeviceId;
		}
		BOOL OnSuccess()
		{
			// aliasing just for conveniences
			NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::REPLY* pReply = m_pReply;
			pLogDevInfo->LogicalDeviceType = pReply->LogicalDeviceType;
			pLogDevInfo->GrantedAccess = pReply->GrantedAccess;
			pLogDevInfo->MountedAccess = pReply->MountedAccess;

			if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(pLogDevInfo->LogicalDeviceType)) 
			{
				pLogDevInfo->SubType.LogicalDiskInformation.Blocks = 
					pReply->LogicalDiskInfo.Blocks;
				pLogDevInfo->SubType.LogicalDiskInformation.ContentEncrypt.Revision =
					pReply->LogicalDiskInfo.ContentEncrypt.Revision;
				pLogDevInfo->SubType.LogicalDiskInformation.ContentEncrypt.Type =
					pReply->LogicalDiskInfo.ContentEncrypt.Type;
				pLogDevInfo->SubType.LogicalDiskInformation.ContentEncrypt.KeyLength =
					pReply->LogicalDiskInfo.ContentEncrypt.KeyLength;
			}
			else if (IS_NDAS_LOGICALDEVICE_TYPE_DVD_GROUP(pLogDevInfo->LogicalDeviceType)) 
			{
			}

			pLogDevInfo->nUnitDeviceEntries = pReply->nUnitDeviceEntries;

			if (pLogDevInfo->nUnitDeviceEntries > 0) {

				pLogDevInfo->FirstUnitDevice.Index = 0;
				pLogDevInfo->FirstUnitDevice.UnitNo = 
					pReply->UnitDeviceEntries[0].UnitNo;

				XTLVERIFY(NdasIdDeviceToStringExW(
					&pReply->UnitDeviceEntries[0].DeviceId, 
					pLogDevInfo->FirstUnitDevice.szDeviceStringId,
					NULL,
					NULL,
					&pReply->UnitDeviceEntries[0].NdasIdExtension));
			}

			pLogDevInfo->LogicalDeviceParams = pReply->LogicalDeviceParams;

			return TRUE;
		}
	};
	return Command(logicalDeviceId, pLogDevInfo).Process();
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
	struct Command : NdasCmd<NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2>
	{
		//
		// output parameter
		//
		NDASLOGICALDEVICEMEMBERENUMPROCW lpEnumProc;
		LPVOID lpContext;
		Command(
			NDAS_LOGICALDEVICE_ID logicalDeviceId,
			NDASLOGICALDEVICEMEMBERENUMPROCW lpEnumProc,
			LPVOID lpContext) :
			lpEnumProc(lpEnumProc),
			lpContext(lpContext)
		{
			NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::
				REQUEST* pRequest = m_pRequest;
			pRequest->LogicalDeviceId = logicalDeviceId;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2::
				REPLY* pReply = m_pReply;
			
			for (DWORD i = 0; i < pReply->nUnitDeviceEntries; ++i) 
			{
				NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW userEntry = {0};

				userEntry.Index = i;

				XTLVERIFY(NdasIdDeviceToStringExW(
					&pReply->UnitDeviceEntries[i].DeviceId,
					userEntry.szDeviceStringId,
					NULL,
					NULL,
					&pReply->UnitDeviceEntries[i].NdasIdExtension));

				userEntry.UnitNo = pReply->UnitDeviceEntries[i].UnitNo;

				BOOL bCont = lpEnumProc(&userEntry, lpContext);
				if (!bCont) 
				{
					break;
				}
			}
			return TRUE;
		}
	};
	return Command(logicalDeviceId, lpEnumProc, lpContext).Process();
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
	struct Command : NdasCmd<NDAS_CMD_QUERY_HOST_LOGICALDEVICE>
	{
		//
		// output parameter
		//
		NDASQUERYHOSTENUMPROC lpEnumProc;
		LPVOID lpContext;
		Command(
			NDAS_LOGICALDEVICE_ID logicalDeviceId,
			NDASQUERYHOSTENUMPROC lpEnumProc,
			LPVOID lpContext) :
			lpEnumProc(lpEnumProc),
			lpContext(lpContext)
		{
			NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REQUEST* pRequest = m_pRequest;
			pRequest->LogicalDeviceId = logicalDeviceId;
		}

		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_HOST_LOGICALDEVICE::REPLY* pReply = m_pReply;

			BOOL fContinue = TRUE;
			for (DWORD i = 0; fContinue && i < pReply->EntryCount; ++i)
			{
				fContinue = lpEnumProc(
					&pReply->Entry[i].HostGuid,
					pReply->Entry[i].Access,
					lpContext);
			}
			return TRUE;
		}
	};
	return Command(logicalDeviceId, lpEnumProc, lpContext).Process();
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
	struct Command : NdasCmd<NDAS_CMD_QUERY_HOST_UNITDEVICE>
	{
		//
		// output parameter
		//
		NDASQUERYHOSTENUMPROC lpEnumProc;
		LPVOID lpContext;
		Command(
			const NDAS_UNITDEVICE_ID_EX& unitDeviceId,
			NDASQUERYHOSTENUMPROC lpEnumProc,
			LPVOID lpContext) :
			lpEnumProc(lpEnumProc),
			lpContext(lpContext)
		{
			NDAS_CMD_QUERY_HOST_UNITDEVICE::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = unitDeviceId.DeviceIdEx;
			pRequest->UnitNo = unitDeviceId.UnitNo;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_HOST_UNITDEVICE::REPLY* pReply = m_pReply;
			BOOL fContinue = TRUE;
			for (DWORD i = 0; fContinue && i < pReply->EntryCount; ++i)
			{
				fContinue = lpEnumProc(
					&pReply->Entry[i].HostGuid,
					pReply->Entry[i].Access,
					lpContext);
			}
			return TRUE;
		}
	};
	return Command(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo), 
		lpEnumProc, 
		lpContext).Process();
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
	struct Command : NdasCmd<NDAS_CMD_QUERY_HOST_INFO>
	{
		//
		// output parameter
		//
		NDAS_HOST_INFOW* pHostInfo;
		Command(LPCGUID lpHostGuid, NDAS_HOST_INFOW* pHostInfo) :
			pHostInfo(pHostInfo)
		{
			NDAS_CMD_QUERY_HOST_INFO::REQUEST* pRequest = m_pRequest;
			pRequest->HostGuid = *lpHostGuid;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_HOST_INFO::REPLY* pReply = m_pReply;
			*pHostInfo = pReply->HostInfo;
			return TRUE;
		}
	};
	return Command(lpHostGuid, pHostInfo).Process();
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoA(
	IN LPCGUID lpHostGuid, 
	IN OUT NDAS_HOST_INFOA* pHostInfo)
{
	NDAS_HOST_INFOW hostInfoW = {0};
	BOOL fSuccess = NdasQueryHostInfoW(lpHostGuid, &hostInfoW);
	if (!fSuccess)
	{
		return FALSE;
	}
	pHostInfo->OSType = hostInfoW.OSType;
	for (DWORD i = 0; i < RTL_NUMBER_OF(hostInfoW.szHostname); ++i)
	{
		pHostInfo->szHostname[i] = static_cast<CHAR>(hostInfoW.szHostname[i]);
	}
	for (DWORD i = 0; i < RTL_NUMBER_OF(hostInfoW.szFQDN); ++i)
	{
		pHostInfo->szFQDN[i] = static_cast<CHAR>(hostInfoW.szFQDN[i]);
	}
	pHostInfo->OSVerInfo = hostInfoW.OSVerInfo;
	pHostInfo->ReservedVerInfo = hostInfoW.ReservedVerInfo;
	pHostInfo->NDASSWVerInfo = hostInfoW.NDASSWVerInfo;
	pHostInfo->LPXAddrs = hostInfoW.LPXAddrs;
	pHostInfo->IPV4Addrs = hostInfoW.IPV4Addrs;
	pHostInfo->IPV6Addrs = hostInfoW.IPV6Addrs;
	pHostInfo->TransportFlags = hostInfoW.TransportFlags;
	pHostInfo->FeatureFlags = hostInfoW.FeatureFlags;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NdasRequestSurrenderAccess
//
//////////////////////////////////////////////////////////////////////////

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
	struct Command : NdasCmd<NDAS_CMD_REQUEST_SURRENDER_ACCESS>
	{
		Command(
			LPCGUID lpHostGuid, 
			DWORD dwSlotNo, 
			DWORD dwUnitNo, 
			ACCESS_MASK access)
		{
			NDAS_CMD_REQUEST_SURRENDER_ACCESS::REQUEST* pRequest = m_pRequest;
			pRequest->HostGuid = *lpHostGuid;
			pRequest->DeviceIdOrSlot.UseSlotNo = TRUE;
			pRequest->DeviceIdOrSlot.SlotNo = dwSlotNo;
			pRequest->UnitNo = dwUnitNo;
			pRequest->Access = access;
		}
	};
	return Command(lpHostGuid, dwSlotNo, dwUnitNo, access).Process();
}

//////////////////////////////////////////////////////////////////////////
//
// NdasNotifyUnitDeviceChange
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyUnitDeviceChange(
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo)
{
	struct Command : NdasCmd<NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE>
	{
		Command(DWORD SlotNo, DWORD UnitNo)
		{
			NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot.UseSlotNo = TRUE;
			pRequest->DeviceIdOrSlot.SlotNo = SlotNo;
			pRequest->UnitNo = UnitNo;
		}
	};
	return Command(dwSlotNo, dwUnitNo).Process();
}

//////////////////////////////////////////////////////////////////////////
//
// NdasNotifyDeviceChange
//
//////////////////////////////////////////////////////////////////////////

BOOL
NdasNotifyDeviceChangeImpl(
	const NDAS_DEVICE_ID_EX& is)
{
	struct Command : NdasCmd<NDAS_CMD_NOTIFY_DEVICE_CHANGE>
	{
		Command(const NDAS_DEVICE_ID_EX& is)
		{
			NDAS_CMD_NOTIFY_DEVICE_CHANGE::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = is;
		}
	};
	return Command(is).Process();
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyDeviceChange(
	IN DWORD dwSlotNo)
{
	return NdasNotifyDeviceChangeImpl(CNdasDeviceIdEx(dwSlotNo));
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyDeviceChangeByIdW(
	IN LPCWSTR lpszNdasId)
{
	if (!IsValidNdasId(lpszNdasId)) return FALSE;

	return NdasNotifyDeviceChangeImpl(CNdasDeviceIdEx(lpszNdasId));
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyDeviceChangeByIdA(
	IN LPCSTR lpszNdasId)
{
	if (!IsValidNdasId(lpszNdasId)) return FALSE;

	return NdasNotifyDeviceChangeImpl(CNdasDeviceIdEx(lpszNdasId));
}

//////////////////////////////////////////////////////////////////////////
//
// NdasSetServiceParam
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetServiceParam(CONST NDAS_SERVICE_PARAM* pParam)
{
	CHECK_READ_PTR(pParam, sizeof(NDAS_SERVICE_PARAM));
	struct Command : NdasCmd<NDAS_CMD_SET_SERVICE_PARAM>
	{
		Command(CONST NDAS_SERVICE_PARAM* pParam)
		{
			NDAS_CMD_SET_SERVICE_PARAM::REQUEST* pRequest = m_pRequest;
			pRequest->Param = *pParam;
		}
	};
	return Command(pParam).Process();
}

//////////////////////////////////////////////////////////////////////////
//
// NdasGetServiceParam
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetServiceParam(DWORD ParamCode, NDAS_SERVICE_PARAM* pParam)
{
	CHECK_WRITE_PTR(pParam, sizeof(NDAS_SERVICE_PARAM));
	struct Command : NdasCmd<NDAS_CMD_GET_SERVICE_PARAM>
	{
		//
		// output parameter
		//
		NDAS_SERVICE_PARAM* Param;
		Command(
			DWORD ParamCode, 
			NDAS_SERVICE_PARAM* Param) : 
			Param(Param)
		{
			NDAS_CMD_GET_SERVICE_PARAM::REQUEST* pRequest = m_pRequest;
			pRequest->ParamCode = ParamCode;
		}
		BOOL OnSuccess()
		{
			NDAS_CMD_GET_SERVICE_PARAM::REPLY* pReply = m_pReply;
			*Param = pReply->Param;
			return TRUE;
		}
	};
	return Command(ParamCode, pParam).Process();
}

//////////////////////////////////////////////////////////////////////////
//
// NdasQueryDeviceStats
//
//////////////////////////////////////////////////////////////////////////

BOOL
NdasQueryDeviceStatsImpl(
	const NDAS_DEVICE_ID_EX& is, 
	PNDAS_DEVICE_STAT pStat)
{
	struct Command : NdasCmd<NDAS_CMD_QUERY_DEVICE_STAT>
	{
		//
		// output parameter
		//
		PNDAS_DEVICE_STAT pStat;
		// constructor
		Command(
			const NDAS_DEVICE_ID_EX& is,
			PNDAS_DEVICE_STAT pStat) : pStat(pStat)
		{
			NDAS_CMD_QUERY_DEVICE_STAT::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = is;
		}
		// success processor
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_DEVICE_STAT::REPLY* pReply = m_pReply;
			if (pStat->Size != pReply->DeviceStat.Size)
			{
				::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER);
				return FALSE;
			}
			*pStat = pReply->DeviceStat;
			return TRUE;
		}
	};

	if (sizeof(NDAS_DEVICE_STAT) != pStat->Size)
	{
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return Command(is,pStat).Process();
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStats(
	IN DWORD dwSlotNo,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats)
{
	return NdasQueryDeviceStatsImpl(
		CNdasDeviceIdEx(dwSlotNo), 
		pDeviceStats);
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStatsByIdW(
	IN LPCWSTR lpszNdasId,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats)
{
	if (!IsValidNdasId(lpszNdasId)) return FALSE;

	return NdasQueryDeviceStatsImpl(
		CNdasDeviceIdEx(lpszNdasId), 
		pDeviceStats);
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStatsByIdA(
	IN LPCSTR lpszNdasId,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats)
{
	if (!IsValidNdasId(lpszNdasId)) return FALSE;

	return NdasQueryDeviceStatsImpl(
		CNdasDeviceIdEx(lpszNdasId), 
		pDeviceStats);
}

//////////////////////////////////////////////////////////////////////////
//
// NdasQueryUnitDeviceStats
//
//////////////////////////////////////////////////////////////////////////

BOOL
NdasQueryUnitDeviceStatsImpl(
	const NDAS_UNITDEVICE_ID_EX& unitDeviceId, 
	PNDAS_UNITDEVICE_STAT pudstat)
{
	if (pudstat->Size != sizeof(NDAS_UNITDEVICE_STAT))
	{
		SetLastError(NDASUSER_ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	struct Command : NdasCmd<NDAS_CMD_QUERY_UNITDEVICE_STAT>
	{
		//
		// output parameter
		//
		PNDAS_UNITDEVICE_STAT pudstat;
		// constructor
		Command(
			const NDAS_UNITDEVICE_ID_EX& unitDeviceId, 
			PNDAS_UNITDEVICE_STAT pudstat) :
			pudstat(pudstat)
		{
			NDAS_CMD_QUERY_UNITDEVICE_STAT::REQUEST* pRequest = m_pRequest;
			pRequest->DeviceIdOrSlot = unitDeviceId.DeviceIdEx;
			pRequest->UnitNo = unitDeviceId.UnitNo;;
		}
		// success process
		BOOL OnSuccess()
		{
			NDAS_CMD_QUERY_UNITDEVICE_STAT::REPLY* pReply = m_pReply;

			if (pudstat->Size != pReply->UnitDeviceStat.Size)
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}

			*pudstat = pReply->UnitDeviceStat;
			return TRUE;
		}
	};
	return Command(unitDeviceId, pudstat).Process();
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStats(
	IN DWORD dwSlotNo, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat)
{
	return NdasQueryUnitDeviceStatsImpl(
		CNdasUnitDeviceIdEx(dwSlotNo, dwUnitNo), 
		pUnitDeviceStat);
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStatsByIdW(
	IN LPCWSTR lpszNdasId, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat)
{
	if (!IsValidNdasId(lpszNdasId)) return FALSE;

	return NdasQueryUnitDeviceStatsImpl(
		CNdasUnitDeviceIdEx(lpszNdasId, dwUnitNo), 
		pUnitDeviceStat);
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStatsByIdA(
	IN LPCSTR lpszNdasId, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat)
{
	if (!IsValidNdasId(lpszNdasId)) return FALSE;

	return NdasQueryUnitDeviceStatsImpl(
		CNdasUnitDeviceIdEx(lpszNdasId, dwUnitNo), 
		pUnitDeviceStat);
}

//////////////////////////////////////////////////////////////////////////
//
// NdasGetLogicalDeviceForPath
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetLogicalDeviceForPathW(
	IN LPCWSTR szPath, 
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	UNREFERENCED_PARAMETER(szPath);
	UNREFERENCED_PARAMETER(pLogicalDeviceId);
	return FALSE;
}

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetLogicalDeviceForPathA(
	IN LPCSTR szPath, 
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId)
{
	UNREFERENCED_PARAMETER(szPath);
	UNREFERENCED_PARAMETER(pLogicalDeviceId);
	return FALSE;
}
