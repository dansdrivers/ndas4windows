#include <windows.h>
#include <stdio.h>
#include <tchar.h>
//#include <ndas/ndastype.h>
//#include <ndas/ndasuser.h>
#include <ndas/ndascomm.h>

int __cdecl main(int argc, char *argv[])
{
	DWORD dwError = 0;
	BOOL bResult;
	HNDAS hNdas;
	BYTE data[512];

	if(2 != argc)
	{
		printf(
			"usage : apitest.exe ID-KEY\n"
			"\n"
			"ID-KEY : 20 chars of id and 5 chars of key of the NDAS Device ex(01234ABCDE56789FGHIJ13579)\n"
			);
		printf("\n");
		return FALSE;
	}

	// Initialize NdasComm library
	bResult = NdasCommInitialize();
	if(FALSE == bResult)
	{
		dwError = ::GetLastError();
		return FALSE;
	}

	// Initialize NDASCOMM_CONNECTION_INFO structure
	NDASCOMM_CONNECTION_INFO ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A;
	ci.UnitNo = 0;
	ci.bWriteAccess = TRUE;
	ci.protocol = NDASCOMM_TRANSPORT_LPX;
	strncpy(ci.DeviceIDA.szDeviceStringId, argv[1], 20);
	strncpy(ci.DeviceIDA.szDeviceStringKey, argv[1] +20, 5);


	// Retrieve stats of the NDAS Device.
	NDASCOMM_UNIT_DEVICE_STAT NdasStat;
	ci.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
	bResult = NdasCommGetUnitDeviceStat(&ci, &NdasStat, 0, NULL);
	if(FALSE == bResult)
	{
		dwError = ::GetLastError();
		return FALSE;
	}

	// Create connection
	HNDAS hNDAS;
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
	hNDAS = NdasCommConnect(&ci, 15000, NULL);
	if(NULL == hNDAS)
	{
		dwError = ::GetLastError();
		return FALSE;
	}

	DWORD dwTimeout;
	NdasCommGetTransmitTimeout(hNDAS, &dwTimeout);
	NdasCommSetTransmitTimeout(hNDAS, 10000);
	NdasCommGetTransmitTimeout(hNDAS, &dwTimeout);

	NDASCOMM_VCMD_PARAM param_vcmd;
	bResult = NdasCommVendorCommand(
		hNDAS, 
		ndascomm_vcmd_get_standby_timer,
		&param_vcmd,
		NULL, 0, NULL, 0);

	if(0) // super user only
	{
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = TRUE;
		param_vcmd.SET_STANDBY_TIMER.TimeValue = 30;

		bResult = NdasCommVendorCommand(
			hNDAS, 
			ndascomm_vcmd_set_standby_timer,
			&param_vcmd,
			NULL, 0, NULL, 0);

		bResult = NdasCommVendorCommand(
			hNDAS, 
			ndascomm_vcmd_get_standby_timer,
			&param_vcmd,
			NULL, 0, NULL, 0);
	}

	BYTE Buffer[6];
	DWORD BufferLen;
	bResult = NdasCommGetHostAddress(hNDAS, NULL, &BufferLen);
	bResult = NdasCommGetHostAddress(hNDAS, Buffer, &BufferLen);

	// Retrieve information of the NDAS Device.
	UINT8 HWVersion;
	bResult = NdasCommGetDeviceInfo(
		hNDAS, 
		ndascomm_handle_info_hw_version, 
		&HWVersion, 
		sizeof(HWVersion));
	if(FALSE == bResult)
	{
		dwError = ::GetLastError();
		return FALSE;
	}

	// Retrieve unit information of the NDAS Device.
	NDASCOMM_UNIT_DEVICE_INFO NdasInfo;
	bResult = NdasCommGetUnitDeviceInfo(
		hNDAS, 
		&NdasInfo);
	if(FALSE == bResult)
	{
		dwError = ::GetLastError();
		return FALSE;
	}

//	// enable standby timer to 10 min.
//	if(0 == HWVersion) // Ver 1.0 does not support standby timer
//	{
//	}
//	else
//	{
//		UINT64 param;
//		param = 0x0000000080000000 /* enable */ | 10 /* minutes */;
//		bResult = NdasCommVendorCommand(
//			hNDAS, 
//			0x14, // ndascomm_vcmd_set_standby_timer
//			(UINT8 *)&param, 
//			sizeof(param), 
//			NULL, 0,
//			NULL, 0);
//	}

	if(0 == HWVersion) // Ver 1.0 does not support standby timer
	{
	}
	else
	{
		printf("Sends WIN_STANDBY\n");
		NDASCOMM_IDE_REGISTER reg;
		ZeroMemory(&reg, sizeof(NDASCOMM_IDE_REGISTER));
		reg.command.command = 0xE2, // WIN_STANDBY;
		bResult = NdasCommIdeCommand(hNDAS, &reg, NULL, 0, NULL, 0);
		if(FALSE == bResult)
		{
			dwError = ::GetLastError();
			return FALSE;
		}
	}

	{
		INT64 i64Location;
		UINT ui64SectorCount;

		DWORD dwTickCount = GetTickCount();
		// reads 1 sector from sector 0
		i64Location = 0;
		ui64SectorCount = 1;
		bResult = NdasCommBlockDeviceRead(hNDAS, i64Location, ui64SectorCount, data);
		if(FALSE == bResult)
		{
			dwError = ::GetLastError();
			return FALSE;
		}

		// writes 1 sector to last sector
		i64Location = -1;
		ui64SectorCount = 1;
		bResult = NdasCommBlockDeviceWrite(hNDAS, i64Location, ui64SectorCount, data);
		if(FALSE == bResult)
		{
			dwError = ::GetLastError();
			return FALSE;
		}
	}


	bResult = NdasCommDisconnect(hNDAS);
	if(NULL == hNDAS)
	{
		dwError = ::GetLastError();
		return FALSE;
	}

	return TRUE;
}
