// ndasop.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include <ndas/ndastypeex.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasop.h>
#include "resource.h"
#include <socketlpx.h>
#include <scrc32.h>
#include <objbase.h>

extern HMODULE NDASOPModule;

#include <xtl/xtltrace.h>
#ifdef RUN_WPP
#include "ndasop.tmh"
#endif

#define HEAP_SAFE_FREE(HEAP) if(HEAP) {::HeapFree(::GetProcessHeap(), NULL, HEAP); HEAP = NULL;}

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

static
__forceinline 
void 
SetNdasConnectionInfoFromDIBIndex(
	NDASCOMM_CONNECTION_INFO* lpci,
	BOOL bWriteAccess,
	NDAS_DIB_V2* lpdibv2,
	DWORD index)
{
	::ZeroMemory(lpci, sizeof(NDASCOMM_CONNECTION_INFO));
	lpci->Size = sizeof(NDASCOMM_CONNECTION_INFO);
	lpci->AddressType = NDASCOMM_CIT_DEVICE_ID;
	lpci->WriteAccess = bWriteAccess;
	lpci->LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	lpci->Protocol = NDASCOMM_TRANSPORT_LPX;
	::CopyMemory(
		lpci->Address.DeviceId.Node, 
		lpdibv2->UnitDisks[index].MACAddr, 
		sizeof(lpci->Address.DeviceId.Node));
	lpci->UnitNo = lpdibv2->UnitDisks[index].UnitNumber;
	lpci->Address.DeviceId.VID = lpdibv2->UnitDisks[index].VID;
}

// facility macro
#define ENUM_NDAS_DEVICES_IN_DIB(IT, DIB, HANDLE_NDAS, WRITE_ACCESS, CONN_INFO) \
	for( \
		( \
			(IT) = 0 \
		); \
		( \
			((IT) < (DIB).nDiskCount + (DIB).nSpareCount) ? \
				( \
					SetNdasConnectionInfoFromDIBIndex(&CONN_INFO, WRITE_ACCESS, &DIB, IT), \
					(HANDLE_NDAS) = NdasCommConnect(&(CONN_INFO)), \
					TRUE \
				) : FALSE \
		); \
		( \
			(HANDLE_NDAS) ? NdasCommDisconnect((HANDLE_NDAS)) : 0, \
			(HANDLE_NDAS) = NULL, \
			(IT)++ \
		) \
	)


static
BOOL
NdasOpClearMBR(HNDAS hNDAS, BOOL bInitMBR = FALSE);

static
BOOL
NdasOpInitMBR(HNDAS hNDAS);

static
BOOL
NdasOpClearXArea(HNDAS hNDAS);

static
BOOL
NdasOpVerifyDiskCount(UINT32 BindType, UINT32 nDiskCount);

static
BOOL
NdasOpGetUnitDiskLocation(
	HNDAS hNDAS,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	UNIT_DISK_LOCATION *pUnitDiskLocation);

////////////////////////////////////////////////////////////////////////////////////////////////
//
// NDAS Op API Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////

NDASOP_LINKAGE
DWORD
NDASOPAPI
NdasOpGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDASOP_API_VERSION_MAJOR, 
		NDASOP_API_VERSION_MINOR);
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpMigrate(
   CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo
   )
{
	BOOL bReturn = FALSE;
	BOOL bResults;
	HNDAS hNDAS = NULL;
	HNDAS *ahNDAS = NULL;
	NDASCOMM_CONNECTION_INFO CI;
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 DIB_V2;
	UINT32 nDIBSize;
	NDAS_RAID_META_DATA NewRmd;
	NDAS_RAID_META_DATA TmpRmd;	
	NDAS_RAID_META_DATA OldRmd;	
	unsigned _int32 iOldMediaType;
	UINT32 i, j;
	UINT32 nTotalDiskCount;
	LONG FaultDisk = -1; // Not exist. Index is role number.
	UINT32 HighUsn = 0;
	PBYTE Rev1Bitmap = NULL;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)));

// read & create original DIB_V2
	// connect to the NDAS Device
	hNDAS = NdasCommConnect(pConnectionInfo);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
		hNDAS || NDASCOMM_ERROR_RW_USER_EXIST != ::GetLastError());
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	// fail if the NDAS Unit Device is not a single disk
	nDIBSize = sizeof(DIB_V2);
	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &nDIBSize);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount+DIB_V2.nSpareCount;

	// check whether able migrate or not
	// rebuild DIB_V2
	iOldMediaType = DIB_V2.iMediaType;
	switch(iOldMediaType)
	{
	case NMT_MIRROR: // to RAID1R3
	case NMT_RAID1: // to RAID1R3
	case NMT_RAID1R2: // to RAID1R3	
		DIB_V2.iMediaType = NMT_RAID1R3;
		break;
	case NMT_RAID4: // to RAID4R
	case NMT_RAID4R2: // to RAID4R	
		DIB_V2.iMediaType = NMT_RAID4R3;
		break;
	default:
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out, FALSE);
		break;
	}

	ZeroMemory(&DIB_V1, sizeof(NDAS_DIB));
	NDAS_DIB_V1_INVALIDATE(DIB_V1);

	// create ahNDAS
	ahNDAS = (HNDAS *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		nTotalDiskCount * sizeof(HNDAS));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, ahNDAS);
	::ZeroMemory(ahNDAS, nTotalDiskCount * sizeof(HNDAS));

	//
	// connect to all devices. NdasOpMigrate assume all disk is accessible.
	// 
	for(i = 0; i < nTotalDiskCount; i++)
	{
		SetNdasConnectionInfoFromDIBIndex(
			&CI, TRUE, &DIB_V2, i);

		ahNDAS[i] = NdasCommConnect(&CI);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			ahNDAS[i] || NDASCOMM_ERROR_RW_USER_EXIST != ::GetLastError());
		TEST_AND_GOTO_IF_FAIL(out, ahNDAS[i]);
	}


	//
	// Search for fault disk.
	// 
	// Each RAID revision has different method to record out-of-sync disk!!
	//	
	switch(iOldMediaType)
	{
	case NMT_MIRROR:
		{
			// No fault tolerant existed. Assume disk is not in sync.
			FaultDisk = 1;

			// NMT_MIRROR's first node is always smaller.
			NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};
			udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
			bResults = NdasCommGetUnitDeviceHardwareInfoW(ahNDAS[0], &udinfo);
			TEST_AND_GOTO_IF_FAIL(out, bResults);

			DIB_V2.sizeXArea = NDAS_BLOCK_SIZE_XAREA;
			DIB_V2.sizeUserSpace = udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;
			DIB_V2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;
		}		
		break;
	case NMT_RAID1:
		{
			//
			// Migrate out-of-sync information.
			// NMT_RAID1 don't have RMD and don't have spare disk.
			//
			// Check which node's bitmap is clean. Clean one is defective one.
			//
			// If bitmap is recorded, that disk is correct disk.
			//
			Rev1Bitmap = (PBYTE)::HeapAlloc(::GetProcessHeap(),
					HEAP_ZERO_MEMORY, NDAS_BLOCK_SIZE_BITMAP_REV1 * SECTOR_SIZE);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, Rev1Bitmap);
			for(i=0 ; i < nTotalDiskCount; i++) {
				bResults = NdasCommBlockDeviceRead(ahNDAS[i], NDAS_BLOCK_LOCATION_BITMAP,
					NDAS_BLOCK_SIZE_BITMAP_REV1, Rev1Bitmap);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
				// is bitmap clean?
				for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP_REV1 * SECTOR_SIZE; j++)
				{
					if(Rev1Bitmap[j])
						break;
				}
				if(NDAS_BLOCK_SIZE_BITMAP_REV1 * SECTOR_SIZE != j) {
					// Bitmap is not clean, which means the other disk is fault.
					FaultDisk = (i==0)?1:0;
					break;
				}
			}
			// We can keep sizeXArea and sizeUserSpace
			DIB_V2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;
		}
		break;
	case NMT_RAID1R2: // to RAID1R3
		//
		// Read all RMD and find out-of-sync disk.
		// If different RMD shows different disk is out-of-sync, it is RAID-failure case
		// If no out-of-sync disk exists and mount flag is mounted status, it is unclean unmount state. 
		//			Select any non-spare disk as out-of-sync.
		// 
		for(i=0 ; i < nTotalDiskCount; i++) {
			bResults = NdasCommBlockDeviceRead(ahNDAS[i], NDAS_BLOCK_LOCATION_RMD,
				1, (PBYTE) &TmpRmd);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
			if (TmpRmd.Signature != NDAS_RAID_META_DATA_SIGNATURE) {
				//
				// Not a valid RMD.
				//
				goto out;
			}
			for(j=0; j < nTotalDiskCount; j++) {
				if (TmpRmd.UnitMetaData[j].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED) {
					if (FaultDisk == -1) {
						FaultDisk = j;
					} else if (FaultDisk == j) {
						// Same disk is marked as fault. Okay.
					} else {
						// 
						// RAID status is not consistent. Cannot migrate.
						//
						goto out;
					}
				}
			}
			if (TmpRmd.uiUSN > HighUsn) {
				//
				// Save RMD from node with highest USN
				//
				::CopyMemory(&OldRmd, &TmpRmd, sizeof(OldRmd));
			}			
		}

		if (FaultDisk == -1) {
			// Check clean-unmount
			if (OldRmd.state & NDAS_RAID_META_DATA_STATE_MOUNTED) {
				// Select any non-spare disk.
				FaultDisk = 1; // Select second disk.
			}
		}
		// We can keep sizeXArea and sizeUserSpace
		DIB_V2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;		
		break;
	default:
		::SetLastError(ERROR_INVALID_PARAMETER);
		goto out;
	}

	//
	// Fill more DIB/RMD fields
	//
	
	if (NMT_MIRROR == iOldMediaType ||
		NMT_RAID1 == iOldMediaType) {
		// There wasn't RMD.
		// Fill default.
		::ZeroMemory(&NewRmd, sizeof(NDAS_RAID_META_DATA));
		NewRmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;
		::CoCreateGuid(&NewRmd.RaidSetId);
		::CoCreateGuid(&NewRmd.ConfigSetId);		
		NewRmd.uiUSN = 1; // initial value
		
		for(i = 0; i < nTotalDiskCount; i++) {
			NewRmd.UnitMetaData[i].iUnitDeviceIdx = (unsigned _int16)i;
			if (i == FaultDisk) {
				NewRmd.UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
			}
		}
	} else if (NMT_RAID1R2 == iOldMediaType) {
		// NMT_RAID1R2 RMD and NMT_RAID1R3 RMD is compatible
		::CopyMemory(&NewRmd, &OldRmd, sizeof(NewRmd));

		// To prevent former RAID member from interrupting, generate new GUID.
		NewRmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;
		::CoCreateGuid(&NewRmd.RaidSetId);
		::CoCreateGuid(&NewRmd.ConfigSetId);
		NewRmd.uiUSN = 1; // initial value
		
		if (FaultDisk != -1) {
			NewRmd.UnitMetaData[FaultDisk].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		}
		NewRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;
	}
	
	// write DIB_V2 & RMD
	for(i = 0; i < nTotalDiskCount; i++)
	{
		// clear X Area
		bResults = NdasOpClearXArea(ahNDAS[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		DIB_V2.iSequence = i;

		// set CRC32
		SET_DIB_CRC(crc32_calc, DIB_V2);
		SET_RMD_CRC(crc32_calc, NewRmd);

		// write DIB_V1, DIBs_V2, RMD(ignore RAID type)
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&DIB_V1);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)&NewRmd);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&NewRmd);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}

	if (FaultDisk != -1) {
		NDAS_OOS_BITMAP_BLOCK BmpBuf;
		// I'm lazy to convert old version's bitmap to current version's bitmap
		// Just mark all bit dirty.
		for(i = 0; i < nTotalDiskCount; i++) {
			BmpBuf.SequenceNumHead = 0;
			BmpBuf.SequenceNumTail = 0;
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0xff);
			for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP; j++)
			{
				bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
					NDAS_BLOCK_LOCATION_BITMAP + j, 1, (PBYTE)&BmpBuf);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}
		}
	}

	bReturn = TRUE;
out:
	DWORD dwLastErrorBackup = ::GetLastError();

	HEAP_SAFE_FREE(Rev1Bitmap);

	if(ahNDAS)
	{
		for(i = 0; i < nTotalDiskCount; i++)
		{
			if(ahNDAS[i])
			{
				bResults = NdasCommDisconnect(ahNDAS[i]);
				_ASSERT(bResults);
				ahNDAS[i] = NULL;
			}
		}

		HEAP_SAFE_FREE(ahNDAS);
	}

	if(hNDAS)
	{
		bResults = NdasCommDisconnect(hNDAS);
		_ASSERT(bResults);
		hNDAS = NULL;
	}

	::SetLastError(dwLastErrorBackup);
	return bReturn;
}

