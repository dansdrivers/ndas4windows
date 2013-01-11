// ndasop.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include <ndas/ndasctype.h>
#include <ndas/ndastype.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasop.h>
#include <scrc32.h>
#include <xdebug.h>

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


// AING : 2TB is limit of disk in windows at the moment.
// http://support.microsoft.com/default.aspx?scid=kb;en-us;325722
// ms-help://MS.MSDNQTR.2003OCT.1033/ntserv/html/S48B2.htm
#define	NDAS_SIZE_DISK_LIMIT ((UINT64)2 * 1024 * 1024 * 1024 * 1024 / SECTOR_SIZE)

static
BOOL
NdasOpClearMBR(HNDAS hNdasDevice);

static
BOOL
NdasOpClearXArea(HNDAS hNdasDevice);

static
DWORD
NdasOpVerifyDiskCount(UINT32 BindType, UINT32 nDiskCount);

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
UINT32
NDASOPAPI
NdasOpBind(
	UINT32	nDiskCount,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	UINT32	BindType
	)
{
	UINT32 iReturn = 0xFFFFFFFF;
	BOOL bResults;
	UINT32 i, j;
	HNDAS *ahNdasDevice;
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 *DIBs_V2 = NULL;
	UINT32 nDIBSize;

	BOOL bClearMBR;
	BOOL bMigrateMirrorV1;
	
	BYTE emptyData[128 * SECTOR_SIZE];
	NDASCOMM_UNIT_DEVICE_INFO UnitDeviceInfo;
	NDASCOMM_CONNECTION_INFO ConnectionInfoDiscover;
	NDASCOMM_UNIT_DEVICE_STAT UnitDeviceStat;
	UINT64 ui64TotalSector = 0;
	UINT32 MediaType;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * nDiskCount));

	bResults = NdasOpVerifyDiskCount(BindType, nDiskCount);
	TEST_AND_RETURN_IF_FAIL(bResults);

	ZeroMemory(&DIB_V1, sizeof(NDAS_DIB));
	if(NMT_SINGLE != BindType)
		NDAS_DIB_V1_INVALIDATE(DIB_V1);

	DIBs_V2 = (PNDAS_DIB_V2)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		nDiskCount * sizeof(NDAS_DIB));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, DIBs_V2);

	ahNdasDevice = (HNDAS *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		nDiskCount * sizeof(HNDAS));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, ahNdasDevice);

	bClearMBR = TRUE;
	bMigrateMirrorV1 = FALSE;

	// gather information & initialize DIBs
	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i; // record last accessing disk

		// other type is not supported yet
		// using mac address in pConnectionInfo to build NDAS_DIB_V2
		_ASSERT(NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX == pConnectionInfo->address_type);

		ahNdasDevice[i] = NdasCommConnect(&pConnectionInfo[i], 0, NULL);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			ahNdasDevice[i] || NDASCOMM_ERROR_RW_USER_EXIST != ::GetLastError());
		TEST_AND_GOTO_IF_FAIL(out, ahNdasDevice[i]);

		RtlCopyMemory(&ConnectionInfoDiscover, &pConnectionInfo[i],
			sizeof(ConnectionInfoDiscover));
		ConnectionInfoDiscover.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
		TEST_AND_GOTO_IF_FAIL(out, NdasCommGetUnitDeviceStat(&ConnectionInfoDiscover, &UnitDeviceStat, 0, NULL));
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			1 == UnitDeviceStat.NRRWHost && (0 == UnitDeviceStat.NRROHost || 0xFFFFFFFF == UnitDeviceStat.NRROHost));

		// fail if not a single disk
		nDIBSize = sizeof(DIBs_V2[i]);
		bResults = NdasOpReadDIB(ahNdasDevice[i], &DIBs_V2[i], &nDIBSize);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(NMT_SINGLE == BindType)
		{
//			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out,
//				NMT_SINGLE == DIBs_V2[i].iMediaType); // accepts all types
			if(NMT_MIRROR == DIBs_V2[i].iMediaType ||
				(NMT_RAID1 == DIBs_V2[i].iMediaType 
				&& 2 == DIBs_V2[i].nDiskCount))
				bClearMBR = FALSE;
		}
		else if(NMT_RAID1 == BindType)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_SINGLE_DISK, out,
				NMT_SINGLE == DIBs_V2[i].iMediaType ||
				NMT_MIRROR == DIBs_V2[i].iMediaType ); // accepts NMT_MIRROR to migrate
		}
		else
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_SINGLE_DISK, out,
				NMT_SINGLE == DIBs_V2[i].iMediaType); // bind only single disks
		}

		// determine migrate
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_BIND_MIGRATE,
			FALSE == bMigrateMirrorV1 || NMT_MIRROR == DIBs_V2[i].iMediaType);

		if(2 == nDiskCount && NMT_MIRROR == DIBs_V2[i].iMediaType)
		{
			bMigrateMirrorV1 = TRUE;
		}

		ZeroMemory(&DIBs_V2[i], sizeof(NDAS_DIB_V2));
		DIBs_V2[i].Signature = NDAS_DIB_V2_SIGNATURE;
		DIBs_V2[i].MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
		DIBs_V2[i].MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
		DIBs_V2[i].sizeXArea = NDAS_BLOCK_SIZE_XAREA; // 2MB
		DIBs_V2[i].iMediaType = (BindType == NMT_SAFE_RAID1) ? NMT_RAID1 : BindType;
		DIBs_V2[i].nDiskCount = nDiskCount;
		DIBs_V2[i].iSequence = i;

		bResults = NdasCommGetUnitDeviceInfo(ahNdasDevice[i], &UnitDeviceInfo);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

#ifdef __REDUCED_UNIT_SIZE__
		UnitDeviceInfo.SectorCount = __REDUCED_UNIT_SIZE__;
