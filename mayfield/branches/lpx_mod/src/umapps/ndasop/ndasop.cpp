// ndasop.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "ndasmsg.h"

#include "xdebug.h"

#include "ndasctype.h"
#include "ndastype.h"

#include "hash.h"
#include "socketlpx.h"
#include "ndasop.h"
#include "autores.h"
#include "ndascomm_api.h"
#include "scrc32.h"

struct AutoHNDASConfig
{
	static HNDAS GetInvalidValue() { return (HNDAS) NULL; }
	static void Release(HNDAS h)
	{ 
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::NdasCommDisconnect(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HNDAS,AutoHNDASConfig> AutoHNDAS;

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

BOOL
NdasOpReadDIB(
				 HNDAS hNdasDevice,
				 NDAS_DIB_V2 *pDIB_V2,
				 UINT32 *pnDIBSize);

BOOL
NdasOpClearMBR(
			   HNDAS hNdasDevice
			   );

BOOL
NdasOpClearXArea(
			   HNDAS hNdasDevice
			   );

DWORD
NdasOpVerifyDiskCount(
					  UINT32 BindType,
					  UINT32 nDiskCount);

////////////////////////////////////////////////////////////////////////////////////////////////
//
// NDAS Op API Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////

NDASOP_API
DWORD
NdasOpGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDASOP_API_VERSION_MAJOR, 
		NDASOP_API_VERSION_MINOR);
}

NDASOP_API
UINT32
NdasOpBind(
	UINT32	nDiskCount,
	NDAS_CONNECTION_INFO *pConnectionInfo,
	UINT32	BindType
	)
{
	UINT32 iReturn;
	BOOL bResults;
	UINT32 i, j;
	HNDAS *ahNdasDevice;
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 *DIBs_V2 = NULL;
	UINT32 nDIBSize;

	BOOL bClearMBR;
	BOOL bMigrateMirrorV1;
	
	CHAR emptyData[128 * SECTOR_SIZE];
	NDAS_UNIT_DEVICE_INFO UnitDeviceInfo;
	NDAS_UNIT_DEVICE_DYN_INFO UnitDynInfo;
	UINT64 ui64TotalSector = 0;
	UINT32 MediaType;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pConnectionInfo, sizeof(NDAS_CONNECTION_INFO) * nDiskCount));

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
		_ASSERT(NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS == pConnectionInfo->type);

		ahNdasDevice[i] = NdasCommConnect(&pConnectionInfo[i]);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			ahNdasDevice[i] || NDASCOMM_ERROR_RW_USER_EXIST != ::GetLastError());
		TEST_AND_GOTO_IF_FAIL(out, ahNdasDevice[i]);

		NdasCommGetUnitDeviceDynInfo(&pConnectionInfo[i], &UnitDynInfo);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			1 == UnitDynInfo.NRRWHost && 0 == UnitDynInfo.NRROHost);

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
		DIBs_V2[i].sizeXArea = NDAS_SIZE_XAREA; // 2M
		DIBs_V2[i].iMediaType = BindType;
		DIBs_V2[i].nDiskCount = nDiskCount;
		DIBs_V2[i].iSequence = i;

		bResults = NdasCommGetUnitDeviceInfo(ahNdasDevice[i], &UnitDeviceInfo);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// calculate user space, sectors per bit (bitmap)
		switch(BindType)
		{
		case NMT_AGGREGATE:
			// just % 128 of free space
			DIBs_V2[i].sizeUserSpace = UnitDeviceInfo.SectorCount 
				- NDAS_SIZE_XAREA;
			DIBs_V2[i].sizeUserSpace -= DIBs_V2[i].sizeUserSpace % 128;

			ui64TotalSector += DIBs_V2[i].sizeUserSpace;
			break;
		case NMT_RAID0:
			// % 128 of smallest free space
			if(0 == i) // initialize
				DIBs_V2[0].sizeUserSpace = UnitDeviceInfo.SectorCount 
				- NDAS_SIZE_XAREA;

			DIBs_V2[0].sizeUserSpace = min(UnitDeviceInfo.SectorCount 
				- NDAS_SIZE_XAREA,
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
			// % 128 of smaller free space in pair
			if(i % 2) // finalize pair
			{
				if(bMigrateMirrorV1)
				{
					// migration code
					// in V1, mirror disks use first disk as primary
					TEST_AND_GOTO_WITH_ERROR_IF_FAIL(
						NDASOP_ERROR_INVALID_BIND_MIGRATE, out,
						DIBs_V2[i].sizeUserSpace - NDAS_SIZE_XAREA >
						DIBs_V2[i -1].sizeUserSpace);
					DIBs_V2[i].sizeUserSpace = DIBs_V2[i -1].sizeUserSpace;
				}
				else
				{
					DIBs_V2[i].sizeUserSpace = min(UnitDeviceInfo.SectorCount
						- NDAS_SIZE_XAREA,
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
					- NDAS_SIZE_XAREA;
			}
			DIBs_V2[i].AutoRecover = TRUE;
			break;
		case NMT_RAID4:
			// % 128 of smallest free space
			if(0 == i) // initialize
				DIBs_V2[0].sizeUserSpace = UnitDeviceInfo.SectorCount 
				- NDAS_SIZE_XAREA;

			DIBs_V2[0].sizeUserSpace = min(UnitDeviceInfo.SectorCount 
				- NDAS_SIZE_XAREA,
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
					pConnectionInfo[j].MacAddress, 
					sizeof(DIBs_V2[i].UnitDisks[j].MACAddr));
				DIBs_V2[i].UnitDisks[j].UnitNumber = (BYTE)pConnectionInfo[j].UnitNo;
			}
		}

		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_IMPLEMENTED, out, 
			ui64TotalSector <= NDAS_SIZE_DISK_LIMIT);
	}

	ZeroMemory(emptyData, SECTOR_SIZE * 128);
	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i;

		if(NMT_SINGLE != BindType)
		{
			// clear MBR
			if(bClearMBR && !bMigrateMirrorV1)
			{
				bResults = NdasOpClearMBR(ahNdasDevice[i]);
				TEST_AND_GOTO_IF_FAIL(out, bResults);
			}

			// clear X Area
			bResults = NdasOpClearXArea(ahNdasDevice[i]);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
		}

		// set CRC32
		DIBs_V2[i].crc32 = crc32_calc((unsigned char *)&DIBs_V2[i],
			sizeof(DIBs_V2[i].bytes_248));
		DIBs_V2[i].crc32_unitdisks = crc32_calc((unsigned char *)DIBs_V2[i].UnitDisks,
			sizeof(DIBs_V2[i].UnitDisks));

		// write DIB_V1, DIBs_V2
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNdasDevice[i], 
			NDAS_BLOCK_LOCATION_DIB_V1, 1, (PCHAR)&DIB_V1);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNdasDevice[i], 
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PCHAR)&DIBs_V2[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		if(nDiskCount > NDAS_MAX_UNITS_IN_V2)
		{
			_ASSERT(nDiskCount > NDAS_MAX_UNITS_IN_V2); // not coded
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

		HeapFree(::GetProcessHeap(), NULL, ahNdasDevice);
		ahNdasDevice = NULL;
	}

	if(DIBs_V2)
		HeapFree(::GetProcessHeap(), NULL, DIBs_V2);

	::SetLastError(dwLastErrorBackup);
	return iReturn;
}

