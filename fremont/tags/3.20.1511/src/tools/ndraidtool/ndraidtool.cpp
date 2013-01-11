/*
Copyright(C) 2002-2005 XIMETA, Inc.
*/

#include "stdafx.h"

/* Little endian UINT64 representation */

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
CpInfo(int argc, _TCHAR* argv[])
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	BOOL bResults;
	NDAS_DEVICE_ID SrcAddr1;
	NDASOP_RAID_INFO RaidInfo = {0};
	PTCHAR str;
	PTCHAR EmptyStr=_T("");
	DWORD MemberCount;
	DWORD i, j;
	DWORD Flags;
	PNDAS_OOS_BITMAP_BLOCK BmpBuffer = NULL;
	
	//
	//	Get arguments.
	//
	if(argc < 1) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}
	_ftprintf(stderr, _T("RAID Info using %s as primary device \n"), argv[0]);

	// Argv[0]: SrcAddr1
	bResults = ConvertMacToNodeId(argv[0], &SrcAddr1);
	if (!bResults) {
		_ftprintf(stderr, _T("Invalid device ID.\n"));
		return -1;
	}
	SrcAddr1.VID = 1; // to do: get VID as parameter

	SetLastError(0);
	API_CALL(NdasCommInitialize());

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
	ci.Address.DeviceId.VID = 1;
	
	hNDAS = NdasCommConnect(&ci);
	if (!hNDAS) {
		_ftprintf(stdout, _T("Cannot connect\n"));
		goto out;
	}

	//
	// Get and dump RAID info
	//
	RaidInfo.Size = sizeof(RaidInfo);
 	bResults = NdasOpGetRaidInfo(hNDAS,&RaidInfo);
	switch(RaidInfo.Type) {
		case NMT_SINGLE:		str=_T("Single"); break;
		case NMT_MIRROR:		str=_T("Mirror without repair info"); break;
		case NMT_AGGREGATE:		str=_T("Aggregation"); break;
		case NMT_RAID0:			str=_T("RAID0"); break;
		case NMT_RAID1:			str=_T("RAID1R1(~3.10)"); break;
		case NMT_RAID4:			str=_T("RAID4R1(~3.10)"); break;
		case NMT_RAID1R2:		str=_T("RAID1R2(3.11~)"); break;
		case NMT_RAID4R2:		str=_T("RAID4R2(3.11~)"); break;
		case NMT_RAID1R3:		str=_T("RAID1R3(3.20~)"); break;
		case NMT_RAID4R3:		str=_T("RAID4R3(3.20~)"); break;
		case NMT_AOD:			str=_T("Append only disk"); break;
		case NMT_VDVD:			str=_T("Virtual DVD"); break;
		case NMT_CDROM:		str=_T("packet device, CD / DVD"); break;
		case NMT_OPMEM:			str=_T("packet device, Magnetic Optical"); break;
		case NMT_FLASH:			str=_T("flash card"); break;
		case NMT_CONFLICT:		str=_T("DIB is conflicting"); break;
		default: str=_T("Unknown"); break;
	}
	_ftprintf(stdout, _T("Type: %s\n"), str);
	
	_ftprintf(stdout, _T("Mountability Flags: %x(%s%s%s%s%s%s)\n"), RaidInfo.MountablityFlags, 
		(RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE)?_T("Unmountable "):EmptyStr,
		(RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE)?_T("Mountable "):EmptyStr,
		(RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_NORMAL)?_T("in normal mode "):EmptyStr,
		(RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_DEGRADED)?_T("in degraded mode "):EmptyStr,
		(RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MISSING_SPARE)?_T("with missing spare"):EmptyStr,
		(RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_SPARE_EXIST)?_T("with spare"):EmptyStr
	);

	_ftprintf(stdout, _T("Fail reason: %x(%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)\n"), RaidInfo.FailReason, 
		(RaidInfo.FailReason == NDAS_RAID_FAIL_REASON_NONE)?_T("None"):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_RMD_CORRUPTED)?_T("RMD corrupted "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE)?_T("Offline "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DIB_MISMATCH)?_T("DIB mismatch "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_SPARE_USED)?_T("Spare used "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_INCONSISTENT_DIB)?_T("Inconsistent DIB "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION)?_T("Unsupported DIB version "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED)?_T("Migration required"):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID)?_T("Unsupported RAID type"):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DEFECTIVE)?_T("Defective disk "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET)?_T("RAID set ID mismatch "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED)?_T("Unregistered "):EmptyStr,		
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL)?_T("IO failure "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_NOT_A_RAID)?_T("Not a RAID "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_IRRECONCILABLE)?_T("Irreconcilable "):EmptyStr,
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_DISABLED)?_T("Disabled member "):EmptyStr
	);

	MemberCount = RaidInfo.MemberCount;
	
	_ftprintf(stdout, _T("RAID Set ID  = %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n"),
		RaidInfo.RaidSetId.Data1, RaidInfo.RaidSetId.Data2, RaidInfo.RaidSetId.Data3,
		RaidInfo.RaidSetId.Data4[0], RaidInfo.RaidSetId.Data4[1], RaidInfo.RaidSetId.Data4[2], RaidInfo.RaidSetId.Data4[3], 
		RaidInfo.RaidSetId.Data4[4], RaidInfo.RaidSetId.Data4[5], RaidInfo.RaidSetId.Data4[6], RaidInfo.RaidSetId.Data4[7]
	);
	_ftprintf(stdout, _T("Config Set ID= %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n"),
		RaidInfo.ConfigSetId.Data1, RaidInfo.ConfigSetId.Data2, RaidInfo.ConfigSetId.Data3,
		RaidInfo.ConfigSetId.Data4[0], RaidInfo.ConfigSetId.Data4[1], RaidInfo.ConfigSetId.Data4[2], RaidInfo.ConfigSetId.Data4[3], 
		RaidInfo.ConfigSetId.Data4[4], RaidInfo.ConfigSetId.Data4[5], RaidInfo.ConfigSetId.Data4[6], RaidInfo.ConfigSetId.Data4[7]
	);

	for(i=0;i<MemberCount;i++) {
		_ftprintf(stdout, _T(" * Member %d - DeviceId %02x:%02x:%02x:%02x:%02x:%02x-%02x Unit %d\n"),
			i, 
			RaidInfo.Members[i].DeviceId.Node[0], RaidInfo.Members[i].DeviceId.Node[1], RaidInfo.Members[i].DeviceId.Node[2], 
			RaidInfo.Members[i].DeviceId.Node[3], RaidInfo.Members[i].DeviceId.Node[4], RaidInfo.Members[i].DeviceId.Node[5], 
			RaidInfo.Members[i].DeviceId.VID, RaidInfo.Members[i].UnitNo
		);
		Flags = RaidInfo.Members[i].Flags;
		_ftprintf(stdout, _T("                Flags: %x(%s%s%s%s%s%s%s%s%s%s%s%s%s)\n"),
			Flags,
			(Flags & NDAS_RAID_MEMBER_FLAG_ACTIVE)?_T("Active member "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_SPARE)?_T("Spare "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_ONLINE)?_T("Online "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_OFFLINE)?_T("Offline "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET)?_T("Different RAID Set ID "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_IO_FAILURE)?_T("IO failure "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED)?_T("RMD corrupted "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH)?_T("DIB mismatch "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC)?_T("Out-of-sync "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_BAD_SECTOR)?_T("Bad sector "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_BAD_DISK)?_T("Bad disk "):EmptyStr,			
			(Flags & NDAS_RAID_MEMBER_FLAG_REPLACED_BY_SPARE)?_T("Replaced by spare "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE)?_T("Irreconcilable "):EmptyStr,
			(Flags & NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED)?_T("Unregistered "):EmptyStr
		);
	}

	// Dump Bitmap info.
	if (RaidInfo.Type == NMT_RAID1R3) {
		NDAS_DIB_V2 DIB_V2;
		UINT32 OnBitCount;
		UINT32 BitCount;
		UINT32 BmpSectorCount;
		UINT32 CurBitCount;
		UCHAR OnBits[] = {1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7};
		UINT32 BitOn, BitOnStart, BitOnEnd;
		
		bResults = NdasCommBlockDeviceRead(
			hNDAS, 
			NDAS_BLOCK_LOCATION_DIB_V2, 
			1, 
			(PBYTE)&DIB_V2);
		if (!bResults) {
			goto out;
		}
		BitCount = (UINT32)((DIB_V2.sizeUserSpace + DIB_V2.iSectorsPerBit - 1)/DIB_V2.iSectorsPerBit);
		BmpSectorCount = (BitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;
		OnBitCount = 0;
		_ftprintf(stdout, _T("Bitmap sector per bit=0x%x, Bit count =0x%x, BmpSector count=0x%x\n"), 
			DIB_V2.iSectorsPerBit, BitCount, BmpSectorCount);
		BmpBuffer = (PNDAS_OOS_BITMAP_BLOCK) malloc(BmpSectorCount* 512);

		bResults =NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_BITMAP, BmpSectorCount, (PBYTE)BmpBuffer);
		if (!bResults) {
			_ftprintf(stdout, _T("Failed to read BMP.\n"));
			goto out;
		}
		CurBitCount = 0;
		for(i=0;i<BmpSectorCount;i++) {
			_ftprintf(stdout, _T("  Bitmap sector %d, Seq head=%I64x, tail=%I64x\n"), i, BmpBuffer[i].SequenceNumHead,BmpBuffer[i].SequenceNumTail);
			BitOn = FALSE;
			BitOnStart = BitOnEnd = 0;
			for(j=0;j<NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8;j++) {
				if (BitOn == FALSE && (BmpBuffer[i].Bits[j/8] & OnBits[j%8])) {
					BitOn = TRUE;
					BitOnStart = i * NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8 + j;
					_ftprintf(stdout, _T("    Bit on from bit %x ~ "), BitOnStart);
				}
				if (BitOn == TRUE && (BmpBuffer[i].Bits[j/8] & OnBits[j%8]) == 0) {
					BitOn = FALSE;
					BitOnEnd = i * NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8 + j;
					_ftprintf(stdout, _T("%x\n"), BitOnEnd-1);
				}
				if (BmpBuffer[i].Bits[j/8] & OnBits[j%8]) 
					OnBitCount++;
				CurBitCount++;
				if (CurBitCount >= BitCount)
					break;
			}
			if (BitOn == TRUE) {
				_ftprintf(stdout, _T("%x\n"), i * NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8 + j);
			}
		}
		_ftprintf(stdout, _T("%d bit is on out of %d bits. %.1lf%% out-of-sync.\n"), OnBitCount, BitCount, ((double)OnBitCount)/BitCount*100);
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
	if (BmpBuffer)
	{
		free(BmpBuffer);
	}
	NdasCommUninitialize();

	return GetLastError();


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
	SrcAddr1.VID = 1;
	
	bResults = ConvertMacToNodeId(argv[1], &SrcAddr2);
	if (!bResults) {
		_ftprintf(stderr, _T("Invalid device ID.\n"));
		return -1;
	}
	SrcAddr2.VID = 1;
	
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
DEF_COMMAND_1(_c_info, "info", "Get and dump RAID info.")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_compare, CpComare, 2, 2},
	{ _c_info, CpInfo, 1, 1},
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