#endif // __REDUCED_UNIT_SIZE__

		// calculate user space, sectors per bit (bitmap)
		switch(BindType)
		{
		case NMT_AGGREGATE:
			// just % 128 of free space
			DIBs_V2[i].sizeUserSpace = UnitDeviceInfo.SectorCount 
				- NDAS_BLOCK_SIZE_XAREA;
			DIBs_V2[i].sizeUserSpace -= DIBs_V2[i].sizeUserSpace % 128;

			ui64TotalSector += DIBs_V2[i].sizeUserSpace;
			break;
		case NMT_RAID0:
			// % 128 of smallest free space
			if(0 == i) // initialize
				DIBs_V2[0].sizeUserSpace = UnitDeviceInfo.SectorCount 
				- NDAS_BLOCK_SIZE_XAREA;

			DIBs_V2[0].sizeUserSpace = min(UnitDeviceInfo.SectorCount 
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
		case NMT_RAID1:
		case NMT_SAFE_RAID1:
			// % 128 of smaller free space in pair
			if(i % 2) // finalize pair
			{
				if(bMigrateMirrorV1 || NMT_SAFE_RAID1 == BindType)
				{
					// migration or add mirror code
					// mirror disks use first disk as primary
					TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
						NDASOP_ERROR_INVALID_BIND_MIGRATE, out,
						UnitDeviceInfo.SectorCount - NDAS_BLOCK_SIZE_XAREA >=
						DIBs_V2[i -1].sizeUserSpace);
					DIBs_V2[i].sizeUserSpace = DIBs_V2[i -1].sizeUserSpace;
				}
				else
				{
					DIBs_V2[i].sizeUserSpace = min(UnitDeviceInfo.SectorCount
						- NDAS_BLOCK_SIZE_XAREA,
						DIBs_V2[i -1].sizeUserSpace);
				}

				// sectors per bit
				for(j = 7; TRUE; j++)
				{
					if(DIBs_V2[i].sizeUserSpace <= SECTOR_SIZE * 2048 * (8 << j))
					{
						DIBs_V2[i].iSectorsPerBit = 1 << j;
						break;
					}
					_ASSERT(j <= 12); // protect overflow
				}

				DIBs_V2[i].sizeUserSpace -= DIBs_V2[i].sizeUserSpace 
					% DIBs_V2[i].iSectorsPerBit;

				DIBs_V2[i -1].sizeUserSpace = DIBs_V2[i].sizeUserSpace;
				DIBs_V2[i -1].iSectorsPerBit = DIBs_V2[i].iSectorsPerBit;

				ui64TotalSector += DIBs_V2[i].sizeUserSpace;
			}
			else // initialize pair
			{
				DIBs_V2[i].sizeUserSpace = UnitDeviceInfo.SectorCount 
					- NDAS_BLOCK_SIZE_XAREA;
			}
			DIBs_V2[i].AutoRecover = TRUE;
			break;
		case NMT_RAID4:
			// % 128 of smallest free space
			if(0 == i) // initialize
				DIBs_V2[0].sizeUserSpace = UnitDeviceInfo.SectorCount 
				- NDAS_BLOCK_SIZE_XAREA;

			DIBs_V2[0].sizeUserSpace = min(UnitDeviceInfo.SectorCount 
				- NDAS_BLOCK_SIZE_XAREA,
				DIBs_V2[0].sizeUserSpace);

			if(nDiskCount -1 == i) // finalize
			{
				// sectors per bit
				for(j = 7; TRUE; j++)
				{
					if(DIBs_V2[0].sizeUserSpace <= SECTOR_SIZE * 2048 * (8 << j))
					{
						DIBs_V2[0].iSectorsPerBit = 1 << j;
						break;
					}
					_ASSERT(j <= 12); // protect overflow
				}

				DIBs_V2[0].sizeUserSpace -= DIBs_V2[0].sizeUserSpace 
					% DIBs_V2[0].iSectorsPerBit;

				for(j = 1; j < nDiskCount; j++)
				{
					DIBs_V2[j].sizeUserSpace = DIBs_V2[0].sizeUserSpace;
					DIBs_V2[j].iSectorsPerBit = DIBs_V2[0].iSectorsPerBit;
				}

				ui64TotalSector = DIBs_V2[0].sizeUserSpace * (nDiskCount -1);
			}
			DIBs_V2[i].AutoRecover = TRUE;
			break;
		case NMT_SINGLE:
			ZeroMemory(&DIBs_V2[i], sizeof(NDAS_DIB_V2));
			ui64TotalSector = UnitDeviceInfo.SectorCount;
			break;
		}

		if(NMT_SINGLE != BindType)
		{
			for(j = 0; j < nDiskCount; j++)
			{
				CopyMemory(DIBs_V2[i].UnitDisks[j].MACAddr, 
					pConnectionInfo[j].AddressLPX, 
					sizeof(DIBs_V2[i].UnitDisks[j].MACAddr));
				DIBs_V2[i].UnitDisks[j].UnitNumber = (BYTE)pConnectionInfo[j].UnitNo;
			}
		}

		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_SIZE_LIMIT, out, 
			ui64TotalSector <= NDAS_SIZE_DISK_LIMIT);
	}

	ZeroMemory(emptyData, SECTOR_SIZE * 128);
	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i;

		// clear MBR
		if(bClearMBR && !bMigrateMirrorV1 && NMT_SAFE_RAID1 != BindType)
		{
			bResults = NdasOpClearMBR(ahNdasDevice[i]);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}

		// clear X Area
		bResults = NdasOpClearXArea(ahNdasDevice[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// set CRC32
		if(NMT_SINGLE != BindType)
		{
			DIBs_V2[i].crc32 = crc32_calc((unsigned char *)&DIBs_V2[i],
				sizeof(DIBs_V2[i].bytes_248));
			DIBs_V2[i].crc32_unitdisks = crc32_calc((unsigned char *)DIBs_V2[i].UnitDisks,
				sizeof(DIBs_V2[i].UnitDisks));
		}

		// write DIB_V1, DIBs_V2
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNdasDevice[i], 
			NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&DIB_V1);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNdasDevice[i], 
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIBs_V2[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(nDiskCount > NDAS_MAX_UNITS_IN_V2)
		{
			_ASSERT(nDiskCount > NDAS_MAX_UNITS_IN_V2); // not coded
		}
	}

	if(NMT_SAFE_RAID1 == BindType) // invalidate whole bitmap
	{
		// Corruption Bitmap : 128 * 16 sectors  = 1MB
		FillMemory(emptyData, sizeof(emptyData), 0xff);

		for(i = 0; i < 16; i++)
		{
			bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNdasDevice[0], 
				NDAS_BLOCK_LOCATION_BITMAP + i * 128, 128, emptyData);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}
	}


	// success
	iReturn = nDiskCount;
