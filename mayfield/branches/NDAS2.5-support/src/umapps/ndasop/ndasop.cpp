// ndasop.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include <ndas/ndastypeex.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasop.h>
#include <scrc32.h>
#include <xdebug.h>
#include <objbase.h>

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


// AING : 2TB is limit of disk in windows at the moment.
// http://support.microsoft.com/default.aspx?scid=kb;en-us;325722
// ms-help://MS.MSDNQTR.2003OCT.1033/ntserv/html/S48B2.htm
#define	NDAS_SIZE_DISK_LIMIT ((UINT64)2 * 1024 * 1024 * 1024 * 1024 / SECTOR_SIZE)

static
BOOL
NdasOpClearMBR(HNDAS hNDAS);

static
BOOL
NdasOpClearXArea(HNDAS hNDAS);

static
BOOL
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
	NDAS_DIB_V2 DIB_V2, DIB_V2tmp;
	UINT32 nDIBSize;
	NDAS_RAID_META_DATA rmd;
	unsigned _int32 iOldMediaType;
	UINT32 i;
	UINT32 nDiskCount;

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

	nDiskCount = DIB_V2.nDiskCount;

// check whether able migrate or not
// rebuild DIB_V2
	iOldMediaType = DIB_V2.iMediaType;
	switch(iOldMediaType)
	{
	case NMT_MIRROR: // to RAID1R
		DIB_V2.iMediaType = NMT_RAID1R;
		break;
	case NMT_RAID1: // to RAID1R
		DIB_V2.iMediaType = NMT_RAID1R;
		break;
	case NMT_RAID4: // to RAID4R
		DIB_V2.iMediaType = NMT_RAID4R;
		break;
	default:
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out, FALSE);
		break;
	}

	// init rmd
	::ZeroMemory(&rmd, sizeof(NDAS_RAID_META_DATA));
	rmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;
	::CoCreateGuid(&rmd.guid);
	rmd.uiUSN = 1; // initial value

	ZeroMemory(&DIB_V1, sizeof(NDAS_DIB));
	NDAS_DIB_V1_INVALIDATE(DIB_V1);

	// create ahNDAS
	ahNDAS = (HNDAS *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 
		nDiskCount * sizeof(HNDAS));
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_MEMORY, out, ahNDAS);
	::ZeroMemory(ahNDAS, nDiskCount * sizeof(HNDAS));

	// connect to all devices
	for(i = 0; i < nDiskCount; i++)
	{
		::ZeroMemory(&CI, sizeof(NDASCOMM_CONNECTION_INFO));
		CI.Size = sizeof(NDASCOMM_CONNECTION_INFO);
		CI.AddressType = NDASCOMM_CIT_DEVICE_ID;
		CI.WriteAccess = TRUE;
		CI.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
		CI.Protocol = NDASCOMM_TRANSPORT_LPX;
		::CopyMemory(
			CI.Address.DeviceId.Node, 
			DIB_V2.UnitDisks[i].MACAddr, 
			sizeof(CI.Address.DeviceId.Node));
		CI.UnitNo = DIB_V2.UnitDisks[i].UnitNumber;

		ahNDAS[i] = NdasCommConnect(&CI);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(NDASOP_ERROR_ALREADY_USED, out,
			ahNDAS[i] || NDASCOMM_ERROR_RW_USER_EXIST != ::GetLastError());
		TEST_AND_GOTO_IF_FAIL(out, ahNDAS[i]);

		NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};
		udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
		bResults = NdasCommGetUnitDeviceHardwareInfoW(ahNDAS[i], &udinfo);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		// init rmd.UnitMetaData[i]
		rmd.UnitMetaData[i].iUnitDeviceIdx = (unsigned _int16)i;
		rmd.UnitMetaData[i].UnitDeviceStatus = NULL;

		switch(iOldMediaType)
		{
		case NMT_MIRROR:
			// fix size user space
			if(0 == i)
			{
				DIB_V2.sizeXArea = NDAS_BLOCK_SIZE_XAREA;
				DIB_V2.sizeUserSpace = udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;
				DIB_V2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;
			}
			else if(1 == i)
			{
				rmd.UnitMetaData[i].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
			}
			break;
		default:
			break;
		}
	}

	// write DIB_V2 & RMD
	for(i = 0; i < nDiskCount; i++)
	{
		// clear MBR - migration must not do this
//		bResults = NdasOpClearMBR(ahNDAS[i]);

		// clear X Area
		bResults = NdasOpClearXArea(ahNDAS[i]);
		TEST_AND_GOTO_IF_FAIL(out, bResults);

		DIB_V2.iSequence = i;

		// set CRC32
		SET_DIB_CRC(crc32_calc, DIB_V2);
		SET_RMD_CRC(crc32_calc, rmd);

		// write DIB_V1, DIBs_V2, RMD(ignore RAID type)
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&DIB_V1);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)&rmd);
		bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[i], 
			NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&rmd);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}

	bReturn = TRUE;
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

	if(hNDAS)
	{
		if(hNDAS)
		{
			bResults = NdasCommDisconnect(hNDAS);
			_ASSERT(bResults);
			hNDAS = NULL;
		}
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
	HNDAS *ahNDAS;
	NDAS_DIB DIB_V1;
	NDAS_DIB_V2 *DIBs_V2 = NULL;
	UINT32 nDIBSize;

	BOOL bClearMBR;
	BOOL bMigrateMirrorV1;
	
	BYTE emptyData[SECTOR_SIZE];
	NDASCOMM_CONNECTION_INFO ConnectionInfoDiscover;
	UINT64 ui64TotalSector = 0;
	UINT32 MediaType;
	NDAS_RAID_META_DATA rmd;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * nDiskCount));

	switch(BindType)
	{
	case NMT_SINGLE:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_SAFE_RAID1:
	case NMT_RAID1R:
	case NMT_RAID4R:
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
	::CoCreateGuid(&rmd.guid);
	rmd.uiUSN = 1; // initial value

	// gather information & initialize DIBs
	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i; // record last accessing disk

		// other type is not supported yet
		// using mac address in pConnectionInfo to build NDAS_DIB_V2
		_ASSERT(NDASCOMM_CIT_DEVICE_ID == pConnectionInfo->AddressType);

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
					(
						(
							NMT_RAID1 == DIBs_V2[i].iMediaType ||
							NMT_RAID1R == DIBs_V2[i].iMediaType
						) &&
						2 == DIBs_V2[i].nDiskCount
					)
				)
				bClearMBR = FALSE;
		}
		else if(NMT_RAID1R == BindType)
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
		DIBs_V2[i].iMediaType = (BindType == NMT_SAFE_RAID1) ? NMT_RAID1R : BindType;
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
		case NMT_RAID1R:
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
						udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA >=
						DIBs_V2[i -1].sizeUserSpace);
					DIBs_V2[i].sizeUserSpace = DIBs_V2[i -1].sizeUserSpace;
				}
				else
				{
					DIBs_V2[i].sizeUserSpace = min(
						udinfo.SectorCount.QuadPart- NDAS_BLOCK_SIZE_XAREA,
						DIBs_V2[i -1].sizeUserSpace);
					//
					// Reduce user space by 1%. HDDs with same giga size labels have different sizes.
					//		Sometimes, it is up to 7.5% difference due to 1k !=1000.
					//		Once, I found Maxter 160G HDD size - Samsung 160G HDD size = 4G. 
					//		Even with same maker's HDD with same gig has different sector size.
					//	To do: Give user a option to select this margin.
					//
					
					DIBs_V2[i].sizeUserSpace = DIBs_V2[i].sizeUserSpace * 99/ 100;
				}

				// set fault to backup device
				if(NMT_SAFE_RAID1 == BindType)
				{
					rmd.UnitMetaData[i].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
				}

				// sectors per bit
				for(j = 16; TRUE; j++)
				{
					if(DIBs_V2[i].sizeUserSpace <= ((UINT64) (1<<j)) * NDAS_BLOCK_SIZE_BITMAP * NDAS_BIT_PER_OOS_BITMAP_BLOCK ) // 512 GB : 128 SPB
					{
						// Sector per bit is big enough to be covered by bitmap
						DIBs_V2[i].iSectorsPerBit = 1 << j;
						break;
					}
				}

				// Trim user space that is out of bitmap align.
				DIBs_V2[i].sizeUserSpace -= DIBs_V2[i].sizeUserSpace 
					% DIBs_V2[i].iSectorsPerBit;

				DIBs_V2[i -1].sizeUserSpace = DIBs_V2[i].sizeUserSpace;
				DIBs_V2[i -1].iSectorsPerBit = DIBs_V2[i].iSectorsPerBit;

				ui64TotalSector += DIBs_V2[i].sizeUserSpace;
			}
			else // initialize pair
			{
				DIBs_V2[i].sizeUserSpace = 
					udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;
			}
			DIBs_V2[i].AutoRecover = TRUE;
			break;
		case NMT_RAID4R:
			// % 128 of smallest free space
			if(0 == i) // initialize
				DIBs_V2[0].sizeUserSpace = 
				udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;

			DIBs_V2[0].sizeUserSpace = min(
				udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA,
				DIBs_V2[0].sizeUserSpace);

			if(nDiskCount -1 == i) // finalize
			{
				// sectors per bit
				for(j = 7; TRUE; j++)
				{
					if(DIBs_V2[i].sizeUserSpace <= 1 << (23 + j)) // 512 GB : 128 SPB
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
			ui64TotalSector = udinfo.SectorCount.QuadPart;
			break;
		}

		if(NMT_SINGLE != BindType)
		{
			for(j = 0; j < nDiskCount; j++)
			{
				C_ASSERT(
					sizeof(pConnectionInfo[j].Address.DeviceId.Node) ==
					sizeof(DIBs_V2[i].UnitDisks[j].MACAddr));

				CopyMemory(
					DIBs_V2[i].UnitDisks[j].MACAddr, 
					pConnectionInfo[j].Address.DeviceId.Node, 
					sizeof(DIBs_V2[i].UnitDisks[j].MACAddr));
				DIBs_V2[i].UnitDisks[j].UnitNumber = (BYTE)pConnectionInfo[j].UnitNo;
			}
		}

		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_SIZE_LIMIT, out, 
			ui64TotalSector <= NDAS_SIZE_DISK_LIMIT);
	}
	SET_RMD_CRC(crc32_calc, rmd);


	ZeroMemory(emptyData, SECTOR_SIZE);
	for(i = 0; i < nDiskCount; i++)
	{
		iReturn = i;

		// clear MBR
		if(bClearMBR && !bMigrateMirrorV1 && NMT_SAFE_RAID1 != BindType)
		{
			bResults = NdasOpClearMBR(ahNDAS[i]);
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
	}

	if(NMT_SAFE_RAID1 == BindType) // invalidate whole bitmap
	{
		// Corruption Bitmap : 1MB
		FillMemory(emptyData, sizeof(emptyData), 0xff);

		for(i = 0; i < NDAS_BLOCK_SIZE_BITMAP; i++)
		{
			bResults = NdasCommBlockDeviceWriteSafeBuffer(ahNDAS[0], 
				NDAS_BLOCK_LOCATION_BITMAP + i, 1, emptyData);
			TEST_AND_GOTO_IF_FAIL(out, bResults);
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
	UINT32 i;
	DWORD dwUnitNumber;
	UINT32 nTotalDiskCount;
	NDAS_UNITDEVICE_HARDWARE_INFOW UnitInfo;

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
		goto process_v1;
	}

	if(NDAS_BLOCK_SIZE_XAREA != DIB_V2.sizeXArea &&
		NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE != DIB_V2.sizeXArea)
	{
		goto process_v1;
	}

	UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
	NdasCommGetUnitDeviceHardwareInfo(hNDAS, &UnitInfo);

	if(DIB_V2.sizeUserSpace + DIB_V2.sizeXArea > UnitInfo.SectorCount.QuadPart)
	{
		goto process_v1;
	}

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	// check DIB_V2.nDiskCount
	if(!NdasOpVerifyDiskCount(DIB_V2.iMediaType, DIB_V2.nDiskCount))
		goto process_v1;

	if(DIB_V2.iSequence >= nTotalDiskCount)
		goto process_v1;

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

				// Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND incase of more than 32 NDAS Unit devices exist 4. Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 if  NDAS_DIB_V2 information is not acceptable
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

					if(nTotalDiskCount <= DIB_V2.iSequence)
						goto process_v1;

					bResults = NdasCommGetDeviceID(hNDAS, MACAddr, &dwUnitNumber);
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
		pDIB_V2->nSpareCount = 0;

		// only 1 unit		
		bResults = NdasCommGetDeviceID(hNDAS, 
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
			bResults = NdasCommGetDeviceID(hNDAS, 
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

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpIsBitmapClean(
    HNDAS hNDAS,
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
		bResults = NdasCommBlockDeviceRead(hNDAS,
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
	case NMT_RAID1R:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			nDiskCount >= 2 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND);
		break;
	case NMT_RAID4:
		TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_DISK_COUNT, 
			3 == nDiskCount || 5 == nDiskCount || 9 == nDiskCount);
		break;
	case NMT_RAID4R:
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
NdasOpClearMBR(
    HNDAS hNDAS)
{
	BOOL bResults;
	int i;
	BYTE emptyData[SECTOR_SIZE];
	ZeroMemory(emptyData, sizeof(emptyData));
	
	for(i = 0; i < (NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR); i++)
	{
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
			NDAS_BLOCK_LOCATION_MBR + i, 1, emptyData);
	}

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

	// Last written sector
	for(i = 0; i < NDAS_BLOCK_SIZE_LWR; i++)
	{
		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, 
			NDAS_BLOCK_LOCATION_LWR + i, 1, emptyData);
		TEST_AND_RETURN_IF_FAIL(bResults);
	}

	return TRUE;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDWrite(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	NDAS_RAID_META_DATA *rmd
	)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i, j;
	UINT32 uiUSNMax;
	HNDAS hNDAS;
	UINT32 nDiskCount;
	NDAS_DIB_V2 DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2_Ref);
	NDAS_RAID_META_DATA rmd_tmp;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(rmd, sizeof(NDAS_RAID_META_DATA)));

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R == DIB_V2_Ref.iMediaType || NMT_RAID4R == DIB_V2_Ref.iMediaType);

	nDiskCount = DIB_V2_Ref.nDiskCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER, out, 0 < nDiskCount);

	// read NDAS_BLOCK_LOCATION_RMD_T to get highest USN
	uiUSNMax = rmd->uiUSN;

	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2_Ref, hNDAS, FALSE, l_ci)
	{
		if(!hNDAS)
		{
			// device does not exist, just skip
			// TEST_AND_GOTO_IF_FAIL(out, hNDAS);
			continue;
		}

		bResults = NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)&rmd_tmp);
		if(!bResults)
			continue;

		if(
			NDAS_RAID_META_DATA_SIGNATURE != rmd->Signature || 
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
			}
		}
	}

	// ndasbind MUST reinitialize RMD
	::CoCreateGuid(&rmd->guid);

	// increase USN to highest
	rmd->uiUSN = uiUSNMax +1;
	SET_RMD_CRC(crc32_calc, *rmd);

	// write rmd to NDAS_BLOCK_LOCATION_RMD_T
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2_Ref, hNDAS, TRUE, l_ci)
	{
		if(!hNDAS)
			continue;

		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)rmd);
		if(!bResults)
			continue;
	}

	// write rmd to NDAS_BLOCK_LOCATION_RMD
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2_Ref, hNDAS, TRUE, l_ci)
	{
		if(!hNDAS)
			continue;

		bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)rmd);
		if(!bResults)
			continue;
	}

	bReturn = TRUE;
