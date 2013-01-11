/*
Copyright(C) 2002-2005 XIMETA, Inc.
*/

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////
//
//	OemCode
//

//
//	XIMETA NDAS chip Ver 2.0
//

static const BYTE NdasPrivOemCodeX1[8] = { 0x1e, 0x13, 0x50, 0x47, 0x1a, 0x32, 0x2b, 0x3e };


//////////////////////////////////////////////////////////////////////////
//
//
//

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

#ifdef _DEBUG
#define DBGTRACE(e,stmt) _ftprintf(stderr, L"%s(%d): error %u(%08x): %s\n", __FILE__, __LINE__, e, e, stmt)
#else
#define DBGTRACE(e,stmt) __noop;
#endif



//////////////////////////////////////////////////////////////////////////
//
//	Option callbacks.
//


int
CpSetDeviceStandby(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BYTE DeviceID[6];
	DWORD dwUnitNo;
	NDASCOMM_VCMD_PARAM param_vcmd;
	LONG				standByTimeOut;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	standByTimeOut = _tstol(argv[2]);
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
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_W; /* wide char set */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.bWriteAccess = FALSE; /* Connect with read-write privilege */
	ci.protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	CopyMemory(ci.PrivilegedOEMCode.Bytes, NdasPrivOemCodeX1, sizeof(NdasPrivOemCodeX1));  /* Log in as super user */
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.DeviceIDW.wszDeviceStringId, argv[0], 20); /* ID */
	_tcsncpy(ci.DeviceIDW.wszDeviceStringKey, argv[1], 5); /* Key */
	API_CALL_JMP(
		hNDAS = NdasCommConnect(
		&ci,
		0 /* synchronous mode */,
		NULL /* no connection hint */
		),
		out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, DeviceID, &dwUnitNo), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Query Standby timer
	//

	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	RtlZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));

	if(standByTimeOut) {
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = 1;
		param_vcmd.SET_STANDBY_TIMER.TimeValue = standByTimeOut;
	} else {
		param_vcmd.SET_STANDBY_TIMER.EnableTimer = 0;
		param_vcmd.SET_STANDBY_TIMER.TimeValue = 0;
	}
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_set_standby_timer, &param_vcmd, NULL, 0, NULL, 0), out);

	_ftprintf(stderr, _T("Resetting the device...\n"));
	RtlZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
	API_CALL_JMP(NdasCommVendorCommand(hNDAS, ndascomm_vcmd_reset, &param_vcmd, NULL, 0, NULL, 0), out);

out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

#define MAX_CONNECTION_COUNT 64