out:
	DWORD dwLastErrorBackup = ::GetLastError();

	if(ahNdasDevice)
	{
		for(i = 0; i < nDiskCount; i++)
		{
			if(ahNdasDevice[i])
			{
				bResults = NdasCommDisconnect(ahNdasDevice[i]);
				_ASSERT(bResults);
				ahNdasDevice[i] = NULL;
			}
		}

		HEAP_SAFE_FREE(ahNdasDevice);
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

// supports only 32 disks only at this time.
/*
	Repair Process
	. check parameters
	. read DIB information of a disk
	. IsBindStatusOk()
	. accept only NMT_RAID1, NMT_RAID4
	. enumerate all disks (check failed disk count, validate DIB)
	.. leave all disks open(do not disconnect)
	.. check failed disk count : RAID1 repairs 1st broken pair and
		return 'NDASOP_INFORMATION_REPAIRED_PARTIALLY'
	.. validate DIB : every members of each DIBs
	. start repair (NdasOpRepairRAID1, NdasOpRepairRAID4)
*/
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRepair(
    CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
    CONST NDASCOMM_CONNECTION_INFO *pConnectionInfoReplace)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;
	BOOL bUserCancel = FALSE;

	HNDAS hNdasDevice = NULL;
	HNDAS *hNdasDevices = NULL;
	NDAS_DIB_V2	Ref_DIB_V2; // reference DIB
	NDAS_DIB_V2	*DIBs_V2 = NULL;
	UINT32	nDIB_V2_Size, i, j, k;
	NDASCOMM_CONNECTION_INFO ConnectionInfo;
	unsigned _int32 nDiskCount;
	unsigned _int32 iMediaType;
	PBYTE Bitmap = NULL;
	BOOL bFailed = FALSE;
	UINT32 iFailedDisk, iRecordDisk;
	int iMissingMember;
	BYTE emptyData [128 * SECTOR_SIZE];
	DWORD dwUnitNumber;

	unsigned _int32 iSectorsPerBit;
	LAST_WRITTEN_SECTORS LWSs;

	UINT32 nRecoverSectorInBitmap = 0xFFFFFFFF;
	PBYTE BufferRecover = NULL, BufferRecover2 = NULL;
	BYTE OneSector[SECTOR_SIZE];
	ULONG *pBufferRecover, *pBufferRecover2;
	UINT32 nBitsSetTotal = 0, nBitsRecovered = 0;

	// read reference device
	hNdasDevice = NdasCommConnect(pConnectionInfo, 0, NULL);
	TEST_AND_GOTO_IF_FAIL(out, hNdasDevice);

	nDIB_V2_Size = sizeof(Ref_DIB_V2);
	bResults = NdasOpReadDIB(hNdasDevice, &Ref_DIB_V2, &nDIB_V2_Size);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNdasDevice);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// brief validate Ref_DIB_V2
	iMediaType = Ref_DIB_V2.iMediaType;
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_FAIL, out, 
		NMT_RAID1 == iMediaType || NMT_RAID4 == iMediaType);

	nDiskCount = Ref_DIB_V2.nDiskCount;
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, 
		2 <= nDiskCount);

	iSectorsPerBit = Ref_DIB_V2.iSectorsPerBit;
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, 
		128 <= iSectorsPerBit);

	// allocate
	hNdasDevices = (HNDAS *)::HeapAlloc(::GetProcessHeap(),
		HEAP_ZERO_MEMORY, nDiskCount * sizeof(HNDAS));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
		NDASOP_ERROR_NOT_BOUND_DISK, out, hNdasDevices);

	DIBs_V2 = (PNDAS_DIB_V2)::HeapAlloc(::GetProcessHeap(),
		HEAP_ZERO_MEMORY, nDiskCount * sizeof(NDAS_DIB_V2));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
		NDASOP_ERROR_NOT_BOUND_DISK, out, DIBs_V2);

	Bitmap = (PBYTE)::HeapAlloc(::GetProcessHeap(),
		HEAP_ZERO_MEMORY, NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
		NDASOP_ERROR_NOT_BOUND_DISK, out, Bitmap);

	iMissingMember = -1;
	iRecordDisk = -1;

	// read all DIBs
	for(i = 0; i < nDiskCount; i++)
	{
		ZeroMemory(&ConnectionInfo, sizeof(ConnectionInfo));
		ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		ConnectionInfo.UnitNo = Ref_DIB_V2.UnitDisks[i].UnitNumber;
		ConnectionInfo.bWriteAccess = TRUE;
		ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
		CopyMemory(ConnectionInfo.AddressLPX,
			Ref_DIB_V2.UnitDisks[i].MACAddr, sizeof(ConnectionInfo.AddressLPX));

		hNdasDevices[i] = NdasCommConnect(&ConnectionInfo, 0, NULL);
		if(!hNdasDevices[i])
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_TOO_MANY_MISSING_MEMBER,
				out, -1 == iMissingMember);
			iMissingMember = i;

			continue;
		}
			
		TEST_AND_GOTO_IF_FAIL(out, hNdasDevices[i]);

		nDIB_V2_Size = sizeof(Ref_DIB_V2);
		bResults = NdasOpReadDIB(hNdasDevices[i], &DIBs_V2[i], &nDIB_V2_Size);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// verify DIBs
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out,
			DIBs_V2[i].iMediaType == iMediaType &&
			DIBs_V2[i].nDiskCount == nDiskCount &&
			DIBs_V2[i].iSequence == i &&
			DIBs_V2[i].sizeUserSpace == Ref_DIB_V2.sizeUserSpace &&
			DIBs_V2[i].iSectorsPerBit == Ref_DIB_V2.iSectorsPerBit &&
			!memcmp(DIBs_V2[i].UnitDisks, Ref_DIB_V2.UnitDisks, sizeof(UNIT_DISK_LOCATION) * nDiskCount));

		bResults = NdasCommBlockDeviceRead(hNdasDevices[i], NDAS_BLOCK_LOCATION_BITMAP,
			NDAS_BLOCK_SIZE_BITMAP, Bitmap);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// is bitmap clean?
		for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE; j++)
		{
			if(Bitmap[j])
				break;
		}

		if(NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE != j) // not clean
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, !bFailed);

			bFailed = TRUE;
			iRecordDisk = i;
		}        
	}

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NO_MISSING_MEMBER,
		out, -1 != iMissingMember);

	if(bFailed)
	{
		if(NMT_RAID1 == iMediaType)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_CRITICAL_REPAIR,
				out, iMissingMember == (iRecordDisk ^1));
		}
		else if(NMT_RAID4 == iMediaType)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_CRITICAL_REPAIR,
				out, iMissingMember == (iRecordDisk == 0) ? nDiskCount -1 : iRecordDisk -1);
		}
	}
	else
	{
		// set iRecordDisk
		if(NMT_RAID1 == iMediaType)
		{
			iRecordDisk = iMissingMember ^ 1;
		}
		else if(NMT_RAID4 == iMediaType)
		{
			iRecordDisk = (iMissingMember < (int)nDiskCount -1) ?  iMissingMember + 1 : 0;
		}
	}

	iFailedDisk = iMissingMember;

	// connect to replace disk
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		out, pConnectionInfoReplace->bWriteAccess);

	hNdasDevices[iFailedDisk] = NdasCommConnect(pConnectionInfoReplace, 0, NULL);
	TEST_AND_GOTO_IF_FAIL(out, hNdasDevices[iFailedDisk]);

	// read & check size replacement
	NDASCOMM_UNIT_DEVICE_INFO UnitInfo;
	bResults = NdasCommGetUnitDeviceInfo(hNdasDevices[iFailedDisk], &UnitInfo);
	TEST_AND_GOTO_IF_FAIL(out, bResults);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_REPLACE_TOO_SMALL, out,
		UnitInfo.SectorCount > DIBs_V2[iRecordDisk].sizeUserSpace + NDAS_BLOCK_SIZE_XAREA);

	bResults = NdasOpClearXArea(hNdasDevices[iFailedDisk]);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// create new DIB for replacement
	CopyMemory(&DIBs_V2[iFailedDisk], &Ref_DIB_V2, nDIB_V2_Size);
	DIBs_V2[iFailedDisk].iSequence = iFailedDisk;
	DIBs_V2[iFailedDisk].crc32 = crc32_calc((unsigned char *)&DIBs_V2[iFailedDisk],
		sizeof(DIBs_V2[iFailedDisk].bytes_248));

	
	// replace device id in all DIBs
	for(i = 0; i < nDiskCount; i++)
	{		
		bResults = NdasCommGetDeviceID(hNdasDevices[iFailedDisk], 
			DIBs_V2[i].UnitDisks[iFailedDisk].MACAddr,
			&dwUnitNumber);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		DIBs_V2[i].UnitDisks[iFailedDisk].UnitNumber = (unsigned _int8)dwUnitNumber;
		
		DIBs_V2[i].crc32_unitdisks = crc32_calc((unsigned char *)DIBs_V2[i].UnitDisks,
			sizeof(DIBs_V2[i].UnitDisks));

		// write DIB on replacement
		bResults = NdasCommBlockDeviceWrite(hNdasDevices[i], 
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIBs_V2[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}

	// set all bitmap bits of iRecordDisk
	// Corruption Bitmap : 128 * 16 sectors  = 1MB
	for(i = 0; i < 16; i++)
	{
		FillMemory(emptyData, sizeof(emptyData), 0xFF);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevices[iRecordDisk],
			NDAS_BLOCK_LOCATION_BITMAP + i * 128, 128, emptyData);
		TEST_AND_RETURN_IF_FAIL(bResults);
	}

	// Copy Last written sector
	bResults = NdasCommBlockDeviceRead(hNdasDevices[iRecordDisk],
		NDAS_BLOCK_LOCATION_WRITE_LOG, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevices[iFailedDisk],
		NDAS_BLOCK_LOCATION_WRITE_LOG, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	bReturn = TRUE;
out:
	DWORD dwLastErrorBackup = ::GetLastError();

	if(hNdasDevices)
	{
		for(i = 0; i < nDiskCount; i++)
		{
			if(hNdasDevices[i])
			{
				bResults =NdasCommDisconnect(hNdasDevices[i]);
				_ASSERT(bResults);
			}
			hNdasDevices[i] = NULL;
		}
		HEAP_SAFE_FREE(hNdasDevices);
	}

	HEAP_SAFE_FREE(Bitmap);
	HEAP_SAFE_FREE(DIBs_V2);

	::SetLastError(dwLastErrorBackup);
	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRecover(
    CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
    LPNDAS_OP_RECOVER_CALLBACK callback_func,
    LPVOID lpParameter)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;
	BOOL bUserCancel = FALSE;

	HNDAS hNdasDevice = NULL;
	HNDAS *hNdasDevices = NULL;
	NDAS_DIB_V2	Ref_DIB_V2; // reference DIB
	NDAS_DIB_V2	*DIBs_V2 = NULL;
	UINT32	nDIB_V2_Size, i, j, k;
	NDASCOMM_CONNECTION_INFO ConnectionInfo;
	unsigned _int32 nDiskCount;
	unsigned _int32 iMediaType;
	PBYTE Bitmap = NULL;
	BOOL bFailed = FALSE;
	UINT32 iFailedDisk, iRecordDisk;

	unsigned _int32 iSectorsPerBit, iSectorsToRecover;
	LAST_WRITTEN_SECTORS LWSs;

	UINT32 nRecoverSectorInBitmap = 0xFFFFFFFF;
	PBYTE BufferRecover = NULL, BufferRecover2 = NULL;
	BYTE OneSector[SECTOR_SIZE];
	ULONG *pBufferRecover, *pBufferRecover2;
	UINT32 nBitsSetTotal = 0, nBitsRecovered = 0;

	// read reference device
	hNdasDevice = NdasCommConnect(pConnectionInfo, 0, NULL);
	TEST_AND_GOTO_IF_FAIL(out, hNdasDevice);

	nDIB_V2_Size = sizeof(Ref_DIB_V2);
	bResults = NdasOpReadDIB(hNdasDevice, &Ref_DIB_V2, &nDIB_V2_Size);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNdasDevice);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// brief validate Ref_DIB_V2
	iMediaType = Ref_DIB_V2.iMediaType;
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_FAIL, out, 
		NMT_RAID1 == iMediaType || NMT_RAID4 == iMediaType);

	nDiskCount = Ref_DIB_V2.nDiskCount;
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, 
		2 <= nDiskCount);

	iSectorsPerBit = Ref_DIB_V2.iSectorsPerBit;
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, 
		128 <= iSectorsPerBit);

	// allocate
	hNdasDevices = (HNDAS *)::HeapAlloc(::GetProcessHeap(),
		HEAP_ZERO_MEMORY, nDiskCount * sizeof(HNDAS));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
		NDASOP_ERROR_NOT_BOUND_DISK, out, hNdasDevices);

	DIBs_V2 = (PNDAS_DIB_V2)::HeapAlloc(::GetProcessHeap(),
		HEAP_ZERO_MEMORY, nDiskCount * sizeof(NDAS_DIB_V2));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
		NDASOP_ERROR_NOT_BOUND_DISK, out, DIBs_V2);

	Bitmap = (PBYTE)::HeapAlloc(::GetProcessHeap(),
		HEAP_ZERO_MEMORY, NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
		NDASOP_ERROR_NOT_BOUND_DISK, out, Bitmap);

	// read all DIBs
	for(i = 0; i < nDiskCount; i++)
	{
		ZeroMemory(&ConnectionInfo, sizeof(ConnectionInfo));
		ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		ConnectionInfo.UnitNo = Ref_DIB_V2.UnitDisks[i].UnitNumber;
		ConnectionInfo.bWriteAccess = TRUE;
		ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
		CopyMemory(ConnectionInfo.AddressLPX,
			Ref_DIB_V2.UnitDisks[i].MACAddr, sizeof(ConnectionInfo.AddressLPX));

		hNdasDevices[i] = NdasCommConnect(&ConnectionInfo, 0, NULL);
		TEST_AND_GOTO_IF_FAIL(out, hNdasDevices[i]);

		nDIB_V2_Size = sizeof(Ref_DIB_V2);
		bResults = NdasOpReadDIB(hNdasDevices[i], &DIBs_V2[i], &nDIB_V2_Size);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// verify DIBs
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out,
			DIBs_V2[i].iMediaType == iMediaType &&
			DIBs_V2[i].nDiskCount == nDiskCount &&
			DIBs_V2[i].iSequence == i &&
			DIBs_V2[i].sizeUserSpace == Ref_DIB_V2.sizeUserSpace &&
			DIBs_V2[i].iSectorsPerBit == Ref_DIB_V2.iSectorsPerBit &&
			!memcmp(DIBs_V2[i].UnitDisks, Ref_DIB_V2.UnitDisks, sizeof(UNIT_DISK_LOCATION) * nDiskCount));

		bResults = NdasCommBlockDeviceRead(hNdasDevices[i], NDAS_BLOCK_LOCATION_BITMAP,
			NDAS_BLOCK_SIZE_BITMAP, Bitmap);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// is bitmap clean?
		for(j = 0; j < NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE; j++)
		{
			if(Bitmap[j])
				break;
		}

		if(NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE != j) // not clean
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, !bFailed);

			bFailed = TRUE;
			iRecordDisk = i;
		}        
	}


	if(NMT_RAID1 == iMediaType)
		iFailedDisk = iRecordDisk ^1;
	else if(NMT_RAID4 == iMediaType)
		iFailedDisk = (iRecordDisk == 0) ? nDiskCount -1 : iRecordDisk -1;

	// read bitmap
	bResults = NdasCommBlockDeviceRead(hNdasDevices[iRecordDisk], NDAS_BLOCK_LOCATION_BITMAP,
		NDAS_BLOCK_SIZE_BITMAP, Bitmap);

	// merge all the LWSs to bitmap
	_ASSERT(sizeof(LWSs) / sizeof(LWSs.LWS[0]) == 32);

	for(i = 0; i < nDiskCount; i++)
	{
		bResults = NdasCommBlockDeviceRead(hNdasDevices[i], NDAS_BLOCK_LOCATION_WRITE_LOG, 1, (PBYTE)&LWSs);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		for(j = 0; i < sizeof(LWSs) / sizeof(LWSs.LWS[0]); i++)
		{
			NDAS_BITMAP_SET(Bitmap, LWSs.LWS[j].logicalBlockAddress / iSectorsPerBit);
			NDAS_BITMAP_SET(Bitmap, LWSs.LWS[j].logicalBlockAddress / iSectorsPerBit +1);
		}
	}


	// start recover

	for(i = 0; i < Ref_DIB_V2.sizeUserSpace / Ref_DIB_V2.iSectorsPerBit; i++) // check whole bits in bitmap
	{
		if(NDAS_BITMAP_ISSET(Bitmap, i))
			nBitsSetTotal++;
	}

	BufferRecover = (PBYTE)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, iSectorsPerBit * SECTOR_SIZE);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, BufferRecover);
	
	BufferRecover2 = (PBYTE)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, iSectorsPerBit * SECTOR_SIZE);
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_BOUND_DISK, out, BufferRecover2);

	i = 0;
	NDAS_BITMAP_FIND_SET_BIT(Bitmap, i, NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE * 8);

	if(callback_func)
	{
		bResults = callback_func(NDAS_RECOVER_STATUS_INITIALIZE, nBitsSetTotal, SECTOR_SIZE * iSectorsPerBit, lpParameter);
		if(!bResults)
		{
			bUserCancel = TRUE;
			goto out;
		}
	}

	while(i != 0xFFFFFFFF)
	{
		_ASSERT(NDAS_BITMAP_ISSET(Bitmap, i));

		if(i * iSectorsPerBit < Ref_DIB_V2.sizeUserSpace) // recover only where IS in user space
		{
			if((i +1) * iSectorsPerBit < (UINT32)Ref_DIB_V2.sizeUserSpace)
				iSectorsToRecover = iSectorsPerBit;
			else 
				iSectorsToRecover = iSectorsPerBit - ((i +1) * iSectorsPerBit - (UINT32)Ref_DIB_V2.sizeUserSpace);

			// generate buffer from healthy device(s)
			if(NMT_RAID1 == iMediaType)
			{
				bResults = NdasCommBlockDeviceRead(hNdasDevices[iRecordDisk], i * iSectorsPerBit, iSectorsToRecover, BufferRecover);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}
			else if(NMT_RAID4 == iMediaType)
			{
				ZeroMemory(BufferRecover, iSectorsPerBit * SECTOR_SIZE);
				
				for(j = 0; j < nDiskCount; j++)
				{
					if(j == iFailedDisk)
						continue;

					bResults = NdasCommBlockDeviceRead(hNdasDevices[j], i * iSectorsPerBit, iSectorsToRecover, BufferRecover2);
					TEST_AND_GOTO_IF_FAIL(out, bResults);
					pBufferRecover = (ULONG *)BufferRecover;
					pBufferRecover2 = (ULONG *)BufferRecover2;

					k = (iSectorsToRecover * SECTOR_SIZE) / sizeof(ULONG);
					while(k--)
					{
						*pBufferRecover ^= *pBufferRecover2;
						pBufferRecover++;
						pBufferRecover2++;
					}
				}
			}

			// write buffer to defected device
			bResults = NdasCommBlockDeviceWrite(hNdasDevices[iFailedDisk], i * iSectorsPerBit, iSectorsToRecover, BufferRecover);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}
		else
		{
			// just clear the bit
		}

		NDAS_BITMAP_RESET(Bitmap, i);

		nRecoverSectorInBitmap = NDAS_BITMAP_IDX_TO_SECTOR(i);
		NDAS_BITMAP_FIND_SET_BIT(Bitmap, i, NDAS_BLOCK_SIZE_BITMAP * SECTOR_SIZE * 8);

		if(nRecoverSectorInBitmap != NDAS_BITMAP_IDX_TO_SECTOR(i))
		{
			ZeroMemory(OneSector, sizeof(OneSector));
			bResults = NdasCommBlockDeviceWrite(hNdasDevices[iRecordDisk], NDAS_BLOCK_LOCATION_BITMAP + nRecoverSectorInBitmap, 1, OneSector);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}

		nBitsRecovered++;

		if(callback_func)
		{
			bResults = callback_func(NDAS_RECOVER_STATUS_RUNNING, nBitsSetTotal, min(nBitsRecovered, nBitsSetTotal), lpParameter);
			if(!bResults)
			{
				bUserCancel = TRUE;
				goto out;
			}
		}
	}

	// clear LWSs
	bResults = NdasCommBlockDeviceRead(hNdasDevices[iRecordDisk], NDAS_BLOCK_LOCATION_WRITE_LOG, 1, (PBYTE)&LWSs);

	for(i = 0; i < nDiskCount; i++)
		bResults = NdasCommBlockDeviceWrite(hNdasDevices[i], NDAS_BLOCK_LOCATION_WRITE_LOG, 1, (PBYTE)&LWSs);

	if(callback_func)
		bResults = callback_func(NDAS_RECOVER_STATUS_COMPLETE, nBitsSetTotal, nBitsSetTotal, lpParameter);

	bReturn = TRUE;
