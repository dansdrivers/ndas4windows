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



//////////////////////////////////////////////////////////////////////////
//
//	Option callbacks.
//

int 
CpWriteBacl(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	int i, j;
	_TCHAR *pchtext;
	ULONG ulPartition;
	BLOCK_ACCESS_CONTROL_LIST *BACL_Allocated = NULL;
	PBLOCK_ACCESS_CONTROL_LIST_ELEMENT pBACL_Element;

	// MBR
	BYTE DataMBR[512];
	UINT64 ui64StartSector[4], ui64SectorCount[4];
	CONST UINT32 MBR_PartitionInfoOffset = 0x01be;
	CONST UINT32 MBR_PartitionInfoSize = 16;
	CONST UINT32 MBR_RelativeSectorOffset = 8;
	CONST UINT32 MBR_TotalSectorsOffset = 12;
	//
	//	Get arguments.
	//
	if(argc < 3) {
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
	API_CALL_JMP(NdasCommBlockDeviceRead(hNDAS, 0, 1, DataMBR), out);
	for(i = 0; i < 4; i++)
	{
		ui64StartSector[i] = (UINT64)*(DWORD *)(DataMBR + MBR_PartitionInfoOffset + i * MBR_PartitionInfoSize + MBR_RelativeSectorOffset);
		ui64SectorCount[i] = (UINT64)*(DWORD *)(DataMBR + MBR_PartitionInfoOffset + i * MBR_PartitionInfoSize + MBR_TotalSectorsOffset);
	}

	API_CALL_JMP(BACL_Allocated = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		sizeof(BLOCK_ACCESS_CONTROL_LIST) + (argc -2 -1) * sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT)), out);

	pBACL_Element = &BACL_Allocated->Elements[0];
	for(i = 0; i < argc -2; i++)
	{		
		for(pchtext = argv[2 +i]; *pchtext; pchtext++)
		{
			if(_T('W') == *pchtext)
			{
				pBACL_Element->AccessMask |= BACL_ACCESS_MASK_WRITE;
			}
			else if(_T('R') == *pchtext)
			{
				pBACL_Element->AccessMask |= BACL_ACCESS_MASK_READ;
			}
			else if(_T('S') == *pchtext)
			{
				API_CALL_JMP(
					2 == _stscanf(pchtext +1, _T("%I64d,%I64d"), &pBACL_Element->ui64StartSector, &pBACL_Element->ui64SectorCount),
					out);

				pBACL_Element++;
				BACL_Allocated->ElementCount++;
				break;
			}
			else if(_T('P') == *pchtext)
			{
				API_CALL_JMP(
					1 == _stscanf(pchtext +1, _T("%d"), &ulPartition),
					out);

				pBACL_Element->ui64StartSector = ui64StartSector[ulPartition];
				pBACL_Element->ui64SectorCount = ui64SectorCount[ulPartition];

				pBACL_Element++;
				BACL_Allocated->ElementCount++;
				break;
			}
			else
			{
				_ftprintf(stderr, _T("Unknown parameter. %s\n"), argv[2 +i]);
				goto out;
			}
		}
	}

	// write BACL on disk
	API_CALL_JMP(NdasOpWriteBACL(hNDAS, BACL_Allocated), out);

out:
	if(GetLastError()) {
		_ftprintf(stdout, _T("Error! Code:%08lx\n"), GetLastError());
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	if(BACL_Allocated)
		::HeapFree(::GetProcessHeap(), NULL, BACL_Allocated);

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int 
CpReadBACL(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BLOCK_ACCESS_CONTROL_LIST BACL, *BACL_Allocated = NULL;

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

	// get BACL element count
	BACL.ElementCount = 0;
	API_CALL_JMP(NdasOpReadBACL(hNDAS, &BACL), out);
	if(0 == BACL.ElementCount)
	{
		_ftprintf(stdout, _T("BACL not set on the device\n"));
		goto out;
	}

	// allocate and read BACL
	API_CALL_JMP(BACL_Allocated = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		sizeof(BLOCK_ACCESS_CONTROL_LIST) + (BACL.ElementCount -1) * sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT)), out);
	BACL_Allocated->ElementCount = BACL.ElementCount;
	API_CALL_JMP(NdasOpReadBACL(hNDAS, BACL_Allocated), out);

	_ftprintf(stdout, _T("Partition : W R Start , Count\n"));
	for(UINT32 i = 0; i < BACL_Allocated->ElementCount; i++)
	{
		_ftprintf(stdout, _T("%03d       : %s %s %I64u(%I64x) , %I64u(%I64x)\n"),
			i, 
			(BACL_Allocated->Elements[i].AccessMask & BACL_ACCESS_MASK_WRITE) ? _T("W") : _T(" "),
			(BACL_Allocated->Elements[i].AccessMask & BACL_ACCESS_MASK_READ) ? _T("R") : _T(" "),
			BACL_Allocated->Elements[i].ui64StartSector,
			BACL_Allocated->Elements[i].ui64StartSector,
			BACL_Allocated->Elements[i].ui64SectorCount,
			BACL_Allocated->Elements[i].ui64SectorCount);
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

	if(BACL_Allocated)
		::HeapFree(::GetProcessHeap(), NULL, BACL_Allocated);

	NdasCommUninitialize();

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return GetLastError();
}

int 
CpClearBACL(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	int i, j;
	_TCHAR *pchtext;
	ULONG ulPartition;
	BLOCK_ACCESS_CONTROL_LIST BACL_Empty;

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

	BACL_Empty.ElementCount = 0;
	_ftprintf(stdout, _T("Erasing BACL...\n"));

	// write BACL on disk
	API_CALL_JMP(NdasOpWriteBACL(hNDAS, &BACL_Empty), out);

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

DEF_COMMAND_1(_c_set_bacl, "setbacl", "<NetDisk ID> <Write key> [WR(Pn|Sn,n)] {ex : setbacl <Netdisk ID><Write key> WP0 RS0,63 means writable only to first partition, read only to 63 sectors from 0}")
DEF_COMMAND_1(_c_read_bacl, "readbacl", "<NetDisk ID> <Write key>")
DEF_COMMAND_1(_c_clear_bacl, "clearbacl", "<NetDisk ID> <Write key>")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_set_bacl, CpWriteBacl, 3, 7},
	{ _c_read_bacl, CpReadBACL, 2, 2},
	{ _c_clear_bacl, CpClearBACL, 2, 2},
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
		_T("ndas4routersetup: write partition mask information on DIB of NDAS device for router\n")
		_T("\n")
		_T(" usage: ndas4routersetup <command> [options]\n")
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
