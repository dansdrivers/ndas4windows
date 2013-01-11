/*
Copyright(C) 2002-2005 XIMETA, Inc.
*/

#include "stdafx.h"

static const NDAS_OEM_CODE
NDAS_PRIVILEGED_OEM_CODE_DEFAULT = {
	0x1E, 0x13, 0x50, 0x47, 0x1A, 0x32, 0x2B, 0x3E };

//////////////////////////////////////////////////////////////////////////

#define API_CALL(_stmt_) \
	do { \
	if (!(_stmt_)) { \
	DBGTRACE(GetLastError(), #_stmt_); \
	return (int)GetLastError(); \
	} else { \
	SetLastError(0); \
	} \
	} while(0)

#define API_CALL_JMP(_stmt_, jmp_loc) \
	do { \
	if (!(_stmt_)) { \
	DBGTRACE(GetLastError(), #_stmt_); \
	goto jmp_loc; \
	} else { \
	SetLastError(0); \
	} \
	} while(0)

#ifdef _DEBUG
#define DBGTRACE(e,stmt) _ftprintf(stderr, L"%s(%d): error %u(%08x): %s\n", __FILE__, __LINE__, e, e, stmt)
#else
#define DBGTRACE(e,stmt) __noop;
#endif

int
CpSetDeviceStandby(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG				standByTimeOut;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	standByTimeOut = _tcstoul(argv[2], NULL, 10);
	if(standByTimeOut == 0) {

		//
		//	Check to see if a user inputs the zero.
		//

		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
				_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
				return -1;
		}
	}



	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	/* Log in as super user */
	ci.PrivilegedOEMCode = NDAS_PRIVILEGED_OEM_CODE_DEFAULT;
	/* Privileged Connection cannot use lock commands */
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out );

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Set Standby timer
	//

	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));

	if(standByTimeOut) {
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = 1;
		param_vcmd.SET_STANDBY_TIMER.TimeValue = standByTimeOut;
	} else {
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = 0;
		param_vcmd.SET_STANDBY_TIMER.TimeValue = 0;
	}
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_standby_timer, &param_vcmd, NULL, 0, NULL, 0), out);

	_ftprintf(stderr, _T("Resetting the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpQueryDeviceStandby(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out );

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Query Standby timer
	//

	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_get_standby_timer, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	if(param_vcmd.GET_STANDBY_TIMER.EnableTimer) {
		_ftprintf(stderr, _T("\nStandby timeout: %d minutes.\n"), param_vcmd.GET_STANDBY_TIMER.TimeValue);
	} else {
		_ftprintf(stderr, _T("\nStandby timeout: Disabled.\n"));
	}

	return GetLastError();
}

int
CpDeviceStandby(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_IDE_REGISTER IdeRegister;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID*/
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Request the NDAS device to enter Standby mode
	//
	ZeroMemory(&IdeRegister, sizeof(NDASCOMM_IDE_REGISTER));
	IdeRegister.command.command = 0xE0; /* WIN_STANDBYNOW1 */
	IdeRegister.device.dev = 0;
	API_CALL_JMP(NdasCommIdeCommand(hNDAS, &IdeRegister, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}


int
CpSetLock(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG		lockIdx;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	lockIdx = _tcstoul(argv[2], NULL, 10);
	if(lockIdx == 0) {

		//
		//	Check to see if a user inputs the zero.
		//

		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
				_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
				return -1;
		}
	}



	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-only privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out );

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Set NDAS device's lock
	//

	_ftprintf(stderr, _T("Acquiring lock #%u of  the device...\n"), (ULONG)(UINT8)lockIdx);
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	param_vcmd.GET_SEMA.Index = (UINT8)lockIdx;
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_sema, &param_vcmd, NULL, 0, NULL, 0), out);


out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}


int
CpFreeLock(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG		lockIdx;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	lockIdx = _tcstoul(argv[2], NULL, 10);
	if(lockIdx == 0) {

		//
		//	Check to see if a user inputs the zero.
		//

		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
				_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
				return -1;
		}
	}



	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-only privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Free NDAS device's lock
	//

	_ftprintf(stderr, _T("Freeing lock #%u of  the device...\n"), (ULONG)(UINT8)lockIdx);
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	param_vcmd.FREE_SEMA.Index = (UINT8)lockIdx;
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_free_sema, &param_vcmd, NULL, 0, NULL, 0), out);