NDASOP_LINKAGE
UINT32
NDASOPAPI
NdasOpBind(
	UINT32	nDiskCount,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	UINT32	BindType,
	UINT32	uiUserSpace
	)
{
	UINT32 iReturn = 0xFFFFFFFF;
	BOOL bResults;
	UINT32 i, j;
	HNDAS *ahNDAS = NULL;
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 *DIBs_V2 = NULL;
	UINT32 nDIBSize;

	BOOL bClearMBR;
	BOOL bMigrateMirrorV1;
	
	NDAS_OOS_BITMAP_BLOCK BmpBuf;
	NDASCOMM_CONNECTION_INFO ConnectionInfoDiscover;
	UINT64 ui64TotalSector = 0;
	NDAS_RAID_META_DATA rmd;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * nDiskCount));

	switch(BindType)
	{
	case NMT_SINGLE:
	case NMT_AGGREGATE:
	case NMT_SAFE_AGGREGATE:
	case NMT_RAID0:
	case NMT_SAFE_RAID1:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_RAID5:
		break;
	default:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, FALSE);
		break;
	}

	bResults = NdasOpVerifyDiskCount(BindType, nDiskCount);
	TEST_AND_RETURN_IF_FAIL(bResults);

	ZeroMemory(&DIB_V1, sizeof(NDAS_DIB));
	if(NMT_SINGLE != BindType)
		NDAS_DIB_V1_INVALIDATE(DIB_V1);

	DIBs_V2 = (PNDAS_DIB_V2)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		nDiskCount * sizeof(NDAS_DIB));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, DIBs_V2);

	ahNDAS = (HNDAS *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		nDiskCount * sizeof(HNDAS));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, ahNDAS);

	bClearMBR = TRUE;
	bMigrateMirrorV1 = FALSE;

	// init rmd
	::ZeroMemory(&rmd, sizeof(NDAS_RAID_META_DATA));
	rmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;
	::CoCreateGuid(&rmd.RaidSetId);
	::CoCreateGuid(&rmd.ConfigSetId);	
	rmd.uiUSN = 1; // initial value

	// gather information & initialize DIBs
	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i; // record last accessing disk

		// connect to the NDAS Device
		ahNDAS[i] = NdasCommConnect(&pConnectionInfo[i]);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			ahNDAS[i] || NDASCOMM_ERROR_RW_USER_EXIST != ::GetLastError());
		TEST_AND_GOTO_IF_FAIL(out, ahNDAS[i]);

		// discover the NDAS Device so that stop process if any connected user exists.
		// V2.0 does not report RO count correctly
		RtlCopyMemory(&ConnectionInfoDiscover, &pConnectionInfo[i],
			sizeof(ConnectionInfoDiscover));
		ConnectionInfoDiscover.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
		NDAS_UNITDEVICE_STAT udstat = {0};
		udstat.Size = sizeof(NDAS_UNITDEVICE_STAT);

		TEST_AND_GOTO_IF_FAIL(out, NdasCommGetUnitDeviceStat(&ConnectionInfoDiscover, &udstat));

		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(
			NDASOP_ERROR_ALREADY_USED, 
			out,
			1 == udstat.RWHostCount && 
			(0 == udstat.ROHostCount ||
			 NDAS_HOST_COUNT_UNKNOWN == udstat.ROHostCount));

		// fail if the NDAS Unit Device is not a single disk
		nDIBSize = sizeof(DIBs_V2[i]);
		bResults = NdasOpReadDIB(ahNDAS[i], &DIBs_V2[i], &nDIBSize);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(NMT_SINGLE == BindType)
		{
			// Do not clear MBR so that result single disks still contain data
			if (NMT_MIRROR == DIBs_V2[i].iMediaType ||
					((NMT_RAID1 == DIBs_V2[i].iMediaType ||
					NMT_RAID1R2 == DIBs_V2[i].iMediaType) &&
						2 == DIBs_V2[i].nDiskCount	) ||
					NMT_RAID1R3 == DIBs_V2[i].iMediaType
				) {
				bClearMBR = FALSE;
			}
		}
		else if(NMT_RAID1R3 == BindType)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_SINGLE_DISK, out,
				NMT_SINGLE == DIBs_V2[i].iMediaType ||
				NMT_MIRROR == DIBs_V2[i].iMediaType ); // accepts NMT_MIRROR also as migration
		}
		else
		{
			// common case
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_SINGLE_DISK, out,
				NMT_SINGLE == DIBs_V2[i].iMediaType); // bind only single disks
		}

		// Do I migrate?
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_BIND_MIGRATE,
			FALSE == bMigrateMirrorV1 || NMT_MIRROR == DIBs_V2[i].iMediaType);

		if(2 == nDiskCount && NMT_MIRROR == DIBs_V2[i].iMediaType)
		{
			// Yes I do.
			bMigrateMirrorV1 = TRUE;
		}

		// Create DIB_V2 information
		ZeroMemory(&DIBs_V2[i], sizeof(NDAS_DIB_V2));
		DIBs_V2[i].Signature = NDAS_DIB_V2_SIGNATURE;
		DIBs_V2[i].MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
		DIBs_V2[i].MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
		DIBs_V2[i].sizeXArea = NDAS_BLOCK_SIZE_XAREA; // 2MB
		DIBs_V2[i].iMediaType =
			(BindType == NMT_SAFE_RAID1) ? NMT_RAID1R3 :
			(BindType == NMT_SAFE_AGGREGATE) ? NMT_AGGREGATE :
			BindType;
		DIBs_V2[i].nDiskCount = nDiskCount;
		DIBs_V2[i].nSpareCount =  0;
		DIBs_V2[i].iSequence = i;

		// init rmd.UnitMetaData[i]
		rmd.UnitMetaData[i].iUnitDeviceIdx = (unsigned _int16)i;
		rmd.UnitMetaData[i].UnitDeviceStatus = NULL;

		NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};
		udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
		bResults = NdasCommGetUnitDeviceHardwareInfoW(ahNDAS[i], &udinfo);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(0 != uiUserSpace)
		{
			udinfo.SectorCount.QuadPart = uiUserSpace;
		}

		// calculate user space, sectors per bit (bitmap)
		switch(BindType)
		{
		case NMT_AGGREGATE:
		case NMT_SAFE_AGGREGATE:
			// just % 128 of free space
			DIBs_V2[i].sizeUserSpace = udinfo.SectorCount.QuadPart 
				- NDAS_BLOCK_SIZE_XAREA;
			DIBs_V2[i].sizeUserSpace -= DIBs_V2[i].sizeUserSpace % 128;

			ui64TotalSector += DIBs_V2[i].sizeUserSpace;
			break;
		case NMT_RAID0:
			// % 128 of smallest free space
			if(0 == i) // initialize
				DIBs_V2[0].sizeUserSpace = udinfo.SectorCount.QuadPart 
				- NDAS_BLOCK_SIZE_XAREA;

			DIBs_V2[0].sizeUserSpace = min(udinfo.SectorCount.QuadPart 
				- NDAS_BLOCK_SIZE_XAREA,
				DIBs_V2[0].sizeUserSpace);

			if(nDiskCount -1 == i) // finalize
			{
				DIBs_V2[0].sizeUserSpace -= DIBs_V2[0].sizeUserSpace % 128;
				for(j = 1; j < nDiskCount; j++)
				{
					DIBs_V2[j].sizeUserSpace = DIBs_V2[0].sizeUserSpace;
				}

				ui64TotalSector = DIBs_V2[0].sizeUserSpace * nDiskCount;
			}

			break;
		case NMT_RAID1R3:
		case NMT_SAFE_RAID1:
			if(i % 2) // finalize pair
			{
				if(bMigrateMirrorV1 || NMT_SAFE_RAID1 == BindType)
				{
					// migration or add mirror code
					// mirror disks use first disk as primary. So take value from it.
					TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
						NDASOP_ERROR_INVALID_BIND_MIGRATE, out,
						udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA >=
						DIBs_V2[i -1].sizeUserSpace);
					// Keep previous user space
					DIBs_V2[i].sizeUserSpace = DIBs_V2[i -1].sizeUserSpace;
					DIBs_V2[i].iSectorsPerBit = DIBs_V2[i-1].iSectorsPerBit;
				}
				else
				{
					// Calc user space for new RAID1
					DIBs_V2[i].sizeUserSpace = min(
						udinfo.SectorCount.QuadPart- NDAS_BLOCK_SIZE_XAREA,
						DIBs_V2[i -1].sizeUserSpace);
					//
					// Reduce user space by 0.5%. HDDs with same giga size labels have different sizes.
					//		Sometimes, it is up to 7.5% difference due to 1k !=1000.
					//		Once, I found Maxter 160G HDD size - Samsung 160G HDD size = 4G. 
					//		Even with same maker's HDD with same gig has different sector size.
					//	To do: Give user a option to select this margin.
					//
					
					DIBs_V2[i].sizeUserSpace = DIBs_V2[i].sizeUserSpace * 199/ 200;

					// Increase sectors per bit if user space is larger than default maximum.
					for(j = 16; TRUE; j++)
					{
						if(DIBs_V2[i].sizeUserSpace <= ( ((UINT64)1<<j)) * NDAS_BLOCK_SIZE_BITMAP * NDAS_BIT_PER_OOS_BITMAP_BLOCK ) // 512 GB : 128 SPB
						{
							// Sector per bit is big enough to be covered by bitmap
							DIBs_V2[i].iSectorsPerBit = 1 << j;
							break;
						}
						_ASSERT(j <= 32); // protect overflow
					}

					// Trim user space that is out of bitmap align.
					DIBs_V2[i].sizeUserSpace -= DIBs_V2[i].sizeUserSpace 
						% DIBs_V2[i].iSectorsPerBit;
				}

				// set fault to backup device
				if(NMT_SAFE_RAID1 == BindType)
				{
					rmd.UnitMetaData[i].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
				}

				DIBs_V2[i -1].sizeUserSpace = DIBs_V2[i].sizeUserSpace;
				DIBs_V2[i -1].iSectorsPerBit = DIBs_V2[i].iSectorsPerBit;

				ui64TotalSector += DIBs_V2[i].sizeUserSpace;
			}
			else // initialize pair
			{
				DIBs_V2[i].sizeUserSpace = 
					udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;
				DIBs_V2[i].iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;
			}
//			DIBs_V2[i].AutoRecover = TRUE;
			break;
		case NMT_RAID4R3:
		case NMT_RAID5:
			if(0 == i) {// First disk
				DIBs_V2[0].sizeUserSpace = 
					udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;
				DIBs_V2[i].iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;
			} else {
				// Get minimun size among all disks.
				DIBs_V2[0].sizeUserSpace = min(
					udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA,
					DIBs_V2[0].sizeUserSpace);

				if(nDiskCount -1 == i) 
				{
					// For last disk, calculate required bitmap count and update all DIBs

					// Increase sectors per bit if user space is larger than default maximum.
					for(j = 16; TRUE; j++)
					{
						if(DIBs_V2[0].sizeUserSpace <= ( ((UINT64)1<<j)) * NDAS_BLOCK_SIZE_BITMAP * NDAS_BIT_PER_OOS_BITMAP_BLOCK ) // 512 GB : 128 SPB
						{
							// Sector per bit is big enough to be covered by bitmap
							DIBs_V2[0].iSectorsPerBit = 1 << j;
							break;
						}
						_ASSERT(j <= 32); // protect overflow
					}

					// Trim user space that is out of bitmap align.
					DIBs_V2[0].sizeUserSpace -= DIBs_V2[0].sizeUserSpace 
						% DIBs_V2[0].iSectorsPerBit;

					
					for(j = 1; j < nDiskCount; j++)
					{
						DIBs_V2[j].sizeUserSpace = DIBs_V2[0].sizeUserSpace;
						DIBs_V2[j].iSectorsPerBit = DIBs_V2[0].iSectorsPerBit;
					}

					ui64TotalSector = DIBs_V2[0].sizeUserSpace * (nDiskCount - 1);
				}
			}
			break;
		case NMT_SINGLE:
			ZeroMemory(&DIBs_V2[i], sizeof(NDAS_DIB_V2));
			ui64TotalSector = udinfo.SectorCount.QuadPart;
			break;
		default:
			::SetLastError(ERROR_INVALID_PARAMETER);
			goto out;
		}

		if(NMT_SINGLE != BindType)
		{
			for(j = 0; j < nDiskCount; j++)
			{
				bResults = NdasOpGetUnitDiskLocation(
					NULL,
					&pConnectionInfo[j],
					&DIBs_V2[i].UnitDisks[j]);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}
		}
	}
	SET_RMD_CRC(crc32_calc, rmd);

	// Set unit device information
	if(NMT_SINGLE != BindType)
	{
		// Added as of 3.20. Store version of HW to DIB.
		// This version is used only when RAID is mounted in degraded mode, and its HW version is unknown.
		// No backward compatability issue occurs because older RAID1 should be migrated to be used.
		NDAS_DEVICE_HARDWARE_INFO dinfo;
		for(i = 0; i < nDiskCount; i++) {
			ZeroMemory(&dinfo, sizeof(NDAS_DEVICE_HARDWARE_INFO));
			dinfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
			if (!NdasCommGetDeviceHardwareInfo(ahNDAS[i],&dinfo))
				goto out;
			for(j=0;j<nDiskCount;j++) {
				DIBs_V2[j].UnitDiskInfos[i].HwVersion = dinfo.HardwareVersion;
			}
		}
	}

	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i;

		// clear MBR
		if (bClearMBR &&
			!bMigrateMirrorV1 &&
			NMT_SAFE_RAID1 != BindType &&
			NMT_SAFE_AGGREGATE != BindType)
		{
			BOOL bInitMBR = FALSE;
			if ((NMT_RAID0 == BindType && i == 0) ||
				(NMT_RAID1R3 == BindType) ||
				(NMT_AGGREGATE == BindType && i == 0))
			{
				bInitMBR = TRUE;
			}
			bResults = NdasOpClearMBR(ahNDAS[i], bInitMBR);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}

		// clear X Area
		bResults = NdasOpClearXArea(ahNDAS[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(NMT_SINGLE == BindType)
			continue;

		// set CRC32
		SET_DIB_CRC(crc32_calc, DIBs_V2[i]);

		// write DIB_V1, DIBs_V2, RMD(ignore RAID type)
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&DIB_V1);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIBs_V2[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)&rmd);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&rmd);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(nDiskCount > NDAS_MAX_UNITS_IN_V2)
		{
			_ASSERT(nDiskCount > NDAS_MAX_UNITS_IN_V2); // not coded
		}
		if(NMT_SAFE_RAID1 == BindType) // Initialize whole bitmap
		{
			UINT32  BmpSectorCount;
			UINT32  BitCount;
			UINT32 RemaingBitsCount;
			UCHAR OnBits[] = {1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7};			
			BmpBuf.SequenceNumHead = 0;
			BmpBuf.SequenceNumTail = 0;

			BitCount = (DWORD)((DIBs_V2[i].sizeUserSpace + DIBs_V2[i].iSectorsPerBit - 1)/DIBs_V2[i].iSectorsPerBit);
			BmpSectorCount = (BitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0xff);	
			for (j = 0; j < BmpSectorCount-1; ++j)
			{
				bResults = NdasCommBlockDeviceWriteSafeBuffer(
					ahNDAS[i], 
					NDAS_BLOCK_LOCATION_BITMAP + j, 
					1, 
					(PBYTE)&BmpBuf);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}


			//
			// For last usued sector set bits that are required.
			//
			RemaingBitsCount = BitCount - (NDAS_BIT_PER_OOS_BITMAP_BLOCK * (BmpSectorCount - 1));
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0);
			for (j=0;j<RemaingBitsCount;j++) {
				//
				// Set bit
				//
				BmpBuf.Bits[j/8] |= OnBits[j%8];
			}
			bResults = NdasCommBlockDeviceWriteSafeBuffer(
				ahNDAS[i], 
				NDAS_BLOCK_LOCATION_BITMAP + BmpSectorCount-1, 
				1, 
				(PBYTE)&BmpBuf);

			//
			// Clear unused bitmaps
			//
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0);
			for (j = BmpSectorCount; j < NDAS_BLOCK_SIZE_BITMAP; ++j)
			{
				bResults = NdasCommBlockDeviceWriteSafeBuffer(
					ahNDAS[i], 
					NDAS_BLOCK_LOCATION_BITMAP + j, 
					1, 
					(PBYTE)&BmpBuf);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}
		}
	}

	// success
	iReturn = nDiskCount;
