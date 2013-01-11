// ndascomm.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "ndasmsg.h"

#include "xdebug.h"

#include "ndasctype.h"

#include "socketlpx.h"
#include "ndascomm_api.h"
#include "ndasid.h"
#include "ndasdevop.h"
#include "autores.h"

//#ifdef ASSERT_IF_TEST_FAIL
//#define _ASSERT_TEST(condition) _ASSERT(condition)
//#else
//#define _ASSERT_TEST(condition) 
//#endif

#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL_DEBUG(error_code, condition) if(condition) {} else {_ASSERT(condition); ::SetLastError(error_code); return FALSE;}
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL_DEBUG(error_code, destination, condition) if(condition) {} else {_ASSERT(condition); ::SetLastError(error_code); goto destination;}

#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL_RELEASE(error_code, condition) if(condition) {} else {::SetLastError(error_code); return FALSE;}
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(error_code, destination, condition) if(condition) {} else {::SetLastError(error_code); goto destination;}

#ifdef _DEBUG
#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL TEST_AND_RETURN_WITH_ERROR_IF_FAIL_DEBUG
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL TEST_AND_GOTO_WITH_ERROR_IF_FAIL_DEBUG
#else
#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL TEST_AND_RETURN_WITH_ERROR_IF_FAIL_RELEASE
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE
#endif

#define TEST_AND_RETURN_IF_FAIL(condition) if(condition) {} else {return FALSE;}
#define TEST_AND_GOTO_IF_FAIL(destination, condition) if(condition) {} else {goto destination;}

#define MAKE_USER_ID(unit, accesswrite)										\
	(	((unit) == 0) ?														\
		((accesswrite) ? FIRST_TARGET_RW_USER : FIRST_TARGET_RO_USER) :		\
		((accesswrite) ? SECOND_TARGET_RW_USER : SECOND_TARGET_RO_USER) )

