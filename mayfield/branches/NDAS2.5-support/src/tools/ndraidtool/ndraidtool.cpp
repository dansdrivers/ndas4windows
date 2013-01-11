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
/* Little endian UINT64 representation */
static const NDAS_OEM_CODE NDAS_DEFAULT_PRIVILEGED_OEM_CODE = { 0x3E2B321A4750131E };

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

BOOL ConvertMacToNodeId(
	_TCHAR* pStr,
	PNDAS_DEVICE_ID pId
) {
	_TCHAR* pStart, *pEnd;

	if(pStr == NULL)
		return FALSE;

	pStart = pStr;

	for(int i = 0; i < 6; i++) {
		
		pId->Node[i] = (UCHAR)_tcstoul(pStart, &pEnd, 16);
		
		pStart += 3;
	}
	return TRUE;
}

int 
CpComare(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS1 = NULL;
	HNDAS hNDAS2 = NULL;	
	NDASCOMM_CONNECTION_INFO ci;
	BOOL bResults;
	PBYTE Buffer1 = NULL, Buffer2 = NULL;
	NDAS_DIB_V2 Dib;
	static const SectorPerOp = 64;
	NDAS_DEVICE_ID SrcAddr1, SrcAddr2;
	UINT64 StartAddr;
	UINT64 IoLength;
	UINT32 MisMatchCount;
	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}
	
	_ftprintf(stderr, _T("%s %s\n"), argv[0], argv[1]);
	
	// Argv[0]: SrcAddr1
	bResults = ConvertMacToNodeId(argv[0], &SrcAddr1);
	if (!bResults) {
		_ftprintf(stderr, _T("Invalid device ID.\n"));
		return -1;
	}

	bResults = ConvertMacToNodeId(argv[1], &SrcAddr2);
	if (!bResults) {
		_ftprintf(stderr, _T("Invalid device ID.\n"));
		return -1;
	}

	Buffer1 = (PBYTE)malloc(SectorPerOp * 512);
	Buffer2 = (PBYTE)malloc(SectorPerOp * 512);
	
	SetLastError(0);
	API_CALL(NdasCommInitialize());

	//
	//	Connect and login to source 1
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; 
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */

	ci.Address.DeviceId = SrcAddr1;

	API_CALL_JMP( hNDAS1 = NdasCommConnect(&ci), out);

	//
	//	Connect and login to source 2
	//

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; 
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = FALSE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0; /* Log in as normal user */
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */

	ci.Address.DeviceId = SrcAddr2;

	API_CALL_JMP( hNDAS2 = NdasCommConnect(&ci), out);

	bResults = NdasCommBlockDeviceRead(
		hNDAS1, 
		NDAS_BLOCK_LOCATION_DIB_V2, 
		1, 
		(PBYTE)&Dib);
	if (!bResults) {
		goto out;
	}
	if (Dib.Signature != NDAS_DIB_V2_SIGNATURE) {
		_ftprintf(stdout, _T("Dib not found\n"));
		goto out;
	}
	// Compare 
	_ftprintf(stdout, _T("Comparing 0x%I64x sectors\n"), Dib.sizeUserSpace);
	StartAddr = 0;
	IoLength = SectorPerOp;
	MisMatchCount = 0;
	while(TRUE) {
		if (StartAddr + IoLength >= Dib.sizeUserSpace) {
			// Last part.
			IoLength = Dib.sizeUserSpace - StartAddr;
		} 
		bResults = NdasCommBlockDeviceRead(
			hNDAS1, 
			StartAddr, 
			IoLength, 
			Buffer1);
		if (!bResults) {
			_ftprintf(stdout, _T("Failed to read from source 1\n"));
			goto out;
		}
		bResults = NdasCommBlockDeviceRead(
			hNDAS2, 
			StartAddr, 
			IoLength, 
			Buffer2);
		if (!bResults) {
			_ftprintf(stdout, _T("Failed to read from source 2\n"));
			goto out;
		}
		if (memcmp(Buffer1, Buffer2, (size_t)IoLength * 512) == 0) {
			
		} else {
			MisMatchCount++;
			_ftprintf(stdout, _T("Mismatch at 0x%I64x:%x\n"), StartAddr, IoLength);
			if (MisMatchCount > 20) {
				_ftprintf(stdout, _T("Too much mismatch. Exiting\n"));	
				break;
			}
		}
		if (StartAddr%(100 * 1024 * 2)==0) { // Print progress in every 100M
			_ftprintf(stdout, _T("%d%%\n"), StartAddr*100/Dib.sizeUserSpace);
		}
		StartAddr += IoLength;
		if (StartAddr >= Dib.sizeUserSpace) {
			break;
		}
	}
out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}
	if (Buffer1)
		free(Buffer1);
	if (Buffer2)
		free(Buffer2);
	if(hNDAS1)
	{
		NdasCommDisconnect(hNDAS1);
		hNDAS1 = NULL;
	}

	if(hNDAS2)
	{
		NdasCommDisconnect(hNDAS2);
		hNDAS2 = NULL;
	}

	NdasCommUninitialize();

	return GetLastError();
}

typedef int (*CMDPROC)(int argc, _TCHAR* argv[]);

typedef struct _CPROC_ENTRY {
	LPCTSTR* szCommands;
	CMDPROC proc;
	DWORD nParamMin;
	DWORD nParamMax;
} CPROC_ENTRY, *PCPROC_ENTRY, *CPROC_TABLE;

#define DEF_COMMAND_1(c,x,h) LPCTSTR c [] = {_T(x), NULL, _T(h), NULL};

DEF_COMMAND_1(_c_compare, "compare", "Compare contents of the two netdisk")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_compare, CpComare, 2, 2},
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
		_T("Copyright (C) 2003-2006 XIMETA, Inc.\n")
	);

	for (i = 0; i < nCommands; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(i);
		_tprintf(_T(" "));
		PrintOpt(i);
		_tprintf(_T("\n"));
	}
}


#define MAX_CANDIDATES RTL_NUMBER_OF(_cproc_table)

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	SIZE_T cCandIndices = MAX_CANDIDATES;
	SIZE_T i;

	DWORD dwOpAPIVersion = NdasOpGetAPIVersion();

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