out:
	DWORD dwLastErrorBackup = ::GetLastError();

	if(ahNDAS)
	{
		for(i = 0; i < nDiskCount; i++)
		{
			if(ahNDAS[i])
			{
				bResults = NdasCommDisconnect(ahNDAS[i]);
				_ASSERT(bResults);
				ahNDAS[i] = NULL;
			}
		}

		HEAP_SAFE_FREE(ahNDAS);
	}

	HEAP_SAFE_FREE(DIBs_V2);

	::SetLastError(dwLastErrorBackup);
	return iReturn;
}

#define NDAS_BITMAP_SET(BITMAP_ARRAY, IDX) (BITMAP_ARRAY)[(IDX) / 8] |= 0x01 << ((IDX) %8)
#define NDAS_BITMAP_RESET(BITMAP_ARRAY, IDX) (BITMAP_ARRAY)[(IDX) / 8] &= ~(0x01 << ((IDX) %8))
#define NDAS_BITMAP_ISSET(BITMAP_ARRAY, IDX) ((BITMAP_ARRAY)[(IDX) / 8] & 0x01 << ((IDX) %8))
#define NDAS_BITMAP_IDX_TO_SECTOR(IDX) ((IDX) / 8 / SECTOR_SIZE)

#define NDAS_BITMAP_FIND_SET_BIT(BITMAP_ARRAY, IDX, LIMIT)	\
	while((IDX) < (LIMIT))	\
	{	\
		if(NDAS_BITMAP_ISSET(BITMAP_ARRAY, (IDX)))	\
			break;	\
		(IDX)++;	\
	}	\
	if((IDX) >= (LIMIT))	\
		(IDX) = 0xFFFFFFFF;



/*
1. Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2
2. Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct
3. Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND incase of more than 32 NDAS Unit devices exist 4. Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 if  NDAS_DIB_V2 information is not acceptable
5. Check Signature and Version informations in NDAS_DIB_V1 and translate the NDAS_DIB_V1 to an NDAS_DIB_V2
6. Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1 is not acceptable either
*/

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReadDIB(
    HNDAS hNDAS,
    NDAS_DIB_V2 *pDIB_V2,
    UINT32 *pnDIBSize)
{
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 DIB_V2;
	BOOL bResults;
	UINT32 nDIBSize;
	DWORD dwUnitNumber;
	BYTE byteVID;
	UINT32 nTotalDiskCount;
	NDAS_UNITDEVICE_HARDWARE_INFOW UnitInfo;
	BOOLEAN ConflictingDib = FALSE;
	
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pnDIBSize, sizeof(UINT32)));

	// Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2

	bResults = NdasCommBlockDeviceRead(
		hNDAS, 
		NDAS_BLOCK_LOCATION_DIB_V2, 
		1, 
		(PBYTE)&DIB_V2);

	TEST_AND_RETURN_IF_FAIL(bResults);

	// Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct


	if(NDAS_DIB_V2_SIGNATURE != DIB_V2.Signature)
	{
		goto process_v1;
	}

	if(!IS_DIB_CRC_VALID(crc32_calc, DIB_V2))
	{
		ConflictingDib = TRUE;
		goto process_v1;
	}

	if(NDAS_BLOCK_SIZE_XAREA != DIB_V2.sizeXArea &&
		NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE != DIB_V2.sizeXArea)
	{
		ConflictingDib = TRUE;
		goto process_v1;
	}

	UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
	NdasCommGetUnitDeviceHardwareInfo(hNDAS, &UnitInfo);

	if(DIB_V2.sizeUserSpace + DIB_V2.sizeXArea > UnitInfo.SectorCount.QuadPart)
	{
		ConflictingDib = TRUE;
		goto process_v1;
	}

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	// check DIB_V2.nDiskCount
	if(!NdasOpVerifyDiskCount(DIB_V2.iMediaType, DIB_V2.nDiskCount)) {
		ConflictingDib = TRUE;
		goto process_v1;
	}
	if(DIB_V2.iSequence >= nTotalDiskCount) {
		ConflictingDib = TRUE;
		goto process_v1;
	}
	if(nTotalDiskCount > 32 + 64 + 64) // AING : PROTECTION
		goto process_v1;

	// check done, copy DIB_V2 information from NDAS Device to pDIB_V2
	{
		// code does not support if version in DIB_V2 is greater than the version defined
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_UNSUPPORTED, 
			NDAS_DIB_VERSION_MAJOR_V2 > DIB_V2.MajorVersion ||
			(NDAS_DIB_VERSION_MAJOR_V2 == DIB_V2.MajorVersion && 
			NDAS_DIB_VERSION_MINOR_V2 >= DIB_V2.MinorVersion));

		nDIBSize = (GET_TRAIL_SECTOR_COUNT_V2(nTotalDiskCount) + 1) 
			* sizeof(NDAS_DIB_V2);

		if(NULL != pnDIBSize)
		{
			if(*pnDIBSize >= nDIBSize) // make sure there is enough space
			{
				TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
					!::IsBadWritePtr(pDIB_V2, sizeof(NDAS_DIB_V2)));
				CopyMemory(pDIB_V2, &DIB_V2, sizeof(NDAS_DIB_V2));

				// Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND 
				//		in case of more than 32 NDAS Unit devices exist 4. 
				// Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 
				//		if  NDAS_DIB_V2 information is not acceptable
				if(nDIBSize > sizeof(NDAS_DIB_V2))
				{
					bResults = NdasCommBlockDeviceRead(
						hNDAS, 
						NDAS_BLOCK_LOCATION_ADD_BIND, 
						GET_TRAIL_SECTOR_COUNT_V2(nTotalDiskCount),
						(PBYTE)(pDIB_V2 +1));
					TEST_AND_RETURN_IF_FAIL(bResults);
				}

				if(NMT_SINGLE != DIB_V2.iMediaType)
				{
					unsigned _int8 MACAddr[6];
					unsigned _int8 UnitNumber;
					unsigned _int8 VID;

					if(nTotalDiskCount <= DIB_V2.iSequence)
						goto process_v1;

					bResults = NdasCommGetDeviceID(hNDAS, NULL, MACAddr, &dwUnitNumber, &byteVID);
					UnitNumber = (unsigned _int8)dwUnitNumber;
					VID = byteVID;

					//
					// Assume VID 0 and 1 is same.
					//
					if(memcmp(DIB_V2.UnitDisks[DIB_V2.iSequence].MACAddr,
						MACAddr, sizeof(MACAddr)) ||
						DIB_V2.UnitDisks[DIB_V2.iSequence].UnitNumber != UnitNumber ||
						(DIB_V2.UnitDisks[DIB_V2.iSequence].VID != VID &&
						!((VID==NDAS_VID_DEFAULT && DIB_V2.UnitDisks[DIB_V2.iSequence].VID==NDAS_VID_NONE) ||
						(VID==NDAS_VID_NONE && DIB_V2.UnitDisks[DIB_V2.iSequence].VID==NDAS_VID_DEFAULT)))) 
					{
						// DIB information is not consistent with this unit's information.
						ConflictingDib = TRUE;
						goto process_v1;
					}
				}
			}
			else // do not copy, possibly just asking size
			{
				*pnDIBSize = nDIBSize;
			}
		}
		else // copy 1 sector only
		{
			TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
				!::IsBadWritePtr(pDIB_V2, sizeof(NDAS_DIB_V2)));
			CopyMemory(pDIB_V2, &DIB_V2, sizeof(NDAS_DIB_V2));
		}

		return TRUE;
	}

process_v1:
	
	// Check Signature and Version informations in NDAS_DIB_V1 and translate the NDAS_DIB_V1 to an NDAS_DIB_V2

	// initialize DIB V1
	nDIBSize = sizeof(DIB_V2); // maximum 2 disks

	// ensure buffer
	if(NULL == pDIB_V2 || *pnDIBSize < nDIBSize)		
	{
		*pnDIBSize = nDIBSize; // set size needed
		return TRUE;
	}

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pDIB_V2, sizeof(NDAS_DIB_V2)));

	// write to pDIB_V2 directly
	ZeroMemory(pDIB_V2, nDIBSize);

	bResults = NdasCommBlockDeviceRead(hNDAS, 
		NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&DIB_V1);
	TEST_AND_RETURN_IF_FAIL(bResults);


	NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};
	udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	bResults = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &udinfo);
	TEST_AND_RETURN_IF_FAIL(bResults);

	pDIB_V2->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIB_V2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIB_V2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIB_V2->sizeXArea = NDAS_BLOCK_SIZE_XAREA;
	pDIB_V2->iSectorsPerBit = 0; // no backup information
	pDIB_V2->sizeUserSpace = udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA; // in case of mirror, use primary disk size

	if (ConflictingDib) {
		// Fill as one disk if DIB info is conflicting.
		pDIB_V2->iMediaType = NMT_CONFLICT;
		pDIB_V2->iSequence = 0;
		pDIB_V2->nDiskCount = 1;
		pDIB_V2->nSpareCount = 0;
		bResults = NdasOpGetUnitDiskLocation(
			hNDAS, 
			NULL, 
			&pDIB_V2->UnitDisks[0]);
	}  else if(IS_NDAS_DIBV1_WRONG_VERSION(DIB_V1) || // no DIB information
		(NDAS_DIB_DISK_TYPE_MIRROR_MASTER != DIB_V1.DiskType &&
		NDAS_DIB_DISK_TYPE_MIRROR_SLAVE != DIB_V1.DiskType &&
		NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST != DIB_V1.DiskType &&
		NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND != DIB_V1.DiskType))
	{
		// Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1 is not acceptable either	
		pDIB_V2->iMediaType = NMT_SINGLE;
		pDIB_V2->iSequence = 0;
		pDIB_V2->nDiskCount = 1;
		pDIB_V2->nSpareCount = 0;

		// only 1 unit		
		bResults = NdasOpGetUnitDiskLocation(
			hNDAS, 
			NULL, 
			&pDIB_V2->UnitDisks[0]);
	}
	else
	{
		// pair(2) disks (mirror, aggregation)
		UNIT_DISK_LOCATION *pUnitDiskLocation0, *pUnitDiskLocation1;
		if(NDAS_DIB_DISK_TYPE_MIRROR_MASTER == DIB_V1.DiskType ||
			NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST == DIB_V1.DiskType)
		{
			pUnitDiskLocation0 = &pDIB_V2->UnitDisks[0];
			pUnitDiskLocation1 = &pDIB_V2->UnitDisks[1];
		}
		else
		{
			pUnitDiskLocation0 = &pDIB_V2->UnitDisks[1];
			pUnitDiskLocation1 = &pDIB_V2->UnitDisks[0];
		}

		// 1st unit
		if(
			0x00 == DIB_V1.EtherAddress[0] &&
			0x00 == DIB_V1.EtherAddress[1] &&
			0x00 == DIB_V1.EtherAddress[2] &&
			0x00 == DIB_V1.EtherAddress[3] &&
			0x00 == DIB_V1.EtherAddress[4] &&
			0x00 == DIB_V1.EtherAddress[5]) 
		{
			// usually, there is no ehteraddress information
			bResults = NdasOpGetUnitDiskLocation(
				hNDAS, 
				NULL,
				pUnitDiskLocation0);
			TEST_AND_RETURN_IF_FAIL(bResults);
		}
		else
		{
			// but, if there is.
			CopyMemory(&pUnitDiskLocation0->MACAddr, DIB_V1.EtherAddress, 
				sizeof(pUnitDiskLocation0->MACAddr));
			pUnitDiskLocation0->UnitNumber = DIB_V1.UnitNumber;
		}

		// 2nd unit
		CopyMemory(pUnitDiskLocation1->MACAddr, DIB_V1.PeerAddress, 
			sizeof(pUnitDiskLocation1->MACAddr));
		pUnitDiskLocation1->UnitNumber = DIB_V1.UnitNumber;
		pUnitDiskLocation1->VID = 1;

		pDIB_V2->nDiskCount = 2;
		pDIB_V2->nSpareCount = 0;

		switch(DIB_V1.DiskType)
		{
		case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
			pDIB_V2->iMediaType = NMT_MIRROR;
			pDIB_V2->iSequence = 0;
			break;
		case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
			pDIB_V2->iMediaType = NMT_MIRROR;
			pDIB_V2->iSequence = 1;
			break;
		case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
			pDIB_V2->iMediaType = NMT_AGGREGATE;
			pDIB_V2->iSequence = 0;
			break;
		case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
			pDIB_V2->iMediaType = NMT_AGGREGATE;
			pDIB_V2->iSequence = 1;
			break;
		default:
			// must not jump to here
			TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_UNSUPPORTED, FALSE);	
		}
	}

	*pnDIBSize = nDIBSize;

	SET_DIB_CRC(crc32_calc, *pDIB_V2);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Functions, not for exporting