out:
	return bReturn;
}

/*
ensure that the raid is bound correctly
This function assumes that ...
. Use same password, protocol ... as ci
. Bind is correct (allows missing device though)
. 
*/

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDRead(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	NDAS_RAID_META_DATA *rmd
)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i, j;
	UINT32 uiUSNMax;
	HNDAS hNDAS;
	UINT32 nDiskCount;
	NDAS_DIB_V2 DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2_Ref);
	NDAS_RAID_META_DATA rmd_tmp;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(rmd, sizeof(NDAS_RAID_META_DATA)));

	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2_Ref, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R == DIB_V2_Ref.iMediaType || NMT_RAID4R == DIB_V2_Ref.iMediaType);

	nDiskCount = DIB_V2_Ref.nDiskCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER, out, 0 < nDiskCount);

	// enumerate RMD and check status
	uiUSNMax = 0;

	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2_Ref, hNDAS, FALSE, l_ci)
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

	bReturn = TRUE;
out:

	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReplaceDevice(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ci_replace,
	CONST UINT32 nReplace)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i, j;
	UINT32 uiUSNMax;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci_replace, sizeof(NDASCOMM_CONNECTION_INFO)));

	// fast check
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		NDASCOMM_CIT_DEVICE_ID == ci_replace->AddressType);

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R == DIB_V2.iMediaType || NMT_RAID4R == DIB_V2.iMediaType);

	// read RMD
	bResults = NdasOpRMDRead(ci, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		out, nReplace < nTotalDiskCount);

	// check all devices
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		if(nReplace == i)
		{
			// do not care if the device is alive or not
			continue;
		}

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
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// ok let's start to write

	// replace into DIB
	::CopyMemory(
		DIB_V2.UnitDisks[nReplace].MACAddr, 
		ci_replace->Address.DeviceId.Node, 
		sizeof(ci_replace->Address.DeviceId.Node));
	C_ASSERT(sizeof(ci_replace->Address.DeviceId.Node) == 
		sizeof(DIB_V2.UnitDisks[nReplace].MACAddr));

	DIB_V2.UnitDisks[nReplace].UnitNumber = (unsigned _int8)ci_replace->UnitNo;

	// replace into RMD
	if(nReplace < DIB_V2.nDiskCount)
	{
		// If there is already a fault device, it should be nReplace. If not, we can't proceed (2 fails).
		for(i = 0; i < DIB_V2.nDiskCount; i++)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_TOO_MANY_MISSING_MEMBER, out,
				!(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & rmd.UnitMetaData[i].UnitDeviceStatus) ||
				nReplace == i);
		}
		// Set fault to RMD at nReplace
		rmd.UnitMetaData[nReplace].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		// Clear defective flag. 
		rmd.UnitMetaData[nReplace].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_DEFECTIVE;
		// Unset unmount flag -> driver will do full recover
		rmd.state &= ~NDAS_RAID_META_DATA_STATE_UNMOUNTED;
	}
	else
	{
		// spare : there's nothing to do here
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
	bResults = NdasOpRMDWrite(ci, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	return bReturn;
}

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReplaceUnitDevice(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ci_replace,
	CONST UINT32 nReplace)
{
	BOOL bResults = FALSE;
	BOOL bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO l_ci;
	UINT32 i, j;
	UINT32 uiUSNMax;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;
	NDAS_UNITDEVICE_HARDWARE_INFOW UnitInfo;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci_replace, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		ci->WriteAccess); // use write access. this function does not support run time replace yet.
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		ci_replace->WriteAccess);

	// fast check
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		NDASCOMM_CIT_DEVICE_ID == ci_replace->AddressType);

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R == DIB_V2.iMediaType || NMT_RAID4R == DIB_V2.iMediaType);

	// read RMD
	bResults = NdasOpRMDRead(ci, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		out, nReplace < nTotalDiskCount);

	// check all devices
	// use write access. this function does not support run time replace yet.
	ENUM_NDAS_DEVICES_IN_DIB(i, DIB_V2, hNDAS, TRUE, l_ci)
	{
		TEST_AND_GOTO_IF_FAIL(out, hNDAS); // ok to use goto here

		if(nReplace == i)
		{
			// nReplace might be single
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

	// check replace size
	hNDAS = NdasCommConnect(ci_replace);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	::ZeroMemory(&UnitInfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));
	UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	bResults = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &UnitInfo);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_CAPACITY, out,
		UnitInfo.SectorCount.QuadPart >= DIB_V2.sizeUserSpace + NDAS_BLOCK_SIZE_XAREA);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// ok let's start to write

	// replace into RMD
	if(nReplace < DIB_V2.nDiskCount)
	{
		// If there is already a fault device, it should be nReplace. If not, we can't proceed (2 fails).
		for(i = 0; i < DIB_V2.nDiskCount; i++)
		{
			TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_TOO_MANY_MISSING_MEMBER, out,
				!(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & rmd.UnitMetaData[i].UnitDeviceStatus) ||
				nReplace == i);
		}
		// Set fault to RMD at nReplace
		rmd.UnitMetaData[nReplace].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		// Clear defective flag. 
		rmd.UnitMetaData[nReplace].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_DEFECTIVE;
		// Unset unmount flag -> driver will do full recover
		rmd.state &= ~NDAS_RAID_META_DATA_STATE_UNMOUNTED;
	}
	else
	{
		// spare : there's nothing to do here
	}

	SET_RMD_CRC(crc32_calc, rmd);

	// write back DIB to replace
	hNDAS = NdasCommConnect(ci_replace);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	DIB_V2.iSequence = nReplace;
	SET_DIB_CRC(crc32_calc, DIB_V2);

	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS,
		NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

	if(!bResults)
	{
		NdasCommDisconnect(hNDAS);
		goto out;
	}

	NdasCommDisconnect(hNDAS);

	// write back RMD
	bResults = NdasOpRMDWrite(ci, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	return bReturn;
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
	UINT32 i, j;
	UINT32 uiUSNMax;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci, sizeof(NDASCOMM_CONNECTION_INFO)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ciSpare, sizeof(NDASCOMM_CONNECTION_INFO)));
	// fast check
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		NDASCOMM_CIT_DEVICE_ID == ciSpare->AddressType);

	// read DIB
	hNDAS = NdasCommConnect(ci);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R == DIB_V2.iMediaType || NMT_RAID4R == DIB_V2.iMediaType);

	// read RMD
	bResults = NdasOpRMDRead(ci, &rmd);
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
		udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
		bResults = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &udinfo);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
		TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_NOT_ENOUGH_CAPACITY, out,
			udinfo.SectorCount.QuadPart >= DIB_V2.sizeUserSpace + NDAS_BLOCK_SIZE_XAREA);

		bResults = NdasCommDisconnect(hNDAS);
		TEST_AND_GOTO_IF_FAIL(out, bResults);
	}


	// ok let's start to write

	// add spare to DIB
	DIB_V2.nSpareCount++;
	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;

	::CopyMemory(
		DIB_V2.UnitDisks[nTotalDiskCount -1].MACAddr, 
		ciSpare->Address.DeviceId.Node, 
		sizeof(ciSpare->Address.DeviceId.Node));
	
	C_ASSERT(
		sizeof(DIB_V2.UnitDisks[nTotalDiskCount -1].MACAddr) ==
		sizeof(ciSpare->Address.DeviceId.Node));

	DIB_V2.UnitDisks[nTotalDiskCount -1].UnitNumber = (unsigned _int8)ciSpare->UnitNo;

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
	bResults = NdasOpRMDWrite(ci, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
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
	UINT32 i, j;
	UINT32 uiUSNMax;
	HNDAS hNDAS;
	UINT32 nTotalDiskCount;
	NDAS_DIB_V2 DIB_V2, DIB_V2_Ref;
	UINT32 sizeDIB = sizeof(DIB_V2);
	NDAS_RAID_META_DATA rmd;
	UINT32 iDeviceNo, iDeviceNoInRMD;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(ci_spare, sizeof(NDASCOMM_CONNECTION_INFO)));

	// ci_spare is also used to find the device in DIB using device id
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		NDASCOMM_CIT_DEVICE_ID == ci_spare->AddressType);

	// read DIB
	hNDAS = NdasCommConnect(ci_spare);
	TEST_AND_GOTO_IF_FAIL(out, hNDAS);

	bResults = NdasOpReadDIB(hNDAS, &DIB_V2, &sizeDIB);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasCommDisconnect(hNDAS);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	// check RAID type
	TEST_AND_GOTO_WITH_ERROR_IF_FAIL(NDASOP_ERROR_BIND_UNSUPPORTED, out,
		NMT_RAID1R == DIB_V2.iMediaType || NMT_RAID4R == DIB_V2.iMediaType);

	// read RMD
	bResults = NdasOpRMDRead(ci_spare, &rmd);
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
	bResults = NdasOpRMDWrite(&l_ci, &rmd);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bResults = NdasOpBind(1, ci_spare, NMT_SINGLE, 0);
	TEST_AND_GOTO_IF_FAIL(out, bResults);

	bReturn = TRUE;
out:
	return bReturn;
}
