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

  This program is designed using wide char code. But NDASComm supports ASCII
  also.

  Data on the NDAS Unit device can be changed or deleted, do not store any
  important data on the NDAS Unit device before the test go.
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ndas/ndascomm.h> /* include this header file to use NDASComm API */
#include <ndas/ndasmsg.h> /* included to process Last Error from NDASComm API */

#ifdef _DEBUG
#define DBGTRACE(e,stmt) wprintf(L"%s(%d): error %u(%08x): %s\n", __FILE__, __LINE__, e, e, stmt)
#else
#define DBGTRACE(e,stmt) __noop;
#endif

#define API_CALL(_stmt_) \
	do { \
	if (!(_stmt_)) { \
	DBGTRACE(GetLastError(), #_stmt_); \
	return (int)GetLastError(); \
	} \
	} while(0)

#define API_CALL_JMP(_stmt_, jmp_loc) \
	do { \
	if (!(_stmt_)) { \
	DBGTRACE(GetLastError(), #_stmt_); \
	goto jmp_loc; \
	} \
	} while(0)

#define HASH_KEY_USER_X1		0x1F4A50731530EABB
#define HASH_KEY_USER_X2		0x19C567E8F6921D75
#define HASH_KEY_SUPER_X1		0x3e2b321a4750131e
#define HASH_KEY_SUPER_X2		0xCE00983088A66118

static const BYTE NdasOemCodeX1[8] = { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F }; 
static const BYTE NdasOemCodeX2[8] = { 0x75, 0x1D, 0x92, 0xF6, 0xE8, 0x67, 0xC5, 0x19 };
static const BYTE NdasPrivOemCodeX1[8] = { 0x1e, 0x13, 0x50, 0x47, 0x1a, 0x32, 0x2b, 0x3e };
static const BYTE NdasPrivOemCodeX2[8] = { 0x18, 0x61, 0xA6, 0x88, 0x30, 0x98, 0x00, 0xCE };

/*
main function
returns non-zero if any function fails.
*/
int __cdecl wmain( int argc, wchar_t *argv[ ], wchar_t *envp[ ] )
{
	BOOL bResult;
	DWORD dwError = 0;
	DWORD i;

	DWORD dwVersion;

	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;

	PBYTE Buffer;
	DWORD dwBufferLen;
	BYTE data[512]; /* 1 sector sized buffer */
	INT64 i64Location;
	UINT ui64SectorCount;
	NDASCOMM_IDE_REGISTER IdeRegister;
	BYTE pbData[8];
	DWORD cbData = sizeof(pbData);
	NDASCOMM_VCMD_PARAM param_vcmd;
	UINT32 uiTimeValue;
	BOOL bEnableTimer;

	NDAS_DEVICE_HARDWARE_INFO dinfo;

	BYTE vid;

	/* simple check parameter */
	if(2 != argc)
	{
		wprintf(
			L"usage : apitest.exe ID-KEY\n"
			L"\n"
			L"ID-KEY : 20 chars of id and 5 chars of key of the NDAS Device ex(01234ABCDE56789FGHIJ13579)\n"
			L"ex : apitest.exe 01234ABCDE56789FGHIJ13579\n"
			);
		wprintf(L"\n");
		return -1;
	}

	wprintf(L"\n\n\n* Start NDASComm API test on the NDAS Device : %s\n", argv[1]);

	wprintf(L"* Initialize NdasComm library : NdasCommInitialize()\n");
	API_CALL(NdasCommInitialize());

	wprintf(L"* Get API Version : NdasCommGetAPIVersion()\n");
	API_CALL_JMP(dwVersion = NdasCommGetAPIVersion(), out);
	wprintf(L"- Version : Major %d, Minor %d\n",
		(int)LOWORD(dwVersion), (int)HIWORD(dwVersion));

//////////////////////////////////////////////////////////////
//#define	TEST_SET_PASSWORD

#ifdef TEST_SET_PASSWORD
	wprintf(L"### TESTING 'Set user password' ###\n");

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_IDW; /* wide char set */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */

	CopyMemory(
		ci.OEMCode.Bytes,
		NdasOemCodeX1, 
		sizeof(NdasOemCodeX1)); /* HASH_KEY_USER_X1 */ /* Use default password */

	CopyMemory(
		ci.PrivilegedOEMCode.Bytes, 
		NdasPrivOemCodeX1, 
		sizeof(NdasPrivOemCodeX1)); /* HASH_KEY_SUPER_X1 */ /* Log in as super user */

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */

	wcsncpy(ci.NdasIdW.Id, argv[1], 20); /* ID */
	wcsncpy(ci.NdasIdW.Key, argv[1] +20, 5); /* Key */

	wprintf(L"* Connect to the NDAS Device : NdasCommConnect()\n");
	API_CALL_JMP(
		hNDAS = NdasCommConnect(
		&ci,
		0 /* synchronous mode */,
		NULL /* no connection hint */
		),
		out);

	wprintf(L"* Setting user password : NdasCommVendorCommand()\n");

	*(UINT64 *)param_vcmd.SET_SUPERVISOR_PW.SupervisorPassword = HASH_KEY_SUPER_X1;
	bResult = NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_supervisor_pw, &param_vcmd, NULL, 0, NULL, 0);
	if(!bResult)
	{
		if(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED != GetLastError())
		{
			API_CALL_JMP(FALSE && "NdasCommVendorCommand", out);
		}
		wprintf(L"- Not supported for this Hardware version\n");
	}

	*(UINT64 *)param_vcmd.SET_USER_PW.UserPassword = HASH_KEY_USER_X1;
	bResult = NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_user_pw, &param_vcmd, NULL, 0, NULL, 0);
	if(!bResult)
	{
		if(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED != GetLastError())
		{
			API_CALL_JMP(FALSE && "NdasCommVendorCommand", out);
		}
		wprintf(L"- Not supported for this Hardware version\n");
	}

#define TEST_SET_PASSWORD_RESET
#ifdef TEST_SET_PASSWORD_RESET
	wprintf(L"* Resetting : NdasCommVendorCommand()\n");
	bResult = NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0);
	if(!bResult)
	{
		if(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED != GetLastError())
		{
			API_CALL_JMP(FALSE && "NdasCommVendorCommand", out);
		}
		wprintf(L"- Not supported for this Hardware version\n");
	}