//
////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
NdasOpVerifyDiskCount(
    UINT32 BindType,
    UINT32 nDiskCount)
{
	// check disk count
	switch(BindType)
	{
	case NMT_AGGREGATE:
	case NMT_SAFE_AGGREGATE:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount >= 2 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND);
		break;
	case NMT_RAID0:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			2 == nDiskCount || 4 == nDiskCount || 8 == nDiskCount);
		break;
	case NMT_SAFE_RAID1:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount == 2);
		break;
	case NMT_RAID1:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount >= 2 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND
			&& !(nDiskCount %2));
		break;
	case NMT_RAID1R2:
	case NMT_RAID1R3:		
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount >= 2 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND);
		break;
	case NMT_RAID4:
	case NMT_RAID4R3:
	case NMT_RAID5:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			3 == nDiskCount || 5 == nDiskCount || 9 == nDiskCount);
		break;
	case NMT_RAID4R2:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount >= 3 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND);
		break;
	case NMT_SINGLE:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount > 0 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND);
		break;
	default:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT,
			FALSE);
		break;
	}

	return TRUE;
}

BOOL
NdasOpGetUnitDiskLocation(
	HNDAS hNDAS,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	UNIT_DISK_LOCATION *pUnitDiskLocation)
{
	BOOL bResults;
	BYTE DeviceId[LPXADDR_NODE_LENGTH];
	DWORD UnitNo;
	BYTE VID;
	
	if((!hNDAS && !pConnectionInfo) || !pUnitDiskLocation)
		return FALSE;

	bResults = NdasCommGetDeviceID(hNDAS, pConnectionInfo, DeviceId, &UnitNo, &VID);

	if(!bResults)
		return FALSE;

	CopyMemory(pUnitDiskLocation->MACAddr, DeviceId, sizeof(pUnitDiskLocation->MACAddr));
	pUnitDiskLocation->UnitNumber = (BYTE)UnitNo;
	pUnitDiskLocation->VID = VID;

	return TRUE;
}

BOOL
NdasOpClearMBR(
    HNDAS hNDAS,
	BOOL bInitMBR)
{
	BOOL bResults;
	int i;
	BYTE emptyData[SECTOR_SIZE];
	ZeroMemory(emptyData, sizeof(emptyData));

	bResults = TRUE;
	for(i = 0; i < (NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR); i++)
	{
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_MBR + i, 1, emptyData);

		if(FALSE == bResults)
		{
			goto out;
		}
	}

	if(bInitMBR)
	{
		bResults = NdasOpInitMBR(hNDAS);
	}
out:
	return bResults;
}

BOOL
NdasOpInitMBR(
	HNDAS hNDAS)
{
	BOOL bResults;
	int i;

	_ASSERT(NDASOPModule);
	HRSRC hrMBR = FindResource(NDASOPModule, MAKEINTRESOURCE(IDR_MBR_BIN), RT_RCDATA);
	_ASSERT(hrMBR);
	if(!hrMBR)
		return FALSE;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER, 512 == SizeofResource(NDASOPModule, hrMBR));

	HGLOBAL hMBR = LoadResource(NDASOPModule, hrMBR);
	if(!hMBR)
		return FALSE;

	void *mbr = LockResource(hMBR);
	_ASSERT(mbr);
	if(!mbr)
	{
		FreeResource(hMBR);
		return FALSE;
	}

	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
		0, 1, mbr);

	FreeResource(hMBR);
	return bResults;
}

BOOL
NdasOpClearXArea(
    HNDAS hNDAS)
{
	BOOL bResults;
	int i;
	BYTE emptyData[SECTOR_SIZE];

	ZeroMemory(emptyData, sizeof(emptyData));

	// NDAS_DIB_V1
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
		NDAS_BLOCK_LOCATION_DIB_V1, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// NDAS_DIB_V2
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
		NDAS_BLOCK_LOCATION_DIB_V2, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Content encryption information
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
		NDAS_BLOCK_LOCATION_ENCRYPT, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// RAID Meta Data
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
		NDAS_BLOCK_LOCATION_RMD, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Additional bind informations : 256 sectors
	for(i = 0; i < 256; i++)
	{
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_ADD_BIND + i, 1, emptyData);
		TEST_AND_RETURN_IF_FAIL(bResults);
	}

	// Corruption Bitmap 
	for(i = 0; i < NDAS_BLOCK_SIZE_BITMAP; i++)
	{
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
			NDAS_BLOCK_LOCATION_BITMAP + i, 1, emptyData);
		TEST_AND_RETURN_IF_FAIL(bResults);
	}
#if 0 	// Last written sector is not used anymore
	// Last written sector
	for(i = 0; i < NDAS_BLOCK_SIZE_LWR_REV1; i++)
	{
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_LWR_REV1 + i, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);
	}
#endif
	return TRUE;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDWrite(
	IN NDAS_DIB_V2 *DIB_V2_Ref,
	OUT NDAS_RAID_META_DATA *rmd
	)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	HNDAS hNDAS = NULL;
	UINT32 nDiskCount;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(rmd, sizeof(NDAS_RAID_META_DATA)));

	//
	// Clear NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag.
	// We will not call NdasOpRMDWrite for conflicted RAID status. So it is safe to clear NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED
	rmd->state &= ~NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R3 == DIB_V2_Ref->iMediaType || 
		NMT_RAID4R3 == DIB_V2_Ref->iMediaType ||
		NMT_RAID5 == DIB_V2_Ref->iMediaType	);

	nDiskCount = DIB_V2_Ref->nDiskCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER, out, 0 < nDiskCount);

	// ndasbind MUST reinitialize RMD
	::CoCreateGuid(&rmd->RaidSetId);
	::CoCreateGuid(&rmd->ConfigSetId);

	// increase USN to highest
	rmd->uiUSN = rmd->uiUSN +1;
	SET_RMD_CRC(crc32_calc, *rmd);

	// write rmd to NDAS_BLOCK_LOCATION_RMD_T
	ENUM_NDAS_DEVICES_IN_DIB(i, *DIB_V2_Ref, hNDAS, TRUE, l_ci)
	{
		if(!hNDAS)
			continue;

		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)rmd);
		if(!bResults)
			continue;
	}

	// write rmd to NDAS_BLOCK_LOCATION_RMD
	ENUM_NDAS_DEVICES_IN_DIB(i, *DIB_V2_Ref, hNDAS, TRUE, l_ci)
	{
		if(!hNDAS)
			continue;

		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)rmd);
		if(!bResults)
			continue;
	}

	bReturn = TRUE;
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}
	return bReturn;
}

/*
ensure that the raid is bound correctly
This function assumes that ...
. Use same password, protocol ... as ci
. Bind is correct (allows missing device though)
. 
Parameter 'idx' has default parameter (-1). If idx is not -1,
function read RMD from selected device.
*/

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDRead(
	IN NDAS_DIB_V2* DIB_V2_Ref,
	NDAS_RAID_META_DATA *rmd,
	INT32 idx
)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	UINT32 uiUSNMax;
	HNDAS hNDAS = NULL;
	UINT32 nDiskCount;
	NDAS_RAID_META_DATA rmd_tmp;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(rmd, sizeof(NDAS_RAID_META_DATA)));

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R2 == DIB_V2_Ref->iMediaType || NMT_RAID4R2 == DIB_V2_Ref->iMediaType ||
		NMT_RAID1R3 == DIB_V2_Ref->iMediaType || NMT_RAID4R3 == DIB_V2_Ref->iMediaType ||
		NMT_RAID5 == DIB_V2_Ref->iMediaType );

	nDiskCount = DIB_V2_Ref->nDiskCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER, out, 0 < nDiskCount);

	if(-1 == idx)
	{
		// enumerate RMD and check status
		uiUSNMax = 0;

		ENUM_NDAS_DEVICES_IN_DIB(i, *DIB_V2_Ref, hNDAS, FALSE, l_ci)
		{
			if(!hNDAS)
			{
				// device does not exist, just skip
				// TEST_AND_GOTO_IF_FAIL(out, hNDAS);
				continue;
			}

			bResults = NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&rmd_tmp);
			if(!bResults)
				continue;

			if(
				NDAS_RAID_META_DATA_SIGNATURE != rmd_tmp.Signature || 
				!IS_RMD_CRC_VALID(crc32_calc, rmd_tmp))
			{
				// invalid rmd
				continue;
			}
			else
			{
				if(uiUSNMax < rmd_tmp.uiUSN)
				{
					uiUSNMax = rmd_tmp.uiUSN;
					// newer one
					::CopyMemory(rmd, &rmd_tmp, sizeof(NDAS_RAID_META_DATA));
				}
			}
		}

		if(0 == uiUSNMax)
		{
			// not found, init rmd here
			::ZeroMemory(rmd, sizeof(NDAS_RAID_META_DATA));
			rmd->Signature = NDAS_RAID_META_DATA_SIGNATURE;
			rmd->uiUSN = 1;
			for(i = 0; i < nDiskCount; i ++)
			{
				rmd->UnitMetaData[i].iUnitDeviceIdx = (unsigned _int16)i;
			}
			SET_RMD_CRC(crc32_calc, *rmd);
		}
	}
	else
	{
		SetNdasConnectionInfoFromDIBIndex(&l_ci, FALSE, DIB_V2_Ref, idx);
		hNDAS = NdasCommConnect(&l_ci);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_FAIL, out, hNDAS);


		bResults = NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)rmd);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_FAIL, out, bResults);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_FAIL, out,
			NDAS_RAID_META_DATA_SIGNATURE == rmd->Signature && 
			IS_RMD_CRC_VALID(crc32_calc, *rmd));		

		bResults = NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_FAIL, out, bResults);
	}

	bReturn = TRUE;
out:

	if(hNDAS)
	{
		bResults = NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpClearDefectMark(
	CONST NDASCOMM_CONNECTION_INFO *ci,	// Primary device's CI. CI should not be defective member
	CONST UINT32 nDefectIndex	// DIB index whose defect is cleared
) {
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;
	NDAS_DIB_V2 DIB_V2;
	UINT32 sizeDIB = sizeof(DIB_V2);
	UINT32 nTotalDiskCount;
	UINT32 nBACLECount = 0;
	NDAS_RAID_META_DATA rmd;
	BLOCK_ACCESS_CONTROL_LIST *pBACL = NULL;
	NDAS_OOS_BITMAP_BLOCK BmpBuf;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i, j;
	HNDAS hNDAS;

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// read BACL
	if(0 != DIB_V2.BACLSize)
	{
		nBACLECount = 
			(DIB_V2.BACLSize - (sizeof(BLOCK_ACCESS_CONTROL_LIST) - sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT))) / 
			sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT);
		pBACL = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512 * BACL_SECTOR_SIZE(nBACLECount));
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, pBACL);

		bResults = NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(nBACLECount), (BYTE *)pBACL);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}
	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// read RMD
	bResults = NdasOpRMDRead(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);
	
	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		out, nDefectIndex < nTotalDiskCount);

	DWORD RoleIndex = 0;
	for(i = 0; i < nTotalDiskCount; i++)
	{
		if (rmd.UnitMetaData[i].iUnitDeviceIdx == nDefectIndex) {
			RoleIndex = i;
			break;
		}
	}

	if (RoleIndex < DIB_V2.nDiskCount) {
		// Set out of sync mark to RMD at nReplace
		rmd.UnitMetaData[RoleIndex].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
	}
	// Clear defective flag. 
	rmd.UnitMetaData[RoleIndex].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_DEFECTIVE;
	SET_RMD_CRC(crc32_calc, rmd);


	// write back DIB
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		// write BACL
		if(bResults && pBACL)
		{
			bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(nBACLECount), (BYTE *)pBACL);
		}

		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			bReturn = FALSE;
			goto out;
		}

		// Set bitmap to recover all if replaces disk is not spare.
		if(RoleIndex < DIB_V2.nDiskCount) {
			BmpBuf.SequenceNumHead = 0;
			BmpBuf.SequenceNumTail = 0;
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0xff);
			for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP; j++)
			{
				bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
					NDAS_BLOCK_LOCATION_BITMAP + j, 1,(PBYTE) &BmpBuf);
				if(!bResults)
				{
					NdasCommDisconnect(hNDAS);
					bReturn = FALSE;
					goto out;
				}
			}		
		}
	}

	//
	// write back RMD. 
	//
	bResults = NdasOpRMDWrite(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);


	bReturn = TRUE;
out:
	if (hNDAS) {

		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	HEAP_SAFE_FREE(pBACL);
	return bReturn;

}

