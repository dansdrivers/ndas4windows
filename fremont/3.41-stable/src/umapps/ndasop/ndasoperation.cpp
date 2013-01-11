// ndasoperation.cpp : Defines the entry point for the DLL application.
// revised by William Kim 8/13/2008

#include "stdafx.h"

#include <ndas/ndasdib.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasop.h>
#include "resource.h"
#include <socketlpx.h>
#include <scrc32.h>
#include <objbase.h>
#include <cguid.h>

extern HMODULE NDASOPModule;

#ifdef RUN_WPP
#include "ndasop.tmh"
#endif


LONG DbgLevelNdasOp = DBG_LEVEL_NDAS_OP;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelNdasOp) {								\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

NDASOP_LINKAGE
DWORD
NDASOPAPI
NdasOpGetAPIVersion()
{
	return (DWORD)MAKELONG( NDASOP_API_VERSION_MAJOR, NDASOP_API_VERSION_MINOR );
}

static 
HRESULT
NdasOpVerifyDiskCount (
	UINT32 BindType,
	UINT32 DiskCount
	)
{
	// check disk count

	switch (BindType) {

	case NMT_AGGREGATE:
	case NMT_SAFE_AGGREGATE:
		
		if ((DiskCount >= 2 && DiskCount <= NDAS_MAX_UNITS_IN_BIND) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT;
		}

		break;

	case NMT_RAID0:

		if ((2 == DiskCount || 4 == DiskCount || 8 == DiskCount) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_SAFE_RAID1:

		if ((DiskCount == 2) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_RAID1:

		if ((DiskCount >= 2 && DiskCount <= NDAS_MAX_UNITS_IN_BIND && !(DiskCount %2)) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_RAID1R2:
	case NMT_RAID1R3:	

		if ((DiskCount >= 2 && DiskCount <= NDAS_MAX_UNITS_IN_BIND) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_RAID4:
	case NMT_RAID4R3:
	case NMT_RAID5:

		if ((3 == DiskCount || 5 == DiskCount || 9 == DiskCount) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_RAID4R2:

		if ((DiskCount >= 3 && DiskCount <= NDAS_MAX_UNITS_IN_BIND) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_SINGLE:

		if ((DiskCount > 0 && DiskCount <= NDAS_MAX_UNITS_IN_BIND) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	case NMT_SAFE_RAID_ADD:

		if ((DiskCount == 3 ||  DiskCount == 4 || DiskCount == 6 || DiskCount == 1) == FALSE) {

			ATLASSERT(FALSE);
			return NDASOP_ERROR_INVALID_DISK_COUNT; 
		}

		break;

	default:

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_DISK_COUNT; 
	}

	return S_OK;
}

static
NDAS_LOGICALDEVICE_TYPE
NdasVsmConvertToNdasLogicalUnitType (
	NDAS_MEDIA_TYPE type
	)
{
	switch (type) {

	case NMT_SINGLE:	return NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE;
	case NMT_MIRROR:	return NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED;
	case NMT_AGGREGATE:	return NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED;
	case NMT_RAID1:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1;
	case NMT_RAID4:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4;
	case NMT_RAID0:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID0;
	case NMT_RAID0R2:	return NDAS_LOGICALDEVICE_TYPE_DISK_RAID0;
	case NMT_RAID1R2:	return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2;
	case NMT_RAID4R2:	return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2;
	case NMT_RAID1R3:	return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3;
	case NMT_RAID4R3:	return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3;
	case NMT_RAID5:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID5;
	case NMT_VDVD:		return NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD;
	case NMT_CDROM:		return NDAS_LOGICALDEVICE_TYPE_DVD;
	case NMT_OPMEM:		return NDAS_LOGICALDEVICE_TYPE_MO;
	case NMT_FLASH:		return NDAS_LOGICALDEVICE_TYPE_FLASHCARD;
	case NMT_CONFLICT:
	case NMT_INVALID:
	case NMT_SAFE_RAID1:
	case NMT_AOD:
	case NMT_SAFE_AGGREGATE:
	default:

		ATLASSERT(FALSE);

		return NDAS_LOGICALDEVICE_TYPE_UNKNOWN;
	}
}

static
VOID
NdasVsmConvertToNdasDeviceId (
	OUT	NDAS_DEVICE_ID		*ndi, 
	IN	UNIT_DISK_LOCATION	*udl
	)
{
	ATLASSERT(ndi);
	ATLASSERT(udl);

	ZeroMemory( ndi, sizeof(NDAS_DEVICE_ID) );

	::CopyMemory( ndi->Node, udl->MACAddr, 6 );

	ndi->Vid = udl->VID;
}

static 
HRESULT
NdasOpClearMbr (
    HNDAS Hndas
	)
{
	int  i;
	BYTE emptyData[SECTOR_SIZE];

	ZeroMemory(emptyData, sizeof(emptyData));

	for (i = 0; i <= (NDAS_BLOCK_LOCATION_USER - NDAS_BLOCK_LOCATION_MBR); i++) {

		if (NdasCommBlockDeviceWriteSafeBuffer( Hndas, NDAS_BLOCK_LOCATION_MBR + i, 1, emptyData) == FALSE) {
			
			return E_FAIL;
		}
	}
	
	return S_OK;
}

static 
HRESULT
NdasOpClearXArea (
    HNDAS Hndas
	)
{
	int i;
	BYTE emptyData[SECTOR_SIZE];

	ZeroMemory( emptyData, sizeof(emptyData) );

	// NDAS_DIB_V1

	ATLVERIFY( NdasCommBlockDeviceWriteSafeBuffer(Hndas, NDAS_BLOCK_LOCATION_DIB_V1, 1, emptyData) );

	ATLVERIFY( NdasCommBlockDeviceWriteSafeBuffer(Hndas, NDAS_BLOCK_LOCATION_DIB_V2, 1, emptyData) );

	ATLVERIFY( NdasCommBlockDeviceWriteSafeBuffer(Hndas, NDAS_BLOCK_LOCATION_ENCRYPT, 1, emptyData) );

	ATLVERIFY( NdasCommBlockDeviceWriteSafeBuffer(Hndas, NDAS_BLOCK_LOCATION_RMD, 1, emptyData) );

	// Additional bind informations : 256 sectors

	for (i = 0; i < 256; i++) {

		if (NdasCommBlockDeviceWriteSafeBuffer(Hndas, NDAS_BLOCK_LOCATION_ADD_BIND + i, 1, emptyData) == FALSE) {
			
			break;
		}
	}

	for (i = 0; i < 256; i++) {

		if (NdasCommBlockDeviceWriteSafeBuffer(Hndas, NDAS_BLOCK_LOCATION_BITMAP + i, 1, emptyData) == FALSE) {

			break;
		}
	}

	return S_OK;
}

static
__forceinline 
void 
SetNdasConnectionInfoFromDIBIndex (
	NDASCOMM_CONNECTION_INFO	*lpci,
	BOOL						bWriteAccess,
	NDAS_DIB_V2					*lpdibv2,
	DWORD						index
	)
{
	::ZeroMemory(lpci, sizeof(NDASCOMM_CONNECTION_INFO));
	
	lpci->Size = sizeof(NDASCOMM_CONNECTION_INFO);
	lpci->AddressType = NDASCOMM_CIT_DEVICE_ID;
	lpci->WriteAccess = bWriteAccess;
	lpci->LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	lpci->Protocol = NDASCOMM_TRANSPORT_LPX;
	
	::CopyMemory( lpci->Address.DeviceId.Node, 
				  lpdibv2->UnitLocation[index].MACAddr, 
				  sizeof(lpci->Address.DeviceId.Node) );

	//lpci->UnitNo = lpdibv2->UnitLocation[index].UnitNumber;
	lpci->Address.DeviceId.Vid = lpdibv2->UnitLocation[index].VID;
}

static 
void 
SetNdasConnectionInfo (
	PNDASCOMM_CONNECTION_INFO	ConnectionInfo,
	BOOL						WriteAccess,
	NDAS_DEVICE_ID				NdasDeviceId,
	UINT8						UnitNo
	)
{
	ZeroMemory( ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) );
	
	ConnectionInfo->Size		= sizeof(NDASCOMM_CONNECTION_INFO);
	ConnectionInfo->AddressType = NDASCOMM_CIT_DEVICE_ID;
	ConnectionInfo->WriteAccess = WriteAccess;
	ConnectionInfo->LoginType	= NDASCOMM_LOGIN_TYPE_NORMAL;
	ConnectionInfo->Protocol	= NDASCOMM_TRANSPORT_LPX;
	
	ConnectionInfo->Address.DeviceId = NdasDeviceId;
	ConnectionInfo->UnitNo			 = UnitNo;

	return;
}

NDASOP_LINKAGE
HRESULT
NDASOPAPI
GetNdasSimpleSerialNo (
	PNDAS_UNITDEVICE_HARDWARE_INFO	UnitInfo,
	PCHAR							NdasSimpleSerialNo
	)
{
	int		converted;
	CHAR	ndasSimpleSerialNo[20+4] = {0};
		
	converted = ::WideCharToMultiByte( CP_ACP, 0, 
									   UnitInfo->SerialNumber, RTL_NUMBER_OF(UnitInfo->SerialNumber), 
									   ndasSimpleSerialNo, RTL_NUMBER_OF(ndasSimpleSerialNo), 
									   NULL, NULL );
	if (converted != 24) {

		ATLASSERT(FALSE);
		return E_FAIL;
	}

	UINT serialNoLen = strlen(ndasSimpleSerialNo);
	UINT startIdx = (serialNoLen < NDAS_DIB_SERIAL_LEN) ? 0 : (serialNoLen - NDAS_DIB_SERIAL_LEN);

	CopyMemory( NdasSimpleSerialNo, &ndasSimpleSerialNo[startIdx], NDAS_DIB_SERIAL_LEN );
		
	NdasUiDbgCall( 4, "serialNoLen = %d, ndasSimpleSerialNo = %s NdasSimpleSerialNo = %s\n", 
				   serialNoLen, ndasSimpleSerialNo, NdasSimpleSerialNo );

	return S_OK;
}

// 1. Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2
// 2. Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct
// 3. Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND incase of more than 32 NDAS Unit devices exist 4. Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 if  NDAS_DIB_V2 information is not acceptable
// 4. Check Signature and Version informations in NDAS_DIB_V1 and translate the NDAS_DIB_V1 to an NDAS_DIB_V2
// 5. Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1 is not acceptable either

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpReadDib (
	HNDAS		Hndas,
	NDAS_DIB_V2 *DibV2,
	UINT32		*DibSize
	)
{
	HRESULT		hresult;

	NDAS_DIB	dibV1;
	NDAS_DIB_V2 dibV2;

	UINT32		dibSize;

	UINT32		totalDiskCount;

	NDAS_UNITDEVICE_HARDWARE_INFOW unitInfo;

	BOOLEAN conflictDib = FALSE;

	
	if (::IsBadWritePtr(DibSize, sizeof(UINT32))) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER;
	}

	// Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2

	if (NdasCommBlockDeviceRead(Hndas, NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&dibV2) == FALSE) {
		
		return GetLastError();
	}

	// Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct

	if (dibV2.Signature != NDAS_DIB_V2_SIGNATURE) {

		goto process_v1;
	}

	if (!IS_DIB_CRC_VALID(crc32_calc, dibV2)) {

		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	if (dibV2.nDiskCount + dibV2.nSpareCount > NDAS_MAX_UNITS_IN_V2_1) {

		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	if (dibV2.sizeXArea != NDAS_BLOCK_SIZE_XAREA && dibV2.sizeXArea != NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE) {

		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	unitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);

	NdasCommGetUnitDeviceHardwareInfo( Hndas, &unitInfo );

	NdasUiDbgCall( 4, _T("NdasCommGetUnitDeviceHardwareInfo\n") );
	NdasUiDbgCall( 4, _T("unitInfo.SerialNumber = %s\n"), unitInfo.SerialNumber );
	NdasUiDbgCall( 4, _T("unitInfo.Model = %s\n"), unitInfo.Model );

	if (dibV2.sizeUserSpace + dibV2.sizeXArea > unitInfo.SectorCount.QuadPart) {

		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	totalDiskCount = dibV2.nDiskCount + dibV2.nSpareCount;

	// check DIB_V2.nDiskCount

	hresult = NdasOpVerifyDiskCount(dibV2.iMediaType, dibV2.nDiskCount);
	
	if (FAILED(hresult)) {
	
		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	if (dibV2.iSequence >= totalDiskCount) {

		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	if (totalDiskCount > 32 + 64 + 64) {

		ATLASSERT(FALSE);
		conflictDib = TRUE;
		goto process_v1;
	}

	// check done, copy DIB_V2 information from NDAS Device to pDIB_V2

	// code does not support if version in DIB_V2 is greater than the version defined
	
	if (!(NDAS_DIB_VERSION_MAJOR_V2 > dibV2.MajorVersion ||
		  (NDAS_DIB_VERSION_MAJOR_V2 == dibV2.MajorVersion && NDAS_DIB_VERSION_MINOR_V2 >= dibV2.MinorVersion))) {
			
		ATLASSERT(FALSE);
		
		return NDASOP_ERROR_DEVICE_UNSUPPORTED; 		
	}

	dibSize = (GET_TRAIL_SECTOR_COUNT_V2(totalDiskCount) + 1) * sizeof(NDAS_DIB_V2);

	if (DibSize != NULL) {

		if (*DibSize >= dibSize) { // make sure there is enough space

			if (::IsBadWritePtr(DibV2, dibSize)) {
				
				ATLASSERT(FALSE);				
				return NDASOP_ERROR_INVALID_PARAMETER;
			}

			CopyMemory( DibV2, &dibV2, sizeof(NDAS_DIB_V2) );

			// Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND 
			//		in case of more than 32 NDAS Unit devices exist 4. 
			// Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 
			//		if  NDAS_DIB_V2 information is not acceptable
	
			if (dibSize > sizeof(NDAS_DIB_V2)) {

				if (NdasCommBlockDeviceRead(Hndas, 
											NDAS_BLOCK_LOCATION_ADD_BIND, 
											GET_TRAIL_SECTOR_COUNT_V2(totalDiskCount),
											(PBYTE)(DibV2 + 1)) == FALSE) {

					return GetLastError();
				}
			
			} else { // do not copy, possibly just asking size

				*DibSize = dibSize;
			}

		} else { // copy 1 sector only

			if (::IsBadWritePtr(DibV2, dibSize)) {

				ATLASSERT(FALSE);

				return NDASOP_ERROR_INVALID_PARAMETER; 		
			}

			CopyMemory( DibV2, &dibV2, sizeof(NDAS_DIB_V2) );
		}

		if (dibV2.iMediaType != NMT_SINGLE) {

			UINT8 macAddr[6];
			UINT8 unitNumber;
			UINT8 vid;
			CHAR ndasSimpleSerialNo[20+4] = {0};

			if (totalDiskCount <= dibV2.iSequence) {

				ATLASSERT(FALSE);
				goto process_v1;
			}

			if (NdasCommGetDeviceID(Hndas, NULL, macAddr, &unitNumber, &vid) == FALSE) {
				
				ATLASSERT(FALSE);

				hresult = GetLastError();
				goto process_v1;
			}
			
			hresult = GetNdasSimpleSerialNo( &unitInfo, ndasSimpleSerialNo );

			if (FAILED(hresult)) {

				ATLASSERT(FALSE);
				goto process_v1;
			}

			if (memcmp(dibV2.UnitLocation[dibV2.iSequence].MACAddr, macAddr, sizeof(macAddr))) {

				// DIB information is not consistent with this unit's information.

				ATLASSERT(FALSE);
				conflictDib = TRUE;
				goto process_v1;
			}

			if (dibV2.UnitLocation[dibV2.iSequence].VID != vid) {

				// Assume Vid 0 and 1 is same.

				if (!(vid == NDAS_VID_DEFAULT && dibV2.UnitLocation[dibV2.iSequence].VID == NDAS_VID_NONE ||
					  vid == NDAS_VID_NONE && dibV2.UnitLocation[dibV2.iSequence].VID == NDAS_VID_DEFAULT)) {

					ATLASSERT(FALSE);
					conflictDib = TRUE;
					goto process_v1;
				}
			}

			if (memcmp(dibV2.UnitSimpleSerialNo[dibV2.iSequence], ndasSimpleSerialNo, NDAS_DIB_SERIAL_LEN)) {

				UCHAR zeroUnitSimpleSerialNo[NDAS_DIB_SERIAL_LEN] = {0};

				if (memcmp(dibV2.UnitSimpleSerialNo[dibV2.iSequence], zeroUnitSimpleSerialNo, NDAS_DIB_SERIAL_LEN)) {

					// DIB information is not consistent with this unit's information.

					ATLASSERT(FALSE);
					conflictDib = TRUE;
					goto process_v1;
				}
			}
		}

		return S_OK;
	}

process_v1:

	// Check Signature and Version informations in NDAS_DIB_V1 and translate the NDAS_DIB_V1 to an NDAS_DIB_V2

	// initialize DIB V1

	dibSize = sizeof(dibV2); // maximum 2 disks

	// ensure buffer

	if (DibV2 == NULL || *DibSize < dibSize) {

		*DibSize = dibSize; // set size needed
		return E_FAIL;
	}

	if (::IsBadWritePtr(DibSize, sizeof(UINT32))) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER; 		
	}

	ZeroMemory( &dibV2, dibSize );

	if (NdasCommBlockDeviceRead(Hndas, NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&dibV1) == FALSE) {
		
		ATLASSERT(FALSE);
	}

	NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};

	unitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	
	if (NdasCommGetUnitDeviceHardwareInfoW(Hndas, &unitInfo) == FALSE) {

		return GetLastError();
	}

	NdasUiDbgCall( 3, _T("conflictDib = %d, IS_NDAS_DIBV1_WRONG_VERSION(dibV1) = %d, dibV1.DiskType = %d, dibV1.Sequence = %d\n"), 
						 conflictDib, IS_NDAS_DIBV1_WRONG_VERSION(dibV1),
						 dibV1.DiskType, dibV1.Sequence );

	dibV2.Signature			= NDAS_DIB_V2_SIGNATURE;
	dibV2.MajorVersion		= NDAS_DIB_VERSION_MAJOR_V2;
	dibV2.MinorVersion		= NDAS_DIB_VERSION_MINOR_V2;
	dibV2.sizeXArea			= NDAS_BLOCK_SIZE_XAREA;
	dibV2.iSectorsPerBit	= 0; // no backup information
	dibV2.sizeUserSpace		= unitInfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA; // in case of mirror, use primary disk size

	if (conflictDib) {

		// Fill as one disk if DIB info is conflicting.

		dibV2.iMediaType	= NMT_CONFLICT;
		dibV2.iSequence		= 0;
		dibV2.nDiskCount	= 1;
		dibV2.nSpareCount	= 0;

		if (NdasCommGetDeviceID( Hndas, 
								 NULL, 
								 dibV2.UnitLocation[0].MACAddr, 
								 &dibV2.UnitLocation[0].UnitNoObsolete, 
								 &dibV2.UnitLocation[0].VID) == FALSE) {
			
			ATLASSERT(FALSE);
		}

		hresult = GetNdasSimpleSerialNo ( &unitInfo, dibV2.UnitSimpleSerialNo[0] );

	}  else if (IS_NDAS_DIBV1_WRONG_VERSION(dibV1) || // no DIB information
				(NDAS_DIB_DISK_TYPE_MIRROR_MASTER		!= dibV1.DiskType &&
				 NDAS_DIB_DISK_TYPE_MIRROR_SLAVE		!= dibV1.DiskType &&
				 NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST	!= dibV1.DiskType &&
				 NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND	!= dibV1.DiskType)) {

		// Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1 is not acceptable either	

		dibV2.iMediaType	= NMT_SINGLE;
		dibV2.iSequence		= 0;
		dibV2.nDiskCount	= 1;
		dibV2.nSpareCount	= 0;

		// only 1 unit

		if (NdasCommGetDeviceID( Hndas, 
								 NULL, 
								 dibV2.UnitLocation[0].MACAddr, 
								 &dibV2.UnitLocation[0].UnitNoObsolete, 
								 &dibV2.UnitLocation[0].VID) == FALSE) {
			
			ATLASSERT(FALSE);
		}

	} else {

		// pair(2) disks (mirror, aggregation)
		
		UNIT_DISK_LOCATION *UnitDiskLocation0, *UnitDiskLocation1;

		if (NDAS_DIB_DISK_TYPE_MIRROR_MASTER	 == dibV1.DiskType ||
			NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST == dibV1.DiskType) {

			UnitDiskLocation0 = &dibV2.UnitLocation[0];
			UnitDiskLocation1 = &dibV2.UnitLocation[1];
		
		} else {

			UnitDiskLocation0 = &dibV2.UnitLocation[1];
			UnitDiskLocation1 = &dibV2.UnitLocation[0];
		}

		// 1st unit
		
		if (dibV1.EtherAddress[0] == 0x00 &&
			dibV1.EtherAddress[1] == 0x00 &&
			dibV1.EtherAddress[2] == 0x00 &&
			dibV1.EtherAddress[3] == 0x00 &&
			dibV1.EtherAddress[4] == 0x00 &&
			dibV1.EtherAddress[5] == 0x00) {

			// usually, there is no ether net address information

			if (NdasCommGetDeviceID( Hndas, 
									 NULL, 
									 UnitDiskLocation0->MACAddr, 
									 &UnitDiskLocation0->UnitNoObsolete, 
									 &UnitDiskLocation0->VID) == FALSE) {
			
				ATLASSERT(FALSE);
			}

		} else {

			// but, if there is.
	
			ATLASSERT(FALSE);

			CopyMemory( &UnitDiskLocation0->MACAddr, dibV1.EtherAddress, sizeof(UnitDiskLocation0->MACAddr) );
			UnitDiskLocation0->UnitNoObsolete = dibV1.UnitNumber;
			UnitDiskLocation0->VID = NDAS_VID_DEFAULT;
		}

		// 2nd unit

		CopyMemory( UnitDiskLocation1->MACAddr, dibV1.PeerAddress, sizeof(UnitDiskLocation1->MACAddr) );

		UnitDiskLocation1->UnitNoObsolete = dibV1.PeerUnitNumber;
		UnitDiskLocation1->VID = NDAS_VID_DEFAULT;

		ATLASSERT( UnitDiskLocation0->VID == NDAS_VID_DEFAULT && UnitDiskLocation1->VID == NDAS_VID_DEFAULT );

		dibV2.nDiskCount	= 2;
		dibV2.nSpareCount	= 0;

		NdasUiDbgCall( 3, _T("UnitDiskLocation0->MACAddr(%02X:%02X:%02X:%02X:%02X:%02X), dibV1.DiskType = %d, dibV1.Sequence = %d\n"), 
							 UnitDiskLocation0->MACAddr[0], UnitDiskLocation0->MACAddr[1], 	UnitDiskLocation0->MACAddr[2],							  
							 UnitDiskLocation0->MACAddr[3], UnitDiskLocation0->MACAddr[4], 	UnitDiskLocation0->MACAddr[5],							  
							 dibV1.DiskType, dibV1.Sequence );

		NdasUiDbgCall( 3, _T("UnitDiskLocation1->MACAddr(%02X:%02X:%02X:%02X:%02X:%02X), dibV1.DiskType = %d, dibV1.Sequence = %d\n"), 
							 UnitDiskLocation1->MACAddr[0], UnitDiskLocation1->MACAddr[1], 	UnitDiskLocation1->MACAddr[2],							  
							 UnitDiskLocation1->MACAddr[3], UnitDiskLocation1->MACAddr[4], 	UnitDiskLocation1->MACAddr[5],							  
							 dibV1.DiskType, dibV1.Sequence );

		NdasUiDbgCall( 3, _T("dibV1.EtherAddress(%02X:%02X:%02X:%02X:%02X:%02X), dibV1.DiskType = %d, dibV1.Sequence = %d\n"), 
							 dibV1.EtherAddress[0], dibV1.EtherAddress[1], 	dibV1.EtherAddress[2],							  
							 dibV1.EtherAddress[3], dibV1.EtherAddress[4], 	dibV1.EtherAddress[5],							  
							 dibV1.DiskType, dibV1.Sequence );

		switch (dibV1.DiskType) {

		case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:

			dibV2.iMediaType = NMT_MIRROR;
			dibV2.iSequence	 = 0;

			break;

		case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:

			dibV2.iMediaType = NMT_MIRROR;
			dibV2.iSequence  = 1;

			break;

		case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:

			dibV2.iMediaType = NMT_AGGREGATE;
			dibV2.iSequence  = 0;
			
			break;

		case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:

			dibV2.iMediaType = NMT_AGGREGATE;
			dibV2.iSequence  = 1;

			break;

		default:

			// must not jump to here

			ATLASSERT(FALSE);

			return NDASOP_ERROR_DEVICE_UNSUPPORTED; 		
		}
	}

	SET_DIB_CRC( crc32_calc, dibV2 );

	NdasUiDbgCall( 3, _T("dibV2->iMediaType = %d, dibV2.iSequence = %d\n"), dibV2.iMediaType, dibV2.iSequence );

	*DibSize = dibSize;
	memcpy( DibV2, &dibV2, dibSize );

	return S_OK;
}

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpBind (
	PNDASCOMM_CONNECTION_INFO ConnectionInfo,
	UINT32					  DiskCount,
	NDAS_MEDIA_TYPE			  BindType,
	UINT32					  UserSpace,
	UINT32					  TargetNidx
	)
{
	HRESULT hresult;

	UINT32	nidx = 0xFFFFFFFF;
	UINT32	ridx;

	UINT32	dibSize;
	
	UINT8	targetRidx;

	NDAS_DIB_V2						*bindDibV2		= NULL;
	NDAS_RAID_META_DATA				*bindRmd		= NULL;
	LARGE_INTEGER					*sectorCount	= NULL;
	HNDAS							*hndas			= NULL;
	NDAS_DEVICE_HARDWARE_INFO		*hardwareInfo	= NULL;
	NDAS_UNITDEVICE_HARDWARE_INFOW	*unitInfo		= NULL;

	NDASCOMM_CONNECTION_INFO		connectionInfoDiscover;
	

	ATLASSERT( DiskCount <= NDAS_MAX_UNITS_IN_V2 );

	if (::IsBadReadPtr(ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * DiskCount)) {

		ATLASSERT(FALSE);

		return NDASOP_ERROR_INVALID_PARAMETER; 		
	}

	switch (BindType) {

	case NMT_SINGLE:
	case NMT_AGGREGATE:
	case NMT_SAFE_AGGREGATE:
	case NMT_RAID0:
	case NMT_SAFE_RAID1:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_RAID5:
	case NMT_SAFE_RAID_ADD:
	case NMT_SAFE_RAID_REMOVE:
	case NMT_SAFE_RAID_REPLACE:
	case NMT_SAFE_RAID_CLEAR_DEFECT:

		break;

	default:

		ATLASSERT(FALSE);

		return NDASOP_ERROR_BIND_UNSUPPORTED; 		
	}

	if (BindType != NMT_SAFE_RAID_REMOVE && BindType != NMT_SAFE_RAID_REPLACE && BindType != NMT_SAFE_RAID_CLEAR_DEFECT) {

		hresult = NdasOpVerifyDiskCount(BindType, DiskCount);
			
		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			goto out;
		}
	}

	bindDibV2 = (NDAS_DIB_V2 *)::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, DiskCount * sizeof(NDAS_DIB_V2) );

	if (bindDibV2 == NULL) {

		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY; 		
		goto out;
	}

	bindRmd = (NDAS_RAID_META_DATA *)::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, DiskCount * sizeof(NDAS_RAID_META_DATA) );

	if (bindRmd == NULL) {

		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY; 		
		goto out;
	}

	sectorCount = (LARGE_INTEGER *)::HeapAlloc( ::GetProcessHeap(), 
												 HEAP_ZERO_MEMORY, 
												 DiskCount * sizeof(LARGE_INTEGER) );

	if (sectorCount == NULL) {

		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY; 		
		goto out;
	}

	hndas = (HNDAS *)::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, DiskCount * sizeof(HNDAS) );

	if (hndas == NULL) {

		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY; 		
		goto out;
	}

	hardwareInfo = (NDAS_DEVICE_HARDWARE_INFO *)::HeapAlloc( ::GetProcessHeap(), 
															  HEAP_ZERO_MEMORY, 
															  DiskCount * sizeof(NDAS_DEVICE_HARDWARE_INFO) );

	if (hardwareInfo == NULL) {

		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY; 		
		goto out;
	}

	unitInfo = (NDAS_UNITDEVICE_HARDWARE_INFOW *)::HeapAlloc( ::GetProcessHeap(), 
															  HEAP_ZERO_MEMORY, 
															  DiskCount * sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW) );

	if (unitInfo == NULL) {

		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY; 		
		goto out;
	}

	// gather information & initialize DIBs

	SetLastError(0);

	for (nidx=0; nidx<DiskCount; nidx++) {

		// connect to the NDAS Device

		hndas[nidx] = NdasCommConnect(&ConnectionInfo[nidx]);

		if (hndas[nidx] == NULL) {

			NdasUiDbgCall( 4, _T("GetLastError() = %x, NDASCOMM_ERROR_RW_USER_EXIST = %x nidx = %d\n"), 
					  GetLastError(), NDASCOMM_ERROR_RW_USER_EXIST, nidx );

			if (BindType == NMT_SINGLE)	{

				continue;
			}

			if (BindType == NMT_SAFE_RAID_REPLACE && nidx == TargetNidx) {

				continue;
			}

			if (BindType == NMT_SAFE_RAID_REMOVE /*&& (nidx == TargetNidx || TargetNidx == DiskCount-1)*/) { // Check later

				continue;
			}

			hresult = GetLastError();
			goto out;
		}

		NdasUiDbgCall( 4, _T("GetLastError() = %x, NDASCOMM_ERROR_RW_USER_EXIST = %x\n"), 
					 GetLastError(), NDASCOMM_ERROR_RW_USER_EXIST );

		if (GetLastError() == NDASCOMM_ERROR_RW_USER_EXIST) {

			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}

		// discover the NDAS Device so that stop process if any connected user exists.
		// V2.0 does not report RO count correctly

		RtlCopyMemory( &connectionInfoDiscover, &ConnectionInfo[nidx], sizeof(connectionInfoDiscover) );
	
		connectionInfoDiscover.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
		
		NDAS_UNITDEVICE_STAT udstat = {0};

		udstat.Size = sizeof(NDAS_UNITDEVICE_STAT);

		if (!NdasCommGetUnitDeviceStat(&connectionInfoDiscover, &udstat)) {

			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}

		if (udstat.RwConnectionCount != 1 || !(udstat.RoConnectionCount == 0 || udstat.RoConnectionCount == NDAS_HOST_COUNT_UNKNOWN)) {

			ATLASSERT(FALSE);

			hresult = NDASOP_ERROR_ALREADY_USED;
			goto out;
		}
	}

	if (BindType == NMT_SINGLE) {

		for (nidx=0; nidx<DiskCount; nidx++) {

			if (hndas[nidx] == NULL) {

				continue;
			}

			hresult = NdasOpReadDib( hndas[nidx], &bindDibV2[nidx], &dibSize );

			if (FAILED(hresult)) {

				ATLASSERT(FALSE);
				continue;
			}

			// clear X Area

			hresult = NdasOpClearXArea(hndas[nidx]);

			if (FAILED(hresult)) {

				ATLASSERT(FALSE);
			}

			// clear MBR

			if (bindDibV2[nidx].iMediaType == NMT_MIRROR	||
				bindDibV2[nidx].iMediaType == NMT_RAID1		||				
				bindDibV2[nidx].iMediaType == NMT_RAID1R2	||	
				bindDibV2[nidx].iMediaType == NMT_RAID1R3	||
				bindDibV2[nidx].nDiskCount == 2) {

				continue;
			}

			hresult = NdasOpClearMbr(hndas[nidx]);

			if (FAILED(hresult)) {

				ATLASSERT(FALSE);
				continue;
			}
		}

		goto out;
	}

	ATLASSERT( BindType == NMT_AGGREGATE || BindType == NMT_SAFE_AGGREGATE || BindType == NMT_RAID0	||
			   BindType == NMT_RAID1R3   || BindType == NMT_SAFE_RAID1     || 
			   BindType == NMT_RAID4R3	 || BindType == NMT_RAID5		   || 
			   BindType == NMT_SAFE_RAID_ADD	 || BindType == NMT_SAFE_RAID_REMOVE || 
			   BindType == NMT_SAFE_RAID_REPLACE || BindType == NMT_SAFE_RAID_CLEAR_DEFECT );

	for (nidx=0; nidx<DiskCount; nidx++) {

		if (hndas[nidx] == NULL) {
			
			ATLASSERT( nidx == TargetNidx && BindType == NMT_SAFE_RAID_REPLACE || BindType == NMT_SAFE_RAID_REMOVE );
			continue;
		}

		// creating bind with non-lockable device(v1.0) is not supported

		hardwareInfo[nidx].Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
		
		NdasCommGetDeviceHardwareInfo( hndas[nidx], &hardwareInfo[nidx] );

		if (hardwareInfo[nidx].HardwareVersion == 0) {

			ATLASSERT(FALSE);

			hresult = NDASOP_ERROR_DEVICE_1_0_NOT_SUPPORTED; 		
			goto out;
		}

		// fail if the NDAS Unit Device is not a single disk

		dibSize = sizeof(bindDibV2[nidx]);

		hresult = NdasOpReadDib( hndas[nidx], &bindDibV2[nidx], &dibSize );
		
		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			goto out;
		}

		if (BindType == NMT_SAFE_RAID_ADD) {

			if (nidx < DiskCount - 1) {

				if (bindDibV2[nidx].iMediaType == NMT_RAID1R3 ||
					bindDibV2[nidx].iMediaType == NMT_RAID4R3 ||
					bindDibV2[nidx].iMediaType == NMT_RAID5) {

					continue;
				}

				ATLASSERT(FALSE);
				hresult = NDASOP_ERROR_INVALID_PARAMETER;
				goto out;
			}
		}

		if (BindType == NMT_SAFE_RAID_REMOVE		||
			BindType == NMT_SAFE_RAID_CLEAR_DEFECT	||
			BindType == NMT_SAFE_RAID_REPLACE && nidx < DiskCount-1) {

			if (bindDibV2[nidx].iMediaType == NMT_RAID1R3 ||
				bindDibV2[nidx].iMediaType == NMT_RAID4R3 ||
				bindDibV2[nidx].iMediaType == NMT_RAID5) {

				continue;
			}

			ATLASSERT(FALSE);
			hresult = NDASOP_ERROR_INVALID_PARAMETER;
			goto out;
		}

		if (BindType == NMT_SAFE_AGGREGATE) {

			if (DiskCount > 2 && nidx < DiskCount - 1) {

				if (bindDibV2[nidx].iMediaType == NMT_AGGREGATE) {

					continue;
				}

				ATLASSERT(FALSE);
				hresult = NDASOP_ERROR_INVALID_PARAMETER;
				goto out;
			}
		}

		if (BindType == NMT_SAFE_RAID1) {

			if (bindDibV2[nidx].iMediaType == NMT_MIRROR) {
			
				ATLASSERT( DiskCount == 2 );
				continue;
			}
		}

		if (bindDibV2[nidx].iMediaType != NMT_SINGLE) {

			ATLASSERT(FALSE);
			hresult = NDASOP_ERROR_INVALID_PARAMETER;
			goto out;
		}
	}

	for (nidx=0; nidx<DiskCount; nidx++) {

		if (hndas[nidx] == NULL) {

			ATLASSERT( nidx == TargetNidx && BindType == NMT_SAFE_RAID_REPLACE || BindType == NMT_SAFE_RAID_REMOVE );
			continue;
		}

		unitInfo[nidx].Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);

		if (NdasCommGetUnitDeviceHardwareInfoW(hndas[nidx], &unitInfo[nidx]) == FALSE) {

			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}
	}

	NDAS_DIB	dibV1;
	NDAS_DIB_V2 dibV2;

	ZeroMemory( &dibV1, sizeof(NDAS_DIB) );

	if (BindType != NMT_SINGLE) {

		NDAS_DIB_V1_INVALIDATE(dibV1);
	}

	NDAS_RAID_META_DATA	rmd;

	if (BindType == NMT_SAFE_RAID_ADD) { // add spare

		if (bindDibV2[DiskCount-1].sizeUserSpace < bindDibV2[0].sizeUserSpace) {

			ATLASSERT(FALSE);

			hresult = NDASOP_ERROR_NOT_ENOUGH_CAPACITY; 		
			goto out;
		}

		for (nidx=0; nidx<DiskCount-1; nidx++) {

			if (NdasCommBlockDeviceRead( hndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&bindRmd[nidx]) == FALSE) {

				ATLASSERT(FALSE);
				hresult = GetLastError();
				goto out;
			}
		}

		UINT uptodateNidx = 0;

		for (nidx=uptodateNidx+1; nidx<DiskCount-1; nidx++) {

			if (bindRmd[nidx].uiUSN > bindRmd[uptodateNidx].uiUSN) {

				uptodateNidx = nidx;
			}
		}

		CopyMemory( &dibV2, &bindDibV2[uptodateNidx], sizeof(dibV2) );
		CopyMemory( &rmd, &bindRmd[uptodateNidx], sizeof(rmd) );
	
		if (NdasCommGetDeviceID( hndas[DiskCount-1], 
								 NULL, 
								 dibV2.UnitLocation[DiskCount-1].MACAddr, 
								 &dibV2.UnitLocation[DiskCount-1].UnitNoObsolete, 
								 &dibV2.UnitLocation[DiskCount-1].VID) == FALSE) {
			
			hresult = GetLastError();

			ATLASSERT(FALSE);
			goto out;
		}

		hresult = GetNdasSimpleSerialNo( &unitInfo[DiskCount-1], dibV2.UnitSimpleSerialNo[DiskCount-1] );
	
		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			goto out;
		}

		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = dibV2.sizeUserSpace;
		}

		dibV2.nSpareCount++;

		dibV2.UnitDiskInfos[DiskCount-1].HwVersion = hardwareInfo[DiskCount-1].HardwareVersion;

		rmd.uiUSN ++;

		rmd.UnitMetaData[DiskCount-1].Nidx = (UINT8) DiskCount-1;
		rmd.UnitMetaData[DiskCount-1].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_SPARE;

		goto copyMetaData;
	}

	if (BindType == NMT_SAFE_RAID_REMOVE) {

		for (nidx=0; nidx<DiskCount; nidx++) {

			if (hndas[nidx] == NULL) {

				continue;
			}

			if (NdasCommBlockDeviceRead(hndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&bindRmd[nidx]) == FALSE) {

				ATLASSERT(FALSE);
				hresult = GetLastError();
				goto out;
			}
		}

		UINT uptodateNidx = 0;

		for (nidx=uptodateNidx+1; nidx<DiskCount; nidx++) {

			if (hndas[nidx] == NULL) {

				continue;
			}

			if (bindRmd[nidx].uiUSN > bindRmd[uptodateNidx].uiUSN) {

				uptodateNidx = nidx;
			}
		}

		CopyMemory( &dibV2, &bindDibV2[uptodateNidx], sizeof(dibV2) );
		CopyMemory( &rmd, &bindRmd[uptodateNidx], sizeof(rmd) );

		for (ridx=0; ridx<DiskCount; ridx++) {

			if (hndas[rmd.UnitMetaData[ridx].Nidx] == NULL) {

				if (rmd.UnitMetaData[ridx].Nidx != TargetNidx && 
					(dibV2.nSpareCount == 0 || rmd.UnitMetaData[dibV2.nDiskCount].Nidx != TargetNidx)) {
						
					ATLASSERT(FALSE);

					hresult = NDASOP_ERROR_INVALID_PARAMETER;
					goto out;
				}

				continue;
			}
		}

		if (hndas[TargetNidx]) { // don't write to removed index

			ATLVERIFY( NdasCommDisconnect(hndas[TargetNidx]) == TRUE );
			hndas[TargetNidx] = NULL;
		}

		for (targetRidx=0; targetRidx < dibV2.nDiskCount+dibV2.nSpareCount; targetRidx++) {

			if (TargetNidx == rmd.UnitMetaData[targetRidx].Nidx) {

				break;
			}
		}

		if (dibV2.nSpareCount == 0) {

			ZeroMemory( &dibV2.UnitLocation[TargetNidx], sizeof(UNIT_DISK_LOCATION) );
			dibV2.UnitLocation[TargetNidx].VID = NDAS_VID_NONE;

			ZeroMemory( &dibV2.UnitSimpleSerialNo[TargetNidx], NDAS_DIB_SERIAL_LEN );

			rmd.UnitMetaData[targetRidx].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_OFFLINE;			

		} else {
			
			ATLASSERT( targetRidx < dibV2.nDiskCount+dibV2.nSpareCount );

			if (targetRidx == dibV2.nDiskCount) { // remove spare

				NdasUiDbgCall( 4, _T("remove spare\n") );

				ATLASSERT( TargetNidx == rmd.UnitMetaData[dibV2.nDiskCount].Nidx ); 

				for (UINT8 nidx2=TargetNidx+1; nidx2 < dibV2.nDiskCount+dibV2.nSpareCount; nidx2++) {

					CopyMemory( &dibV2.UnitDiskInfos[nidx2-1], &dibV2.UnitDiskInfos[nidx2], sizeof(UNIT_DISK_INFO) );
					CopyMemory( &dibV2.UnitLocation[nidx2-1], &dibV2.UnitLocation[nidx2], sizeof(UNIT_DISK_LOCATION) );
					CopyMemory( &dibV2.UnitSimpleSerialNo[nidx2-1], &dibV2.UnitSimpleSerialNo[nidx2], NDAS_DIB_SERIAL_LEN );

					hndas[nidx2-1] = hndas[nidx2];
				}

				for (ridx=0; ridx < dibV2.nDiskCount; ridx++) {

					if (rmd.UnitMetaData[ridx].Nidx > TargetNidx) {
					
						rmd.UnitMetaData[ridx].Nidx--;
					}
				}

				ZeroMemory( &dibV2.UnitDiskInfos[dibV2.nDiskCount], sizeof(UNIT_DISK_INFO) );
				ZeroMemory( &dibV2.UnitLocation[dibV2.nDiskCount], sizeof(UNIT_DISK_LOCATION) );
				ZeroMemory( &dibV2.UnitSimpleSerialNo[dibV2.nDiskCount], NDAS_DIB_SERIAL_LEN );
				ZeroMemory( &rmd.UnitMetaData[dibV2.nDiskCount], sizeof(NDAS_UNIT_META_DATA) );

				hndas[dibV2.nDiskCount] = NULL;

			} else { // replace removed disk with spare

				NdasUiDbgCall( 4, _T("replace removed disk with spare\n") );

				CopyMemory( &dibV2.UnitDiskInfos[TargetNidx], 
							&dibV2.UnitDiskInfos[rmd.UnitMetaData[dibV2.nDiskCount].Nidx], 
							sizeof(UNIT_DISK_INFO) );

				CopyMemory( &dibV2.UnitLocation[TargetNidx], 
							&dibV2.UnitLocation[rmd.UnitMetaData[dibV2.nDiskCount].Nidx], 
							sizeof(UNIT_DISK_LOCATION) );

				CopyMemory( &dibV2.UnitSimpleSerialNo[TargetNidx], 
							&dibV2.UnitSimpleSerialNo[rmd.UnitMetaData[dibV2.nDiskCount].Nidx], 
							NDAS_DIB_SERIAL_LEN );

				ZeroMemory( &rmd.UnitMetaData[targetRidx], sizeof(NDAS_UNIT_META_DATA) );

				rmd.UnitMetaData[targetRidx].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
				rmd.UnitMetaData[targetRidx].Nidx = (UINT8)TargetNidx;

				ZeroMemory( &dibV2.UnitDiskInfos[rmd.UnitMetaData[dibV2.nDiskCount].Nidx], sizeof(UNIT_DISK_INFO) );
				ZeroMemory( &dibV2.UnitLocation[rmd.UnitMetaData[dibV2.nDiskCount].Nidx], sizeof(UNIT_DISK_LOCATION) );
				ZeroMemory( &dibV2.UnitSimpleSerialNo[rmd.UnitMetaData[dibV2.nDiskCount].Nidx], NDAS_DIB_SERIAL_LEN );

				ZeroMemory( &rmd.UnitMetaData[dibV2.nDiskCount], sizeof(NDAS_UNIT_META_DATA) );

				hndas[TargetNidx]	= hndas[DiskCount-1];
				hndas[DiskCount-1]	= NULL;
			}
		}
	
		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = dibV2.sizeUserSpace;
		}

		dibV2.nSpareCount = 0;

		rmd.uiUSN ++;

		goto copyMetaData;
	}

	if (BindType == NMT_SAFE_RAID_REPLACE) {

		for (nidx=0; nidx<DiskCount-1; nidx++) {

			if (hndas[nidx] == NULL) {

				ATLASSERT( nidx == TargetNidx );
				continue;
			}

			if (NdasCommBlockDeviceRead(hndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&bindRmd[nidx]) == FALSE) {

				ATLASSERT(FALSE);
				hresult = GetLastError();
				goto out;
			}
		}

		UINT uptodateNidx = 0;

		for (nidx=uptodateNidx+1; nidx<DiskCount- 1; nidx++) {

			if (hndas[nidx] == NULL) {

				continue;
			}

			if (bindRmd[nidx].uiUSN > bindRmd[uptodateNidx].uiUSN) {

				uptodateNidx = nidx;
			}
		}

		if (bindDibV2[DiskCount-1].sizeUserSpace < bindDibV2[uptodateNidx].sizeUserSpace) {

			ATLASSERT(FALSE);

			hresult = NDASOP_ERROR_NOT_ENOUGH_CAPACITY;
			goto out;
		}

		CopyMemory( &dibV2, &bindDibV2[uptodateNidx], sizeof(dibV2) );
		CopyMemory( &rmd, &bindRmd[uptodateNidx], sizeof(rmd) );

		if (hndas[TargetNidx]) { // don't write to removed index

			ATLVERIFY( NdasCommDisconnect(hndas[TargetNidx]) == TRUE );
			hndas[TargetNidx] = NULL;
		}

		ZeroMemory( &dibV2.UnitDiskInfos[TargetNidx], sizeof(UNIT_DISK_INFO) );
		ZeroMemory( &dibV2.UnitLocation[TargetNidx], sizeof(UNIT_DISK_LOCATION) );
		ZeroMemory( &dibV2.UnitSimpleSerialNo[TargetNidx], NDAS_DIB_SERIAL_LEN );

		if (NdasCommGetDeviceID( hndas[DiskCount-1], 
								 NULL, 
								 dibV2.UnitLocation[TargetNidx].MACAddr, 
								 &dibV2.UnitLocation[TargetNidx].UnitNoObsolete, 
								 &dibV2.UnitLocation[TargetNidx].VID) == FALSE) {
			
			hresult = GetLastError();

			ATLASSERT(FALSE);
			goto out;
		}

		hresult = GetNdasSimpleSerialNo ( &unitInfo[DiskCount-1], dibV2.UnitSimpleSerialNo[TargetNidx] );

		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			goto out;
		}

		dibV2.UnitDiskInfos[TargetNidx].HwVersion = hardwareInfo[DiskCount-1].HardwareVersion;

		UINT32 spareNidx = rmd.UnitMetaData[dibV2.nDiskCount].Nidx;

		for (targetRidx=0; targetRidx < dibV2.nDiskCount+dibV2.nSpareCount; targetRidx++) {

			if (TargetNidx == rmd.UnitMetaData[targetRidx].Nidx) {

				break;
			}
		}

		if (targetRidx == dibV2.nDiskCount) { // replace spare

			ATLASSERT( TargetNidx == spareNidx );
			rmd.UnitMetaData[targetRidx].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_SPARE;
		
		} else {

			rmd.UnitMetaData[targetRidx].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		}

		hndas[TargetNidx]	= hndas[DiskCount-1];
		hndas[DiskCount-1]	= NULL;
	
		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = dibV2.sizeUserSpace;
		}

		rmd.uiUSN ++;

		goto copyMetaData;
	}

	if (BindType == NMT_SAFE_RAID_CLEAR_DEFECT) {

		for (nidx=0; nidx<DiskCount; nidx++) {

			if (NdasCommBlockDeviceRead(hndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&bindRmd[nidx]) == FALSE) {
				
				ATLASSERT(FALSE);
				hresult = GetLastError();
				goto out;
			}
		}

		UINT uptodateNidx = 0;

		for (nidx=uptodateNidx+1; nidx<DiskCount- 1; nidx++) {

			if (bindRmd[nidx].uiUSN > bindRmd[uptodateNidx].uiUSN) {

				uptodateNidx = nidx;
			}
		}

		if (bindDibV2[DiskCount-1].sizeUserSpace < bindDibV2[uptodateNidx].sizeUserSpace) {

			ATLASSERT(FALSE);

			hresult = NDASOP_ERROR_NOT_ENOUGH_CAPACITY; 		
			goto out;
		}

		CopyMemory( &dibV2, &bindDibV2[uptodateNidx], sizeof(dibV2) );
		CopyMemory( &rmd, &bindRmd[uptodateNidx], sizeof(rmd) );

		for (targetRidx=0; targetRidx < dibV2.nDiskCount+dibV2.nSpareCount; targetRidx++) {

			if (TargetNidx == rmd.UnitMetaData[targetRidx].Nidx) {

				break;
			}
		}

		ATLASSERT( rmd.UnitMetaData[targetRidx].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_DEFECTIVE );

		rmd.UnitMetaData[targetRidx].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_DEFECTIVE;

		if (rmd.UnitMetaData[targetRidx].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE) {

			ATLASSERT(FALSE);
			goto out;

			rmd.UnitMetaData[targetRidx].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_SPARE;

		} else {

			rmd.UnitMetaData[targetRidx].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		}
			
		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = dibV2.sizeUserSpace;
		}

		rmd.uiUSN ++;

		goto copyMetaData;
	}

	ZeroMemory( &dibV2, sizeof(NDAS_DIB_V2) );

	switch (BindType) {

	case NMT_AGGREGATE:
			
		dibV2.sizeStartOffset = NDAS_BLOCK_LOCATION_USER;
	
		// just % 128 of free space

		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = unitInfo[nidx].SectorCount.QuadPart;

			if (UserSpace) {

				sectorCount[nidx].QuadPart = UserSpace;
			}

			sectorCount[nidx].QuadPart -= NDAS_BLOCK_SIZE_XAREA;
			sectorCount[nidx].QuadPart -= (1024 * 1024 * 1024 >> 9); // Margin 1Gbytes
		}

		break;

	case NMT_SAFE_AGGREGATE:

		for (nidx=0; nidx<DiskCount-1; nidx++) {

			sectorCount[nidx].QuadPart = bindDibV2[nidx].sizeUserSpace;
		}

		sectorCount[DiskCount-1].QuadPart = unitInfo[DiskCount-1].SectorCount.QuadPart;

		if (UserSpace) {

			sectorCount[DiskCount-1].QuadPart = UserSpace;
		}

		sectorCount[DiskCount-1].QuadPart -= NDAS_BLOCK_SIZE_XAREA;
		sectorCount[DiskCount-1].QuadPart -= (1024 * 1024 * 1024 >> 9); // Margin 1Gbytes

		break;

	case NMT_RAID0: {

		dibV2.sizeStartOffset = NDAS_BLOCK_LOCATION_USER;

		UINT minNidx = 0;

		for (nidx=1; nidx<DiskCount; nidx++) {

			if (unitInfo[nidx].SectorCount.QuadPart < unitInfo[minNidx].SectorCount.QuadPart) {

				minNidx = nidx;
			}
		}

		sectorCount[minNidx].QuadPart = unitInfo[minNidx].SectorCount.QuadPart;

		if (UserSpace) {

			sectorCount[minNidx].QuadPart = UserSpace;
		}

		sectorCount[minNidx].QuadPart -= NDAS_BLOCK_SIZE_XAREA;
		sectorCount[minNidx].QuadPart -= (1024 * 1024 * 1024 >> 9); // Margin 1Gbytes

		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = sectorCount[minNidx].QuadPart;
		}

		break;
	}

	case NMT_SAFE_RAID1:

		if (bindDibV2[1].sizeUserSpace < bindDibV2[0].sizeUserSpace) {

			ATLASSERT(FALSE);

			::SetLastError(NDASOP_ERROR_NOT_ENOUGH_CAPACITY); 		
			goto out;
		}

		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = bindDibV2[0].sizeUserSpace;
		}

		dibV2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;

		break;

	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_RAID5: {

		if (BindType == NMT_RAID1R3) {

			dibV2.sizeStartOffset = 0;
		
		} else {

			dibV2.sizeStartOffset = NDAS_BLOCK_LOCATION_USER;
		}

		UINT minNidx = 0;

		for (nidx=1; nidx<DiskCount; nidx++) {

			if (unitInfo[nidx].SectorCount.QuadPart < unitInfo[minNidx].SectorCount.QuadPart) {

				minNidx = nidx;
			}
		}

		sectorCount[minNidx].QuadPart = unitInfo[minNidx].SectorCount.QuadPart;

		if (UserSpace) {

			sectorCount[minNidx].QuadPart = UserSpace;
		}

		sectorCount[minNidx].QuadPart -= NDAS_BLOCK_SIZE_XAREA;

		//sectorCount[minNidx].QuadPart -= (1024 * 1024 * 1024 >> 9); // Margin 1Gbytes
	
		// Reduce user space by 0.5%. HDDs with same giga size labels have different sizes.
		// Sometimes, it is up to 7.5% difference due to 1k !=1000.
		// Once, I found Maxter 160G HDD size - Samsung 160G HDD size = 4G. 
		// Even with same maker's HDD with same gig has different sector size.
		// To do: Give user a option to select this margin.
										
		sectorCount[minNidx].QuadPart = sectorCount[minNidx].QuadPart * 199 / 200;

		// Increase sectors per bit if user space is larger than default maximum.
					
		for (INT j = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT_LOG; TRUE; j++) {

			if (sectorCount[minNidx].QuadPart <= 
				(((INT64)1<<j)) * NDAS_BLOCK_SIZE_BITMAP * NDAS_BIT_PER_OOS_BITMAP_BLOCK) { // 512 GB : 128 SPB

				// Sector per bit is big enough to be covered by bitmap

				dibV2.iSectorsPerBit = 1 << j;
		
				break;
			}

			ATLASSERT(j <= 32); // protect overflow
		}

		// Trim user space that is out of bitmap align.
					
		sectorCount[minNidx].QuadPart -= sectorCount[minNidx].QuadPart % dibV2.iSectorsPerBit;

		for (nidx=0; nidx<DiskCount; nidx++) {

			sectorCount[nidx].QuadPart = sectorCount[minNidx].QuadPart;
		}

		break;
	}

	default:

		ATLASSERT(FALSE);
	}

	// DIB_V2 information

	dibV2.Signature		= NDAS_DIB_V2_SIGNATURE;
	dibV2.MajorVersion	= NDAS_DIB_VERSION_MAJOR_V2;
	dibV2.MinorVersion	= NDAS_DIB_VERSION_MINOR_V2;
	dibV2.sizeXArea		= NDAS_BLOCK_SIZE_XAREA; // 2MB

	dibV2.iMediaType	= (BindType == NMT_SAFE_RAID1) ? NMT_RAID1R3 :
						  (BindType == NMT_SAFE_AGGREGATE) ? NMT_AGGREGATE :
						  BindType;

	dibV2.nDiskCount	= DiskCount;
	dibV2.nSpareCount	= 0;

	for (nidx=0; nidx<DiskCount; nidx++) {

		CHAR	serialNumber[20+4] = {0};
		
		if (NdasCommGetDeviceID( hndas[nidx], 
								 NULL, 
								 dibV2.UnitLocation[nidx].MACAddr, 
								 &dibV2.UnitLocation[nidx].UnitNoObsolete, 
								 &dibV2.UnitLocation[nidx].VID) == FALSE) {
			
			hresult = GetLastError();

			ATLASSERT(FALSE);
			goto out;
		}

		hresult = GetNdasSimpleSerialNo( &unitInfo[nidx], dibV2.UnitSimpleSerialNo[nidx] );

		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			goto out;
		}

		dibV2.UnitDiskInfos[nidx].HwVersion = hardwareInfo[nidx].HardwareVersion;
	}

	for (nidx=0; nidx<DiskCount; nidx++) {

		for (UINT32 nidx2=nidx+1; nidx2<DiskCount; nidx2++) {

			if (memcmp(dibV2.UnitSimpleSerialNo[nidx], dibV2.UnitSimpleSerialNo[nidx2], NDAS_DIB_SERIAL_LEN) == 0) {

				ATLASSERT(FALSE);
				hresult = NDASOP_ERROR_INVALID_PARAMETER;

				goto out;
			}
		}
	}

	ZeroMemory( &rmd, sizeof(NDAS_RAID_META_DATA) );

	rmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;

	::CoCreateGuid( &rmd.RaidSetId );
	::CoCreateGuid( &rmd.ConfigSetId );	

	rmd.uiUSN = 1; // initial value

	for (nidx=0; nidx<DiskCount; nidx++) {

		rmd.UnitMetaData[nidx].Nidx	 = (UINT8) nidx;

		if (BindType == NMT_SAFE_RAID1) {

			if (nidx==1) {

				rmd.UnitMetaData[nidx].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
			}
		}
	}