out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpQueryLockOwner(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG		lockIdx;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	lockIdx = _tcstoul(argv[2], NULL, 10);
	if(lockIdx == 0) {

		//
		//	Check to see if a user inputs the zero.
		//

		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
				_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
				return -1;
		}
	}



	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-only privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out );

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Query lock owner
	//

	_ftprintf(stderr, _T("Querying lock #%u's owner of  the device...\n"), (ULONG)(UINT8)lockIdx);
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	param_vcmd.GET_OWNER_SEMA.Index = (UINT8)lockIdx;
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_get_owner_sema, &param_vcmd, NULL, 0, NULL, 0), out);


out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	} else {
		_ftprintf(stdout, _T("Owner's addr:%02X%02X%02X%02X%02X%02X%02X%02X\n"),
			param_vcmd.GET_OWNER_SEMA.AddressLPX[0],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[1],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[2],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[3],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[4],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[5],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[6],
			param_vcmd.GET_OWNER_SEMA.AddressLPX[7]);
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpSetConTimeout(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG				conTimeOut;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	conTimeOut = _tcstoul(argv[2], NULL, 10);
	if(conTimeOut == 0) {

		//
		//	Check to see if a user inputs the zero.
		//

		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
				_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
				return -1;
		}
	}



	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	/* Log in as super user */
	ci.PrivilegedOEMCode = NDAS_PRIVILEGED_OEM_CODE_DEFAULT;
	/* Privileged Connection cannot use lock commands */
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Set connection timeout
	//

	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));

	param_vcmd.SET_MAX_CONN_TIME.MaxConnTime = conTimeOut;
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_max_conn_time, &param_vcmd, NULL, 0, NULL, 0), out);

	_ftprintf(stderr, _T("Resetting the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpQueryConTimeout(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Query Connection timeout
	//

	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_get_max_conn_time, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	_ftprintf(stderr, _T("\nConnection timeout: %d.\n"), param_vcmd.GET_MAX_CONN_TIME.MaxConnTime);

	return GetLastError();
}

int
CpSetMacAddress(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	_TCHAR *MacAddress;
	size_t argv_len;
	ULONG	MacAddressSegs[6];
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	// Mac address : XX:XX:XX:XX:XX:XX
	MacAddress = argv[2];

	if(17 != _tcslen(MacAddress) ||
		_T(':') != MacAddress[2] ||
		_T(':') != MacAddress[5] ||
		_T(':') != MacAddress[8] ||
		_T(':') != MacAddress[11] ||
		_T(':') != MacAddress[14])
	{
		_ftprintf(stderr, _T("ERROR: Invalid MAC address. \n"));
		return -1;
	}

	if(6 != _stscanf(MacAddress, _T("%02X:%02X:%02X:%02X:%02X:%02X"),
		&MacAddressSegs[0],
		&MacAddressSegs[1],
		&MacAddressSegs[2],
		&MacAddressSegs[3],
		&MacAddressSegs[4],
		&MacAddressSegs[5]))
	{
		_ftprintf(stderr, _T("ERROR: Invalid MAC address. \n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	/* Log in as super user */
	ci.PrivilegedOEMCode = NDAS_PRIVILEGED_OEM_CODE_DEFAULT;
	/* Privileged Connection cannot use lock commands */
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Set Mac address
	//

	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));

	param_vcmd.SET_LPX_ADDRESS.AddressLPX[0] = (BYTE)MacAddressSegs[0];
	param_vcmd.SET_LPX_ADDRESS.AddressLPX[1] = (BYTE)MacAddressSegs[1];
	param_vcmd.SET_LPX_ADDRESS.AddressLPX[2] = (BYTE)MacAddressSegs[2];
	param_vcmd.SET_LPX_ADDRESS.AddressLPX[3] = (BYTE)MacAddressSegs[3];
	param_vcmd.SET_LPX_ADDRESS.AddressLPX[4] = (BYTE)MacAddressSegs[4];
	param_vcmd.SET_LPX_ADDRESS.AddressLPX[5] = (BYTE)MacAddressSegs[5];
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_lpx_address, &param_vcmd, NULL, 0, NULL, 0), out);

	_ftprintf(stderr, _T("Resetting the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpSetRetTime(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG				retTime;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	retTime = _tcstoul(argv[2], NULL, 10);
	if(retTime == 0) {

		//
		//	Check to see if a user inputs the zero.
		//

		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
				_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
				return -1;
		}
	}



	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	/* Log in as super user */
	ci.PrivilegedOEMCode = NDAS_PRIVILEGED_OEM_CODE_DEFAULT;
	/* Privileged Connection cannot use lock commands */
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Set connection timeout
	//

	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));

	param_vcmd.SET_RET_TIME.RetTime = retTime;
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_ret_time, &param_vcmd, NULL, 0, NULL, 0), out);

	_ftprintf(stderr, _T("Resetting the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpQueryRetTime(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Query Retransmission time
	//

	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_get_ret_time, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	_ftprintf(stderr, _T("\nRetransmission time: %d milli-seconds.\n"), param_vcmd.GET_MAX_CONN_TIME.MaxConnTime);

	return GetLastError();
}

int
CpSetEncryptionMode(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	ULONG				HeaderEnc;
	ULONG				DataEnc;	
	BYTE VID;

	//
	//	Get arguments.
	//
	if(argc < 4) {
		_ftprintf(stderr, _T("ERROR: Header encryption mode and data encryption mode is required.\n"));
		return -1;
	}

	HeaderEnc = _tcstoul(argv[2], NULL, 10);
	if(HeaderEnc != 0 && HeaderEnc!=1) {
		_ftprintf(stderr, _T("ERROR: Avaiable header encryption mode is 0 (off) or 1 (hash encryption).\n"));
		return -1;
	}

	DataEnc = _tcstoul(argv[3], NULL, 10);
	if(DataEnc != 0 && DataEnc!=1) {
		_ftprintf(stderr, _T("ERROR: Avaiable header encryption mode is 0 (off) or 1 (hash encryption).\n"));
		return -1;
	}

	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	/* Log in as super user */
	ci.PrivilegedOEMCode = NDAS_PRIVILEGED_OEM_CODE_DEFAULT;
	/* Privileged Connection cannot use lock commands */
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Set encryption mode
	//

	_tprintf(_T("Header encryption = %d Data encryption = %d\n"), HeaderEnc, DataEnc);
	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));

	param_vcmd.SET_ENC_OPT.EncryptHeader = HeaderEnc;
	param_vcmd.SET_ENC_OPT.EncryptData = DataEnc;	

	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_enc_opt, &param_vcmd, NULL, 0, NULL, 0), out);

	_ftprintf(stderr, _T("Resetting the device...\n"));
	ZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int