//
// Reconfigure RAID without given disk. Assume 
//
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRemoveFromRaid(
	CONST NDASCOMM_CONNECTION_INFO *ci,	// Primary device's CI
	CONST UINT32 nRemoveIndex	// DIB index to being replaced
) {
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;
	BOOL bRemovingSpare;
	NDASCOMM_CONNECTION_INFO l_ci;
	HNDAS hNDAS;
	UINT32 nBACLECount = 0;
	NDAS_DIB_V2 DIB_V2;
	NDAS_RAID_META_DATA rmd;
	UINT32 nTotalDiskCount;
	UINT32 sizeDIB = sizeof(DIB_V2);
	BLOCK_ACCESS_CONTROL_LIST *pBACL = NULL;
	NDAS_OOS_BITMAP_BLOCK BmpBuf;
	UINT32 i, j;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// read BACL
	if(0 != DIB_V2.BACLSize)
	{
		nBACLECount = 
			(DIB_V2.BACLSize - (sizeof(BLOCK_ACCESS_CONTROL_LIST) - sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT))) / 
			sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT);
		pBACL = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512 * BACL_SECTOR_SIZE(nBACLECount));
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, pBACL);

		bResults = NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(nBACLECount), (BYTE *)pBACL);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type. Currently support RAID1 only
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R3 == DIB_V2.iMediaType /*|| 
		NMT_RAID4R3 == DIB_V2.iMediaType ||
		NMT_RAID5 == DIB_V2.iMediaType */
		);

	// Cannot remove disk when there is no spare disk.
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_OPERATION, out,
		DIB_V2.nSpareCount > 0);
	
	// read RMD
	bResults = NdasOpRMDRead(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		out, nRemoveIndex < nTotalDiskCount);

	DWORD RemovalRoleIndex = 0;
	
	for(i = 0; i < nTotalDiskCount ; i++)
	{
		if (rmd.UnitMetaData[i].iUnitDeviceIdx == nRemoveIndex) {
			RemovalRoleIndex = i;
			break;
		}
	}
	
	if (RemovalRoleIndex < DIB_V2.nDiskCount) {
		bRemovingSpare = FALSE;
	} else {
		bRemovingSpare = TRUE;
	}

	//
	// Collected enough information. 
	//

	//
	// Reconfiguration RAID members
	//
	UNIT_DISK_LOCATION	UnitDisks[NDAS_MAX_UNITS_IN_V2] = {0}; 		//  Unit map in DIB
	DWORD UnitIndex;
	DWORD IndexToRemove;
	//  1. First re-order Unitdisk based on RMD role map
	for(i=0;i<nTotalDiskCount;i++) {
		UnitIndex = rmd.UnitMetaData[i].iUnitDeviceIdx;
		::CopyMemory(&UnitDisks[i], &DIB_V2.UnitDisks[UnitIndex], sizeof(UNIT_DISK_LOCATION));
		rmd.UnitMetaData[i].iUnitDeviceIdx = (UINT16) i;
	}
	// 2. Put last disk(must be spare) to removed place. 	
	IndexToRemove = RemovalRoleIndex; // Role number is same as unit number.
	::CopyMemory(&UnitDisks[IndexToRemove], &UnitDisks[nTotalDiskCount-1], sizeof(UNIT_DISK_LOCATION));
	// 3. Set out-of-sync flag and bitmap if replaced role is active RAID member.
	if (!bRemovingSpare) {
		// Overwrite previous unit status. 
		rmd.UnitMetaData[IndexToRemove].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
	}

	// 4. Remove last entry and finish
	DIB_V2.nSpareCount--;
	::CopyMemory(DIB_V2.UnitDisks, UnitDisks, NDAS_MAX_UNITS_IN_V2 * sizeof(UNIT_DISK_LOCATION));

	// ok let's start to write

	SET_RMD_CRC(crc32_calc, rmd);

	// write back DIB
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		DIB_V2.iSequence = i;
		SET_DIB_CRC(crc32_calc, DIB_V2);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

		// write BACL
		if(bResults && pBACL)
		{
			bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(nBACLECount), (BYTE *)pBACL);
		}

		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			bReturn = FALSE;
			goto out;
		}

		// Set all bitmap to recover all if replaces disk is not spare.
		if(!bRemovingSpare) {
			BmpBuf.SequenceNumHead = 0;
			BmpBuf.SequenceNumTail = 0;
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0xff);
			for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP; j++)
			{
				bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
					NDAS_BLOCK_LOCATION_BITMAP + j, 1,(PBYTE) &BmpBuf);
				if(!bResults)
				{
					NdasCommDisconnect(hNDAS);
					bReturn = FALSE;
					goto out;
				}
			}		
		}
	}

	//
	// write back RMD. 
	//
	bResults = NdasOpRMDWrite(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}
	HEAP_SAFE_FREE(pBACL);
	return bReturn;
}

//
// Replace NDAS device itself.
//
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReplaceDevice(
	CONST NDASCOMM_CONNECTION_INFO *ci,	// Primary device's CI. Assume this device is always correct.
	CONST NDASCOMM_CONNECTION_INFO *ci_replace,	// Device to replace
	CONST UINT32 nReplace	// DIB index to being replaced
)  {
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i, j;
	HNDAS hNDAS = NULL;
	HNDAS hPrimary = NULL;
	UINT32 nTotalDiskCount;
	UINT32 nBACLECount = 0;
	NDAS_DIB_V2 DIB_V2;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;
	BLOCK_ACCESS_CONTROL_LIST *pBACL = NULL;
	NDAS_OOS_BITMAP_BLOCK BmpBuf;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci_replace, sizeof(NDASCOMM_CONNECTION_INFO)));

	// read DIB
	hPrimary = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hPrimary);

	bResults = NdasOpReadDIB(hPrimary, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// read BACL
	if(0 != DIB_V2.BACLSize)
	{
		nBACLECount = 
			(DIB_V2.BACLSize - (sizeof(BLOCK_ACCESS_CONTROL_LIST) - sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT))) / 
			sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT);
		pBACL = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512 * BACL_SECTOR_SIZE(nBACLECount));
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, pBACL);

		bResults = NdasCommBlockDeviceRead(hPrimary, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(nBACLECount), (BYTE *)pBACL);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}


	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R3 == DIB_V2.iMediaType /* ||
		NMT_RAID4R3 == DIB_V2.iMediaType ||
		NMT_RAID5 == DIB_V2.iMediaType */
		);

	//
	// read RMD from primary device
	//
	bResults = NdasCommBlockDeviceRead(hPrimary, NDAS_BLOCK_LOCATION_RMD, 1, (BYTE *)&rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		out, nReplace < nTotalDiskCount);

	if (hPrimary) {
		bResults = NdasCommDisconnect(hPrimary);
		hPrimary = NULL;
	}

#if 0	// Assume that already checked or user selected to ignore the problem. 
	// check all devices
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, FALSE, l_ci)
	{
		if(nReplace == i)
		{
			// do not care if the device is alive or not
			continue;
		}
		if (!hNDAS) {
			// Continue even if target is inaccessible.
			continue;
		}

		bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			goto out;
		}

		// compare unit disks set
		if(memcmp(DIB_V2.UnitDisks, DIB_V2_Ref.UnitDisks, 
			sizeof(UNIT_DISK_LOCATION) * nTotalDiskCount))
		{
			NdasCommDisconnect(hNDAS);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_DEVICE_UNSUPPORTED, out, FALSE);
		}

		// this device is ok
	}
#endif
	// check replace size
	hNDAS = NdasCommConnect(ci_replace);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	NDAS_UNITDEVICE_HARDWARE_INFOW UnitInfo;
	::ZeroMemory(&UnitInfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));
	UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	bResults = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &UnitInfo);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_CAPACITY, out,
		UnitInfo.SectorCount.QuadPart >= DIB_V2.sizeUserSpace + NDAS_BLOCK_SIZE_XAREA);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// ok let's start to write

	// replace into DIB
	bResults = NdasOpGetUnitDiskLocation(
		NULL,
		ci_replace,
		&DIB_V2.UnitDisks[nReplace]);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	DWORD RoleIndex = 0;
	for(i = 0; i < nTotalDiskCount; i++)
	{
		if (rmd.UnitMetaData[i].iUnitDeviceIdx == nReplace) {
			RoleIndex = i;
			break;
		}
	}			
	// replace into RMD
	if(RoleIndex < DIB_V2.nDiskCount)
	{
		// If there is already a fault device, it should be nReplace. If not, we can't proceed (2 fails).
		for(i = 0; i < DIB_V2.nDiskCount; i++)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_TOO_MANY_MISSING_MEMBER, out,
				!(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & rmd.UnitMetaData[i].UnitDeviceStatus) ||
				RoleIndex == i);

		}
		// Set fault to RMD at nReplace
		rmd.UnitMetaData[RoleIndex].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
	}
	else
	{
		// Spare disk
	}
	// Clear defective flag. 
	rmd.UnitMetaData[RoleIndex].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_DEFECTIVE;
	rmd.state &= ~NDAS_RAID_META_DATA_STATE_UNMOUNTED;

	SET_RMD_CRC(crc32_calc, rmd);

	// write back DIB
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		DIB_V2.iSequence = i;
		SET_DIB_CRC(crc32_calc, DIB_V2);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

		// write BACL
		if(bResults && pBACL)
		{
			bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(nBACLECount), (BYTE *)pBACL);
		}

		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			bReturn = FALSE;
			goto out;
		}

		//
		// Set all bitmap to recover all if replaced disk is not spare.
		//
		if(RoleIndex < DIB_V2.nDiskCount) {
			BmpBuf.SequenceNumHead = 0;
			BmpBuf.SequenceNumTail = 0;
			FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0xff);
			for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP; j++)
			{
				bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
					NDAS_BLOCK_LOCATION_BITMAP + j, 1,(PBYTE) &BmpBuf);
				if(!bResults)
				{
					NdasCommDisconnect(hNDAS);
					bReturn = FALSE;
					goto out;
				}
			}
		} else {

#if 0
			// Replaced device is spare disk. Copy bitmap from primary unit. 

			for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP; j++)
			{
				bResults = NdasCommBlockDeviceRead(hPrimary, 
					NDAS_BLOCK_LOCATION_BITMAP + j, 1,(PBYTE) &BmpBuf);
				if(!bResults)
				{
					NdasCommDisconnect(hNDAS);
					bReturn = FALSE;
					goto out;
				}			
				bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
					NDAS_BLOCK_LOCATION_BITMAP + j, 1,(PBYTE) &BmpBuf);
				if(!bResults)
				{
					NdasCommDisconnect(hNDAS);
					bReturn = FALSE;
					goto out;
				}
			}
#endif
			// 
			// Replaced disk is spare disk, so bitmap of disk is not used. But in case, just fill all dirty only to spare disk	.
			//
			if (nReplace == i) {
				BmpBuf.SequenceNumHead = 0;
				BmpBuf.SequenceNumTail = 0;
				FillMemory(BmpBuf.Bits, sizeof(BmpBuf.Bits), 0xff);
				for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP; j++)
				{
					bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
						NDAS_BLOCK_LOCATION_BITMAP + j, 1,(PBYTE) &BmpBuf);
					if(!bResults)
					{
						NdasCommDisconnect(hNDAS);
						bReturn = FALSE;
						goto out;
					}
				}
			} 
		}
	}

	//
	// write back RMD. Don't use ci. ci may be being replaced.
	//
	bResults = NdasOpRMDWrite(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	if (hPrimary) {
		bResults = NdasCommDisconnect(hPrimary);
	}

	HEAP_SAFE_FREE(pBACL);
	return bReturn;
}


NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpAppend(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ciAppend)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;
	DWORD HardwareVersion = 0;
	NDAS_UNITDEVICE_HARDWARE_INFOW udinfo;
	
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ciAppend, sizeof(NDASCOMM_CONNECTION_INFO)));

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_AGGREGATE == DIB_V2.iMediaType);

	nTotalDiskCount = DIB_V2.nDiskCount;

	// check all devices
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			hNDAS = NULL;
			goto out;
		}

		// compare unit disks set
		if(memcmp(DIB_V2.UnitDisks, DIB_V2_Ref.UnitDisks, 
			sizeof(UNIT_DISK_LOCATION) * nTotalDiskCount) ||
			NMT_AGGREGATE != DIB_V2_Ref.iMediaType)
		{
			NdasCommDisconnect(hNDAS);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_DEVICE_UNSUPPORTED, out, FALSE);
		}

		// this device is ok
	}

	// ok let's start to write
	// add disk to DIB
	DIB_V2.nDiskCount++;
	nTotalDiskCount = DIB_V2.nDiskCount;

	bResults = NdasOpVerifyDiskCount(NMT_AGGREGATE, nTotalDiskCount);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasOpGetUnitDiskLocation(
		NULL,
		ciAppend,
		&DIB_V2.UnitDisks[nTotalDiskCount -1]);
	TEST_AND_GOTO_IF_FAIL(out, bResults);
	
	DIB_V2.UnitDiskInfos[nTotalDiskCount -1].HwVersion = HardwareVersion;

	// write back DIB
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		DIB_V2.iSequence = i;

		if(nTotalDiskCount -1 == i)
		{
			// calculate the size of the append disk
			::ZeroMemory(&udinfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));
			udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
			bResults = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &udinfo);

			// just % 128 of free space
			DIB_V2.sizeUserSpace = udinfo.SectorCount.QuadPart 
				- NDAS_BLOCK_SIZE_XAREA;
			DIB_V2.sizeUserSpace -= DIB_V2.sizeUserSpace % 128;
		}
		else
		{
			// keep current size
			bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
			if(!bResults)
			{
				NdasCommDisconnect(hNDAS);
				goto out;
			}

			DIB_V2.sizeUserSpace = DIB_V2_Ref.sizeUserSpace;			
		}

		SET_DIB_CRC(crc32_calc, DIB_V2);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			goto out;
		}
	}

	bReturn = TRUE;
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}
	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRAID1R3IsFixRequired(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	BOOL bFixIt)
{
	BOOL bReturn = FALSE;
	BOOL bResults = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd_ref, rmd[2];
	GUID RaidSetId, RaidSetIdBackup;
	UINT32 idx_dib;

	BOOL missing[2];
	NDAS_UNIT_META_DATA *pUMD;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);
	TEST_AND_GOTO_IF_FAIL(out, NMT_RAID1R3 == DIB_V2.iMediaType);

	// degrade mode will never touch RMD unit device order
	// you can use any RMD to get sequence
	bResults = NdasOpRMDRead(&DIB_V2, &rmd_ref);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);


	// read RMD of 1st
	for(UINT32 i = 0; i < 2; i++)
	{
		idx_dib = rmd_ref.UnitMetaData[i].iUnitDeviceIdx;
		SetNdasConnectionInfoFromDIBIndex(&l_ci, FALSE, &DIB_V2, idx_dib);
		hNDAS = NdasCommConnect(&l_ci);
		if(!hNDAS)
		{
			missing[i] = TRUE;
		}
		else
		{
			NdasCommDisconnect(hNDAS);
			hNDAS = NULL;

			missing[i] = FALSE;
			bResults = NdasOpRMDRead(&DIB_V2, &rmd[i], idx_dib);
			TEST_AND_GOTO_IF_FAIL(out, bResults);

			TEST_AND_GOTO_IF_FAIL(out,
				!(rmd[i].UnitMetaData[0].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_DEFECTIVE));
		}
	}

	if (FALSE == missing[0] && FALSE == missing[1])
	{
		if (!(rmd[0].UnitMetaData[1].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE) &&
			!(rmd[1].UnitMetaData[0].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE))
		{
			bResults = FALSE; // everything is normal
			goto out;
		}
		else if ((rmd[0].UnitMetaData[1].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE) &&
			(rmd[1].UnitMetaData[0].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE))
		{
			bResults = FALSE; // conflict
			goto out;
		}
		else
		{
			if (((rmd[0].UnitMetaData[1].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE) &&
				(rmd[0].RaidSetIdBackup == rmd[1].RaidSetId)) ||
				((rmd[1].UnitMetaData[0].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE) &&
				(rmd[1].RaidSetIdBackup == rmd[0].RaidSetId)))
			{
				// clear offline flag
				if(bFixIt)
				{
					rmd_ref.UnitMetaData[0].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_OFFLINE;
					rmd_ref.UnitMetaData[1].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_OFFLINE;
					bResults = NdasOpRMDWrite(&DIB_V2, &rmd_ref);
					TEST_AND_GOTO_IF_FAIL(out, bResults);
				}
				bResults = TRUE;
				goto out;
			}
		}
	}
	else if (TRUE == missing[0] && TRUE == missing[1])
	{
		// can't do anything
		bResults = FALSE;
		goto out;
	}
	else
	{
		if (((TRUE == missing[0]) &&
			(rmd[1].UnitMetaData[0].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE)) ||
			((TRUE == missing[1]) &&
			(rmd[0].UnitMetaData[1].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE)))
		{
			// already degrade mode
			bResults = FALSE;
			goto out;
		}

		if(bFixIt)
		{
			if(TRUE == missing[0])
			{
				rmd_ref.UnitMetaData[0].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_OFFLINE;
				rmd_ref.UnitMetaData[1].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_OFFLINE;
			}
			else
			{
				rmd_ref.UnitMetaData[0].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_OFFLINE;
				rmd_ref.UnitMetaData[1].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_OFFLINE;
			}
			rmd_ref.RaidSetIdBackup = rmd_ref.RaidSetId;
			bResults = NdasOpRMDWrite(&DIB_V2, &rmd_ref);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}
	}
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRAID4R3or5IsFixRequired(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	BOOL bFixIt)
{
	BOOL bReturn = FALSE;
	BOOL bResults = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	HNDAS hNDAS = NULL;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd_ref, rmd[2];
	GUID RaidSetId, RaidSetIdBackup;
	UINT32 idx_dib;

//	BOOL missing[2];
	INT32 missing = -1, offline = -1;
	NDAS_UNIT_META_DATA *pUMD;

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);
	TEST_AND_GOTO_IF_FAIL(out, NMT_RAID1R3 == DIB_V2.iMediaType);

	// degrade mode will never touch RMD unit device order
	// you can use any RMD to get sequence
	bResults = NdasOpRMDRead(&DIB_V2, &rmd_ref);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	for (UINT32 i = 0; i < DIB_V2.nDiskCount + DIB_V2.nSpareCount; i++)
	{
		TEST_AND_GOTO_IF_FAIL(out,
			!(rmd_ref.UnitMetaData[i].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_DEFECTIVE));
	}

	// ignore spare
	for (UINT32 i = 0;
		(
			(i < DIB_V2.nDiskCount) ?
				(
					idx_dib = rmd_ref.UnitMetaData[i].iUnitDeviceIdx,
					SetNdasConnectionInfoFromDIBIndex(&l_ci, FALSE, &DIB_V2, idx_dib),
					hNDAS = NdasCommConnect(&(l_ci)),
					TRUE
				) : FALSE
		);
		(
			hNDAS ? NdasCommDisconnect(hNDAS) : 0,
			hNDAS = NULL,
			i++
		)
		)
	{
		if(!hNDAS)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_TOO_MANY_MISSING_MEMBER,
				out,
				-1 != missing);

			missing = i;
//			continue;
		}
		if(rmd_ref.UnitMetaData[i].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_INVALID_RAID,
				out,
				-1 != offline);
			offline = i;
		}
	}

	if(-1 == missing)
	{
		if(-1 == offline)
		{
			// normal
			bResults = FALSE;
			goto out;
		}
		else
		{
			if(bFixIt)
			{
				rmd_ref.UnitMetaData[offline].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_OFFLINE;
				bResults = NdasOpRMDWrite(&DIB_V2, &rmd_ref);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}
			bResults = TRUE;
			goto out;
		}
	}
	else
	{
		if(-1 == offline)
		{
			if(bFixIt)
			{
				rmd_ref.UnitMetaData[missing].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_OFFLINE;
				rmd_ref.RaidSetIdBackup = rmd_ref.RaidSetId;
				bResults = NdasOpRMDWrite(&DIB_V2, &rmd_ref);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}
			bResults = TRUE;
			goto out;
		}
		else if(offline == missing)
		{
			// already degrade mode
			bResults = FALSE;
			goto out;
		}
		else
		{
			_ASSERT(FALSE);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_INVALID_RAID,
				out,
				FALSE);
		}
	}
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	return FALSE;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSpareAdd(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ciSpare)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;
	DWORD HardwareVersion = 0;
	
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ciSpare, sizeof(NDASCOMM_CONNECTION_INFO)));

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R3 == DIB_V2.iMediaType /* || 
		NMT_RAID4R3 == DIB_V2.iMediaType ||
		NMT_RAID5 == DIB_V2.iMediaType */);

	// read RMD
	bResults = NdasOpRMDRead(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	// check all devices
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

        bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			goto out;
		}

		// compare unit disks set
		if(memcmp(DIB_V2.UnitDisks, DIB_V2_Ref.UnitDisks, 
			sizeof(UNIT_DISK_LOCATION) * nTotalDiskCount))
		{
			NdasCommDisconnect(hNDAS);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_DEVICE_UNSUPPORTED, out, FALSE);
		}

		// this device is ok
	}

	// check spare size
	{
		hNDAS = NdasCommConnect(ciSpare);
		TEST_AND_GOTO_IF_FAIL(out, hNDAS);

		NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};
		NDAS_DEVICE_HARDWARE_INFO dinfo = {0};

		udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
		bResults = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &udinfo);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_CAPACITY, out,
			udinfo.SectorCount.QuadPart >= DIB_V2.sizeUserSpace + NDAS_BLOCK_SIZE_XAREA);

		dinfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
		bResults = NdasCommGetDeviceHardwareInfo(hNDAS, &dinfo);
		if (!bResults) {
			goto out;
		}

		HardwareVersion = dinfo.HardwareVersion;

		bResults = NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}


	// ok let's start to write

	// add spare to DIB
	DIB_V2.nSpareCount++;
	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	bResults = NdasOpVerifyDiskCount(NMT_AGGREGATE, nTotalDiskCount);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasOpGetUnitDiskLocation(
		NULL,
		ciSpare,
		&DIB_V2.UnitDisks[nTotalDiskCount -1]);
	TEST_AND_GOTO_IF_FAIL(out, bResults);
	
	DIB_V2.UnitDiskInfos[nTotalDiskCount -1].HwVersion = HardwareVersion;

	//
	// No need to copy OOS bitmap. spare disk's OOS bitmap will be ignored.
	// 

	// add spare to RMD
	rmd.UnitMetaData[nTotalDiskCount -1].iUnitDeviceIdx = nTotalDiskCount -1;
	rmd.UnitMetaData[nTotalDiskCount -1].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_SPARE;

	// write back DIB
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		DIB_V2.iSequence = i;
		SET_DIB_CRC(crc32_calc, DIB_V2);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			goto out;
		}
	}
	
	// write back RMD
	bResults = NdasOpRMDWrite(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSpareRemove(
	CONST NDASCOMM_CONNECTION_INFO *ci_spare
)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;
	UINT32 iDeviceNo = 0, iDeviceNoInRMD = 0;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci_spare, sizeof(NDASCOMM_CONNECTION_INFO)));

	// read DIB
	hNDAS = NdasCommConnect(ci_spare);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	hNDAS = NULL;
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R3 == DIB_V2.iMediaType /* || 
		NMT_RAID4R3 == DIB_V2.iMediaType ||
		NMT_RAID4R5 == DIB_V2.iMediaType */
		);

	// read RMD
	bResults = NdasOpRMDRead(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	// do not accept if there is no spare disk
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out, DIB_V2.nSpareCount);

	// find device no
	for(i = 0; i < nTotalDiskCount; i++)
	{
		C_ASSERT(sizeof(DIB_V2.UnitDisks[i].MACAddr) ==
			sizeof(ci_spare->Address.DeviceId.Node));
		if(!memcmp(DIB_V2.UnitDisks[i].MACAddr, ci_spare->Address.DeviceId.Node, 
			sizeof(DIB_V2.UnitDisks[i].MACAddr)))
		{
			iDeviceNo = i;
			break;
		}
	}

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		i != nTotalDiskCount);

	// find device no in RMD
	// do not accept if iDeviceNo is not a spare disk
	for(i = 0; i < nTotalDiskCount; i++)
	{
		if(iDeviceNo == rmd.UnitMetaData[i].iUnitDeviceIdx)
		{
			iDeviceNoInRMD = i;
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
				i >= DIB_V2.nDiskCount);
		}
	}

	// check all devices
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			goto out;
		}

		// compare unit disks set
		if(memcmp(DIB_V2.UnitDisks, DIB_V2_Ref.UnitDisks, 
			sizeof(UNIT_DISK_LOCATION) * nTotalDiskCount))
		{
			NdasCommDisconnect(hNDAS);
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
				NDASOP_ERROR_DEVICE_UNSUPPORTED, out, FALSE);
		}

		// this device is ok
	}

	// ok let's start to write
	
	// set new DIB
	DIB_V2.nSpareCount--;
	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;
	::MoveMemory(
		&DIB_V2.UnitDisks[iDeviceNo],
		&DIB_V2.UnitDisks[iDeviceNo +1],
		(nTotalDiskCount - iDeviceNo) * sizeof(UNIT_DISK_LOCATION)
		);
	::ZeroMemory(&DIB_V2.UnitDisks[nTotalDiskCount], sizeof(UNIT_DISK_LOCATION));

	SET_DIB_CRC(crc32_calc, DIB_V2);

	// set new RMD
	::MoveMemory(
		&rmd.UnitMetaData[iDeviceNoInRMD],
		&rmd.UnitMetaData[iDeviceNoInRMD +1],
		(nTotalDiskCount - iDeviceNoInRMD) * sizeof(NDAS_UNIT_META_DATA)
		);
	::ZeroMemory(&rmd.UnitMetaData[nTotalDiskCount], sizeof(NDAS_UNIT_META_DATA));

	for(i = 0; i < nTotalDiskCount; i++)
	{
		if(iDeviceNo < rmd.UnitMetaData[i].iUnitDeviceIdx)
			rmd.UnitMetaData[i].iUnitDeviceIdx--;
	}

	SET_RMD_CRC(crc32_calc, rmd);

	// write back DIB
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		DIB_V2.iSequence = i;
		SET_DIB_CRC(crc32_calc, DIB_V2);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

		if(!bResults)
		{
			NdasCommDisconnect(hNDAS);
			goto out;
		}
	}

	// write back RMD
	bResults = NdasOpRMDWrite(&DIB_V2, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasOpBind(1, ci_spare, NMT_SINGLE, 0);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}
	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpWriteBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL)
{
	NDAS_DIB_V2 DIB_V2;
	BLOCK_ACCESS_CONTROL_LIST *pBACL_Disk = NULL;
	UINT32 crc;
	BOOL bResults, bReturn = FALSE;
	UINT32 sizeDIB = sizeof(DIB_V2);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pBACL, sizeof(BLOCK_ACCESS_CONTROL_LIST)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		pBACL->ElementCount < 64);

	// read DIB to replace with
	bResults = NdasOpReadDIB(hNdasDevice, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// write DIB
	DIB_V2.BACLSize = (0 != pBACL->ElementCount) ? BACL_SIZE(pBACL->ElementCount) : 0;
	SET_DIB_CRC(crc32_calc, DIB_V2);
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

	if(0 == pBACL->ElementCount)
	{
		// nothing to do anymore
		bReturn = TRUE;
		goto out;
	}

	// init pBACL_Disk
	pBACL_Disk = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512 * BACL_SECTOR_SIZE(pBACL->ElementCount));
	TEST_AND_GOTO_IF_FAIL(out, pBACL_Disk);
	::CopyMemory(pBACL_Disk, pBACL, BACL_SIZE(pBACL->ElementCount));
	pBACL_Disk->Signature = BACL_SIGNATURE;
	pBACL_Disk->Version = BACL_VERSION;
	pBACL_Disk->crc = crc32_calc(
		(unsigned char *)&pBACL->Elements[0], 
		sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT) * (pBACL->ElementCount));

	// write pBACL_Disk on disk
	bResults = NdasCommBlockDeviceWriteSafeBuffer(
		hNdasDevice,
		NDAS_BLOCK_LOCATION_BACL,
		BACL_SECTOR_SIZE(pBACL->ElementCount),
		(BYTE *)pBACL_Disk);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	HEAP_SAFE_FREE(pBACL_Disk);

	return bReturn;
}


NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReadBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL)
{
	NDAS_DIB_V2 DIB_V2;
	BLOCK_ACCESS_CONTROL_LIST *pBACL_Disk = NULL;
	UINT32 crc;

	UINT32 ElementCount;
	BOOL bResults, bReturn = FALSE;
	UINT32 sizeDIB = sizeof(DIB_V2);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pBACL, sizeof(BLOCK_ACCESS_CONTROL_LIST)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		pBACL->ElementCount < 64);

	// read DIB to get element size of BACL
	bResults = NdasOpReadDIB(hNdasDevice, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	if(0 != DIB_V2.BACLSize)
	{
		// read BACL
		ElementCount = 
			(DIB_V2.BACLSize - (sizeof(BLOCK_ACCESS_CONTROL_LIST) - sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT))) / 
			sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT);
		TEST_AND_GOTO_IF_FAIL(out, 0 < ElementCount && ElementCount < 64);

		pBACL_Disk = (BLOCK_ACCESS_CONTROL_LIST *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512 * BACL_SECTOR_SIZE(ElementCount));
		TEST_AND_GOTO_IF_FAIL(out, pBACL_Disk);

		bResults = NdasCommBlockDeviceRead(hNdasDevice, NDAS_BLOCK_LOCATION_BACL, BACL_SECTOR_SIZE(ElementCount), (BYTE *)pBACL_Disk);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		TEST_AND_GOTO_IF_FAIL(out, BACL_SIGNATURE == pBACL_Disk->Signature);
		TEST_AND_GOTO_IF_FAIL(out, BACL_VERSION >= pBACL_Disk->Version);	
		TEST_AND_GOTO_IF_FAIL(out, 0 < pBACL_Disk->ElementCount && pBACL_Disk->ElementCount < 64);
		TEST_AND_GOTO_IF_FAIL(out,
			crc32_calc((unsigned char *)&pBACL_Disk->Elements[0],
			sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT) * (pBACL_Disk->ElementCount)) == 
			pBACL_Disk->crc);
	}

	if(0 == pBACL->ElementCount)
	{
		// just set BACL element size. we are done.
		pBACL->ElementCount = (0 != DIB_V2.BACLSize) ? pBACL_Disk->ElementCount : 0;

		bReturn = TRUE;
		goto out;
	}

	::CopyMemory(pBACL, pBACL_Disk, BACL_SIZE(pBACL_Disk->ElementCount));

	bReturn = TRUE;
out:
	
	HEAP_SAFE_FREE(pBACL_Disk);

	return bReturn;
}


BOOL
_NdasOpGetOutOfSyncStatus(
	HNDAS hNdasDevice, 
	PNDAS_DIB_V2 Dib,
	DWORD* TotalBits, 
	DWORD* SetBits
) {
	UINT32 OnBitCount;
	UINT32 BitCount;
	UINT32 BmpSectorCount;
	UINT32 CurBitCount;
	UCHAR OnBits[] = {1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7};
	PNDAS_OOS_BITMAP_BLOCK BmpBuffer = NULL;
	BOOL bResults;
	DWORD i, j;
	
	*TotalBits = 0;
	*SetBits = 0;

	if (Dib->iSectorsPerBit ==0) {
		return FALSE;
	}
	BitCount = (DWORD)((Dib->sizeUserSpace + Dib->iSectorsPerBit - 1)/Dib->iSectorsPerBit);
	BmpSectorCount = (BitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;

	OnBitCount = 0;
	BmpBuffer = (PNDAS_OOS_BITMAP_BLOCK) ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		BmpSectorCount * sizeof(NDAS_OOS_BITMAP_BLOCK));

	bResults =NdasCommBlockDeviceRead(hNdasDevice, NDAS_BLOCK_LOCATION_BITMAP, BmpSectorCount, (PBYTE)BmpBuffer);
	if (!bResults) {
		::HeapFree(::GetProcessHeap(), 0, BmpBuffer);
		return bResults;
	}
	CurBitCount = 0;
	for(i=0;i<BmpSectorCount;i++) {
		for(j=0;j<NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8;j++) {
			if (BmpBuffer[i].Bits[j/8] & OnBits[j%8]) 
				OnBitCount++;
			CurBitCount++;
			if (CurBitCount >= BitCount)
				break;
		}
	}
	*TotalBits = BitCount;
	*SetBits = OnBitCount;

	if (BmpBuffer) {
		::HeapFree(::GetProcessHeap(), 0, BmpBuffer);
	}
	
	return bResults;
}