copyMetaData:

	SET_RMD_CRC( crc32_calc, rmd );

	for (nidx=0; nidx<DiskCount; nidx++) {

		if (hndas[nidx] == NULL) {

			ATLASSERT( BindType == NMT_SAFE_RAID_REMOVE || 
					   BindType == NMT_SAFE_RAID_REPLACE && nidx == DiskCount-1 );
			continue;
		}

		NdasUiDbgCall( 4, _T("write disk nidx=%d\n"), nidx );

		BOOL clearMbr;
		
		clearMbr = FALSE;
		
		if (BindType == NMT_AGGREGATE									||
			BindType == NMT_RAID0										||
			BindType == NMT_RAID1R3										||
			BindType == NMT_SAFE_RAID1		&& (nidx == DiskCount-1)	||
			BindType == NMT_RAID4R3										||
			BindType == NMT_RAID5										||
			BindType == NMT_SAFE_RAID_ADD	&& (nidx == DiskCount-1)) {
				
			clearMbr = TRUE;
		}

		if (clearMbr) {

			hresult = NdasOpClearMbr(hndas[nidx]);

			if (FAILED(hresult)) {
				
				ATLASSERT(FALSE);
				goto out;
			}

			// clear X Area

			hresult = NdasOpClearXArea(hndas[nidx]);

			if (FAILED(hresult)) {

				ATLASSERT(FALSE);
				goto out;
			}
		}

		dibV2.iSequence = nidx;
		dibV2.sizeUserSpace = sectorCount[nidx].QuadPart;

		// set CRC32

		SET_DIB_CRC( crc32_calc, dibV2 );

		// write DIB_V1, dibV2, RMD(ignore RAID type)
		
		if (NdasCommBlockDeviceWriteSafeBuffer( hndas[nidx], NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&dibV1) == FALSE) {

			hresult = GetLastError();
			ATLASSERT(FALSE);
			goto out;
		}
		
		if (NdasCommBlockDeviceWriteSafeBuffer(hndas[nidx], NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&dibV2) == FALSE) {

			hresult = GetLastError();
			ATLASSERT(FALSE);
			goto out;
		}

		if (NdasCommBlockDeviceWriteSafeBuffer(hndas[nidx], NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)&rmd) == FALSE) {

			hresult = GetLastError();
			ATLASSERT(FALSE);
			goto out;
		}

		if (NdasCommBlockDeviceWriteSafeBuffer(hndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&rmd) == FALSE) {

			hresult = GetLastError();
			ATLASSERT(FALSE);
			goto out;
		}

		// Initialize whole bitmap
		
		if (BindType == NMT_SAFE_RAID1																				 ||
			BindType == NMT_SAFE_RAID_REPLACE		&& targetRidx < dibV2.nDiskCount								 || // not spare replaced 
			BindType == NMT_SAFE_RAID_REMOVE		&& targetRidx < dibV2.nDiskCount && dibV2.nDiskCount < DiskCount || // replaced with spare 
			BindType == NMT_SAFE_RAID_CLEAR_DEFECT	&& targetRidx < dibV2.nDiskCount) { // not spare replaced 
		
			NDAS_OOS_BITMAP_BLOCK	bmpBuf;

			UINT32  bmpSectorCount;
			UINT32  bitCount;
			UINT32  remaingBitsCount;
			UCHAR   dirtyBits[] = {1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7};			
	
			bmpBuf.SequenceNumHead = 0;
			bmpBuf.SequenceNumTail = 0;

			bitCount = (DWORD)((dibV2.sizeUserSpace + dibV2.iSectorsPerBit - 1)/dibV2.iSectorsPerBit);
			bmpSectorCount = (bitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;

			NdasUiDbgCall( 4, _T("bmpSectorCount = %d\n"), bmpSectorCount );

			FillMemory( bmpBuf.Bits, sizeof(bmpBuf.Bits), 0xff );	

			for (UINT j=0; j<bmpSectorCount-1; j++) {

				if (NdasCommBlockDeviceWriteSafeBuffer( hndas[nidx], NDAS_BLOCK_LOCATION_BITMAP + j, 1, (PBYTE)&bmpBuf) == FALSE) {

					hresult = GetLastError();
					ATLASSERT(FALSE);
					goto out;
				}
			}

			// For last unused sector set bits that are required.

			remaingBitsCount = bitCount - (NDAS_BIT_PER_OOS_BITMAP_BLOCK * (bmpSectorCount - 1));

			FillMemory(bmpBuf.Bits, sizeof(bmpBuf.Bits), 0);

			for (UINT j=0; j<remaingBitsCount; j++) {

				// Set bit

				bmpBuf.Bits[j/8] |= dirtyBits[j%8];
			}

			if (NdasCommBlockDeviceWriteSafeBuffer( hndas[nidx], NDAS_BLOCK_LOCATION_BITMAP + bmpSectorCount-1, 1, (PBYTE)&bmpBuf) == FALSE) {

				hresult = GetLastError();
				ATLASSERT(FALSE);
				goto out;
			}			

			// Clear unused bitmaps

			FillMemory( bmpBuf.Bits, sizeof(bmpBuf.Bits), 0 );

			for (UINT j = bmpSectorCount; j < NDAS_BLOCK_SIZE_BITMAP; j++) {

				if (NdasCommBlockDeviceWriteSafeBuffer( hndas[nidx], NDAS_BLOCK_LOCATION_BITMAP + j, 1, (PBYTE)&bmpBuf) == FALSE) {

					hresult = GetLastError();
					ATLASSERT(FALSE);
					goto out;
				}			
			}
		}
	}