#else
	wprintf(L"* Disconnect the connection from the NDAS Device : NdasCommDisconnect()\n");
	API_CALL_JMP(NdasCommDisconnect(hNDAS), out);
#endif //TEST_SET_PASSWORD_RESET
	wprintf(L"### TESTED 'Set user passoword' ###\n");

#endif //TEST_SET_PASSWORD
//////////////////////////////////////////////////////////////

	wprintf(L"* Initialize connection info to create connection to the NDAS Device\n");
	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_IDW; /* wide char set */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	wcsncpy(ci.Address.NdasIdW.Id, argv[1], 20); /* ID */
	wcsncpy(ci.Address.NdasIdW.Key, argv[1] + 20, 5); /* Key */

	wprintf(L"* Connect to the NDAS Device : NdasCommConnect()\n");
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out );

	wprintf(L"* Retrieve NDAS Device ID & unit number : NdasCommGetDeviceID()\n");
	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &vid), out);
	wprintf(L"- DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);

	wprintf(L"* Retrieve the address of the host attached to the NDAS Device : NdasCommGetHostAddress()\n");
	API_CALL_JMP(NdasCommGetHostAddress(hNDAS, NULL, &dwBufferLen), out);
	wprintf(L"- buffer length : %d\n", dwBufferLen);
	Buffer = malloc(dwBufferLen);
	API_CALL_JMP(NdasCommGetHostAddress(hNDAS, Buffer, &dwBufferLen), out);
	wprintf(L"- Host Address : ");
	for(i = 0 ; i < dwBufferLen; i++)
	{
		wprintf(L"%02X", (UINT)Buffer[i]);
	}
	wprintf(L"\n");
	free(Buffer);

	ui64SectorCount = 1;
	i64Location = 0;

	wprintf(L"* Read %d sector(s) of data from Address %d : NdasCommBlockDeviceRead()\n",
		ui64SectorCount, i64Location);
	API_CALL_JMP(NdasCommBlockDeviceRead(hNDAS, i64Location, ui64SectorCount, data), out);

	i64Location = 1;
	wprintf(L"* Write %d sector(s) of data to Address %d : NdasCommBlockDeviceWrite()\n",
		ui64SectorCount, i64Location);
	API_CALL_JMP(NdasCommBlockDeviceWrite(hNDAS, i64Location, ui64SectorCount, data), out);

	ui64SectorCount = 2;
	i64Location = 2;
	wprintf(L"* Verify %d sector(s) from Address %d : NdasCommBlockDeviceVerify()\n",
		ui64SectorCount, i64Location);
	API_CALL_JMP(NdasCommBlockDeviceVerify(hNDAS, i64Location, ui64SectorCount), out);

	IdeRegister.command.command = 0xEC; /* WIN_IDENTIFY */
	IdeRegister.device.dev = 0;
	wprintf(L"* Identify the NDAS Unit Device : NdasCommIdeCommand()\n");
	API_CALL_JMP(NdasCommIdeCommand(hNDAS, &IdeRegister, NULL, 0, data, 512), out);
	/* data[] now has 512 bytes of identified data as per ANSI NCITS ATA6 rev.1b spec */


	ZeroMemory(&dinfo, sizeof(NDAS_DEVICE_HARDWARE_INFO));
	dinfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
	API_CALL_JMP(NdasCommGetDeviceHardwareInfo(hNDAS,&dinfo), out);
	wprintf(L"Hardware Version : %d\n", dinfo.HardwareVersion);

	wprintf(L"* get standby timer : NdasCommVendorCommand()\n");
	bResult = NdasCommVendorCommand(hNDAS, ndascomm_vcmd_get_ret_time, &param_vcmd, NULL, 0, NULL, 0);
	if(!bResult)
	{
		if(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED != GetLastError())
		{
			API_CALL_JMP(FALSE && "NdasCommVendorCommand", out);
		}
		wprintf(L"- Not supported for this Hardware version\n");
	}
	else
	{
		uiTimeValue = param_vcmd.GET_STANDBY_TIMER.TimeValue;
		bEnableTimer = param_vcmd.GET_STANDBY_TIMER.EnableTimer;
		wprintf(L"- standby timer : %d, enable : %d\n", uiTimeValue, bEnableTimer);

		param_vcmd.SET_STANDBY_TIMER.TimeValue = uiTimeValue;
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = bEnableTimer ? 0 : 1;
		wprintf(L"* set standby timer : NdasCommVendorCommand()\n");
		API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_ret_time, &param_vcmd, NULL, 0, NULL, 0), out);

		uiTimeValue = param_vcmd.SET_STANDBY_TIMER.TimeValue;
		bEnableTimer = param_vcmd.SET_STANDBY_TIMER.EnableTimer;
		wprintf(L"- standby timer : %d, enable : %d\n", uiTimeValue, bEnableTimer);
	}

	wprintf(L"* Disconnect the connection from the NDAS Device : NdasCommDisconnect()\n");
	API_CALL_JMP(NdasCommDisconnect(hNDAS), out);

	wprintf(L"* Uninitialize NDASComm API : NdasCommUninitialize()\n");

out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	API_CALL(NdasCommUninitialize());
	return GetLastError();
}
