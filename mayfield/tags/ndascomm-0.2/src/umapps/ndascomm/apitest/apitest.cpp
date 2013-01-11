/*
  apitest.cpp : NDASComm API library test program for windows
  Copyright(C) 2002-2005 XIMETA, Inc.

  Gi youl Kim <kykim@ximeta.com>

  This library is provided to support manufacturing processes and 
  for diagnostic purpose. Do not redistribute this library.
*/

/*
  This test program tests NDASComm API library by connection, disconnection,
  read, write, ATA command, vendor specific command operating to the NDAS
  Device selected at command line.

  Also you can use this program as how-to for NDASComm API library.

  This program is designed using ASCII code. But NDASComm supports wide char
  also.

  Data on the NDAS Unit device can be changed or deleted, do not store any
  important data on the NDAS Unit device before the test go.
*/

#include <windows.h>
#include <stdio.h>
#include <ndas/ndascomm.h> // include this header file to use NDASComm API
#include <ndas/ndasmsg.h> // included to process Last Error from NDASComm API

#ifdef _DEBUG
#define DBGTRACE(e,stmt) ::printf("%s(%d): error %u(%08x): %s\n", __FILE__, __LINE__, e, e, stmt)
#else
#define DBGTRACE(e,stmt) __noop;
#endif