int
CpQueryDeviceStandby(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS[MAX_CONNECTION_COUNT] = {NULL};
	NDASCOMM_CONNECTION_INFO ci[MAX_CONNECTION_COUNT];
	BYTE DeviceID[6];
	DWORD dwUnitNo[MAX_CONNECTION_COUNT];
	NDASCOMM_VCMD_PARAM param_vcmd[MAX_CONNECTION_COUNT];
	int i;
	int ret;
	int j;
	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	API_CALL(NdasCommInitialize());

	for(j=0;j<10000;j++) {
		_ftprintf(stdout, _T("Starting the %d operation...\n"), j+1);
		//
		//	Connect and login
		//
		_tprintf(_T("Connecting "));
		for(i=0;i<MAX_CONNECTION_COUNT;i++) {
			_tprintf(_T(" %d"), i);			
			ZeroMemory(&ci, sizeof(ci[i]));
			ci[i].address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_W; /* wide char set */
			ci[i].UnitNo = 0; /* Use first Unit Device */
			ci[i].bWriteAccess = FALSE; /* Connect with read-write privilege */
			ci[i].protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
			ci[i].OEMCode.UI64Value = 0; /* Use default password */
			ci[i].PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
			ci[i].login_type = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
			_tcsncpy(ci[i].DeviceIDW.wszDeviceStringId, argv[0], 20); /* ID */
			_tcsncpy(ci[i].DeviceIDW.wszDeviceStringKey, argv[1], 5); /* Key */
			hNDAS[i] = NdasCommConnect(
							&ci[i],
							0 /* synchronous mode */,
							NULL /* no connection hint */
							);
			if (hNDAS[i] ==0)
				break;
		}
		_tprintf(_T("\n"));
#if 0
		//
		//	Display NDAS device info
		//

		API_CALL_JMP(NdasCommGetDeviceID(hNDAS, DeviceID, &dwUnitNo), out);
		_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
			DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
			(int)dwUnitNo);
#endif
		_tprintf(_T("Querying "));
		for(i=0;i<MAX_CONNECTION_COUNT;i++) {
			if (hNDAS[i]) {
				_tprintf(_T(" %d"), i);	
				RtlZeroMemory(&param_vcmd, sizeof(NDASCOMM_VCMD_PARAM));
				ret = NdasCommVendorCommand(hNDAS[i], ndascomm_vcmd_get_standby_timer, &param_vcmd[i], NULL, 0, NULL, 0);
				if (ret==0) 
					_tprintf(_T("Querying %d failed\n"), i);	
			}
		}
		_tprintf(_T("\n"));

		_tprintf(_T("Disconnecting "));	
		for(i=0;i<MAX_CONNECTION_COUNT;i++) {
			if (hNDAS[i]) {
				_tprintf(_T(" %d"), i);			
				NdasCommDisconnect(hNDAS[i]);
				hNDAS[i] = NULL;
			}
		}
		_tprintf(_T("\n"));		
	}
	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	if(param_vcmd[0].GET_STANDBY_TIMER.EnableTimer) {
		_ftprintf(stderr, _T("\nStandby timeout: %d minutes.\n"), param_vcmd[0]	.GET_STANDBY_TIMER.TimeValue);
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

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));
	API_CALL(NdasCommInitialize());


	//
	//	Connect and login
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_W; /* wide char set */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.bWriteAccess = TRUE; /* Connect with read-write privilege */
	ci.protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	_tcsncpy(ci.DeviceIDW.wszDeviceStringId, argv[0], 20); /* ID */
	_tcsncpy(ci.DeviceIDW.wszDeviceStringKey, argv[1], 5); /* Key */
	API_CALL_JMP(
		hNDAS = NdasCommConnect(
		&ci,
		0 /* synchronous mode */,
		NULL /* no connection hint */
		),
		out);

	//
	//	Display NDAS device info
	//

	API_CALL_JMP(NdasCommGetDeviceID(hNDAS, DeviceID, &dwUnitNo), out);
	_tprintf(L"DeviceID : %02X%02X%02X%02X%02X%02X, Unit No. : %d\n",
		DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3], DeviceID[4], DeviceID[5],
		(int)dwUnitNo);


	//
	//	Request the NDAS device to enter Standby mode
	//
	RtlZeroMemory(&IdeRegister, sizeof(NDASCOMM_IDE_REGISTER));
	IdeRegister.command.command = 0xE0; /* WIN_STANDBYNOW1 */
	IdeRegister.device.dev = 0;
	API_CALL_JMP(NdasCommIdeCommand(hNDAS, &IdeRegister, NULL, 0, NULL, 0), out);

out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

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
#define DEF_COMMAND_2(c,x,y,h) LPCTSTR c [] = {_T(x), _T(y), NULL, _T(h), NULL};
#define DEF_COMMAND_3(c,x,y,z,h) LPCTSTR c [] = {_T(x), _T(y), _T(z), NULL, _T(h), NULL};
#define DEF_COMMAND_4(c,x,y,z,w,h) LPCTSTR c[] = {_T(x), _T(y), _T(z), _T(w), NULL, _T(h), NULL};

DEF_COMMAND_2(_c_set_standby, "set", "standby", "<NetDisk ID> <Write key> <timeout minutes>")
DEF_COMMAND_2(_c_query_standby, "query", "standby", "<NetDisk ID> <Write key>")
DEF_COMMAND_2(_c_standby_now, "now", "standby", "<NetDisk ID> <Write key>")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_set_standby, CpSetDeviceStandby, 3, 3},
	{ _c_query_standby, CpQueryDeviceStandby, 2, 2},
	{ _c_standby_now, CpDeviceStandby, 2, 2},
};