out:

	DWORD lastErrorBackup = ::GetLastError();

	if (hndas) {

		for (nidx=0; nidx<DiskCount; nidx++) {

			if (hndas[nidx]) {

				ATLVERIFY( NdasCommDisconnect(hndas[nidx]) );

				hndas[nidx] = NULL;
			}
		}

		::HeapFree( ::GetProcessHeap(), NULL, hndas ); 
		hndas = NULL;
	}

	if (hardwareInfo) {
		
		::HeapFree( ::GetProcessHeap(), NULL, hardwareInfo ); 
		hardwareInfo = NULL;
	}

	if (unitInfo) {

		::HeapFree( ::GetProcessHeap(), NULL, unitInfo ); 
		unitInfo = NULL;
	}

	if (sectorCount) {

		::HeapFree( ::GetProcessHeap(), NULL, sectorCount ); 
		sectorCount = NULL;
	}

	if (bindRmd) {

		::HeapFree( ::GetProcessHeap(), NULL, bindRmd ); 
		bindRmd = NULL;
	}


	if (bindDibV2) {

		::HeapFree( ::GetProcessHeap(), NULL, bindDibV2 ); 
		bindDibV2 = NULL;
	}

	::SetLastError(lastErrorBackup);

	return hresult;
}

#define DEVICE_NOT_SET (UINT32)(0xFFFFFFFF)

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpMigrate (
   CONST PNDASCOMM_CONNECTION_INFO ConnectionInfo
   )
{
	HRESULT	hresult;
	HNDAS	hndas = NULL;

	HNDAS					 *ahndas = NULL;
	NDASCOMM_CONNECTION_INFO aConnectionInfo;
	
	NDAS_DIB	dibV1;
	NDAS_DIB_V2 dibV2;

	UINT32 dibSize;

	NDAS_RAID_META_DATA newRmd;
	NDAS_RAID_META_DATA tmpRmd;	
	NDAS_RAID_META_DATA oldRmd;	
	
	UINT32 oldMediaType;
	UINT32 j;
	
	UINT32	nidx;

	UINT32 totalDiskCount;
	
	UINT32 faultDisk  = DEVICE_NOT_SET; // Not exist. Index is role number.
	UINT32 HighUsn	  = 0;
	PBYTE  Rev1Bitmap = NULL;

	if (IsBadReadPtr(ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO))) {
		
		ATLASSERT(FALSE);

		hresult = NDASOP_ERROR_INVALID_PARAMETER;
		goto out;
	}

	// read & create original DIB_V2
	// connect to the NDAS Device

	hndas = NdasCommConnect(ConnectionInfo);

	if (hndas == NULL) {

		ATLASSERT(FALSE);
		hresult = GetLastError();
		goto out;
	}

	// fail if the NDAS Unit Device is not a single disk

	dibSize = sizeof(dibV2);

	hresult = NdasOpReadDib( hndas, &dibV2, &dibSize );

	if (FAILED(hresult)) {

		ATLASSERT(FALSE);
		goto out;
	}

	NdasUiDbgCall( 2, "dibV2.nDiskCount = %d, dibV2.nSpareCount = %d\n", dibV2.nDiskCount, dibV2.nSpareCount );

	ATLVERIFY( NdasCommDisconnect(hndas) );
	hndas = NULL;

	totalDiskCount = dibV2.nDiskCount + dibV2.nSpareCount;

	// check whether able migrate or not
	// rebuild DIB_V2
	
	oldMediaType = dibV2.iMediaType;

	switch (oldMediaType) {

	case NMT_MIRROR:	// to RAID1R3
	case NMT_RAID1:		// to RAID1R3
	case NMT_RAID1R2:	// to RAID1R3	
		
		dibV2.iMediaType = NMT_RAID1R3;
		break;

	case NMT_RAID4:		// to RAID4R
	case NMT_RAID4R2:	// to RAID4R	

		dibV2.iMediaType = NMT_RAID4R3;
		break;

	default:

		ATLASSERT(FALSE);
		hresult = NDASOP_ERROR_BIND_UNSUPPORTED;
		goto out;
	}

	ZeroMemory( &dibV1, sizeof(NDAS_DIB) );

	NDAS_DIB_V1_INVALIDATE(dibV1);

	// create ahndas

	ahndas = (HNDAS *)::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, totalDiskCount * sizeof(HNDAS) );
	
	if (ahndas == NULL) {
		
		hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY;
		goto out;
	}

	::ZeroMemory( ahndas, totalDiskCount * sizeof(HNDAS) );

	// connect to all devices. NdasOpMigrate assume all disk is accessible.
 
	for (nidx = 0; nidx < totalDiskCount; nidx++) {

		SetNdasConnectionInfoFromDIBIndex( &aConnectionInfo, TRUE, &dibV2, nidx );

		ahndas[nidx] = NdasCommConnect(&aConnectionInfo);

		if (ahndas[nidx] == NULL) {

			hresult = GetLastError();
			goto out;
		}
	}

	// Search for fault disk.
	// 
	// Each RAID revision has different method to record out-of-sync disk!!
	
	switch (oldMediaType) {

	case NMT_MIRROR: {

		// No fault tolerant existed. Assume disk is not in sync.
	
		faultDisk = 1;

		// NMT_MIRROR's first node is always smaller.

		NDAS_UNITDEVICE_HARDWARE_INFOW udinfo = {0};

		udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);

		if (NdasCommGetUnitDeviceHardwareInfoW(ahndas[0], &udinfo) == FALSE) {
			
			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}

		dibV2.sizeXArea		 = NDAS_BLOCK_SIZE_XAREA;
		dibV2.sizeUserSpace  = udinfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA;
		dibV2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;

		break;
	}		

	case NMT_RAID1: {

		// Migrate out-of-sync information.
		// NMT_RAID1 don't have RMD and don't have spare disk.
		//
		// Check which node's bitmap is clean. Clean one is defective one.
		//
		// If bitmap is recorded, that disk is correct disk.

		Rev1Bitmap = (PBYTE)::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, NDAS_BLOCK_SIZE_BITMAP_REV1 * SECTOR_SIZE );

		if (Rev1Bitmap == NULL) {
			
			ATLASSERT(FALSE);
			hresult = NDASOP_ERROR_NOT_ENOUGH_MEMORY;
			goto out;
		}
		
		for (nidx=0; nidx < totalDiskCount; nidx++) {
			
			if (NdasCommBlockDeviceRead(ahndas[nidx], NDAS_BLOCK_LOCATION_BITMAP, NDAS_BLOCK_SIZE_BITMAP_REV1, Rev1Bitmap) == FALSE) {
				
				hresult = GetLastError();
				goto out;
			}
		
			// is bitmap clean?

			for (j = 0; j < NDAS_BLOCK_SIZE_BITMAP_REV1 * SECTOR_SIZE; j++) {

				if (Rev1Bitmap[j]) {

					break;
				}
			}
			
			if (NDAS_BLOCK_SIZE_BITMAP_REV1 * SECTOR_SIZE != j) {
			
				// Bitmap is not clean, which means the other disk is fault.

				faultDisk = (nidx==0) ? 1:0;
				break;
			}
		}

		// We can keep sizeXArea and sizeUserSpace

		dibV2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;

		break;
	}

	case NMT_RAID1R2: // to RAID1R3

		// Read all RMD and find out-of-sync disk.
		// If different RMD shows different disk is out-of-sync, it is RAID-failure case
		// If no out-of-sync disk exists and mount flag is mounted status, it is unclean unmount state. 
		// Select any non-spare disk as out-of-sync.

		for (nidx=0 ; nidx < totalDiskCount; nidx++) {

			if (NdasCommBlockDeviceRead( ahndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE) &tmpRmd) == FALSE) {
				
				hresult = GetLastError();
				goto out;
			}

			if (tmpRmd.Signature != NDAS_RAID_META_DATA_SIGNATURE) {

				// Not a valid RMD.

				hresult = NDASOP_ERROR_INVALID_PARAMETER;
				goto out;
			}

			faultDisk = DEVICE_NOT_SET;

			for (j=0; j < totalDiskCount; j++) {
				
				if (tmpRmd.UnitMetaData[j].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED) {
				
					if (faultDisk == DEVICE_NOT_SET) {

						faultDisk = j;

					} else if (faultDisk == j) {

						// Same disk is marked as fault. Okay.

					} else {

						// RAID status is not consistent. Cannot migrate.
						
						hresult = NDASOP_ERROR_INVALID_PARAMETER;
						goto out;
					}
				}
			}

			if (tmpRmd.uiUSN > HighUsn) {

				// Save RMD from node with highest USN

				::CopyMemory( &oldRmd, &tmpRmd, sizeof(oldRmd) );
			}			
		}

		if (faultDisk == DEVICE_NOT_SET) {

			// Check clean-unmount

			if (oldRmd.state & NDAS_RAID_META_DATA_STATE_MOUNTED) {

				// Select any non-spare disk.

				faultDisk = 1; // Select second disk.
			}
		}

		// We can keep sizeXArea and sizeUserSpace

		dibV2.iSectorsPerBit = NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT;		
		break;

	default:

		ATLASSERT(FALSE);
		hresult = ERROR_INVALID_PARAMETER;
		goto out;
	}

	// Fill more DIB/RMD fields
	
	if (NMT_MIRROR == oldMediaType || NMT_RAID1 == oldMediaType) {

		// There wasn't RMD.
		// Fill default.

		::ZeroMemory(&newRmd, sizeof(NDAS_RAID_META_DATA));

		newRmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;

		::CoCreateGuid(&newRmd.RaidSetId);
		::CoCreateGuid(&newRmd.ConfigSetId);		

		newRmd.uiUSN = 1; // initial value
		
		for (nidx = 0; nidx < totalDiskCount; nidx++) {

			newRmd.UnitMetaData[nidx].Nidx = (UINT8)nidx;
			
			if (nidx == faultDisk) {
			
				newRmd.UnitMetaData[nidx].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
			}
		}

	} else if (NMT_RAID1R2 == oldMediaType) {

		// NMT_RAID1R2 RMD and NMT_RAID1R3 RMD is compatible

		::CopyMemory(&newRmd, &oldRmd, sizeof(newRmd));

		// To prevent former RAID member from interrupting, generate new GUID.

		newRmd.Signature = NDAS_RAID_META_DATA_SIGNATURE;

		::CoCreateGuid(&newRmd.RaidSetId);
		::CoCreateGuid(&newRmd.ConfigSetId);

		newRmd.uiUSN = 1; // initial value
		
		if (faultDisk != DEVICE_NOT_SET) {

			newRmd.UnitMetaData[faultDisk].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		}

		newRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;
	}
	
	// write DIB_V2 & RMD

	for (nidx = 0; nidx < totalDiskCount; nidx++) {

		// clear X Area
		
		hresult = NdasOpClearXArea(ahndas[nidx]);

		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			goto out;
		}

		dibV2.iSequence = nidx;

		// set CRC32
		
		SET_DIB_CRC(crc32_calc, dibV2);
		SET_RMD_CRC(crc32_calc, newRmd);

		// write DIB_V1, DIBs_V2, RMD(ignore RAID type)
		
		if (NdasCommBlockDeviceWriteSafeBuffer(ahndas[nidx], NDAS_BLOCK_LOCATION_DIB_V1, 1, (PBYTE)&dibV1) == FALSE) {
			
			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}
	
		if (NdasCommBlockDeviceWriteSafeBuffer(ahndas[nidx], NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&dibV2) == FALSE) {

			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}

		if (NdasCommBlockDeviceWriteSafeBuffer(ahndas[nidx], NDAS_BLOCK_LOCATION_RMD_T, 1, (PBYTE)&newRmd) == FALSE) {

			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}

		if (NdasCommBlockDeviceWriteSafeBuffer(ahndas[nidx], NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&newRmd) == FALSE) {

			ATLASSERT(FALSE);
			hresult = GetLastError();
			goto out;
		}
	}

	if (faultDisk != DEVICE_NOT_SET) {

		NDAS_OOS_BITMAP_BLOCK	bmpBuf;

		UINT32  bmpSectorCount;
		UINT32  bitCount;
		UINT32  remaingBitsCount;
		UCHAR   dirtyBits[] = {1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7};			

		for (nidx = 0; nidx < totalDiskCount; nidx++) {
	
			bmpBuf.SequenceNumHead = 0;
			bmpBuf.SequenceNumTail = 0;

			bitCount = (DWORD)((dibV2.sizeUserSpace + dibV2.iSectorsPerBit - 1)/dibV2.iSectorsPerBit);
			bmpSectorCount = (bitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;

			NdasUiDbgCall( 4, _T("bmpSectorCount = %d\n"), bmpSectorCount );

			FillMemory( bmpBuf.Bits, sizeof(bmpBuf.Bits), 0xff );	

			for (UINT j=0; j<bmpSectorCount-1; j++) {

				if (NdasCommBlockDeviceWriteSafeBuffer( ahndas[nidx], 
														NDAS_BLOCK_LOCATION_BITMAP + j, 
														1, 
														(PBYTE)&bmpBuf) == FALSE) {
															

					ATLASSERT(FALSE);
					hresult = GetLastError();
					goto out;
				}
			}

			// For last unused sector set bits that are required.

			remaingBitsCount = bitCount - (NDAS_BIT_PER_OOS_BITMAP_BLOCK * (bmpSectorCount - 1));

			FillMemory( bmpBuf.Bits, sizeof(bmpBuf.Bits), 0 );

			for (UINT j = 0; j < remaingBitsCount; j++) {

				// Set bit

				bmpBuf.Bits[j/8] |= dirtyBits[j%8];
			}

			if (NdasCommBlockDeviceWriteSafeBuffer( ahndas[nidx], 
													NDAS_BLOCK_LOCATION_BITMAP + bmpSectorCount-1, 
													1, 
													(PBYTE)&bmpBuf) == FALSE) {

				ATLASSERT(FALSE);
				hresult = GetLastError();
				goto out;
			}

			// Clear unused bitmaps

			FillMemory( bmpBuf.Bits, sizeof(bmpBuf.Bits), 0 );

			for (UINT j = bmpSectorCount; j < NDAS_BLOCK_SIZE_BITMAP; j++) {

				if (NdasCommBlockDeviceWriteSafeBuffer( ahndas[nidx], 
														NDAS_BLOCK_LOCATION_BITMAP + j, 
														1, 
														(PBYTE)&bmpBuf) == FALSE) {
															
					ATLASSERT(FALSE);
					hresult = GetLastError();
					goto out;
				}
			}
		}
	}

	hresult = S_OK;

out:

	if (Rev1Bitmap) {
		
		::HeapFree( ::GetProcessHeap(), 0, Rev1Bitmap );
		Rev1Bitmap = NULL;
	}

	if (ahndas) {

		for (nidx = 0; nidx < totalDiskCount; nidx++) {

			if (ahndas[nidx]) {

				ATLVERIFY( NdasCommDisconnect(ahndas[nidx]) );
				ahndas[nidx] = NULL;
			}
		}

		::HeapFree( ::GetProcessHeap(), 0, ahndas );
		ahndas = NULL;
	}

	if (hndas) {

		ATLVERIFY( NdasCommDisconnect(hndas) );
		hndas = NULL;
	}

	return hresult;
}