/*++

NdasCommGetPassword function ...

Parameters:

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

BOOL
NdasCommGetPassword(
					BYTE *pAddress,
					UINT64 *pi64Password);

/*++

NdasCommIsHandleValidForRW function tests the NDAS handle if it is good for read/write block device or not
Because NdasCommIsHandleValidForRW set last error, caller function does not need to set last error.

Parameters:

hNDASDevice
[in] NDAS HANDLE which is LANSCSI_PATH pointer type

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

BOOL
NdasCommIsHandleValidForRW(HNDAS hNDASDevice);

BOOL
NdasCommIsHandleValid(HNDAS hNDASDevice);

////////////////////////////////////////////////////////////////////////////////////////////////
//
// NDAS Communication API Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////

BOOL g_bInitialized = FALSE;

NDASCOMM_API
BOOL
NdasCommInitialize()
{
	int iResults;
	WSADATA				wsaData;

	if(g_bInitialized)
		return TRUE;
	
	// make connection
	iResults = WSAStartup( MAKEWORD(2, 2), &wsaData );

	switch(iResults)
	{
	case 0:
		break;
	case WSASYSNOTREADY:
	case WSAVERNOTSUPPORTED:
	case WSAEINPROGRESS:
	case WSAEPROCLIM:
	case WSAEFAULT:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, FALSE);
		break;
	default:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, FALSE);
		break;
	}

	return TRUE;
}

NDASCOMM_API
DWORD
NdasCommGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDASCOMM_API_VERSION_MAJOR, 
		NDASCOMM_API_VERSION_MINOR);
}

BOOL
NdasCommConnectionInfoToDeviceId(
	PNDAS_CONNECTION_INFO pConnectionInfo,
	PNDAS_UNITDEVICE_ID pUnitDeviceID)
{
	BOOL bResults;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, !::IsBadReadPtr(pConnectionInfo, sizeof(NDAS_CONNECTION_INFO)));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, 
		NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS == pConnectionInfo->type ||
		NDAS_CONNECTION_INFO_TYPE_IDW == pConnectionInfo->type ||
		NDAS_CONNECTION_INFO_TYPE_IDA == pConnectionInfo->type);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, 0 == pConnectionInfo->UnitNo || 1 == pConnectionInfo->UnitNo);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, IPPROTO_LPXTCP == pConnectionInfo->protocol);

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, !::IsBadWritePtr(pUnitDeviceID, sizeof(NDAS_UNITDEVICE_ID)));

	switch(pConnectionInfo->type)
	{
	case NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS:
		CopyMemory(pUnitDeviceID->DeviceId.Node, pConnectionInfo->MacAddress, LPXADDR_NODE_LENGTH);
		break;
	case NDAS_CONNECTION_INFO_TYPE_IDW:
		{
			WCHAR wszID[20 +1] = {0};
			WCHAR wszKey[5 +1] = {0};

			CopyMemory(wszID, pConnectionInfo->wszDeviceStringId, sizeof(WCHAR) * 20);
			CopyMemory(wszKey, pConnectionInfo->wszDeviceStringKey, sizeof(WCHAR) * 5);

			if(pConnectionInfo->bWriteAccess)
			{

				bResults = ValidateStringIdKeyW(wszID, wszKey, &pConnectionInfo->bWriteAccess);
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, bResults);
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, pConnectionInfo->bWriteAccess);
			}

			bResults = ConvertStringIdToRealIdW(wszID, &pUnitDeviceID->DeviceId);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, bResults);
		}
		break;
	case NDAS_CONNECTION_INFO_TYPE_IDA:
		{
			CHAR szID[20 +1] = {0, };
			CHAR szKey[5 +1] = {0, };

			CopyMemory(szID, pConnectionInfo->szDeviceStringId, 20);
			CopyMemory(szKey, pConnectionInfo->szDeviceStringKey, 5);

			if(pConnectionInfo->bWriteAccess)
			{

				bResults = ValidateStringIdKeyA(szID, szKey, &pConnectionInfo->bWriteAccess);
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, bResults);
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, pConnectionInfo->bWriteAccess);
			}

			bResults = ConvertStringIdToRealIdA(szID, &pUnitDeviceID->DeviceId);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, bResults);
		}
		break;
	default:
		bResults = FALSE;
		_ASSERT(bResults);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, bResults);
		break;
	}

	pUnitDeviceID->UnitNo = pConnectionInfo->UnitNo;
	return TRUE;

fail:
	return FALSE;
}

NDASCOMM_API
HNDAS
NdasCommConnect(
	PNDAS_CONNECTION_INFO pConnectionInfo)
{
	BOOL bResults;
	PLANSCSI_PATH pPath = NULL;
	NDAS_UNITDEVICE_ID UnitDeviceID;
	BYTE HWVersionTrying;
	LONG lLoginResult, lResult;

	// process parameter
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, !::IsBadReadPtr(pConnectionInfo, sizeof(NDAS_CONNECTION_INFO)));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, 
		NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS == pConnectionInfo->type ||
		NDAS_CONNECTION_INFO_TYPE_IDW == pConnectionInfo->type ||
		NDAS_CONNECTION_INFO_TYPE_IDA == pConnectionInfo->type);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, 0 == pConnectionInfo->UnitNo || 1 == pConnectionInfo->UnitNo);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, IPPROTO_LPXTCP == pConnectionInfo->protocol);

	bResults = NdasCommConnectionInfoToDeviceId(pConnectionInfo, &UnitDeviceID);
	TEST_AND_GOTO_IF_FAIL(fail, bResults);

	pPath = (PLANSCSI_PATH)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(LANSCSI_PATH));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, fail, pPath);

	HWVersionTrying = LANSCSIIDE_VERSION_1_0;
conn:
	ZeroMemory(pPath, sizeof(LANSCSI_PATH));
	CopyMemory(&pPath->UnitDeviceID, &UnitDeviceID, sizeof(pPath->UnitDeviceID));

	// initialize pPath
	pPath->iUserID = MAKE_USER_ID(pConnectionInfo->UnitNo, pConnectionInfo->bWriteAccess);
	pPath->iCommandTag = 0;
	pPath->HPID = 0;
	pPath->iHeaderEncryptAlgo = 0;
	pPath->iDataEncryptAlgo = 0;
	pPath->iPassword = pConnectionInfo->ui64OEMCode;
	NdasCommGetPassword(pPath->UnitDeviceID.DeviceId.Node, &pPath->iPassword);
	pPath->iSessionPhase = LOGOUT_PHASE;
	pPath->HWVersion = HWVersionTrying;

	// Without HW version information, we need to connect & login for V 1.0, 1.1, 2.0 until login succeed.
	MakeConnection(pPath); // always return true
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASCOMM_WARNING_CONNECTION_FAIL, fail, pPath->connsock);

	// login
	// AING_TO_DO : need to change return values in Login()
	lLoginResult = Login(pPath, LOGIN_TYPE_NORMAL);
	if(lLoginResult)
	{
		BYTE phase = LOBYTE(LOWORD(lLoginResult));
		BYTE err = HIBYTE(LOWORD(lLoginResult));
		BYTE response = LOBYTE(HIWORD(lLoginResult));

		if(1 == err || 2 == err)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_LOGIN_COMMUNICATION, fail, FALSE);
		}

		if(3 == phase && 4 == err &&
			LANSCSI_RESPONSE_T_COMMAND_FAILED == response)
		{
			// phase 3 : possible multi RW login
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASCOMM_ERROR_RW_USER_EXIST, fail, FALSE);
		}

		if(4 == err)
		{
			switch(response)
			{
			case LANSCSI_RESPONSE_RI_NOT_EXIST:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_RI_NOT_EXIST, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_RI_BAD_COMMAND:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_RI_BAD_COMMAND, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_RI_COMMAND_FAILED:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_RI_COMMAND_FAILED, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_RI_VERSION_MISMATCH: // version check				
				switch(HWVersionTrying)
				{
				case LANSCSIIDE_VERSION_1_0: // raise version & try again
					HWVersionTrying = LANSCSIIDE_VERSION_1_1;
					closesocket(pPath->connsock);
					goto conn;
				case LANSCSIIDE_VERSION_1_1: // raise version & try again
					HWVersionTrying = LANSCSIIDE_VERSION_2_0;
					closesocket(pPath->connsock);
					goto conn;
				case LANSCSIIDE_VERSION_2_0:
				default:
					TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, fail, FALSE);
					break;
				}
				break;
			case LANSCSI_RESPONSE_RI_AUTH_FAILED:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_RI_AUTH_FAILED, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_T_NOT_EXIST:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_T_NOT_EXIST, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_T_BAD_COMMAND:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_T_BAD_COMMAND, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_T_COMMAND_FAILED:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_T_COMMAND_FAILED, fail, FALSE);
				break;
			case LANSCSI_RESPONSE_T_BROKEN_DATA:
				TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_T_BROKEN_DATA, fail, FALSE);
				break;
			}
		}

		// default case
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_LOGIN_UNKNOWN, fail, FALSE);
	}


	// get disk information
	lResult = GetDiskInfo(pPath, pPath->UnitDeviceID.UnitNo);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, fail, !lResult);	// fail if non-zero(-1)

	if(pPath->PerTarget[pConnectionInfo->UnitNo].MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE)
	{
		//  ensure pPath is valid
		bResults = NdasCommIsHandleValidForRW(pPath);
		TEST_AND_RETURN_IF_FAIL(bResults);
	}

	// complete

	return (HNDAS)pPath;

fail:
	DWORD dwLastErrorBackup = ::GetLastError();

	if(pPath)
	{
		NdasCommDisconnect((HNDAS)pPath);
	}

	::SetLastError(dwLastErrorBackup);
	return NULL;

}

NDASCOMM_API
BOOL
NdasCommDisconnect(
  HNDAS hNDASDevice)
{
	LONG lResults;


	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, !::IsBadReadPtr(pPath, sizeof(pPath)));
//	ret = NdasCommIsHandleValidForRW(hNDASDevice); // do not validate

	if(pPath->connsock)
	{
		if(FLAG_FULL_FEATURE_PHASE == pPath->iSessionPhase)
		{
			lResults = Logout(pPath);
			_ASSERT(!lResults);
			_ASSERT(LOGOUT_PHASE == pPath->iSessionPhase);
		}

		lResults = closesocket(pPath->connsock);
		_ASSERT(!lResults);
	}
	pPath->connsock = NULL;

	HeapFree(::GetProcessHeap(), NULL, pPath);
	pPath = NULL;
	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommBlockDeviceRead(
	HNDAS	hNDASDevice,
	INT64	i64Location,
	UINT64	ui64SectorCount,
	PCHAR	pData)
{
	BOOL	bResults;
	LONG	lResults;
	INT64	i64LocationRevised;
	UCHAR	response;
	PLANSCSI_PATH pPath = NULL;
	UINT16	l_usSectorCount;
	UINT64	l_ui64SectorCountLeft;
	PCHAR	l_pData;
	
	pPath = (PLANSCSI_PATH)hNDASDevice;
	
	bResults = NdasCommIsHandleValidForRW(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		0 == pPath->UnitDeviceID.UnitNo ||
		1 == pPath->UnitDeviceID.UnitNo);

	i64LocationRevised = (i64Location < 0) ? 
		(INT64)pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount + i64Location : i64Location;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		i64LocationRevised >= 0 && i64LocationRevised < (INT64)pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount);

	//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, usSectorCount > 0 && usSectorCount <= LANSCSI_MAX_REQUESTBLOCK);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, usSectorCount > 0);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, !::IsBadReadPtr(pData, (UINT32)ui64SectorCount * LANSCSI_BLOCK_SIZE));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pData);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE == pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType);

	l_pData = pData;
	l_ui64SectorCountLeft = ui64SectorCount;
	while(l_ui64SectorCountLeft > 0){
		l_usSectorCount = (UINT16)min(l_ui64SectorCountLeft, LANSCSI_MAX_REQUESTBLOCK);
		lResults = IdeCommand(pPath, pPath->UnitDeviceID.UnitNo, 0, WIN_READ, i64LocationRevised, l_usSectorCount, 0, l_pData, &response);
		// AING_TO_DO : should be switch after IdeCommand changed
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, !lResults);

		l_ui64SectorCountLeft -= l_usSectorCount;
		i64LocationRevised += l_usSectorCount;
		l_pData += l_usSectorCount * LANSCSI_BLOCK_SIZE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommBlockDeviceWrite(
	HNDAS	hNDASDevice,
	INT64	i64Location,
	UINT64	ui64SectorCount,
	PCHAR	pData)
{
	BOOL	bResults;
	LONG	lResults;
	INT64	i64LocationRevised;
	UCHAR	response;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
	UINT16	l_usSectorCount;
	UINT64	l_ui64SectorCountLeft;
	PCHAR	l_pData;

	bResults = NdasCommIsHandleValidForRW(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		0 == pPath->UnitDeviceID.UnitNo || 
		1 == pPath->UnitDeviceID.UnitNo);

	i64LocationRevised = (i64Location < 0) ? 
		(INT64)pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount + i64Location : i64Location;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		i64LocationRevised >= 0 && i64LocationRevised < (INT64)pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount);

	//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, usSectorCount > 0 && usSectorCount <= LANSCSI_MAX_REQUESTBLOCK);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, usSectorCount > 0);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, !::IsBadWritePtr(pData, (UINT32)ui64SectorCount * LANSCSI_BLOCK_SIZE));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE == pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType);

	l_pData = pData;
	l_ui64SectorCountLeft = ui64SectorCount;
	while(l_ui64SectorCountLeft > 0){
		l_usSectorCount = (UINT16)min(l_ui64SectorCountLeft, LANSCSI_MAX_REQUESTBLOCK);
		lResults = IdeCommand(pPath, pPath->UnitDeviceID.UnitNo, 0, WIN_WRITE, i64LocationRevised, l_usSectorCount, 0, l_pData, &response);
		// AING_TO_DO : should be switch after IdeCommand changed
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, !lResults);

		l_ui64SectorCountLeft -= l_usSectorCount;
		i64LocationRevised += l_usSectorCount;
		l_pData += l_usSectorCount * LANSCSI_BLOCK_SIZE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommBlockDeviceWriteSafeBuffer(
	HNDAS	hNDASDevice,
	INT64	i64Location,
	UINT64	ui64SectorCount,
	PCHAR	pData)
{
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
	CHAR *l_data;
	BOOL bResults;
	
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath);

	if(pPath->iDataEncryptAlgo)
	{
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, !::IsBadWritePtr(pData, (UINT32)ui64SectorCount * LANSCSI_BLOCK_SIZE));

		l_data = (CHAR *)::HeapAlloc(::GetProcessHeap(), 0, LANSCSI_BLOCK_SIZE * (UINT32)ui64SectorCount);
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY, l_data);
		CopyMemory(l_data, pData, LANSCSI_BLOCK_SIZE * (UINT32)ui64SectorCount);
		bResults = NdasCommBlockDeviceWrite(hNDASDevice, i64Location, ui64SectorCount, l_data);
		::HeapFree(::GetProcessHeap(), 0, l_data);
		return bResults;
	}
	else
	{
		return NdasCommBlockDeviceWrite(hNDASDevice, i64Location, ui64SectorCount, pData);
	}
}

NDASCOMM_API
BOOL
NdasCommBlockDeviceVerify(
						  HNDAS	hNDASDevice,
						  INT64	i64Location,
						  UINT16	usSectorCount
						  )
{
	BOOL	bResults;
	LONG	lResults;
	INT64	i64LocationRevised;
	UCHAR	response;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
	UINT16	l_usSectorCount;

	bResults = NdasCommIsHandleValidForRW(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		0 == pPath->UnitDeviceID.UnitNo || 
		1 == pPath->UnitDeviceID.UnitNo);

	i64LocationRevised = (i64Location < 0) ? 
		(INT64)pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount + i64Location : i64Location;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		i64LocationRevised >= 0 && i64LocationRevised < (INT64)pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, usSectorCount > 0);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE == pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType);

	do{
		l_usSectorCount = min(usSectorCount, LANSCSI_MAX_REQUESTBLOCK);
		lResults = IdeCommand(pPath, pPath->UnitDeviceID.UnitNo, 0, WIN_VERIFY, i64LocationRevised, l_usSectorCount, 0, NULL, &response);
		// AING_TO_DO : should be switch after IdeCommand changed
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, !lResults);

		usSectorCount -= l_usSectorCount;
	} while(usSectorCount > 0);

	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommBlockDeviceSetFeature(
							  HNDAS hNDASDevice,
							  BYTE feature,
							  UINT16 param
							  )
{
	BOOL	bResults;
	LONG	lResults;
	UCHAR	response;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;

	bResults = NdasCommIsHandleValid(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		0 == pPath->UnitDeviceID.UnitNo || 
		1 == pPath->UnitDeviceID.UnitNo);

	lResults = IdeCommand(pPath, pPath->UnitDeviceID.UnitNo, 0, WIN_SETFEATURES, 0, param, feature, NULL, &response);
	// AING_TO_DO : should be switch after IdeCommand changed
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, !lResults);

	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommGetDeviceInfo(
	HNDAS hNDASDevice,
	PNDAS_DEVICE_INFO pInfo)
{
	BOOL	bResults;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
	LANSCSI_PATH path_discovery;
	INT32 i;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pInfo, sizeof(NDAS_DEVICE_INFO)));

	bResults = NdasCommIsHandleValid(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	pInfo->HWType = pPath->HWType;
	pInfo->HWVersion = pPath->HWVersion;
	pInfo->HWProtoType = pPath->HWProtoType;
	pInfo->HWProtoVersion = pPath->HWProtoVersion;
	pInfo->iNumberofSlot = pPath->iNumberofSlot;
	pInfo->iMaxBlocks = pPath->iMaxBlocks;
	pInfo->iMaxTargets = pPath->iMaxTargets;
	pInfo->iMaxLUs = pPath->iMaxLUs;
	pInfo->iHeaderEncryptAlgo = pPath->iHeaderEncryptAlgo;
	pInfo->iDataEncryptAlgo = pPath->iDataEncryptAlgo;

	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommGetUnitDeviceInfo(
	HNDAS hNDASDevice,
	PNDAS_UNIT_DEVICE_INFO pUnitInfo)
{
	BOOL	bResults;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
	LANSCSI_PATH path_discovery;
	INT32 i;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pUnitInfo, sizeof(NDAS_UNIT_DEVICE_INFO)));

	bResults = NdasCommIsHandleValid(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	if(NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE == pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType)
	{
		pUnitInfo->SectorCount = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount;
		pUnitInfo->bLBA = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bLBA;
		pUnitInfo->bLBA48 = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bLBA48;
	}
	else
	{
		pUnitInfo->SectorCount = 0;
		pUnitInfo->bLBA = FALSE;
		pUnitInfo->bLBA48 = FALSE;
	}

	CopyMemory(pUnitInfo->Model, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].Model, sizeof(pUnitInfo->Model));
	CopyMemory(pUnitInfo->FwRev, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].FwRev, sizeof(pUnitInfo->FwRev));
	CopyMemory(pUnitInfo->SerialNo, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SerialNo, sizeof(pUnitInfo->SerialNo));
	pUnitInfo->bPIO = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bPIO;
	pUnitInfo->bDma = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bDma;
	pUnitInfo->bUDma = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bUDma;
	pUnitInfo->MediaType = pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType;


	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommGetUnitDeviceDynInfo(
	PNDAS_CONNECTION_INFO pConnectionInfo,
	PNDAS_UNIT_DEVICE_DYN_INFO pUnitDynInfo)
{
	BOOL	bResults;
	LONG	lResults;
	LANSCSI_PATH path_discovery;
	NDAS_UNITDEVICE_ID UnitDeviceID;
	BYTE HWVersionTrying;
	INT32 i;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pUnitDynInfo, sizeof(NDAS_UNIT_DEVICE_DYN_INFO)));

	bResults = NdasCommConnectionInfoToDeviceId(pConnectionInfo, &UnitDeviceID);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// login & discovery
	HWVersionTrying = LANSCSIIDE_VERSION_1_0;
conn:
	ZeroMemory(&path_discovery, sizeof(LANSCSI_PATH));
	CopyMemory(&path_discovery.UnitDeviceID, &UnitDeviceID, sizeof(path_discovery.UnitDeviceID));

	MakeConnection(&path_discovery);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL_RELEASE(NDASCOMM_WARNING_CONNECTION_FAIL, path_discovery.connsock);
	path_discovery.iUserID = 0;
	path_discovery.iCommandTag = 0;
	path_discovery.HPID = 0;
	path_discovery.iHeaderEncryptAlgo = 0;
	path_discovery.iDataEncryptAlgo = 0;
	path_discovery.iPassword = pConnectionInfo->ui64OEMCode;
	NdasCommGetPassword(path_discovery.UnitDeviceID.DeviceId.Node, &path_discovery.iPassword);
	path_discovery.iSessionPhase = LOGOUT_PHASE;
	path_discovery.HWVersion = HWVersionTrying;

	lResults = Discovery(&path_discovery);
	if(lResults) // version check
	{
		// retry if version problem
		LONG lLoginResult = lResults;
		BYTE phase = LOBYTE(LOWORD(lLoginResult));
		BYTE err = HIBYTE(LOWORD(lLoginResult));
		BYTE response = LOBYTE(HIWORD(lLoginResult));

		if(4 == err && LANSCSI_RESPONSE_RI_VERSION_MISMATCH == response)
			switch(HWVersionTrying)
		{
			case LANSCSIIDE_VERSION_1_0: // raise version & try again
				HWVersionTrying = LANSCSIIDE_VERSION_1_1;
				closesocket(path_discovery.connsock);
				goto conn;
			case LANSCSIIDE_VERSION_1_1: // raise version & try again
				HWVersionTrying = LANSCSIIDE_VERSION_2_0;
				closesocket(path_discovery.connsock);
				goto conn;
			case LANSCSIIDE_VERSION_2_0:
			default:
				TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, FALSE);
				break;
		}
	}
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INTERNAL, !lResults);

	closesocket(path_discovery.connsock);

	pUnitDynInfo->iNRTargets = path_discovery.iNRTargets;
	pUnitDynInfo->bPresent = path_discovery.PerTarget[path_discovery.UnitDeviceID.UnitNo].bPresent;
	pUnitDynInfo->NRRWHost = path_discovery.PerTarget[path_discovery.UnitDeviceID.UnitNo].NRRWHost;
	pUnitDynInfo->NRROHost = path_discovery.PerTarget[path_discovery.UnitDeviceID.UnitNo].NRROHost;
	pUnitDynInfo->TargetData = path_discovery.PerTarget[path_discovery.UnitDeviceID.UnitNo].TargetData;
	return TRUE;
}

NDASCOMM_API
BOOL
NdasCommGetDeviceId(
					HNDAS hNDASDevice,
					BYTE *pDeviceId,
					BYTE *pUnitNo
					)
{
	BOOL bResults;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pDeviceId, sizeof(pPath->UnitDeviceID.DeviceId.Node)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pUnitNo, sizeof(BYTE)));

	bResults = NdasCommIsHandleValid(pPath);
	TEST_AND_RETURN_IF_FAIL(bResults);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pDeviceId);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pUnitNo);

	CopyMemory(pDeviceId, pPath->UnitDeviceID.DeviceId.Node, sizeof(pPath->UnitDeviceID.DeviceId.Node));
	*pUnitNo = (BYTE)pPath->UnitDeviceID.UnitNo;
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Functions, not for exporting
//
////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
NdasCommGetPassword(
				  BYTE *pAddress,
				  UINT64 *pi64Password)
{
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pAddress, 6));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pi64Password, sizeof(UINT64)));

	if(*pi64Password)
		return TRUE;

	// password
	// if it's sample's address, use its password
	if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0xf0 &&
		pAddress[2] == 0x0f)
	{
		*pi64Password = HASH_KEY_SAMPLE;
	}
#ifdef OEM_RUTTER
	else if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0 &&
		pAddress[3] & 0xFE == 0x20
		)
	{
		*pi64Password = HASH_KEY_RUTTER;
	}
#endif // OEM_RUTTER
	else if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0)
	{
		*pi64Password = HASH_KEY_USER;
	}
	else
	{
		//
		//	default to XIMETA
		//
		*pi64Password = HASH_KEY_USER;
	}

	return TRUE;
}

BOOL
NdasCommIsHandleValid(HNDAS hNDASDevice)
{
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pPath, sizeof(LANSCSI_PATH)));

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->connsock);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, FLAG_FULL_FEATURE_PHASE == pPath->iSessionPhase);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		0 == pPath->UnitDeviceID.UnitNo ||
		1 == pPath->UnitDeviceID.UnitNo);

	// HWVersion is detected after 1st step
	/*
	HWVersion			HWProtoVersion
	V 1.0				0						0
	V 1.1				1						1
	V 2.0				2						1
	*/

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
		LANSCSIIDE_VERSION_1_0 == pPath->HWVersion || 
		LANSCSIIDE_VERSION_1_1 == pPath->HWVersion ||
		LANSCSIIDE_VERSION_2_0 == pPath->HWVersion);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
		(LANSCSIIDE_VERSION_1_0 == pPath->HWVersion && LSIDEPROTO_VERSION_1_0 == pPath->HWProtoVersion) ||
		((LANSCSIIDE_VERSION_1_1 == pPath->HWVersion || LANSCSIIDE_VERSION_2_0 == pPath->HWVersion )&& LSIDEPROTO_VERSION_1_1 == pPath->HWProtoVersion));

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->HWType == 0);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->HWProtoType == pPath->HWType);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->iNumberofSlot > 0);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->iMaxBlocks <= LANSCSI_MAX_REQUESTBLOCK);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->iMaxTargets >= pPath->iNumberofSlot);

	return TRUE;
}

BOOL
NdasCommIsHandleValidForRW(HNDAS hNDASDevice)
{
	BOOL bResults;
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;

	bResults = NdasCommIsHandleValid(hNDASDevice);
	TEST_AND_RETURN_IF_FAIL(bResults);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bLBA);
	//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bLBA48);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE);

	return TRUE;
}