//
// Assumption: all members are registered and has enough access right before calling this function
//			or caller should check them after calling this function.
//
// To fix: this function access to unregistered/disabled unit.
// 		need to pass connection info and unregister/disabled/online/offline info.
//
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpGetRaidInfo(
	HNDAS hNdasDevice,		// Primary device
	PNDASOP_RAID_INFO RaidInfo)
{
	NDAS_DIB_V2 			PrimaryDIB_V2 ={0};
	NDAS_RAID_META_DATA 	PrimaryRmd = {0};
	PNDAS_DIB_V2 			MemberDIB_V2 = NULL;
	PNDAS_RAID_META_DATA 	MemberRmd = NULL;
	BOOL InternalErr = FALSE;
	BOOL bReturn = TRUE;
	UINT32 MountabilityFlags = 0;
	DWORD FailReason = NDAS_RAID_FAIL_REASON_NONE;
	UINT32 nDIBSize = sizeof(NDAS_DIB_V2);
	UINT32 TotalDiskCount = 0;
	UINT32 i, j;
	HNDAS *ahNDAS = NULL;
	NDASCOMM_CONNECTION_INFO CI;
	DWORD PrimaryDevNo;
	DWORD UsableDevCount;
	DWORD MissingSpareCount;
	DWORD AvailSpareCount;
	DWORD ActiveDiskCount;
	DWORD OutOfSyncCount;
	BOOL SpareUsed = FALSE;
	UINT32 HighestUsn;
	
 	if (RaidInfo->Size != sizeof(NDASOP_RAID_INFO)) {
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
 	}

	::ZeroMemory(RaidInfo, sizeof(NDASOP_RAID_INFO));
	RaidInfo->Size = sizeof(NDASOP_RAID_INFO);

	//
	// Read DIB from primary device	
	//
	if(!NdasOpReadDIB(hNdasDevice, &PrimaryDIB_V2, &nDIBSize)) {
		// Cannot read DIB

		MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL;
		goto out;
	}
	
	//
	// Check DIB version
	//
	if(IS_HIGHER_VERSION_V2(PrimaryDIB_V2)) {
		MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;		
		FailReason |= NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION;
		goto out;
	}

	// Extract required information from DIB
	TotalDiskCount = PrimaryDIB_V2.nDiskCount + PrimaryDIB_V2.nSpareCount;
	RaidInfo->MemberCount = TotalDiskCount;
	RaidInfo->Type = PrimaryDIB_V2.iMediaType;

	// Fill device info for each member
	for(i = 0; i < TotalDiskCount; i++)
	{
		::CopyMemory(
			RaidInfo->Members[i].DeviceId.Node, 
			PrimaryDIB_V2.UnitDisks[i].MACAddr, 
			sizeof(RaidInfo->Members[i].DeviceId.Node));
		RaidInfo->Members[i].DeviceId.VID = PrimaryDIB_V2.UnitDisks[i].VID;
		if (PrimaryDIB_V2.UnitDisks[i].VID == 0) {
			RaidInfo->Members[i].DeviceId.VID = 1;
		}
		RaidInfo->Members[i].UnitNo = PrimaryDIB_V2.UnitDisks[i].UnitNumber;
	}

	//
	// Check RAID type
	//
	switch(PrimaryDIB_V2.iMediaType) {
	case NMT_INVALID:
	case NMT_SINGLE:
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_CDROM:	
	case NMT_OPMEM:
	case NMT_FLASH:
		MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		FailReason |= NDAS_RAID_FAIL_REASON_NOT_A_RAID;	// This should not happen anyway..
		goto out;
	case NMT_CONFLICT:
		MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		FailReason |= NDAS_RAID_FAIL_REASON_INCONSISTENT_DIB;
		goto out;
	case NMT_MIRROR:
	case NMT_RAID1:	
	case NMT_RAID4:	
	case NMT_RAID1R2:
	case NMT_RAID4R2:
		MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		FailReason |= NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED;
		// We cannot mount the disk, but we can fill member info.
		break;
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_RAID5:		
		// We support only this.
		break;
	default: // Unknown type
		MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		FailReason |= NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID;
		goto out;
	}

	PrimaryDevNo = PrimaryDIB_V2.iSequence;

	// Read RMD from primary device.
	if (NMT_RAID1R3 == PrimaryDIB_V2.iMediaType || 
		NMT_RAID4R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID5 == PrimaryDIB_V2.iMediaType) {
		bReturn = NdasCommBlockDeviceRead(hNdasDevice, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&PrimaryRmd);
		if(!bReturn) {
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL;
			goto out;
		}
		if(NDAS_RAID_META_DATA_SIGNATURE != PrimaryRmd.Signature || 
			!IS_RMD_CRC_VALID(crc32_calc, PrimaryRmd)) {
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			FailReason |= NDAS_RAID_FAIL_REASON_RMD_CORRUPTED;
			// Mark flag
			if (PrimaryDIB_V2.iSequence < MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER) {
				RaidInfo->Members[PrimaryDevNo].Flags |= NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED;
			}
//			goto out;
		}
		::CopyMemory(&RaidInfo->RaidSetId, &PrimaryRmd.RaidSetId, sizeof(GUID));
		::CopyMemory(&RaidInfo->RaidSetIdBackup, &PrimaryRmd.RaidSetIdBackup, sizeof(GUID));
		::CopyMemory(&RaidInfo->ConfigSetId, &PrimaryRmd.ConfigSetId, sizeof(GUID));
	}

	ahNDAS = (HNDAS *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		TotalDiskCount * sizeof(HNDAS));
	if (!ahNDAS) {
		InternalErr = TRUE;
		goto out;
	}
	MemberDIB_V2 = (PNDAS_DIB_V2) ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		TotalDiskCount * sizeof(NDAS_DIB_V2));
	if (!MemberDIB_V2) {
		InternalErr = TRUE;
		goto out;
	}
		
	MemberRmd = (PNDAS_RAID_META_DATA) ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		TotalDiskCount * sizeof(NDAS_RAID_META_DATA));
	if (!MemberRmd) {
		InternalErr = TRUE;
		goto out;
	}


	// Try to connect to all members and check RMD
	for(i = 0; i < TotalDiskCount; i++) {
		if (i == PrimaryDevNo) {
			// No need to read primary device again.
			ahNDAS[i] = hNdasDevice;
			::CopyMemory(&MemberDIB_V2[i], &PrimaryDIB_V2, sizeof(NDAS_DIB_V2));
			::CopyMemory(&MemberRmd[i], &PrimaryRmd, sizeof(NDAS_RAID_META_DATA));
			// Assume primary dev is all right.
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_ONLINE;
			continue;
		}

		SetNdasConnectionInfoFromDIBIndex(
			&CI, FALSE, &PrimaryDIB_V2, i);
		ahNDAS[i] = NdasCommConnect(&CI);
		if (ahNDAS[i]) {
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_ONLINE;
		} else {
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_OFFLINE;
			ahNDAS[i] = 0;
			FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
			continue;
		}
		
		// Read DIB from each member
		nDIBSize = sizeof(NDAS_DIB_V2);
		bReturn = NdasOpReadDIB(ahNDAS[i], &MemberDIB_V2[i], &nDIBSize);
		if (!bReturn) {
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_IO_FAILURE;
			FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL;
			continue;
		}

		// Compare key members...
		if (MemberDIB_V2[i].sizeXArea != PrimaryDIB_V2.sizeXArea ||
			MemberDIB_V2[i].iSectorsPerBit != PrimaryDIB_V2.iSectorsPerBit ||
			MemberDIB_V2[i].iMediaType != PrimaryDIB_V2.iMediaType ||
			MemberDIB_V2[i].nDiskCount != PrimaryDIB_V2.nDiskCount ||
			MemberDIB_V2[i].nSpareCount != PrimaryDIB_V2.nSpareCount ||
			memcmp(&MemberDIB_V2[i].UnitDisks, &PrimaryDIB_V2.UnitDisks, sizeof(UNIT_DISK_LOCATION) * TotalDiskCount) !=0) 
		{
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH;
			FailReason |= NDAS_RAID_FAIL_REASON_DIB_MISMATCH;
			continue;
		}
		// sizeUserSpace should be same for some RAID mode.
		if ((PrimaryDIB_V2.iMediaType == NMT_RAID0 ||
			PrimaryDIB_V2.iMediaType == NMT_RAID1R3 ||
			PrimaryDIB_V2.iMediaType == NMT_RAID4R3 ||
			PrimaryDIB_V2.iMediaType == NMT_RAID5) &&
			MemberDIB_V2[i].sizeUserSpace != PrimaryDIB_V2.sizeUserSpace) 
		{
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH;
			FailReason |= NDAS_RAID_FAIL_REASON_DIB_MISMATCH;
			continue;
		}
		// Check sequence number is correct
		if (MemberDIB_V2[i].iSequence != i) {
			RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH;
			FailReason |= NDAS_RAID_FAIL_REASON_DIB_MISMATCH;
			continue;
		}

		// Read RMD if exist
		if (NMT_RAID1R3 == PrimaryDIB_V2.iMediaType || 
			NMT_RAID4R3 == PrimaryDIB_V2.iMediaType ||
			NMT_RAID5 == PrimaryDIB_V2.iMediaType) {
			bReturn = NdasCommBlockDeviceRead(ahNDAS[i], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&MemberRmd[i]);
			if(NDAS_RAID_META_DATA_SIGNATURE != MemberRmd[i].Signature || 
				!IS_RMD_CRC_VALID(crc32_calc, MemberRmd[i]))
			{
				RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED;
				FailReason |= NDAS_RAID_FAIL_REASON_RMD_CORRUPTED;
				continue;
			}
			if (memcmp(&PrimaryRmd.RaidSetId,  &MemberRmd[i].RaidSetId, sizeof(GUID)) != 0) {
				RaidInfo->Members[i].Flags |= NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET;
				FailReason |= NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET;
				continue;
			}
			if (memcmp(&PrimaryRmd.ConfigSetId,  &MemberRmd[i].ConfigSetId, sizeof(GUID)) != 0) {
				SpareUsed = TRUE;
				// If disk with different config set exist, do not permit mounting until the confliction is resolved.
				MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
				FailReason |= NDAS_RAID_FAIL_REASON_SPARE_USED;
				// we don't know which device is replaced by spare yet.
				continue;
			}			
		}
	}

	//
	// For redundent RAID, all disk may not have same RMD. So we need to find up-to-date RMD.
	//
	if (NMT_RAID1R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID4R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID5 == PrimaryDIB_V2.iMediaType) {
		UCHAR UnitDeviceStatus;
		BOOL SpareFlagFound = FALSE;
		INT32 ReplacedDisk = -1;
		UINT32 HighestUsn = 0;
		if (SpareUsed) {
			// Find which device is replaced by spare.
			for(i = 0; i < TotalDiskCount; i++) { // DIB index
				for(j = 0; j < TotalDiskCount; j++) {// Role index
					if (!NdasOpIsValidMember(RaidInfo->Members[i].Flags)) {
						//
						//Don't access member RMD of corrupted disk
						//
						continue;
					}					
					// Find role index
					UnitDeviceStatus = MemberRmd[i].UnitMetaData[j].UnitDeviceStatus;
					if (UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE) {
						PrimaryDevNo = i;
						::CopyMemory(&PrimaryRmd, &MemberRmd[i], sizeof(PrimaryRmd));
						::CopyMemory(&RaidInfo->ConfigSetId, &PrimaryRmd.ConfigSetId, sizeof(GUID));
						SpareFlagFound = TRUE;
						ReplacedDisk = MemberRmd[i].UnitMetaData[j].iUnitDeviceIdx;
						break;
					}
				}
				if (SpareFlagFound)
					break;
			}
			if (!SpareFlagFound) {
				// RMD information is inconsistent. This cannot be happen because RMD and changed config ID is updated same time.
				_ASSERT(FALSE);
			}
		}
		//
		// Find up-to-date disk among non-spare and non-replaced-by-spare disks, out of sync
		// Spare disk may not have up-to-date OOS bitmap if it is not turned on all time.
		//
		for(i = 0; i < TotalDiskCount; i++) {
			if (!NdasOpIsValidMember(RaidInfo->Members[i].Flags)) {
				//
				//Don't access invalid memer
				//
				continue;
			}					
		
			
			// uiUsn is 0 for non-connectable disks.
			if (MemberRmd[i].uiUSN >= HighestUsn) {
				UINT32 RoleNo;
				for(j=0;j<TotalDiskCount;j++) {
					if (MemberRmd[i].UnitMetaData[j].iUnitDeviceIdx == i) {
						break;
					}
				}
				RoleNo = j;
				//
				// When USN is same, select non-OOS, non-spare, non-replaced-by-spare disk
				//
				if (i != ReplacedDisk && 
					!(MemberRmd[i].UnitMetaData[RoleNo].UnitDeviceStatus & (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED |NDAS_UNIT_META_BIND_STATUS_SPARE))) {
					PrimaryDevNo = i;
					HighestUsn = MemberRmd[i].uiUSN;
				}
			}
		}
		// If no device is found among valid disk, use any disk except replaced disk.(This can happen if only spare and oos disk is online)
		if (HighestUsn == 0) {
			for(i = 0; i < TotalDiskCount; i++) {
				if (!NdasOpIsValidMember(RaidInfo->Members[i].Flags)) {
					//
					//Don't access invalid memer
					//
					continue;
				}			

				// uiUsn is 0 for non-connectable disks.
				if (MemberRmd[i].uiUSN > HighestUsn && 
					i != ReplacedDisk) {
					PrimaryDevNo = i;
				}
			}
		}
	}

	if (NMT_RAID1R3 == PrimaryDIB_V2.iMediaType || 
		NMT_RAID4R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID5 == PrimaryDIB_V2.iMediaType) {

		NDASCOMM_CONNECTION_INFO l_ci;
		SetNdasConnectionInfoFromDIBIndex(
			&l_ci,
			FALSE,
			&PrimaryDIB_V2,
			PrimaryDevNo);
		if (
			(NMT_RAID1R3 == PrimaryDIB_V2.iMediaType && NdasOpRAID1R3IsFixRequired(&l_ci, FALSE)) ||
			((NMT_RAID4R3 == PrimaryDIB_V2.iMediaType || NMT_RAID5 == PrimaryDIB_V2.iMediaType) && NdasOpRAID1R3IsFixRequired(&l_ci, FALSE)))
		{
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			FailReason |= NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT;
			goto out;
		}

		//
		// Check RMD consistency.
		//
		BOOLEAN DegradedMountCount = 0;
		UCHAR UnitDeviceStatus;

		if (!NdasOpIsValidMember(RaidInfo->Members[PrimaryDevNo].Flags)) {
			//
			// Primary device is not a valid member, which means no disk is valid
			//
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			// Fail reason must be set prior 
			_ASSERT(FailReason);
			goto out;
		}
		
		// Set Active/spare flag from primary dev's RMD
		for(i = 0; i < TotalDiskCount; i++) {
			if (i<PrimaryDIB_V2.nDiskCount) {
				RaidInfo->Members[PrimaryRmd.UnitMetaData[i].iUnitDeviceIdx].Flags |= NDAS_RAID_MEMBER_FLAG_ACTIVE;
			} else {
				RaidInfo->Members[PrimaryRmd.UnitMetaData[i].iUnitDeviceIdx].Flags |= NDAS_RAID_MEMBER_FLAG_SPARE;
			}
		}
		
		// Collect info from each RMD
		for(i = 0; i<TotalDiskCount; i++) {
			if (NdasOpIsValidMember(RaidInfo->Members[i].Flags)) {
				//
				//NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED is meaningful only in non-spare disk.
				//
				if (!(MemberRmd[i].UnitMetaData[i].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_SPARE)
					&& MemberRmd[i].state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
					DegradedMountCount++;
				}
			}
		}

		// Get defective information from most updated RMD.		
		for(i = 0; i < TotalDiskCount; i++) {  // i is role index.
			UINT32 UnitIndex;
			UnitIndex = MemberRmd[PrimaryDevNo].UnitMetaData[i].iUnitDeviceIdx;
			UnitDeviceStatus = MemberRmd[PrimaryDevNo].UnitMetaData[i].UnitDeviceStatus;
			if (UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED) {
				RaidInfo->Members[UnitIndex].Flags |= NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC;
			}
			if (UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE) {
				RaidInfo->Members[UnitIndex].Flags |= NDAS_RAID_MEMBER_FLAG_REPLACED_BY_SPARE;
			}
			if (UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) {
				//
				// Defective flag is meaningful only when disk is not replaced or used by other RAID.
				//
				if (!(RaidInfo->Members[UnitIndex].Flags & 
					(NDAS_RAID_MEMBER_FLAG_OFFLINE|
					NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET|
					NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH)
					)) {
					if (UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK) {
						RaidInfo->Members[UnitIndex].Flags |= NDAS_RAID_MEMBER_FLAG_BAD_DISK;
					}
					if (UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR) {
						RaidInfo->Members[UnitIndex].Flags |= NDAS_RAID_MEMBER_FLAG_BAD_SECTOR;
					}
					FailReason |= NDAS_RAID_FAIL_REASON_DEFECTIVE;
				}
			}
		}
		
		if (DegradedMountCount == PrimaryDIB_V2.nDiskCount) {
			//Disks are used in degraded mode separately. Cannot resolve problem automatically.
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			FailReason |= NDAS_RAID_FAIL_REASON_IRRECONCILABLE;
			for(i = 0; i < PrimaryDIB_V2.nDiskCount; i++) {
				RaidInfo->Members[PrimaryRmd.UnitMetaData[i].iUnitDeviceIdx].Flags |= NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE;
			}			
			goto out;
		}
	}

	UsableDevCount = 0;
	MissingSpareCount = 0;
	AvailSpareCount = 0;
	ActiveDiskCount = 0;
	OutOfSyncCount = 0;
	
	for(i = 0; i < TotalDiskCount; i++) {
		if (NdasOpIsValidMember(RaidInfo->Members[i].Flags) && !NdasOpIsConflictMember(RaidInfo->Members[i].Flags)) {
			UsableDevCount++;
			if (RaidInfo->Members[i].Flags & NDAS_RAID_MEMBER_FLAG_ACTIVE) {
				ActiveDiskCount++;
				if (RaidInfo->Members[i].Flags & NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC) {
					OutOfSyncCount++;
				}
			}
			if (RaidInfo->Members[i].Flags & NDAS_RAID_MEMBER_FLAG_SPARE) {
				AvailSpareCount++;
			}
		} else {
			if (RaidInfo->Members[i].Flags & NDAS_RAID_MEMBER_FLAG_SPARE) {
				MissingSpareCount++;
			}
		}
	}
	
	if (NMT_RAID1R3 == PrimaryDIB_V2.iMediaType || 
		NMT_RAID4R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID5 == PrimaryDIB_V2.iMediaType) {
		if (AvailSpareCount) 
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_SPARE_EXIST;
		if (MissingSpareCount)
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_MISSING_SPARE;
		if (FailReason & NDAS_RAID_FAIL_REASON_SPARE_USED) {
			// Spare is used. User needs to resolve the problem.
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		}  else if (ActiveDiskCount == PrimaryDIB_V2.nDiskCount) {
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_MOUNTABLE | NDAS_RAID_MOUNTABILITY_NORMAL;
			if (OutOfSyncCount) {
				MountabilityFlags |= NDAS_RAID_MOUNTABILITY_OUT_OF_SYNC;
			}
		} else if (ActiveDiskCount == PrimaryDIB_V2.nDiskCount-1) {
			if (OutOfSyncCount == 0) {
				MountabilityFlags |= NDAS_RAID_MOUNTABILITY_MOUNTABLE | NDAS_RAID_MOUNTABILITY_DEGRADED;
			} else {
				// Member is missing and alive member is out-of-sync. We cannot use this RAID
				MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			}
		} else {
			MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;	
		}
	} else {
		if (FailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED) {
			// Unmountable flag is already on.
		} else {
			if (UsableDevCount == TotalDiskCount) {
				MountabilityFlags |= NDAS_RAID_MOUNTABILITY_MOUNTABLE | NDAS_RAID_MOUNTABILITY_NORMAL;
			} else {
				// Any missing member is failure.
				MountabilityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
				// FailReason must be already set by unit test code.
			}
		}
	}

	if ((NMT_RAID1R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID4R3 == PrimaryDIB_V2.iMediaType ||
		NMT_RAID5 == PrimaryDIB_V2.iMediaType
		)&&
		(MountabilityFlags & NDAS_RAID_MOUNTABILITY_OUT_OF_SYNC)) {
		DWORD TotalBits, SetBits;
		_NdasOpGetOutOfSyncStatus(ahNDAS[PrimaryDevNo], &PrimaryDIB_V2, &TotalBits, &SetBits);
		RaidInfo->TotalBitCount = TotalBits;
		RaidInfo->OosBitCount = SetBits; 
	}
	
out:
	if (ahNDAS) {
		for(i = 0; i < TotalDiskCount; i++) {
			if (ahNDAS[i] && ahNDAS[i] != hNdasDevice) {
				NdasCommDisconnect(ahNDAS[i]);
			} else {
				ahNDAS[i] = 0;
			}
		}
	}	
	// One of flag should be on and both should not have 
	_ASSERT((MountabilityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE) ||
		(MountabilityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE));
	// Both flags should not be on at the same time.
	_ASSERT((MountabilityFlags & (NDAS_RAID_MOUNTABILITY_UNMOUNTABLE| NDAS_RAID_MOUNTABILITY_MOUNTABLE))
		!= (NDAS_RAID_MOUNTABILITY_UNMOUNTABLE| NDAS_RAID_MOUNTABILITY_MOUNTABLE));

	// If unmountable, there must be reason.
	if (MountabilityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE) {
		_ASSERT(FailReason);
	}
	
	RaidInfo->MountablityFlags = MountabilityFlags;
	RaidInfo->FailReason =  FailReason;
			
	if (ahNDAS) {
		::HeapFree(::GetProcessHeap(), NULL, ahNDAS);
	}
	if (MemberDIB_V2) {
		::HeapFree(::GetProcessHeap(), NULL, MemberDIB_V2);
	}
	if (MemberRmd) {
		::HeapFree(::GetProcessHeap(), NULL, MemberRmd);
	}
	if (InternalErr) {
		return FALSE;
	} else {
		return TRUE;
	}
}