HRESULT
NdasOpGetOutOfSyncStatus (
	HNDAS			Hndas, 
	PNDAS_DIB_V2	DibV2,
	DWORD*			TotalBits, 
	DWORD*			SetBits
	) 
{
	BOOL	result;

	UINT32	totalBits;
	UINT32	setBits;
	UINT32	curBitCount;
	
	UINT32	bmpSectorCount;

	PNDAS_OOS_BITMAP_BLOCK bmpBuffer = NULL;

	DWORD i, j;
	
	*TotalBits = 0;
	*SetBits = 0;

	if (DibV2->iSectorsPerBit == 0) {

		ATLASSERT(FALSE);
		return FALSE;
	}

	totalBits		= (DWORD)((DibV2->sizeUserSpace + DibV2->iSectorsPerBit - 1)/DibV2->iSectorsPerBit);
	bmpSectorCount	= (totalBits + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;

	setBits = 0;

	bmpBuffer = (PNDAS_OOS_BITMAP_BLOCK) ::HeapAlloc( ::GetProcessHeap(), 
													  HEAP_ZERO_MEMORY, 
													  bmpSectorCount * sizeof(NDAS_OOS_BITMAP_BLOCK) );

	result = NdasCommBlockDeviceRead( Hndas, NDAS_BLOCK_LOCATION_BITMAP, bmpSectorCount, (PBYTE)bmpBuffer );

	if (result == FALSE) {

		ATLASSERT(FALSE);
		::HeapFree( ::GetProcessHeap(), 0, bmpBuffer );
		
		return FALSE;
	}

	curBitCount = 0;
	
	BOOL cleanupBitMap = FALSE;

	UCHAR	dirtyBits[] = { 1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7 };
	UCHAR	cleanBits[] = { ~(1<<0), ~(1<<1) , ~(1<<2), ~(1<<3), ~(1<<4), ~(1<<5), ~(1<<6), ~(1<<7) };

	for (i=0; i<bmpSectorCount; i++) {

		for (j=0; j<NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8; j++) {

			if (curBitCount < totalBits) {

				if (bmpBuffer[i].Bits[j/8] & dirtyBits[j%8]) {

					setBits++;
				}

				curBitCount++;
				continue;
			}

			// bug fix of 3.20 & 3.30 migration

			if (bmpBuffer[i].Bits[j/8] & dirtyBits[j%8]) {

				bmpBuffer[i].Bits[j/8] &= cleanBits[j%8];

				NdasUiDbgCall( 4, _T("i = %x, j= %x\n"), i, j );
				//ATLASSERT(FALSE);
				cleanupBitMap = TRUE;			
			}
		}

		if (cleanupBitMap == TRUE) {

			break;
		}
	}

	if (cleanupBitMap == TRUE) {

		NDASCOMM_CONNECTION_INFO ci;
		HNDAS					 hndas = NULL;
		BOOL					 result;

		for (UINT32 nidx=0; nidx<DibV2->nDiskCount+DibV2->nSpareCount; nidx++) {
	
			SetNdasConnectionInfoFromDIBIndex( &ci, TRUE, DibV2, nidx );

			if ((hndas = NdasCommConnect(&ci)) == NULL) {

				ATLASSERT(FALSE);
				break;
			}

			NdasCommBlockDeviceWrite( hndas, NDAS_BLOCK_LOCATION_BITMAP+i, 1, (PBYTE)&bmpBuffer[i] );
			NdasCommDisconnect(hndas);
		}
	}

	*TotalBits	= totalBits;
	*SetBits	= setBits;

	if (bmpBuffer) {

		::HeapFree( ::GetProcessHeap(), 0, bmpBuffer );
	}
	
	return TRUE;
}

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasVsmInitializeLogicalUnitDefinition (
	IN		PNDAS_DEVICE_ID				 DeviceId,
	IN		PCHAR						 UnitSimpleSerialNo,
	IN OUT	PNDAS_LOGICALUNIT_DEFINITION Definition
	)
{
	if (DeviceId == NULL) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER;
	}

	if (Definition->Size != sizeof(NDAS_LOGICALUNIT_DEFINITION)) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER;
	}

	::ZeroMemory(Definition, sizeof(NDAS_LOGICALUNIT_DEFINITION));

	Definition->Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);
	Definition->Type = NDAS_LOGICALDEVICE_TYPE_UNKNOWN;

	Definition->ConfigurationGuid = GUID_NULL;

	Definition->DiskCount  = 1;
	Definition->SpareCount = 0;

	Definition->NotLockable = FALSE;
	Definition->StartOffset = 0;

	Definition->NdasChildDeviceId[0] = *DeviceId;
	CopyMemory(Definition->NdasChildSerial[0], UnitSimpleSerialNo, NDAS_DIB_SERIAL_LEN );

	return S_OK;
}

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasVsmReadLogicalUnitDefinition (
	IN	   HNDAS					    NdasHandle,
	IN OUT PNDAS_LOGICALUNIT_DEFINITION Definition
	)
{
	HRESULT			hresult;
	NDAS_DEVICE_ID	ndasDeviceId;

	if (NdasHandle == NULL) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER;
	}

	if (sizeof(NDAS_LOGICALUNIT_DEFINITION) != Definition->Size) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER;
	}

	NdasCommGetDeviceID( NdasHandle, NULL, ndasDeviceId.Node, NULL, &ndasDeviceId.Vid );

	::ZeroMemory( Definition, sizeof(NDAS_LOGICALUNIT_DEFINITION) );

	Definition->Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);

	// Read DIB

	NDAS_DIB_V2 dibV2;
	UINT32		sizeDIB = sizeof(dibV2);
	HRESULT		success = NdasOpReadDib( NdasHandle, &dibV2, &sizeDIB );

	if (FAILED(success)) {

		ATLASSERT(FALSE);
		return success;
	}

	Definition->Type = NdasVsmConvertToNdasLogicalUnitType(dibV2.iMediaType);

	Definition->NotLockable = FALSE;
	Definition->StartOffset = dibV2.sizeStartOffset;

	switch (dibV2.iMediaType) {

	case NMT_MIRROR:
	case NMT_RAID1:
	case NMT_RAID4:
	case NMT_RAID1R2:
	case NMT_RAID4R2:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_RAID5:

		break;

	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_AOD:
	case NMT_RAID0R2:

		Definition->ConfigurationGuid  = GUID_NULL;

		Definition->DiskCount  = dibV2.nDiskCount;
		Definition->SpareCount = 0;

		for (UINT32 nidx = 0; nidx < dibV2.nDiskCount; nidx++) {

			if (dibV2.UnitDiskInfos[nidx].HwVersion == 0) {

				Definition->NotLockable = TRUE;
			}

			NdasVsmConvertToNdasDeviceId( &Definition->NdasChildDeviceId[nidx], &dibV2.UnitLocation[nidx] );
			CopyMemory( &Definition->NdasChildSerial[nidx], &dibV2.UnitSimpleSerialNo[nidx], NDAS_DIB_SERIAL_LEN );

			Definition->ActiveNdasUnits[nidx] = TRUE;
		}

		return S_OK;

	case NMT_SINGLE: {

		NDAS_UNITDEVICE_HARDWARE_INFO unitInfo;
		
		unitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);

		if (NdasCommGetUnitDeviceHardwareInfo(NdasHandle, &unitInfo) == FALSE) {

			ATLASSERT(FALSE);
			return GetLastError();
		}

		Definition->ConfigurationGuid  = GUID_NULL;

		Definition->DiskCount  = 1;
		Definition->SpareCount = 0;

		if (dibV2.UnitDiskInfos[0].HwVersion == 0) {

			Definition->NotLockable = TRUE;
		}

		NdasVsmConvertToNdasDeviceId( &Definition->NdasChildDeviceId[0], &dibV2.UnitLocation[0] );
		
		hresult = GetNdasSimpleSerialNo(&unitInfo, Definition->NdasChildSerial[0]);

		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			return hresult;
		}

		Definition->ActiveNdasUnits[0] = TRUE;

		return S_OK;
	}

	default:

		ATLASSERT(FALSE);
		SetLastError(NDASOP_ERROR_INVALID_PARAMETER);

		return S_OK;
	}

	// from here, RMD required. (RAID1, 4, 5...)
	// read rmd

	NDAS_RAID_META_DATA raidMetaData;

	if (NdasCommBlockDeviceRead(NdasHandle, NDAS_BLOCK_LOCATION_RMD, 1, &raidMetaData) == FALSE) {
		
		ATLASSERT(FALSE);
		return GetLastError();
	}

	Definition->ConfigurationGuid  = raidMetaData.RaidSetId;

	Definition->DiskCount  = dibV2.nDiskCount;
	Definition->SpareCount = dibV2.nSpareCount;

	for (UINT32 nidx = 0; nidx < Definition->DiskCount + Definition->SpareCount; nidx++) {

		NdasVsmConvertToNdasDeviceId( &Definition->NdasChildDeviceId[nidx], &dibV2.UnitLocation[nidx] );
		CopyMemory( &Definition->NdasChildSerial[nidx], &dibV2.UnitSimpleSerialNo[nidx], NDAS_DIB_SERIAL_LEN );
	}

	for (UINT8 ridx = 0; ridx < Definition->DiskCount + Definition->SpareCount; ridx++) {

		if (dibV2.UnitDiskInfos[ridx].HwVersion == 0) {

			Definition->NotLockable = TRUE;
		}

		NDAS_UNIT_META_DATA *unitMetaData    = &raidMetaData.UnitMetaData[ridx];
		UINT8				unitDeviceStatus = unitMetaData->UnitDeviceStatus;

		if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_OFFLINE) {

			Definition->ActiveNdasUnits[ridx] = FALSE;
		
		} else {

			Definition->ActiveNdasUnits[ridx] = TRUE;
		}
	}

	return S_OK;
}

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasVsmGetRaidSimpleStatus (
	IN  HNDAS						 NdasHandle,
	IN  PNDAS_LOGICALUNIT_DEFINITION Definition,
	IN  PUINT8						 NdasUnitNo,
	OUT	LPDWORD						 RaidSimpleStatusFlags
	)
{
	NdasUiDbgCall( 2, "enter\n" );

	HRESULT hresult = NDASOP_ERROR_INVALID_PARAMETER;

	if (NdasHandle == NULL || RaidSimpleStatusFlags == NULL || Definition == NULL) {

		ATLASSERT(FALSE);
		return NDASOP_ERROR_INVALID_PARAMETER;
	}

	*RaidSimpleStatusFlags = 0;

	switch (Definition->Type) {

	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:

		break;

	default:

		ATLASSERT(FALSE);
		return S_OK;
	}

	NDAS_LOGICALUNIT_DEFINITION definition;

	NDASCOMM_CONNECTION_INFO connectionInfo;
	HNDAS					 hndas;

	NDAS_RAID_META_DATA	rmd;
	UINT32				nidx;
	UINT32				usn = 0;

	for (nidx = 0; nidx < Definition->DiskCount + Definition->SpareCount; nidx++) {

		if (NdasUnitNo[nidx] != 0 && NdasUnitNo[nidx] != 1) {

			ATLASSERT( NdasUnitNo[nidx] == 0xff );
			continue;
		}

		SetNdasConnectionInfo( &connectionInfo, FALSE, Definition->NdasChildDeviceId[nidx], NdasUnitNo[nidx] );

		hndas = NdasCommConnect(&connectionInfo);
			
		if (hndas == NULL) {

			ATLASSERT(FALSE);
			continue;
		}

		if (NdasCommBlockDeviceRead(hndas, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&rmd) == FALSE) {
			
			ATLASSERT(FALSE);

			ATLVERIFY( NdasCommDisconnect(hndas) );
	
			hndas = NULL;
			continue;
		}

		if (rmd.Signature != NDAS_RAID_META_DATA_SIGNATURE || !IS_RMD_CRC_VALID(crc32_calc, rmd)) {

			ATLASSERT(FALSE);

			ATLVERIFY( NdasCommDisconnect(hndas) );

			hndas = NULL;
			continue;
		}

		if (usn < rmd.uiUSN) {

			usn = rmd.uiUSN;
		}

		definition.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);

		hresult = NdasVsmReadLogicalUnitDefinition(hndas, &definition);

		if (FAILED(hresult)) {

			ATLASSERT(FALSE);
			break;
		}

		if (memcmp(Definition, &definition, sizeof(Definition)) != 0) {

			ATLASSERT(FALSE);
			hresult = NDASOP_ERROR_INVALID_PARAMETER;
			break;
		}

		ATLVERIFY( NdasCommDisconnect(hndas) );

		hndas = NULL;
		continue;
	}

	if (hndas) {

		if (NdasCommDisconnect(hndas) == FALSE) {

			ATLASSERT(FALSE);
		}

		hndas = NULL;
	}

	if (FAILED(hresult)) {

		return hresult;
	}

	UINT8 ridx;

	for (ridx = 0; ridx < Definition->DiskCount + Definition->SpareCount; ridx++) {

		UINT8 unitDeviceStatus = rmd.UnitMetaData[ridx].UnitDeviceStatus;

		if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK) {

			*RaidSimpleStatusFlags |= 
				(ridx < Definition->DiskCount) ? NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_REGULAR : NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_SPARE;
		}

		if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR) {

			*RaidSimpleStatusFlags |= 
				(ridx < Definition->DiskCount) ? NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_REGULAR : NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_SPARE;
		}

		if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE) {

			*RaidSimpleStatusFlags |= 
				(ridx < Definition->DiskCount) ? NDAS_RAID_SIMPLE_STATUS_REPLACED_IN_REGULAR : NDAS_RAID_SIMPLE_STATUS_REPLACED_IN_SPARE;
		}
		
		if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) {

			continue;
		}

		if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED) {

			*RaidSimpleStatusFlags |= NDAS_RAID_SIMPLE_STATUS_NOT_SYNCED;
		}
	}

	// Emergency state if RAID is not recoverable any more

	if (*RaidSimpleStatusFlags & NDAS_RAID_SIMPLE_STATUS_NOT_SYNCED && 
		(*RaidSimpleStatusFlags & NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_REGULAR  ||
		 *RaidSimpleStatusFlags & NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_REGULAR)) {

		*RaidSimpleStatusFlags |= NDAS_RAID_SIMPLE_STATUS_EMERGENCY;
	}

	// Emergency state if one is not syned and another is disconnected(degraded)

	if (*RaidSimpleStatusFlags & NDAS_RAID_SIMPLE_STATUS_NOT_SYNCED) {

		for (ridx = 0; ridx < Definition->DiskCount + Definition->SpareCount; ridx++) {

			UINT8 unitDeviceStatus = rmd.UnitMetaData[ridx].UnitDeviceStatus;
			
			if (FALSE == Definition->ActiveNdasUnits[ridx] && !(unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

				*RaidSimpleStatusFlags |= NDAS_RAID_SIMPLE_STATUS_EMERGENCY;
				break;
			}
		}
	}

	NdasUiDbgCall( 2, "RaidSimpleStatusFlags = %x\n", *RaidSimpleStatusFlags );

	return S_OK;
}

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


NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpWriteBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL)
{
	NDAS_DIB_V2 DIB_V2;
	BLOCK_ACCESS_CONTROL_LIST *pBACL_Disk = NULL;
	UINT32 crc;
	BOOL bResults;
	HRESULT hresult = E_FAIL;
	UINT32 sizeDIB = sizeof(DIB_V2);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadReadPtr(pBACL, sizeof(BLOCK_ACCESS_CONTROL_LIST)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		pBACL->ElementCount < 64);

	// read DIB to replace with
	hresult = NdasOpReadDib(hNdasDevice, &DIB_V2, &sizeDIB);

	if (FAILED(hresult)) {

		goto out;
	}

	// write DIB
	DIB_V2.BACLSize = (0 != pBACL->ElementCount) ? BACL_SIZE(pBACL->ElementCount) : 0;
	SET_DIB_CRC(crc32_calc, DIB_V2);
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice,
		NDAS_BLOCK_LOCATION_DIB_V2, 1, (PBYTE)&DIB_V2);

	if(0 == pBACL->ElementCount)
	{
		// nothing to do anymore
		hresult = S_OK;
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

	hresult = S_OK;
out:
	HEAP_SAFE_FREE(pBACL_Disk);

	return hresult;
}


NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpReadBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL)
{
	NDAS_DIB_V2 DIB_V2;
	BLOCK_ACCESS_CONTROL_LIST *pBACL_Disk = NULL;
	UINT32 crc;

	UINT32 ElementCount;
	BOOL bResults;
	HRESULT hresult = E_FAIL;
	UINT32 sizeDIB = sizeof(DIB_V2);

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		!::IsBadWritePtr(pBACL, sizeof(BLOCK_ACCESS_CONTROL_LIST)));
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASOP_ERROR_INVALID_PARAMETER,
		pBACL->ElementCount < 64);

	// read DIB to get element size of BACL
	hresult = NdasOpReadDib(hNdasDevice, &DIB_V2, &sizeDIB);

	if (FAILED(hresult)) {

		goto out;
	}

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

		hresult = S_OK;
		goto out;
	}

	::CopyMemory(pBACL, pBACL_Disk, BACL_SIZE(pBACL_Disk->ElementCount));

	hresult = S_OK;
out:
	
	HEAP_SAFE_FREE(pBACL_Disk);

	return hresult;
}