DWORD GetStringMatchLength(LPCTSTR szToken, LPCTSTR szCommand)
{
	DWORD i = 0;
	for (; szToken[i] != _T('\0') && szCommand[i] != _T('\0'); ++i) {
		TCHAR x[2] = { szToken[i], 0};
		TCHAR y[2] = { szCommand[i], 0};
		if (*CharLower(x) != *CharLower(y))
			if (szToken[i] == _T('\0')) return i;
			else return 0;
	}
	if (szToken[i] != _T('\0')) {
		return 0;
	}
	return i;
}

void FindPartialMatchEntries(
							 LPCTSTR szToken, DWORD dwLevel,
							 SIZE_T* pCand, SIZE_T* pcCand)
{
	DWORD maxMatchLen = 1, matchLen = 0;
	LPCTSTR szCommand = NULL;
	SIZE_T* pNewCand = pCand;
	SIZE_T* pCurNewCand = pCand;
	SIZE_T cNewCand = 0;
	SIZE_T i;

	for (i = 0; i < *pcCand; ++i) {
		szCommand = _cproc_table[pCand[i]].szCommands[dwLevel];
		matchLen = GetStringMatchLength(szToken, szCommand);
		if (matchLen > maxMatchLen) {
			maxMatchLen = matchLen;
			pCurNewCand = pNewCand;
			*pCurNewCand = pCand[i];
			++pCurNewCand;
			cNewCand = 1;
		} else if (matchLen == maxMatchLen) {
			*pCurNewCand = pCand[i];
			++pCurNewCand;
			++cNewCand;
		}
	}

	*pcCand = cNewCand;

	return;
}

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

void PrintCand(const SIZE_T* pCand, SIZE_T cCand)
{
	SIZE_T i;

	for (i = 0; i < cCand; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(pCand[i]);
		_tprintf(_T("\n"));
	}
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
	SIZE_T candIndices[MAX_CANDIDATES] = {0};
	SIZE_T cCandIndices = MAX_CANDIDATES;
	DWORD dwLevel = 0;
	SIZE_T i;

	if (argc < 2) {
		usage();
		return -1;
	}

	for (i = 0; i < cCandIndices; ++i) {
		candIndices[i] = i;
	}

	for (dwLevel = 0; dwLevel + 1 < (DWORD) argc; ++dwLevel) {

		FindPartialMatchEntries(
			argv[1 + dwLevel],
			dwLevel,
			candIndices,
			&cCandIndices);

		if (cCandIndices == 1) {

#if _DEBUG
			_tprintf(_T("Running: "));
			PrintCand(candIndices, cCandIndices);
#endif

			return _cproc_table[candIndices[0]].
				proc(argc - dwLevel - 3, &argv[dwLevel + 3]);

		} else if (cCandIndices == 0) {

			_tprintf(_T("Error: Unknown command.\n\n"));
			usage();
			break;

		} else if (cCandIndices > 1) {

			BOOL fStop = FALSE;
			SIZE_T i;

			// if the current command parts are same, proceed,
			// otherwise emit error
			for ( i = 1; i < cCandIndices; ++i) {
				if (0 != lstrcmpi(
					_cproc_table[candIndices[0]].szCommands[dwLevel],
					_cproc_table[candIndices[i]].szCommands[dwLevel]))
				{
					_tprintf(_T("Error: Ambiguous command:\n\n"));
					PrintCand(candIndices, cCandIndices);
					_tprintf(_T("\n"));
					fStop = TRUE;
					break;
				}
			}

			if (fStop) {
				break;
			}
		}

		// more search

		if (dwLevel + 2 >= (DWORD) argc) {
			_tprintf(_T("Error: Incomplete command:\n\n"));
			PrintCand(candIndices, cCandIndices);
			_tprintf(_T("\n"));
		}
	}

	return -1;
}