out:
	DWORD dwLastErrorBackup = ::GetLastError();

	if(bUserCancel)
	{
		if(callback_func)
			bResults = callback_func(NDAS_RECOVER_STATUS_FAILED, nBitsSetTotal, 0, lpParameter);
		bReturn = TRUE;
	}

	if(hNdasDevices)
	{
		for(i = 0; i < nDiskCount; i++)
		{
			if(hNdasDevices[i])
			{
				bResults =NdasCommDisconnect(hNdasDevices[i]);
				_ASSERT(bResults);
			}
			hNdasDevices[i] = NULL;
		}
		HEAP_SAFE_FREE(hNdasDevices);
	}

	HEAP_SAFE_FREE(Bitmap);
	HEAP_SAFE_FREE(BufferRecover);
	HEAP_SAFE_FREE(BufferRecover2);
	HEAP_SAFE_FREE(DIBs_V2);

	::SetLastError(dwLastErrorBackup);
	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSynchronize(
    CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
    LPNDAS_OP_RECOVER_CALLBACK callback_func,
    LPVOID lpParameter)
{
	return TRUE;
}

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
    HNDAS hNdasDevice,
    NDAS_DIB_V2 *pDIB_V2,
    UINT32 *pnDIBSize)
{
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 DIB_V2;
	BOOL bResults;
	UINT32 nDIBSize;
	UINT32 i;
	DWORD dwUnitNumber;

	NDASCOMM_UNIT_DEVICE_INFO UnitInfo;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pnDIBSize, sizeof(UINT32)));

	// Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2
	bResults = NdasCommBlockDeviceRead(hNdasDevice, 
		NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct


	if(NDAS_DIB_V2_SIGNATURE != DIB_V2.Signature)
		goto process_v1;

	if(DIB_V2.crc32 != crc32_calc((unsigned char *)&DIB_V2, sizeof(DIB_V2.bytes_248)))
		goto process_v1;

	if(DIB_V2.crc32_unitdisks != crc32_calc((unsigned char *)DIB_V2.UnitDisks,	sizeof(DIB_V2.UnitDisks)))
		goto process_v1;

	if(NDAS_BLOCK_SIZE_XAREA != DIB_V2.sizeXArea &&
		NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE != DIB_V2.sizeXArea)
		goto process_v1;

	NdasCommGetUnitDeviceInfo(hNdasDevice, &UnitInfo);
	if(DIB_V2.sizeUserSpace + DIB_V2.sizeXArea > UnitInfo.SectorCount)
		goto process_v1;

	// check DIB_V2.nDiskCount
	switch(DIB_V2.iMediaType)
	{
	case NMT_SINGLE:
		if(DIB_V2.nDiskCount)
			goto process_v1;
		break;
	case NMT_AGGREGATE:
		if(DIB_V2.nDiskCount < 2)
			goto process_v1;
		break;
	case NMT_RAID0:
		if(DIB_V2.nDiskCount != 2 &&
			DIB_V2.nDiskCount != 4 &&
			DIB_V2.nDiskCount != 8)
			goto process_v1;
		break;
	case NMT_RAID1:
		if(DIB_V2.nDiskCount % 2)
			goto process_v1;
		break;
	case NMT_RAID4:
		if(DIB_V2.nDiskCount != 3 &&
			DIB_V2.nDiskCount != 5 &&
			DIB_V2.nDiskCount != 9)
			goto process_v1;
		break;
	default:
		goto process_v1;
		break;
	}

	if(DIB_V2.iSequence >= DIB_V2.nDiskCount)
		goto process_v1;

	if(DIB_V2.nDiskCount > 32 + 64 + 64) // AING : PROTECTION
		goto process_v1;

	// check done, copy DIB_V2 information from NDAS Device to pDIB_V2
	{
		// code does not support if version in DIB_V2 is greater than the version defined
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_DEVICE_UNSUPPORTED, 
			NDAS_DIB_VERSION_MAJOR_V2 > DIB_V2.MajorVersion ||
			(NDAS_DIB_VERSION_MAJOR_V2 == DIB_V2.MajorVersion && 
			NDAS_DIB_VERSION_MINOR_V2 >= DIB_V2.MinorVersion));

		nDIBSize = (GET_TRAIL_SECTOR_COUNT_V2(DIB_V2.nDiskCount) + 1) 
			* sizeof(NDAS_DIB_V2);

		if(NULL != pnDIBSize)
		{
			if(*pnDIBSize >= nDIBSize) // make sure there is enough space
			{
				TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
					!::IsBadWritePtr(pDIB_V2, sizeof(NDAS_DIB_V2)));
				CopyMemory(pDIB_V2, &DIB_V2, sizeof(NDAS_DIB_V2));

				// Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND incase of more than 32 NDAS Unit devices exist 4. Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 if  NDAS_DIB_V2 information is not acceptable
				if(nDIBSize > sizeof(NDAS_DIB_V2))
				{
					bResults = NdasCommBlockDeviceRead(
						hNdasDevice, 
						NDAS_BLOCK_LOCATION_ADD_BIND, 
						GET_TRAIL_SECTOR_COUNT_V2(DIB_V2.nDiskCount),
						(PBYTE)(pDIB_V2 +1));
					TEST_AND_RETURN_IF_FAIL(bResults);
				}

				if(NMT_SINGLE != DIB_V2.iMediaType)
				{
					unsigned _int8 MACAddr[6];
					unsigned _int8 UnitNumber;

					if(DIB_V2.nDiskCount <= DIB_V2.iSequence)
						goto process_v1;

					bResults = NdasCommGetDeviceID(hNdasDevice, MACAddr, &dwUnitNumber);
					UnitNumber = (unsigned _int8)dwUnitNumber;

					if(memcmp(DIB_V2.UnitDisks[DIB_V2.iSequence].MACAddr,
						MACAddr, sizeof(MACAddr)) ||
						DIB_V2.UnitDisks[DIB_V2.iSequence].UnitNumber != UnitNumber)
						goto process_v1;
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

	bResults = NdasCommBlockDeviceRead(hNdasDevice, 
		NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&DIB_V1);
	TEST_AND_RETURN_IF_FAIL(bResults);


	NDASCOMM_UNIT_DEVICE_INFO UnitDeviceInfo;
	bResults = NdasCommGetUnitDeviceInfo(hNdasDevice, &UnitDeviceInfo);
	TEST_AND_RETURN_IF_FAIL(bResults);

	pDIB_V2->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIB_V2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIB_V2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIB_V2->sizeXArea = NDAS_BLOCK_SIZE_XAREA;
	pDIB_V2->iSectorsPerBit = 0; // no backup information
	pDIB_V2->sizeUserSpace = UnitDeviceInfo.SectorCount - NDAS_BLOCK_SIZE_XAREA; // in case of mirror, use primary disk size

	// Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1 is not acceptable either

	if(IS_NDAS_DIBV1_WRONG_VERSION(DIB_V1) || // no DIB information
		(NDAS_DIB_DISK_TYPE_MIRROR_MASTER != DIB_V1.DiskType &&
		NDAS_DIB_DISK_TYPE_MIRROR_SLAVE != DIB_V1.DiskType &&
		NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST != DIB_V1.DiskType &&
		NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND != DIB_V1.DiskType))
	{
		pDIB_V2->iMediaType = NMT_SINGLE;
		pDIB_V2->iSequence = 0;
		pDIB_V2->nDiskCount = 1;

		// only 1 unit		
		bResults = NdasCommGetDeviceID(hNdasDevice, 
			(BYTE *)&pDIB_V2->UnitDisks[0].MACAddr,
			&dwUnitNumber);
		pDIB_V2->UnitDisks[0].UnitNumber = (unsigned _int8)dwUnitNumber;
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
			bResults = NdasCommGetDeviceID(hNdasDevice, 
				pUnitDiskLocation0->MACAddr, &dwUnitNumber);
			pUnitDiskLocation0->UnitNumber = (unsigned _int8)dwUnitNumber;
			TEST_AND_RETURN_IF_FAIL(bResults);
		}
		else
		{
			CopyMemory(&pUnitDiskLocation0->MACAddr, DIB_V1.EtherAddress, 
				sizeof(pUnitDiskLocation0->MACAddr));
			pUnitDiskLocation0->UnitNumber = DIB_V1.UnitNumber;
		}

		// 2nd unit
		CopyMemory(pUnitDiskLocation1->MACAddr, DIB_V1.PeerAddress, 
			sizeof(pUnitDiskLocation1->MACAddr));
		pUnitDiskLocation1->UnitNumber = DIB_V1.UnitNumber;

		pDIB_V2->nDiskCount = 2;

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

	pDIB_V2->crc32 = crc32_calc((unsigned char *)pDIB_V2,
		sizeof(pDIB_V2->bytes_248));
	pDIB_V2->crc32_unitdisks = crc32_calc((unsigned char *)pDIB_V2->UnitDisks,
		sizeof(pDIB_V2->UnitDisks));

	return TRUE;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpIsBitmapClean(
    HNDAS hNdasDevice,
    BOOL *pbIsClean)
{
	BOOL bResults;

	BYTE BitmapData[128 * 512];

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pbIsClean, sizeof(BOOL)));

	// 1MB from NDAS_BLOCK_LOCATION_BITMAP
	int i, j;
	PULONG pBitmapData;
	for(i = 0; i < 16; i++) 
	{
		bResults = NdasCommBlockDeviceRead(hNdasDevice,
			NDAS_BLOCK_LOCATION_BITMAP + (i * 128), 128, BitmapData);
		TEST_AND_RETURN_IF_FAIL(bResults);

		for(j = 0, pBitmapData = (PULONG)BitmapData; j < 128 * 512 / 4; j++)
		{
			if(*pBitmapData)
			{
				*pbIsClean = FALSE;
				return TRUE;
			}
			pBitmapData++;
		}
	}	

	*pbIsClean = TRUE;
	return TRUE;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpGetLastWrttenSectorInfo(
	HNDAS hNdasDevice,
	PLAST_WRITTEN_SECTOR pLWS)
{
	BOOL bResults;

	BYTE sector[512];

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pLWS, sizeof(LAST_WRITTEN_SECTOR)));

	bResults = NdasCommBlockDeviceRead(hNdasDevice,
		NDAS_BLOCK_LOCATION_WRITE_LOG, 1, sector);
	TEST_AND_RETURN_IF_FAIL(bResults);

	CopyMemory(pLWS, sector, sizeof(LAST_WRITTEN_SECTOR));

	return TRUE;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpGetLastWrttenSectorsInfo(
	HNDAS hNdasDevice,
	PLAST_WRITTEN_SECTORS pLWS)
{
	BOOL bResults;

	BYTE sector[512];

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pLWS, sizeof(LAST_WRITTEN_SECTORS)));

	bResults = NdasCommBlockDeviceRead(hNdasDevice,
		NDAS_BLOCK_LOCATION_WRITE_LOG, 1, sector);
	TEST_AND_RETURN_IF_FAIL(bResults);

	CopyMemory(pLWS, sector, sizeof(LAST_WRITTEN_SECTORS));

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Functions, not for exporting
//
////////////////////////////////////////////////////////////////////////////////////////////////