CpGetEncryptionMode(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	BYTE VID;
	NDAS_DEVICE_HARDWARE_INFO ndasHardwareInfo = {0};

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	SetLastError(0);
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID; /* Use NDAS ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	API_CALL_JMP( hNDAS = NdasCommConnect(&ci), out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, NULL, DeviceID, &dwUnitNo, &VID), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);

	_tprintf(L"\nNDAS communication encryption information\n");
	ndasHardwareInfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
	BOOL success = NdasCommGetDeviceHardwareInfo(hNDAS, &ndasHardwareInfo);
	if (!success) 
	{
		_tprintf(L"Failed to retreive hardware information\n");
	}
	else
	{
		_tprintf(L"Header encryption mode = %d \n", ndasHardwareInfo.HeaderEncryptionMode);
		_tprintf(L"Data encryption mode = %d \n", ndasHardwareInfo.DataEncryptionMode);
		_tprintf(L"\n");
		SetLastError(0);
	}


out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

//	_ftprintf(stderr, _T("\nRetransmission time: %d milli-seconds.\n"), param_vcmd.GET_MAX_CONN_TIME.MaxConnTime);

	return GetLastError();
}





//////////////////////////////////////////////////////////////////////////
//
// command line client for NDAS device management
//
// usage:
// ndascmd
//
// help or ?
// set standby
// query standby
//

typedef int (*CMDPROC)(int argc, _TCHAR* argv[]);

typedef struct _CPROC_ENTRY {
	LPCTSTR* szCommands;
	CMDPROC proc;
	DWORD nParamMin;
	DWORD nParamMax;
} CPROC_ENTRY, *PCPROC_ENTRY, *CPROC_TABLE;

#define DEF_COMMAND_1(c,x,h) LPCTSTR c [] = {_T(x), NULL, _T(h), NULL};

DEF_COMMAND_1(_c_set_standby, "setstandby", "<NetDisk ID> <Write key> <timeout(minutes)>")
DEF_COMMAND_1(_c_query_standby, "querystandby", "<NetDisk ID> <Write key>")
DEF_COMMAND_1(_c_standby_now, "nowstandby", "<NetDisk ID> <Write key>")
DEF_COMMAND_1(_c_set_lock, "setlock", "<NetDisk ID> <Write key> <lock index>")
DEF_COMMAND_1(_c_free_lock, "freelock", "<NetDisk ID> <Write key> <lock index>")
DEF_COMMAND_1(_c_query_lockowner, "querylockowner", "<NetDisk ID> <Write key> <lock index>")
DEF_COMMAND_1(_c_set_contimeout, "setcontimeout", "<NetDisk ID> <Write key> <time out(ver 2: second, 1.1 or less: milli-second)>")
DEF_COMMAND_1(_c_query_contimeout, "querycontimeout", "<NetDisk ID> <Write key>")
DEF_COMMAND_1(_c_set_rettime, "setrettime", "<NetDisk ID> <Write key> <Retransmission time(milli-seconds(?))>")
DEF_COMMAND_1(_c_set_macaddress, "setmacaddress", "<NetDisk ID> <Write key> <Mac address XX:XX:XX:XX:XX:XX>")
DEF_COMMAND_1(_c_query_rettime, "queryrettime", "<NetDisk ID> <Write key>")
DEF_COMMAND_1(_c_set_encmode, "setencmode", "<NetDisk ID> <Write key> <HeadEnc> <DataEnc>")
DEF_COMMAND_1(_c_query_encmode, "queryencmode", "<NetDisk ID> <Write key>")


// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_set_standby, CpSetDeviceStandby, 3, 3},
	{ _c_query_standby, CpQueryDeviceStandby, 2, 2},
	{ _c_standby_now, CpDeviceStandby, 2, 2},
	{ _c_set_lock, CpSetLock, 3, 3},
	{ _c_free_lock, CpFreeLock, 3, 3},
	{ _c_query_lockowner, CpQueryLockOwner, 3, 3},
	{ _c_set_contimeout, CpSetConTimeout, 3, 3},
	{ _c_query_contimeout, CpQueryConTimeout, 2, 2},
	{ _c_set_rettime, CpSetRetTime, 3, 3},
	{ _c_set_macaddress, CpSetMacAddress, 3, 3},
	{ _c_query_rettime, CpQueryRetTime, 2, 2},
	{ _c_set_encmode, CpSetEncryptionMode, 4, 4},
	{ _c_query_encmode, CpGetEncryptionMode, 2, 2},	
};

void PrintCmd(SIZE_T index)
{
	SIZE_T j = 0;
	_tprintf(_T("%s"), _cproc_table[index].szCommands[j]);
	for (j = 1; _cproc_table[index].szCommands[j]; ++j) {
		_tprintf(_T(" %s"), _cproc_table[index].szCommands[j]);
	}
}

void PrintOpt(SIZE_T index)
{
	LPCTSTR* ppsz = _cproc_table[index].szCommands;

	for (; NULL != *ppsz; ++ppsz) {
		__noop;
	}

	_tprintf(_T("%s"), *(++ppsz));

}


void usage()
{
	const SIZE_T nCommands =
		sizeof(_cproc_table) / sizeof(_cproc_table[0]);
	SIZE_T i;

	_tprintf(
		_T("Copyright (C) 2003-2005 XIMETA, Inc.\n")
		_T("vendorctl: command line vendor-control for NDAS device\n")
		_T("\n")
		_T(" usage: vendorctl <command> [options]\n")
		_T("\n"));

	for (i = 0; i < nCommands; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(i);
		_tprintf(_T(" "));
		PrintOpt(i);
		_tprintf(_T("\n"));
	}

	_tprintf(_T("\n<NetDisk ID> : 20 characters\n"));
	_tprintf(_T("<WriteKey>   : 5 characters\n"));
}


#define MAX_CANDIDATES RTL_NUMBER_OF(_cproc_table)

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	SIZE_T cCandIndices = MAX_CANDIDATES;
	SIZE_T i;

	if (argc < 2) {
		usage();
		return -1;
	}

	for (i = 0; i < cCandIndices; ++i) {
		if(_tcscmp(_cproc_table[i].szCommands[0], argv[1]) == 0) {
			break;
		}
	}


	if (i < cCandIndices) {

		return _cproc_table[i].
			proc(argc - 2, &argv[2]);
	} else {
		usage();
	}

	return -1;
}