NDASOP_API
BOOL
NdasOpRepair(
			 NDAS_CONNECTION_INFO *pConnectionInfo,
			 LPNDAS_OP_REPAIR_CALLBACK callback_func,
			 LPVOID lpParameter
			 )
{
	BOOL bResults = FALSE;
/*
	Repair Process
	. check parameters
	. read DIB information of a disk
	. accept only NMT_RAID1, NMT_RAID4
	. enumerate all disks (check failed disk count, validate DIB)
	.. leave all disks open(do not disconnect)
	.. check failed disk count : RAID1 repairs 1st broken pair and
		return 'NDASOP_INFORMATION_REPAIRED_PARTIALLY'
	.. validate DIB : every members of each DIBs
	. start repair (NdasOpRepairRAID1, NdasOpRepairRAID4)
*/

/*
	HNDAS hNdasDevice = NULL;
	NDAS_DIB_V2	DIB_V2;
	NDAS_DIB_V2	*DIBs_V2 = NULL;
	UINT32	nDIB_V2_Size, i;
	BOOL bFailed = FALSE;
	UINT32 nFailedDisk;
	BYTE buffer[128 * SECTOR_SIZE];
	BYTE *bitmap = NULL; // 1MB
	UINT32 j;
	// check parameters
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_IMPLEMENTED, out, pConnectionInfo);
 
	// check disks
	hNdasDevice = NdasCommConnect(pConnectionInfo);
	if(hNdasDevice) {} else {goto out;}

	// read DIB V2
	bResults = NdasOpReadDIB_V2(hNdasDevice, &DIB_V2, &nDIB_V2_Size);
	_ASSERT(nDIB_V2_Size == sizeof(NDAS_DIB_V2));
	if(ret) {} else {goto out;}

	bResults = NdasCommDisconnect(hNdasDevice);
	if(ret) {} else {goto out;}
	hNdasDevice = NULL;

	// check DIB information
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_IMPLEMENTED, out, 
		NMT_RAID1 == DIB_V2.iMediaType ||
		NMT_RAID4 == DIB_V2.iMediaType);

	bResults = NdasOpVerifyDiskCount(DIB_V2.iMediaType, DIB_V2.nDiskCount);
	if(ret) {} else {goto out;}

	// read all DIBs_V2 & check if all informations if ok
	DIBs_V2 = (PNDAS_DIB_V2)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, DIB_V2.nDiskCount * sizeof(NDAS_DIB));
	bitmap = (PNDAS_DIB_V2)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 1024 * 1024);

	for(i = 0; i < DIB_V2.nDiskCount < i++)
	{
		hNdasDevice = NdasCommConnect(pConnectionInfo);
		if(hNdasDevice) {} else {goto out;}

		bResults = NdasOpReadDIB_V2(hNdasDevice, &DIBs_V2[i], &nDIB_V2_Size);
		_ASSERT(nDIB_V2_Size == sizeof(NDAS_DIB_V2));
		if(ret) {} else {goto out;}

		// compare
		if(DIBs_V2) {} else {goto out;}

		// read & check Bitmap
		for(j = 0; j < 1024 * 1024; j += 128)
		{
			bResults = NdasCommBlockDeviceRead(hNdasDevice, NDAS_BLOCK_LOCATION_BITMAP + j, 128, bitmap + j);
			RtlInitializeBitMap()
		}
		// check Last written sector information
	
		bResults = NdasCommDisconnect(hNdasDevice);
		if(ret) {} else {goto out;}
		hNdasDevice = NULL;
	}

	// repair with bitmap information


	if(callback_func)
		callback_func(0, 0, lpParameter);

	// clear bitmap

	bResults = TRUE;
out:
	if(bitmap)
	{
		::HeapFree(::GetProcessHeap(), 0, bitmap);
		bitmap = NULL;
	}

	if(DIBs_V2)
	{
		::HeapFree(::GetProcessHeap(), 0, DIBs_V2);
		DIBs_V2 = NULL;
	}

	if(hNdasDevice)
	{
		NdasCommDisconnect(hNdasDevice);
		hNdasDevice = NULL;
	}
*/
	return bResults;
}