#define API_CALL(_stmt_) \
	do { \
	if (!(_stmt_)) { \
	DBGTRACE(::GetLastError(), #_stmt_); \
	return (int)::GetLastError(); \
	} \
	} while(0)

/*
main function
returns non-zero if any function fails.
*/
int __cdecl main(int argc, char *argv[])
{
	DWORD dwError = 0;
	BOOL bResult;
	HNDAS hNdas;
	BYTE data[512]; // 1 sector sized buffer

	// simple check parameter
	if(2 != argc)
	{
		printf(
			"usage : apitest.exe ID-KEY\n"
			"\n"
			"ID-KEY : 20 chars of id and 5 chars of key of the NDAS Device ex(01234ABCDE56789FGHIJ13579)\n"
			"ex : apitest.exe 01234ABCDE56789FGHIJ13579\n"
			);
		printf("\n");
		return -1;
	}

	printf("\n\n\n* Start NDASComm API test on the NDAS Device : %s\n", argv[1]);

	printf("* Initialize NdasComm library : NdasCommInitialize()\n");
	API_CALL(NdasCommInitialize());

	DWORD dwVersion;
	printf("* Get API Version : NdasCommGetAPIVersion()\n");
	API_CALL(dwVersion = NdasCommGetAPIVersion());
	printf("- Version : Major %d, Minor %d\n",
		(int)LOWORD(dwVersion), (int)HIWORD(dwVersion));

	printf("* Initialize connection info to create connection to the NDAS Device\n");
	NDASCOMM_CONNECTION_INFO ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A; // ASCII char set
	ci.UnitNo = 0; // Use first Unit Device
	ci.bWriteAccess = TRUE; // Connect with read-write privilege
	ci.protocol = NDASCOMM_TRANSPORT_LPX; // Use LPX protocol
	ci.ui64OEMCode = 0; // Use default password
	ci.bSupervisor = FALSE; // Log in as normal user
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL; // Normal operations
	strncpy(ci.DeviceIDA.szDeviceStringId, argv[1], 20); // ID
	strncpy(ci.DeviceIDA.szDeviceStringKey, argv[1] +20, 5); // Key

	HNDAS hNDAS;
	printf("* Connect to the NDAS Device : NdasCommConnect()\n");
	API_CALL(
		hNDAS = NdasCommConnect(
			&ci,
			0 /* synchronous mode */,
			NULL /* no connection hint */
			)
		);

	BYTE DeviceID[6];
	DWORD UnitNo;
	printf("* Retrieve NDAS Device ID & unit number : NdasCommGetDeviceID()\n");
	API_CALL(NdasCommGetDeviceID(hNDAS, DeviceID, &UnitNo));
	printf("- DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)UnitNo);

	PBYTE Buffer;
	DWORD BufferLen;
	printf("* Retrieve the address of the host attached to the NDAS Device : NdasCommGetHostAddress()\n");
	API_CALL(NdasCommGetHostAddress(hNDAS, NULL, &BufferLen));
	printf("- buffer length : %d\n", BufferLen);
	Buffer = new BYTE[BufferLen];
	API_CALL(NdasCommGetHostAddress(hNDAS, Buffer, &BufferLen));
	printf("- Host Address : ");
	for(DWORD i = 0 ; i < BufferLen; i++)
	{
		printf("%02X", (UINT)Buffer[i]);
	}
	printf("\n");
	delete [] Buffer;

	INT64 i64Location;
	UINT ui64SectorCount;

	ui64SectorCount = 1;
	i64Location = 0;

	printf("* Read %d sector(s) of data from Address %d : NdasCommBlockDeviceRead()\n",
		ui64SectorCount, i64Location);
	API_CALL(NdasCommBlockDeviceRead(hNDAS, i64Location, ui64SectorCount, data));

	i64Location = 1;
	printf("* Write %d sector(s) of data to Address %d : NdasCommBlockDeviceWriteSafeBuffer()\n",
		ui64SectorCount, i64Location);
	API_CALL(NdasCommBlockDeviceWriteSafeBuffer(hNDAS, i64Location, ui64SectorCount, data));

	ui64SectorCount = 2;
	i64Location = 2;
	printf("* Verify %d sector(s) from Address %d : NdasCommBlockDeviceVerify()\n",
		ui64SectorCount, i64Location);
	API_CALL(NdasCommBlockDeviceVerify(hNDAS, i64Location, ui64SectorCount));

	NDASCOMM_IDE_REGISTER IdeRegister;
	IdeRegister.command.command = 0xEC; // WIN_IDENTIFY
	printf("* Identify the NDAS Unit Device : NdasCommIdeCommand()\n");
	API_CALL(NdasCommIdeCommand(hNDAS, &IdeRegister, NULL, 0, data, 512));
	// data[] now has 512 bytes of identified data as per ANSI NCITS ATA6 rev.1b spec

	BYTE pbData[8];
	const DWORD cbData = sizeof(pbData);


	API_CALL(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_version, pbData, cbData));
	printf("Hardware Version : %d\n", *(BYTE*)pbData);

	NDASCOMM_VCMD_PARAM param_vcmd;
	printf("* get standby timer : NdasCommVendorCommand()\n");
	bResult = NdasCommVendorCommand(hNDAS, ndascomm_vcmd_get_ret_time, &param_vcmd, NULL, 0, NULL, 0);
	if(!bResult)
	{
		if(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED != ::GetLastError())
		{
			API_CALL(FALSE && "NdasCommVendorCommand");
		}
		printf("- Not supported for this Hardware version\n");
	}
	else
	{
		UINT32 TimeValue = param_vcmd.GET_STANDBY_TIMER.TimeValue;
		BOOL EnableTimer = param_vcmd.GET_STANDBY_TIMER.EnableTimer;
		printf("- standby timer : %d, enable : %d\n", TimeValue, EnableTimer);

		param_vcmd.SET_STANDBY_TIMER.TimeValue = TimeValue;
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = EnableTimer ? 0 : 1;
		printf("* set standby timer : NdasCommVendorCommand()\n");
		API_CALL(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_ret_time, &param_vcmd, NULL, 0, NULL, 0));

		TimeValue = param_vcmd.SET_STANDBY_TIMER.TimeValue;
		EnableTimer = param_vcmd.SET_STANDBY_TIMER.EnableTimer;
		printf("- standby timer : %d, enable : %d\n", TimeValue, EnableTimer);
	}

	printf("* Disconnect the connection from the NDAS Device : NdasCommDisconnect()\n");
	API_CALL(NdasCommDisconnect(hNDAS));

	printf("* Uninitialize NDASComm API : NdasCommUninitialize()\n");
	API_CALL(NdasCommUninitialize());

	return 0;
}