DWORD
NdasOpVerifyDiskCount(
    UINT32 BindType,
    UINT32 nDiskCount)
{
	// check disk count
	switch(BindType)
	{
	case NMT_AGGREGATE:
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
	case NMT_RAID4:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			3 == nDiskCount || 5 == nDiskCount || 9 == nDiskCount);
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
NdasOpClearMBR(
    HNDAS hNdasDevice)
{
	BOOL bResults;
	BYTE emptyData [(NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR)
		* SECTOR_SIZE];
	ZeroMemory(emptyData, sizeof(emptyData));

	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_MBR, 
		(NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR), emptyData);

	return bResults;
}

BOOL
NdasOpClearXArea(
    HNDAS hNdasDevice)
{
	BOOL bResults;
	int i;
	BYTE emptyData[128 * SECTOR_SIZE];

	ZeroMemory(emptyData, sizeof(emptyData));

	// NDAS_DIB_V1
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_DIB_V1, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// NDAS_DIB_V2
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_DIB_V2, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Content encryption information
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_ENCRYPT, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Additional bind informations : 256 sectors
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_ADD_BIND, 128, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_ADD_BIND + 128, 128, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Corruption Bitmap : 128 * 16 sectors  = 1MB
	for(i = 0; i < 16; i++)
	{
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice, 
			NDAS_BLOCK_LOCATION_BITMAP + i * 128, 128, emptyData);
		TEST_AND_RETURN_IF_FAIL(bResults);
	}

	// Last written sector
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_WRITE_LOG, 1, emptyData);
	TEST_AND_RETURN_IF_FAIL(bResults);

	return TRUE;
}