NDASOP_API
BOOL
NdasOpSynchronize(
				  NDAS_CONNECTION_INFO *pConnectionInfo,
				  LPNDAS_OP_REPAIR_CALLBACK callback_func,
				  LPVOID lpParameter
				  )
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

NDASOP_API
BOOL
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

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pnDIBSize, sizeof(UINT32)));

	// Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2
	bResults = NdasCommBlockDeviceRead(hNdasDevice, 
		NDAS_BLOCK_LOCATION_DIB_V2, 1, (PCHAR)&DIB_V2);
	TEST_AND_RETURN_IF_FAIL(bResults);

	// Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct
	if(NDAS_DIB_V2_SIGNATURE == DIB_V2.Signature &&
		DIB_V2.crc32 == crc32_calc((unsigned char *)&DIB_V2,
		sizeof(DIB_V2.bytes_248)) &&
		DIB_V2.crc32_unitdisks == crc32_calc((unsigned char *)DIB_V2.UnitDisks,
		sizeof(DIB_V2.UnitDisks))
		) // DIB V2
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
						(PCHAR)(pDIB_V2 +1));
					TEST_AND_RETURN_IF_FAIL(bResults);
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
		NDAS_BLOCK_LOCATION_DIB_V1, 1, (PCHAR)&DIB_V1);
	TEST_AND_RETURN_IF_FAIL(bResults);


	NDAS_UNIT_DEVICE_INFO UnitDeviceInfo;
	bResults = NdasCommGetUnitDeviceInfo(hNdasDevice, &UnitDeviceInfo);
	TEST_AND_RETURN_IF_FAIL(bResults);

	pDIB_V2->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIB_V2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIB_V2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIB_V2->sizeXArea = NDAS_SIZE_XAREA;
	pDIB_V2->iSectorsPerBit = 0; // no backup information
	pDIB_V2->sizeUserSpace = UnitDeviceInfo.SectorCount - NDAS_SIZE_XAREA; // in case of mirror, use primary disk size

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
		bResults = NdasCommGetDeviceId(hNdasDevice, 
			(BYTE *)&pDIB_V2->UnitDisks[0].MACAddr,
			&pDIB_V2->UnitDisks[0].UnitNumber);
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
			bResults = NdasCommGetDeviceId(hNdasDevice, 
				pUnitDiskLocation0->MACAddr, &pUnitDiskLocation0->UnitNumber);
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

NDASOP_API
BOOL
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
			NDAS_BLOCK_LOCATION_BITMAP + (i * 128), 128, (PCHAR)BitmapData);
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

NDASOP_API
BOOL
__stdcall
NdasOpGetLastWrttenSectorInfo(
	HNDAS hNdasDevice,
	PLAST_WRITTEN_SECTOR pLWS)
{
	BOOL bResults;

	CHAR sector[512];

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pLWS, sizeof(LAST_WRITTEN_SECTOR)));

	bResults = NdasCommBlockDeviceRead(hNdasDevice,
		NDAS_BLOCK_LOCATION_WRITE_LOG, 1, sector);
	TEST_AND_RETURN_IF_FAIL(bResults);

	CopyMemory(pLWS, sector, sizeof(LAST_WRITTEN_SECTOR));

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
			   HNDAS hNdasDevice
			   )
{
	BOOL bResults;
	CHAR emptyData [(NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR)
		* SECTOR_SIZE];
	ZeroMemory(emptyData, sizeof(emptyData));

	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_MBR, 
		(NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR), emptyData);

	return bResults;
}

BOOL
NdasOpClearXArea(
				 HNDAS hNdasDevice
				 )
{
	BOOL bResults;
	int i;
	CHAR emptyData[128 * SECTOR_SIZE];

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
